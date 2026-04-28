#include "stdafx.h"
#include "constants.h"
#include "item.h"
#include "item_manager.h"
#include "unique_item.h"
#include "packet.h"
#include "desc.h"
#include "char_interface.hpp"
#include "dragon_soul_table.h"
#include "log.h"
#include "DragonSoul.h"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/systems/ItemSystem.hpp"
//#include <boost/lexical_cast.hpp>

template <typename T> T MINMAX(T min, T value, T max)
{
	T tv;

	tv = (min > value ? min : value);
	return (max < tv) ? max : tv;
}

typedef std::vector <std::string> TTokenVector;


namespace {

LPITEM LegacyDragonSoulItemOf(entt::entity item)
{
	const uint32_t id = ItemSystem::GetItemID(item);
	return id != 0 ? ITEM_MANAGER::instance().Find(id) : nullptr;
}

void SyncDragonSoulItemEntity(entt::entity item)
{
	if (item != entt::null)
		ItemSystem::SyncItemStateFromLegacy(item);
}

void SyncDragonSoulItemPtr(LPITEM item)
{
	SyncDragonSoulItemEntity(EntityFactory::CreateItemEntity(g_registry, item));
}

void SyncDragonSoulGridItems(LPCHARACTER ch, const TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (!ch)
		return;

	for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; ++i)
	{
		LPITEM item = ch->GetItem(aItemPoses[i]);
		if (item)
			SyncDragonSoulItemPtr(item);
	}
}

} // namespace

int Gamble(std::vector<float>& vec_probs)
{
	float range = 0.f;
	for (size_t i = 0; i < vec_probs.size(); i++)
	{
		range += vec_probs[i];
	}
	float fProb = fnumber(0.f, range);
	float sum = 0.f;
	for (size_t idx = 0; idx < vec_probs.size(); idx++)
	{
		sum += vec_probs[idx];
		if (sum >= fProb)
			return idx;
	}
	return -1;
}

// 가중치 테이블(prob_lst)을 받아 random_set.size()개의 index를 선택하여 random_set을 return
bool MakeDistinctRandomNumberSet(std::list <float> prob_lst, OUT std::vector<int>& random_set)
{
	int size = prob_lst.size();
	int n = random_set.size();
	if (size < n)
		return false;

	std::vector <int> select_bit(size, 0);
	for (int i = 0; i < n; i++)
	{
		float range = 0.f;
		for (std::list <float>::iterator it = prob_lst.begin(); it != prob_lst.end(); it++)
		{
			range += *it;
		}
		float r = fnumber (0.f, range);
		float sum = 0.f;
		int idx = 0;
		for (std::list <float>::iterator it = prob_lst.begin(); it != prob_lst.end(); it++)
		{
			while (select_bit[idx++]);

			sum += *it;
			if (sum >= r)
			{
				select_bit[idx - 1] = 1;
				random_set[i] = idx - 1;
				prob_lst.erase(it);
				break;
			}
		}
	}
	return true;
}

/* 용혼석 Vnum에 대한 comment
 * ITEM VNUM을 10만 자리부터, FEDCBA라고 한다면
 * FE : 용혼석 종류.	D : 등급
 * C : 단계			B : 강화
 * A : 여벌의 번호들...
 */

uint8_t GetType(uint32_t dwVnum)
{
	return (dwVnum / 10000);
}

uint8_t GetGradeIdx(uint32_t dwVnum)
{
	return (dwVnum / 1000) % 10;
}

uint8_t GetStepIdx(uint32_t dwVnum)
{
	return (dwVnum / 100) % 10;
}

uint8_t GetStrengthIdx(uint32_t dwVnum)
{
	return (dwVnum / 10) % 10;
}

bool DSManager::ReadDragonSoulTableFile(const char * c_pszFileName)
{
	m_pTable = new DragonSoulTable();
	return m_pTable->ReadDragonSoulTableFile(c_pszFileName);
}

void DSManager::GetDragonSoulInfo(uint32_t dwVnum, uint8_t& bType, uint8_t& bGrade, uint8_t& bStep, uint8_t& bStrength) const
{
	bType = GetType(dwVnum);
	bGrade = GetGradeIdx(dwVnum);
	bStep = GetStepIdx(dwVnum);
	bStrength = GetStrengthIdx(dwVnum);
}

bool DSManager::IsValidCellForThisItem(const LPITEM pItem, const TItemPos& Cell) const
{
	return IsValidCellForThisItem(EntityFactory::CreateItemEntity(g_registry, pItem), Cell);
}

bool DSManager::IsValidCellForThisItem(entt::entity item, const TItemPos& Cell) const
{
	if (!ItemSystem::IsValidItem(item))
		return false;

	uint16_t wBaseCell = GetBasePosition(item);
	if (WORD_MAX == wBaseCell)
		return false;

	if (Cell.window_type != DRAGON_SOUL_INVENTORY
		|| (Cell.cell < wBaseCell || Cell.cell >= wBaseCell + DRAGON_SOUL_BOX_SIZE))
	{
		return false;
	}
	else
		return true;
}


uint16_t DSManager::GetBasePosition(const LPITEM pItem) const
{
	return GetBasePosition(EntityFactory::CreateItemEntity(g_registry, pItem));
}

uint16_t DSManager::GetBasePosition(entt::entity item) const
{
	if (!ItemSystem::IsValidItem(item))
		return WORD_MAX;

	uint8_t type, grade_idx, step_idx, strength_idx;
	GetDragonSoulInfo(ItemSystem::GetItemVnum(item), type, grade_idx, step_idx, strength_idx);

	uint8_t col_type = ItemSystem::GetItemSubType(item);
	uint8_t row_type = grade_idx;
	if (row_type > DRAGON_SOUL_GRADE_MAX)
		return WORD_MAX;

#ifdef ENABLE_DS_GRADE_MYTH
	return 300 + (col_type * DRAGON_SOUL_GRADE_MAX * DRAGON_SOUL_BOX_SIZE + row_type * DRAGON_SOUL_BOX_SIZE);
#else
	return 300 + (col_type * DRAGON_SOUL_STEP_MAX * DRAGON_SOUL_BOX_SIZE + row_type * DRAGON_SOUL_BOX_SIZE);
#endif
}


