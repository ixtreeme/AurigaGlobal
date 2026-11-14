#include "stdafx.h"
#include "AccountCollectionSystem.hpp"
#include "mount_inventory_helper.h"
#include "pet_inventory_helper.h"
#include "../ecs/Registry.hpp"
#include "../ecs/components/identity_components.hpp"
#include "../ecs/components/character_runtime_components.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/PetInventorySystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "db.h"
#include "desc.h"
#include "p2p.h"
#include "packet.h"
#include "constants.h"
#include <Core/buffer.h>
#include <memory>
#include <set>

namespace {
bool Loaded(entt::entity owner, uint8_t window) {
    return window == MOUNT_INVENTORY ? MountSystem::GetMountInventory(owner) != entt::null
        : PetInventorySystem::GetPetInventory(owner) != entt::null;
}
}
namespace AccountCollectionSystem {
CollectionBonuses::Totals Collect(entt::entity owner, uint8_t window) {
    CollectionBonuses::Totals result(window);
    if (!Loaded(owner,window)) return result;
    const int size = window == MOUNT_INVENTORY
        ? MountSystem::GetMountInventoryWidth(owner) * MountSystem::GetMountInventorySize(owner)
        : PetInventorySystem::GetPetInventoryWidth(owner) * PetInventorySystem::GetPetInventorySize(owner);
    for (int pos=0; pos<size; ++pos) {
        const auto item = window == MOUNT_INVENTORY ? MountSystem::GetMountInventoryItem(owner,pos)
            : PetInventorySystem::GetPetInventoryItem(owner,pos);
        if (!ItemSystem::IsValidItem(item) || ItemSystem::GetItemOwner(item)!=owner ||
            ItemSystem::GetItemWindow(item)!=window || ItemSystem::GetItemCell(item)!=pos) continue;
        const auto* proto=ItemSystem::GetItemProto(item);
        if (!proto || !(window==MOUNT_INVENTORY ? CMountInventoryHelper::CanMoveIntoMountInventory(item)
                : CPetInventoryHelper::CanMoveIntoPetInventory(item))) continue;
        if (!result.AddItem(ItemSystem::GetItemVnum(item))) continue;
        const auto add=[&](uint8_t type,double value) {
            if (type<MAX_APPLY_NUM && aApplyInfo[type].bPointType!=POINT_NONE) result.AddApply(type,value);
        };
        for (const auto& apply:proto->aApplies) add(apply.bType,apply.lValue);
        for (int i=0;i<ITEM_ATTRIBUTE_MAX_NUM;++i) {
            const auto attr=ItemSystem::GetItemAttribute(item,i);add(attr.bType,attr.sValue);
        }
    }
    return result;
}
void Compute(entt::entity owner,uint8_t window) {
    auto totals=Collect(owner,window);
    // Aggregate by point type once, preserving fractional bonuses from both sources.
    std::map<uint8_t,double> points;
    for (const auto& [type,bonus]:totals.bonuses)
        if (type<MAX_APPLY_NUM && aApplyInfo[type].bPointType!=POINT_NONE)
            points[aApplyInfo[type].bPointType]+=bonus.Total(totals.Tiers());
    for (const auto& [type,value]:points) {
        if (!g_registry.valid(owner)) return;
        if (value!=0) ecs::PointSystem::Change(owner,type,value);
    }
}
void Send(entt::entity owner,uint8_t window) {
    auto* desc=ecs::PlayerRuntime::GetDesc(owner);
    if (!desc) return;
    const auto totals=Collect(owner,window);
    TPacketGCCollectionBonuses header{};
    header.header=HEADER_GC_COLLECTION_BONUSES;
    header.size=sizeof(header)+totals.bonuses.size()*sizeof(TCollectionBonusData);
    header.window=window;header.loaded=Loaded(owner,window);
    header.itemCount=totals.Count();header.tiers=totals.Tiers();header.threshold=totals.threshold;
    header.bonusCount=totals.bonuses.size();
    TEMP_BUFFER buffer;buffer.write(&header,sizeof(header));
    for (const auto& [type,bonus]:totals.bonuses) {
        TCollectionBonusData row{type,bonus.items,bonus.perTier};buffer.write(&row,sizeof(row));
    }
    desc->Packet(buffer.read_peek(),buffer.size());
}
bool Persist(const char* sql,uint32_t account,uint8_t window) {
    // Commit before acknowledging the client or invalidating another core. A new CH
    // can then read the shared account tables without racing the old async SQL queue.
    std::unique_ptr<SQLMsg> result(DBManager::instance().DirectQuery("%s",sql));
    if (!result || result->uiSQLErrno) {
        sys_err("ACCOUNT_COLLECTION_SAVE_FAILED account=%u window=%u",account,window);
        return false;
    }
    TPacketGGCollectionChanged packet{HEADER_GG_COLLECTION_CHANGED,account,window};
    P2P_MANAGER::instance().Send(&packet,sizeof(packet));
    return true;
}
void Invalidate(uint32_t account,uint8_t window) {
    if (!account || (window!=MOUNT_INVENTORY && window!=PET_INVENTORY)) return;
    std::vector<entt::entity> owners;
    for (const auto owner:g_registry.view<ecs::AccountID>())
        if (ecs::PlayerRuntime::GetAccountID(owner)==account && ecs::PlayerRuntime::GetDesc(owner)) owners.push_back(owner);
    for (const auto owner:owners) {
        if (window==MOUNT_INVENTORY) MountSystem::DestroyMountInventory(owner);
        else PetInventorySystem::DestroyPetInventory(owner);
        // Remove the old bank's bonuses while its authoritative snapshot is reloaded.
        ecs::PointSystem::Compute(owner);NetworkSyncSystem::PointsPacket(owner);
        if (auto* desc=ecs::PlayerRuntime::GetDesc(owner)) {
            if (window==MOUNT_INVENTORY) {
                TPacketGCMountInventory packet{};packet.bHeader=HEADER_GC_MOUNT_INVENTORY;
                packet.size=sizeof(packet);packet.bWidth=12;packet.bHeight=16;desc->Packet(&packet,sizeof(packet));
            } else {
                TPacketGCPetInventory packet{};packet.bHeader=HEADER_GC_PET_INVENTORY;
                packet.size=sizeof(packet);packet.bWidth=12;packet.bHeight=16;desc->Packet(&packet,sizeof(packet));
            }
        }
        Send(owner,window);
        if (window==MOUNT_INVENTORY) MountSystem::QueryMountInventory(owner);
        else PetInventorySystem::QueryPetInventory(owner);
    }
}
}
