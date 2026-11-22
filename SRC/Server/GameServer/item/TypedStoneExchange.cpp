#include "stdafx.h"
#include "TypedStoneExchange.hpp"

#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "TypedQuestItems.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../network/desc.h"
#include "../core/packet.h"

#include <item/ItemWireCodec.hpp>
#include <item/TypedItemDetails.hpp>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <span>
#include <vector>

namespace {
namespace items = auriga::item;
namespace runtime = items::runtime;
constexpr std::uint32_t MerchantRace = 9005;
constexpr std::uint32_t StonesPerGaya = 5;
constexpr std::uint32_t MaxExchange = 50000;
constexpr std::string_view RewardCode = "catalog.10.gaya_4dbbae9647e8";
static_assert(sizeof(TPacketCGTypedStoneExchange) == 19);

enum class Message : std::uint8_t { Open = 1, Stock = 2, Close = 3 };
enum class Action : std::uint8_t { One = 1, All = 2, Close = 3 };
enum class Status : std::uint8_t { Ready, Applied, NeedStones, Busy, NoRoom, Limit, Changed, Failed };

struct StoneMaterial
{
    items::ItemKey key;
    std::uint16_t level;
};

struct StoneExchangeSession
{
    entt::entity npc;
    std::uint64_t token;
    items::ItemKey reward;
    std::vector<StoneMaterial> materials;
    std::uint64_t shownRevision = 0;
    bool pending = false;
};

struct StoneStock
{
    std::vector<std::uint64_t> counts;
    std::uint32_t bulk = 0;
    bool limited = false;
};

class StoneExchangePacket
{
public:
    explicit StoneExchangePacket(Message message, std::uint64_t token)
    {
        Put(HEADER_GC_TYPED_STONE_EXCHANGE, 1);
        Put(0, 4);
        Put(1, 1);
        Put(static_cast<std::uint8_t>(message), 1);
        Put(token, 8);
    }

    void Put(std::uint64_t value, unsigned size)
    {
        for (unsigned i = 0; i < size; ++i)
            bytes.push_back(static_cast<std::byte>((value >> (8 * i)) & 255));
    }