bool DSManager::RefreshItemAttributes(LPITEM pDS)
{
	if (!pDS->IsDragonSoul())
	{
		sys_err ("This item(ID : %d) is not DragonSoul.", pDS->GetID());
		return false;
	}

	uint8_t ds_type, grade_idx, step_idx, strength_idx;
	GetDragonSoulInfo(pDS->GetVnum(), ds_type, grade_idx, step_idx, strength_idx);

	DragonSoulTable::TVecApplys vec_basic_applys;
	DragonSoulTable::TVecApplys vec_addtional_applys;

	if (!m_pTable->GetBasicApplys(ds_type, vec_basic_applys))
	{
		sys_err ("There is no BasicApply about %d type dragon soul.", ds_type);
		return false;
	}

	if (!m_pTable->GetAdditionalApplys(ds_type, vec_addtional_applys))
	{
		sys_err ("There is no AdditionalApply about %d type dragon soul.", ds_type);
		return false;
	}

	// add_min과 add_max는 더미로 읽음.
	int basic_apply_num, add_min, add_max;
	if (!m_pTable->GetApplyNumSettings(ds_type, grade_idx, basic_apply_num, add_min, add_max))
	{
		sys_err ("In ApplyNumSettings, INVALID VALUES Group type(%d), GRADE idx(%d)", ds_type, grade_idx);
		return false;
	}

	float fWeight = 0.f;
	if (!m_pTable->GetWeight(ds_type, grade_idx, step_idx, strength_idx, fWeight))
	{
		return false;
	}
	fWeight /= 100.f;

	int n = MIN(basic_apply_num, vec_basic_applys.size());
	for (int i = 0; i < n; i++)
	{
		const SApply& basic_apply = vec_basic_applys[i];
		uint8_t bType = basic_apply.apply_type;
		short sValue = (short)(ceil((float)basic_apply.apply_value * fWeight - 0.01f));

		pDS->SetForceAttribute(i, bType, sValue);
	}

	for (int i = DRAGON_SOUL_ADDITIONAL_ATTR_START_IDX; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
	{
		uint8_t bType = pDS->GetAttributeType(i);
		short sValue = 0;
		if (APPLY_NONE == bType)
			continue;
		for (size_t j = 0; j < vec_addtional_applys.size(); j++)
		{
			if (vec_addtional_applys[j].apply_type == bType)
			{
				sValue = vec_addtional_applys[j].apply_value;
				break;
			}
		}
		pDS->SetForceAttribute(i, bType, (short)(ceil((float)sValue * fWeight - 0.01f)));
	}
	return true;
}

bool DSManager::PutAttributes(LPITEM pDS)
{
	if (!pDS->IsDragonSoul())
	{
		sys_err ("This item(ID : %d) is not DragonSoul.", pDS->GetID());
		return false;
	}

	uint8_t ds_type, grade_idx, step_idx, strength_idx;
	GetDragonSoulInfo(pDS->GetVnum(), ds_type, grade_idx, step_idx, strength_idx);

	DragonSoulTable::TVecApplys vec_basic_applys;
	DragonSoulTable::TVecApplys vec_addtional_applys;

	if (!m_pTable->GetBasicApplys(ds_type, vec_basic_applys))
	{
		sys_err ("There is no BasicApply about %d type dragon soul.", ds_type);
		return false;
	}
	if (!m_pTable->GetAdditionalApplys(ds_type, vec_addtional_applys))
	{
		sys_err ("There is no AdditionalApply about %d type dragon soul.", ds_type);
		return false;
	}


	int basic_apply_num, add_min, add_max;
	if (!m_pTable->GetApplyNumSettings(ds_type, grade_idx, basic_apply_num, add_min, add_max))
	{
		sys_err ("In ApplyNumSettings, INVALID VALUES Group type(%d), GRADE idx(%d)", ds_type, grade_idx);
		return false;
	}

	float fWeight = 0.f;
	if (!m_pTable->GetWeight(ds_type, grade_idx, step_idx, strength_idx, fWeight))
	{
		return false;
	}
	fWeight /= 100.f;

	int n = MIN(basic_apply_num, vec_basic_applys.size());
	for (int i = 0; i < n; i++)
	{
		const SApply& basic_apply = vec_basic_applys[i];
		uint8_t bType = basic_apply.apply_type;
		short sValue = (short)(ceil((float)basic_apply.apply_value * fWeight - 0.01f));

		pDS->SetForceAttribute(i, bType, sValue);
	}

	uint8_t additional_attr_num = MIN(number (add_min, add_max), 4);

	std::vector <int> random_set;
	if (additional_attr_num > 0)
	{
		random_set.resize(additional_attr_num);
		std::list <float> list_probs;
		for (size_t i = 0; i < vec_addtional_applys.size(); i++)
		{
			list_probs.push_back(vec_addtional_applys[i].prob);
		}
		if (!MakeDistinctRandomNumberSet(list_probs, random_set))
		{
			sys_err ("MakeDistinctRandomNumberSet error.");
			return false;
		}

		for (int i = 0; i < additional_attr_num; i++)
		{
			int r = random_set[i];
			const SApply& additional_attr = vec_addtional_applys[r];
			uint8_t bType = additional_attr.apply_type;
			short sValue = (short)(ceil((float)additional_attr.apply_value * fWeight - 0.01f));

			pDS->SetForceAttribute(DRAGON_SOUL_ADDITIONAL_ATTR_START_IDX + i, bType, sValue);
		}
	}

	return true;
}

bool DSManager::DragonSoulItemInitialize(LPITEM pItem)
{
	if (nullptr == pItem || !pItem->IsDragonSoul())
		return false;
	PutAttributes(pItem);
	int time = DSManager::instance().GetDuration(pItem);
	if (time > 0)
		pItem->SetSocket(ITEM_SOCKET_REMAIN_SEC, time);
	return true;
}

uint32_t DSManager::MakeDragonSoulVnum(uint8_t bType, uint8_t grade, uint8_t step, uint8_t refine)
{
	return bType * 10000 + grade * 1000 + step * 100 + refine * 10;
}

int DSManager::GetDuration(const LPITEM pItem) const
{
	return pItem->GetDuration();
}

// 용혼석을 받아서 용심을 추출하는 함수
bool DSManager::ExtractDragonHeart(LPCHARACTER ch, LPITEM pItem, LPITEM pExtractor)
{
	if (nullptr == ch || nullptr == pItem)
		return false;
	if (pItem->IsEquipped())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 623, "");
#endif
		return false;
	}

	uint32_t dwVnum = pItem->GetVnum();
	uint8_t ds_type, grade_idx, step_idx, strength_idx;
	GetDragonSoulInfo(dwVnum, ds_type, grade_idx, step_idx, strength_idx);

	int iBonus = 0;

	if (nullptr != pExtractor)
	{
		iBonus = pExtractor->GetValue(0);
	}

	std::vector <float> vec_chargings;
	std::vector <float> vec_probs;

	if (!m_pTable->GetDragonHeartExtValues(ds_type, grade_idx, vec_chargings, vec_probs))
	{
		return false;
	}

	int idx = Gamble(vec_probs);

	//float sum = 0.f;
	if (-1 == idx)
	{
		sys_err ("Gamble is failed. ds_type(%d), grade_idx(%d)", ds_type, grade_idx);
		return false;
	}

	float fCharge = vec_chargings[idx] * (100 + iBonus) / 100.f;
