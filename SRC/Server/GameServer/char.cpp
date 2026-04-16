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

void CHARACTER::Initialize()
{

	CEntity::Initialize(ENTITY_CHARACTER);

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
	m_posStart.x = m_posStart.y = 0;
	m_posDest.x = m_posDest.y = 0;
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
	m_pkDeadEvent = nullptr;
	m_pkStunEvent = nullptr;
	m_pkSaveEvent = nullptr;
	m_pkRecoveryEvent = nullptr;
	m_pkTimedEvent = nullptr;
	m_pkFishingEvent = nullptr;
	m_pkWarpEvent = nullptr;
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
	//m_dwBattlePassStayOnlineNextTick = 0;
	m_pkBattlePassStayOnlineEvent = nullptr;
#endif

	// MINING
	m_pkMiningEvent = nullptr;
	// END_OF_MINING

	m_pkPoisonEvent = nullptr;
#ifdef ENABLE_WOLFMAN_CHARACTER
	m_pkBleedingEvent = NULL;
#endif
	m_pkFireEvent = nullptr;
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

	EnterIdleState(this);
	m_dwStateDuration = 1;

	m_dwLastAttackTime = get_dword_time() - 20000;

	m_bAddChrState = 0;
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

	m_dwMoveStartTime = 0;
	m_dwMoveDuration = 0;

	m_dwFlyTargetID = 0;

	m_dwNextStatePulse = 0;

	m_dwLastDeadTime = get_dword_time() - 180000;

	m_bSkipSave = false;

	m_bItemLoaded = false;

	m_bHasPoisoned = false;
#ifdef ENABLE_WOLFMAN_CHARACTER
	m_bHasBled = false;
#endif
	m_pkDungeon = nullptr;
	m_iEventAttr = 0;

	m_kAttackLog.dwVID = 0;
	m_kAttackLog.dwTime = 0;

	m_bNowWalking = m_bWalking = false;
	ResetChangeAttackPositionTime();

	m_bDetailLog = false;
	m_bMonsterLog = false;

	m_bDisableCooltime = false;

	m_iAlignment = 0;
	m_iRealAlignment = 0;

	m_iKillerModePulse = 0;
	m_bPKMode = PK_MODE_PEACE;

	m_dwQuestNPCVID = 0;
	m_dwQuestByVnum = 0;
	m_pQuestItem = nullptr;

	m_szMobileAuth[0] = '\0';

	m_dwUnderGuildWarInfoMessageTime = get_dword_time() - 60000;

	m_bUnderRefine = false;

	// REFINE_NPC
	m_dwRefineNPCVID = 0;
	// END_OF_REFINE_NPC

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
	//offlineshop-updated 03/08/19
	m_pkOfflineShopGuest = nullptr;

	//offlineshop-updated 05/08/19
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

	// EQUIP_LAST_SKILL_DELAY
	m_dwLastSkillTime = get_dword_time();
	// END_OF_EQUIP_LAST_SKILL_DELAY

	// MOB_SKILL_COOLTIME
	memset(m_adwMobSkillCooltime, 0, sizeof(m_adwMobSkillCooltime));
	// END_OF_MOB_SKILL_COOLTIME

	m_isinPCBang = false;

	// ARENA
	m_pArena = nullptr;
	m_nPotionLimit = quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count");
	// END_ARENA

	//PREVENT_TRADE_WINDOW
	m_isOpenSafebox = 0;
	//END_PREVENT_TRADE_WINDOW

	//PREVENT_REFINE_HACK
	m_iRefineTime = 0;
	//END_PREVENT_REFINE_HACK

	//RESTRICT_USE_SEED_OR_MOONBOTTLE
	m_iSeedTime = 0;
	//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
	//PREVENT_PORTAL_AFTER_EXCHANGE
	m_iExchangeTime = 0;
	//END_PREVENT_PORTAL_AFTER_EXCHANGE
	//
	m_iMyShopTime = 0;

	m_deposit_pulse = 0;

	

	m_strNewName = "";

	m_known_guild.clear();

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

	m_pointsInstant.iDragonSoulActiveDeck = -1;

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
#ifdef ENABLE_NEW_FISHING_SYSTEM
	m_pkFishingNewEvent = nullptr;
	m_bFishCatch = 0;
	m_dwLastCatch = 0;
	m_dwCatchFailed = 0;
#endif
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

void CHARACTER::Create(const char* c_pszName, uint32_t vid, bool isPC)
{
	static int s_crc = 172814;

	char crc_string[128 + 1];
	snprintf(crc_string, sizeof(crc_string), "%s%p%d", c_pszName, this, ++s_crc);
	m_vid = VID(vid, GetCRC32(crc_string, strlen(crc_string)));
	if (isPC)
		m_stName = c_pszName;
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

void CHARACTER::OpenMyShop(const char* c_pszSign, TShopItemTable* pTable, uint8_t bItemCount
#ifdef KASMIR_PAKET_SYSTEM
	, uint32_t KasmirNpc, uint8_t KasmirBaslik
#endif
)
{
	if (!CanHandleItem())
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 291, "");
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
		ChatPacketNew(CHAT_TYPE_INFO, 503, "");
#endif
		return;
	}
#endif

	if (GetMyShop())	// AI1I 1YAI ?­·Á AÖA¸¸é ´Ý´Â´U.
	{
		CloseMyShop();
		return;
	}

	// ÁoÇaÁßAÎ Äu1oA®°! AÖA¸¸é »óÁ!A» ?­ 1ö 3o´U.
	quest::PC* pPC = quest::CQuestManager::instance().GetPCForce(GetPlayerID());

	// GetPCForce´Â NULLAI 1ö 3oA¸1Ç·Î µu·Î E®AÎÇIÁö 3EA1
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
		ChatPacketNew(CHAT_TYPE_INFO, 226,
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
		ChatPacketNew(CHAT_TYPE_INFO, 358, "");
#endif
		return;
	}

#ifdef KASMIR_PAKET_SYSTEM
	m_bKasmirPaketBaslik = KasmirBaslik;
	if (m_bKasmirPaketBaslik < 1 && m_bKasmirPaketBaslik > 6)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 46, "");
