#include "stdafx.h"
#include "ecs/systems/PointSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"

#ifdef ENABLE_SWITCHBOT
#include "new_switchbot.h"
#include "desc.h"
#include "item_manager.h"
#include "char_manager.h"
#include "buffer_manager.h"
#include "config.h"
#include "p2p.h"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/Registry.hpp"
#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

namespace {
bool RequiresZodiacChanger(uint32_t vnum)
{
#ifdef DISABLE_ZODIAC_ATT
    return vnum == 12314141;
#else
    // Keep the switchbot's existing material rules, which are deliberately
    // separate from the broader base-apply exceptions used in bonus generation.
    constexpr uint32_t ranges[][2] = {
        {19290, 19312}, {19490, 19512}, {19690, 19712}, {19890, 19912},
        {300, 319}, {1180, 1189}, {2200, 2209}, {3220, 3229},
        {5160, 5169}, {7300, 7309}, {8500, 8569}, {8640, 8739}
    };
    for (const auto& range : ranges)
        if (vnum >= range[0] && vnum <= range[1])
            return true;
    constexpr uint32_t singles[] = {
        329, 339, 349, 359, 369, 379, 389, 399, 1199, 1209, 1219, 1229,
        2219, 2229, 2239, 2249, 3239, 3249, 3259, 3269,
        5179, 5189, 5199, 5209, 7319, 7329, 7339, 7349
    };
    return std::find(std::begin(singles), std::end(singles), vnum) != std::end(singles);
#endif
}

bool AllowsLimitedChanger(entt::entity item)
{
    const auto* proto = ItemSystem::GetItemProto(item);
    if (!proto)
        return false;
    if (proto->bType != ITEM_WEAPON) {
        if (proto->bType != ITEM_ARMOR)
            return false;
        switch (proto->bSubType) {
            case ARMOR_BODY: case ARMOR_HEAD: case ARMOR_SHIELD: case ARMOR_WRIST:
            case ARMOR_FOOTS: case ARMOR_NECK: case ARMOR_EAR: break;
            default: return false;
        }
    }
    for (const auto& limit : proto->aLimits)
        if (limit.bType == LIMIT_LEVEL && limit.lValue > 30)
            return false;
    return true;
}

entt::entity FindMaterial(entt::entity owner, entt::entity target, uint32_t vnum)
{
    const auto findInWindow = [&](uint8_t window, uint16_t size) {
        for (uint16_t cell = 0; cell < size; ++cell) {
            const entt::entity material = ItemSystem::GetItem(owner, TItemPos(window, cell));
            if (ItemSystem::IsValidItem(material) && ItemSystem::GetItemVnum(material) == vnum &&
                ItemSystem::CanPayItemAttributeCost(target, material, SWITCHBOT_PRICE_AMOUNT))
                return material;
        }
        return entt::entity{entt::null};
    };
#ifdef ENABLE_EXTRA_INVENTORY
    if (const auto material = findInWindow(EXTRA_INVENTORY, EXTRA_INVENTORY_MAX_NUM); material != entt::null)
        return material;
#endif
    return findInWindow(INVENTORY, INVENTORY_MAX_NUM);
}
void RecordSwitchProgress(entt::entity owner, uint32_t materialVnum)
{
    if (materialVnum == 0)
        return; // Yang payment is not an item-use mission.
#ifdef ENABLE_RANKING
    const int category = materialVnum == 86051 || materialVnum == 88965 ? 13 : 12;
    ecs::PlayerRuntime::SetRankPoints(owner, category,
        ecs::PlayerRuntime::GetRankPoints(owner, category) + 1);
#endif
#ifdef ENABLE_BATTLE_PASS
    const uint8_t battlePass = ecs::PlayerRuntime::GetBattlePassId(owner);
    if (battlePass != 0) {
        for (const auto mission : {USE_ITEM, USE_ITEM1, USE_ITEM2}) {
            uint32_t requiredVnum = 0;
            uint32_t requiredCount = 0;
            if (CBattlePass::instance().BattlePassMissionGetInfo(battlePass, mission, &requiredVnum, &requiredCount) &&
                requiredVnum == materialVnum && ecs::PlayerRuntime::GetMissionProgress(owner, mission, battlePass) < requiredCount)
                ecs::PlayerRuntime::UpdateMissionProgress(owner, mission, battlePass, 1, requiredCount);
        }
    }
#endif
}
} // namespace

