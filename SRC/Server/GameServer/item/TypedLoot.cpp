#include "stdafx.h"
#include "TypedLoot.hpp"
#include "TypedShop.hpp"
#include "TypedTrade.hpp"
#include "TypedCraft.hpp"
#include "TypedItemMissions.hpp"
#include "TypedContainerOpening.hpp"
#include "TypedTokenRedemption.hpp"
#include "TypedSocketMaintenance.hpp"
#include "TypedAttributes.hpp"
#include "TypedEquipment.hpp"
#include "TypedAccessoryClock.hpp"
#include "ItemCatalogRuntime.hpp"
#include "TypedItemOps.hpp"
#include "TypedDrops.hpp"
#include "ItemStore.hpp"
#include "TypedInventorySync.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../quest/questpc.h"
#include <item/OwnedGround.hpp>
#include <random>
#include <set>
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
	bool pending = false; // a drop, pickup or expiry is in the item store
    std::uint32_t sourcePid = 0;
    std::uint64_t ownedId = 0;
    std::vector<std::byte> reservedView;
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

static std::uint32_t CreateLoot(const items::ItemGrant& grant, std::int32_t mapIndex, std::int32_t x, std::int32_t y, std::int32_t z, entt::entity claimant, bool publish)
{
	const auto catalog = items::runtime::CurrentCatalog();
	const auto* item = catalog ? catalog->Find(grant.key) : nullptr;
	if (!item || !grant.count || mapIndex <= 0)
		return 0;
	const std::uint32_t vid = ITEM_MANAGER::instance().AllocateGroundVID();
	if (!vid)
		return 0;
	auto insert = items::EncodeTypedGround({ vid, x, y, z, grant.key, grant.refinementLevel, grant.count, item->base.contentCode });
	if (!insert)
	{
		LOG_ERROR("TypedLoot: {} cannot be shown on the ground (error {})", item->base.contentCode, static_cast<int>(insert.error()));
		return 0;
	}
	const bool claimed = claimant != entt::null && g_registry.valid(claimant) && ecs::PlayerRuntime::IsPC(claimant) &&
		!CBattleArena::IsBattleArenaMap(mapIndex);
	const auto now = NowMs();
	const items::GroundLoot loot { grant, claimed ? ecs::PlayerRuntime::GetPlayerID(claimant) : 0u,
		now + items::GroundClaimMs, now + items::GroundLifetimeMs };
	const entt::entity entity = g_registry.create();
	g_registry.emplace<ecs::SpatialKindTag>(entity, ecs::SpatialKind::TypedLoot);
	if (publish) g_registry.emplace<ecs::TypedLootView>(entity, vid, std::move(*insert),
		claimed ? std::string(ecs::PlayerRuntime::GetName(claimant)) : std::string());
	if (!ecs::SpatialService::InsertEntity(g_registry, entity, static_cast<std::uint32_t>(mapIndex), x, y, z))
	{
		if (g_registry.valid(entity))
			g_registry.destroy(entity);
		return 0;
	}
	Entry entry{entity,loot};
    if(!publish) entry.reservedView=std::move(*insert);
    Loot().insert_or_assign(vid,std::move(entry));
	if(publish) ecs::SpatialService::UpdateSectree(g_registry, entity); // publication follows SQL for owned drops
	LOG_INFO("TypedLoot: {} x{} (+{}) on map {} at {},{} vid {} claim {}", item->base.contentCode, grant.count,
		grant.refinementLevel, mapIndex, x, y, vid, loot.claimPid);
	return vid;
}

bool DropTypedLoot(const items::ItemGrant& grant,std::int32_t map,std::int32_t x,std::int32_t y,std::int32_t z,entt::entity claimant)
{ return CreateLoot(grant,map,x,y,z,claimant,true)!=0; }

namespace
{
void ExpireOwned(std::uint32_t vid);
bool PickupOwned(entt::entity character,std::uint32_t vid);
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
	if (entry.ownedId) return PickupOwned(character,vid);
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
	TickTypedItemRefreshes();
    TickTypedAccessoryClocks();
	TickTypedEquipment();
	TickTypedShops();
	TickTypedTrades();
	TickTypedCrafting();
    TickTypedItemMissions();
    TickTypedContainerOpening();
    TickTypedTokenRedemption();
    TickTypedSocketMaintenance();
    TickTypedAttributes();
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
			if (!entry.pending) { if(entry.ownedId) ExpireOwned(vid); else gone.push_back(vid); }
			continue;
		}
		if (entry.loot.claimPid && !items::GroundClaimActive(entry.loot, now))
			ClearClaim(entry);
	}
	for (const auto vid : gone)
		Retire(vid);
}

