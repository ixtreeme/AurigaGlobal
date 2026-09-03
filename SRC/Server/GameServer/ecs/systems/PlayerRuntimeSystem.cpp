#include "../../stdafx.h"
#include "AffectSystem.hpp"
#include "ActivitySystem.hpp"
#include "ChatSystem.hpp"
#include "PointSystem.hpp"
#include "SocialSystem.hpp"

#include "PlayerRuntimeSystem.hpp"
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
#include "../../PetSystem.h"
#include "../../party.h"
#include "../../questmanager.h"
#include "../../regen.h"
#include "../../safebox.h"
#include "../../shop.h"
#include "../../shop_manager.h"
#include "../../start_position.h"
#include "../../skill_power.h"
#include "../../target.h"
#include "../../war_map.h"
#include "../../wedding.h"
#include "../../DragonSoul.h"

extern bool RaceToJob(unsigned race, unsigned* ret_job);

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
	LPCHARACTER legacyExcept = nullptr;
	if (except != entt::null && g_registry.valid(except))
	{
		if (const auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(except))
			legacyExcept = legacy->ptr;
	}

	LPCHARACTER found = CHARACTER_MANAGER::instance().FindSpecifyPC(
		jobFlag, mapIndex, legacyExcept, minLevel, maxLevel);
	return found ? found->GetEntityHandle() : entt::null;
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
        *p = ev;
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

	if (const auto* exchange = g_registry.try_get<ecs::ExchangeRef>(e); exchange && exchange->exchange)
		return false;

	const auto* shop = g_registry.try_get<ecs::ShopState>(e);
	if (shop && (shop->currentShop || shop->myShop || shop->shopOwner != entt::null || shop->underRefine))
		return false;

	if (const auto* safebox = g_registry.try_get<ecs::SafeboxRef>(e); safebox && safebox->isOpening)
		return false;

	if (const auto* cube = g_registry.try_get<ecs::CubeWindowComponent>(e); cube && cube->pNpc)
		return false;

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (const auto* attr = g_registry.try_get<ecs::AttrTransferWindowComponent>(e); attr && attr->pNpc)
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

bool SetRace(entt::entity e, uint8_t race)
{
	if (e == entt::null || !g_registry.valid(e) || race >= MAIN_RACE_MAX_NUM)
		return false;

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
	if (!skipPersistence)
		ecs::QuestSystem::SetFlag(e, questFlag, hidden ? 1 : 0);
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

	const auto* exchange = g_registry.try_get<ecs::ExchangeRef>(e);
	const auto* shop = g_registry.try_get<ecs::ShopState>(e);
	const auto* safebox = g_registry.try_get<ecs::SafeboxRef>(e);
	const auto* cube = g_registry.try_get<ecs::CubeWindowComponent>(e);
	const bool activeWindow = (exchange && exchange->exchange) ||
		(shop && (shop->myShop || (checkShopOwner && shop->shopOwner != entt::null))) ||
		(safebox && safebox->isOpening) || (cube && cube->pNpc)
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
#ifdef ENABLE_WOLFMAN_CHARACTER
	case MAIN_RACE_WOLFMAN_M: targetRace = MAIN_RACE_WOLFMAN_M; break;
#endif
	default: return false;
	}

	return SetRace(e, targetRace);
}

int GetDuelOption(entt::entity e, const char* option)
{
    if (e == entt::null || !g_registry.valid(e) || !option)
        return 0;
    const auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(e);
    return legacy && legacy->ptr ? legacy->ptr->GetDuel(option) : 0;
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

    // Compatibility boundary until CHARACTER's duplicate quest context is removed.
    if (const auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(e); legacy && legacy->ptr)
        legacy->ptr->SetQuestNPCID(id);

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
    // Compatibility boundary: CHARACTER_MANAGER still owns legacy object lifetime.
    if (e == entt::null || !g_registry.valid(e))
        return;
    if (const auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(e); legacy && legacy->ptr)
        M2_DESTROY_CHARACTER(legacy->ptr);
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
	return character ? character->GetBattlePassId() : 0;
}

uint32_t GetMissionProgress(entt::entity e, uint32_t missionID, uint32_t battlePassID)
{
	LPCHARACTER character = LegacyCharOf(e);
	return character ? character->GetMissionProgress(missionID, battlePassID) : 0;
}

bool UpdateMissionProgress(entt::entity e, uint32_t missionID, uint32_t battlePassID,
	uint32_t updateValue, uint32_t totalValue, bool overrideValue)
{
	LPCHARACTER character = LegacyCharOf(e);
	if (!character)
		return false;

	character->UpdateMissionProgress(
		missionID, battlePassID, updateValue, totalValue, overrideValue);
	return true;
}
#endif

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

#ifdef ENABLE_PVP_ADVANCED
int GetDuelImpl(entt::entity e, const char* type)
{
    const char* szTableStaticPvP[] = { BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };

    int m_nDuelTable[] = { (ecs::QuestSystem::GetFlag(e, szTableStaticPvP[0])), (ecs::QuestSystem::GetFlag(e, szTableStaticPvP[1])), (ecs::QuestSystem::GetFlag(e, szTableStaticPvP[2])), (ecs::QuestSystem::GetFlag(e, szTableStaticPvP[3])), (ecs::QuestSystem::GetFlag(e, szTableStaticPvP[4])), (ecs::QuestSystem::GetFlag(e, szTableStaticPvP[5])), (ecs::QuestSystem::GetFlag(e, szTableStaticPvP[6])), (ecs::QuestSystem::GetFlag(e, szTableStaticPvP[7])), (ecs::QuestSystem::GetFlag(e, szTableStaticPvP[8])), (ecs::QuestSystem::GetFlag(e, szTableStaticPvP[9])) };

    if (!strcmp(type, "BlockChangeItem") && m_nDuelTable[0] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockBuff") && m_nDuelTable[1] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockPotion") && m_nDuelTable[2] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockRide") && m_nDuelTable[3] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockPet") && m_nDuelTable[4] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockPoly") && m_nDuelTable[5] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockParty") && m_nDuelTable[6] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockExchange") && m_nDuelTable[7] > 0) {
        return true;
    }
    if (!strcmp(type, "BetMoney") && m_nDuelTable[8] > 0) {
        return true;
    }
    if (!strcmp(type, "IsFight") && m_nDuelTable[9] > 0) {
        return true;
    }
    return false;
}

void SetDuelImpl(entt::entity e, const char* type, int value)
{
    const char* szTableStaticPvP[] = { BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };

    if (!strcmp(type, "BlockChangeItem")) {
        ecs::QuestSystem::SetFlag(e, szTableStaticPvP[0], value);
    }
    if (!strcmp(type, "BlockBuff")) {
        ecs::QuestSystem::SetFlag(e, szTableStaticPvP[1], value);
    }
    if (!strcmp(type, "BlockPotion")) {
        ecs::QuestSystem::SetFlag(e, szTableStaticPvP[2], value);
    }
    if (!strcmp(type, "BlockRide")) {
        ecs::QuestSystem::SetFlag(e, szTableStaticPvP[3], value);
    }
    if (!strcmp(type, "BlockPet")) {
        ecs::QuestSystem::SetFlag(e, szTableStaticPvP[4], value);
    }
    if (!strcmp(type, "BlockPoly")) {
        ecs::QuestSystem::SetFlag(e, szTableStaticPvP[5], value);
    }
    if (!strcmp(type, "BlockParty")) {
        ecs::QuestSystem::SetFlag(e, szTableStaticPvP[6], value);
    }
    if (!strcmp(type, "BlockExchange")) {
        ecs::QuestSystem::SetFlag(e, szTableStaticPvP[7], value);
    }
    if (!strcmp(type, "BetMoney")) {
        ecs::QuestSystem::SetFlag(e, szTableStaticPvP[8], value);
    }
    if (!strcmp(type, "IsFight")) {
        ecs::QuestSystem::SetFlag(e, szTableStaticPvP[9], value);
    }
}
#endif
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

uint64_t CHARACTER::GetQuestDamage(int race)
{
    return ecs::PlayerRuntime::GetQuestDamage(GetEntityHandle(), race);
}
#endif

#ifdef ENABLE_ANTICHEAT
void CHARACTER::ProcessCheatCheck(int32_t time)
{
    if (GetGMLevel() == GM_PLAYER)
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

		const auto* target = g_registry.try_get<ecs::RaceComponent>(entity);
		m_points.job = target ? static_cast<uint8_t>(target->value) : m_points.job;
		LOG_INFO("CHANGE_SEX: {} ({} -> {})", GetName(), static_cast<int>(sourceRace),
			static_cast<int>(m_points.job));
		return true;
	}

    int src_race = GetRaceNum();

    switch (src_race)
    {
    case MAIN_RACE_WARRIOR_M:
        m_points.job = MAIN_RACE_WARRIOR_W;
        break;

    case MAIN_RACE_WARRIOR_W:
        m_points.job = MAIN_RACE_WARRIOR_M;
        break;

    case MAIN_RACE_ASSASSIN_M:
        m_points.job = MAIN_RACE_ASSASSIN_W;
        break;

    case MAIN_RACE_ASSASSIN_W:
        m_points.job = MAIN_RACE_ASSASSIN_M;
        break;

    case MAIN_RACE_SURA_M:
        m_points.job = MAIN_RACE_SURA_W;
        break;

    case MAIN_RACE_SURA_W:
        m_points.job = MAIN_RACE_SURA_M;
        break;

    case MAIN_RACE_SHAMAN_M:
        m_points.job = MAIN_RACE_SHAMAN_W;
        break;

    case MAIN_RACE_SHAMAN_W:
        m_points.job = MAIN_RACE_SHAMAN_M;
        break;
#ifdef ENABLE_WOLFMAN_CHARACTER
    case MAIN_RACE_WOLFMAN_M:
        m_points.job = MAIN_RACE_WOLFMAN_M;
        break;
#endif
    default:
        LOG_ERROR("CHANGE_SEX: {} unknown race {}", GetName(), static_cast<int>(src_race));
        return false;
    }

    LOG_INFO("CHANGE_SEX: {} ({} -> {})", GetName(), static_cast<int>(src_race), static_cast<int>(m_points.job));
    return true;
}

uint16_t CHARACTER::GetRaceNum() const
{
    if (m_dwPolymorphRace)
        return m_dwPolymorphRace;

    if (m_pkMobData)
        return m_pkMobData->m_table.dwVnum;

    return m_points.job;
}

void CHARACTER::SetRace(uint8_t race)
{
    if (race >= MAIN_RACE_MAX_NUM)
    {
        LOG_ERROR("CHARACTER::SetRace(name={}, race={}).OUT_OF_RACE_RANGE", GetName(), static_cast<int>(race));
        return;
    }

	m_points.job = race;
	ecs::PlayerRuntime::SetRace(GetEntityHandle(), race);
}

uint8_t CHARACTER::GetJob() const
{
	return ecs::PlayerRuntime::GetJob(GetEntityHandle());
}

void CHARACTER::SetLevel(uint8_t level)
{
    if (auto* ecsLevel = EnsureLevelComponent(GetEntityHandle()))
        ecsLevel->value = level;

    if (IsPC())
    {
        if (level < PK_PROTECT_LEVEL)
            SetPKMode(PK_MODE_PROTECT);
        else if (GetGMLevel() != GM_PLAYER)
            SetPKMode(PK_MODE_PROTECT);
        else if (GetPKMode() == PK_MODE_PROTECT)
            SetPKMode(PK_MODE_PEACE);
    }
}

int CHARACTER::GetLevel() const
{
    if (const auto* ecsLevel = TryGetLevelComponent(GetEntityHandle()))
        return ecsLevel->value;

    return 0;
}

uint32_t CHARACTER::GetExp() const
{
    if (const auto* exp = TryGetExperienceComponent(GetEntityHandle()))
        return static_cast<uint32_t>(std::clamp<int64_t>(exp->current, 0, UINT32_MAX));

    return 0;
}

void CHARACTER::SetExp(uint32_t exp)
{
    if (auto* ecsExp = EnsureExperienceComponent(GetEntityHandle()))
        ecsExp->current = exp;
}

int64_t CHARACTER::GetGold() const
{
    if (const auto* gold = TryGetGoldAmountComponent(GetEntityHandle()))
        return gold->amount;

    return 0;
}

void CHARACTER::SetGold(int64_t gold)
{
    if (auto* wallet = EnsureGoldAmountComponent(GetEntityHandle()))
        wallet->amount = gold;
}

void CHARACTER::SetEmpire(uint8_t bEmpire)
{
    m_bEmpire = bEmpire;
	ecs::PlayerRuntime::SetEmpire(GetEntityHandle(), bEmpire);
}

uint8_t CHARACTER::GetEmpire() const
{
	return ecs::PlayerRuntime::GetEmpire(GetEntityHandle());
}

uint8_t CHARACTER::GetCharType() const
{
    return m_bCharType;
}

uint32_t CHARACTER::GetAIFlag() const
{
    if (const auto* flags = TryGetRuntimeFlagsComponent(GetEntityHandle()))
        return flags->aiFlag;

    return 0;
}

void CHARACTER::SetHP(int64_t hp)
{
    if (auto* health = EnsureHealthComponent(GetEntityHandle()))
        health->current = static_cast<int32_t>(std::clamp<int64_t>(hp, 0, INT32_MAX));
}

int64_t CHARACTER::GetHP() const
{
    if (const auto* health = TryGetHealthComponent(GetEntityHandle()))
        return health->current;

    return 0;
}

void CHARACTER::SetSP(int64_t sp)
{
    if (auto* mana = EnsureManaComponent(GetEntityHandle()))
        mana->current = static_cast<int32_t>(std::clamp<int64_t>(sp, 0, INT32_MAX));
}

int64_t CHARACTER::GetSP() const
{
    if (const auto* mana = TryGetManaComponent(GetEntityHandle()))
        return mana->current;

    return 0;
}

void CHARACTER::SetStamina(int stamina)
{
    if (auto* staminaComp = EnsureStaminaComponent(GetEntityHandle()))
        staminaComp->current = static_cast<int32_t>(std::clamp<int64_t>(stamina, 0, INT32_MAX));
}

int CHARACTER::GetStamina() const
{
    if (const auto* stamina = TryGetStaminaComponent(GetEntityHandle()))
        return stamina->current;

    return 0;
}

int32_t CHARACTER::GetInstantFlag() const
{
    if (const auto* flags = TryGetRuntimeFlagsComponent(GetEntityHandle()))
        return flags->instantFlag;

    return 0;
}

uint32_t CHARACTER::GetLastShoutPulse() const
{
    if (const auto* flags = TryGetRuntimeFlagsComponent(GetEntityHandle()))
        return flags->lastShoutPulse;

    return 0;
}

void CHARACTER::SetLastShoutPulse(uint32_t pulse)
{
    if (auto* flags = EnsureRuntimeFlagsComponent(GetEntityHandle()))
        flags->lastShoutPulse = pulse;
}

uint8_t CHARACTER::GetGMLevel() const
{
    if (test_server)
        return GM_IMPLEMENTOR;

    if (const auto* flags = TryGetRuntimeFlagsComponent(GetEntityHandle()))
        return flags->gmLevel;

    return GM_PLAYER;
}

void CHARACTER::SetGMLevel()
{
	ecs::PlayerRuntime::RefreshGMLevel(GetEntityHandle());
}

BOOL CHARACTER::IsGM() const
{
    if (GetGMLevel() != GM_PLAYER)
        return true;

    return test_server ? true : false;
}

uint32_t CHARACTER::GetAID() const
{
    char szQuery[1024 + 1];
    uint32_t dwAID = 0;

    snprintf(szQuery, sizeof(szQuery), "SELECT id FROM player_index%s WHERE pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u OR pid5=%u AND empire=%u",
        get_table_postfix(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetEmpire());

    std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(szQuery));
    if (msg->Get()->uiNumRows == 0)
        return 0;

    MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
    str_to_number(dwAID, row[0]);
    return dwAID;
}

