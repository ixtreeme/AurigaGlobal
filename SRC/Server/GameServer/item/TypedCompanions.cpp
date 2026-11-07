#include "stdafx.h"
#include "TypedCompanions.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "TypedInventorySync.hpp"
#include "TypedPetHatch.hpp"
#include "TypedDrops.hpp"
#include "TypedRecovery.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../entity/char_manager.h"
#include "../entity/mob_manager.h"
#include "../network/desc.h"
#include "../pet/New_PetSystem.h"
#include "../pet/PetSystem.h"
#include "../story/CaravanQuestSystem.hpp"
#include "../world/arena.h"
#include "../core/packet.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <openssl/rand.h>

namespace
{
namespace item = auriga::item;
namespace store = item::runtime;
using Action = item::CompanionAction;
using Clock = std::chrono::steady_clock;

struct GrowthPetClock
{
    item::persistence::LoadedItemOwner state;
    std::uint64_t seal = 0;
    std::uint64_t token = 0;
    Clock::duration accrued{};
    std::optional<Clock::time_point> followingSince;
    std::deque<std::uint64_t> mobAwards;
    bool following = false;
    bool stopped = false;
    bool pending = false;
    std::vector<std::function<void()>> waiters;
};

using ClockKey = std::tuple<item::ItemOwner,std::uint64_t,std::uint64_t>;
std::map<ClockKey,std::shared_ptr<GrowthPetClock>> growthClocks;

ClockKey Key(item::ItemOwnerFence fence, std::uint64_t seal)
{
    return {fence.owner,fence.sessionGeneration,seal};
}

void SetFollowing(GrowthPetClock& clock, bool following, bool timed = false)
{
    clock.following = following;
    const auto now = Clock::now();
    if (following && timed && !clock.followingSince)
        clock.followingSince = now;
    else if ((!following || !timed) && clock.followingSince)
    {
        clock.accrued += now - *clock.followingSince;
        clock.followingSince.reset();
    }
}

std::uint64_t Elapsed(const GrowthPetClock& clock)
{
    auto total = clock.accrued;
    if (clock.followingSince)
        total += Clock::now() - *clock.followingSince;
    return std::chrono::duration_cast<std::chrono::milliseconds>(total).count();
}

entt::entity CurrentOwner(const GrowthPetClock& clock)
{
    const auto owner = CHARACTER_MANAGER::instance().FindEntityByPID(static_cast<std::uint32_t>(clock.state.fence.owner.id));
    const auto* login = g_registry.valid(owner) ? g_registry.try_get<ecs::TypedItemLogin>(owner) : nullptr;
    return login && login->token == clock.token ? owner : entt::null;
}

const item::PersistentItem* FindSeal(const item::persistence::LoadedItemOwner& state, std::uint64_t id)
{
    const auto found = std::ranges::find_if(state.items, [id](const auto& i) {
        return i.id.Value() == id && !i.destroyed && i.placement
            && i.placement->kind == item::ItemStorageKind::Inventory && (i.companion || i.growthPet);
    });
    return found == state.items.end() ? nullptr : &*found;
}

bool UsesActiveTime(const item::PersistentItem* seal)
{
    return seal && seal->growthPet && seal->growthPet->life.clock == item::CompanionClock::ActiveTime;
}
bool FixedHasLife(entt::entity owner,const item::PersistentItem& seal,const item::ItemCompanionRow& row)
{
    const auto& life=seal.companion->lifetime;if(life.expired)return false;
    if(row.clock==item::CompanionClock::ActiveTime)return TypedFixedCompanionElapsed(owner,seal.id.Value())<life.remainingMs;
    return row.clock==item::CompanionClock::Permanent || !life.started || TypedItemEffectNow()<life.deadlineUtcMs;
}

item::PetLifecycleRequest GrowthRequest(item::PetCareContext context, const item::PersistentItem& seal, bool active)
{
    return {context,{seal.key,seal.id.Value(),seal.revision,seal.owner.id,seal.count,seal.refinementLevel,
        false,false,false,true,active,active ? seal.id.Value() : 0},seal.revision,TypedItemEffectNow(),*seal.growthPet};
}

struct CompanionProjection
{
    std::uint64_t live = 0;
    std::uint32_t checked = 0;
    std::uint32_t race = 0;
    std::uint32_t ridingRace = 0;
};

bool OwnRiding(entt::entity owner)
{
    const auto* projection=g_registry.try_get<CompanionProjection>(owner);
    return projection && projection->ridingRace && MountSystem::GetMountVnum(owner)==projection->ridingRace;
}

bool InGame(entt::entity owner)
{
    const auto* desc = g_registry.valid(owner) ? ecs::PlayerRuntime::GetDesc(owner) : nullptr;
    return desc && desc->IsPhase(PHASE_GAME);
}

bool WorldAllowsFollower(entt::entity owner)
{
#ifdef ENABLE_PVP_ADVANCED
    if (ecs::PlayerRuntime::GetDuelOption(owner, "BlockPet"))
        return false;
#endif
    return AffectSystem::IsLoaded(owner) && !AffectSystem::IsPolymorphed(owner)
        && !CombatSystem::IsDead(owner) && !ecs::PlayerRuntime::IsObserverMode(owner)
        && ecs::PlayerRuntime::GetMapIndex(owner) != 113;
}

bool OtherFollower(entt::entity owner, std::uint64_t live)
{
    return PetSystem::CountSummoned(owner) > (live ? 1u : 0u)
        || MountSystem::IsCostumeMountSummoned(owner) || (MountSystem::IsRiding(owner) && !OwnRiding(owner))
        || MountSystem::GetSummonedHorse(owner) != entt::null
#ifdef __NEWPET_SYSTEM__
        || NewPetSystem::CountSummoned(owner)
#endif
        ;
}

bool WorldAllowsRiding(entt::entity owner)
{
    return WorldAllowsFollower(owner) && !CaravanQuest::Locked(owner)
        && !CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(owner));
}

