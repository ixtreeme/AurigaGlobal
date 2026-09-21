#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "dungeon.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "party.h"
#include "affect.h"
#include "packet.h"
#include "desc.h"
#include "config.h"
#include "regen.h"
#include "start_position.h"
#include "item.h"
#include "item_manager.h"
#include "utils.h"
#include "questmanager.h"
#include "ecs/EventDispatcher.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/social_components.hpp"
#include "ecs/components/dirty_components.hpp"
#include "ecs/components/spatial_components.hpp"
#include "ecs/events.hpp"
#include "ecs/systems/ItemSystem.hpp"

EVENTINFO(dungeon_id_info)
{
	entt::entity dungeon { entt::null };

	dungeon_id_info()
	{
	}
};

EVENTFUNC(dungeon_dead_event);

namespace
{
	// The retired instance's relations are cleared by the system, so the only
	// thing the death callback needs is a re-arm while players still stand on
	// the private map.
	void ArmDeadEvent(entt::entity dungeon, ecs::DungeonState& state)
	{
		dungeon_id_info* info = AllocEventInfo<dungeon_id_info>();
		info->dungeon = dungeon;

		event_cancel(&state.deadEvent);

		const int iSec = state.completed ? 3 : 300;
		state.deadEvent = event_create(dungeon_dead_event, info, PASSES_PER_SEC(iSec));
	}
}

EVENTFUNC(dungeon_dead_event)
{
	dungeon_id_info* info = dynamic_cast<dungeon_id_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("dungeon_dead_event> <Factor> Null pointer");
		return 0;
	}

	ecs::DungeonState* state = DungeonSystem::Find(info->dungeon);

	// A cancelled or replaced callback cannot act on a recycled instance.
	if (!state || state->deadEvent != event)
		return 0;

	// The member set can be wrong (a member never tracked, a stale handle); the
	// map is the source of truth. Never tear it down under live players.
	if (DungeonSystem::HasLivePlayers(info->dungeon))
		return PASSES_PER_SEC(3);

	state->deadEvent = nullptr;

	const uint32_t id = state->id;
	CDungeonManager::instance().Destroy(id);
	g_dispatcher.trigger(ecs::EvDungeonDead { info->dungeon });
	return 0;
}

namespace DungeonSystem
{
	bool IsValid(entt::entity dungeon)
	{
		return Find(dungeon) != nullptr;
	}

	IdType GetId(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		return state ? state->id : 0;
	}

	int32_t GetMapIndex(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		return state ? state->mapIndex : 0;
	}

	void Initialize(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		state->completed = false;
		state->deadEvent = nullptr;
		state->regenId = 0;
	}

	void Destroy(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		// Drop the party handles first: the members' own cleanup below must not
		// walk a dying instance's party counters.
		std::vector<entt::entity> parties;
		parties.reserve(state->partyCounts.size());
		for (const auto& row : state->partyCounts)
			parties.push_back(row.first);

		state->partyCounts.clear();

		for (const entt::entity party : parties)
		{
			if (party != entt::null && g_registry.valid(party))
				PartySystem::SetDungeon(party, entt::null);
		}

		// Members and monsters: clear the membership directly. The dec
		// bookkeeping of a dying instance is meaningless and would re-enter
		// the teardown.
		auto clearMembership = [](entt::entity e)
		{
			if (e == entt::null || !g_registry.valid(e))
				return;

			if (auto* membership = g_registry.try_get<ecs::DungeonMembership>(e))
			{
				membership->dungeon = entt::null;
				g_registry.emplace_or_replace<ecs::DirtyTag>(e);
			}
		};

		for (const entt::entity member : state->members)
			clearMembership(member);

		for (const entt::entity monster : state->monsters)
			clearMembership(monster);

		state->members.clear();
		state->monsters.clear();

		ClearRegen(dungeon);

		state = Find(dungeon);
		if (!state)
			return;

		event_cancel(&state->deadEvent);

		g_registry.destroy(dungeon);
	}

