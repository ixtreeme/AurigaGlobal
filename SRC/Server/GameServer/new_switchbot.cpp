#include "stdafx.h"

#ifdef ENABLE_SWITCHBOT
#include "new_switchbot.h"
#include "desc.h"
#include "item.h"
#include "item_manager.h"
#include "char_manager.h"
#include "buffer_manager.h"
#include "char_interface.hpp"
#include "config.h"
#include "p2p.h"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/Registry.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/CharacterAccessors.hpp"
#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

bool ValidPosition(uint32_t wCell)
{
	return wCell < SWITCHBOT_SLOT_COUNT;
}

const float c_fSpeed = 0.20f;

bool SwitchbotHelper::IsValidItem(LPITEM pkItem)
{
	if (!pkItem)
	{
		return false;
	}

	switch (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)))
	{
	case ITEM_WEAPON:
		return true;

	case ITEM_ARMOR:
		switch (ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)))
		{
		case ARMOR_BODY:
		case ARMOR_HEAD:
		case ARMOR_SHIELD:
		case ARMOR_WRIST:
		case ARMOR_FOOTS:
		case ARMOR_NECK:
		case ARMOR_EAR:
			return true;
		}

	default:
		return false;
	}
}


CSwitchbot::CSwitchbot()
{
	m_pkSwitchEvent = nullptr;
	m_table = {};
	m_isWarping = false;
}

CSwitchbot::~CSwitchbot()
{
	if (m_pkSwitchEvent)
	{
		event_cancel(&m_pkSwitchEvent);
		m_pkSwitchEvent = nullptr;
	}

	m_table = {};
	m_isWarping = false;
}

void CSwitchbot::SetTable(TSwitchbotTable table)
{
	m_table = table;
}

TSwitchbotTable CSwitchbot::GetTable()
{
	return m_table;
}

void CSwitchbot::SetPlayerId(uint32_t player_id)
{
	m_table.player_id = player_id;
}

uint32_t CSwitchbot::GetPlayerId(uint32_t player_id)
{
	return m_table.player_id;
}

void CSwitchbot::RegisterItem(uint16_t wCell, uint32_t item_id)
{
	if (!ValidPosition(wCell))
	{
		return;
	}

	m_table.items[wCell] = item_id;
}

void CSwitchbot::UnregisterItem(uint16_t wCell)
{
	if (!ValidPosition(wCell))
	{
		return;
	}

	m_table.items[wCell] = 0;
	m_table.active[wCell] = false;
	m_table.finished[wCell] = false;
	memset(&m_table.alternatives[wCell], 0, sizeof(m_table.alternatives[wCell]));
}

void CSwitchbot::SetAttributes(uint8_t slot, std::vector<TSwitchbotAttributeAlternativeTable> vec_alternatives)
{
	if (!ValidPosition(slot))
	{
		return;
	}

	for (uint8_t alternative = 0; alternative < SWITCHBOT_ALTERNATIVE_COUNT; ++alternative)
	{
		for (uint8_t attrIdx = 0; attrIdx < MAX_NORM_ATTR_NUM; ++attrIdx)
		{
			m_table.alternatives[slot][alternative].attributes[attrIdx].bType = vec_alternatives[alternative].attributes[attrIdx].bType;
			m_table.alternatives[slot][alternative].attributes[attrIdx].sValue = vec_alternatives[alternative].attributes[attrIdx].sValue;
		}
	}
}

void CSwitchbot::SetActive(uint8_t slot, bool active)
{
	if (!ValidPosition(slot))
	{
		return;
	}

	m_table.active[slot] = active;
	m_table.finished[slot] = false;
}

EVENTINFO(TSwitchbotEventInfo)
{
	CSwitchbot* pkSwitchbot;

	TSwitchbotEventInfo() : pkSwitchbot(nullptr)
	{
	}
};

EVENTFUNC(switchbot_event)
{
	TSwitchbotEventInfo* info = dynamic_cast<TSwitchbotEventInfo*>(event->info);

	if (info == nullptr)
	{
		sys_err("switchbot_event> <Factor> Info Null pointer");
		return 0;
	}

	if (!info->pkSwitchbot)
	{
		sys_err("switchbot_event> <Factor> Switchbot Null pointer");
		return 0;
	}

	info->pkSwitchbot->SwitchItems();

	return PASSES_PER_SEC(c_fSpeed);
}

void CSwitchbot::Start()
{
	TSwitchbotEventInfo* info = AllocEventInfo<TSwitchbotEventInfo>();
	info->pkSwitchbot = this;

	m_pkSwitchEvent = event_create(switchbot_event, info, c_fSpeed);

	CSwitchbotManager::Instance().SendSwitchbotUpdate(m_table.player_id);
}

