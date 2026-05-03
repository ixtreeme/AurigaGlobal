#include "../../stdafx.h"

#include "SpatialService.hpp"

#include "../../entity.h"
#include "../../building.h"
#include "../../char.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../new_offlineshop.h"
#include "../../sectree.h"
#include "../../sectree_manager.h"
#include "../../utils.h"
#include "../AIHelpers.hpp"
#include "../CBuildingRegistry.hpp"
#include "../EntityInvariants.hpp"
#include "../ItemRegistry.hpp"
#include "../OfflineShopEntityRegistry.hpp"
#include "../Registry.hpp"
#include "../SpatialHelpers.hpp"
#include "../VIDRegistry.hpp"
#include "../components/identity_components.hpp"
#include "../components/item_components.hpp"
#include "../components/spatial_components.hpp"
#include "../components/transform_components.hpp"
#include "../systems/PlayerRuntimeSystem.hpp"

namespace {

ecs::SpatialKind KindFromLegacyType(int type)
{
    switch (type) {
    case ENTITY_ITEM:
        return ecs::SpatialKind::Item;
    case ENTITY_OBJECT:
        return ecs::SpatialKind::Building;
    case ENTITY_NEWSHOPS:
        return ecs::SpatialKind::OfflineShop;
    case ENTITY_CHARACTER:
    default:
        return ecs::SpatialKind::Character;
    }
}

void SyncSpatialComponents(entt::registry& reg,
    entt::entity e,
    ecs::SpatialKind kind,
    uint32_t mapIndex,
    int32_t x,
    int32_t y,
    int32_t z)
{
    if (e == entt::null || !reg.valid(e))
        return;

    reg.emplace_or_replace<ecs::SpatialEntity>(e);
    reg.emplace_or_replace<ecs::SpatialKindTag>(e, ecs::SpatialKindTag { kind });
    reg.emplace_or_replace<ecs::Position>(e, x, y, z);
    reg.emplace_or_replace<ecs::PositionZ>(e, ecs::PositionZ { z });
    reg.emplace_or_replace<ecs::MapIndex>(e, static_cast<int32_t>(mapIndex));
}

void SyncVIDFromLegacy(entt::registry& reg, entt::entity e, LPENTITY legacy)
{
    if (e == entt::null || !reg.valid(e) || !legacy)
        return;

    switch (legacy->GetType()) {
    case ENTITY_CHARACTER:
        if (auto* ch = static_cast<LPCHARACTER>(legacy))
            reg.emplace_or_replace<ecs::VIDComponent>(e, ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(ch)));
        break;
    case ENTITY_ITEM:
        if (auto* item = static_cast<LPITEM>(legacy))
            reg.emplace_or_replace<ecs::VIDComponent>(e, item->GetVID());
        break;
    case ENTITY_OBJECT:
        if (auto* object = static_cast<building::CObject*>(legacy))
            reg.emplace_or_replace<ecs::VIDComponent>(e, object->GetVID());
        break;
#ifdef ENABLE_NEW_SHOP_IN_CITIES
    case ENTITY_NEWSHOPS:
        if (auto* shop = static_cast<offlineshop::ShopEntity*>(legacy))
            reg.emplace_or_replace<ecs::VIDComponent>(e, shop->GetVID());
        break;
#endif
    default:
        break;
    }
}

}