	bool HasLivePlayers(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return false;

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (!map)
			return false;

		struct FFindPlayer
		{
			bool found { false };

			void operator()(entt::entity e)
			{
				if (!found && ecs::IsCharacter(e) && ecs::PlayerRuntime::IsPC(e))
					found = true;
			}
		} f;

		map->for_each(f);
		return f.found;
	}

	void IncMember(entt::entity dungeon, entt::entity character)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state || character == entt::null)
			return;

		state->members.insert(character);

		event_cancel(&state->deadEvent);
	}

	void DecMember(entt::entity dungeon, entt::entity character)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		auto it = state->members.find(character);

		if (it == state->members.end())
			return;

		state->members.erase(it);

		if (state->members.empty())
			ArmDeadEvent(dungeon, *state);
	}

	void IncPartyMember(entt::entity dungeon, entt::entity pParty, entt::entity character)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		auto it = state->partyCounts.find(pParty);

		if (it != state->partyCounts.end())
			it->second++;
		else
			state->partyCounts.emplace(pParty, 1);

		IncMember(dungeon, character);
	}

	void DecPartyMember(entt::entity dungeon, entt::entity pParty, entt::entity character)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		auto it = state->partyCounts.find(pParty);

		if (it == state->partyCounts.end())
			LOG_ERROR("cannot find party");
		else
		{
			it->second--;

			// Defensive: a counter that was never armed must still release.
			if (it->second <= 0)
				QuitParty(dungeon, pParty);
		}

		DecMember(dungeon, character);
	}

	void AddMonster(entt::entity dungeon, entt::entity monster)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state || monster == entt::null)
			return;

		state->monsters.insert(monster);
	}

	void RemoveMonster(entt::entity monster)
	{
		if (monster == entt::null || !g_registry.valid(monster))
			return;

		const auto* membership = g_registry.try_get<ecs::DungeonMembership>(monster);
		if (!membership || membership->dungeon == entt::null)
			return;

		ecs::DungeonState* state = Find(membership->dungeon);
		if (state)
			state->monsters.erase(monster);
	}

	int32_t CountMonster(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return 0;

		// Handles whose entity is gone are dropped here, so a monster destroyed
		// outside the kill path cannot leave the count behind.
		int32_t count = 0;

		for (auto it = state->monsters.begin(); it != state->monsters.end(); )
		{
			const entt::entity monster = *it;

			if (monster == entt::null || !g_registry.valid(monster))
				it = state->monsters.erase(it);
			else
			{
				++count;
				++it;
			}
		}

		return count;
	}

	entt::entity GetMemberDungeon(entt::entity e)
	{
		if (e == entt::null || !g_registry.valid(e))
			return entt::null;

		const auto* membership = g_registry.try_get<ecs::DungeonMembership>(e);
		if (!membership || membership->dungeon == entt::null)
			return entt::null;

		return IsValid(membership->dungeon) ? membership->dungeon : entt::null;
	}

	void SetMemberDungeon(entt::entity e, entt::entity pkDungeon)
	{
		if (e == entt::null || !g_registry.valid(e))
			return;

		if (pkDungeon != entt::null && !IsValid(pkDungeon))
			pkDungeon = entt::null;

		auto& membership = g_registry.get_or_emplace<ecs::DungeonMembership>(e);
		const entt::entity previous = membership.dungeon;

		if (previous == pkDungeon)
		{
			if (pkDungeon != entt::null)
				g_registry.emplace_or_replace<ecs::DirtyTag>(e);
			return;
		}

		if (pkDungeon != entt::null && previous != entt::null)
		{
			LOG_ERROR("{} is trying to reassigning dungeon (current {}, new dungeon {})", ecs::PlayerRuntime::GetName(e).data(), static_cast<uint32_t>(previous), static_cast<uint32_t>(pkDungeon));
		}

		if (previous != entt::null)
		{
			if (ecs::PlayerRuntime::IsPC(e))
			{
				const entt::entity party = ecs::SocialSystem::GetParty(e);
				if (party != entt::null)
					DecPartyMember(previous, party, e);
				else
					DecMember(previous, e);
			}
			else if (ecs::PlayerRuntime::IsMonster(e) || ecs::PlayerRuntime::IsStone(e))
			{
				RemoveMonster(e);
			}
		}

		membership.dungeon = pkDungeon;

		if (pkDungeon != entt::null)
		{
			if (ecs::PlayerRuntime::IsPC(e))
			{
				const entt::entity party = ecs::SocialSystem::GetParty(e);
				if (party != entt::null)
					IncPartyMember(pkDungeon, party, e);
				else
					IncMember(pkDungeon, e);
			}
			else if (ecs::PlayerRuntime::IsMonster(e) || ecs::PlayerRuntime::IsStone(e))
			{
				AddMonster(pkDungeon, e);
			}
		}
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	}

	// The membership is only valid while the character is on the dungeon's map.
	// Legacy cleared it at entity destruction alone, so an ex-member stayed
	// counted and a one-member set could destroy an instance under the rest of
	// the party.
	void ClearMemberDungeonIfOtherMap(entt::entity e, int32_t mapIndex)
	{
		if (e == entt::null || !g_registry.valid(e))
			return;

		const auto* membership = g_registry.try_get<ecs::DungeonMembership>(e);
		if (!membership || membership->dungeon == entt::null)
			return;

		if (GetMapIndex(membership->dungeon) != mapIndex)
			SetMemberDungeon(e, entt::null);
	}

	namespace
	{
		struct FWarpToDungeonCoords		{
			entt::entity dungeon { entt::null };
			entt::entity party { entt::null };
			int32_t mapIndex { 0 };
			int32_t x { 0 };
			int32_t y { 0 };

			void operator () (entt::entity member)
			{
				// The membership carries the counting: SetDungeon registers the
				// member (and its party) so a warp out can release it again.
				ecs::SocialSystem::SetDungeon(member, dungeon);
				ecs::MovementSystem::SaveExitLocation(member);
				ecs::MovementSystem::WarpSet(member, x, y, mapIndex);
			}
		};

		struct FWarpToDungeon
		{
			entt::entity dungeon { entt::null };
			entt::entity party { entt::null };
			int32_t mapIndex { 0 };
			int32_t x { 0 };
			int32_t y { 0 };

			void operator () (entt::entity member)
			{
				ecs::SocialSystem::SetDungeon(member, dungeon);
				ecs::MovementSystem::SaveExitLocation(member);
				ecs::MovementSystem::WarpSet(member, x, y, mapIndex);
			}
		};

		struct FWarpToPosition
		{
			int32_t lMapIndex;
			int32_t x;
			int32_t y;
			FWarpToPosition(int32_t lMapIndex, int32_t x, int32_t y)
				: lMapIndex(lMapIndex), x(x), y(y)
				{}

			void operator()(entt::entity chEntity)
			{
				if (!ecs::IsCharacter(chEntity)) {
					return;
				}

				if (!ecs::PlayerRuntime::IsPC(chEntity)) {
					return;
				}
				if (ecs::PlayerRuntime::GetMapIndex(chEntity) == lMapIndex)
				{
					ecs::MovementSystem::Show(chEntity, lMapIndex, x, y, 0);
					ecs::MovementSystem::Stop(chEntity);
				}
				else
				{
					ecs::MovementSystem::WarpSet(chEntity, x,y,lMapIndex);
				}
			}
		};

		struct FExitDungeonLobby
		{
			uint8_t lobby;
			FExitDungeonLobby() : lobby(0) {};

			void operator()(entt::entity chEntity)
			{
				if (ecs::IsCharacter(chEntity))
				{

					if (ecs::PlayerRuntime::IsPC(chEntity))
					{
						if (lobby == 1)
						{
							ecs::MovementSystem::WarpSet(chEntity, 535400, 1428400);
						}
						else if (lobby == 2)
						{
							ecs::MovementSystem::WarpSet(chEntity, 536900, 1331400);
						}
						else if (lobby == 3)
						{
							ecs::MovementSystem::WarpSet(chEntity, 645800, 351400);
						}
					}
				}
			}
		};

		struct FCmdChat
		{
			FCmdChat(const char * psz) : m_psz(psz)
			{
			}

			void operator() (entt::entity chEntity)
			{
				if (ecs::IsCharacter(chEntity))
				{

					if (ecs::PlayerRuntime::IsPC(chEntity))
					{
						ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "%s", m_psz);
					}
				}
			}

			const char * m_psz;
		};

		struct FNotice
		{
			FNotice(
#ifdef TEXTS_IMPROVEMENT
			uint32_t idx, bool big,
#endif
			const char * psz):
#ifdef TEXTS_IMPROVEMENT
			m_idx(idx), m_big(big),
#endif
			m_psz(psz)
			{
			}

			void operator() (entt::entity chEntity) {
				if (ecs::IsCharacter(chEntity)) {

					if (ecs::PlayerRuntime::IsPC(chEntity)) {
#ifdef TEXTS_IMPROVEMENT
						if (m_big == true)
						{
							ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_BIG_NOTICE, m_idx, m_psz);
						}
						else
						{
							ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_NOTICE, m_idx, m_psz);
						}
#else
						ecs::ChatSystem::Send(chEntity, CHAT_TYPE_NOTICE, "%s", m_psz);
#endif
					}
				}
			}

