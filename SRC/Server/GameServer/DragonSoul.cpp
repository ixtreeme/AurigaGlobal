#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/DragonSoulSystem.hpp"
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
#include "ecs/systems/PointSystem.hpp"
//#include <boost/lexical_cast.hpp>

template <typename T> T MINMAX(T min, T value, T max)
{
	T tv;

	tv = (min > value ? min : value);
	return (max < tv) ? max : tv;
}

typedef std::vector <std::string> TTokenVector;


namespace {

void SyncDragonSoulItemEntity(entt::entity item)
{
	if (item != entt::null)
		ItemSystem::SyncItemStateFromLegacy(item);
}

void SyncDragonSoulItemPtr(entt::entity item)
{
	SyncDragonSoulItemEntity(item);
}

TItemPos DragonSoulItemPosition(entt::entity item)
{
	return ItemSystem::IsValidItem(item)
		? TItemPos(ItemSystem::GetItemWindow(item), ItemSystem::GetItemCell(item))
		: NPOS;
}

bool ConsumeDragonSoulMaterials(const std::set<entt::entity>& items, int amount)
{
	int remaining = amount;
	for (const entt::entity item : items)
	{
		if (remaining <= 0)
			break;

		const uint32_t available = ItemSystem::GetItemCount(item);
		const uint32_t consumed = MIN(static_cast<uint32_t>(remaining), available);
		if (consumed == 0 || !ItemSystem::ConsumeItemEcs(item, consumed))
			return false;
		remaining -= static_cast<int>(consumed);
	}
	return remaining == 0;
}

void SyncDragonSoulGridItems(entt::entity ch, const TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (!ecs::PlayerRuntime::IsValid(ch))
		return;

	const entt::entity owner = ch;
	for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; ++i)
		SyncDragonSoulItemPtr(ItemSystem::GetItem(owner, aItemPoses[i]));
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
bool MakeDistinctRandomNumberSet(std::list<float> probabilities, OUT std::vector<int>& random_set)
{
	std::vector<float> weights(probabilities.begin(), probabilities.end());
	size_t positive = 0;
	for (const float weight : weights) {
		if (!std::isfinite(weight) || weight < 0.f)
			return false;
		positive += weight > 0.f;
	}
	if (positive < random_set.size())
		return false;
	std::vector<int> result;
	result.reserve(random_set.size());
	while (result.size() < random_set.size()) {
		float total = 0.f;
		int lastPositive = -1;
		for (size_t i = 0; i < weights.size(); ++i) {
			total += weights[i];
			if (weights[i] > 0.f)
				lastPositive = static_cast<int>(i);
		}
		if (!std::isfinite(total) || total <= 0.f || lastPositive < 0)
			return false;
		const float draw = fnumber(0.f, total);
		if (!std::isfinite(draw) || draw < 0.f || draw > total)
			return false;
		float sum = 0.f;
		int selected = lastPositive; // Inclusive upper endpoint / float rounding.
		for (size_t i = 0; i < weights.size(); ++i) {
			if (weights[i] <= 0.f)
				continue;
			sum += weights[i];
			if (draw <= sum) {
				selected = static_cast<int>(i);
				break;
			}
		}
		result.push_back(selected);
		weights[selected] = 0.f;
	}
	random_set = std::move(result);
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

bool DSManager::ReadDragonSoulTableFile(const char* filename)
{
	auto table = std::make_unique<DragonSoulTable>();
	if (!table->ReadDragonSoulTableFile(filename))
		return false;
	m_pTable = std::move(table);
	return true;
}
void DSManager::GetDragonSoulInfo(uint32_t dwVnum, uint8_t& bType, uint8_t& bGrade, uint8_t& bStep, uint8_t& bStrength) const
{
	bType = GetType(dwVnum);
	bGrade = GetGradeIdx(dwVnum);
	bStep = GetStepIdx(dwVnum);
	bStrength = GetStrengthIdx(dwVnum);
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


bool DSManager::PrepareAttributes(entt::entity item, ecs::ItemAttributes& result, bool refresh)
{
	if (!m_pTable || !ItemSystem::IsDragonSoulItem(item))
		return false;
	const auto* current = g_registry.try_get<ecs::ItemAttributes>(item);
	if (!current)
		return false;
	const uint32_t vnum = ItemSystem::GetItemVnum(item);
	uint8_t type, grade, step, strength;
	GetDragonSoulInfo(vnum, type, grade, step, strength);
	if (vnum / 10000 > UINT8_MAX || grade >= DRAGON_SOUL_GRADE_MAX ||
		step >= DRAGON_SOUL_STEP_MAX || strength >= DRAGON_SOUL_STRENGTH_MAX)
		return false;
	DragonSoulTable::TVecApplys basic, additional;
	int basicCount = 0, addMin = 0, addMax = 0;
	float weight = 0.f;
	if (!m_pTable->GetBasicApplys(type, basic) || !m_pTable->GetAdditionalApplys(type, additional) ||
		!m_pTable->GetApplyNumSettings(type, grade, basicCount, addMin, addMax) ||
		!m_pTable->GetWeight(type, grade, step, strength, weight))
		return false;
	constexpr int additionalStart = DRAGON_SOUL_ADDITIONAL_ATTR_START_IDX;
	constexpr int additionalSlots = ITEM_ATTRIBUTE_MAX_NUM - additionalStart;
	if (basicCount < 0 || basicCount > additionalStart || static_cast<size_t>(basicCount) > basic.size() ||
		addMin < 0 || addMax < addMin || addMax > additionalSlots || !std::isfinite(weight) || weight < 0.f)
		return false;
	weight /= 100.f;
	auto convert = [weight](const SApply& apply, TPlayerItemAttribute& attribute) {
		if (apply.apply_type <= APPLY_NONE || apply.apply_type >= MAX_APPLY_NUM || apply.apply_type > UINT8_MAX)
			return false;
		// Preserve the original rounding rule, but never narrow NaN/overflow.
		const float value = std::ceil(static_cast<float>(apply.apply_value) * weight - 0.01f);
		if (!std::isfinite(value) || value < INT16_MIN || value > INT16_MAX)
			return false;
		attribute = {static_cast<uint8_t>(apply.apply_type), static_cast<int16_t>(value)};
		return true;
	};
	ecs::ItemAttributes prepared = refresh ? *current : ecs::ItemAttributes{};
	for (int i = 0; i < basicCount; ++i)
		if (!convert(basic[i], prepared.attrs[i]))
			return false;
	if (refresh) {
		for (int i = additionalStart; i < ITEM_ATTRIBUTE_MAX_NUM; ++i) {
			const auto applyType = prepared.attrs[i].bType;
			if (applyType == APPLY_NONE)
				continue;
			const auto found = std::find_if(additional.begin(), additional.end(),
				[applyType](const SApply& apply) { return apply.apply_type == applyType; });
			if (found == additional.end() || !convert(*found, prepared.attrs[i]))
				return false;
		}
	} else {
		std::list<float> probabilities;
		std::vector<TPlayerItemAttribute> values(additional.size());
		for (size_t i = 0; i < additional.size(); ++i) {
			if (!std::isfinite(additional[i].prob) || additional[i].prob < 0.f || !convert(additional[i], values[i]))
				return false;
			probabilities.push_back(additional[i].prob);
		}
		std::vector<int> selected(number(addMin, addMax));
		if (!MakeDistinctRandomNumberSet(probabilities, selected))
			return false;
		for (size_t i = 0; i < selected.size(); ++i)
			prepared.attrs[additionalStart + i] = values[selected[i]];
	}
	result = prepared;
	return true;
}

bool DSManager::RefreshItemAttributes(entt::entity item)
{
	ecs::ItemAttributes attributes;
	return PrepareAttributes(item, attributes, true) && ItemSystem::SetItemAttributesEcs(item, attributes);
}

bool DSManager::PutAttributes(entt::entity item)
{
	ecs::ItemAttributes attributes;
	return PrepareAttributes(item, attributes, false) && ItemSystem::SetItemAttributesEcs(item, attributes);
}

#ifdef ENABLE_DS_ENCHANT
DSManager::EnchantResult DSManager::EnchantWithItemCost(entt::entity owner, entt::entity item, entt::entity material)
{
	if (!ecs::PlayerRuntime::IsPC(owner) || !ItemSystem::IsDragonSoulItem(item) ||
		ItemSystem::GetItemOwner(item) != owner || ItemSystem::IsItemExchanging(item) || ItemSystem::IsItemLocked(item))
		return EnchantResult::InvalidTarget;
	const bool equipped = ItemSystem::IsItemEquipped(item);
	if ((equipped && DragonSoulSystem::IsDeckActivated(owner)) || IsActiveDragonSoul(item))
		return EnchantResult::Active;
	const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
	if (!location)
		return EnchantResult::InvalidTarget;
	// Equipment locations store the absolute main-inventory cell, whereas
	// GetItem(EQUIPMENT, cell) accepts a relative equipment index.
	if (equipped) {
		if (location->cell < DRAGON_SOUL_EQUIP_SLOT_START || location->cell >= DRAGON_SOUL_EQUIP_SLOT_END ||
			(location->window != EQUIPMENT && location->window != INVENTORY) ||
			ItemSystem::GetInventoryItem(owner, location->cell) != item)
			return EnchantResult::InvalidTarget;
	} else if (ItemSystem::GetItem(owner, TItemPos(location->window, location->cell)) != item) {
		return EnchantResult::InvalidTarget;
	}
	const uint32_t vnum = ItemSystem::GetItemVnum(item);
	if (GetGradeIdx(vnum) !=
#ifdef ENABLE_DS_GRADE_MYTH
		DRAGON_SOUL_GRADE_MYTH
#else
		DRAGON_SOUL_GRADE_LEGENDARY
#endif
		|| GetStepIdx(vnum) != DRAGON_SOUL_STEP_HIGHEST)
		return EnchantResult::InvalidGrade;
	if (item == material || !ItemSystem::CanConsumeOwnedItem(owner, material) ||
		ItemSystem::GetItemType(material) != ITEM_USE || ItemSystem::GetItemSubType(material) != USE_DS_ENCHANT)
		return EnchantResult::InvalidMaterial;
	ecs::ItemAttributes attributes;
	if (!PrepareAttributes(item, attributes, false))
		return EnchantResult::Failed;
	if (!ItemSystem::ConsumeItemEcs(material))
		return EnchantResult::Failed;
	return ItemSystem::SetItemAttributesEcs(item, attributes) ? EnchantResult::Success : EnchantResult::Failed;
}
#endif
bool DSManager::DragonSoulItemInitialize(entt::entity item)
{
	if (!ItemSystem::IsDragonSoulItem(item))
		return false;
	if (!PutAttributes(item))
		return false;

	const int duration = GetDuration(item);
	if (duration > 0)
		ItemSystem::SetItemSocketEcs(item, ITEM_SOCKET_REMAIN_SEC, duration);
	return true;
}
uint32_t DSManager::MakeDragonSoulVnum(uint8_t bType, uint8_t grade, uint8_t step, uint8_t refine)
{
	return bType * 10000 + grade * 1000 + step * 100 + refine * 10;
}

int DSManager::GetDuration(entt::entity item) const
{
	return ItemSystem::GetItemDuration(item);
}
// 용혼석을 받아서 용심을 추출하는 함수
bool DSManager::ExtractDragonHeart(LPCHARACTER ch, entt::entity item, entt::entity extractor)
{
	if (!ch || !ItemSystem::IsDragonSoulItem(item))
		return false;

	const entt::entity owner = ((ch) ? (ch)->GetEntityHandle() : entt::null);
	const bool hasExtractor = extractor != entt::null;
	if (hasExtractor && !ItemSystem::IsValidItem(extractor))
		return false;

	if (ItemSystem::IsItemEquipped(item))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 623, "");
#endif
		return false;
	}

	uint8_t ds_type, grade_idx, step_idx, strength_idx;
	GetDragonSoulInfo(ItemSystem::GetItemVnum(item), ds_type, grade_idx, step_idx, strength_idx);

	const int bonus = hasExtractor ? ItemSystem::GetItemValue(extractor, 0) : 0;
	std::vector<float> chargings;
	std::vector<float> probabilities;
	if (!m_pTable->GetDragonHeartExtValues(ds_type, grade_idx, chargings, probabilities))
		return false;

	const int resultIndex = Gamble(probabilities);
	if (resultIndex < 0 || static_cast<size_t>(resultIndex) >= chargings.size())
	{
		LOG_ERROR("Gamble is failed. ds_type({}), grade_idx({})", static_cast<int>(ds_type), static_cast<int>(grade_idx));
		return false;
	}

	float charge = chargings[resultIndex] * (100 + bonus) / 100.f;
#ifdef ENABLE_DS_EDITS
	charge = static_cast<float>(bonus);
#else
	charge = MINMAX<float>(0.f, charge, 100.f);
#endif

	if (charge < FLT_EPSILON)
	{
		LogManager::instance().ItemLogEntity(ch, item, "DS_HEART_EXTRACT_FAIL", "");
		ItemSystem::ConsumeItemEcs(item, 1);
		if (hasExtractor)
			ItemSystem::ConsumeItemEcs(extractor, 1);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 624, "");
#endif
		return false;
	}

