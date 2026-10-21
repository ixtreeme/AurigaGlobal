#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/quest/questmanager.h"
#include "../../SRC/Server/GameServer/quest/quest_runtime.h"
#include "../../SRC/Server/GameServer/quest/questevent.h"
#include "../../SRC/Server/GameServer/entity/char.h"
#include "../../SRC/Server/GameServer/entity/char_manager.h"
#include "../../SRC/Server/GameServer/entity/target.h"
#include "../../SRC/Server/GameServer/social/party.h"
#include "../../SRC/Server/GameServer/network/desc.h"
#include "../../SRC/Server/GameServer/network/desc_client.h"
#include "../../SRC/Server/GameServer/network/desc_manager.h"
#include "../../SRC/Server/GameServer/world/sectree_manager.h"
#include "../../SRC/Server/GameServer/social/shop_manager.h"
#include "../../SRC/Server/GameServer/social/shop.h"
#include "../../SRC/Server/GameServer/item/attr_transfer.h"
#include "../../SRC/Server/GameServer/dungeon/OrcsDungeon.h"
#include "../../SRC/Server/GameServer/dungeon/TritonTempleDungeon.h"
#include "../../SRC/Server/GameServer/dungeon/ValentineDungeon.h"
#include "../../SRC/Server/GameServer/dungeon/RuneDungeon.h"
#include "../../SRC/Server/GameServer/dungeon/PyramidDungeonRazor93.h"
#include "../../SRC/Server/GameServer/dungeon/NightmareDungeonRazor93.h"
#include "../../SRC/Server/GameServer/dungeon/Halloween2022Dungeon.h"
#include "../../SRC/Server/GameServer/dungeon/VikingDungeon.h"
#include "../../SRC/Server/GameServer/dungeon/EasterDungeon.h"
#include "../../SRC/Server/GameServer/ecs/services/SpatialService.hpp"
#include "../../SRC/Server/GameServer/core/constants.h"
#include "../../SRC/Server/GameServer/core/config.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/quest_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/vital_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/ai_components.hpp"
#include "../../SRC/Server/GameServer/combat/affect.h"
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
#include <limits>
#include <common/event_flag.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <source_location>