void CSwitchbot::Stop()
{
	if (m_pkSwitchEvent)
	{
		event_cancel(&m_pkSwitchEvent);
		m_pkSwitchEvent = nullptr;
	}

	memset(&m_table.active, 0, sizeof(m_table.active));

	CSwitchbotManager::Instance().SendSwitchbotUpdate(m_table.player_id);
}

void CSwitchbot::Pause()
{
	if (m_pkSwitchEvent)
	{
		event_cancel(&m_pkSwitchEvent);
		m_pkSwitchEvent = nullptr;
	}
}

bool CSwitchbot::IsActive(uint8_t slot)
{
	if (!ValidPosition(slot))
	{
		return false;
	}

	return m_table.active[slot];
}

bool CSwitchbot::HasActiveSlots()
{
	for (const auto& it : m_table.active)
	{
		if (it)
		{
			return true;
		}
	}

	return false;
}

bool CSwitchbot::IsSwitching()
{
	return m_pkSwitchEvent != nullptr;
}

bool CSwitchbot::IsWarping()
{
	return m_isWarping;
}

void CSwitchbot::SetIsWarping(bool warping)
{
	m_isWarping = warping;
}

#ifdef ENABLE_APPLY_NORMAL_HIT_DAMAGE_BONUS_50_NOTICE_RAZOR93

// --
static const char* GetHighAvgDmgFmtByLang(int lang)
{
	switch (lang)
	{
	case LANGUAGE_HU: return "|cffc71585[%s]|r Magas Átlagos kár-t forgatott: |cffffd700|H%s|h[%s]|h|r";
	case LANGUAGE_DE: return "|cffc71585[%s]|r hat soeben einen hohen Durchschnittsschaden-Bonus gerollt: |cffffd700|H%s|h[%s]|h|r";
	case LANGUAGE_RO: return "|cffc71585[%s]|r tocmai a rulat un bonus mare de Dauna medie: |cffffd700|H%s|h[%s]|h|r";
	case LANGUAGE_IT: return "|cffc71585[%s]|r ha appena rollato un alto bonus ai danni medi: |cffffd700|H%s|h[%s]|h|r";
	case LANGUAGE_TR: return "|cffc71585[%s]|r az önce yüksek Ortalama Hasar bonusu cevirdi: |cffffd700|H%s|h[%s]|h|r";
	case LANGUAGE_PL: return "|cffc71585[%s]|r wlasnie wylosowal wysoki bonus do srednich obrazen: |cffffd700|H%s|h[%s]|h|r";
	case LANGUAGE_PT: return "|cffc71585[%s]|r acabou de rolar um alto bônus de Dano Médio: |cffffd700|H%s|h[%s]|h|r";
	case LANGUAGE_ES: return "|cffc71585[%s]|r acaba de sacar un alto bono de Daño medio: |cffffd700|H%s|h[%s]|h|r";
	case LANGUAGE_CZ: return "|cffc71585[%s]|r práve vyroloval vysoky bonus na prumerné poskozeni: |cffffd700|H%s|h[%s]|h|r";
	default:          return "|cffc71585[%s]|r just rolled high average damage: |cffffd700|H%s|h[%s]|h|r"; // EN
	}
}

// --- 
std::string MakeFullItemLink(LPITEM pkItem, LPCHARACTER pkKiller)
{
	char itemlink[512];
	int len = 0;

	len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
		ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, pkItem)),
		ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pkItem), 0),
		ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pkItem), 1),
		ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pkItem), 2),
		0, // transmute
		0  // transmute2
	);

	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
	{
		uint8_t type = pkItem->GetAttributeType(i);
		short   val = pkItem->GetAttributeValue(i);
		if (type && val)
			len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
	}

	// killer nyelve (fallback: EN)
	int lang = LANGUAGE_EN;
	if (pkKiller && pkKiller->GetDesc())
		lang = pkKiller->GetDesc()->GetLanguage();

	const char* fmt = GetHighAvgDmgFmtByLang(lang);

	char szChat[1024];
	snprintf(szChat, sizeof(szChat), fmt,
		pkKiller ? pkKiller->GetName() : "Player",
		itemlink,
		pkItem ? pkItem->GetName() : "item");

	return std::string(szChat);
}

#endif // ENABLE_APPLY_NORMAL_HIT_DAMAGE_BONUS_50_NOTICE_RAZOR93