	const entt::entity dragonHeart = ITEM_MANAGER::instance().CreateItem(DRAGON_HEART_VNUM);
	if (dragonHeart == entt::null)
	{
		LOG_ERROR("Cannot create DRAGON_HEART({}).", DRAGON_HEART_VNUM);
		return false;
	}

	const int chargePercent = static_cast<int>(charge + 0.5f);
	ItemSystem::SetItemSocketEcs(dragonHeart, ITEM_SOCKET_CHARGING_AMOUNT_IDX, chargePercent);

	auto hint = std::to_string(chargePercent);
	hint += "%s";
	LogManager::instance().ItemLogEntity(ch, item, "DS_HEART_EXTRACT_SUCCESS", hint.c_str());

	if (!ItemSystem::ConsumeItemEcs(item, 1))
	{
		ItemSystem::DestroyItemEntityEcs(dragonHeart, "DS_HEART_INPUT_INVALID");
		return false;
	}
	if (hasExtractor && !ItemSystem::ConsumeItemEcs(extractor, 1))
	{
		ItemSystem::DestroyItemEntityEcs(dragonHeart, "DS_HEART_EXTRACTOR_INVALID");
		return false;
	}

	ItemSystem::AutoGiveItem(owner, dragonHeart, true);
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 624, "");
#endif
	return true;
}
bool DSManager::ExtractDragonHeartEcs(entt::entity owner, entt::entity item, entt::entity extractor)
{
	LPCHARACTER ch = ecs::LegacyCharOf(owner);
	if (!ch || item == entt::null)
		return false;

	const bool result = ExtractDragonHeart(ch, item, extractor);
	SyncDragonSoulItemEntity(item);
	SyncDragonSoulItemEntity(extractor);
	return result;
}


