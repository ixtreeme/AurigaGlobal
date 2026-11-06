#include "stdafx.h"
#include "TypedPetHatch.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "TypedCompanions.hpp"
#include "TypedDrops.hpp"
#include "TypedInventorySync.hpp"
#include "TypedRecovery.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../entity/char_manager.h"
#include "../entity/mob_manager.h"
#include "../network/desc.h"
#include "../pet/PetSystem.h"
#include "../quest/questpc.h"
#include "../social/banword.h"
#include "../core/packet.h"

#include <algorithm>
#include <openssl/rand.h>
#include <type_traits>

namespace
{
namespace item = auriga::item;
namespace store = item::runtime;
enum class Action : std::uint8_t { Preview = 1, Confirm, Close, Inspect, FoodList, FeedPreview, FeedConfirm, Progress, Evolve };
enum class Status : std::uint8_t
{
    Closed, Ready, Pending, Hatched, Invalid, Denied, Busy, Stale, Failed,
    InsufficientGold, NoRoom, InvalidName, Details, FoodList, FeedReady, Fed, NoEffect, Evolved
};

struct PetProgressView { std::uint64_t seal; };

struct FeedingView
{
    std::uint64_t food = 0;
    std::uint32_t units = 0;
    item::PetFoodKind kind{};
    std::uint64_t after = 0;
    std::vector<std::uint64_t> options;
};

struct PetFeedSession
{
    item::PersistentItem food;
    std::uint64_t seal = 0;
    item::ItemCatalogDigest catalogHash;
    item::PetFeedingService service{};
    item::GrowthPetProgress progress;
    std::uint64_t capacity = 0;
    bool revived = false;
    FeedingView view;
    std::uint64_t token = 0;
    std::uint32_t opened = 0;
    Status status = Status::FeedReady;
};

struct PetHatchSession
{
    item::PersistentItem egg;
    item::ItemKey target;
    item::ItemWallet wallet;
    item::ItemCatalogDigest catalogHash;
    std::uint64_t cost = 0;
    std::uint64_t token = 0;
    std::uint32_t opened = 0;
    std::uint64_t seal = 0;
    Status status = Status::Ready;
};

struct PacketWriter
{
    std::vector<std::uint8_t> bytes;
    template <typename T> void Put(T value)
    {
        static_assert(std::is_unsigned_v<T>);
        for (std::size_t i = 0; i < sizeof(T); ++i)
            bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    }
};

bool InGame(entt::entity entity)
{
    const auto* desc = g_registry.valid(entity) ? ecs::PlayerRuntime::GetDesc(entity) : nullptr;
    return desc && desc->IsPhase(PHASE_GAME) && ecs::PlayerRuntime::IsPC(entity);
}

bool Admitted(entt::entity entity)
{
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    const auto gm = ecs::PlayerRuntime::GetGMLevel(entity);
    if (gm > GM_PLAYER && gm < GM_IMPLEMENTOR) return false;
#endif
    const auto* quest = g_registry.try_get<ecs::QuestPCState>(entity);
    return quest && quest->pc && !quest->pc->IsRunning() && InventorySystem::CanHandleItems(entity)
        && !CombatSystem::IsDead(entity) && !CombatSystem::IsStun(entity)
        && !ecs::PlayerRuntime::IsObserverMode(entity) && !AffectSystem::IsPolymorphed(entity)
        && !ecs::SocialSystem::HasExchange(entity) && !ecs::SessionSystem::IsSafeboxOpen(entity)
        && !ecs::SessionSystem::IsCubeOpen(entity) && ecs::SocialSystem::GetMyShop(entity) == entt::null
        && ecs::SocialSystem::GetShopOwner(entity) == entt::null;
}

entt::entity CurrentLogin(std::uint32_t pid, std::uint64_t token)
{
    const auto entity = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
    const auto* login = g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItemLogin>(entity) : nullptr;
    return login && login->token == token ? entity : entt::null;
}

const item::PersistentItem* InBag(const ecs::TypedItems& typed, std::uint64_t id)
{
    const auto& state = typed.state;
    const auto found = std::ranges::find_if(state.items, [&](const auto& row) {
        return row.id.Value() == id && !row.destroyed && row.owner == state.fence.owner
            && row.placement && row.placement->kind == item::ItemStorageKind::Inventory;
    });
    return found != state.items.end() ? &*found : nullptr;
}

const item::ItemWallet* Gold(const ecs::TypedItems& typed)
{
    const auto& state = typed.state;
    const auto found = std::ranges::find_if(state.wallets, [&](const auto& wallet) {
        return wallet.owner == state.fence.owner && wallet.currency == item::WalletCurrency::Gold;
    });
    return found != state.wallets.end() ? &*found : nullptr;
}

const item::PetHatchProfile* HatchProfile(const item::ItemCatalog& catalog, item::ItemKey key)
{
    const auto* entry = catalog.Find(key);
    const auto* egg = entry && !entry->base.retired ? std::get_if<item::ItemContainerRow>(&entry->category) : nullptr;
    return egg && egg->kind == item::ContainerKind::PetEgg && egg->hatchProfile
        ? catalog.FindPetHatchProfile(*egg->hatchProfile) : nullptr;
}

void Reply(entt::entity entity, Status status, std::uint64_t id, std::uint64_t token = 0,
    std::uint64_t cost = 0, std::optional<item::ItemKey> target = {}, const item::PersistentItem* seal = nullptr,
    const FeedingView* feeding = nullptr, bool progression = false)
{
    if (!InGame(entity)) return;
    const auto* typed = g_registry.try_get<ecs::TypedItems>(entity);
    const auto* wallet = typed ? Gold(*typed) : nullptr;
    const auto catalog = store::CurrentCatalog();
    const auto* entry = target && catalog ? catalog->Find(*target) : nullptr;
    const std::string_view nameKey = entry && entry->base.nameKey.size() <= 256 ? std::string_view(entry->base.nameKey) : std::string_view{};
    const auto* pet = seal && seal->growthPet ? &*seal->growthPet : nullptr;

    PacketWriter packet;
    packet.Put<std::uint8_t>(HEADER_GC_TYPED_PET_HATCH);
    packet.Put<std::uint32_t>(0);
    packet.Put<std::uint8_t>(progression ? 3 : feeding ? 2 : 1);
    packet.Put<std::uint8_t>(static_cast<std::uint8_t>(status));
    packet.Put(id);
    packet.Put(token);
    packet.Put(cost);
    packet.Put<std::uint64_t>(wallet ? wallet->balance : 0);
    packet.Put<std::uint16_t>(target ? static_cast<std::uint16_t>(target->Category()) : 0);
    packet.Put<std::uint32_t>(target ? target->LocalId() : 0);
    packet.Put<std::uint16_t>(static_cast<std::uint16_t>(nameKey.size()));
    packet.bytes.insert(packet.bytes.end(), nameKey.begin(), nameKey.end());
    packet.Put<std::uint64_t>(pet ? seal->id.Value() : 0);
    if (pet)
    {
        packet.Put<std::uint8_t>(static_cast<std::uint8_t>(pet->name.size()));
        packet.bytes.insert(packet.bytes.end(), pet->name.begin(), pet->name.end());
        packet.Put(pet->bornAtUtcMs);
        packet.Put(pet->progress.level);
        packet.Put(pet->progress.stage);
        packet.Put<std::uint8_t>(static_cast<std::uint8_t>(pet->life.clock));
        packet.Put(pet->life.capacityMs);
        const bool projectTime = status == Status::Details || status == Status::FoodList || status == Status::FeedReady;
        const auto elapsed = projectTime && pet->life.clock == item::CompanionClock::ActiveTime
            ? TypedGrowthPetElapsed(entity,seal->id.Value()) : 0;
        packet.Put(pet->life.remainingMs - (std::min)(pet->life.remainingMs,elapsed));
        packet.Put(pet->traits.maximumHpPercentTenths);
        packet.Put(pet->traits.monsterDamagePercentTenths);
        packet.Put(pet->traits.pveNormalDamagePercentTenths);
        if (progression)
        {
            const auto& companion = std::get<item::ItemCompanionRow>(entry->category);
            const auto& profile = *catalog->FindGrowthPetProfile(*companion.growthProfile);
            const auto& level = profile.levels[pet->progress.level-1];
            const auto& stage = profile.stages[pet->progress.stage];
            const bool atGate = pet->progress.level == stage.maximumLevel && pet->progress.level < profile.maximumLevel;
            const bool following = PetSystem::LiveTypedSeal(entity)==seal->id.Value();
            const bool canEvolve = atGate && following && !entry->base.retired &&
                (pet->life.clock==item::CompanionClock::Permanent || elapsed<pet->life.remainingMs);
            packet.Put(pet->progress.mobExperience);
            packet.Put(level.mobExperience);
            packet.Put(pet->progress.itemExperience);
            packet.Put(level.itemExperience);
            packet.Put<std::uint8_t>((profile.experiencePolicy==item::GrowthExpPolicy::MobAndItem ? 1 : 0) |
                (atGate ? 2 : 0) | (canEvolve ? 4 : 0));
        }
    }
    if (feeding)
    {
        packet.Put(feeding->food);
        packet.Put(feeding->units);
        packet.Put(static_cast<std::uint8_t>(feeding->kind));
        packet.Put(feeding->after);
        packet.Put(static_cast<std::uint16_t>(feeding->options.size()));
        for (const auto food : feeding->options)
            packet.Put(food);
    }
    const auto length = static_cast<std::uint32_t>(packet.bytes.size());
    for (unsigned i = 0; i < 4; ++i)
        packet.bytes[1 + i] = static_cast<std::uint8_t>(length >> (i * 8));
    ecs::PlayerRuntime::GetDesc(entity)->LargePacket(packet.bytes.data(), static_cast<int>(length));
}

void ReplySession(entt::entity entity, const PetHatchSession& session, Status status)
{
    const auto* typed = g_registry.try_get<ecs::TypedItems>(entity);
    const auto* seal = status == Status::Hatched && typed ? InBag(*typed, session.seal) : nullptr;
    if (status == Status::Hatched && (!seal || !seal->growthPet))
        status = Status::Failed;
    Reply(entity, status, session.egg.id.Value(), session.token, session.cost, session.target, seal);
}

std::optional<std::uint16_t> Roll(item::PetBirthRange range)
{
    const auto choices = std::uint32_t(range.maximum - range.minimum) / range.step + 1;
    constexpr auto values = std::uint64_t(UINT32_MAX) + 1;
    const auto limit = values - values % choices;
    std::uint32_t draw;
    do
    {
        if (RAND_bytes(reinterpret_cast<unsigned char*>(&draw), sizeof(draw)) != 1) return {};
    } while (draw >= limit);
    return static_cast<std::uint16_t>(range.minimum + (draw % choices) * range.step);
}

Status Refusal(item::PetCareError error)
{
    switch (error)
    {
    case item::PetCareError::Forbidden: return Status::Denied;
    case item::PetCareError::InvalidName: return Status::InvalidName;
    case item::PetCareError::InsufficientGold: return Status::InsufficientGold;
    case item::PetCareError::NoEffect: return Status::NoEffect;
    default: return Status::Invalid;
    }
}

Status Refusal(item::ItemPersistenceError error)
{
    switch (error)
    {
    case item::ItemPersistenceError::InvalidPlacement: return Status::NoRoom;
    case item::ItemPersistenceError::Conflict:
    case item::ItemPersistenceError::InvalidFence:
    case item::ItemPersistenceError::CatalogMismatch: return Status::Stale;
    default: return Status::Failed;
    }
}

void Confirm(entt::entity entity, const TPacketCGTypedPetHatch& packet, PetHatchSession& session,
    const item::ItemCatalog& catalog)
{
    if (session.status == Status::Hatched || session.status == Status::Failed || session.status == Status::Pending)
    {
        ReplySession(entity, session, session.status);
        return;
    }
    if (!Admitted(entity))
    {
        ReplySession(entity, session, Status::Denied);
        return;
    }
    if (g_registry.all_of<ecs::TypedItemBusy>(entity))
    {
        ReplySession(entity, session, Status::Busy);
        return;
    }
    const auto& typed = g_registry.get<ecs::TypedItems>(entity);
    const auto* egg = InBag(typed, packet.itemId);
    const auto* wallet = Gold(typed);
    const auto* profile = egg ? HatchProfile(catalog, egg->key) : nullptr;
    if (get_dword_time() - session.opened >= 120000u || !egg || *egg != session.egg
        || !wallet || *wallet != session.wallet || !profile || session.catalogHash != catalog.ContentHash())
    {
        ReplySession(entity, session, Status::Stale);
        return;
    }
    if (wallet->balance < profile->goldCost)
    {
        ReplySession(entity, session, Status::InsufficientGold);
        return;
    }

    const std::string name(packet.name, packet.nameLength);
    if (!item::ValidPetName(name) || CBanwordManager::instance().CheckString(name.data(), name.size()))
    {
        ReplySession(entity, session, Status::InvalidName);
        return;
    }
    const auto ids = store::PeekInstanceIds(1);
    if (ids.empty())
    {
        ReplySession(entity, session, Status::Busy);
        return;
    }
    const auto hp = Roll(profile->maximumHp);
    const auto monster = Roll(profile->monsterDamage);
    const auto pve = Roll(profile->pveNormalDamage);
    item::PetHatchRequest request{{}, {egg->key}};
    if (!hp || !monster || !pve || RAND_bytes(request.context.operationId.data(), 16) != 1)
    {
        ReplySession(entity, session, Status::Failed);
        return;
    }

    const auto& owner = typed.state.fence;
    request.context.actorId = ecs::PlayerRuntime::GetPlayerID(entity);
    request.context.sessionGeneration = owner.sessionGeneration;
    request.context.rulesetRevision = 1;
    request.context.characterLevel = ecs::PointSystem::GetLevel(entity);
    request.context.characterClass = ecs::PlayerRuntime::GetJob(entity) + 1;
    request.context.sex = ecs::PlayerRuntime::GetSex(entity) + 1;
    request.context.admittedByServer = request.context.canUseItems = request.context.ownerAlive = true;
    request.context.inCombat = ecs::PlayerRuntime::GetPosition(entity) == POS_FIGHTING;
    request.egg.instanceId = egg->id.Value();
    request.egg.revision = egg->revision;
    request.egg.ownerId = egg->owner.id;
    request.egg.count = egg->count;
    request.egg.refinementLevel = egg->refinementLevel;
    request.walletRevision = wallet->revision;
    request.gold = wallet->balance;
    request.newInstanceId = ids.front().Value();
    request.newInstanceReservedByServer = request.nameAcceptedByServer = true;
    request.bornAtUtcMs = TypedItemEffectNow();
    request.name = name;
    request.recordedTraits = {*hp, *monster, *pve};

    auto commit = item::MakePetHatchCommit(catalog, owner, typed.state.storages, typed.state.items, typed.state.wallets, request);
    if (!commit)
    {
        ReplySession(entity, session, std::visit([](auto error) { return Refusal(error); }, commit.error()));
        return;
    }
    const auto pid = static_cast<std::uint32_t>(request.context.actorId);
    const auto login = g_registry.get<ecs::TypedItemLogin>(entity).token;
    const auto nonce = session.token;
    const auto fence = owner;
    auto permit = ecs::resource_admission::Enter({entity}, auriga::resource_admission::Operation::ItemUse);
    if (CurrentLogin(pid, login) != entity) return;
    auto* current = g_registry.try_get<PetHatchSession>(entity);
    const auto* currentItems = g_registry.try_get<ecs::TypedItems>(entity);
    if (!current || current->token != nonce) return;
    if (!permit || !permit->Current() || !Admitted(entity))
    {
        ReplySession(entity, *current, Status::Denied);
        return;
    }
    if (!currentItems || currentItems->state.fence != fence || g_registry.all_of<ecs::TypedItemBusy>(entity))
    {
        ReplySession(entity, *current, Status::Stale);
        return;
    }
    auto held = std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
    current->seal = request.newInstanceId;
    current->status = Status::Pending;
    g_registry.insert<ecs::TypedItemBusy>(&entity, &entity + 1);
    if (CurrentLogin(pid, login) != entity) return;
    current = g_registry.try_get<PetHatchSession>(entity);
    if (!current || current->token != nonce || !held->Current())
    {
        g_registry.remove<ecs::TypedItemBusy>(entity);
        if (current && current->token == nonce)
        {
            current->status = Status::Failed;
            ReplySession(entity, *current, current->status);
        }
        return;
    }
    store::SpendInstanceIds(1);
    const bool queued = store::RequestCommit(fence.owner, fence.sessionGeneration, std::move(*commit),
        [pid, login, nonce, held](store::CommitOutcome outcome) {
            const auto entity = CurrentLogin(pid, login);
            if (entity == entt::null) return;
            if (!outcome.state)
            {
                LOG_ERROR("Typed pet hatch owner reload failed pid {}", pid);
                if (auto* desc = ecs::PlayerRuntime::GetDesc(entity)) desc->SetPhase(PHASE_CLOSE);
                return;
            }
            g_registry.emplace_or_replace<ecs::TypedItems>(entity, std::move(*outcome.state));
            if (CurrentLogin(pid, login) != entity) return;
            g_registry.remove<ecs::TypedItemBusy>(entity);
            if (CurrentLogin(pid, login) != entity) return;
            SendTypedInventory(entity);
            if (auto* session = g_registry.try_get<PetHatchSession>(entity); session && session->token == nonce)
            {
                session->status = outcome.write ? Status::Hatched : Status::Failed;
                ReplySession(entity, *session, session->status);
            }
            PumpTypedItemGrants(entity);
        });
    if (!queued)
    {
        g_registry.remove<ecs::TypedItemBusy>(entity);
    }
    if (auto* active = CurrentLogin(pid, login) == entity ? g_registry.try_get<PetHatchSession>(entity) : nullptr;
        active && active->token == nonce)
    {
        if (!queued) active->status = Status::Failed;
        ReplySession(entity, *active, active->status);
    }
}

void FeedReply(entt::entity entity, Status status, std::uint64_t seal, std::uint64_t token = 0,
    const FeedingView& view = {}, const item::PersistentItem* pet = nullptr)
{
    Reply(entity,status,seal,token,0,pet ? std::optional{pet->key} : std::nullopt,pet,&view);
}

void ReplyFeedSession(entt::entity entity, const PetFeedSession& session, const item::PersistentItem* seal)
{
    const bool details = session.status == Status::FeedReady || session.status == Status::Fed;
    if (details && (!seal || !seal->growthPet))
        FeedReply(entity,Status::Failed,session.seal,session.token);
    else if (details)
        FeedReply(entity,session.status,session.seal,session.token,session.view,seal);
    else
        FeedReply(entity,session.status,session.seal,session.token);
}

bool HandleFeeding(entt::entity entity, Action action, const TPacketCGTypedPetHatch& packet,
    const item::ItemCatalog& catalog)
{
    using Service = item::PetFeedingService;
    auto* session = g_registry.try_get<PetFeedSession>(entity);
    const bool confirm = action == Action::FeedConfirm;
    const bool matching = confirm && session && session->token == packet.token && session->seal == packet.itemId;
    const auto& state = g_registry.get<ecs::TypedItems>(entity).state;
    const auto* seal = InBag(g_registry.get<ecs::TypedItems>(entity),packet.itemId);
    if (matching && session->status != Status::FeedReady)
    {
        ReplyFeedSession(entity,*session,seal);
        return true;
    }
    if (g_registry.all_of<ecs::TypedItemBusy>(entity))
    {
        FeedReply(entity,Status::Busy,packet.itemId,confirm ? packet.token : 0);
        return true;
    }
    if (confirm && !matching)
    {
        FeedReply(entity,Status::Stale,packet.itemId,packet.token);
        return true;
    }
    if (!confirm)
        g_registry.remove<PetFeedSession>(entity);
    if (!seal || !seal->growthPet || !Admitted(entity))
    {
        FeedReply(entity,Status::Denied,packet.itemId,confirm ? packet.token : 0);
        return true;
    }

    const auto live = RefreshTypedCompanion(entity);
    const auto service = live == packet.itemId ? Service::Cube : Service::Direct;
    if ((service == Service::Cube) != item::RecallRequested(*seal) || seal->growthPet->carrierInstanceId)
    {
        FeedReply(entity,Status::Denied,packet.itemId,confirm ? packet.token : 0);
        return true;
    }
    const auto& typed = g_registry.get<ecs::TypedItems>(entity);
    if (action == Action::FoodList)
    {
        FeedingView list;
        for (const auto& food : state.items)
        {
            if (food.destroyed || !food.placement || food.placement->kind != item::ItemStorageKind::Inventory ||
                food.owner != state.fence.owner || (food.boundTo && food.boundTo != food.owner) ||
                (food.automatic && food.automatic->active) || (food.toggle && food.toggle->active))
                continue;
            const auto* rule = catalog.FindPetFeedRule(food.key,service);
            if (rule && !catalog.Find(food.key)->base.retired &&
                (rule->consumption == item::PetFoodConsumption::WholeStack || food.count >= rule->units))
                list.options.push_back(food.id.Value());
        }
        FeedReply(entity,Status::FoodList,packet.itemId,0,list,seal);
        return true;
    }

    const auto* food = InBag(typed,confirm ? session->food.id.Value() : packet.token);
    if (confirm && (!food || *food != session->food || service != session->service ||
        session->catalogHash != catalog.ContentHash() || get_dword_time() - session->opened >= 120000u ||
        seal->growthPet->progress != session->progress || seal->growthPet->life.capacityMs != session->capacity))
    {
        session->status = Status::Stale;
        ReplyFeedSession(entity,*session,nullptr);
        return true;
    }
    const auto* rule = food ? catalog.FindPetFeedRule(food->key,service) : nullptr;
    if (!rule)
    {
        FeedReply(entity,Status::Invalid,packet.itemId,confirm ? packet.token : 0);
        return true;
    }
    const auto fence = state.fence;
    const auto pid = ecs::PlayerRuntime::GetPlayerID(entity);
    const auto login = g_registry.get<ecs::TypedItemLogin>(entity).token;
    item::PetCareContext context{{},pid,fence.sessionGeneration,1,static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(entity)),
        static_cast<std::uint8_t>(ecs::PlayerRuntime::GetJob(entity)+1),
        static_cast<std::uint8_t>(ecs::PlayerRuntime::GetSex(entity)+1),true,true,true,false,false,
        ecs::PlayerRuntime::GetPosition(entity)==POS_FIGHTING};
    if (RAND_bytes(context.operationId.data(),static_cast<int>(context.operationId.size())) != 1)
    {
        FeedReply(entity,Status::Failed,packet.itemId,confirm ? packet.token : 0);
        return true;
    }
    auto feed = item::MakePetFeedCommit(catalog,fence,state.storages,state.items,context,
        food->id,seal->id,service,TypedGrowthPetElapsed(entity,packet.itemId));
    if (!feed)
    {
        FeedReply(entity,std::visit([](auto error) { return Refusal(error); },feed.error()),
            packet.itemId,confirm ? packet.token : 0);
        return true;
    }
    const auto& plan = feed->plan;
    const FeedingView view{food->id.Value(),plan.consumeFoodUnits,rule->kind,
        plan.progress ? plan.progress->after.itemExperience : plan.after.remainingMs};
    if (!confirm)
    {
        PetFeedSession quote{*food,packet.itemId,catalog.ContentHash(),service,seal->growthPet->progress,
            seal->growthPet->life.capacityMs,plan.revived,view};
        if (RAND_bytes(reinterpret_cast<unsigned char*>(&quote.token),sizeof(quote.token)) != 1 || !quote.token)
            FeedReply(entity,Status::Failed,packet.itemId);
        else
        {
            quote.opened = get_dword_time();
            const auto& opened = g_registry.emplace<PetFeedSession>(entity,std::move(quote));
            ReplyFeedSession(entity,opened,seal);
        }
        return true;
    }
    if (view.units != session->view.units || plan.revived != session->revived)
    {
        session->status = Status::Stale;
        ReplyFeedSession(entity,*session,nullptr);
        return true;
    }