std::uint32_t RidingRace(const item::ItemCatalog& catalog,const item::ItemCompanionRow& row)
{
    const auto* form=row.kind==item::CompanionKind::Mount && row.ridingForm ? catalog.FindCompanionForm(*row.ridingForm) : nullptr;
    return form && form->mobTemplateId<=UINT16_MAX && CMobManager::instance().Get(form->mobTemplateId) ? form->mobTemplateId : 0;
}

bool CanSummonFromBag(entt::entity owner)
{
    return InventorySystem::CanHandleItems(owner) && !CombatSystem::IsStun(owner)
        && !ecs::SocialSystem::HasExchange(owner) && !ecs::SessionSystem::IsSafeboxOpen(owner)
        && !ecs::SessionSystem::IsCubeOpen(owner) && ecs::SocialSystem::GetShopOwner(owner) == entt::null
        && ecs::SocialSystem::GetMyShop(owner) == entt::null;
}

const item::PersistentItem* Requested(const ecs::TypedItems& typed)
{
    const auto& items = typed.state.items;
    const auto found = std::ranges::find_if(items, [](const auto& i) {
        return !i.destroyed && i.placement && i.placement->kind == item::ItemStorageKind::Inventory
            && item::RecallRequested(i);
    });
    return found != items.end() ? &*found : nullptr;
}

const item::ItemCompanionRow* CompanionRow(const item::ItemCatalog& catalog, item::ItemKey key)
{
    const auto* entry = catalog.Find(key);
    return entry && item::SupportsPersistentItem(catalog, *entry) ? std::get_if<item::ItemCompanionRow>(&entry->category) : nullptr;
}

std::uint32_t FollowerRace(const item::ItemCatalog& catalog, const item::ItemCompanionRow& row,
    const item::PersistentItem& seal,entt::entity owner)
{
    auto formId = row.form;
    if (seal.growthPet)
    {
        const auto* profile = catalog.FindGrowthPetProfile(*row.growthProfile);
        formId = profile->stages[seal.growthPet->progress.stage].form;
        if(const auto* typed=g_registry.try_get<ecs::TypedItems>(owner))
            if(const auto skin=TypedPetSkinForm(owner,typed->state,seal.id.Value()))formId=skin;
    }
    const auto* form = catalog.FindCompanionForm(formId);
    if (!form || form->mobTemplateId > UINT16_MAX || !CMobManager::instance().Get(form->mobTemplateId))
        return 0;
    return form->mobTemplateId;
}

void SendRefusal(entt::entity owner, const char* message)
{
    ecs::ChatSystem::Send(owner, CHAT_TYPE_INFO, "%s", message);
    SendTypedInventory(owner);
}

