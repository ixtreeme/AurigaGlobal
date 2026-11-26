#include "stdafx.h"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "TypedItemOps.hpp"
#include "TypedRecovery.hpp"
#include "TypedInventorySync.hpp"
#include "TypedLoot.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../entity/char.h"
#include "../network/desc.h"
#include <Headers/Header_GC.h>
#include "../core/utils.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"

#include <Core/Logging.hpp>
#include <item/ItemCatalog.hpp>

#include <algorithm>
#include <climits>
#include <limits>
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
	{
		// A full inventory drops the loot at the owner's feet, theirs for 60 s
		// (IC-035 C7c), with the legacy scatter.
		const std::int32_t x = ecs::PlayerRuntime::GetX(owner) + number(-7, 7) * 20;
		const std::int32_t y = ecs::PlayerRuntime::GetY(owner) + number(-7, 7) * 20;
		if (DropTypedLoot(grant, ecs::PlayerRuntime::GetMapIndex(owner), x, y, ecs::PlayerRuntime::GetZ(owner), owner))
			ecs::ChatSystem::Send(owner, CHAT_TYPE_INFO, "Nincs hely az inventoryban, a zsákmány a földre esett: %s ×%u.", code.c_str(), count);
		else
		{
			LOG_ERROR("TypedDrops: pid {} has no room for {} x{}, and it cannot go on the ground; the drop is lost", pid, code, count);
			ecs::ChatSystem::Send(owner, CHAT_TYPE_INFO, "Nincs hely az inventoryban, a zsákmány elveszett: %s ×%u.", code.c_str(), count);
		}
		break;
	}
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

items::MobDropContext MakeTypedMobDropContext(entt::entity victim, entt::entity killer)
{
	items::MobDropContext kill;
	kill.mobVnum = ecs::PlayerRuntime::GetRaceNum(victim);
	kill.mobRank = static_cast<std::uint8_t>(ecs::PlayerRuntime::GetMobRank(victim));
	kill.stone = ecs::PlayerRuntime::IsStone(victim);
	kill.killerLevel = static_cast<std::uint16_t>(std::clamp(ecs::PointSystem::GetLevel(killer), 1, 65535));
	kill.doubleDrop = ecs::PlayerRuntime::GetPremiumRemainSeconds(killer, PREMIUM_ITEM) > 0
#ifdef ENABLE_NEW_COMMON_BONUSES
		|| ecs::PointSystem::GetPrecise(killer, POINT_DOUBLE_DROP_ITEM) > 0
#endif
		;
	return kill;
}

void SendTypedMobDropPreview(entt::entity player, entt::entity target, std::uint32_t requestedVid)
{
	auto* desc = ecs::PlayerRuntime::GetDesc(player);
	if (!desc) return;
	const auto catalog = items::runtime::TypedDropsEnabled() ? items::runtime::CurrentDropCatalog() : nullptr;
	const auto race = target != entt::null && catalog ? ecs::PlayerRuntime::GetRaceNum(target) : 0;

	// GC frame: byte header, uint32 size, byte version, uint32 VID,
	// uint32 race (zero when unavailable), uint16 row count, byte flags.
	std::vector<std::uint8_t> packet;
	const auto put = [&](std::uint32_t value, unsigned bytes) {
		for (unsigned i = 0; i < bytes; ++i)
			packet.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
	};
	const auto putText = [&](const std::string& text) {
		put(static_cast<std::uint32_t>(text.size()), 2);
		packet.insert(packet.end(), text.begin(), text.end());
	};

	put(HEADER_GC_TYPED_MOB_DROP_PREVIEW, 1);
	put(0, 4);
	put(1, 1);
	put(requestedVid, 4);
	put(race, 4);
	put(0, 2);
	put(0, 1);
	constexpr std::size_t maxFrame = 1 << 20;
	constexpr auto maxShort = std::numeric_limits<std::uint16_t>::max();
	std::uint16_t count = 0;
	bool truncated = false;
	if (race != 0)
	{
		for (const auto& drop : items::PreviewMobDrops(*catalog, MakeTypedMobDropContext(target, player)))
		{
			const auto* entry = catalog->Find(drop.item);
			if (!entry) continue;
			const auto& base = entry->base;
			if (base.nameKey.size() > maxShort || base.contentCode.size() > maxShort ||
				base.iconAsset.size() > maxShort || base.descriptionKey.size() > maxShort)
			{
				truncated = true;
				continue;
			}
			const auto rowSize = 20 + base.nameKey.size() + base.contentCode.size() +
				base.iconAsset.size() + base.descriptionKey.size();
			if (count == maxShort || packet.size() + rowSize > maxFrame)
			{
				truncated = true;
				break;
			}
			put(static_cast<std::uint16_t>(drop.item.Category()), 2);
			put(drop.item.LocalId(), 4);
			put(drop.refinementLevel, 2);
			put(drop.amount, 4);
			putText(base.nameKey);
			putText(base.contentCode);
			putText(base.iconAsset);
			putText(base.descriptionKey);
			++count;
		}
	}
	for (unsigned i = 0; i < 4; ++i)
		packet[1 + i] = static_cast<std::uint8_t>(packet.size() >> (i * 8));
	packet[14] = static_cast<std::uint8_t>(count);
	packet[15] = static_cast<std::uint8_t>(count >> 8);
	packet[16] = truncated ? 1 : 0;
	if (packet.size() > 65535)
		desc->LargePacket(packet.data(), static_cast<int>(packet.size()));
	else
		desc->Packet(packet.data(), static_cast<int>(packet.size()));
}

std::vector<items::MobDrop> RollTypedMobDrops(const items::MobDropContext& kill)
{
	if (!items::runtime::TypedDropsEnabled())
		return {};
	const auto catalog = items::runtime::CurrentDropCatalog();
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
	auto queued=grant;if(!queued.createdAtUtcMs)queued.createdAtUtcMs=TypedItemEffectNow();
	if (!items::QueueGrant(queue.pending, queued, stack))
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
	if (PumpRemoteTypedItemRefresh(owner)) return;
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
