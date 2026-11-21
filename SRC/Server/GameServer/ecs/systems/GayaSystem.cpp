#include "../../core/stdafx.h"

#include "GayaSystem.hpp"
#include "../../core/locale_service.h"
#include "../../core/JsonFile.h"
#include "../../core/utils.h"
#include "../../quest/questmanager.h"
#include "../../item/ItemCatalogRuntime.hpp"
#include "../../item/TypedQuestItems.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../Registry.hpp"
#include "../../network/desc.h"
#include <Headers/Header_GC.h>
#include <item/ItemWireCodec.hpp>
#include <item/TypedItemDetails.hpp>
#include <Core/Logging.hpp>

#include <algorithm>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace GayaSystem {

namespace {
namespace item = auriga::item;
constexpr std::uint64_t MarketPeriod = 5 * 60 * 60;
constexpr std::size_t OpenMarketSlots = 3;
struct TypedMarketOffer
{
    item::MissionItemAmount reward;
    std::uint32_t price;
};
struct TypedMarketState
{
    std::uint64_t session;
    std::uint64_t period;
    std::vector<TypedMarketOffer> offers;
};

// GC 206 v1: envelope, session, three offers in slot order, public item details.
std::vector<std::byte> EncodeMarket(const item::ItemCatalog& catalog, std::uint64_t session,
    std::span<const TypedMarketOffer, OpenMarketSlots> offers)
{
    std::vector<item::ItemGrant> rewards;
    rewards.reserve(offers.size());
    for (const auto& offer : offers)
        rewards.push_back({offer.reward.key, offer.reward.level, offer.reward.count});
    const auto details = item::EncodeTypedItemDetails(catalog, {}, false, rewards);
    if (!details) return {};

    constexpr std::size_t PrefixSize = 1 + 4 + 1 + 8 + OpenMarketSlots * (6 + 2 + 4 + 4);
    const auto size = PrefixSize + details->size();
    if (size > (1u << 20)) return {};
    std::vector<std::byte> packet;
    packet.reserve(size);
    const auto put = [&](std::uint64_t value, unsigned width) {
        for (unsigned i = 0; i < width; ++i)
            packet.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xff));
    };
    put(HEADER_GC_TYPED_GAYA_MARKET, 1);
    put(size, 4);
    put(1, 1);
    put(session, 8);
    for (const auto& offer : offers)
    {
        const auto key = item::EncodeItemKey(offer.reward.key);
        packet.insert(packet.end(), key.begin(), key.end());
        put(offer.reward.level, 2);
        put(offer.reward.count, 4);
        put(offer.price, 4);
    }
    packet.insert(packet.end(), details->begin(), details->end());
    return packet;
}

std::uint64_t CurrentMarketPeriod()
{
    return static_cast<std::uint64_t>(get_global_time()) / MarketPeriod;
}

bool MarketEnabled(entt::entity pc)
{
    return ecs::PlayerRuntime::IsValid(pc) && ecs::PlayerRuntime::IsPC(pc) &&
        quest::CQuestManager::instance().GetEventFlag("gaya_disable") != 1;
}
}