#endif
		return;
	}
#endif

	// MYSHOP_PRICE_LIST
	std::map<uint32_t, uint32_t> itemkind;  // 3AAIAU Á3·uo° °!°Ý, first: vnum, second: ´ÜAI 1ö·® °!°Ý
	// END_OF_MYSHOP_PRICE_LIST

	std::set<TItemPos> cont;
	for (uint8_t i = 0; i < bItemCount; ++i)
	{
		if (cont.contains((pTable + i)->pos))
		{
			sys_err("MYSHOP: duplicate shop item detected! (name: %s)", GetName());
			return;
		}

		// ANTI_GIVE, ANTI_MYSHOP check
		LPITEM pkItem = GetItem((pTable + i)->pos);

		if (pkItem)
		{
			const TItemTable* item_table = pkItem->GetProto();

			if (item_table && (IS_SET(item_table->dwAntiFlags, ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_MYSHOP)))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 416, "%s", pkItem->GetName());
#endif
				return;
			}

			if (pkItem->IsEquipped() == true)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 541, "");
#endif
				return;
			}

			if (true == pkItem->isLocked())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 656, "");
#endif
				return;
			}

			// MYSHOP_PRICE_LIST
			itemkind[pkItem->GetVnum()] = (pTable + i)->price / pkItem->GetCount();
			// END_OF_MYSHOP_PRICE_LIST
		}

		cont.insert((pTable + i)->pos);
	}

	// MYSHOP_PRICE_LIST
	// o¸µu¸® °31ö¸¦ °¨1O1AA2´U.
	if (CountSpecifyItem(71049)
#ifdef KASMIR_PAKET_SYSTEM
		|| CountSpecifyItem(88901)
#endif
		) { // on´Ü o¸µu¸®´Â 3o3ÖÁö 3E°í °!°ÝÁ¤o¸¸¦ AúAaÇN´U.

		//
		// 3AAIAU °!°ÝÁ¤o¸¸¦ AúAaÇI±â A§ÇO 3AAIAU °!°ÝÁ¤o¸ A?A¶A» ¸¸µé3î DB Ä31A?! o¸31´U.
		//
		// @fixme403 BEGIN
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
		// @fixme403 END
	}
	// END_OF_MYSHOP_PRICE_LIST
	else if (CountSpecifyItem(50200))
		RemoveSpecifyItem(50200, 1);
	else
		return; // o¸µu¸®°! 3oA¸¸é Áß´Ü.

	if (m_pkExchange)
		m_pkExchange->Cancel();

	TPacketGCShopSign p;

	p.bHeader = HEADER_GC_SHOP_SIGN;
	p.dwVID = GetVID();
	strlcpy(p.szSign, c_pszSign, sizeof(p.szSign));
