#pragma once

#include <string>

struct AppConfig {
    std::string eniFileName = "ENI.xml";
    int axisCount = 6;
    int firstStation = 1;
    int axisNo = 0;
    int cycleTimeUs = 500;
    int cycles = 1;
    double distance = 1000.0;
    double feed = 500.0;
    double accel = 1000.0;
    double decel = 1000.0;
    double settleSeconds = 0.2;
    bool enableMotion = false;
    bool keepServoOn = false;
    bool clearAlarm = true;
    bool resetPosition = false;
};

AppConfig parseArgs(int argc, char** argv);
void printUsage(const char* programName);