#ifdef ENABLE_DS_EDITS
	fCharge = float(iBonus);
#else
	fCharge = MINMAX <float> (0.f, fCharge, 100.f);
#endif
	if (fCharge < FLT_EPSILON)
	{
		ItemSystem::ConsumeItemEcs(
			EntityFactory::CreateItemEntity(g_registry, pItem), 1);
		if (nullptr != pExtractor)
		{
			ItemSystem::ConsumeItemEcs(
				EntityFactory::CreateItemEntity(g_registry, pExtractor), 1);
		}
		LogManager::instance().ItemLog(ch, pItem, "DS_HEART_EXTRACT_FAIL", "");
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 624, "");
#endif
		return false;
	}
	else
	{
		LPITEM pDH = ITEM_MANAGER::instance().CreateItem(DRAGON_HEART_VNUM);

		if (nullptr == pDH)
		{
			sys_err ("Cannot create DRAGON_HEART(%d).", DRAGON_HEART_VNUM);
			return false;
		}

		ItemSystem::ConsumeItemEcs(
			EntityFactory::CreateItemEntity(g_registry, pItem), 1);
		if (nullptr != pExtractor)
		{
			ItemSystem::ConsumeItemEcs(
				EntityFactory::CreateItemEntity(g_registry, pExtractor), 1);
		}

		int iCharge = (int)(fCharge + 0.5f);
		pDH->SetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX, iCharge);
		ch->AutoGiveItem(pDH, true);

		auto s = std::to_string(iCharge);
		s += "%s";
		LogManager::instance().ItemLog(ch, pItem, "DS_HEART_EXTRACT_SUCCESS", s.c_str());
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 624, "");
#endif
		return true;
	}
}

bool DSManager::ExtractDragonHeartEcs(entt::entity owner, entt::entity item, entt::entity extractor)
{
	LPCHARACTER ch = ecs::LegacyCharOf(owner);
	LPITEM legacyItem = LegacyDragonSoulItemOf(item);
	LPITEM legacyExtractor = extractor != entt::null ? LegacyDragonSoulItemOf(extractor) : nullptr;
	if (!ch || !legacyItem)
		return false;

	const bool result = ExtractDragonHeart(ch, legacyItem, legacyExtractor);
	SyncDragonSoulItemEntity(item);
	SyncDragonSoulItemEntity(extractor);
	return result;
}


// 특정 용혼석을 장비창에서 제거할 때에 성공 여부를 결정하고, 실패시 부산물을 주는 함수.
bool DSManager::PullOut(LPCHARACTER ch, TItemPos DestCell, LPITEM& pItem, LPITEM pExtractor)
{
	if (nullptr == ch || nullptr == pItem)
	{
		sys_err ("NULL POINTER. ch(%p) or pItem(%p)", ch, pItem);
		return false;
	}

	// 목표 위치가 valid한지 검사 후, valid하지 않다면 임의의 빈 공간을 찾는다.
	if (!IsValidCellForThisItem(pItem, DestCell))
	{
		int iEmptyCell = ch->GetEmptyDragonSoulInventory(pItem);
		if (iEmptyCell < 0)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 626, "");
#endif
			return false;
		}
		else
		{
			DestCell.window_type = DRAGON_SOUL_INVENTORY;
			DestCell.cell = iEmptyCell;
		}
	}

	if (!pItem->IsEquipped() || !pItem->RemoveFromCharacter())
		return false;

	bool bSuccess;
	uint32_t dwByProduct = 0;
	int iBonus = 0;
	float fProb;
	float fDice;
	// 용혼석 추출 성공 여부 결정.
	{
		//uint32_t dwVnum = pItem->GetVnum();

		uint8_t ds_type, grade_idx, step_idx, strength_idx;
		GetDragonSoulInfo(pItem->GetVnum(), ds_type, grade_idx, step_idx, strength_idx);

		// 추출 정보가 없다면 일단 무조건 성공하는 것이라 생각하자.
		if (!m_pTable->GetDragonSoulExtValues(ds_type, grade_idx, fProb, dwByProduct))
		{
			pItem->AddToCharacter(ch, DestCell);
			return true;
		}
		
		fDice = fnumber(0.f, 100.f);
		bSuccess = fDice <= (fProb * (100 + iBonus) / 100.f);
		if (nullptr != pExtractor)
		{
			iBonus = pExtractor->GetValue(ITEM_VALUE_DRAGON_SOUL_POLL_OUT_BONUS_IDX);
			ItemSystem::ConsumeItemEcs(
				EntityFactory::CreateItemEntity(g_registry, pExtractor), 1);
			bSuccess = number(1, 100) <= iBonus ? true : false;
		}
	}

	// 캐릭터의 용혼석 추출 및 추가 혹은 제거. 부산물 제공.
	{
		char buf[128];

		if (bSuccess)
		{
			if (pExtractor)
			{
				sprintf(buf, "dice(%d) prob(%d + %d) EXTR(VN:%d)", (int)fDice, (int)fProb, iBonus, pExtractor->GetVnum());
			}
			else
			{
				sprintf(buf, "dice(%d) prob(%d)", (int)fDice, (int)fProb);
			}
			
			LogManager::instance().ItemLog(ch, pItem, "DS_PULL_OUT_SUCCESS", buf);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 534, "%s", pItem->GetName(ch->GetDesc() ? ch->GetDesc()->GetLanguage() : 0));
#endif
			pItem->AddToCharacter(ch, DestCell);
			return true;
		}
		else
		{
			if (pExtractor)
			{
				sprintf(buf, "dice(%d) prob(%d + %d) EXTR(VN:%d) ByProd(VN:%d)", (int)fDice, (int)fProb, iBonus, pExtractor->GetVnum(), dwByProduct);
			}
			else
			{
				sprintf(buf, "dice(%d) prob(%d) ByProd(VNUM:%d)", (int)fDice, (int)fProb, dwByProduct);
			}
			
			LogManager::instance().ItemLog(ch, pItem, "DS_PULL_OUT_FAILED", buf);
			ItemSystem::DestroyItemEntityEcs(
				EntityFactory::CreateItemEntity(g_registry, pItem),
				"DRAGON_SOUL_BYPRODUCT");
			pItem = nullptr;
			if (dwByProduct)
			{
				LPITEM pByProduct = ch->AutoGiveItem(dwByProduct);
#ifdef TEXTS_IMPROVEMENT
				if (pByProduct) {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 535, "%s", pByProduct->GetName(ch->GetDesc() ? ch->GetDesc()->GetLanguage() : 0));
				} else {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 536, "");
				}
