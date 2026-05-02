#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "ItemRegistry.hpp"
#include "Registry.hpp"
#include "components/item_components.hpp"
#include "../item.h"
#include <Core/Logging.hpp>

namespace ecs::ItemInvariants {

inline bool HasMinimumItemComponents(entt::registry& reg, entt::entity itemE)
{
    return itemE != entt::null
        && reg.valid(itemE)
        && reg.all_of<ecs::ItemIdentity>(itemE)
        && reg.all_of<ecs::ItemLocation>(itemE)
        && reg.all_of<ecs::ItemCount>(itemE)
        && reg.all_of<ecs::ItemPrototypeMeta>(itemE)
        && reg.all_of<ecs::ItemOwner>(itemE)
        && reg.all_of<ecs::ItemFlags>(itemE)
        && reg.all_of<ecs::ItemSockets>(itemE)
        && reg.all_of<ecs::ItemAttributes>(itemE);
}

inline const char* DescribeItemEntity(entt::registry& reg, entt::entity itemE)
{
    if (itemE == entt::null)
        return "NULL";
    if (!reg.valid(itemE))
        return "INVALID";
    if (HasMinimumItemComponents(reg, itemE))
        return "COMPLETE";
    return "MISSING_CORE";
}

inline void ValidateItemEntity(entt::registry& reg, entt::entity itemE, const char* context)
{
    if (itemE == entt::null || !reg.valid(itemE))
        return;

    const char* missing = nullptr;
    if (!reg.all_of<ecs::ItemIdentity>(itemE))
        missing = "ItemIdentity";
    else if (!reg.all_of<ecs::ItemLocation>(itemE))
        missing = "ItemLocation";
    else if (!reg.all_of<ecs::ItemCount>(itemE))
        missing = "ItemCount";
    else if (!reg.all_of<ecs::ItemPrototypeMeta>(itemE))
        missing = "ItemPrototypeMeta";
    else if (!reg.all_of<ecs::ItemOwner>(itemE))
        missing = "ItemOwner";
    else if (!reg.all_of<ecs::ItemFlags>(itemE))
        missing = "ItemFlags";
    else if (!reg.all_of<ecs::ItemSockets>(itemE))
        missing = "ItemSockets";
    else if (!reg.all_of<ecs::ItemAttributes>(itemE))
        missing = "ItemAttributes";

    if (missing) {
        LOG_WARN("[ITEM_INVARIANT] entity={} ctx={} missing component {}",
            static_cast<uint32_t>(itemE), context ? context : "unknown", missing);
    }
}

inline void ValidateLegacyItemHasEntity(LPITEM legacyItem, const char* context)
{
    if (!legacyItem)
        return;

    const uint32_t itemID = legacyItem->GetID();
    if (itemID == 0)
        return;

    const entt::entity itemE = CItemRegistry::Instance().Find(itemID);
    if (itemE == entt::null || !g_registry.valid(itemE)) {
        LOG_WARN("[ITEM_BYPASS] legacy item id={} vid={} vnum={} ctx={} has no ECS entity",
            itemID, legacyItem->GetVID(), legacyItem->GetVnum(), context ? context : "unknown");
        return;
    }

    ValidateItemEntity(g_registry, itemE, context);
}

} // namespace ecs::ItemInvariants