void CSwitchbot::SwitchItems()
{
	for (uint8_t bSlot = 0; bSlot < SWITCHBOT_SLOT_COUNT; ++bSlot)
	{
		if (!m_table.active[bSlot])
		{
			continue;
		}

		m_table.finished[bSlot] = false;

		const uint32_t item_id = m_table.items[bSlot];

		LPITEM pkItem = ITEM_MANAGER::Instance().Find(item_id);
		if (!pkItem)
		{
			continue;
		}

		const entt::entity ownerEntity = ItemSystem::GetItemOwnerEntity(EntityFactory::CreateItemEntity(g_registry, pkItem));
		LPCHARACTER pkOwner = ecs::LegacyCharOf(ownerEntity);
		if (!pkOwner)
		{
			return;
		}

		if (CheckItem(pkItem, bSlot))
		{
			LPDESC desc = pkOwner->GetDesc();
			if (desc)
			{
				char buf[512];
#ifdef ENABLE_MULTI_LANGUAGE
				TPacketGCWhisper pack;
				pack.bHeader = HEADER_GC_WHISPER;
				pack.bType = WHISPER_TYPE_SYSTEM;
				int len;
				switch (desc->GetLanguage()) {
					case LANGUAGE_RO: {
						strlcpy(pack.szNameFrom, "[Switchbot]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "Switchbot-ul a gãsit bonusurile pentru %s de pe slot-ul: %d.", pkItem->GetName(), bSlot + 1);
						break;
					}
					case LANGUAGE_IT: {
						strlcpy(pack.szNameFrom, "[Girabonus]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "Il girabonus ha trovato i bonus per %s slot(%d).", pkItem->GetName(), bSlot + 1);
						break;
					}
					case LANGUAGE_TR: {
						strlcpy(pack.szNameFrom, "[BonusTuru]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "Bonus turu, %s yuvasi (%d) için bonuslar buldu.", pkItem->GetName(), bSlot + 1);
						break;
					}
					case LANGUAGE_DE: {
						strlcpy(pack.szNameFrom, "[Switchbot]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "Der Switchbot hat Boni für %s Steckplätze(%d) gefunden.", pkItem->GetName(), bSlot + 1);
						break;
					}
					case LANGUAGE_PL: {
						strlcpy(pack.szNameFrom, "[Switchbot]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "Switchbot znalazl bonusy dla %s slotów(%d).", pkItem->GetName(), bSlot + 1);
						break;
					}
					case LANGUAGE_PT: {
						strlcpy(pack.szNameFrom, "[Switchbot]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "O Switchbot encontrou o bonus para %s slot(%d).", pkItem->GetName(), bSlot + 1);
						break;
					}
					case LANGUAGE_ES: {
						strlcpy(pack.szNameFrom, "[Girabonus]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "El girabonus encontró bonos para %s ranuras(%d).", pkItem->GetName(), bSlot + 1);
						break;
					}
					case LANGUAGE_CZ: {
						strlcpy(pack.szNameFrom, "[Žirabonus]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "Žirabonus našel bonusy pro %s slotu(%d).", pkItem->GetName(), bSlot + 1);
						break;
					}
					case LANGUAGE_HU: {
						strlcpy(pack.szNameFrom, "[Switchbot]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "A switchbot bónuszokat talált %s slot(%d) számára.", pkItem->GetName(), bSlot + 1);
						break;
					}
					default: {
						strlcpy(pack.szNameFrom, "[Switchbot]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "The switchbot have founded the bonus for %s slot(%d).", pkItem->GetName(), bSlot + 1);
						break;
					}
				}
#else
				int len = snprintf(buf, sizeof(buf), LC_TEXT("Bonuschange of %s (Slot: %d) successfully finished."), pkItem->GetName(), bSlot + 1);
#endif
				pack.wSize = sizeof(TPacketGCWhisper) + len;
				pkOwner->GetDesc()->BufferedPacket(&pack, sizeof(pack));
				pkOwner->GetDesc()->Packet(buf, len);
			}

			SetActive(bSlot, false);

			m_table.finished[bSlot] = true;

			if (!HasActiveSlots())
			{
				Stop();
			}
			else
			{
				CSwitchbotManager::Instance().SendSwitchbotUpdate(m_table.player_id);
			}
		}
		else
		{
			bool stop = true;
			if (SWITCHBOT_PRICE_TYPE == 1)
			{
				uint32_t dwTargetVnum = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, pkItem));
				bool bZodiacItem = (
#ifdef DISABLE_ZODIAC_ATT
	
				(dwTargetVnum == 12314141)
					) ? true : false;
#else
									((dwTargetVnum >= 19290) && (dwTargetVnum <= 19312)) || 
									((dwTargetVnum >= 19490) && (dwTargetVnum <= 19512)) || 
									((dwTargetVnum >= 19690) && (dwTargetVnum <= 19712)) || 
									((dwTargetVnum >= 19890) && (dwTargetVnum <= 19912)) || 
									((dwTargetVnum >= 300) && (dwTargetVnum <= 319)) || 
									(dwTargetVnum == 329) || 
									(dwTargetVnum == 339) || 
									(dwTargetVnum == 349) || 
									(dwTargetVnum == 359) || 
									(dwTargetVnum == 369) || 
									(dwTargetVnum == 379) || 
									(dwTargetVnum == 389) || 
									(dwTargetVnum == 399) || 
									((dwTargetVnum >= 1180) && (dwTargetVnum <= 1189)) || 
									(dwTargetVnum == 1199) || 
									(dwTargetVnum == 1209) || 
									(dwTargetVnum == 1219) || 
									(dwTargetVnum == 1229) || 
									((dwTargetVnum >= 2200) && (dwTargetVnum <= 2209)) || 
									(dwTargetVnum == 2219) || 
									(dwTargetVnum == 2229) || 
									(dwTargetVnum == 2239) || 
									(dwTargetVnum == 2249) || 
									((dwTargetVnum >= 3220) && (dwTargetVnum <= 3229)) || 
									(dwTargetVnum == 3239) || 
									(dwTargetVnum == 3249) || 
									(dwTargetVnum == 3259) || 
									(dwTargetVnum == 3269) || 
									((dwTargetVnum >= 5160) && (dwTargetVnum <= 5169)) || 
									(dwTargetVnum == 5179) || 
									(dwTargetVnum == 5189) || 
									(dwTargetVnum == 5199) || 
									(dwTargetVnum == 5209) || 
									((dwTargetVnum >= 7300) && (dwTargetVnum <= 7309)) || 
									(dwTargetVnum == 7319) || 
									(dwTargetVnum == 7329) || 
									(dwTargetVnum == 7339) || 
									(dwTargetVnum == 7349) ||
									((dwTargetVnum >= 8500) && (dwTargetVnum <= 8569)) ||
									((dwTargetVnum >= 8640) && (dwTargetVnum <= 8739))
									)? true : false;
#endif
				
				if (bZodiacItem) {
					if (pkOwner->CountSpecifyItem(86060) >= SWITCHBOT_PRICE_AMOUNT) {
						stop = false;
					}

					LPITEM tpkItem = pkOwner->FindSpecifyItem(86060, true);
					if (tpkItem && !stop)
					{
						ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, tpkItem), SWITCHBOT_PRICE_AMOUNT);
#ifdef ENABLE_RANKING
						pkOwner->SetRankPoints(12, pkOwner->GetRankPoints(12) + 1);
#endif
#ifdef ENABLE_BATTLE_PASS
							uint8_t bBattlePassId = pkOwner->GetBattlePassId();
						if(bBattlePassId)
						{
							uint32_t dwItemVnum, dwUseCount;
							if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM, &dwItemVnum, &dwUseCount))
							{
								if(dwItemVnum == ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) && pkOwner->GetMissionProgress(USE_ITEM, bBattlePassId) < dwUseCount)
									pkOwner->UpdateMissionProgress(USE_ITEM, bBattlePassId, 1, dwUseCount);
							}

							if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM1, &dwItemVnum, &dwUseCount))
							{
								if (dwItemVnum == ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) && pkOwner->GetMissionProgress(USE_ITEM1, bBattlePassId) < dwUseCount)
									pkOwner->UpdateMissionProgress(USE_ITEM1, bBattlePassId, 1, dwUseCount);
							}

							if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM2, &dwItemVnum, &dwUseCount))
							{
								if (dwItemVnum == ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) && pkOwner->GetMissionProgress(USE_ITEM2, bBattlePassId) < dwUseCount)
									pkOwner->UpdateMissionProgress(USE_ITEM2, bBattlePassId, 1, dwUseCount);
							}
						}
#endif
						ItemSystem::ChangeItemAttributeEcs(EntityFactory::CreateItemEntity(g_registry, pkItem));
						SendItemUpdate(pkOwner, bSlot, pkItem);



						continue;
					}
				} else {
					for (const auto& itemVnum : c_arSwitchingItems)
					{
						//CHECK_LIMITED_ITEM START
						if (itemVnum == 71151 || itemVnum == 76023)
						{
							if ((ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ITEM_WEAPON) || ((ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ITEM_ARMOR && ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_BODY)
#define __USE_ADD_WITH_ALL_ITEMS__
#ifdef __USE_ADD_WITH_ALL_ITEMS__
								|| (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ITEM_ARMOR && ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_HEAD)
								|| (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ITEM_ARMOR && ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_SHIELD)
								|| (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ITEM_ARMOR && ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_WRIST)
								|| (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ITEM_ARMOR && ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_FOOTS)
								|| (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ITEM_ARMOR && ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_NECK)
								|| (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ITEM_ARMOR && ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_EAR)
#endif
								))
							{
								bool bCanUse = true;
								for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
								{
									if (pkItem->GetLimitType(i) == LIMIT_LEVEL && pkItem->GetLimitValue(i) > 30)
									{
										bCanUse = false;
										break;
									}
								}
								if (false == bCanUse)
								{
									continue;
								}
								else
								{
									if (pkOwner->CountSpecifyItem(itemVnum) >= SWITCHBOT_PRICE_AMOUNT)
									{
										stop = false;
									}
									
									LPITEM tpkItem = pkOwner->FindSpecifyItem(itemVnum, true);
									if (tpkItem && !stop)
									{
										ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, tpkItem), SWITCHBOT_PRICE_AMOUNT);
#ifdef ENABLE_RANKING
										if (ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) == 86051 || ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) == 88965)
											pkOwner->SetRankPoints(13, pkOwner->GetRankPoints(13) + 1);
										else
											pkOwner->SetRankPoints(12, pkOwner->GetRankPoints(12) + 1);
#endif

#ifdef ENABLE_BATTLE_PASS
										uint8_t bBattlePassId = pkOwner->GetBattlePassId();
										if(bBattlePassId)
										{
											uint32_t dwItemVnum, dwUseCount;
											if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM, &dwItemVnum, &dwUseCount))
											{
												if(dwItemVnum == ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) && pkOwner->GetMissionProgress(USE_ITEM, bBattlePassId) < dwUseCount)
													pkOwner->UpdateMissionProgress(USE_ITEM, bBattlePassId, 1, dwUseCount);
											}

											if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM1, &dwItemVnum, &dwUseCount))
											{
												if (dwItemVnum == ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) && pkOwner->GetMissionProgress(USE_ITEM1, bBattlePassId) < dwUseCount)
													pkOwner->UpdateMissionProgress(USE_ITEM1, bBattlePassId, 1, dwUseCount);
											}

											if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM2, &dwItemVnum, &dwUseCount))
											{
												if (dwItemVnum == ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) && pkOwner->GetMissionProgress(USE_ITEM2, bBattlePassId) < dwUseCount)
													pkOwner->UpdateMissionProgress(USE_ITEM2, bBattlePassId, 1, dwUseCount);
											}
										}
