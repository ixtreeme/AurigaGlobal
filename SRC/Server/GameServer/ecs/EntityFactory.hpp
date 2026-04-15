#pragma once

#include <entt/entt.hpp>

#include <common/tables.h>

#include "../typedef.h"

class EntityFactory {
public:
    static entt::entity CreatePC(entt::registry& reg, const TPlayerTable& data, LPDESC desc, uint32_t legacyVID);
    static entt::entity CreateMonster(entt::registry& reg, const TMobTable& data, int x, int y, int mapIndex, uint32_t legacyVID);
    static entt::entity CreateNPC(entt::registry& reg, const TMobTable& data, int x, int y, int mapIndex, uint32_t legacyVID);
    static entt::entity CreateStone(entt::registry& reg, const TMobTable& data, int x, int y, int mapIndex, uint32_t legacyVID);

    static void Destroy(entt::registry& reg, entt::entity e);
};
