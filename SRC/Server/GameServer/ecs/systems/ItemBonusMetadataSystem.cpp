#include "../../core/stdafx.h"
#include "../../entity/char.h"
#include "../../item/item_manager.h"
#include "../../network/desc.h"
#include "PlayerRuntimeSystem.hpp"
#include "../Registry.hpp"
#include "../components/character_stats_components.hpp"
#include <common/Headers/GC_Packet.h>
#include <algorithm>
#include <cstring>
#include <vector>

namespace NetworkSyncSystem {
void SendItemBonusCatalog(entt::entity e) {
    if (!g_registry.valid(e)) return;
    auto* stats=g_registry.try_get<ecs::CharacterStatsComponent>(e);
    auto* desc=ecs::PlayerRuntime::GetDesc(e);
    if (!stats || !desc) return;
    auto& manager=ITEM_MANAGER::instance();
    const auto revision=manager.BonusCatalogRevision();
    if (stats->bonusCatalogRevision==revision) return;
    const auto& protos=manager.GetTable();
    constexpr size_t chunk=256;
    bool clear=true;
    for (size_t begin=0;begin<protos.size() || clear;begin+=chunk) {
        if (!g_registry.valid(e) || ecs::PlayerRuntime::GetDesc(e)!=desc) return;
        const auto count=std::min(chunk,protos.size()-std::min(begin,protos.size()));
        std::vector<uint8_t> bytes(sizeof(TPacketGCItemBonusCatalog)+count*sizeof(TItemBonusCatalogEntry));
        TPacketGCItemBonusCatalog header {};
        header.header=HEADER_GC_ITEM_BONUS_CATALOG;header.size=static_cast<uint32_t>(bytes.size());
        header.count=static_cast<uint16_t>(count);header.clear=clear;
        std::memcpy(bytes.data(),&header,sizeof(header));
        for (size_t i=0;i<count;++i) {
            const auto& proto=protos[begin+i];
            TItemBonusCatalogEntry entry {};
            entry.vnum=proto.dwVnum;entry.range=proto.dwVnumRange;
            std::copy_n(proto.aApplies,std::min<size_t>(5,ITEM_APPLY_MAX_NUM),entry.applies);
            if (const auto* extra=manager.GetExtraProto(proto.dwVnum)) {
#ifdef ENABLE_RARITY_SYSTEM
                entry.rarity=static_cast<uint8_t>(std::clamp(extra->iRarity,0,255));
#endif
#ifdef ENABLE_NEW_EXTRA_BONUS
                std::copy_n(extra->ExtraBonus,std::min<size_t>(2,NEW_EXTRA_BONUS_COUNT),entry.extra);
#endif
            }
            std::memcpy(bytes.data()+sizeof(header)+i*sizeof(entry),&entry,sizeof(entry));
        }
        desc->LargePacket(bytes.data(),static_cast<int>(bytes.size()));
        clear=false;
        if (!count) break;
    }
    if (g_registry.valid(e))
        if (auto* current=g_registry.try_get<ecs::CharacterStatsComponent>(e)) current->bonusCatalogRevision=revision;
}
}