#endif
										ItemSystem::ChangeItemAttributeEcs(EntityFactory::CreateItemEntity(g_registry, pkItem));
										SendItemUpdate(pkOwner, bSlot, pkItem);
										break;
									}
								}
							}
							else
							{
								continue;
							}
						}
						//CHECK_LIMITED_ITEM END
						
						if (pkOwner->CountSpecifyItem(itemVnum) >= SWITCHBOT_PRICE_AMOUNT)
						{
							stop = false;
						}
						LPITEM tpkItem = pkOwner->FindSpecifyItem(itemVnum, true);
						if (tpkItem && !stop)
						{
							ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, tpkItem), SWITCHBOT_PRICE_AMOUNT);
#ifdef ENABLE_RANKING
							if (ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) == 86051 || ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) == 88965)
								pkOwner->SetRankPoints(13, pkOwner->GetRankPoints(13) + 1);
							else
								pkOwner->SetRankPoints(12, pkOwner->GetRankPoints(12) + 1);
#endif

#ifdef ENABLE_BATTLE_PASS
							uint8_t bBattlePassId = pkOwner->GetBattlePassId();
							if(bBattlePassId)
							{
								uint32_t dwItemVnum, dwUseCount;
								if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM, &dwItemVnum, &dwUseCount))
								{
									if(dwItemVnum == ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) && pkOwner->GetMissionProgress(USE_ITEM, bBattlePassId) < dwUseCount)
										pkOwner->UpdateMissionProgress(USE_ITEM, bBattlePassId, 1, dwUseCount);
								}

								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM1, &dwItemVnum, &dwUseCount))
								{
									if (dwItemVnum == ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) && pkOwner->GetMissionProgress(USE_ITEM1, bBattlePassId) < dwUseCount)
										pkOwner->UpdateMissionProgress(USE_ITEM1, bBattlePassId, 1, dwUseCount);
								}

								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM2, &dwItemVnum, &dwUseCount))
								{
									if (dwItemVnum == ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, tpkItem)) && pkOwner->GetMissionProgress(USE_ITEM2, bBattlePassId) < dwUseCount)
										pkOwner->UpdateMissionProgress(USE_ITEM2, bBattlePassId, 1, dwUseCount);
								}
							}
