#include "stdafx.h"


#include <common/VnumHelper.h>

#include "char.h"

#include "config.h"
#include "utils.h"
#include "crc32.h"
#include "char_manager.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "buffer_manager.h"
#include "item_manager.h"
#include "motion.h"
#include "vector.h"
#include "packet.h"
#include "cmd.h"
#include "fishing.h"
#include "exchange.h"
#include "battle.h"
#include "affect.h"
#include "shop.h"
#include "shop_manager.h"
#include "safebox.h"
#include "MountInventory.h"
#include "regen.h"
#include "pvp.h"
#include "party.h"
#include "start_position.h"
#include "questmanager.h"
#include "log.h"
#include "p2p.h"
#include "guild.h"
#include "guild_manager.h"
#include "dungeon.h"
#include "messenger_manager.h"
#include "unique_item.h"
#include "priv_manager.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/VIDRegistry.hpp"
#include "ecs/components/combat_components.hpp"
#include "ecs/components/dirty_components.hpp"
#include "ecs/components/identity_components.hpp"
#include "ecs/components/movement_components.hpp"
#include "war_map.h"
#include "banword.h"
#include "target.h"
#include "wedding.h"
#include "mob_manager.h"
#include "mining.h"
#include "arena.h"
#include "dev_log.h"
#include "horsename_manager.h"
#include "pcbang.h"
#include "gm.h"
#include "map_location.h"
#include "skill_power.h"
#include "buff_on_attributes.h"
#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
#include "MountSystem.h"
#endif

#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

#ifdef __PET_SYSTEM__
#include "PetSystem.h"
#endif
#ifdef __NEWPET_SYSTEM__
#include "New_PetSystem.h"
#endif
#include <boost/algorithm/string/find.hpp>

#include "DragonSoul.h"
#include <common/CommonDefines.h>

#include "Poly/Constants.h"
#ifdef __SEND_TARGET_INFO__
#include <algorithm>
#include <iterator>
#endif
#ifdef ENABLE_SWITCHBOT
#include "new_switchbot.h"
#endif
#ifdef ENABLE_RUNE_SYSTEM
#include <common/rune_length.h>
#endif
#ifdef ENABLE_STOLE_COSTUME
#include <common/stole_length.h>
#endif
#include "mount_inventory_helper.h"
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
#include "OrcsDungeon.h"
#include "TritonTempleDungeon.h"
#include "ValentineDungeon.h"
#include "RuneDungeon.h"
#include "PyramidDungeonRazor93.h"
#include "NightmareDungeonRazor93.h"
#include "Halloween2022Dungeon.h"
#include "VikingDungeon.h"
#include "EasterDungeon.h"
#endif
//#include "LostCastleDungeon.h"
using namespace std;



namespace
{
	inline entt::entity EcsEntityOf(const CHARACTER* ch)
	{
		if (!ch)
			return entt::null;

		return CVIDRegistry::Instance().Find(ch->GetVID());
	}

	inline bool HasCombatState(const CHARACTER* ch)
	{
		const entt::entity e = EcsEntityOf(ch);
		return e != entt::null && g_registry.valid(e) &&
			g_registry.all_of<ecs::CombatActiveTag>(e);
	}

	inline bool HasMoveState(const CHARACTER* ch)
	{
		const entt::entity e = EcsEntityOf(ch);
		return e != entt::null && g_registry.valid(e) &&
			g_registry.all_of<ecs::MovementDestination>(e);
	}

	inline bool HasIdleState(const CHARACTER* ch)
	{
		const entt::entity e = EcsEntityOf(ch);
		if (e == entt::null || !g_registry.valid(e))
			return true;

		return !g_registry.all_of<ecs::CombatActiveTag>(e) &&
			!g_registry.all_of<ecs::MovementDestination>(e);
	}

	inline void EnterIdleState(CHARACTER* ch)
	{
		const entt::entity e = EcsEntityOf(ch);
		if (e == entt::null || !g_registry.valid(e))
			return;

		g_registry.remove<ecs::CombatActiveTag>(e);
		g_registry.remove<ecs::CombatTarget>(e);
		g_registry.remove<ecs::MovementDestination>(e);
	}

	inline void EnterBattleState(CHARACTER* ch)
	{
		const entt::entity e = EcsEntityOf(ch);
		if (e == entt::null || !g_registry.valid(e))
			return;

		g_registry.emplace_or_replace<ecs::CombatActiveTag>(e);
	}
	}




extern const uint8_t g_aBuffOnAttrPoints;
extern bool RaceToJob(unsigned race, unsigned* ret_job);

