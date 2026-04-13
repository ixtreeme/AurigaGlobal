#include "stdafx.h"

#ifdef ENABLE_NEW_FISHING_SYSTEM
#include "utils.h"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "desc.h"
#include "item.h"
#include "item_manager.h"
#include "unique_item.h"
#include "fishing.h"
#include "vector.h"
#include "packet.h"
#include "sectree.h"
#include "sectree_manager.h"
#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

EVENTFUNC(fishing_event) {
	fishingnew_event_info * info = dynamic_cast<fishingnew_event_info *>(event->info);
	if (info == nullptr) {
		return 0;
	}

	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(info->pid);
	if (!ch) {
		return 0;
	}

	if (ch->GetFishCatch() >= FISHING_NEED_CATCH){
		ch->fishing_catch_decision(info->vnum);
		return 0;
	} else {
		LPITEM rod = ch->GetWear(WEAR_WEAPON);
		if (!(rod && rod->GetType() == ITEM_ROD)) {
			ch->fishing_new_stop();
			return 0;
		}

		if (info->sec == 1)
		{
			TItemTable* pTable = ITEM_MANAGER::instance().GetTable(info->vnum);
			if (pTable)
			{
#ifdef TEXTS_IMPROVEMENT
#ifdef ENABLE_MULTI_NAMES
				uint8_t lang = 0;
				if (LPDESC d = ch->GetDesc())
					lang = d->GetLanguage();
				ch->ChatPacketNew(CHAT_TYPE_INFO, 896, "%s", pTable->szLocaleName[lang]);
#else
				ch->ChatPacketNew(CHAT_TYPE_INFO, 896, "%s", pTable->szLocaleName);
#endif
#endif
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				ch->ChatPacketNew(CHAT_TYPE_INFO, 897, "");
#endif
			}
		}

		uint32_t failed = ch->GetFishCatchFailed();
		if (failed > 0) {
			info->sec += failed;
			ch->SetFishCatchFailed(0);
		}

		if (info->sec >= 15) {
			ch->fishing_new_stop();
			return 0;
		}

		++info->sec;
		return (PASSES_PER_SEC(1));
	}
};

void CHARACTER::fishing_new_start() {
	if (m_pkFishingNewEvent) {
		return;
	}

	LPSECTREE_MAP pkSectreeMap = SECTREE_MANAGER::instance().GetMap(GetMapIndex());
	if (!pkSectreeMap) {
		return;
	}

	int x = GetX(), y = GetY();
	LPSECTREE tree = pkSectreeMap->Find(x, y);
	if (!tree) {
		return;
	}

	if (tree->IsAttr(x, y, ATTR_BLOCK)) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 894, "");
#endif
		return;
	}

	if (GetEmptyInventory(1) == -1) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 899, "");
#endif
		return;
	}

	LPITEM rod = GetWear(WEAR_WEAPON);
	if (!rod || rod->GetType() != ITEM_ROD) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 895, "");
#endif
		return;
	}

	if (rod->GetSocket(2) == 0) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 281, "");
#endif
		return;
	}

	float fx, fy;
	GetDeltaByDegree(GetRotation(), 400.0f, &fx, &fy);

	SetFishCatch(0);
	SetFishCatchFailed(0);
	SetLastCatchTime(0);

	uint32_t dwVnum = rod->GetVnum();
	bool second = dwVnum >= 27400 && dwVnum <= 27490 ? false : true;

	fishingnew_event_info* info = AllocEventInfo<fishingnew_event_info>();
	info->pid = GetPlayerID();
	info->vnum = fishingnew::GetFishCatchedVnum(100, 15 + GetPoint(POINT_FISHING_RARE) + rod->GetSocket(2), second);
	info->chance = 100;
	info->sec = 1;
	m_pkFishingNewEvent = event_create(fishing_event, info, PASSES_PER_SEC(1));

	TPacketFishingNew p;
	p.header = HEADER_GC_FISHING_NEW;
	p.subheader = FISHING_SUBHEADER_NEW_START;
	p.vid = GetVID();
	p.dir = (uint8_t)(GetRotation() / 5);
	p.need = FISHING_NEED_CATCH;
	p.count = 0;
	PacketAround(&p, sizeof(p));
}