#endif
							ItemSystem::ChangeItemAttributeEcs(EntityFactory::CreateItemEntity(g_registry, pkItem));
							SendItemUpdate(pkOwner, bSlot, pkItem);
							break;
						}
					}
				}
			}
			else if (SWITCHBOT_PRICE_TYPE == 2)
			{
				if (pkOwner->GetGold() >= SWITCHBOT_PRICE_AMOUNT)
				{
					stop = false;
				}
			}

			if (stop)
			{
				SetActive(bSlot, false);
				if (!HasActiveSlots()) {
					Stop();
				} else {
					CSwitchbotManager::Instance().SendSwitchbotUpdate(pkOwner->GetPlayerID());
				}

#ifdef TEXTS_IMPROVEMENT
				if (SWITCHBOT_PRICE_TYPE == 1) {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkOwner), CHAT_TYPE_INFO, 754, "");
				} else {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkOwner), CHAT_TYPE_INFO, 755, "");
				}
#endif
				return;
			}
		}
	}
}
#ifdef ENABLE_APPLY_NORMAL_HIT_DAMAGE_BONUS_50_NOTICE_RAZOR93
bool CSwitchbot::CheckItem(LPITEM pkItem, uint8_t slot)
{
	if (!ValidPosition(slot))
		return false;

	if (!pkItem)
		return false;

	bool checked = false;

	for (const auto& alternative : m_table.alternatives[slot])
	{
		if (!alternative.IsConfigured())
			continue;

		uint8_t configuredAttrCount = 0;
		uint8_t correctAttrCount = 0;

		for (const auto& destAttr : alternative.attributes)
		{
			if (!destAttr.bType || !destAttr.sValue)
				continue;

			++configuredAttrCount;

			for (uint8_t attrIdx = 0; attrIdx < MAX_NORM_ATTR_NUM; ++attrIdx)
			{
				const TPlayerItemAttribute& curAttr = ItemSystem::GetItemAttribute(EntityFactory::CreateItemEntity(g_registry, pkItem), attrIdx);

				if (curAttr.bType != destAttr.bType || curAttr.sValue < destAttr.sValue)
					continue;

				//kiras
				static std::map<uint32_t, time_t> lastNoticedTime;
				const time_t now = time(nullptr);
				
				if (curAttr.bType == APPLY_NORMAL_HIT_DAMAGE_BONUS && curAttr.sValue > BONUSZ)
				{
					uint32_t itemID = ItemSystem::GetItemID(EntityFactory::CreateItemEntity(g_registry, pkItem)); // vagy más egyedi azonosító
					if (now - lastNoticedTime[itemID] > BONUSZ_TIME) // csak 10 másodpercenként 1x
					{
						lastNoticedTime[itemID] = now;

						const entt::entity ownerEntity = ItemSystem::GetItemOwnerEntity(EntityFactory::CreateItemEntity(g_registry, pkItem));
						LPCHARACTER pkOwner = ecs::LegacyCharOf(ownerEntity);
						if (pkOwner)
						{
							std::string chatMsg = MakeFullItemLink(pkItem, pkOwner);
							BroadcastNotice(chatMsg.c_str());
						}
					}
				}

				
				++correctAttrCount;
				break;
			}
		}

		checked = true;

		if (configuredAttrCount == correctAttrCount)
			return true;
	}

	if (!checked)
		return true;

	return false;
}
#else