// <Factor> DynamicCharacterPtr member function definitions

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
		id = character->GetVID();
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



#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
EVENTFUNC(battle_pass_stay_online_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);
	if (!info || !info->ch)
		return 0;

	LPCHARACTER ch = info->ch;

	if (!ch->GetDesc())
		return PASSES_PER_SEC(60);

	const uint8_t bBattlePassId = ch->GetBattlePassId();
	if (!bBattlePassId)
		return PASSES_PER_SEC(60);

	uint32_t dwNotUsed = 0, dwCount = 0;
	if (!CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, STAY_ONLINE_MINUTES, &dwNotUsed, &dwCount))
		return PASSES_PER_SEC(60);

	if (ch->IsCompletedMission(STAY_ONLINE_MINUTES))
		return PASSES_PER_SEC(60);

	if (ch->GetMissionProgress(STAY_ONLINE_MINUTES, bBattlePassId) >= dwCount)
		return PASSES_PER_SEC(60);

	// Nálad ez a helyes progress növelés
	ch->UpdateMissionProgress(STAY_ONLINE_MINUTES, bBattlePassId, 1, dwCount);

	return PASSES_PER_SEC(60);
}
#endif

#ifdef ENABLE_PVP_ADVANCED
#endif



void EncodeMovePacket(TPacketGCMove& pack, uint32_t dwVID, uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y, uint32_t dwDuration, uint32_t dwTime, float bRot)
{
	pack.bHeader = HEADER_GC_MOVE;
	pack.bFunc = bFunc;
	pack.bArg = bArg;
	pack.dwVID = dwVID;
	pack.dwTime = dwTime ? dwTime : get_dword_time();
	pack.bRot = bRot;
	pack.lX = x;
	pack.lY = y;
	pack.dwDuration = dwDuration;
}


// #define ENABLE_SHOWNPCLEVEL
// Entity?! 3»°! 3aA¸3µ´U°í A?A¶A» o¸31´U.
#ifdef ENABLE_FAKE_SHOP_HEADER
//#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
#ifdef DISABLE_CORE_PULSE_RAZOR93


template <typename ...Args>
void SendI18nChatPacket(CHARACTER* ch, uint8_t type, const char* format, Args ... args)
{
	const auto locale = GetLocale(ch);
	std::string resultString;
	try { resultString = fmt::sprintf(locale->stringTable.Translate(format), std::forward<Args>(args)...); }
	catch (const fmt::format_error& err) { resultString = locale->stringTable.Translate(format); }
	SendChatPacket(ch, type, resultString);
}
#endif
//void CHARACTER::UpdateMountCountOverhead(LPCHARACTER viewer)
//{
//	if (!IsPC()) // Én magam játékos vagyok-e?
//		return;
//
//	if (!viewer->IsPC()) // Aki kapja, az is játékos legyen
//		return;
//
//	if (!viewer->GetDesc()) // Kell hogy legyen kliens socket
//		return;
//
//	int beltItemCount = 0;
//	for (int i = BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END; ++i)
//	{
//		if (GetInventoryItem(i))
//			++beltItemCount;
//	}
//
//	TPacketGCFakeShopSign p;
//	p.bHeader = HEADER_GC_FAKE_SHOP_SIGN;
//
//	p.dwVID = GetVID(); // ÉN vagyok a tulaj
//	p.iMountCount = beltItemCount;
//
//	viewer->GetDesc()->Packet(&p, sizeof(p));
//}



#endif


//void CHARACTER::UpdateBeltCountToClients()
//{
//	std::string updatedName = GetDisplayedNameWithBeltCount();
//
//	// Ha nem változott, ne küldj semmit
//	if (updatedName == m_strLastSentDisplayedNameWithBelt)
//		return;
//
//	TPacketGCBeltNameUpdate p;
//	memset(&p, 0, sizeof(p));
//	p.header = HEADER_GC_BELT_NAME_UPDATE;
//	strncpy(p.name, updatedName.c_str(), sizeof(p.name) - 1);
//
//	// Saját kliensnek
//	if (GetDesc())
//		GetDesc()->Packet(&p, sizeof(p));
//
//
//	m_strLastSentDisplayedNameWithBelt = updatedName;
//}


