#include "stdafx.h"
#include "TypedLoot.hpp"
#include "ItemCatalogRuntime.hpp"
#include "TypedItemOps.hpp"
#include "item_manager.h"
#include "../core/packet.h"
#include "../ecs/Registry.hpp"
#include "../ecs/SpatialHelpers.hpp"
#include "../ecs/components/spatial_components.hpp"
#include "../ecs/components/transform_components.hpp"
#include "../ecs/services/SpatialService.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/ViewSystem.hpp"
#include "../entity/char_manager.h"
#include "../world/BattleArena.h"

#include <Core/Logging.hpp>
#include <item/ItemCatalog.hpp>
#include <item/TypedGround.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
namespace items = auriga::item;

struct Entry
{
	entt::entity entity = entt::null;
	items::GroundLoot loot;
	bool pending = false; // a pickup is in the item store
};

// By ground VID. The entity carries only what clients see (TypedLootView).
std::unordered_map<std::uint32_t, Entry>& Loot()
{
	static std::unordered_map<std::uint32_t, Entry> loot;
	return loot;
}

std::int64_t NowMs()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Refusals at most once in 3 s per character: the client's auto-pickup asks
// four times a second.
bool MayTell(std::uint32_t pid)
{
	static std::unordered_map<std::uint32_t, std::int64_t> last;
	const auto now = NowMs();
	auto& at = last[pid];
	if (at && now - at < 3'000)
		return false;
	at = now;
	return true;
}

void Tell(entt::entity character, const char* text)
{
	if (MayTell(ecs::PlayerRuntime::GetPlayerID(character)))
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s", text);
}

// Off the ground: GC_ITEM_GROUND_DEL to whoever sees it, then gone.
void Retire(std::uint32_t vid)
{
	auto& all = Loot();
	const auto it = all.find(vid);
	if (it == all.end())
		return;
	const entt::entity entity = it->second.entity;
	all.erase(it);
	if (!g_registry.valid(entity))
		return;
	ecs::SpatialService::RemoveEntity(g_registry, entity);
	if (g_registry.valid(entity))
		g_registry.destroy(entity);
}

void ClearClaim(Entry& entry)
{
	entry.loot.claimPid = 0;
	auto* view = g_registry.try_get<ecs::TypedLootView>(entry.entity);
	if (!view || view->claimName.empty())
		return;
	view->claimName.clear();
	TPacketGCItemOwnership packet {};
	packet.bHeader = HEADER_GC_ITEM_OWNERSHIP;
	packet.dwVID = view->vid;
	ecs::ViewSystem::PacketView(entry.entity, &packet, sizeof(packet)); // the legacy "claim over" label
}

// The legacy pickup distance (ItemSystem::DistanceValid).
bool InReach(entt::entity loot, entt::entity character)
{
	const auto* where = g_registry.try_get<ecs::Position>(loot);
	const auto* map = g_registry.try_get<ecs::MapIndex>(loot);
	if (!where || !map || map->value != ecs::PlayerRuntime::GetMapIndex(character))
		return false;
	const std::int64_t dx = std::abs(std::int64_t(where->x) - ecs::PlayerRuntime::GetX(character));
	const std::int64_t dy = std::abs(std::int64_t(where->y) - ecs::PlayerRuntime::GetY(character));
	return std::max(dx, dy) + std::min(dx, dy) / 2 <= 2400;
}
} // namespace

bool DropTypedLoot(const items::ItemGrant& grant, std::int32_t mapIndex, std::int32_t x, std::int32_t y, std::int32_t z, entt::entity claimant)
{
	const auto catalog = items::runtime::CurrentCatalog();
	const auto* item = catalog ? catalog->Find(grant.key) : nullptr;
	if (!item || !grant.count || mapIndex <= 0)
		return false;
	const std::uint32_t vid = ITEM_MANAGER::instance().AllocateGroundVID();
	if (!vid)
		return false;
	auto insert = items::EncodeTypedGround({ vid, x, y, z, grant.key, grant.refinementLevel, grant.count, item->base.contentCode });
	if (!insert)
	{
		LOG_ERROR("TypedLoot: {} cannot be shown on the ground (error {})", item->base.contentCode, static_cast<int>(insert.error()));
		return false;
	}
	const bool claimed = claimant != entt::null && g_registry.valid(claimant) && ecs::PlayerRuntime::IsPC(claimant) &&
		!CBattleArena::IsBattleArenaMap(mapIndex);
	const auto now = NowMs();
	const items::GroundLoot loot { grant, claimed ? ecs::PlayerRuntime::GetPlayerID(claimant) : 0u,
		now + items::GroundClaimMs, now + items::GroundLifetimeMs };
	const entt::entity entity = g_registry.create();
	g_registry.emplace<ecs::SpatialKindTag>(entity, ecs::SpatialKind::TypedLoot);
	g_registry.emplace<ecs::TypedLootView>(entity, vid, std::move(*insert),
		claimed ? std::string(ecs::PlayerRuntime::GetName(claimant)) : std::string());
	if (!ecs::SpatialService::InsertEntity(g_registry, entity, static_cast<std::uint32_t>(mapIndex), x, y, z))
	{
		if (g_registry.valid(entity))
			g_registry.destroy(entity);
		return false;
	}
	Loot().insert_or_assign(vid, Entry { entity, loot, false });
	ecs::SpatialService::UpdateSectree(g_registry, entity); // shows it to the characters around
	LOG_INFO("TypedLoot: {} x{} (+{}) on map {} at {},{} vid {} claim {}", item->base.contentCode, grant.count,
		grant.refinementLevel, mapIndex, x, y, vid, loot.claimPid);
	return true;
}

