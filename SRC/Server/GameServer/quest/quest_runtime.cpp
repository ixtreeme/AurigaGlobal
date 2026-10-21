#include "stdafx.h"
#include "quest_runtime.h"

namespace quest {
void RuntimeGeneration::Swap(CQuestManager& q) noexcept {
    std::swap(vm, q.L); sources.swap(q.m_questSources); npcs.swap(q.m_mapNPC); aliases.swap(q.m_mapNPCNameID);
    timers.swap(q.m_mapTimerID); categories.swap(q.m_questCategories); names.swap(q.m_hmQuestName);
    indices.swap(q.m_mapQuestNameByIndex); starts.swap(q.m_hmQuestStartScript); events.swap(q.m_mapEventName);
#ifdef __QUEST_RENEWAL__
    categoryIndices.swap(q.QuestCategoryIndexMap);
#endif
}
RuntimeGeneration::~RuntimeGeneration() { if (vm) CloseLuaRuntime(vm); }
RuntimeScope::RuntimeScope(CQuestManager& q, std::shared_ptr<RuntimeGeneration> runtime, bool executing) : manager(q), previous(q.m_currentRuntime), execution(executing) {
    if (runtime && runtime != previous) manager.SwitchRuntime(runtime);
    if (execution) ++manager.m_runtimeExecutionDepth;
}
RuntimeScope::~RuntimeScope() { if (execution) --manager.m_runtimeExecutionDepth; if (manager.m_currentRuntime != previous) manager.SwitchRuntime(previous); }
}