// 특정 용혼석을 장비창에서 제거할 때에 성공 여부를 결정하고, 실패시 부산물을 주는 함수.
bool DSManager::PullOut(LPCHARACTER ch, TItemPos DestCell, entt::entity& item, entt::entity extractor)
{
	if (!ch || !ItemSystem::IsDragonSoulItem(item))
	{
		LOG_ERROR("Invalid dragon soul pull-out input. ch({}) item({})",
			static_cast<const void*>(ch), static_cast<uint32_t>(item));
		return false;
	}

	const entt::entity owner = ((ch) ? (ch)->GetEntityHandle() : entt::null);
	const bool hasExtractor = extractor != entt::null;
	if (hasExtractor && !ItemSystem::IsValidItem(extractor))
		return false;
	const uint32_t extractorVnum = hasExtractor ? ItemSystem::GetItemVnum(extractor) : 0;

	if (!IsValidCellForThisItem(item, DestCell))
	{
		const int emptyCell = ItemSystem::GetEmptyDragonSoulInventory(owner, item);
		if (emptyCell < 0)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 626, "");
#endif
			return false;
		}
		DestCell = TItemPos(DRAGON_SOUL_INVENTORY, emptyCell);
	}

	if (!ItemSystem::IsItemEquipped(item) || !ItemSystem::RemoveItemEcs(item))
		return false;

	uint8_t ds_type, grade_idx, step_idx, strength_idx;
	GetDragonSoulInfo(ItemSystem::GetItemVnum(item), ds_type, grade_idx, step_idx, strength_idx);

	float probability = 0.f;
	uint32_t byProductVnum = 0;
	if (!m_pTable->GetDragonSoulExtValues(ds_type, grade_idx, probability, byProductVnum))
		return ItemSystem::PlaceItemEcs(owner, item, DestCell.window_type, DestCell.cell);

	const float dice = fnumber(0.f, 100.f);
	int bonus = 0;
	bool success = dice <= probability;
	if (hasExtractor)
	{
		bonus = ItemSystem::GetItemValue(extractor, ITEM_VALUE_DRAGON_SOUL_POLL_OUT_BONUS_IDX);
		if (!ItemSystem::ConsumeItemEcs(extractor, 1))
			return false;
		success = number(1, 100) <= bonus;
	}

	char logHint[128];
	if (success)
	{
		if (hasExtractor)
			sprintf(logHint, "dice(%d) prob(%d + %d) EXTR(VN:%d)", static_cast<int>(dice), static_cast<int>(probability), bonus, extractorVnum);
		else
			sprintf(logHint, "dice(%d) prob(%d)", static_cast<int>(dice), static_cast<int>(probability));

		LogManager::instance().ItemLogEntity(ch, item, "DS_PULL_OUT_SUCCESS", logHint);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 534, "%s", ItemSystem::GetItemName(item));