    const auto nonce = session->token;
    auto permit = ecs::resource_admission::Enter({entity},auriga::resource_admission::Operation::ItemUse);
    if (CurrentLogin(pid,login) != entity)
        return true;
    session = g_registry.try_get<PetFeedSession>(entity);
    if (!session || session->token != nonce)
        return true;
    if (!permit || !permit->Current() || !Admitted(entity) || g_registry.all_of<ecs::TypedItemBusy>(entity) ||
        g_registry.get<ecs::TypedItems>(entity).state.fence != fence)
    {
        FeedReply(entity,Status::Busy,packet.itemId,nonce);
        return true;
    }
    auto held = std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
    session->view = view;
    session->status = Status::Pending;
    g_registry.emplace_or_replace<ecs::TypedItemBusy>(entity);
    const bool queued = store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(feed->commit),
        [pid,login,nonce,held](store::CommitOutcome outcome) {
            const auto entity = CurrentLogin(pid,login);
            if (entity == entt::null) return;
            if (!outcome.state)
            {
                LOG_ERROR("Typed pet feed owner reload failed pid {}",pid);
                if (auto* desc = ecs::PlayerRuntime::GetDesc(entity)) desc->SetPhase(PHASE_CLOSE);
                return;
            }
            g_registry.emplace_or_replace<ecs::TypedItems>(entity,std::move(*outcome.state));
            g_registry.remove<ecs::TypedItemBusy>(entity);
            SendTypedInventory(entity);
            if (auto* session = g_registry.try_get<PetFeedSession>(entity); session && session->token == nonce)
            {
                session->status = outcome.write ? Status::Fed : Status::Failed;
                ReplyFeedSession(entity,*session,InBag(g_registry.get<ecs::TypedItems>(entity),session->seal));
                if (outcome.write)
                    SendTypedPetProgress(entity,session->seal);
            }
            PumpTypedItemGrants(entity);
        });
    if (!queued)
    {
        g_registry.remove<ecs::TypedItemBusy>(entity);
        session->status = Status::Failed;
    }
    ReplyFeedSession(entity,*session,nullptr);
    return true;
}

