#include "stdafx.h"
#include "constants.h"
#include "log.h"
#include "item.h"
#include "char.h"
#include "desc.h"
#include "item_manager.h"
#ifdef ENABLE_NEWSTUFF
#include "config.h"
#endif

#ifndef ENABLE_SWITCHBOT
const int MAX_NORM_ATTR_NUM = ITEM_MANAGER::MAX_NORM_ATTR_NUM;
const int MAX_RARE_ATTR_NUM = ITEM_MANAGER::MAX_RARE_ATTR_NUM;
#endif


namespace
{
	bool IsZodiacAttributeItemVnum(uint32_t dwVnum)
	{
#ifdef DISABLE_ZODIAC_ATT
		return (dwVnum == 12314141);
#else
		return
			((dwVnum >= 19290) && (dwVnum <= 19312)) ||
			((dwVnum >= 19490) && (dwVnum <= 19512)) ||
			((dwVnum >= 19690) && (dwVnum <= 19712)) ||
			((dwVnum >= 19890) && (dwVnum <= 19912)) ||
			((dwVnum >= 300) && (dwVnum <= 319)) ||
			(dwVnum == 329) ||
			(dwVnum == 339) ||
			(dwVnum == 349) ||
			(dwVnum == 359) ||
			(dwVnum == 369) ||
			(dwVnum == 379) ||
			(dwVnum == 389) ||
			(dwVnum == 399) ||
			((dwVnum >= 1180) && (dwVnum <= 1189)) ||
			(dwVnum == 1199) ||
			(dwVnum == 1209) ||
			(dwVnum == 1219) ||
			(dwVnum == 1229) ||
			((dwVnum >= 2200) && (dwVnum <= 2209)) ||
			(dwVnum == 2219) ||
			(dwVnum == 2229) ||
			(dwVnum == 2239) ||
			(dwVnum == 2249) ||
			((dwVnum >= 3220) && (dwVnum <= 3229)) ||
			(dwVnum == 3239) ||
			(dwVnum == 3249) ||
			(dwVnum == 3259) ||
			(dwVnum == 3269) ||
			((dwVnum >= 5160) && (dwVnum <= 5169)) ||
			(dwVnum == 5179) ||
			(dwVnum == 5189) ||
			(dwVnum == 5199) ||
			(dwVnum == 5209) ||
			((dwVnum >= 7300) && (dwVnum <= 7309)) ||
			(dwVnum == 7319) ||
			(dwVnum == 7329) ||
			(dwVnum == 7339) ||
			(dwVnum == 7349) ||
			((dwVnum >= 1700) && (dwVnum <= 1713)) ||
			((dwVnum >= 1720) && (dwVnum <= 1733)) ||
			((dwVnum >= 1740) && (dwVnum <= 1753)) ||
			((dwVnum >= 1760) && (dwVnum <= 1773)) ||
			((dwVnum >= 1780) && (dwVnum <= 1793)) ||
			((dwVnum >= 1800) && (dwVnum <= 1813)) ||
			((dwVnum >= 8500) && (dwVnum <= 8839));
#endif
	}
}



int CItem::GetAttributeSetIndex()
{
	if (GetType() == ITEM_WEAPON)
	{
		if (GetSubType() == WEAPON_ARROW)
			return -1;

		return ATTRIBUTE_SET_WEAPON;
	}

	if (GetType() == ITEM_ARMOR)
	{
		switch (GetSubType())
		{
			case ARMOR_BODY:
				return ATTRIBUTE_SET_BODY;

			case ARMOR_WRIST:
				return ATTRIBUTE_SET_WRIST;

			case ARMOR_FOOTS:
				return ATTRIBUTE_SET_FOOTS;

			case ARMOR_NECK:
				return ATTRIBUTE_SET_NECK;

			case ARMOR_HEAD:
				return ATTRIBUTE_SET_HEAD;

			case ARMOR_SHIELD:
				return ATTRIBUTE_SET_SHIELD;

			case ARMOR_EAR:
				return ATTRIBUTE_SET_EAR;
		
#if defined(ENABLE_PENDANT) && defined(ENABLE_NEW_BONUS_TALISMAN)
			case ARMOR_PENDANT:
				return ATTRIBUTE_SET_PENDANT;
#endif
		}
	}
#ifdef ENABLE_ATTR_COSTUMES
	else if (GetType() == ITEM_COSTUME)
	{
		switch (GetSubType())
		{
			case COSTUME_BODY:
				return ATTRIBUTE_SET_COSTUME_BODY;
			case COSTUME_HAIR:
				return ATTRIBUTE_SET_COSTUME_HAIR;
			case COSTUME_WEAPON:
				return ATTRIBUTE_SET_COSTUME_WEAPON;
#ifdef ENABLE_STOLE_COSTUME
			case COSTUME_STOLE:
				return ATTRIBUTE_SET_COSTUME_STOLE;
#endif
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
			case COSTUME_MOUNT:
				break;
#endif
		}
	}
#endif

	return -1;
}

