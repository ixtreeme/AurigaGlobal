#include "stdafx.h"
#include "TypedEquipment.hpp"
#include "TypedPolymorph.hpp"
#include "TypedAccessoryClock.hpp"
#include "TypedRecovery.hpp"
#include "TypedCompanions.hpp"
#include <item/EffectRuntime.hpp>
#include <item/AccessoryRuntime.hpp>
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/components/character_stats_components.hpp"
#include "../ecs/components/vital_components.hpp"
#include "../network/desc.h"
#include <Core/Logging.hpp>

void RefreshTypedEquipment(entt::entity e)
{
    if(!g_registry.valid(e))return;
    RefreshTypedPolymorph(e);
    const auto* items=g_registry.try_get<ecs::TypedItems>(e);const auto catalog=auriga::item::runtime::CurrentCatalog();if(!items || !catalog)return;
    const auto* pointState=g_registry.try_get<ecs::CharacterStatsComponent>(e);
    if(pointState && pointState->recomputing)return; // finish the current source snapshot first
    auto* old=g_registry.try_get<ecs::TypedEquipmentProjection>(e);
    const auto petBonuses=TypedGrowthPetPassiveSource(e);
    const auto elapsed=TypedAccessoryElapsed(e);
    const bool mayExpire=std::ranges::any_of(items->state.items,[elapsed](const auto& i){return !i.destroyed && i.placement && i.placement->kind==auriga::item::ItemStorageKind::Equipment && i.accessory && i.accessory->activeGrade && !i.accessory->permanent && elapsed>=i.accessory->remainingMs;});
    if(old && old->fence==items->state.fence && old->applied && old->stats && old->stats->petBonuses==petBonuses &&
        !mayExpire && items->state.recovery.effects.empty() && std::ranges::none_of(items->state.items,[](const auto& i){return i.toggle && i.toggle->active;}))return;
    auto effective=items->state.items;
    const auto time=auriga::item::ProjectAccessoryTime(*catalog,effective,elapsed);
    const auto facts=TypedItemEffectFacts(e);const auto toggles=auriga::item::ProjectToggleItems(*catalog,effective,TypedRecoveryElapsed(e),facts);if(toggles)effective=*toggles;
    auto schedule=auriga::item::AdvanceRecoverySchedule(*catalog,items->state.recovery,TypedRecoveryElapsed(e));if(schedule)auriga::item::NormalizeTimedItems(*catalog,*schedule,TypedItemEffectNow(),&facts);
    const auto bonuses=schedule ? auriga::item::CollectItemEffectBonuses(*catalog,*schedule,effective) : std::expected<std::vector<auriga::item::BonusEntry>,auriga::item::ItemPersistenceError>{std::unexpected(auriga::item::ItemPersistenceError::InvalidStoredData)};
    const auto built=time && toggles && bonuses ? auriga::item::BuildEquipmentStats(*catalog,items->state.fence.owner,effective,*bonuses) : std::expected<auriga::item::EquipmentStats,auriga::item::EquipmentStatsError>{std::unexpected(auriga::item::EquipmentStatsError::InvalidItem)};
    auto stats=built ? std::optional{*built} : std::optional<auriga::item::EquipmentStats>{};
    if(stats)stats->petBonuses=petBonuses;
    const bool changed=!old || old->stats!=stats;const bool recompute=changed || !old->applied;
    ecs::TypedEquipmentProjection next;next.fence=items->state.fence;next.stats=stats;next.revision=old ? old->revision+(changed ? 1 : 0) : 1;
    next.applied=g_registry.all_of<ecs::CharacterStatsComponent,ecs::CharacterPoints,ecs::Health,ecs::Mana,ecs::Stamina>(e);
    g_registry.emplace_or_replace<ecs::TypedEquipmentProjection>(e,std::move(next));
    if(!built)LOG_ERROR("TypedEquipment: pid {} rejected equipment projection (error {})",ecs::PlayerRuntime::GetPlayerID(e),static_cast<unsigned>(built.error()));
    if(recompute && g_registry.valid(e) && g_registry.all_of<ecs::CharacterStatsComponent,ecs::CharacterPoints,ecs::Health,ecs::Mana,ecs::Stamina>(e))
    {
        ecs::PointSystem::Compute(e);
        NetworkSyncSystem::PointsPacket(e);
    }
}
void TickTypedEquipment()
{
    std::vector<entt::entity> owners;for(auto e:g_registry.view<ecs::TypedItems>())owners.push_back(e);
    for(auto e:owners)
    {
        const auto* old=g_registry.try_get<ecs::TypedEquipmentProjection>(e);
        const auto before=old ? old->revision : 0;
        RefreshTypedEquipment(e);
        const auto* current=g_registry.try_get<ecs::TypedEquipmentProjection>(e);
        if(current && current->revision!=before)SendTypedEquipment(e);
    }
}
void SendTypedEquipment(entt::entity e)
{
    if(!g_registry.valid(e))return;const auto* p=g_registry.try_get<ecs::TypedEquipmentProjection>(e);auto* desc=ecs::PlayerRuntime::GetDesc(e);
    if(!p || !p->fence.revision || !desc || !desc->IsPhase(PHASE_GAME))return;
    const auto projection=*p;std::array<std::int64_t,13> points{};unsigned i=0;
    for(auto type:{POINT_ST,POINT_DX,POINT_HT,POINT_IQ,POINT_ATT_GRADE,POINT_MAGIC_ATT_GRADE,POINT_DEF_GRADE,POINT_MAGIC_DEF_GRADE,POINT_ATT_SPEED,POINT_MOV_SPEED,POINT_CASTING_SPEED,POINT_MAX_HP,POINT_MAX_SP})
        points[i++]=(type==POINT_ATT_SPEED || type==POINT_MOV_SPEED || type==POINT_CASTING_SPEED)
            ? ecs::PointSystem::GetLimitPoint(e,static_cast<std::uint8_t>(type)) : ecs::PointSystem::Get(e,static_cast<std::uint8_t>(type));
    const auto bytes=auriga::item::EncodeEquipmentStats(projection.stats ? &*projection.stats : nullptr,ecs::PlayerRuntime::GetPacketVID(e),projection.revision,projection.fence.revision,points);
    desc->Packet(bytes.data(),static_cast<int>(bytes.size()));
}