namespace
{
namespace runtime = items::runtime;
namespace persistence = items::persistence;
using Lock = std::pair<std::uint32_t,std::uint64_t>;

std::array<std::uint8_t,16> GroundOperation()
{
	static std::mt19937_64 random{std::random_device{}()};
	std::array<std::uint8_t,16> op{}; for(auto& b:op)b=static_cast<std::uint8_t>(random()); op[0]|=1; return op;
}
entt::entity Online(std::uint32_t pid)
{
	auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);
	return e!=entt::null && g_registry.valid(e) ? e : entt::null;
}
std::vector<Lock> LockOwners(std::uint32_t source,std::uint32_t receiver)
{
	std::vector<Lock> locks;
	for(auto pid:{source,receiver})
	{
		if(!pid || (pid==source && !locks.empty()))continue;
		const auto e=Online(pid); if(e==entt::null)continue;
		const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);
		if(login && g_registry.all_of<ecs::TypedItems>(e))
		{
			if(g_registry.all_of<ecs::TypedItemBusy>(e))return {};
			locks.emplace_back(pid,login->token);
		}
	}
	for(const auto& [pid,token]:locks)g_registry.emplace_or_replace<ecs::TypedItemBusy>(Online(pid));
	return locks;
}
void FinishOwners(std::vector<std::expected<persistence::LoadedItemOwner,items::ItemPersistenceError>> states,const std::vector<Lock>& locks)
{
	std::set<std::uint32_t> refresh;
	for(auto& state:states)if(state)
	{
		const auto pid=static_cast<std::uint32_t>(state->fence.owner.id);const auto e=Online(pid);
		if(e==entt::null) NotifyRemoteTypedItemOwner(*state);
		auto* current=e!=entt::null ? g_registry.try_get<ecs::TypedItems>(e) : nullptr;
		// Also handles a source that logged in while the transfer was pending.
		if(current && current->state.fence.sessionGeneration==state->fence.sessionGeneration && current->state.fence.revision<=state->fence.revision)
		{ g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*state));refresh.insert(pid); }
	}
	for(const auto& [pid,token]:locks)
	{
		const auto e=Online(pid);const auto* login=e!=entt::null ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;
		if(login && login->token==token){g_registry.remove<ecs::TypedItemBusy>(e);refresh.insert(pid);}
	}
	for(auto pid:refresh){const auto e=Online(pid);SendTypedInventory(e);PumpTypedItemGrants(e);}
}
std::optional<items::ItemPlacement> FreeGroundPosition(const items::ItemCatalog& catalog,
	const persistence::LoadedItemOwner& state,const items::PersistentItem& item,items::ItemStorageKind kind)
{
	const auto* d=catalog.Find(item.key);if(!d)return {};
	for(const auto& layout:state.storages)
	{
		if(layout.storage.kind!=kind || (kind==items::ItemStorageKind::TradeEscrow && layout.storage.id!=items::OwnedGroundStorageId))continue;
		std::set<std::uint32_t> occupied;
		for(const auto& existing:state.items)if(existing.placement && existing.placement->kind==kind && existing.placement->storageId==layout.storage.id)
		{
			const auto cells=items::OccupiedItemCells(catalog,existing,layout);if(!cells)return {};
			occupied.insert(cells->begin(),cells->end());
		}
		const auto page=std::uint32_t{layout.columns}*layout.rows;
		for(std::uint32_t at=0;at<std::uint64_t{page}*layout.pages;++at)
		{
			if(at%page%layout.columns+d->base.width>layout.columns || at%page/layout.columns+d->base.height>layout.rows)continue;
			bool free=true;
			for(unsigned y=0;y<d->base.height;++y)for(unsigned x=0;x<d->base.width;++x)
				if(occupied.contains(at+y*layout.columns+x))free=false;
			if(free)return items::ItemPlacement{kind,layout.storage.id,at};
		}
	}
	return {};
}
bool GroundAdmission(entt::entity character)
{
	if(!g_registry.valid(character) || !ecs::PlayerRuntime::IsPC(character) || CombatSystem::IsDead(character) ||
		ecs::PlayerRuntime::IsObserverMode(character) || !InventorySystem::CanHandleItems(character))return false;
	const auto* quest=g_registry.try_get<ecs::QuestPCState>(character);
	return quest && quest->pc && !quest->pc->IsRunning();
}

