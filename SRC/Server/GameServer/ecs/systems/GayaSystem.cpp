#include "../../core/stdafx.h"

#include "GayaSystem.hpp"
#include "../../entity/char.h"
#include "../../core/config.h"
#include "../../core/locale_service.h"
#include "../../core/JsonFile.h"
#include "../../quest/questmanager.h"
#include "../../item/ItemCatalogRuntime.hpp"
#include "../../item/TypedQuestItems.hpp"
#include "ItemSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "PointSystem.hpp"
#include "../Registry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include <Core/Logging.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <vector>

namespace
{

struct GayaLoadEntry
{
	uint32_t glimmerstone { 0 };
	uint32_t glimmerstone_count { 0 };
	uint32_t grade_stone { 0 };
	uint32_t give_gaya { 0 };
	uint32_t prob_gaya { 0 };
	uint32_t cost_gaya_yang { 0 };
};

struct GayaRuntimeState
{
	GayaLoadEntry config;
};

GayaRuntimeState* GetGayaRuntime(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;

	return &g_registry.get_or_emplace<GayaRuntimeState>(e);
}

void MarkDirty(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e))
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

} // namespace

namespace GayaSystem {

namespace {
namespace item = auriga::item;
constexpr std::uint64_t MarketPeriod = 5 * 60 * 60;
constexpr std::size_t OpenMarketSlots = 3;
struct TypedMarketOffer
{
    item::MissionItemAmount reward;
    std::uint32_t price;
    std::uint32_t iconId;
};
struct TypedMarketState
{
    std::uint64_t session;
    std::uint64_t period;
    std::vector<TypedMarketOffer> offers;
};

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
            const auto icon = row.at("icon").get<std::uint32_t>();
            const auto* definition = key ? catalog->Find(*key) : nullptr;
            if (!definition || definition->base.retired || !item::SupportsPersistentGrant(*catalog, *definition) ||
                !catalog->RefinedBaseStats(*key, level) || !count || count > definition->base.maxStack ||
                !price || price > item::MaxNativeGayaBalance || !icon)
                throw std::runtime_error("invalid typed offer");
            offers.push_back({{*key, level, count}, price, icon});
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
    auto& state = g_registry.emplace_or_replace<TypedMarketState>(pc, session, period, std::move(offers));
    ecs::ChatSystem::Send(pc, CHAT_TYPE_COMMAND, "GayaTypedMarketBegin %llu", static_cast<unsigned long long>(session));
    for (std::size_t i = 0; i < state.offers.size(); ++i)
    {
        const auto& offer = state.offers[i];
        ecs::ChatSystem::Send(pc, CHAT_TYPE_COMMAND, "GayaTypedMarketOffer %llu %u %u %u %u",
            static_cast<unsigned long long>(session), static_cast<unsigned>(i), offer.iconId, offer.reward.count, offer.price);
    }
    ecs::ChatSystem::Send(pc, CHAT_TYPE_COMMAND, "GayaTypedMarketOpen %llu", static_cast<unsigned long long>(session));
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

void Load(entt::entity pc)
{
	auto* state = GetGayaRuntime(pc);
	if (!state)
		return;

	char file_name[256 + 1];

	state->config = {};

	snprintf(file_name, sizeof(file_name), "%s/gaya.json", LocaleService_GetBasePath().c_str());

	nlohmann::json doc;

	if (!JsonFile::Load(file_name, doc))
	{
		LOG_ERROR("Gaya: Error, it's not possible load {} !", file_name);
		return;
	}

	if (doc.contains("config") && doc.at("config").is_object())
	{
		const auto& config = doc.at("config");
		state->config.glimmerstone = config.value("glimmerstone", 0);
		state->config.glimmerstone_count = config.value("glimmerstone_count", 0);
		state->config.grade_stone = config.value("grade_stone", 0);
		state->config.give_gaya = config.value("give_gaya", 0);
		state->config.prob_gaya = config.value("prob_gaya", 0);
		state->config.cost_gaya_yang = config.value("cost_gaya_yang", 0);
	}

	MarkDirty(pc);
}

void CraftItems(entt::entity pc, int slot)
{
	auto* state = GetGayaRuntime(pc);
	if (!state)
		return;


#ifdef ENABLE_EXTRA_INVENTORY
	const entt::entity item = ItemSystem::GetItem(pc, TItemPos(EXTRA_INVENTORY, slot));
#else
	const entt::entity item = ItemSystem::GetItem(pc, TItemPos(INVENTORY, slot));
#endif
	if (!ItemSystem::IsValidItem(item)) {
		return;
	}

	const uint32_t glimmerstoneVnum = state->config.glimmerstone;
	const uint32_t glimmerstoneCount = state->config.glimmerstone_count;
	const int gradeStone = state->config.grade_stone;
	const int pointGaya = state->config.give_gaya;
	const int randomPointGaya = number(1, 100);
	const int probabilityGaya = state->config.prob_gaya;
	const int costYang = state->config.cost_gaya_yang;

	if (ItemSystem::GetItemType(item) != ITEM_METIN ||
		ItemSystem::GetItemRefineLevel(item) > gradeStone) {
		return;
	}

	if (ItemSystem::CountItem(pc, glimmerstoneVnum) < static_cast<int>(glimmerstoneCount))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(pc, CHAT_TYPE_INFO, 525, "%d#%s", glimmerstoneCount,
			ItemSystem::GetItemNameByVnum(glimmerstoneVnum));
#endif
		return;
	}

	if (ecs::PointSystem::GetGold(pc) < costYang) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(pc, CHAT_TYPE_INFO, 232, "");
#endif
		return;
	}

	if (randomPointGaya <= probabilityGaya) {
#ifdef ENABLE_RANKING
		ecs::PlayerRuntime::SetRankPoints(
			pc, 11, ecs::PlayerRuntime::GetRankPoints(pc, 11) + pointGaya);
#endif
		ecs::PointSystem::Change(pc, POINT_GAYA, pointGaya);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(pc, CHAT_TYPE_INFO, 526, "");
#endif
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(pc, CHAT_TYPE_INFO, 527, "");
	}
#endif

	ItemSystem::RemoveSpecifyItemEcs(pc, glimmerstoneVnum, glimmerstoneCount);
	ecs::PointSystem::Change(pc, POINT_GOLD, -costYang);
	ItemSystem::ConsumeItemEcs(item);
	ecs::ChatSystem::Send(pc, CHAT_TYPE_COMMAND, "GayaCheck");
	MarkDirty(pc);
}

} // namespace GayaSystem
