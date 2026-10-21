#include "stdafx.h"
#include "quest_runtime.h"
#include "quest_data.h"
#include "config.h"
#include <algorithm>
#include <stdexcept>

namespace quest {
void CQuestManager::EnsureRuntime() {
    if (m_currentRuntime) return;
    auto runtime = std::make_shared<RuntimeGeneration>();
    runtime->originalQuestRoot = std::filesystem::absolute(g_stQuestDir).lexically_normal().generic_string();
    runtime->originalDataRoot = std::filesystem::path(runtime->originalQuestRoot).parent_path().generic_string();
    m_currentRuntime = m_activeRuntime = runtime;
    m_runtimes.push_back(runtime);
}
void CQuestManager::SwitchRuntime(const std::shared_ptr<RuntimeGeneration>& runtime) noexcept {
    if (runtime == m_currentRuntime) return;
    if (m_currentRuntime) m_currentRuntime->Swap(*this);
    m_currentRuntime = runtime;
    if (m_currentRuntime) m_currentRuntime->Swap(*this);
}
void CQuestManager::CloseRuntime(const std::shared_ptr<RuntimeGeneration>& runtime) {
    RuntimeScope scope(*this, runtime);
    if (L) { CloseLuaRuntime(L); L = nullptr; }
    runtime->closed = true;
}
std::string CQuestManager::RuntimeRevision() const { return m_activeRuntime ? m_activeRuntime->revision : ""; }
void CQuestManager::CollectRuntimes() {
    for (auto it = m_runtimes.begin(); it != m_runtimes.end();) {
        if (*it != m_activeRuntime && *it != m_preparedRuntime && *it != m_currentRuntime && it->use_count() == 1) {
            CloseRuntime(*it); it = m_runtimes.erase(it);
        } else ++it;
    }
}
bool CQuestManager::PrepareReload(const std::string& dataRoot, const std::string& expected, std::string& error) {
    EnsureRuntime();
    if (m_runtimeExecutionDepth || m_currentRuntime != m_activeRuntime) { error = "quest execution is in progress"; return false; }
    CollectRuntimes();
    if (m_preparedRuntime) {
        if (!expected.empty() && m_preparedRuntime->revision == expected) return true;
        error = "another quest generation is already prepared"; return false;
    }
    if (m_runtimes.size() >= 16) { error = "16 quest generations are retained by unfinished dialogs/timers; finish or cancel them before another reload"; return false; }
    auto candidate = std::make_shared<RuntimeGeneration>();
    candidate->originalDataRoot = m_activeRuntime->originalDataRoot;
    candidate->originalQuestRoot = m_activeRuntime->originalQuestRoot;
    candidate->stableIds = m_activeRuntime->stableIds;
    candidate->staging = true;
    try {
        candidate->Capture(dataRoot, dataRoot == candidate->originalDataRoot ? candidate->originalQuestRoot :
            (std::filesystem::path(dataRoot) / "quest").generic_string());
        if (!expected.empty() && candidate->revision != expected) throw std::runtime_error("quest snapshot checksum differs from requested revision");
        for (const auto* file : {"settings.lua", "restart_city_pos.lua"}) {
            const auto old = m_activeRuntime->files.find(file), next = candidate->files.find(file);
            if (old != m_activeRuntime->files.end() && (next == candidate->files.end() || old->second != next->second))
                throw std::runtime_error(std::string(file) + " registers server-wide data and requires a server restart");
        }
        data::Configuration config;
        if (!data::ParseConfiguration(candidate->files.at("quest/questnpc.json"), candidate->files.at("quest/questcategory.json"), config, error))
            throw std::runtime_error(error);
        m_runtimes.push_back(candidate);
        {
            RuntimeScope scope(*this, candidate);
            if (!Initialize(config)) throw std::runtime_error(L && lua_isstring(L, -1) ? lua_tostring(L, -1) : "candidate Lua initialization failed; see quest log");
            if (RuntimeSetupFingerprint(L) != RuntimeSetupFingerprint(m_activeRuntime->vm))
                throw std::runtime_error("Lua bootstrap registrations changed; apply the saved quest package with the server stopped and restart");
            const auto& previous = m_activeRuntime->sources;
            for (const auto& old : previous) {
                const auto found = std::find_if(m_questSources.begin(), m_questSources.end(), [&](const auto& q) { return q.name == old.name; });
                if (found == m_questSources.end()) throw std::runtime_error("hotreload cannot remove a persisted quest: " + old.name);
                for (const auto& [name, id] : old.states)
                    if (!found->states.contains(name) || found->states.at(name) != id)
                        throw std::runtime_error("hotreload cannot remove/change persisted state: " + old.name + '.' + name);
            }
            for (const auto vnum : m_registeredNPCVnum)
                if (!m_mapNPC.contains(vnum)) m_mapNPC[vnum].Set(vnum, std::to_string(vnum));
            // Keep timer NPC identifiers stable, including timers scheduled by old dialogs.
            for (const auto& [name, id] : m_activeRuntime->timers) {
                m_mapTimerID[name] = id; m_mapNPC[id].Set(id, name);
            }
        }
        m_preparedRuntime = candidate;
        LOG_INFO("QUEST hotreload prepared {}", candidate->revision);
        error.clear(); return true;
    } catch (const std::exception& ex) {
        error = ex.what(); CloseRuntime(candidate);
        m_runtimes.erase(std::remove(m_runtimes.begin(), m_runtimes.end(), candidate), m_runtimes.end());
        LOG_ERROR("QUEST hotreload rejected: {}", error); return false;
    }
}
bool CQuestManager::CommitReload(const std::string& revision, std::string& error) {
    if (RuntimeRevision() == revision && !revision.empty()) { AbortReload(revision); error.clear(); return true; }
    if (!m_preparedRuntime || m_preparedRuntime->revision != revision) { error = "quest generation was not prepared"; return false; }
    if (m_runtimeExecutionDepth || m_currentRuntime != m_activeRuntime) { error = "quest execution is in progress"; return false; }
    auto next = m_preparedRuntime;
    SwitchRuntime(next);
    next->staging = false; SetRuntimeStaging(L, false);
    m_activeRuntime = next; m_preparedRuntime.reset();
    m_CurrentRunningState = nullptr;
    LOG_INFO("QUEST hotreload activated {}", revision);
    CollectRuntimes(); error.clear(); return true;
}
void CQuestManager::AbortReload(const std::string& revision) {
    if (!m_preparedRuntime || m_preparedRuntime->revision != revision) return;
    auto candidate = std::move(m_preparedRuntime); CloseRuntime(candidate);
    m_runtimes.erase(std::remove(m_runtimes.begin(), m_runtimes.end(), candidate), m_runtimes.end());
}
bool CQuestManager::Reload(std::string* out) {
    std::string error;
    if (!m_reloadToken.empty()) {
        error = "Use the server manager to reload all running cores together";
        if (out) *out = error;
        LOG_ERROR("QUEST reload: {}", error); return false;
    }
    EnsureRuntime();
    bool okay = PrepareReload(m_activeRuntime->originalDataRoot, "", error);
    if (okay) okay = CommitReload(m_preparedRuntime->revision, error);
    if (out) *out = error;
    return okay;
}
NPC& CQuestManager::GetNPC(unsigned int vnum) {
    if (!m_mapNPC.contains(vnum)) m_mapNPC[vnum].Set(vnum, vnum ? std::to_string(vnum) : "notarget");
    return m_mapNPC.at(vnum);
}
}
