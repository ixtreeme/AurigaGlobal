#pragma once

#include <cstdint>

#include <entt/entt.hpp>
#include <common/tables.h>

#include "../../affect.h"

namespace AffectSystem {

void ApplyFire(entt::entity target, entt::entity attacker, int amount, int count);
void RemoveFire(entt::entity e);

void ApplyPoison(entt::entity target, entt::entity attacker);
void RemovePoison(entt::entity e);

void ApplyBleeding(entt::entity target, entt::entity attacker);
void RemoveBleeding(entt::entity e);
void CancelDamageEvents(entt::entity e);

bool IsImmune(entt::entity e, uint32_t immuneFlag);
void ApplyMobAttribute(entt::entity target, const TMobTable* table);

CAffect* FindAffect(entt::entity e, uint32_t type, uint8_t apply = APPLY_NONE);
CAffect* FindAffect(entt::entity e, uint32_t type, uint8_t apply, int32_t value);
bool IsAffectFlag(entt::entity e, uint32_t flag);
bool AddAffect(entt::entity e, uint32_t type, uint8_t applyOn, int32_t applyValue,
               uint32_t flag, int32_t duration, int32_t spCost, bool overwrite,
               bool isCube = false);
bool RemoveAffect(entt::entity e, uint32_t type);
bool RemoveAffect(entt::entity e, CAffect* affect);
void RemoveBadAffects(entt::entity e);
void RemoveGoodAffects(entt::entity e);
void ClearAffect(entt::entity e, bool save);
void RefreshAffect(entt::entity e);
void SetPolymorph(entt::entity e, uint32_t raceVnum, bool maintainStats = false);
bool IsPolymorphed(entt::entity e);
int GetPolymorphPower(entt::entity e);
bool IsPolyMaintainStat(entt::entity e);
uint32_t GetPolymorphVnum(entt::entity e);
void UpdateAffect(entt::registry& reg, uint32_t tick);

} // namespace AffectSystem

void AffectSystem_Update(entt::registry& reg, uint32_t tick);