void OpenTypedMarket(entt::entity pc)
{
    if (!MarketEnabled(pc)) return;
    auto* desc = ecs::PlayerRuntime::GetDesc(pc);
    if (!desc || !desc->IsPhase(PHASE_GAME)) return;
    const auto catalog = item::runtime::CurrentCatalog();
    nlohmann::json config;
    const auto path = fmt::format("{}/gaya.json", LocaleService_GetBasePath());
    if (!catalog || !JsonFile::Load(path, config) || !config.contains("typed_market"))
    {
        ecs::ChatSystem::Send(pc, CHAT_TYPE_INFO, "A Gaya-piac jelenleg nem elérhető.");
        return;
    }
    std::vector<TypedMarketOffer> offers;
    try
    {
        const auto& market = config.at("typed_market");
        const auto& rows = market.at("offers");
        if (market.at("version") != 1 || !rows.is_array() || rows.size() < OpenMarketSlots || rows.size() > 128)
            throw std::runtime_error("invalid offer list");
        for (const auto& row : rows)
        {
            const auto key = item::ItemKey::Create(row.at("category").get<std::uint16_t>(), row.at("local").get<std::uint32_t>());
            const auto level = row.at("refinement").get<std::uint16_t>();
            const auto count = row.at("count").get<std::uint32_t>();
            const auto price = row.at("gaya").get<std::uint32_t>();
            const auto* definition = key ? catalog->Find(*key) : nullptr;
            if (!definition || definition->base.retired || !item::SupportsPersistentGrant(*catalog, *definition) ||
                !catalog->RefinedBaseStats(*key, level) || !count || count > definition->base.maxStack ||
                !price || price > item::MaxNativeGayaBalance)
                throw std::runtime_error("invalid typed offer");
            offers.push_back({{*key, level, count}, price});
        }
    }
    catch (const std::exception& error)
    {
        LOG_ERROR("Typed Gaya market: {}", error.what());
        ecs::ChatSystem::Send(pc, CHAT_TYPE_INFO, "A Gaya-piac kínálata jelenleg nem elérhető.");
        return;
    }

    const auto period = CurrentMarketPeriod();
    // Reopening cannot change the player's offers within the same rotation.
    std::seed_seq seed{ecs::PlayerRuntime::GetPlayerID(pc), static_cast<std::uint32_t>(period)};
    std::mt19937 random(seed);
    std::shuffle(offers.begin(), offers.end(), random);
    offers.erase(offers.begin() + OpenMarketSlots, offers.end());
    static std::uint64_t nextSession = 0;
    const auto session = ++nextSession;
    const auto packet = EncodeMarket(*catalog, session, std::span<const TypedMarketOffer, OpenMarketSlots>(offers));
    if (packet.empty())
    {
        LOG_ERROR("Typed Gaya market: could not encode offers");
        ecs::ChatSystem::Send(pc, CHAT_TYPE_INFO, "A Gaya-piac kínálata jelenleg nem elérhető.");
        return;
    }
    g_registry.emplace_or_replace<TypedMarketState>(pc, session, period, std::move(offers));
    desc->LargePacket(packet.data(), static_cast<int>(packet.size()));
}

void BuyTypedMarketOffer(entt::entity pc,std::uint64_t session,std::uint32_t slot)
{
    if (!MarketEnabled(pc)) return;
    const auto* state = g_registry.try_get<TypedMarketState>(pc);
    if (!state || session != state->session || state->period != CurrentMarketPeriod() || slot >= state->offers.size())
    {
        ecs::ChatSystem::Send(pc, CHAT_TYPE_INFO, "A Gaya-piac ajánlata lejárt. Nyisd meg újra a piacot.");
        return;
    }
    const auto offer = state->offers[slot];
    auto result = RequestTypedGameplayItemExchange(pc, {}, {offer.reward}, offer.price,
        [pc](bool written) {
            if (written) ecs::ChatSystem::Send(pc, CHAT_TYPE_INFO, "Sikeres vásárlás a Gaya-piacon.");
            else ecs::ChatSystem::Send(pc, CHAT_TYPE_INFO, "A Gaya-piac vásárlását nem sikerült menteni.");
        }, item::WalletCurrency::Gaya);
    if (!result)
    {
        using E = item::ItemMissionError;
        if (result.error() == E::NoMoney)
            ecs::ChatSystem::Send(pc, CHAT_TYPE_INFO, "Nincs elég Gayád a vásárláshoz.");
        else if (result.error() == E::NoRoom)
            ecs::ChatSystem::Send(pc, CHAT_TYPE_INFO, "Nincs elég szabad hely a leltárban.");
        else
            ecs::ChatSystem::Send(pc, CHAT_TYPE_INFO, "A Gaya-piac vásárlása most nem érhető el.");
    }
}

} // namespace GayaSystem