#endif
		return ItemSystem::PlaceItemEcs(owner, item, DestCell.window_type, DestCell.cell);
	}

	if (hasExtractor)
		sprintf(logHint, "dice(%d) prob(%d + %d) EXTR(VN:%d) ByProd(VN:%d)", static_cast<int>(dice), static_cast<int>(probability), bonus, extractorVnum, byProductVnum);
	else
		sprintf(logHint, "dice(%d) prob(%d) ByProd(VNUM:%d)", static_cast<int>(dice), static_cast<int>(probability), byProductVnum);

	LogManager::instance().ItemLogEntity(ch, item, "DS_PULL_OUT_FAILED", logHint);
	ItemSystem::DestroyItemEntityEcs(item, "DRAGON_SOUL_BYPRODUCT");
	item = entt::null;

	if (byProductVnum != 0)
	{
		const entt::entity byProduct = ItemSystem::AutoGiveItemEcs(owner, byProductVnum, 1, -1, true);
#ifdef TEXTS_IMPROVEMENT
		if (byProduct != entt::null)
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 535, "%s", ItemSystem::GetItemName(byProduct));
		else
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 536, "");
#endif
	}
#ifdef TEXTS_IMPROVEMENT
	else
	{
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 537, "");
	}
#endif
	return false;
}
bool DSManager::PullOutEcs(entt::entity owner, TItemPos DestCell, entt::entity& item, entt::entity extractor)
{
	LPCHARACTER ch = ecs::LegacyCharOf(owner);
	if (!ch || item == entt::null)
		return false;

	const bool result = PullOut(ch, DestCell, item, extractor);
	SyncDragonSoulItemEntity(item);
	SyncDragonSoulItemEntity(extractor);
	return result;
}


bool DSManager::DoRefineGrade(entt::entity ch, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (!ecs::PlayerRuntime::IsValid(ch) || !DragonSoulSystem::CanRefine(ch))
		return false;

	std::set<entt::entity> items;
	for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; ++i)
	{
		if (aItemPoses[i].IsEquipPosition())
			return false;

		const entt::entity item = ItemSystem::GetItem(ch, aItemPoses[i]);
		if (item == entt::null)
			continue;
		if (!ItemSystem::IsDragonSoulItem(item))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 628, "");
#endif
			SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, DragonSoulItemPosition(item));
			return false;
		}
		items.insert(item);
	}

	if (items.empty())
	{
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);
		return false;
	}

	uint8_t dsType, grade, step, strength;
	GetDragonSoulInfo(ItemSystem::GetItemVnum(*items.begin()), dsType, grade, step, strength);

	int neededCount = 0;
	int fee = 0;
	std::vector<float> probabilities;
	if (!m_pTable->GetRefineGradeValues(dsType, grade, neededCount, fee, probabilities))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 627, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, DragonSoulItemPosition(*items.begin()));
		return false;
	}

	for (const entt::entity item : items)
	{
		const uint32_t vnum = ItemSystem::GetItemVnum(item);
		if (ItemSystem::IsItemEquipped(item) || dsType != GetType(vnum) || grade != GetGradeIdx(vnum))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 628, "");