#ifdef KASMIR_PAKET_SYSTEM
	p.bShopKasmirTitle = KasmirBaslik;
#endif
	PacketAround(&p, sizeof(TPacketGCShopSign));

	m_pkMyShop = CShopManager::instance().CreatePCShop(this, pTable, bItemCount);

	if (IsPolymorphed() == true)
	{
		RemoveAffect(AFFECT_POLYMORPH);
	}

	if (GetHorse())
	{
		HorseSummon(false, true);
	}
	// new mount AI?ë Áß?!, °3AÎ »óÁ! ?­¸é AÚµ? unmount
	// StopRidingA¸·Î ´o¸¶?îA®±îÁö A3¸®ÇI¸é ÁÁAoµY ?Ö ±×·¸°Ô 3EÇO3u´ÂÁö 3Ë 1ö 3o´U.
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
#ifdef KASMIR_PAKET_SYSTEM
		m_bKasmirPaketBaslik = 0;
		m_bKasmirPaketDurum = false;
#endif

		TPacketGCShopSign p;

		p.bHeader = HEADER_GC_SHOP_SIGN;
		p.dwVID = GetVID();
#ifdef KASMIR_PAKET_SYSTEM
		p.bShopKasmirTitle = m_bKasmirPaketBaslik;
#endif
		p.szSign[0] = '\0';

		PacketAround(&p, sizeof(p));
#ifdef ENABLE_WOLFMAN_CHARACTER
		SetPolymorph(m_points.job, true);
		// SetPolymorph(0, true);
#else
		SetPolymorph(GetJob(), true);
#endif
	}
}

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

bool CHARACTER::IsNextMountPulse() const { return (m_mountPulse == 0 || (m_mountPulse < thecore_pulse())); }
void CHARACTER::UpdateMountPulse() { m_mountPulse = thecore_pulse() + THECORE_SECS_TO_PASSES(1); }

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



void CHARACTER::SetPosition(int pos)
{
	if (pos == POS_STANDING)
	{
		REMOVE_BIT(m_bAddChrState, ADD_CHARACTER_STATE_DEAD);
		REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_STUN);

		event_cancel(&m_pkDeadEvent);
		event_cancel(&m_pkStunEvent);
	}
	else if (pos == POS_DEAD)
		SET_BIT(m_bAddChrState, ADD_CHARACTER_STATE_DEAD);

	if (!IsStone())
	{
		switch (pos)
		{
		case POS_FIGHTING:
			if (!HasCombatState(this))
				MonsterLog("[BATTLE] 1Î?i´Â »óAÂ");

			EnterBattleState(this);
			break;

		default:
			if (!HasIdleState(this))
				MonsterLog("[IDLE] 1¬´Â »óAÂ");

			EnterIdleState(this);
			break;
		}
	}

	m_pointsInstant.position = pos;
}

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
















bool CHARACTER::Return()
{
	if (!IsNPC())
		return false;

	int x, y;
	/*
	   float fDist = DISTANCE_SQRT(m_pkMobData->m_posLastAttacked.x - GetX(), m_pkMobData->m_posLastAttacked.y - GetY());
	   float fx, fy;
	   GetDeltaByDegree(GetRotation(), fDist, &fx, &fy);
	   x = GetX() + (int) fx;
	   y = GetY() + (int) fy;
	 */
	SetVictim(nullptr);

	x = m_pkMobInst->m_posLastAttacked.x;
	y = m_pkMobInst->m_posLastAttacked.y;

	SetRotationToXY(x, y);

	if (!Goto(x, y))
		return false;

	SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);

	if (test_server)
		sys_log(0, "%s %p A÷±âÇI°í µ13A°!AÚ! %d %d", GetName(), this, x, y);

	if (GetParty())
		GetParty()->SendMessage(this, PM_RETURN, x, y);

	return true;
}

