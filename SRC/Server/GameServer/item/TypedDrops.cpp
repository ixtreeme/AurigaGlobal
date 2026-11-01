#include "stdafx.h"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "TypedItemOps.hpp"
#include "../core/utils.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"

#include <Core/Logging.hpp>
#include <item/ItemCatalog.hpp>

#include <algorithm>
#include <climits>
#include <random>
#include <string>

namespace
{
namespace items = auriga::item;

std::uint32_t Draw(std::uint32_t bound)
{
	// The legacy tables drew with number(1, bound). A weight total above
	// INT_MAX, which the legacy files could not hold, gets its own draw.
	if (bound <= static_cast<std::uint32_t>(INT_MAX))
		return static_cast<std::uint32_t>(number(1, static_cast<int>(bound)));
	static std::mt19937 engine { std::random_device {}() };
	return std::uniform_int_distribution<std::uint32_t>(1, bound)(engine);
}

std::string CodeOf(const items::ItemKey& key)
{
	const auto catalog = items::runtime::CurrentCatalog();
	const auto* entry = catalog ? catalog->Find(key) : nullptr;
	return entry ? entry->base.contentCode
		: std::to_string(static_cast<unsigned>(key.Category())) + ":" + std::to_string(key.LocalId());
}

void Refused(entt::entity owner, const items::ItemGrant& grant, TypedGrantRefusal why)
{
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(owner);
	const std::string code = CodeOf(grant.key);
	const auto count = static_cast<unsigned>(grant.count);
	switch (why)
	{
	case TypedGrantRefusal::NoRoom:
		// A full inventory drops the loot on the ground in C7c (IC-035).
		LOG_INFO("TypedDrops: pid {} has no room for {} x{}; the drop is lost", pid, code, count);
		ecs::ChatSystem::Send(owner, CHAT_TYPE_INFO, "Nincs hely az inventoryban, a zsákmány elveszett: %s ×%u.", code.c_str(), count);
		break;
	case TypedGrantRefusal::NoIds:
		LOG_ERROR("TypedDrops: no reserved item IDs; pid {} lost {} x{}", pid, code, count);
		ecs::ChatSystem::Send(owner, CHAT_TYPE_INFO, "A zsákmány átadása nem sikerült (nincs tárgyazonosító): %s ×%u.", code.c_str(), count);
		break;
	case TypedGrantRefusal::Invalid:
		LOG_ERROR("TypedDrops: pid {} cannot get {} x{} (+{})", pid, code, count, grant.refinementLevel);
		break;
	case TypedGrantRefusal::NotEnabled:
	case TypedGrantRefusal::Busy:
		LOG_TRACE("TypedDrops: pid {} cannot take {} x{} now", pid, code, count);
		break;
	}
}
} // namespace

std::vector<items::MobDrop> RollTypedMobDrops(const items::MobDropContext& kill)
{
	if (!items::runtime::TypedDropsEnabled())
		return {};
	const auto catalog = items::runtime::CurrentCatalog();
	if (!catalog)
		return {};
	auto drops = items::PlanMobDrops(*catalog, kill, Draw);
	if (!drops)
	{
		LOG_ERROR("TypedDrops: mob {} roll failed (error {}, killer level {}, range {})", kill.mobVnum,
			static_cast<int>(drops.error()), kill.killerLevel, kill.randomRange);
		return {};
	}
	return std::move(*drops);
}

void QueueTypedItemGrant(entt::entity owner, const items::ItemGrant& grant)
{
	if (owner == entt::null || !g_registry.valid(owner) || !g_registry.all_of<ecs::TypedItems, ecs::TypedItemLogin>(owner))
		return; // no typed items: not provisioned, or the login load has not answered yet
	const auto catalog = items::runtime::CurrentCatalog();
	const auto* entry = catalog ? catalog->Find(grant.key) : nullptr;
	if (!entry)
		return;
	auto& queue = g_registry.get_or_emplace<ecs::TypedGrantQueue>(owner);
	const std::uint32_t stack = std::max<std::uint32_t>(entry->base.maxStack, 1);
	if (!items::QueueGrant(queue.pending, grant, stack))
	{
		LOG_ERROR("TypedDrops: pid {} has {} drops waiting; {} x{} is lost", ecs::PlayerRuntime::GetPlayerID(owner),
			queue.pending.size(), entry->base.contentCode, grant.count);
		return;
	}
	PumpTypedItemGrants(owner);
}

void PumpTypedItemGrants(entt::entity owner)
{
	if (owner == entt::null || !g_registry.valid(owner))
		return;
	while (auto* queue = g_registry.try_get<ecs::TypedGrantQueue>(owner))
	{
		if (queue->pending.empty())
		{
			g_registry.remove<ecs::TypedGrantQueue>(owner);
			return;
		}
		if (g_registry.all_of<ecs::TypedItemBusy>(owner))
			return; // the store's answer pumps again
		const items::ItemGrant grant = queue->pending.front();
		queue->pending.pop_front();
		const auto sent = RequestTypedItemGrant(owner, grant);
		if (sent)
			return;
		if (sent.error() == TypedGrantRefusal::Busy)
		{
			g_registry.get<ecs::TypedGrantQueue>(owner).pending.push_front(grant);
			return;
		}
		Refused(owner, grant, sent.error());
	}
}