    void Append(std::span<const std::byte> value)
    {
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    bool Send(entt::entity character)
    {
        auto* desc = ecs::PlayerRuntime::GetDesc(character);
        if (!desc || !desc->IsPhase(PHASE_GAME) || bytes.size() > (1u << 20))
            return false;
        for (unsigned i = 0; i < 4; ++i)
            bytes[i + 1] = static_cast<std::byte>((bytes.size() >> (8 * i)) & 255);
        desc->LargePacket(bytes.data(), static_cast<int>(bytes.size()));
        return true;
    }

private:
    std::vector<std::byte> bytes;
};

bool CanExchange(entt::entity character, entt::entity npc)
{
    if (npc == entt::null || !g_registry.valid(npc) || !ecs::PlayerRuntime::IsValid(npc) ||
        ecs::PlayerRuntime::GetRaceNum(npc) != MerchantRace ||
        ecs::PlayerRuntime::GetMapIndex(character) != ecs::PlayerRuntime::GetMapIndex(npc) ||
        CombatSystem::IsDead(character) || CombatSystem::IsStun(character) ||
        ecs::PlayerRuntime::IsObserverMode(character) || ecs::SocialSystem::HasExchange(character) ||
        ecs::SocialSystem::GetMyShop(character) != entt::null || ecs::SocialSystem::GetShopOwner(character) != entt::null ||
        ecs::SessionSystem::IsSafeboxOpen(character) || ecs::SessionSystem::IsCubeOpen(character))
        return false;

    const auto dx = std::abs(std::int64_t(ecs::PlayerRuntime::GetX(character)) - ecs::PlayerRuntime::GetX(npc));
    const auto dy = std::abs(std::int64_t(ecs::PlayerRuntime::GetY(character)) - ecs::PlayerRuntime::GetY(npc));
    return std::max(dx, dy) + std::min(dx, dy) / 2 < 1000;
}

const items::ItemCatalogEntry* FindReward(const items::ItemCatalog& catalog)
{
    const auto& entries = catalog.Entries();
    const auto found = std::ranges::find_if(entries, [](const auto& row) { return row.second.base.contentCode == RewardCode; });
    if (found == entries.end() || found->second.base.retired || !items::SupportsPersistentGrant(catalog, found->second))
        return nullptr;
    return &found->second;
}

bool HasMaterial(const items::ItemCatalog& catalog, items::ItemKey key, std::uint16_t level)
{
    const auto* entry = catalog.Find(key);
    return entry && !entry->base.retired && catalog.RefinedBaseStats(key, level).has_value();
}

std::vector<StoneMaterial> CollectMaterials(const items::ItemCatalog& catalog)
{
    constexpr std::array<std::pair<std::uint32_t, std::uint16_t>, 16> special{{
        {34, 0}, {35, 0}, {36, 0}, {204, 1},
        {205, 2}, {206, 2}, {207, 2}, {208, 2},
        {209, 3}, {210, 3}, {211, 3}, {212, 3},
        {213, 4}, {214, 4}, {215, 4}, {216, 4},
    }};
    std::vector<StoneMaterial> materials;
    const auto add = [&](std::uint32_t local, std::uint16_t level) {
        const auto key = items::ItemKey::Create(items::ItemCategory::SocketStone, local);
        if (key && HasMaterial(catalog, *key, level))
            materials.push_back({*key, level});
    };
    for (std::uint16_t level = 0; level < 5; ++level)
    {
        for (std::uint32_t local = 20; local < 34; ++local)
            add(local, level);
        if (level < 4)
            for (const auto& [local, grade] : std::span(special).subspan(level * 4, 4))
                add(local, grade);
    }
    return materials;
}

StoneStock CountStock(const StoneExchangeSession& session, const items::persistence::LoadedItemOwner& state)
{
    StoneStock stock;
    stock.counts.reserve(session.materials.size());
    std::uint64_t total = 0;
    for (const auto& row : session.materials)
    {
        const auto available = items::CountGameplayExchangeItems(state.items, state.fence.owner, row.key, row.level);
        stock.counts.push_back(available);
        const auto batches = available / StonesPerGaya;
        if (batches > MaxExchange / StonesPerGaya || total + batches > MaxExchange)
            stock.limited = true;
        total += batches;
    }
    if (!stock.limited)
        stock.bulk = static_cast<std::uint32_t>(total);
    return stock;
}

void CloseExchange(entt::entity character, std::uint64_t token)
{
    g_registry.remove<StoneExchangeSession>(character);
    StoneExchangePacket(Message::Close, token).Send(character);
}

bool SendStock(entt::entity character, StoneExchangeSession& session, Status status,
    std::uint32_t awarded = 0, const items::ItemCatalog* opening = nullptr)
{
    const auto* inventory = g_registry.try_get<ecs::TypedItems>(character);
    if (!inventory)
        return false;
    const auto& state = inventory->state;
    const auto stock = CountStock(session, state);
    StoneExchangePacket packet(opening ? Message::Open : Message::Stock, session.token);
    packet.Put(state.fence.revision, 8);
    packet.Put(static_cast<std::uint8_t>(status), 1);
    packet.Put(awarded, 4);
    packet.Put(stock.bulk, 4);
    packet.Put(stock.limited, 1);
    packet.Put(session.materials.size(), 1);
    if (opening)
    {
        packet.Append(items::EncodeItemKey(session.reward));
        packet.Put(StonesPerGaya, 2);
    }

    std::vector<items::ItemGrant> definitions;
    for (std::size_t i = 0; i < session.materials.size(); ++i)
    {
        const auto& row = session.materials[i];
        if (opening)
        {
            packet.Append(items::EncodeItemKey(row.key));
            packet.Put(row.level, 2);
            definitions.push_back({row.key, row.level, 1});
        }
        packet.Put(stock.counts[i], 8);
    }
    if (opening)
    {
        definitions.push_back({session.reward, 0, 1});
        const auto details = items::EncodeTypedItemDetails(*opening, {}, false, definitions);
        if (!details)
            return false;
        packet.Append(*details);
    }
    if (!packet.Send(character))
        return false;
    session.shownRevision = state.fence.revision;
    return true;
}

Status Refusal(items::ItemMissionError error)
{
    using E = items::ItemMissionError;
    switch (error)
    {
        case E::Requirements: return Status::NeedStones;
        case E::NoRoom: return Status::NoRoom;
        case E::Limit: return Status::Limit;
        default: return Status::Failed;
    }
}
}

bool TryOpenTypedStoneExchange(entt::entity character, entt::entity npc)
{
    if (!ecs::PlayerRuntime::IsPC(character) || npc == entt::null || !g_registry.valid(npc) ||
        ecs::PlayerRuntime::GetRaceNum(npc) != MerchantRace)
        return false;
    if (!CanExchange(character, npc))
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A kőbeváltáshoz menj közelebb a kereskedőhöz, és zárd be a többi műveletet.");
        return true;
    }
    if (g_registry.all_of<ecs::TypedItemBusy>(character))
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Az előző tárgyművelet még folyamatban van.");
        return true;
    }
    const auto catalog = runtime::CurrentCatalog();
    const auto* reward = catalog ? FindReward(*catalog) : nullptr;
    if (reward)
    {
        static std::uint64_t nextToken = 0;
        StoneExchangeSession next{npc, ++nextToken, reward->base.key, CollectMaterials(*catalog)};
        if (!next.materials.empty() && SendStock(character, next, Status::Ready, 0, catalog.get()))
        {
            g_registry.emplace_or_replace<StoneExchangeSession>(character, std::move(next));
            return true;
        }
    }
    ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A kőbeváltás most nem érhető el.");
    return true;
}