#endif
			SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, DragonSoulItemPosition(item));
			return false;
		}
	}

	const int suppliedCount = static_cast<int>(items.size());
	if (suppliedCount != neededCount)
	{
		LOG_ERROR("Possiblity of invalid client. Name {}", ecs::PlayerRuntime::GetName(ch).data());
		const uint8_t subHeader = suppliedCount < neededCount
			? DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL
			: DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL;
		SendRefineResultPacket(ch, subHeader, NPOS);
		return false;
	}

	if (ecs::PointSystem::GetGold(ch) < fee)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 232, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
		return false;
	}

	const int resultGrade = Gamble(probabilities);
	if (resultGrade < 0)
	{
		LOG_ERROR("Gamble failed. See RefineGardeTables' probabilities");
		return false;
	}

	const uint32_t resultVnum = MakeDragonSoulVnum(dsType, static_cast<uint8_t>(resultGrade), 0, 0);
	const entt::entity resultItem = ITEM_MANAGER::instance().CreateItem(resultVnum);
	if (resultItem == entt::null)
	{
		LOG_ERROR("INVALID DRAGON SOUL({})", resultVnum);
		return false;
	}

	if (!ConsumeDragonSoulMaterials(items, neededCount))
	{
		ItemSystem::DestroyItemEntityEcs(resultItem, "DRAGON_SOUL_REFINE_INPUT_INVALID");
		return false;
	}

	ecs::PointSystem::Change(ch, POINT_GOLD, -fee);
	ItemSystem::AutoGiveItem(ch, resultItem, true);

	char logHint[128];
	sprintf(logHint, "GRADE : %d -> %d", grade, resultGrade);
	const bool success = resultGrade > grade;
	LogManager::instance().ItemLogEntity(ch, resultItem,
		success ? "DS_GRADE_REFINE_SUCCESS" : "DS_GRADE_REFINE_FAIL", logHint);
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, success ? 629 : 630, "");
#endif
	SendRefineResultPacket(ch,
		success ? DS_SUB_HEADER_REFINE_SUCCEED : DS_SUB_HEADER_REFINE_FAIL,
		DragonSoulItemPosition(resultItem));
	return success;
}
bool DSManager::DoRefineGradeEcs(entt::entity owner, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (!ecs::PlayerRuntime::IsValid(owner))
		return false;

	const bool result = DoRefineGrade(owner, aItemPoses);
	SyncDragonSoulGridItems(owner, aItemPoses);
	return result;
}


bool DSManager::DoRefineStep(entt::entity ch, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (!ecs::PlayerRuntime::IsValid(ch) || !DragonSoulSystem::CanRefine(ch))
		return false;

	std::set<entt::entity> items;
	for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; ++i)
	{
		const entt::entity item = ItemSystem::GetItem(ch, aItemPoses[i]);
		if (item == entt::null)
			continue;
		if (!ItemSystem::IsDragonSoulItem(item))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 628, "");
#endif
			SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, DragonSoulItemPosition(item));
			return false;
		}
		items.insert(item);
	}

	if (items.empty())
	{
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);
		return false;
	}

	uint8_t dsType, grade, step, strength;
	GetDragonSoulInfo(ItemSystem::GetItemVnum(*items.begin()), dsType, grade, step, strength);

	int neededCount = 0;
	int fee = 0;
	std::vector<float> probabilities;
	if (!m_pTable->GetRefineStepValues(dsType, step, neededCount, fee, probabilities))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 627, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, DragonSoulItemPosition(*items.begin()));
		return false;
	}

	for (const entt::entity item : items)
	{
		const uint32_t vnum = ItemSystem::GetItemVnum(item);
		if (ItemSystem::IsItemEquipped(item) || dsType != GetType(vnum) ||
			grade != GetGradeIdx(vnum) || step != GetStepIdx(vnum))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 628, "");
#endif
			SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, DragonSoulItemPosition(item));
			return false;
		}
	}

	const int suppliedCount = static_cast<int>(items.size());
	if (suppliedCount != neededCount)
	{
		LOG_ERROR("Possiblity of invalid client. Name {}", ecs::PlayerRuntime::GetName(ch).data());
		const uint8_t subHeader = suppliedCount < neededCount
			? DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL
			: DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL;
		SendRefineResultPacket(ch, subHeader, NPOS);
		return false;
	}

	if (ecs::PointSystem::GetGold(ch) < fee)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 232, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
		return false;
	}

	const int resultStep = Gamble(probabilities);
	if (resultStep < 0)
	{
		LOG_ERROR("Gamble failed. See RefineStepTables' probabilities");
		return false;
	}

	const uint32_t resultVnum = MakeDragonSoulVnum(dsType, grade, static_cast<uint8_t>(resultStep), 0);
	const entt::entity resultItem = ITEM_MANAGER::instance().CreateItem(resultVnum);
	if (resultItem == entt::null)
	{
		LOG_ERROR("INVALID DRAGON SOUL({})", resultVnum);
		return false;
	}

	if (!ConsumeDragonSoulMaterials(items, neededCount))
	{
		ItemSystem::DestroyItemEntityEcs(resultItem, "DRAGON_SOUL_REFINE_INPUT_INVALID");
		return false;
	}

	ecs::PointSystem::Change(ch, POINT_GOLD, -fee);
	ItemSystem::AutoGiveItem(ch, resultItem, true);

	char logHint[128];
	sprintf(logHint, "STEP : %d -> %d", step, resultStep);
	const bool success = resultStep > step;
	LogManager::instance().ItemLogEntity(ch, resultItem,
		success ? "DS_STEP_REFINE_SUCCESS" : "DS_STEP_REFINE_FAIL", logHint);
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, success ? 629 : 630, "");
#endif
	SendRefineResultPacket(ch,
		success ? DS_SUB_HEADER_REFINE_SUCCEED : DS_SUB_HEADER_REFINE_FAIL,
		DragonSoulItemPosition(resultItem));
	return success;
}
bool DSManager::DoRefineStepEcs(entt::entity owner, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (!ecs::PlayerRuntime::IsValid(owner))
		return false;

	const bool result = DoRefineStep(owner, aItemPoses);
	SyncDragonSoulGridItems(owner, aItemPoses);
	return result;
}


