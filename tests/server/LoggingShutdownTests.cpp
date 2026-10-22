// Logging after logging::Shutdown(). The GameServer's managers outlived the core and logged
// from their destructors (CAsyncSQL::Destroy): LOG_INFO went to a null logger and every orderly
// shutdown ended in SIGSEGV (FreeBSD core of ch1, 2026-10-22). The LOG_* macros must always
// have a logger, and Shutdown() must still flush what was queued.
#include <Core/Logging.hpp>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
int failures = 0;

void Check(bool ok, const char* name)
{
    std::cout << name << ": " << (ok ? "PASS" : "FAIL") << '\n';
    failures += ok ? 0 : 1;
}

std::string Read(const std::filesystem::path& path)
{
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}
}

int main()
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / ("auriga-logging-shutdown-" + std::to_string(std::time(nullptr)));
    std::error_code ec;
    fs::remove_all(dir, ec);

    logging::Init(dir.string());
    LOG_INFO("queued before shutdown");
    logging::Shutdown();
    Check(Read(dir / "syslog.txt").find("queued before shutdown") != std::string::npos, "shutdown flushes the queued lines");

    // What a destructor running after thecore_destroy() does.
    LOG_INFO("logged after shutdown {}", 1);
    LOG_WARN("logged after shutdown");
    LOG_ERROR("logged after shutdown");
    SPDLOG_INFO("logged after shutdown through the default logger");
    Check(logging::GetLogger() != nullptr && logging::GetErrorLogger() != nullptr, "a logger exists after shutdown");

    logging::Init(dir.string());
    LOG_INFO("logged after a restart");
    logging::Shutdown();
    Check(Read(dir / "syslog.txt").find("logged after a restart") != std::string::npos, "logging starts again after a shutdown");

    fs::remove_all(dir, ec);
    std::cout << (failures == 0 ? "ALL PASSED" : "FAILED") << '\n';
    return failures == 0 ? 0 : 1;
}
