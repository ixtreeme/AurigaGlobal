#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/horse_rider.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/arena.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/event_queue.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/events.hpp"
#include "../../SRC/Server/GameServer/ecs/components/pet_mount_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/dirty_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SkillSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include <Core/Logging.hpp>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

entt::registry g_registry;
entt::dispatcher g_dispatcher;
namespace {
HEART horseHeart {};
int checks = 0, now = 1'000'000, packets = 0, computes = 0, skillPackets = 0, unmounts = 0, summons = 0;
bool dead = false, stunned = false, polymorphed = false, arena = false, war = false;
uint32_t armorVnum = 0;
std::function<void(entt::entity)> onPacket, onTick, onConstruct, onUnmount, onMount;
struct Player {};
void Check(bool value, const char* reason) { ++checks; if (!value) throw std::runtime_error(reason); }
void CheckLive(entt::entity e) { Check(g_registry.valid(e), "service received a dead entity"); }
void Constructed(entt::registry&, entt::entity e) { if (onConstruct) { auto fn=onConstruct; fn(e); } }
void Ticked(const ecs::EvHorseStaminaConsume& event) { if (onTick) { auto fn=onTick; fn(event.riderEntity); } }
void Reset() {
    onPacket = onTick = onConstruct = onUnmount = onMount = {};
    g_registry.clear(); event_destroy(); horseHeart.pulse=0; horseHeart.passes_per_sec=25;
    packets=computes=skillPackets=unmounts=summons=0;
    dead=stunned=polymorphed=arena=war=false; armorVnum=0; now=1'000'000;
}
entt::entity Rider(int level=10, int hp=15, int stamina=5, bool riding=false) {
    const auto e=g_registry.create(); g_registry.emplace<Player>(e);
    THorseInfo data {}; data.bLevel=level; data.sHealth=hp; data.sStamina=stamina;
    data.bRiding=riding; data.dwHorseHealthDropTime=now+100'000;
    MountSystem::LoadHorseData(e,data);
    return e;
}
int Run(int pulse) { horseHeart.pulse=pulse; return event_process(pulse); }
auto& State(entt::entity e) { return g_registry.get<ecs::HorseRuntime>(e); }
void NativeData() {
    Reset();
    const auto e=Rider();
    auto data=MountSystem::StoreHorseData(e);
    Check(data.bLevel==10 && data.sHealth==15 && data.sStamina==5 && !data.bRiding, "native hydration/serialization mismatch");
    Check(packets==0 && computes==0 && skillPackets==0, "hydration invoked gameplay callbacks");
    data.bLevel=255; data.sHealth=32767; data.sStamina=-12; data.bRiding=1;
    MountSystem::LoadHorseData(e,data, std::numeric_limits<uint32_t>::max());
    Check(MountSystem::GetHorseLevel(e)==30 && MountSystem::GetHorseHealth(e)==50 &&
        MountSystem::GetHorseStamina(e)==200 && MountSystem::IsHorseRiding(e), "malformed DB horse fields were not bounded");
    Check(MountSystem::GetHorseGrade(e)==2 && !MountSystem::CanUseHorseSkill(e), "migration changed fixed grade policy");
    MountSystem::SetHorseLevel(e,0);
    Check(MountSystem::GetHorseLevel(e)==0 && !MountSystem::IsHorseRiding(e) && !State(e).consume && !State(e).regen,
        "level zero left an active riding timer");
    MountSystem::SetHorseLevel(e,500);
    Check(MountSystem::GetHorseLevel(e)==30 && skillPackets==2, "native level/skill synchronization failed");
    MountSystem::ChangeHorseHealth(e,std::numeric_limits<int64_t>::min());
    MountSystem::ChangeHorseStamina(e,std::numeric_limits<int64_t>::max());
    Check(MountSystem::GetHorseHealth(e)==0 && MountSystem::GetHorseStamina(e)==200, "extreme stat input overflowed");
    Check(MountSystem::ReviveHorse(e) && !MountSystem::ReviveHorse(e), "revive gate changed");
    g_registry.destroy(e);
    Check(MountSystem::StoreHorseData(e).bLevel==0 && !MountSystem::StartRiding(e), "stale entity was accepted");
    const auto nonPlayer=g_registry.create(); MountSystem::SetHorseLevel(nonPlayer,30);
    Check(!g_registry.all_of<ecs::HorseRuntime>(nonPlayer), "non-character entity gained a horse");
}
void TimerLifecycle() {
    Reset(); auto e=Rider(10,15,2);
    Check(MountSystem::StartRiding(e), "native rider could not mount");
    auto consumed=State(e).consume;
    Check(consumed && !State(e).regen && event_time(consumed)==25*360, "consume timer not scheduled");
    Check(!MountSystem::StartRiding(e), "second mount duplicated a timer");
    Check(Run(25*360)==1 && MountSystem::GetHorseStamina(e)==1, "real consume tick failed");
    Check(Run(25*720)==1 && MountSystem::GetHorseStamina(e)==0 && !MountSystem::IsHorseRiding(e),
        "exhaustion did not dismount");
    Check(!State(e).consume && State(e).regen && unmounts==1, "exhaustion did not switch timer ownership");
    auto regen=State(e).regen;
    Check(Run(25*1440)==1 && MountSystem::GetHorseStamina(e)==1, "real regen tick failed");
    MountSystem::ChangeHorseStamina(e,1000);
    Check(Run(25*2160)==1 && !State(e).regen && !regen->q_el, "full stamina kept repeating timer");
    MountSystem::EnterHorse(e); auto retained=State(e).regen;
    g_registry.destroy(e);
    Check(retained->q_el && retained->q_el->bCancel, "entity destruction did not cancel its real timer");
    const auto replacement=Rider();
    Check(replacement!=e && entt::to_entity(replacement)==entt::to_entity(e), "fixture did not recycle generation");
    Check(retained->func(retained,0)==0 && MountSystem::GetHorseStamina(replacement)==5,
        "old timer modified a recycled character");
    event_destroy();
    Check(!retained->q_el, "shutdown left queue backlinks");
}
void ExpiryAndPolicies() {
    Reset(); auto e=Rider();
    State(e).healthDropTime=now-3*24*60*60;
    MountSystem::CheckHorseHealthDropTime(e,false);
    Check(MountSystem::GetHorseHealth(e)==14 && State(e).healthDropTime==uint32_t(now),
        "exact health deadline changed");
    ++now; MountSystem::CheckHorseHealthDropTime(e,false);
    Check(MountSystem::GetHorseHealth(e)==13, "elapsed health interval was not applied");
    State(e).healthDropTime=0;
    now=std::numeric_limits<int32_t>::max();
    MountSystem::CheckHorseHealthDropTime(e);
    Check(MountSystem::GetHorseHealth(e)==0 && !State(e).consume && !State(e).regen,
        "old horse expiry did not terminate safely");
    for (int policy=0;policy<7;++policy) {
        Reset(); e=Rider();
        if(policy==0) dead=true; if(policy==1) polymorphed=true; if(policy==2) arena=true;
        if(policy==3) armorVnum=11901; if(policy==4) war=true;
        if(policy==5) State(e).health=0; if(policy==6) State(e).stamina=0;
        Check(!MountSystem::StartRiding(e) && !State(e).consume && !MountSystem::IsHorseRiding(e),
            "mount policy was bypassed without a character shell");
    }
}
void CallbackSafety() {
    for(int action=0;action<3;++action) {
        Reset(); const auto e=g_registry.create(); g_registry.emplace<Player>(e);
        entt::entity replacement=entt::null;
        onConstruct=[&](entt::entity current) {
            if(action==0) { g_registry.destroy(current); replacement=g_registry.create(); }
            else if(action==1) g_registry.remove<ecs::HorseRuntime>(current);
            else throw std::runtime_error("construction failure");
        };
        bool threw=false;
        try { MountSystem::SetHorseLevel(e,10); } catch(const std::runtime_error&) {threw=true;}
        onConstruct={};
        Check(threw==(action==2), "construction exception changed");
        Check(action==2 || !g_registry.valid(e) || !g_registry.all_of<ecs::HorseRuntime>(e),
            "construction callback deletion was overwritten");
        if(action==0) Check(g_registry.valid(replacement), "replacement generation was destroyed");
    }
    Reset(); auto e=Rider(10,15,5);
    onPacket=[&](entt::entity current) { g_registry.destroy(current); };
    Check(MountSystem::StartRiding(e) && !g_registry.valid(e), "mount publication accessed a deleted rider");
    Check(summons==0, "mount continued after publication deleted its rider");

    Reset(); e=Rider();
    onPacket=[&](entt::entity current) {
        onPacket={};
        Check(MountSystem::StopRiding(current), "reentrant dismount failed");
        Check(MountSystem::StartRiding(current), "reentrant replacement ride failed");
    };
    Check(MountSystem::StartRiding(e) && MountSystem::IsHorseRiding(e) && State(e).consume,
        "old mount publication overwrote a replacement ride");
    Check(summons==2 && unmounts==1, "superseded mount continued world publication");

    Reset(); e=Rider();
    onMount=[&](entt::entity current) { g_registry.destroy(current); };
    Check(MountSystem::StartRiding(e) && !g_registry.valid(e), "mount model publication used a deleted rider");

    Reset(); e=Rider(); Check(MountSystem::StartRiding(e), "level publication fixture failed");
    onPacket=[&](entt::entity current) { g_registry.remove<ecs::HorseRuntime>(current); };
    MountSystem::SetHorseLevel(e,30);
    Check(g_registry.valid(e) && !g_registry.all_of<ecs::HorseRuntime>(e) && computes==0 && skillPackets==0,
        "level publication continued after horse component removal");

    Reset(); e=Rider(10,15,5); Check(MountSystem::StartRiding(e), "tick fixture could not mount");
    auto retained=State(e).consume; entt::entity replacement=entt::null;
    onTick=[&](entt::entity current) { g_registry.destroy(current); replacement=Rider(); };
    Check(Run(25*360)==1 && !g_registry.valid(e) && g_registry.valid(replacement) && !retained->q_el,
        "timer callback deletion/recycling rescheduled old work");

    Reset(); e=Rider(); Check(MountSystem::StartRiding(e), "replacement timer fixture failed");
    retained=State(e).consume;
    MountSystem::LoadHorseData(e,MountSystem::StoreHorseData(e));
    MountSystem::EnterHorse(e);
    const auto newer=State(e).consume;
    Check(newer && newer!=retained && retained->func(retained,0)==0 && State(e).consume==newer,
        "stale callback cleared a replacement timer");

    Reset(); e=Rider(); Check(MountSystem::StartRiding(e), "unmount fixture failed");
    onUnmount=[&](entt::entity current) { g_registry.destroy(current); };
    Check(MountSystem::StopRiding(e) && !g_registry.valid(e), "unmount quest callback used a dead rider");
}
}
// Horse logic and the event scheduler are production code; player/network,
// skill, quest and world-spawn services are controlled test seams.
LPHEART thecore_heart=&horseHeart;
time_t get_global_time() { return now; }
int passes_per_sec=25;
void ContinueOnFatalError() { throw std::runtime_error("unexpected fatal event"); }
void ShutdownOnFatalError() { throw std::runtime_error("unexpected shutdown"); }
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return spdlog::default_logger(); }
namespace ecs::PlayerRuntime {
bool IsPC(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<Player>(e); }
uint32_t GetPlayerID(entt::entity e) { CheckLive(e); return entt::to_integral(e)+1; }
int32_t GetMapIndex(entt::entity e) { CheckLive(e); return 1; }
uint32_t GetRaceNum(entt::entity e) { CheckLive(e); return 20101; }
}
namespace CombatSystem {
bool IsDead(entt::entity e) { CheckLive(e); return dead; }
bool IsStun(entt::entity e) { CheckLive(e); return stunned; }
}
namespace AffectSystem {
bool IsPolymorphed(entt::entity e) { CheckLive(e); return polymorphed; }
bool RemoveAffect(entt::entity e,uint32_t) { CheckLive(e); return true; }
}
bool CArenaManager::IsArenaMap(uint32_t) { return arena; }
CWarMap* ecs::SocialSystem::GetWarMap(entt::entity e) { CheckLive(e); return war ? reinterpret_cast<CWarMap*>(uintptr_t(1)) : nullptr; }
namespace ItemSystem {
entt::entity GetWearItem(entt::entity e,uint8_t) { CheckLive(e); return armorVnum ? e : entt::entity(entt::null); }
uint32_t GetItemVnum(entt::entity) { return armorVnum; }
bool IsValidItem(entt::entity e) { return g_registry.valid(e) && armorVnum; }
}
namespace MountSystem {
bool IsHorseRiding(entt::entity e) { const auto* s=g_registry.valid(e)?g_registry.try_get<ecs::MountState>(e):nullptr; return s&&s->horseRiding; }
bool IsRiding(entt::entity e) { return IsHorseRiding(e)||GetMountVnum(e)!=0; }
uint32_t GetMountVnum(entt::entity e) { const auto* s=g_registry.valid(e)?g_registry.try_get<ecs::MountState>(e):nullptr; return s?s->mountVnum:0; }
void SetMountVnum(entt::entity e,uint32_t vnum) { CheckLive(e); g_registry.get<ecs::MountState>(e).mountVnum=vnum; if(onMount){auto fn=onMount;fn(e);} }
entt::entity GetSummonedHorse(entt::entity e) { const auto* s=g_registry.valid(e)?g_registry.try_get<ecs::SummonedHorse>(e):nullptr; return s?s->horse:entt::entity(entt::null); }
void SetSummonedHorse(entt::entity e,entt::entity horse) { CheckLive(e); g_registry.get_or_emplace<ecs::SummonedHorse>(e).horse=horse; }
uint32_t GetMyHorseVnum(entt::entity e) { CheckLive(e); return 20101; }
void SummonHorse(entt::entity e,bool summon,bool,uint32_t,const char*) { CheckLive(e); ++summons; if(!summon)SetSummonedHorse(e,entt::null); }
}
void ecs::ChatSystem::Send(entt::entity e,uint8_t,const char*,...) { CheckLive(e); ++packets; if(onPacket){auto fn=onPacket;fn(e);} }
void ecs::ChatSystem::SendNew(entt::entity e,uint8_t,uint32_t,const char*,...) { CheckLive(e); }
void ecs::PointSystem::Compute(entt::entity e) { CheckLive(e); ++computes; }
void ecs::PointSystem::Change(entt::entity e,uint8_t,int64_t,bool,bool,bool) { CheckLive(e); }
void SkillSystem::SetSkillLevel(entt::entity e,uint32_t,uint8_t) { CheckLive(e); }
void SkillSystem::SendSkillLevelPacket(entt::entity e) { CheckLive(e); ++skillPackets; }
void NetworkSyncSystem::UpdatePacket(entt::entity e) { CheckLive(e); }
quest::CQuestManager::CQuestManager()=default;
quest::CQuestManager::~CQuestManager()=default;
quest::NPC::NPC()=default;
quest::NPC::~NPC()=default;
quest::PC::PC():m_RunningQuestState(nullptr){}
quest::PC::~PC()=default;
void quest::CQuestManager::Unmount(unsigned int pid) { ++unmounts; if(onUnmount){auto fn=onUnmount;fn(entt::entity(pid-1));} }
int main() {
    CArenaManager arenaManager; quest::CQuestManager quests;
    g_registry.on_construct<ecs::HorseRuntime>().connect<&Constructed>();
    g_dispatcher.sink<ecs::EvHorseStaminaConsume>().connect<&Ticked>();
    try {
        NativeData(); TimerLifecycle(); ExpiryAndPolicies(); CallbackSafety(); Reset();
        std::cout<<"Horse runtime: "<<checks<<" checks passed\n";
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n'; Reset(); return 1;
    }
}