#ifdef TEXTS_IMPROVEMENT
			uint32_t m_idx;
			bool m_big;
#endif
			const char * m_psz;
		};

		struct FKillSectree
		{
			void operator () (entt::entity character)
			{
				if (!ecs::IsCharacter(character))
					return;

				if (!ecs::PlayerRuntime::IsPC(character) && !ecs::PlayerRuntime::IsPet(character) && !ecs::PlayerRuntime::IsMount(character)
#ifdef __NEWPET_SYSTEM__
					 && !ecs::PlayerRuntime::IsNewPet(character)
#endif
				)
				{
					CombatSystem::Dead(character);
				}
			}
		};

		struct FKillMonstersSectree
		{
			void operator () (entt::entity character)
			{
				if (!ecs::IsCharacter(character))
					return;

				if (!ecs::PlayerRuntime::IsPC(character) && (ecs::PlayerRuntime::GetCharType(character) == CHAR_TYPE_MONSTER || ecs::PlayerRuntime::IsStone(character)))
				{
					CombatSystem::Dead(character);
				}
			}
		};

#ifdef __DEFENSE_WAVE__
		struct FKillMonstersHydraSectree
		{
			void operator () (entt::entity character)
			{
				if (!ecs::IsCharacter(character))
					return;

				if (!ecs::PlayerRuntime::IsPC(character) && (ecs::PlayerRuntime::GetCharType(character) == CHAR_TYPE_MONSTER || ecs::PlayerRuntime::IsStone(character)))
				{
					int32_t racevnum = ecs::PlayerRuntime::GetRaceNum(character);
					if (racevnum != 3963 && racevnum != 3964)
					{
						CombatSystem::Dead(character);
					}
				}
			}
		};