void UpdateGrowthClock(entt::entity owner, const item::persistence::LoadedItemOwner& state, std::uint64_t live)
{
    for (auto it = growthClocks.lower_bound(Key(state.fence,0)); it != growthClocks.end()
        && std::get<0>(it->first) == state.fence.owner && std::get<1>(it->first) == state.fence.sessionGeneration; ++it)
        if (it->second->seal != live)
            SetFollowing(*it->second,false);

    const auto* seal = live ? FindSeal(state,live) : nullptr;
    if (!seal || !seal->growthPet)
        return;
    const auto* login = g_registry.try_get<ecs::TypedItemLogin>(owner);
    if (!login)
        return;
    auto& clock = growthClocks[Key(state.fence,live)];
    if (!clock)
        clock = std::make_shared<GrowthPetClock>(GrowthPetClock{state,live,login->token});
    if (!clock->stopped)
        SetFollowing(*clock,true,UsesActiveTime(seal));
}

void StopGrowthClock(GrowthPetClock& clock)
{
    SetFollowing(clock,false);
    clock.stopped = true;
}

void TickGrowthClocks()
{
    const auto catalog = store::CurrentCatalog();
    if (!catalog)
        return;
    std::vector<std::function<void()>> resume;
    for (auto it = growthClocks.begin(); it != growthClocks.end();)
    {
        const auto clock = it->second;
        const auto owner = CurrentOwner(*clock);
        if (owner == entt::null)
            StopGrowthClock(*clock);
        if (clock->pending)
        {
            ++it;
            continue;
        }
        const auto* seal = FindSeal(clock->state,clock->seal);
        const auto elapsed = Elapsed(*clock);
        const bool timeOwed = UsesActiveTime(seal) && seal->growthPet->life.remainingMs && elapsed;
        if (!seal || !seal->growthPet || (!clock->following && !timeOwed && clock->mobAwards.empty()))
        {
            for (auto& waiter : clock->waiters)
                resume.push_back(std::move(waiter));
            it = growthClocks.erase(it);
            continue;
        }
        ++it;
        const bool timeDue = timeOwed && (!clock->following || elapsed >= 5000 || elapsed >= seal->growthPet->life.remainingMs);
        if ((!clock->stopped && owner != entt::null && g_registry.all_of<ecs::TypedItemBusy>(owner)) ||
            (!timeDue && clock->mobAwards.empty()))
            continue;

        const auto fence = clock->state.fence;
        item::PetCareContext context;
        context.actorId = fence.owner.id;
        context.sessionGeneration = fence.sessionGeneration;
        context.rulesetRevision = 1;
        context.admittedByServer = true;
        if (RAND_bytes(context.operationId.data(),static_cast<int>(context.operationId.size())) != 1)
            continue;
        const auto count = timeDue ? 0u : (std::min)(clock->mobAwards.size(),item::MaxGrowthPetMobAwards);
        const std::vector<std::uint64_t> awards(clock->mobAwards.begin(),clock->mobAwards.begin()+count);
        auto commit = timeDue ? item::MakeGrowthPetCompanionCommit(*catalog,fence,clock->state.storages,clock->state.items,
            GrowthRequest(context,*seal,true),std::nullopt,elapsed)
            : item::MakeGrowthPetProgressCommit(*catalog,fence,clock->state.storages,clock->state.items,context,seal->id,awards);
        if (!commit)
        {
            if (count)
            {
                if (commit.error() != item::ItemPersistenceError::RequirementNotMet)
                    LOG_ERROR("Typed growth pet EXP plan failed owner {} error {}",fence.owner.id,static_cast<unsigned>(commit.error()));
                clock->mobAwards.erase(clock->mobAwards.begin(),clock->mobAwards.begin()+count);
            }
            continue;
        }
        if (!clock->stopped)
            g_registry.emplace_or_replace<ecs::TypedItemBusy>(owner);
        const bool queued = store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(*commit),
            [clock,count](store::CommitOutcome outcome) {
                if (outcome.write && count)
                    clock->mobAwards.erase(clock->mobAwards.begin(),clock->mobAwards.begin()+count);
                const auto owner = CurrentOwner(*clock);
                if (owner == entt::null || clock->stopped)
                    return;
                if (!outcome.state)
                {
                    LOG_ERROR("Typed growth pet owner reload failed pid {}", clock->state.fence.owner.id);
                    if (auto* desc = ecs::PlayerRuntime::GetDesc(owner))
                        desc->SetPhase(PHASE_CLOSE);
                    return;
                }
                g_registry.emplace_or_replace<ecs::TypedItems>(owner,std::move(*outcome.state));
                g_registry.remove<ecs::TypedItemBusy>(owner);
                SendTypedInventory(owner);
                if (outcome.write && count)
                    SendTypedPetProgress(owner,clock->seal);
                PumpTypedItemGrants(owner);
            });
        if (!queued && !clock->stopped)
            g_registry.remove<ecs::TypedItemBusy>(owner);
    }
    for (auto& waiter : resume)
        waiter();
}
}