entt::registry g_registry;
entt::dispatcher g_dispatcher;
int test_server=0;
namespace quest {
int npc_lock(lua_State*);
int npc_unlock(lua_State*);
int game_set_event_flag(lua_State*);
int game_get_event_flag(lua_State*);
int game_get_event_flag_string(lua_State*);
}
namespace {
int checks=0, clicks=0, chats=0, attrCalls=0, affects=0, saves=0, pidLookups=0;
std::vector<TPacketSetEventFlag> eventRequests;
bool hasTarget=false, hasChat=false;
TargetInfo targetInfo;
std::map<uint32_t,entt::entity> players;
std::vector<uint32_t> gifts;
std::vector<int64_t> experience;
std::function<void()> onGive, onTarget, onChat, onAttr, onPublish;
std::function<void(entt::entity)> onConstruct;
std::function<void(quest::CQuestManager&)> onInitializeLua;
bool initializeLuaResult = true;
bool realLuaFixture = false;
int nativeEffects = 0;
std::function<void()> onTimer;
int TestMutation(lua_State*) { ++nativeEffects; return 0; }
std::function<void(uint32_t, const std::string&)> onNpcSet;
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
void WideEventFlags(lua_State* L) {
    Reset(); quest::CQuestManager q;
    int dbToken = 0;
    db_clientdesc = reinterpret_cast<CLIENT_DESC*>(&dbToken);
    eventRequests.clear();
    for (const int64_t value : {int64_t{2147483648}, int64_t{4294967296}, int64_t{1} << 40, INT64_MIN, INT64_MAX}) {
        q.SetEventFlag("time64", value);
        Check(q.GetEventFlag("time64") == value && q.GetEventFlag("unknown") == 0, "event flag storage truncated value");
        lua_settop(L, 0); lua_pushstring(L, "time64");
        const auto text = std::to_string(value); lua_pushlstring(L, text.data(), text.size());
        const auto count = eventRequests.size();
        Check(quest::game_set_event_flag(L) == 0 && eventRequests.size() == count + 1 &&
            eventRequests.back().lValue == value && std::string(eventRequests.back().szFlagName) == "time64",
            "exact Lua event flag did not reach DB request intact");
        lua_settop(L, 0); lua_pushstring(L, "time64");
        Check(quest::game_get_event_flag_string(L) == 1 && std::string(lua_tostring(L, -1)) == text,
            "exact Lua event flag read lost precision");
    }
    q.SetEventFlag("time64", 4294967296LL);
    lua_settop(L, 0); lua_pushstring(L, "time64");
    Check(quest::game_get_event_flag(L) == 1 && lua_tonumber(L, -1) == 4294967296.0, "numeric Lua timestamp read truncated");
    const auto count = eventRequests.size();
    for (const double invalid : {std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(), 1.5, 9223372036854775808.0, 9007199254740992.0}) {
        lua_settop(L, 0); lua_pushstring(L, "time64"); lua_pushnumber(L, invalid);
        quest::game_set_event_flag(L);
    }
    lua_settop(L, 0); lua_pushstring(L, "time64"); lua_pushstring(L, "9223372036854775808");
    quest::game_set_event_flag(L);
    q.RequestSetEventFlag(std::string(EVENT_FLAG_NAME_MAX_LEN + 1, 'x'), 1);
    Check(eventRequests.size() == count, "invalid/unrepresentable event request reached DB");
    lua_settop(L, 0); lua_pushstring(L, "time64"); lua_pushnumber(L, 4294967297.0);
    quest::game_set_event_flag(L);
    Check(eventRequests.size() == count + 1 && eventRequests.back().lValue == 4294967297LL,
        "numeric Lua event timestamp narrowed");
    db_clientdesc = nullptr; lua_settop(L, 0);
}

void GenerationHotreload() {
    Reset();
    static HEART heart {}; thecore_heart=&heart;
    const auto root = std::filesystem::temp_directory_path() / ("auriga-hotreload-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "quest");
    struct Cleanup {
        std::filesystem::path root; std::string old = g_stQuestDir;
        ~Cleanup() { realLuaFixture=false; onInitializeLua={};onNpcSet={};onTimer={};g_stQuestDir=old;
            std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    } cleanup {root};
    auto write = [&](const char* name, const char* value) { std::ofstream(root/name, std::ios::binary) << value; };
    write("settings.lua", "-- immutable setup"); write("restart_city_pos.lua", "-- immutable setup");
    write("quest/questnpc.json", R"({"npcs":[]})"); write("quest/questcategory.json", R"({"categories":[]})");
    write("quest/quest_list", "main.quest\n");
    write("quest/main.quest", "quest main begin state start begin when login begin end when pulse.server_timer begin end end end");
    write("quest/questlib.lua", "helper='old'"); write("quest/extra.lua", "return 'old-file'");
    g_stQuestDir = (root/"quest").string(); realLuaFixture=true;
    onInitializeLua=[](quest::CQuestManager&) {}; onNpcSet=[](uint32_t,const std::string&) {};
    quest::CQuestManager q;
    Check(q.Initialize(), "initial real Lua fixture failed");
    auto original=q.ActiveRuntime(); const auto firstId=q.GetQuestIndexByName("main");
    const auto player=Character(501);q.GetPC(player)->SetLoaded();q.GetPC(player)->SetFlag("main.saved",42);
    auto dialog=q.OpenState("main",0);
    const std::string script="local n=coroutine.yield('select', {'a','b'}); answer=helper..':'..dofile('quest/extra.lua')..':'..n";
    Check(luaL_loadbufferx(dialog.co,script.data(),script.size(),"@suspended-dialog","t")==LUA_OK && q.RunState(dialog),"dialog failed to suspend");
    auto single=quest::quest_create_server_timer_event("pulse",2,q.LoadTimerScript("pulse"),false,1);
    auto loop=quest::quest_create_server_timer_event("pulse",2,q.LoadTimerScript("pulse"),true,2);
    auto late=quest::quest_create_server_timer_event("pulse",3600,q.LoadTimerScript("pulse"),false,3);
    q.AddServerTimer("pulse",1,single);q.AddServerTimer("pulse",2,loop);
    q.AddServerTimer("pulse",3,late);
    write("quest/questlib.lua", "helper='new'");write("quest/extra.lua", "return 'new-file'");
    write("quest/quest_list", "extra.quest\nmain.quest\n");
    write("quest/extra.quest", "quest extra begin state start begin when login begin end end end");
    std::string error;
    Check(q.PrepareReload(root.generic_string(),"",error), "valid generation was rejected");
    Check(q.ActiveRuntime()==original && q.GetQuestIndexByName("extra")==0,"prepare activated early");
    // The explicit prepare is already staged. Commit the captured fingerprint.
    quest::RuntimeSnapshot next;next.Capture(root.generic_string(),(root/"quest").generic_string());
    Check(q.CommitReload(next.revision,error), "prepared generation did not commit");
    Check(q.ActiveRuntime()!=original && q.GetQuestIndexByName("main")==firstId && q.GetQuestIndexByName("extra")>firstId,"quest IDs changed after insertion/reorder");
    Check(q.GetPC(player)->GetFlag("main.saved")==42,"reload changed player flags");
    dialog.args=1;lua_pushinteger(dialog.co,2);
    Check(!q.RunState(dialog),"old dialog failed to finish");
    {quest::RuntimeScope scope(q,original);lua_getglobal(q.GetLuaState(),"answer");
        Check(std::string(lua_tostring(q.GetLuaState(),-1))=="old:old-file:2","suspended dialog read new-generation globals/files");lua_pop(q.GetLuaState(),1);}
    q.CloseState(dialog);
    auto fresh=q.OpenState("main",0);
    const std::string freshScript="answer=helper..':'..dofile('quest/extra.lua')";
    Check(luaL_loadbufferx(fresh.co,freshScript.data(),freshScript.size(),"@fresh-event","t")==LUA_OK && !q.RunState(fresh),"new event failed");
    lua_getglobal(q.GetLuaState(),"answer");Check(std::string(lua_tostring(q.GetLuaState(),-1))=="new:new-file","new event used stale library");lua_pop(q.GetLuaState(),1);q.CloseState(fresh);
    std::vector<std::string> ticks;
    onTimer=[&] {lua_getglobal(q.GetLuaState(),"helper");ticks.emplace_back(lua_tostring(q.GetLuaState(),-1));lua_pop(q.GetLuaState(),1);};
    Check(single->func(single,0)==0,"one-shot timer repeated");
    Check(loop->func(loop,0)==50,"loop timer interval changed");
    Check(ticks==std::vector<std::string>({"old","new"}),"timer generation migration policy failed");
    Check(static_cast<quest::quest_server_event_info*>(loop->info)->runtime==q.ActiveRuntime(),"loop timer retained old generation");
    q.ClearServerTimer("pulse",2);q.ClearServerTimer("pulse",3);
    Check(!static_cast<quest::quest_server_event_info*>(late->info)->runtime,"canceled distant timer retained old VM until its deadline");
    quest::CancelTimerEvent(&single);quest::CancelTimerEvent(&loop);
    std::weak_ptr<quest::RuntimeGeneration> retired=original;
    original.reset();q.CollectRuntimes();
    Check(retired.expired(),"finished dialog/timers retained retired Lua VM");
    event_destroy();
    const auto active=q.ActiveRuntime();
    write("quest/questlib.lua","test_mutation(); helper='unsafe'");nativeEffects=0;
    Check(!q.PrepareReload(root.generic_string(),"",error) && nativeEffects==0 && q.ActiveRuntime()==active,"staging mutated live game state");
    write("quest/questlib.lua","helper='third'");
    write("quest/main.quest","quest main begin state start begin when login begin if then end end end end");
    Check(!q.PrepareReload(root.generic_string(),"",error) && q.ActiveRuntime()==active,"bad syntax replaced active generation");
    write("quest/main.quest","quest main begin state start begin when login begin end end end");
    write("quest/quest_list","extra.quest\n");
    Check(!q.PrepareReload(root.generic_string(),"",error),"removed persisted quest was accepted");
    write("quest/quest_list","main.quest\nextra.quest\n");write("settings.lua","changed=true");
    Check(!q.PrepareReload(root.generic_string(),"",error),"shared setup changed during hotreload");
    q.DisconnectPC(player);
    event_destroy();Reset();
}
void QuestDataInitialization() {
    Reset(); quest::CQuestManager q;
    const auto path = std::filesystem::temp_directory_path() /
        ("auriga-quest-runtime-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(std::filesystem::create_directory(path), "quest fixture directory already exists");
    struct Cleanup {
        std::filesystem::path path;
        std::string oldDirectory = g_stQuestDir;
        ~Cleanup() {
            onInitializeLua = {}; onNpcSet = {}; initializeLuaResult = true; g_stQuestDir = oldDirectory;
            std::error_code ignored;
            std::filesystem::remove(path / "questnpc.json", ignored);
            std::filesystem::remove(path / "questcategory.json", ignored);
            std::filesystem::remove(path / "quest_list", ignored);
            std::filesystem::remove(path, ignored);
        }
    } cleanup {path};
    g_stQuestDir = path.string();
    int initializations = 0;
    std::vector<std::pair<uint32_t, std::string>> scripts;
    onInitializeLua = [&](quest::CQuestManager& manager) {
        ++initializations;
        manager.RegisterQuest("guild_war_join", 7);
        manager.RegisterQuest("uncategorized", 8);
    };
    onNpcSet = [&](uint32_t vnum, const std::string& name) { scripts.emplace_back(vnum, name); };
    auto write = [&](const char* file, const char* text) { std::ofstream(path / file, std::ios::binary) << text; };
    Check(!q.Initialize() && initializations == 0 && scripts.empty() && !q.GetLuaState(),
        "invalid quest data reached Lua/script loading");
    write("questnpc.json", R"({"npcs":[{"vnum":20016,"name":"blacksmith"}]})");
    write("questcategory.json", R"({"categories":[{"quest":"guild_war_join","category":3},{"quest":"not_compiled","category":4}]})");
    write("quest_list", "");
    Check(q.Initialize() && initializations == 1, "valid quest JSON did not initialize");
    q.CurrentRuntime()->originalDataRoot = path.generic_string();
    q.CurrentRuntime()->originalQuestRoot = path.generic_string();
    Check(scripts.size() == 2 && scripts[0] == std::make_pair(20016u, std::string("blacksmith")) &&
        scripts[1] == std::make_pair(0u, std::string("notarget")), "NPC aliases or reserved scripts changed");
    Check(q.FindNPCIDByName("blacksmith") == 20016 && q.FindNPCIDByName("missing") == 0,
        "NPC JSON did not populate runtime name resolution");
    Check(q.GetQuestCategoryByQuestIndex(7) == 3 && q.GetQuestCategoryByQuestIndex(8) == 0 &&
        q.ReadQuestCategoryFile(7) == 3 && q.ReadQuestCategoryFile(8) == 99 && q.ReadQuestCategoryFile(65535) == 99,
        "category lookup or historical defaults changed");
    Check(q.QuestCategoryIndexMap.size() == 1, "uncompiled quest became a categorized quest");
    auto* oldLua = q.GetLuaState();
    write("questcategory.json", R"({"categories":[{"quest":"guild_war_join","category":"bad"}]})");
    q.Reload();
    Check(q.GetLuaState() == oldLua && initializations == 1 && scripts.size() == 2 &&
        q.GetQuestCategoryByQuestIndex(7) == 3 && q.FindNPCIDByName("blacksmith") == 20016,
        "invalid reload destroyed live quest state");
    onInitializeLua = [&](quest::CQuestManager& manager) { ++initializations; manager.RegisterQuest("main", 9); };
    write("questnpc.json", R"({"npcs":[]})");
    write("questcategory.json", R"({"categories":[{"quest":"main","category":65535}]})");
    q.Reload();
    Check(initializations == 2 && scripts.size() == 3 && scripts.back() == std::make_pair(0u, std::string("notarget")),
        "valid reload did not retain the reserved script entry");
    Check(q.FindNPCIDByName("blacksmith") == 0 && q.GetQuestIndexByName("guild_war_join") == 0 &&
        q.GetQuestCategoryByQuestIndex(7) == 0 && q.ReadQuestCategoryFile(7) == 99 &&
        q.GetQuestCategoryByQuestIndex(9) == 65535 && q.ReadQuestCategoryFile(9) == 65535,
        "reload retained stale name/index/category associations");
    oldLua = q.GetLuaState();
    onInitializeLua = [&](quest::CQuestManager& manager) { ++initializations; manager.RegisterQuest("failed_candidate", 10); };
    initializeLuaResult = false;
    q.Reload();
    Check(q.GetLuaState() == oldLua && q.GetQuestIndexByName("main") == 9 &&
        q.GetQuestIndexByName("failed_candidate") == 0 && q.GetQuestCategoryByQuestIndex(9) == 65535,
        "failed Lua candidate replaced the previous runtime");
    initializeLuaResult = true;
    onInitializeLua = [&](quest::CQuestManager& manager) { ++initializations; manager.RegisterQuest("main", 9); };
    const auto connected = Character(1234);
    Check(q.GetPC(1234) != nullptr, "reload fixture PC was not created");
    q.Reload();
    Check(initializations == 4 && q.GetLuaState() == oldLua && q.GetQuestIndexByName("main") == 9,
        "hotreload failed with a connected quest PC");
    g_registry.destroy(connected);
    std::filesystem::remove(path / "questcategory.json");
    Check(q.GetQuestCategoryByQuestIndex(9) == 65535 && q.ReadQuestCategoryFile(9) == 65535,
        "category lookup still reads a runtime text/file path");
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
}
entt::entity ecs::SocialSystem::GetParty(entt::entity) {return entt::null;}
uint32_t PartySystem::GetLeaderPID(entt::entity) {throw std::runtime_error("unexpected party");}
entt::entity PartySystem::GetLeader(entt::entity) {throw std::runtime_error("unexpected party");}
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
[[noreturn]] void Unexpected(std::source_location site = std::source_location::current()) {
    throw std::runtime_error(std::string("unexpected external service: ") + site.function_name() + ':' + std::to_string(site.line()));
}
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
void CLIENT_DESC::DBPacket(uint8_t header,uint32_t,const void* data,uint32_t size) {
    Check(this == db_clientdesc && header == HEADER_GD_SET_EVENT_FLAG && size == sizeof(TPacketSetEventFlag),
        "unexpected event flag DB frame");
    TPacketSetEventFlag packet {}; std::memcpy(&packet, data, sizeof(packet)); eventRequests.push_back(packet);
}
void CLIENT_DESC::DBPacketHeader(uint8_t,uint32_t,uint32_t) {Unexpected();}
const DESC_MANAGER::DESC_SET& DESC_MANAGER::GetClientSet() {Unexpected();}
void CHARACTER_MANAGER::DestroyCharacter(entt::entity) {Unexpected();}
entt::entity CHARACTER_MANAGER::SpawnMobEntity(uint32_t,int32_t,int32_t,int32_t,int32_t,bool,int,bool) {Unexpected();}
bool CHARACTER_MANAGER::GetCharactersByRaceNum(uint32_t,std::vector<entt::entity>&) {Unexpected();}
namespace quest {
void NPC::Set(uint32_t vnum,const std::string& name) {if(onNpcSet) onNpcSet(vnum,name);}
bool CQuestManager::InitializeLua() {
    if(!onInitializeLua)Unexpected();
    L=lua_open();
    if (realLuaFixture) {
        auto& runtime = *CurrentRuntime();
        if (runtime.files.empty()) runtime.Capture(runtime.originalDataRoot, runtime.originalQuestRoot);
        Check(LuaOpenQuestLibraries(L), "fixture libraries failed");
        RememberStandardAPIs(L, runtime);
        lua_pushcfunction(L, TestMutation); lua_setglobal(L, "test_mutation");
        lua_pushcfunction(L, TestMutation); lua_setglobal(L, "add_goto_info");
        GuardRuntimeAPIs(L, runtime);
        if (lua_dofile(L, "quest/questlib.lua") != LUA_OK) return false;
        m_questSources = source::LoadFiles(L, runtime.files);
        unsigned id = 0;
        for (const auto& definition : m_questSources) {
            RegisterQuest(definition.name, ++id);
            if (lua_dobuffer(L, definition.stateCode.data(), definition.stateCode.size(), definition.file.c_str()) != LUA_OK) return false;
        }
    }
    if(realLuaFixture) FinishRuntimeBootstrap(L);
    onInitializeLua(*this); return initializeLuaResult;
}
void CQuestManager::GotoEndState(QuestState&) {}
void CQuestManager::GotoSelectState(QuestState& qs) { qs.suspend_state = SUSPEND_STATE_SELECT; }
void CQuestManager::GotoPauseState(QuestState& qs) { qs.suspend_state = SUSPEND_STATE_PAUSE; }
void CQuestManager::GotoInputState(QuestState& qs) { qs.suspend_state = SUSPEND_STATE_INPUT; }
void CQuestManager::GotoConfirmState(QuestState& qs) { qs.suspend_state = SUSPEND_STATE_CONFIRM; }
void CQuestManager::GotoSelectItemState(QuestState& qs) { qs.suspend_state = SUSPEND_STATE_SELECT_ITEM; }
void CQuestManager::AddLuaFunctionTable(const char*,luaL_reg*,bool) const {Unexpected();}
bool NPC::OnServerTimer(PC&) {if(onTimer) {onTimer(); return true;} Unexpected();}
bool NPC::OnDie(PC&) {Unexpected();}
bool NPC::OnQuestDamage(PC&) {Unexpected();}
bool NPC::OnTimer(PC&) {if(onTimer) {onTimer(); return true;} Unexpected();}
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
uint64_t ecs::PointSystem::GetMaxHP(entt::entity e) {CheckLive(e);return g_registry.get<TargetData>(e).maxHP;}
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
entt::entity ecs::SocialSystem::GetShop(entt::entity) {Unexpected();}
entt::entity ecs::SocialSystem::GetMyShop(entt::entity e) {CheckLive(e);return entt::null;}
entt::entity ecs::SocialSystem::GetShopOwner(entt::entity) {Unexpected();}
void ecs::SocialSystem::SetShop(entt::entity,entt::entity) {Unexpected();}
void ecs::SocialSystem::SetShopOwner(entt::entity,entt::entity) {Unexpected();}
uint64_t ecs::PlayerRuntime::GetHP(entt::entity e) {CheckLive(e);return g_registry.get<TargetData>(e).hp;}
int32_t ecs::PlayerRuntime::GetX(entt::entity e) {CheckLive(e);return g_registry.get<TargetData>(e).x;}
int32_t ecs::PlayerRuntime::GetY(entt::entity e) {CheckLive(e);return g_registry.get<TargetData>(e).y;}
LPSECTREE ecs::PlayerRuntime::GetSectree(entt::entity e) {CheckLive(e);return searchTree;}
bool ecs::PlayerRuntime::IsBuilding(entt::entity e) {CheckLive(e);return g_registry.get<ecs::CharacterType>(e).value==CHAR_TYPE_BUILDING;}
bool ecs::PlayerRuntime::IsMonster(entt::entity e) {CheckLive(e);return g_registry.all_of<ecs::TagMonster>(e);}
bool CombatSystem::IsDead(entt::entity e) {CheckLive(e);return g_registry.get<TargetData>(e).dead;}
bool AttrTransfer_is_open(entt::entity) {Unexpected();}
bool ecs::SessionSystem::IsSafeboxOpen(entt::entity) {Unexpected();}
bool ecs::SessionSystem::IsCubeOpen(entt::entity) {Unexpected();}
void ShopSystem::RemoveGuest(entt::entity, entt::entity) {Unexpected();}
bool ShopSystem::AddGuest(entt::entity, entt::entity, uint32_t, bool) {Unexpected();}
bool ShopSystem::IsValid(entt::entity) {Unexpected();}
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
void QuestPCOwnership() {
    Reset();quest::CQuestManager q;
    const auto pc=Character(1),other=Character(2);
    auto* first=q.GetPC(pc);
    const auto* state=g_registry.try_get<ecs::QuestPCState>(pc);
    Check(first && state && state->pc.get()==first,"quest PC was not attached to its character entity");
    Check(q.GetPC(pc)==first,"quest PC identity changed between reads");
    Check(q.GetPC(other) && q.GetPC(other)!=first,"two characters shared one quest PC");
    Check(!g_registry.try_get<ecs::QuestPCState>(other)->pc->IsRunning(),"fresh quest PC started a quest");
    quest::PC* server=q.GetPCForce(0);
    Check(server && server==q.GetPCForce(0),"server timer PC was not manager-owned");
    Check(!q.GetPCForce(999),"offline pid received a quest PC");
    q.GetPC(pc);
    q.DisconnectPC(pc);
    Check(!g_registry.all_of<ecs::QuestPCState>(pc) && !q.GetCurrentPC() && q.GetCurrentCharacter()==entt::null,
        "disconnect retained the quest PC component");
    q.GetPC(pc);
    g_registry.destroy(pc);
    Check(!q.GetCurrentPC() && q.GetCurrentCharacter()==entt::null,
        "entity destruction left the manager on a freed quest PC");
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

int main(int argc, char** argv) {
    CHARACTER_MANAGER characters;CTargetManager targets;
    if (argc == 4 && std::string(argv[1]) == "--reload-core") {
        try {
            const auto root=std::filesystem::canonical(argv[2]);const std::string name=argv[3];
            g_stQuestDir=(root/"data/quest").generic_string();realLuaFixture=true;
            onNpcSet=[](uint32_t,const std::string&) {};
            onInitializeLua=[&](quest::CQuestManager& manager) {
                if(manager.CurrentRuntime()->staging && std::filesystem::exists(root/"run"/("reject-"+name)))
                    throw std::runtime_error("injected core preparation failure");
            };
            quest::CQuestManager q;
            q.ConfigureReloadControl(root.generic_string(),name,(root/"data").generic_string(),g_stQuestDir);
            if(!q.Initialize()) throw std::runtime_error("test core initialization failed");
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
            while(!std::filesystem::exists(root/"run/test-stop") && std::chrono::steady_clock::now()<deadline) {
                if(!std::filesystem::exists(root/"run"/("stall-"+name))) q.PollReloadControl();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return 0;
        } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
    }
    g_registry.on_construct<ecs::QuestContext>().connect<&Constructed>();
    g_registry.on_construct<ecs::ClickTrigger>().connect<&TriggerConstructed>();
    auto* L=lua_open();
    try {
        GenerationHotreload();QuestDataInitialization();WideEventFlags(L);References();OtherPCBlocks();QuestPCOwnership();LuaAndDispatch(L);RewardBatches();RewardCompletion();ConstructionCallbacks();
        NativeClick();TriggerConstruction();NativeVictimSearch();Reset();
        lua_close(L);std::cout<<"Quest runtime: "<<checks<<" checks passed\n";
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';Reset();lua_close(L);return 1;
    }
}