void TransferOwned(std::uint32_t vid,entt::entity picker)
{
	auto found=Loot().find(vid);if(found==Loot().end() || found->second.pending)return;
	auto& entry=found->second;
	const auto receiver=picker==entt::null ? 0u : ecs::PlayerRuntime::GetPlayerID(picker);
	for(auto pid:{entry.sourcePid,receiver})
		if(const auto e=Online(pid);e!=entt::null && g_registry.all_of<ecs::TypedItemBusy>(e))return;
	const auto* receiving=picker!=entt::null ? g_registry.try_get<ecs::TypedItems>(picker) : nullptr;
	if(receiver && !receiving)return;
	const auto session=receiving ? receiving->state.fence.sessionGeneration : 0;
	const auto source=entry.sourcePid;
	const auto id=items::ItemInstanceId::Create(entry.ownedId);
	const auto run=runtime::ItemStore::Instance().GroundRun();if(!id || !run)return;
	auto locks=LockOwners(source,receiver);
	entry.pending=true;
	const bool queued=runtime::ItemStore::Instance().Submit([vid,source,receiver,session,id=*id,run=*run,op=GroundOperation(),locks](CAsyncSQL& sql,const items::ItemCatalog& catalog)->runtime::ItemStore::Completion {
		auto owner=persistence::LoadCurrentItemOwner(sql,catalog,{items::ItemOwnerKind::Character,source});
		std::expected<items::ItemCommit,items::ItemPersistenceError> commit=std::unexpected(items::ItemPersistenceError::SqlFailure);
		std::vector<runtime::OwnerRead> reads{{{items::ItemOwnerKind::Character,source},0}};
		if(!owner)commit=std::unexpected(owner.error());
		else if(!receiver)commit=items::MakeGroundExpireCommit(catalog,op,owner->fence,owner->storages,owner->items,id);
		else
		{
			auto target=source==receiver ? owner : persistence::LoadItemOwner(sql,catalog,{items::ItemOwnerKind::Character,receiver},session);
			if(!target)commit=std::unexpected(target.error());
			else if(target->fence.sessionGeneration!=session)commit=std::unexpected(items::ItemPersistenceError::InvalidFence);
			else
			{
				const auto original=std::ranges::find(owner->items,id,&items::PersistentItem::id);
				if(original==owner->items.end())commit=std::unexpected(items::ItemPersistenceError::MissingItem);
				else if(const auto place=FreeGroundPosition(catalog,*target,*original,items::ItemStorageKind::Inventory))
				{
					auto layouts=owner->storages;auto all=owner->items;
					if(source!=receiver){layouts.insert(layouts.end(),target->storages.begin(),target->storages.end());all.insert(all.end(),target->items.begin(),target->items.end());reads.push_back({target->fence.owner,session});}
					commit=items::MakeGroundPickupCommit(catalog,op,owner->fence,target->fence,layouts,all,id,*place);
				}
				else commit=std::unexpected(items::ItemPersistenceError::InvalidPlacement);
			}
		}
		if(commit){commit->ground=run;commit->ground->instance=id.Value();commit->ground->create=false;}
		return [vid,receiver,locks,reads=std::move(reads),commit=std::move(commit)]() mutable {
			auto failure=[&]{FinishOwners({},locks);if(auto it=Loot().find(vid);it!=Loot().end())it->second.pending=false;};
			if(!commit)
			{
				if(const auto e=Online(receiver);e!=entt::null && commit.error()==items::ItemPersistenceError::InvalidPlacement)Tell(e,"Nincs elég hely az inventoryban.");
				else LOG_ERROR("OwnedGround: vid {} planning refused (error {})",vid,static_cast<int>(commit.error()));
				failure();return;
			}
			if(!runtime::RequestCommitMany(std::move(reads),std::move(*commit),[vid,locks](runtime::MultiCommitOutcome outcome) mutable {
				FinishOwners(std::move(outcome.states),locks);
				if(outcome.write)Retire(vid);
				else {if(auto it=Loot().find(vid);it!=Loot().end())it->second.pending=false;LOG_ERROR("OwnedGround: vid {} write refused (error {})",vid,static_cast<int>(outcome.write.error()));}
			}))failure();
		};
	});
	if(!queued){entry.pending=false;FinishOwners({},locks);}
}
void ExpireOwned(std::uint32_t vid){TransferOwned(vid,entt::null);}
bool PickupOwned(entt::entity character,std::uint32_t vid)
{
	auto it=Loot().find(vid);if(it==Loot().end())return true;
	if(!g_registry.valid(it->second.entity) || items::GroundLootExpired(it->second.loot,NowMs())){ExpireOwned(vid);return true;}
	if(GroundAdmission(character) && InReach(it->second.entity,character))TransferOwned(vid,character);
	return true;
}
}

