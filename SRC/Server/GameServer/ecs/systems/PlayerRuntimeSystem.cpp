#include "../../stdafx.h"
#include "../components/activity_components.hpp"
#include "../AIHelpers.hpp"
#include <utility>
#include "ViewSystem.hpp"
#include "AffectSystem.hpp"
#include "ActivitySystem.hpp"
#include "ChatSystem.hpp"
#include "PointSystem.hpp"
#include "SocialSystem.hpp"

#include "PlayerRuntimeSystem.hpp"
#include "SkillSystem.hpp"
#include "../EcsDiagnostics.hpp"
#include "InventorySystem.hpp"
#include "SessionSystem.hpp"
#include "MountSystem.hpp"
#include "QuestSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "MovementSystem.hpp"
#include "../CharacterAccessors.hpp"
#include "../VIDRegistry.hpp"
#include "ItemSystem.hpp"
#include "../components/visibility_components.hpp"
#include "../services/SpatialService.hpp"

#include <algorithm>
#include <cctype>

#include "../../char.h"
#include "../../char_manager.h"
#include "../../config.h"
#include "../../constants.h"
#include "../../desc.h"
#include "../../buffer_manager.h"
#include "../../battle_pass.h"
#include "../../banword.h"
#include "../../crc32.h"
#include "../../db.h"
#include "../../desc_client.h"
#include "../../dungeon.h"
#include "../../ecs/EntityFactory.hpp"
#include "../../ecs/PositionSync.hpp"
#include "../../ecs/SpatialHelpers.hpp"
#include "../../ecs/Registry.hpp"
#include "../../ecs/components/appearance_components.hpp"
#include "../../ecs/components/ai_components.hpp"
#include "../../ecs/components/character_runtime_components.hpp"
#include "../../ecs/components/combat_components.hpp"
#include "../../ecs/components/dirty_components.hpp"
#include "../../ecs/components/identity_components.hpp"
#include "AISystem.hpp"
#include "CombatSystem.hpp"
#include "../components/ai_components.hpp"
#include "../../ecs/components/inventory_components.hpp"
#include "../../ecs/components/movement_components.hpp"
#include "../../ecs/components/quest_components.hpp"
#include "../../ecs/components/session_components.hpp"
#include "../../ecs/components/skill_components.hpp"
#include "../../ecs/components/social_components.hpp"
#include "../../ecs/components/status_components.hpp"
#include "../../ecs/components/transform_components.hpp"
#include "../../ecs/components/vital_components.hpp"
#include "../../exchange.h"
#include "../../gm.h"
#include "../../guild_manager.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../log.h"
#include "../../marriage.h"
#include "../components/spatial_components.hpp"
#include "../services/EntityNetworkDispatch.hpp"
#include "../../messenger_manager.h"
#include "../../mining.h"
#include "../../mob_manager.h"
#include "../../MountSystem.h"
#include "../../MountInventory.h"
#include "../../new_offlineshop.h"
#include "../../New_PetSystem.h"
#include "../components/pet_mount_components.hpp"
#include "../../PetSystem.h"
#include "../../party.h"
#include "../../questmanager.h"
#include "../../regen.h"
#include "../../safebox.h"
#include "../../shop.h"
#include "../../shop_manager.h"
#include "../../start_position.h"
#include "DragonSoulSystem.hpp"
#include "GayaSystem.hpp"
#include "../../skill_power.h"
#include "../../target.h"
#include "../../war_map.h"
#include "../../wedding.h"
#include "../../DragonSoul.h"

extern bool RaceToJob(unsigned race, unsigned* ret_job);
EVENTFUNC(destroy_when_idle_event);

namespace {

LPITEM ResolveLegacyItem(entt::entity item)
{
	if (!ItemSystem::IsValidItem(item))
		return nullptr;

	return ITEM_MANAGER::instance().Find(ItemSystem::GetItemID(item));
}

} // namespace

namespace ecs::PlayerRuntime {

entt::entity FindByPlayerID(uint32_t playerID)
{
	if (playerID == 0)
		return entt::null;

	auto players = g_registry.view<ecs::PlayerID>();
	for (const entt::entity player : players)
	{
		if (players.get<ecs::PlayerID>(player).pid == playerID)
			return player;
	}

	return entt::null;
}

entt::entity FindByPlayerName(std::string_view name)
{
	if (name.empty())
		return entt::null;

	const auto equalCaseInsensitive = [](std::string_view left, std::string_view right) {
		return left.size() == right.size() && std::equal(
			left.begin(), left.end(), right.begin(),
			[](unsigned char lhs, unsigned char rhs) {
				return std::tolower(lhs) == std::tolower(rhs);
			});
	};

	auto players = g_registry.view<ecs::PlayerID, ecs::PlayerName>();
	for (const entt::entity player : players)
	{
		if (equalCaseInsensitive(players.get<ecs::PlayerName>(player).value, name))
			return player;
	}

	return entt::null;
}

entt::entity FindByVID(uint32_t vid)
{
	if (vid == 0)
		return entt::null;

	const entt::entity character = CVIDRegistry::Instance().Find(vid);
	return character != entt::null && g_registry.valid(character) ? character : entt::null;
}

entt::entity FindSpecifyPC(uint32_t jobFlag, int32_t mapIndex, entt::entity except,
	int minLevel, int maxLevel)
{
	return CHARACTER_MANAGER::instance().FindSpecifyPC(
		jobFlag, mapIndex, except, minLevel, maxLevel);
}

LPDESC GetDesc(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;

	const auto* session = g_registry.try_get<ecs::NetworkSession>(e);
	return session ? session->desc : nullptr;
}

uint32_t GetPlayerID(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* playerID = g_registry.try_get<ecs::PlayerID>(e))
			return playerID->pid;
	}

	return 0;
}

uint32_t GetAccountID(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;

	const auto* account = g_registry.try_get<ecs::AccountID>(e);
	return account ? account->aid : 0;
}

uint8_t GetEmpire(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* empire = g_registry.try_get<ecs::EmpireComponent>(e))
			return empire->value;
	}

	return 0;
}

uint8_t GetGMLevel(entt::entity e)
{
	if (test_server)
		return GM_IMPLEMENTOR;

	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* gmLevel = g_registry.try_get<ecs::GMLevel>(e))
			return gmLevel->level;

		if (const auto* flags = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e))
			return flags->gmLevel;
	}

	return 0;
}

void SetEmpire(entt::entity e, uint8_t empire)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& state = g_registry.get_or_emplace<ecs::EmpireComponent>(e);
	state.value = empire;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

int GetChangeEmpireCount(entt::entity e)
{
	const uint32_t accountID = GetAccountID(e);
	if (accountID == 0)
		return 0;

	char query[256];
	snprintf(query, sizeof(query),
		"SELECT change_count FROM change_empire WHERE account_id = %u", accountID);
	std::unique_ptr<SQLMsg> message(DBManager::instance().DirectQuery(query));
	if (!message || message->Get()->uiNumRows == 0)
		return 0;

	MYSQL_ROW row = mysql_fetch_row(message->Get()->pSQLResult);
	uint32_t count = 0;
	if (row && row[0])
		str_to_number(count, row[0]);

	if (e != entt::null && g_registry.valid(e))
		g_registry.get_or_emplace<ecs::EmpireComponent>(e).changeCount = count;
	return static_cast<int>(count);
}

void IncrementChangeEmpireCount(entt::entity e)
{
	const uint32_t accountID = GetAccountID(e);
	if (accountID == 0)
		return;

	const int count = GetChangeEmpireCount(e) + 1;
	char query[256];
	if (count == 1)
		snprintf(query, sizeof(query),
			"INSERT INTO change_empire VALUES(%u, %d, NOW())", accountID, count);
	else
		snprintf(query, sizeof(query),
			"UPDATE change_empire SET change_count=%d WHERE account_id=%u", count, accountID);

	std::unique_ptr<SQLMsg> message(DBManager::instance().DirectQuery(query));
	if (e != entt::null && g_registry.valid(e))
	{
		auto& state = g_registry.get_or_emplace<ecs::EmpireComponent>(e);
		state.changeCount = static_cast<uint32_t>(count);
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	}
}

int ChangeEmpire(entt::entity e, uint8_t empire)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	if (GetEmpire(e) == empire)
		return 1;

	const uint32_t accountID = GetAccountID(e);
	if (accountID == 0)
		return 0;

	char query[1025];
	snprintf(query, sizeof(query),
		"SELECT pid1, pid2, pid3, pid4, pid5 FROM player_index%s WHERE id=%u AND empire=%u",
		get_table_postfix(), accountID, GetEmpire(e));
	std::unique_ptr<SQLMsg> message(DBManager::instance().DirectQuery(query));
	if (!message || message->Get()->uiNumRows == 0)
		return 0;

	uint32_t playerIDs[5] {};
	MYSQL_ROW row = mysql_fetch_row(message->Get()->pSQLResult);
	for (size_t index = 0; index < std::size(playerIDs); ++index)
	{
		if (row && row[index])
			str_to_number(playerIDs[index], row[index]);
	}

	for (const uint32_t playerID : playerIDs)
	{
		if (playerID == 0)
			continue;
		snprintf(query, sizeof(query),
			"SELECT guild_id FROM guild_member%s WHERE pid=%u", get_table_postfix(), playerID);
		std::unique_ptr<SQLMsg> guildMessage(DBManager::instance().DirectQuery(query));
		if (guildMessage && guildMessage->Get()->uiNumRows > 0)
		{
			MYSQL_ROW guildRow = mysql_fetch_row(guildMessage->Get()->pSQLResult);
			uint32_t guildID = 0;
			if (guildRow && guildRow[0])
				str_to_number(guildID, guildRow[0]);
			if (CGuildManager::instance().FindGuild(guildID))
				return 2;
		}
	}

	for (const uint32_t playerID : playerIDs)
	{
		if (playerID != 0 && marriage::CManager::instance().IsEngagedOrMarried(playerID))
			return 3;
	}

	snprintf(query, sizeof(query),
		"UPDATE player_index%s SET empire=%u WHERE id=%u AND empire=%u",
		get_table_postfix(), empire, accountID, GetEmpire(e));
	std::unique_ptr<SQLMsg> updateMessage(DBManager::instance().DirectQuery(query));
	if (!updateMessage || updateMessage->Get()->uiAffectedRows <= 0)
		return 0;

	SetEmpire(e, empire);
	IncrementChangeEmpireCount(e);
#ifdef ENABLE_BUG_FIXES
	NetworkSyncSystem::UpdatePacket(e);
#endif
	return 999;
}

void RefreshGMLevel(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	uint8_t level = GM_PLAYER;
	if (LPDESC desc = GetDesc(e))
	{
		const std::string_view name = GetName(e);
		level = gm_get_level(name.data(), desc->GetHostName(), desc->GetAccountTable().login);
	}

	auto& flags = g_registry.get_or_emplace<ecs::CharacterRuntimeFlagsComponent>(e);
	flags.gmLevel = level;
	g_registry.emplace_or_replace<ecs::GMLevel>(e, ecs::GMLevel { level });

	auto* status = g_registry.try_get<ecs::StatusFlags>(e);
	if (!status)
		status = &g_registry.emplace<ecs::StatusFlags>(e, ecs::StatusFlags {});
	status->isGM = level != GM_PLAYER || test_server;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

uint8_t GetBlockMode(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;

	const auto* flags = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e);
	return flags ? flags->blockMode : 0;
}

bool IsBlockMode(entt::entity e, uint8_t flag)
{
	return (GetBlockMode(e) & flag) != 0;
}

// The player's own setting, which is also kept in the quest flags so a
// relog restores it. SetBlockModeForce below is the quest script's way in
// and deliberately does not write them back.
void SetBlockMode(entt::entity e, uint8_t flag)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	g_registry.get_or_emplace<ecs::CharacterRuntimeFlagsComponent>(e).blockMode = flag;

	ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "setblockmode %d", flag);

	SetQuestFlag(e, "game_option.block_exchange", flag & BLOCK_EXCHANGE ? 1 : 0);
	SetQuestFlag(e, "game_option.block_party_invite", flag & BLOCK_PARTY_INVITE ? 1 : 0);
	SetQuestFlag(e, "game_option.block_guild_invite", flag & BLOCK_GUILD_INVITE ? 1 : 0);
	SetQuestFlag(e, "game_option.block_whisper", flag & BLOCK_WHISPER ? 1 : 0);
	SetQuestFlag(e, "game_option.block_messenger_invite", flag & BLOCK_MESSENGER_INVITE ? 1 : 0);
	SetQuestFlag(e, "game_option.block_party_request", flag & BLOCK_PARTY_REQUEST ? 1 : 0);

	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

// A party member left behind with nobody around is retired after five
// minutes. The timer was the last LPEVENT CHARACTER kept for itself.

void StartDestroyWhenIdleEvent(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	if (GetCharEvent(e, CharEvent::DestroyWhenIdle))
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();
	info->ch = e;

	SetCharEvent(e, CharEvent::DestroyWhenIdle,
		event_create(destroy_when_idle_event, info, PASSES_PER_SEC(300)));
}

void SetBlockModeForce(entt::entity e, uint8_t blockMode)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& flags = g_registry.get_or_emplace<ecs::CharacterRuntimeFlagsComponent>(e);
	flags.blockMode = blockMode;
	ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "setblockmode %d", blockMode);
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

uint32_t GetPacketVID(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* vid = g_registry.try_get<ecs::VIDComponent>(e))
			return vid->value;
	}

	return 0;
}

uint32_t GetRaceNum(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* race = g_registry.try_get<ecs::RaceState>(e))
			return race->polymorphRace != 0 ? race->polymorphRace : race->baseRace;
	}

	return 0;
}

std::string_view GetName(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* name = g_registry.try_get<ecs::PlayerName>(e))
			return name->value;
	}

	return {};
}

std::string_view GetPendingName(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return {};
	const auto* pending = g_registry.try_get<ecs::PendingPlayerName>(e);
	return pending ? std::string_view(pending->value) : std::string_view {};
}

void SetPendingName(entt::entity e, std::string_view name)
{
	if (e == entt::null || !g_registry.valid(e))
		return;
	auto& pending = g_registry.get_or_emplace<ecs::PendingPlayerName>(e);
	pending.value.assign(name.data(), name.size());
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

int RequestNameChange(entt::entity e, std::string_view name)
{
#ifdef ENABLE_LOCALECHECK_CHANGENAME
	return 5;
#else
	if (e == entt::null || !g_registry.valid(e) || name.empty())
		return 1;
	if (!GetPendingName(e).empty())
		return 0;

	const std::string requestedName(name);
	if (!check_name(requestedName.c_str()))
		return 2;

	char query[1024];
	snprintf(query, sizeof(query), "SELECT COUNT(*) FROM player%s WHERE name='%s'",
		get_table_postfix(), requestedName.c_str());
	std::unique_ptr<SQLMsg> checkMessage(DBManager::instance().DirectQuery(query));
	if (checkMessage && checkMessage->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row = mysql_fetch_row(checkMessage->Get()->pSQLResult);
		int count = 0;
		if (row && row[0])
			str_to_number(count, row[0]);
		if (count != 0)
			return 3;
	}

	const uint32_t playerID = GetPlayerID(e);
	if (playerID == 0)
		return 1;
	db_clientdesc->DBPacketHeader(HEADER_GD_FLUSH_CACHE, 0, sizeof(uint32_t));
	db_clientdesc->Packet(&playerID, sizeof(uint32_t));

	const std::string currentName(GetName(e));
	MessengerManager::instance().RemoveAllList(currentName.c_str());
	const LPDESC desc = GetDesc(e);
	LogManager::instance().ChangeNameLog(playerID, currentName.c_str(), requestedName.c_str(),
		desc ? desc->GetHostName() : "");

	snprintf(query, sizeof(query), "UPDATE player%s SET name='%s' WHERE id=%u",
		get_table_postfix(), requestedName.c_str(), playerID);
	std::unique_ptr<SQLMsg> updateMessage(DBManager::instance().DirectQuery(query));
	SetPendingName(e, requestedName);
	return 4;
#endif
}

int32_t GetMapIndex(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* map = g_registry.try_get<ecs::MapIndex>(e))
			return map->value;
	}

	return 0;
}

int32_t GetX(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* pos = g_registry.try_get<ecs::Position>(e))
			return pos->x;
	}

	return 0;
}

int32_t GetZ(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* pos = g_registry.try_get<ecs::Position>(e))
			return pos->z;
	}

	return 0;
}

int32_t GetY(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* pos = g_registry.try_get<ecs::Position>(e))
			return pos->y;
	}

	return 0;
}

float GetRotation(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* flags = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e))
			return flags->rotation;
	}

	return 0.0f;
}

LPSECTREE GetSectree(entt::entity e)
{
	return ecs::SpatialService::GetSectree(g_registry, e);
}

bool IsValid(entt::entity e)
{
	return e != entt::null && g_registry.valid(e);
}

bool IsPC(entt::entity e)
{
	return e != entt::null && g_registry.valid(e) && g_registry.all_of<ecs::TagPC>(e);
}

bool IsNPC(entt::entity e)
{
	return e != entt::null && g_registry.valid(e) && g_registry.all_of<ecs::TagNPC>(e);
}

bool IsGuardNPC(entt::entity e)
{
    // CHARACTER::IsNPC() is m_bCharType != CHAR_TYPE_PC - "not a PC", which
    // takes in monsters and stones. That is not IsNPC(e) above, which reads
    // the TagNPC component and means CHAR_TYPE_NPC alone, so this goes to the
    // CharacterType component the factory fills from m_bCharType directly.
    if (e == entt::null || !g_registry.valid(e))
        return false;

    const auto* type = g_registry.try_get<ecs::CharacterType>(e);
    if (!type || type->value == CHAR_TYPE_PC)
        return false;

    const uint32_t race = GetRaceNum(e);
    return race == 11000 || race == 11002 || race == 11004;
}

