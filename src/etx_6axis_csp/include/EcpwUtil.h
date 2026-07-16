#pragma once

#include "ECPW.h"

#include <cstdio>
#include <stdexcept>
#include <string>

inline std::string ecpwLastMessage()
{
    const char* message = ECPWGetLastErrMsg();
    return message ? std::string(message) : std::string();
}

inline void throwOnEcpError(const char* step, ECP_ERR err)
{
    if (err == ECP_OK) {
        std::printf("[ OK ] %s\n", step);
        return;
    }

    std::string message = "[FAIL] ";
    message += step;
    message += " err=";
    message += std::to_string(static_cast<int>(err));
    const std::string last = ecpwLastMessage();
    if (!last.empty()) {
        message += " last='";
        message += last;
        message += "'";
    }
    throw std::runtime_error(message);
}

inline ECP_SLV_TAG makeSerialSlave(int station)
{
    ECP_SLV_TAG slave{};
    slave.TagType = ECP_SLV_TAG_TYPE::ECP_TAG_SERIAL;
    slave.StationTag = static_cast<U16>(station);
    return slave;
}