bool CSwitchbot::CheckItem(LPITEM pkItem, uint8_t slot)
{
	if (!ValidPosition(slot))
	{
		return false;
	}

	if (!pkItem)
	{
		return false;
	}

	bool checked = 0;

	for (const auto& alternative : m_table.alternatives[slot])
	{
		if (!alternative.IsConfigured())
		{
			continue;
		}

		uint8_t configuredAttrCount = 0;
		uint8_t correctAttrCount = 0;

		for (const auto& destAttr : alternative.attributes)
		{
			if (!destAttr.bType || !destAttr.sValue)
			{
				continue;
			}

			++configuredAttrCount;

			for (uint8_t attrIdx = 0; attrIdx < MAX_NORM_ATTR_NUM; ++attrIdx)
			{
				const TPlayerItemAttribute& curAttr = ItemSystem::GetItemAttribute(EntityFactory::CreateItemEntity(g_registry, pkItem), attrIdx);

				if (curAttr.bType != destAttr.bType || curAttr.sValue < destAttr.sValue)
				{
					continue;
				}

				++correctAttrCount;
				break;
			}
		}

		checked = true;

		if (configuredAttrCount == correctAttrCount)
		{

			return true;
		}
	}
	

	if (!checked)
	{
		return true;
	}

	return false;
}
#endif
void CSwitchbot::SendItemUpdate(LPCHARACTER ch, uint8_t slot, LPITEM item)
{
	LPDESC desc = ch->GetDesc();
	if (!desc)
	{
		return;
	}

	TPacketGCSwitchbot pack;
	pack.header = HEADER_GC_SWITCHBOT;
	pack.subheader = SUBHEADER_GC_SWITCHBOT_UPDATE_ITEM;
	pack.size = sizeof(TPacketGCSwitchbot) + sizeof(TSwitchbotUpdateItem);

	TSwitchbotUpdateItem update = {};
	update.slot = slot;
	update.vnum = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item));
	update.count = ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item));

	memcpy(update.alSockets, item->GetSockets(), sizeof(update.alSockets));
	memcpy(update.aAttr, item->GetAttributes(), sizeof(update.aAttr));

	desc->BufferedPacket(&pack, sizeof(pack));
	desc->Packet(&update, sizeof(TSwitchbotUpdateItem));
}

