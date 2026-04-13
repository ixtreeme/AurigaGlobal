#pragma once

#include <string>


namespace IXAC::Net
{
    struct Config
    {
        std::string serverUrl;
        std::string apiKey;
        int         connectTimeoutSec = 5;
        int         requestTimeoutSec = 10;
        bool        verifySsl = true;
    };

    bool Init();
    void Shutdown();

    void SetConfig(const Config& cfg);

    void OnCheatDetected(const std::string& logPath);
}
