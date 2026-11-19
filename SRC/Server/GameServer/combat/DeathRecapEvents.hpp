#pragma once
#include <entt/entity/entity.hpp>
#include <cstdint>
namespace DeathRecap::Events {
struct Context {entt::entity attacker=entt::null;std::uint32_t skill=0;std::uint8_t type=0;const bool* critical=nullptr;bool dot=false;};
inline thread_local Context current;
inline thread_local std::uint32_t skill=0;
inline thread_local bool periodic=false;
struct SkillScope {std::uint32_t previous;explicit SkillScope(std::uint32_t value):previous(skill){skill=value;}~SkillScope(){skill=previous;}};
struct PeriodicScope {bool previous;PeriodicScope():previous(periodic){periodic=true;}~PeriodicScope(){periodic=previous;}};
struct DamageScope {
    Context previous;
    DamageScope(entt::entity attacker,std::uint8_t type):previous(current){current={attacker,skill,type,nullptr,periodic||type==9};}
    void Critical(const bool& value){current.critical=&value;}
    ~DamageScope(){current=previous;}
};
inline void(*loss)(entt::entity,std::uint64_t,std::uint64_t,std::uint64_t,const Context&)=nullptr;
inline void(*death)(entt::entity,entt::entity)=nullptr;
inline void(*clear)(entt::entity)=nullptr;
inline void Loss(entt::entity e,std::uint64_t before,std::uint64_t after,std::uint64_t maxHp){if(loss&&before>after)loss(e,before,after,maxHp,current);}
inline void Death(entt::entity e,entt::entity killer){if(death)death(e,killer);}
inline void Clear(entt::entity e){if(clear)clear(e);}
}