//void CHARACTER::UpdateBeltCountToClients()
//{
//	std::string updatedName = GetDisplayedNameWithBeltCount();
//
//	TPacketGCBeltNameUpdate p;
//	memset(&p, 0, sizeof(p));
//
//	p.header = HEADER_GC_BELT_NAME_UPDATE; // <- EZ A SOR ITT KEL
//	strncpy(p.name, updatedName.c_str(), sizeof(p.name) - 1);
//
//	if (GetDesc())
//		GetDesc()->Packet(&p, sizeof(p));
//}
//void CHARACTER::UpdateBeltCountToClients()
//{
//	std::string updatedName = GetDisplayedNameWithBeltCount();
//
//	sys_log(0, "DEBUG: UpdateBeltCountToClients called, sending name: %s", updatedName.c_str());
//
//	TPacketGCBeltNameUpdate p;
//	memset(&p, 0, sizeof(p));
//
//	p.header = HEADER_GC_BELT_NAME_UPDATE;
//	strncpy(p.name, updatedName.c_str(), sizeof(p.name) - 1);
//
//	if (GetDesc())
//	{
//		sys_log(0, "DEBUG: Sending belt name update packet to client");
//		GetDesc()->Packet(&p, sizeof(p));
//	}
//	else
//	{
//		sys_log(0, "WARNING: GetDesc() is null, cannot send belt name update");
//	}
//}


//#endif




#ifdef ENABLE_FAKE_SHOP_HEADERd

EVENTINFO(update_mount_count_event_info)
{
	LPCHARACTER ch;
};

EVENTFUNC(UpdateMountCountEvent)
{
	update_mount_count_event_info* info = dynamic_cast<update_mount_count_event_info*>(event->info);
	if (!info || !info->ch)
		return 0;

	info->ch->UpdateMountCountOverhead(info->ch);
	info->ch->UpdateMountInventoryCountOverhead(info->ch);

	return 0;
}

#endif


#define ENABLE_GM_FLAG_IF_TEST_SERVER
#define ENABLE_GM_FLAG_FOR_LOW_WIZARD

