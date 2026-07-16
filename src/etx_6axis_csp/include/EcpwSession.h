#pragma once

#include "AppConfig.h"
#include "ECPW.h"

class EcpwSession {
public:
    explicit EcpwSession(const AppConfig& config);
    ~EcpwSession();

    EcpwSession(const EcpwSession&) = delete;
    EcpwSession& operator=(const EcpwSession&) = delete;

    unsigned short slaveCount() const { return slaveCount_; }

private:
    bool initialized_ = false;
    bool connected_ = false;
    unsigned short slaveCount_ = 0;
};