bool CItem::HasAttr(uint8_t bApply)
{
	bool ignoreBaseApplies = false;

#ifdef ENABLE_PENDANT
	// Talizm·n / pendant: lehessen ugyanaz a bÛnusz az alap bÛnusz mellett is
	if ((GetType() == ITEM_ARMOR && GetSubType() == ARMOR_NUM_TYPES) || (GetWearFlag() & WEARABLE_PENDANT))
		ignoreBaseApplies = true;
#endif

	if (!ignoreBaseApplies)
	{
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			if (m_pProto->aApplies[i].bType == bApply)
				return true;

#ifdef ENABLE_ITEM_EXTRA_PROTO
		if (HasExtraProto())
		{
#ifdef ENABLE_NEW_EXTRA_BONUS
			for (int i = 0; i < NEW_EXTRA_BONUS_COUNT; ++i)
				if (m_ExtraProto->ExtraBonus[i].bType == bApply)
					return true;
#endif
		}
#endif
	}

 
	for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i)
		if (GetAttributeType(i) == bApply)
			return true;

	return false;
}


bool CItem::HasRareAttr(uint8_t bApply)
{
	for (int i = 0; i < MAX_RARE_ATTR_NUM; ++i)
		if (GetAttributeType(i + 5) == bApply)
			return true;

	return false;
}

void CItem::AddAttribute(uint8_t bApply, short sValue)
{
	int iSameAttrCount = 0;
	for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i)
	{
		if (GetAttributeType(i) == bApply)
			++iSameAttrCount;
	}

	if (IsZodiacAttributeItemVnum(GetVnum()))
	{
		if (iSameAttrCount >= 1)
			return;
	}
	else if (HasAttr(bApply))
		return;

	int i = GetAttributeCount();

	if (i >= MAX_NORM_ATTR_NUM)
		sys_err("item attribute overflow!");
	else
	{
		if (sValue)
			SetAttribute(i, bApply, sValue);
	}
}
#ifdef ENABLE_CHANGE_NORMAL_HIT_RAZOR93
void CItem::AddAttribute2(uint8_t bApply, short sValue)
{
	if (HasAttr(bApply))
		return;

	int i = GetAttributeCount();

	if (i >= MAX_NORM_ATTR_NUM+2)
		sys_err("item attribute overflow!");
	else
	{
		if (sValue)
			SetAttribute2(i, bApply, sValue);
	}
}

void CItem::SetAttribute2(int i, uint8_t bType, short sValue)
{
	assert(i < MAX_NORM_ATTR_NUM+2);

	m_aAttr[i].bType = bType;
	m_aAttr[i].sValue = sValue;
	UpdatePacket();
	Save();

	if (bType)
	{
		const char* pszIP = nullptr;

		if (GetOwner() && GetOwner()->GetDesc())
			pszIP = GetOwner()->GetDesc()->GetHostName();

		LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(i, bType, sValue, GetID(), "SET_ATTR", "", pszIP ? pszIP : "", GetOriginalVnum()));
	}
}