bool CHARACTER::Follow(LPCHARACTER pkChr, float fMinDistance)
{

	if (IsPC())
	{
		sys_err("CHARACTER::Follow : PC cannot use this method", GetName());
		return false;
	}

	// TRENT_MONSTER
	if (IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_NOMOVE))
	{
		if (pkChr->IsPC()) // ÂN3A°!´Â »ó´ë°! PCAI ¶§
		{
			// If i'm in a party. I must obey party leader's AI.
			if (!GetParty() || !GetParty()->GetLeader() || GetParty()->GetLeader() == this)
			{
				if (get_dword_time() - m_pkMobInst->m_dwLastAttackedTime >= 15000) // ¸¶Áö¸·A¸·Î °o°Ý1?AoÁö 15AE°! Áö3µ°í
				{
					// ¸¶Áö¸· ¸ÂAo °÷A¸·Î oÎAÍ 501IAÍ AI»ó Â÷AI3a¸é A÷±âÇI°í µ13A°L´U.
					if (m_pkMobData->m_table.wAttackRange < DISTANCE_APPROX(pkChr->GetX() - GetX(), pkChr->GetY() - GetY()))
						if (Return())
							return true;
				}
			}
		}
		return false;
	}
	// END_OF_TRENT_MONSTER

	int32_t x = pkChr->GetX();
	int32_t y = pkChr->GetY();

	if (pkChr->IsPC()) // ÂN3A°!´Â »ó´ë°! PCAI ¶§
	{
		// If i'm in a party. I must obey party leader's AI.
		if (!GetParty() || !GetParty()->GetLeader() || GetParty()->GetLeader() == this)
		{
			if (get_dword_time() - m_pkMobInst->m_dwLastAttackedTime >= 15000) // ¸¶Áö¸·A¸·Î °o°Ý1?AoÁö 15AE°! Áö3µ°í
			{
				// ¸¶Áö¸· ¸ÂAo °÷A¸·Î oÎAÍ 501IAÍ AI»ó Â÷AI3a¸é A÷±âÇI°í µ13A°L´U.
				if (5000 < DISTANCE_APPROX(m_pkMobInst->m_posLastAttacked.x - GetX(), m_pkMobInst->m_posLastAttacked.y - GetY()))
					if (Return())
						return true;
			}
		}
	}

#ifndef ENABLE_BUG_FIXES
	if (IsGuardNPC())
	{
		if (5000 < DISTANCE_APPROX(m_pkMobInst->m_posLastAttacked.x - GetX(), m_pkMobInst->m_posLastAttacked.y - GetY()))
			if (Return())
				return true;
	}
#endif