void CHARACTER::SetQuestNPCID(uint32_t vid)
{
    m_dwQuestNPCVID = vid;
    const entt::entity owner = GetEntityHandle();
    if (owner != entt::null && g_registry.valid(owner))
    {
        auto& context = g_registry.get_or_emplace<ecs::QuestContext>(owner);
        context.npcVID = vid;
    }
}

const std::string CHARACTER::GetNewName() const
{
	return std::string(ecs::PlayerRuntime::GetPendingName(GetEntityHandle()));
}

void CHARACTER::SetNewName(const std::string name)
{
	m_strNewName = name;
	ecs::PlayerRuntime::SetPendingName(GetEntityHandle(), name);
}

LPCHARACTER CHARACTER::GetQuestNPC() const
{
    return CHARACTER_MANAGER::instance().Find(m_dwQuestNPCVID);
}

void CHARACTER::SetQuestBy(uint32_t questVnum)
{
    m_dwQuestByVnum = questVnum;
    ecs::PlayerRuntime::SetQuestBy(GetEntityHandle(), questVnum);
}

uint32_t CHARACTER::GetQuestBy() const
{
    const entt::entity self = GetEntityHandle();
    return self != entt::null && g_registry.valid(self)
        ? ecs::PlayerRuntime::GetQuestBy(self)
        : m_dwQuestByVnum;
}

void CHARACTER::SetQuestItemPtr(entt::entity item)
{
	const entt::entity owner = GetEntityHandle();
	if (owner == entt::null || !g_registry.valid(owner))
		return;

	auto& context = g_registry.get_or_emplace<ecs::QuestContext>(owner);
	context.questItem = ItemSystem::IsValidItem(item) ? item : entt::null;
}

void CHARACTER::ClearQuestItemPtr()
{
	SetQuestItemPtr(entt::null);
}

entt::entity CHARACTER::GetQuestItemEntity() const
{
	const entt::entity owner = GetEntityHandle();
	if (owner == entt::null || !g_registry.valid(owner))
		return entt::null;

	const auto* context = g_registry.try_get<ecs::QuestContext>(owner);
	return context && ItemSystem::IsValidItem(context->questItem) ? context->questItem : entt::null;
}

LPITEM CHARACTER::GetQuestItemPtr() const
{
	return ResolveLegacyItem(GetQuestItemEntity());
}

LPDUNGEON CHARACTER::GetDungeonForce() const
{
    if (m_lWarpMapIndex > 10000)
        return CDungeonManager::instance().FindByMapIndex(m_lWarpMapIndex);

    return m_pkDungeon;
}

void CHARACTER::SetBlockMode(uint8_t bFlag)
{
    if (auto* flags = EnsureRuntimeFlagsComponent(GetEntityHandle()))
        flags->blockMode = bFlag;

    ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_COMMAND, "setblockmode %d", bFlag);

    SetQuestFlag("game_option.block_exchange", bFlag & BLOCK_EXCHANGE ? 1 : 0);
    SetQuestFlag("game_option.block_party_invite", bFlag & BLOCK_PARTY_INVITE ? 1 : 0);
    SetQuestFlag("game_option.block_guild_invite", bFlag & BLOCK_GUILD_INVITE ? 1 : 0);
    SetQuestFlag("game_option.block_whisper", bFlag & BLOCK_WHISPER ? 1 : 0);
    SetQuestFlag("game_option.block_messenger_invite", bFlag & BLOCK_MESSENGER_INVITE ? 1 : 0);
    SetQuestFlag("game_option.block_party_request", bFlag & BLOCK_PARTY_REQUEST ? 1 : 0);
}

void CHARACTER::SetBlockModeForce(uint8_t bFlag)
{
	ecs::PlayerRuntime::SetBlockModeForce(GetEntityHandle(), bFlag);
}

uint8_t CHARACTER::GetBlockMode() const
{
    if (const auto* flags = TryGetRuntimeFlagsComponent(GetEntityHandle()))
        return flags->blockMode;

    return 0;
}

bool CHARACTER::IsBlockMode(uint8_t bFlag) const
{
    return (GetBlockMode() & bFlag) != 0;
}

void CHARACTER::SetImmuneFlag(uint32_t dw)
{
    if (auto* flags = EnsureRuntimeFlagsComponent(GetEntityHandle()))
        flags->immuneFlag = dw;
    auto& immunity = g_registry.get_or_emplace<ecs::ImmunityFlags>(GetEntityHandle());
    immunity.flags = dw;
}

uint32_t CHARACTER::GetImmuneFlag() const
{
    if (const auto* flags = TryGetRuntimeFlagsComponent(GetEntityHandle()))
        return flags->immuneFlag;

    return 0;
}

void CHARACTER::SetPotionLimit(int count)
{
    ecs::PlayerRuntime::SetPotionLimit(GetEntityHandle(), count);
}

int CHARACTER::GetPotionLimit() const
{
    return ecs::PlayerRuntime::GetPotionLimit(GetEntityHandle());
}

// The pet / mount creature markers. These write both stores at one point so
// they cannot drift: the legacy bit that EncodeInsertPacket reads, and the
// StatusFlags bit the native character-insert builder reads. Before this they
// had no ECS writer at all, so the native builder's pet and mount detection
// was dead while legacy's worked.
void CHARACTER::SetPet()
{
    m_bIsPet = true;
    if (auto* status = g_registry.try_get<ecs::StatusFlags>(GetEntityHandle()))
        status->isPet = true;
}

void CHARACTER::SetMount()
{
    m_bIsMount = true;
    if (auto* status = g_registry.try_get<ecs::StatusFlags>(GetEntityHandle()))
        status->isMount = true;
}

void CHARACTER::SetNewPet()
{
    m_bIsNewPet = true;
    if (auto* status = g_registry.try_get<ecs::StatusFlags>(GetEntityHandle()))
        status->isNewPet = true;
}

bool CHARACTER::IsGuardNPC() const
{
    return ecs::PlayerRuntime::IsGuardNPC(GetEntityHandle());
}

int CHARACTER::GetQuestFlag(const std::string& flag) const
{
    int ret = 0;
    quest::CQuestManager& q = quest::CQuestManager::instance();
    quest::PC* pPC = q.GetPC(GetPlayerID());
    if (pPC)
        ret = pPC->GetFlag(flag);

    return ret;
}

void CHARACTER::SetQuestFlag(const std::string& flag, int value)
{
    quest::CQuestManager& q = quest::CQuestManager::instance();
    quest::PC* pPC = q.GetPC(GetPlayerID());
    pPC->SetFlag(flag, value);
}

void CHARACTER::SetItemAward_vnum(unsigned int vnum)
{
	itemAward_vnum = vnum;
	const entt::entity entity = GetEntityHandle();
	if (entity != entt::null && g_registry.valid(entity))
	{
		auto& award = g_registry.get_or_emplace<ecs::ItemAward>(entity);
		award.vnum = vnum;
		g_registry.emplace_or_replace<ecs::DirtyTag>(entity);
	}
}

void CHARACTER::SetItemAward_cmd(char* cmd)
{
	strlcpy(itemAward_cmd, cmd ? cmd : "", sizeof(itemAward_cmd));
	const entt::entity entity = GetEntityHandle();
	if (entity != entt::null && g_registry.valid(entity))
	{
		auto& award = g_registry.get_or_emplace<ecs::ItemAward>(entity);
		award.command = cmd ? cmd : "";
		g_registry.emplace_or_replace<ecs::DirtyTag>(entity);
	}
}

#ifdef ENABLE_VOTE4BUFF
long long CHARACTER::GetVoteCoin()
{
    std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT coins FROM account.account WHERE id = '%d';", GetDesc()->GetAccountTable().id));
    if (pMsg->Get()->uiNumRows == 0)
        return 0;
    MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
    long long coin = 0;
    str_to_number(coin, row[0]);
    return coin;
}

void CHARACTER::SetVoteCoin(long long amount)
{
    std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("UPDATE account.account SET coins = '%lld' WHERE id = '%d';", amount, GetDesc()->GetAccountTable().id));
}
#endif

#ifdef ENABLE_ITEMSHOP
uint32_t CHARACTER::GetDragonCoin()
{
    std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT coins FROM account.account WHERE id = '%u';", GetDesc()->GetAccountTable().id));
    if (pMsg->Get()->uiNumRows == 0)
        return 0;
    MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
    uint32_t dc = 0;
    str_to_number(dc, row[0]);
    return dc;
}

void CHARACTER::SetDragonCoin(uint32_t amount)
{
    std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("UPDATE account.account SET coins = '%lld' WHERE id = '%u';", amount, GetDesc()->GetAccountTable().id));
}

void CHARACTER::SetProtectTime(const std::string& flagname, int value)
{
    auto it = m_protection_Time.find(flagname);
    if (it != m_protection_Time.end())
        it->second = value;
    else
        m_protection_Time.insert(make_pair(flagname, value));
}

int CHARACTER::GetProtectTime(const std::string& flagname) const
{
    auto it = m_protection_Time.find(flagname);
    if (it != m_protection_Time.end())
        return it->second;
    return 0;
}
#endif

const TMobTable& CHARACTER::GetMobTable() const
{
    return m_pkMobData->m_table;
}

bool CHARACTER::IsRaceFlag(uint32_t dwBit) const
{
    return m_pkMobData ? IS_SET(m_pkMobData->m_table.dwRaceFlag, dwBit) : 0;
}

uint32_t CHARACTER::GetMobDamageMin() const
{
    return m_pkMobData->m_table.dwDamageRange[0];
}

uint32_t CHARACTER::GetMobDamageMax() const
{
    return m_pkMobData->m_table.dwDamageRange[1];
}

float CHARACTER::GetMobDamageMultiply() const
{
    float fDamMultiply = GetMobTable().fDamMultiply;

    if (IsBerserk())
        fDamMultiply = fDamMultiply * 2.0f;

    return fDamMultiply;
}

uint32_t CHARACTER::GetMobDropItemVnum() const
{
    if (!m_pkMobData)
    {
        LOG_ERROR("GetMobDropItemVnum: NULL mob data (vid={} race={} name={} map={} x={} y={})", GetPacketVID(), GetRaceNum(), GetName(), GetMapIndex(), GetX(), GetY());
        return 0;
    }

    return m_pkMobData->m_table.dwDropItemVnum;
}

bool CHARACTER::IsSummonMonster() const
{
    return GetSummonVnum() != 0;
}

uint32_t CHARACTER::GetSummonVnum() const
{
    return m_pkMobData ? m_pkMobData->m_table.dwSummonVnum : 0;
}

uint32_t CHARACTER::GetPolymorphItemVnum() const
{
    return m_pkMobData ? m_pkMobData->m_table.dwPolymorphItemVnum : 0;
}

uint32_t CHARACTER::GetMonsterDrainSPPoint() const
{
    return m_pkMobData ? m_pkMobData->m_table.dwDrainSP : 0;
}

uint8_t CHARACTER::GetMobRank() const
{
	return ecs::PlayerRuntime::GetMobRank(GetEntityHandle());
}

uint8_t CHARACTER::GetMobSize() const
{
    if (!m_pkMobData)
        return MOBSIZE_MEDIUM;

    return m_pkMobData->m_table.bSize;
}

uint16_t CHARACTER::GetMobAttackRange() const
{
    if (!m_pkMobData)
    {
        LOG_ERROR("GetMobAttackRange: m_pkMobData NULL! (VID: {}, Name: {}, Race:{})", GetPacketVID(), GetName(), GetRaceNum());
        return 0;
    }

    switch (GetMobBattleType())
    {
    case BATTLE_TYPE_RANGE:
    case BATTLE_TYPE_MAGIC:
#ifdef __DEFENSE_WAVE__
        if (GetRaceNum() == 3960 || GetRaceNum() == 3961 || GetRaceNum() == 3962)
            return m_pkMobData->m_table.wAttackRange + GetPoint(POINT_BOW_DISTANCE) + 4000;
        else
            return m_pkMobData->m_table.wAttackRange;
#else
        return m_pkMobData->m_table.wAttackRange + GetPoint(POINT_BOW_DISTANCE);
#endif

    default:
#ifdef __DEFENSE_WAVE__
        if ((GetRaceNum() <= 3955 && GetRaceNum() >= 3950 && GetRaceNum() != 3953) ||
            (GetRaceNum() <= 3605 && GetRaceNum() >= 3601 && GetRaceNum() != 3602))
            return m_pkMobData->m_table.wAttackRange + 300;
        else
            return m_pkMobData->m_table.wAttackRange;
#else
        return m_pkMobData->m_table.wAttackRange;
#endif
    }
}

uint8_t CHARACTER::GetMobBattleType() const
{
    if (!m_pkMobData)
        return BATTLE_TYPE_MELEE;

    return m_pkMobData->m_table.bBattleType;
}

void CHARACTER::ResetPlayTime(uint32_t dwTimeRemain)
{
    m_dwPlayStartTime = get_dword_time() - dwTimeRemain;
}

int CHARACTER::GetPremiumRemainSeconds(uint8_t bType) const
{
	return ecs::PlayerRuntime::GetPremiumRemainSeconds(GetEntityHandle(), bType);
}

bool CHARACTER::SetPCBang(bool flag)
{
	m_isinPCBang = flag;
	const entt::entity character = GetEntityHandle();
	if (character != entt::null && g_registry.valid(character))
	{
		auto& login = g_registry.get_or_emplace<ecs::LoginInfo>(character);
		login.isPCBang = flag;
		g_registry.emplace_or_replace<ecs::DirtyTag>(character);
	}
	return m_isinPCBang;
}

void CHARACTER::UpdateDepositPulse()
{
    m_deposit_pulse = thecore_pulse() + PASSES_PER_SEC(60 * 5);
}

bool CHARACTER::CanDeposit() const
{
    return (m_deposit_pulse == 0 || (m_deposit_pulse < thecore_pulse()));
}

uint32_t CHARACTER::GetNextExp() const
{
    if (PLAYER_MAX_LEVEL_CONST < GetLevel())
        return 2500000000u;
    else
        return exp_table[GetLevel()];
}

#ifdef __NEWPET_SYSTEM__
uint32_t CHARACTER::PetGetNextExp() const
{
    if (IsNewPet()) {
        if (120 < GetLevel())
            return 2500000000;
        else
            return exppet_table[GetLevel()];
    }
    return 0;
}
#endif

int CHARACTER::GetSkillPowerByLevel(int level, bool bMob) const
{
    return CTableBySkill::instance().GetSkillPowerByLevelFromType(GetJob(), GetSkillGroup(), MINMAX(0, level, (int)SKILL_MAX_LEVEL), bMob);
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
    if (m_dwBattlePassEndTime > 0)
        remain = (int)(m_dwBattlePassEndTime - get_global_time());

    if (remain <= 0)
    {
        remain = GetSecondsTillNextMonth();
        m_dwBattlePassEndTime = get_global_time() + remain;
    }

    if (!GetBattlePassId())
        AddAffect(AFFECT_BATTLE_PASS, POINT_BATTLE_PASS_ID, kDefaultBattlePassId, 0, remain, 0, true);
    m_bIsLoadedBattlePass = true;
}
#endif

#ifdef ENABLE_BATTLE_PASS
void CHARACTER::LoadBattlePass(uint32_t dwCount, TPlayerBattlePassMission* data)
{
    m_bIsLoadedBattlePass = false;

    for (auto it = m_listBattlePass.begin(); it != m_listBattlePass.end(); ++it)
        delete (*it);
    m_listBattlePass.clear();

    const uint8_t kDefaultBattlePassId = 1;

    int remain = 0;
    if (m_dwBattlePassEndTime > 0)
        remain = (int)(m_dwBattlePassEndTime - get_global_time());

    if (remain <= 0)
    {
        remain = GetSecondsTillNextMonth();
        m_dwBattlePassEndTime = get_global_time() + remain;
    }

    if (!GetBattlePassId())
        AddAffect(AFFECT_BATTLE_PASS, POINT_BATTLE_PASS_ID, kDefaultBattlePassId, 0, remain, 0, true);

    if (dwCount == 0 || !data)
    {
        m_bIsLoadedBattlePass = true;
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

        m_listBattlePass.push_back(newMission);
    }

    m_bIsLoadedBattlePass = true;
}

