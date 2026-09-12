#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/OXEvent.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/log.h"
#include "../../SRC/Server/GameServer/affect.h"
#include "../../SRC/Server/GameServer/cmd.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ViewSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
namespace {
HEART oxHeart {};
int checks=0, status=OXEVENT_OPEN;
struct Player { uint32_t pid; int32_t map=OXEVENT_MAP_INDEX, x=896500, y=24600; bool eligible=true; };
std::vector<entt::entity> gifts, warps, audience, winners;
std::vector<uint32_t> notices, logs;
std::vector<std::pair<entt::entity,uint8_t>> effects;
int chats=0, views=0;
std::function<void(entt::entity)> onGive,onWarp,onEffect,onView,onShow,onChat,onLog;
std::function<void(uint32_t)> onNotice;
CAffect eligibleAffect {};
void Check(bool value,const char* why) { ++checks;if(!value)throw std::runtime_error(why); }
Player& Data(entt::entity e) { Check(g_registry.valid(e) && g_registry.all_of<Player>(e),"service received retired/non-player entity");return g_registry.get<Player>(e); }
void Invoke(const std::function<void(entt::entity)>& callback,entt::entity e) {if(callback){auto copy=callback;copy(e);}}
entt::entity Make(uint32_t pid=1) {const auto e=g_registry.create();g_registry.emplace<Player>(e,pid);return e;}
int Run(int pulse) {oxHeart.pulse=pulse;return event_process(pulse);}
void Reset(COXEventManager& ox) {
    onGive=onWarp=onEffect=onView=onShow=onChat=onLog={};onNotice={};
    ox.Initialize();g_registry.clear();event_destroy();oxHeart.pulse=0;oxHeart.passes_per_sec=25;
    status=OXEVENT_OPEN;gifts.clear();warps.clear();audience.clear();winners.clear();
    notices.clear();logs.clear();effects.clear();chats=views=0;
}
void Membership(COXEventManager& ox) {
    Reset(ox);const auto first=Make(1), other=Make(2);
    Check(!ox.Enter(entt::null),"null entry accepted");
    auto nonplayer=g_registry.create();Check(!ox.Enter(nonplayer),"non-player entry accepted");
    Data(other).map=1;Check(!ox.Enter(other),"foreign-map entry accepted");Data(other).map=OXEVENT_MAP_INDEX;
    Check(ox.Enter(first) && ox.Enter(first) && ox.Enter(other) && ox.GetAttenderCount()==2,"entry idempotence/count failed");
    g_registry.destroy(first);const auto reconnected=Make(1);
    Check(entt::to_entity(first)==entt::to_entity(reconnected),"fixture did not recycle player index");
    Check(ox.GetAttenderCount()==1,"reconnected PID inherited attendance");
    ox.GiveItemToAttender(19,2);
    Check(gifts==std::vector<entt::entity>{other},"recycled generation received old winner reward");
    Data(other).map=1;Check(ox.GetAttenderCount()==0,"departed player remained an attender");
    Check(ox.Enter(reconnected),"explicit re-entry failed");
    const auto watcher=Make(3);Data(watcher).x=896300;Data(watcher).y=28900;
    Check(ox.Enter(watcher) && ox.GetAttenderCount()==1,"audience joined winner list");
    ox.CloseEvent();
    Check(ox.GetAttenderCount()==0 && warps.size()==2,"close missed a live participant or warped a departed player");
}
void Answers(COXEventManager& ox) {
    Reset(ox);const auto yes=Make(1),no=Make(2),retired=Make(3);
    ox.Enter(yes);ox.Enter(no);ox.Enter(retired);
    Data(yes).x=896600;Data(yes).y=22900;Data(no).x=896300;Data(no).y=26400;
    g_registry.destroy(retired);const auto replacement=Make(3);
    ox.CheckAnswer(true);
    Check(ox.GetAttenderCount()==1 && effects.size()==2,"answer classification/count failed");
    Check(std::find(effects.begin(),effects.end(),std::make_pair(yes,uint8_t(SE_SUCCESS)))!=effects.end() &&
        std::find(effects.begin(),effects.end(),std::make_pair(no,uint8_t(SE_FAIL)))!=effects.end(),
        "answer rectangle boundaries changed");
    ox.WarpToAudience();Check(audience==std::vector<entt::entity>{no},"wrong elimination target warped");
    ox.WarpToAudience();Check(audience.size()==1,"missed batch replayed");
    ox.LogWinner();Check(winners==std::vector<entt::entity>{yes},"winner selection rebound a retired entity");

    Reset(ox);const auto a=Make(4),b=Make(5);ox.Enter(a);ox.Enter(b);
    Data(a).x=Data(b).x=895000;
    onView=[&](entt::entity e){g_registry.destroy(e);};
    ox.CheckAnswer(false);
    Check(effects.empty() && ox.GetAttenderCount()==0,"success effect used a player deleted by view publication");

    Reset(ox);const auto c=Make(6),d=Make(7);ox.Enter(c);ox.Enter(d);
    onEffect=[&](entt::entity){onEffect={};ox.CloseEvent();};
    ox.CheckAnswer(true);
    Check(effects.size()==1 && ox.GetAttenderCount()==0,"answer scan continued after event closure");
    ox.WarpToAudience();Check(audience.empty(),"close left a missed-player batch behind");
}
void Rewards(COXEventManager& ox) {
    Reset(ox);const auto a=Make(1),b=Make(2);ox.Enter(a);ox.Enter(b);
    Check(!ox.GiveItemToAttender(0,1) && !ox.GiveItemToAttender(19,0),"empty grant accepted");
    onGive=[&](entt::entity e){Check(!ox.GiveItemToAttender(19,1),"nested reward call accepted");};
    Check(ox.GiveItemToAttender(19,2) && gifts.size()==2 && logs.size()==2,"reward reentry duplicated batch");
    gifts.clear();logs.clear();
    uint32_t retiredPID=0;
    onGive=[&](entt::entity e){onGive={};retiredPID=Data(e).pid;g_registry.destroy(e);Make(retiredPID);};
    Check(ox.GiveItemToAttender(19,1) && gifts.size()==2 && logs.size()==2,"disconnect broke value-snapshot audit");
    Check(logs.front()==retiredPID,"audit switched to replacement recipient");

    Reset(ox);const auto c=Make(3),d=Make(4);ox.Enter(c);ox.Enter(d);
    onGive=[&](entt::entity){onGive={};ox.CloseEvent();};
    ox.GiveItemToAttender(19,1);
    Check(gifts.size()==1 && logs.size()==1 && ox.GetAttenderCount()==0,"closed cohort continued granting items");

    Reset(ox);const auto f=Make(6);ox.Enter(f);
    onGive=[&](entt::entity){throw std::runtime_error("fixture");};
    bool threw=false;try {ox.GiveItemToAttender(19,1);}catch(const std::runtime_error&){threw=true;}
    onGive={};Check(threw && ox.GiveItemToAttender(19,1),"exception left reward guard permanently locked");
#ifdef ENABLE_BLOCK_MULTIFARM
    gifts.clear();Data(f).eligible=false;ox.GiveItemToAttender(19,1);
    Check(gifts.empty(),"multifarm reward restriction was lost");
#endif
}
void PublicationAndTimers(COXEventManager& ox) {
    Reset(ox);const auto admin=Make(1);
    ox.AddQuiz(0,"100",true);ox.AddQuiz(0,"101",false);
    onChat=[&](entt::entity){onChat={};ox.ClearQuiz();};
    Check(ox.ShowQuizList(admin) && chats==3,"quiz list retained invalidated vector references");
    ox.AddQuiz(0,"100",true);onChat=[&](entt::entity e){g_registry.destroy(e);};
    Check(!ox.ShowQuizList(admin),"quiz list continued with retired admin");
    onChat={};

    Reset(ox);ox.AddQuiz(0,"100",true);
    Check(ox.Quiz(1,15),"level equal to vector size was not clamped");
    Run(1);Check(notices.back()==579,"initial quiz timer did not start at countdown");
    ox.AddQuiz(0,"101",false);Check(ox.Quiz(0,15),"replacement quiz failed");
    Run(2);Check(notices.back()==579,"replacement quiz inherited previous timer stage");
    Run(252);Check(notices.back()==582,"answer timer stage failed");
    Run(377);Check(status==OXEVENT_CLOSE && notices.back()==583,"answer cleanup stage failed");
    ox.AddQuiz(0,"102",true);ox.Quiz(0,15);
    const auto noticeCount=notices.size();ox.Initialize();Run(1000);
    Check(notices.size()==noticeCount,"Initialize abandoned a live quiz timer");
#ifdef TEXTS_IMPROVEMENT
    ox.AddQuiz(0,"invalid-id",true);
    Check(!ox.Quiz(0,30),"invalid text ID accepted");
    ox.ClearQuiz();ox.AddQuiz(0,"99999999999999999999",true);
    Check(!ox.Quiz(0,30),"overflowing text ID accepted");
#endif

    Reset(ox);const auto player=Make(1);ox.Enter(player);ox.CheckAnswer(true);
    ox.CheckAnswer(true);
    onShow=[&](entt::entity){ox.WarpToAudience();};
    ox.WarpToAudience();Check(audience.size()==1,"audience warp replayed detached batch");
    onWarp=[&](entt::entity){Check(ox.CloseEvent(),"nested close failed");Check(!ox.Enter(Make(8)),"entry accepted during close");};
    ox.CloseEvent();Check(warps.size()==1 && ox.GetAttenderCount()==0,"close reentered old cohort");

    Reset(ox);const auto first=Make(1),second=Make(2);ox.Enter(first);ox.Enter(second);
    onEffect=[&](entt::entity){ox.CheckAnswer(true);};
    ox.CheckAnswer(true);onEffect={};ox.WarpToAudience();
    Check(effects.size()==2 && audience.size()==2,"nested answer evaluation lost an elimination batch");
}
} // namespace