namespace {
uint8_t CharTypeOf(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return CHAR_TYPE_PC;
    const auto* type = g_registry.try_get<ecs::CharacterType>(e);
    return type ? type->value : CHAR_TYPE_PC;
}
} // namespace

bool IsBuilding(entt::entity e)
{
    return CharTypeOf(e) == CHAR_TYPE_BUILDING;
}

bool IsMount(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;
    const auto* flags = g_registry.try_get<ecs::StatusFlags>(e);
    return flags && flags->isMount;
}

int64_t GetSP(entt::entity e)
{
    if (const auto* mana = g_registry.try_get<ecs::Mana>(e))
        return mana->current;
    return 0;
}

int GetQuestFlag(entt::entity e, const std::string& flag)
{
    int ret = 0;
    quest::CQuestManager& q = quest::CQuestManager::instance();
    quest::PC* pPC = q.GetPC(GetPlayerID(e));
    if (pPC)
        ret = pPC->GetFlag(flag);

    return ret;
}

void SetQuestFlag(entt::entity e, const std::string& flag, int value)
{
    quest::CQuestManager& q = quest::CQuestManager::instance();
    quest::PC* pPC = q.GetPC(GetPlayerID(e));
    pPC->SetFlag(flag, value);
}

// Whether a warp is already scheduled for this character.
// Seconds left in the calendar month. Nothing here is per-character;
// it was a CHARACTER method only because its callers were.
// Whether this character carries the boost affect for a given battle pass.
int GetPetEnchant(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;
    const auto* pet = g_registry.try_get<ecs::PetEnchant>(e);
    return pet ? pet->value : 0;
}

void SetPetEnchant(entt::entity e, int value)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::PetEnchant>(e).value = value;
}

void SetUseSeedOrMoonBottleTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::SeedBottleTime>(e).pulse = thecore_pulse();
}

// The mission list, created on first ask so every caller sees one list.
std::list<TPlayerBattlePassMission*>& GetBattlePassMissions(entt::entity e)
{
    return g_registry.get_or_emplace<ecs::BattlePassMissions>(e).missions;
}

// A boost halves what a mission needs, rounding up.
uint32_t GetBattlePassAdjustedTotal(entt::entity e, uint32_t dwMissionID, uint32_t dwBattlePassID, uint32_t dwBaseTotal)
{
    if (dwBaseTotal <= 1)
        return dwBaseTotal;

    if (!ecs::PlayerRuntime::HasBattlePassBoost(e, (uint8_t)dwBattlePassID))
        return dwBaseTotal;

    return (dwBaseTotal + 1) / 2;
}

// After a boost is gained, every unfinished mission of that pass is
// re-checked against its new, lower target.
void ApplyBattlePassBoostRecalc(entt::entity e, uint8_t bBattlePassId)
{
    auto it = GetBattlePassMissions(e).begin();
    while (it != GetBattlePassMissions(e).end())
    {
        TPlayerBattlePassMission* m = *it++;
        if (!m || m->dwBattlePassId != bBattlePassId)
            continue;

        if (m->bCompleted)
            continue;

        uint32_t dwInfo1 = 0, dwBaseNeed = 0;
        if (!CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, (uint8_t)m->dwMissionId, &dwInfo1, &dwBaseNeed))
            continue;

        const uint32_t dwNeed = GetBattlePassAdjustedTotal(e, m->dwMissionId, bBattlePassId, dwBaseNeed);

        if (m->dwExtraInfo >= dwNeed)
            ecs::PlayerRuntime::UpdateMissionProgress(e, m->dwMissionId, bBattlePassId, dwNeed, dwNeed, true);
    }
}

namespace {
ecs::AntiFloodState& AntiFlood(entt::entity e)
{
    return g_registry.get_or_emplace<ecs::AntiFloodState>(e);
}
}

int GetCmdAntiFloodPulse(entt::entity e) { return AntiFlood(e).cmdPulse; }
void SetCmdAntiFloodPulse(entt::entity e, int pulse) { AntiFlood(e).cmdPulse = pulse; }
uint32_t IncreaseCmdAntiFloodCount(entt::entity e) { return ++AntiFlood(e).cmdCount; }
void SetCmdAntiFloodCount(entt::entity e, uint32_t count) { AntiFlood(e).cmdCount = count; }

void SetItemUseAntiFloodPulse(entt::entity e, int pulse) { AntiFlood(e).itemUsePulse = pulse; }
uint32_t IncreaseItemUseAntiFloodCount(entt::entity e) { return ++AntiFlood(e).itemUseCount; }
void SetItemUseAntiFloodCount(entt::entity e, uint32_t count) { AntiFlood(e).itemUseCount = count; }

uint32_t GetBoxUseTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;
    const auto* box = g_registry.try_get<ecs::BoxUseTime>(e);
    return box ? box->value : 0;
}

void SetBoxUseTime(entt::entity e, uint32_t when)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::BoxUseTime>(e).value = when;
}

bool HasBattlePassBoost(entt::entity e, uint8_t bBattlePassId)
{
    CAffect* p = AffectSystem::FindAffect(e, AFFECT_BATTLE_PASS_BOOST, POINT_BATTLE_PASS_ID);
    return p && p->lApplyValue == bBattlePassId;
}

int GetSecondsTillNextMonth()
{
    time_t iTime;
    time(&iTime);
    struct tm endTime = *localtime(&iTime);

    int iCurrentMonth = endTime.tm_mon;

    endTime.tm_hour = 0;
    endTime.tm_min = 0;
    endTime.tm_sec = 0;
    endTime.tm_mday = 1;

    if (iCurrentMonth == 12)
    {
        endTime.tm_mon = 0;
        endTime.tm_year = endTime.tm_year + 1;
    }
    else
    {
        endTime.tm_mon = iCurrentMonth + 1;
    }

    int seconds = difftime(mktime(&endTime), iTime);

    return seconds;
}

bool IsWarping(entt::entity e)
{
    return GetCharEvent(e, CharEvent::Warp) != nullptr;
}

bool IsImmortal(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;
    const auto* state = g_registry.try_get<ecs::NewPetSkillState>(e);
    return state && state->immortalSource != entt::null && g_registry.valid(state->immortalSource);
}

uint32_t GetMonsterDrainSPPoint(entt::entity e)
{
    const TMobTable* table = GetMobTable(e);
    return table ? table->dwDrainSP : 0;
}

bool IsWarp(entt::entity e)
{
    return CharTypeOf(e) == CHAR_TYPE_WARP;
}

bool IsGoto(entt::entity e)
{
    return CharTypeOf(e) == CHAR_TYPE_GOTO;
}

bool IsStone(entt::entity e)
{
	return e != entt::null && g_registry.valid(e) && g_registry.all_of<ecs::TagStone>(e);
}

bool IsMonster(entt::entity e)
{
	return e != entt::null && g_registry.valid(e) && g_registry.all_of<ecs::TagMonster>(e);
}

uint8_t GetMobRank(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return MOB_RANK_KNIGHT;

	const auto* mob = g_registry.try_get<ecs::MobDataRef>(e);
	return mob && mob->data ? mob->data->m_table.bRank : MOB_RANK_KNIGHT;
}

int GetPremiumRemainSeconds(entt::entity e, uint8_t premiumType)
{
	if (e == entt::null || !g_registry.valid(e) || premiumType >= PREMIUM_MAX_NUM)
		return 0;

	const auto* login = g_registry.try_get<ecs::LoginInfo>(e);
	return login ? login->premiumTimes[premiumType] - get_global_time() : 0;
}

bool IsPCBang(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return false;

	const auto* login = g_registry.try_get<ecs::LoginInfo>(e);
	return login && login->isPCBang;
}

bool IsObserverMode(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return false;

	if (g_registry.all_of<ecs::ObserverModeTag>(e))
		return true;

	const auto* status = g_registry.try_get<ecs::StatusFlags>(e);
	return status && status->isObserverMode;
}

bool IsArenaObserverMode(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return false;

	const auto* status = g_registry.try_get<ecs::StatusFlags>(e);
	return status && status->isArenaObserver;
}

bool IsBattlePassLoaded(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;
    const auto* missions = g_registry.try_get<ecs::BattlePassMissions>(e);
    return missions && missions->loaded;
}

void SetBattlePassLoaded(entt::entity e, bool loaded)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::BattlePassMissions>(e).loaded = loaded;
}

const ecs::MobileAuth& GetMobileAuth(entt::entity e)
{
    static const ecs::MobileAuth none {};
    if (e == entt::null || !g_registry.valid(e))
        return none;
    const auto* mobile = g_registry.try_get<ecs::MobileAuth>(e);
    return mobile ? *mobile : none;
}

void SetMobilePhone(entt::entity e, const char* phone)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::MobileAuth>(e).phone = phone ? phone : "";
}

void SetArenaObserverMode(entt::entity e, bool flag)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    g_registry.get_or_emplace<ecs::StatusFlags>(e).isArenaObserver = flag;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

bool GetArenaObserverMode(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;

    const auto* status = g_registry.try_get<ecs::StatusFlags>(e);
    return status && status->isArenaObserver;
}

// The quest reward waiting to be handed out, and the command that goes with
// it. Two CHARACTER fields beside ecs::ItemAward, which the setters also wrote.
void SetItemAwardVnum(entt::entity e, uint32_t vnum)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    g_registry.get_or_emplace<ecs::ItemAward>(e).vnum = vnum;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

uint32_t GetItemAwardVnum(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;

    const auto* award = g_registry.try_get<ecs::ItemAward>(e);
    return award ? award->vnum : 0;
}

void SetItemAwardCommand(entt::entity e, const char* command)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    g_registry.get_or_emplace<ecs::ItemAward>(e).command = command ? command : "";
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

const char* GetItemAwardCommand(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return "";

    const auto* award = g_registry.try_get<ecs::ItemAward>(e);
    return award ? award->command.c_str() : "";
}

CArena* GetArena(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;

	const auto* membership = g_registry.try_get<ecs::ArenaMembership>(e);
	return membership ? membership->arena : nullptr;
}

namespace {

LPEVENT* CharEventSlot(entt::entity e, ecs::PlayerRuntime::CharEvent slot)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    auto& events = g_registry.get_or_emplace<ecs::LegacyCharEvents>(e);
    switch (slot) {
    case ecs::PlayerRuntime::CharEvent::Dead:     return &events.dead;
    case ecs::PlayerRuntime::CharEvent::Stun:     return &events.stun;
    case ecs::PlayerRuntime::CharEvent::Recovery: return &events.recovery;
    case ecs::PlayerRuntime::CharEvent::Fishing:  return &events.fishing;
    case ecs::PlayerRuntime::CharEvent::Timed:    return &events.timed;
    case ecs::PlayerRuntime::CharEvent::Warp:     return &events.warp;
    case ecs::PlayerRuntime::CharEvent::WarpNPC:  return &events.warpNPC;
    case ecs::PlayerRuntime::CharEvent::BattlePassStayOnline: return &events.battlePassStayOnline;
    case ecs::PlayerRuntime::CharEvent::Drop: return &events.drop;
    case ecs::PlayerRuntime::CharEvent::Mining: return &events.mining;
    case ecs::PlayerRuntime::CharEvent::DestroyWhenIdle: return &events.destroyWhenIdle;
    }
    return nullptr;
}

} // namespace

LPEVENT GetCharEvent(entt::entity e, CharEvent slot)
{
    LPEVENT* p = CharEventSlot(e, slot);
    return p ? *p : nullptr;
}

void SetCharEvent(entt::entity e, CharEvent slot, LPEVENT ev)
{
    if (LPEVENT* p = CharEventSlot(e, slot))
    {
        if (slot == CharEvent::Timed && *p != ev)
            event_cancel(p);
        *p = ev;
    }
}

void CancelCharEvent(entt::entity e, CharEvent slot)
{
    // event_cancel takes the address of the slot and nulls it, which is why
    // this hands out the address rather than a copy.
    if (LPEVENT* p = CharEventSlot(e, slot))
        event_cancel(p);
}

void MonsterLog(entt::entity e, const char* text)
{
    if (!test_server)
        return;

    // CHARACTER::MonsterLog skipped PCs via IsPC(), which is GetDesc() != nullptr
    // - a client is attached - not the TagPC component. Keep that test.
    if (e == entt::null || !g_registry.valid(e) || GetDesc(e))
        return;

    char chatbuf[CHAT_MAX_LEN + 1];
    int len = snprintf(chatbuf, sizeof(chatbuf), "%lu)%s",
        static_cast<unsigned long>(GetPacketVID(e)), text ? text : "");
    if (len < 0 || len >= static_cast<int>(sizeof(chatbuf)))
        len = sizeof(chatbuf) - 1;
    ++len;

    TPacketGCChat pack_chat;
    pack_chat.header = HEADER_GC_CHAT;
    pack_chat.size = sizeof(TPacketGCChat) + len;
    pack_chat.type = CHAT_TYPE_TALKING;
    pack_chat.id = GetPacketVID(e);
    pack_chat.bEmpire = 0;

    TEMP_BUFFER buf;
    buf.write(&pack_chat, sizeof(TPacketGCChat));
    buf.write(chatbuf, len);

    CHARACTER_MANAGER::instance().PacketMonsterLog(e, buf.read_peek(), buf.size());
}

void SetPotionLimit(entt::entity e, int count)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    g_registry.get_or_emplace<ecs::ArenaMembership>(e).potionLimit = count;
}

int GetPotionLimit(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;

    const auto* membership = g_registry.try_get<ecs::ArenaMembership>(e);
    return membership ? membership->potionLimit : 0;
}

void SetArena(entt::entity e, CArena* arena)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& membership = g_registry.get_or_emplace<ecs::ArenaMembership>(e);
	membership.arena = arena;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

bool CanWarp(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return false;

	const int iPulse = thecore_pulse();
	const int limitTime = PASSES_PER_SEC(g_nPortalLimitTime);

	if (const auto* warp = g_registry.try_get<ecs::WarpBlockState>(e))
	{
		if ((iPulse - warp->safeboxLoadTime) < limitTime)
			return false;
		if ((iPulse - warp->exchangeTime) < limitTime)
			return false;
		if ((iPulse - warp->myShopTime) < limitTime)
			return false;
		if ((iPulse - warp->refineTime) < limitTime)
			return false;
	}

	if (ecs::SocialSystem::HasExchange(e))
		return false;

	const auto* shop = g_registry.try_get<ecs::ShopState>(e);
	if (shop && (shop->currentShop || shop->myShop || shop->shopOwner != entt::null || shop->underRefine))
		return false;

	if (const auto* safebox = g_registry.try_get<ecs::SafeboxRef>(e); safebox && safebox->isOpening)
		return false;

	// CHARACTER::CanWarp refused while the safebox window was open, this one
	// only while its contents were still in flight. The window guard is the
	// one that stops an item being duplicated across a map change.
	if (ecs::SessionSystem::IsSafeboxOpen(e))
		return false;

	if (ecs::SessionSystem::IsCubeOpen(e))
		return false;

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (AttrTransfer_is_open(e))
		return false;
#endif

#ifdef ENABLE_ACCE_SYSTEM
	if (const auto* acce = g_registry.try_get<ecs::AcceWindowComponent>(e);
		acce && (acce->combinationOpen || acce->absorptionOpen))
		return false;
#endif

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
	if (shop && shop->wheelDestiny)
		return false;
#endif

#ifdef __ENABLE_NEW_OFFLINESHOP__
	if (shop && (shop->offlineShopGuest || shop->auctionGuest))
		return false;

	if (shop && (iPulse - shop->offlineShopUseTime) < limitTime)
		return false;
#endif

	return true;
}

uint8_t GetSex(entt::entity e)
{
	uint32_t race = GetRaceNum(e);
	if (e != entt::null && g_registry.valid(e))
	{
		if (const auto* raceComponent = g_registry.try_get<ecs::RaceComponent>(e))
			race = raceComponent->value;
	}
    switch (race)
    {
    case MAIN_RACE_ASSASSIN_W:
    case MAIN_RACE_SHAMAN_W:
    case MAIN_RACE_WARRIOR_W:
    case MAIN_RACE_SURA_W:
        return SEX_FEMALE;
    default:
        return SEX_MALE;
    }
}

uint8_t GetJob(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return JOB_WARRIOR;
	uint32_t race = 0;
	if (const auto* raceComponent = g_registry.try_get<ecs::RaceComponent>(e))
		race = raceComponent->value;
	else if (const auto* state = g_registry.try_get<ecs::RaceState>(e))
		race = state->baseRace;
	unsigned job = JOB_WARRIOR;
	return RaceToJob(race, &job) ? static_cast<uint8_t>(job) : JOB_WARRIOR;
}

// The vnum a mob drops beside its loot, and the item a polymorph turns into.
// Both used to dereference m_pkMobData; GetMobTable answers null for anything
// that has none.
uint32_t GetMobDropItemVnum(entt::entity e)
{
	const TMobTable* table = GetMobTable(e);
	if (!table)
	{
		LOG_ERROR("GetMobDropItemVnum: no mob table (vid={} race={} name={} map={} x={} y={})",
			GetPacketVID(e), GetRaceNum(e), GetName(e), GetMapIndex(e), GetX(e), GetY(e));
		return 0;
	}

	return table->dwDropItemVnum;
}