bool CItem::ChangeKKAK(int iAddonType)
{
	(void)iAddonType; // 

	// random  
	int iSkillBonus = MINMAX(-30, int(gauss_random(0, 5) + 0.5f), 30);
	int iNormalHitBonus = 0;
	if (abs(iSkillBonus) <= 20)
		iNormalHitBonus = -2 * iSkillBonus + abs(number(-8, 8) + number(-8, 8)) + number(1, 4);
	else
		iNormalHitBonus = -2 * iSkillBonus + number(1, 5);

	// 71/72  
	//RemoveAttributeType(APPLY_SKILL_DAMAGE_BONUS);
	//RemoveAttributeType(APPLY_NORMAL_HIT_DAMAGE_BONUS);
	AddAttr4(APPLY_NORMAL_HIT_DAMAGE_BONUS, iNormalHitBonus);
	AddAttr4(APPLY_SKILL_DAMAGE_BONUS, iSkillBonus);

	return true;
}
bool CItem::AddRareAttribute3(uint8_t bApply, short sValue)
{
	int count = GetRareAttrCount();

	if (count >= ITEM_ATTRIBUTE_RARE_NUM+2)
		return false;

	int pos = count + ITEM_ATTRIBUTE_RARE_START;
	TPlayerItemAttribute& attr = m_aAttr[pos];

	int nAttrSet = GetAttributeSetIndex();
	std::vector<int> avail;

	for (int i = 0; i < MAX_APPLY_NUM; ++i)
	{
		const TItemAttrTable& r = g_map_itemRare[i];

		if (r.dwApplyIndex != 0 && r.bMaxLevelBySet[nAttrSet] > 0 && HasRareAttr(i) != true)
		{
			avail.push_back(i);
		}
	}
}

void CItem::AddAttr4(uint8_t bApply, uint8_t bLevel)
{
	if (HasAttr(bApply))
		return;

	if (bLevel <= 0)
		return;

	int i = GetAttributeCount();

	if (i < 5)
		return;
	else
	{
		const TItemAttrTable& r = g_map_itemAttr[bApply];
		int32_t lVal = r.lValues[MIN(4, bLevel - 1)];
#ifdef ENABLE_ATTR_COSTUMES
		if (GetType() == ITEM_COSTUME)
			lVal = r.lValues[MIN(9, bLevel + 5 - 1)];
#endif

		if (lVal)
			SetAttribute(i, bApply, lVal);
	}
}
#endif
void CItem::AddAttr(uint8_t bApply, uint8_t bLevel)
{
	int iSameAttrCount = 0;
	for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i)
	{
		if (GetAttributeType(i) == bApply)
			++iSameAttrCount;
	}

	if (IsZodiacAttributeItemVnum(GetVnum()))
	{
		if (iSameAttrCount >= 1)
			return;
	}
	else if (HasAttr(bApply))
		return;

	if (bLevel <= 0)
		return;

	int i = GetAttributeCount();

	if (i == MAX_NORM_ATTR_NUM)
		sys_err("item attribute overflow!");
	else
	{
		const TItemAttrTable & r = g_map_itemAttr[bApply];
		int32_t lVal = r.lValues[MIN(4, bLevel - 1)];
#ifdef ENABLE_ATTR_COSTUMES
		if (GetType() == ITEM_COSTUME)
			lVal = r.lValues[MIN(9, bLevel + 5 - 1)];
#endif
		
		if (lVal)
			SetAttribute(i, bApply, lVal);
	}
}

