#include "AppConfig.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

bool hasValue(int index, int argc)
{
    return index + 1 < argc;
}

int parseInt(const char* value, const char* name)
{
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0') {
        throw std::runtime_error(std::string("invalid integer for ") + name);
    }
    return static_cast<int>(parsed);
}

double parseDouble(const char* value, const char* name)
{
    char* end = nullptr;
    const double parsed = std::strtod(value, &end);
    if (end == value || *end != '\0') {
        throw std::runtime_error(std::string("invalid number for ") + name);
    }
    return parsed;
}

} // namespace

void printUsage(const char* programName)
{
    std::cout
        << "Usage: " << programName << " [options]\n\n"
        << "Required for real motion:\n"
        << "  --enable-motion              clear alarms, servo on, and move axes\n\n"
        << "Options:\n"
        << "  --eni NAME                   ENI file under /usr/lib/ECPL/ENI (default: ENI.xml)\n"
        << "  --axes N                     number of axes/drives (default: 6)\n"
        << "  --first-station N            first EtherCAT serial station (default: 1)\n"
        << "  --axis-no N                  axis number inside each drive (default: 0)\n"
        << "  --cycle-us US                required master/DC cycle (default: 500)\n"
        << "  --cycles N                   forward/back cycles (default: 1)\n"
        << "  --distance U                 relative distance in user units (default: 1000)\n"
        << "  --feed U_PER_SEC             feed speed in user units/sec (default: 500)\n"
        << "  --accel U_PER_SEC2           acceleration (default: 1000)\n"
        << "  --decel U_PER_SEC2           deceleration (default: 1000)\n"
        << "  --settle SEC                 wait between moves (default: 0.2)\n"
        << "  --no-clear-alarm             skip ClearAlarm before ServoON\n"
        << "  --reset-position             set current position to zero before motion\n"
        << "  --keep-servo-on              leave servo enabled when program exits normally\n"
        << "  --help                       show this help\n";
}

AppConfig parseArgs(int argc, char** argv)
{
    AppConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--enable-motion") {
            config.enableMotion = true;
        } else if (arg == "--keep-servo-on") {
            config.keepServoOn = true;
        } else if (arg == "--no-clear-alarm") {
            config.clearAlarm = false;
        } else if (arg == "--reset-position") {
            config.resetPosition = true;
        } else if (arg == "--eni" && hasValue(i, argc)) {
            config.eniFileName = argv[++i];
        } else if (arg == "--axes" && hasValue(i, argc)) {
            config.axisCount = parseInt(argv[++i], "--axes");
        } else if (arg == "--first-station" && hasValue(i, argc)) {
            config.firstStation = parseInt(argv[++i], "--first-station");
        } else if (arg == "--axis-no" && hasValue(i, argc)) {
            config.axisNo = parseInt(argv[++i], "--axis-no");
        } else if (arg == "--cycle-us" && hasValue(i, argc)) {
            config.cycleTimeUs = parseInt(argv[++i], "--cycle-us");
        } else if (arg == "--cycles" && hasValue(i, argc)) {
            config.cycles = parseInt(argv[++i], "--cycles");
        } else if (arg == "--distance" && hasValue(i, argc)) {
            config.distance = parseDouble(argv[++i], "--distance");
        } else if (arg == "--feed" && hasValue(i, argc)) {
            config.feed = parseDouble(argv[++i], "--feed");
        } else if (arg == "--accel" && hasValue(i, argc)) {
            config.accel = parseDouble(argv[++i], "--accel");
        } else if (arg == "--decel" && hasValue(i, argc)) {
            config.decel = parseDouble(argv[++i], "--decel");
        } else if (arg == "--settle" && hasValue(i, argc)) {
            config.settleSeconds = parseDouble(argv[++i], "--settle");
        } else {
            throw std::runtime_error("unknown or incomplete option: " + arg);
        }
    }

    if (config.axisCount <= 0 || config.axisCount > 36) {
        throw std::runtime_error("--axes must be in range 1..36");
    }
    if (config.cycles <= 0) {
        throw std::runtime_error("--cycles must be greater than zero");
    }
    constexpr int supportedCycles[] = {125, 250, 500, 1000, 2000, 4000, 8000, 10000};
    if (std::find(std::begin(supportedCycles), std::end(supportedCycles), config.cycleTimeUs)
        == std::end(supportedCycles)) {
        throw std::runtime_error("--cycle-us is not supported by Panasonic A6B DC/SM2");
    }
    if (config.feed <= 0 || config.accel <= 0 || config.decel <= 0) {
        throw std::runtime_error("--feed, --accel and --decel must be greater than zero");
    }
    if (config.distance == 0) {
        throw std::runtime_error("--distance must not be zero");
    }

    return config;
}