uint32_t GetPolymorphItemVnum(entt::entity e)
{
	const TMobTable* table = GetMobTable(e);
	return table ? table->dwPolymorphItemVnum : 0;
}

// When this character last shouted, which the fifteen second limit reads.
uint32_t GetLastShoutPulse(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;

	const auto* flags = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e);
	return flags ? flags->lastShoutPulse : 0;
}

void SetLastShoutPulse(entt::entity e, uint32_t pulse)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	g_registry.get_or_emplace<ecs::CharacterRuntimeFlagsComponent>(e).lastShoutPulse = pulse;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

bool SetRace(entt::entity e, uint8_t race)
{
	if (e == entt::null || !g_registry.valid(e))
		return false;

	if (race >= MAIN_RACE_MAX_NUM)
	{
		// CHARACTER::SetRace logged this and the entity form only said false.
		LOG_ERROR("SetRace(name={}, race={}).OUT_OF_RACE_RANGE", GetName(e), static_cast<int>(race));
		return false;
	}

	g_registry.emplace_or_replace<ecs::RaceComponent>(e,
		ecs::RaceComponent { static_cast<uint16_t>(race) });
	auto& raceState = g_registry.get_or_emplace<ecs::RaceState>(e);
	raceState.baseRace = race;
	if (auto* points = g_registry.try_get<ecs::CharacterPoints>(e))
		points->base.job = race;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	return true;
}

bool SetCostumeHidden(entt::entity e, uint8_t part, bool hidden, bool skipPersistence)
{
	if (e == entt::null || !g_registry.valid(e) || part < 1 || part > 4)
		return false;
	auto& flags = g_registry.get_or_emplace<ecs::HideCostumeFlags>(e);
	const char* command = nullptr;
	const char* questFlag = nullptr;
	switch (part)
	{
	case 1:
		flags.body = hidden;
		command = "SetBodyCostumeHidden %d";
		questFlag = "costume_option.hide_body";
		break;
	case 2:
		flags.hair = hidden;
		command = "SetHairCostumeHidden %d";
		questFlag = "costume_option.hide_hair";
		break;
	case 3:
		flags.accessory = hidden;
		command = "SetAcceCostumeHidden %d";
		questFlag = "costume_option.hide_acce";
		break;
	case 4:
		flags.weapon = hidden;
		command = "SetWeaponCostumeHidden %d";
		questFlag = "costume_option.hide_weapon";
		break;
	default:
		return false;
	}
	ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, command, hidden ? 1 : 0);
	if (!g_registry.valid(e))
		return false;
	if (!skipPersistence)
		ecs::QuestSystem::SetFlag(e, questFlag, hidden ? 1 : 0);
	if (!g_registry.valid(e))
		return false;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	return true;
}

bool IsCostumeHidden(entt::entity e, uint8_t part)
{
	if (e == entt::null || !g_registry.valid(e))
		return false;
	const auto* flags = g_registry.try_get<ecs::HideCostumeFlags>(e);
	if (!flags)
		return false;
	switch (part)
	{
	case 1: return flags->body;
	case 2: return flags->hair;
	case 3: return flags->accessory;
	case 4: return flags->weapon;
	default: return false;
	}
}

bool IsHack(entt::entity e, bool sendMessage, bool checkShopOwner, int limitTime)
{
	if (e == entt::null || !g_registry.valid(e))
		return true;

	if (test_server)
		sendMessage = true;

	const auto blockedByTime = [&](int lastPulse) {
		if (thecore_pulse() - lastPulse >= PASSES_PER_SEC(limitTime))
			return false;
#ifdef TEXTS_IMPROVEMENT
		if (sendMessage)
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 234, "%d", limitTime);
#endif
		return true;
	};

	if (const auto* warp = g_registry.try_get<ecs::WarpBlockState>(e))
	{
		if (blockedByTime(warp->safeboxLoadTime) || blockedByTime(warp->exchangeTime) ||
			blockedByTime(warp->myShopTime) || blockedByTime(warp->refineTime))
			return true;
	}

	const auto* shop = g_registry.try_get<ecs::ShopState>(e);
	const auto* safebox = g_registry.try_get<ecs::SafeboxRef>(e);

	const bool activeWindow = ecs::SocialSystem::HasExchange(e) ||
		(shop && (shop->myShop || (checkShopOwner && shop->shopOwner != entt::null))) ||
		(safebox && safebox->isOpening) || ecs::SessionSystem::IsCubeOpen(e)
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
		|| (shop && shop->wheelDestiny)
#endif
		;

	if (!activeWindow)
		return false;

#ifdef TEXTS_IMPROVEMENT
	if (sendMessage)
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 236, "");
#endif
	return true;
}

bool IsHack(entt::entity e, bool sendMessage, bool checkShopOwner)
{
	return IsHack(e, sendMessage, checkShopOwner, g_nPortalLimitTime);
}

bool ChangeSex(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return false;

	const auto* race = g_registry.try_get<ecs::RaceComponent>(e);
	if (!race)
		return false;

	uint8_t targetRace = static_cast<uint8_t>(race->value);
	switch (race->value)
	{
	case MAIN_RACE_WARRIOR_M: targetRace = MAIN_RACE_WARRIOR_W; break;
	case MAIN_RACE_WARRIOR_W: targetRace = MAIN_RACE_WARRIOR_M; break;
	case MAIN_RACE_ASSASSIN_M: targetRace = MAIN_RACE_ASSASSIN_W; break;
	case MAIN_RACE_ASSASSIN_W: targetRace = MAIN_RACE_ASSASSIN_M; break;
	case MAIN_RACE_SURA_M: targetRace = MAIN_RACE_SURA_W; break;
	case MAIN_RACE_SURA_W: targetRace = MAIN_RACE_SURA_M; break;
	case MAIN_RACE_SHAMAN_M: targetRace = MAIN_RACE_SHAMAN_W; break;
	case MAIN_RACE_SHAMAN_W: targetRace = MAIN_RACE_SHAMAN_M; break;
	default: return false;
	}

	return SetRace(e, targetRace);
}

namespace {
const char* DuelFlag(const char* option)
{
#ifdef ENABLE_PVP_ADVANCED
    static constexpr const char* names[] = {
        "BlockChangeItem", "BlockBuff", "BlockPotion", "BlockRide", "BlockPet",
        "BlockPoly", "BlockParty", "BlockExchange", "BetMoney", "IsFight" };
    static constexpr const char* flags[] = {
        BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET,
        BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };
    if (option)
        for (size_t i = 0; i < std::size(names); ++i)
            if (strcmp(option, names[i]) == 0)
                return flags[i];
#endif
    return nullptr;
}
}

int GetDuelOption(entt::entity e, const char* option)
{
    if (!g_registry.valid(e))
        return 0;
    const char* flag = DuelFlag(option);
    // Preserve the legacy boolean contract, including BetMoney.
    return flag && ecs::QuestSystem::GetFlag(e, flag) > 0;
}

void SetDuelOption(entt::entity e, const char* option, int value)
{
    if (!g_registry.valid(e))
        return;
    if (const char* flag = DuelFlag(option))
        ecs::QuestSystem::SetFlag(e, flag, value);
}

int GetPosition(entt::entity e)
{
    const auto* runtime = ecs::TryGetRuntimeFlags(e);
    return runtime ? runtime->position : POS_STANDING;
}

entt::entity GetQuestNPC(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return entt::null;

    const auto* context = g_registry.try_get<ecs::QuestContext>(e);
    if (!context || context->npcVID == 0)
        return entt::null;

    const entt::entity npc = CVIDRegistry::Instance().Find(context->npcVID);
    return npc != entt::null && g_registry.valid(npc) ? npc : entt::null;
}

uint32_t GetQuestNPCID(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;

    const auto* context = g_registry.try_get<ecs::QuestContext>(e);
    return context ? context->npcVID : 0;
}

bool SetQuestNPCID(entt::entity e, uint32_t id)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;

    auto& context = g_registry.get_or_emplace<ecs::QuestContext>(e);
    context.npcVID = id;

    return true;
}

uint32_t GetQuestBy(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;
    const auto* context = g_registry.try_get<ecs::QuestContext>(e);
    return context ? context->byVnum : 0;
}

bool SetQuestBy(entt::entity e, uint32_t questVnum)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;
    auto& context = g_registry.get_or_emplace<ecs::QuestContext>(e);
    context.byVnum = questVnum;
    return true;
}

void DestroyCharacter(entt::entity e)
{
    M2_DESTROY_CHARACTER(e);
}

#ifdef __PET_SYSTEM__
CPetSystem* GetPetSystem(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;
	const auto* refs = g_registry.try_get<ecs::PetRuntimeRefs>(e);
	return refs ? refs->petSystem : nullptr;
}
#endif

#ifdef __NEWPET_SYSTEM__
CNewPetSystem* GetNewPetSystem(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;
	const auto* refs = g_registry.try_get<ecs::PetRuntimeRefs>(e);
	return refs ? refs->newPetSystem : nullptr;
}

void SetEggVID(entt::entity e, int vid)
{
	if (e == entt::null || !g_registry.valid(e))
		return;
	auto& refs = g_registry.get_or_emplace<ecs::PetRuntimeRefs>(e);
	refs.eggVID = vid;
}

int GetEggVID(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	const auto* refs = g_registry.try_get<ecs::PetRuntimeRefs>(e);
	return refs ? refs->eggVID : 0;
}
#endif

#ifdef __DUNGEON_INFO_SYSTEM__
uint64_t GetQuestDamage(entt::entity e, int race)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;

	const auto* damage = g_registry.try_get<ecs::DungeonDamage>(e);
	if (!damage)
		return 0;

	const auto it = damage->highestByRace.find(race);
	return it == damage->highestByRace.end() ? 0 : static_cast<uint64_t>(it->second);
}
#endif

#ifdef ENABLE_BATTLE_PASS
uint8_t GetBattlePassID(entt::entity e)
{
	LPCHARACTER character = LegacyCharOf(e);
	return character ? ecs::PlayerRuntime::GetBattlePassId(e) : 0;
}

#endif

// The item a quest is currently working on. All four CHARACTER accessors
// were wrappers over QuestContext::questItem, one of them converting the
// entity back into a pointer on the way out.
entt::entity GetQuestItem(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return entt::null;

	const auto* context = g_registry.try_get<ecs::QuestContext>(e);
	return context && ItemSystem::IsValidItem(context->questItem) ? context->questItem : entt::null;
}

void SetQuestItem(entt::entity e, entt::entity item)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	g_registry.get_or_emplace<ecs::QuestContext>(e).questItem =
		ItemSystem::IsValidItem(item) ? item : entt::null;
}

#ifdef ENABLE_RANKING
int64_t GetRankPoints(entt::entity e, int category)
{
	if (e == entt::null || !g_registry.valid(e) ||
		category < 0 || category >= RANKING_MAX_CATEGORIES)
		return 0;

	const auto* rank = g_registry.try_get<ecs::RankPoints>(e);
	return rank ? rank->points[category] : 0;
}

bool SetRankPoints(entt::entity e, int category, int64_t value)
{
	if (e == entt::null || !g_registry.valid(e) ||
		category < 0 || category >= RANKING_MAX_CATEGORIES)
		return false;

	auto& rank = g_registry.get_or_emplace<ecs::RankPoints>(e);
	rank.points[category] = value;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);

	// CHARACTER::SetRankPoints saved on every change and every caller went
	// through it, so the save comes along rather than being dropped.
	ecs::SessionSystem::Save(e);
	return true;
}
#endif

#ifdef ENABLE_VOTE4BUFF
int64_t GetVoteCoin(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;

	const auto* account = g_registry.try_get<ecs::AccountID>(e);
	if (!account)
		return 0;

	std::unique_ptr<SQLMsg> message(DBManager::instance().DirectQuery(
		"SELECT coins FROM account.account WHERE id = '%u';", account->aid));
	if (!message || message->Get()->uiNumRows == 0)
		return 0;

	MYSQL_ROW row = mysql_fetch_row(message->Get()->pSQLResult);
	int64_t coins = 0;
	if (row && row[0])
		str_to_number(coins, row[0]);
	return coins;
}

bool SetVoteCoin(entt::entity e, int64_t amount)
{
	if (e == entt::null || !g_registry.valid(e))
		return false;

	const auto* account = g_registry.try_get<ecs::AccountID>(e);
	if (!account)
		return false;

	DBManager::instance().DirectQuery(
		"UPDATE account.account SET coins = '%lld' WHERE id = '%u';",
		static_cast<long long>(amount), account->aid);
	return true;
}
#endif

} // namespace ecs::PlayerRuntime
#include "../../../common/rune_length.h"
#include "../../../common/stole_length.h"
#include <Core/Logging.hpp>
#ifdef ENABLE_ANTICHEAT
#include "../../hwidmanager.h"
#endif

extern bool RaceToJob(unsigned race, unsigned* ret_job);
EVENTFUNC(drop_event);
EVENTFUNC(destroy_when_idle_event);
EVENTFUNC(kill_ore_load_event);

namespace
{
static ecs::AppearancePartsComponent* EnsureAppearancePartsComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::AppearancePartsComponent>(e);
}

static const ecs::AppearancePartsComponent* TryGetAppearancePartsComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::AppearancePartsComponent>(e);
}

static ecs::CharacterRuntimeFlagsComponent* EnsureRuntimeFlagsComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::CharacterRuntimeFlagsComponent>(e);
}

static const ecs::CharacterRuntimeFlagsComponent* TryGetRuntimeFlagsComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e);
}

static ecs::Health* EnsureHealthComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::Health>(e);
}

static ecs::Mana* EnsureManaComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::Mana>(e);
}

static ecs::Stamina* EnsureStaminaComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::Stamina>(e);
}

static ecs::LevelComponent* EnsureLevelComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::LevelComponent>(e);
}

static ecs::Experience* EnsureExperienceComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::Experience>(e);
}

static ecs::GoldAmount* EnsureGoldAmountComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::GoldAmount>(e);
}

static const ecs::Health* TryGetHealthComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::Health>(e);
}

static const ecs::Mana* TryGetManaComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::Mana>(e);
}

static const ecs::Stamina* TryGetStaminaComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::Stamina>(e);
}

static const ecs::LevelComponent* TryGetLevelComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::LevelComponent>(e);
}

static const ecs::Experience* TryGetExperienceComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::Experience>(e);
}

static const ecs::GoldAmount* TryGetGoldAmountComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::GoldAmount>(e);
}

inline bool HasCombatState(entt::entity e)
{
    return e != entt::null && g_registry.valid(e) &&
        g_registry.all_of<ecs::CombatActiveTag>(e);
}

inline bool HasIdleState(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return true;

    return !g_registry.all_of<ecs::CombatActiveTag>(e) &&
        !g_registry.all_of<ecs::MovementDestination>(e);
}

inline void EnterIdleState(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    g_registry.remove<ecs::CombatActiveTag>(e);
    g_registry.remove<ecs::CombatTarget>(e);
    g_registry.remove<ecs::MovementDestination>(e);
}


}


#ifdef __DUNGEON_INFO_SYSTEM__
void CHARACTER::SetQuestDamage(int race, int dmg)
{
    if (race != 693 &&
        race != 768 &&
        race != 1093 &&
        race != 2092 &&
        race != 2493 &&
        race != 2598 &&
        race != 3962 &&
        race != 4011 &&
        race != 4158 &&
        race != 6091 &&
        race != 6191 &&
        race != 6192 &&
        race != 6118 &&
        race != 6393)
        return;

    auto it = dungeonDamage.find(race);
    if (it == dungeonDamage.end())
        dungeonDamage.insert(dungeonDamage.begin(), std::pair(race, dmg));
    else if (dmg > it->second)
        it->second = dmg;

    const entt::entity character = GetEntityHandle();
    if (character != entt::null && g_registry.valid(character))
    {
        auto& damage = g_registry.get_or_emplace<ecs::DungeonDamage>(character);
        auto [ecsIt, inserted] = damage.highestByRace.try_emplace(race, dmg);
        if (!inserted && dmg > ecsIt->second)
            ecsIt->second = dmg;
    }
}

#endif

#ifdef ENABLE_ANTICHEAT
void CHARACTER::ProcessCheatCheck(int32_t time)
{
    if (ecs::PlayerRuntime::GetGMLevel(GetEntityHandle()) == GM_PLAYER)
    {
        if (m_rewardCount == 0)
            m_firstReward = time;

        m_rewardCount++;

        if (m_rewardCount >= 7)
        {
            const int32_t n = time - m_firstReward;
            if (n <= 7)
            {
                CHwidManager::Instance().SendBlockHwid("ANTICHEAT", GetName());

                LPDESC desc = GetDesc();
                if (desc)
                    desc->DelayedDisconnect(5);
            }
            else
            {
                m_rewardCount = 0;
            }
        }
    }
}

void CHARACTER::ClearCheatChecks()
{
    m_firstReward = 0;
    m_rewardCount = 0;
    m_checkRepeated = 0;
}
#endif