#ifdef __NEWPET_SYSTEM__
	if (HasMoveState(pkChr) &&
		GetMobBattleType() != BATTLE_TYPE_RANGE &&
		GetMobBattleType() != BATTLE_TYPE_MAGIC &&
		false == IsPet() && false == IsNewPet()
#else
	if (HasMoveState(pkChr) &&
		GetMobBattleType() != BATTLE_TYPE_RANGE &&
		GetMobBattleType() != BATTLE_TYPE_MAGIC &&
		false == IsPet()
#endif
		)


	{
		// ´ë»óAI AIµ?ÁßAI¸é ?1Ao AIµ?A» ÇN´U
		// 3a?Í »ó´ë1aAÇ 1ÓµµÂ÷?Í °A¸®·ÎoÎAÍ ¸¸3- 1A°LA» ?1»óÇN EÄ
		// »ó´ë1aAI ±× 1A°L±îÁö Á÷1±A¸·Î AIµ?ÇN´U°í °!Á¤ÇI?© °A±â·Î AIµ?ÇN´U.
		float rot = pkChr->GetRotation();
		float rot_delta = GetDegreeDelta(rot, GetDegreeFromPositionXY(GetX(), GetY(), pkChr->GetX(), pkChr->GetY()));

		float yourSpeed = pkChr->GetMoveSpeed();
		float mySpeed = GetMoveSpeed();

		float fDist = DISTANCE_SQRT(x - GetX(), y - GetY());
		float fFollowSpeed = mySpeed - yourSpeed * cos(rot_delta * M_PI / 180);

		if (fFollowSpeed >= 0.1f)
		{
			float fMeetTime = fDist / fFollowSpeed;
			float fYourMoveEstimateX, fYourMoveEstimateY;

			if (fMeetTime * yourSpeed <= 100000.0f)
			{
				GetDeltaByDegree(pkChr->GetRotation(), fMeetTime * yourSpeed, &fYourMoveEstimateX, &fYourMoveEstimateY);

				x += (int32_t)fYourMoveEstimateX;
				y += (int32_t)fYourMoveEstimateY;

				float fDistNew = sqrt(((double)x - GetX()) * (x - GetX()) + ((double)y - GetY()) * (y - GetY()));
				if (fDist < fDistNew)
				{
					x = (int32_t)(GetX() + (x - GetX()) * fDist / fDistNew);
					y = (int32_t)(GetY() + (y - GetY()) * fDist / fDistNew);
				}
			}
		}
	}

	// °!·Á´Â A§Ä!¸¦ 1U¶óoÁ3ß ÇN´U.
	SetRotationToXY(x, y);

	float fDist = DISTANCE_SQRT(x - GetX(), y - GetY());

	if (fDist <= fMinDistance)
		return false;

	float fx, fy;

	if (IsChangeAttackPosition(pkChr) && GetMobRank() < MOB_RANK_BOSS)
	{
		// »ó´ë1a ÁÖo- ·L´ýÇN °÷A¸·Î AIµ?
		SetChangeAttackPositionTime();

		int retry = 16;
		int dx, dy;
		int rot = (int)GetDegreeFromPositionXY(x, y, GetX(), GetY());

		while (--retry)
		{
			if (fDist < 500.0f)
				GetDeltaByDegree((rot + number(-90, 90) + number(-90, 90)) % 360, fMinDistance, &fx, &fy);
			else
				GetDeltaByDegree(number(0, 359), fMinDistance, &fx, &fy);

			dx = x + (int)fx;
			dy = y + (int)fy;

			LPSECTREE tree = SECTREE_MANAGER::instance().Get(GetMapIndex(), dx, dy);

			if (nullptr == tree)
				break;

			if (0 == (tree->GetAttribute(dx, dy) & (ATTR_BLOCK | ATTR_OBJECT)))
				break;
		}

		//sys_log(0, "±UA3 3îµo°!·Î AIµ? %s retry %d", GetName(), retry);
		if (!Goto(dx, dy))
			return false;
	}
	else
	{
		// Á÷1± µu¶ó°!±â
		float fDistToGo = fDist - fMinDistance;
		GetDeltaByDegree(GetRotation(), fDistToGo, &fx, &fy);

		//sys_log(0, "Á÷1±A¸·Î AIµ? %s", GetName());
		if (!Goto(GetX() + (int)fx, GetY() + (int)fy))
			return false;
	}

	SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
	//MonsterLog("ÂN3A°!±â; %s", pkChr->GetName());
	return true;
}



int CHARACTER::GetLeadershipSkillLevel() const
{
	return GetSkillLevel(SKILL_LEADERSHIP);
}











void CHARACTER::ReviveInvisible(int iDur)
{
	AddAffect(AFFECT_REVIVE_INVISIBLE, POINT_NONE, 0, AFF_REVIVE_INVISIBLE, iDur, 0, true);
}