namespace auriga::item::runtime
{
std::function<void(MultiCommitOutcome*)> BeginGrowthPetClockCommit(const std::vector<OwnerRead>& owners,const ItemCommit& commit)
{
    struct Write { std::shared_ptr<GrowthPetClock> clock; std::size_t ownerIndex; std::uint64_t spent; };
    std::vector<Write> writes;
    for (const auto& fence : commit.owners)
    {
        const auto owner = std::ranges::find(owners,fence.owner,&OwnerRead::owner);
        if (owner == owners.end())
            continue;
        for (auto it = growthClocks.lower_bound(Key(fence,0)); it != growthClocks.end()
            && std::get<0>(it->first) == fence.owner && std::get<1>(it->first) == fence.sessionGeneration; ++it)
        {
            auto clock = it->second;
            const auto change = std::ranges::find(commit.items,clock->seal,[](const auto& i) { return i.after.id.Value(); });
            std::uint64_t spent = 0;
            if (change != commit.items.end() && change->before && change->before->growthPet && change->after.growthPet)
            {
                const auto before = change->before->growthPet->life.remainingMs;
                const auto after = change->after.growthPet->life.remainingMs;
                if (const auto charge = item::PetFeedClockCharge(commit,clock->seal))
                    spent = *charge;
                else if (before > after)
                    spent = before - after;
            }
            clock->pending = true;
            writes.push_back({std::move(clock),static_cast<std::size_t>(owner - owners.begin()),spent});
        }
    }
    return [writes = std::move(writes)](MultiCommitOutcome* outcome) {
        for (const auto& write : writes)
        {
            auto& clock = *write.clock;
            clock.pending = false;
            if (!outcome)
                continue;
            if (outcome->write)
            {
                const auto now = Clock::now();
                auto accrued = clock.accrued;
                if (clock.followingSince)
                {
                    accrued += now - *clock.followingSince;
                    clock.followingSince = now;
                }
                clock.accrued = std::max(Clock::duration::zero(),accrued - std::chrono::milliseconds(write.spent));
            }
            const auto& state = outcome->states[write.ownerIndex];
            if (state)
                clock.state = *state;
            else
            {
                LOG_ERROR("Typed growth pet clock lost its owner fence pid {}",clock.state.fence.owner.id);
                StopGrowthClock(clock);
                clock.state.items.clear();
            }
        }
    };
}

bool DeferGrowthPetLogin(std::uint32_t pid,std::function<void()> resume)
{
    const ItemOwner owner{ItemOwnerKind::Character,pid};
    for (auto it = growthClocks.lower_bound({owner,0,0}); it != growthClocks.end() && std::get<0>(it->first) == owner; ++it)
        if (it->second->stopped)
        {
            it->second->waiters.push_back(std::move(resume));
            return true;
        }
    return false;
}
}

std::uint64_t TypedGrowthPetElapsed(entt::entity owner,std::uint64_t seal)
{
    const auto* typed = g_registry.try_get<ecs::TypedItems>(owner);
    if (!typed)
        return 0;
    const auto clock = growthClocks.find(Key(typed->state.fence,seal));
    return clock == growthClocks.end() ? 0 : Elapsed(*clock->second);
}

