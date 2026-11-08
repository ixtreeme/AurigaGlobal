#pragma once
#include <entt/entity/fwd.hpp>
#include <expected>
#include <string>
#include <functional>
#include <cstdint>
struct GameSqlConfig;
std::expected<void,std::string> LoadTypedMissionRuntime(const std::string& folder,const GameSqlConfig&);
void TickTypedMissionRuntime();
bool HandleTypedMissionPacket(entt::entity,const char*);
void TypedMissionNpcInteracted(entt::entity,uint32_t npcVid);
void TypedMissionMobKilled(entt::entity,uint32_t race,bool boss);
// Lua uses server-authored mission IDs and the very same durable operation.
// A true return means queued; the callback alone reports the settled result.
bool TypedMissionQuestAction(entt::entity,const std::string& code,unsigned action,std::function<void(bool)> done);
int TypedMissionQuestStatus(entt::entity,const std::string& code);