#endif

		struct FPurgeSectree
		{
			void operator () (entt::entity entity)
			{
				if (ItemSystem::IsValidItem(entity)) {
					ItemSystem::DestroyItemEntityEcs(entity, "DUNGEON_ENTITY_CLEANUP");
					return;
				}
				const auto* kind = g_registry.try_get<ecs::SpatialKindTag>(entity);
				if (!kind || kind->kind != ecs::SpatialKind::Character) return;
				if (!ecs::PlayerRuntime::IsPC(entity) && !ecs::PlayerRuntime::IsPet(entity)
#ifdef __NEWPET_SYSTEM__
					&& !ecs::PlayerRuntime::IsNewPet(entity)
#endif
				)
					M2_DESTROY_CHARACTER(entity);
			}
		};
	}

	void Join_Coords(entt::entity dungeon, entt::entity character, int32_t X, int32_t Y, int32_t index)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		if (character == entt::null || !g_registry.valid(character) ||
			SECTREE_MANAGER::instance().GetMap(state->mapIndex) == nullptr)
		{
			LOG_ERROR("CDungeon: invalid entity or missing SECTREE_MAP for #{}", state->mapIndex);
			return;
		}

		// Entering is what registers the member: without this the instance
		// could never retire. The relation also drives the leave bookkeeping.
		ecs::SocialSystem::SetDungeon(character, dungeon);

		ecs::MovementSystem::SaveExitLocation(character);
		ecs::MovementSystem::WarpSet(character, X * 100, Y * 100, state->mapIndex);
	}

	void JoinParty_Coords(entt::entity dungeon, entt::entity pParty, int32_t X, int32_t Y, int32_t index)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		if (SECTREE_MANAGER::instance().GetMap(state->mapIndex) == nullptr)
		{
			LOG_ERROR("CDungeon: SECTREE_MAP not found for #{}", state->mapIndex);
			return;
		}

		PartySystem::SetDungeon(pParty, dungeon);

		FWarpToDungeonCoords f;
		f.dungeon = dungeon;
		f.party = pParty;
		f.mapIndex = state->mapIndex;
		f.x = X * 100;
		f.y = Y * 100;

		PartySystem::ForEachOnMapMember(pParty, f, index);
	}

	void JoinParty(entt::entity dungeon, entt::entity pParty)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		LPSECTREE_MAP pkSectreeMap = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (pkSectreeMap == nullptr) {
			LOG_ERROR("CDungeon: SECTREE_MAP not found for #{}", state->mapIndex);
			return;
		}

		PartySystem::SetDungeon(pParty, dungeon); // @warme011 the begin of the nightmare

		FWarpToDungeon f;
		f.dungeon = dungeon;
		f.party = pParty;
		f.mapIndex = state->mapIndex;
		f.x = pkSectreeMap->m_setting.posSpawn.x;
		f.y = pkSectreeMap->m_setting.posSpawn.y;

		PartySystem::ForEachOnlineMember(pParty, f);
	}

	void QuitParty(entt::entity dungeon, entt::entity pParty)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		PartySystem::SetDungeon(pParty, entt::null);

		auto it = state->partyCounts.find(pParty); // @warme011 boom! crash!
		if (it != state->partyCounts.end())
			state->partyCounts.erase(it);
	}

	void SetFlag(entt::entity dungeon, std::string name, int32_t value)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		auto it = state->flags.find(name);
		if (it != state->flags.end())
		{
			it->second = value;
		}
		else
		{
			state->flags.insert(make_pair(name, value));
		}
	}

	int GetFlag(entt::entity dungeon, std::string name)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return 0;

		auto it = state->flags.find(name);
		if (it != state->flags.end())
			return it->second;
		else
			return 0;
	}

	// Unique mobs are held by entity. A pointer outlived its mob whenever the mob
	// was destroyed without DeadCharacter hearing about it, and the next read went
	// through freed memory; a stale handle just stops being valid.
	void SetUnique(entt::entity dungeon, const char* key, uint32_t vid)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		const entt::entity mob = ecs::PlayerRuntime::FindByVID(vid);
		if (mob == entt::null) {
			LOG_ERROR("Unknown monster: {} for dungeon {}.", vid, state->mapIndex);
			return;
		}

		state->uniqueMobs.insert(std::make_pair(std::string(key), mob));
		AffectSystem::AddAffect(mob, AFFECT_DUNGEON_UNIQUE, POINT_NONE, 0, AFF_DUNGEON_UNIQUE, 65535, 0, true);
	}

	void KillUnique(entt::entity dungeon, std::string_view key)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		const std::string keyString(key);
		auto it = state->uniqueMobs.find(keyString);
		if (it == state->uniqueMobs.end())
		{
			LOG_ERROR("Unknown get unique: {} for dungeon {}.", keyString.c_str(), state->mapIndex);
			return;
		}

		const entt::entity mob = it->second;
		state->uniqueMobs.erase(it);
		CombatSystem::Dead(mob);
	}

	int32_t GetUniqueVid(entt::entity dungeon, std::string_view key)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return -1;

		const std::string keyString(key);
		auto it = state->uniqueMobs.find(keyString);
		if (it == state->uniqueMobs.end())
		{
			LOG_TRACE("Unknown get unique: {} for dungeon {}.", keyString.c_str(), state->mapIndex);
			return -1;
		}

		if (it->second == entt::null || !g_registry.valid(it->second))
			return -1;

		return static_cast<int32_t>(ecs::PlayerRuntime::GetPacketVID(it->second));
	}

	void DeadCharacter(entt::entity dungeon, entt::entity character)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		if (!ecs::PlayerRuntime::IsPC(character))
		{
			if (AffectSystem::FindAffect(character, AFFECT_DUNGEON_UNIQUE)) {
				auto it = state->uniqueMobs.begin();
				for ( ; it != state->uniqueMobs.end(); ) {
					if (it->second == character)
					{
						it = state->uniqueMobs.erase(it);
						break;
					}
					else
					{
						++it;
					}
				}
			}
		}
	}

	bool IsUniqueDead(entt::entity dungeon, std::string_view key)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return false;

		const std::string keyString(key);
		auto it = state->uniqueMobs.find(keyString);
		if (it == state->uniqueMobs.end())
		{
			LOG_ERROR("Unknown unique: {} for dungeon {}.", keyString.c_str(), state->mapIndex);
			return false;
		}

		return CombatSystem::IsDead(it->second);
	}

	entt::entity SpawnMob(entt::entity dungeon, int32_t vnum, int32_t x, int32_t y, int32_t dir)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return entt::null;

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (!map) {
			LOG_ERROR("cannot find map by index {}", state->mapIndex);
			return entt::null;
		}

		const entt::entity mob = CHARACTER_MANAGER::instance().SpawnMobEntity(vnum, state->mapIndex, map->m_setting.iBaseX+x*100, map->m_setting.iBaseY+y*100, 0, false, dir == 0 ? -1 : dir);

		if (mob != entt::null)
		{
			ecs::SocialSystem::SetDungeon(mob, dungeon);
		}
		else
		{
			LOG_ERROR("cannot spawn: vnum({}), x({}), y({}), dir({}) inside the map {}", vnum, x, y, dir, state->mapIndex);
		}

		return mob;
	}

	void SpawnRegen(entt::entity dungeon, const char* filename, bool once)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		if (!filename)
		{
			LOG_ERROR("CDungeon::SpawnRegen(filename=NULL, once={}) - m_lMapIndex[{}]", once, state->mapIndex);
			return;
		}

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (!map)
		{
			LOG_ERROR("CDungeon::SpawnRegen(filename={}, once={}) - m_lMapIndex[{}]", filename, once, state->mapIndex);
			return;
		}

		regen_do(filename, state->mapIndex, map->m_setting.iBaseX, map->m_setting.iBaseY, dungeon, once);
	}

	void AddRegen(entt::entity dungeon, LPREGEN regen)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state || !regen)
			return;

		regen->id = state->regenId++;
		state->regens.push_back(regen);
	}

	void ClearRegen(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		for (auto it = state->regens.begin(); it != state->regens.end(); ++it)
		{
			LPREGEN regen = *it;

			event_cancel(&regen->event);
			M2_DELETE(regen);
		}
		state->regens.clear();
	}

	bool IsValidRegen(entt::entity dungeon, LPREGEN regen, size_t regen_id) {
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return false;

		auto it = std::find(state->regens.begin(), state->regens.end(), regen);
		if (it == state->regens.end()) {
			return false;
		}
		LPREGEN found = *it;
		return (found->id == regen_id);
	}

	void KillAll(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (map == nullptr)
		{
			LOG_ERROR("CDungeon: SECTREE_MAP not found for #{}", state->mapIndex);
			return;
		}

		FKillSectree f;
		map->for_each(f);
	}

	void KillAllMonsters(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (map == nullptr)
		{
			LOG_ERROR("CDungeon: SECTREE_MAP not found for #{}", state->mapIndex);
			return;
		}

		FKillMonstersSectree f;
		map->for_each(f);
	}