void CItem::PutAttributeWithLevel(uint8_t bLevel)
{
	int iAttributeSet = GetAttributeSetIndex();
	if (iAttributeSet < 0)
		return;

	if (bLevel > ITEM_ATTRIBUTE_MAX_LEVEL)
		return;

	std::vector<int> avail;

	int total = 0;

	// ∫Ÿ¿œ ºˆ ¿÷¥¬ º”º∫ πËø≠¿ª ±∏√‡
	for (int i = 0; i < MAX_APPLY_NUM; ++i)
	{
		const TItemAttrTable & r = g_map_itemAttr[i];

		if (!r.bMaxLevelBySet[iAttributeSet])
			continue;

		if (IsZodiacAttributeItemVnum(GetVnum()))
		{
			int iSameAttrCount = 0;
			for (int j = 0; j < MAX_NORM_ATTR_NUM; ++j)
			{
				if (GetAttributeType(j) == i)
					++iSameAttrCount;
			}

			if (iSameAttrCount >= 1)
				continue;
		}
		else if (HasAttr(i))
		{
			continue;
		}

		avail.push_back(i);
		total += r.dwProb;
	}

	if (avail.empty())
	{
		return;
	}

	// ±∏√‡µ» πËø≠∑Œ »Æ∑¸ ∞ËªÍ¿ª ≈Î«ÿ ∫Ÿ¿œ º”º∫ º±¡§
	unsigned int prob = number(1, total);
	int attr_idx = APPLY_NONE;

	for (uint32_t i = 0; i < avail.size(); ++i)
	{
		const TItemAttrTable & r = g_map_itemAttr[avail[i]];

		if (prob <= r.dwProb)
		{
			attr_idx = avail[i];
			break;
		}

		prob -= r.dwProb;
	}

	if (!attr_idx)
	{
		sys_err("Cannot put item attribute %d %d", iAttributeSet, bLevel);
		return;
	}

	const TItemAttrTable & r = g_map_itemAttr[attr_idx];

	// ¡æ∑˘∫∞ º”º∫ ∑π∫ß √÷¥Î∞™ ¡¶«—
	if (bLevel > r.bMaxLevelBySet[iAttributeSet])
		bLevel = r.bMaxLevelBySet[iAttributeSet];

	AddAttr(attr_idx, bLevel);
}

void CItem::PutAttribute(const int * aiAttrPercentTable)
{
	int iAttrLevelPercent = number(1, 100);
	int i;

	for (i = 0; i < ITEM_ATTRIBUTE_MAX_LEVEL; ++i)
	{
		if (iAttrLevelPercent <= aiAttrPercentTable[i])
			break;

		iAttrLevelPercent -= aiAttrPercentTable[i];
	}

	PutAttributeWithLevel(i + 1);
}



void CItem::ChangeAttribute(const int* aiChangeProb)
{
	int iAttributeCount = GetAttributeCount();

	ClearAttribute();

	if (iAttributeCount == 0)
		return;

	TItemTable const * pProto = GetProto();

	if (pProto && pProto->sAddonType)
	{
		ApplyAddon(pProto->sAddonType);
	}

	static const int tmpChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] =
	{
		0, 10, 40, 35, 15,
	};

	for (int i = GetAttributeCount(); i < iAttributeCount; ++i)
	{
#ifdef ATTR_LOCK		
		if (GetLockedAttr() == i)
		{
			continue;
		}
#endif
		if (aiChangeProb == nullptr)
		{
			PutAttribute(tmpChangeProb);
		}
		else
		{
			PutAttribute(aiChangeProb);
		}
	}
}

void CItem::AddAttribute()
{
	static const int aiItemAddAttributePercent[ITEM_ATTRIBUTE_MAX_LEVEL] =
	{
		40, 50, 10, 0, 0
	};

	if (GetAttributeCount() < MAX_NORM_ATTR_NUM)
		PutAttribute(aiItemAddAttributePercent);
}

void CItem::ClearAttribute()
{
	for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i)
	{
#ifdef ATTR_LOCK		
		if (GetLockedAttr() == i)
		{
			continue;
		}
#endif
		m_aAttr[i].bType = 0;
		m_aAttr[i].sValue = 0;
	}
}

int CItem::GetAttributeCount()
{
	int i;

	for (i = 0; i < MAX_NORM_ATTR_NUM; ++i)
	{
		if (GetAttributeType(i) == 0)
			break;
	}

	return i;
}

int CItem::FindAttribute(uint8_t bType)
{
	for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i)
	{
		if (GetAttributeType(i) == bType)
			return i;
	}

	return -1;
}

bool CItem::RemoveAttributeAt(int index)
{
	if (GetAttributeCount() <= index)
		return false;

	for (int i = index; i < MAX_NORM_ATTR_NUM - 1; ++i)
	{
		SetAttribute(i, GetAttributeType(i + 1), GetAttributeValue(i + 1));
	}

	SetAttribute(MAX_NORM_ATTR_NUM - 1, APPLY_NONE, 0);
	return true;
}

