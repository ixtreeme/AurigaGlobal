#pragma once

#include <entt/entity/entity.hpp>

struct lua_State;
namespace auriga::item { struct PersistentItem; }

namespace quest
{
void UseTypedRuneCooldownStone(entt::entity, const auriga::item::PersistentItem&);
int pc_typed_change_name(lua_State*);
int pc_typed_change_empire(lua_State*);
int pc_typed_change_sex(lua_State*);
int pc_typed_change_guild_master(lua_State*);
int pc_typed_reset_dungeon_cooldown(lua_State*);
int pc_typed_reset_level(lua_State*);
}