#ifdef __DEFENSE_WAVE__
	void KillAllMonstersHydra(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (map == nullptr)
		{
			LOG_ERROR("CDungeon: SECTREE_MAP not found for #{}", state->mapIndex);
			return;
		}

		FKillMonstersHydraSectree f;
		map->for_each(f);
	}
#endif

	void Purge(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		LPSECTREE_MAP pkMap = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (pkMap == nullptr) {
			LOG_ERROR("CDungeon: SECTREE_MAP not found for #{}", state->mapIndex);
			return;
		}
		FPurgeSectree f;
		pkMap->for_each(f);
	}

	void ExitAllLobby(entt::entity dungeon, uint8_t lobby)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (!map)
		{
			LOG_ERROR("cannot find map by index {}", state->mapIndex);
			return;
		}

		FExitDungeonLobby f;
		f.lobby = lobby;

		map->for_each(f);
		state->completed = true;
	}

	void CmdChat(entt::entity dungeon, const char* msg)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (!map)
		{
			LOG_ERROR("cannot find map by index {}", state->mapIndex);
			return;
		}

		FCmdChat f(msg);
		map->for_each(f);
	}

	void Notice(
		entt::entity dungeon,
#ifdef TEXTS_IMPROVEMENT
		uint32_t idx,
#endif
		const char* msg
#ifdef TEXTS_IMPROVEMENT
		, bool big
#endif
	)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		LOG_INFO("XXX Dungeon Notice {} {}", static_cast<uint32_t>(dungeon), msg);
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (!pMap)
		{
			LOG_ERROR("cannot find map by index {}", state->mapIndex);
			return;
		}

		FNotice f(
#ifdef TEXTS_IMPROVEMENT
		idx, big,
#endif
		msg);
		pMap->for_each(f);
	}

	void JumpAll(entt::entity dungeon, int32_t idx, int32_t x, int32_t y)
	{
		x *= 100;
		y *= 100;

		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(idx);
		if (!pMap)
		{
			LOG_ERROR("cannot find map by index {}", idx);
			return;
		}

		FWarpToPosition f(idx, x, y);

		pMap->for_each(f);
	}