bool CHARACTER::ChangeSex()
{
	const entt::entity entity = GetEntityHandle();
	if (entity != entt::null && g_registry.valid(entity))
	{
		const auto* source = g_registry.try_get<ecs::RaceComponent>(entity);
		const uint16_t sourceRace = source ? source->value : 0;
		if (!ecs::PlayerRuntime::ChangeSex(entity))
		{
			LOG_ERROR("CHANGE_SEX: {} unknown race {}", GetName(), static_cast<int>(sourceRace));
			return false;
		}

		LOG_INFO("CHANGE_SEX: {} ({} -> {})", GetName(), static_cast<int>(sourceRace),
			static_cast<int>(ecs::PlayerRuntime::GetRaceNum(entity)));
		return true;
	}

    const int src_race = GetRaceNum();

    // This branch used to assign m_points.job and nothing else. That field was
    // the race for as long as it had readers; with the race living in
    // RaceState, writing it here would have changed nothing at all, so the
    // swap goes through SetRace like every other race change.
    uint8_t dst_race = 0;
    switch (src_race)
    {
    case MAIN_RACE_WARRIOR_M:  dst_race = MAIN_RACE_WARRIOR_W;  break;
    case MAIN_RACE_WARRIOR_W:  dst_race = MAIN_RACE_WARRIOR_M;  break;
    case MAIN_RACE_ASSASSIN_M: dst_race = MAIN_RACE_ASSASSIN_W; break;
    case MAIN_RACE_ASSASSIN_W: dst_race = MAIN_RACE_ASSASSIN_M; break;
    case MAIN_RACE_SURA_M:     dst_race = MAIN_RACE_SURA_W;     break;
    case MAIN_RACE_SURA_W:     dst_race = MAIN_RACE_SURA_M;     break;
    case MAIN_RACE_SHAMAN_M:   dst_race = MAIN_RACE_SHAMAN_W;   break;
    case MAIN_RACE_SHAMAN_W:   dst_race = MAIN_RACE_SHAMAN_M;   break;
    default:
        LOG_ERROR("CHANGE_SEX: {} unknown race {}", GetName(), static_cast<int>(src_race));
        return false;
    }

    ecs::PlayerRuntime::SetRace(GetEntityHandle(), dst_race);

    LOG_INFO("CHANGE_SEX: {} ({} -> {})", GetName(), static_cast<int>(src_race), static_cast<int>(dst_race));
    return true;
}

uint16_t CHARACTER::GetRaceNum() const
{
    if (m_dwPolymorphRace)
        return m_dwPolymorphRace;

    if (m_pkMobData)
        return m_pkMobData->m_table.dwVnum;

    // RaceState.baseRace is what SetRace writes and what the entity-native
    // GetRaceNum answers with; m_points.job was a second copy of it.
    if (const auto* race = g_registry.try_get<ecs::RaceState>(GetEntityHandle()))
        return static_cast<uint16_t>(race->baseRace);

    return 0;
}

uint8_t CHARACTER::GetCharType() const
{
    return m_bCharType;
}

namespace ecs::PlayerRuntime {

void SetDungeonTicketExtraMetin(entt::entity e, bool value)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    g_registry.get_or_emplace<ecs::DungeonTicketExtraMetin>(e).value = value;
}

bool IsDungeonTicketExtraMetin(entt::entity e)
{
    const auto* flag = g_registry.try_get<ecs::DungeonTicketExtraMetin>(e);
    return flag && flag->value;
}

// The proto assignment still allocates CMobInstance on the CHARACTER, so this
// resolves. What it gains is that the spawn path no longer holds a pointer for
// it - ecs::MobDataRef is the component side and EntityFactory writes that.
// The creature flags, straight off StatusFlags. The comment on that component
// spells out that these are about the creature itself, not its owner.
bool IsPet(entt::entity e)
{
    const auto* status = g_registry.try_get<ecs::StatusFlags>(e);
    return status && status->isPet;
}

bool IsNewPet(entt::entity e)
{
    const auto* status = g_registry.try_get<ecs::StatusFlags>(e);
    return status && status->isNewPet;
}

void SetProto(entt::entity e, const CMob* pkMob)
{
    if (LPCHARACTER ch = ecs::LegacyCharOf(e))
        ch->SetProto(pkMob);
}

void SetCoward(entt::entity e)
{
    if (auto* flags = ecs::TryGetRuntimeFlags(e))
        SET_BIT(flags->aiFlag, AIFLAG_COWARD);
    AIHelpers::SetCoward(e, true);
}

void SetNoAttackShinsu(entt::entity e)
{
    if (auto* flags = ecs::TryGetRuntimeFlags(e))
        SET_BIT(flags->aiFlag, AIFLAG_NOATTACKSHINSU);
    AIHelpers::SetNoAttackShinsu(e, true);
}

void SetNoAttackChunjo(entt::entity e)
{
    if (auto* flags = ecs::TryGetRuntimeFlags(e))
        SET_BIT(flags->aiFlag, AIFLAG_NOATTACKCHUNJO);
    AIHelpers::SetNoAttackChunjo(e, true);
}

void SetNoAttackJinno(entt::entity e)
{
    if (auto* flags = ecs::TryGetRuntimeFlags(e))
        SET_BIT(flags->aiFlag, AIFLAG_NOATTACKJINNO);
    AIHelpers::SetNoAttackJinno(e, true);
}

void SetAttackMob(entt::entity e)
{
    if (auto* flags = ecs::TryGetRuntimeFlags(e))
        SET_BIT(flags->aiFlag, AIFLAG_ATTACKMOB);
    AIHelpers::SetAttackMob(e, true);
}

// The aiFlag word and the AIFlags component both carry this; the setters
// write both, so either answer alone would do. Both are read, as before.
bool IsReviver(entt::entity e)
{
    if (IS_SET(GetAIFlag(e), AIFLAG_REVIVE))
        return true;

    const auto* flags = AIHelpers::TryGetFlags(e);
    return flags && flags->isReviver;
}

// How many free spins of the wheel are left. Two named reads of one quest
// flag; they were CHARACTER methods over the entity form already.
int GetWheelFreeCount(entt::entity e)
{
    return GetQuestFlag(e, "wheel.free");
}

void SetWheelFreeCount(entt::entity e, int count)
{
    SetQuestFlag(e, "wheel.free", count);
}

uint32_t GetAIFlag(entt::entity e)
{
    if (const auto* flags = TryGetRuntimeFlagsComponent(e))
        return flags->aiFlag;

    return 0;
}

// m_tvLastSyncTime had one reader and one writer and no component; it is
// ecs::LastSyncTime now.
void SetLastSyncTime(entt::entity e, const timeval& tv)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    g_registry.get_or_emplace<ecs::LastSyncTime>(e).tv = tv;
}

const timeval& GetLastSyncTime(entt::entity e)
{
    static const timeval zero { 0, 0 };
    if (e == entt::null || !g_registry.valid(e))
        return zero;

    return g_registry.get_or_emplace<ecs::LastSyncTime>(e).tv;
}

} // namespace ecs::PlayerRuntime

namespace ecs::PlayerRuntime {

// The small point writers and readers, entity-native. Each was already nothing
// but Ensure*/TryGet* on GetEntityHandle() plus one component field, so this
// only moves them where PointSystem::Change can reach them without a pointer.

void SetHP(entt::entity e, int64_t hp)
{
    if (auto* health = EnsureHealthComponent(e))
        health->current = static_cast<int32_t>(std::clamp<int64_t>(hp, 0, INT32_MAX));
}

void SetMaxHP(entt::entity e, int64_t value)
{
    if (auto* health = EnsureHealthComponent(e))
        health->max = static_cast<int32_t>(std::clamp<int64_t>(value, 0, INT32_MAX));
}

void SetSP(entt::entity e, int64_t sp)
{
    if (auto* mana = EnsureManaComponent(e))
        mana->current = static_cast<int32_t>(std::clamp<int64_t>(sp, 0, INT32_MAX));
}

void SetMaxSP(entt::entity e, int64_t value)
{
    if (auto* mana = EnsureManaComponent(e))
        mana->max = static_cast<int32_t>(std::clamp<int64_t>(value, 0, INT32_MAX));
}

void SetStamina(entt::entity e, int64_t value)
{
    if (auto* stamina = EnsureStaminaComponent(e))
        stamina->current = static_cast<int32_t>(std::clamp<int64_t>(value, 0, INT32_MAX));
}

void SetMaxStamina(entt::entity e, int64_t value)
{
    if (auto* stamina = EnsureStaminaComponent(e))
        stamina->max = static_cast<int32_t>(std::clamp<int64_t>(value, 0, INT32_MAX));
}

int GetStamina(entt::entity e)
{
    if (const auto* stamina = TryGetStaminaComponent(e))
        return stamina->current;

    return 0;
}

int64_t GetMaxStamina(entt::entity e)
{
    if (const auto* stamina = TryGetStaminaComponent(e))
        return stamina->max;

    return 0;
}

void SetExp(entt::entity e, uint32_t exp)
{
    if (auto* ecsExp = EnsureExperienceComponent(e))
        ecsExp->current = exp;
}

uint32_t GetExp(entt::entity e)
{
    if (const auto* exp = TryGetExperienceComponent(e))
        return static_cast<uint32_t>(std::clamp<int64_t>(exp->current, 0, UINT32_MAX));

    return 0;
}

uint32_t GetNextExp(entt::entity e)
{
    const int level = ecs::PointSystem::GetLevel(e);
    if (PLAYER_MAX_LEVEL_CONST < level)
        return 2500000000u;

    return exp_table[level];
}

void SetGold(entt::entity e, int64_t gold)
{
    if (auto* wallet = EnsureGoldAmountComponent(e))
        wallet->amount = gold;
}

uint32_t GetImmuneFlag(entt::entity e)
{
    if (const auto* flags = TryGetRuntimeFlagsComponent(e))
        return flags->immuneFlag;

    return 0;
}

void SetImmuneFlag(entt::entity e, uint32_t value)
{
    if (auto* flags = EnsureRuntimeFlagsComponent(e))
        flags->immuneFlag = value;

    if (e != entt::null && g_registry.valid(e))
        g_registry.get_or_emplace<ecs::ImmunityFlags>(e).flags = value;
}

void SetLevel(entt::entity e, uint8_t level)
{
    if (auto* ecsLevel = EnsureLevelComponent(e))
        ecsLevel->value = level;

    // CHARACTER::IsPC() is the descriptor test, not the TagPC component.
    if (GetDesc(e))
    {
        if (level < PK_PROTECT_LEVEL)
            CombatSystem::SetPKMode(e, PK_MODE_PROTECT);
        else if (ecs::PlayerRuntime::GetGMLevel(e) != GM_PLAYER)
            CombatSystem::SetPKMode(e, PK_MODE_PROTECT);
        else if (CombatSystem::GetPKMode(e) == PK_MODE_PROTECT)
            CombatSystem::SetPKMode(e, PK_MODE_PEACE);
    }
}

} // namespace ecs::PlayerRuntime

BOOL CHARACTER::IsGM() const
{
    if (ecs::PlayerRuntime::GetGMLevel(GetEntityHandle()) != GM_PLAYER)
        return true;

    return test_server ? true : false;
}

uint32_t CHARACTER::GetAID() const
{
    char szQuery[1024 + 1];
    uint32_t dwAID = 0;

    snprintf(szQuery, sizeof(szQuery), "SELECT id FROM player_index%s WHERE pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u OR pid5=%u AND empire=%u",
        get_table_postfix(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), ecs::PlayerRuntime::GetEmpire(GetEntityHandle()));

    std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(szQuery));
    if (msg->Get()->uiNumRows == 0)
        return 0;

    MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
    str_to_number(dwAID, row[0]);
    return dwAID;
}

// Pet/mount markers live only in StatusFlags; legacy readers use the same store.

#ifdef ENABLE_VOTE4BUFF
#endif

#ifdef ENABLE_ITEMSHOP
namespace ecs::PlayerRuntime {

uint32_t GetDragonCoin(entt::entity e)
{
    auto* desc = GetDesc(e);
    if (!desc || desc->GetAccountTable().id == 0)
        return 0;
    std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT coins FROM account.account WHERE id = '%u';", desc->GetAccountTable().id));
    if (!pMsg || !pMsg->Get() || pMsg->Get()->uiNumRows == 0 || !pMsg->Get()->pSQLResult)
        return 0;
    MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
    if (!row || !row[0])
        return 0;
    uint32_t dc = 0;
    str_to_number(dc, row[0]);
    return dc;
}

void SetDragonCoin(entt::entity e, uint32_t amount)
{
    auto* desc = GetDesc(e);
    if (!desc || desc->GetAccountTable().id == 0)
        return;
    std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("UPDATE account.account SET coins = '%u' WHERE id = '%u';", amount, desc->GetAccountTable().id));
}

void SetProtectTime(entt::entity e, std::string_view flag, int value)
{
    if (g_registry.valid(e))
        g_registry.get_or_emplace<ecs::ProtectionTimes>(e).values.insert_or_assign(std::string(flag), value);
}

int GetProtectTime(entt::entity e, std::string_view flag)
{
    if (!g_registry.valid(e))
        return 0;
    const auto* times = g_registry.try_get<ecs::ProtectionTimes>(e);
    if (!times)
        return 0;
    const auto it = times->values.find(flag);
    return it != times->values.end() ? it->second : 0;
}

} // namespace ecs::PlayerRuntime

#endif

namespace ecs::PlayerRuntime {

const TMobTable* GetMobTable(entt::entity e)
{
    // The class version dereferences m_pkMobData unguarded and is only ever
    // called behind an IsPC/IsNPC test. This one returns null instead of
    // crashing, so callers that cannot prove the test can check.
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    const auto* mob = g_registry.try_get<ecs::MobDataRef>(e);
    return (mob && mob->data) ? &mob->data->m_table : nullptr;
}

} // namespace ecs::PlayerRuntime

namespace ecs::PlayerRuntime {

bool IsRaceFlag(entt::entity e, uint32_t dwBit)
{
    const TMobTable* table = GetMobTable(e);
    return table && IS_SET(table->dwRaceFlag, dwBit);
}

int64_t GetHP(entt::entity e)
{
    if (const auto* health = TryGetHealthComponent(e))
        return health->current;

    return 0;
}

int GetHPPct(entt::entity e)
{
    const int64_t maxHP = ecs::PointSystem::GetMaxHP(e);
    if (maxHP <= 0)
        return 0;

    return static_cast<int>((ecs::PlayerRuntime::GetHP(e) * 100) / maxHP);
}

} // namespace ecs::PlayerRuntime

void CHARACTER::ResetPlayTime(uint32_t dwTimeRemain)
{
    m_dwPlayStartTime = get_dword_time() - dwTimeRemain;
}

bool CHARACTER::SetPCBang(bool flag)
{
	const entt::entity character = GetEntityHandle();
	if (character != entt::null && g_registry.valid(character))
	{
		auto& login = g_registry.get_or_emplace<ecs::LoginInfo>(character);
		login.isPCBang = flag;
		g_registry.emplace_or_replace<ecs::DirtyTag>(character);
	}
	return flag;
}

uint32_t CHARACTER::GetNextExp() const
{
    if (PLAYER_MAX_LEVEL_CONST < ecs::PointSystem::GetLevel(GetEntityHandle()))
        return 2500000000u;
    else
        return exp_table[ecs::PointSystem::GetLevel(GetEntityHandle())];
}


int CHARACTER::GetSkillPowerByLevel(int level, bool bMob) const
{
    return CTableBySkill::instance().GetSkillPowerByLevelFromType(ecs::PlayerRuntime::GetJob(GetEntityHandle()), GetSkillGroup(), MINMAX(0, level, (int)SKILL_MAX_LEVEL), bMob);
}

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
std::string CHARACTER::GetLang() {
    auto language = GetDesc()->GetLanguage();
    std::string langs[] = { "en","en","ro","it","tr","de","pl","pt","es","cz","hu" };
    if (language == 0)
        return langs[language + 1];
    else
        return langs[language];
}
#endif

#ifdef ENABLE_BATTLE_PASS

void CHARACTER::EnsureFreeBattlePassActive()
{
    const uint8_t kDefaultBattlePassId = 1;

    int remain = 0;
    if (AffectSystem::GetBattlePassDeadline(GetEntityHandle()) > 0)
        remain = AffectSystem::GetBattlePassRemainingSeconds(GetEntityHandle());

    if (remain <= 0)
    {
        remain = ecs::PlayerRuntime::GetSecondsTillNextMonth();
        AffectSystem::SetBattlePassDeadline(GetEntityHandle(), get_global_time() + remain);
    }

    if (!ecs::PlayerRuntime::GetBattlePassId(GetEntityHandle()))
        AffectSystem::AddAffect(GetEntityHandle(), AFFECT_BATTLE_PASS, POINT_BATTLE_PASS_ID, kDefaultBattlePassId, 0, remain, 0, true);
    ecs::PlayerRuntime::SetBattlePassLoaded(GetEntityHandle(), true);
}
#endif

#ifdef ENABLE_BATTLE_PASS
void CHARACTER::LoadBattlePass(uint32_t dwCount, TPlayerBattlePassMission* data)
{
    ecs::PlayerRuntime::SetBattlePassLoaded(GetEntityHandle(), false);

    for (auto it = ecs::PlayerRuntime::GetBattlePassMissions(GetEntityHandle()).begin(); it != ecs::PlayerRuntime::GetBattlePassMissions(GetEntityHandle()).end(); ++it)
        delete (*it);
    ecs::PlayerRuntime::GetBattlePassMissions(GetEntityHandle()).clear();

    const uint8_t kDefaultBattlePassId = 1;

    int remain = 0;
    if (AffectSystem::GetBattlePassDeadline(GetEntityHandle()) > 0)
        remain = AffectSystem::GetBattlePassRemainingSeconds(GetEntityHandle());

    if (remain <= 0)
    {
        remain = ecs::PlayerRuntime::GetSecondsTillNextMonth();
        AffectSystem::SetBattlePassDeadline(GetEntityHandle(), get_global_time() + remain);
    }

    if (!ecs::PlayerRuntime::GetBattlePassId(GetEntityHandle()))
        AffectSystem::AddAffect(GetEntityHandle(), AFFECT_BATTLE_PASS, POINT_BATTLE_PASS_ID, kDefaultBattlePassId, 0, remain, 0, true);

    if (dwCount == 0 || !data)
    {
        ecs::PlayerRuntime::SetBattlePassLoaded(GetEntityHandle(), true);
        return;
    }

    for (size_t i = 0; i < dwCount; ++i, ++data)
    {
        TPlayerBattlePassMission* newMission = new TPlayerBattlePassMission;
        newMission->dwPlayerId = data->dwPlayerId;
        newMission->dwMissionId = data->dwMissionId;
        newMission->dwBattlePassId = data->dwBattlePassId;
        newMission->dwExtraInfo = data->dwExtraInfo;
        newMission->bCompleted = data->bCompleted;
        newMission->bIsUpdated = data->bIsUpdated;

        ecs::PlayerRuntime::GetBattlePassMissions(GetEntityHandle()).push_back(newMission);
    }

    ecs::PlayerRuntime::SetBattlePassLoaded(GetEntityHandle(), true);
}

