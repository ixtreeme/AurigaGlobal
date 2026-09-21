#ifndef __INC_METIN_II_GAME_DUNGEON_H
#define __INC_METIN_II_GAME_DUNGEON_H

#include "sectree_manager.h"
#include "ecs/Registry.hpp"

#include <entt/entt.hpp>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ecs
{
	// Authoritative state of one dungeon instance. The durable identity of the
	// instance for the managers is the id (and the private map index); the
	// character relations (members' DungeonMembership, parties' dungeon handle)
	// are entity handles that every reader validates.
	struct DungeonState
	{
		uint32_t id { 0 };
		int32_t originalMapIndex { 0 };
		int32_t mapIndex { 0 };

		// Characters counted into the instance (IncMember/IncPartyMember); the
		// set is what arms the dead event when it becomes empty.
		std::unordered_set<entt::entity> members;
		// Every monster/stone assigned through SetDungeon, so CountMonster can
		// prune handles whose entity is gone instead of drifting (B4).
		std::unordered_set<entt::entity> monsters;

		std::map<std::string, int> flags;
		std::unordered_map<entt::entity, int> partyCounts;
		std::map<std::string, entt::entity> uniqueMobs;

		std::vector<LPREGEN> regens;
		size_t regenId { 0 };

		LPEVENT deadEvent { nullptr };

		bool completed { false };
#ifdef __DEFENSE_WAVE__
		entt::entity mast { entt::null };
#endif
	};
}

// Native API over ecs::DungeonState. Every entry point validates the dungeon
// entity before use, so a retired or recycled handle is a no-op. The dungeon
// is always the first parameter; there is no heap CDungeon object any more.
namespace DungeonSystem
{
	using IdType = uint32_t;

	// Defined here because ForEachMember below resolves the state inline.
	inline ecs::DungeonState* Find(entt::entity dungeon)
	{
		if (dungeon == entt::null || !g_registry.valid(dungeon))
			return nullptr;

		return g_registry.try_get<ecs::DungeonState>(dungeon);
	}

	bool IsValid(entt::entity dungeon);

	IdType GetId(entt::entity dungeon);
	int32_t GetMapIndex(entt::entity dungeon);

	void Initialize(entt::entity dungeon);
	// Clears the member/party/monster relations, cancels the timers and regens
	// and retires the entity. The caller (CDungeonManager) owns the private map.
	void Destroy(entt::entity dungeon);
	// Whether any player is still standing on the dungeon's private map.
	bool HasLivePlayers(entt::entity dungeon);

	// Membership. Entering through the warps registers the members so the
	// instance can actually retire; leaving the map clears the membership
	// through SocialSystem::SetDungeon.
	void Join_Coords(entt::entity dungeon, entt::entity character, int32_t X, int32_t Y, int32_t index);
	void JoinParty_Coords(entt::entity dungeon, entt::entity pParty, int32_t X, int32_t Y, int32_t index);
	void JoinParty(entt::entity dungeon, entt::entity pParty);
	void QuitParty(entt::entity dungeon, entt::entity pParty);
	void IncMember(entt::entity dungeon, entt::entity character);
	void DecMember(entt::entity dungeon, entt::entity character);
	void IncPartyMember(entt::entity dungeon, entt::entity pParty, entt::entity character);
	void DecPartyMember(entt::entity dungeon, entt::entity pParty, entt::entity character);

	// The monster side of the membership: a handle set that CountMonster
	// prunes, so a monster destroyed outside the kill path cannot leave the
	// count behind (B4).
	void AddMonster(entt::entity dungeon, entt::entity monster);
	void RemoveMonster(entt::entity monster);
	int32_t CountMonster(entt::entity dungeon);

	// The character-side relation. SocialSystem::SetDungeon/GetDungeon and the
	// movement leave hook delegate here, so the membership bookkeeping lives
	// with the dungeon state itself.
	void SetMemberDungeon(entt::entity e, entt::entity dungeon);
	entt::entity GetMemberDungeon(entt::entity e);
	void ClearMemberDungeonIfOtherMap(entt::entity e, int32_t mapIndex);

	void Purge(entt::entity dungeon);
	void KillAll(entt::entity dungeon);
	void KillAllMonsters(entt::entity dungeon);
#ifdef __DEFENSE_WAVE__
	void KillAllMonstersHydra(entt::entity dungeon);
#endif

	void SetFlag(entt::entity dungeon, std::string name, int32_t value);
	int GetFlag(entt::entity dungeon, std::string name);

	entt::entity SpawnMob(entt::entity dungeon, int32_t vnum, int32_t x, int32_t y, int32_t dir = 0);

	void SpawnRegen(entt::entity dungeon, const char* filename, bool once = true);
	void AddRegen(entt::entity dungeon, LPREGEN regen);
	void ClearRegen(entt::entity dungeon);
	bool IsValidRegen(entt::entity dungeon, LPREGEN regen, size_t regen_id);

	void SetUnique(entt::entity dungeon, const char* key, uint32_t vid);
	void KillUnique(entt::entity dungeon, std::string_view key);
	bool IsUniqueDead(entt::entity dungeon, std::string_view key);
	// -1 when the key is unknown, so a caller can tell it apart from vid 0.
	int32_t GetUniqueVid(entt::entity dungeon, std::string_view key);

	void DeadCharacter(entt::entity dungeon, entt::entity character);

	void JumpAll(entt::entity dungeon, int32_t idx, int32_t x, int32_t y);

	void ExitAllLobby(entt::entity dungeon, uint8_t lobby);

#ifdef __DEFENSE_WAVE__
	entt::entity GetMast(entt::entity dungeon);
	void SetMast(entt::entity dungeon, entt::entity mast);
	void UpdateMastHP(entt::entity dungeon);
	void RestoreMastPartialHP(entt::entity dungeon);
#endif

	void CmdChat(entt::entity dungeon, const char* msg);

	void Notice(
		entt::entity dungeon,
#ifdef TEXTS_IMPROVEMENT
		uint32_t idx,
#endif
		const char* msg
#ifdef TEXTS_IMPROVEMENT
		, bool big = false
#endif
	);

	template <class Func>
	Func ForEachMember(entt::entity dungeon, Func f)
	{
		ecs::DungeonState* state = Find(dungeon);
		if (!state)
			return f;

		// The walk may retire members or even the dungeon; iterate a snapshot.
		const std::unordered_set<entt::entity> members = state->members;

		for (const entt::entity member : members)
		{
			f(member);
			if (!g_registry.valid(dungeon))
				break;
		}

		return f;
	}
}

class CDungeonManager : public singleton<CDungeonManager>
{
	public:
		typedef std::map<DungeonSystem::IdType, entt::entity> TDungeonMap;
		typedef std::map<int32_t, entt::entity> TMapDungeon;

	public:
		CDungeonManager();
		virtual ~CDungeonManager();

		entt::entity	Create(int32_t lOriginalMapIndex);
		void		Destroy(DungeonSystem::IdType dungeon_id);
		entt::entity	Find(DungeonSystem::IdType dungeon_id);
		entt::entity	FindByMapIndex(int32_t lMapIndex);

	private:
		TDungeonMap	m_map_pkDungeon;
		TMapDungeon	m_map_pkMapDungeon;

		// <Factor> Introduced unsigned 32-bit dungeon identifier
		DungeonSystem::IdType next_id_;
};

#endif