#ifdef __DEFENSE_WAVE__
	struct SUpdateMastHp
	{
		SUpdateMastHp(int64_t value) : m_value(value) {}

		void operator () (entt::entity chEntity)
		{
			if (ecs::IsCharacter(chEntity))
			{

				if (ecs::PlayerRuntime::IsPC(chEntity))
				{
					ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "BINARY_Update_Mast_HP %d", m_value);
				}
			}
		}

		int64_t m_value;
	};

	entt::entity GetMast(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		return state ? state->mast : entt::null;
	}

	void SetMast(entt::entity dungeon, entt::entity mast)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (state)
			state->mast = mast;
	}

	void UpdateMastHP(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (!map)
		{
			LOG_ERROR("cannot find map by index {}", state->mapIndex);
			return;
		}

		const entt::entity mast = GetMast(dungeon);
		if (ecs::PlayerRuntime::IsValid(mast))
		{
			SUpdateMastHp f(ecs::PlayerRuntime::GetHP(mast));
			map->for_each(f);
		}
	}

	void RestoreMastPartialHP(entt::entity dungeon)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return;

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(state->mapIndex);
		if (!map)
		{
			LOG_ERROR("cannot find map by index {}", state->mapIndex);
			return;
		}

		const entt::entity mast = GetMast(dungeon);
		if (ecs::PlayerRuntime::IsValid(mast))
		{
			int64_t hp = ecs::PlayerRuntime::GetHP(mast);
			int32_t add = 600000;
			if (hp + add >= 12000000)
			{
				ecs::PlayerRuntime::SetHP(mast, 12000000);
			}
			else
			{
				ecs::PlayerRuntime::SetHP(mast, hp + add);
			}

			SUpdateMastHp f(ecs::PlayerRuntime::GetHP(mast));
			map->for_each(f);
		}
	}