void CHARACTER::CowardEscape()
{
	int iDist[4] = {500, 1000, 3000, 5000};

	for (int iDistIdx = 2; iDistIdx >= 0; --iDistIdx)
		for (int iTryCount = 0; iTryCount < 8; ++iTryCount)
		{
			SetRotation(number(0, 359));

			float fx, fy;
			float fDist = number(iDist[iDistIdx], iDist[iDistIdx + 1]);

			GetDeltaByDegree(GetRotation(), fDist, &fx, &fy);

			bool bIsWayBlocked = false;
			for (int j = 1; j <= 100; ++j)
			{
				if (!SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), GetX() + (int)fx * j / 100, GetY() + (int)fy * j / 100))
				{
					bIsWayBlocked = true;
					break;
				}
			}

			if (bIsWayBlocked)
				continue;

			m_dwStateDuration = PASSES_PER_SEC(1);

			int iDestX = GetX() + (int)fx;
			int iDestY = GetY() + (int)fy;

			if (Goto(iDestX, iDestY))
				SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);

			sys_log(0, "WAEGU move to %d %d (far)", iDestX, iDestY);
			return;
		}
}




void CHARACTER::DetermineDropMetinStone()
{
#ifdef ENABLE_NEWSTUFF
	if (g_NoDropMetinStone)
	{
		m_dwDropMetinStone = 0;
		return;
	}
#endif

	static const uint32_t c_adwMetin[] =
	{
#if defined(ENABLE_WOLFMAN_CHARACTER) && defined(USE_WOLFMAN_STONES)
		28012,
#endif
		28030,
		28031,
		28032,
		28033,
		28034,
		28035,
		28036,
		28037,
		28038,
		28039,
		28040,
		28041,
		28042,
		28043,
#if defined(ENABLE_MAGIC_REDUCTION_SYSTEM) && defined(USE_MAGIC_REDUCTION_STONES)
		28044,
		28045,
#endif
	};
	uint32_t stone_num = GetRaceNum();
	int idx = std::lower_bound(aStoneDrop, aStoneDrop + STONE_INFO_MAX_NUM, stone_num) - aStoneDrop;
	if (idx >= STONE_INFO_MAX_NUM || aStoneDrop[idx].dwMobVnum != stone_num)
	{
		m_dwDropMetinStone = 0;
	}
	else
	{
		const SStoneDropInfo& info = aStoneDrop[idx];
		m_bDropMetinStonePct = info.iDropPct;
		{
			m_dwDropMetinStone = c_adwMetin[number(0, sizeof(c_adwMetin) / sizeof(uint32_t) - 1)];
			int iGradePct = number(1, 100);
			for (int iStoneLevel = 0; iStoneLevel < STONE_LEVEL_MAX_NUM; iStoneLevel++)
			{
				int iLevelGradePortion = info.iLevelPct[iStoneLevel];
				if (iGradePct <= iLevelGradePortion)
				{
					break;
				}
				else
				{
					iGradePct -= iLevelGradePortion;
					m_dwDropMetinStone += 100; // µ1 +a -> +(a+1)AI µÉ¶§¸¶´U 1003? Áo°!
				}
			}
		}
	}
}

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

bool CHARACTER::CanSummon(int iLeaderShip)
{
	return ((iLeaderShip >= 20) || ((iLeaderShip >= 12) && ((m_dwLastDeadTime + 180) > get_dword_time())));
}

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


uint8_t CHARACTER::GetMountCounter() const
{
	return m_bMountCounter;
}

void CHARACTER::ResetMountCounter()
{
	m_bMountCounter = 0;
}

uint8_t CHARACTER::IncreaseMountCounter()
{
	return ++m_bMountCounter;
}

// ¸»AI3a ´U¸Y°ÍA» A¸°í AÖ3a?
bool CHARACTER::IsRiding() const
{
	return IsHorseRiding() || GetMountVnum();
}

#ifdef __NEWPET_SYSTEM__
#endif

#ifdef ENABLE_RANKING


#endif

#ifdef __ENABLE_NEW_OFFLINESHOP__
#endif







#ifdef ENABLE_ACCE_SYSTEM





#endif

#ifdef __HIDE_COSTUME_SYSTEM__
void CHARACTER::SetBodyCostumeHidden(bool hidden, bool pass)
{
	m_bHideBodyCostume = hidden;
	ChatPacket(CHAT_TYPE_COMMAND, "SetBodyCostumeHidden %d", m_bHideBodyCostume ? 1 : 0);
	if (!pass) {
		SetQuestFlag("costume_option.hide_body", m_bHideBodyCostume ? 1 : 0);
	}
}