bool CItem::RemoveAttributeType(uint8_t bType)
{
	int index = FindAttribute(bType);
	return index != -1 && RemoveAttributeType(index);
}

void CItem::SetAttributes(const TPlayerItemAttribute* c_pAttribute)
{
	memcpy(m_aAttr, c_pAttribute, sizeof(m_aAttr));
	Save();
}

void CItem::SetAttribute(int i, uint8_t bType, short sValue)
{
	assert(i < MAX_NORM_ATTR_NUM);

	m_aAttr[i].bType = bType;
	m_aAttr[i].sValue = sValue;
	UpdatePacket();
	Save();

	if (bType)
	{
		const char * pszIP = nullptr;

		if (GetOwner() && GetOwner()->GetDesc())
			pszIP = GetOwner()->GetDesc()->GetHostName();

		LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(i, bType, sValue, GetID(), "SET_ATTR", "", pszIP ? pszIP : "", GetOriginalVnum()));
	}
}

void CItem::SetForceAttribute(int i, uint8_t bType, short sValue)
{
	assert(i < ITEM_ATTRIBUTE_MAX_NUM);

	m_aAttr[i].bType = bType;
	m_aAttr[i].sValue = sValue;
	UpdatePacket();
	Save();

	if (bType)
	{
		const char * pszIP = nullptr;

		if (GetOwner() && GetOwner()->GetDesc())
			pszIP = GetOwner()->GetDesc()->GetHostName();

		LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(i, bType, sValue, GetID(), "SET_FORCE_ATTR", "", pszIP ? pszIP : "", GetOriginalVnum()));
	}
}


void CItem::CopyAttributeTo(LPITEM pItem)
{
	pItem->SetAttributes(m_aAttr);
}

int CItem::GetRareAttrCount()
{
	int ret = 0;

	for (uint32_t dwIdx = ITEM_ATTRIBUTE_RARE_START; dwIdx < ITEM_ATTRIBUTE_RARE_END; dwIdx++)
	{
		if (m_aAttr[dwIdx].bType != 0)
			ret++;
	}

	return ret;
}

bool CItem::ChangeRareAttribute()
{
	if (GetRareAttrCount() == 0)
		return false;

	int cnt = GetRareAttrCount();

	for (int i = 0; i < cnt; ++i)
	{
		m_aAttr[i + ITEM_ATTRIBUTE_RARE_START].bType = 0;
		m_aAttr[i + ITEM_ATTRIBUTE_RARE_START].sValue = 0;
	}

	if (GetOwner() && GetOwner()->GetDesc())
		LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(GetOwner(), this, "SET_RARE_CHANGE", ""))
	else
		LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(0, 0, 0, GetID(), "SET_RARE_CHANGE", "", "", GetOriginalVnum()))

	for (int i = 0; i < cnt; ++i)
	{
		AddRareAttribute();
	}

	return true;
}

bool CItem::AddRareAttribute()
{
	int count = GetRareAttrCount();

	if (count >= ITEM_ATTRIBUTE_RARE_NUM)
		return false;

	int pos = count + ITEM_ATTRIBUTE_RARE_START;
	TPlayerItemAttribute & attr = m_aAttr[pos];

	int nAttrSet = GetAttributeSetIndex();
	std::vector<int> avail;

	for (int i = 0; i < MAX_APPLY_NUM; ++i)
	{
		const TItemAttrTable & r = g_map_itemRare[i];

		if (r.dwApplyIndex != 0 && r.bMaxLevelBySet[nAttrSet] > 0 && HasRareAttr(i) != true)
		{
			avail.push_back(i);
		}
	}

	if (avail.empty())
	{
		sys_err("Couldn't add a rare bonus - item_attr_rare has incorrect values!");
		return false;
	}

	const TItemAttrTable& r = g_map_itemRare[avail[number(0, avail.size() - 1)]];
	int nAttrLevel = 5;

	if (nAttrLevel > r.bMaxLevelBySet[nAttrSet])
		nAttrLevel = r.bMaxLevelBySet[nAttrSet];

	attr.bType = r.dwApplyIndex;
	attr.sValue = r.lValues[nAttrLevel - 1];

	UpdatePacket();

	Save();

	const char * pszIP = nullptr;

	if (GetOwner() && GetOwner()->GetDesc())
		pszIP = GetOwner()->GetDesc()->GetHostName();

	LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(pos, attr.bType, attr.sValue, GetID(), "SET_RARE", "", pszIP ? pszIP : "", GetOriginalVnum()));
	return true;
}

