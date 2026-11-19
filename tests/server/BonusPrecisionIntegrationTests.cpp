#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/entity/char.h"
#include "../../SRC/Server/GameServer/network/desc.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/character_stats_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <type_traits>

entt::registry g_registry;
namespace { bool nested=false, destroyOwner=false; }
void DESC::Packet(const void*, int) { std::abort(); }
namespace ecs::PlayerRuntime { LPDESC GetDesc(entt::entity) { return nullptr; } }
namespace ecs::PointSystem {
int64_t Get(entt::entity e, uint8_t type) {
    auto* s=g_registry.try_get<ecs::CharacterStatsComponent>(e);
    return s ? s->points[type] : 0;
}
bool Set(entt::entity e, uint8_t type, int64_t value) {
    if(!g_registry.valid(e)) return false;
    auto& s=g_registry.get<ecs::CharacterStatsComponent>(e);
    s.points[type]=value;
    if(!s.precisionWrite || s.precisionActiveType != type)s.bonusFractionRaw[type]=0;
    return true;
}
void Change(entt::entity e, uint8_t type, int64_t amount, bool, bool
#ifdef __ENABLE_BLOCK_EXP__
    ,bool
#endif
) {
    if(!g_registry.valid(e)) return;
    auto& s=g_registry.get<ecs::CharacterStatsComponent>(e);
    if(IsPrecisionPoint(type)) {
        if(s.precisionEntryType==type)s.precisionEntryType=-1;
        else { ChangePrecise(e,type,static_cast<double>(amount)); return; }
    }
    Set(e,type,Get(e,type)+amount);
    if(nested) {nested=false;ChangePrecise(e,POINT_ATTBONUS_BOSS,2.224);}
    if(destroyOwner) {destroyOwner=false;g_registry.destroy(e);}
}
}
template<class T> void Fixture(const std::filesystem::path& folder,const char* name,const T& packet) {
    std::ofstream file(folder/name,std::ios::binary);file.write(reinterpret_cast<const char*>(&packet),sizeof(packet));
    if(!file)throw std::runtime_error("Fixture write failed");
}
static void Check(bool value) { if (!value) throw std::runtime_error("Bonus regression failed"); }
int main(int argc,char** argv) {
    using namespace ecs::PointSystem;
    static_assert(sizeof(TPlayerItemAttribute)==9);
    static_assert(std::is_trivially_copyable_v<TPlayerItemAttribute>);
    static_assert(sizeof(TPacketGCItemSet)==107);
    static_assert(sizeof(TPacketGCItemUpdate)==94);
    static_assert(sizeof(TPacketGCAffectAdd)==26);
    static_assert(sizeof(TPacketGCBonusPoints)==2296);
    static_assert(sizeof(TPacketGCBonusPoint)==11);
    auto e=g_registry.create();g_registry.emplace<ecs::CharacterStatsComponent>(e);
    constexpr auto type=POINT_ATTBONUS_MONSTER;
    Check(IsPrecisionPoint(type));Check(!IsPrecisionPoint(POINT_GOLD));Check(!IsPrecisionPoint(POINT_HP));
    ChangePrecise(e,type,1.113);Check(GetPreciseRaw(e,type)==1113000000LL);
    ChangePrecise(e,type,2.224);Check(GetPreciseRaw(e,type)==3337000000LL);
    ChangePrecise(e,type,-1.113);Check(GetPreciseRaw(e,type)==2224000000LL);
    ChangePrecise(e,type,-2.224);Check(GetPreciseRaw(e,type)==0);
    for(int i=0;i<100000;i++)ChangePrecise(e,type,1.113);
    Check(GetPreciseRaw(e,type)==111300000000000LL);
    for(int i=0;i<100000;i++)ChangePrecise(e,type,-1.113);
    Check(GetPreciseRaw(e,type)==0);
    ChangePrecise(e,type,2000000000.113);Check(GetPreciseRaw(e,type)==2000000000113000000LL);
    ChangePrecise(e,type,-2000000000.113);Check(GetPreciseRaw(e,type)==0);
    ChangePrecise(e,type,std::numeric_limits<double>::quiet_NaN());
    ChangePrecise(e,type,std::numeric_limits<double>::infinity());
    ChangePrecise(e,type,1e100);Check(GetPreciseRaw(e,type)==0);
    ChangePrecise(e,POINT_MALL_ATTBONUS,99.999);ChangePrecise(e,POINT_MALL_ATTBONUS,.002);
    Check(GetPreciseRaw(e,POINT_MALL_ATTBONUS)==100000000000LL);
    Check(SetPrecise(e,type,1.113));Check(GetPreciseRaw(e,type)==1113000000LL);
    Check(Set(e,type,int64_t(0)));Check(GetPreciseRaw(e,type)==0);
    nested=true;ChangePrecise(e,type,1.113);
    Check(GetPreciseRaw(e,type)==1113000000LL && GetPreciseRaw(e,POINT_ATTBONUS_BOSS)==2224000000LL);
    Check(!g_registry.get<ecs::CharacterStatsComponent>(e).precisionWrite);
    Check(g_registry.get<ecs::CharacterStatsComponent>(e).precisionEntryType==-1);
    destroyOwner=true;ChangePrecise(e,type,1.113);Check(!g_registry.valid(e));
    ChangePrecise(e,type,1.113);SendPrecisionPoints(e); // stale owner: safe no-op
    if(argc>1) {
        std::filesystem::path folder=argv[1];std::filesystem::create_directories(folder);
        TPacketGCItemSet item{};item.header=21;item.Cell.window_type=1;item.Cell.cell=7;item.vnum=30006;item.count=1;
        item.aAttr[0]={uint8_t(APPLY_ATTBONUS_MONSTER),1.113};item.lockedattr=-1;
        Fixture(folder,"item-set.bin",item);
        TPacketGCItemUpdate update{};update.header=25;update.Cell=item.Cell;update.count=1;update.aAttr[0]=item.aAttr[0];update.lockedattr=-1;
        Fixture(folder,"item-update.bin",update);
        TPacketGCAffectAdd affect{};affect.bHeader=126;affect.elem.dwType=651;affect.elem.bApplyOn=type;affect.elem.lApplyValue=1.113;affect.elem.lDuration=3600;
        Fixture(folder,"affect.bin",affect);
        TPacketGCBonusPoints snapshot{};snapshot.header=181;snapshot.enabled[type]=1;snapshot.raw[type]=1113000000LL;
        Fixture(folder,"bonus-snapshot.bin",snapshot);
        TPacketGCBonusPoint delta{};delta.header=182;delta.type=type;delta.enabled=1;delta.raw=-1113000000LL;
        Fixture(folder,"bonus-delta.bin",delta);
        std::ofstream sizes(folder/"sizes.txt");
        sizes<<"ItemAttribute="<<sizeof(TPlayerItemAttribute)<<"\nItemSet="<<sizeof(item)<<"\nItemUpdate="<<sizeof(update)
             <<"\nAffect="<<sizeof(affect)<<"\nBonusPointType="<<int(type)<<"\n";
    }
    std::cout<<"PASS production precision accumulator, 200000 equip/remove operations, caps, large exact sources, invalid values, reentrancy, destroyed owner and native packet ABI\n";
}
