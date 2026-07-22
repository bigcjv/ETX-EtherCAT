#pragma once

#include "AppConfig.h"
#include "ECPW.h"

#include <string>

class AxisController {
public:
    AxisController(ECP_SLV_TAG slave, int axisNo, std::string name);

    const ECP_SLV_TAG& slave() const { return slave_; }
    int axisNo() const { return axisNo_; }
    const std::string& name() const { return name_; }

    void validateCia402() const;
    void prepareCsp(const AppConfig& config) const;
    void servoOn(const AppConfig& config) const;
    void servoOff() const;
    void printPosition() const;

private:
    ECP_SLV_TAG* apiSlave() const { return const_cast<ECP_SLV_TAG*>(&slave_); }

    ECP_SLV_TAG slave_;
    int axisNo_;
    std::string name_;
};
