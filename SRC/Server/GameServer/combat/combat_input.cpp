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
#include "battle.h"
#include "skill.h"
#include "affect.h"
#include "pvp.h"
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

void CInputMain::Attack(entt::entity character, const uint8_t header, const char* data)
{
	if (!ecs::IsCharacter(character))
		return;


	struct type_identifier
	{
		uint8_t header;
		uint8_t type;
	};

	const struct type_identifier* const type = reinterpret_cast<const struct type_identifier*>(data);

	if (type->type > 0)
	{
		if (false == SkillSystem::CanUseSkill(character, type->type))
		{
			return;
		}

		switch (type->type)
		{
			case SKILL_GEOMPUNG:
			case SKILL_SANGONG:
			case SKILL_YEONSA:
			case SKILL_KWANKYEOK:
			case SKILL_HWAJO:
			case SKILL_GIGUNG:
			case SKILL_PABEOB:
			case SKILL_MARYUNG:
			case SKILL_TUSOK:
			case SKILL_MAHWAN:
			case SKILL_BIPABU:
			case SKILL_NOEJEON:
			case SKILL_CHAIN:
			case SKILL_HORSE_WILDATTACK_RANGE:
				if (HEADER_CG_SHOOT != type->header)
				{
					return;
				}
				break;
		}
	}

	switch (header)
	{
		case HEADER_CG_ATTACK:
			{
				if (nullptr == ecs::PlayerRuntime::GetDesc(character))
				{
					return;
				}

				const TPacketCGAttack* const packMelee = reinterpret_cast<const TPacketCGAttack*>(data);

				ecs::PlayerRuntime::GetDesc(character)->AssembleCRCMagicCube(packMelee->bCRCMagicCubeProcPiece, packMelee->bCRCMagicCubeFilePiece);

				const auto victim = CHARACTER_MANAGER::instance().FindEntity(packMelee->dwVID);

				if (!ecs::PlayerRuntime::IsValid(victim) || character == victim)
				{
					return;
				}

				const auto* victimType = g_registry.try_get<ecs::CharacterType>(victim);
				if (!victimType)
					return;
				switch (victimType->value)
				{
					case CHAR_TYPE_NPC:
					case CHAR_TYPE_WARP:
					case CHAR_TYPE_GOTO:
						return;
				}

				if (packMelee->bType > 0)
				{
					if (false == SkillSystem::CheckSkillHit(character, packMelee->bType, victim))
					{
						return;
					}
				}

				g_registry.emplace_or_replace<ecs::CombatTarget>(character, victim, get_dword_time());
				g_registry.emplace_or_replace<ecs::CombatActiveTag>(character);
				g_registry.emplace_or_replace<ecs::DirtyTag>(character);
				ecs::MovementSystem::OnMove(character, true);
				// Damage execution is still the legacy engine boundary.
				CombatSystem::Attack(character, victim, packMelee->bType);
			}
			break;

		case HEADER_CG_SHOOT:
			{
				const TPacketCGShoot* const packShoot = reinterpret_cast<const TPacketCGShoot*>(data);

				CombatSystem::Shoot(character, packShoot->bType);
			}
			break;
	}
}

void CInputMain::UseSkill(entt::entity character, const char * pcData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate UseSkill handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGUseSkill * p = (TPacketCGUseSkill *) pcData;
	SkillSystem::UseSkill(character, p->dwVnum, CHARACTER_MANAGER::instance().FindEntity(p->dwVID));
}