void RefreshTypedStoneExchange(entt::entity character)
{
    auto* session = g_registry.try_get<StoneExchangeSession>(character);
    if (!session || session->pending)
        return;
    if (!CanExchange(character, session->npc))
    {
        CloseExchange(character, session->token);
        return;
    }
    const auto* inventory = g_registry.try_get<ecs::TypedItems>(character);
    if (inventory && inventory->state.fence.revision != session->shownRevision)
        SendStock(character, *session, Status::Ready);
}

void HandleTypedStoneExchangePacket(entt::entity character, const char* data)
{
    if (!data || !g_registry.valid(character))
        return;
    TPacketCGTypedStoneExchange request{};
    std::memcpy(&request, data, sizeof(request));
    auto* session = g_registry.try_get<StoneExchangeSession>(character);
    if (!session || request.session != session->token)
        return;
    const auto action = static_cast<Action>(request.action);
    if (action == Action::Close)
    {
        if (request.revision == 0 && request.row == 0)
            CloseExchange(character, session->token);
        return;
    }
    if ((action != Action::One && action != Action::All) ||
        (action == Action::One && request.row >= session->materials.size()) ||
        (action == Action::All && request.row != 0))
        return;

    const auto* inventory = g_registry.try_get<ecs::TypedItems>(character);
    if (!inventory || !CanExchange(character, session->npc))
    {
        CloseExchange(character, session->token);
        return;
    }
    if (session->pending || g_registry.all_of<ecs::TypedItemBusy>(character))
    {
        SendStock(character, *session, Status::Busy);
        return;
    }
    if (request.revision != inventory->state.fence.revision)
    {
        SendStock(character, *session, Status::Changed);
        return;
    }
    const auto catalog = runtime::CurrentCatalog();
    const auto* reward = catalog ? FindReward(*catalog) : nullptr;
    if (!reward || reward->base.key != session->reward)
    {
        CloseExchange(character, session->token);
        return;
    }

    const auto stock = CountStock(*session, inventory->state);
    if (action == Action::All && stock.limited)
    {
        SendStock(character, *session, Status::Limit);
        return;
    }
    std::vector<items::MissionItemAmount> costs;
    std::uint32_t gain = 0;
    for (std::size_t i = 0; i < stock.counts.size(); ++i)
    {
        if (action == Action::One && i != request.row)
            continue;
        const auto availableBatches = stock.counts[i] / StonesPerGaya;
        if (!availableBatches)
            continue;
        const auto& row = session->materials[i];
        if (!HasMaterial(*catalog, row.key, row.level))
        {
            CloseExchange(character, session->token);
            return;
        }
        const auto batches = action == Action::One ? 1u : static_cast<std::uint32_t>(availableBatches);
        costs.push_back({row.key, row.level, batches * StonesPerGaya});
        gain += batches;
    }
    if (!gain)
    {
        SendStock(character, *session, Status::NeedStones);
        return;
    }

    session->pending = true;
    const auto token = session->token;
    const auto queued = RequestTypedGameplayItemExchange(character, std::move(costs), {{session->reward, 0, gain}}, 0,
        [character, token, gain](bool written) {
            auto* active = g_registry.try_get<StoneExchangeSession>(character);
            if (!active || active->token != token)
                return;
            active->pending = false;
            if (!CanExchange(character, active->npc))
                CloseExchange(character, token);
            else
                SendStock(character, *active, written ? Status::Applied : Status::Failed, written ? gain : 0);
        });
    if (!queued)
    {
        session->pending = false;
        SendStock(character, *session, Refusal(queued.error()));
    }
}
