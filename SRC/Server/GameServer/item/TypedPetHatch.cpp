#include "stdafx.h"
#include "TypedPetHatch.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
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
#include "../network/desc.h"
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
enum class Action : std::uint8_t { Preview = 1, Confirm, Close, Inspect };
enum class Status : std::uint8_t
{
    Closed, Ready, Pending, Hatched, Invalid, Denied, Busy, Stale, Failed,
    InsufficientGold, NoRoom, InvalidName, Details
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
    std::uint64_t cost = 0, std::optional<item::ItemKey> target = {}, const item::PersistentItem* seal = nullptr)
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
    packet.Put<std::uint8_t>(1);
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
        packet.Put(pet->life.remainingMs);
        packet.Put(pet->traits.maximumHpPercentTenths);
        packet.Put(pet->traits.monsterDamagePercentTenths);
        packet.Put(pet->traits.pveNormalDamagePercentTenths);
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
    if (!packet.itemId || action < Action::Preview || action > Action::Inspect
        || packet.nameLength > sizeof(packet.name)
        || (action != Action::Confirm && packet.nameLength)
        || (action == Action::Confirm && !packet.nameLength)
        || ((action == Action::Preview || action == Action::Inspect) && packet.token)
        || std::any_of(packet.name + packet.nameLength, std::end(packet.name), [](char c) { return c != 0; }))
    {
        Reply(entity, Status::Invalid, packet.itemId, packet.token);
        return true;
    }

    const auto catalog = store::CurrentCatalog();
    if (!catalog) return true;
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
            Reply(entity, Status::Closed, packet.itemId, packet.token);
        }
        return true;
    }

    const auto& typed = g_registry.get<ecs::TypedItems>(entity);
    const auto* source = InBag(typed, packet.itemId);
    if (action == Action::Inspect)
    {
        g_registry.remove<PetHatchSession>(entity);
        if (source && source->growthPet)
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