#endif
}

entt::entity CDungeonManager::Create(int32_t lOriginalMapIndex)
{
	uint32_t lMapIndex = SECTREE_MANAGER::instance().CreatePrivateMap(lOriginalMapIndex);

	if (!lMapIndex)
	{
		LOG_INFO("Fail to Create Dungeon : OrginalMapindex {} NewMapindex {}", lOriginalMapIndex, lMapIndex);
		return entt::null;
	}

	// <Factor> TODO: Change id assignment, or drop it
	DungeonSystem::IdType id = next_id_++;
	while (Find(id) != entt::null) {
		id = next_id_++;
	}

	const entt::entity dungeon = g_registry.create();
	auto& state = g_registry.emplace<ecs::DungeonState>(dungeon);

	state.id = id;
	state.originalMapIndex = lOriginalMapIndex;
	state.mapIndex = lMapIndex;

	DungeonSystem::Initialize(dungeon);

	m_map_pkDungeon.insert(std::make_pair(id, dungeon));
	m_map_pkMapDungeon.insert(std::make_pair(lMapIndex, dungeon));

	return dungeon;
}

void CDungeonManager::Destroy(DungeonSystem::IdType dungeon_id)
{
	LOG_INFO("DUNGEON destroy : map index {}", dungeon_id);

	const entt::entity dungeon = Find(dungeon_id);
	if (dungeon == entt::null) {
		return;
	}

	ecs::DungeonState* state = DungeonSystem::Find(dungeon);
	const int32_t lMapIndex = state ? state->mapIndex : 0;

	m_map_pkDungeon.erase(dungeon_id);

	if (lMapIndex)
		m_map_pkMapDungeon.erase(lMapIndex);

	// The state and its relations go first: the map drain below would
	// otherwise re-enter a half-dead instance through the memberships.
	DungeonSystem::Destroy(dungeon);

	uint32_t server_timer_arg = lMapIndex;
	quest::CQuestManager::instance().CancelServerTimers(server_timer_arg);

	SECTREE_MANAGER::instance().DestroyPrivateMap(lMapIndex);
}

entt::entity CDungeonManager::Find(DungeonSystem::IdType dungeon_id)
{
	auto it = m_map_pkDungeon.find(dungeon_id);
	if (it == m_map_pkDungeon.end())
		return entt::null;

	ecs::DungeonState* state = DungeonSystem::Find(it->second);
	if (!state || state->id != dungeon_id)
		return entt::null;

	return it->second;
}

entt::entity CDungeonManager::FindByMapIndex(int32_t lMapIndex)
{
	auto it = m_map_pkMapDungeon.find(lMapIndex);
	if (it == m_map_pkMapDungeon.end()) {
		return entt::null;
	}

	ecs::DungeonState* state = DungeonSystem::Find(it->second);
	if (!state || state->mapIndex != lMapIndex)
		return entt::null;

	return it->second;
}

CDungeonManager::CDungeonManager()
	: next_id_(0)
{
}

CDungeonManager::~CDungeonManager()
{
}