#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
void CHARACTER::CancelStayOnlineEvent()
{
    if (m_pkStayOnlineEvent)
    {
        event_cancel(&m_pkStayOnlineEvent);
        m_pkStayOnlineEvent = nullptr;
    }
}
#endif

#ifdef ENABLE_FREE_PASS_RAZOR93
bool CHARACTER::HasBattlePassBoost(uint8_t bBattlePassId)
{
    CAffect* p = FindAffect(AFFECT_BATTLE_PASS_BOOST, POINT_BATTLE_PASS_ID);
    return (p && p->lApplyValue == bBattlePassId);
}

uint32_t CHARACTER::GetBattlePassAdjustedTotal(uint32_t dwMissionID, uint32_t dwBattlePassID, uint32_t dwBaseTotal)
{
    if (dwBaseTotal <= 1)
        return dwBaseTotal;

    if (!HasBattlePassBoost((uint8_t)dwBattlePassID))
        return dwBaseTotal;

    return (dwBaseTotal + 1) / 2;
}

void CHARACTER::ApplyBattlePassBoostRecalc(uint8_t bBattlePassId)
{
    auto it = m_listBattlePass.begin();
    while (it != m_listBattlePass.end())
    {
        TPlayerBattlePassMission* m = *it++;
        if (!m || m->dwBattlePassId != bBattlePassId)
            continue;

        if (m->bCompleted)
            continue;

        uint32_t dwInfo1 = 0, dwBaseNeed = 0;
        if (!CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, (uint8_t)m->dwMissionId, &dwInfo1, &dwBaseNeed))
            continue;

        const uint32_t dwNeed = GetBattlePassAdjustedTotal(m->dwMissionId, bBattlePassId, dwBaseNeed);

        if (m->dwExtraInfo >= dwNeed)
            UpdateMissionProgress(m->dwMissionId, bBattlePassId, dwNeed, dwNeed, true);
    }
}
#endif

uint32_t CHARACTER::GetMissionProgress(uint32_t dwMissionID, uint32_t dwBattlePassID)
{
    auto it = m_listBattlePass.begin();
    while (it != m_listBattlePass.end())
    {
        TPlayerBattlePassMission* pkMission = *it++;
        if (pkMission->dwMissionId == dwMissionID && pkMission->dwBattlePassId == dwBattlePassID)
            return pkMission->dwExtraInfo;
    }

    return 0;
}

bool CHARACTER::IsCompletedMission(uint8_t bMissionType)
{
    auto it = m_listBattlePass.begin();
    while (it != m_listBattlePass.end())
    {
        TPlayerBattlePassMission* pkMission = *it++;
        if (pkMission->dwMissionId == bMissionType)
            return (pkMission->bCompleted ? true : false);
    }

    return false;
}

void CHARACTER::UpdateMissionProgress(uint32_t dwMissionID, uint32_t dwBattlePassID, uint32_t dwUpdateValue, uint32_t dwTotalValue, bool isOverride)
{
    if (!m_bIsLoadedBattlePass)
        return;
#ifdef ENABLE_FREE_PASS_RAZOR93
    dwTotalValue = GetBattlePassAdjustedTotal(dwMissionID, dwBattlePassID, dwTotalValue);
#endif
    bool foundMission = false;
    uint32_t dwSaveProgress = 0;

    auto it = m_listBattlePass.begin();
    while (it != m_listBattlePass.end())
    {
        TPlayerBattlePassMission* pkMission = *it++;

        if (pkMission->dwMissionId == dwMissionID && pkMission->dwBattlePassId == dwBattlePassID)
        {
            pkMission->bIsUpdated = 1;
#ifdef ENABLE_FREE_PASS_RAZOR93
            if (pkMission->bCompleted)
                return;
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
                    CancelStayOnlineEvent();
#endif
                CBattlePass::instance().BattlePassRewardMission(this, dwMissionID, dwBattlePassID);
            }

            dwSaveProgress = pkMission->dwExtraInfo;
            foundMission = true;
            break;
        }
    }

    if (!foundMission)
    {
        TPlayerBattlePassMission* newMission = new TPlayerBattlePassMission;
        newMission->dwPlayerId = GetPlayerID();
        newMission->dwMissionId = dwMissionID;
        newMission->dwBattlePassId = dwBattlePassID;

        if (dwUpdateValue >= dwTotalValue)
        {
            newMission->dwExtraInfo = dwTotalValue;
            newMission->bCompleted = 1;
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
            if (newMission->dwMissionId == STAY_ONLINE_MINUTES)
                CancelStayOnlineEvent();
#endif
            CBattlePass::instance().BattlePassRewardMission(this, dwMissionID, dwBattlePassID);

            dwSaveProgress = dwTotalValue;
        }
        else
        {
            newMission->dwExtraInfo = dwUpdateValue;
            newMission->bCompleted = 0;

            dwSaveProgress = dwUpdateValue;
        }

        newMission->bIsUpdated = 1;

        m_listBattlePass.push_back(newMission);
    }

    if (!GetDesc())
        return;

    TPacketGCBattlePassUpdate packet;
    packet.bHeader = HEADER_GC_BATTLE_PASS_UPDATE;
    packet.bMissionType = dwMissionID;
    packet.dwNewProgress = dwSaveProgress;
    GetDesc()->Packet(&packet, sizeof(TPacketGCBattlePassUpdate));
}

uint8_t CHARACTER::GetBattlePassId()
{
    CAffect* pAffect = FindAffect(AFFECT_BATTLE_PASS, POINT_BATTLE_PASS_ID);

    if (!pAffect)
        return 0;

    return pAffect->lApplyValue;
}

int CHARACTER::GetSecondsTillNextMonth()
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
uint16_t CHARACTER::GetRuneEffect() {
    if (!IsPC())
        return 0;

    if (GetQuestFlag("rune.hide_effect") == 1)
        return 0;

    uint16_t r = 1;
    int iMaxSubTypes = RUNE_SUBTYPES - 1;
    int32_t lMaxTime = 0;
    int32_t lOnePercent = 0;
    int32_t lRemainPercent = 0;

    for (int i = 0; i < iMaxSubTypes; i++) {
        const entt::entity item = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_RUNE1 + i);
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

    if (!IsPC() || IsPolymorphed() || pkVictim->IsPC())
        return 0;

    if (bSoulType >= SOUL_MAX_NUM)
        return 0;

    const CAffect* pAffect = FindAffect(AFFECT_SOUL_RED + bSoulType);
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
                    RemoveAffect(const_cast<CAffect*>(pAffect));
                }

                ItemSystem::SetItemSocket(soulItem, 2, 0);
                // The soul timer itself is still owned by CItem. Keep this one
                // transition visible until item events become ECS components.
                if (LPITEM legacySoulItem = ResolveLegacyItem(soulItem))
                    legacySoulItem->StartSoulItemEvent();
            }

            ItemSystem::SetItemSocket(
                soulItem, 2, iCurrentMinutes * 10000 + iNextStrikes);
        }
    }

    return iDamageAdd;
}
#endif

#ifdef __SKILL_COLOR_SYSTEM__
void CHARACTER::SetSkillColor(uint32_t* dwSkillColor) {
    memcpy(m_dwSkillColor, dwSkillColor, sizeof(m_dwSkillColor));
    if (auto* skillColor = g_registry.try_get<ecs::SkillColor>(GetEntityHandle())) {
        memcpy(skillColor->data, m_dwSkillColor, sizeof(skillColor->data));
        g_registry.emplace_or_replace<ecs::DirtyTag>(GetEntityHandle());
    }
    NetworkSyncSystem::UpdatePacket(GetEntityHandle());
}
#endif

void CHARACTER::SetShop(LPSHOP pkShop)
{
    const auto e = GetEntityHandle();
    ecs::SocialSystem::SetShop(e, pkShop);
    m_pkShop = pkShop;
    if (!pkShop)
        m_pkChrShopOwner = nullptr;
}

void CHARACTER::SetShopOwner(entt::entity chEntity)
{
    LPCHARACTER ch = ecs::LegacyCharOf(chEntity);
    const auto e = GetEntityHandle();
    ecs::SocialSystem::SetShopOwner(e, chEntity);
    m_pkChrShopOwner = ch;
}

#ifdef __ENABLE_NEW_OFFLINESHOP__
void CHARACTER::SetOfflineShopGuest(offlineshop::CShop* pkShop)
{
    const auto e = GetEntityHandle();
    if (e != entt::null && g_registry.valid(e))
    {
        auto& shop = g_registry.get_or_emplace<ecs::ShopState>(e);
        shop.offlineShopGuest = pkShop;
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    }

    m_pkOfflineShopGuest = pkShop;
}

void CHARACTER::SetAuctionGuest(offlineshop::CAuction* pk)
{
    const auto e = GetEntityHandle();
    if (e != entt::null && g_registry.valid(e))
    {
        auto& shop = g_registry.get_or_emplace<ecs::ShopState>(e);
        shop.auctionGuest = pk;
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    }

    m_pkAuctionGuest = pk;
}

void CHARACTER::SetOfflineShopUseTime()
{
    m_iOfflineShopUseTime = thecore_pulse();
    const auto e = GetEntityHandle();
    if (e != entt::null && g_registry.valid(e))
    {
        auto& shop = g_registry.get_or_emplace<ecs::ShopState>(e);
        shop.offlineShopUseTime = m_iOfflineShopUseTime;
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    }
}
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

void CHARACTER::SetExchange(CExchange* pkExchange)
{
    const auto e = GetEntityHandle();
    if (e != entt::null && g_registry.valid(e))
    {
        auto& exchange = g_registry.get_or_emplace<ecs::ExchangeRef>(e);
        exchange.exchange = pkExchange;
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    }

    m_pkExchange = pkExchange;
}

void CHARACTER::SetRegen(LPREGEN pkRegen)
{
    m_pkRegen = pkRegen;
    if (pkRegen != nullptr) {
        regen_id_ = pkRegen->id;
    }
    m_fRegenAngle = GetRotation();
    m_posRegen = GetXYZ();
}

LPCHARACTER CHARACTER::GetMarryPartner() const
{
    return m_pkChrMarried;
}

void CHARACTER::SetMarryPartner(entt::entity chEntity)
{
    LPCHARACTER ch = ecs::LegacyCharOf(chEntity);
    m_pkChrMarried = ch;
}

void CHARACTER::SetDungeon(LPDUNGEON pkDungeon)
{
    if (pkDungeon && m_pkDungeon)
    {
        LOG_ERROR("{} is trying to reassigning dungeon (current {}, new party {})", GetName(), static_cast<const void*>(get_pointer(m_pkDungeon)), static_cast<const void*>(get_pointer(pkDungeon)));
    }

    if (m_pkDungeon)
    {
        if (IsPC())
        {
            if (GetParty())
                m_pkDungeon->DecPartyMember(GetParty(), this);
            else
                m_pkDungeon->DecMember(this);
        }
    }

    m_pkDungeon = pkDungeon;

    if (pkDungeon)
    {
        if (IsPC())
        {
            if (GetParty())
                m_pkDungeon->IncPartyMember(GetParty(), this);
            else
                m_pkDungeon->IncMember(this);
        }
        else if (IsMonster() || IsStone())
        {
            m_pkDungeon->IncMonster();
        }
    }
}

void CHARACTER::SetWarMap(CWarMap* pWarMap)
{
    if (m_pWarMap)
        m_pWarMap->DecMember(this);

    m_pWarMap = pWarMap;

    if (m_pWarMap)
        m_pWarMap->IncMember(this);
}

void CHARACTER::SetWeddingMap(marriage::WeddingMap* pMap)
{
    if (m_pWeddingMap)
        m_pWeddingMap->DecMember(this);

    m_pWeddingMap = pMap;

    if (m_pWeddingMap)
        m_pWeddingMap->IncMember(this);
}

void CHARACTER::OpenAcce(bool bCombination)
{
    if (isAcceOpened(bCombination))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 659, "");
#endif
        return;
    }

    if (bCombination)
    {
        if (m_bAcceAbsorption)
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 660, "");
#endif
            return;
        }

        m_bAcceCombination = true;
        if (const auto e = GetEntityHandle(); e != entt::null && g_registry.valid(e))
        {
            auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
            acce.combinationOpen = true;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
    }
    else
    {
        if (m_bAcceCombination)
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 661, "");
#endif
            return;
        }

        m_bAcceAbsorption = true;
        if (const auto e = GetEntityHandle(); e != entt::null && g_registry.valid(e))
        {
            auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
            acce.absorptionOpen = true;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
    }

    TItemPos tPos;
    tPos.window_type = INVENTORY;
    tPos.cell = 0;

    TPacketAcce sPacket;
    sPacket.header = HEADER_GC_ACCE;
    sPacket.subheader = ACCE_SUBHEADER_GC_OPEN;
    sPacket.bWindow = bCombination;
    sPacket.dwPrice = 0;
    sPacket.bPos = 0;
    sPacket.tPos = tPos;
    sPacket.dwItemVnum = 0;
    sPacket.dwMinAbs = 0;
    sPacket.dwMaxAbs = 0;
    GetDesc()->Packet(&sPacket, sizeof(TPacketAcce));

    ClearAcceMaterials();
}

void CHARACTER::CloseAcce()
{
    if ((!m_bAcceCombination) && (!m_bAcceAbsorption))
        return;

    bool bWindow = (m_bAcceCombination == true ? true : false);

    TItemPos tPos;
    tPos.window_type = INVENTORY;
    tPos.cell = 0;

    TPacketAcce sPacket;
    sPacket.header = HEADER_GC_ACCE;
    sPacket.subheader = ACCE_SUBHEADER_GC_CLOSE;
    sPacket.bWindow = bWindow;
    sPacket.dwPrice = 0;
    sPacket.bPos = 0;
    sPacket.tPos = tPos;
    sPacket.dwItemVnum = 0;
    sPacket.dwMinAbs = 0;
    sPacket.dwMaxAbs = 0;
    GetDesc()->Packet(&sPacket, sizeof(TPacketAcce));

    if (bWindow)
        m_bAcceCombination = false;
    else
        m_bAcceAbsorption = false;

    if (const auto e = GetEntityHandle(); e != entt::null && g_registry.valid(e))
    {
        auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
        acce.combinationOpen = m_bAcceCombination;
        acce.absorptionOpen = m_bAcceAbsorption;
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    }

    ClearAcceMaterials();
}

void CHARACTER::ClearAcceMaterials()
{
	auto pkItemMaterial = GetAcceMaterials();
	for (int i = 0; i < ACCE_WINDOW_MAX_MATERIALS; ++i)
	{
		if (pkItemMaterial[i] == entt::null)
			continue;

		ItemSystem::UnlockItem(pkItemMaterial[i]);
		pkItemMaterial[i] = entt::null;
	}
}

bool CHARACTER::AcceIsSameGrade(int32_t lGrade)
{
	auto pkItemMaterial = GetAcceMaterials();
	if (pkItemMaterial[0] == entt::null)
		return false;

	bool bReturn = ItemSystem::GetItemValue(pkItemMaterial[0], ACCE_GRADE_VALUE_FIELD) == lGrade;
    return bReturn;
}