#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
#endif

#ifdef ENABLE_FREE_PASS_RAZOR93
#endif

namespace ecs::PlayerRuntime {
uint32_t GetMissionProgress(entt::entity e, uint32_t dwMissionID, uint32_t dwBattlePassID)
{
    auto it = ecs::PlayerRuntime::GetBattlePassMissions(e).begin();
    while (it != ecs::PlayerRuntime::GetBattlePassMissions(e).end())
    {
        TPlayerBattlePassMission* pkMission = *it++;
        if (pkMission->dwMissionId == dwMissionID && pkMission->dwBattlePassId == dwBattlePassID)
            return pkMission->dwExtraInfo;
    }

    return 0;
}

bool IsCompletedMission(entt::entity e, uint8_t bMissionType)
{
    auto it = ecs::PlayerRuntime::GetBattlePassMissions(e).begin();
    while (it != ecs::PlayerRuntime::GetBattlePassMissions(e).end())
    {
        TPlayerBattlePassMission* pkMission = *it++;
        if (pkMission->dwMissionId == bMissionType)
            return (pkMission->bCompleted ? true : false);
    }

    return false;
}

bool UpdateMissionProgress(entt::entity e, uint32_t dwMissionID, uint32_t dwBattlePassID, uint32_t dwUpdateValue, uint32_t dwTotalValue, bool isOverride)
{
	if (e == entt::null || !g_registry.valid(e))
		return false;

	// BattlePassRewardMission still takes the character; that is its own
	// migration.
	LPCHARACTER self = LegacyCharOf(e);
	if (!self)
		return false;

    if (!ecs::PlayerRuntime::IsBattlePassLoaded(e))
        return false;
#ifdef ENABLE_FREE_PASS_RAZOR93
    dwTotalValue = ecs::PlayerRuntime::GetBattlePassAdjustedTotal(e, dwMissionID, dwBattlePassID, dwTotalValue);
#endif
    bool foundMission = false;
    uint32_t dwSaveProgress = 0;

    auto it = ecs::PlayerRuntime::GetBattlePassMissions(e).begin();
    while (it != ecs::PlayerRuntime::GetBattlePassMissions(e).end())
    {
        TPlayerBattlePassMission* pkMission = *it++;

        if (pkMission->dwMissionId == dwMissionID && pkMission->dwBattlePassId == dwBattlePassID)
        {
            pkMission->bIsUpdated = 1;
#ifdef ENABLE_FREE_PASS_RAZOR93
            if (pkMission->bCompleted)
                return false;
#endif
            if (isOverride)
                pkMission->dwExtraInfo = dwUpdateValue;
            else
                pkMission->dwExtraInfo += dwUpdateValue;

            if (pkMission->dwExtraInfo >= dwTotalValue)
            {
                pkMission->dwExtraInfo = dwTotalValue;
                pkMission->bCompleted = 1;

#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
                if (pkMission->dwMissionId == STAY_ONLINE_MINUTES)
                    ecs::PlayerRuntime::CancelCharEvent(e,
                        ecs::PlayerRuntime::CharEvent::BattlePassStayOnline);
#endif
                CBattlePass::instance().BattlePassRewardMission(self, dwMissionID, dwBattlePassID);
            }

            dwSaveProgress = pkMission->dwExtraInfo;
            foundMission = true;
            break;
        }
    }

    if (!foundMission)
    {
        TPlayerBattlePassMission* newMission = new TPlayerBattlePassMission;
        newMission->dwPlayerId = GetPlayerID(e);
        newMission->dwMissionId = dwMissionID;
        newMission->dwBattlePassId = dwBattlePassID;

        if (dwUpdateValue >= dwTotalValue)
        {
            newMission->dwExtraInfo = dwTotalValue;
            newMission->bCompleted = 1;
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
            if (newMission->dwMissionId == STAY_ONLINE_MINUTES)
                ecs::PlayerRuntime::CancelCharEvent(e,
                        ecs::PlayerRuntime::CharEvent::BattlePassStayOnline);
#endif
            CBattlePass::instance().BattlePassRewardMission(self, dwMissionID, dwBattlePassID);

            dwSaveProgress = dwTotalValue;
        }
        else
        {
            newMission->dwExtraInfo = dwUpdateValue;
            newMission->bCompleted = 0;

            dwSaveProgress = dwUpdateValue;
        }

        newMission->bIsUpdated = 1;

        ecs::PlayerRuntime::GetBattlePassMissions(e).push_back(newMission);
    }

    if (!GetDesc(e))
        return false;

    TPacketGCBattlePassUpdate packet;
    packet.bHeader = HEADER_GC_BATTLE_PASS_UPDATE;
    packet.bMissionType = dwMissionID;
    packet.dwNewProgress = dwSaveProgress;
    GetDesc(e)->Packet(&packet, sizeof(TPacketGCBattlePassUpdate));
	return true;
}

uint8_t GetBattlePassId(entt::entity e)
{
    const CAffect* affect = AffectSystem::FindAffect(e, AFFECT_BATTLE_PASS, POINT_BATTLE_PASS_ID);
    return affect ? static_cast<uint8_t>(affect->lApplyValue) : 0;
}
}

#endif

#if defined(BL_OFFLINE_MESSAGE)
void CHARACTER::SendOfflineMessage(const char* To, const char* Message)
{
    if (!GetDesc())
        return;

    if (strlen(To) < 1)
        return;

    TPacketGDSendOfflineMessage p;
    strlcpy(p.szFrom, GetName(), sizeof(p.szFrom));
    strlcpy(p.szTo, To, sizeof(p.szTo));
    strlcpy(p.szMessage, Message, sizeof(p.szMessage));
    db_clientdesc->DBPacket(HEADER_GD_SEND_OFFLINE_MESSAGE, GetDesc()->GetHandle(), &p, sizeof(p));

    SetLastOfflinePMTime();
}

void CHARACTER::ReadOfflineMessages()
{
    if (!GetDesc())
        return;

    TPacketGDReadOfflineMessage p;
    strlcpy(p.szName, GetName(), sizeof(p.szName));
    db_clientdesc->DBPacket(HEADER_GD_REQUEST_OFFLINE_MESSAGES, GetDesc()->GetHandle(), &p, sizeof(p));
}
#endif

#ifdef ENABLE_RUNE_SYSTEM
namespace ecs::PlayerRuntime {

uint16_t GetRuneEffect(entt::entity e)
{

    if (!(GetDesc(e) != nullptr))
        return 0;

    if (ecs::QuestSystem::GetFlag(e, "rune.hide_effect") == 1)
        return 0;

    uint16_t r = 1;
    int iMaxSubTypes = RUNE_SUBTYPES - 1;
    int32_t lMaxTime = 0;
    int32_t lOnePercent = 0;
    int32_t lRemainPercent = 0;

    for (int i = 0; i < iMaxSubTypes; i++) {
        const entt::entity item = ItemSystem::GetWearItem(e, WEAR_RUNE1 + i);
        if (!ItemSystem::IsValidItem(item)) {
            r = 0;
            break;
        }
        else {
            if (ItemSystem::GetItemSocket(item, 1) != 1) {
                r = 0;
                break;
            }
            else {
                lMaxTime = ItemSystem::GetItemValue(item, 0);
                lOnePercent = lMaxTime / 100;
                if (lOnePercent <= 0) {
                    r = 0;
                    break;
                }
                lRemainPercent = ItemSystem::GetItemSocket(item, ITEM_SOCKET_REMAIN_SEC) / lOnePercent;
                if (lRemainPercent < RUNE_EFFECT_FROM) {
                    r = 0;
                    break;
                }
            }
        }
    }

    return r;
}

} // namespace ecs::PlayerRuntime

#endif

bool CHARACTER::CanTakeInventoryItem(entt::entity item, TItemPos* cell)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
    ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "char.cpp::bool CHARACTER::CanTakeInventoryItem");
#endif
    if (!cell || !ItemSystem::IsValidItem(item))
        return false;

    const int iEmpty = ItemSystem::GetEmptyInventoryPositionEcs(GetEntityHandle(), item);
    if (iEmpty == -1)
        return false;

    if (ItemSystem::IsDragonSoulItem(item))
    {
        cell->window_type = DRAGON_SOUL_INVENTORY;
    }

#ifdef ENABLE_EXTRA_INVENTORY
    else if (ItemSystem::IsExtraItem(item))
    {
        cell->window_type = EXTRA_INVENTORY;
    }
#endif
    else
    {
        cell->window_type = INVENTORY;
    }

    cell->cell = static_cast<uint16_t>(iEmpty);
    return true;
}

#ifdef ENABLE_SOUL_SYSTEM
int CHARACTER::GetSoulItemDamage(entt::entity victim, int iDamage, uint8_t bSoulType)
{
    LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
    if (!pkVictim)
        return 0;

    if (!IsPC() || AffectSystem::IsPolymorphed(GetEntityHandle()) || pkVictim->IsPC())
        return 0;

    if (bSoulType >= SOUL_MAX_NUM)
        return 0;

    const CAffect* pAffect = AffectSystem::FindAffect(GetEntityHandle(), AFFECT_SOUL_RED + bSoulType);
    int iDamageAdd = 0;
    if (pAffect)
    {
        const entt::entity soulItem =
            ItemSystem::FindItemByID(GetEntityHandle(), pAffect->lSPCost);
        if (ItemSystem::IsValidItem(soulItem))
        {
            int iCurrentMinutes = ItemSystem::GetItemSocket(soulItem, 2) / 10000;
            int iCurrentStrike = ItemSystem::GetItemSocket(soulItem, 2) % 10000;

            int valueIndex = MINMAX(3, 2 + (iCurrentMinutes / 60), 5);
            float fDamageIncrease = float(ItemSystem::GetItemValue(soulItem, valueIndex) / 10.0f);

            iDamageAdd = (fDamageIncrease * iDamage) - iDamage;
            int iNextStrikes = iCurrentStrike - 1;
            if (iNextStrikes <= 0)
            {
                iCurrentMinutes = MINMAX(0, iCurrentMinutes - 60, 180);
                iNextStrikes = ItemSystem::GetItemValue(soulItem, 2);

                if (iCurrentMinutes < 60)
                {
                    ItemSystem::UnlockItem(soulItem);
                    ItemSystem::SetItemSocket(soulItem, 1, false);
                    AffectSystem::RemoveAffect(GetEntityHandle(), const_cast<CAffect*>(pAffect));
                }

                ItemSystem::SetItemSocket(soulItem, 2, 0);
                ItemSystem::StartSoulItemEventEcs(soulItem);
            }

            ItemSystem::SetItemSocket(
                soulItem, 2, iCurrentMinutes * 10000 + iNextStrikes);
        }
    }

    return iDamageAdd;
}
#endif


// ShopState is the only copy. CShop::RemoveGuest clears it through
// normal close path and stayed pointing at the NPC. Eleven "is this
// player busy" guards read this getter, so one closed shop left the
// player unable to open anything at all.
#ifdef __ENABLE_NEW_OFFLINESHOP__
#endif

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
void CHARACTER::SetWheelDestiny(std::shared_ptr<CWheelDestiny> pt)
{
    pWheelDestiny = std::move(pt);
    const auto e = GetEntityHandle();
    if (e != entt::null && g_registry.valid(e))
    {
        auto& shop = g_registry.get_or_emplace<ecs::ShopState>(e);
        shop.wheelDestiny = pWheelDestiny;
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    }
}
#endif

void CHARACTER::SetRegen(LPREGEN pkRegen)
{
    m_pkRegen = pkRegen;
    if (pkRegen != nullptr) {
        regen_id_ = pkRegen->id;
    }
    m_fRegenAngle = GetRotation();
    m_posRegen = GetXYZ();
}

namespace ecs::PlayerRuntime {

} // namespace ecs::PlayerRuntime

#ifdef ENABLE_SORT_INVEN
void CHARACTER::EditMyInven()
{
    // Disabled until the inventory sorter is rebuilt on the ECS inventory API.
}

void CHARACTER::EditMyExtraInven()
{
    // Disabled until the extra-inventory sorter is rebuilt on the ECS inventory API.
}
#endif
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
static int NeedKeys[] = { 2,2,2,2,3,3,4,4,4,5,5,5,6,6,6,7,7,7 };
bool CHARACTER::Update_Inven()
{
#ifdef ENABLE_SPAM_CHECK
    int32_t time = GetLastUnlock() - get_global_time();
    if (time > 0) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 234, "%d", time);
#endif
        return false;
    }
#endif

#define key2 72320
    int needkey = NeedKeys[Inven_Point()];
    if (CountSpecifyItem(key2) >= needkey) {
        RemoveSpecifyItem(key2, needkey);
        PointChange(POINT_INVEN, 1, false);
        ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_COMMAND, "refreshinven");
        NetworkSyncSystem::UpdatePacket(GetEntityHandle());
#ifdef ENABLE_SPAM_CHECK
        SetLastUnlock();
#endif
        return true;
    }
    else {
        int need_key = needkey - CountSpecifyItem(key2);
        ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_COMMAND, "update_envanter_need %d", need_key);
        return false;
    }
}
#endif

#ifdef __ENABLE_NEW_OFFLINESHOP__
#endif

#ifdef __NEWPET_SYSTEM__

#endif

#ifdef ENABLE_RANKING
void CHARACTER::RankingSubcategory(int iArg)
{
    if (!GetDesc())
        return;

    if ((iArg < 0) || (iArg >= RANKING_MAX_CATEGORIES))
        return;

    TPacketGCRankingTable p;
    int j = 0;

    char szQuery1[1024] = { 0 };
    snprintf(szQuery1, sizeof(szQuery1), "SELECT account_id, level, name, r%d FROM player.player%s WHERE account_id=(SELECT id FROM account.account%s WHERE status='OK' AND id=account_id) AND name not in(SELECT mName FROM common.gmlist%s) ORDER BY r%d desc, level desc, name asc LIMIT 50", iArg, get_table_postfix(), get_table_postfix(), get_table_postfix(), iArg);
    std::unique_ptr<SQLMsg> pRes1(DBManager::instance().DirectQuery(szQuery1));
    uint32_t iRes = pRes1->Get()->uiNumRows;
    if (iRes > 0) {
        MYSQL_ROW data;
        while ((data = mysql_fetch_row(pRes1->Get()->pSQLResult))) {
            int col = 1;
            p.list[j].iPosition = j;
            p.list[j].iRealPosition = 0;
            p.list[j].iLevel = atoi(data[col++]);
            strlcpy(p.list[j].szName, data[col++], sizeof(p.list[j].szName));
            p.list[j].iPoints = atoi(data[col]);
            j += 1;
        }
    }

    if (j < MAX_RANKING_LIST) {
        for (int i = j; i < MAX_RANKING_LIST; i++) {
            p.list[i].iPosition = i;
            p.list[i].iRealPosition = 0;
            p.list[i].iLevel = 0;
            p.list[i].iPoints = 0;
            strlcpy(p.list[i].szName, "", sizeof(p.list[i].szName));
        }
    }

    char szQuery2[1024] = { 0 };
    if (ecs::PlayerRuntime::GetGMLevel(GetEntityHandle()) > GM_PLAYER) {
        snprintf(szQuery2, sizeof(szQuery2), "SELECT * FROM (SELECT @rank:=0) a, (SELECT @rank:=@rank+1 r, r%d, name, level FROM player.player%s AS res ORDER BY r%d desc, level desc, name asc) as custom WHERE name='%s'", iArg, get_table_postfix(), iArg, GetName());
    }
    else {
        snprintf(szQuery2, sizeof(szQuery2), "SELECT * FROM (SELECT @rank:=0) a, (SELECT @rank:=@rank+1 r, r%d, name, level FROM player.player%s AS res WHERE name not in(SELECT mName FROM common.gmlist) ORDER BY r%d desc, level desc, name asc) as custom WHERE name='%s'", iArg, get_table_postfix(), iArg, GetName());
    }
    std::unique_ptr<SQLMsg> pRes2(DBManager::instance().DirectQuery(szQuery2));
    iRes = pRes2->Get()->uiNumRows;
    if (iRes > 0) {
        j = MAX_RANKING_LIST - 1;
        MYSQL_ROW data = mysql_fetch_row(pRes2->Get()->pSQLResult);
        p.list[j].iPosition = j;
        p.list[j].iRealPosition = atoi(data[1]);
        p.list[j].iLevel = atoi(data[4]);
        p.list[j].iPoints = atoi(data[2]);
        strlcpy(p.list[j].szName, GetName(), sizeof(p.list[j].szName));
    }

    GetDesc()->Packet(&p, sizeof(p));
}
#endif

