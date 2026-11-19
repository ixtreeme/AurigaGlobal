// CombatStateTests exercise the untyped combat kernel. Durable typed services
// are covered by their own domain/integration tests and are inactive here.
#include "../../SRC/Server/GameServer/item/TypedSoulClock.hpp"
void TypedMissionMobKilled(entt::entity,unsigned,bool){}
TypedBowProjectileScope::TypedBowProjectileScope(entt::entity e):entity(e){}
TypedBowProjectileScope::~TypedBowProjectileScope()=default;
bool HasTypedBowAmmunition(entt::entity){return false;}
bool CanTypedBowShoot(entt::entity,entt::entity){return true;}
bool TypedSoulDamageLocked(entt::entity){return false;}
bool QueueTypedSoulDamage(entt::entity,entt::entity,std::int64_t,std::uint8_t,std::function<void(std::int64_t)>){return false;}
namespace SkillSystem {bool CheckSkillHit(entt::entity,unsigned char,entt::entity){return true;}}
