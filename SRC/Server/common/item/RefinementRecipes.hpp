#pragma once
#include "ItemKey.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace auriga::item
{
class ItemCatalog;
inline constexpr std::size_t MaxRefinementIngredients = 32;
inline constexpr std::size_t MaxRefinementRecipes = 100'000;
inline constexpr std::size_t MaxRefinementInventory = 1024;
enum class RefinementFailure : std::uint8_t { Keep = 1, Destroy = 2, SetLevel = 3 };
// A recipe selects one exact source state. Different smiths/scrolls can offer
// different recipes; admission to that service belongs to the owner thread.
struct RefinementIngredient
{
    ItemKey key;
    std::uint16_t refinementLevel = 0;
    std::uint32_t count = 0;
};
struct RefinementRecipe
{
    std::uint32_t id = 0;
    std::string code;
    ItemKey source;
    std::uint16_t sourceLevel = 0;
    ItemKey target;
    std::uint16_t targetLevel = 0;
    std::uint64_t goldCost = 0;
    std::uint16_t successChanceBp = 0;
    RefinementFailure failure = RefinementFailure::Keep;
    std::optional<std::uint16_t> failureLevel;
    std::vector<RefinementIngredient> ingredients;
};
enum class RefinementRecipeError
{
    InvalidIdentity, MissingItem, RetiredItem, InvalidLevel, InvalidTransition,
    InvalidCost, InvalidChance, InvalidFailure, TooManyIngredients,
    InvalidIngredient, DuplicateIngredient,
};
// Uses the new catalog only. No old item IDs, recipes, value slots or registry.
std::expected<void, RefinementRecipeError> ValidateRefinementRecipe(const RefinementRecipe&, const ItemCatalog&);

// Values copied on the owner thread. Instance ID is a persistent identity, not
// a VID or ECS handle. Count/revision/owner are checked again by the DB writer.
struct RefinementItemSnapshot
{
    std::uint64_t instanceId = 0, revision = 0, ownerId = 0;
    ItemKey key;
    std::uint16_t refinementLevel = 0;
    std::uint32_t count = 0;
    bool locked = false;
};
struct RefinementUseContext
{
    std::array<std::uint8_t,16> operationId {};
    std::uint64_t actorId = 0, walletRevision = 0, gold = 0;
    // Must be explicitly established by server service/distance/state rules.
    bool allowedByRules = false;
};
struct RefinementDebit
{
    std::uint64_t instanceId = 0, expectedRevision = 0;
    std::uint32_t expectedCount = 0, consumeCount = 0;
};
struct RefinementPlan
{
    std::array<std::uint8_t,16> operationId {};
    std::uint64_t catalogRevision = 0;
    std::array<std::uint8_t,32> catalogHash {};
    std::uint32_t recipeId = 0;
    std::uint64_t actorId = 0, sourceInstanceId = 0, expectedSourceRevision = 0;
    std::uint64_t expectedWalletRevision = 0, expectedGold = 0, goldCost = 0;
    ItemKey source;
    std::uint16_t sourceLevel = 0;
    std::optional<ItemKey> result;
    std::optional<std::uint16_t> resultLevel;
    bool success = false;
    std::uint16_t recordedRoll = 0;
    std::vector<RefinementDebit> debits;
};
enum class RefinementUseError
{
    Forbidden, InvalidContext, MissingRecipe, InvalidSource, InvalidInventory,
    InsufficientGold, InsufficientMaterials, InvalidDecision,
};
// A complete owning proposal, NOT a resource mutation. The operation receipt,
// source revision, wallet revision and ALL material revisions must commit in
// one transaction before changing ECS, stats or sending success to a client.
// No RNG is sampled here. For 0/100% chance roll must be 0; otherwise 1..10000.
std::expected<RefinementPlan, RefinementUseError> PrepareRefinement(
    const ItemCatalog&, std::uint32_t recipeId, const RefinementUseContext&,
    const RefinementItemSnapshot& source, std::span<const RefinementItemSnapshot> inventory,
    std::uint16_t recordedRoll);
} // namespace auriga::item
