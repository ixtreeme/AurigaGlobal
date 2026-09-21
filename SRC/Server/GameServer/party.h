// Parties are ECS state now: ecs::PartyState on a registry-owned party entity,
// indexed by CPartyManager through the durable player id (and the packet VID
// for mob parties). There is no heap CParty object and no raw party pointer on
// the character; the character relation is a generation-checked entity handle
// in ecs::SocialRefs. PartySystem free functions below replace the old class.
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/Registry.hpp"
#ifndef __INC_METIN_II_GAME_PARTY_H__
#define __INC_METIN_II_GAME_PARTY_H__

#include "char_interface.hpp"
#include <Core/Logging.hpp>
#include <entt/entt.hpp>
#include <array>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

enum // unit : minute
{
	PARTY_ENOUGH_MINUTE_FOR_EXP_BONUS = 60, // the long-party exp bonus unlocks after this many minutes
	PARTY_HEAL_COOLTIME_LONG = 60,
	PARTY_HEAL_COOLTIME_SHORT = 30,
	PARTY_MAX_MEMBER = 30,//rarzor93 csoport tagok szama 2024-12-30
	PARTY_DEFAULT_RANGE = 5000,
};

enum EPartyRole
{
	PARTY_ROLE_NORMAL,
	PARTY_ROLE_LEADER,
	PARTY_ROLE_ATTACKER,
	PARTY_ROLE_TANKER,
	PARTY_ROLE_BUFFER,
	PARTY_ROLE_SKILL_MASTER,
	PARTY_ROLE_HASTE,
	PARTY_ROLE_DEFENDER,
	PARTY_ROLE_MAX_NUM,
};

enum EPartyExpDistributionModes
{
	PARTY_EXP_DISTRIBUTION_NON_PARITY,
	PARTY_EXP_DISTRIBUTION_PARITY,
	PARTY_EXP_DISTRIBUTION_MAX_NUM
};


namespace ecs
{
	// One membership row. The map is keyed by the member's durable player id
	// (offline members and P2P packets identify players by pid); for mob
	// parties the packet VID is the key. A row holds an entity handle while its
	// character is linked; a handle whose entity is gone counts as not linked.
	struct PartyMember
	{
		entt::entity member { entt::null };
		bool bNear { false };
		uint8_t bRole { PARTY_ROLE_NORMAL };
		uint8_t bLevel { 0 };
		std::string strName;
	};

	// Authoritative state of one party. The durable identifier is the leader
	// player id; the member rows hold entity handles and the update event is
	// owned here so a stale callback cannot act on a recycled party entity.
	struct PartyState
	{
		std::map<uint32_t, PartyMember> members;
		uint32_t leaderPID { 0 };
		// The round-robin loot ownership position, kept as a member pid so
		// membership changes cannot dangle it the way an iterator would.
		uint32_t nextOwnerPID { 0 };
		LPEVENT updateEvent { nullptr };

		int expDistributionMode { PARTY_EXP_DISTRIBUTION_NON_PARITY };
		uint32_t startTime { 0 };
		uint32_t healTime { 0 };
		bool healReady { false };
		bool canUsePartyHeal { false };
		int roleCount[PARTY_ROLE_MAX_NUM] {};
		int maxRole[PARTY_ROLE_MAX_NUM] {};
		int longTimeExpBonus { 0 };

		// Used by Update.
		int leadership { 0 };
		int expBonus { 0 };
		int attBonus { 0 };
		int defBonus { 0 };
		int nearMemberCount { 0 };

		bool isPCParty { false };

		std::map<std::string, int> flags;
		// The dungeon instance this party joined, validated on every read.
		entt::entity dungeon { entt::null };
	};
}

// Native API over ecs::PartyState. Every entry point validates the party
// entity before use, so a retired or recycled handle is a no-op. The party
// side is always the first parameter; character parameters are the characters'
// own side.
namespace PartySystem
{
	// Defined here because the member walks below resolve the state inline.
	inline ecs::PartyState* Find(entt::entity party)
	{
		if (party == entt::null || !g_registry.valid(party))
			return nullptr;

		return g_registry.try_get<ecs::PartyState>(party);
	}

