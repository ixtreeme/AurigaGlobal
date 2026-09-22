#include "../../core/stdafx.h"

#include "QuestSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "ItemSystem.hpp"
#include "../components/quest_components.hpp"

#include "../Registry.hpp"
#include "../components/identity_components.hpp"
#include "../../quest/questmanager.h"
#include "../../quest/questpc.h"

#include <string>

namespace ecs::QuestSystem {

int32_t GetFlag(entt::entity e, std::string_view flagName)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;

	const auto* identity = g_registry.try_get<ecs::PlayerID>(e);
	if (!identity)
		return 0;

	quest::PC* pc = quest::CQuestManager::instance().GetPC(e);
	return pc ? pc->GetFlag(std::string(flagName)) : 0;
}

void SetFlag(entt::entity e, std::string_view flagName, int32_t value)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	const auto* identity = g_registry.try_get<ecs::PlayerID>(e);
	if (!identity)
		return;

	if (quest::PC* pc = quest::CQuestManager::instance().GetPC(e))
		pc->SetFlag(std::string(flagName), value);
}

} // namespace ecs::QuestSystem

// Quest context accessors use generation-bearing entity references.
namespace ecs::PlayerRuntime {
namespace {
ecs::QuestContext* EnsureQuestContext(entt::entity e)
{
    if (!g_registry.valid(e)) return nullptr;
    if (!g_registry.all_of<ecs::QuestContext>(e))
        g_registry.insert<ecs::QuestContext>(&e, &e + 1);
    return g_registry.valid(e) ? g_registry.try_get<ecs::QuestContext>(e) : nullptr;
}
}
entt::entity GetQuestNPC(entt::entity e)
{
    const auto* context = g_registry.valid(e) ? g_registry.try_get<ecs::QuestContext>(e) : nullptr;
    return context && g_registry.valid(context->npc) ? context->npc : entt::null;
}
bool SetQuestNPC(entt::entity e, entt::entity npc)
{
    if (npc != entt::null && !g_registry.valid(npc)) return false;
    auto* context = EnsureQuestContext(e);
    if (!context || (npc != entt::null && !g_registry.valid(npc))) return false;
    context->npc = npc;
    return true;
}
entt::entity GetQuestNPCLockOwner(entt::entity npc)
{
    const auto* context = g_registry.valid(npc) ? g_registry.try_get<ecs::QuestContext>(npc) : nullptr;
    return context && IsPC(context->lockOwner) ? context->lockOwner : entt::null;
}
bool SetQuestNPCLockOwner(entt::entity npc, entt::entity owner)
{
    if (!IsNPC(npc) || (owner != entt::null && !IsPC(owner))) return false;
    auto* context = EnsureQuestContext(npc);
    if (!context || !IsNPC(npc) || (owner != entt::null && !IsPC(owner))) return false;
    context->lockOwner = owner;
    return true;
}

uint32_t GetQuestBy(entt::entity e)
{
    const auto* context = g_registry.valid(e) ? g_registry.try_get<ecs::QuestContext>(e) : nullptr;
    return context ? context->byVnum : 0;
}
bool SetQuestBy(entt::entity e, uint32_t questVnum)
{
    auto* context = EnsureQuestContext(e);
    if (!context) return false;
    context->byVnum = questVnum;
    return true;
}
entt::entity GetQuestItem(entt::entity e)
{
    const auto* context = g_registry.valid(e) ? g_registry.try_get<ecs::QuestContext>(e) : nullptr;
    return context && ItemSystem::IsValidItem(context->questItem) ? context->questItem : entt::null;
}
void SetQuestItem(entt::entity e, entt::entity item)
{
    if (auto* context = EnsureQuestContext(e))
        context->questItem = ItemSystem::IsValidItem(item) ? item : entt::null;
}

} // namespace ecs::PlayerRuntime