item::PetPassiveSource TypedGrowthPetPassiveSource(entt::entity owner)
{
    item::PetPassiveSource source;
    if (!InGame(owner)) return source;
    const auto live=PetSystem::LiveTypedSeal(owner);
    const auto* typed=live ? g_registry.try_get<ecs::TypedItems>(owner) : nullptr;
    const auto catalog=store::CurrentCatalog();
    const auto* seal=typed ? FindSeal(typed->state,live) : nullptr;
    if (!seal || !seal->growthPet || !item::RecallRequested(*seal) || !catalog ||
        !WorldAllowsFollower(owner) || OtherFollower(owner,live))
        return source;
    item::PetCareContext context{{1},typed->state.fence.owner.id,typed->state.fence.sessionGeneration,1};
    context.admittedByServer=context.ownerAlive=true;
    auto request=GrowthRequest(context,*seal,true);
    const auto time=item::PlanPetMaintenance(*catalog,request,TypedGrowthPetElapsed(owner,live));
    if (!time || time->expired) return source;
    request.state=time->pet.after;
    const auto bonuses=item::CollectPetPassiveBonuses(*catalog,request,true,ecs::SocialSystem::GetDungeon(owner)!=entt::null);
    if (!bonuses)
    {
        LOG_ERROR("Typed pet passive projection failed owner {}",typed->state.fence.owner.id);
        return source;
    }
    if (std::ranges::any_of(bonuses->total,[](auto value){return value!=0;}))
    {
        source.seal=live;
        source.bonuses=*bonuses;
    }
    return source;
}

std::optional<bool> TypedMountFlightPermission(entt::entity owner)
{
    if(!OwnRiding(owner))return std::nullopt;
    const auto* typed=g_registry.try_get<ecs::TypedItems>(owner);const auto catalog=store::CurrentCatalog();
    const auto* seal=typed ? Requested(*typed) : nullptr;const auto* row=seal && catalog ? CompanionRow(*catalog,seal->key) : nullptr;
    return row && seal->companion && row->kind==item::CompanionKind::Mount && row->flyingMount &&
        seal->companion->ridingRequested && FixedHasLife(owner,*seal,*row) && WorldAllowsRiding(owner);
}
item::FixedCompanionSource TypedFixedCompanionBonuses(entt::entity owner)
{
    if(!InGame(owner))return {};
    const auto* typed=g_registry.try_get<ecs::TypedItems>(owner);const auto catalog=store::CurrentCatalog();
    const auto* seal=typed ? Requested(*typed) : nullptr;const auto* row=seal && catalog ? CompanionRow(*catalog,seal->key) : nullptr;
    const auto* projection=g_registry.try_get<CompanionProjection>(owner);
    if(!row || !seal->companion || !projection || projection->live!=seal->id.Value() || !WorldAllowsFollower(owner) ||
        OtherFollower(owner,PetSystem::LiveTypedSeal(owner)))return {};
    const bool riding=row->kind==item::CompanionKind::Mount;
    if(riding ? !OwnRiding(owner) || !WorldAllowsRiding(owner) : PetSystem::LiveTypedSeal(owner)!=seal->id.Value())return {};
    const auto bonuses=item::CollectFixedCompanionBonuses(*catalog,*seal,true,riding,ecs::SocialSystem::GetDungeon(owner)!=entt::null,
        TypedItemEffectNow(),TypedFixedCompanionElapsed(owner,seal->id.Value()));
    if(!bonuses){LOG_ERROR("Fixed companion bonus projection failed owner {}",typed->state.fence.owner.id);return {};}
    return *bonuses;
}
void StopTypedGrowthPetClock(entt::entity owner)
{
    StopTypedFixedCompanions(owner);
    StopTypedPetSkins(owner);
    const auto* typed = g_registry.try_get<ecs::TypedItems>(owner);
    if (!typed)
        return;
    const auto fence = typed->state.fence;
    for (auto it = growthClocks.lower_bound(Key(fence,0)); it != growthClocks.end()
        && std::get<0>(it->first) == fence.owner && std::get<1>(it->first) == fence.sessionGeneration; ++it)
        StopGrowthClock(*it->second);
}