	bool IsValid(entt::entity party);

	// A row holds a handle while its character is linked; a handle whose entity
	// is gone counts as not linked.
	inline bool IsLinked(entt::entity member) { return member != entt::null && g_registry.valid(member); }

	// The character-side relation: the party the character belongs to.
	entt::entity GetCharacterParty(entt::entity character);
	void SetCharacterParty(entt::entity character, entt::entity party);

	// Lifecycle.
	void Initialize(entt::entity party);
	void Destroy(entt::entity party);

	// Membership.
	void P2PJoin(entt::entity party, uint32_t dwPID);
	void P2PQuit(entt::entity party, uint32_t dwPID);
	void Join(entt::entity party, uint32_t dwPID);
	void Quit(entt::entity party, uint32_t dwPID);
	void Link(entt::entity party, entt::entity character);
	void Unlink(entt::entity party, entt::entity character);
	void UpdateOnlineState(entt::entity party, uint32_t dwPID, const char* name);
	void UpdateOfflineState(entt::entity party, uint32_t dwPID);
	void RequestSetMemberLevel(entt::entity party, uint32_t pid, uint8_t level);
	void P2PSetMemberLevel(entt::entity party, uint32_t pid, uint8_t level);

	// Queries.
	uint32_t GetLeaderPID(entt::entity party);
	entt::entity GetLeader(entt::entity party);
	uint32_t GetMemberCount(entt::entity party);
	uint32_t GetNearMemberCount(entt::entity party);
	bool IsMember(entt::entity party, uint32_t pid);
	bool IsNearLeader(entt::entity party, uint32_t pid);
	bool IsPositionNearLeader(entt::entity party, entt::entity character);
	int GetPartyBonusExpPercent(entt::entity party);
	int GetPartyBonusAttackGrade(entt::entity party);
	int GetPartyBonusDefenseGrade(entt::entity party);
	int ComputePartyBonusExpPercent(entt::entity party);
	int ComputePartyBonusAttackGrade(entt::entity party);
	int ComputePartyBonusDefenseGrade(entt::entity party);
	int GetExpBonusPercent(entt::entity party);
	int GetExpDistributionMode(entt::entity party);
	uint8_t GetRole(entt::entity party, uint32_t pid);
	bool IsRole(entt::entity party, uint32_t pid, uint8_t bRole);
	uint8_t GetMemberMaxLevel(entt::entity party);
	uint8_t GetMemberMinLevel(entt::entity party);
	uint8_t CountMemberByVnum(entt::entity party, uint32_t dwVnum);
	bool IsPartyInDungeon(entt::entity party, int mapIndex);
	void SetFlag(entt::entity party, std::string_view name, int value);
	int GetFlag(entt::entity party, std::string_view name);
	entt::entity GetNextOwnership(entt::entity party, entt::entity fallback, int32_t x, int32_t y);

	// Mutations.
	void SetPCParty(entt::entity party, bool b);
	bool SetRole(entt::entity party, uint32_t pid, uint8_t bRole, bool on);
	void SetParameter(entt::entity party, int iMode);
	void ComputeRolePoint(entt::entity party, entt::entity character, uint8_t bRole, bool bAdd);
	void SendMessage(entt::entity party, entt::entity character, uint8_t bMsg, uint32_t dwArg1, uint32_t dwArg2);
	void HealParty(entt::entity party);
	void SummonToLeader(entt::entity party, uint32_t pid);
	void Update(entt::entity party);
	void SetDungeon(entt::entity party, entt::entity dungeon);
	entt::entity GetDungeon(entt::entity party);

