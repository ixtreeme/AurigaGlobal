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

void CHARACTER::Destroy()
{
	// Phase 7: destroy parallel ECS entity
	{
		entt::entity e = CVIDRegistry::Instance().Find(GetVID());
		if (e != entt::null)
			EntityFactory::Destroy(g_registry, e);
	}

	CloseMyShop();

	if (m_pkRegen)
	{
		if (m_pkDungeon) {
			// Dungeon regen may not be valid at this point
			if (m_pkDungeon->IsValidRegen(m_pkRegen, regen_id_)) {
				--m_pkRegen->count;
			}
		}
		else {
			// Is this really safe?
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
	}
#endif

#ifdef __NEWPET_SYSTEM__
	if (m_newpetSystem)
	{
		m_newpetSystem->Destroy();
		delete m_newpetSystem;

		m_newpetSystem = nullptr;
	}
#endif

	HorseSummon(false);

	if (GetRider())
		GetRider()->ClearHorseInfo();

	if (GetDesc())
	{
		GetDesc()->BindCharacter(nullptr);
		//		BindDesc(NULL);
	}

	if (m_pkExchange)
		m_pkExchange->Cancel();

	SetVictim(nullptr);

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

	// <Factor> m_pkParty becomes NULL after CParty destructor call!
	LPPARTY party = m_pkParty;
	if (party)
	{
		if (party->GetLeaderPID() == GetVID() && !IsPC())
		{
			M2_DELETE(party);
		}
		else
		{
			party->Unlink(this);

			if (!IsPC())
				party->Quit(GetVID());
		}

		SetParty(nullptr); // 3EÇOµµ µÇÁö¸¸ 3EAüÇI°Ô.
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
	event_cancel(&m_pkRecoveryEvent);
	event_cancel(&m_pkDeadEvent);
	event_cancel(&m_pkSaveEvent);
	event_cancel(&m_pkTimedEvent);
	event_cancel(&m_pkStunEvent);
	event_cancel(&m_pkFishingEvent);
	event_cancel(&m_pkPoisonEvent);
#ifdef ENABLE_WOLFMAN_CHARACTER
	event_cancel(&m_pkBleedingEvent);
#endif
	event_cancel(&m_pkFireEvent);
	event_cancel(&m_pkPartyRequestEvent);
	//DELAYED_WARP
	event_cancel(&m_pkWarpEvent);
	//END_DELAYED_WARP
#ifdef ENABLE_NEW_FISHING_SYSTEM
	event_cancel(&m_pkFishingNewEvent);
#endif
	// RECALL_DELAY
	//event_cancel(&m_pkRecallEvent);
	// END_OF_RECALL_DELAY
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
	if (m_pkBattlePassStayOnlineEvent)
	{
		event_cancel(&m_pkBattlePassStayOnlineEvent);
		m_pkBattlePassStayOnlineEvent = nullptr;
	}
#endif

	// MINING
	event_cancel(&m_pkMiningEvent);
	// END_OF_MINING
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
	//event_cancel(&m_pkAffectEvent);
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

	if (GetSectree())
		GetSectree()->RemoveEntity(this);

	if (m_bMonsterLog)
		CHARACTER_MANAGER::instance().UnregisterForMonsterLog(this);
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
			ChatPacket(CHAT_TYPE_COMMAND, szBuf);

			for (size_t i = 0; i < _countof(szTableStaticPvP); i++)
			{
				SetQuestFlag(szTableStaticPvP[i], 0);	//codice di merda indovina... ... il ciclo for e sprecato qui o sbaglio?
			}
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

void CHARACTER::RestartAtSamePos()
{
	if (m_bIsObserver)
		return;

	EncodeRemovePacket(this);
	EncodeInsertPacket(this);

	ENTITY_MAP::iterator it = m_map_view.begin();

	while (it != m_map_view.end())
	{
		LPENTITY entity = (it++)->first;

		EncodeRemovePacket(entity);
		if (!m_bIsObserver)
			EncodeInsertPacket(entity);

		if (entity->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER lpChar = (LPCHARACTER)entity;
			if (lpChar->IsPC() || lpChar->IsNPC() || lpChar->IsMonster())
			{
				if (!entity->IsObserverMode())
					entity->EncodeInsertPacket(this);
			}
		}
		else
		{
			if (!entity->IsObserverMode())
			{
				entity->EncodeInsertPacket(this);
			}
		}
	}
}

// #define ENABLE_SHOWNPCLEVEL
// Entity?! 3»°! 3aA¸3µ´U°í A?A¶A» o¸31´U.
void CHARACTER::EncodeInsertPacket(LPENTITY entity) {
	LPDESC d;
	if (!(d = entity->GetDesc()))
		return;

	LPCHARACTER ch = (LPCHARACTER)entity;
	ch->SendGuildName(GetGuild());

#ifdef ENABLE_SOUL_SYSTEM
	TAffectFlag sendAffectFlag = m_afAffectFlag;
	if (sendAffectFlag.IsSet(AFF_SOUL_RED) && sendAffectFlag.IsSet(AFF_SOUL_BLUE))
	{
		sendAffectFlag.Reset(AFF_SOUL_RED);
		sendAffectFlag.Reset(AFF_SOUL_BLUE);
		sendAffectFlag.Set(AFF_SOUL_MIX);
	}
#endif

	TPacketGCCharacterAdd pack;
	pack.header = HEADER_GC_CHARACTER_ADD;
	pack.dwVID = m_vid;
	pack.bType = GetCharType();
	pack.angle = GetRotation();
	pack.x = GetX();
	pack.y = GetY();
	pack.z = GetZ();
	pack.wRaceNum = GetRaceNum();
	if ((pack.wRaceNum >= 20101 && pack.wRaceNum <= 20109) || IsPet()
#ifdef __NEWPET_SYSTEM__
		|| IsNewPet()
#endif
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		|| m_bIsMount == true
#endif
		)
	{
#ifdef ENABLE_MULTI_NAMES
		pack.transname = false;
#endif
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		if (m_bIsMount == true) {
			pack.bMovingSpeed = GetLimitPoint(POINT_MOV_SPEED);
		}
		else {
			pack.bMovingSpeed = IsPC() ? GetLimitPoint(POINT_MOV_SPEED) : 150;
		}
#else
		pack.bMovingSpeed = 150;
#endif
	}
	else {
#ifdef ENABLE_MULTI_NAMES
		pack.transname = true;
#endif
		pack.bMovingSpeed = GetLimitPoint(POINT_MOV_SPEED);
	}
	pack.bAttackSpeed = GetLimitPoint(POINT_ATT_SPEED);
#ifdef ENABLE_SOUL_SYSTEM
	pack.dwAffectFlag[0] = sendAffectFlag.bits[0];
	pack.dwAffectFlag[1] = sendAffectFlag.bits[1];
#else
	pack.dwAffectFlag[0] = m_afAffectFlag.bits[0];
	pack.dwAffectFlag[1] = m_afAffectFlag.bits[1];
#endif

	pack.bStateFlag = m_bAddChrState;

	int iDur = 0;
	if (m_posDest.x != pack.x || m_posDest.y != pack.y) {
		iDur = (m_dwMoveStartTime + m_dwMoveDuration) - get_dword_time();
		if (iDur <= 0) {
			pack.x = m_posDest.x;
			pack.y = m_posDest.y;
		}
	}

	d->Packet(&pack, sizeof(pack));
	if (IsPC() == true || m_bCharType == CHAR_TYPE_NPC) {
		TPacketGCCharacterAdditionalInfo addPacket;
		addPacket.dwLevel = 0;
		addPacket.sAlignment = 0;
		addPacket.dwMountVnum = 0;
#ifdef ENABLE_MULTI_LANGUAGE
		addPacket.bLanguage = 0;
#endif
		if (!IsPC()) {
			memcpy(addPacket.dwSkillColor, GetSkillColor(), sizeof(addPacket.dwSkillColor));
		}

		addPacket.header = HEADER_GC_CHAR_ADDITIONAL_INFO;
		addPacket.dwVID = m_vid;
		addPacket.bPKMode = m_bPKMode;
		addPacket.bEmpire = m_bEmpire;
		addPacket.dwGuildID = 0;
		//#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
		//		std::string sNameWithCount = GetDisplayedNameWithBeltCount();
		//		strlcpy(addPacket.name, sNameWithCount.c_str(), sizeof(addPacket.name));
		//#else
		strlcpy(addPacket.name, GetName(), sizeof(addPacket.name));
		//std::string sNameWithCount = GetDisplayedNameWithBeltCount();
		//strlcpy(addPacket.name, sNameWithCount.c_str(), sizeof(addPacket.name));
		addPacket.awPart[CHR_EQUIPPART_ARMOR] = GetPart(PART_MAIN);
		addPacket.awPart[CHR_EQUIPPART_WEAPON] = GetPart(PART_WEAPON);
		addPacket.awPart[CHR_EQUIPPART_HEAD] = GetPart(PART_HEAD);
		addPacket.awPart[CHR_EQUIPPART_HAIR] = GetPart(PART_HAIR);
#ifdef ENABLE_RUNE_SYSTEM
		addPacket.awPart[CHR_EQUIPPART_RUNE] = GetPart(PART_RUNE);
#endif
#ifdef ENABLE_ACCE_SYSTEM
		addPacket.awPart[CHR_EQUIPPART_ACCE] = GetPart(PART_ACCE);
#endif
#ifdef ENABLE_COSTUME_EFFECT
		addPacket.awPart[CHR_EQUIPPART_EFFECT_BODY] = GetPart(PART_EFFECT_BODY);
		addPacket.awPart[CHR_EQUIPPART_EFFECT_WEAPON] = GetPart(PART_EFFECT_WEAPON);
#endif
		if (IsPC()) {
			addPacket.dwLevel = GetLevel();

			addPacket.dwMountVnum = GetMountVnum();
			addPacket.dwGuildID = GetGuild() ? GetGuild()->GetID() : 0;
			addPacket.sAlignment = m_iAlignment / 10;

#ifdef __SKILL_COLOR_SYSTEM__
			memcpy(addPacket.dwSkillColor, GetSkillColor(), sizeof(addPacket.dwSkillColor));
#endif


		}
#ifdef __NEWPET_SYSTEM__
		if (IsNewPet()) {
			addPacket.dwLevel = GetLevel();
		}
#else
#endif
		d->Packet(&addPacket, sizeof(TPacketGCCharacterAdditionalInfo));
}

	if (iDur) {
		TPacketGCMove pack;
		EncodeMovePacket(pack, GetVID(), FUNC_MOVE, 0, m_posDest.x, m_posDest.y, iDur, 0, (GetRotation() / 5));
		d->Packet(&pack, sizeof(pack));

		TPacketGCWalkMode p;
		p.vid = GetVID();
		p.header = HEADER_GC_WALK_MODE;
		p.mode = m_bNowWalking ? WALKMODE_WALK : WALKMODE_RUN;

		d->Packet(&p, sizeof(p));
	}

	if (entity->IsType(ENTITY_CHARACTER) && GetDesc()) {
		LPCHARACTER ch = (LPCHARACTER)entity;
		if (ch->IsWalking()) {
			TPacketGCWalkMode p;
			p.vid = ch->GetVID();
			p.header = HEADER_GC_WALK_MODE;
			p.mode = ch->m_bNowWalking ? WALKMODE_WALK : WALKMODE_RUN;
			GetDesc()->Packet(&p, sizeof(p));
		}
	}

	if (GetMyShop()) {
		TPacketGCShopSign p;
		p.bHeader = HEADER_GC_SHOP_SIGN;
		p.dwVID = GetVID();
#ifdef KASMIR_PAKET_SYSTEM
		p.bShopKasmirTitle = m_bKasmirPaketBaslik;
#endif
		strlcpy(p.szSign, m_stShopSign.c_str(), sizeof(p.szSign));

		d->Packet(&p, sizeof(TPacketGCShopSign));
	}

	if (entity->IsType(ENTITY_CHARACTER)) {
		sys_log(3, "EntityInsert %s (RaceNum %d) (%d %d) TO %s",
			GetName(), GetRaceNum(), GetX() / SECTREE_SIZE, GetY() / SECTREE_SIZE, ((LPCHARACTER)entity)->GetName());
	}
#ifdef ENABLE_FAKE_SHOP_HEADER
	// Csak akkor fusson, ha ÉN (this) PC vagyok és a nézo is PC!
	if (IsPC() && entity->IsType(ENTITY_CHARACTER))
	{
		LPCHARACTER viewer = (LPCHARACTER)entity;
		if (viewer->IsPC() && viewer->GetDesc())
			//UpdateMountCountOverhead(viewer);
			UpdateMountInventoryCountOverhead(viewer);
	}
#endif

}
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
void CHARACTER::EncodeRemovePacket(LPENTITY entity)
{
	if (entity->GetType() != ENTITY_CHARACTER)
		return;

	LPDESC d;

	if (!(d = entity->GetDesc()))
		return;

	TPacketGCCharacterDelete pack;

	pack.header = HEADER_GC_CHARACTER_DEL;
	pack.id = m_vid;

	d->Packet(&pack, sizeof(TPacketGCCharacterDelete));

	if (entity->IsType(ENTITY_CHARACTER))
		sys_log(3, "EntityRemove %s(%d) FROM %s", GetName(), (uint32_t)m_vid, ((LPCHARACTER)entity)->GetName());
}


LPCHARACTER CHARACTER::FindCharacterInView(const char* c_pszName, bool bFindPCOnly)
{
	ENTITY_MAP::iterator it = m_map_view.begin();

	for (; it != m_map_view.end(); ++it)
	{
		if (!it->first->IsType(ENTITY_CHARACTER))
			continue;

		LPCHARACTER tch = (LPCHARACTER)it->first;

		if (bFindPCOnly && tch->IsNPC())
			continue;

		if (!strcasecmp(tch->GetName(), c_pszName))
			return (tch);
	}

	return nullptr;
}

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
void CHARACTER::SetPlayerProto(const TPlayerTable* t)
{
	if (!GetDesc() || !*GetDesc()->GetHostName())
		sys_err("cannot get desc or hostname");
	else
		SetGMLevel();

	m_bCharType = CHAR_TYPE_PC;

	m_dwPlayerID = t->id;

	m_iAlignment = t->lAlignment;
	m_iRealAlignment = t->lAlignment;

	m_points.voice = t->voice;

	m_points.skill_group = t->skill_group;

	m_pointsInstant.bBasePart = t->part_base;
	SetPart(PART_HAIR, t->parts[PART_HAIR]);
#ifdef ENABLE_ACCE_SYSTEM
	SetPart(PART_ACCE, t->parts[PART_ACCE]);
#endif

	m_points.iRandomHP = t->sRandomHP;
	m_points.iRandomSP = t->sRandomSP;

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
	SetXYZ(t->x, t->y, t->z);

	ComputePoints();

	SetHP(t->hp);
	SetSP(t->sp);
	SetStamina(t->stamina);

	//GMAI¶§ o¸EL¸?µa
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
			m_bPKMode = PK_MODE_PROTECT;
		}
	}




	if (GetLevel() < PK_PROTECT_LEVEL)
		m_bPKMode = PK_MODE_PROTECT;

	m_stMobile = t->szMobile;

	SetHorseData(t->horse);

	if (GetHorseLevel() > 0)
		UpdateHorseDataByLogoff(t->logoff_interval);

	memcpy(m_aiPremiumTimes, t->aiPremiumTimes, sizeof(t->aiPremiumTimes));

	m_dwLogOffInterval = t->logoff_interval;

	sys_log(0, "PLAYER_LOAD: %s PREMIUM %d %d, LOGGOFF_INTERVAL %u PTR: %p", t->name, m_aiPremiumTimes[0], m_aiPremiumTimes[1], t->logoff_interval, this);

	if (GetGMLevel() != GM_PLAYER)
	{
		LogManager::instance().CharLog(this, GetGMLevel(), "GM_LOGIN", "");
		sys_log(0, "GM_LOGIN(gmlevel=%d, name=%s(%d), pos=(%d, %d)", GetGMLevel(), GetName(), GetPlayerID(), GetX(), GetY());
	}

#ifdef ENABLE_RANKING
	for (int i = 0; i < RANKING_MAX_CATEGORIES; ++i)
		m_lRankPoints[i] = t->lRankPoints[i];
#endif

#ifdef __PET_SYSTEM__
	// NOTE: AI´Ü Ä3¸-AÍ°! PCAÎ °a?i?!¸¸ PetSystemA» °®µµ·I ÇÔ. A-·´ ¸Ó1A´ç ¸?¸?¸® »ç?ë·ü¶§1®?! NPC±îÁö ÇI±ä Á»..
	if (m_petSystem)
	{
		m_petSystem->Destroy();
		delete m_petSystem;
	}

	m_petSystem = M2_NEW CPetSystem(this);
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
#endif
}

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

void CHARACTER::SetProto(const CMob* pkMob)
{
	if (m_pkMobInst)
		M2_DELETE(m_pkMobInst);

	m_pkMobData = pkMob;
	m_pkMobInst = M2_NEW CMobInstance;

	m_bPKMode = PK_MODE_FREE;

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

	////////////////////
	m_pointsInstant.dwAIFlag = t->dwAIFlag;
	SetImmuneFlag(t->dwImmuneFlag);

	AssignTriggers(t);

	ApplyMobAttribute(t);

	if (IsStone())
	{
		DetermineDropMetinStone();
		//DetermineDropMetinStofa(); @Razor93
		//DetermineDropMetinSacca(); @Razor93
	}

	if (IsWarp() || IsGoto())
	{
		StartWarpNPCEvent();
	}

	CHARACTER_MANAGER::instance().RegisterRaceNumMap(this);

	// MINING
	if (mining::IsVeinOfOre(GetRaceNum()))
	{
		char_event_info* info = AllocEventInfo<char_event_info>();

		info->ch = this;

		m_pkMiningEvent = event_create(kill_ore_load_event, info, PASSES_PER_SEC(number(7 * 60, 15 * 60)));
	}
	// END_OF_MINING
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


void CHARACTER::MonsterLog(const char* format, ...)
{
	if (!test_server)
		return;

	if (IsPC())
		return;

	char chatbuf[CHAT_MAX_LEN + 1];
	int len = snprintf(chatbuf, sizeof(chatbuf), "%lu)", static_cast<unsigned long>(GetVID()));

	if (len < 0 || len >= (int)sizeof(chatbuf))
		len = sizeof(chatbuf) - 1;

	va_list args;

	va_start(args, format);

	int len2 = vsnprintf(chatbuf + len, sizeof(chatbuf) - len, format, args);

	if (len2 < 0 || len2 >= (int)sizeof(chatbuf) - len)
		len += (sizeof(chatbuf) - len) - 1;
	else
		len += len2;

	// \0 1®AÚ A÷ÇÔ
	++len;

	va_end(args);

	TPacketGCChat pack_chat;

	pack_chat.header = HEADER_GC_CHAT;
	pack_chat.size = sizeof(TPacketGCChat) + len;
	pack_chat.type = CHAT_TYPE_TALKING;
	pack_chat.id = GetVID();
	pack_chat.bEmpire = 0;

	TEMP_BUFFER buf;
	buf.write(&pack_chat, sizeof(TPacketGCChat));
	buf.write(chatbuf, len);

	CHARACTER_MANAGER::instance().PacketMonsterLog(this, buf.read_peek(), buf.size());
}

void CHARACTER::ChatPacket(uint8_t type, const char* format, ...)
{
	LPDESC d = GetDesc();
	if (!d || !format)
		return;

	char chatbuf[CHAT_MAX_LEN + 1];
	va_list args;
	va_start(args, format);
	int len = vsnprintf(chatbuf, sizeof(chatbuf), format, args);
	va_end(args);

	struct packet_chat pack_chat;

	pack_chat.header = HEADER_GC_CHAT;
	pack_chat.size = sizeof(struct packet_chat) + len;
	pack_chat.type = type;
	pack_chat.id = 0;
	pack_chat.bEmpire = d->GetEmpire();

	TEMP_BUFFER buf;
	buf.write(&pack_chat, sizeof(struct packet_chat));
	buf.write(chatbuf, len);

	d->Packet(buf.read_peek(), buf.size());

	if (type == CHAT_TYPE_COMMAND && test_server)
		sys_log(0, "SEND_COMMAND %s %s", GetName(), chatbuf);
}

// MINING

bool CHARACTER::StartStateMachine(int iNextPulse)
{
	if (CHARACTER_MANAGER::instance().AddToStateList(this))
	{
		m_dwNextStatePulse = thecore_heart->pulse + iNextPulse;
		return true;
	}

	return false;
}

void CHARACTER::StopStateMachine()
{
	CHARACTER_MANAGER::instance().RemoveFromStateList(this);
}

void CHARACTER::UpdateStateMachine(uint32_t dwPulse)
{
	if (dwPulse < m_dwNextStatePulse)
		return;

	if (IsDead())
		return;

	Update();
	m_dwNextStatePulse = dwPulse + m_dwStateDuration;
}

void CHARACTER::SetNextStatePulse(int iNextPulse)
{
	CHARACTER_MANAGER::instance().AddToStateList(this);
	m_dwNextStatePulse = iNextPulse;

	if (iNextPulse < 10)
		MonsterLog("´UA1»óAÂ·Î3î1­°!AÚ");
}


// Ä3¸-AÍ AÎ1oAI1o 3÷µYAIA® ÇÔ1ö.
void CHARACTER::UpdateCharacter(uint32_t dwPulse)
{
	CFSM::Update();

}



void CHARACTER::SetPart(uint8_t bPartPos, uint16_t wVal)
{
	assert(bPartPos < PART_MAX_NUM);
	m_pointsInstant.parts[bPartPos] = wVal;
}

uint16_t CHARACTER::GetPart(uint8_t bPartPos) const
{
	assert(bPartPos < PART_MAX_NUM);

#ifdef __HIDE_COSTUME_SYSTEM__
	if (bPartPos == PART_MAIN && GetWear(WEAR_COSTUME_BODY) && IsBodyCostumeHidden() == true) {
		if (const LPITEM pArmor = GetWear(WEAR_BODY))
#ifdef __CHANGE_LOOK_SYSTEM__
			return pArmor->GetTransmutation() != 0 ? pArmor->GetTransmutation() : pArmor->GetVnum();
#else
			return pArmor->GetVnum();
#endif
		else
			return 0;
	}
	else if (bPartPos == PART_HAIR && GetWear(WEAR_COSTUME_HAIR) && IsHairCostumeHidden() == true)
		return 0;
#ifdef ENABLE_STOLE_COSTUME
	else if (bPartPos == PART_ACCE && GetWear(WEAR_COSTUME_ACCE) && IsAcceCostumeHidden() == true) {
		LPITEM pAcce = GetWear(WEAR_COSTUME_ACCE_SLOT);
		if (pAcce) {
			uint32_t toSetValue = pAcce->GetVnum();
			toSetValue -= 85000;
			if (pAcce->GetSocket(ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
				toSetValue += 1000;

			return toSetValue;
		}
		else
			return 0;
	}
#else
	else if (bPartPos == PART_ACCE && GetWear(WEAR_COSTUME_ACCE_SLOT) && IsAcceCostumeHidden() == true)
		return 0;
#endif
	else if (bPartPos == PART_WEAPON && GetWear(WEAR_COSTUME_WEAPON) && IsWeaponCostumeHidden() == true)
	{
		if (const LPITEM pWeapon = GetWear(WEAR_WEAPON))
#ifdef __CHANGE_LOOK_SYSTEM__
			return pWeapon->GetTransmutation() != 0 ? pWeapon->GetTransmutation() : pWeapon->GetVnum();
#else
			return pWeapon->GetVnum();
#endif
		else
			return 0;
	}
#endif

	return m_pointsInstant.parts[bPartPos];
}

uint16_t CHARACTER::GetOriginalPart(uint8_t bPartPos) const
{
	switch (bPartPos)
	{
	case PART_MAIN:
	{
		if (!IsPC())
			return GetPart(PART_MAIN);

#ifdef __HIDE_COSTUME_SYSTEM__
		if (GetWear(WEAR_COSTUME_BODY) && IsBodyCostumeHidden() == true) {
			if (const LPITEM pArmor = GetWear(WEAR_BODY))
				return pArmor->GetVnum();
		}
#endif

		return m_pointsInstant.bBasePart;
	}
	case PART_HAIR:
	{
#ifdef __HIDE_COSTUME_SYSTEM__
		if (GetWear(WEAR_COSTUME_HAIR) && IsHairCostumeHidden() == true)
			return 0;
#endif

		return GetPart(PART_HAIR);
	}
#ifdef ENABLE_ACCE_SYSTEM
	case PART_ACCE:
	{
#ifdef __HIDE_COSTUME_SYSTEM__
#ifdef ENABLE_STOLE_COSTUME
		if (GetWear(WEAR_COSTUME_ACCE) && IsAcceCostumeHidden() == true) {
			LPITEM pAcce = GetWear(WEAR_COSTUME_ACCE_SLOT);
			if (pAcce) {
				uint32_t toSetValue = pAcce->GetVnum();
				toSetValue -= 85000;
				if (pAcce->GetSocket(ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
					toSetValue += 1000;

				return toSetValue;
			}
			else
				return 0;
		}
#else
		if (GetWear(WEAR_COSTUME_ACCE_SLOT) && IsAcceCostumeHidden() == true)
			return 0;
#endif
#else
		if (GetWear(WEAR_COSTUME_ACCE_SLOT))
			return 0;
#endif
		return GetPart(PART_ACCE);
	}
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	case PART_WEAPON:
	{
#ifdef __HIDE_COSTUME_SYSTEM__
		if (GetWear(WEAR_COSTUME_WEAPON) && IsWeaponCostumeHidden() == true) {
			if (const LPITEM pWeapon = GetWear(WEAR_WEAPON))
				return pWeapon->GetVnum();
		}
#endif
		return GetPart(PART_WEAPON);
#endif
	}
	default:
		return 0;
	}
}

bool CHARACTER::SetSyncOwner(LPCHARACTER ch, bool bRemoveFromList)
{
	// TRENT_MONSTER
	if (IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_NOMOVE))
		return false;
	// END_OF_TRENT_MONSTER

	if (ch) // @fixme131
	{
		if (!battle_is_attackable(ch, this))
		{
			SendDamagePacket(ch, 0, DAMAGE_BLOCK);
			return false;
		}
	}

	if (ch == this)
	{
		sys_err("SetSyncOwner owner == this (%p)", this);
		return false;
	}

	if (!ch)
	{
		if (bRemoveFromList && m_pkChrSyncOwner)
		{
			m_pkChrSyncOwner->m_kLst_pkChrSyncOwned.remove(this);
		}

		if (m_pkChrSyncOwner)
			sys_log(1, "SyncRelease %s %p from %s", GetName(), this, m_pkChrSyncOwner->GetName());

		// ¸®1oA®?!1­ Á¦°AÇIÁö 3E´o¶óµµ A÷AÎAÍ´Â NULL·Î 1ÂAAµÇ3î3ß ÇN´U.
		m_pkChrSyncOwner = nullptr;
	}
	else
	{
		if (!IsSyncOwner(ch))
			return false;

		// °A¸®°! 200 AI»óAI¸é SyncOwner°! µÉ 1ö 3o´U.
		if (DISTANCE_APPROX(GetX() - ch->GetX(), GetY() - ch->GetY()) > 250)
		{
			sys_log(1, "SetSyncOwner distance over than 250 %s %s", GetName(), ch->GetName());

			// SyncOwnerAI °a?i Owner·Î ÇY1AÇN´U.
			if (m_pkChrSyncOwner == ch)
				return true;

			return false;
		}

		if (m_pkChrSyncOwner != ch)
		{
			if (m_pkChrSyncOwner)
			{
				sys_log(1, "SyncRelease %s %p from %s", GetName(), this, m_pkChrSyncOwner->GetName());
				m_pkChrSyncOwner->m_kLst_pkChrSyncOwned.remove(this);
			}

			m_pkChrSyncOwner = ch;
			m_pkChrSyncOwner->m_kLst_pkChrSyncOwned.push_back(this);

			static const timeval zero_tv = { 0, 0 };
			SetLastSyncTime(zero_tv);

			sys_log(1, "SetSyncOwner set %s %p to %s", GetName(), this, ch->GetName());
		}

		m_fSyncTime = get_float_time();
	}

	// TODO: Sync Owner°! °°´o¶óµµ °e1Ó A?A¶A» o¸3»°í AÖA¸1Ç·Î,
	//       µ?±âE­ µE 1A°LAI 3AE AI»ó Áö3µA» ¶§ Ç®3îÁÖ´Â A?A¶A»
	//       o¸3»´Â 1a1ÄA¸·Î ÇI¸é A?A¶A» ÁUAI 1ö AÖ´U.
	TPacketGCOwnership pack;

	pack.bHeader = HEADER_GC_OWNERSHIP;
	pack.dwOwnerVID = ch ? ch->GetVID() : 0;
	pack.dwVictimVID = GetVID();

	PacketAround(&pack, sizeof(TPacketGCOwnership));
	return true;
}

struct FuncClearSync
{
	void operator () (LPCHARACTER ch)
	{
		assert(ch != NULL);
		ch->SetSyncOwner(nullptr, false);	// false ÇA·!±×·Î ÇO3ß for_each °! Á¦´ë·Î µ·´U.
	}
};

void CHARACTER::ClearSync()
{
	SetSyncOwner(nullptr);

	// 3A·! for_each?!1­ 3a¸¦ m_pkChrSyncOwner·Î °!Áo AÚµéAÇ A÷AÎAÍ¸¦ NULL·Î ÇN´U.
	std::for_each(m_kLst_pkChrSyncOwned.begin(), m_kLst_pkChrSyncOwned.end(), FuncClearSync());
	m_kLst_pkChrSyncOwned.clear();
}

bool CHARACTER::IsSyncOwner(LPCHARACTER ch) const
{
	if (m_pkChrSyncOwner == ch)
		return true;

	// ¸¶Áö¸·A¸·Î µ?±âE­ µE 1A°LAI 3AE AI»ó Áö3µ´U¸é 1OA-±ÇAI 3A1«?!°Ôµµ
	// 3o´U. µu¶ó1­ 3A1«3a SyncOwnerAI1Ç·Î true ¸®AI
	if (get_float_time() - m_fSyncTime >= 3.0f)
		return true;

	return false;
}



bool CHARACTER::OnIdle()
{
	return false;
}

void CHARACTER::OnMove(bool bIsAttack)
{
	m_dwLastMoveTime = get_dword_time();

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

void CHARACTER::OnClick(LPCHARACTER pkChrCauser)
{
	if (!pkChrCauser)
	{
		sys_err("OnClick %s by NULL", GetName());
		return;
	}

	uint32_t vid = GetVID();
	sys_log(0, "OnClick %s[vnum: %d vid: %d] by %s", GetName(), GetRaceNum(), vid, pkChrCauser->GetName());

	// »óÁ!A» ?¬»óAÂ·Î Äu1oA®¸¦ ÁoÇaÇO 1ö 3o´U.
	{
		// ´Ü, AÚ1AAo AÚ1AAÇ »óÁ!A» A¬¸-ÇO 1ö AÖ´U.
		if (pkChrCauser->GetMyShop() && pkChrCauser != this)
		{
			sys_err("OnClick Fail (%s->%s) - pc has shop", pkChrCauser->GetName(), GetName());
			return;
		}
	}

	// ±3E-ÁßAI¶§ Äu1oA®¸¦ ÁoÇaÇO 1ö 3o´U.
	{
		if (pkChrCauser->GetExchange())
		{
			sys_err("OnClick Fail (%s->%s) - pc is exchanging", pkChrCauser->GetName(), GetName());
			return;
		}
	}

	if (IsPC())
	{
		// A¸°UA¸·Î 13Á¤µE °a?i´Â PC?! AÇÇN A¬¸-µµ Äu1oA®·Î A3¸®ÇIµµ·I ÇO´I´U.
		if (!CTargetManager::instance().GetTargetInfo(pkChrCauser->GetPlayerID(), TARGET_TYPE_VID, GetVID()))
		{
			// 2005.03.17.myevan.A¸°UAI 3A´N °a?i´Â °3AÎ »óÁ! A3¸® ±â´ÉA» AUµ?1AA2´U.
			if (GetMyShop())
			{
				if (pkChrCauser->IsDead() == true) return;

				//PREVENT_TRADE_WINDOW
				if (pkChrCauser == this) // AÚ±â´Â °!´É
				{
					if ((GetExchange() || IsOpenSafebox() || GetShopOwner()) || IsCubeOpen())
					{
#ifdef TEXTS_IMPROVEMENT
						pkChrCauser->ChatPacketNew(CHAT_TYPE_INFO, 291, "");
#endif
						return;
					}

#ifdef __ATTR_TRANSFER_SYSTEM__
					if (IsAttrTransferOpen())
					{
#ifdef TEXTS_IMPROVEMENT
						pkChrCauser->ChatPacketNew(CHAT_TYPE_INFO, 291, "");
#endif
						return;
					}
#endif
				}
				else // ´U¸Y »ç¶÷AI A¬¸-ÇßA»¶§
				{
					// A¬¸-ÇN »ç¶÷AI ±3E-/Ac°í/°3AÎ»óÁ!/»óÁ!AI?ëÁßAI¶ó¸é oO°!
					if ((pkChrCauser->GetExchange() || pkChrCauser->IsOpenSafebox() || pkChrCauser->GetMyShop() || pkChrCauser->GetShopOwner()) || pkChrCauser->IsCubeOpen())
					{
#ifdef TEXTS_IMPROVEMENT
						pkChrCauser->ChatPacketNew(CHAT_TYPE_INFO, 291, "");
#endif
						return;
					}

#ifdef __ATTR_TRANSFER_SYSTEM__
					if (pkChrCauser->IsAttrTransferOpen())
					{
#ifdef TEXTS_IMPROVEMENT
						pkChrCauser->ChatPacketNew(CHAT_TYPE_INFO, 291, "");
#endif
						return;
					}
#endif

					// A¬¸-ÇN ´ë»óAI ±3E-/Ac°í/»óÁ!AI?ëÁßAI¶ó¸é oO°!
					//if ((GetExchange() || IsOpenSafebox() || GetShopOwner()))
					if ((GetExchange() || IsOpenSafebox() || IsCubeOpen()))
					{
#ifdef TEXTS_IMPROVEMENT
						pkChrCauser->ChatPacketNew(CHAT_TYPE_INFO, 369, "%s", GetName());
#endif
						return;
					}

#ifdef __ATTR_TRANSFER_SYSTEM__
					if (IsAttrTransferOpen())
					{
#ifdef TEXTS_IMPROVEMENT
						pkChrCauser->ChatPacketNew(CHAT_TYPE_INFO, 369, "%s", GetName());
#endif
						return;
					}
#endif
				}
				//END_PREVENT_TRADE_WINDOW

				if (pkChrCauser->GetShop())
				{
					pkChrCauser->GetShop()->RemoveGuest(pkChrCauser);
					pkChrCauser->SetShop(nullptr);
				}

				GetMyShop()->AddGuest(pkChrCauser, GetVID(), false);
				pkChrCauser->SetShopOwner(this);
				return;
			}

			if (test_server)
				sys_err("%s.OnClickFailure(%s) - target is PC", pkChrCauser->GetName(), GetName());

			return;
		}
	}

	pkChrCauser->SetQuestNPCID(GetVID());

	if (quest::CQuestManager::instance().Click(pkChrCauser->GetPlayerID(), this))
	{
		return;
	}

	// NPC Aü?ë ±â´É 1öÇa : »óÁ! ?­±â µî
	if (!IsPC())
	{
		if (!m_triggerOnClick.pFunc)
		{
			// NPC A®¸®°A 1A1oAU ·Î±× o¸±â
			//sys_err("%s.OnClickFailure(%s) : triggerOnClick.pFunc is EMPTY(pid=%d)",
			//			pkChrCauser->GetName(),
			//			GetName(),
			//			pkChrCauser->GetPlayerID());
			return;
		}

		m_triggerOnClick.pFunc(this, pkChrCauser);
	}

}

void CHARACTER::SetStone(LPCHARACTER pkChrStone)
{
	m_pkChrStone = pkChrStone;

	if (m_pkChrStone)
	{
		if (!pkChrStone->m_set_pkChrSpawnedBy.contains(this))
			pkChrStone->m_set_pkChrSpawnedBy.insert(this);
	}
}

#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
struct FuncDeadSpawnedByStone
{
	LPCHARACTER m_pkKiller;

	FuncDeadSpawnedByStone(LPCHARACTER pkKiller)
		: m_pkKiller(pkKiller)
	{
	}

	void operator () (LPCHARACTER ch)
	{
		if (m_pkKiller && m_pkKiller->IsPC())
			ch->RegisterDamageForExp(m_pkKiller, 1);

		ch->Dead(nullptr);      // marad: haljon meg a kovel együtt
		ch->SetStone(nullptr);
	}
};


#else
struct FuncDeadSpawnedByStone
{
	void operator () (LPCHARACTER ch)
	{


		ch->Dead(nullptr);

		ch->SetStone(nullptr);
	}
};
#endif
#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
void CHARACTER::ClearStone(LPCHARACTER pkKiller)
{
	if (!m_set_pkChrSpawnedBy.empty())
	{
		FuncDeadSpawnedByStone f(pkKiller);
		std::for_each(m_set_pkChrSpawnedBy.begin(), m_set_pkChrSpawnedBy.end(), f);
		m_set_pkChrSpawnedBy.clear();
	}

	if (!m_pkChrStone)
		return;

	m_pkChrStone->m_set_pkChrSpawnedBy.erase(this);
	m_pkChrStone = nullptr;
}


#else
void CHARACTER::ClearStone()
{
	if (!m_set_pkChrSpawnedBy.empty())
	{

		FuncDeadSpawnedByStone f;

		
		std::for_each(m_set_pkChrSpawnedBy.begin(), m_set_pkChrSpawnedBy.end(), f);
		m_set_pkChrSpawnedBy.clear();
	}

	if (!m_pkChrStone)
		return;

	m_pkChrStone->m_set_pkChrSpawnedBy.erase(this);
	m_pkChrStone = nullptr;
}
#endif
void CHARACTER::ClearTarget()
{
	if (m_pkChrTarget)
	{
		m_pkChrTarget->m_set_pkChrTargetedBy.erase(this);
		m_pkChrTarget = nullptr;
	}

	TPacketGCTarget p;

	p.header = HEADER_GC_TARGET;
	p.dwVID = 0;
	p.bHPPercent = 0;
#ifdef __VIEW_TARGET_DECIMAL_HP__
	p.iMinHP = 0;
	p.iMaxHP = 0;
#endif

	CHARACTER_SET::iterator it = m_set_pkChrTargetedBy.begin();

	while (it != m_set_pkChrTargetedBy.end())
	{
		LPCHARACTER pkChr = *(it++);
		pkChr->m_pkChrTarget = nullptr;

		if (!pkChr->GetDesc())
		{
			sys_err("%s %p does not have desc", pkChr->GetName(), get_pointer(pkChr));
			abort();
		}

		pkChr->GetDesc()->Packet(&p, sizeof(TPacketGCTarget));
	}

	m_set_pkChrTargetedBy.clear();
}

void CHARACTER::SetTarget(LPCHARACTER pkChrTarget)
{
	if (m_pkChrTarget == pkChrTarget)
		return;

	if (m_pkChrTarget)
		m_pkChrTarget->m_set_pkChrTargetedBy.erase(this);

	m_pkChrTarget = pkChrTarget;

	TPacketGCTarget p;

	p.header = HEADER_GC_TARGET;

	if (m_pkChrTarget)
	{
		m_pkChrTarget->m_set_pkChrTargetedBy.insert(this);

		p.dwVID = m_pkChrTarget->GetVID();

#ifdef __VIEW_TARGET_PLAYER_HP__
		if ((m_pkChrTarget->GetMaxHP() <= 0))
		{
			p.bHPPercent = 0;
#ifdef __VIEW_TARGET_DECIMAL_HP__
			p.iMinHP = 0;
			p.iMaxHP = 0;
#endif
		}
		else if (m_pkChrTarget->IsPC() && !m_pkChrTarget->IsPolymorphed())
		{
			p.bHPPercent = MINMAX(0, m_pkChrTarget->GetHPPct(), 100);
#ifdef __VIEW_TARGET_DECIMAL_HP__
			p.iMinHP = m_pkChrTarget->GetHP();
			p.iMaxHP = m_pkChrTarget->GetMaxHP();
#endif
		}
#else
		if ((m_pkChrTarget->IsPC() && !m_pkChrTarget->IsPolymorphed()) || (m_pkChrTarget->GetMaxHP() <= 0))
			p.bHPPercent = 0;
#endif
		else
		{
			if (m_pkChrTarget->GetRaceNum() == 20101 ||
				m_pkChrTarget->GetRaceNum() == 20102 ||
				m_pkChrTarget->GetRaceNum() == 20103 ||
				m_pkChrTarget->GetRaceNum() == 20104 ||
				m_pkChrTarget->GetRaceNum() == 20105 ||
				m_pkChrTarget->GetRaceNum() == 20106 ||
				m_pkChrTarget->GetRaceNum() == 20107 ||
				m_pkChrTarget->GetRaceNum() == 20108 ||
				m_pkChrTarget->GetRaceNum() == 20109)
			{
				LPCHARACTER owner = m_pkChrTarget->GetVictim();

				if (owner)
				{
					int iHorseHealth = owner->GetHorseHealth();
					int iHorseMaxHealth = owner->GetHorseMaxHealth();
#ifdef __VIEW_TARGET_DECIMAL_HP__
					if (iHorseMaxHealth)
					{
						p.bHPPercent = MINMAX(0, iHorseHealth * 100 / iHorseMaxHealth, 100);
						p.iMinHP = 100;
						p.iMaxHP = 100;
					}
					else
					{
						p.bHPPercent = 100;
						p.iMinHP = 100;
						p.iMaxHP = 100;
					}
				}
				else
				{
					p.bHPPercent = 100;
					p.iMinHP = 100;
					p.iMaxHP = 100;
				}
			}
			else
			{
				if (m_pkChrTarget->GetMaxHP() <= 0) // @fixme136
				{
					p.bHPPercent = 0;
					p.iMinHP = 0;
					p.iMaxHP = 0;
				}
				else
				{
					p.bHPPercent = min((m_pkChrTarget->GetHP() * 100) / m_pkChrTarget->GetMaxHP(), (int64_t)100);
					p.iMinHP = m_pkChrTarget->GetHP();
					p.iMaxHP = m_pkChrTarget->GetMaxHP();
				}
			}
		}
	}
	else
	{
		p.dwVID = 0;
		p.bHPPercent = 0;
		p.iMinHP = 0;
		p.iMaxHP = 0;
	}
#else
					if (iHorseMaxHealth)
						p.bHPPercent = MINMAX(0, iHorseHealth * 100 / iHorseMaxHealth, 100);

					else
						p.bHPPercent = 100;
}
				else
					p.bHPPercent = 100;
			}
			else
			{
				if (m_pkChrTarget->GetMaxHP() <= 0) // @fixme136
					p.bHPPercent = 0;
				else
					p.bHPPercent = MINMAX(0, (m_pkChrTarget->GetHP() * 100) / m_pkChrTarget->GetMaxHP(), 100);
			}
		}
	}
	else
	{
		p.dwVID = 0;
		p.bHPPercent = 0;
	}
#endif
#ifdef ELEMENT_TARGET
	p.bElement = 0;
	if (m_pkChrTarget) {
		if (m_pkChrTarget->IsPC()) {
			LPITEM item = m_pkChrTarget->GetWear(WEAR_PENDANT);
			if (item) {
				uint32_t vnum = item->GetVnum();
				if (vnum >= 10750 && vnum <= 10950) {
					p.bElement = 1;
				}
				else if (vnum >= 9600 && vnum <= 9800) {
					p.bElement = 2;
				}
				else if (vnum >= 9830 && vnum <= 10030) {
					p.bElement = 3;
				}
				else if (vnum >= 10520 && vnum <= 10720) {
					p.bElement = 4;
				}
				else if (vnum >= 10060 && vnum <= 10260) {
					p.bElement = 5;
				}
				else if (vnum >= 10290 && vnum <= 10490) {
					p.bElement = 6;
				}
			}
		}
		else if (m_pkChrTarget->IsMonster() || m_pkChrTarget->IsStone()) {
			if (m_pkChrTarget->IsRaceFlag(RACE_FLAG_ATT_ELEC)) {
				p.bElement = 1;
			}
			else if (m_pkChrTarget->IsRaceFlag(RACE_FLAG_ATT_FIRE)) {
				p.bElement = 2;
			}
			else if (m_pkChrTarget->IsRaceFlag(RACE_FLAG_ATT_ICE)) {
				p.bElement = 3;
			}
			else if (m_pkChrTarget->IsRaceFlag(RACE_FLAG_ATT_WIND)) {
				p.bElement = 4;
			}
			else if (m_pkChrTarget->IsRaceFlag(RACE_FLAG_ATT_EARTH)) {
				p.bElement = 5;
			}
			else if (m_pkChrTarget->IsRaceFlag(RACE_FLAG_ATT_DARK)) {
				p.bElement = 6;
			}
		}
	}
#endif
	GetDesc()->Packet(&p, sizeof(TPacketGCTarget));
}

void CHARACTER::BroadcastTargetPacket()
{
	if (m_set_pkChrTargetedBy.empty())
		return;

	TPacketGCTarget p;

	p.header = HEADER_GC_TARGET;
	p.dwVID = GetVID();

#ifdef __VIEW_TARGET_DECIMAL_HP__
	if (GetMaxHP() <= 0) // @fixme136
	{
		p.bHPPercent = 0;
		p.iMinHP = 0;
		p.iMaxHP = 0;
	}
	else
	{
		p.bHPPercent = min((GetHP() * 100) / GetMaxHP(), (int64_t)100);
		p.iMinHP = GetHP();
		p.iMaxHP = GetMaxHP();
	}
#else
	if (IsPC())
		p.bHPPercent = 0;
	else if (GetMaxHP() <= 0) // @fixme136
		p.bHPPercent = 0;
	else
		p.bHPPercent = MINMAX(0, GetHPPct(), 100);
#endif

	CHARACTER_SET::iterator it = m_set_pkChrTargetedBy.begin();

	while (it != m_set_pkChrTargetedBy.end())
	{
		LPCHARACTER pkChr = *it++;

		if (!pkChr->GetDesc())
		{
			sys_err("%s %p does not have desc", pkChr->GetName(), get_pointer(pkChr));
			abort();
		}

		pkChr->GetDesc()->Packet(&p, sizeof(TPacketGCTarget));
	}
}

void CHARACTER::CheckTarget()
{
	if (!m_pkChrTarget)
		return;

	if (DISTANCE_APPROX(GetX() - m_pkChrTarget->GetX(), GetY() - m_pkChrTarget->GetY()) >= 4800)
		SetTarget(nullptr);
}

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


bool CHARACTER::BuildUpdatePartyPacket(TPacketGCPartyUpdate& out)
{
	if (!GetParty())
		return false;

	memset(&out, 0, sizeof(out));

	out.header = HEADER_GC_PARTY_UPDATE;
	out.pid = GetPlayerID();
	if (GetMaxHP() <= 0) // @fixme136
		out.percent_hp = 0;
	else
		out.percent_hp = MINMAX((int64_t)0, GetHP() * 100 / GetMaxHP(), (int64_t)100);
	out.role = GetParty()->GetRole(GetPlayerID());

	sys_log(1, "PARTY %s role is %d", GetName(), out.role);

	LPCHARACTER l = GetParty()->GetLeaderCharacter();

	if (l && DISTANCE_APPROX(GetX() - l->GetX(), GetY() - l->GetY()) < PARTY_DEFAULT_RANGE)
	{
		out.affects[0] = GetParty()->GetPartyBonusExpPercent();
		out.affects[1] = GetPoint(POINT_PARTY_ATTACKER_BONUS);
		out.affects[2] = GetPoint(POINT_PARTY_TANKER_BONUS);
		out.affects[3] = GetPoint(POINT_PARTY_BUFFER_BONUS);
		out.affects[4] = GetPoint(POINT_PARTY_SKILL_MASTER_BONUS);
		out.affects[5] = GetPoint(POINT_PARTY_HASTE_BONUS);
		out.affects[6] = GetPoint(POINT_PARTY_DEFENDER_BONUS);
	}

	return true;
}

int CHARACTER::GetLeadershipSkillLevel() const
{
	return GetSkillLevel(SKILL_LEADERSHIP);
}

void CHARACTER::SetNowWalking(bool bWalkFlag)
{
	//if (m_bNowWalking != bWalkFlag || IsNPC())
	if (m_bNowWalking != bWalkFlag)
	{
		if (bWalkFlag)
		{
			m_bNowWalking = true;
			m_dwWalkStartTime = get_dword_time();
		}
		else
		{
			m_bNowWalking = false;
		}

		//if (m_bNowWalking)
		{
			TPacketGCWalkMode p;
			p.vid = GetVID();
			p.header = HEADER_GC_WALK_MODE;
			p.mode = m_bNowWalking ? WALKMODE_WALK : WALKMODE_RUN;

			PacketView(&p, sizeof(p));
		}

		if (IsNPC())
		{
			if (m_bNowWalking)
				MonsterLog("°E´Â´U");
			else
				MonsterLog("¶Ú´U");
		}

		//sys_log(0, "%s is now %s", GetName(), m_bNowWalking?"walking.":"running.");
	}
}

void CHARACTER::StartStaminaConsume()
{
	if (m_bStaminaConsume)
		return;
	PointChange(POINT_STAMINA, 0);
	m_bStaminaConsume = true;
	//ChatPacket(CHAT_TYPE_COMMAND, "StartStaminaConsume %d %d", STAMINA_PER_STEP * passes_per_sec, GetStamina());
	if (IsStaminaHalfConsume())
		ChatPacket(CHAT_TYPE_COMMAND, "StartStaminaConsume %d %d", STAMINA_PER_STEP * passes_per_sec / 2, GetStamina());
	else
		ChatPacket(CHAT_TYPE_COMMAND, "StartStaminaConsume %d %d", STAMINA_PER_STEP * passes_per_sec, GetStamina());
}

void CHARACTER::StopStaminaConsume()
{
	if (!m_bStaminaConsume)
		return;
	PointChange(POINT_STAMINA, 0);
	m_bStaminaConsume = false;
	ChatPacket(CHAT_TYPE_COMMAND, "StopStaminaConsume %d", GetStamina());
}

bool CHARACTER::IsStaminaConsume() const
{
	return m_bStaminaConsume;
}

bool CHARACTER::IsStaminaHalfConsume() const
{
	return IsEquipUniqueItem(UNIQUE_ITEM_HALF_STAMINA);
}

void CHARACTER::ResetStopTime()
{
	m_dwStopTime = get_dword_time();
}

uint32_t CHARACTER::GetStopTime() const
{
	return m_dwStopTime;
}

void CHARACTER::ResetPoint(int iLv)
{
	uint8_t bJob = GetJob();

	PointChange(POINT_LEVEL, iLv - GetLevel());

	SetRealPoint(POINT_ST, JobInitialPoints[bJob].st);
	SetPoint(POINT_ST, GetRealPoint(POINT_ST));

	SetRealPoint(POINT_HT, JobInitialPoints[bJob].ht);
	SetPoint(POINT_HT, GetRealPoint(POINT_HT));

	SetRealPoint(POINT_DX, JobInitialPoints[bJob].dx);
	SetPoint(POINT_DX, GetRealPoint(POINT_DX));

	SetRealPoint(POINT_IQ, JobInitialPoints[bJob].iq);
	SetPoint(POINT_IQ, GetRealPoint(POINT_IQ));

	SetRandomHP((iLv - 1) * number(JobInitialPoints[GetJob()].hp_per_lv_begin, JobInitialPoints[GetJob()].hp_per_lv_end));
	SetRandomSP((iLv - 1) * number(JobInitialPoints[GetJob()].sp_per_lv_begin, JobInitialPoints[GetJob()].sp_per_lv_end));

	// @fixme104
	int iLvl = iLv;
#ifdef ENABLE_STATUS_MAX_344_POINTS
	if (iLvl > 0)
		iLvl -= 1;
#endif
	PointChange(POINT_STAT, (MINMAX(1, iLvl, g_iStatusPointGetLevelLimit) * 3) + GetPoint(POINT_LEVEL_STEP) - GetPoint(POINT_STAT));

	ComputePoints();

	// E¸o1
	PointChange(POINT_HP, GetMaxHP() - GetHP());
	PointChange(POINT_SP, GetMaxSP() - GetSP());

	PointsPacket();

	LogManager::instance().CharLog(this, 0, "RESET_POINT", "");
}

bool CHARACTER::IsChangeAttackPosition(LPCHARACTER target) const
{
	if (!IsNPC())
		return true;

	uint32_t dwChangeTime = AI_CHANGE_ATTACK_POISITION_TIME_NEAR;

	if (DISTANCE_APPROX(GetX() - target->GetX(), GetY() - target->GetY()) >
		AI_CHANGE_ATTACK_POISITION_DISTANCE + GetMobAttackRange())
		dwChangeTime = AI_CHANGE_ATTACK_POISITION_TIME_FAR;

	return get_dword_time() - m_dwLastChangeAttackPositionTime > dwChangeTime;
}

void CHARACTER::GiveRandomSkillBook()
{
	LPITEM item = AutoGiveItem(50300);

	if (nullptr != item)
	{
		extern const uint32_t GetRandomSkillVnum(uint8_t bJob = JOB_MAX_NUM);
		uint32_t dwSkillVnum = 0;
		// 50% of getting random books or getting one of the same player's race
		if (!number(0, 1))
			dwSkillVnum = GetRandomSkillVnum(GetJob());
		else
			dwSkillVnum = GetRandomSkillVnum();
		item->SetSocket(0, dwSkillVnum);
	}
}

void CHARACTER::ReviveInvisible(int iDur)
{
	AddAffect(AFFECT_REVIVE_INVISIBLE, POINT_NONE, 0, AFF_REVIVE_INVISIBLE, iDur, 0, true);
}

void CHARACTER::ToggleMonsterLog()
{
	m_bMonsterLog = !m_bMonsterLog;

	if (m_bMonsterLog)
	{
		CHARACTER_MANAGER::instance().RegisterForMonsterLog(this);
	}
	else
	{
		CHARACTER_MANAGER::instance().UnregisterForMonsterLog(this);
	}
}

void CHARACTER::SendGreetMessage()
{
	auto v = DBManager::instance().GetGreetMessage();

	for (auto it = v.begin(); it != v.end(); ++it)
	{
		ChatPacket(CHAT_TYPE_NOTICE, it->c_str());
	}
}

void CHARACTER::BeginStateEmpty()
{
	MonsterLog("!");
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

int CHARACTER::ChangeEmpire(uint8_t empire)
{
	if (GetEmpire() == empire)
		return 1;

	char szQuery[1024 + 1];
	uint32_t dwAID;
	uint32_t dwPID[4];
	memset(dwPID, 0, sizeof(dwPID));

	snprintf(szQuery, sizeof(szQuery),
		"SELECT id, pid1, pid2, pid3, pid4, pid5 FROM player_index%s WHERE pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u OR pid5=%u AND empire=%u",
		get_table_postfix(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetEmpire());

	std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(szQuery));
	if (msg->Get()->uiNumRows == 0)
		return 0;

	MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
	str_to_number(dwAID, row[0]);
	str_to_number(dwPID[0], row[1]);
	str_to_number(dwPID[1], row[2]);
	str_to_number(dwPID[2], row[3]);
	str_to_number(dwPID[3], row[4]);

	for (int i = 0; i < 4; ++i)
	{
		snprintf(szQuery, sizeof(szQuery), "SELECT guild_id FROM guild_member%s WHERE pid=%u", get_table_postfix(), dwPID[i]);
		std::unique_ptr<SQLMsg> guildMsg(DBManager::instance().DirectQuery(szQuery));
		if (guildMsg->Get()->uiNumRows > 0)
		{
			uint32_t dwGuildID = 0;
			MYSQL_ROW guildRow = mysql_fetch_row(guildMsg->Get()->pSQLResult);
			str_to_number(dwGuildID, guildRow[0]);
			if (CGuildManager::instance().FindGuild(dwGuildID) != nullptr)
				return 2;
		}
	}

	for (int i = 0; i < 4; ++i)
	{
		if (marriage::CManager::instance().IsEngagedOrMarried(dwPID[i]) == true)
			return 3;
	}

	snprintf(szQuery, sizeof(szQuery), "UPDATE player_index%s SET empire=%u WHERE pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u OR pid5=%u AND empire=%u",
		get_table_postfix(), empire, GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetEmpire());

	std::unique_ptr<SQLMsg> updateMsg(DBManager::instance().DirectQuery(szQuery));
	if (updateMsg->Get()->uiAffectedRows <= 0)
		return 0;

	const entt::entity e = EcsEntityOf(this);
	if (e != entt::null && g_registry.valid(e))
	{
		auto& emp = g_registry.get_or_emplace<ecs::EmpireComponent>(e);
		emp.value = empire;
		++emp.changeCount;
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	}

	SetChangeEmpireCount();
#ifdef ENABLE_BUG_FIXES
	SetEmpire(empire);
	UpdatePacket();
#endif
	return 999;
}

int CHARACTER::GetChangeEmpireCount() const
{
	char szQuery[1024 + 1];
	uint32_t dwAID = GetAID();

	if (dwAID == 0)
		return 0;

	snprintf(szQuery, sizeof(szQuery), "SELECT change_count FROM change_empire WHERE account_id = %u", dwAID);
	std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(szQuery));
	if (msg->Get()->uiNumRows == 0)
		return 0;

	MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
	uint32_t count = 0;
	str_to_number(count, row[0]);
	return count;
}

void CHARACTER::SetChangeEmpireCount()
{
	char szQuery[1024 + 1];
	uint32_t dwAID = GetAID();

	if (dwAID == 0)
		return;

	int count = GetChangeEmpireCount();
	const entt::entity e = EcsEntityOf(this);
	if (e != entt::null && g_registry.valid(e))
	{
		auto& emp = g_registry.get_or_emplace<ecs::EmpireComponent>(e);
		emp.changeCount = static_cast<uint32_t>(count + 1);
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	}

	if (count == 0)
	{
		++count;
		snprintf(szQuery, sizeof(szQuery), "INSERT INTO change_empire VALUES(%u, %d, NOW())", dwAID, count);
	}
	else
	{
		++count;
		snprintf(szQuery, sizeof(szQuery), "UPDATE change_empire SET change_count=%d WHERE account_id=%u", count, dwAID);
	}

	std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(szQuery));
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
int CHARACTER::GetDuel(const char* type) const
{
	const char* szTableStaticPvP[] = { BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };

	int m_nDuelTable[] = { (GetQuestFlag(szTableStaticPvP[0])), (GetQuestFlag(szTableStaticPvP[1])), (GetQuestFlag(szTableStaticPvP[2])), (GetQuestFlag(szTableStaticPvP[3])), (GetQuestFlag(szTableStaticPvP[4])), (GetQuestFlag(szTableStaticPvP[5])), (GetQuestFlag(szTableStaticPvP[6])), (GetQuestFlag(szTableStaticPvP[7])), (GetQuestFlag(szTableStaticPvP[8])), (GetQuestFlag(szTableStaticPvP[9])) };

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

void CHARACTER::SetDuel(const char* type, int value)
{
	const char* szTableStaticPvP[] = { BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };

	if (!strcmp(type, "BlockChangeItem")) {
		SetQuestFlag(szTableStaticPvP[0], value);
	}
	if (!strcmp(type, "BlockBuff")) {
		SetQuestFlag(szTableStaticPvP[1], value);
	}
	if (!strcmp(type, "BlockPotion")) {
		SetQuestFlag(szTableStaticPvP[2], value);
	}
	if (!strcmp(type, "BlockRide")) {
		SetQuestFlag(szTableStaticPvP[3], value);
	}
	if (!strcmp(type, "BlockPet")) {
		SetQuestFlag(szTableStaticPvP[4], value);
	}
	if (!strcmp(type, "BlockPoly")) {
		SetQuestFlag(szTableStaticPvP[5], value);
	}
	if (!strcmp(type, "BlockParty")) {
		SetQuestFlag(szTableStaticPvP[6], value);
	}
	if (!strcmp(type, "BlockExchange")) {
		SetQuestFlag(szTableStaticPvP[7], value);
	}
	if (!strcmp(type, "BetMoney")) {
		SetQuestFlag(szTableStaticPvP[8], value);
	}
	if (!strcmp(type, "IsFight")) {
		SetQuestFlag(szTableStaticPvP[9], value);
	}
}
#endif

void CHARACTER::MountVnum(uint32_t vnum)
{
	if (m_dwMountVnum == vnum)
		return;
	if ((m_dwMountVnum != 0) && (vnum != 0)) //@fixme108 set recursively to 0 for eventuality
		MountVnum(0);

	m_dwMountVnum = vnum;
	m_dwMountTime = get_dword_time();

	if (m_bIsObserver)
		return;

	//NOTE : MountÇN´U°í ÇO1­ Client SideAÇ °´A1¸¦ »eÁ¦ÇIÁo 3E´Â´U.
	//±×¸®°í 1­1öSide?!1­ AAA»¶§ A§Ä! AIµ?Ao ÇIÁö 3E´Â´U. ?Ö3ÄÇI¸é Client Side?!1­ Coliision Adjust¸¦ ÇO1ö AÖ´ÂµY
	//°´A1¸¦ 1O¸e1AÄ×´U°! 1­1öA§Ä!·Î AIµ?1AA°¸é AI¶§ collision check¸¦ ÇIÁö´Â 3EA¸1Ç·Î 1e°a?! 3c°A3a ¶O°í 3a°!´Â 1®Á¦°! Á¸AçÇN´U.
	m_posDest.x = m_posStart.x = GetX();
	m_posDest.y = m_posStart.y = GetY();
	//EncodeRemovePacket(this);
	EncodeInsertPacket(this);

	ENTITY_MAP::iterator it = m_map_view.begin();

	while (it != m_map_view.end())
	{
		LPENTITY entity = (it++)->first;

		//MountÇN´U°í ÇO1­ Client SideAÇ °´A1¸¦ »eÁ¦ÇIÁo 3E´Â´U.
		//EncodeRemovePacket(entity);
		//if (!m_bIsObserver)
		EncodeInsertPacket(entity);

		//if (!entity->IsObserverMode())
		//	entity->EncodeInsertPacket(this);
	}

	SetValidComboInterval(0);
	SetComboSequence(0);

	ComputePoints();
}

void CHARACTER::SyncPacket()
{
	TEMP_BUFFER buf;

	TPacketCGSyncPositionElement elem;

	elem.dwVID = GetVID();
	elem.lX = GetX();
	elem.lY = GetY();

	TPacketGCSyncPosition pack;

	pack.bHeader = HEADER_GC_SYNC_POSITION;
	pack.wSize = sizeof(TPacketGCSyncPosition) + sizeof(elem);

	buf.write(&pack, sizeof(pack));
	buf.write(&elem, sizeof(elem));

	PacketAround(buf.read_peek(), buf.size());
}


// ADD_REFINE_BUILDING
int64_t CHARACTER::ComputeRefineFee(int64_t iCost, int64_t iMultiply) const
{
	CGuild* pGuild = GetRefineGuild();
	if (pGuild)
	{
		if (pGuild == GetGuild())
			return iCost * iMultiply * 9 / 10;

		// ´U¸Y Á¦±1 »ç¶÷AI 1AµµÇI´Â °a?i Aß°!·Î 31e ´o
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
		// AÚ±â ±aµaAI¸é iTotalMoney?! AI1I 10%°! Á¦?ÜµÇ3îAÖ´U
		if (pGuild != GetGuild())
		{
			pGuild->RequestDepositMoney(this, iFee);
			iRemain -= iFee;
		}
	}

	PointChange(POINT_GOLD, -iRemain);
}
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

void CHARACTER::GoHome()
{
	WarpSet(EMPIRE_START_X(GetEmpire()), EMPIRE_START_Y(GetEmpire()));
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

void CHARACTER::StartDestroyWhenIdleEvent()
{
	if (m_pkDestroyWhenIdleEvent)
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();

	info->ch = this;

	m_pkDestroyWhenIdleEvent = event_create(destroy_when_idle_event, info, PASSES_PER_SEC(300));
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
long long CHARACTER::GetRankPoints(int iArg)
{
	if ((iArg < 0) || (iArg >= RANKING_MAX_CATEGORIES))
		return 0;

	return m_lRankPoints[iArg];
}

void CHARACTER::SetRankPoints(int iArg, long long lPoint)
{
	if ((iArg < 0) || (iArg >= RANKING_MAX_CATEGORIES))
		return;

	/*
	iArg : 0	- chi ha vinto piu duelli -
	iArg : 1	- chi ha perso piu duelli -
	iArg : 2	- m. m. di uccisioni in guerra -
	iArg : 3	- d. medi maggiori vs player -
	iArg : 4	- d. abilita maggiore vs player -
	iArg : 5	- chi ha distrutto piu metin -
	iArg : 6	- chi ha ucciso piu mostri -
	iArg : 7	- chi ha ucciso piu boss -
	iArg : 8	- d. medi maggiori vs boss -
	iArg : 9	- d. abilita maggiori vs boss -
	iArg : 10	- chi ha raccolto piu yang -
	iArg : 11	- chi ha raccolto piu gaya -
	iArg : 12	- chi ha usato piu i. oggetto -
	iArg : 13	- chi ha usato piu i. talismani -
	iArg : 14	- chi ha pescato piu pesci -
	iArg : 15	- m. numero di tempo in game -
	iArg : 16	- piu dungeon completati -
	iArg : 17	- chi ha aperto piu forzieri -
	iArg : 18	- d. medio mas. vs mob -
	iArg : 19	- d. abilita mas. vs mob -
	*/
	m_lRankPoints[iArg] = lPoint;
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

#ifdef __ENABLE_NEW_OFFLINESHOP__
#endif



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

	// If we currently are in a dungeon.
	if (lMapIndex >= 10000)
	{
		sys_err("Invalid change channel request from dungeon %d!", lMapIndex);
		return false;
	}

	// If we are on CH99.
	if (g_bChannel == 99)
	{
		sys_err("%s attempted to change channel from CH99, ignoring req.", GetName());
		return false;
	}

	Stop();
	Save();

	if (GetSectree())
	{
		GetSectree()->RemoveEntity(this);
		ViewCleanup();
		EncodeRemovePacket(this);
	}

	m_lWarpMapIndex = lMapIndex;
	m_posWarp.x = x;
	m_posWarp.y = y;

	// TODO: This log message should mention channel we are changing to instead of port.
	sys_log(0, "ChangeChannel %s, %ld %ld map %ld to port %d", GetName(), x, y, GetMapIndex(), wPort);

	TPacketGCWarp p;

	p.bHeader = HEADER_GC_WARP;
	p.lX = x;
	p.lY = y;
	p.lAddr = lAddr;
	p.wPort = wPort;

	GetDesc()->Packet(&p, sizeof(p));

	char buf[256];
	// TODO: This log message should mention channel we are changing to instead of port
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
		sys_err("No switch channel event info!");
		return 0;
	}

	LPCHARACTER	ch = info->ch;
	if (!ch)
	{
		sys_err("No char to work on for the switch.");
		return 0;
	}

	if (!ch->GetDesc())
		return 0;

	if (info->secs > 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ch->ChatPacketNew(CHAT_TYPE_INFO, 658, "%d", info->secs);
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
void CHARACTER::BlockProcessed() {
	if (!m_pkDropEvent) {
		sys_err("<drop_event> process failed, event is null.");
	}
	else {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 42, "");
#endif
		event_cancel(&m_pkDropEvent);
		m_pkDropEvent = nullptr;
		sys_log(0, "<drop_event> processed.");
	}
}

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

void CHARACTER::BlockDrop() {
	if (!IsPC()) {
		return;
	}

	if (GetMapIndex() != 358 && GetMapIndex() != 359 && GetMapIndex() != 360 && GetMapIndex() != 361) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 36, "");
#endif
		return;
	}

	if (m_pkDropEvent) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 44, "");
#endif
		return;
	}

	drop_event_info* info = AllocEventInfo<drop_event_info>();
	info->ch = this;
	info->time = get_global_time() + 5;
	info->drop = false;
	m_pkDropEvent = event_create(drop_event, info, PASSES_PER_SEC(1));
#ifdef TEXTS_IMPROVEMENT
	ChatPacketNew(CHAT_TYPE_INFO, 43, "%d", 5);
#endif
}

void CHARACTER::UnblockDrop() {
	if (GetMapIndex() != 358 && GetMapIndex() != 359 && GetMapIndex() != 360 && GetMapIndex() != 361) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 36, "");
#endif
		return;
	}

	if (m_pkDropEvent) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 44, "");
#endif
		return;
	}

	drop_event_info* info = AllocEventInfo<drop_event_info>();
	info->ch = this;
	info->time = get_global_time() + 5;
	info->drop = true;
	m_pkDropEvent = event_create(drop_event, info, PASSES_PER_SEC(1));
#ifdef TEXTS_IMPROVEMENT
	ChatPacketNew(CHAT_TYPE_INFO, 43, "%d", 5);
#endif
}

void CHARACTER::SetDropStatus() {
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







