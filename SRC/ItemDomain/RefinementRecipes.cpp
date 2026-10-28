#include <item/RefinementRecipes.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"
#include <algorithm>
#include <limits>
#include <set>

namespace auriga::item
{
namespace
{
constexpr auto MaxGold = static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)());
bool LevelExists(const ItemCatalog& catalog, ItemKey key, std::uint16_t level)
{
    const auto* item = catalog.Find(key);
    if (!item) return false;
    const auto* profile = catalog.FindRefinement(item->base.refinementProfile);
    return profile && profile->FindLevel(level);
}
bool RefinableInstance(const ItemCatalogEntry& item)
{
    const auto* stone=std::get_if<ItemSocketStoneRow>(&item.category);
    if(std::holds_alternative<ItemToolRow>(item.category))return false; // Dedicated training/refinement path; never bypass its required points.
    return item.base.maxStack==1 || (stone && stone->kind==SocketStoneKind::Insertable);
}
}
std::expected<void, RefinementRecipeError> ValidateRefinementRecipe(const RefinementRecipe& recipe, const ItemCatalog& catalog)
{
    if (!recipe.id || !detail::ContentCode(recipe.code)) return std::unexpected(RefinementRecipeError::InvalidIdentity);
    const auto* source = catalog.Find(recipe.source);
    const auto* target = catalog.Find(recipe.target);
    if (!source || !target) return std::unexpected(RefinementRecipeError::MissingItem);
    if (source->base.retired || target->base.retired) return std::unexpected(RefinementRecipeError::RetiredItem);
    // A stackable stone can be refined only as a concrete count-one instance.
    // PrepareRefinement still rejects count>1; no entire stack is upgraded.
    if (!RefinableInstance(*source) || !RefinableInstance(*target)) return std::unexpected(RefinementRecipeError::InvalidTransition);
    if (!LevelExists(catalog, recipe.source, recipe.sourceLevel) || !LevelExists(catalog, recipe.target, recipe.targetLevel))
        return std::unexpected(RefinementRecipeError::InvalidLevel);
    if (recipe.source == recipe.target && recipe.targetLevel != recipe.sourceLevel + 1u)
        return std::unexpected(RefinementRecipeError::InvalidTransition);
    if (recipe.goldCost > MaxGold) return std::unexpected(RefinementRecipeError::InvalidCost);
    if (recipe.successChanceBp > 10000) return std::unexpected(RefinementRecipeError::InvalidChance);
    if (recipe.failure != RefinementFailure::Keep && recipe.failure != RefinementFailure::Destroy && recipe.failure != RefinementFailure::SetLevel)
        return std::unexpected(RefinementRecipeError::InvalidFailure);
    if (recipe.failure == RefinementFailure::SetLevel)
    {
        if (!recipe.failureLevel || *recipe.failureLevel >= recipe.sourceLevel || !LevelExists(catalog, recipe.source, *recipe.failureLevel))
            return std::unexpected(RefinementRecipeError::InvalidFailure);
    }
    else if (recipe.failureLevel) return std::unexpected(RefinementRecipeError::InvalidFailure);
    if (recipe.ingredients.size() > MaxRefinementIngredients) return std::unexpected(RefinementRecipeError::TooManyIngredients);
    std::set<std::pair<ItemKey,std::uint16_t>> unique;
    for (const auto& ingredient : recipe.ingredients)
    {
        const auto* item = catalog.Find(ingredient.key);
        if (!item || item->base.retired || !ingredient.count || !LevelExists(catalog, ingredient.key, ingredient.refinementLevel))
            return std::unexpected(RefinementRecipeError::InvalidIngredient);
        if (!unique.emplace(ingredient.key, ingredient.refinementLevel).second)
            return std::unexpected(RefinementRecipeError::DuplicateIngredient);
    }
    return {};
}