bool IsDragonSoulRefineMaterial(entt::entity item)
{
	if (ItemSystem::GetItemType(item) != ITEM_MATERIAL)
		return false;
	return (ItemSystem::GetItemSubType(item) == MATERIAL_DS_REFINE_NORMAL ||
		ItemSystem::GetItemSubType(item) == MATERIAL_DS_REFINE_BLESSED ||
		ItemSystem::GetItemSubType(item) == MATERIAL_DS_REFINE_HOLLY);
}

bool DSManager::DoRefineStrength(entt::entity ch, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (!ecs::PlayerRuntime::IsValid(ch) || !DragonSoulSystem::CanRefine(ch))
		return false;

	std::set<entt::entity> items;
	for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; ++i)
	{
		const entt::entity item = ItemSystem::GetItem(ch, aItemPoses[i]);
		if (item != entt::null)
			items.insert(item);
	}
	if (items.empty())
		return false;

	entt::entity refineStone = entt::null;
	entt::entity dragonSoul = entt::null;
	for (const entt::entity item : items)
	{
		if (ItemSystem::IsItemEquipped(item))
			return false;

		if (ItemSystem::IsDragonSoulItem(item))
		{
			if (dragonSoul != entt::null)
			{
				SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL, DragonSoulItemPosition(item));
				return false;
			}
			dragonSoul = item;
		}
		else if (IsDragonSoulRefineMaterial(item))
		{
			if (refineStone != entt::null)
			{
				SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL, DragonSoulItemPosition(item));
				return false;
			}
			refineStone = item;
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 628, "");
#endif
			SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, DragonSoulItemPosition(item));
			return false;
		}
	}

	if (dragonSoul == entt::null || refineStone == entt::null)
	{
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL, NPOS);
		return false;
	}

	uint8_t type, grade, step, strength;
	GetDragonSoulInfo(ItemSystem::GetItemVnum(dragonSoul), type, grade, step, strength);

	float nextWeight = 0.f;
	if (!m_pTable->GetWeight(type, grade, step, strength + 1, nextWeight) || nextWeight < FLT_EPSILON)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 627, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_MAX_REFINE, DragonSoulItemPosition(dragonSoul));
		return false;
	}

	int fee = 0;
	float probability = 0.f;
	if (!m_pTable->GetRefineStrengthValues(type, ItemSystem::GetItemSubType(refineStone), strength, fee, probability))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 627, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL, DragonSoulItemPosition(dragonSoul));
		return false;
	}

	if (ecs::PointSystem::GetGold(ch) < fee)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 232, "");
#endif
		SendRefineResultPacket(ch, DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY, NPOS);
		return false;
	}

	const bool success = fnumber(0.f, 100.f) <= probability;
	entt::entity result = entt::null;
	if (success || strength != 0)
	{
		const uint8_t resultStrength = success ? strength + 1 : strength - 1;
		const uint32_t resultVnum = MakeDragonSoulVnum(type, grade, step, resultStrength);
		result = ITEM_MANAGER::instance().CreateItem(resultVnum);
		if (result == entt::null)
		{
			LOG_ERROR("INVALID DRAGON SOUL({})", resultVnum);
			return false;
		}
		if (!ItemSystem::CopyItemAttributesEcs(dragonSoul, result) || !RefreshItemAttributes(result))
		{
			ItemSystem::DestroyItemEntityEcs(result, "DRAGON_SOUL_REFINE_RESULT_INVALID");
			return false;
		}
	}

	char logHint[128];
	sprintf(logHint, "STRENGTH : %d -> %d", strength,
		success ? static_cast<int>(strength) + 1 : static_cast<int>(strength) - 1);
	LogManager::instance().ItemLogEntity(ch, dragonSoul,
		success ? "DS_STRENGTH_REFINE_SUCCESS" : "DS_STRENGTH_REFINE_FAIL", logHint);

	if (!ItemSystem::ConsumeItemEcs(dragonSoul, 1) || !ItemSystem::ConsumeItemEcs(refineStone, 1))
	{
		if (result != entt::null)
			ItemSystem::DestroyItemEntityEcs(result, "DRAGON_SOUL_REFINE_INPUT_INVALID");
		return false;
	}

	ecs::PointSystem::Change(ch, POINT_GOLD, -fee);
	if (result != entt::null)
		ItemSystem::AutoGiveItem(ch, result, true);