void CHARACTER::SetHairCostumeHidden(bool hidden, bool pass)
{
	m_bHideHairCostume = hidden;
	ChatPacket(CHAT_TYPE_COMMAND, "SetHairCostumeHidden %d", m_bHideHairCostume ? 1 : 0);
	if (!pass) {
		SetQuestFlag("costume_option.hide_hair", m_bHideHairCostume ? 1 : 0);
	}
}

#ifdef ENABLE_ACCE_SYSTEM
void CHARACTER::SetAcceCostumeHidden(bool hidden, bool pass)
{
	m_bHideAcceCostume = hidden;
	ChatPacket(CHAT_TYPE_COMMAND, "SetAcceCostumeHidden %d", m_bHideAcceCostume ? 1 : 0);
	if (!pass) {
		SetQuestFlag("costume_option.hide_acce", m_bHideAcceCostume ? 1 : 0);
	}
}
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
void CHARACTER::SetWeaponCostumeHidden(bool hidden, bool pass)
{
	m_bHideWeaponCostume = hidden;
	ChatPacket(CHAT_TYPE_COMMAND, "SetWeaponCostumeHidden %d", m_bHideWeaponCostume ? 1 : 0);
	if (!pass) {
		SetQuestFlag("costume_option.hide_weapon", m_bHideWeaponCostume ? 1 : 0);
	}
}
#endif
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
void CHARACTER::MountSummon(LPITEM mountItem)
{
#define MOUNT_SYSTEM_FIX_POLY
#ifdef MOUNT_SYSTEM_FIX_POLY
	if (IsPolymorphed() == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 732, "");
#endif
		return;
	}
#endif	
	if (GetMapIndex() == 113)
		return;

	if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
		return;

	CMountSystem* mountSystem = GetMountSystem();
	uint32_t mobVnum = 0;

	if (!mountSystem || !mountItem)
		return;

#ifdef __CHANGELOOK_SYSTEM__	
	if (mountItem->GetTransmutation())
	{
		const TItemTable* itemTable = ITEM_MANAGER::instance().GetTable(mountItem->GetTransmutation());

		if (itemTable)
			mobVnum = itemTable->alValues[1];
		else
			mobVnum = mountItem->GetValue(1);
	}
	else
		mobVnum = mountItem->GetValue(1);
#else
	if (mountItem->GetValue(1) != 0)
		mobVnum = mountItem->GetValue(1);
#endif

	if (IsHorseRiding())
		StopRiding();

	if (GetHorse())
		HorseSummon(false);

	mountSystem->Summon(mobVnum, mountItem, false);
}

void CHARACTER::MountUnsummon(LPITEM mountItem)
{
	CMountSystem* mountSystem = GetMountSystem();
	uint32_t mobVnum = 0;

	if (!mountSystem || !mountItem)
		return;

#ifdef __CHANGELOOK_SYSTEM__	
	if (mountItem->GetTransmutation())
	{
		const TItemTable* itemTable = ITEM_MANAGER::instance().GetTable(mountItem->GetTransmutation());

		if (itemTable)
			mobVnum = itemTable->alValues[1];
		else
			mobVnum = mountItem->GetValue(1);
	}
	else
		mobVnum = mountItem->GetValue(1);
#else
	if (mountItem->GetValue(1) != 0)
		mobVnum = mountItem->GetValue(1);
#endif

	if (GetMountVnum() == mobVnum)
		mountSystem->Unmount(mobVnum);

	mountSystem->Unsummon(mobVnum);
}