SwitchbotHelper::Outcome SwitchbotHelper::TrySwitch(entt::entity owner, entt::entity item, uint8_t slot)
{
    if (!ecs::PlayerRuntime::IsValid(owner) || !ecs::PlayerRuntime::IsPC(owner) ||
        slot >= SWITCHBOT_SLOT_COUNT || !ItemSystem::IsValidItem(item) ||
        ItemSystem::GetItemOwner(item) != owner || ItemSystem::GetItemWindow(item) != SWITCHBOT ||
        ItemSystem::GetItemCell(item) != slot || ItemSystem::GetItem(owner, TItemPos(SWITCHBOT, slot)) != item ||
        ItemSystem::IsItemEquipped(item) || ItemSystem::IsItemExchanging(item) || ItemSystem::IsItemLocked(item) ||
        ItemSystem::GetItemAttributeSetIndex(item) < 0 || ItemSystem::GetItemAttributeCount(item) == 0)
        return {};

    if (SWITCHBOT_PRICE_TYPE == 2) {
        if (ecs::PointSystem::GetGold(owner) < SWITCHBOT_PRICE_AMOUNT)
            return {Result::NoPayment};
        return {ItemSystem::ChangeItemAttributeWithGoldCost(item, SWITCHBOT_PRICE_AMOUNT)
            ? Result::Success : Result::RollFailed};
    }
    if (SWITCHBOT_PRICE_TYPE != 1)
        return {};

    const bool zodiac = RequiresZodiacChanger(ItemSystem::GetItemVnum(item));
    const auto tryMaterial = [&](uint32_t vnum) -> Outcome {
        const auto material = FindMaterial(owner, item, vnum);
        if (material == entt::null)
            return {Result::NoPayment};
        return {ItemSystem::ChangeItemAttributeWithItemCost(item, material, SWITCHBOT_PRICE_AMOUNT)
            ? Result::Success : Result::RollFailed, vnum};
    };
    if (zodiac)
        return tryMaterial(86060);
    for (const auto vnum : c_arSwitchingItems) {
        if ((vnum == 71151 || vnum == 76023) && !AllowsLimitedChanger(item))
            continue;
        const auto outcome = tryMaterial(vnum);
        if (outcome.result != Result::NoPayment)
            return outcome;
    }
    return {Result::NoPayment};
}

bool ValidPosition(uint32_t wCell)
{
	return wCell < SWITCHBOT_SLOT_COUNT;
}

const float c_fSpeed = 0.20f;