#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, success ? 629 : 630, "");
#endif
	SendRefineResultPacket(ch,
		success ? DS_SUB_HEADER_REFINE_SUCCEED : DS_SUB_HEADER_REFINE_FAIL,
		DragonSoulItemPosition(result));
	return true;
}
bool DSManager::DoRefineStrengthEcs(entt::entity owner, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])
{
	if (!ecs::PlayerRuntime::IsValid(owner))
		return false;

	const bool result = DoRefineStrength(owner, aItemPoses);
	SyncDragonSoulGridItems(owner, aItemPoses);
	return result;
}


#ifdef ENABLE_DS_REFINE_ALL
void DSManager::DoRefineAll(entt::entity ch, uint8_t subheader, uint8_t type, uint8_t requestedGrade)
{
	if (!ecs::PlayerRuntime::IsValid(ch) || (subheader != DS_SUB_HEADER_DO_REFINE_GRADE && subheader != DS_SUB_HEADER_DO_REFINE_STEP))
		return;
	if (type > 5 || requestedGrade > 5)
		return;
	if (subheader == DS_SUB_HEADER_DO_REFINE_GRADE && requestedGrade == 5)
		return;
	if (!DragonSoulSystem::CanRefine(ch))
		return;

#ifdef ENABLE_SPAM_CHECK
	const int32_t remainingDelay = DragonSoulSystem::GetLastRefineTime(ch) - get_global_time();
	if (remainingDelay > 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 234, "%d", remainingDelay);
#endif
		return;
	}
	DragonSoulSystem::SetLastRefineTime(ch);
#endif

	const entt::entity owner = ch;
	const int32_t firstCell = 300 + (192 * type) + (requestedGrade * DRAGON_SOUL_BOX_SIZE);
	const bool gradeMode = subheader == DS_SUB_HEADER_DO_REFINE_GRADE;
	const int firstIndex = gradeMode ? DRAGON_SOUL_GRADE_NORMAL : DRAGON_SOUL_STEP_LOWEST;
	const int lastIndex = gradeMode
#ifdef ENABLE_DS_GRADE_MYTH
		? DRAGON_SOUL_GRADE_LEGENDARY
#else
		? DRAGON_SOUL_GRADE_ANCIENT
#endif
		: DRAGON_SOUL_STEP_HIGH;

	for (int refineIndex = firstIndex; refineIndex <= lastIndex; ++refineIndex)
	{
		std::set<entt::entity> items;
		for (int32_t i = 0; i < DRAGON_SOUL_BOX_SIZE; ++i)
		{
			const entt::entity item = ItemSystem::GetItem(
				owner, TItemPos(DRAGON_SOUL_INVENTORY, i + firstCell));
			if (!ItemSystem::IsDragonSoulItem(item) || ItemSystem::IsItemEquipped(item))
				continue;

			const uint32_t vnum = ItemSystem::GetItemVnum(item);
			const int itemIndex = gradeMode ? GetGradeIdx(vnum) : GetStepIdx(vnum);
			if (itemIndex == refineIndex)
				items.insert(item);
		}

		if (items.size() < 2)
			continue;

		entt::entity previous = entt::null;
		for (const entt::entity current : items)
		{
			if (previous == entt::null)
			{
				previous = current;
				continue;
			}

			uint8_t dsType, grade, step, strength;
			GetDragonSoulInfo(ItemSystem::GetItemVnum(current), dsType, grade, step, strength);

			int neededCount = 0;
			int fee = 0;
			std::vector<float> probabilities;
			const bool tableValid = gradeMode
				? m_pTable->GetRefineGradeValues(dsType, grade, neededCount, fee, probabilities)
				: m_pTable->GetRefineStepValues(dsType, step, neededCount, fee, probabilities);
			if (!tableValid)
			{
				previous = entt::null;
				continue;
			}
			if (neededCount != 2)
				return;

			const uint32_t previousVnum = ItemSystem::GetItemVnum(previous);
			const bool pairValid = dsType == GetType(previousVnum) && grade == GetGradeIdx(previousVnum) &&
				(gradeMode || step == GetStepIdx(previousVnum));
			if (!pairValid)
			{
				previous = entt::null;
				continue;
			}

			if (ecs::PointSystem::GetGold(owner) < fee)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 232, "");
#endif
				return;
			}

			const int resultIndex = Gamble(probabilities);
			if (resultIndex < 0)
			{
				previous = entt::null;
				continue;
			}

			const uint32_t resultVnum = gradeMode
				? MakeDragonSoulVnum(dsType, static_cast<uint8_t>(resultIndex), 0, 0)
				: MakeDragonSoulVnum(dsType, grade, static_cast<uint8_t>(resultIndex), 0);
			const entt::entity result = ITEM_MANAGER::instance().CreateItem(resultVnum);
			if (result == entt::null)
			{
				LOG_ERROR("INVALID DRAGON SOUL({})", resultVnum);
				previous = entt::null;
				continue;
			}

			if (!ItemSystem::ConsumeItemEcs(previous, 1) || !ItemSystem::ConsumeItemEcs(current, 1))
			{
				ItemSystem::DestroyItemEntityEcs(result, "DRAGON_SOUL_REFINE_INPUT_INVALID");
				return;
			}

			ecs::PointSystem::Change(owner, POINT_GOLD, -fee);
			if (ItemSystem::AutoGiveDS(owner, result, true))
			{
				char logHint[128];
				if (gradeMode)
					sprintf(logHint, "GRADE : %d -> %d", grade, resultIndex);
				else
					sprintf(logHint, "STEP : %d -> %d", step, resultIndex);

				const bool success = gradeMode ? resultIndex > grade : resultIndex > step;
				LogManager::instance().ItemLogEntity(ch, result,
					gradeMode
						? (success ? "DS_GRADE_REFINE_SUCCESS" : "DS_GRADE_REFINE_FAIL")
						: (success ? "DS_STEP_REFINE_SUCCESS" : "DS_STEP_REFINE_FAIL"),
					logHint);
			}

			previous = entt::null;
		}
	}
}