uint32_t CHARACTER::GetAcceCombinePrice(int32_t lGrade
#ifdef ENABLE_STOLE_COSTUME
    , bool isCostume
#endif
)
{
    uint32_t dwPrice;
    switch (lGrade)
    {
    case 2:
    {
#ifdef ENABLE_STOLE_COSTUME
        dwPrice = isCostume ? COSTUME_STOLE_GRADE_2_PRICE : ACCE_GRADE_2_PRICE;
#else
        dwPrice = ACCE_GRADE_2_PRICE;
#endif
    }
    break;
    case 3:
    {
#ifdef ENABLE_STOLE_COSTUME
        dwPrice = isCostume ? COSTUME_STOLE_GRADE_3_PRICE : ACCE_GRADE_3_PRICE;
#else
        dwPrice = ACCE_GRADE_2_PRICE;
#endif
    }
    break;
    case 4:
    {
#ifdef ENABLE_STOLE_COSTUME
        dwPrice = isCostume ? 0 : ACCE_GRADE_4_PRICE;
#else
        dwPrice = ACCE_GRADE_2_PRICE;
#endif
    }
    break;
    default:
    {
#ifdef ENABLE_STOLE_COSTUME
        dwPrice = isCostume ? COSTUME_STOLE_GRADE_1_PRICE : ACCE_GRADE_1_PRICE;
#else
        dwPrice = ACCE_GRADE_1_PRICE;
#endif
    }
    break;
    }

    return dwPrice;
}

uint8_t CHARACTER::CheckEmptyMaterialSlot()
{
	const auto pkItemMaterial = GetAcceMaterials();
    for (int i = 0; i < ACCE_WINDOW_MAX_MATERIALS; ++i)
    {
		if (pkItemMaterial[i] == entt::null)
            return i;
    }

    return 255;
}

void CHARACTER::GetAcceCombineResult(uint32_t& dwItemVnum, uint32_t& dwMinAbs, uint32_t& dwMaxAbs)
{
	const auto pkItemMaterial = GetAcceMaterials();

    if (m_bAcceCombination)
    {
		if (pkItemMaterial[0] != entt::null && pkItemMaterial[1] != entt::null)
		{
			int32_t lVal = ItemSystem::GetItemValue(pkItemMaterial[0], ACCE_GRADE_VALUE_FIELD);
            if (lVal == 4)
            {
				dwItemVnum = ItemSystem::GetItemOriginalVnum(pkItemMaterial[0]);
				dwMinAbs = ItemSystem::GetItemSocket(pkItemMaterial[0], ACCE_ABSORPTION_SOCKET);
                uint32_t dwMaxAbsCalc = (dwMinAbs + ACCE_GRADE_4_ABS_RANGE > ACCE_GRADE_4_ABS_MAX ? ACCE_GRADE_4_ABS_MAX : (dwMinAbs + ACCE_GRADE_4_ABS_RANGE));
                dwMaxAbs = dwMaxAbsCalc;
            }
            else
            {
				uint32_t dwMaskVnum = ItemSystem::GetItemOriginalVnum(pkItemMaterial[0]);
                TItemTable* pTable = ITEM_MANAGER::instance().GetTable(dwMaskVnum + 1);
                if (pTable)
                    dwMaskVnum += 1;

                dwItemVnum = dwMaskVnum;
                switch (lVal)
                {
                case 2:
                {
                    dwMinAbs = ACCE_GRADE_3_ABS;
                    dwMaxAbs = ACCE_GRADE_3_ABS;
                }
                break;
                case 3:
                {
                    dwMinAbs = ACCE_GRADE_4_ABS_MIN;
                    dwMaxAbs = ACCE_GRADE_4_ABS_MAX_COMB;
                }
                break;
                default:
                {
                    dwMinAbs = ACCE_GRADE_2_ABS;
                    dwMaxAbs = ACCE_GRADE_2_ABS;
                }
                break;
                }
            }
        }
        else
        {
            dwItemVnum = 0;
            dwMinAbs = 0;
            dwMaxAbs = 0;
        }
    }
    else
    {
		if (pkItemMaterial[0] != entt::null && pkItemMaterial[1] != entt::null)
		{
			dwItemVnum = ItemSystem::GetItemOriginalVnum(pkItemMaterial[0]);
			dwMinAbs = ItemSystem::GetItemSocket(pkItemMaterial[0], ACCE_ABSORPTION_SOCKET);
            dwMaxAbs = dwMinAbs;
        }
        else
        {
            dwItemVnum = 0;
            dwMinAbs = 0;
            dwMaxAbs = 0;
        }
    }
}

void CHARACTER::AddAcceMaterial(TItemPos tPos, uint8_t bPos)
{
    if (bPos >= ACCE_WINDOW_MAX_MATERIALS)
    {
        if (bPos == 255)
        {
            bPos = CheckEmptyMaterialSlot();
            if (bPos >= ACCE_WINDOW_MAX_MATERIALS)
                return;
        }
        else
            return;
    }

	const entt::entity item = ItemSystem::GetItem(GetEntityHandle(), tPos);
	if (item == entt::null)
		return;
	else if ((ItemSystem::GetItemCell(item) >= INVENTORY_MAX_NUM) || ItemSystem::IsItemEquipped(item) || tPos.IsBeltInventoryPosition() || ItemSystem::GetItemType(item) == ITEM_DS)
		return;
	else if ((ItemSystem::GetItemType(item) != ITEM_COSTUME) && (m_bAcceCombination))
		return;
	else if ((ItemSystem::GetItemType(item) != ITEM_COSTUME) && (bPos == 0) && (m_bAcceAbsorption))
		return;
	else if (ItemSystem::IsItemLocked(item))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 519, "");
#endif
        return;
    }
	else if ((ItemSystem::GetItemType(item) == ITEM_ARMOR) && (ItemSystem::GetItemSubType(item) == ARMOR_BODY))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 519, "");
#endif
        return;
    }
#ifdef __SOULBINDING_SYSTEM__
	else if (ItemSystem::IsItemBound(item))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 519, "");
#endif
        return;
    }
#endif
#ifdef ENABLE_STOLE_COSTUME
	else if (m_bAcceAbsorption && bPos == 0 && ItemSystem::GetItemSubType(item) != COSTUME_ACCE)
    {
        return;
    }
#endif
	else if ((m_bAcceCombination) && (bPos == 1) && (!AcceIsSameGrade(ItemSystem::GetItemValue(item, ACCE_GRADE_VALUE_FIELD))))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 662, "");
#endif
        return;
    }
#ifdef ENABLE_STOLE_COSTUME
	else if ((m_bAcceCombination) && (ItemSystem::GetItemSubType(item) == COSTUME_STOLE) && (ItemSystem::GetItemValue(item, 0) == 4))
    {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 20, "%s", ItemSystem::GetItemName(item));
#endif
        return;
    }
#endif
	else if ((m_bAcceCombination) && (ItemSystem::GetItemSocket(item, ACCE_ABSORPTION_SOCKET) >= ACCE_GRADE_4_ABS_MAX))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 663, "%d", ACCE_GRADE_4_ABS_MAX);
#endif
        return;
    }
    else if ((bPos == 1) && (m_bAcceAbsorption))
    {
		if ((ItemSystem::GetItemType(item) != ITEM_WEAPON) && (ItemSystem::GetItemType(item) != ITEM_ARMOR))
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 520, "");
#endif
            return;
        }
		else if ((ItemSystem::GetItemType(item) == ITEM_ARMOR) && (ItemSystem::GetItemSubType(item) != ARMOR_BODY))
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 520, "");
#endif
            return;
        }
    }
    else if
#ifdef ENABLE_STOLE_COSTUME
    (
#endif
		((ItemSystem::GetItemSubType(item) != COSTUME_ACCE)
#ifdef ENABLE_STOLE_COSTUME
			&& (ItemSystem::GetItemSubType(item) != COSTUME_STOLE))
#endif
        && (m_bAcceCombination))
        return;
    else if
#ifdef ENABLE_STOLE_COSTUME
    (
#endif
		((ItemSystem::GetItemSubType(item) != COSTUME_ACCE)
#ifdef ENABLE_STOLE_COSTUME
			&& (ItemSystem::GetItemSubType(item) != COSTUME_STOLE))
#endif
        && (bPos == 0) && (m_bAcceAbsorption))
        return;
	else if ((ItemSystem::GetItemSocket(item, ACCE_ABSORBED_SOCKET) > 0) && (bPos == 0) && (m_bAcceAbsorption))
		return;

	auto pkItemMaterial = GetAcceMaterials();
	if ((bPos == 1) && pkItemMaterial[0] == entt::null)
        return;

#ifdef ENABLE_STOLE_COSTUME
	if ((!m_bAcceAbsorption) && (bPos == 1) && (ItemSystem::GetItemSubType(pkItemMaterial[0]) != ItemSystem::GetItemSubType(item))) {
#ifdef TEXTS_IMPROVEMENT
		if (ItemSystem::GetItemSubType(pkItemMaterial[0]) == COSTUME_STOLE) {
            ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 18, "");
        }
        else {
            ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 822, "");
        }
#endif
        return;
    }
	else if (!m_bAcceAbsorption && bPos == 1 && ItemSystem::GetItemSubType(pkItemMaterial[0]) == COSTUME_STOLE && ItemSystem::GetItemVnum(pkItemMaterial[0]) != ItemSystem::GetItemVnum(item)) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 1293, "");
#endif
        return;
    }
#endif

	if (pkItemMaterial[bPos] != entt::null)
		return;

	pkItemMaterial[bPos] = item;
	ItemSystem::LockItem(pkItemMaterial[bPos]);

    uint32_t dwItemVnum, dwMinAbs, dwMaxAbs;
    GetAcceCombineResult(dwItemVnum, dwMinAbs, dwMaxAbs);

    TPacketAcce sPacket;
    sPacket.header = HEADER_GC_ACCE;
    sPacket.subheader = ACCE_SUBHEADER_GC_ADDED;
    sPacket.bWindow = m_bAcceCombination == true ? true : false;
	sPacket.dwPrice = GetAcceCombinePrice(ItemSystem::GetItemValue(item, ACCE_GRADE_VALUE_FIELD)
#ifdef ENABLE_STOLE_COSTUME
		, ItemSystem::GetItemSubType(item) == COSTUME_STOLE
#endif
    );
    sPacket.bPos = bPos;
    sPacket.tPos = tPos;
    sPacket.dwItemVnum = dwItemVnum;
    sPacket.dwMinAbs = dwMinAbs;
    sPacket.dwMaxAbs = dwMaxAbs;
    GetDesc()->Packet(&sPacket, sizeof(TPacketAcce));
}

void CHARACTER::RemoveAcceMaterial(uint8_t bPos)
{
    if (bPos >= ACCE_WINDOW_MAX_MATERIALS)
        return;

	auto pkItemMaterial = GetAcceMaterials();

    uint32_t dwPrice = 0;

    if (bPos == 1)
    {
		if (pkItemMaterial[bPos] != entt::null)
		{
			ItemSystem::UnlockItem(pkItemMaterial[bPos]);
			pkItemMaterial[bPos] = entt::null;
		}

		if (pkItemMaterial[0] != entt::null) {
			dwPrice = GetAcceCombinePrice(ItemSystem::GetItemValue(pkItemMaterial[0], ACCE_GRADE_VALUE_FIELD)
#ifdef ENABLE_STOLE_COSTUME
				, ItemSystem::GetItemSubType(pkItemMaterial[0]) == COSTUME_STOLE
#endif
            );
        }
    }
    else
        ClearAcceMaterials();

    TItemPos tPos;
    tPos.window_type = INVENTORY;
    tPos.cell = 0;

    TPacketAcce sPacket;
    sPacket.header = HEADER_GC_ACCE;
    sPacket.subheader = ACCE_SUBHEADER_GC_REMOVED;
    sPacket.bWindow = m_bAcceCombination == true ? true : false;
    sPacket.dwPrice = dwPrice;
    sPacket.bPos = bPos;
    sPacket.tPos = tPos;
    sPacket.dwItemVnum = 0;
    sPacket.dwMinAbs = 0;
    sPacket.dwMaxAbs = 0;
    GetDesc()->Packet(&sPacket, sizeof(TPacketAcce));
}

uint8_t CHARACTER::CanRefineAcceMaterials()
{
    if (GetOfflineShopGuest() || GetAuctionGuest())
        return 0;

    if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen()
#ifdef __ATTR_TRANSFER_SYSTEM__
        || IsAttrTransferOpen()
#endif
        )
        return 0;

    uint8_t bReturn = 0;
	auto pkItemMaterial = GetAcceMaterials();
    if (m_bAcceCombination)
    {
        for (int i = 0; i < ACCE_WINDOW_MAX_MATERIALS; ++i)
        {
			if (pkItemMaterial[i] != entt::null)
			{
				if ((ItemSystem::GetItemType(pkItemMaterial[i]) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(pkItemMaterial[i]) == COSTUME_ACCE))
                    bReturn = 1;
#ifdef ENABLE_STOLE_COSTUME
				else if ((ItemSystem::GetItemType(pkItemMaterial[i]) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(pkItemMaterial[i]) == COSTUME_STOLE))
                    bReturn = 1;
#endif
                else
                {
                    bReturn = 0;
                    break;
                }
            }
            else
            {
                bReturn = 0;
                break;
            }
        }
    }
    else if (m_bAcceAbsorption)
    {
		if (pkItemMaterial[0] != entt::null && pkItemMaterial[1] != entt::null)
		{
			if ((ItemSystem::GetItemType(pkItemMaterial[0]) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(pkItemMaterial[0]) == COSTUME_ACCE))
                bReturn = 2;
            else
                bReturn = 0;

			if ((ItemSystem::GetItemType(pkItemMaterial[1]) == ITEM_WEAPON) || ((ItemSystem::GetItemType(pkItemMaterial[1]) == ITEM_ARMOR) && (ItemSystem::GetItemSubType(pkItemMaterial[1]) == ARMOR_BODY)))
                bReturn = 2;
#ifdef ATTR_LOCK
			if ((ItemSystem::GetItemType(pkItemMaterial[1]) == ITEM_WEAPON) || ((ItemSystem::GetItemType(pkItemMaterial[1]) == ITEM_ARMOR) && (ItemSystem::GetItemSubType(pkItemMaterial[1]) == ARMOR_BODY)))
			{
				if (ItemSystem::GetItemLockedAttributeIndex(pkItemMaterial[1]) != -1)
                {
                    bReturn = 0;
#ifdef TEXTS_IMPROVEMENT
                    ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 783, "");
#endif
                }
            }
#endif
            else
                bReturn = 0;

			if (ItemSystem::GetItemSocket(pkItemMaterial[0], ACCE_ABSORBED_SOCKET) > 0)
                bReturn = 0;
        }
        else
            bReturn = 0;
    }

    return bReturn;
}

