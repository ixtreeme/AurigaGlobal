#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "constants.h"
#include "db.h"
#include "desc.h"
#include "desc_manager.h"
#include "log.h"
#include "packet.h"
#include "protocol.h"
#include "utils.h"
#include "fishing.h"
#include "map_location.h"
#include "sectree_manager.h"
#include "start_position.h"
#include "regen.h"
#include "arena.h"
#include "BattleArena.h"
#include "building.h"
#include "../ecs/components/dirty_components.hpp"
#include "../ecs/systems/ActivitySystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "desc_client.h"
#include "gm.h"

void CInputMain::Fishing(entt::entity character, const char* c_pData)
{
	TPacketCGFishing* p = (TPacketCGFishing*)c_pData;
	ecs::MovementSystem::SetRotation(character, p->dir * 5);
	ActivitySystem::Fishing(character);
	return;
}

void CInputMain::Warp(entt::entity character, const char * pcData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Warp handler ECS
// DUAL-PATH: legacy only during migration window
	ecs::MovementSystem::WarpEnd(character);
}
