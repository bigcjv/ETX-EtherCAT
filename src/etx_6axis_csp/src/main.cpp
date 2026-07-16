#include "AppConfig.h"
#include "AxisController.h"
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

void stopAll(const std::vector<AxisController>& axes)
{
    for (const auto& axis : axes) {
        try {
            axis.stopSmooth();
        } catch (const std::exception& ex) {
            std::printf("%s\n", ex.what());
        }
    }
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

} // namespace

int main(int argc, char** argv)
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        const AppConfig config = parseArgs(argc, argv);

        std::printf("ETX 6-axis CSP demo starting\n");
        std::printf("axes=%d firstStation=%d axisNo=%d cycles=%d distance=%.3f motion=%s\n",
                    config.axisCount,
                    config.firstStation,
                    config.axisNo,
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

        for (const auto& axis : axes) {
            axis.servoOn(config);
        }

        for (int cycle = 0; cycle < config.cycles && !g_stopRequested.load(); ++cycle) {
            std::printf("Cycle %d/%d forward\n", cycle + 1, config.cycles);
            for (const auto& axis : axes) {
                axis.moveRelative(config.distance, config);
            }
            waitSettle(config.settleSeconds);

            if (g_stopRequested.load()) {
                break;
            }

            std::printf("Cycle %d/%d backward\n", cycle + 1, config.cycles);
            for (const auto& axis : axes) {
                axis.moveRelative(-config.distance, config);
            }
            waitSettle(config.settleSeconds);
        }

        stopAll(axes);
        if (!config.keepServoOn) {
            servoOffAll(axes);
        }

        std::printf("ETX 6-axis CSP demo complete\n");
        return 0;
    } catch (const std::exception& ex) {
        std::printf("fatal: %s\n", ex.what());
        return 1;
    }
}