void RequestTypedItemDrop(entt::entity character,std::uint64_t itemId,std::uint32_t units)
{
	if(!GroundAdmission(character))return;
	const auto permission=ecs::resource_admission::Enter({character},auriga::resource_admission::Operation::GroundDrop);
	if(!permission || !permission->Current())return;
	auto* typed=g_registry.try_get<ecs::TypedItems>(character);
	const auto* login=g_registry.try_get<ecs::TypedItemLogin>(character);
	const auto catalog=runtime::CurrentCatalog();const auto run=runtime::ItemStore::Instance().GroundRun();
	if(!typed || !login || !catalog || !run || g_registry.all_of<ecs::TypedItemBusy>(character))return;
	const auto id=items::ItemInstanceId::Create(itemId);if(!id)return;
	const auto source=std::ranges::find(typed->state.items,*id,&items::PersistentItem::id);if(source==typed->state.items.end())return;
	const auto place=FreeGroundPosition(*catalog,typed->state,*source,items::ItemStorageKind::TradeEscrow);if(!place)return;
	const bool split=units && units<source->count;
	const auto ids=split ? runtime::PeekInstanceIds(1) : std::vector<items::ItemInstanceId>{};
	if(split && ids.empty())return;
	auto commit=items::MakeGroundDropCommit(*catalog,GroundOperation(),typed->state.fence,typed->state.storages,typed->state.items,*id,*place,units,split ? std::optional(ids.front()) : std::nullopt);
	if(!commit){Tell(character,"Ez a tárgy nem dobható el, vagy érvénytelen a darabszám.");SendTypedInventory(character);return;}
	static std::unordered_map<std::uint32_t,std::int64_t> nextDrop;
	const auto pid=ecs::PlayerRuntime::GetPlayerID(character);const auto now=NowMs();
	if(nextDrop[pid]>now)return;nextDrop[pid]=now+250;
	const auto fence=typed->state.fence;const auto token=login->token;
	const auto vid=CreateLoot({source->key,source->refinementLevel,units ? units : source->count},ecs::PlayerRuntime::GetMapIndex(character),
		ecs::PlayerRuntime::GetX(character),ecs::PlayerRuntime::GetY(character),ecs::PlayerRuntime::GetZ(character),entt::null,false);
	if(!vid){Tell(character,"Itt nem lehet tárgyat a földre tenni.");return;}
    const auto* currentLogin=g_registry.valid(character) ? g_registry.try_get<ecs::TypedItemLogin>(character) : nullptr;
    const auto* currentItems=g_registry.valid(character) ? g_registry.try_get<ecs::TypedItems>(character) : nullptr;
    if(!currentLogin || currentLogin->token!=token || !currentItems || currentItems->state.fence!=fence || !permission->Current()) {Retire(vid);return;}
	const auto dropped=split ? ids.front().Value() : itemId;
	auto& entry=Loot().at(vid);entry.ownedId=dropped;entry.sourcePid=pid;entry.pending=true;
	commit->ground=*run;commit->ground->instance=dropped;commit->ground->create=true;
	if(split)runtime::SpendInstanceIds(1);
	g_registry.emplace_or_replace<ecs::TypedItemBusy>(character);
	const bool queued=runtime::RequestCommit(fence.owner,fence.sessionGeneration,std::move(*commit),[vid,pid,token](runtime::CommitOutcome outcome) mutable {
		std::vector<std::expected<persistence::LoadedItemOwner,items::ItemPersistenceError>> states;states.push_back(std::move(outcome.state));
		FinishOwners(std::move(states),{{pid,token}});
		if(!outcome.write){Retire(vid);if(auto e=Online(pid);e!=entt::null)Tell(e,"A tárgy eldobása nem sikerült.");return;}
		auto found=Loot().find(vid);if(found==Loot().end())return;
		auto& entry=found->second;entry.pending=false;entry.loot.expiresMs=NowMs()+items::GroundLifetimeMs;
		if(!g_registry.valid(entry.entity) || !ecs::SectorOf(g_registry,entry.entity)){ExpireOwned(vid);return;}
		g_registry.emplace<ecs::TypedLootView>(entry.entity,vid,std::move(entry.reservedView),std::string{});
		ecs::SpatialService::UpdateSectree(g_registry,entry.entity);
	});
	if(!queued){Retire(vid);FinishOwners({},{{pid,token}});}
}
static_assert(sizeof(TPacketCGTypedItemDrop)==13,"CG_TYPED_ITEM_DROP wire size");
bool HandleTypedItemDropPacket(entt::entity character,const char* data)
{
	const auto* packet=reinterpret_cast<const TPacketCGTypedItemDrop*>(data);
	RequestTypedItemDrop(character,packet->qwItemId,packet->dwCount);return true;
}
