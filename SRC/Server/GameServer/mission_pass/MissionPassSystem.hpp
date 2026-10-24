#pragma once
#include "MissionPassRules.hpp"
#include <entt/entt.hpp>
#include "../command/cmd.h"
ACMD(do_missionpass);
namespace AurigaMissionPass {
void Tick();
void Flush();
void Record(entt::entity player,Kind kind,uint32_t subject=0,uint32_t amount=1,
    uint32_t targetLevel=0,uint32_t participants=1,bool guildParty=false,bool partyEligible=true);
void OnKill(entt::entity player,entt::entity victim);
void DungeonEnter(entt::entity player,int32_t map);
void DungeonComplete(int32_t map,uint32_t type);
void HandleCommand(entt::entity player,const char* arguments);
}