void ProgressReply(entt::entity entity,Status status,std::uint64_t id,const item::PersistentItem* seal=nullptr)
{
    Reply(entity,status,id,0,0,seal ? std::optional{seal->key} : std::nullopt,seal,nullptr,true);
}

void Evolve(entt::entity entity,const TPacketCGTypedPetHatch& packet,const item::ItemCatalog& catalog)
{
    if (g_registry.all_of<ecs::TypedItemBusy>(entity))
    {
        ProgressReply(entity,Status::Busy,packet.itemId);
        return;
    }
    const auto& typed = g_registry.get<ecs::TypedItems>(entity);
    const auto* seal = InBag(typed,packet.itemId);
    if (!seal || !seal->growthPet || !Admitted(entity) || RefreshTypedCompanion(entity)!=packet.itemId)
    {
        ProgressReply(entity,Status::Denied,packet.itemId);
        return;
    }
    const auto& pet = *seal->growthPet;
    if (packet.token!=std::uint64_t(pet.progress.stage)+1)
    {
        ProgressReply(entity,Status::Stale,packet.itemId);
        return;
    }
    const auto& row = std::get<item::ItemCompanionRow>(catalog.Find(seal->key)->category);
    const auto& profile = *catalog.FindGrowthPetProfile(*row.growthProfile);
    const auto stage = std::size_t(pet.progress.stage);
    if (stage+1>=profile.stages.size() || pet.progress.level!=profile.stages[stage].maximumLevel)
    {
        ProgressReply(entity,Status::NoEffect,packet.itemId);
        return;
    }
    const auto* form = catalog.FindCompanionForm(profile.stages[stage+1].form);
    if (!form || form->mobTemplateId>UINT16_MAX || !CMobManager::instance().Get(form->mobTemplateId))
    {
        ProgressReply(entity,Status::Denied,packet.itemId);
        return;
    }

    const auto fence = typed.state.fence;
    const auto pid = ecs::PlayerRuntime::GetPlayerID(entity);
    const auto login = g_registry.get<ecs::TypedItemLogin>(entity).token;
    item::PetCareContext context{{},pid,fence.sessionGeneration,1,static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(entity)),
        static_cast<std::uint8_t>(ecs::PlayerRuntime::GetJob(entity)+1),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetSex(entity)+1),
        true,true,true,false,false,ecs::PlayerRuntime::GetPosition(entity)==POS_FIGHTING};
    item::GrowthPetEvolution evolution{TypedItemEffectNow(),TypedGrowthPetElapsed(entity,packet.itemId)};
    const auto* training = row.trainingProfile ? catalog.FindPetTrainingProfile(*row.trainingProfile) : nullptr;
    if (training)
    {
        const auto growth = [&](std::uint16_t period) -> std::optional<std::uint16_t> {
            if (!period || (pet.progress.level+1)%period) return std::uint16_t{0};
            return Roll({training->evolutionMinimum,training->evolutionMaximum,1});
        };
        const auto hp = growth(training->hpEvolutionPeriod);
        const auto monster = growth(training->monsterEvolutionPeriod);
        const auto pve = growth(training->pveEvolutionPeriod);
        if (!hp || !monster || !pve)
        {
            ProgressReply(entity,Status::Failed,packet.itemId);
            return;
        }
        evolution.growth = {*hp,*monster,*pve};
    }
    if (RAND_bytes(context.operationId.data(),static_cast<int>(context.operationId.size()))!=1)
    {
        ProgressReply(entity,Status::Failed,packet.itemId);
        return;
    }
    auto commit = item::MakeGrowthPetProgressCommit(catalog,fence,typed.state.storages,typed.state.items,context,seal->id,{},evolution);
    auto permit = ecs::resource_admission::Enter({entity},auriga::resource_admission::Operation::ItemUse);
    if (CurrentLogin(pid,login)!=entity) return;
    const auto* current = g_registry.try_get<ecs::TypedItems>(entity);
    if (!commit || !permit || !permit->Current() || !Admitted(entity) || !current || current->state.fence!=fence ||
        PetSystem::LiveTypedSeal(entity)!=packet.itemId || g_registry.all_of<ecs::TypedItemBusy>(entity))
    {
        ProgressReply(entity,Status::Denied,packet.itemId);
        return;
    }
    auto held = std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
    g_registry.emplace_or_replace<ecs::TypedItemBusy>(entity);
    const bool queued = store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(*commit),
        [pid,login,id=std::uint64_t(packet.itemId),held](store::CommitOutcome outcome) {
            const auto entity = CurrentLogin(pid,login);
            if (entity==entt::null) return;
            if (!outcome.state)
            {
                LOG_ERROR("Typed pet evolution owner reload failed pid {}",pid);
                if (auto* desc = ecs::PlayerRuntime::GetDesc(entity)) desc->SetPhase(PHASE_CLOSE);
                return;
            }
            g_registry.emplace_or_replace<ecs::TypedItems>(entity,std::move(*outcome.state));
            g_registry.remove<ecs::TypedItemBusy>(entity);
            SendTypedInventory(entity);
            const auto* seal = InBag(g_registry.get<ecs::TypedItems>(entity),id);
            if (outcome.write && seal && seal->growthPet)
                ProgressReply(entity,Status::Evolved,id,seal);
            else
                ProgressReply(entity,Status::Failed,id);
            PumpTypedItemGrants(entity);
        });
    if (!queued) g_registry.remove<ecs::TypedItemBusy>(entity);
    ProgressReply(entity,queued ? Status::Pending : Status::Failed,packet.itemId);
}
}