void CHARACTER::fishing_new_stop() {
	if (!m_pkFishingNewEvent) {
		return;
	}

	event_cancel(&m_pkFishingNewEvent);
	m_pkFishingNewEvent = nullptr;

	LPITEM rod = GetWear(WEAR_WEAPON);
	if (rod && rod->GetType() == ITEM_ROD) {
		rod->SetSocket(2, 0);
	}

	TPacketFishingNew p;
	p.header = HEADER_GC_FISHING_NEW;
	p.subheader = FISHING_SUBHEADER_NEW_STOP;
	p.vid = GetVID();
	p.dir = 0;
	p.need = 0;
	p.count = 0;
	PacketAround(&p, sizeof(p));
}

void CHARACTER::fishing_new_catch() {
	if (!m_pkFishingNewEvent) {
		return;
	}

	if (GetLastCatchTime() > get_global_time()) {
		return;
	}

	uint8_t v = GetFishCatch() + 1;
	SetLastCatchTime(get_global_time() + 1);
	SetFishCatch(v);

	TPacketFishingNew p;
	p.header = HEADER_GC_FISHING_NEW;
	p.subheader = FISHING_SUBHEADER_NEW_CATCH;
	p.vid = GetVID();
	p.dir = 0;
	p.need = 0;
	p.count = v;
	PacketAround(&p, sizeof(p));
}

void CHARACTER::fishing_new_catch_failed() {
	if (!m_pkFishingNewEvent) {
		return;
	}

	SetFishCatchFailed(GetFishCatchFailed() + 1);

	TPacketFishingNew p;
	p.header = HEADER_GC_FISHING_NEW;
	p.subheader = FISHING_SUBHEADER_NEW_CATCH_FAILED;
	p.vid = GetVID();
	p.dir = 0;
	p.need = 0;
	p.count = 0;
	PacketAround(&p, sizeof(p));
}

void CHARACTER::fishing_catch_decision(uint32_t itemVnum) {
	if (!m_pkFishingNewEvent) {
		return;
	}

	event_cancel(&m_pkFishingNewEvent);
	m_pkFishingNewEvent = nullptr;

	LPITEM rod = GetWear(WEAR_WEAPON);
	if (!rod)
	{
		return;
	}

	if (rod->GetType() == ITEM_ROD)
	{
		if (rod->GetRefinedVnum()> 0 && rod->GetSocket(0) < rod->GetValue(2) && number(1, rod->GetValue(1)) == 1)
		{
			rod->SetSocket(0, rod->GetSocket(0) + 1);
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 283, "%d#%d", rod->GetSocket(0), rod->GetValue(2));
#endif
			if (rod->GetSocket(0) == rod->GetValue(2))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 279, "");
				ChatPacketNew(CHAT_TYPE_INFO, 280, "");
#endif
			}
		}

		rod->SetSocket(2, 0);
	}

	uint8_t chance;
	switch (itemVnum) {
		case 27803:
		case 27806:
		case 27816:
		case 27807:
		case 27818:
		case 27805:
		case 27822:
		case 27823:
		case 27824:
		case 27825:
		case 71136:	//Hatalmas nyalóka (1h)
		case 39065://Utalvány (1 SÉ)
		case 2870:	//Birodalom Rúna  
		case 2871:	//Ork Rúna  
		case 2873:	//Jég Birodalom Rúna  
		case 99998:	// Boss Pont
		case 39066:	// Gaya
		case 39068:	// Auriga Coin (10m Yang)
		case 80003:// Ezüst rúd (50.000 Yang)
		case 80004:// Ezüst rúd (100.000 Yang)
		case 80005:// Aranyrúd (500.000 Yang)
		case 80006:// Arany rúd (1 millió Yang)
		case 80007:// Arany rúd (2 millió Yang)
		case 89106:// Ork Run Belépö
		case 71175:// Nemere jegy
		case 30625:// Egy Darab menhir iszap 

			{
				chance = 50;
			}
			break;

		case 2872://Nefrit Rúna 5x
		case 2874:	//Sivatagi Rúna 15x
		case 30179:	// Csavart kulcs
		case 76019:	// Tritontemplom kulcs
		case 30713:	// Owl jegy
		case 30798:	// Ankh kereszt
		case 71095:	// Belépési engedély
		case 30320:	// Sárga Zsugorított f
		case 76025:	// Ochao jegy
		case 30613:	// Tündérek köve
		case 71174://Razador jegy
		case 30325://pok kulcs
		case 89101://Rúna jegy

		{
			chance = 15;
		}
		break;
		case 2875://Rémálom Rúna 5x
		case 2876://Sötét erdő Rúna 5x//
		//case 99998://bosspont
		case 70606://Felolvasztási bónusz
		case 70605://fagyasztási bónusz csere
		case 30617://Legendás Bónuszoló
		case 30618://Legendás Megváltoztató
		case 86052://Talizmánerösíto
		case 86051://Talizmán büvölo
		case 71123:// Sárkány pikkely
		case 71129:// Sárkány karom
		//case 71136:// Hatalmas Nyalóka (2h)
		case 27804:
		case 27811:
		case 27810:
		case 27809:
		case 27814:
		case 27812:
		case 27808:
		case 27826:
		case 27827:
		case 27813:
		case 27815:
		case 27819:
		case 27820:
		case 27821:
		case 2877:	//Tűz Rúna 5x
		case 2878:	//SD5 Rúna 5x
		case 60011:	//Paranoia Köve +6
		case 60031:	//Metin elleni ko+6
		case 60041:	//Stone of Boss+6
		case 2858:	//Profiq Pepsi-je
		case 53251:	//Frank (15d)
		case 18090:	//Turmalin öv+0
		case 55706:	//Mini Meley
		//case 71123:	//Sárkány pikkely
		//case 71129:	//Sárkány karom
		case 60010:	//Paranoia Köve +5
		case 60020:	//Háború Köve +5
		case 60030:	//Metin elleni ko+5
		case 60040:	//Boss Elleni  Kö+5
		case 60050:	//Inteligencia Köve +
		case 60060:	//Állatok Köve +5
		case 60070:	//Ügyesség Köve +5
		case 60080:	//Erö Köve+5
		case 72726:	//Nap elixír (E)
		case 72730:	//Hold elixír (E)
		case 50525:	//Tökéletes lélekkõ
			{
				chance = 5;
			}
			break;
		case 611516://szoposszaju hal mount
		{
			chance = 1;
		}
		break;
		default:
			{
				chance = 0;
			}
			break;
	}

	if (GetPoint(POINT_FISHING_RARE) > 0 && chance == 5) {
		chance += 20;
	}

	uint32_t dwVnum = rod->GetVnum();
	if (dwVnum >= 27400 && dwVnum <= 27490) {
		chance += (rod->GetValue(0) / 10) * 2;
	} else {
		chance += rod->GetValue(0) / 10;
	}

	TPacketFishingNew p;
	p.header = HEADER_GC_FISHING_NEW;
	if (number(1, 100) >= chance) {
		p.subheader = FISHING_SUBHEADER_NEW_CATCH_FAIL;
	} else {
#ifdef ENABLE_RANKING
		SetRankPoints(14, GetRankPoints(14) + 1);
#endif
#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = GetBattlePassId();
			if (bBattlePassId) {
				uint32_t dwCount, dwNotUsed;
				if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, CATCH_FISH, &dwNotUsed, &dwCount)) {
					if(GetMissionProgress(CATCH_FISH, bBattlePassId) < dwCount) {
						UpdateMissionProgress(CATCH_FISH, bBattlePassId, 1, dwCount);
					}
				}
			}