void CHARACTER::RefineAcceMaterials()
{
    uint8_t bCan = CanRefineAcceMaterials();
    if (bCan == 0)
        return;

	auto pkItemMaterial = GetAcceMaterials();
	if (!ItemSystem::IsValidItem(pkItemMaterial[0]) ||
		!ItemSystem::IsValidItem(pkItemMaterial[1]))
		return;

    uint32_t dwItemVnum, dwMinAbs, dwMaxAbs;
    GetAcceCombineResult(dwItemVnum, dwMinAbs, dwMaxAbs);

	int64_t dwPrice = GetAcceCombinePrice(ItemSystem::GetItemValue(pkItemMaterial[0], ACCE_GRADE_VALUE_FIELD)
#ifdef ENABLE_STOLE_COSTUME
		, ItemSystem::GetItemSubType(pkItemMaterial[0]) == COSTUME_STOLE
#endif
    );


    if (bCan == 1)
    {
#ifdef ENABLE_STOLE_COSTUME
		bool bStole = ItemSystem::GetItemSubType(pkItemMaterial[0]) == COSTUME_STOLE;
#endif
        int iSuccessChance = 0;
		int32_t lVal = ItemSystem::GetItemValue(pkItemMaterial[0], ACCE_GRADE_VALUE_FIELD);
        switch (lVal)
        {
        case 2:
        {
#ifdef ENABLE_STOLE_COSTUME
            if (bStole) {
                iSuccessChance = STOLA_COMBINE_GRADE_2;
                break;
            }
#endif
            iSuccessChance = ACCE_COMBINE_GRADE_2;
        }
        break;
        case 3:
        {
#ifdef ENABLE_STOLE_COSTUME
            if (bStole) {
                iSuccessChance = STOLA_COMBINE_GRADE_3;
                break;
            }
#endif
            iSuccessChance = ACCE_COMBINE_GRADE_3;
        }
        break;
        case 4:
        {
            iSuccessChance = ACCE_COMBINE_GRADE_4;
        }
        break;
        default:
        {
#ifdef ENABLE_STOLE_COSTUME
            if (bStole) {
                iSuccessChance = STOLA_COMBINE_GRADE_1;
                break;
            }
#endif
            iSuccessChance = ACCE_COMBINE_GRADE_1;
        }
        break;
        }

        if (GetGold() < dwPrice)
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 232, "");
#endif
            return;
        }

        int iChance = number(1, 100);
        bool bSucces = (iChance <= iSuccessChance ? true : false);
        if (bSucces)
        {
			const entt::entity resultItem =
				ItemSystem::CreateItemEcs(dwItemVnum, 1, 0, false);
			if (!ItemSystem::IsValidItem(resultItem))
            {
                LOG_ERROR("{} can't be created.", dwItemVnum);
				return;
			}

#ifdef ENABLE_STOLE_COSTUME
			if (ItemSystem::GetItemSubType(resultItem) != COSTUME_STOLE)
				ItemSystem::CopyAllAttrToEcs(pkItemMaterial[0], resultItem);
#else
			ItemSystem::CopyAllAttrToEcs(pkItemMaterial[0], resultItem);
#endif
            LogManager::instance().ItemLogEntity(
				GetEntityHandle(), resultItem, "COMBINE SUCCESS",
				ItemSystem::GetItemName(resultItem));
            uint32_t dwAbs = (dwMinAbs == dwMaxAbs ? dwMinAbs : number(dwMinAbs + 1, dwMaxAbs));
			ItemSystem::SetItemSocket(resultItem, ACCE_ABSORPTION_SOCKET, dwAbs);
			ItemSystem::SetItemSocket(resultItem, ACCE_ABSORBED_SOCKET, ItemSystem::GetItemSocket(pkItemMaterial[0], ACCE_ABSORBED_SOCKET));

            PointChange(POINT_GOLD, -dwPrice);
			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, ItemSystem::GetItemVnum(pkItemMaterial[0]), -dwPrice);

			uint16_t wCell = ItemSystem::GetItemCell(pkItemMaterial[0]);
			const entt::entity material0 = pkItemMaterial[0];
			const entt::entity material1 = pkItemMaterial[1];
			pkItemMaterial[0] = entt::null;
			pkItemMaterial[1] = entt::null;
			ItemSystem::DestroyItemEntityEcs(
				material0, "COMBINE (REFINE SUCCESS)");
			ItemSystem::DestroyItemEntityEcs(
				material1, "COMBINE (REFINE SUCCESS)");

			if (!ItemSystem::PlaceItemEcs(
					GetEntityHandle(), resultItem, INVENTORY, wCell))
			{
				ItemSystem::DestroyItemEntityEcs(
					resultItem, "COMBINE RESULT PLACE FAILED");
				ClearAcceMaterials();
				return;
			}
			ItemSystem::FlushDelayedSaveEcs(resultItem);
			ItemSystem::AttrLogEcs(resultItem);

#ifdef TEXTS_IMPROVEMENT
            if (lVal == 4) {
                ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 521, "%d", dwAbs);
            }
            else {
                ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 389, "");
            }
#endif
            EffectPacket(SE_EFFECT_ACCE_SUCCEDED);
			LogManager::instance().AcceLog(GetPlayerID(), GetX(), GetY(), dwItemVnum, ItemSystem::GetItemID(resultItem), 1, dwAbs, 1);

            ClearAcceMaterials();
        }
        else
		{
            PointChange(POINT_GOLD, -dwPrice);
			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, ItemSystem::GetItemVnum(pkItemMaterial[0]), -dwPrice);
			const entt::entity failedMaterial = pkItemMaterial[1];
			pkItemMaterial[1] = entt::null;
			ItemSystem::DestroyItemEntityEcs(
				failedMaterial, "COMBINE (REFINE FAIL)");
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 390, "");
#endif
            LogManager::instance().AcceLog(GetPlayerID(), GetX(), GetY(), dwItemVnum, 0, 0, 0, 0);
		}

        TItemPos tPos;
        tPos.window_type = INVENTORY;
        tPos.cell = 0;

        TPacketAcce sPacket;
        sPacket.header = HEADER_GC_ACCE;
        sPacket.subheader = ACCE_SUBHEADER_CG_REFINED;
        sPacket.bWindow = m_bAcceCombination == true ? true : false;
        sPacket.dwPrice = dwPrice;
        sPacket.bPos = 0;
        sPacket.tPos = tPos;
        sPacket.dwItemVnum = 0;
        sPacket.dwMinAbs = 0;
        if (bSucces)
            sPacket.dwMaxAbs = 100;
        else
            sPacket.dwMaxAbs = 0;

        GetDesc()->Packet(&sPacket, sizeof(TPacketAcce));
    }
    else
    {
		ItemSystem::CopyItemAttributesEcs(pkItemMaterial[1], pkItemMaterial[0]);
		LogManager::instance().ItemLogEntity(
			GetEntityHandle(), pkItemMaterial[0], "ABSORB (REFINE SUCCESS)",
			ItemSystem::GetItemName(pkItemMaterial[0]));
		ItemSystem::SetItemSocket(pkItemMaterial[0], ACCE_ABSORBED_SOCKET, ItemSystem::GetItemOriginalVnum(pkItemMaterial[1]));
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			if (ItemSystem::GetItemAttributeValue(pkItemMaterial[0], i) < 0)
				ItemSystem::SetItemForceAttributeEcs(pkItemMaterial[0], i, ItemSystem::GetItemAttributeType(pkItemMaterial[0], i), 0);
		}

		const entt::entity absorbedMaterial = pkItemMaterial[1];
		pkItemMaterial[1] = entt::null;
		ItemSystem::DestroyItemEntityEcs(
			absorbedMaterial, "ABSORBED (REFINE SUCCESS)");

		ItemSystem::FlushDelayedSaveEcs(pkItemMaterial[0]);
		ItemSystem::AttrLogEcs(pkItemMaterial[0]);

#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 629, "");
#endif
        ClearAcceMaterials();

        TItemPos tPos;
        tPos.window_type = INVENTORY;
        tPos.cell = 0;

        TPacketAcce sPacket;
        sPacket.header = HEADER_GC_ACCE;
        sPacket.subheader = ACCE_SUBHEADER_CG_REFINED;
        sPacket.bWindow = m_bAcceCombination == true ? true : false;
        sPacket.dwPrice = dwPrice;
        sPacket.bPos = 255;
        sPacket.tPos = tPos;
        sPacket.dwItemVnum = 0;
        sPacket.dwMinAbs = 0;
        sPacket.dwMaxAbs = 1;
        GetDesc()->Packet(&sPacket, sizeof(TPacketAcce));
    }
}

bool CHARACTER::CleanAcceAttr(entt::entity pkItem, entt::entity pkTarget)
{
    if (!CanHandleItem())
        return false;
    else if (!ItemSystem::IsValidItem(pkItem) || !ItemSystem::IsValidItem(pkTarget))
        return false;

    if ((ItemSystem::GetItemType(pkTarget) != ITEM_COSTUME) &&
		(ItemSystem::GetItemSubType(pkTarget) != COSTUME_ACCE))
        return false;

    if (ItemSystem::GetItemSocket(pkTarget, ACCE_ABSORBED_SOCKET) <= 0)
        return false;

    ItemSystem::SetItemSocket(pkTarget, ACCE_ABSORBED_SOCKET, 0);
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        ItemSystem::SetItemForceAttributeEcs(pkTarget, i, 0, 0);

    ItemSystem::ConsumeItemEcs(pkItem);
    LogManager::instance().ItemLogEntity(
		GetEntityHandle(), pkTarget, "USE_DETACHMENT (CLEAN ATTR)",
		ItemSystem::GetItemName(pkTarget));
    return true;
}

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

bool CHARACTER::IsHack(bool bSendMsg, bool bCheckShopOwner, int limittime)
{
	return ecs::PlayerRuntime::IsHack(GetEntityHandle(), bSendMsg, bCheckShopOwner, limittime);
}

void CHARACTER::Say(const std::string& s)
{
    struct ::packet_script packet_script;

    packet_script.header = HEADER_GC_SCRIPT;
    packet_script.skin = 1;
    packet_script.src_size = s.size();
    packet_script.size = packet_script.src_size + sizeof(struct packet_script);

    TEMP_BUFFER buf;

    buf.write(&packet_script, sizeof(struct packet_script));
    buf.write(&s[0], s.size());

    if (IsPC())
    {
        GetDesc()->Packet(buf.read_peek(), buf.size());
    }
}

#ifdef __ENABLE_NEW_OFFLINESHOP__
void CHARACTER::SetShopSafebox(offlineshop::CShopSafebox* pk)
{
    if (m_pkShopSafebox && pk == nullptr)
        m_pkShopSafebox->SetOwner(nullptr);

    else if (m_pkShopSafebox == nullptr && pk)
        pk->SetOwner(this);

    m_pkShopSafebox = pk;
}
#endif

void CHARACTER::SetArena(CArena* arena)
{
	m_pArena = arena;
	ecs::PlayerRuntime::SetArena(GetEntityHandle(), arena);
}

CArena* CHARACTER::GetArena() const
{
	const entt::entity entity = GetEntityHandle();
	if (entity != entt::null && g_registry.valid(entity))
		return ecs::PlayerRuntime::GetArena(entity);

	return m_pArena;
}

#ifdef __NEWPET_SYSTEM__
void CHARACTER::SetEggVid(int vid)
{
	m_eggvid = vid;
	ecs::PlayerRuntime::SetEggVID(GetEntityHandle(), vid);
}

int CHARACTER::GetEggVid() const
{
	const entt::entity entity = GetEntityHandle();
	if (entity != entt::null && g_registry.valid(entity))
		return ecs::PlayerRuntime::GetEggVID(entity);
	return m_eggvid;
}
#endif

void CHARACTER::SetArenaObserverMode(bool flag)
{
	m_ArenaObserver = flag;

	const entt::entity entity = GetEntityHandle();
	if (entity == entt::null || !g_registry.valid(entity))
		return;

	auto& status = g_registry.get_or_emplace<ecs::StatusFlags>(entity);
	status.isArenaObserver = flag;
	g_registry.emplace_or_replace<ecs::DirtyTag>(entity);
}

bool CHARACTER::GetArenaObserverMode() const
{
	const entt::entity entity = GetEntityHandle();
	if (entity != entt::null && g_registry.valid(entity))
	{
		if (const auto* status = g_registry.try_get<ecs::StatusFlags>(entity))
			return status->isArenaObserver;
	}

	return m_ArenaObserver;
}

#ifdef ENABLE_RANKING
long long CHARACTER::GetRankPoints(int iArg)
{
    if ((iArg < 0) || (iArg >= RANKING_MAX_CATEGORIES))
        return 0;

	const entt::entity entity = GetEntityHandle();
	if (entity != entt::null && g_registry.valid(entity) &&
		g_registry.all_of<ecs::RankPoints>(entity))
		return ecs::PlayerRuntime::GetRankPoints(entity, iArg);

	return m_lRankPoints[iArg];
}

void CHARACTER::SetRankPoints(int iArg, long long lPoint)
{
    if ((iArg < 0) || (iArg >= RANKING_MAX_CATEGORIES))
        return;

	m_lRankPoints[iArg] = lPoint;
	ecs::PlayerRuntime::SetRankPoints(GetEntityHandle(), iArg, lPoint);
	Save();
}

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
    if (GetGMLevel() > GM_PLAYER) {
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
int CHARACTER::GetDuel(const char* type) const
{
    return GetDuelImpl(GetEntityHandle(), type);
}

void CHARACTER::SetDuel(const char* type, int value)
{
    SetDuelImpl(GetEntityHandle(), type, value);
}
#endif

void CHARACTER::SetPart(uint8_t bPartPos, uint16_t wVal)
{
    assert(bPartPos < PART_MAX_NUM);

    if (auto* appearance = EnsureAppearancePartsComponent(GetEntityHandle()))
        appearance->parts[bPartPos] = wVal;
}

uint16_t CHARACTER::GetPart(uint8_t bPartPos) const
{
    assert(bPartPos < PART_MAX_NUM);
	const entt::entity character = GetEntityHandle();

#ifdef __HIDE_COSTUME_SYSTEM__
    if (bPartPos == PART_MAIN &&
		ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_BODY)) &&
		IsBodyCostumeHidden() == true) {
		const entt::entity armor = ItemSystem::GetWearItem(character, WEAR_BODY);
		if (!ItemSystem::IsValidItem(armor))
			return 0;
		const uint32_t transmutation = ItemSystem::GetItemTransmutationVnum(armor);
		return transmutation != 0 ? transmutation : ItemSystem::GetItemVnum(armor);
    }
    else if (bPartPos == PART_HAIR &&
		ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_HAIR)) &&
		IsHairCostumeHidden() == true)
        return 0;
#ifdef ENABLE_STOLE_COSTUME
    else if (bPartPos == PART_ACCE &&
		ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_ACCE)) &&
		IsAcceCostumeHidden() == true) {
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
		IsAcceCostumeHidden() == true)
        return 0;
#endif
    else if (bPartPos == PART_WEAPON &&
		ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_WEAPON)) &&
		IsWeaponCostumeHidden() == true)
    {
		const entt::entity weapon = ItemSystem::GetWearItem(character, WEAR_WEAPON);
		if (!ItemSystem::IsValidItem(weapon))
			return 0;
		const uint32_t transmutation = ItemSystem::GetItemTransmutationVnum(weapon);
		return transmutation != 0 ? transmutation : ItemSystem::GetItemVnum(weapon);
    }
#endif

    if (const auto* appearance = TryGetAppearancePartsComponent(GetEntityHandle()))
        return appearance->parts[bPartPos];

    return 0;
}

uint16_t CHARACTER::GetOriginalPart(uint8_t bPartPos) const
{
	const entt::entity character = GetEntityHandle();
    switch (bPartPos)
    {
    case PART_MAIN:
    {
        if (!IsPC())
            return GetPart(PART_MAIN);

#ifdef __HIDE_COSTUME_SYSTEM__
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_BODY)) &&
			IsBodyCostumeHidden() == true) {
			const entt::entity armor = ItemSystem::GetWearItem(character, WEAR_BODY);
			if (ItemSystem::IsValidItem(armor))
				return ItemSystem::GetItemVnum(armor);
        }
#endif

        if (const auto* appearance = TryGetAppearancePartsComponent(GetEntityHandle()))
            return appearance->basePart;

        return 0;
    }
    case PART_HAIR:
    {
#ifdef __HIDE_COSTUME_SYSTEM__
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_HAIR)) &&
			IsHairCostumeHidden() == true)
            return 0;
#endif

        return GetPart(PART_HAIR);
    }
#ifdef ENABLE_ACCE_SYSTEM
    case PART_ACCE:
    {
#ifdef __HIDE_COSTUME_SYSTEM__
#ifdef ENABLE_STOLE_COSTUME
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_ACCE)) &&
			IsAcceCostumeHidden() == true) {
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
			IsAcceCostumeHidden() == true)
            return 0;
#endif
#else
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_ACCE_SLOT)))
            return 0;
#endif
        return GetPart(PART_ACCE);
    }
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
    case PART_WEAPON:
    {
#ifdef __HIDE_COSTUME_SYSTEM__
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_WEAPON)) &&
			IsWeaponCostumeHidden() == true) {
			const entt::entity weapon = ItemSystem::GetWearItem(character, WEAR_WEAPON);
			if (ItemSystem::IsValidItem(weapon))
				return ItemSystem::GetItemVnum(weapon);
        }
#endif
        return GetPart(PART_WEAPON);
#endif
    }
    default:
        return 0;
    }
}

void CHARACTER::SetMaxHP(int64_t iVal)
{
    if (auto* health = EnsureHealthComponent(GetEntityHandle()))
        health->max = static_cast<int32_t>(std::clamp<int64_t>(iVal, 0, INT32_MAX));
}