bool SwitchbotHelper::IsValidItem(entt::entity item)
{
	if (item == entt::null)
	{
		return false;
	}

	switch (ItemSystem::GetItemType(item))
	{
	case ITEM_WEAPON:
		return true;

	case ITEM_ARMOR:
		switch (ItemSystem::GetItemSubType(item))
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
		LOG_ERROR("switchbot_event> <Factor> Info Null pointer");
		return 0;
	}

	if (!info->pkSwitchbot)
	{
		LOG_ERROR("switchbot_event> <Factor> Switchbot Null pointer");
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
std::string MakeFullItemLink(entt::entity item, entt::entity killer)
{
	char itemlink[512];
	int len = 0;

	len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
		ItemSystem::GetItemVnum(item),
		ItemSystem::GetItemSocket(item, 0),
		ItemSystem::GetItemSocket(item, 1),
		ItemSystem::GetItemSocket(item, 2),
		0, // transmute
		0  // transmute2
	);

	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
	{
		uint8_t type = ItemSystem::GetItemAttributeType(item, i);
		short   val = ItemSystem::GetItemAttributeValue(item, i);
		if (type && val)
			len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
	}

	// killer nyelve (fallback: EN)
	int lang = LANGUAGE_EN;
	if (ecs::PlayerRuntime::GetDesc(killer))
		lang = ecs::PlayerRuntime::GetDesc(killer)->GetLanguage();

	const char* fmt = GetHighAvgDmgFmtByLang(lang);

	char szChat[1024];
	snprintf(szChat, sizeof(szChat), fmt,
		ecs::PlayerRuntime::IsValid(killer) ? ecs::PlayerRuntime::GetName(killer).data() : "Player",
		itemlink,
		item != entt::null ? ItemSystem::GetItemName(item) : "item");

	return std::string(szChat);
}

#endif // ENABLE_APPLY_NORMAL_HIT_DAMAGE_BONUS_50_NOTICE_RAZOR93

void CSwitchbot::SwitchItems()
{
    if (m_isWarping)
        return;
    const auto stopSlot = [this](uint8_t slot) {
        SetActive(slot, false);
        if (!HasActiveSlots())
            Stop();
        else
            CSwitchbotManager::Instance().SendSwitchbotUpdate(m_table.player_id);
    };
	for (uint8_t bSlot = 0; bSlot < SWITCHBOT_SLOT_COUNT; ++bSlot)
	{
		if (!m_table.active[bSlot])
		{
			continue;
		}

		m_table.finished[bSlot] = false;

		const uint32_t item_id = m_table.items[bSlot];

        const entt::entity itemEntity = ItemSystem::FindItemByID(item_id);
        const entt::entity owner = ItemSystem::GetItemOwner(itemEntity);
        if (!ItemSystem::IsValidItem(itemEntity) || !ecs::PlayerRuntime::IsValid(owner) ||
            ecs::PlayerRuntime::GetPlayerID(owner) != m_table.player_id ||
            ItemSystem::GetItemWindow(itemEntity) != SWITCHBOT || ItemSystem::GetItemCell(itemEntity) != bSlot ||
            ItemSystem::GetItem(owner, TItemPos(SWITCHBOT, bSlot)) != itemEntity)
        {
            stopSlot(bSlot);
            continue;
        }

		if (CheckItem(itemEntity, bSlot))
		{
			LPDESC desc = ecs::PlayerRuntime::GetDesc(owner);
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
						len = snprintf(buf, sizeof(buf), "Switchbot-ul a gãsit bonusurile pentru %s de pe slot-ul: %d.", ItemSystem::GetItemName(itemEntity), bSlot + 1);
						break;
					}
					case LANGUAGE_IT: {
						strlcpy(pack.szNameFrom, "[Girabonus]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "Il girabonus ha trovato i bonus per %s slot(%d).", ItemSystem::GetItemName(itemEntity), bSlot + 1);
						break;
					}
					case LANGUAGE_TR: {
						strlcpy(pack.szNameFrom, "[BonusTuru]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "Bonus turu, %s yuvasi (%d) için bonuslar buldu.", ItemSystem::GetItemName(itemEntity), bSlot + 1);
						break;
					}
					case LANGUAGE_DE: {
						strlcpy(pack.szNameFrom, "[Switchbot]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "Der Switchbot hat Boni für %s Steckplätze(%d) gefunden.", ItemSystem::GetItemName(itemEntity), bSlot + 1);
						break;
					}
					case LANGUAGE_PL: {
						strlcpy(pack.szNameFrom, "[Switchbot]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "Switchbot znalazl bonusy dla %s slotów(%d).", ItemSystem::GetItemName(itemEntity), bSlot + 1);
						break;
					}
					case LANGUAGE_PT: {
						strlcpy(pack.szNameFrom, "[Switchbot]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "O Switchbot encontrou o bonus para %s slot(%d).", ItemSystem::GetItemName(itemEntity), bSlot + 1);
						break;
					}
					case LANGUAGE_ES: {
						strlcpy(pack.szNameFrom, "[Girabonus]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "El girabonus encontró bonos para %s ranuras(%d).", ItemSystem::GetItemName(itemEntity), bSlot + 1);
						break;
					}
					case LANGUAGE_CZ: {
						strlcpy(pack.szNameFrom, "[Žirabonus]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "Žirabonus našel bonusy pro %s slotu(%d).", ItemSystem::GetItemName(itemEntity), bSlot + 1);
						break;
					}
					case LANGUAGE_HU: {
						strlcpy(pack.szNameFrom, "[Switchbot]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "A switchbot bónuszokat talált %s slot(%d) számára.", ItemSystem::GetItemName(itemEntity), bSlot + 1);
						break;
					}
					default: {
						strlcpy(pack.szNameFrom, "[Switchbot]", sizeof(pack.szNameFrom));
						len = snprintf(buf, sizeof(buf), "The switchbot have founded the bonus for %s slot(%d).", ItemSystem::GetItemName(itemEntity), bSlot + 1);
						break;
					}
				}
#else
				int len = snprintf(buf, sizeof(buf), LC_TEXT("Bonuschange of %s (Slot: %d) successfully finished."), ItemSystem::GetItemName(itemEntity), bSlot + 1);
#endif
				pack.wSize = sizeof(TPacketGCWhisper) + len;
				ecs::PlayerRuntime::GetDesc(owner)->BufferedPacket(&pack, sizeof(pack));
				ecs::PlayerRuntime::GetDesc(owner)->Packet(buf, len);
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
            const auto outcome = SwitchbotHelper::TrySwitch(owner, itemEntity, bSlot);
            if (outcome.result == SwitchbotHelper::Result::Success)
            {
                RecordSwitchProgress(owner, outcome.materialVnum);
                SendItemUpdate(owner, bSlot, itemEntity);
                continue;
            }

            stopSlot(bSlot);
            if (outcome.result == SwitchbotHelper::Result::NoPayment)
            {
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, SWITCHBOT_PRICE_TYPE == 1 ? 754 : 755, "");
#endif
            }
            else if (outcome.result == SwitchbotHelper::Result::RollFailed)
            {
                LOG_ERROR("Switchbot reroll failed: player {} item {} slot {}; attributes and payment unchanged",
                    m_table.player_id, item_id, bSlot);
            }
		}
	}
}
#ifdef ENABLE_APPLY_NORMAL_HIT_DAMAGE_BONUS_50_NOTICE_RAZOR93
bool CSwitchbot::CheckItem(entt::entity item, uint8_t slot)
{
	if (!ValidPosition(slot))
		return false;

	if (item == entt::null)
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
				const TPlayerItemAttribute& curAttr = ItemSystem::GetItemAttribute(item, attrIdx);

				if (curAttr.bType != destAttr.bType || curAttr.sValue < destAttr.sValue)
					continue;

				//kiras
				static std::map<uint32_t, time_t> lastNoticedTime;
				const time_t now = time(nullptr);

				if (curAttr.bType == APPLY_NORMAL_HIT_DAMAGE_BONUS && curAttr.sValue > BONUSZ)
				{
					uint32_t itemID = ItemSystem::GetItemID(item); // vagy más egyedi azonosító
					if (now - lastNoticedTime[itemID] > BONUSZ_TIME) // csak 10 másodpercenként 1x
					{
						lastNoticedTime[itemID] = now;

						const entt::entity ownerEntity = ItemSystem::GetItemOwnerEntity(item);
						if (ecs::PlayerRuntime::IsValid(ownerEntity))
						{
							std::string chatMsg = MakeFullItemLink(item, ownerEntity);
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

bool CSwitchbot::CheckItem(entt::entity item, uint8_t slot)
{
	if (!ValidPosition(slot))
	{
		return false;
	}

	if (item == entt::null)
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
				const TPlayerItemAttribute& curAttr = ItemSystem::GetItemAttribute(item, attrIdx);

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
void CSwitchbot::SendItemUpdate(entt::entity ch, uint8_t slot, entt::entity item)
{
	LPDESC desc = ecs::PlayerRuntime::GetDesc(ch);
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
	update.vnum = ItemSystem::GetItemVnum(item);
	update.count = ItemSystem::GetItemCount(item);

	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		update.alSockets[i] = ItemSystem::GetItemSocket(item, i);
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		update.aAttr[i] = ItemSystem::GetItemAttribute(item, i);

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
		LOG_ERROR("No Switchbot found for player_id {} slot {}", player_id, slot);
		return;
	}

	if (pkSwitchbot->IsActive(slot))
	{
		LOG_ERROR("Switchbot slot {} already running for player_id {}", slot, player_id);
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
		LOG_ERROR("No Switchbot found for player_id {} slot {}", player_id, slot);
		return;
	}

	if (!pkSwitchbot->IsActive(slot))
	{
		LOG_ERROR("Switchbot slot {} is not running for player_id {}", slot, player_id);
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
		//"No switchbot found to transfer. (pid %d source_port %d target_port %d)", player_id, mother_port, wTargetPort);
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

void CSwitchbotManager::SendItemAttributeInformations(entt::entity ch)
{
	if (!ecs::PlayerRuntime::IsValid(ch))
	{
		return;
	}

	LPDESC desc = ecs::PlayerRuntime::GetDesc(ch);
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

	const entt::entity ch = CHARACTER_MANAGER::Instance().FindEntityByPID(player_id);
	if (ch == entt::null)
	{
		return;
	}

	LPDESC desc = ecs::PlayerRuntime::GetDesc(ch);
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

void CSwitchbotManager::EnterGame(entt::entity ch)
{
	SendItemAttributeInformations(ch);
	SetIsWarping(ecs::PlayerRuntime::GetPlayerID(ch), false);
	SendSwitchbotUpdate(ecs::PlayerRuntime::GetPlayerID(ch));

	CSwitchbot* pkSwitchbot = FindSwitchbot(ecs::PlayerRuntime::GetPlayerID(ch));
	if (pkSwitchbot && pkSwitchbot->HasActiveSlots() && !pkSwitchbot->IsSwitching())
	{
		pkSwitchbot->Start();
	}
}
#endif