#endif
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 537, "");
			}
#endif
		}
	}

	return bSuccess;
}

bool DSManager::PullOutEcs(entt::entity owner, TItemPos DestCell, entt::entity& item, entt::entity extractor)
{
	LPCHARACTER ch = ecs::LegacyCharOf(owner);
	LPITEM legacyItem = LegacyDragonSoulItemOf(item);
	LPITEM legacyExtractor = extractor != entt::null ? LegacyDragonSoulItemOf(extractor) : nullptr;
	if (!ch || !legacyItem)
		return false;

	const bool result = PullOut(ch, DestCell, legacyItem, legacyExtractor);
	item = EntityFactory::CreateItemEntity(g_registry, legacyItem);
	SyncDragonSoulItemEntity(item);
	SyncDragonSoulItemEntity(extractor);
	return result;
}


bool DSManager::DoRefineGrade(LPCHARACTER ch, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (nullptr == ch)
		return false;

	if (nullptr == aItemPoses)
	{
		return false;
	}

	if (!ch->DragonSoul_RefineWindow_CanRefine()) {
		return false;
	}

	// 혹시나 모를 중복되는 item pointer 없애기 위해서 set 사용
	// 이상한 패킷을 보낼 경우, 중복된 TItemPos가 있을 수도 있고, 잘못된 TItemPos가 있을 수도 있다.
	std::set <LPITEM> set_items;
	for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; i++)
	{
		if (aItemPoses[i].IsEquipPosition())
			return false;
		LPITEM pItem = ch->GetItem(aItemPoses[i]);
		if (nullptr != pItem)
		{
			// 용혼석이 아닌 아이템이 개량창에 있을 수 없다.
			if (!pItem->IsDragonSoul())
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 628, "");
#endif
				SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, TItemPos(pItem->GetWindow(), pItem->GetCell()));
				return false;
			}

			set_items.insert(pItem);
		}
	}

	if (set_items.size() == 0)
	{
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);
		return false;
	}

	int count = set_items.size();
	int need_count = 0;
	int fee = 0;
	std::vector <float> vec_probs;
	//float prob_sum;

	uint8_t ds_type, grade_idx, step_idx, strength_idx;
	int result_grade;

	// 가장 처음 것을 강화의 기준으로 삼는다.
	std::set <LPITEM>::iterator it = set_items.begin();
	{
		LPITEM pItem = *it;

		GetDragonSoulInfo(pItem->GetVnum(), ds_type, grade_idx, step_idx, strength_idx);

		if (!m_pTable->GetRefineGradeValues(ds_type, grade_idx, need_count, fee, vec_probs))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 627, "");
#endif
			SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, TItemPos(pItem->GetWindow(), pItem->GetCell()));

			return false;
		}
	}
	while (++it != set_items.end())
	{
		LPITEM pItem = *it;

		// 클라 ui에서 장착한 아이템은 개량창에 올릴 수 없도록 막았기 때문에,
		// 별도의 알림 처리는 안함.
		if (pItem->IsEquipped())
		{
			return false;
		}

		if (ds_type != GetType(pItem->GetVnum()) || grade_idx != GetGradeIdx(pItem->GetVnum()))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 628, "");
#endif
			SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, TItemPos(pItem->GetWindow(), pItem->GetCell()));

			return false;
		}
	}

	// 클라에서 한번 갯수 체크를 하기 때문에 count != need_count라면 invalid 클라일 가능성이 크다.
	if (count != need_count)
	{
		sys_err ("Possiblity of invalid client. Name %s", ch->GetName());
		uint8_t bSubHeader = count < need_count? DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL : DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL;
		SendRefineResultPacket(ch, bSubHeader, NPOS);
		return false;
	}

	if (ch->GetGold() < fee)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 232, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
		return false;
	}

	if (-1 == (result_grade = Gamble(vec_probs)))
	{
		sys_err ("Gamble failed. See RefineGardeTables' probabilities");
		return false;
	}

	LPITEM pResultItem = ITEM_MANAGER::instance().CreateItem(MakeDragonSoulVnum(ds_type, (uint8_t)result_grade, 0, 0));

	if (nullptr == pResultItem)
	{
		sys_err ("INVALID DRAGON SOUL(%d)", MakeDragonSoulVnum(ds_type, (uint8_t)result_grade, 0, 0));
		return false;
	}

	ch->PointChange(POINT_GOLD, -fee);
	int left_count = need_count;

	for (std::set <LPITEM>::iterator it = set_items.begin(); it != set_items.end(); it++)
	{
		LPITEM pItem = *it;
		int n = pItem->GetCount();
		if (left_count > n)
		{
			LPITEM removed = pItem->RemoveFromCharacter();
			ItemSystem::DestroyItemEntityEcs(
				EntityFactory::CreateItemEntity(g_registry, removed),
				"DRAGON_SOUL_REFINE_CONSUME");
			left_count -= n;
		}
		else
		{
			ItemSystem::ConsumeItemEcs(
				EntityFactory::CreateItemEntity(g_registry, pItem),
				left_count);
		}
	}

	ch->AutoGiveItem(pResultItem, true);

	if (result_grade > grade_idx)
	{
		char buf[128];
		sprintf(buf, "GRADE : %d -> %d", grade_idx, result_grade);
		LogManager::instance().ItemLog(ch, pResultItem, "DS_GRADE_REFINE_SUCCESS", buf);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 629, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_SUCCEED, TItemPos (pResultItem->GetWindow(), pResultItem->GetCell()));
		return true;
	}
	else
	{
		char buf[128];
		sprintf(buf, "GRADE : %d -> %d", grade_idx, result_grade);
		LogManager::instance().ItemLog(ch, pResultItem, "DS_GRADE_REFINE_FAIL", buf);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 630, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL, TItemPos (pResultItem->GetWindow(), pResultItem->GetCell()));
		return false;
	}
}

bool DSManager::DoRefineGradeEcs(entt::entity owner, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	LPCHARACTER ch = ecs::LegacyCharOf(owner);
	if (!ch)
		return false;

	const bool result = DoRefineGrade(ch, aItemPoses);
	SyncDragonSoulGridItems(ch, aItemPoses);
	return result;
}