void CItem::AddRareAttribute2(const int * aiAttrPercentTable)
{
	static const int aiItemAddAttributePercent[ITEM_ATTRIBUTE_MAX_LEVEL] =
	{
		40, 50, 10, 0, 0
	};
	if (aiAttrPercentTable == nullptr)
		aiAttrPercentTable = aiItemAddAttributePercent;

	if (GetRareAttrCount() < MAX_RARE_ATTR_NUM)
		PutRareAttribute(aiAttrPercentTable);
}

void CItem::PutRareAttribute(const int * aiAttrPercentTable)
{
	int iAttrLevelPercent = number(1, 100);
	int i;

	for (i = 0; i < ITEM_ATTRIBUTE_MAX_LEVEL; ++i)
	{
		if (iAttrLevelPercent <= aiAttrPercentTable[i])
			break;

		iAttrLevelPercent -= aiAttrPercentTable[i];
	}

	PutRareAttributeWithLevel(i + 1);
}

void CItem::PutRareAttributeWithLevel(uint8_t bLevel)
{
	int iAttributeSet = GetAttributeSetIndex();
	if (iAttributeSet < 0)
		return;

	if (bLevel > ITEM_ATTRIBUTE_MAX_LEVEL)
		return;

	std::vector<int> avail;

	int total = 0;

	// ∫Ÿ¿œ ºˆ ¿÷¥¬ º”º∫ πËø≠¿ª ±∏√‡
	for (int i = 0; i < MAX_APPLY_NUM; ++i)
	{
		const TItemAttrTable & r = g_map_itemRare[i];

		if (r.bMaxLevelBySet[iAttributeSet] && !HasRareAttr(i))
		{
			avail.push_back(i);
			total += r.dwProb;
		}
	}

	if (avail.empty())
	{
		return;
	}

	// ±∏√‡µ» πËø≠∑Œ »Æ∑¸ ∞ËªÍ¿ª ≈Î«ÿ ∫Ÿ¿œ º”º∫ º±¡§
	unsigned int prob = number(1, total);
	int attr_idx = APPLY_NONE;

	for (uint32_t i = 0; i < avail.size(); ++i)
	{
		const TItemAttrTable & r = g_map_itemRare[avail[i]];

		if (prob <= r.dwProb)
		{
			attr_idx = avail[i];
			break;
		}

		prob -= r.dwProb;
	}

	if (!attr_idx)
	{
		sys_err("Cannot put item rare attribute %d %d", iAttributeSet, bLevel);
		return;
	}

	const TItemAttrTable & r = g_map_itemRare[attr_idx];

	// ¡æ∑˘∫∞ º”º∫ ∑π∫ß √÷¥Î∞™ ¡¶«—
	if (bLevel > r.bMaxLevelBySet[iAttributeSet])
		bLevel = r.bMaxLevelBySet[iAttributeSet];

	AddRareAttr(attr_idx, bLevel);
}

void CItem::AddRareAttr(uint8_t bApply, uint8_t bLevel)
{
	if (HasRareAttr(bApply))
		return;

	if (bLevel <= 0)
		return;

	int i = ITEM_ATTRIBUTE_RARE_START + GetRareAttrCount();

	if (i == ITEM_ATTRIBUTE_RARE_END)
		sys_err("item rare attribute overflow!");
	else
	{
		const TItemAttrTable & r = g_map_itemRare[bApply];
		int32_t lVal = r.lValues[MIN(4, bLevel - 1)];

		if (lVal)
			SetForceAttribute(i, bApply, lVal);
	}
}