	// Packet senders.
	void SendPartyJoinOneToAll(entt::entity party, uint32_t dwPID);
	void SendPartyJoinAllToOne(entt::entity party, entt::entity character);
	void SendPartyRemoveOneToAll(entt::entity party, uint32_t pid);
	void SendPartyInfoOneToAll(entt::entity party, uint32_t pid);
	void SendPartyInfoOneToAll(entt::entity party, entt::entity character);
	void SendPartyInfoAllToOne(entt::entity party, entt::entity character);
	void SendPartyLinkOneToAll(entt::entity party, entt::entity character);
	void SendPartyLinkAllToOne(entt::entity party, entt::entity character);
	void SendPartyUnlinkOneToAll(entt::entity party, entt::entity character);
	void SendParameter(entt::entity party, entt::entity character);
	void SendParameterToAll(entt::entity party);
#ifdef TEXTS_IMPROVEMENT
	void ChatPacketToAllMemberNew(entt::entity party, uint8_t type, uint32_t idx, const char* format, ...);
#endif

	// Member walks. These are templates so functors that accumulate state stay
	// the caller's own object; the entity walk validates the member handle.
	template <class Func>
	void ForEachMember(entt::entity party, Func& f)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		for (auto& row : state->members)
		{
			f(row.first);
			if (!g_registry.valid(party))
				return;
		}
	}

	template <class Func>
	void ForEachOnlineMember(entt::entity party, Func& f)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		for (auto& row : state->members)
		{
			if (!IsLinked(row.second.member))
				continue;

			f(row.second.member);
			if (!g_registry.valid(party))
				return;
		}
	}

	template <class Func>
	void ForEachNearMember(entt::entity party, Func& f)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		for (auto& row : state->members)
		{
			if (!row.second.bNear || !IsLinked(row.second.member))
				continue;

			f(row.second.member);
			if (!g_registry.valid(party))
				return;
		}
	}

	template <class Func>
	void ForEachOnMapMember(entt::entity party, Func& f, int32_t lMapIndex)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		for (auto& row : state->members)
		{
			const entt::entity member = row.second.member;
			if (!IsLinked(member) || ecs::PlayerRuntime::GetMapIndex(member) != lMapIndex)
				continue;

			f(member);
			if (!g_registry.valid(party))
				return;
		}
	}

	template <class Func>
	bool ForEachOnMapMemberBool(entt::entity party, Func& f, int32_t lMapIndex)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return true;

		for (auto& row : state->members)
		{
			const entt::entity member = row.second.member;
			if (IsLinked(member) && ecs::PlayerRuntime::GetMapIndex(member) == lMapIndex && !f(member))
				return false;

			if (!g_registry.valid(party))
				return true;
		}

		return true;
	}
}

class CPartyManager : public singleton<CPartyManager>
{
	public:
		// The index is service state: pid (or mob packet VID) to party entity.
		typedef std::map<uint32_t, entt::entity> TPartyMap;
		typedef std::set<entt::entity> TPCPartySet;

	public:
		CPartyManager();
		virtual ~CPartyManager();

		void		Initialize();

		void		EnablePCParty() { m_bEnablePCParty = true; LOG_INFO("PARTY Enable"); }
		void		DisablePCParty() { m_bEnablePCParty = false; LOG_INFO("PARTY Disable"); }
		bool		IsEnablePCParty() { return m_bEnablePCParty; }

		entt::entity	CreateParty(entt::entity leader);
		void		DeleteParty(entt::entity party);
		void		DeleteAllParty();
		bool		SetParty(entt::entity character);

		void		SetPartyMember(uint32_t dwPID, entt::entity party);

		void		P2PLogin(uint32_t pid, const char* name);
		void		P2PLogout(uint32_t pid);

		entt::entity	P2PCreateParty(uint32_t pid);
		void		P2PDeleteParty(uint32_t pid);
		void		P2PJoinParty(uint32_t leader, uint32_t pid, uint8_t role = 0);
		void		P2PQuitParty(uint32_t pid);

	private:
		TPartyMap	m_map_pkParty;		// pid (or mob packet vid) to party entity

		TPCPartySet	m_set_pkPCParty;	// every PC party the manager owns

		bool		m_bEnablePCParty;	// whether PC parties are enabled on this channel
};

enum EPartyMessages
{
	PM_ATTACK,		// Attack him
	PM_RETURN,		// Return back to position
	PM_ATTACKED_BY,	// I was attacked by someone
	PM_AGGRO_INCREASE,	// My aggro is increased
};

#endif
