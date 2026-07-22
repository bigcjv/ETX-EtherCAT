#include "AppConfig.h"
#include "AxisController.h"
#include "CycleConfigValidator.h"
#include "EcpwSession.h"
#include "EcpwUtil.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <exception>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic_bool g_stopRequested{false};

void signalHandler(int)
{
    g_stopRequested.store(true);
}

void waitSettle(double seconds)
{
    if (seconds <= 0) {
        return;
    }
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

std::vector<AxisController> makeAxes(const AppConfig& config)
{
    std::vector<AxisController> axes;
    axes.reserve(static_cast<size_t>(config.axisCount));
    for (int i = 0; i < config.axisCount; ++i) {
        const int station = config.firstStation + i;
        axes.emplace_back(makeSerialSlave(station), config.axisNo, "Axis" + std::to_string(i + 1));
    }
    return axes;
}

void servoOffAll(const std::vector<AxisController>& axes)
{
    for (const auto& axis : axes) {
        try {
            axis.servoOff();
        } catch (const std::exception& ex) {
            std::printf("%s\n", ex.what());
        }
    }
}

class MotionGuard {
public:
    MotionGuard(const std::vector<AxisController>& axes, U8 groupId)
        : axes_(axes), groupId_(groupId)
    {
    }

    ~MotionGuard()
    {
        if (armed_) {
            cleanup(false);
        }
    }

    void arm() { armed_ = true; }

    void complete(bool keepServoOn)
    {
        cleanup(keepServoOn);
        armed_ = false;
    }

private:
    void cleanup(bool keepServoOn) const
    {
        ECPWGroupStop(groupId_, ECP_STOP_SMOOTH);
        ECPWGroupDisable(groupId_);
        if (!keepServoOn) {
            servoOffAll(axes_);
        }
    }

    const std::vector<AxisController>& axes_;
    U8 groupId_;
    bool armed_ = false;
};

void setupGroup(const std::vector<AxisController>& axes, U8 groupId)
{
    ECPWGroupDisable(groupId);
    throwOnEcpError("GroupClearAxes", ECPWGroupClearAxes(groupId));
    for (const auto& axis : axes) {
        ECP_SLV_TAG slave = axis.slave();
        throwOnEcpError((axis.name() + " GroupAddAxis").c_str(),
                        ECPWGroupAddAxis(&slave, static_cast<U8>(axis.axisNo()), groupId));
    }

    U8 count = 0;
    throwOnEcpError("GroupGetAxesCount", ECPWGroupGetAxesCount(groupId, &count));
    if (count != axes.size()) {
        throw std::runtime_error("group axis count does not match requested axis count");
    }

    throwOnEcpError("GroupEnable", ECPWGroupEnable(groupId));
}

void moveGroupRelative(U8 groupId, size_t axisCount, double distance, const AppConfig& config)
{
    std::vector<F64> positions(axisCount, distance);

    ECP_LIN_DATA lin{};
    lin.Positions = positions.data();
    lin.MotionProfile.Feed = config.feed;
    lin.MotionProfile.Accel = config.accel;
    lin.MotionProfile.Decel = config.decel;
    lin.MotionProfile.VelStart = 0;
    lin.MotionProfile.VelEnd = 0;
    lin.MotionProfile.SFactor = 0;
    lin.MotionProfile.OverlapRate = 0;
    lin.MotionProfile.CoordType = ECP_COORD_REL;
    lin.MotionProfile.IsCheckInPos = 1;
    lin.Fillet.dist = 0;

    std::printf("Group%u: synchronized relative move %.3f on %zu axes\n",
                groupId, distance, axisCount);
    throwOnEcpError("GroupMoveLin", ECPWGroupMoveLin(groupId, &lin, nullptr));
}

} // namespace

int main(int argc, char** argv)
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        const AppConfig config = parseArgs(argc, argv);
        validateCycleConfiguration(config);

        std::printf("ETX 6-axis CSP demo starting\n");
        std::printf("axes=%d firstStation=%d axisNo=%d cycleUs=%d cycles=%d distance=%.3f motion=%s\n",
                    config.axisCount,
                    config.firstStation,
                    config.axisNo,
                    config.cycleTimeUs,
                    config.cycles,
                    config.distance,
                    config.enableMotion ? "ENABLED" : "DISABLED");

        EcpwSession session(config);
        if (session.slaveCount() < static_cast<unsigned short>(config.firstStation + config.axisCount - 1)) {
            throw std::runtime_error("connected slave count is less than requested 6-axis range");
        }

        auto axes = makeAxes(config);
        for (const auto& axis : axes) {
            axis.validateCia402();
            axis.prepareCsp(config);
        }

        if (!config.enableMotion) {
            std::printf("Motion is disabled. Re-run with --enable-motion after safety checks.\n");
            return 0;
        }

        constexpr U8 groupId = 0;
        MotionGuard motionGuard(axes, groupId);
        motionGuard.arm();
        for (const auto& axis : axes) {
            axis.servoOn(config);
        }
        setupGroup(axes, groupId);

        for (int cycle = 0; cycle < config.cycles && !g_stopRequested.load(); ++cycle) {
            std::printf("Cycle %d/%d forward\n", cycle + 1, config.cycles);
            moveGroupRelative(groupId, axes.size(), config.distance, config);
            for (const auto& axis : axes) {
                axis.printPosition();
            }
            waitSettle(config.settleSeconds);

            if (g_stopRequested.load()) {
                break;
            }

            std::printf("Cycle %d/%d backward\n", cycle + 1, config.cycles);
            moveGroupRelative(groupId, axes.size(), -config.distance, config);
            for (const auto& axis : axes) {
                axis.printPosition();
            }
            waitSettle(config.settleSeconds);
        }

        motionGuard.complete(config.keepServoOn);

        std::printf("ETX 6-axis CSP demo complete\n");
        return 0;
    } catch (const std::exception& ex) {
        std::printf("fatal: %s\n", ex.what());
        return 1;
    }
}
