#include "EcpwSession.h"

#include "EcpwUtil.h"

#include <cstdio>

EcpwSession::EcpwSession(const AppConfig& config)
{
    std::printf("ECPW version: %s\n", ECPWGetVersion());
    throwOnEcpError("ECPWInit", ECPWInit());
    initialized_ = true;

    if (!config.eniFileName.empty() && config.eniFileName != "ENI.xml") {
        throwOnEcpError("ECPWSetENIFileName",
                        ECPWSetENIFileName(const_cast<char*>(config.eniFileName.c_str())));
    }

    throwOnEcpError("ECPWConnect", ECPWConnect(&slaveCount_));
    connected_ = true;

    std::printf("ECPW connected, slave count=%u, ENI=%s\n",
                slaveCount_, ECPWGetENIFileName());
}

EcpwSession::~EcpwSession()
{
    if (connected_) {
        const ECP_ERR err = ECPWDisconnect();
        std::printf("ECPWDisconnect err=%d\n", static_cast<int>(err));
    }
    if (initialized_) {
        const ECP_ERR err = ECPWClose();
        std::printf("ECPWClose err=%d\n", static_cast<int>(err));
    }
}

