#include "stdafx.h"
#include "TypedQuestSelection.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "../ecs/Registry.hpp"
#include <algorithm>

std::vector<quest::QuestState::ItemChoice> TypedQuestStoneChoices(entt::entity player)
{
    using namespace auriga::item;
    std::vector<quest::QuestState::ItemChoice> choices;
    const auto* inventory = g_registry.valid(player) ? g_registry.try_get<ecs::TypedItems>(player) : nullptr;
    const auto catalog = runtime::CurrentCatalog();
    if (!inventory || !catalog) return choices;
    for (const auto& item : inventory->state.items) {
        if (item.key.Category() != ItemCategory::SocketStone ||
            !PlainInventoryItem(item, inventory->state.fence.owner) || item.placement->anchor >= 90) continue;
        const auto* definition = catalog->Find(item.key);
        const auto* stone = definition ? std::get_if<ItemSocketStoneRow>(&definition->category) : nullptr;
        const auto* rules = catalog->FindDefinitionRules(item.key, item.refinementLevel);
        if (!stone || definition->base.retired || stone->kind != SocketStoneKind::Insertable ||
            item.refinementLevel + (rules ? rules->displayRefinementOffset : 0) > 2) continue;
        choices.push_back({0, item.placement->anchor, item.id.Value(), item.revision});
    }
    std::ranges::sort(choices, {}, &quest::QuestState::ItemChoice::cell);
    if (choices.size() > 55) choices.resize(55);
    return choices;
}