#endif

			p.subheader = FISHING_SUBHEADER_NEW_CATCH_SUCCESS;

			LPITEM pReward = AutoGiveItem(itemVnum, 1, -1, false);
			if (pReward)

			{
#ifdef ENABLE_MULTI_NAMES
				uint8_t lang = 0;
				if (LPDESC d = GetDesc())
					lang = d->GetLanguage();

				TItemTable* pTable = ITEM_MANAGER::instance().GetTable(itemVnum);
				const char* szName = (pTable ? pTable->szLocaleName[lang] : "UNKNOWN_ITEM");
#else
				TItemTable* pTable = ITEM_MANAGER::instance().GetTable(itemVnum);
				const char* szName = (pTable ? pTable->szLocaleName : "UNKNOWN_ITEM");
#endif
				//LPCHARACTER ch = CHARACTER_MANAGER::GetName().FindByPID(info->pid);
				
				const uint32_t rewardVnum = pReward ? pReward->GetVnum() : 0;

				if (rewardVnum == 611516)
				{
					char buf[256];
					snprintf(buf, sizeof(buf),
						"%s kifogta a Legendas vizi szornyet: %s",
						GetName(), szName);
					BroadcastNotice(buf);
				}

				 
				ChatPacket(CHAT_TYPE_INFO, "%s kaptal.", szName);
			}
			else
			{
				 
				ChatPacket(CHAT_TYPE_INFO, "nincs hely az inventoryban.");
			}

	}
	p.vid = GetVID();
	p.dir = 0;
	p.need = 0;
	p.count = 0;
	PacketAround(&p, sizeof(p));
}
#endif