int64_t CHARACTER::GetMaxHP() const
{
    if (const auto* health = TryGetHealthComponent(GetEntityHandle()))
        return health->max;

    return 0;
}

void CHARACTER::SetMaxSP(int64_t iVal)
{
    if (auto* mana = EnsureManaComponent(GetEntityHandle()))
        mana->max = static_cast<int32_t>(std::clamp<int64_t>(iVal, 0, INT32_MAX));
}

int64_t CHARACTER::GetMaxSP() const
{
    if (const auto* mana = TryGetManaComponent(GetEntityHandle()))
        return mana->max;

    return 0;
}

void CHARACTER::SetMaxStamina(int64_t iVal)
{
    if (auto* stamina = EnsureStaminaComponent(GetEntityHandle()))
        stamina->max = static_cast<int32_t>(std::clamp<int64_t>(iVal, 0, INT32_MAX));
}

int64_t CHARACTER::GetMaxStamina() const
{
    if (const auto* stamina = TryGetStaminaComponent(GetEntityHandle()))
        return stamina->max;

    return 0;
}

void CHARACTER::Destroy()
{
	// Keep the ECS entity alive for the complete teardown. Inventory, session,
	// shop and social state are ECS-owned now, so destroying the entity before
	// ClearItem()/CloseMyShop() turns those cleanup calls into silent no-ops.
	const entt::entity entityToDestroy = GetEntityHandle();

    CloseMyShop();

    if (m_pkRegen)
    {
        if (m_pkDungeon) {
            if (m_pkDungeon->IsValidRegen(m_pkRegen, regen_id_)) {
                --m_pkRegen->count;
            }
        }
        else {
            --m_pkRegen->count;
        }
        m_pkRegen = nullptr;
    }

    if (m_pkDungeon)
    {
        SetDungeon(nullptr);
    }

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    if (m_mountSystem)
    {
        m_mountSystem->Destroy();
        delete m_mountSystem;

        m_mountSystem = nullptr;
    }

    if (GetMountVnum())
    {
        RemoveAffect(AFFECT_MOUNT);
        RemoveAffect(AFFECT_MOUNT_BONUS);
    }
    HorseSummon(false);
#endif
#ifdef __PET_SYSTEM__
    if (m_petSystem)
    {
        m_petSystem->Destroy();
        delete m_petSystem;

        m_petSystem = nullptr;
		if (GetEntityHandle() != entt::null && g_registry.valid(GetEntityHandle()))
			g_registry.get_or_emplace<ecs::PetRuntimeRefs>(GetEntityHandle()).petSystem = nullptr;
    }
#endif

#ifdef __NEWPET_SYSTEM__
    if (m_newpetSystem)
    {
        m_newpetSystem->Destroy();
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

    if (m_pkExchange)
        m_pkExchange->Cancel();

    SetVictim(entt::null);

    if (GetShop())
    {
        GetShop()->RemoveGuest(this);
        SetShop(nullptr);
    }

    ClearStone();
    ClearSync();
    ClearTarget();

    if (nullptr == m_pkMobData)
    {
        DragonSoul_CleanUp();
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

    if (m_pkMobInst)
    {
        M2_DELETE(m_pkMobInst);
        m_pkMobInst = nullptr;
    }

    m_pkMobData = nullptr;

    if (m_pkSafebox)
    {
        M2_DELETE(m_pkSafebox);
        m_pkSafebox = nullptr;
    }

    if (m_pkMall)
    {
        M2_DELETE(m_pkMall);
        m_pkMall = nullptr;
    }

    for (TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.begin(); it != m_map_buff_on_attrs.end(); it++)
    {
        if (nullptr != it->second)
        {
            M2_DELETE(it->second);
        }
    }
    m_map_buff_on_attrs.clear();

    m_set_pkChrSpawnedBy.clear();

    StopMuyeongEvent();
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
    StopGyeongGongEvent();
#endif
    event_cancel(&m_pkWarpNPCEvent);
    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Recovery);
    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Dead);
    event_cancel(&m_pkSaveEvent);
    event_cancel(&m_pkTimedEvent);
    ecs::PlayerRuntime::CancelCharEvent(GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Stun);
    event_cancel(&m_pkFishingEvent);
    AffectSystem::CancelDamageEvents(GetEntityHandle());
    event_cancel(&m_pkPartyRequestEvent);
    event_cancel(&m_pkWarpEvent);
#ifdef ENABLE_NEW_FISHING_SYSTEM
    ActivitySystem::StopFishing(GetEntityHandle());
#endif
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
    if (m_pkBattlePassStayOnlineEvent)
    {
        event_cancel(&m_pkBattlePassStayOnlineEvent);
        m_pkBattlePassStayOnlineEvent = nullptr;
    }
#endif

    event_cancel(&m_pkMiningEvent);
#ifdef ENABLE_BLOCK_MULTIFARM
    if (m_pkDropEvent) {
        event_cancel(&m_pkDropEvent);
        m_pkDropEvent = nullptr;
    }
#endif
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
    event_cancel(&m_pkStayOnlineEvent);
#endif

    for (auto it = m_mapMobSkillEvent.begin(); it != m_mapMobSkillEvent.end(); ++it)
    {
        LPEVENT pkEvent = it->second;
        event_cancel(&pkEvent);
    }
    m_mapMobSkillEvent.clear();
#ifdef __DUNGEON_INFO_SYSTEM__
    dungeonDamage.clear();
#endif
    ClearAffect();

    event_cancel(&m_pkDestroyWhenIdleEvent);

    if (m_pSkillLevels)
    {
        M2_DELETE_ARRAY(m_pSkillLevels);
        m_pSkillLevels = nullptr;
    }

    if (m_pkMountInventory)
    {
        M2_DELETE(m_pkMountInventory);
        m_pkMountInventory = nullptr;
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

void CHARACTER::ResetPoint(int iLv)
{
	ecs::PointSystem::ResetAllPoints(GetEntityHandle(), iLv);
}

void CHARACTER::GiveRandomSkillBook()
{
    const entt::entity item = ItemSystem::AutoGiveItemEcs(GetEntityHandle(), 50300);

    if (ItemSystem::IsValidItem(item))
    {
        extern const uint32_t GetRandomSkillVnum(uint8_t bJob = JOB_MAX_NUM);
        uint32_t dwSkillVnum = 0;
        if (!number(0, 1))
            dwSkillVnum = GetRandomSkillVnum(GetJob());
        else
            dwSkillVnum = GetRandomSkillVnum();
        ItemSystem::SetItemSocket(item, 0, dwSkillVnum);
    }
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

void CHARACTER::BeginStateEmpty()
{
    MonsterLog("!");
}

int CHARACTER::ChangeEmpire(uint8_t empire)
{
	return ecs::PlayerRuntime::ChangeEmpire(GetEntityHandle(), empire);
}

int CHARACTER::GetChangeEmpireCount() const
{
	return ecs::PlayerRuntime::GetChangeEmpireCount(GetEntityHandle());
}

void CHARACTER::SetChangeEmpireCount()
{
	ecs::PlayerRuntime::IncrementChangeEmpireCount(GetEntityHandle());
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
    m_posStart.x = GetX();
    m_posStart.y = GetY();
    ecs::MovementSystem::SyncDestinationClear(GetEntityHandle());

    EncodeInsertPacket(this);

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
                LPENTITY viewer = ecs::SpatialService::LPENTITYFromEntity(g_registry, viewerE);
                if (viewer)
                    EncodeInsertPacket(viewer);
            }
        }
    }

    SetValidComboInterval(0);
    SetComboSequence(0);

    ComputePoints();
}

int64_t CHARACTER::ComputeRefineFee(int64_t iCost, int64_t iMultiply) const
{
    CGuild* pGuild = GetRefineGuild();
    if (pGuild)
    {
        if (pGuild == GetGuild())
            return iCost * iMultiply * 9 / 10;

        LPCHARACTER chRefineNPC = CHARACTER_MANAGER::instance().Find(m_dwRefineNPCVID);
        if (chRefineNPC && chRefineNPC->GetEmpire() != GetEmpire())
            return iCost * iMultiply * 3;

        return iCost * iMultiply;
    }
    else
        return iCost;
}

void CHARACTER::PayRefineFee(int64_t iTotalMoney)
{
    int64_t iFee = iTotalMoney / 10;
    CGuild* pGuild = GetRefineGuild();

    int64_t iRemain = iTotalMoney;

    if (pGuild)
    {
        if (pGuild != GetGuild())
        {
            pGuild->RequestDepositMoney(GetEntityHandle(), iFee);
            iRemain -= iFee;
        }
    }

    PointChange(POINT_GOLD, -iRemain);
}

void CHARACTER::StartDestroyWhenIdleEvent()
{
    if (m_pkDestroyWhenIdleEvent)
        return;

    char_event_info* info = AllocEventInfo<char_event_info>();

    info->ch = this;

    m_pkDestroyWhenIdleEvent = event_create(destroy_when_idle_event, info, PASSES_PER_SEC(300));
}

void CHARACTER::SetPlayerProto(const TPlayerTable* t)
{
    if (!GetDesc() || !*GetDesc()->GetHostName())
        LOG_ERROR("cannot get desc or hostname");
    else
        SetGMLevel();

    m_bCharType = CHAR_TYPE_PC;

    m_dwPlayerID = t->id;

    m_iAlignment = t->lAlignment;
    m_iRealAlignment = t->lAlignment;
    if (auto* combat = g_registry.try_get<ecs::CombatStats>(GetEntityHandle())) {
        combat->alignment = m_iAlignment;
        combat->realAlignment = m_iRealAlignment;
    }

    m_points.voice = t->voice;

    m_points.skill_group = t->skill_group;

    if (auto* appearance = EnsureAppearancePartsComponent(GetEntityHandle()))
        appearance->basePart = t->part_base;
    SetPart(PART_HAIR, t->parts[PART_HAIR]);
#ifdef ENABLE_ACCE_SYSTEM
    SetPart(PART_ACCE, t->parts[PART_ACCE]);
#endif

    SetRandomHP(t->sRandomHP);
    SetRandomSP(t->sRandomSP);

    if (m_pSkillLevels) {
        M2_DELETE_ARRAY(m_pSkillLevels);
    }

    m_pSkillLevels = M2_NEW TPlayerSkill[SKILL_MAX_NUM];
    memcpy(m_pSkillLevels, t->skills, sizeof(TPlayerSkill) * SKILL_MAX_NUM);
#ifdef ENABLE_BATTLE_PASS
    m_dwBattlePassEndTime = t->dwBattlePassEndTime;
#endif

    if (t->lMapIndex >= 10000)
    {
        m_posWarp.x = t->lExitX;
        m_posWarp.y = t->lExitY;
        m_lWarpMapIndex = t->lExitMapIndex;
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

    SetRace(t->job);

    SetLevel(t->level);
    SetExp(t->exp);
    SetGold(t->gold);
#ifdef ENABLE_GAYA_SYSTEM
    SetGaya(t->gaya);
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
    m_posStart.x = t->x;
    m_posStart.y = t->y;
    m_posStart.z = t->z;
    ecs::MovementSystem::SyncDestinationClear(GetEntityHandle());

    ComputePoints();

    SetHP(t->hp);
    SetSP(t->sp);
    SetStamina(t->stamina);

#ifndef ENABLE_GM_FLAG_IF_TEST_SERVER
    if (!test_server)
#endif
    {
#ifdef ENABLE_GM_FLAG_FOR_LOW_WIZARD
        if (GetGMLevel() > GM_PLAYER)
#else
        if (GetGMLevel() > GM_LOW_WIZARD)
#endif
        {
            m_afAffectFlag.Set(AFF_YMIR);
            g_registry.get_or_emplace<ecs::CombatStats>(GetEntityHandle()).pkMode = PK_MODE_PROTECT;
        }
    }

    if (GetLevel() < PK_PROTECT_LEVEL) {
        g_registry.get_or_emplace<ecs::CombatStats>(GetEntityHandle()).pkMode = PK_MODE_PROTECT;
    }

    m_stMobile = t->szMobile;

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

    if (GetGMLevel() != GM_PLAYER)
    {
        LogManager::instance().CharLog(this, GetGMLevel(), "GM_LOGIN", "");
        LOG_INFO("GM_LOGIN(gmlevel={}, name={}({}), pos=({}, {})", static_cast<int>(GetGMLevel()), GetName(), GetPlayerID(), GetX(), GetY());
    }

#ifdef ENABLE_RANKING
    for (int i = 0; i < RANKING_MAX_CATEGORIES; ++i)
        m_lRankPoints[i] = t->lRankPoints[i];
#endif

#ifdef __PET_SYSTEM__
    if (m_petSystem)
    {
        m_petSystem->Destroy();
        delete m_petSystem;
    }

    m_petSystem = M2_NEW CPetSystem(this);
	if (GetEntityHandle() != entt::null && g_registry.valid(GetEntityHandle()))
		g_registry.get_or_emplace<ecs::PetRuntimeRefs>(GetEntityHandle()).petSystem = m_petSystem;
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    if (m_mountSystem)
    {
        m_mountSystem->Destroy();
        delete m_mountSystem;
    }

    m_mountSystem = M2_NEW CMountSystem(this);
#endif

#ifdef __NEWPET_SYSTEM__
    if (m_newpetSystem)
    {
        m_newpetSystem->Destroy();
        delete m_newpetSystem;
    }

    m_newpetSystem = M2_NEW CNewPetSystem(this);
	if (GetEntityHandle() != entt::null && g_registry.valid(GetEntityHandle()))
		g_registry.get_or_emplace<ecs::PetRuntimeRefs>(GetEntityHandle()).newPetSystem = m_newpetSystem;
#endif
}

void CHARACTER::SetProto(const CMob* pkMob)
{
    if (m_pkMobInst)
        M2_DELETE(m_pkMobInst);

    m_pkMobData = pkMob;
    m_pkMobInst = M2_NEW CMobInstance;

    g_registry.get_or_emplace<ecs::CombatStats>(GetEntityHandle()).pkMode = PK_MODE_FREE;

    const TMobTable* t = &m_pkMobData->m_table;

    m_bCharType = t->bType;

    SetLevel(t->bLevel);
    SetEmpire(t->bEmpire);

    SetExp(t->dwExp);
    SetRealPoint(POINT_ST, t->bStr);
    SetRealPoint(POINT_DX, t->bDex);
    SetRealPoint(POINT_HT, t->bCon);
    SetRealPoint(POINT_IQ, t->bInt);

    ComputePoints();

    SetHP(GetMaxHP());
    SetSP(GetMaxSP());
    if (auto* flags = EnsureRuntimeFlagsComponent(GetEntityHandle()))
        flags->aiFlag = t->dwAIFlag;
    SetImmuneFlag(t->dwImmuneFlag);

    AssignTriggers(t);

    ApplyMobAttribute(t);

    if (IsStone())
    {
        DetermineDropMetinStone();
    }

    if (IsWarp() || IsGoto())
    {
        StartWarpNPCEvent();
    }

    CHARACTER_MANAGER::instance().RegisterRaceNumMap(GetEntityHandle());

    if (mining::IsVeinOfOre(GetRaceNum()))
    {
        char_event_info* info = AllocEventInfo<char_event_info>();

        info->ch = this;

        m_pkMiningEvent = event_create(kill_ore_load_event, info, PASSES_PER_SEC(number(7 * 60, 15 * 60)));
    }
}

bool CHARACTER::StartStateMachine(int iNextPulse)
{
    if (CHARACTER_MANAGER::instance().AddToStateList(GetEntityHandle()))
    {
        m_dwNextStatePulse = thecore_heart->pulse + iNextPulse;
        return true;
    }

    return false;
}

void CHARACTER::StopStateMachine()
{
    CHARACTER_MANAGER::instance().RemoveFromStateList(GetEntityHandle());
}

void CHARACTER::UpdateStateMachine(uint32_t dwPulse)
{
    if (dwPulse < m_dwNextStatePulse)
        return;

    if (IsDead())
        return;

    AISystem::UpdateStateMachine(GetEntityHandle());
    m_dwNextStatePulse = dwPulse + m_dwStateDuration;
}

void CHARACTER::SetNextStatePulse(int iNextPulse)
{
    CHARACTER_MANAGER::instance().AddToStateList(GetEntityHandle());
    m_dwNextStatePulse = iNextPulse;

    if (iNextPulse < 10)
        MonsterLog("´UA1»óAÂ·Î3î1­°!AÚ");
}

void CHARACTER::UpdateCharacter(uint32_t dwPulse)
{
    AISystem::UpdateStateMachine(GetEntityHandle());
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

bool CHARACTER::OnIdle()
{
    return false;
}

void CHARACTER::OnMove(bool bIsAttack)
{
    m_dwLastMoveTime = get_dword_time();
    ecs::SyncPositionComponents(g_registry, GetEntityHandle(), GetMapIndex(), GetX(), GetY(), GetZ());

    if (bIsAttack)
    {
        m_dwLastAttackTime = m_dwLastMoveTime;

        if (IsAffectFlag(AFF_REVIVE_INVISIBLE))
            RemoveAffect(AFFECT_REVIVE_INVISIBLE);

        if (IsAffectFlag(AFF_EUNHYUNG))
        {
            RemoveAffect(SKILL_EUNHYUNG);
            SetAffectedEunhyung();
        }
        else
        {
            ClearAffectedEunhyung();
        }

        /*if (IsAffectFlag(AFF_JEONSIN))
          RemoveAffect(SKILL_JEONSINBANGEO);*/
    }

    /*if (IsAffectFlag(AFF_GUNGON))
      RemoveAffect(SKILL_GUNGON);*/

    // MINING
    mining_cancel();
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
        if (pkCauser->GetMyShop() && pkCauser != this)
        {
            LOG_ERROR("OnClick Fail ({}->{}) - pc has shop", pkCauser->GetName(), GetName());
            return;
        }
    }

    {
        if (pkCauser->GetExchange())
        {
            LOG_ERROR("OnClick Fail ({}->{}) - pc is exchanging", pkCauser->GetName(), GetName());
            return;
        }
    }

    if (IsPC())
    {
        if (!CTargetManager::instance().GetTargetInfo(pkCauser->GetPlayerID(), TARGET_TYPE_VID, GetPacketVID()))
        {
            if (GetMyShop())
            {
                if (pkCauser->IsDead() == true)
                    return;

                if (pkCauser == this)
                {
                    if ((GetExchange() || IsOpenSafebox() || GetShopOwner()) || IsCubeOpen())
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }

#ifdef __ATTR_TRANSFER_SYSTEM__
                    if (IsAttrTransferOpen())
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
                    if ((pkCauser->GetExchange() || pkCauser->IsOpenSafebox() || pkCauser->GetMyShop() || pkCauser->GetShopOwner()) || pkCauser->IsCubeOpen())
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }

#ifdef __ATTR_TRANSFER_SYSTEM__
                    if (pkCauser->IsAttrTransferOpen())
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }
#endif

                    if ((GetExchange() || IsOpenSafebox() || IsCubeOpen()))
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 369, "%s", GetName());
#endif
                        return;
                    }

#ifdef __ATTR_TRANSFER_SYSTEM__
                    if (IsAttrTransferOpen())
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 369, "%s", GetName());
#endif
                        return;
                    }
