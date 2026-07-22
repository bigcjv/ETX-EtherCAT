#include "AxisController.h"

#include "EcpwUtil.h"

#include <cstdio>
#include <stdexcept>
#include <utility>

AxisController::AxisController(ECP_SLV_TAG slave, int axisNo, std::string name)
    : slave_(slave), axisNo_(axisNo), name_(std::move(name))
{
}

void AxisController::validateCia402() const
{
    ECP_SLV_CONFIG config{};
    throwOnEcpError((name_ + " GetSlvConf").c_str(), ECPWGetSlvConf(apiSlave(), &config));

    std::printf(
        "%s: station=%u profile=%d axes=%u vendor=0x%08x product=0x%08x name=%s\n",
        name_.c_str(),
        slave_.StationTag,
        static_cast<int>(config.DeviceProfileType),
        config.AxisNumber,
        config.VendorID,
        config.ProductCode,
        config.ManufacturerDeviceName);

    if (config.DeviceProfileType != ECP_DEVICE_PROFILE_TYPE::ECP_DP_CIA402) {
        throw std::runtime_error(name_ + " is not a CiA402 drive");
    }
    if (axisNo_ < 0 || axisNo_ >= config.AxisNumber) {
        throw std::runtime_error(name_ + " axis number is outside drive AxisNumber");
    }

    U32 errorCode = 0;
    throwOnEcpError((name_ + " GetErrorCode").c_str(),
                    ECPWGetErrorCode(apiSlave(), static_cast<U8>(axisNo_), &errorCode));
    if (errorCode != 0) {
        char message[96]{};
        std::snprintf(message, sizeof(message), "%s reports drive error 0x%08x",
                      name_.c_str(), errorCode);
        throw std::runtime_error(message);
    }
}

void AxisController::prepareCsp(const AppConfig& config) const
{
    bool supportCsp = false;
    throwOnEcpError((name_ + " GetIsSupportMOP(CSP)").c_str(),
                    ECPWGetIsSupportMOP(apiSlave(), static_cast<U8>(axisNo_), ECP_MOP_CSP, &supportCsp));
    if (!supportCsp) {
        throw std::runtime_error(name_ + " does not report CSP support");
    }

    throwOnEcpError((name_ + " SetMOP(CSP)").c_str(),
                    ECPWSetMOP(apiSlave(), static_cast<U8>(axisNo_), ECP_MOP_CSP));

    ECPWSetInPosWindow(apiSlave(), static_cast<U8>(axisNo_), 100);
    ECPWSetInPosTimeout(apiSlave(), static_cast<U8>(axisNo_), 3000);

    std::printf("%s: CSP ready, feed=%.3f accel=%.3f decel=%.3f\n",
                name_.c_str(), config.feed, config.accel, config.decel);
}

void AxisController::servoOn(const AppConfig& config) const
{
    if (config.clearAlarm) {
        throwOnEcpError((name_ + " ClearAlarm").c_str(),
                        ECPWClearAlarm(apiSlave(), static_cast<U8>(axisNo_), 5000, nullptr));
    }

    throwOnEcpError((name_ + " ServoON").c_str(),
                    ECPWServoON(apiSlave(), static_cast<U8>(axisNo_), 5000, nullptr));

    if (config.resetPosition) {
        throwOnEcpError((name_ + " CSPHoming_ResetPos").c_str(),
                        ECPWCSPHoming_ResetPos(apiSlave(), static_cast<U8>(axisNo_), 0, nullptr));
    }
}

void AxisController::servoOff() const
{
    throwOnEcpError((name_ + " ServoOFF").c_str(),
                    ECPWServoOFF(apiSlave(), static_cast<U8>(axisNo_), 5000, nullptr));
}

void AxisController::printPosition() const
{
    F64 actual = 0;
    const ECP_ERR err = ECPWGetPosition(apiSlave(), static_cast<U8>(axisNo_), ECP_MON_POS_ACT_CMD, &actual);
    if (err == ECP_OK) {
        std::printf("%s: actual command position %.3f\n", name_.c_str(), actual);
    }
}