std::expected<RefinementPlan, RefinementUseError> PrepareRefinement(
    const ItemCatalog& catalog, std::uint32_t recipeId, const RefinementUseContext& context,
    const RefinementItemSnapshot& source, std::span<const RefinementItemSnapshot> inventory, std::uint16_t recordedRoll)
{
    if (!context.allowedByRules) return std::unexpected(RefinementUseError::Forbidden);
    if (!context.actorId || !context.walletRevision || context.walletRevision == (std::numeric_limits<std::uint64_t>::max)() ||
        context.gold > MaxGold || std::ranges::all_of(context.operationId, [](auto byte) { return byte == 0; }))
        return std::unexpected(RefinementUseError::InvalidContext);
    const auto* recipe = catalog.FindRefinementRecipe(recipeId);
    if (!recipe) return std::unexpected(RefinementUseError::MissingRecipe);
    const auto maxRevision = (std::numeric_limits<std::uint64_t>::max)();
    if (!source.instanceId || !source.revision || source.revision == maxRevision || source.ownerId != context.actorId ||
        source.key != recipe->source || source.refinementLevel != recipe->sourceLevel || source.count != 1 || source.locked)
        return std::unexpected(RefinementUseError::InvalidSource);
    if (context.gold < recipe->goldCost) return std::unexpected(RefinementUseError::InsufficientGold);
    const bool guaranteed = recipe->successChanceBp == 0 || recipe->successChanceBp == 10000;
    if ((guaranteed && recordedRoll != 0) || (!guaranteed && (recordedRoll == 0 || recordedRoll > 10000)))
        return std::unexpected(RefinementUseError::InvalidDecision);
    if (inventory.size() > MaxRefinementInventory) return std::unexpected(RefinementUseError::InvalidInventory);
    std::vector<const RefinementItemSnapshot*> sorted;
    sorted.reserve(inventory.size());
    for (const auto& row : inventory)
    {
        const auto* item = catalog.Find(row.key);
        if (!row.instanceId || !row.revision || row.revision == maxRevision || row.ownerId != context.actorId ||
            !row.count || !item || row.count > item->base.maxStack || !LevelExists(catalog, row.key, row.refinementLevel))
            return std::unexpected(RefinementUseError::InvalidInventory);
        if (row.instanceId == source.instanceId)
        {
            // The owner may include its source in the inventory snapshot. It
            // can never consume that same physical instance as an ingredient.
            if (row.revision != source.revision || row.key != source.key || row.refinementLevel != source.refinementLevel ||
                row.count != source.count || row.locked != source.locked)
                return std::unexpected(RefinementUseError::InvalidInventory);
        }
        sorted.push_back(&row);
    }
    std::ranges::sort(sorted, {}, [](auto* row) { return row->instanceId; });
    for (std::size_t i = 1; i < sorted.size(); ++i)
        if (sorted[i-1]->instanceId == sorted[i]->instanceId) return std::unexpected(RefinementUseError::InvalidInventory);
    RefinementPlan plan {context.operationId, catalog.Revision(), catalog.ContentHash(), recipeId,
        context.actorId, source.instanceId, source.revision, context.walletRevision, context.gold, recipe->goldCost,
        source.key, source.refinementLevel, {}, {}, false, recordedRoll, {}};
    for (const auto& ingredient : recipe->ingredients)
    {
        std::uint32_t remaining = ingredient.count;
        for (const auto* row : sorted)
        {
            if (row->instanceId == source.instanceId || row->locked || row->key != ingredient.key || row->refinementLevel != ingredient.refinementLevel) continue;
            const auto count = (std::min)(remaining, row->count);
            if (count) plan.debits.push_back({row->instanceId, row->revision, row->count, count});
            remaining -= count;
            if (!remaining) break;
        }
        if (remaining) return std::unexpected(RefinementUseError::InsufficientMaterials);
    }
    std::ranges::sort(plan.debits, {}, &RefinementDebit::instanceId);
    plan.success = recipe->successChanceBp == 10000 || (recipe->successChanceBp != 0 && recordedRoll <= recipe->successChanceBp);
    if (plan.success) { plan.result = recipe->target; plan.resultLevel = recipe->targetLevel; }
    else if (recipe->failure != RefinementFailure::Destroy)
    {
        plan.result = source.key;
        plan.resultLevel = recipe->failure == RefinementFailure::Keep ? source.refinementLevel : *recipe->failureLevel;
    }
    return plan;
}
} // namespace auriga::item