void FinishTypedGrowthPetClocks()
{
    for (auto& [key,clock] : growthClocks)
    {
        StopGrowthClock(*clock);
        clock->waiters.clear();
    }
    while (!growthClocks.empty())
    {
        store::ItemStore::Instance().Drain();
        TickGrowthClocks();
        if (!growthClocks.empty())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    FinishTypedPetSkins();
    FinishTypedFixedCompanions();
}

std::uint64_t RefreshTypedCompanion(entt::entity owner)
{
    if (!InGame(owner))
        return 0;
    const auto* typed = g_registry.try_get<ecs::TypedItems>(owner);
    if (!typed && !g_registry.all_of<CompanionProjection>(owner))
        return 0;
    const auto catalog = store::CurrentCatalog();
    const auto* desired = typed ? Requested(*typed) : nullptr;
    const auto* entry = desired && catalog ? catalog->Find(desired->key) : nullptr;
    const auto* row = desired && catalog ? CompanionRow(*catalog, desired->key) : nullptr;
    auto live = PetSystem::LiveTypedSeal(owner);
    const auto id = desired ? desired->id.Value() : 0;
    bool lifeRemaining = true;
    if (UsesActiveTime(desired))
    {
        const auto clock = growthClocks.find(Key(typed->state.fence,id));
        lifeRemaining = clock == growthClocks.end() || Elapsed(*clock->second) < desired->growthPet->life.remainingMs;
    }
    if(desired && desired->companion && row)lifeRemaining=FixedHasLife(owner,*desired,*row);
    const auto race = row ? FollowerRace(*catalog,*row,*desired,owner) : 0;
    const auto* previous = g_registry.try_get<CompanionProjection>(owner);
    const bool formChanged = live && live==id && previous && previous->race!=race;
    const bool canFollow = race && lifeRemaining && !entry->base.retired && WorldAllowsFollower(owner) && !OtherFollower(owner, live);
    const bool wantsRide=desired && desired->companion && desired->companion->ridingRequested;
    const auto rideRace=wantsRide && row ? RidingRace(*catalog,*row) : 0;
    const bool canRide=wantsRide && canFollow && rideRace && WorldAllowsRiding(owner);
    if(previous && previous->ridingRace && (!canRide || previous->live!=id || previous->ridingRace!=rideRace))
    {
        const bool own=OwnRiding(owner);
        g_registry.get<CompanionProjection>(owner).ridingRace=0;
        if(own)MountSystem::SetMountVnum(owner,0);
    }
    if (live && (wantsRide || live != id || !canFollow || formChanged))
    {
        PetSystem::RecallTyped(owner, live);
        live = 0;
    }
    if (!desired)
    {
        if (typed)
        {
            UpdateGrowthClock(owner,typed->state,0);
            UpdateTypedPetSkins(owner,typed->state,0);
            UpdateTypedFixedCompanions(owner,typed->state,0);
        }
        g_registry.remove<CompanionProjection>(owner);
        return 0;
    }

    auto& projection = g_registry.get_or_emplace<CompanionProjection>(owner);
    const auto now = get_dword_time();
    if(wantsRide)
    {
        if(canRide && !OwnRiding(owner) && !MountSystem::IsRiding(owner))
        {
            // Own the projection before publishing the normal rider insert.
            projection.ridingRace=rideRace;
            MountSystem::SetMountVnum(owner,rideRace);
        }
        projection.live=canRide && OwnRiding(owner) ? id : 0;
        projection.checked=now;
        UpdateGrowthClock(owner,typed->state,0);
        UpdateTypedPetSkins(owner,typed->state,0);
        UpdateTypedFixedCompanions(owner,typed->state,projection.live);
        return projection.live;
    }
    projection.ridingRace=0;
    // Reconcile pending intent after the login affect load or a temporary map
    // restriction. Rate-limit failed materialization without changing SQL.
    if (!live && canFollow && (formChanged || now - projection.checked >= 500u))
    {
        const auto name = desired->growthPet ? std::string_view(desired->growthPet->name) : std::string_view{};
        if (PetSystem::SummonTyped(owner, race, id, row->kind == item::CompanionKind::Mount, name))
        {
            live = id;
            projection.race = race;
        }
    }
    if (now - projection.checked >= 500u)
        projection.checked = now;
    projection.live = live;
    UpdateGrowthClock(owner,typed->state,live);
    UpdateTypedPetSkins(owner,typed->state,live);
    UpdateTypedFixedCompanions(owner,typed->state,live);
    return live;
}

void AwardTypedGrowthPetMobExperience(entt::entity owner,entt::entity defeated,int reward)
{
    // Match the legacy pet's 45% scale without coupling the player's reward
    // to the asynchronous typed item transaction.
    const auto experience = reward > 0 ? std::uint64_t(reward)*9/20 : 0;
    if (!experience || !InGame(owner) || (!ecs::PlayerRuntime::IsMonster(defeated) &&
        !ecs::PlayerRuntime::IsStone(defeated)))
        return;
    const auto live = PetSystem::LiveTypedSeal(owner);
    const auto* typed = live ? g_registry.try_get<ecs::TypedItems>(owner) : nullptr;
    if (!typed)
        return;
    const auto clock = growthClocks.find(Key(typed->state.fence,live));
    const auto* seal = FindSeal(typed->state,live);
    if (clock==growthClocks.end() || clock->second->stopped || !seal || !seal->growthPet ||
        !WorldAllowsFollower(owner) || (UsesActiveTime(seal) && Elapsed(*clock->second)>=seal->growthPet->life.remainingMs))
        return;
    clock->second->mobAwards.push_back(experience);
}

void TickTypedCompanions()
{
    // Refresh may remove a projection. Keep a stable list across that pass.
    std::vector<entt::entity> due;
    const auto now = get_dword_time();
    for (auto [owner, projection] : g_registry.view<CompanionProjection>().each())
        if (now - projection.checked >= 500u)
            due.push_back(owner);
    for (auto owner : due)
    {
        const auto* projection = g_registry.try_get<CompanionProjection>(owner);
        if (!projection || !InGame(owner))
            continue;
        const auto before = projection->live;
        if (RefreshTypedCompanion(owner) != before)
            SendTypedInventory(owner);
    }
    TickGrowthClocks();
    TickTypedPetSkins();
    TickTypedFixedCompanions();
}

bool HandleTypedCompanionPacket(entt::entity owner, const char* data)
{
    static_assert(sizeof(TPacketCGTypedCompanion) == 10);
    if (!InGame(owner))
        return true;
    TPacketCGTypedCompanion packet{};
    std::memcpy(&packet, data, sizeof(packet));
    const auto action = static_cast<Action>(packet.bAction);
    if (!packet.qwItemId || action<Action::Summon || action>Action::Dismount)
        return true;
    const auto* typed = g_registry.try_get<ecs::TypedItems>(owner);
    const auto* login = g_registry.try_get<ecs::TypedItemLogin>(owner);
    const auto catalog = store::CurrentCatalog();
    if (!typed || !login || !catalog)
        return true;
    if (g_registry.all_of<ecs::TypedItemBusy>(owner))
    {
        SendRefusal(owner, "Az előző tárgyművelet még folyamatban van.");
        return true;
    }

    const auto& state = typed->state;
    const auto* found = FindSeal(state,packet.qwItemId);
    if (!found)
    {
        SendRefusal(owner, "A pecsét nincs a leltáradban.");
        return true;
    }
    const bool summon = action == Action::Summon;
    const auto* row = CompanionRow(*catalog, found->key);
    const bool rideAction=action==Action::Mount || action==Action::Dismount;
    if(rideAction && (!row || row->kind!=item::CompanionKind::Mount || !found->companion))
    {
        SendRefusal(owner,"Csak hátaspecsétre ülhetsz fel.");return true;
    }
    if ((action==Action::Summon && item::RecallRequested(*found)) ||
        (action==Action::Dismiss && !item::RecallRequested(*found)) ||
        (rideAction && found->companion->ridingRequested==(action==Action::Mount)))
    {
        SendTypedInventory(owner);
        return true;
    }
    const auto* active = Requested(*typed);
    const auto live = PetSystem::LiveTypedSeal(owner);
    const auto facts = TypedItemEffectFacts(owner);
    const bool worldAllowsFollower = WorldAllowsFollower(owner);
    if (!row || (summon && (!facts.allowed || !facts.alive || !worldAllowsFollower || !CanSummonFromBag(owner)
        || (active && active->id != found->id) || OtherFollower(owner, live) || !FollowerRace(*catalog, *row, *found,owner))))
    {
        SendRefusal(owner, "A társ ebben az állapotban nem idézhető meg.");
        return true;
    }
    if(action==Action::Mount && (!facts.allowed || !facts.alive || !CanSummonFromBag(owner) ||
        !WorldAllowsRiding(owner) || live!=found->id.Value() || !active || active->id!=found->id ||
        OtherFollower(owner,live) || !RidingRace(*catalog,*row)))
    {
        SendRefusal(owner,"A megidézett hátasra ebben az állapotban nem ülhetsz fel.");return true;
    }

    const auto pid = ecs::PlayerRuntime::GetPlayerID(owner);
    std::array<std::uint8_t,16> operationId{};
    if (RAND_bytes(operationId.data(),static_cast<int>(operationId.size())) != 1)
    {
        SendRefusal(owner, "A társ művelete nem indítható el.");
        return true;
    }
    auto commit = [&]() -> std::expected<item::ItemCommit,item::ItemPersistenceError> {
        if (found->growthPet)
        {
            const item::PetCareContext context{operationId,pid,state.fence.sessionGeneration,1,
                facts.actor.level,static_cast<std::uint8_t>(facts.actor.classIndex+1),
                static_cast<std::uint8_t>(facts.actor.sexIndex+1),true,facts.allowed,facts.alive,
                ecs::PlayerRuntime::IsObserverMode(owner),AffectSystem::IsPolymorphed(owner),facts.actor.inCombat};
            return item::MakeGrowthPetCompanionCommit(*catalog,state.fence,state.storages,state.items,
                GrowthRequest(context,*found,live==found->id.Value()),action);
        }
        item::CompanionRequest request{found->key};
        request.state = *found->companion;
        if(request.state.ridingRequested)
        {
            request.state.phase=item::CompanionPhase::Riding;
            request.state.activeFormId=*row->ridingForm;
        }
        else if (live == found->id.Value())
        {
            request.state.phase = item::CompanionPhase::Following;
            request.state.activeFormId = row->form;
        }
        request.instanceId = found->id.Value();
        request.revision = found->revision;
        request.ownerId = state.fence.owner.id;
        request.actorId = pid;
        request.rulesetRevision = 1;
        request.count = found->count;
        request.refinementLevel = found->refinementLevel;
        request.characterLevel = facts.actor.level;
        request.characterClass = facts.actor.classIndex + 1;
        request.sex = facts.actor.sexIndex + 1;
        request.action = action;
        request.nowUtcMs = TypedItemEffectNow();
        request.elapsedOwnerMs=TypedFixedCompanionElapsed(owner,found->id.Value());
        request.elapsedWasActive=request.elapsedOwnerMs!=0;
        request.admittedByServer = true;
        request.ownerAlive = facts.alive;
        request.inCombat = facts.actor.inCombat;
        request.observer=ecs::PlayerRuntime::IsObserverMode(owner);
        request.polymorphed=AffectSystem::IsPolymorphed(owner);
        request.worldAllowsCompanion = worldAllowsFollower;
        request.worldAllowsRiding = WorldAllowsRiding(owner);
        request.operationId = operationId;
        return item::MakeCompanionCommit(*catalog, state.fence, state.storages, state.items, request);
    }();
    auto permit = ecs::resource_admission::Enter({owner}, auriga::resource_admission::Operation::ItemUse);
    if (!commit || !permit || !permit->Current())
    {
        SendRefusal(owner, "A társ művelete nem engedélyezett.");
        return true;
    }

    const auto token = login->token;
    const auto fence = state.fence;
    auto held = std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
    g_registry.emplace_or_replace<ecs::TypedItemBusy>(owner);
    const bool queued = store::RequestCommit(fence.owner, fence.sessionGeneration, std::move(*commit),
        [pid, token, held](store::CommitOutcome outcome) {
            const auto owner = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
            const auto* login = g_registry.valid(owner) ? g_registry.try_get<ecs::TypedItemLogin>(owner) : nullptr;
            if (!login || login->token != token)
                return;
            if (!outcome.state)
            {
                LOG_ERROR("Typed companion owner reload failed pid {}", pid);
                if (auto* desc = ecs::PlayerRuntime::GetDesc(owner))
                    desc->SetPhase(PHASE_CLOSE);
                return;
            }
            g_registry.emplace_or_replace<ecs::TypedItems>(owner, std::move(*outcome.state));
            g_registry.remove<ecs::TypedItemBusy>(owner);
            if (!outcome.write)
                SendRefusal(owner, "A társ művelete nem menthető.");
            else
                SendTypedInventory(owner);
            PumpTypedItemGrants(owner);
        });
    if (!queued)
    {
        g_registry.remove<ecs::TypedItemBusy>(owner);
        SendRefusal(owner, "A tárgytároló nem elérhető.");
    }
    return true;
}
