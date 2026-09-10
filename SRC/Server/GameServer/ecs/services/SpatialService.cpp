#include "../../stdafx.h"

#include "SpatialService.hpp"

#include "../../entity.h"
#include "../../building.h"
#include "../../char.h"
#include "../../config.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../new_offlineshop.h"
#include "../../sectree.h"
#include "../../sectree_manager.h"
#include "../../utils.h"
#include "../CBuildingRegistry.hpp"
#include "../EntityInvariants.hpp"
#include "../EventDispatcher.hpp"
#include "../ItemRegistry.hpp"
#include "../OfflineShopEntityRegistry.hpp"
#include "../Registry.hpp"
#include "../SpatialHelpers.hpp"
#include "../VIDRegistry.hpp"
#include "../events.hpp"
#include "../components/identity_components.hpp"
#include "../components/item_components.hpp"
#include "../components/spatial_components.hpp"
#include "../components/transform_components.hpp"
#include "../components/visibility_components.hpp"
#include "../systems/PlayerRuntimeSystem.hpp"
#include "EntityNetworkDispatch.hpp"
#include "VisibilityService.hpp"
#include "../systems/VisibilitySystem.hpp"

namespace ecs::SpatialService {

LPENTITY LPENTITYFromEntity(entt::registry& reg, entt::entity e)
{
    if (e == entt::null || !reg.valid(e))
        return nullptr;

    if (const auto* legacy = reg.try_get<ecs::LegacyCharPtr>(e))
        return legacy->ptr;

    if (const auto* item = reg.try_get<ecs::ItemIdentity>(e)) {
        if (LPITEM legacyItem = ITEM_MANAGER::instance().Find(item->id);
            legacyItem && legacyItem->GetEntityHandle() == e)
            return legacyItem;
        if (item->vid != 0) {
            auto* legacyItem = ITEM_MANAGER::instance().FindByVID(item->vid);
            if (legacyItem && legacyItem->GetEntityHandle() == e) return legacyItem;
        }
    }

    if (auto* building = ecs::CBuildingRegistry::FindLegacyByEntity(e))
        return static_cast<LPENTITY>(building);

#ifdef ENABLE_NEW_SHOP_IN_CITIES
    if (auto* shop = ecs::OfflineShopEntityRegistry::FindLegacyByEntity(e))
        return static_cast<LPENTITY>(shop);
#endif

    return nullptr;
}

entt::entity EntityFromLPENTITY(LPENTITY entity)
{
    if (!entity)
        return entt::null;

    // Characters and items carry the handle themselves; no cast, no switch.
    if (const entt::entity self = entity->GetEntityHandle(); self != entt::null)
        return self;

    switch (entity->GetType()) {
    case ENTITY_OBJECT:
        return ecs::CBuildingRegistry::FindByID(static_cast<building::CObject*>(entity)->GetID());
#ifdef ENABLE_NEW_SHOP_IN_CITIES
    case ENTITY_NEWSHOPS:
        return ecs::OfflineShopEntityRegistry::FindByVID(static_cast<offlineshop::ShopEntity*>(entity)->GetVID());
#endif
    default:
        return entt::null;
    }
}

bool InsertEntity(entt::registry& reg, entt::entity e, uint32_t mapIndex, int32_t x, int32_t y, int32_t z)
{
    if (!reg.valid(e) || mapIndex == 0 || mapIndex > INT32_MAX ||
        ecs::VisibilitySystem::IsRemoving(reg, e)) return false;
    auto* tree = ecs::SectorAt(int32_t(mapIndex), x, y);
    if (!tree || tree->IsDestroying() || ecs::SectorOf(reg, e)) return false;
    const auto* initialVersion = reg.try_get<ecs::SpatialRevision>(e);
    const uint64_t revision = initialVersion ? initialVersion->value : 0;
    const bool hadSpatial = reg.all_of<ecs::SpatialEntity>(e);
    const bool hadView = reg.all_of<ecs::ViewActiveTag>(e);
    const bool hadDirty = reg.all_of<ecs::VisibilityDirty>(e);
    bool committed = false;
    const auto rollback = [&] {
        const auto ours = [&] {
            if (committed || !reg.valid(e) || ecs::SectorOf(reg, e)) return false;
            const auto* version = reg.try_get<ecs::SpatialRevision>(e);
            return (version ? version->value : 0) == revision;
        };
        if (ours() && !hadSpatial) reg.remove<ecs::SpatialEntity>(e);
        if (ours() && !hadView) reg.remove<ecs::ViewActiveTag>(e);
        if (ours() && !hadDirty) reg.remove<ecs::VisibilityDirty>(e);
    };
    struct Rollback {
        const decltype(rollback)& fn;
        ~Rollback() { fn(); }
    } undo {rollback};
    const auto eligible = [&] {
        if (!reg.valid(e) || ecs::SectorOf(reg, e)) return false;
        const auto* version = reg.try_get<ecs::SpatialRevision>(e);
        if ((version ? version->value : 0) != revision) return false;
        if (reg.all_of<ecs::ItemIdentity>(e)) {
            const auto* owner = reg.try_get<ecs::ItemOwner>(e);
            const auto* location = reg.try_get<ecs::ItemLocation>(e);
            const auto* equipped = reg.try_get<ecs::ItemEquipped>(e);
            if ((owner && (owner->owner != entt::null || owner->ownerPID != 0)) ||
                (equipped && equipped->equipped) || !location || location->window != GROUND) return false;
        }
        return true;
    };
    if (!eligible()) return false;
    // Preparation may publish on_construct signals, so never retain component
    // references or a CEntity across it. Item identity needs no legacy object.
    const auto prepare = [&]<class T>() {
        if (!reg.valid(e)) return false;
        if (!reg.all_of<T>(e)) reg.insert<T>(&e, &e + 1);
        return reg.valid(e) && reg.all_of<T>(e);
    };
    if (!prepare.template operator()<ecs::SpatialKindTag>() ||
        !prepare.template operator()<ecs::Position>() ||
        !prepare.template operator()<ecs::PositionZ>() ||
        !prepare.template operator()<ecs::MapIndex>() ||
        !prepare.template operator()<ecs::ViewMap>() ||
        !prepare.template operator()<ecs::ViewerMap>() ||
        !prepare.template operator()<ecs::ViewAgeMap>() ||
        !prepare.template operator()<ecs::ViewActiveTag>() ||
        !prepare.template operator()<ecs::VisibilityDirty>() ||
        !prepare.template operator()<ecs::SpatialRevision>() ||
        !prepare.template operator()<ecs::SpatialEntity>()) return false;
    if (!reg.valid(e) || !reg.all_of<ecs::SpatialKindTag, ecs::Position, ecs::PositionZ,
        ecs::MapIndex, ecs::SpatialEntity, ecs::SpatialRevision>(e) || !eligible()) return false;
    if (reg.all_of<ecs::ItemIdentity>(e))
        reg.get<ecs::SpatialKindTag>(e).kind = ecs::SpatialKind::Item;
    else if (reg.all_of<ecs::BuildingState>(e))
        reg.get<ecs::SpatialKindTag>(e).kind = ecs::SpatialKind::Building;
    else if (reg.all_of<ecs::OfflineShopState>(e))
        reg.get<ecs::SpatialKindTag>(e).kind = ecs::SpatialKind::OfflineShop;
    reg.get<ecs::Position>(e) = {x, y, z};
    reg.get<ecs::PositionZ>(e).z = z;
    reg.get<ecs::MapIndex>(e).value = int32_t(mapIndex);
    if (!tree->InsertEntity(e)) return false;
    committed = true;
    // Membership is committed. The caller publishes with UpdateSectree after
    // installing its timers/state; insertion itself sends no network packets.
    return true;
}

void RemoveEntity(entt::registry& reg, entt::entity e)
{
    if (!reg.valid(e)) return;
    auto* tree = ecs::SectorOf(reg, e);
    if (tree) tree->RemoveEntity(e);
    if (!reg.valid(e) || ecs::SectorOf(reg, e)) return;
    // Visibility state is detached before packets; a callback may reinsert.
    ecs::VisibilitySystem::Remove(reg, e);
}

void UpdateSectree(entt::registry& reg, entt::entity e)
{
    ecs::VisibilitySystem::Refresh(reg, e);
}

void ForEachAround(entt::registry& reg, entt::entity source, int32_t range, const std::function<void(entt::entity)>& callback)
{
    if (!callback || !reg.valid(source)) return;
    const auto* position = reg.try_get<ecs::Position>(source);
    const auto* map = reg.try_get<ecs::MapIndex>(source);
    auto* tree = ecs::SectorOf(reg, source);
    if (!position || !map || !tree) return;
    const auto origin = *position;
    const auto mapIndex = map->value;
    const auto* version = reg.try_get<ecs::SpatialRevision>(source);
    const uint64_t revision = version ? version->value : 0;
    auto visit = [&](entt::entity e) {
        if (!reg.valid(source) || ecs::SectorOf(reg, source) != tree ||
            !reg.all_of<ecs::SpatialEntity>(e)) return;
        const auto* now = reg.try_get<ecs::Position>(source);
        const auto* current = reg.try_get<ecs::SpatialRevision>(source);
        if (!now || now->x != origin.x || now->y != origin.y || now->z != origin.z ||
            (current ? current->value : 0) != revision) return;
        const auto* pos = reg.try_get<ecs::Position>(e);
        const auto* targetMap = reg.try_get<ecs::MapIndex>(e);
        if (!pos || !targetMap || targetMap->value != mapIndex) return;
        const auto dx = std::abs(int64_t(pos->x) - origin.x), dy = std::abs(int64_t(pos->y) - origin.y);
        if (range > 0 && std::max(dx, dy) + std::min(dx, dy) / 2 > range) return;
        callback(e);
    };
    tree->ForEachAround(visit);
}

LPSECTREE GetSectree(entt::registry& reg, entt::entity e)
{
    return ecs::SectorOf(reg, e);
}

void ForEachInMap(entt::registry& reg, uint32_t mapIndex, const std::function<void(entt::entity)>& callback)
{
    if (!callback)
        return;

    std::vector<entt::entity> entities;
    for (auto e : reg.view<ecs::SpatialEntity, ecs::MapIndex>())
        if (reg.get<ecs::MapIndex>(e).value == static_cast<int32_t>(mapIndex) &&
            SectreeMember(e, ecs::SectorOf(reg, e))) entities.push_back(e);
    for (auto e : entities) {
        if (!reg.valid(e) || !reg.all_of<ecs::SpatialEntity, ecs::MapIndex>(e)) continue;
        if (reg.get<ecs::MapIndex>(e).value == static_cast<int32_t>(mapIndex) &&
            SectreeMember(e, ecs::SectorOf(reg, e))) callback(e);
    }
}

}