void CHARACTER::CheckMount()
{
	CMountSystem* mountSystem = GetMountSystem();
	LPITEM mountItem = GetWear(WEAR_COSTUME_MOUNT);
	uint32_t mobVnum = 0;

	if (!mountSystem || !mountItem)
		return;

#ifdef __CHANGELOOK_SYSTEM__	
	if (mountItem->GetTransmutation())
	{
		const TItemTable* itemTable = ITEM_MANAGER::instance().GetTable(mountItem->GetTransmutation());

		if (itemTable)
			mobVnum = itemTable->alValues[1];
		else
			mobVnum = mountItem->GetValue(1);
	}
	else
		mobVnum = mountItem->GetValue(1);
#else
	if (mountItem->GetValue(1) != 0)
		mobVnum = mountItem->GetValue(1);
#endif

	if (mountSystem->CountSummoned() == 0)
	{
		mountSystem->Summon(mobVnum, mountItem, false);
	}
}

bool CHARACTER::IsRidingMount()
{
	return (GetWear(WEAR_COSTUME_MOUNT) || FindAffect(AFFECT_MOUNT));
}
#endif

#ifdef ENABLE_COSTUME_PET
void CHARACTER::UpdatePetSkin() {
	if (!m_petSystem)
		return;

	m_petSystem->UpdatePetSkin();
}

uint32_t CHARACTER::GetPetSkinVnum() {
	LPITEM item = GetWear(WEAR_COSTUME_PET_SKIN);
	return item != nullptr ? item->GetValue(0) : 0;
}
#endif

#ifdef ENABLE_COSTUME_MOUNT
void CHARACTER::UpdateMountSkin() {
	if (!m_mountSystem)
		return;

	m_mountSystem->UpdateMountSkin();

	if (IsRiding()) {
		LPITEM item = GetWear(WEAR_COSTUME_MOUNT);
		if (!item)
			return;

		uint32_t mobVnum = 0;
#ifdef __CHANGELOOK_SYSTEM__
		if (item->GetTransmutation())
		{
			const TItemTable* itemTable = ITEM_MANAGER::instance().GetTable(item->GetTransmutation());
			if (itemTable)
				mobVnum = itemTable->alValues[1];
			else
				mobVnum = item->GetValue(1);
		}
		else
			mobVnum = item->GetValue(1);
#else
		if (item->GetValue(1) != 0)
			mobVnum = item->GetValue(1);
#endif

		m_mountSystem->Unmount(mobVnum);
		m_mountSystem->Mount(mobVnum, item);
	}
}

uint32_t CHARACTER::GetMountSkinVnum() {
	LPITEM item = GetWear(WEAR_COSTUME_MOUNT_SKIN);
	return item != nullptr ? item->GetValue(0) : 0;
}
#endif

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
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

void CHARACTER::ComputeMountInventoryBonuses()
{
	std::map<uint8_t, int32_t> mount_bonus_map;

	CMountInventory* mi = GetMountInventory();
	if (!mi)
		return;

	const auto& valid_items = CMountInventoryHelper::GetAllowedItems();
	const int total = mi->GetWidth() * mi->GetSize();

	for (int pos = 0; pos < total; ++pos)
	{
		LPITEM item = mi->Get(pos);
		if (!item)
			continue;

		const uint32_t vnum = item->GetVnum();
		if (!valid_items.contains(vnum))
			continue;

		const TItemTable* proto = item->GetProto();
		if (!proto)
			continue;

		// 1) item_proto apply 
		for (const auto& apply : proto->aApplies)
		{
			if (apply.bType == APPLY_NONE || apply.lValue == 0)
				continue;

			if (apply.bType >= MAX_APPLY_NUM)
				continue;

			const uint8_t pointType = aApplyInfo[apply.bType].bPointType;
			if (pointType != POINT_NONE)
				mount_bonus_map[pointType] += apply.lValue;
		}

		// 2) item attribute 
		const TPlayerItemAttribute* attrs = item->GetAttributes();
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const uint8_t bType = attrs[i].bType;
			const int16_t sVal = attrs[i].sValue;

			if (bType == APPLY_NONE || sVal == 0)
				continue;

			if (bType >= MAX_APPLY_NUM)
				continue;

			const uint8_t pointType = aApplyInfo[bType].bPointType;
			if (pointType != POINT_NONE)
				mount_bonus_map[pointType] += sVal;
		}
	}

	for (const auto& it : mount_bonus_map)
		PointChange(it.first, it.second);
}







