#pragma once

#include <entt/entt.hpp>

#include <common/tables.h>

#include "../core/typedef.h"

class EntityFactory {
public:
    static entt::entity EnsureCharacterEntity(entt::registry& reg, uint32_t legacyVID);
    static entt::entity CreatePC(entt::registry& reg, const TPlayerTable& data, LPDESC desc, uint32_t legacyVID);
    static entt::entity CreateMonster(entt::registry& reg, const TMobTable& data, int x, int y, int mapIndex, uint32_t legacyVID);
    static entt::entity CreateNPC(entt::registry& reg, const TMobTable& data, int x, int y, int mapIndex, uint32_t legacyVID);
    static entt::entity CreateStone(entt::registry& reg, const TMobTable& data, int x, int y, int mapIndex, uint32_t legacyVID);
    static entt::entity CreateItemEntity(entt::registry& reg, const TItemTable* proto,
        uint32_t vnum, uint32_t id, uint32_t vid, uint32_t mask = 0);
    static void DestroyItemEntity(entt::registry& reg, entt::entity item);

    static void Destroy(entt::registry& reg, entt::entity e);
};
