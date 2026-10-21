#pragma once
#include "questmanager.h"
#include "quest_snapshot.h"
#include <filesystem>
#include <set>

namespace quest {
// Only one generation is mounted in CQuestManager at a time. Moving a map
// preserves its nodes, so suspended NPC menus keep valid script pointers.
struct RuntimeGeneration : RuntimeSnapshot {
    lua_State* vm = nullptr;
    source::Catalog sources;
    decltype(CQuestManager::m_mapNPC) npcs;
    decltype(CQuestManager::m_mapNPCNameID) aliases;
    decltype(CQuestManager::m_mapTimerID) timers;
    decltype(CQuestManager::m_questCategories) categories;
    decltype(CQuestManager::m_hmQuestName) names;
    decltype(CQuestManager::m_mapQuestNameByIndex) indices;
    decltype(CQuestManager::m_hmQuestStartScript) starts;
    CQuestManager::TEventNameMap events;
#ifdef __QUEST_RENEWAL__
    decltype(CQuestManager::QuestCategoryIndexMap) categoryIndices;
#endif
    std::map<std::string, unsigned int> stableIds;
    bool closed = false;
    void Swap(CQuestManager& manager) noexcept;
    ~RuntimeGeneration();
};

class RuntimeScope {
    CQuestManager& manager;
    std::shared_ptr<RuntimeGeneration> previous;
    bool execution;
public:
    RuntimeScope(CQuestManager& manager, std::shared_ptr<RuntimeGeneration> runtime, bool execution = false);
    ~RuntimeScope();
    RuntimeScope(const RuntimeScope&) = delete;
    RuntimeScope& operator=(const RuntimeScope&) = delete;
};

// Captures game API functions before loading user libraries. During staging,
// native mutations are rejected; setup-only registrations are replayed as no-ops.
}