void SendTypedPetProgress(entt::entity entity,std::uint64_t id)
{
    if (!InGame(entity)) return;
    const auto* view = g_registry.try_get<PetProgressView>(entity);
    const auto* typed = g_registry.try_get<ecs::TypedItems>(entity);
    const auto* seal = view && view->seal==id && typed ? InBag(*typed,id) : nullptr;
    if (seal && seal->growthPet)
        ProgressReply(entity,Status::Details,id,seal);
}

bool HandleTypedPetHatchPacket(entt::entity entity, const char* data)
{
    static_assert(sizeof(TPacketCGTypedPetHatch) == 83);
    static_assert(HEADER_CG_TYPED_PET_HATCH != HEADER_CG_CLIENT_VERSION);
    if (!InGame(entity) || !g_registry.all_of<ecs::TypedItems, ecs::TypedItemLogin>(entity))
        return true;
    TPacketCGTypedPetHatch packet{};
    std::memcpy(&packet, data, sizeof(packet));
    const auto action = static_cast<Action>(packet.action);
    if (!packet.itemId || action < Action::Preview || action > Action::Evolve
        || packet.nameLength > sizeof(packet.name)
        || (action != Action::Confirm && packet.nameLength)
        || (action == Action::Confirm && !packet.nameLength)
        || ((action == Action::Preview || action == Action::Inspect || action == Action::FoodList || action == Action::Progress) && packet.token)
        || ((action == Action::FeedPreview || action == Action::FeedConfirm) && !packet.token)
        || (action == Action::Evolve && (!packet.token || packet.token>16))
        || std::any_of(packet.name + packet.nameLength, std::end(packet.name), [](char c) { return c != 0; }))
    {
        Reply(entity, Status::Invalid, packet.itemId, packet.token);
        return true;
    }

    const auto catalog = store::CurrentCatalog();
    if (!catalog) return true;
    if (action >= Action::FoodList && action <= Action::FeedConfirm)
        return HandleFeeding(entity,action,packet,*catalog);
    if (action == Action::Evolve)
    {
        Evolve(entity,packet,*catalog);
        return true;
    }
    auto* session = g_registry.try_get<PetHatchSession>(entity);
    if (session && session->status == Status::Pending && action != Action::Confirm)
    {
        Reply(entity, Status::Busy, packet.itemId, packet.token);
        return true;
    }
    if (action == Action::Confirm)
    {
        if (!session || session->token != packet.token || session->egg.id.Value() != packet.itemId)
            Reply(entity, Status::Stale, packet.itemId, packet.token);
        else
            Confirm(entity, packet, *session, *catalog);
        return true;
    }
    if (action == Action::Close)
    {
        if (session ? session->token != packet.token || session->egg.id.Value() != packet.itemId : packet.token != 0)
            Reply(entity, Status::Stale, packet.itemId, packet.token);
        else
        {
            g_registry.remove<PetHatchSession>(entity);
            if (const auto* view = g_registry.try_get<PetProgressView>(entity); view && view->seal==packet.itemId)
                g_registry.remove<PetProgressView>(entity);
            Reply(entity, Status::Closed, packet.itemId, packet.token);
        }
        return true;
    }

    const auto& typed = g_registry.get<ecs::TypedItems>(entity);
    const auto* source = InBag(typed, packet.itemId);
    if (action == Action::Inspect || action == Action::Progress)
    {
        g_registry.remove<PetHatchSession>(entity);
        if (action == Action::Progress)
        {
            if (source && source->growthPet)
            {
                g_registry.emplace_or_replace<PetProgressView>(entity,PetProgressView{packet.itemId});
                ProgressReply(entity,Status::Details,packet.itemId,source);
            }
            else
                ProgressReply(entity,Status::Invalid,packet.itemId);
        }
        else if (source && source->growthPet)
            Reply(entity, Status::Details, packet.itemId, 0, 0, source->key, source);
        else
            Reply(entity, Status::Invalid, packet.itemId);
        return true;
    }
    if (!Admitted(entity) || g_registry.all_of<ecs::TypedItemBusy>(entity))
    {
        Reply(entity, Status::Denied, packet.itemId);
        return true;
    }
    const auto* wallet = Gold(typed);
    const auto* profile = source ? HatchProfile(*catalog, source->key) : nullptr;
    const auto* target = profile ? catalog->Find(profile->target) : nullptr;
    if (!wallet || !profile || !target || target->base.retired || !item::PlainInventoryItem(*source, typed.state.fence.owner))
    {
        g_registry.remove<PetHatchSession>(entity);
        Reply(entity, Status::Invalid, packet.itemId);
        return true;
    }

    PetHatchSession next{*source, profile->target, *wallet, catalog->ContentHash(), profile->goldCost};
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&next.token), sizeof(next.token)) != 1 || !next.token)
    {
        Reply(entity, Status::Failed, packet.itemId);
        return true;
    }
    next.opened = get_dword_time();
    next.status = wallet->balance < profile->goldCost ? Status::InsufficientGold : Status::Ready;
    const auto& opened = g_registry.emplace_or_replace<PetHatchSession>(entity, std::move(next));
    ReplySession(entity, opened, opened.status);
    return true;
}
