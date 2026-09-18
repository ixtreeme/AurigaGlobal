#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/questevent.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/target.h"
#include "../../SRC/Server/GameServer/party.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/desc_client.h"
#include "../../SRC/Server/GameServer/desc_manager.h"
#include "../../SRC/Server/GameServer/sectree_manager.h"
#include "../../SRC/Server/GameServer/shop_manager.h"
#include "../../SRC/Server/GameServer/shop.h"
#include "../../SRC/Server/GameServer/attr_transfer.h"
#include "../../SRC/Server/GameServer/OrcsDungeon.h"
#include "../../SRC/Server/GameServer/TritonTempleDungeon.h"
#include "../../SRC/Server/GameServer/ValentineDungeon.h"
#include "../../SRC/Server/GameServer/RuneDungeon.h"
#include "../../SRC/Server/GameServer/PyramidDungeonRazor93.h"
#include "../../SRC/Server/GameServer/NightmareDungeonRazor93.h"
#include "../../SRC/Server/GameServer/Halloween2022Dungeon.h"
#include "../../SRC/Server/GameServer/VikingDungeon.h"
#include "../../SRC/Server/GameServer/EasterDungeon.h"
#include "../../SRC/Server/GameServer/ecs/services/SpatialService.hpp"
#include "../../SRC/Server/GameServer/constants.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/quest_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/vital_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/ai_components.hpp"
#include "../../SRC/Server/GameServer/affect.h"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SessionSystem.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
entt::dispatcher g_dispatcher;
int test_server=0;
namespace quest {
int npc_lock(lua_State*);
int npc_unlock(lua_State*);
}
namespace {
int checks=0, clicks=0, chats=0, attrCalls=0, affects=0, saves=0, pidLookups=0;
bool hasTarget=false, hasChat=false;
TargetInfo targetInfo;
std::map<uint32_t,entt::entity> players;
std::vector<uint32_t> gifts;
std::vector<int64_t> experience;
std::function<void()> onGive, onTarget, onChat, onAttr, onPublish;
std::function<void(entt::entity)> onConstruct;
struct TargetData {
    int32_t x=0,y=0,level=1,maxHP=100;
    int64_t hp=100;
    uint8_t empire=1;
    bool dead=false,immune=false,member=true;
    std::set<uint32_t> affects;
};
SECTREE* searchTree=nullptr;
std::vector<entt::entity> searchCandidates;
std::function<void(entt::entity)> onMember;
uint32_t nextExp[PLAYER_MAX_LEVEL_CONST+1] {};
void Check(bool okay,const char* why) { ++checks; if(!okay)throw std::runtime_error(why); }
void CheckLive(entt::entity e) { Check(g_registry.valid(e),"service received a stale entity"); }
void Constructed(entt::registry&,entt::entity e) { if(onConstruct){auto fn=onConstruct;fn(e);} }
entt::entity Character(uint32_t pid=0,uint32_t vid=100,uint32_t race=101) {
    auto e=g_registry.create();
    g_registry.emplace<ecs::CharacterType>(e, static_cast<uint8_t>(pid?CHAR_TYPE_PC:CHAR_TYPE_NPC));
    g_registry.emplace<ecs::VIDComponent>(e,vid);
    g_registry.emplace<ecs::RaceState>(e,race,0u);
    g_registry.emplace<ecs::PlayerName>(e,"quest-fixture");
    if(pid) {
        g_registry.emplace<ecs::TagPC>(e);
        g_registry.emplace<ecs::PlayerID>(e,pid);
        g_registry.emplace<ecs::Experience>(e,90,100);
        players[pid]=e;
    }
    return e;
}
void Reset() {
    onGive=onTarget=onChat=onAttr=onPublish={};onConstruct={};
    searchTree=nullptr;searchCandidates.clear();onMember={};
    g_registry.clear();players.clear();gifts.clear();experience.clear();
    clicks=chats=attrCalls=affects=saves=pidLookups=0;hasTarget=hasChat=false;nextExp[1]=100;
}
void References() {
    Reset();quest::CQuestManager q;
    const auto pc=Character(1), other=Character(2), npc=Character();
    Check(q.GetPC(1) && q.GetCurrentCharacter()==pc,"PC context did not bind an entity");
    Check(ecs::PlayerRuntime::SetQuestNPC(pc,npc) && q.GetCurrentNPCEntity()==npc,"NPC entity was not retained");
    Check(ecs::PlayerRuntime::SetQuestNPCLockOwner(npc,pc),"NPC lock was rejected");
    Check(ecs::PlayerRuntime::SetQuestNPC(npc,other) &&
        ecs::PlayerRuntime::GetQuestNPCLockOwner(npc)==pc,"target and lock owner share storage");
    const auto vid=g_registry.get<ecs::VIDComponent>(npc).value;
    g_registry.destroy(npc);
    const auto replacement=Character(0,vid);
    Check(entt::to_entity(replacement)==entt::to_entity(npc),"fixture did not recycle the NPC slot");
    Check(q.GetCurrentNPCEntity()==entt::null,"reused NPC VID/index inherited the quest target");
    Check(!ecs::PlayerRuntime::SetQuestNPC(pc,npc),"stale target accepted");
    const auto item=g_registry.create();
    ecs::PlayerRuntime::SetQuestItem(pc,item);
    Check(ecs::PlayerRuntime::GetQuestItem(pc)==item && ecs::PlayerRuntime::SetQuestBy(pc,42) &&
        ecs::PlayerRuntime::GetQuestBy(pc)==42,"native quest item/by context failed");
    g_registry.destroy(item);(void)g_registry.create();
    Check(ecs::PlayerRuntime::GetQuestItem(pc)==entt::null,"recycled item inherited quest selection");
    Check(ecs::PlayerRuntime::SetQuestNPC(pc,replacement),"replacement could not be explicitly selected");
    Check(ecs::PlayerRuntime::SetQuestNPCLockOwner(replacement,pc),"replacement NPC lock failed");
    g_registry.destroy(pc);const auto reconnected=Character(1);
    Check(q.GetCurrentCharacter()==entt::null && ecs::PlayerRuntime::GetQuestNPCLockOwner(replacement)==entt::null,
        "reconnected PID/entity index inherited an old context or lock");
    Check(q.GetPC(1) && q.GetCurrentCharacter()==reconnected,"explicit reconnect binding failed");
    Check(!q.GetPC(999) && q.GetCurrentCharacter()==entt::null && !q.GetCurrentPC(),
        "failed selection left the previous player's context active");
    q.GetPC(1); q.DisconnectPC(reconnected);
    Check(q.GetCurrentCharacter()==entt::null && !q.GetCurrentPC(),"disconnect retained current quest PC");
}
void OtherPCBlocks() {
    Reset();quest::CQuestManager q;
    auto one=Character(1),two=Character(2),three=Character(3);
    q.GetPC(1);auto* root=q.GetCurrentPC();
    q.BeginOtherPCBlock(2);q.BeginOtherPCBlock(3);
    Check(q.GetCurrentCharacter()==three && q.GetOtherPCBlockRootPC()==root,"nested selection failed");
    q.EndOtherPCBlock();Check(q.GetCurrentCharacter()==two,"nested restore failed");
    q.DisconnectPC(one);g_registry.destroy(one);auto replacement=Character(1);
    q.EndOtherPCBlock();
    Check(q.GetCurrentCharacter()==entt::null && !q.IsInOtherPCBlock() && !q.GetOtherPCBlockRootPC(),
        "PC stack rebound an old PID to a replacement generation");
    q.GetPC(2);q.BeginOtherPCBlock(999);
    Check(q.GetCurrentCharacter()==entt::null,"missing other-PC target retained caller");
    q.EndOtherPCBlock();Check(q.GetCurrentCharacter()==two,"missing target could not restore caller");
    q.EndOtherPCBlock();Check(q.GetCurrentCharacter()==two,"empty stack changed the context");
}
void LuaAndDispatch(lua_State* L) {
    Reset();quest::CQuestManager q;
    auto pc=Character(1),member=Character(2),npc=Character();
    q.GetPC(1)->SetLoaded();q.GetPC(2)->SetLoaded();q.GetPC(1);
    ecs::PlayerRuntime::SetQuestNPC(pc,npc);
    lua_settop(L,0);quest::npc_lock(L);
    Check(lua_toboolean(L,-1) && ecs::PlayerRuntime::GetQuestNPCLockOwner(npc)==pc,"Lua native NPC lock failed");
    q.GetPC(2);ecs::PlayerRuntime::SetQuestNPC(member,npc);
    lua_settop(L,0);quest::npc_lock(L);Check(!lua_toboolean(L,-1),"foreign NPC lock was stolen");
    quest::npc_unlock(L);Check(ecs::PlayerRuntime::GetQuestNPCLockOwner(npc)==pc,"foreign unlock succeeded");
    q.GetPC(1);quest::npc_unlock(L);
    Check(ecs::PlayerRuntime::GetQuestNPCLockOwner(npc)==entt::null,"owner could not unlock");
    q.AttrIn(pc,member,5);
    Check(attrCalls==1 && q.GetCurrentPartyMemberEntity()==member,"attribute event lost its native member");
    q.GetPC(999);Check(q.GetCurrentPartyMemberEntity()==entt::null,"failed PC context exposed old member");
    q.GetPC(pc);
    g_registry.destroy(member);const auto replacement=Character(2);
    Check(q.GetCurrentPartyMemberEntity()==entt::null,"member context followed a recycled entity");
    q.AttrOut(pc,member,5);Check(attrCalls==1,"attribute event accepted a stale member");

    // Seed the real manager NPC entry via its kill event (scripts are doubles).
    q.Kill(1,101);hasTarget=true;
    Check(!q.Click(pc,entt::null),"null click accepted");
    onTarget=[&]{ g_registry.destroy(npc); };
    Check(!q.Click(pc,npc) && clicks==0,"click continued after target deletion");
    onTarget={};npc=Character();hasChat=true;
    onChat=[&]{ q.DisconnectPC(pc);g_registry.destroy(pc); };
    Check(!q.Click(pc,npc) && clicks==0,"chat fallback dereferenced a disconnected quest PC");
}
void RewardBatches() {
    Reset();quest::CQuestManager q;
    auto e=Character(7);auto* pc=q.GetPC(7);
    pc->GiveItem("a",1001,2);pc->GiveItem("b",1002,1);
    onGive=[&]{ pc->Reward(e); };
    pc->Reward(e);
    Check(gifts==std::vector<uint32_t>({1001,1002}) && !pc->HasReward(),"reentry duplicated the reward batch");
    gifts.clear();pc->GiveItem("c",1003,1);
    onGive=[&]{onGive={};pc->GiveItem("new",1004,1);};
    pc->Reward(e);
    Check(gifts==std::vector<uint32_t>({1003}) && pc->HasReward(),"publication discarded a newly queued reward");
    pc->Reward(e);Check(gifts.back()==1004 && !pc->HasReward(),"new reward batch was not delivered");
    onGive={};
    pc->GiveExp("eq",10);pc->Reward(e);Check(experience.back()==10,"exact EXP threshold changed");
    g_registry.get<ecs::Experience>(e).current=90;
    pc->GiveExp("cap",11);pc->Reward(e);Check(experience.back()==9,"strict EXP overshoot cap changed");
    g_registry.get<ecs::Experience>(e).current=100;
    const auto count=experience.size();pc->GiveExp("full",UINT32_MAX);pc->Reward(e);
    Check(experience.size()==count,"EXP cap underflowed at the threshold");
    g_registry.get<ecs::Experience>(e).current=101;
    pc->GiveExp("over",UINT32_MAX);pc->Reward(e);
    Check(experience.size()==count,"EXP cap underflowed above the threshold");
    pc->GiveItem("wrong",1005,1);
    const auto stranger=Character(8);pc->Reward(stranger);
    Check(pc->HasReward(),"reward was claimed by a different player");
    gifts.clear();
    pc->GiveItem("last",1006,1);
    onGive=[&]{onGive={};q.DisconnectPC(e);g_registry.destroy(e);Character(7);};
    pc->Reward(e);
    Check(gifts==std::vector<uint32_t>({1005}),"reward continued into a replacement player generation");
}
void RewardCompletion() {
    Reset();quest::CQuestManager q;
    auto e=Character(11);auto* pc=q.GetPC(e);
    pc->GiveItem("completion",1001,1);
    onGive=[&]{onGive={};q.DisconnectPC(e);g_registry.destroy(e);Character(11);};
    pc->EndRunning();
    Check(saves==0 && gifts.size()==1,"EndRunning touched/saved a disconnected reward recipient");
    e=players[11];pc=q.GetPC(e);
    pc->GiveItem("pending",1002,1);
    onGive=[&]{
        onGive={};pc->SetFlag(".claimed",1);pc->GiveExp("claimed",10);
    };
    pc->EndRunning();
    Check(saves==1 && pc->HasReward(),"EndRunning discarded a reward notification queued by a callback");
    const auto oldCount=experience.size();
    onPublish=[&]{onPublish={};q.DisconnectPC(e);g_registry.destroy(e);Character(11);};
    pc->Reward(e);
    Check(experience.size()==oldCount,"notification callback continued into stale reward recipient");
}
void ConstructionCallbacks() {
    for(int operation=0;operation<4;++operation) for(int mode=0;mode<3;++mode) {
        Reset();const auto pc=Character(1),npc=Character(),item=g_registry.create();
        const auto e=operation==1?npc:pc;
        onConstruct=[&](entt::entity current){
            if(mode==0)g_registry.destroy(current);
            else if(mode==1)g_registry.remove<ecs::QuestContext>(current);
            else throw std::runtime_error("fixture");
        };
        bool okay=false,threw=false;
        try {
            switch(operation) {
                case 0:okay=ecs::PlayerRuntime::SetQuestNPC(e,npc);break;
                case 1:okay=ecs::PlayerRuntime::SetQuestNPCLockOwner(e,pc);break;
                case 2:okay=ecs::PlayerRuntime::SetQuestBy(e,42);break;
                case 3:ecs::PlayerRuntime::SetQuestItem(e,item);break;
            }
        } catch(const std::runtime_error&) {threw=true;}
        onConstruct={};
        Check(!okay && threw==(mode==2),"quest context construction callback was overwritten");
        if(mode<2)Check(!g_registry.valid(e) || !g_registry.all_of<ecs::QuestContext>(e),
            "quest setter recreated context removed by its construction observer");
    }
    Reset();const auto pc=Character(1),npc=Character();
    onConstruct=[&](entt::entity){g_registry.destroy(pc);};
    Check(!ecs::PlayerRuntime::SetQuestNPCLockOwner(npc,pc),"lock retained owner destroyed during construction");
    onConstruct={};
}
} // namespace

