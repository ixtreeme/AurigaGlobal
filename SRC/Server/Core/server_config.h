#pragma once
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace auriga::config {
enum class Role { Database, Login, Game };
struct StartupOptions {
    std::filesystem::path file { "server.json" };
    std::string instance;
    bool checkOnly { false };
    bool help { false };
};
struct Section {
    nlohmann::json values;
    std::filesystem::path source;
    std::filesystem::path workingDirectory;
    std::string instance;
};
// Removes our long options, preserving GameServer's historical short options.
bool ParseArguments(Role role, int& argc, char** argv, StartupOptions& out, std::string& error);
bool Load(const StartupOptions& options, Role role, Section& out, std::string& error);
bool Resolve(const nlohmann::json& document, const std::filesystem::path& source,
             Role role, const std::string& instance, Section& out, std::string& error);
bool EnterWorkingDirectory(const Section& section, std::string& error);
// Managed processes accept a local PID/token-matched graceful shutdown request.
bool ShutdownRequested();
const char* RoleName(Role role);
void PrintUsage(Role role);
void PrintChecked(const Section& section, Role role);
}