#ifdef ENABLE_PVP_ADVANCED
#endif

namespace ecs::PlayerRuntime {

void SetPart(entt::entity e, uint8_t partPos, uint16_t value)
{
    assert(partPos < PART_MAX_NUM);

    if (auto* appearance = EnsureAppearancePartsComponent(e))
        appearance->parts[partPos] = value;
}

} // namespace ecs::PlayerRuntime

namespace ecs::PlayerRuntime {

uint16_t GetPart(entt::entity e, uint8_t bPartPos)
{
    assert(bPartPos < PART_MAX_NUM);
	const entt::entity character = e;

#ifdef __HIDE_COSTUME_SYSTEM__
    if (bPartPos == PART_MAIN &&
		ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_BODY)) &&
		IsCostumeHidden(e, 1) == true) {
		const entt::entity armor = ItemSystem::GetWearItem(character, WEAR_BODY);
		if (!ItemSystem::IsValidItem(armor))
			return 0;
		const uint32_t transmutation = ItemSystem::GetItemTransmutationVnum(armor);
		return transmutation != 0 ? transmutation : ItemSystem::GetItemVnum(armor);
    }
    else if (bPartPos == PART_HAIR &&
		ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_HAIR)) &&
		IsCostumeHidden(e, 2) == true)
        return 0;
#ifdef ENABLE_STOLE_COSTUME
    else if (bPartPos == PART_ACCE &&
		ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_ACCE)) &&
		IsCostumeHidden(e, 3) == true) {
		const entt::entity acce = ItemSystem::GetWearItem(character, WEAR_COSTUME_ACCE_SLOT);
        if (ItemSystem::IsValidItem(acce)) {
            uint32_t toSetValue = ItemSystem::GetItemVnum(acce);
            toSetValue -= 85000;
            if (ItemSystem::GetItemSocket(acce, ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
                toSetValue += 1000;

            return toSetValue;
        }
        else
            return 0;
    }
#else
    else if (bPartPos == PART_ACCE &&
		ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_ACCE_SLOT)) &&
		IsCostumeHidden(e, 3) == true)
        return 0;
#endif
    else if (bPartPos == PART_WEAPON &&
		ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_WEAPON)) &&
		IsCostumeHidden(e, 4) == true)
    {
		const entt::entity weapon = ItemSystem::GetWearItem(character, WEAR_WEAPON);
		if (!ItemSystem::IsValidItem(weapon))
			return 0;
		const uint32_t transmutation = ItemSystem::GetItemTransmutationVnum(weapon);
		return transmutation != 0 ? transmutation : ItemSystem::GetItemVnum(weapon);
    }
#endif

    if (const auto* appearance = TryGetAppearancePartsComponent(e))
        return appearance->parts[bPartPos];

    return 0;
}

} // namespace ecs::PlayerRuntime

namespace ecs::PlayerRuntime {

uint16_t GetOriginalPart(entt::entity e, uint8_t bPartPos)
{
	const entt::entity character = e;
    switch (bPartPos)
    {
    case PART_MAIN:
    {
        if (!(GetDesc(e) != nullptr))
            return GetPart(e, PART_MAIN);

#ifdef __HIDE_COSTUME_SYSTEM__
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_BODY)) &&
			IsCostumeHidden(e, 1) == true) {
			const entt::entity armor = ItemSystem::GetWearItem(character, WEAR_BODY);
			if (ItemSystem::IsValidItem(armor))
				return ItemSystem::GetItemVnum(armor);
        }
#endif

        if (const auto* appearance = TryGetAppearancePartsComponent(e))
            return appearance->basePart;

        return 0;
    }
    case PART_HAIR:
    {
#ifdef __HIDE_COSTUME_SYSTEM__
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_HAIR)) &&
			IsCostumeHidden(e, 2) == true)
            return 0;
#endif

        return GetPart(e, PART_HAIR);
    }
#ifdef ENABLE_ACCE_SYSTEM
    case PART_ACCE:
    {
#ifdef __HIDE_COSTUME_SYSTEM__
#ifdef ENABLE_STOLE_COSTUME
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_ACCE)) &&
			IsCostumeHidden(e, 3) == true) {
			const entt::entity acce = ItemSystem::GetWearItem(character, WEAR_COSTUME_ACCE_SLOT);
            if (ItemSystem::IsValidItem(acce)) {
                uint32_t toSetValue = ItemSystem::GetItemVnum(acce);
                toSetValue -= 85000;
                if (ItemSystem::GetItemSocket(acce, ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
                    toSetValue += 1000;

                return toSetValue;
            }
            else
                return 0;
        }
#else
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_ACCE_SLOT)) &&
			IsCostumeHidden(e, 3) == true)
            return 0;
#endif
#else
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_ACCE_SLOT)))
            return 0;
#endif
        return GetPart(e, PART_ACCE);
    }
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
    case PART_WEAPON:
    {
#ifdef __HIDE_COSTUME_SYSTEM__
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_WEAPON)) &&
			IsCostumeHidden(e, 4) == true) {
			const entt::entity weapon = ItemSystem::GetWearItem(character, WEAR_WEAPON);
			if (ItemSystem::IsValidItem(weapon))
				return ItemSystem::GetItemVnum(weapon);
        }
#endif
        return GetPart(e, PART_WEAPON);
#endif
    }
    default:
        return 0;
    }
}

} // namespace ecs::PlayerRuntime

void CHARACTER::Destroy()
{
	// Keep the ECS entity alive for the complete teardown. Inventory, session,
	// shop and social state are ECS-owned now, so destroying the entity before
	// ClearItem()/ecs::SocialSystem::CloseMyShop(GetEntityHandle()) turns those cleanup calls into silent no-ops.
	const entt::entity entityToDestroy = GetEntityHandle();

    ecs::SocialSystem::CloseMyShop(GetEntityHandle());

    if (m_pkRegen)
    {
        if (ecs::SocialSystem::GetDungeon(GetEntityHandle())) {
            if (ecs::SocialSystem::GetDungeon(GetEntityHandle())->IsValidRegen(m_pkRegen, regen_id_)) {
                --m_pkRegen->count;
            }
        }
        else {
            --m_pkRegen->count;
        }
        m_pkRegen = nullptr;
    }

    if (ecs::SocialSystem::GetDungeon(GetEntityHandle()))
    {
        ecs::SocialSystem::SetDungeon(GetEntityHandle(), nullptr);
    }

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    if (m_mountSystem)
    {
        delete m_mountSystem;

        m_mountSystem = nullptr;
        if (GetEntityHandle() != entt::null && g_registry.valid(GetEntityHandle()))
            g_registry.get_or_emplace<ecs::MountRuntimeRefs>(GetEntityHandle()).mountSystem = nullptr;
    }

    if (GetMountVnum())
    {
        AffectSystem::RemoveAffect(GetEntityHandle(), AFFECT_MOUNT);
        AffectSystem::RemoveAffect(GetEntityHandle(), AFFECT_MOUNT_BONUS);
    }
    HorseSummon(false);
#endif
#ifdef __PET_SYSTEM__
    if (m_petSystem)
    {
        delete m_petSystem;

        m_petSystem = nullptr;
		if (GetEntityHandle() != entt::null && g_registry.valid(GetEntityHandle()))
			g_registry.get_or_emplace<ecs::PetRuntimeRefs>(GetEntityHandle()).petSystem = nullptr;
    }
#endif

#ifdef __NEWPET_SYSTEM__
    if (m_newpetSystem)
    {
        delete m_newpetSystem;

        m_newpetSystem = nullptr;
		if (GetEntityHandle() != entt::null && g_registry.valid(GetEntityHandle()))
			g_registry.get_or_emplace<ecs::PetRuntimeRefs>(GetEntityHandle()).newPetSystem = nullptr;
    }
#endif

    HorseSummon(false);

    if (GetRider())
        GetRider()->ClearHorseInfo();

    if (GetDesc())
    {
        GetDesc()->BindCharacter(nullptr);
    }

    ExchangeSystem::Cancel(GetEntityHandle());

    CombatSystem::SetVictim(GetEntityHandle(), entt::null);

    if (CShop* shop = ecs::SocialSystem::GetShop(GetEntityHandle()))
    {
        shop->RemoveGuest(GetEntityHandle());
        ecs::SocialSystem::SetShop(GetEntityHandle(), nullptr);
    }

    CombatSystem::ClearStone(GetEntityHandle());
    NetworkSyncSystem::ClearSync(GetEntityHandle());
    CombatSystem::ClearTarget(GetEntityHandle());

    if (nullptr == m_pkMobData)
    {
        DragonSoulSystem::CleanUp(GetEntityHandle());
        ClearItem();
    }

    LPPARTY party = m_pkParty;
    if (party)
    {
        if (party->GetLeaderPID() == GetLegacyVID() && !IsPC())
        {
            M2_DELETE(party);
        }
        else
        {
            party->Unlink(GetEntityHandle());

            if (!IsPC())
                party->Quit(GetLegacyVID());
        }

        SetParty(nullptr);
    }

    // Mob runtime state goes with the entity; there is no allocation to free.
    if (g_registry.valid(GetEntityHandle()))
        g_registry.remove<ecs::MobInstanceState>(GetEntityHandle());

    m_pkMobData = nullptr;

    SafeboxSystem::Close(GetEntityHandle(), SAFEBOX, false);
    SafeboxSystem::Close(GetEntityHandle(), MALL, false);

    ecs::PlayerRuntime::BuffOnAttr_Destroy(GetEntityHandle());


    StopMuyeongEvent();
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
    StopGyeongGongEvent();
#endif
    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::WarpNPC);
    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Recovery);
    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Dead);
    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Save);
    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Timed);
    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Stun);
    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Fishing);
    AffectSystem::CancelDamageEvents(GetEntityHandle());
    event_cancel(&m_pkPartyRequestEvent);
    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Warp);
#ifdef ENABLE_NEW_FISHING_SYSTEM
    ActivitySystem::StopFishing(GetEntityHandle());
#endif
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
    if (ecs::PlayerRuntime::GetCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::BattlePassStayOnline))
    {
        ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::BattlePassStayOnline);
    }
#endif

    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Mining);
#ifdef ENABLE_BLOCK_MULTIFARM
    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Drop);
#endif

    SkillSystem::CancelAllMobSkillEvents(GetEntityHandle());
#ifdef __DUNGEON_INFO_SYSTEM__
    dungeonDamage.clear();
#endif
    AffectSystem::ClearAffect(GetEntityHandle(), false);

    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::DestroyWhenIdle);


    if (MountSystem::GetMountInventory(GetEntityHandle()))
    {
        M2_DELETE(MountSystem::GetMountInventory(GetEntityHandle()));
        MountSystem::SetMountInventory(GetEntityHandle(), nullptr);
    }
    m_bMountInventoryLoaded = false;

    CEntity::Destroy();

    const entt::entity e = GetEntityHandle();
    if (GetSectree())
        GetSectree()->RemoveEntity(this);
    if (e != entt::null && g_registry.valid(e))
    {
        g_registry.remove<ecs::SectorPlacement>(e);
        g_registry.remove<ecs::ViewActiveTag>(e);
    }

	if (m_bMonsterLog)
		CHARACTER_MANAGER::instance().UnregisterForMonsterLog(GetEntityHandle());

	if (entityToDestroy != entt::null && g_registry.valid(entityToDestroy))
		EntityFactory::Destroy(g_registry, entityToDestroy);
}

void CHARACTER::ToggleMonsterLog()
{
    m_bMonsterLog = !m_bMonsterLog;

    if (m_bMonsterLog)
    {
        CHARACTER_MANAGER::instance().RegisterForMonsterLog(GetEntityHandle());
    }
    else
    {
        CHARACTER_MANAGER::instance().UnregisterForMonsterLog(GetEntityHandle());
    }
}

void CHARACTER::SendGreetMessage()
{
    auto v = DBManager::instance().GetGreetMessage();

    for (auto it = v.begin(); it != v.end(); ++it)
    {
        ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_NOTICE, it->c_str());
    }
}

void CHARACTER::MountVnum(uint32_t vnum)
{
    if (m_dwMountVnum == vnum)
        return;
    if ((m_dwMountVnum != 0) && (vnum != 0))
        MountVnum(0);

    m_dwMountVnum = vnum;
    m_dwMountTime = get_dword_time();

    const auto e = GetEntityHandle();
    if (e != entt::null && g_registry.valid(e))
    {
        auto& mount = g_registry.get_or_emplace<ecs::MountState>(e);
        mount.mountVnum = vnum;
        mount.mountTime = m_dwMountTime;
        if (vnum == 0)
            mount.horseRiding = false;
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    }

    if (m_bIsObserver)
        return;

    // Phase C.3: legacy destination field write removed. SyncDestinationClear
    // drops ECS MovementDestination so subsequent INSERT packets emit
    // current position (GetX/Y fallback in GetCurrentDestX/Y).
    ecs::MovementSystem::SyncDestinationClear(GetEntityHandle());

    ecs::EntityNetworkDispatch::SendInsert(g_registry, GetEntityHandle(), GetEntityHandle());

    // Phase 15E-final.LPENTITY.4-architect H fixup-6:
    // Replace stale m_map_view walk with ECS ViewerMap.viewers walk.
    // Pre-D.6 the legacy CFuncViewInsert polling kept m_map_view in sync;
    // after D.6 stubbed that polling for character paths, m_map_view is
    // a frozen-at-spawn write-only store. The mount-state CharacterAdd
    // re-broadcast at MountVnum change therefore reached zero peers,
    // leaving every viewer's client rendering the stale (mount-on)
    // state of the rider while the freshly spawned mount mob walked
    // alongside as a duplicate.
    if (e != entt::null && g_registry.valid(e))
    {
        if (auto* viewerMap = g_registry.try_get<ecs::ViewerMap>(e))
        {
            const auto viewers = viewerMap->viewers;
            for (const entt::entity viewerE : viewers)
            {
                if (viewerE == entt::null || !g_registry.valid(viewerE))
                    continue;
                ecs::EntityNetworkDispatch::SendInsert(g_registry, GetEntityHandle(), viewerE);
            }
        }
    }

    CombatSystem::SetValidComboInterval(GetEntityHandle(), 0);
    CombatSystem::SetComboSequence(GetEntityHandle(), 0);

    ComputePoints();
}