bool DSManager::DoRefineStep(LPCHARACTER ch, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (nullptr == ch)
		return false;
	if (nullptr == aItemPoses)
	{
		return false;
	}

	if (!ch->DragonSoul_RefineWindow_CanRefine()) {
		return false;
	}

	// 혹시나 모를 중복되는 item pointer 없애기 위해서 set 사용
	// 이상한 패킷을 보낼 경우, 중복된 TItemPos가 있을 수도 있고, 잘못된 TItemPos가 있을 수도 있다.
	std::set <LPITEM> set_items;
	for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; i++)
	{
		LPITEM pItem = ch->GetItem(aItemPoses[i]);
		if (nullptr != pItem)
		{
			// 용혼석이 아닌 아이템이 개량창에 있을 수 없다.
			if (!pItem->IsDragonSoul())
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 628, "");
#endif
				SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, TItemPos(pItem->GetWindow(), pItem->GetCell()));
				return false;
			}
			set_items.insert(pItem);
		}
	}

	if (set_items.size() == 0)
	{
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);
		return false;
	}

	std::string stGroupName;
	int count = set_items.size();
	int need_count = 0;
	int fee = 0;
	std::vector <float> vec_probs;

	uint8_t ds_type, grade_idx, step_idx, strength_idx;
	int result_step;

	// 가장 처음 것을 강화의 기준으로 삼는다.
	std::set <LPITEM>::iterator it = set_items.begin();
	{
		LPITEM pItem = *it;
		GetDragonSoulInfo(pItem->GetVnum(), ds_type, grade_idx, step_idx, strength_idx);

		if (!m_pTable->GetRefineStepValues(ds_type, step_idx, need_count, fee, vec_probs))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 627, "");
#endif
			SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, TItemPos(pItem->GetWindow(), pItem->GetCell()));
			return false;
		}
	}

	while(++it != set_items.end())
	{
		LPITEM pItem = *it;
		// 클라 ui에서 장착한 아이템은 개량창에 올릴 수 없도록 막았기 때문에,
		// 별도의 알림 처리는 안함.
		if (pItem->IsEquipped())
		{
			return false;
		}
		if (ds_type != GetType(pItem->GetVnum()) || grade_idx != GetGradeIdx(pItem->GetVnum()) || step_idx != GetStepIdx(pItem->GetVnum()))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 628, "");
#endif
			SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, TItemPos(pItem->GetWindow(), pItem->GetCell()));
			return false;
		}
	}

	// 클라에서 한번 갯수 체크를 하기 때문에 count != need_count라면 invalid 클라일 가능성이 크다.
	if (count != need_count)
	{
		sys_err ("Possiblity of invalid client. Name %s", ch->GetName());
		uint8_t bSubHeader = count < need_count? DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL : DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL;
		SendRefineResultPacket(ch, bSubHeader, NPOS);
		return false;
	}

	if (ch->GetGold() < fee)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 232, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
		return false;
	}

	//float sum = 0.f;

	if (-1 == (result_step = Gamble(vec_probs)))
	{
		sys_err ("Gamble failed. See RefineStepTables' probabilities");
		return false;
	}

	LPITEM pResultItem = ITEM_MANAGER::instance().CreateItem(MakeDragonSoulVnum(ds_type, grade_idx, (uint8_t)result_step, 0));

	if (nullptr == pResultItem)
	{
		sys_err ("INVALID DRAGON SOUL(%d)", MakeDragonSoulVnum(ds_type, grade_idx, (uint8_t)result_step, 0));
		return false;
	}

	ch->PointChange(POINT_GOLD, -fee);
	int left_count = need_count;
	for (std::set <LPITEM>::iterator it = set_items.begin(); it != set_items.end(); it++)
	{
		LPITEM pItem = *it;
		int n = pItem->GetCount();
		if (left_count > n)
		{
			LPITEM removed = pItem->RemoveFromCharacter();
			ItemSystem::DestroyItemEntityEcs(
				EntityFactory::CreateItemEntity(g_registry, removed),
				"DRAGON_SOUL_REFINE_CONSUME");
			left_count -= n;
		}
		else
		{
			ItemSystem::ConsumeItemEcs(
				EntityFactory::CreateItemEntity(g_registry, pItem),
				left_count);
		}
	}

	ch->AutoGiveItem(pResultItem, true);
	if (result_step > step_idx)
	{
		char buf[128];
		sprintf(buf, "STEP : %d -> %d", step_idx, result_step);
		LogManager::instance().ItemLog(ch, pResultItem, "DS_STEP_REFINE_SUCCESS", buf);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 629, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_SUCCEED, TItemPos (pResultItem->GetWindow(), pResultItem->GetCell()));
		return true;
	}
	else
	{
		char buf[128];
		sprintf(buf, "STEP : %d -> %d", step_idx, result_step);
		LogManager::instance().ItemLog(ch, pResultItem, "DS_STEP_REFINE_FAIL", buf);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 630, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL, TItemPos (pResultItem->GetWindow(), pResultItem->GetCell()));
		return false;
	}
}

bool DSManager::DoRefineStepEcs(entt::entity owner, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	LPCHARACTER ch = ecs::LegacyCharOf(owner);
	if (!ch)
		return false;

	const bool result = DoRefineStep(ch, aItemPoses);
	SyncDragonSoulGridItems(ch, aItemPoses);
	return result;
}


bool IsDragonSoulRefineMaterial(LPITEM pItem)
{
	if (pItem->GetType() != ITEM_MATERIAL)
		return false;
	return (pItem->GetSubType() == MATERIAL_DS_REFINE_NORMAL ||
		pItem->GetSubType() == MATERIAL_DS_REFINE_BLESSED ||
		pItem->GetSubType() == MATERIAL_DS_REFINE_HOLLY);
}