void DSManager::DoRefineAllEcs(entt::entity owner, uint8_t subheader, uint8_t type, uint8_t grade)
{
	if (!ecs::PlayerRuntime::IsValid(owner))
		return;

	DoRefineAll(owner, subheader, type, grade);
	for (int i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i)
		SyncDragonSoulItemPtr(ItemSystem::GetItem(owner, TItemPos(DRAGON_SOUL_INVENTORY, i)));
}

#endif
void DSManager::SendRefineResultPacket(entt::entity ch, uint8_t bSubHeader, const TItemPos& pos)
{
	TPacketGCDragonSoulRefine pack;
	pack.bSubType = bSubHeader;

	if (pos.IsValidItemPosition())
	{
		pack.Pos = pos;
	}
	LPDESC d = ecs::PlayerRuntime::GetDesc(ch);
	if (nullptr == d)
	{
		return ;
	}
	else
	{
		d->Packet(&pack, sizeof(pack));
	}
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


bool DSManager::IsActiveDragonSoul(entt::entity item) const
{
	return ItemSystem::GetItemSocket(item, ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX) != 0;
}


bool DSManager::ActivateDragonSoul(entt::entity item)
{
	if (!ItemSystem::IsDragonSoulItem(item))
		return false;

	const entt::entity owner = ItemSystem::GetItemOwnerEntity(item);
	LPCHARACTER ch = ecs::LegacyCharOf(owner);
	if (!ch)
		return false;

	const int deck = DragonSoulSystem::GetActiveDeck(owner);
	if (deck < 0)
		return false;

	const uint16_t cell = ItemSystem::GetItemCell(item);
	if (cell < DRAGON_SOUL_EQUIP_SLOT_START + DS_SLOT_MAX * deck ||
		cell >= DRAGON_SOUL_EQUIP_SLOT_START + DS_SLOT_MAX * (deck + 1))
		return false;

	if (IsTimeLeftDragonSoul(item) && !IsActiveDragonSoul(item))
	{
		char logHint[128];
		sprintf(logHint, "LEFT TIME(%d)", LeftTime(item));
		LogManager::instance().ItemLogEntity(ch, item, "DS_ACTIVATE", logHint);
		if (!ItemSystem::ModifyItemPointsEcs(item, true))
			return false;
		ItemSystem::SetItemSocketEcs(item, ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX, 1);
		ItemSystem::StartTimerBasedOnWearExpireEventEcs(item);
	}
	return true;
}
bool DSManager::ActivateDragonSoulEcs(entt::entity item)
{
	const bool result = ActivateDragonSoul(item);
	SyncDragonSoulItemEntity(item);
	return result;
}


bool DSManager::DeactivateDragonSoul(entt::entity item, bool bSkipRefreshOwnerActiveState)
{
	if (!ItemSystem::IsDragonSoulItem(item))
		return false;

	const entt::entity owner = ItemSystem::GetItemOwnerEntity(item);
	LPCHARACTER ch = ecs::LegacyCharOf(owner);
	if (!ch || !IsActiveDragonSoul(item))
		return false;

	ItemSystem::StopTimerBasedOnWearExpireEventEcs(item);
	ItemSystem::SetItemSocketEcs(item, ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX, 0);
	ItemSystem::ModifyItemPointsEcs(item, false);

	char logHint[128];
	sprintf(logHint, "LEFT TIME(%d)", LeftTime(item));
	LogManager::instance().ItemLogEntity(ch, item, "DS_DEACTIVATE", logHint);

	if (!bSkipRefreshOwnerActiveState)
		RefreshDragonSoulState(ch);
	return true;
}
bool DSManager::DeactivateDragonSoulEcs(entt::entity item, bool bSkipRefreshOwnerActiveState)
{
	const bool result = DeactivateDragonSoul(item, bSkipRefreshOwnerActiveState);
	SyncDragonSoulItemEntity(item);
	return result;
}


void DSManager::RefreshDragonSoulState(LPCHARACTER ch)
{
	if (!ch)
		return;

	const entt::entity owner = ((ch) ? (ch)->GetEntityHandle() : entt::null);
	for (int i = WEAR_MAX_NUM; i < WEAR_MAX_NUM + DS_SLOT_MAX * DRAGON_SOUL_DECK_MAX_NUM; ++i)
	{
		const entt::entity item = ItemSystem::GetWearItem(owner, i);
		if (item != entt::null && IsActiveDragonSoul(item))
			return;
	}
	DragonSoulSystem::DeactivateAll(owner);
}
DSManager::DSManager() = default;
DSManager::~DSManager() = default;
