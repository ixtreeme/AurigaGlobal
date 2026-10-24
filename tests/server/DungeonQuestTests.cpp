#include "../../SRC/Server/GameServer/core/lua_incl.h"
#include "../../SRC/Server/GameServer/quest/quest_source.h"
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>

namespace {
std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing dungeon fixture: " + path.string());
    return {std::istreambuf_iterator<char>(input), {}};
}
void Run(lua_State* L, const std::string& code) {
    if (lua_dobuffer(L, code.data(), code.size(), "@dungeon-regression") != LUA_OK)
        throw std::runtime_error(lua_tostring(L,-1));
}
void Load(lua_State* L, const std::filesystem::path& directory, const char* name) {
    auto q=quest::source::ParseLegacy(Read(directory/(std::string(name)+".lua")),name);
    quest::source::Validate(L,q);
    Run(L,q.stateCode);
    // Assemble matching handlers exactly as NPC::Set does (conditional return
    // included). This reproduces duplicate prototype/event shadowing.
    Run(L,"handlers={}");
    std::map<std::string,std::string> bodies;
    for (const auto& h:q.handlers) bodies[h.event+":"+h.target]+=quest::source::HandlerCode(h);
    for (const auto& [key,body]:bodies)
        Run(L,"handlers['"+key+"']=function()\n"+body+"\nend");
}
}
int CheckDungeonQuests(lua_State* L) {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    auto directory=root/"SRC/Server/Resources/quest/Quest/dungeon";
    // Optional baseline directory makes it possible to demonstrate a red test
    // against the untouched serving files without modifying either copy.
    if (const auto* overridePath=std::getenv("AURIGA_DUNGEON_TEST_SOURCES")) directory=overridePath;
    Run(L,Read(root/"tests/server/fixtures/dungeon_world.lua"));
    int checks=0;
    for (const auto& [name,base]:std::map<std::string,int>{
        {"catacombe",216},{"christmas_zone1",26},{"christmas_zone2",25},
        {"deviltower_zone",66},{"dragonlair_zone",208},{"dungeon_spider",217},
        {"enchanted_forest",210},{"meleylair_zone",212},{"monkey_tower_razor93",367},
        {"nemere_zone",352},{"owl_zone_razor93",18},{"razador_zone",351},
        {"slime_dungeon",27},{"temple_ochao",209},{"underwater_zone",67}}) {
        Load(L,directory,name.c_str());
        Run(L,"reset("+std::to_string(base)+"); fire('login'); assert((flags.floor or 0)>0,'party member cannot start "+name+"'); local n=0; for _ in pairs(timers) do n=n+1 end; assert(n==1); fire('login'); local n2=0;for _ in pairs(timers) do n2=n2+1 end;assert(n2==n);local timer=next(timers);fire('server_timer',timer);assert(exits==0,'prepare failed: "+name+"')");
        ++checks;
    }
    Load(L,directory,"monkey_tower_razor93");
    Run(L,R"lua(
        reset(367);fire('login'); fire('server_timer','monkey_tower_razor93_prepare')
        local gate=flags.floor1_vid1
        assert(monsters[gate].invincible)
        fire('server_timer','monkey_tower_razor93_checker');assert(monsters[gate].invincible)
        for i=1,48 do kill(5121) end
        assert(monsters[gate].invincible,'gate opened while stone monkeys remained')
        for i=1,25 do kill(5122) end
        assert(not monsters[gate].invincible,'last monkey did not unlock gate')
        assert(notices[2156]==1)
        fire('server_timer','monkey_tower_razor93_checker');assert(notices[2156]==1)
        kill(30111); assert(flags.floor==2);kill(8059)
        local correct=flags.unique_vid; selected=correct;monsters[correct]=nil;fire('kill','8061')
        assert(flags.floor==5);fire('server_timer','monkey_tower_razor93_warp')
        assert(warps[#warps].x==653900 and warps[#warps].y==43700)
        selected=flags.boss_vid;item_vnum=50090
        for i=1,5 do current_pid=1+i%2;fire('take','20460') end
        assert(flags.statue_hits==5 and removed==5 and hp_changes==4,'split-party spears did not advance')
        assert(flags.floor==13 and flags.boss_vid_monkey>0)
        local boss=flags.boss_vid_monkey; assert(monsters[boss].invincible)
        for i=1,16 do kill(5126) end
        assert(monsters[boss].invincible)
        -- 5125 is deliberately last: the original duplicate handler hid this.
        for i=1,31 do kill(5125) end
        assert(not monsters[boss].invincible,'last golden monkey did not unlock boss')
        fire('server_timer','monkey_tower_razor93_warp')
        assert(warps[#warps].x==649100 and warps[#warps].y==66700)
        kill(2832);assert(#completions==1 and completions[1].base==367 and completions[1].vnum==2832)
        fire('kill','2832');assert(#completions==1,'completion repeated')
    )lua");checks+=14;
    Run(L,R"lua(
        reset(367);fire('login');spawn_failure=30111;fire('server_timer','monkey_tower_razor93_prepare')
        assert(exits==1 and flags.was_completed==1,'missing gate stranded party')
        reset(367);fire('login');fire('server_timer','monkey_tower_razor93_prepare')
        for vid,mob in pairs(monsters) do if mob.vnum~=30111 then monsters[vid]=nil end end
        fire('server_timer','monkey_tower_razor93_checker')
        assert(not monsters[flags.floor1_vid1].invincible,'checker cannot recover a missed kill callback')
    )lua");checks+=2;
    Run(L,R"lua(
        for floor,pos in pairs({[1]={611800,44700},[2]={629100,43900},[5]={653900,43700},[13]={649100,66700}}) do
            reset(367);current_map=219;flags.floor=floor
            pc.setf('monkey_tower_razor93','idx',map);pc.setf('monkey_tower_razor93','ch',1)
            pc.setf('monkey_tower_razor93','disconnect',1300)
            fire('chat','20009');assert(#warps==1 and joins==0)
            assert(warps[1].x==pos[1] and warps[1].y==pos[2],'rejoin used another map/room')
        end
        reset(367);flags.floor=2;selected=100;flags.unique_vid=100;spawn_failure=20460
        fire('kill','8061');assert(exits==1,'missing statue stranded party')
    )lua");checks+=5;
    Load(L,directory,"catacombe");
    Run(L,"reset(216);flags.floor=4;fire('kill','2598');assert(#completions==1);fire('kill','2598');assert(#completions==1,'bonus boss repeated completion')");++checks;
    Load(L,directory,"razador_zone");
    Run(L,"reset(351);flags.floor=8;fire('server_timer','razador_zone_prepare');assert(notices[1060]==1)");++checks;
    Load(L,directory,"meleylair_zone");
    Run(L,R"lua(
        reset(212);flags.floor=1;fire('chat','20420');assert(#completions==0 and not rewards[50270])
        flags.floor=20;flags.was_completed=1;answers={2};fire('chat','20420')
        assert(not rewards[50270],'close button granted a reward')
        answers={1,1};fire('chat','20420');assert(rewards[50270]==1 and #completions==1)
        fire('chat','20420');assert(rewards[50270]==1,'Meley reward repeated')
    )lua");checks+=4;
    for (const auto& [name,base,target]:{std::tuple{"dragonlair_zone",208,"8031"},std::tuple{"dungeon_spider",217,"2095"}}) {
        Load(L,directory,name);
        Run(L,"reset("+std::to_string(base)+");flags.floor="+std::to_string(base==208?1:3)+";flags.count=1;flags.boss=999;fire('kill','"+target+"');assert(exits==1,'missing boss failed to clean dungeon')");++checks;
    }
    return checks;
}
