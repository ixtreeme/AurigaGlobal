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

bool IsImmune(entt::entity e, uint32_t immuneFlag);
void ApplyMobAttribute(entt::entity target, const TMobTable* table);

CAffect* FindAffect(entt::entity e, uint32_t type, uint8_t apply = APPLY_NONE);
bool IsAffectFlag(entt::entity e, uint32_t flag);
bool AddAffect(entt::entity e, uint32_t type, uint8_t applyOn, int32_t applyValue,
               uint32_t flag, int32_t duration, int32_t spCost, bool overwrite,
               bool isCube = false);
bool RemoveAffect(entt::entity e, uint32_t type);
bool RemoveAffect(entt::entity e, CAffect* affect);
void ClearAffect(entt::entity e, bool save);
void RefreshAffect(entt::entity e);
void UpdateAffect(entt::registry& reg, uint32_t tick);

} // namespace AffectSystem

void AffectSystem_Update(entt::registry& reg, uint32_t tick);