CSwitchbotManager::CSwitchbotManager()
{
	Initialize();
}

CSwitchbotManager::~CSwitchbotManager()
{
	Initialize();
}

void CSwitchbotManager::Initialize()
{
	for (const auto& m_map_Switchbot : m_map_Switchbots)
{
	CSwitchbot* pkSwitchbot = m_map_Switchbot.second;
	if (pkSwitchbot != nullptr) {
		pkSwitchbot->~CSwitchbot();
		delete pkSwitchbot;
		pkSwitchbot = nullptr;
	}
}
	m_map_Switchbots.clear();
}

void CSwitchbotManager::RegisterItem(uint32_t player_id, uint32_t item_id, uint16_t wCell)
{
	CSwitchbot* pkSwitchbot = FindSwitchbot(player_id);
	if (!pkSwitchbot)
	{
		pkSwitchbot = new CSwitchbot();
		pkSwitchbot->SetPlayerId(player_id);
		m_map_Switchbots.insert(std::make_pair(player_id, pkSwitchbot));
	}

	if (pkSwitchbot->IsWarping())
	{
		return;
	}

	pkSwitchbot->RegisterItem(wCell, item_id);
	SendSwitchbotUpdate(player_id);
}

void CSwitchbotManager::UnregisterItem(uint32_t player_id, uint16_t wCell)
{
	CSwitchbot* pkSwitchbot = FindSwitchbot(player_id);
	if (!pkSwitchbot)
	{
		return;
	}

	if (pkSwitchbot->IsWarping())
	{
		return;
	}

	pkSwitchbot->UnregisterItem(wCell);
	SendSwitchbotUpdate(player_id);
}

void CSwitchbotManager::Start(uint32_t player_id, uint8_t slot, std::vector<TSwitchbotAttributeAlternativeTable> vec_alternatives)
{
	if (!ValidPosition(slot))
	{
		return;
	}

	CSwitchbot* pkSwitchbot = FindSwitchbot(player_id);
	if (!pkSwitchbot)
	{
		sys_err("No Switchbot found for player_id %d slot %d", player_id, slot);
		return;
	}

	if (pkSwitchbot->IsActive(slot))
	{
		sys_err("Switchbot slot %d already running for player_id %d", slot, player_id);
		return;
	}

	pkSwitchbot->SetActive(slot, true);
	pkSwitchbot->SetAttributes(slot, vec_alternatives);

	if (pkSwitchbot->HasActiveSlots() && !pkSwitchbot->IsSwitching())
	{
		pkSwitchbot->Start();
	}
	else
	{
		SendSwitchbotUpdate(player_id);
	}
}

void CSwitchbotManager::Stop(uint32_t player_id, uint8_t slot)
{
	if (!ValidPosition(slot))
	{
		return;
	}

	CSwitchbot* pkSwitchbot = FindSwitchbot(player_id);
	if (!pkSwitchbot)
	{
		sys_err("No Switchbot found for player_id %d slot %d", player_id, slot);
		return;
	}

	if (!pkSwitchbot->IsActive(slot))
	{
		sys_err("Switchbot slot %d is not running for player_id %d", slot, player_id);
		return;
	}

	pkSwitchbot->SetActive(slot, false);

	if (!pkSwitchbot->HasActiveSlots() && pkSwitchbot->IsSwitching())
	{
		pkSwitchbot->Stop();
	}
	else
	{
		SendSwitchbotUpdate(player_id);
	}
}

bool CSwitchbotManager::IsActive(uint32_t player_id, uint8_t slot)
{
	if (!ValidPosition(slot))
	{
		return false;
	}

	CSwitchbot* pkSwitchbot = FindSwitchbot(player_id);
	if (!pkSwitchbot)
	{
		return false;
	}

	return pkSwitchbot->IsActive(slot);
}

bool CSwitchbotManager::IsWarping(uint32_t player_id)
{
	CSwitchbot* pkSwitchbot = FindSwitchbot(player_id);
	if (!pkSwitchbot)
	{
		return false;
	}

	return pkSwitchbot->IsWarping();
}

