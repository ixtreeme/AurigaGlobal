#pragma once
#include <entt/entt.hpp>
#include <cstdint>
#include <string>
#include <string_view>
// Optional sinks keep the dungeon/combat domain independently testable. Only
// GameServer installs the database/network implementation.
namespace DungeonUi::Events {
inline void(*enter)(entt::entity,int)=nullptr;
inline void(*complete)(int)=nullptr;
inline void(*reset)(int)=nullptr;
inline void(*damage)(entt::entity,entt::entity,std::uint64_t)=nullptr;
inline void(*death)(entt::entity)=nullptr;
inline void(*healing)(entt::entity,entt::entity,std::uint64_t)=nullptr;
inline std::string(*context)(entt::entity)=nullptr;
inline void(*loot)(std::string_view,std::uint32_t,std::uint64_t,unsigned,unsigned)=nullptr;
inline void(*experience)(entt::entity,std::uint64_t)=nullptr;
inline thread_local entt::entity healer=entt::null;
inline thread_local unsigned experienceDepth=0;
struct HealingScope {entt::entity previous;explicit HealingScope(entt::entity e):previous(healer){healer=e;}~HealingScope(){healer=previous;}};
struct ExperienceScope {bool outer;ExperienceScope():outer(experienceDepth++==0){}~ExperienceScope(){--experienceDepth;}};
inline void Enter(entt::entity e,int map){if(enter)enter(e,map);}
inline void Complete(int map){if(complete)complete(map);}
inline void Reset(int map){if(reset)reset(map);}
inline void Damage(entt::entity a,entt::entity v,std::uint64_t n){if(damage&&n)damage(a,v,n);}
inline void Death(entt::entity v){if(death)death(v);}
inline void Healing(entt::entity a,entt::entity v,std::uint64_t n){if(healing&&n)healing(a,v,n);}
inline std::string Context(entt::entity e){return context?context(e):std::string{};}
inline void Loot(std::string_view run,std::uint32_t pid,std::uint64_t key,unsigned level,unsigned count){if(loot&&count&&!run.empty())loot(run,pid,key,level,count);}
inline void Experience(entt::entity e,std::uint64_t amount){if(experience&&amount)experience(e,amount);}
}