void CHARACTER::SetPlayerProto(const TPlayerTable* t)
{
    if (!GetDesc() || !*GetDesc()->GetHostName())
        LOG_ERROR("cannot get desc or hostname");
    else
        ecs::PlayerRuntime::RefreshGMLevel(GetEntityHandle());

    m_bCharType = CHAR_TYPE_PC;

    m_dwPlayerID = t->id;

    if (auto* combat = g_registry.try_get<ecs::CombatStats>(GetEntityHandle())) {
        combat->alignment = std::min<uint32_t>(t->lAlignment, CombatSystem::MAX_ALIGNMENT);
        combat->realAlignment = combat->alignment;
    }



    if (auto* appearance = EnsureAppearancePartsComponent(GetEntityHandle()))
        appearance->basePart = t->part_base;
    ecs::PlayerRuntime::SetPart(GetEntityHandle(), PART_HAIR, t->parts[PART_HAIR]);
#ifdef ENABLE_ACCE_SYSTEM
    ecs::PlayerRuntime::SetPart(GetEntityHandle(), PART_ACCE, t->parts[PART_ACCE]);
#endif

    ecs::PointSystem::SetRandomHP(GetEntityHandle(), t->sRandomHP);
    ecs::PointSystem::SetRandomSP(GetEntityHandle(), t->sRandomSP);

    SkillSystem::LoadSkillLevels(GetEntityHandle(), t->skills, t->skill_group);
#ifdef ENABLE_BATTLE_PASS
    AffectSystem::SetBattlePassDeadline(GetEntityHandle(), t->dwBattlePassEndTime);
#endif

    if (t->lMapIndex >= 10000)
    {
        ecs::MovementSystem::SetWarpLocationRaw(GetEntityHandle(), t->lExitMapIndex, t->lExitX, t->lExitY);
    }

    SetRealPoint(POINT_PLAYTIME, t->playtime);
    m_dwLoginPlayTime = t->playtime;
    SetRealPoint(POINT_ST, t->st);
    SetRealPoint(POINT_HT, t->ht);
    SetRealPoint(POINT_DX, t->dx);
    SetRealPoint(POINT_IQ, t->iq);

    SetPoint(POINT_ST, t->st);
    SetPoint(POINT_HT, t->ht);
    SetPoint(POINT_DX, t->dx);
    SetPoint(POINT_IQ, t->iq);

    SetPoint(POINT_STAT, t->stat_point);
    SetPoint(POINT_SKILL, t->skill_point);
    SetPoint(POINT_SUB_SKILL, t->sub_skill_point);
    SetPoint(POINT_HORSE_SKILL, t->horse_skill_point);

    SetPoint(POINT_STAT_RESET_COUNT, t->stat_reset_count);

    SetPoint(POINT_LEVEL_STEP, t->level_step);
    SetRealPoint(POINT_LEVEL_STEP, t->level_step);

    ecs::PlayerRuntime::SetRace(GetEntityHandle(), t->job);

    ecs::PlayerRuntime::SetLevel(GetEntityHandle(), t->level);
    ecs::PlayerRuntime::SetExp(GetEntityHandle(), t->exp);
    ecs::PlayerRuntime::SetGold(GetEntityHandle(), t->gold);
#ifdef ENABLE_GAYA_SYSTEM
    ecs::PointSystem::SetGaya(GetEntityHandle(), t->gaya);
#endif
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
    Set_Inventory_Point(t->envanter);
#endif

    SetMapIndex(t->lMapIndex);
    // Phase C.1: legacy m_pos write removed - ECS Position via
    // SyncPositionComponents is the sole source.
    ecs::SyncPositionComponents(g_registry, GetEntityHandle(), t->lMapIndex, t->x, t->y, t->z);

    // Phase C.3: legacy destination field write removed. SyncDestinationClear
    // drops ECS MovementDestination - GetCurrentDestX/Y now returns
    // GetX/Y (the loaded position) so EncodeInsertPacket emits the
    // correct values without legacy dest priming.
    ecs::MovementSystem::SyncDestinationClear(GetEntityHandle());

    ComputePoints();

    ecs::PlayerRuntime::SetHP(GetEntityHandle(), t->hp);
    ecs::PlayerRuntime::SetSP(GetEntityHandle(), t->sp);
    ecs::PlayerRuntime::SetStamina(GetEntityHandle(), t->stamina);

#ifndef ENABLE_GM_FLAG_IF_TEST_SERVER
    if (!test_server)
#endif
    {
#ifdef ENABLE_GM_FLAG_FOR_LOW_WIZARD
        if (ecs::PlayerRuntime::GetGMLevel(GetEntityHandle()) > GM_PLAYER)
#else
        if (ecs::PlayerRuntime::GetGMLevel(GetEntityHandle()) > GM_LOW_WIZARD)
#endif
        {
            AffectSystem::SetFlag(GetEntityHandle(), AFF_YMIR);
            if (ecs::diag::Check(GetEntityHandle(), "SetPlayerProto/GM"))
                g_registry.get_or_emplace<ecs::CombatStats>(GetEntityHandle()).pkMode = PK_MODE_PROTECT;
        }
    }

    if (ecs::PointSystem::GetLevel(GetEntityHandle()) < PK_PROTECT_LEVEL) {
        if (ecs::diag::Check(GetEntityHandle(), "SetPlayerProto/lowLevel"))
            g_registry.get_or_emplace<ecs::CombatStats>(GetEntityHandle()).pkMode = PK_MODE_PROTECT;
    }

    ecs::PlayerRuntime::SetMobilePhone(GetEntityHandle(), t->szMobile);

    SetHorseData(t->horse);

    if (GetHorseLevel() > 0)
        UpdateHorseDataByLogoff(t->logoff_interval);

    memcpy(m_aiPremiumTimes, t->aiPremiumTimes, sizeof(t->aiPremiumTimes));
	if (const entt::entity character = GetEntityHandle();
		character != entt::null && g_registry.valid(character))
	{
		auto& login = g_registry.get_or_emplace<ecs::LoginInfo>(character);
		std::copy_n(std::begin(t->aiPremiumTimes), PREMIUM_MAX_NUM,
			login.premiumTimes.begin());
	}

    m_dwLogOffInterval = t->logoff_interval;

    LOG_INFO("PLAYER_LOAD: {} PREMIUM {} {}, LOGGOFF_INTERVAL {} PTR: {}", t->name, m_aiPremiumTimes[0], m_aiPremiumTimes[1], t->logoff_interval, static_cast<const void*>(this));

    if (ecs::PlayerRuntime::GetGMLevel(GetEntityHandle()) != GM_PLAYER)
    {
        LogManager::instance().CharLog(GetEntityHandle(), ecs::PlayerRuntime::GetGMLevel(GetEntityHandle()), "GM_LOGIN", "");
        LOG_INFO("GM_LOGIN(gmlevel={}, name={}({}), pos=({}, {})", static_cast<int>(ecs::PlayerRuntime::GetGMLevel(GetEntityHandle())), GetName(), GetPlayerID(), GetX(), GetY());
    }

#ifdef __PET_SYSTEM__
    if (m_petSystem)
    {
        delete m_petSystem;
    }

    m_petSystem = M2_NEW CPetSystem(GetEntityHandle());
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    if (m_mountSystem)
    {
        delete m_mountSystem;
    }

    m_mountSystem = M2_NEW CMountSystem(GetEntityHandle());
#endif

#ifdef __NEWPET_SYSTEM__
    if (m_newpetSystem)
    {
        delete m_newpetSystem;
    }

    m_newpetSystem = M2_NEW CNewPetSystem(GetEntityHandle());
#endif
}

void CHARACTER::SetProto(const CMob* pkMob)
{
    m_pkMobData = pkMob;

    // mob_manager.cpp reaches here through an IsPC() test, and IsPC(entt::null)
    // is false - so a character with no entity yet passes the filter instead of
    // being turned back. NDEBUG is set for Release and RelWithDebInfo, so there
    // the invalid handle is not an assert but a write through entt::null.
    if (const entt::entity self = GetEntityHandle(); ecs::diag::Check(self, "SetProto"))
    {
        g_registry.emplace_or_replace<ecs::MobDataRef>(self, pkMob);
        // The mob runtime state starts here, as the CMobInstance allocation
        // did: last-attacked at the origin, every mode switch off.
        auto& mobState = g_registry.emplace_or_replace<ecs::MobInstanceState>(self);
        mobState.lastAttackedTime = get_dword_time();
        g_registry.get_or_emplace<ecs::CombatStats>(self).pkMode = PK_MODE_FREE;
    }

    const TMobTable* t = &m_pkMobData->m_table;

    m_bCharType = t->bType;
    // The factory fills CharacterType at spawn, but SetProto can change the
    // type afterwards and used to leave the component behind.
    if (const entt::entity self = GetEntityHandle();
        self != entt::null && g_registry.valid(self))
        g_registry.emplace_or_replace<ecs::CharacterType>(self, static_cast<uint8_t>(t->bType));

    ecs::PlayerRuntime::SetLevel(GetEntityHandle(), t->bLevel);
    ecs::PlayerRuntime::SetEmpire(GetEntityHandle(), t->bEmpire);

    ecs::PlayerRuntime::SetExp(GetEntityHandle(), t->dwExp);
    SetRealPoint(POINT_ST, t->bStr);
    SetRealPoint(POINT_DX, t->bDex);
    SetRealPoint(POINT_HT, t->bCon);
    SetRealPoint(POINT_IQ, t->bInt);

    ComputePoints();

    ecs::PlayerRuntime::SetHP(GetEntityHandle(), ecs::PointSystem::GetMaxHP(GetEntityHandle()));
    ecs::PlayerRuntime::SetSP(GetEntityHandle(), ecs::PointSystem::GetMaxSP(GetEntityHandle()));
    if (auto* flags = EnsureRuntimeFlagsComponent(GetEntityHandle()))
        flags->aiFlag = t->dwAIFlag;
    ecs::PlayerRuntime::SetImmuneFlag(GetEntityHandle(), t->dwImmuneFlag);

    AssignTriggers(t);

    AffectSystem::ApplyMobAttribute(GetEntityHandle(), t);

    if (IsStone())
    {
        CombatSystem::DetermineDropMetinStone(GetEntityHandle());
    }

    if (ecs::PlayerRuntime::IsWarp(GetEntityHandle()) || ecs::PlayerRuntime::IsGoto(GetEntityHandle()))
    {
        ecs::MovementSystem::StartWarpNPCEvent(GetEntityHandle());
    }

    CHARACTER_MANAGER::instance().RegisterRaceNumMap(GetEntityHandle());

    if (mining::IsVeinOfOre(GetRaceNum()))
    {
        char_event_info* info = AllocEventInfo<char_event_info>();

        info->ch = GetEntityHandle();

        ecs::PlayerRuntime::SetCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Mining,
            event_create(kill_ore_load_event, info, PASSES_PER_SEC(number(7 * 60, 15 * 60))));
    }
}

void CHARACTER::MonsterLog(const char* format, ...)
{
    if (!test_server)
        return;

    if (IsPC())
        return;

    char chatbuf[CHAT_MAX_LEN + 1];
    int len = snprintf(chatbuf, sizeof(chatbuf), "%lu)", static_cast<unsigned long>(GetPacketVID()));

    if (len < 0 || len >= (int)sizeof(chatbuf))
        len = sizeof(chatbuf) - 1;

    va_list args;

    va_start(args, format);

    int len2 = vsnprintf(chatbuf + len, sizeof(chatbuf) - len, format, args);

    if (len2 < 0 || len2 >= (int)sizeof(chatbuf) - len)
        len += (sizeof(chatbuf) - len) - 1;
    else
        len += len2;

    ++len;

    va_end(args);

    TPacketGCChat pack_chat;

    pack_chat.header = HEADER_GC_CHAT;
    pack_chat.size = sizeof(TPacketGCChat) + len;
    pack_chat.type = CHAT_TYPE_TALKING;
    pack_chat.id = GetPacketVID();
    pack_chat.bEmpire = 0;

    TEMP_BUFFER buf;
    buf.write(&pack_chat, sizeof(TPacketGCChat));
    buf.write(chatbuf, len);

    CHARACTER_MANAGER::instance().PacketMonsterLog(GetEntityHandle(), buf.read_peek(), buf.size());
}

void CHARACTER::OnMove(bool bIsAttack)
{
    m_dwLastMoveTime = get_dword_time();
    ecs::SyncPositionComponents(g_registry, GetEntityHandle(), GetMapIndex(), GetX(), GetY(), GetZ());

    if (bIsAttack)
    {
        CombatSystem::SetLastAttackTime(GetEntityHandle(), m_dwLastMoveTime);

        if (AffectSystem::IsAffectFlag(GetEntityHandle(), AFF_REVIVE_INVISIBLE))
            AffectSystem::RemoveAffect(GetEntityHandle(), AFFECT_REVIVE_INVISIBLE);

        if (AffectSystem::IsAffectFlag(GetEntityHandle(), AFF_EUNHYUNG))
        {
            AffectSystem::RemoveAffect(GetEntityHandle(), SKILL_EUNHYUNG);
            SetAffectedEunhyung();
        }
        else
        {
            ClearAffectedEunhyung();
        }

        /*if (AffectSystem::IsAffectFlag(GetEntityHandle(), AFF_JEONSIN))
          AffectSystem::RemoveAffect(GetEntityHandle(), SKILL_JEONSINBANGEO);*/
    }

    /*if (AffectSystem::IsAffectFlag(GetEntityHandle(), AFF_GUNGON))
      AffectSystem::RemoveAffect(GetEntityHandle(), SKILL_GUNGON);*/

    // MINING
    ActivitySystem::CancelMining(GetEntityHandle());
    // END_OF_MINING
}

void CHARACTER::OnClick(entt::entity causer)
{
    LPCHARACTER pkCauser = ecs::LegacyCharOf(causer);
    if (!pkCauser)
    {
        LOG_ERROR("OnClick {} by NULL", GetName());
        return;
    }

    uint32_t vid = GetPacketVID();
    LOG_INFO("OnClick {}[vnum: {} vid: {}] by {}", GetName(), GetRaceNum(), vid, pkCauser->GetName());

    {
        if (ecs::SocialSystem::GetMyShop(causer) && pkCauser != this)
        {
            LOG_ERROR("OnClick Fail ({}->{}) - pc has shop", pkCauser->GetName(), GetName());
            return;
        }
    }

    {
        if (ecs::SocialSystem::HasExchange(causer))
        {
            LOG_ERROR("OnClick Fail ({}->{}) - pc is exchanging", pkCauser->GetName(), GetName());
            return;
        }
    }

    if (IsPC())
    {
        if (!CTargetManager::instance().GetTargetInfo(pkCauser->GetPlayerID(), TARGET_TYPE_VID, GetPacketVID()))
        {
            if (ecs::SocialSystem::GetMyShop(GetEntityHandle()))
            {
                if (CombatSystem::IsDead(causer) == true)
                    return;

                if (pkCauser == this)
                {
                    if ((ecs::SocialSystem::HasExchange(GetEntityHandle()) || ecs::SessionSystem::IsSafeboxOpen(GetEntityHandle()) || ecs::SocialSystem::GetShopOwner(GetEntityHandle()) != entt::null) || IsCubeOpen())
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }

#ifdef __ATTR_TRANSFER_SYSTEM__
                    if (AttrTransfer_is_open(GetEntityHandle()))
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }
#endif
                }
                else
                {
                    if ((ecs::SocialSystem::HasExchange(causer) || ecs::SessionSystem::IsSafeboxOpen(causer) || ecs::SocialSystem::GetMyShop(causer) || ecs::SocialSystem::GetShopOwner(causer) != entt::null) || pkCauser->IsCubeOpen())
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }

#ifdef __ATTR_TRANSFER_SYSTEM__
                    if (AttrTransfer_is_open(causer))
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }
#endif

                    if ((ecs::SocialSystem::HasExchange(GetEntityHandle()) || ecs::SessionSystem::IsSafeboxOpen(GetEntityHandle()) || IsCubeOpen()))
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 369, "%s", GetName());
#endif
                        return;
                    }

#ifdef __ATTR_TRANSFER_SYSTEM__
                    if (AttrTransfer_is_open(GetEntityHandle()))
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 369, "%s", GetName());
#endif
                        return;
                    }
#endif
                }

                if (CShop* shop = ecs::SocialSystem::GetShop(causer))
                {
                    shop->RemoveGuest(causer);
                    ecs::SocialSystem::SetShop(causer, nullptr);
                }

                ecs::SocialSystem::GetMyShop(GetEntityHandle())->AddGuest(causer, GetPacketVID(), false);
                ecs::SocialSystem::SetShopOwner(causer, GetEntityHandle());
                return;
            }

            if (test_server)
                LOG_ERROR("{}.OnClickFailure({}) - target is PC", pkCauser->GetName(), GetName());

            return;
        }
    }

    ecs::PlayerRuntime::SetQuestNPCID(causer, GetPacketVID());

    if (quest::CQuestManager::instance().Click(pkCauser->GetPlayerID(), this))
    {
        return;
    }

    if (!IsPC())
    {
        if (!m_triggerOnClick.pFunc)
        {
            return;
        }

        m_triggerOnClick.pFunc(GetEntityHandle(),
			pkCauser ? causer : entt::null);
    }
}

void CHARACTER::DestroyPvP()
{
    if (GetDesc() != nullptr)
    {
        const char* szTableStaticPvP[] = { BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };

        int moneyBet = ecs::PlayerRuntime::GetQuestFlag(GetEntityHandle(), szTableStaticPvP[8]);
        int isDuel = ecs::PlayerRuntime::GetQuestFlag(GetEntityHandle(), szTableStaticPvP[9]);

        if (isDuel != 0)
        {
            if (moneyBet > 0)
            {
                PointChange(POINT_GOLD, moneyBet, true);
            }

            char szBuf[CHAT_MAX_LEN + 1];
            snprintf(szBuf, sizeof(szBuf), "BINARY_Duel_Delete");
            ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_COMMAND, szBuf);

            for (size_t i = 0; i < _countof(szTableStaticPvP); i++)
            {
                ecs::PlayerRuntime::SetQuestFlag(GetEntityHandle(), szTableStaticPvP[i], 0);
            }
        }
    }
}

void CHARACTER::RestartAtSamePos()
{
    if (m_bIsObserver)
        return;

    const entt::entity self = GetEntityHandle();
    if (self == entt::null || !g_registry.valid(self))
        return;

    // The ECS ViewMap, not m_map_view: this is a CHARACTER, and the legacy map
    // stopped being maintained for characters when D.6 disabled the polling in
    // UpdateSectree, so this walk was reading whatever it was frozen with.
    //
    // Unlike ViewReencode this keeps BOTH packet directions. Fixup-5 dropped
    // the peer direction there because a moving character's animation was
    // being reset on every peer; a restart is a deliberate full respawn, so
    // the peer refresh is the point of it.
    ecs::EntityNetworkDispatch::SendRemove(g_registry, self, self);
    ecs::EntityNetworkDispatch::SendInsert(g_registry, self, self);

    const auto* viewMap = g_registry.try_get<ecs::ViewMap>(self);
    if (!viewMap)
        return;

    const auto visible = viewMap->visible;  // snapshot: the loop dispatches
    for (const entt::entity other : visible)
    {
        if (other == entt::null || !g_registry.valid(other) || other == self)
            continue;

        ecs::EntityNetworkDispatch::SendRemove(g_registry, self, other);
        if (!m_bIsObserver)
            ecs::EntityNetworkDispatch::SendInsert(g_registry, self, other);

        // The original let every non-character through and filtered characters
        // to PC || NPC || monster - which excludes only a CHAR_TYPE_PC entity
        // with no descriptor, i.e. a link-dead player. Keep that.
        bool reverse = true;
        if (const auto* kind = g_registry.try_get<ecs::SpatialKindTag>(other);
            kind && kind->kind == ecs::SpatialKind::Character)
        {
            const auto* type = g_registry.try_get<ecs::CharacterType>(other);
            reverse = ecs::PlayerRuntime::GetDesc(other) != nullptr
                || (type && type->value != CHAR_TYPE_PC);
        }

        if (reverse && !ecs::PlayerRuntime::IsObserverMode(other))
            ecs::EntityNetworkDispatch::SendInsert(g_registry, other, self);
    }
}