// Quest references, manager/PC, and NPC Lua entry points are production code.
// Character data, network, script execution, rewards and party are test seams.
// No CHARACTER objects or legacy character conversion are used by this fixture.
const uint32_t* exp_table=nextExp;
CHARACTER_MANAGER::CHARACTER_MANAGER()=default;
CHARACTER_MANAGER::~CHARACTER_MANAGER()=default;
entt::entity CHARACTER_MANAGER::FindEntityByPID(uint32_t pid) {
    ++pidLookups;
    const auto it=players.find(pid);return it==players.end()?entt::entity(entt::null):it->second;
}
CTargetManager::CTargetManager()=default;
CTargetManager::~CTargetManager()=default;
TargetInfo* CTargetManager::GetTargetInfo(uint32_t,int,int) {return hasTarget?&targetInfo:nullptr;}
namespace quest {
NPC::NPC()=default;NPC::~NPC()=default;
bool NPC::OnKill(PC&) {return false;}
bool NPC::OnPartyKill(PC&) {return false;}
bool NPC::OnAttrIn(PC&) {++attrCalls;if(onAttr){auto fn=onAttr;fn();}return true;}
bool NPC::OnAttrOut(PC&) {++attrCalls;return true;}
bool NPC::OnTarget(PC&,uint32_t,const char*,const char*,bool& result) {result=false;if(onTarget){auto fn=onTarget;fn();}return false;}
bool NPC::HasChat() {return hasChat;}
bool NPC::OnChat(PC&) {++chats;if(onChat){auto fn=onChat;fn();}return false;}
bool NPC::OnClick(PC&) {++clicks;return false;}
void CancelTimerEvent(LPEVENT*) {throw std::runtime_error("unexpected timer");}
}
LPPARTY ecs::SocialSystem::GetParty(entt::entity) {return nullptr;}
uint32_t CParty::GetLeaderPID() {throw std::runtime_error("unexpected party");}
entt::entity CParty::GetLeader() {throw std::runtime_error("unexpected party");}
int32_t ecs::PointSystem::GetLevel(entt::entity e) {
    CheckLive(e);const auto* data=g_registry.try_get<TargetData>(e);return data?data->level:1;
}
void ecs::PointSystem::Change(entt::entity e,uint8_t point,int64_t amount,bool,bool,bool) {
    CheckLive(e);Check(point==POINT_EXP,"unexpected point");experience.push_back(amount);
    g_registry.get<ecs::Experience>(e).current+=amount;
}
entt::entity ItemSystem::AutoGiveItemEcs(entt::entity e,uint32_t vnum,uint32_t count,int,bool,bool) {
    CheckLive(e);Check(count>0,"empty reward");gifts.push_back(vnum);
    if(onGive){auto fn=onGive;fn();}return entt::null;
}
void ecs::ChatSystem::SendNew(entt::entity e,uint8_t,uint32_t,const char*,...) {
    CheckLive(e);if(onPublish){auto fn=onPublish;fn();}
}
bool AffectSystem::RemoveAffect(entt::entity e,uint32_t) {CheckLive(e);++affects;return true;}
bool AffectSystem::AddAffect(entt::entity e,uint32_t,uint8_t,int32_t,uint32_t,int32_t,int32_t,bool,bool) {
    CheckLive(e);++affects;return true;
}
namespace {
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected external service"); }
}
namespace ecs::PlayerRuntime {
bool IsValid(entt::entity e) { return g_registry.valid(e); }
bool IsPC(entt::entity e) { return IsValid(e) && g_registry.all_of<ecs::TagPC>(e); }
bool IsNPC(entt::entity e) {
    const auto* type=IsValid(e)?g_registry.try_get<ecs::CharacterType>(e):nullptr;
    return type && type->value!=CHAR_TYPE_PC;
}
uint32_t GetPlayerID(entt::entity e) {
    const auto* id=IsValid(e)?g_registry.try_get<ecs::PlayerID>(e):nullptr;return id?id->pid:0;
}
uint32_t GetPacketVID(entt::entity e) {
    const auto* id=IsValid(e)?g_registry.try_get<ecs::VIDComponent>(e):nullptr;return id?id->value:0;
}
uint32_t GetRaceNum(entt::entity e) {
    const auto* race=IsValid(e)?g_registry.try_get<ecs::RaceState>(e):nullptr;
    return race?(race->polymorphRace?race->polymorphRace:race->baseRace):0;
}
std::string_view GetName(entt::entity e) {
    const auto* name=IsValid(e)?g_registry.try_get<ecs::PlayerName>(e):nullptr;return name?name->value:std::string_view{};
}
uint32_t GetExp(entt::entity e) {CheckLive(e);return static_cast<uint32_t>(g_registry.get<ecs::Experience>(e).current);}
uint32_t GetNextExp(entt::entity e) {CheckLive(e);return static_cast<uint32_t>(g_registry.get<ecs::Experience>(e).next);}
LPDESC GetDesc(entt::entity) { return nullptr; }
int32_t GetMapIndex(entt::entity) {Unexpected();}
uint8_t GetEmpire(entt::entity e) {CheckLive(e);return g_registry.get<TargetData>(e).empire;}
void DestroyCharacter(entt::entity) {Unexpected();}
}
CLIENT_DESC* db_clientdesc=nullptr;
uint8_t g_bAuthServer=0;
std::string g_stQuestDir;
std::set<std::string> g_setQuestObjectDir;
void DESC::Packet(const void*,int) {Unexpected();}
void CLIENT_DESC::Packet(const void*,int) {Unexpected();}
void CLIENT_DESC::DBPacket(uint8_t,uint32_t,const void*,uint32_t) {Unexpected();}
void CLIENT_DESC::DBPacketHeader(uint8_t,uint32_t,uint32_t) {Unexpected();}
const DESC_MANAGER::DESC_SET& DESC_MANAGER::GetClientSet() {Unexpected();}
void CHARACTER_MANAGER::DestroyCharacter(entt::entity) {Unexpected();}
entt::entity CHARACTER_MANAGER::SpawnMobEntity(uint32_t,int32_t,int32_t,int32_t,int32_t,bool,int,bool) {Unexpected();}
bool CHARACTER_MANAGER::GetCharactersByRaceNum(uint32_t,std::vector<entt::entity>&) {Unexpected();}
namespace quest {
void NPC::Set(uint32_t,const std::string&) {Unexpected();}
bool CQuestManager::InitializeLua() {Unexpected();}
QuestState CQuestManager::OpenState(const std::string&,int) const {Unexpected();}
void CQuestManager::CloseState(QuestState&) const {Unexpected();}
bool CQuestManager::RunState(QuestState&) {Unexpected();}
void CQuestManager::GotoEndState(QuestState&) {Unexpected();}
void CQuestManager::AddLuaFunctionTable(const char*,luaL_reg*,bool) const {Unexpected();}
bool NPC::OnServerTimer(PC&) {Unexpected();}
bool NPC::OnDie(PC&) {Unexpected();}
bool NPC::OnQuestDamage(PC&) {Unexpected();}
bool NPC::OnTimer(PC&) {Unexpected();}
bool NPC::OnLevelUp(PC&) {Unexpected();}
bool NPC::OnLogout(PC&) {Unexpected();}
bool NPC::OnTakeItem(PC&) {Unexpected();}
bool NPC::OnUnmount(PC&) {Unexpected();}
bool NPC::OnLogin(PC&,const char*) {Unexpected();}
bool NPC::OnButton(PC&,uint32_t) {Unexpected();}
bool NPC::OnInfo(PC&,uint32_t) {Unexpected();}
bool NPC::OnItemInformer(PC&,uint32_t) {Unexpected();}
bool NPC::OnUseItem(PC&,bool) {Unexpected();}
bool NPC::OnSIGUse(PC&,bool) {Unexpected();}
bool NPC::OnEnterState(PC&,uint32_t,int) {Unexpected();}
bool NPC::OnLeaveState(PC&,uint32_t,int) {Unexpected();}
bool NPC::OnLetter(PC&,uint32_t,int) {Unexpected();}
}
bool ItemSystem::IsValidItem(entt::entity e) { return g_registry.valid(e) && !g_registry.all_of<ecs::CharacterType>(e); }
uint32_t ItemSystem::GetItemVnum(entt::entity) {Unexpected();}
uint32_t ItemSystem::GetItemOriginalVnum(entt::entity) {Unexpected();}
void ecs::ChatSystem::Send(entt::entity e,uint8_t,const char*,...) {CheckLive(e);if(onPublish){auto fn=onPublish;fn();}}
void ecs::SessionSystem::Save(entt::entity e) {CheckLive(e);++saves;}
void CombatSystem::Dead(entt::entity,entt::entity,bool) {Unexpected();}
float CombatSystem::GetAttackMultiplier(entt::entity) {Unexpected();}
float CombatSystem::GetDamageMultiplier(entt::entity) {Unexpected();}
void CombatSystem::SetAttackMultiplier(entt::entity,float) {Unexpected();}
void CombatSystem::SetDamageMultiplier(entt::entity,float) {Unexpected();}
entt::entity ecs::SocialSystem::GetPartyLeader(entt::entity) {Unexpected();}
CGuild* ecs::SocialSystem::GetGuild(entt::entity) {Unexpected();}
int32_t ecs::PointSystem::GetMaxHP(entt::entity e) {CheckLive(e);return g_registry.get<TargetData>(e).maxHP;}
bool CShopManager::StartShopping(entt::entity,entt::entity,int) {Unexpected();}
bool map_allow_find(int32_t) {Unexpected();}
bool SECTREE_MANAGER::GetMapBasePositionByMapIndex(int32_t,PIXEL_POSITION&) {Unexpected();}
bool DropEvent_CharStone_SetValue(const std::string&,int) {Unexpected();}
bool DropEvent_RefineBox_SetValue(const std::string&,int) {Unexpected();}
void ContinueOnFatalError() {Unexpected();}