void CSwitchbotManager::SetIsWarping(uint32_t player_id, bool warping)
{
	CSwitchbot* pkSwitchbot = FindSwitchbot(player_id);
	if (!pkSwitchbot)
	{
		return;
	}

	pkSwitchbot->SetIsWarping(warping);
}

CSwitchbot* CSwitchbotManager::FindSwitchbot(uint32_t player_id)
{
	const auto& it = m_map_Switchbots.find(player_id);
	if (it == m_map_Switchbots.end())
	{
		return nullptr;
	}

	return it->second;
}

void CSwitchbotManager::P2PSendSwitchbot(uint32_t player_id, uint16_t wTargetPort)
{
	CSwitchbot* pkSwitchbot = FindSwitchbot(player_id);
	if (!pkSwitchbot)
	{
		//sys_err("No switchbot found to transfer. (pid %d source_port %d target_port %d)", player_id, mother_port, wTargetPort);
		return;
	}

	pkSwitchbot->Pause();
	m_map_Switchbots.erase(player_id);

	TPacketGGSwitchbot pack;
	pack.wPort = wTargetPort;
	pack.table = pkSwitchbot->GetTable();

	P2P_MANAGER::Instance().Send(&pack, sizeof(pack));
	delete pkSwitchbot;
	pkSwitchbot = nullptr;
}

void CSwitchbotManager::P2PReceiveSwitchbot(TSwitchbotTable table)
{
	CSwitchbot* pkSwitchbot = FindSwitchbot(table.player_id);
	if (!pkSwitchbot)
	{
		pkSwitchbot = new CSwitchbot();
		m_map_Switchbots.insert(std::make_pair(table.player_id, pkSwitchbot));
	}

	pkSwitchbot->SetTable(table);
}

void CSwitchbotManager::SendItemAttributeInformations(LPCHARACTER ch)
{
	if (!ch)
	{
		return;
	}

	LPDESC desc = ch->GetDesc();
	if (!desc)
	{
		return;
	}

	TPacketGCSwitchbot pack;
	pack.header = HEADER_GC_SWITCHBOT;
	pack.subheader = SUBHEADER_GC_SWITCHBOT_SEND_ATTRIBUTE_INFORMATION;
	pack.size = sizeof(TPacketGCSwitchbot);

	TEMP_BUFFER buf;
	for (uint8_t bAttributeSet = 0; bAttributeSet < ATTRIBUTE_SET_MAX_NUM; ++bAttributeSet)
	{
		for (int iApplyNum = 0; iApplyNum < MAX_APPLY_NUM; ++iApplyNum)
		{
			const TItemAttrTable& r = g_map_itemAttr[iApplyNum];

			uint8_t max = r.bMaxLevelBySet[bAttributeSet];
			if (max > 0)
			{
				TSwitchbottAttributeTable table = {};
				table.attribute_set = bAttributeSet;
				table.apply_num = iApplyNum;
				table.max_value = r.lValues[max-1];

				buf.write(&table, sizeof(table));
			}
		}
	}

	if (buf.size())
	{
		pack.size += buf.size();
		desc->BufferedPacket(&pack, sizeof(pack));
		desc->Packet(buf.read_peek(), buf.size());
	}
	else
	{
		desc->Packet(&pack, sizeof(pack));
	}
}

void CSwitchbotManager::SendSwitchbotUpdate(uint32_t player_id)
{
	CSwitchbot* pkSwitchbot = FindSwitchbot(player_id);
	if (!pkSwitchbot)
	{
		return;
	}

	LPCHARACTER ch = CHARACTER_MANAGER::Instance().FindByPID(player_id);
	if (!ch)
	{
		return;
	}

	LPDESC desc = ch->GetDesc();
	if (!desc)
	{
		return;
	}

	TSwitchbotTable table = pkSwitchbot->GetTable();

	TPacketGCSwitchbot pack;
	pack.header = HEADER_GC_SWITCHBOT;
	pack.subheader = SUBHEADER_GC_SWITCHBOT_UPDATE;
	pack.size = sizeof(TPacketGCSwitchbot) + sizeof(TSwitchbotTable);

	desc->BufferedPacket(&pack, sizeof(pack));
	desc->Packet(&table, sizeof(table));
}

void CSwitchbotManager::EnterGame(LPCHARACTER ch)
{
	SendItemAttributeInformations(ch);
	SetIsWarping(ch->GetPlayerID(), false);
	SendSwitchbotUpdate(ch->GetPlayerID());
	
	CSwitchbot* pkSwitchbot = FindSwitchbot(ch->GetPlayerID());
	if (pkSwitchbot && pkSwitchbot->HasActiveSlots() && !pkSwitchbot->IsSwitching())
	{
		pkSwitchbot->Start();
	}
}
#endif
