#pragma once

#include <entt/entity/fwd.hpp>
#include <item/ItemMissions.hpp>
#include <functional>
#include "../quest/quest.h"

// Exchange quest materials, optional rewards and Yang in one owner commit.
// Recipe-style exchanges may opt in to costs from the special inventory.
std::expected<void, auriga::item::ItemMissionError> RequestTypedQuestItemExchange(entt::entity character,
	std::vector<auriga::item::MissionItemAmount> costs, std::vector<auriga::item::MissionItemAmount> rewards,
	std::uint64_t goldCost, std::function<void(bool written)> done,
	quest::TypedItemUseSource source = {}, std::uint64_t goldReward = 0, bool includeSpecialInventory = false);

std::expected<void, auriga::item::ItemMissionError> RequestTypedGameplayItemExchange(entt::entity character,
	std::vector<auriga::item::MissionItemAmount> costs, std::vector<auriga::item::MissionItemAmount> rewards,
	std::uint64_t price, std::function<void(bool written)> done,
	auriga::item::WalletCurrency currency = auriga::item::WalletCurrency::Gold);

// Guild economy operations attach a validated outcome to the same item write.
std::expected<void, auriga::item::ItemMissionError> RequestTypedGuildExchange(entt::entity character,
    std::vector<auriga::item::MissionItemAmount> costs, std::uint64_t price,
    std::uint16_t outcome, std::vector<std::uint8_t> receipt, std::function<void(bool)> done,
    std::optional<auriga::item::ItemInstanceId> source = {});
