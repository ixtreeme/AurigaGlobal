#include "../../SRC/Server/GameServer/raid/EclipseRaidSystem.hpp"

// These isolated movement/combat/point/affect/skill fixtures have no raid
// controller or raid actors. Keep their external encounter boundary explicit,
// without linking a live GameServer and all its services into a unit test.
// Raid controllers are exercised separately by the ICC domain/runtime suites.
namespace EclipseRaid {
bool MovementLocked(entt::entity) { return false; }
float MovementSpeed(entt::entity) { return 0.0f; }
int64_t FilterHealing(entt::entity, int64_t amount) { return amount; }
void Dispel(entt::entity) {}
bool AllowCombat(entt::entity, entt::entity) { return true; }
int64_t FilterDamage(entt::entity, entt::entity, int64_t amount) { return amount; }
void OnDeath(entt::entity, entt::entity) {}
}