bool DSManager::DoRefineStrength(LPCHARACTER ch, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (nullptr == ch)
		return false;
	if (nullptr == aItemPoses)
	{
		return false;
	}

	if (!ch->DragonSoul_RefineWindow_CanRefine()) {
		return false;
	}

	// 혹시나 모를 중복되는 item pointer 없애기 위해서 set 사용
	// 이상한 패킷을 보낼 경우, 중복된 TItemPos가 있을 수도 있고, 잘못된 TItemPos가 있을 수도 있다.
	std::set <LPITEM> set_items;
	for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; i++)
	{
		LPITEM pItem = ch->GetItem(aItemPoses[i]);
		if (pItem)
		{
			set_items.insert(pItem);
		}
	}
	if (set_items.size() == 0)
	{
		return false;
	}

	int fee;

	LPITEM pRefineStone = nullptr;
	LPITEM pDragonSoul = nullptr;
	for (std::set <LPITEM>::iterator it = set_items.begin(); it != set_items.end(); it++)
	{
		LPITEM pItem = *it;
		// 클라 ui에서 장착한 아이템은 개량창에 올릴 수 없도록 막았기 때문에,
		// 별도의 알림 처리는 안함.
		if (pItem->IsEquipped())
		{
			return false;
		}

		// 용혼석과 강화석만이 개량창에 있을 수 있다.
		// 그리고 하나씩만 있어야한다.
		if (pItem->IsDragonSoul())
		{
			if (pDragonSoul != nullptr)
			{
				SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL, TItemPos(pItem->GetWindow(), pItem->GetCell()));
				return false;
			}
			pDragonSoul = pItem;
		}
		else if(IsDragonSoulRefineMaterial(pItem))
		{
			if (pRefineStone != nullptr)
			{
				SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL, TItemPos(pItem->GetWindow(), pItem->GetCell()));
				return false;
			}
			pRefineStone = pItem;
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 628, "");
#endif
			SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, TItemPos(pItem->GetWindow(), pItem->GetCell()));
			return false;
		}
	}

	uint8_t bType, bGrade, bStep, bStrength;

	if (!pDragonSoul || !pRefineStone)
	{
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);

		return false;
	}

	if (nullptr != pDragonSoul)
	{
		GetDragonSoulInfo(pDragonSoul->GetVnum(), bType, bGrade, bStep, bStrength);

		float fWeight = 0.f;
		// 가중치 값이 없다면 강화할 수 없는 용혼석
		if (!m_pTable->GetWeight(bType, bGrade, bStep, bStrength + 1, fWeight))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 627, "");
#endif
			SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_MAX_REFINE, TItemPos(pDragonSoul->GetWindow(), pDragonSoul->GetCell()));
			return false;
		}
		// 강화했을 때 가중치가 0이라면 더 이상 강화되서는 안된다.
		if (fWeight < FLT_EPSILON)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 627, "");
#endif
			SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_MAX_REFINE, TItemPos(pDragonSoul->GetWindow(), pDragonSoul->GetCell()));
			return false;
		}
	}

	float fProb;
	if (!m_pTable->GetRefineStrengthValues(bType, pRefineStone->GetSubType(), bStrength, fee, fProb))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 627, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, TItemPos(pDragonSoul->GetWindow(), pDragonSoul->GetCell()));

		return false;
	}

	if (ch->GetGold() < fee)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 232, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
		return false;
	}

	ch->PointChange(POINT_GOLD, -fee);
	LPITEM pResult = nullptr;
	uint8_t bSubHeader;

	if (fnumber(0.f, 100.f) <= fProb)
	{
		pResult = ITEM_MANAGER::instance().CreateItem(MakeDragonSoulVnum(bType, bGrade, bStep, bStrength + 1));
		if (nullptr == pResult)
		{
			sys_err ("INVALID DRAGON SOUL(%d)", MakeDragonSoulVnum(bType, bGrade, bStep, bStrength + 1));
			return false;
		}
//		pDragonSoul->RemoveFromCharacter(); ds duplicate fix razor93

		pDragonSoul->CopyAttributeTo(pResult);
		RefreshItemAttributes(pResult);

		ItemSystem::ConsumeItemEcs(
			EntityFactory::CreateItemEntity(g_registry, pDragonSoul), 1);
		ItemSystem::ConsumeItemEcs(
			EntityFactory::CreateItemEntity(g_registry, pRefineStone), 1);

		char buf[128];
		sprintf(buf, "STRENGTH : %d -> %d", bStrength, bStrength + 1);
		LogManager::instance().ItemLog(ch, pDragonSoul, "DS_STRENGTH_REFINE_SUCCESS", buf);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 629, "");
#endif
		ch->AutoGiveItem(pResult, true);
		bSubHeader = DS_SUB_HEADER_REFINE_SUCCEED;
	}
	else
	{
		if (bStrength != 0)
		{
			pResult = ITEM_MANAGER::instance().CreateItem(MakeDragonSoulVnum(bType, bGrade, bStep, bStrength - 1));
			if (nullptr == pResult)
			{
				sys_err ("INVALID DRAGON SOUL(%d)", MakeDragonSoulVnum(bType, bGrade, bStep, bStrength - 1));
				return false;
			}
			pDragonSoul->CopyAttributeTo(pResult);
			RefreshItemAttributes(pResult);
		}
		bSubHeader = DS_SUB_HEADER_REFINE_FAIL;

		char buf[128];
		sprintf(buf, "STRENGTH : %d -> %d", bStrength, bStrength - 1);
		// strength강화는 실패시 깨질 수도 있어, 원본 아이템을 바탕으로 로그를 남김.
		LogManager::instance().ItemLog(ch, pDragonSoul, "DS_STRENGTH_REFINE_FAIL", buf);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 630, "");
#endif
		ItemSystem::ConsumeItemEcs(
			EntityFactory::CreateItemEntity(g_registry, pDragonSoul), 1);
		ItemSystem::ConsumeItemEcs(
			EntityFactory::CreateItemEntity(g_registry, pRefineStone), 1);
		if (nullptr != pResult)
			ch->AutoGiveItem(pResult, true);

	}

	SendRefineResultPacket(ch, bSubHeader, nullptr == pResult? NPOS : TItemPos (pResult->GetWindow(), pResult->GetCell()));

	return true;
}

bool DSManager::DoRefineStrengthEcs(entt::entity owner, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	LPCHARACTER ch = ecs::LegacyCharOf(owner);
	if (!ch)
		return false;

	const bool result = DoRefineStrength(ch, aItemPoses);
	SyncDragonSoulGridItems(ch, aItemPoses);
	return result;
}


#ifdef ENABLE_DS_REFINE_ALL
void DSManager::DoRefineAll(LPCHARACTER ch, uint8_t subheader, uint8_t type, uint8_t grade) {
	if (ch == nullptr || !(subheader == DS_SUB_HEADER_DO_REFINE_GRADE || subheader == DS_SUB_HEADER_DO_REFINE_STEP)) {
		return;
	}

	if (!(type >= 0 && type <= 5) || !(grade >= 0 && grade <= 5)) {
		return;
	}

	if (subheader == DS_SUB_HEADER_DO_REFINE_GRADE && grade == 5) {
		return;
	}

	if (ch && ch->DragonSoul_RefineWindow_CanRefine()) {
#ifdef ENABLE_SPAM_CHECK
		int32_t time = ch->GetLastDSREfine() - get_global_time();
		if (time > 0) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 234, "%d", time);
#endif
			return;
		}
