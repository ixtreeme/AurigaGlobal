#include "../../SRC/Server/GameServer/story/SpyQuestRules.hpp"
#include "../../SRC/Server/GameServer/story/SpyQuestSystem.hpp"
#include "../../SRC/Server/GameServer/story/SpyQuestNavigation.hpp"
#include <sstream>
#include <iostream>
#include <stdexcept>
entt::registry g_registry;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main(){try{
 using namespace SpyQuest;
 auto bridge=[](Cell c){return c.x>=0&&c.x<=20&&c.y>=0&&c.y<=20&&(c.x!=10||c.y==7);};
 RouteSearch search({1,1},{19,19},bridge);int ticks=0;while(search.Step(4)==RouteSearch::Result::Searching&&ticks++<10000){}
 CHECK(search.result==RouteSearch::Result::Found);bool crossed=false;
 for(auto c:search.route){CHECK(bridge(c));if(c.x==10){CHECK(c.y==7);crossed=true;}}CHECK(crossed);
 RouteSearch corner({0,0},{1,1},[](Cell c){return c==Cell{0,0}||c==Cell{1,1};});CHECK(corner.Step()==RouteSearch::Result::Failed);
 RouteSearch cliff({0,0},{2,0},[](Cell c){return c.x>=0&&c.x<=2&&c.y==0;},[](Cell a,Cell b){return a.x!=1&&b.x!=1;});CHECK(cliff.Step()==RouteSearch::Result::Failed);
 std::ostringstream bytes;bytes.write("SPYNAV2\0",8);int32_t dimension=2;bytes.write(reinterpret_cast<char*>(&dimension),4);bytes.write(reinterpret_cast<char*>(&dimension),4);
 for(int i=0;i<4;i++){char flag=i==2?0:1;int32_t height=i==1?600:0;bytes.write(&flag,1);bytes.write(reinterpret_cast<char*>(&height),4);}
 std::istringstream input(bytes.str());Navigation nav;CHECK(nav.Load(input));CHECK(nav.Walk({0,0}));CHECK(!nav.Walk({0,1}));CHECK(!nav.Walk({-1,0}));CHECK(!nav.Step({0,0},{1,0}));
 std::istringstream truncated(bytes.str().substr(0,17));CHECK(!nav.Load(truncated));CHECK(!nav.Ready());
 CHECK(Suspicion(0,4000,false,true)==18);CHECK(Suspicion(0,4000,true,true)==24);CHECK(Suspicion(0,2000,false,true)==35);
 CHECK(Suspicion(40,5500,true,true)<40);CHECK(Suspicion(99,100,true,true)==100);CHECK(Suspicion(0,6500,false,true)==0);CHECK(Suspicion(0,4000,true,false)==0);
 auto owner=g_registry.create(),other=g_registry.create(),actor=g_registry.create();
 g_registry.emplace<ecs::PlayerID>(owner,ecs::PlayerID{7});g_registry.emplace<ecs::TagPC>(owner);
 g_registry.emplace<ecs::PlayerID>(other,ecs::PlayerID{8});g_registry.emplace<ecs::TagPC>(other);
 g_registry.emplace<Actor>(actor,Actor{7,0,false,{}});CHECK(CanView(g_registry,actor,owner));CHECK(!CanView(g_registry,actor,other));
 CHECK(!AllowCombat(owner,actor));g_registry.get<Actor>(actor).fighting=true;CHECK(AllowCombat(owner,actor));CHECK(AllowCombat(actor,owner));CHECK(!AllowCombat(other,actor));
 g_registry.emplace<CinematicLock>(owner);CHECK(!AllowCombat(owner,actor));CHECK(!AllowCombat(actor,owner));g_registry.remove<CinematicLock>(owner);CHECK(AllowCombat(owner,actor));
 CHECK(AllowCombat(owner,other));g_registry.clear();
 std::cout<<"SPY_QUEST_RULES=PASS bridge=ok corners=ok suspicion=ok owner_visibility=ok owner_combat=ok cinematic_lock=ok\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
