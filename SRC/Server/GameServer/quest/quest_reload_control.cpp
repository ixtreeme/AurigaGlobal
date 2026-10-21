#include "stdafx.h"
#include "quest_runtime.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <regex>
#include <ctime>

namespace quest {
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
int ProcessId() {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}
Json Read(const fs::path& file) {
    if (!fs::is_regular_file(file) || fs::is_symlink(file) || fs::file_size(file) > 4 * 1024 * 1024)
        throw std::runtime_error("invalid quest control file");
    std::ifstream input(file, std::ios::binary); return Json::parse(input);
}
void Write(const fs::path& file, const Json& value) {
    auto temporary = file; temporary += ".new-" + std::to_string(ProcessId());
    if (fs::is_symlink(temporary) || fs::is_symlink(file)) throw std::runtime_error("linked quest control file");
    { std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
      output << value.dump(2); output.flush(); if (!output) throw std::runtime_error("quest control response write failed"); }
    fs::permissions(temporary, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace);
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("quest control response publication failed");
#else
    fs::rename(temporary, file);
#endif
}
fs::path SnapshotPath(const std::string& package, const Json& decision) {
    const auto id = decision.at("id").get<std::string>(), revision = decision.at("revision").get<std::string>();
    if (decision.at("schema_version") != 1 || !std::regex_match(id, std::regex("[a-f0-9]{32}")) ||
        !std::regex_match(revision, std::regex("[a-f0-9]{64}"))) throw std::runtime_error("invalid quest transaction identity");
    const auto root = fs::canonical(package);
    const auto expected = root / "run/quest-reloads" / id / "data";
    if (fs::is_symlink(expected) || fs::canonical(expected) != expected || fs::canonical(decision.at("snapshot").get<std::string>()) != expected)
        throw std::runtime_error("quest snapshot must be inside its package transaction directory");
    return fs::canonical(expected);
}
}
void CQuestManager::ConfigureReloadControl(const std::string& package, const std::string& instance,
                                         const std::string& dataRoot, const std::string& questRoot) {
    EnsureRuntime();
    m_reloadPackage = fs::canonical(package).generic_string(); m_reloadInstance = instance;
    m_currentRuntime->originalDataRoot = fs::absolute(dataRoot).lexically_normal().generic_string();
    m_currentRuntime->originalQuestRoot = fs::absolute(questRoot).lexically_normal().generic_string();
    const char* token = std::getenv("AURIGA_SHUTDOWN_TOKEN");
    if (token && std::regex_match(token, std::regex("[a-f0-9]{48}"))) m_reloadToken = token;
}
bool CQuestManager::LoadCommittedRuntime(std::string& error) {
    if (m_reloadPackage.empty()) return true;
    const auto current = fs::path(m_reloadPackage) / "run/quest-reloads/current.json";
    if (!fs::exists(current)) return true;
    try {
        const auto decision = Read(current); const auto snapshot = SnapshotPath(m_reloadPackage, decision);
        EnsureRuntime(); m_currentRuntime->Capture(snapshot.generic_string(), (snapshot / "quest").generic_string());
        if (m_currentRuntime->revision != decision.at("revision").get<std::string>()) throw std::runtime_error("committed quest snapshot checksum mismatch");
        const auto ids = decision.at("quest_ids").get<std::map<std::string, unsigned int>>();
        std::set<unsigned int> used;
        for (const auto& [name, id] : ids)
            if (name.empty() || !id || id > UINT16_MAX || !used.insert(id).second) throw std::runtime_error("invalid committed quest ID map");
        m_currentRuntime->stableIds = ids;
        m_reloadTransaction = decision.at("id").get<std::string>(); return true;
    } catch (const std::exception& ex) { error = ex.what(); return false; }
}
void CQuestManager::PollReloadControl() {
    if (!m_activeRuntime || m_reloadToken.empty() || m_runtimeExecutionDepth || m_pollingReload || m_currentRuntime != m_activeRuntime) return;
    m_pollingReload = true;
    struct Reset { bool& value; ~Reset() { value = false; } } reset {m_pollingReload};
    const int64_t now = std::time(nullptr);
    const auto root = fs::path(m_reloadPackage), work = fs::current_path();
    auto response = [&](const std::string& status, const std::string& error = "") {
        Write(work / ".auriga-quest-response.json", {{"schema_version", 1}, {"id", m_reloadRequest},
            {"pid", ProcessId()}, {"token_hash", RuntimeSha256(m_reloadToken)}, {"instance", m_reloadInstance},
            {"revision", m_preparedRuntime ? m_preparedRuntime->revision : RuntimeRevision()}, {"status", status}, {"error", error},
            {"quest_ids", m_preparedRuntime ? m_preparedRuntime->stableIds : m_activeRuntime->stableIds}});
    };
    try {
        const auto requestFile = work / ".auriga-quest-request.json";
        if (fs::exists(requestFile)) {
            const auto request = Read(requestFile);
            const auto id = request.value("id", ""), phase = request.value("phase", "");
            if (request.value("token", "") == m_reloadToken && (id != m_reloadRequest || phase != m_reloadPhase)) {
                const auto path = SnapshotPath(m_reloadPackage, request);
                const auto expires = request.at("expires").get<int64_t>();
                if (expires < now || expires > now + 180) throw std::runtime_error("quest request expired or invalid expiry");
                m_reloadRequest = id; m_reloadPhase = phase; m_reloadExpiry = expires;
                m_reloadStatusAt = 0;
                const auto revision = request.at("revision").get<std::string>();
                if (phase == "prepare") {
                    std::string error;
                    if (PrepareReload(path.generic_string(), revision, error)) response("prepared");
                    else response("rejected", error);
                } else if (phase == "abort") {
                    AbortReload(revision); response("aborted");
                } else response("rejected", "unknown quest control phase");
            }
        }
        const auto currentFile = root / "run/quest-reloads/current.json";
        if (fs::exists(currentFile)) {
            const auto decision = Read(currentFile);
            const auto id = decision.value("id", ""), revision = decision.value("revision", "");
            if (id != m_reloadTransaction && id == m_reloadRequest && m_preparedRuntime && revision == m_preparedRuntime->revision) {
                SnapshotPath(m_reloadPackage, decision);
                if (decision.at("quest_ids").get<std::map<std::string, unsigned int>>() != m_preparedRuntime->stableIds)
                    throw std::runtime_error("committed quest ID map differs from prepared generation");
                std::string error;
                if (CommitReload(revision, error)) { m_reloadTransaction = id; m_reloadStatusAt = 0; response("activated"); }
                else response("rejected", error);
            }
        }
        if (m_preparedRuntime && now > m_reloadExpiry && m_reloadRequest != m_reloadTransaction) {
            const auto revision = m_preparedRuntime->revision; AbortReload(revision); response("expired");
        }
        if (now != m_reloadStatusAt) {
            CollectRuntimes();
            Write(work / ".auriga-quest-status.json", {{"schema_version", 1}, {"protocol", 1}, {"pid", ProcessId()},
                {"token_hash", RuntimeSha256(m_reloadToken)}, {"instance", m_reloadInstance}, {"updated_at", now},
                {"revision", RuntimeRevision()}, {"transaction", m_reloadTransaction}, {"generations", m_runtimes.size()},
                {"id_hash", RuntimeSha256(Json(m_activeRuntime->stableIds).dump())},
                {"prepared", m_preparedRuntime ? m_preparedRuntime->revision : ""}});
            m_reloadStatusAt = now;
        }
    } catch (const std::exception& error) {
        if (now != m_reloadStatusAt) { LOG_ERROR("QUEST reload control: {}", error.what()); m_reloadStatusAt = now; }
    }
}
void CQuestManager::SynchronizeReload() {
    if (m_preparedRuntime && !m_runtimeExecutionDepth && !m_pollingReload) PollReloadControl();
}
}