namespace ecs::SpatialService {

LPENTITY LPENTITYFromEntity(entt::registry& reg, entt::entity e)
{
    if (e == entt::null || !reg.valid(e))
        return nullptr;

    if (const auto* legacy = reg.try_get<ecs::LegacyCharPtr>(e))
        return legacy->ptr;

    if (const auto* item = reg.try_get<ecs::ItemIdentity>(e)) {
        if (LPITEM legacyItem = ITEM_MANAGER::instance().Find(item->id))
            return legacyItem;
        if (item->vid != 0)
            return ITEM_MANAGER::instance().FindByVID(item->vid);
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

    switch (entity->GetType()) {
    case ENTITY_CHARACTER:
        return AIHelpers::EcsOf(static_cast<LPCHARACTER>(entity));
    case ENTITY_ITEM: {
        auto* item = static_cast<LPITEM>(entity);
        entt::entity itemEntity = CItemRegistry::Instance().Find(item->GetID());
        if (itemEntity == entt::null)
            itemEntity = CItemRegistry::Instance().FindByVID(item->GetVID());
        return itemEntity;
    }
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
    LPENTITY legacy = LPENTITYFromEntity(reg, e);
    if (!legacy)
        return false;

    const ecs::SpatialKind kind = reg.all_of<ecs::SpatialKindTag>(e)
        ? reg.get<ecs::SpatialKindTag>(e).kind
        : KindFromLegacyType(legacy->GetType());

    SyncSpatialComponents(reg, e, kind, mapIndex, x, y, z);
    SyncVIDFromLegacy(reg, e, legacy);

    LPSECTREE tree = ecs::SectorAt(static_cast<int32_t>(mapIndex), x, y);
    if (!tree)
        return false;

    legacy->SetMapIndex(static_cast<int32_t>(mapIndex));
    legacy->SetXYZ(x, y, z);
    if (!tree->InsertEntity(legacy))
        return false;

    ecs::SyncSectorPlacement(reg, e, static_cast<int32_t>(mapIndex), legacy->GetX(), legacy->GetY());
    reg.emplace_or_replace<ecs::ViewActiveTag>(e);
    ecs::Invariants::ValidateSpatialCoverage(reg, e, "spatial.insert");
    return true;
}

void RemoveEntity(entt::registry& reg, entt::entity e)
{
    LPENTITY legacy = LPENTITYFromEntity(reg, e);
    if (!legacy)
        return;

    if (LPSECTREE sectree = legacy->GetSectree())
        sectree->RemoveEntity(legacy);

    if (e != entt::null && reg.valid(e)) {
        reg.remove<ecs::SectorPlacement>(e);
        reg.remove<ecs::ViewActiveTag>(e);
        reg.remove<ecs::SpatialEntity>(e);
    }
}

void UpdateSectree(entt::registry& reg, entt::entity e)
{
    LPENTITY legacy = LPENTITYFromEntity(reg, e);
    if (!legacy)
        return;

    legacy->UpdateSectree();

    if (e != entt::null && reg.valid(e)) {
        SyncSpatialComponents(
            reg,
            e,
            reg.all_of<ecs::SpatialKindTag>(e) ? reg.get<ecs::SpatialKindTag>(e).kind : KindFromLegacyType(legacy->GetType()),
            static_cast<uint32_t>(legacy->GetMapIndex()),
            legacy->GetX(),
            legacy->GetY(),
            legacy->GetZ());
        ecs::SyncSectorPlacement(reg, e, legacy->GetMapIndex(), legacy->GetX(), legacy->GetY());
        ecs::Invariants::ValidateSpatialCoverage(reg, e, "spatial.update_sectree");
    }
}

void ForEachAround(entt::registry& reg, entt::entity source, int32_t range, const std::function<void(entt::entity)>& callback)
{
    if (!callback)
        return;

    const auto* sourcePos = reg.try_get<ecs::Position>(source);
    LPENTITY sourceLegacy = LPENTITYFromEntity(reg, source);
    if (!sourcePos || !sourceLegacy || !sourceLegacy->GetSectree())
        return;

    struct Collector {
        entt::registry& reg;
        entt::entity source;
        const ecs::Position& sourcePos;
        int32_t range;
        const std::function<void(entt::entity)>& callback;

        void operator()(LPENTITY legacy)
        {
            const entt::entity e = ecs::SpatialService::EntityFromLPENTITY(legacy);
            if (e == entt::null || !reg.valid(e) || !reg.all_of<ecs::SpatialEntity>(e))
                return;

            const auto* pos = reg.try_get<ecs::Position>(e);
            if (!pos)
                return;

            if (range > 0 && e != source && DISTANCE_APPROX(pos->x - sourcePos.x, pos->y - sourcePos.y) > range)
                return;

            callback(e);
        }
    } collector { reg, source, *sourcePos, range, callback };

    sourceLegacy->GetSectree()->ForEachAround(collector);
}

LPSECTREE GetSectree(entt::registry& reg, entt::entity e)
{
    LPENTITY legacy = LPENTITYFromEntity(reg, e);
    return legacy ? legacy->GetSectree() : nullptr;
}

void ForEachInMap(entt::registry& reg, uint32_t mapIndex, const std::function<void(entt::entity)>& callback)
{
    if (!callback)
        return;

    auto view = reg.view<ecs::SpatialEntity, ecs::MapIndex>();
    for (const entt::entity e : view) {
        const auto& map = view.get<ecs::MapIndex>(e);
        if (map.value == static_cast<int32_t>(mapIndex))
            callback(e);
    }
}

}