#endif
                }

                if (pkCauser->GetShop())
                {
                    pkCauser->GetShop()->RemoveGuest(pkCauser);
                    pkCauser->SetShop(nullptr);
                }

                GetMyShop()->AddGuest(pkCauser, GetPacketVID(), false);
                pkCauser->SetShopOwner(GetEntityHandle());
                return;
            }

            if (test_server)
                LOG_ERROR("{}.OnClickFailure({}) - target is PC", pkCauser->GetName(), GetName());

            return;
        }
    }

    pkCauser->SetQuestNPCID(GetPacketVID());

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

        m_triggerOnClick.pFunc(this, pkCauser);
    }
}

void CHARACTER::DestroyPvP()
{
    if (GetDesc() != nullptr)
    {
        const char* szTableStaticPvP[] = { BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };

        int moneyBet = GetQuestFlag(szTableStaticPvP[8]);
        int isDuel = GetQuestFlag(szTableStaticPvP[9]);

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
                SetQuestFlag(szTableStaticPvP[i], 0);
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
    if (!IsPC() || !GetDesc() || !CanWarp())
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

    Stop();
    Save();

    if (GetSectree())
    {
        GetSectree()->RemoveEntity(this);
        const entt::entity e = GetEntityHandle();
        if (e != entt::null && g_registry.valid(e))
        {
            g_registry.remove<ecs::SectorPlacement>(e);
            g_registry.remove<ecs::ViewActiveTag>(e);
        }
        ViewCleanup();
        EncodeRemovePacket(this);
    }

    m_lWarpMapIndex = lMapIndex;
    m_posWarp.x = x;
    m_posWarp.y = y;

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
    LogManager::instance().CharLog(this, 0, "CHANGE_CH", buf);

    return true;
}

EVENTINFO(switch_channel_info)
{
    DynamicCharacterPtr ch;
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

    LPCHARACTER ch = info->ch;
    if (!ch)
    {
        LOG_ERROR("No char to work on for the switch.");
        return 0;
    }
	const entt::entity character = ch->GetEntityHandle();

    // Phase 10: WRITES_STATE - deferred until ECS component covers m_pkTimedEvent

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

    ch->SwitchChannel(info->newAddr, info->newPort);
    ch->m_pkTimedEvent = nullptr;
    return 0;
}

bool CHARACTER::StartChannelSwitch(int32_t newAddr, uint16_t newPort)
{
    if (IsHack(false, true, 10))
        return false;

    switch_channel_info* info = AllocEventInfo<switch_channel_info>();
    info->ch = this;
    info->secs = CanWarp() && !IsPosition(POS_FIGHTING) ? 3 : 10;
    info->newAddr = newAddr;
    info->newPort = newPort;

    m_pkTimedEvent = event_create(switch_channel, info, 1);
    return true;
}
#endif

#ifdef ENABLE_BLOCK_MULTIFARM
void CHARACTER::BlockProcessed()
{
    if (!m_pkDropEvent) {
        LOG_ERROR("<drop_event> process failed, event is null.");
    }
    else {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 42, "");
#endif
        event_cancel(&m_pkDropEvent);
        m_pkDropEvent = nullptr;
        LOG_INFO("<drop_event> processed.");
    }
}

void CHARACTER::BlockDrop()
{
    if (!IsPC()) {
        return;
    }

    if (GetMapIndex() != 358 && GetMapIndex() != 359 && GetMapIndex() != 360 && GetMapIndex() != 361) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 36, "");
#endif
        return;
    }

    if (m_pkDropEvent) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 44, "");
#endif
        return;
    }

    drop_event_info* info = AllocEventInfo<drop_event_info>();
    info->ch = this;
    info->time = get_global_time() + 5;
    info->drop = false;
    m_pkDropEvent = event_create(drop_event, info, PASSES_PER_SEC(1));
#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 43, "%d", 5);
#endif
}

void CHARACTER::UnblockDrop()
{
    if (GetMapIndex() != 358 && GetMapIndex() != 359 && GetMapIndex() != 360 && GetMapIndex() != 361) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 36, "");
#endif
        return;
    }

    if (m_pkDropEvent) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 44, "");
#endif
        return;
    }

    drop_event_info* info = AllocEventInfo<drop_event_info>();
    info->ch = this;
    info->time = get_global_time() + 5;
    info->drop = true;
    m_pkDropEvent = event_create(drop_event, info, PASSES_PER_SEC(1));
#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 43, "%d", 5);
#endif
}

void CHARACTER::SetDropStatus()
{
    if (!IsPC())
        return;

    std::string login = GetDesc()->GetAccountTable().login;
    std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT status FROM account.antifarm WHERE login='%s'", login.c_str()));
    if (msg->Get()->uiNumRows != 0) {
        MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
        int32_t r = atoi(row[0]);
        if (r == 1) {
            RemoveAffect(AFFECT_DROP_BLOCK);
            AddAffect(AFFECT_DROP_UNBLOCK, APPLY_NONE, 0, 0, 31536000, 0, true, false);
        }
        else {
            RemoveAffect(AFFECT_DROP_UNBLOCK);
            AddAffect(AFFECT_DROP_BLOCK, APPLY_NONE, 0, 0, 31536000, 0, true, false);
        }
    }
}
#endif

void CHARACTER::OpenMyShop(const char* c_pszSign, TShopItemTable* pTable, uint8_t bItemCount
#ifdef KASMIR_PAKET_SYSTEM
    , uint32_t KasmirNpc, uint8_t KasmirBaslik
#endif
)
{
    if (!CanHandleItem())
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 291, "");
#endif
        return;
    }

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    if (GetGMLevel() > GM_PLAYER && GetGMLevel() < GM_IMPLEMENTOR) {
        return;
    }
#endif

#ifndef ENABLE_OPEN_SHOP_WITH_ARMOR
    if (GetPart(PART_MAIN) > 2)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 503, "");
#endif
        return;
    }
#endif

    if (GetMyShop())
    {
        CloseMyShop();
        return;
    }

    quest::PC* pPC = quest::CQuestManager::instance().GetPCForce(GetPlayerID());
    if (pPC->IsRunning())
        return;

    if (bItemCount == 0)
        return;

    int64_t nTotalMoney = 0;

    for (int n = 0; n < bItemCount; ++n)
    {
        nTotalMoney += static_cast<int64_t>((pTable + n)->price);
    }

    nTotalMoney += static_cast<int64_t>(GetGold());

    if (GOLD_MAX <= nTotalMoney)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 226,
            "%lld"

            , GOLD_MAX);
#endif
        return;
    }

    char szSign[SHOP_SIGN_MAX_LEN + 1];
    strlcpy(szSign, c_pszSign, sizeof(szSign));

    m_stShopSign = szSign;

    if (m_stShopSign.length() == 0)
        return;

    if (CBanwordManager::instance().CheckString(m_stShopSign.c_str(), m_stShopSign.length()))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 358, "");
#endif
        return;
    }

#ifdef KASMIR_PAKET_SYSTEM
    m_bKasmirPaketBaslik = KasmirBaslik;
    if (m_bKasmirPaketBaslik < 1 && m_bKasmirPaketBaslik > 6)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 46, "");
#endif
        return;
    }
    // LPENTITY.4-fixup.2.g: mirror legacy KASMIR title into ECS ShopState
    if (auto* shop = g_registry.try_get<ecs::ShopState>(GetEntityHandle()))
        shop->kasmirTitle = m_bKasmirPaketBaslik;
#endif

    std::map<uint32_t, uint32_t> itemkind;

    std::set<TItemPos> cont;
    for (uint8_t i = 0; i < bItemCount; ++i)
    {
        if (cont.contains((pTable + i)->pos))
        {
            LOG_ERROR("MYSHOP: duplicate shop item detected! (name: {})", GetName());
            return;
        }

        const entt::entity item = ItemSystem::GetItem(GetEntityHandle(), (pTable + i)->pos);

        if (ItemSystem::IsValidItem(item))
        {
            const TItemTable* item_table = ItemSystem::GetItemProto(item);

            if (item_table && (IS_SET(item_table->dwAntiFlags, ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_MYSHOP)))
            {
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 416, "%s", ItemSystem::GetItemName(item));
#endif
                return;
            }

            if (ItemSystem::IsItemEquipped(item) == true)
            {
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 541, "");
#endif
                return;
            }

            if (ItemSystem::IsItemLocked(item))
            {
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 656, "");
#endif
                return;
            }

			const uint32_t itemCount = ItemSystem::GetItemCount(item);
			if (itemCount == 0)
			{
				LOG_ERROR("MYSHOP: zero-count item rejected (name: {} item_id: {})",
					GetName(), ItemSystem::GetItemID(item));
				return;
			}
			itemkind[ItemSystem::GetItemVnum(item)] = (pTable + i)->price / itemCount;
        }

        cont.insert((pTable + i)->pos);
    }

    if (CountSpecifyItem(71049)
#ifdef KASMIR_PAKET_SYSTEM
        || CountSpecifyItem(88901)
#endif
        ) {
        TItemPriceListTable header;
        memset(&header, 0, sizeof(TItemPriceListTable));

        header.dwOwnerID = GetPlayerID();
        header.byCount = itemkind.size();

        size_t idx = 0;
        for (auto it = itemkind.begin(); it != itemkind.end(); ++it)
        {
            header.aPriceInfo[idx].dwVnum = it->first;
            header.aPriceInfo[idx].dwPrice = it->second;
            idx++;
        }

        db_clientdesc->DBPacket(HEADER_GD_MYSHOP_PRICELIST_UPDATE, GetDesc()->GetHandle(), &header, sizeof(TItemPriceListTable));
    }
    else if (CountSpecifyItem(50200))
        RemoveSpecifyItem(50200, 1);
    else
        return;

    if (m_pkExchange)
        m_pkExchange->Cancel();

    TPacketGCShopSign p;

    p.bHeader = HEADER_GC_SHOP_SIGN;
    p.dwVID = GetPacketVID();
    strlcpy(p.szSign, c_pszSign, sizeof(p.szSign));
#ifdef KASMIR_PAKET_SYSTEM
    p.bShopKasmirTitle = KasmirBaslik;
#endif
    PacketAround(&p, sizeof(TPacketGCShopSign));

    m_pkMyShop = CShopManager::instance().CreatePCShop(this, pTable, bItemCount);
    if (const auto e = GetEntityHandle(); e != entt::null && g_registry.valid(e))
    {
        auto& shop = g_registry.get_or_emplace<ecs::ShopState>(e);
        shop.myShop = m_pkMyShop;
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    }

    if (IsPolymorphed() == true)
    {
        RemoveAffect(AFFECT_POLYMORPH);
    }

    if (GetHorse())
    {
        HorseSummon(false, true);
    }
    else if (GetMountVnum())
    {
        RemoveAffect(AFFECT_MOUNT);
        RemoveAffect(AFFECT_MOUNT_BONUS);
    }

    uint32_t dwNpcShop = 30000;
#ifdef KASMIR_PAKET_SYSTEM
    dwNpcShop = KasmirNpc >= 30000 && KasmirNpc <= 30007 ? KasmirNpc : 30000;
#endif
    SetPolymorph(dwNpcShop, true);
}

