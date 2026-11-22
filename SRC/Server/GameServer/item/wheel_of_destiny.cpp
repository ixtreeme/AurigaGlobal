#include "stdafx.h"
#include "wheel_of_destiny.h"
#include "TypedQuestItems.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../network/desc.h"
#include "utils.h"

#include <Headers/Header_GC.h>
#include <item/ItemWireCodec.hpp>
#include <item/TypedItemDetails.hpp>

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
namespace {
namespace item = auriga::item;
constexpr std::uint32_t WheelPrice = 1;
constexpr std::string_view TicketCode = "wheel.fortune_ticket";

struct Prize {
    std::string_view code;
    std::uint32_t count;
    unsigned minimumChance;
};

// The table order is also the position on the wheel.
constexpr Prize prizes[] = {
    {"catalog.10.gaya_4dbbae9647e8", 10, 0},
    {"catalog.16.voucher_100_dc_03622683353d", 10, 25},
    {"catalog.22.mystic_blessing_scroll_b8dbc49e4791", 1, 0},
    {"catalog.16.voucher_1000_dc_e316f2658298", 1, 0},
    {"catalog.10.dungeon_point_10af51240afa", 200, 0},
    {"catalog.22.mystic_blessing_scroll_b8dbc49e4791", 3, 0},
    {TicketCode, 3, 0},
    {"catalog.5.golden_fruit_31fe36387997", 10, 0},
    {"catalog.15.hunting_dog_summoner_273f32f08a28", 1, 25},
    {"catalog.5.legendary_bonus_043af960ba20", 1, 0},
    {"catalog.5.legendary_changer_8e172122f0ec", 1, 0},
    {"catalog.16.gold_ingot_400_million_101fc6bbeec7", 5, 0},
    {"catalog.16.voucher_500_dc_c5cab7c40efd", 1, 0},
    {"catalog.5.fruit_of_life_135bea6f7abb", 5, 0},
    {"material.sarkany_karom", 5, 0},
    {"material.sarkany_pikkely", 5, 0},
};
static_assert(std::size(prizes) == CWheelDestiny::SlotCount);

// GC 207 v1. Open carries the full board; a successful turn selects one of its slots.
enum class WheelMessage : std::uint8_t { Open = 1, Turn = 2, Close = 3 };

class WheelPacket {
public:
    explicit WheelPacket(WheelMessage message)
    {
        Put(HEADER_GC_TYPED_WHEEL, 1);
        Put(0, 4);
        Put(1, 1);
        Put(static_cast<std::uint8_t>(message), 1);
    }

    void Put(std::uint64_t value, unsigned width)
    {
        for (unsigned i = 0; i < width; ++i)
            bytes.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xff));
    }

    void Append(std::span<const std::byte> payload)
    {
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    }

    bool Send(entt::entity owner)
    {
        auto* desc = ecs::PlayerRuntime::GetDesc(owner);
        if (!desc || !desc->IsPhase(PHASE_GAME) || bytes.size() > (1u << 20))
            return false;
        for (unsigned i = 0; i < 4; ++i)
            bytes[1 + i] = static_cast<std::byte>((bytes.size() >> (8 * i)) & 0xff);
        desc->LargePacket(bytes.data(), static_cast<int>(bytes.size()));
        return true;
    }

private:
    std::vector<std::byte> bytes;
};

const item::ItemCatalogEntry* Resolve(const item::ItemCatalog& catalog, std::string_view code)
{
    const auto& entries = catalog.Entries();
    const auto at = std::ranges::find_if(entries, [&](const auto& entry) { return entry.second.base.contentCode == code; });
    return at != entries.end() && !at->second.base.retired ? &at->second : nullptr;
}

std::uint32_t FreeSpins(entt::entity owner)
{
    return static_cast<std::uint32_t>(std::max(0, ecs::PlayerRuntime::GetWheelFreeCount(owner)));
}
}