int passes_per_sec=25;
bool exchanging=false;
bool AffectSystem::IsImmune(entt::entity e,uint32_t) {CheckLive(e);return g_registry.get<TargetData>(e).immune;}
bool AffectSystem::IsAffectFlag(entt::entity e,uint32_t flag) {CheckLive(e);return g_registry.get<TargetData>(e).affects.contains(flag);}
bool ecs::SocialSystem::HasExchange(entt::entity e) {CheckLive(e);return exchanging;}
CShop* ecs::SocialSystem::GetShop(entt::entity) {Unexpected();}
CShop* ecs::SocialSystem::GetMyShop(entt::entity e) {CheckLive(e);return nullptr;}
entt::entity ecs::SocialSystem::GetShopOwner(entt::entity) {Unexpected();}
void ecs::SocialSystem::SetShop(entt::entity,CShop*) {Unexpected();}
void ecs::SocialSystem::SetShopOwner(entt::entity,entt::entity) {Unexpected();}
int64_t ecs::PlayerRuntime::GetHP(entt::entity e) {CheckLive(e);return g_registry.get<TargetData>(e).hp;}
int32_t ecs::PlayerRuntime::GetX(entt::entity e) {CheckLive(e);return g_registry.get<TargetData>(e).x;}
int32_t ecs::PlayerRuntime::GetY(entt::entity e) {CheckLive(e);return g_registry.get<TargetData>(e).y;}
LPSECTREE ecs::PlayerRuntime::GetSectree(entt::entity e) {CheckLive(e);return searchTree;}
bool ecs::PlayerRuntime::IsBuilding(entt::entity e) {CheckLive(e);return g_registry.get<ecs::CharacterType>(e).value==CHAR_TYPE_BUILDING;}
bool ecs::PlayerRuntime::IsMonster(entt::entity e) {CheckLive(e);return g_registry.all_of<ecs::TagMonster>(e);}
bool CombatSystem::IsDead(entt::entity e) {CheckLive(e);return g_registry.get<TargetData>(e).dead;}
bool CEntity::IsType(int) const {Unexpected();}
bool AttrTransfer_is_open(entt::entity) {Unexpected();}
bool ecs::SessionSystem::IsSafeboxOpen(entt::entity) {Unexpected();}
bool ecs::SessionSystem::IsCubeOpen(entt::entity) {Unexpected();}
void CShop::RemoveGuest(entt::entity) {Unexpected();}
bool SectreeMember(entt::entity e,const SECTREE*) {
    if(onMember){auto fn=onMember;fn(e);}
    const auto* data=g_registry.valid(e)?g_registry.try_get<TargetData>(e):nullptr;
    return data && data->member;
}
SECTREE::SECTREE():m_id{},m_iPCCount(0),isClone(false),m_pkAttribute(nullptr){}
SECTREE::~SECTREE()=default;
FCollectEntity SECTREE::SnapshotAround(int) const {
    FCollectEntity result;for(const auto e:searchCandidates)result.Add(e,this);return result;
}
entt::entity ecs::SpatialService::EntityFromLPENTITY(LPENTITY) {Unexpected();}
COrcsDungeon& COrcsDungeon::instance() {Unexpected();}
bool COrcsDungeon::OnClickNpc(entt::entity) {Unexpected();}
CTritonTempleDungeon& CTritonTempleDungeon::instance() {Unexpected();}
bool CTritonTempleDungeon::OnClickNpc(entt::entity) {Unexpected();}
CValentineDungeon& CValentineDungeon::instance() {Unexpected();}
bool CValentineDungeon::OnClickNpc(entt::entity) {Unexpected();}
CRuneDungeon& CRuneDungeon::instance() {Unexpected();}
bool CRuneDungeon::OnClickNpc(entt::entity) {Unexpected();}
CPyramidDungeonRazor93& CPyramidDungeonRazor93::instance() {Unexpected();}
bool CPyramidDungeonRazor93::OnClickNpc(entt::entity) {Unexpected();}
CNightmareDungeonRazor93& CNightmareDungeonRazor93::instance() {Unexpected();}
bool CNightmareDungeonRazor93::OnClickNpc(entt::entity) {Unexpected();}
CHalloween2022Dungeon& CHalloween2022Dungeon::instance() {Unexpected();}
bool CHalloween2022Dungeon::OnClickNpc(entt::entity,entt::entity) {Unexpected();}
CVikingDungeon& CVikingDungeon::instance() {Unexpected();}
bool CVikingDungeon::OnClickNpc(entt::entity,entt::entity) {Unexpected();}
CEasterDungeon& CEasterDungeon::instance() {Unexpected();}
bool CEasterDungeon::OnClickNpc(entt::entity) {Unexpected();}