void CHARACTER::CloseMyShop()
{
    if (GetMyShop())
    {
        m_stShopSign.clear();
        CShopManager::instance().DestroyPCShop(this);
        m_pkMyShop = nullptr;
        if (const auto e = GetEntityHandle(); e != entt::null && g_registry.valid(e))
        {
            auto& shop = g_registry.get_or_emplace<ecs::ShopState>(e);
            shop.myShop = nullptr;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
#ifdef KASMIR_PAKET_SYSTEM
        m_bKasmirPaketBaslik = 0;
        m_bKasmirPaketDurum = false;
        // LPENTITY.4-fixup.2.g: clear ECS mirror on shop close
        if (auto* shop = g_registry.try_get<ecs::ShopState>(GetEntityHandle()))
            shop->kasmirTitle = 0;
#endif

        TPacketGCShopSign p;

        p.bHeader = HEADER_GC_SHOP_SIGN;
        p.dwVID = GetPacketVID();
#ifdef KASMIR_PAKET_SYSTEM
        p.bShopKasmirTitle = m_bKasmirPaketBaslik;
#endif
        p.szSign[0] = '\0';

        PacketAround(&p, sizeof(p));
#ifdef ENABLE_WOLFMAN_CHARACTER
        SetPolymorph(m_points.job, true);
#else
        SetPolymorph(GetJob(), true);
#endif
    }
}

#ifdef __HIDE_COSTUME_SYSTEM__
void CHARACTER::SetBodyCostumeHidden(bool hidden, bool pass)
{
    m_bHideBodyCostume = hidden;
	ecs::PlayerRuntime::SetCostumeHidden(GetEntityHandle(), 1, hidden, pass);
}

bool CHARACTER::IsBodyCostumeHidden() const
{
	return ecs::PlayerRuntime::IsCostumeHidden(GetEntityHandle(), 1);
}

void CHARACTER::SetHairCostumeHidden(bool hidden, bool pass)
{
    m_bHideHairCostume = hidden;
	ecs::PlayerRuntime::SetCostumeHidden(GetEntityHandle(), 2, hidden, pass);
}

bool CHARACTER::IsHairCostumeHidden() const
{
	return ecs::PlayerRuntime::IsCostumeHidden(GetEntityHandle(), 2);
}

#ifdef ENABLE_ACCE_SYSTEM
void CHARACTER::SetAcceCostumeHidden(bool hidden, bool pass)
{
    m_bHideAcceCostume = hidden;
	ecs::PlayerRuntime::SetCostumeHidden(GetEntityHandle(), 3, hidden, pass);
}

bool CHARACTER::IsAcceCostumeHidden() const
{
	return ecs::PlayerRuntime::IsCostumeHidden(GetEntityHandle(), 3);
}
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
void CHARACTER::SetWeaponCostumeHidden(bool hidden, bool pass)
{
    m_bHideWeaponCostume = hidden;
	ecs::PlayerRuntime::SetCostumeHidden(GetEntityHandle(), 4, hidden, pass);
}

bool CHARACTER::IsWeaponCostumeHidden() const
{
	return ecs::PlayerRuntime::IsCostumeHidden(GetEntityHandle(), 4);
}
#endif
#endif

void CHARACTER::Initialize()
{
    CEntity::Initialize(ENTITY_CHARACTER);
    m_entity = entt::null;
    m_dwLegacyVID = 0;
    m_eVictim = entt::null;

    m_bNoOpenedShop = true;
#ifdef ENABLE_EVENT_MANAGER
    m_bDungeonTicketExtraMetin = false;
#endif

#ifdef ENABLE_MAP1_SKILL_MOB
    m_bSkillHit = false;
#endif
    m_bOpeningSafebox = false;
    m_lastAlignmentGrade = 255;
    m_alignBonusHP = 0;
    m_alignBonusMonster = 0;
    m_alignBonusHuman = 0;
    m_alignBonusMetin = 0;
    m_alignBonusBoss = 0;
    m_alignBonusPvm = 0;
    m_alignBonusNormal = 0;
    m_alignBonusSkill = 0;
    m_alignAppliedHP = 0;
    m_alignAppliedMonster = 0;
    m_alignAppliedHuman = 0;
    m_alignAppliedMetin = 0;
    m_alignAppliedBoss = 0;
    m_alignAppliedPvm = 0;
    m_alignAppliedNormal = 0;
    m_alignAppliedSkill = 0;

    m_fSyncTime = get_float_time() - 3;
    m_dwPlayerID = 0;
#ifdef __NEWPET_SYSTEM__
    m_stImmortalSt = 0;
    m_newpetskillcd[0] = 0;
    m_newpetskillcd[1] = 0;
    m_newpetskillcd[2] = 0;
    m_newpetskillcd[3] = 0;
#endif
    m_dwKillerPID = 0;
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
    m_posStart.x = m_posStart.y = 0;
    m_fRegenAngle = 0.0f;

    m_pkMobData = nullptr;
    m_pkMobInst = nullptr;

    m_pkShop = nullptr;
    m_pkChrShopOwner = nullptr;
    m_pkMyShop = nullptr;
    m_pkExchange = nullptr;
    m_pkParty = nullptr;
    m_pkPartyRequestEvent = nullptr;

    m_pGuild = nullptr;

    m_pkChrTarget = nullptr;

    m_pkMuyeongEvent = nullptr;
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
    m_pkGyeongGongEvent = nullptr;
#endif
    m_pkWarpNPCEvent = nullptr;
    m_pkSaveEvent = nullptr;
    m_pkTimedEvent = nullptr;
    m_pkFishingEvent = nullptr;
    m_pkWarpEvent = nullptr;
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
    m_pkBattlePassStayOnlineEvent = nullptr;
#endif

    m_pkMiningEvent = nullptr;

    m_pkAffectEvent = nullptr;
    m_afAffectFlag = TAffectFlag(0, 0);

    m_pkDestroyWhenIdleEvent = nullptr;

    m_pkChrSyncOwner = nullptr;

    memset(&m_points, 0, sizeof(m_points));
    memset(&m_pointsInstant, 0, sizeof(m_pointsInstant));
    memset(&m_quickslot, 0, sizeof(m_quickslot));

    m_bCharType = CHAR_TYPE_MONSTER;

    SetPosition(POS_STANDING);

    m_dwPlayStartTime = m_dwLastMoveTime = get_dword_time();

    EnterIdleState(GetEntityHandle());
    AISystem::GotoState(GetEntityHandle(), ecs::AIFSMState::Idle);
    m_dwStateDuration = 1;

    m_dwLastAttackTime = get_dword_time() - 20000;

    // Phase C.4: legacy m_bAddChrState zero-init removed (entity null at
    // this Initialize point - ECS StatusFlags created with default-zero
    // bits when this CHARACTER is later attached to an ECS entity).
#if defined(BL_OFFLINE_MESSAGE)
    dwLastOfflinePMTime = 0;
#endif
    m_pkChrStone = nullptr;

    m_pkSafebox = nullptr;
    m_iSafeboxSize = -1;
    m_iSafeboxLoadTime = 0;

    m_pkMountInventory = nullptr;
    m_bMountInventoryLoaded = false;

    m_pkMall = nullptr;
    m_iMallLoadTime = 0;

    m_posWarp.x = m_posWarp.y = m_posWarp.z = 0;
    m_lWarpMapIndex = 0;

    m_posExit.x = m_posExit.y = m_posExit.z = 0;
    m_lExitMapIndex = 0;

    m_pSkillLevels = nullptr;

    // Phase C.2: legacy m_dwMoveStartTime / m_dwMoveDuration zero-init
    // removed. The ECS MovementState component is created by EntityFactory
    // with default-zero timing fields when this CHARACTER is later attached
    // to an ECS entity. m_entity is entt::null at this Initialize point so
    // an ECS write here would be a no-op anyway.

    m_dwFlyTargetID = 0;

    m_dwNextStatePulse = 0;

    m_dwLastDeadTime = get_dword_time() - 180000;

    m_bSkipSave = false;

    m_bItemLoaded = false;

    m_pkDungeon = nullptr;
    m_iEventAttr = 0;

    m_kAttackLog.dwVID = 0;
    m_kAttackLog.dwTime = 0;

    // Phase C.2: legacy m_bNowWalking zero-init removed (ECS MovementState
    // default-init handles isNowWalking=false). m_bWalking still legacy.
    m_bWalking = false;
    ResetChangeAttackPositionTime();

    m_bDetailLog = false;
    m_bMonsterLog = false;

    m_bDisableCooltime = false;

    m_iAlignment = 0;
    m_iRealAlignment = 0;

    m_iKillerModePulse = 0;
    g_registry.get_or_emplace<ecs::CombatStats>(GetEntityHandle()).pkMode = PK_MODE_PEACE;

    m_dwQuestNPCVID = 0;
    m_dwQuestByVnum = 0;

    m_szMobileAuth[0] = '\0';

    m_dwUnderGuildWarInfoMessageTime = get_dword_time() - 60000;

    m_bUnderRefine = false;

    m_dwRefineNPCVID = 0;

    m_dwPolymorphRace = 0;

    m_bStaminaConsume = false;

    ResetChainLightningIndex();

    m_dwMountVnum = 0;
    m_chHorse = nullptr;
    m_chRider = nullptr;

    m_pWarMap = nullptr;
    m_pWeddingMap = nullptr;
    m_bChatCounter = 0;
#ifdef ENABLE_FAKE_SHOP_HEADER
    m_lastBeltMountCount = -999;
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
    m_pkOfflineShop = nullptr;
    m_pkShopSafebox = nullptr;
    m_pkAuction = nullptr;
    m_pkAuctionGuest = nullptr;
    m_pkOfflineShopGuest = nullptr;
    m_bIsLookingOfflineshopOfferList = false;
#endif

    ResetStopTime();
#ifdef ENABLE_GAYA_SYSTEM
    LOAD_GAYA();
#endif
    m_dwLastVictimSetTime = get_dword_time() - 3000;
    m_iMaxAggro = -100;

    m_bSendHorseLevel = 0;
    m_bSendHorseHealthGrade = 0;
    m_bSendHorseStaminaGrade = 0;

    m_dwLoginPlayTime = 0;

    m_pkChrMarried = nullptr;

    m_posSafeboxOpen.x = -1000;
    m_posSafeboxOpen.y = -1000;

    m_dwLastSkillTime = get_dword_time();

    memset(m_adwMobSkillCooltime, 0, sizeof(m_adwMobSkillCooltime));

    m_isinPCBang = false;

    m_pArena = nullptr;
    SetPotionLimit(quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count"));

    m_isOpenSafebox = 0;

    m_iRefineTime = 0;

    m_iSeedTime = 0;
    m_iExchangeTime = 0;
    m_iMyShopTime = 0;

    m_deposit_pulse = 0;

    m_strNewName = "";


    m_dwLogOffInterval = 0;

    m_bComboSequence = 0;
    m_dwLastComboTime = 0;
    m_bComboIndex = 0;
    m_iComboHackCount = 0;
    m_dwSkipComboAttackByTime = 0;

    m_dwMountTime = 0;

    m_dwLastGoldDropTime = 0;
#ifdef ENABLE_NEWSTUFF
    m_dwLastBoxUseTime = 0;
    m_dwLastBuySellTime = 0;
#endif

    m_bIsLoadedAffect = false;
    cannot_dead = false;

#ifdef __PET_SYSTEM__
    m_petSystem = nullptr;
    m_bIsPet = false;
#endif

#ifdef __NEWPET_SYSTEM__
    m_newpetSystem = nullptr;
    m_bIsNewPet = false;
    m_eggvid = 0;
#endif
    m_fAttMul = 1.0f;
    m_fDamMul = 1.0f;

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    m_mountSystem = nullptr;
    m_bIsMount = false;
#endif

#ifdef ENABLE_ANTI_CMD_FLOOD
    m_dwCmdAntiFloodCount = 0;
    m_dwCmdAntiFloodPulse = 0;
#endif
    memset(&m_tvLastSyncTime, 0, sizeof(m_tvLastSyncTime));
    m_iSyncHackCount = 0;
#ifdef ENABLE_RANKING
    for (int i = 0; i < RANKING_MAX_CATEGORIES; ++i)
        m_lRankPoints[i] = 0;
#endif

#ifdef ENABLE_ATTR_COSTUMES
    attrdialog_remove = 0;
#endif
#ifdef ENABLE_BATTLE_PASS
    m_listBattlePass.clear();
    m_bIsLoadedBattlePass = false;

    m_dwBattlePassEndTime = 0;

#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
    m_pkStayOnlineEvent = nullptr;
#endif

#endif
    m_stName = "";

#ifdef __SKILL_COLOR_SYSTEM__
    memset(&m_dwSkillColor, 0, sizeof(m_dwSkillColor));
#endif
#ifdef ENABLE_ACCE_SYSTEM
    m_bAcceCombination = false;
    m_bAcceAbsorption = false;
#endif

#ifdef __HIDE_COSTUME_SYSTEM__
    m_bHideBodyCostume = false;
    m_bHideHairCostume = false;
#ifdef ENABLE_ACCE_SYSTEM
    m_bHideAcceCostume = false;
#endif
    m_bHideWeaponCostume = false;
#endif
#ifdef ENABLE_NEW_PET_EDITS
    petenchant = 0;
#endif
#ifdef KASMIR_PAKET_SYSTEM
    m_bKasmirPaketBaslik = 0;
    m_bKasmirPaketDurum = false;
#endif
    isInvincible = false;
    m_iGoToXYTime = 0;
#ifdef ENABLE_SAVEPOINT_SYSTEM
    m_iSavePointTime = 0;
#endif
#ifdef ENABLE_SORT_INVEN
    m_iSortInv1Time = 0;
    m_iSortInv2Time = 0;
#endif
#ifdef ENABLE_LIMIT_BUY_SPEED
    m_iLastBuyTime = 0;
#endif
#ifdef __DUNGEON_INFO_SYSTEM__
    dungeonDamage.clear();
#endif
#ifdef ENABLE_SPAM_CHECK
    m_iLastUnlock = 0;
    m_iLastDSRefine = 0;
#endif
#ifdef ENABLE_ANTICHEAT
    m_firstReward = 0;
    m_rewardCount = 0;
    m_checkRepeated = 0;
    m_dropitemcount = 0;
    m_lastdropitem = 0;
#endif
#ifdef ENABLE_BLOCK_MULTIFARM
    m_pkDropEvent = nullptr;
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

LPCHARACTER DynamicCharacterPtr::Get() const {
    LPCHARACTER p = nullptr;
    if (is_pc) {
        p = CHARACTER_MANAGER::instance().FindByPID(id);
    }
    else {
        p = CHARACTER_MANAGER::instance().Find(id);
    }
    return p;
}

DynamicCharacterPtr& DynamicCharacterPtr::operator=(entt::entity e) {
	if (e == entt::null || !g_registry.valid(e)) {
		Reset();
		return *this;
	}
	// CHARACTER::IsPC() is GetDesc() != nullptr, not the TagPC component, and
	// Get() below resolves the two ids through different maps - so the test
	// has to stay the descriptor one.
	if (ecs::PlayerRuntime::GetDesc(e)) {
		is_pc = true;
		id = ecs::PlayerRuntime::GetPlayerID(e);
	}
	else {
		is_pc = false;
		id = ecs::PlayerRuntime::GetPacketVID(e);
	}
	return *this;
}

DynamicCharacterPtr& DynamicCharacterPtr::operator=(LPCHARACTER character) {
    if (character == nullptr) {
        Reset();
        return *this;
    }
    if (character->IsPC()) {
        is_pc = true;
        id = character->GetPlayerID();
    }
    else {
        is_pc = false;
        id = character->GetLegacyVID();
    }
    return *this;
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

    LPCHARACTER ch = info->ch;
    if (ch == nullptr) {
        return 0;
    }

    // Phase 10: WRITES_STATE - deferred until ECS component covers m_pkMiningEvent

    ch->m_pkMiningEvent = nullptr;
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
#ifdef ENABLE_WOLFMAN_CHARACTER
    case MAIN_RACE_WOLFMAN_M:
#endif
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

    LPCHARACTER ch = info->ch;
    if (ch == nullptr) {
        return 0;
    }

    // Phase 10: WRITES_STATE - deferred until ECS component covers m_pkDestroyWhenIdleEvent

    if (ch->GetVictim())
    {
        return PASSES_PER_SEC(300);
    }

    LOG_INFO("DESTROY_WHEN_IDLE: {}", ch->GetName());

    ch->m_pkDestroyWhenIdleEvent = nullptr;
    M2_DESTROY_CHARACTER(ch);
    return 0;
}

#ifdef ENABLE_BLOCK_MULTIFARM
EVENTFUNC(drop_event)
{
    drop_event_info* info = dynamic_cast<drop_event_info*>(event->info);
    if (!info) {
        LOG_ERROR("<drop_event> event is null.");
        return 0;
    }

    LPCHARACTER ch = info->ch;
    if (!ch) {
        LOG_ERROR("<drop_event> ch is null.");
        return 0;
    }
	const entt::entity character = ch->GetEntityHandle();

    LPDESC d = ecs::PlayerRuntime::GetDesc(character);
    if (!d) {
        LOG_ERROR("<drop_event> {} have no desc connector.", ch->GetName());
        return 0;
    }

    // Phase 10: WRITES_STATE - deferred until ECS component covers m_pkDropEvent

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

        ch->BlockProcessed();
    }

    return PASSES_PER_SEC(1);
}
#endif