EVENTFUNC(kill_ore_load_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);
	if (info == nullptr)
	{
		sys_err("kill_ore_load_even> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER	ch = info->ch;
	if (ch == nullptr) { // <Factor>
		return 0;
	}

	ch->m_pkMiningEvent = nullptr;
	M2_DESTROY_CHARACTER(ch);
	return 0;
}


const int aiRecoveryPercents[10] = { 1, 5, 5, 5, 5, 5, 5, 5, 5, 5 };

EVENTFUNC(recovery_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);
	if (info == nullptr)
	{
		sys_err("recovery_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER	ch = info->ch;

	if (ch == nullptr) { // <Factor>
		return 0;
	}

	if (!ch->IsPC())
	{
		//
		// ¸ó1oAÍ E¸o1
		//
		if (ch->IsAffectFlag(AFF_POISON))
			return PASSES_PER_SEC(max((uint8_t)1, ch->GetMobTable().bRegenCycle));

#ifdef ENABLE_WOLFMAN_CHARACTER
		if (ch->IsAffectFlag(AFF_BLEEDING))
			return PASSES_PER_SEC(MAX(1, ch->GetMobTable().bRegenCycle));
#endif

#ifdef ENABLE_DS_RUNE
		if (ch->GetMobTable().dwVnum == 3996) {
			LPDUNGEON target = ch->GetDungeon();
			if (target) {
				if (target->GetFlag("floor") == 5) {
					ch->DistributeSP(ch);
					if (ch->GetMaxHP() <= ch->GetHP())
						return PASSES_PER_SEC(3);

					int iPercent = 0;
					int iAmount = 0;

					{
						iPercent = 2;
						iAmount = 15 + (ch->GetMaxHP() * iPercent) / 100;
					}

					iAmount += (iAmount * ch->GetPoint(POINT_HP_REGEN)) / 100;
					sys_log(1, "RECOVERY_EVENT: %s %d HP_REGEN %d HP +%d", ch->GetName(), iPercent, ch->GetPoint(POINT_HP_REGEN), iAmount);
					ch->PointChange(POINT_HP, iAmount, false);
					return PASSES_PER_SEC(10);
				}
			}
		}
		else if (ch->GetMobTable().dwVnum == 8202) {
			LPDUNGEON target = ch->GetDungeon();
			if (target) {
				if (target->GetFlag("floor") == 1) {
					ch->DistributeSP(ch);
					if (ch->GetMaxHP() <= ch->GetHP())
						return PASSES_PER_SEC(3);

					int iPercent = 0;
					int iAmount = 0;

					{
						iPercent = 2;
						iAmount = 15 + (ch->GetMaxHP() * iPercent) / 100;
					}

					iAmount += (iAmount * ch->GetPoint(POINT_HP_REGEN)) / 100;
					sys_log(1, "RECOVERY_EVENT: %s %d HP_REGEN %d HP +%d", ch->GetName(), iPercent, ch->GetPoint(POINT_HP_REGEN), iAmount);
					ch->PointChange(POINT_HP, iAmount, false);
					return PASSES_PER_SEC(10);
				}
			}
		}
#endif

		if (!ch->IsDoor())
		{
			ch->MonsterLog("HP_REGEN +%d", max((int64_t)1, (ch->GetMaxHP() * ch->GetMobTable().bRegenPercent) / 100));
			ch->PointChange(POINT_HP, max((int64_t)1, (ch->GetMaxHP() * ch->GetMobTable().bRegenPercent) / 100));
		}

		if (ch->GetHP() >= ch->GetMaxHP())
		{
			ch->m_pkRecoveryEvent = nullptr;
			return 0;
		}

		return PASSES_PER_SEC(max((uint8_t)1, ch->GetMobTable().bRegenCycle));
	}
	else
	{
		//
		// PC E¸o1
		//
		ch->CheckTarget();
		//ch->UpdateSectree(); // ?©±â1­ AI°É ?ÖÇIÁö?
		ch->UpdateKillerMode();

		if (ch->IsAffectFlag(AFF_POISON) == true)
		{
			// Áßµ¶AÎ °a?i AÚµ?E¸o1 ±ÝÁö
			// AÄ1ý1úAÎ °a?i AÚµ?E¸o1 ±ÝÁö
			return 3;
		}
#ifdef ENABLE_WOLFMAN_CHARACTER
		if (ch->IsAffectFlag(AFF_BLEEDING))
			return 3;
#endif
		int iSec = (get_dword_time() - ch->GetLastMoveTime()) / 3000;

		// SP E¸o1 ·çA3.
		// ?Ö AI°É·Î ÇO1­ ÇÔ1ö·Î »©3u´Â°! ?!
		ch->DistributeSP(ch);

		if (ch->GetMaxHP() <= ch->GetHP())
			return PASSES_PER_SEC(3);

		int iPercent = 0;
		int iAmount = 0;

		{
			iPercent = aiRecoveryPercents[min(9, iSec)];
			iAmount = 15 + (ch->GetMaxHP() * iPercent) / 100;
		}

		iAmount += (iAmount * ch->GetPoint(POINT_HP_REGEN)) / 100;

		sys_log(1, "RECOVERY_EVENT: %s %d HP_REGEN %d HP +%d", ch->GetName(), iPercent, ch->GetPoint(POINT_HP_REGEN), iAmount);

		ch->PointChange(POINT_HP, iAmount, false);
		return PASSES_PER_SEC(3);
	}
}




// MINING






// Ä3¸-AÍ AÎ1oAI1o 3÷µYAIA® ÇÔ1ö.





















/* void CHARACTER::DetermineDropMetinStofa() {//@RAzor93
	static const uint32_t c_adwMetin[] = {
										80019,
										80022,
										80023,
										80024,
										80025,
										80026,
										80027,
	};

	uint32_t stone_num = GetRaceNum();
	int idx = std::lower_bound(aStofaDrop, aStofaDrop+STONE_STOFA_INFO_MAX_NUM, stone_num) - aStofaDrop;
	if (idx >= STONE_STOFA_INFO_MAX_NUM || aStofaDrop[idx].dwMobVnum != stone_num) {
		m_dwDropMetinStofa = 0;
	} else {
		const SStofaDropInfo & info = aStofaDrop[idx];
		int random = number(0, sizeof(c_adwMetin)/sizeof(uint32_t) - 1);
		m_dwDropMetinStofa = c_adwMetin[random];
		m_bDropMetinStofaPct = info.iChance[random];
	}
} */

/* void CHARACTER::DetermineDropMetinSacca() {//@Razor93
	static const uint32_t c_adwMetin[] = {
										30094,
										30095,
										30096,
	};

	uint32_t stone_num = GetRaceNum();
	int idx = std::lower_bound(aSaccaDrop, aSaccaDrop+STONE_SACCA_INFO_MAX_NUM, stone_num) - aSaccaDrop;
	if (idx >= STONE_SACCA_INFO_MAX_NUM || aSaccaDrop[idx].dwMobVnum != stone_num) {
		m_dwDropMetinSacca = 0;
	} else {
		const SSaccaDropInfo & info = aSaccaDrop[idx];
		int random = number(0, sizeof(c_adwMetin) / sizeof(uint32_t) - 1);
		m_dwDropMetinSacca = c_adwMetin[random];
		m_bDropMetinSaccaPct = info.iChance[random];
	}
} */


#ifdef ENABLE_PVP_ADVANCED	

#endif




// ADD_REFINE_BUILDING

// END_OF_ADD_REFINE_BUILDING

//Hack 1aÁö¸¦ A§ÇN A1A©.


//------------------------------------------------
//------------------------------------------------

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

	/* default sex = male */
	return SEX_MALE;
}


EVENTFUNC(destroy_when_idle_event)
{
	const auto info = dynamic_cast<char_event_info*>(event->info);
	if (info == nullptr)
	{
		sys_err("destroy_when_idle_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER ch = info->ch;
	if (ch == nullptr) { // <Factor>
		return 0;
	}

	if (ch->GetVictim())
	{
		return PASSES_PER_SEC(300);
	}

	sys_log(1, "DESTROY_WHEN_IDLE: %s", ch->GetName());

	ch->m_pkDestroyWhenIdleEvent = nullptr;
	M2_DESTROY_CHARACTER(ch);
	return 0;
}



#ifdef __NEWPET_SYSTEM__
#endif

#ifdef ENABLE_RANKING


#endif

#ifdef __ENABLE_NEW_OFFLINESHOP__
#endif







#ifdef ENABLE_ACCE_SYSTEM





#endif




//__ENABLE_NEW_OFFLINESHOP__


#ifdef ENABLE_SORT_INVEN













#endif




#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
#endif



#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
EVENTFUNC(stay_online_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);
	if (info == nullptr)
	{
		sys_err("<stay_online_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER	ch = info->ch;
	if (ch == nullptr) { // <Factor>
		return 0;
	}

	uint8_t bBattlePassId = ch->GetBattlePassId();
	if (bBattlePassId)
	{
		uint32_t dwMinutes, dwNotUsed;
		if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, STAY_ONLINE_MINUTES, &dwNotUsed, &dwMinutes))
		{
			if (ch->GetMissionProgress(STAY_ONLINE_MINUTES, bBattlePassId) < dwMinutes)
			{
				ch->UpdateMissionProgress(STAY_ONLINE_MINUTES, bBattlePassId, 1, dwMinutes);
				return PASSES_PER_SEC(60);
			}
		}
	}

	ch->m_pkStayOnlineEvent = nullptr;
	return 0;
}

#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM



#endif

#ifdef ENABLE_BLOCK_MULTIFARM

EVENTFUNC(drop_event)
{
	drop_event_info* info = dynamic_cast<drop_event_info*>(event->info);
	if (!info) {
		sys_err("<drop_event> event is null.");
		return 0;
	}

	LPCHARACTER	ch = info->ch;
	if (!ch) {
		sys_err("<drop_event> ch is null.");
		return 0;
	}

	LPDESC d = ch->GetDesc();
	if (!d) {
		sys_err("<drop_event> %s have no desc connector.", ch->GetName());
		return 0;
	}

	time_t diff = info->time - get_global_time();
	if (diff > 0) {
#ifdef TEXTS_IMPROVEMENT
		ch->ChatPacketNew(CHAT_TYPE_INFO, 43, "%d", diff);
#endif
	}
	else {
		std::string login = ch->GetDesc()->GetAccountTable().login;
		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT status FROM account.antifarm WHERE login='%s'", login.c_str()));
		if (msg->Get()->uiNumRows > 0) {
			MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
			int iStatus = atoi(row[0]);
			bool already = false;
			if (info->drop) {
				if (iStatus == 1) {
					already = true;
#ifdef TEXTS_IMPROVEMENT
					ch->ChatPacketNew(CHAT_TYPE_INFO, 38, "");
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
						ch->ChatPacketNew(CHAT_TYPE_INFO, 37, "");
#endif
					}
					else {
						ch->RemoveAffect(AFFECT_DROP_BLOCK);
						ch->AddAffect(AFFECT_DROP_UNBLOCK, APPLY_NONE, 0, 0, 31536000, 0, true, false);
#ifdef TEXTS_IMPROVEMENT
						ch->ChatPacketNew(CHAT_TYPE_INFO, 40, "");
#endif
					}
				}
			}
			else {
				if (iStatus == 0) {
					already = true;
#ifdef TEXTS_IMPROVEMENT
					ch->ChatPacketNew(CHAT_TYPE_INFO, 39, "");
#endif
				}
				else {
					ch->RemoveAffect(AFFECT_DROP_UNBLOCK);
					ch->AddAffect(AFFECT_DROP_BLOCK, APPLY_NONE, 0, 0, 31536000, 0, true, false);
#ifdef TEXTS_IMPROVEMENT
					ch->ChatPacketNew(CHAT_TYPE_INFO, 41, "");
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