#endif

		int32_t min = 300 + (192 * type) + (grade * DRAGON_SOUL_BOX_SIZE);
		std::set <LPITEM> set_items;
		bool first = false;
		if (subheader == DS_SUB_HEADER_DO_REFINE_GRADE) {
			int32_t grade = DRAGON_SOUL_GRADE_NORMAL;
			bool done = false;
			while (!done) {
				if (grade > 
#ifdef ENABLE_DS_GRADE_MYTH
				DRAGON_SOUL_GRADE_LEGENDARY
#else
				DRAGON_SOUL_GRADE_ANCIENT
#endif
				) {
					done = true;
					break;
				}

#ifdef ENABLE_SPAM_CHECK
				if (!first) {
					ch->SetLastDSREfine();
					first = true;
				}
#endif

				for (int32_t i = 0; i < DRAGON_SOUL_BOX_SIZE; i++) {
					LPITEM item = ch->GetItem(TItemPos(DRAGON_SOUL_INVENTORY, i + min));
					if (item && item->IsDragonSoul() && !item->IsEquipped()) {
						if (GetGradeIdx(item->GetVnum()) == grade) {
							set_items.insert(item);
						}
					}
				}

				if (set_items.size() < 2) {
					set_items.clear();
					grade++;
					continue;
				}

				int32_t n = 0;
				bool endnow = false;
				int32_t cell = 0;

				for (std::set <LPITEM>::iterator it = set_items.begin(); it != set_items.end(); it++) {
					n++;
					LPITEM item = *it;

					if (n % 2 == 0) {
						uint8_t ds_type, grade_idx, step_idx, strength_idx;
						int32_t result_grade;
						GetDragonSoulInfo(item->GetVnum(), ds_type, grade_idx, step_idx, strength_idx);

						int32_t need_count = 0, fee = 0;
						std::vector <float> vec_probs;

						if (!m_pTable->GetRefineGradeValues(ds_type, grade_idx, need_count, fee, vec_probs)) {
							continue;
						}

						if (need_count != 2) {
							endnow = true;
							break;
						}

						LPITEM itemold = ch->GetItem(TItemPos(DRAGON_SOUL_INVENTORY, cell));
						if (!itemold) {
							continue;
						}

						int32_t vnum = itemold->GetVnum();
						if (ds_type != GetType(vnum) || grade_idx != GetGradeIdx(vnum)) {
							continue;
						}

						if (ch->GetGold() < fee) {
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 232, "");
#endif
							endnow = true;
							break;
						}

						if (-1 == (result_grade = Gamble(vec_probs))) {
							continue;
						}

						LPITEM itemres = ITEM_MANAGER::instance().CreateItem(MakeDragonSoulVnum(ds_type, (int8_t)result_grade, 0, 0));
						if (!itemres) {
							sys_err ("INVALID DRAGON SOUL(%d)", MakeDragonSoulVnum(ds_type, (int8_t)result_grade, 0, 0));
							continue;
						}

						ch->PointChange(POINT_GOLD, -fee);

						LPITEM removedOld = itemold->RemoveFromCharacter();
						ItemSystem::DestroyItemEntityEcs(
							EntityFactory::CreateItemEntity(g_registry, removedOld),
							"DRAGON_SOUL_INVALID");
						LPITEM removedItem = item->RemoveFromCharacter();
						ItemSystem::DestroyItemEntityEcs(
							EntityFactory::CreateItemEntity(g_registry, removedItem),
							"DRAGON_SOUL_INVALID");

						if (ch->AutoGiveDS(itemres, true)) {
							char buf[128];
							if (result_grade > grade_idx) {
								sprintf(buf, "GRADE : %d -> %d", grade_idx, result_grade);
								LogManager::instance().ItemLog(ch, itemres, "DS_STEP_REFINE_SUCCESS", buf);
							} else {
								sprintf(buf, "GRADE : %d -> %d", grade_idx, result_grade);
								LogManager::instance().ItemLog(ch, itemres, "DS_STEP_REFINE_FAIL", buf);
							}
						}
					} else {
						cell = item->GetCell();
					}
				}

				set_items.clear();
				if (endnow) {
					done = true;
					break;
				}
			}
		} else {
			int32_t step = DRAGON_SOUL_STEP_LOWEST;
			bool done = false;
			while (!done) {
				if (step > DRAGON_SOUL_STEP_HIGH) {
					done = true;
					break;
				}

#ifdef ENABLE_SPAM_CHECK
				if (!first) {
					ch->SetLastDSREfine();
					first = true;
				}
#endif

				for (int32_t i = 0; i < DRAGON_SOUL_BOX_SIZE; i++) {
					LPITEM item = ch->GetItem(TItemPos(DRAGON_SOUL_INVENTORY, i + min));
					if (item && item->IsDragonSoul() && !item->IsEquipped()) {
						if (GetStepIdx(item->GetVnum()) == step) {
							set_items.insert(item);
						}
					}
				}

				if (set_items.size() < 2) {
					set_items.clear();
					step++;
					continue;
				}

				int32_t n = 0;
				bool endnow = false;
				int32_t cell = 0;

				for (std::set <LPITEM>::iterator it = set_items.begin(); it != set_items.end(); it++) {
					n++;
					LPITEM item = *it;

					if (n % 2 == 0) {
						uint8_t ds_type, grade_idx, step_idx, strength_idx;
						int32_t result_step;
						GetDragonSoulInfo(item->GetVnum(), ds_type, grade_idx, step_idx, strength_idx);

						int32_t need_count = 0, fee = 0;
						std::vector <float> vec_probs;

						if (!m_pTable->GetRefineStepValues(ds_type, step_idx, need_count, fee, vec_probs)) {
							continue;
						}

						if (need_count != 2) {
							endnow = true;
							break;
						}

						LPITEM itemold = ch->GetItem(TItemPos(DRAGON_SOUL_INVENTORY, cell));
						if (!itemold) {
							continue;
						}

						int32_t vnum = itemold->GetVnum();
						if (ds_type != GetType(vnum) || grade_idx != GetGradeIdx(vnum) || step_idx != GetStepIdx(vnum)) {
							continue;
						}

						if (ch->GetGold() < fee) {
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 232, "");
#endif
							endnow = true;
							break;
						}

						if (-1 == (result_step = Gamble(vec_probs))) {
							continue;
						}

						LPITEM itemres = ITEM_MANAGER::instance().CreateItem(MakeDragonSoulVnum(ds_type, grade_idx, (int8_t)result_step, 0));
						if (!itemres) {
							sys_err ("INVALID DRAGON SOUL(%d)", MakeDragonSoulVnum(ds_type, grade_idx, (int8_t)result_step, 0));
							continue;
						}

						ch->PointChange(POINT_GOLD, -fee);

						LPITEM removedOld = itemold->RemoveFromCharacter();
						ItemSystem::DestroyItemEntityEcs(
							EntityFactory::CreateItemEntity(g_registry, removedOld),
							"DRAGON_SOUL_INVALID");
						LPITEM removedItem = item->RemoveFromCharacter();
						ItemSystem::DestroyItemEntityEcs(
							EntityFactory::CreateItemEntity(g_registry, removedItem),
							"DRAGON_SOUL_INVALID");

						if (ch->AutoGiveDS(itemres, true)) {
							char buf[128];
							if (result_step > step_idx) {
								sprintf(buf, "STEP : %d -> %d", step_idx, result_step);
								LogManager::instance().ItemLog(ch, itemres, "DS_STEP_REFINE_SUCCESS", buf);
							} else {
								sprintf(buf, "STEP : %d -> %d", step_idx, result_step);
								LogManager::instance().ItemLog(ch, itemres, "DS_STEP_REFINE_FAIL", buf);
							}
						}
					} else {
						cell = item->GetCell();
					}
				}

				set_items.clear();
				if (endnow) {
					done = true;
					break;
				}
			}
		}
	}

	return;
}