namespace {
int triggerCalls=0;
int CountTrigger(entt::entity npc,entt::entity causer) {
    CheckLive(npc);CheckLive(causer);++triggerCalls;return 1;
}
entt::entity Target(uint8_t type,int32_t x,int32_t y=0) {
    auto e=Character(type==CHAR_TYPE_PC?1:0);
    g_registry.get<ecs::CharacterType>(e).value=type;
    if(type==CHAR_TYPE_MONSTER)g_registry.emplace<ecs::TagMonster>(e);
    g_registry.emplace<ecs::SpatialKindTag>(e,ecs::SpatialKind::Character);
    auto& data=g_registry.emplace<TargetData>(e);data.x=x;data.y=y;
    searchCandidates.push_back(e);return e;
}
void NativeVictimSearch() {
    Reset();SECTREE tree;searchTree=&tree;
    const auto self=Target(CHAR_TYPE_MONSTER,0);
    g_registry.emplace<ecs::AIFlags>(self,ecs::AIFlags{});
    auto& flags=g_registry.get<ecs::AIFlags>(self);flags.isAttackMob=true;
    Check(CombatSystem::FindVictim(self,500)==entt::null,"mob selected itself");
    const auto farther=Target(CHAR_TYPE_PC,100),nearer=Target(CHAR_TYPE_PC,50);
    Check(CombatSystem::FindVictim(self,500)==nearer,"native nearest target search failed");
    Check(CombatSystem::FindVictim(self,48)==nearer && CombatSystem::FindVictim(self,47)==entt::null,
        "DISTANCE_APPROX range threshold changed");
    Check(CombatSystem::FindVictim(self,-1)==entt::null,"negative radius accepted");
    auto& nearData=g_registry.get<TargetData>(nearer);
    for(const auto flag:{AFF_EUNHYUNG,AFF_INVISIBILITY,AFF_REVIVE_INVISIBLE}) {
        nearData.affects.insert(flag);
        Check(CombatSystem::FindVictim(self,500)==farther,"hidden target was selected");
        nearData.affects.clear();
    }
    nearData.dead=true;Check(CombatSystem::FindVictim(self,500)==farther,"dead target selected");nearData.dead=false;
    nearData.member=false;Check(CombatSystem::FindVictim(self,500)==farther,"departed snapshot member selected");nearData.member=true;
    nearData.affects.insert(AFF_TERROR);
    Check(CombatSystem::FindVictim(self,500)==farther,"terror level gate failed");
    nearData.level=2;Check(CombatSystem::FindVictim(self,500)==nearer,"higher-level terror target incorrectly skipped");
    nearData.level=1;nearData.immune=true;Check(CombatSystem::FindVictim(self,500)==nearer,"terror immunity ignored");
    nearData.affects.clear();nearData.immune=false;
    flags.isNoAttackShinsu=true;g_registry.get<TargetData>(farther).empire=2;
    Check(CombatSystem::FindVictim(self,500)==farther,"empire exclusion ignored");
    flags.isNoAttackShinsu=false;

    const auto monster=Target(CHAR_TYPE_MONSTER,10);
    Check(CombatSystem::FindVictim(self,500)==monster,"attack-mob flag did not allow another mob");
    flags.isAggressive=true;Check(CombatSystem::FindVictim(self,500)==nearer,"aggressive searcher attacked a mob");
    flags.isAggressive=false;flags.isAttackMob=false;
    Check(CombatSystem::FindVictim(self,500)==nearer,"passive searcher used candidate's attack-mob flag");
    const auto item=Target(CHAR_TYPE_PC,1);
    g_registry.get<ecs::SpatialKindTag>(item).kind=ecs::SpatialKind::Item;
    Check(CombatSystem::FindVictim(self,500)==nearer,"item spatial kind entered character target search");
    g_registry.remove<ecs::SpatialKindTag>(item);
    Check(CombatSystem::FindVictim(self,500)==nearer,"untyped entity entered character target search");

    const auto building=Target(CHAR_TYPE_BUILDING,300);
    g_registry.get<TargetData>(building).affects.insert(AFF_BUILDING_UPGRADE);
    Check(CombatSystem::FindVictim(self,500)==building,"healthy construction-site preference changed");
    g_registry.get<TargetData>(self).hp=50;
    Check(CombatSystem::FindVictim(self,500)==nearer,"half-health construction-site boundary changed");
    g_registry.get<TargetData>(self).hp=std::numeric_limits<int64_t>::max();
    Check(CombatSystem::FindVictim(self,500)==building,"construction preference overflowed HP multiplication");
    g_registry.destroy(building);g_registry.get<TargetData>(self).hp=100;
    g_registry.destroy(nearer);const auto replacement=Character(1);
    Check(CombatSystem::FindVictim(self,500)==farther,"stale snapshot selected a recycled target");
    onMember=[&](entt::entity e){if(e==monster)g_registry.destroy(self);};
    Check(CombatSystem::FindVictim(self,500)==entt::null,"retired searcher returned a target");

    Reset();searchTree=&tree;
    const auto edge=Target(CHAR_TYPE_MONSTER,std::numeric_limits<int32_t>::min());
    const auto distant=Target(CHAR_TYPE_PC,std::numeric_limits<int32_t>::max());
    Check(CombatSystem::FindVictim(edge,500)==entt::null,"coordinate arithmetic wrapped into a nearby target");
    searchTree=nullptr;Check(CombatSystem::FindVictim(edge,500)==entt::null,"missing sectree accepted");
    Check(CombatSystem::FindVictim(entt::null,500)==entt::null,"null searcher accepted");
}
void NativeClick() {
    Reset();quest::CQuestManager q;triggerCalls=0;
    const auto pc=Character(1),npc=Character();
    q.GetPC(pc)->SetLoaded();q.Kill(1,101);
    ecs::PlayerRuntime::AssignClickTrigger(npc,ON_CLICK_SHOP);
    Check(g_registry.get<ecs::ClickTrigger>(npc).callback!=nullptr,"shop trigger missing");
    g_registry.get<ecs::ClickTrigger>(npc).callback=&CountTrigger;
    const auto beforeLookup=pidLookups;
    ecs::PlayerRuntime::OnClick(npc,pc);
    Check(pidLookups==beforeLookup,"native click converted the causer through a PID lookup");
    Check(clicks==1 && triggerCalls==1 && ecs::PlayerRuntime::GetQuestNPC(pc)==npc,
        "native click did not route quest and trigger with entity context");
    exchanging=true;ecs::PlayerRuntime::OnClick(npc,pc);exchanging=false;
    Check(triggerCalls==1,"exchange gate was bypassed");
    ecs::PlayerRuntime::AssignClickTrigger(npc,ON_CLICK_MAX_NUM);
    Check(!g_registry.all_of<ecs::ClickTrigger>(npc),"invalid trigger type retained an old callback");
    ecs::PlayerRuntime::AssignClickTrigger(npc,ON_CLICK_NONE);
    Check(!g_registry.get<ecs::ClickTrigger>(npc).callback,"NONE retained a click callback");
    g_registry.get<ecs::ClickTrigger>(npc).callback=&CountTrigger;
    hasTarget=true;onTarget=[&]{g_registry.destroy(npc);};
    ecs::PlayerRuntime::OnClick(npc,pc);
    Check(triggerCalls==1,"trigger ran after quest target deletion");
    const auto replacement=Character();
    ecs::PlayerRuntime::AssignClickTrigger(replacement,ON_CLICK_NONE);
    Check(!g_registry.get<ecs::ClickTrigger>(replacement).callback,"new entity inherited old trigger");
    ecs::PlayerRuntime::OnClick(npc,pc);
    Check(triggerCalls==1,"stale click reached a recycled entity");
    hasTarget=false;onTarget={};
    g_registry.remove<ecs::QuestContext>(pc);
    onConstruct=[&](entt::entity current){g_registry.destroy(current);};
    ecs::PlayerRuntime::OnClick(replacement,pc);
    onConstruct={};
    Check(!g_registry.valid(pc) && triggerCalls==1,"click continued after context construction deleted causer");
}
void TriggerConstructed(entt::registry&,entt::entity e) {if(onConstruct){auto fn=onConstruct;fn(e);}}
void TriggerConstruction() {
    for(int mode=0;mode<3;++mode) {
        Reset();const auto npc=Character();
        onConstruct=[&](entt::entity e){
            if(mode==0)g_registry.destroy(e);
            else if(mode==1)g_registry.remove<ecs::ClickTrigger>(e);
            else throw std::runtime_error("fixture");
        };
        bool threw=false;
        try {ecs::PlayerRuntime::AssignClickTrigger(npc,ON_CLICK_SHOP);}
        catch(const std::runtime_error&){threw=true;}
        onConstruct={};
        Check(threw==(mode==2),"trigger construction exception was swallowed");
        if(mode<2)Check(!g_registry.valid(npc) || !g_registry.all_of<ecs::ClickTrigger>(npc),
            "trigger assignment recreated state after callback removal");
    }
}
}

int main() {
    CHARACTER_MANAGER characters;CTargetManager targets;
    g_registry.on_construct<ecs::QuestContext>().connect<&Constructed>();
    g_registry.on_construct<ecs::ClickTrigger>().connect<&TriggerConstructed>();
    auto* L=lua_open();
    try {
        References();OtherPCBlocks();LuaAndDispatch(L);RewardBatches();RewardCompletion();ConstructionCallbacks();
        NativeClick();TriggerConstruction();NativeVictimSearch();Reset();
        lua_close(L);std::cout<<"Quest runtime: "<<checks<<" checks passed\n";
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';Reset();lua_close(L);return 1;
    }
}
