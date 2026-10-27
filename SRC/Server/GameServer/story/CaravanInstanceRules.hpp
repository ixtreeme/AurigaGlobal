#pragma once
#include <cstdint>
namespace CaravanQuest {
constexpr int ForestTemplate=420;
constexpr int TemplateOf(int map){return map>=10000?map/10000:map;}
constexpr bool ForestMap(int map){return TemplateOf(map)==ForestTemplate;}
// A reservation on another map (including another party's forest instance) is
// never permission to remain in this instance. The public template is not playable.
constexpr bool Admitted(int playerMap,int runMap,bool privateRun,bool participant,bool released){
 return playerMap>=10000&&ForestMap(playerMap)&&playerMap==runMap&&privateRun&&participant&&!released;
}
}