void DSManager::DoRefineAllEcs(entt::entity owner, uint8_t subheader, uint8_t type, uint8_t grade)
{
	LPCHARACTER ch = ecs::LegacyCharOf(owner);
	if (!ch)
		return;

	DoRefineAll(ch, subheader, type, grade);
	for (int i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i)
	{
		LPITEM item = ch->GetDragonSoulItem(i);
		if (item)
			SyncDragonSoulItemPtr(item);
	}
}

#endif

void DSManager::SendRefineResultPacket(LPCHARACTER ch, uint8_t bSubHeader, const TItemPos& pos)
{
	TPacketGCDragonSoulRefine pack;
	pack.bSubType = bSubHeader;

	if (pos.IsValidItemPosition())
	{
		pack.Pos = pos;
	}
	LPDESC d = ch->GetDesc();
	if (nullptr == d)
	{
		return ;
	}
	else
	{
		d->Packet(&pack, sizeof(pack));
	}
}

int DSManager::LeftTime(LPITEM pItem) const
{
	return LeftTime(EntityFactory::CreateItemEntity(g_registry, pItem));
}

int DSManager::LeftTime(entt::entity item) const
{
	if (!ItemSystem::IsValidItem(item))
		return false;

	if (ItemSystem::GetItemLimitTimerBasedOnWearIndex(item) >= 0)
	{
		return ItemSystem::GetItemSocket(item, ITEM_SOCKET_REMAIN_SEC);
	}
	else
	{
		return INT_MAX;
	}
}


bool DSManager::IsTimeLeftDragonSoul(LPITEM pItem) const
{
	return IsTimeLeftDragonSoul(EntityFactory::CreateItemEntity(g_registry, pItem));
}

bool DSManager::IsTimeLeftDragonSoul(entt::entity item) const
{
	if (!ItemSystem::IsValidItem(item))
		return false;

	if (ItemSystem::GetItemLimitTimerBasedOnWearIndex(item) >= 0)
	{
		return ItemSystem::GetItemSocket(item, ITEM_SOCKET_REMAIN_SEC) > 0;
	}
	else
	{
		return true;
	}
}


bool DSManager::IsActiveDragonSoul(LPITEM pItem) const
{
	return IsActiveDragonSoul(EntityFactory::CreateItemEntity(g_registry, pItem));
}

bool DSManager::IsActiveDragonSoul(entt::entity item) const
{
	return ItemSystem::GetItemSocket(item, ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX) != 0;
}


bool DSManager::ActivateDragonSoul(LPITEM pItem)
{
	if (nullptr == pItem)
		return false;
	LPCHARACTER pOwner = pItem->GetOwner();
	if (nullptr == pOwner)
		return false;

	int deck_idx = pOwner->DragonSoul_GetActiveDeck();

	if (deck_idx < 0)
		return false;

	if (DRAGON_SOUL_EQUIP_SLOT_START + DS_SLOT_MAX * deck_idx <= pItem->GetCell() &&
			pItem->GetCell() < DRAGON_SOUL_EQUIP_SLOT_START + DS_SLOT_MAX * (deck_idx + 1))
	{
		if (IsTimeLeftDragonSoul(pItem) && !IsActiveDragonSoul(pItem))
		{
			char buf[128];
			sprintf (buf, "LEFT TIME(%d)", LeftTime(pItem));
			LogManager::instance().ItemLog(pOwner, pItem, "DS_ACTIVATE", buf);
			pItem->ModifyPoints(true);
			pItem->SetSocket(ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX, 1);

			pItem->StartTimerBasedOnWearExpireEvent();
		}
		return true;
	}
	else
		return false;
}

bool DSManager::ActivateDragonSoulEcs(entt::entity item)
{
	LPITEM legacyItem = LegacyDragonSoulItemOf(item);
	if (!legacyItem)
		return false;

	const bool result = ActivateDragonSoul(legacyItem);
	SyncDragonSoulItemEntity(item);
	return result;
}


bool DSManager::DeactivateDragonSoul(LPITEM pItem, bool bSkipRefreshOwnerActiveState)
{
	if (nullptr == pItem)
		return false;

	LPCHARACTER pOwner = pItem->GetOwner();
	if (nullptr == pOwner)
		return false;

	if (!IsActiveDragonSoul(pItem))
		return false;

	char buf[128];
	pItem->StopTimerBasedOnWearExpireEvent();
	pItem->SetSocket(ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX, 0);
	pItem->ModifyPoints(false);

	sprintf (buf, "LEFT TIME(%d)", LeftTime(pItem));
	LogManager::instance().ItemLog(pOwner, pItem, "DS_DEACTIVATE", buf);

	if (false == bSkipRefreshOwnerActiveState)
		RefreshDragonSoulState(pOwner);

	return true;
}

bool DSManager::DeactivateDragonSoulEcs(entt::entity item, bool bSkipRefreshOwnerActiveState)
{
	LPITEM legacyItem = LegacyDragonSoulItemOf(item);
	if (!legacyItem)
		return false;

	const bool result = DeactivateDragonSoul(legacyItem, bSkipRefreshOwnerActiveState);
	SyncDragonSoulItemEntity(item);
	return result;
}


void DSManager::RefreshDragonSoulState(LPCHARACTER ch)
{
	if (nullptr == ch)
		return ;
	for (int i = WEAR_MAX_NUM; i < WEAR_MAX_NUM + DS_SLOT_MAX * DRAGON_SOUL_DECK_MAX_NUM; i++)
	{
		LPITEM pItem = ch->GetWear(i);
		if (pItem != nullptr)
		{
			if(IsActiveDragonSoul(pItem))
			{
				return;
			}
		}
	}
	ch->DragonSoul_DeactivateAll();
}

DSManager::DSManager()
{
	m_pTable = nullptr;
}

DSManager::~DSManager()
{
	if (m_pTable)
		delete m_pTable;
}