// Production OX manager, event scheduler and packet buffers; game/quest/DB
// services are controlled doubles. There are no CHARACTER allocations.
LPHEART thecore_heart=&oxHeart;
int passes_per_sec=25;
uint32_t g_start_position[4][2]={{0,0},{100,200},{300,400},{500,600}};
void ContinueOnFatalError() {throw std::runtime_error("fatal event");}
void ShutdownOnFatalError() {throw std::runtime_error("shutdown");}
CAsyncSQL::CAsyncSQL()=default;CAsyncSQL::~CAsyncSQL()=default;
CSemaphore::CSemaphore()=default;CSemaphore::~CSemaphore()=default;
LogManager::LogManager():m_bIsConnect(false){} LogManager::~LogManager()=default;
void LogManager::CharLog(entt::entity e,uint32_t,const char*,const char*) {Data(e);winners.push_back(e);Invoke(onLog,e);}
void LogManager::ItemLog(uint32_t pid,uint32_t,uint32_t count,uint32_t,const char*,const char*,const char*,uint32_t) {Check(count>0,"empty log");logs.push_back(pid);}
namespace quest {
CQuestManager::CQuestManager()=default;CQuestManager::~CQuestManager()=default;
NPC::NPC()=default;NPC::~NPC()=default;PC::PC():m_RunningQuestState(nullptr){}PC::~PC()=default;
int CQuestManager::GetEventFlag(const std::string&) {return status;}
void CQuestManager::RequestSetEventFlag(const std::string&,int value) {status=value;}
}
namespace ecs::PlayerRuntime {
bool IsPC(entt::entity e) {return g_registry.valid(e) && g_registry.all_of<Player>(e);}
uint32_t GetPlayerID(entt::entity e) {return Data(e).pid;}
uint32_t GetPacketVID(entt::entity e) {return Data(e).pid+1000;}
std::string_view GetName(entt::entity e) {Data(e);return "ox-player";}
int32_t GetMapIndex(entt::entity e) {return Data(e).map;}
int32_t GetX(entt::entity e) {return Data(e).x;}
int32_t GetY(entt::entity e) {return Data(e).y;}
int32_t GetZ(entt::entity e) {Data(e);return 0;}
uint8_t GetEmpire(entt::entity e) {Data(e);return 1;}
LPDESC GetDesc(entt::entity e) {Data(e);return nullptr;}
}
void ecs::ChatSystem::SendNew(entt::entity e,uint8_t,uint32_t,const char*,...) {Data(e);++chats;Invoke(onChat,e);}
void SendNoticeNew(uint8_t,uint8_t,int32_t,uint32_t id,const char*,...) {notices.push_back(id);if(onNotice){auto fn=onNotice;fn(id);}}
void NetworkSyncSystem::BroadcastEffect(entt::registry&,entt::entity e,uint8_t type) {Data(e);effects.emplace_back(e,type);Invoke(onEffect,e);}
void ecs::ViewSystem::PacketView(entt::entity e,const void*,int,entt::entity) {Data(e);++views;Invoke(onView,e);}
bool ecs::MovementSystem::WarpSet(entt::entity e,int32_t,int32_t,int32_t) {Data(e);warps.push_back(e);Invoke(onWarp,e);if(g_registry.valid(e))Data(e).map=1;return true;}
bool ecs::MovementSystem::Show(entt::entity e,int32_t,int32_t x,int32_t y,int32_t,bool) {Data(e);audience.push_back(e);Invoke(onShow,e);if(g_registry.valid(e)){Data(e).x=x;Data(e).y=y;}return true;}
entt::entity ItemSystem::AutoGiveItemEcs(entt::entity e,uint32_t,uint32_t count,int,bool,bool) {Data(e);Check(count>0,"empty gift");gifts.push_back(e);Invoke(onGive,e);return entt::null;}
CAffect* AffectSystem::FindAffect(entt::entity e,uint32_t,uint8_t) {return Data(e).eligible?&eligibleAffect:nullptr;}
int main() {
    quest::CQuestManager quest;LogManager log;COXEventManager ox;
    try {
        Membership(ox);Answers(ox);Rewards(ox);PublicationAndTimers(ox);Reset(ox);
        std::cout<<"OX runtime: "<<checks<<" checks passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';Reset(ox);return 1;}
}