bool PickupTypedLoot(entt::entity character, std::uint32_t vid)
{
	auto& all = Loot();
	const auto it = all.find(vid);
	if (it == all.end())
		return false;
	if (!ecs::PlayerRuntime::IsPC(character) || CombatSystem::IsDead(character) || ecs::PlayerRuntime::IsObserverMode(character))
		return true;
	Entry& entry = it->second;
	if (entry.pending)
		return true;
	const auto now = NowMs();
	if (!g_registry.valid(entry.entity) || items::GroundLootExpired(entry.loot, now))
	{
		Retire(vid);
		return true;
	}
	if (!InReach(entry.entity, character))
		return true;
	const std::uint32_t picker = ecs::PlayerRuntime::GetPlayerID(character);
	entt::entity claimant = entt::null;
	bool claimantInParty = false;
	if (items::GroundClaimActive(entry.loot, now) && entry.loot.claimPid != picker)
	{
		claimant = CHARACTER_MANAGER::instance().FindEntityByPID(entry.loot.claimPid);
		const entt::entity party = ecs::SocialSystem::GetParty(character);
		claimantInParty = claimant != entt::null && ecs::PlayerRuntime::IsPC(claimant) && party != entt::null &&
			ecs::SocialSystem::GetParty(claimant) == party;
	}
	const auto decision = items::DecideGroundPickup(entry.loot, now, picker, claimantInParty);
	if (decision == items::GroundPickup::Refused)
	{
		Tell(character, "Ez a zsákmány még nem a tiéd.");
		return true;
	}
	const entt::entity receiver = decision == items::GroundPickup::Claimant ? claimant : character;
	// The loot stays on the ground until the store has written the item.
	const auto sent = RequestTypedItemGrant(receiver, entry.loot.grant, [vid](bool written) {
		auto& all = Loot();
		const auto found = all.find(vid);
		if (found == all.end())
			return;
		if (written)
			Retire(vid);
		else
			found->second.pending = false;
	});
	if (sent)
	{
		entry.pending = true;
		return true;
	}
	switch (sent.error())
	{
	case TypedGrantRefusal::Busy: break; // the client's auto-pickup asks again
	case TypedGrantRefusal::NoRoom: Tell(character, "Nincs elég hely az inventoryban."); break;
	case TypedGrantRefusal::NotEnabled: Tell(character, "Az új tárgyrendszer ennél a karakternél nincs bekapcsolva."); break;
	case TypedGrantRefusal::Invalid:
	case TypedGrantRefusal::NoIds:
		LOG_ERROR("TypedLoot: vid {} cannot be picked up by pid {} (refusal {})", vid, picker, static_cast<int>(sent.error()));
		Tell(character, "Ez a zsákmány most nem vehető fel.");
		break;
	}
	return true;
}

void TickTypedLoot()
{
	auto& all = Loot();
	if (all.empty())
		return;
	const auto now = NowMs();
	std::vector<std::uint32_t> gone;
	for (auto& [vid, entry] : all)
	{
		// Expired, or its map region is gone (a closed dungeon instance).
		const bool placed = g_registry.valid(entry.entity) && ecs::SectorOf(g_registry, entry.entity);
		if (!placed || items::GroundLootExpired(entry.loot, now))
		{
			if (!entry.pending)
				gone.push_back(vid);
			continue;
		}
		if (entry.loot.claimPid && !items::GroundClaimActive(entry.loot, now))
			ClearClaim(entry);
	}
	for (const auto vid : gone)
		Retire(vid);
}