bool CWheelDestiny::Open()
{
    if (!CanClose())
        return false;
    const auto catalog = item::runtime::CurrentCatalog();
    if (!catalog)
        return false;

    WheelPacket packet(WheelMessage::Open);
    packet.Put(WheelPrice, 4);
    packet.Put(FreeSpins(owner), 4);
    decltype(slots) next;
    std::vector<item::ItemGrant> rewards;
    rewards.reserve(SlotCount);
    for (std::size_t i = 0; i < SlotCount; ++i)
    {
        const auto& prize = prizes[i];
        const auto* definition = Resolve(*catalog, prize.code);
        if (!definition || !catalog->RefinedBaseStats(definition->base.key, 0) ||
            !item::SupportsPersistentGrant(*catalog, *definition))
        {
            packet.Put(0, 6);
            packet.Put(0, 4);
            continue;
        }
        next[i] = definition->base.key;
        packet.Append(item::EncodeItemKey(*next[i]));
        packet.Put(prize.count, 4);
        rewards.push_back({*next[i], 0, prize.count});
    }
    if (rewards.empty())
        return false;
    const auto details = item::EncodeTypedItemDetails(*catalog, {}, false, rewards);
    if (!details)
        return false;
    packet.Append(*details);
    if (!packet.Send(owner))
        return false;
    slots = std::move(next);
    return true;
}

void CWheelDestiny::Close()
{
    WheelPacket(WheelMessage::Close).Send(owner);
}

void CWheelDestiny::TurnWheel()
{
    if (!CanClose())
        return;
    const auto catalog = item::runtime::CurrentCatalog();
    const auto* ticket = catalog ? Resolve(*catalog, TicketCode) : nullptr;
    if (!ticket)
    {
        ecs::ChatSystem::Send(owner, CHAT_TYPE_INFO, "A Szerencsekerék jelenleg nem érhető el.");
        return;
    }

    const unsigned chance = turns >= 40 ? 255 : turns >= 25 ? 50 : turns >= 10 ? 25 : 0;
    std::vector<std::size_t> eligible;
    for (std::size_t i = 0; i < SlotCount; ++i)
    {
        if (!slots[i] || prizes[i].minimumChance > chance)
            continue;
        const auto* definition = catalog->Find(*slots[i]);
        if (definition && !definition->base.retired && definition->base.contentCode == prizes[i].code &&
            catalog->RefinedBaseStats(*slots[i], 0) && item::SupportsPersistentGrant(*catalog, *definition))
            eligible.push_back(i);
    }
    if (eligible.empty())
    {
        ecs::ChatSystem::Send(owner, CHAT_TYPE_INFO, "A Szerencsekerék jutalmai jelenleg nem érhetők el.");
        return;
    }
    const auto index = eligible[number(0, static_cast<int>(eligible.size()) - 1)];
    const bool free = FreeSpins(owner) > 0;

    pending = true;
    const std::weak_ptr<CWheelDestiny> wheel = ecs::PlayerRuntime::GetWheelDestiny(owner);
    const auto queued = RequestTypedGameplayItemExchange(owner, {{ticket->base.key, 0, 1}},
        {{*slots[index], 0, prizes[index].count}}, free ? 0 : WheelPrice,
        [wheel, free, index](bool written) {
            const auto session = wheel.lock();
            if (!session)
                return;
            session->pending = false;
            if (!written)
            {
                ecs::ChatSystem::Send(session->owner, CHAT_TYPE_INFO, "A pörgetés sikertelen. A jegy és a Yang visszamaradt.");
                return;
            }
            if (free && FreeSpins(session->owner) > 0)
                ecs::PlayerRuntime::SetWheelFreeCount(session->owner, FreeSpins(session->owner) - 1);
            if (session->turns < 40)
                ++session->turns;
            WheelPacket packet(WheelMessage::Turn);
            packet.Put(index, 1);
            packet.Put(number(1, 8), 1);
            packet.Put(FreeSpins(session->owner), 4);
            session->showingResult = packet.Send(session->owner);
        });
    if (!queued)
    {
        pending = false;
        ecs::ChatSystem::Send(owner, CHAT_TYPE_INFO, "A pörgetéshez ellenőrizd a jegyet, a Yangot és a szabad leltárhelyet.");
    }
}
#endif