#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
bool CHARACTER::SwitchChannel(int32_t newAddr, uint16_t newPort)
{
    if (!IsPC() || !GetDesc() || !ecs::PlayerRuntime::CanWarp(GetEntityHandle()))
        return false;

    int32_t x = GetX();
    int32_t y = GetY();

    int32_t lAddr = newAddr;
    int32_t lMapIndex = GetMapIndex();
    uint16_t wPort = newPort;

    if (lMapIndex >= 10000)
    {
        LOG_ERROR("Invalid change channel request from dungeon {}!", lMapIndex);
        return false;
    }

    if (g_bChannel == 99)
    {
        LOG_ERROR("{} attempted to change channel from CH99, ignoring req.", GetName());
        return false;
    }

    ecs::MovementSystem::Stop(GetEntityHandle());
    ecs::SessionSystem::Save(GetEntityHandle());

    if (GetSectree())
    {
        GetSectree()->RemoveEntity(this);
        const entt::entity e = GetEntityHandle();
        if (e != entt::null && g_registry.valid(e))
        {
            g_registry.remove<ecs::SectorPlacement>(e);
            g_registry.remove<ecs::ViewActiveTag>(e);
        }
        ecs::ViewSystem::ViewCleanup(e);
        ecs::EntityNetworkDispatch::SendRemove(g_registry, e, e);
    }

    ecs::MovementSystem::SetWarpLocationRaw(GetEntityHandle(), lMapIndex, x, y);

    LOG_INFO("ChangeChannel {}, {} {} map {} to port {}", GetName(), x, y, GetMapIndex(), wPort);

    TPacketGCWarp p;

    p.bHeader = HEADER_GC_WARP;
    p.lX = x;
    p.lY = y;
    p.lAddr = lAddr;
    p.wPort = wPort;

    GetDesc()->Packet(&p, sizeof(p));

    char buf[256];
    snprintf(buf, sizeof(buf), "%s Port%d Map%ld x%ld y%ld", GetName(), wPort, GetMapIndex(), x, y);
    LogManager::instance().CharLog(GetEntityHandle(), 0, "CHANGE_CH", buf);

    return true;
}

EVENTINFO(switch_channel_info)
{
    entt::entity ch { entt::null };
    int secs;
    int32_t newAddr;
    uint16_t newPort;
    switch_channel_info()
        : ch(),
        secs(0),
        newAddr(0),
        newPort(0)
    {
    }
};

EVENTFUNC(switch_channel)
{
    switch_channel_info* info = dynamic_cast<switch_channel_info*>(event->info);
    if (!info)
    {
        LOG_ERROR("No switch channel event info!");
        return 0;
    }

    LPCHARACTER ch = ecs::LegacyCharOf(info->ch);
    if (!ch)
    {
        LOG_ERROR("No char to work on for the switch.");
        return 0;
    }
	const entt::entity character = info->ch;

    if (ecs::PlayerRuntime::GetCharEvent(character, ecs::PlayerRuntime::CharEvent::Timed) != event)
        return 0;

    if (!ecs::PlayerRuntime::GetDesc(character))
        return 0;

    if (info->secs > 0)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 658, "%d", info->secs);
#endif
        --info->secs;
        return PASSES_PER_SEC(1);
    }

    ecs::PlayerRuntime::SetCharEvent(character, ecs::PlayerRuntime::CharEvent::Timed, nullptr);
    ch->SwitchChannel(info->newAddr, info->newPort);
    return 0;
}

bool CHARACTER::StartChannelSwitch(int32_t newAddr, uint16_t newPort)
{
    if (ecs::PlayerRuntime::IsHack(GetEntityHandle(), false, true, 10))
        return false;

    switch_channel_info* info = AllocEventInfo<switch_channel_info>();
    info->ch = GetEntityHandle();
    info->secs = ecs::PlayerRuntime::CanWarp(GetEntityHandle()) && !IsPosition(POS_FIGHTING) ? 3 : 10;
    info->newAddr = newAddr;
    info->newPort = newPort;

    ecs::PlayerRuntime::SetCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Timed,
        event_create(switch_channel, info, 1));
    return true;
}
#endif

#ifdef ENABLE_BLOCK_MULTIFARM
#endif

#ifdef __HIDE_COSTUME_SYSTEM__
#ifdef ENABLE_ACCE_SYSTEM
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
#endif
#endif

void CHARACTER::Initialize()
{
    CEntity::Initialize(ENTITY_CHARACTER);
    m_entity = entt::null;
    m_dwLegacyVID = 0;


    ecs::SocialSystem::SetNoOpenedShop(GetEntityHandle(), true);
#ifdef ENABLE_EVENT_MANAGER
#endif

    m_dwPlayerID = 0;
#ifdef __SEND_TARGET_INFO__
    dwLastTargetInfoPulse = 0;
#endif
    m_iMoveCount = 0;

    m_pkRegen = nullptr;
    regen_id_ = 0;
    m_posRegen.x = m_posRegen.y = m_posRegen.z = 0;
    // Phase C.3: legacy destination zero-init removed (entity null at this
    // Initialize point - ECS write would no-op anyway; new MovementDestination
    // is absent until Goto/Move emplaces).
    m_fRegenAngle = 0.0f;

    m_pkMobData = nullptr;

    m_pkParty = nullptr;
    m_pkPartyRequestEvent = nullptr;

    m_pGuild = nullptr;


    m_pkMuyeongEvent = nullptr;
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
    m_pkGyeongGongEvent = nullptr;
#endif

#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
#endif






    m_bCharType = CHAR_TYPE_MONSTER;

    SetPosition(POS_STANDING);

    m_dwPlayStartTime = m_dwLastMoveTime = get_dword_time();

    EnterIdleState(GetEntityHandle());


    // Phase C.4: legacy m_bAddChrState zero-init removed (entity null at
    // this Initialize point - ECS StatusFlags created with default-zero
    // bits when this CHARACTER is later attached to an ECS entity).
#if defined(BL_OFFLINE_MESSAGE)
    dwLastOfflinePMTime = 0;
#endif

    m_bMountInventoryLoaded = false;

    m_iMallLoadTime = 0;






    // Phase C.2: legacy m_dwMoveStartTime / m_dwMoveDuration zero-init
    // removed. The ECS MovementState component is created by EntityFactory
    // with default-zero timing fields when this CHARACTER is later attached
    // to an ECS entity. m_entity is entt::null at this Initialize point so
    // an ECS write here would be a no-op anyway.





    m_bItemLoaded = false;

    m_iEventAttr = 0;


    // Phase C.2: legacy m_bNowWalking zero-init removed (ECS MovementState
    // default-init handles isNowWalking=false and walkPreference=false).
    CombatSystem::ResetChangeAttackPositionTime(GetEntityHandle());

    m_bDetailLog = false;
    m_bMonsterLog = false;

    m_bDisableCooltime = false;




    m_dwPolymorphRace = 0;

    m_bStaminaConsume = false;

    ResetChainLightningIndex();

    m_dwMountVnum = 0;
    m_chRider = nullptr;

#ifdef ENABLE_FAKE_SHOP_HEADER
    m_lastBeltMountCount = -999;
#endif
    ResetStopTime();
#ifdef ENABLE_GAYA_SYSTEM
    GayaSystem::Load(GetEntityHandle());
#endif



    m_dwLoginPlayTime = 0;










    m_strNewName = "";


    m_dwLogOffInterval = 0;

    m_bComboSequence = 0;
    m_dwLastComboTime = 0;
    m_bComboIndex = 0;
    m_iComboHackCount = 0;

    m_dwMountTime = 0;

    m_dwLastGoldDropTime = 0;
#ifdef ENABLE_NEWSTUFF
#endif


#ifdef __PET_SYSTEM__
    m_petSystem = nullptr;
#endif

#ifdef __NEWPET_SYSTEM__
    m_newpetSystem = nullptr;
    m_eggvid = 0;
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    m_mountSystem = nullptr;
#endif

#ifdef ENABLE_ANTI_CMD_FLOOD
#endif
    m_iSyncHackCount = 0;

#ifdef ENABLE_BATTLE_PASS
    ecs::PlayerRuntime::GetBattlePassMissions(GetEntityHandle()).clear();
    ecs::PlayerRuntime::SetBattlePassLoaded(GetEntityHandle(), false);


#endif
    m_stName = "";


#ifdef __HIDE_COSTUME_SYSTEM__
#ifdef ENABLE_ACCE_SYSTEM
#endif
#endif
#ifdef ENABLE_NEW_PET_EDITS
#endif
#ifdef KASMIR_PAKET_SYSTEM
    ecs::SocialSystem::SetKasmirPaket(GetEntityHandle(), false);
#endif
    m_iGoToXYTime = 0;
#ifdef ENABLE_SAVEPOINT_SYSTEM
    m_iSavePointTime = 0;
#endif
#ifdef ENABLE_SORT_INVEN
    m_iSortInv1Time = 0;
    m_iSortInv2Time = 0;
#endif
#ifdef ENABLE_LIMIT_BUY_SPEED
#endif
#ifdef __DUNGEON_INFO_SYSTEM__
    dungeonDamage.clear();
#endif
#ifdef ENABLE_SPAM_CHECK
    m_iLastUnlock = 0;
#endif
#ifdef ENABLE_ANTICHEAT
    m_firstReward = 0;
    m_rewardCount = 0;
    m_checkRepeated = 0;
    m_dropitemcount = 0;
    m_lastdropitem = 0;
#endif
#ifdef ENABLE_BLOCK_MULTIFARM
#endif
}

uint32_t CHARACTER::GetLegacyVID() const
{
    if (m_dwLegacyVID != 0) {
        return m_dwLegacyVID;
    }

    const entt::entity e = m_entity != entt::null ? m_entity : GetEntityHandle();
    if (e != entt::null) {
        if (const auto* vid = g_registry.try_get<ecs::VIDComponent>(e)) {
            return vid->value;
        }
    }

    return 0;
}

uint32_t CHARACTER::GetPacketVID() const
{
    return GetLegacyVID();
}

void CHARACTER::Create(const char* c_pszName, uint32_t vid, bool isPC)
{
    m_dwLegacyVID = vid;
    if (isPC)
        m_stName = c_pszName;
}


CHARACTER::CHARACTER()
{
    Initialize();
}

CHARACTER::~CHARACTER()
{
    Destroy();
}

EVENTFUNC(kill_ore_load_event)
{
    char_event_info* info = dynamic_cast<char_event_info*>(event->info);
    if (info == nullptr)
    {
        LOG_ERROR("kill_ore_load_even> <Factor> Null pointer");
        return 0;
    }

    LPCHARACTER ch = ecs::LegacyCharOf(info->ch);
    if (ch == nullptr) {
        return 0;
    }


    ecs::PlayerRuntime::SetCharEvent(
        info->ch, ecs::PlayerRuntime::CharEvent::Mining, nullptr);
    M2_DESTROY_CHARACTER(ch);
    return 0;
}

ESex GET_SEX(LPCHARACTER ch)
{
    switch (ch->GetRaceNum())
    {
    case MAIN_RACE_WARRIOR_M:
    case MAIN_RACE_SURA_M:
    case MAIN_RACE_ASSASSIN_M:
    case MAIN_RACE_SHAMAN_M:
        return SEX_MALE;

    case MAIN_RACE_ASSASSIN_W:
    case MAIN_RACE_SHAMAN_W:
    case MAIN_RACE_WARRIOR_W:
    case MAIN_RACE_SURA_W:
        return SEX_FEMALE;
    }

    return SEX_MALE;
}

EVENTFUNC(destroy_when_idle_event)
{
    const auto info = dynamic_cast<char_event_info*>(event->info);
    if (info == nullptr)
    {
        LOG_ERROR("destroy_when_idle_event> <Factor> Null pointer");
        return 0;
    }

    LPCHARACTER ch = ecs::LegacyCharOf(info->ch);
    if (ch == nullptr) {
        return 0;
    }

    // Phase 10: WRITES_STATE - deferred until ECS component covers m_pkDestroyWhenIdleEvent

    if (CombatSystem::GetVictim(info->ch) != entt::null)
    {
        return PASSES_PER_SEC(300);
    }

    LOG_INFO("DESTROY_WHEN_IDLE: {}", ch->GetName());

    ecs::PlayerRuntime::SetCharEvent(
        info->ch, ecs::PlayerRuntime::CharEvent::DestroyWhenIdle, nullptr);
    M2_DESTROY_CHARACTER(ch);
    return 0;
}

#ifdef ENABLE_BLOCK_MULTIFARM
namespace ecs::PlayerRuntime {

// The antifarm drop block. The five second countdown lives in CharEvent::Drop;
// CHARACTER::m_pkDropEvent held it before, which is why drop_event could not
// clear its own slot and said so in a comment.
void BlockProcessed(entt::entity e)
{
    if (!GetCharEvent(e, CharEvent::Drop))
    {
        LOG_ERROR("<drop_event> process failed, event is null.");
        return;
    }

#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 42, "");
#endif
    CancelCharEvent(e, CharEvent::Drop);
    LOG_INFO("<drop_event> processed.");
}

namespace {

// BlockDrop and UnblockDrop were the same body twice over, differing only in
// the flag they hand the event and in BlockDrop's IsPC test.
void StartDropStatusChange(entt::entity e, bool drop)
{
    const int32_t map = GetMapIndex(e);
    if (map != 358 && map != 359 && map != 360 && map != 361)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 36, "");
#endif
        return;
    }

    if (GetCharEvent(e, CharEvent::Drop))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 44, "");
#endif
        return;
    }

    drop_event_info* info = AllocEventInfo<drop_event_info>();
    info->ch = e;
    info->time = get_global_time() + 5;
    info->drop = drop;
    SetCharEvent(e, CharEvent::Drop, event_create(drop_event, info, PASSES_PER_SEC(1)));
#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 43, "%d", 5);
#endif
}

} // namespace

void BlockDrop(entt::entity e)
{
    if (!IsPC(e))
        return;

    StartDropStatusChange(e, false);
}

void UnblockDrop(entt::entity e)
{
    StartDropStatusChange(e, true);
}

// The account's antifarm row decides which of the two affects the character
// carries. Read at login, and again whenever the row changes.
void SetDropStatus(entt::entity e)
{
    if (!IsPC(e))
        return;

    LPDESC desc = GetDesc(e);
    if (!desc)
        return;

    std::string login = desc->GetAccountTable().login;
    std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
        "SELECT status FROM account.antifarm WHERE login='%s'", login.c_str()));
    if (msg->Get()->uiNumRows == 0)
        return;

    MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
    if (atoi(row[0]) == 1)
    {
        AffectSystem::RemoveAffect(e, AFFECT_DROP_BLOCK);
        AffectSystem::AddAffect(e, AFFECT_DROP_UNBLOCK, APPLY_NONE, 0, 0, 31536000, 0, true, false);
    }
    else
    {
        AffectSystem::RemoveAffect(e, AFFECT_DROP_UNBLOCK);
        AffectSystem::AddAffect(e, AFFECT_DROP_BLOCK, APPLY_NONE, 0, 0, 31536000, 0, true, false);
    }
}

} // namespace ecs::PlayerRuntime

EVENTFUNC(drop_event)
{
    drop_event_info* info = dynamic_cast<drop_event_info*>(event->info);
    if (!info) {
        LOG_ERROR("<drop_event> event is null.");
        return 0;
    }

    LPCHARACTER ch = ecs::LegacyCharOf(info->ch);
    if (!ch) {
        LOG_ERROR("<drop_event> ch is null.");
        return 0;
    }
	const entt::entity character = info->ch;

    LPDESC d = ecs::PlayerRuntime::GetDesc(character);
    if (!d) {
        LOG_ERROR("<drop_event> {} have no desc connector.", ch->GetName());
        return 0;
    }

    time_t diff = info->time - get_global_time();
    if (diff > 0) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 43, "%d", diff);
#endif
    }
    else {
        std::string login = ecs::PlayerRuntime::GetDesc(character)->GetAccountTable().login;
        std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT status FROM account.antifarm WHERE login='%s'", login.c_str()));
        if (msg->Get()->uiNumRows > 0) {
            MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
            int iStatus = atoi(row[0]);
            bool already = false;
            if (info->drop) {
                if (iStatus == 1) {
                    already = true;
#ifdef TEXTS_IMPROVEMENT
                    ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 38, "");
#endif
                }
                else {
                    int c = 0;
                    std::unique_ptr<SQLMsg> msg2(DBManager::instance().DirectQuery("SELECT COUNT(*) FROM account.antifarm WHERE hwid='%s' and status=1", d->GetHwid()));
                    if (msg2->Get()->uiNumRows > 0) {
                        MYSQL_ROW row2 = mysql_fetch_row(msg2->Get()->pSQLResult);
                        c = atoi(row2[0]);
                    }

                    if (c >= 2) {
                        already = true;
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 37, "");
#endif
                    }
                    else {
                        AffectSystem::RemoveAffect(character, AFFECT_DROP_BLOCK);
                        AffectSystem::AddAffect(character, AFFECT_DROP_UNBLOCK, APPLY_NONE, 0, 0, 31536000, 0, true, false);
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 40, "");
#endif
                    }
                }
            }
            else {
                if (iStatus == 0) {
                    already = true;
#ifdef TEXTS_IMPROVEMENT
                    ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 39, "");
#endif
                }
                else {
                    AffectSystem::RemoveAffect(character, AFFECT_DROP_UNBLOCK);
                    AffectSystem::AddAffect(character, AFFECT_DROP_BLOCK, APPLY_NONE, 0, 0, 31536000, 0, true, false);
#ifdef TEXTS_IMPROVEMENT
                    ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 41, "");
#endif
                }
            }

            if (!already) {
                iStatus = iStatus == 1 ? 0 : 1;
                std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery("UPDATE account.antifarm SET status=%d WHERE login='%s'", iStatus, login.c_str()));
            }
        }

        ecs::PlayerRuntime::BlockProcessed(character);
    }

    return PASSES_PER_SEC(1);
}
#endif


// The wheel counters are quest flags; they moved with them.
