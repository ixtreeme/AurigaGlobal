#include "stdafx.h"
#include "ecs/systems/InventorySystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PointSystem.hpp"
#include <Base/grid.h>
#include "utils.h"
#include "desc.h"
#include "desc_client.h"
#include "char_interface.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "item.h"
#include "item_manager.h"
#include "packet.h"
#include "log.h"
#include "db.h"
#include "locale_service.h"
#include <common/length.h>
#include "exchange.h"
#include "DragonSoul.h"
#include "questmanager.h" // @fixme150
#include "ecs/CharacterAccessors.hpp"

namespace
{
LPITEM ResolveLegacyExchangeItem(entt::entity item)
{
	if (!ItemSystem::IsValidItem(item))
		return nullptr;

	return ITEM_MANAGER::instance().Find(ItemSystem::GetItemID(item));
}
}

void exchange_packet(LPCHARACTER ch, uint8_t sub_header, bool is_me, int64_t arg1, TItemPos arg2, uint32_t arg3, entt::entity item = entt::null);
void exchange_packet(LPCHARACTER ch, uint8_t sub_header, bool is_me, int64_t arg1, TItemPos arg2, uint32_t arg3, entt::entity item)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	if (!ecs::PlayerRuntime::GetDesc(chEntity))
		return;

	struct packet_exchange pack_exchg;

	pack_exchg.header = HEADER_GC_EXCHANGE;
	pack_exchg.sub_header = sub_header;
	pack_exchg.is_me = is_me;
	pack_exchg.arg1 = arg1;
	pack_exchg.arg2 = arg2;
	pack_exchg.arg3 = arg3;

	if (sub_header == EXCHANGE_SUBHEADER_GC_ITEM_ADD && ItemSystem::IsValidItem(item))
	{
#ifdef WJ_ENABLE_TRADABLE_ICON
		pack_exchg.arg4 = TItemPos(ItemSystem::GetItemWindow(item), ItemSystem::GetItemCell(item));
#endif
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			pack_exchg.alSockets[i] = ItemSystem::GetItemSocket(item, i);

		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			pack_exchg.aAttr[i] = ItemSystem::GetItemAttribute(item, i);
#ifdef ATTR_LOCK
		pack_exchg.lockedattr = ItemSystem::GetItemLockedAttributeIndex(item);
#endif
	}
	else
	{
#ifdef WJ_ENABLE_TRADABLE_ICON
		pack_exchg.arg4 = TItemPos(RESERVED_WINDOW, 0);
#endif
		memset(&pack_exchg.alSockets, 0, sizeof(pack_exchg.alSockets));
		memset(&pack_exchg.aAttr, 0, sizeof(pack_exchg.aAttr));
#ifdef ATTR_LOCK
		pack_exchg.lockedattr = -1;
#endif
	}

	ecs::PlayerRuntime::GetDesc(chEntity)->Packet(&pack_exchg, sizeof(pack_exchg));
}

bool CHARACTER::ExchangeStart(entt::entity victimEntity)
{
	LPCHARACTER victim = ecs::LegacyCharOf(victimEntity);
	const entt::entity thisEntity = this ? this->GetEntityHandle() : entt::null;
	if (this == victim)	// 자기 자신과는 교환을 못한다.
		return false;

	if (IsObserverMode())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(thisEntity, CHAT_TYPE_INFO, 256, "");
#endif
		return false;
	}

	if (ecs::PlayerRuntime::IsNPC(victimEntity))
		return false;

#ifdef ENABLE_PVP_ADVANCED
	if ((GetDuel("BlockExchange")))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(thisEntity, CHAT_TYPE_INFO, 516, "");
#endif
		return false;
	}

	if ((victim->GetDuel("BlockExchange")))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(thisEntity, CHAT_TYPE_INFO, 517, "%s", ecs::PlayerRuntime::GetName(victimEntity).data());
#endif
		return false;
	}
#endif

	//PREVENT_TRADE_WINDOW
	if ( IsOpenSafebox() || GetShopOwner() || GetMyShop() || IsCubeOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(thisEntity, CHAT_TYPE_INFO, 292, "");
#endif
		return false;
	}

	if ( victim->IsOpenSafebox() || victim->GetShopOwner() || victim->GetMyShop() || victim->IsCubeOpen() )
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(thisEntity, CHAT_TYPE_INFO, 293, "%s", ecs::PlayerRuntime::GetName(victimEntity).data());
#endif
		return false;
	}

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (IsAttrTransferOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(thisEntity, CHAT_TYPE_INFO, 292, "");
#endif
		return false;
	}

	if (victim->IsAttrTransferOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(thisEntity, CHAT_TYPE_INFO, 293, "%s", ecs::PlayerRuntime::GetName(victimEntity).data());
#endif
		return false;
	}
#endif
	//END_PREVENT_TRADE_WINDOW
	int iDist = DISTANCE_APPROX(GetX() - ecs::PlayerRuntime::GetX(victimEntity), GetY() - ecs::PlayerRuntime::GetY(victimEntity));

	// 거리 체크
	if (iDist >= EXCHANGE_MAX_DISTANCE)
		return false;

	if (GetExchange())
		return false;

	if (ecs::SocialSystem::GetExchange(victimEntity))
	{
		exchange_packet(this, EXCHANGE_SUBHEADER_GC_ALREADY, 0, 0, NPOS, 0);
		return false;
	}

	if (victim->IsBlockMode(BLOCK_EXCHANGE))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(thisEntity, CHAT_TYPE_INFO, 368, "%s", ecs::PlayerRuntime::GetName(victimEntity).data());
#endif
		return false;
	}

	SetExchange(M2_NEW CExchange(this));
	victim->SetExchange(M2_NEW CExchange(victim));

	ecs::SocialSystem::GetExchange(victimEntity)->SetCompany(GetExchange());
	GetExchange()->SetCompany(ecs::SocialSystem::GetExchange(victimEntity));

	//
	SetExchangeTime();
	victim->SetExchangeTime();

	exchange_packet(victim, EXCHANGE_SUBHEADER_GC_START, 0, GetPacketVID(), NPOS, 0);
	exchange_packet(this, EXCHANGE_SUBHEADER_GC_START, 0, ecs::PlayerRuntime::GetPacketVID(victimEntity), NPOS, 0);

	return true;
}

CExchange::CExchange(LPCHARACTER pOwner)
{
	m_pCompany = nullptr;
	m_bAccept = false;
	m_items.fill(entt::null);

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		m_aItemPos[i] = NPOS;
		m_abItemDisplayPos[i] = 0;
	}

	m_lGold = 0;
	m_pOwner = pOwner;
	pOwner->SetExchange(this);

#ifdef __NEW_EXCHANGE_WINDOW__
	m_pGrid = M2_NEW CGrid(6, 4);
#else
	m_pGrid = M2_NEW CGrid(4, 3);
#endif
}

CExchange::~CExchange()
{
	M2_DELETE(m_pGrid);
}

bool CExchange::AddItem(TItemPos item_pos, uint8_t display_pos)
{
	const entt::entity pOwner = m_pOwner ? m_pOwner->GetEntityHandle() : entt::null;
	assert(m_pOwner != NULL && GetCompany());

	if (!item_pos.IsValidItemPosition() || item_pos.IsEquipPosition())
		return false;

	const entt::entity item = ItemSystem::GetItem(pOwner, item_pos);
	if (!ItemSystem::IsValidItem(item))
		return false;

	if (IS_SET(ItemSystem::GetItemAntiFlag(item), ITEM_ANTIFLAG_GIVE))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(pOwner, CHAT_TYPE_INFO, 402, "%s", ItemSystem::GetItemName(item));
#endif
		return false;
	}

	if (ItemSystem::IsItemLocked(item))
		return false;

	if (ItemSystem::IsItemExchanging(item))
	{
		LOG_INFO("EXCHANGE under exchanging");
		return false;
	}

	const uint8_t itemSize = ItemSystem::GetItemSize(item);
	if (!m_pGrid->IsEmpty(display_pos, 1, itemSize))
	{
		LOG_INFO("EXCHANGE not empty item_pos {} {} {}", display_pos, 1, itemSize);
		return false;
	}

	Accept(false);
	GetCompany()->Accept(false);

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (m_items[i] != entt::null && ItemSystem::IsValidItem(m_items[i]))
			continue;

		m_items[i] = item;
		m_aItemPos[i] = item_pos;
		m_abItemDisplayPos[i] = display_pos;
		m_pGrid->Put(display_pos, 1, itemSize);
		ItemSystem::SetItemExchanging(item, true);

		exchange_packet(m_pOwner,
				EXCHANGE_SUBHEADER_GC_ITEM_ADD,
				true,
				ItemSystem::GetItemVnum(item),
				TItemPos(RESERVED_WINDOW, display_pos),
				ItemSystem::GetItemCount(item),
				item);

		exchange_packet(GetCompany()->GetOwner(),
				EXCHANGE_SUBHEADER_GC_ITEM_ADD,
				false,
				ItemSystem::GetItemVnum(item),
				TItemPos(RESERVED_WINDOW, display_pos),
				ItemSystem::GetItemCount(item),
				item);

		LOG_INFO("EXCHANGE AddItem success {} pos({}, {}) {}", ItemSystem::GetItemName(item), item_pos.window_type, item_pos.cell, display_pos);
		return true;
	}

	return false;
}

bool CExchange::RemoveItem(uint8_t pos)
{
	if (pos >= EXCHANGE_ITEM_MAX_NUM)
		return false;

	const entt::entity item = m_items[pos];
	if (!ItemSystem::IsValidItem(item))
	{
		m_items[pos] = entt::null;
		m_aItemPos[pos] = NPOS;
		m_abItemDisplayPos[pos] = 0;
		return false;
	}

	const TItemPos inventoryPos = m_aItemPos[pos];
	ItemSystem::SetItemExchanging(item, false);
	m_pGrid->Get(m_abItemDisplayPos[pos], 1, ItemSystem::GetItemSize(item));

	exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_ITEM_DEL, true, pos, NPOS, 0);
	exchange_packet(GetCompany()->GetOwner(), EXCHANGE_SUBHEADER_GC_ITEM_DEL, false, pos, inventoryPos, 0);

	Accept(false);
	GetCompany()->Accept(false);

	m_items[pos] = entt::null;
	m_aItemPos[pos] = NPOS;
	m_abItemDisplayPos[pos] = 0;
	return true;
}

bool CExchange::AddGold(int64_t gold)
{
	if (gold <= 0)
		return false;

	if (ecs::PointSystem::GetGold(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null)) < gold)
	{
		// 가지고 있는 돈이 부족.
		exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_LESS_GOLD, 0, 0, NPOS, 0);
		return false;
	}

	if (m_lGold > 0)
		return false;

	Accept(false);
	GetCompany()->Accept(false);

	m_lGold = gold;

	exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_GOLD_ADD, true, m_lGold, NPOS, 0);
	exchange_packet(GetCompany()->GetOwner(), EXCHANGE_SUBHEADER_GC_GOLD_ADD, false, m_lGold, NPOS, 0);
	return true;
}

// 돈이 충분히 있는지, 교환하려는 아이템이 실제로 있는지 확인 한다.
bool CExchange::Check(int * piItemCount)
{
	if (ecs::PointSystem::GetGold(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null)) < m_lGold)
		return false;

	int item_count = 0;
	const entt::entity owner = ((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null);

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		const entt::entity item = m_items[i];
		if (item == entt::null)
			continue;

		if (!ItemSystem::IsValidItem(item) || !m_aItemPos[i].IsValidItemPosition())
			return false;

		if (item != ItemSystem::GetItem(owner, m_aItemPos[i]))
			return false;

		++item_count;
	}

	*piItemCount = item_count;
	return true;
}

bool CExchange::CheckSpace()
{
	CGrid * s_grid1 = new CGrid(5, 9);
	CGrid * s_grid2 = new CGrid(5, 9);
#ifdef ENABLE_EXTEND_INVEN_SYSTEM
	CGrid * s_grid3;
	CGrid * s_grid4;
#endif

	LPCHARACTER victim = GetCompany()->GetOwner();
	const entt::entity victimEntity = victim ? victim->GetEntityHandle() : entt::null;

	LPITEM item;
	entt::entity occupiedItem = entt::null;
	int i;
#ifdef ENABLE_EXTEND_INVEN_SYSTEM
	int gridsize = victim->Inven_Point();
#endif
	int INVEN_NUM_SLOT = 45;

#ifdef ENABLE_EXTEND_INVEN_SYSTEM
	if (gridsize >= 9) {
		gridsize -= 9;
		s_grid3 = new CGrid(5, 9);
		s_grid4 = new CGrid(5, gridsize);
	}
	else {
		s_grid3 = new CGrid(5, gridsize);
		s_grid4 = new CGrid(5, 0);
	}
#endif

	s_grid1->Clear();
	s_grid2->Clear();
#ifdef ENABLE_EXTEND_INVEN_SYSTEM
	s_grid3->Clear();
	s_grid4->Clear();
#endif

	for (i = 0; i < INVEN_NUM_SLOT; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetInventoryItem(victimEntity, i)))
			continue;

		s_grid1->Put(i, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = INVEN_NUM_SLOT; i < INVEN_NUM_SLOT * 2; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetInventoryItem(victimEntity, i)))
			continue;

		s_grid2->Put(i - INVEN_NUM_SLOT, 1, ItemSystem::GetItemSize(occupiedItem));
	}

#ifdef ENABLE_EXTEND_INVEN_SYSTEM
	for (i = INVEN_NUM_SLOT * 2; i < INVEN_NUM_SLOT * 3; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetInventoryItem(victimEntity, i)))
			continue;

		s_grid3->Put(i - INVEN_NUM_SLOT * 2, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = INVEN_NUM_SLOT * 3; i < INVEN_NUM_SLOT * 4; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetInventoryItem(victimEntity, i)))
			continue;

		s_grid4->Put(i - INVEN_NUM_SLOT * 3, 1, ItemSystem::GetItemSize(occupiedItem));
	}
#endif

#ifdef ENABLE_EXTRA_INVENTORY
	CGrid * s_gridExtraCat1_1 = new CGrid(5, 9);
	CGrid * s_gridExtraCat1_2 = new CGrid(5, 9);
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	CGrid * s_gridExtraCat1_3;
	CGrid * s_gridExtraCat1_4;

	int gridextra_size_cat1 = ecs::PointSystem::Get(victimEntity, POINT_EXTRA_INVENTORY1) + 4;
	if (gridextra_size_cat1 >= 9) {
		gridextra_size_cat1 -= 9;
		s_gridExtraCat1_3 = new CGrid(5, 9);
		s_gridExtraCat1_4 = new CGrid(5, gridextra_size_cat1);
	}
	else {
		s_gridExtraCat1_3 = new CGrid(5, gridextra_size_cat1);
		s_gridExtraCat1_4 = new CGrid(5, 0);
	}
#else
	CGrid * s_gridExtraCat1_3 = new CGrid(5, 9);
	CGrid * s_gridExtraCat1_4 = new CGrid(5, 9);
#endif

	s_gridExtraCat1_1->Clear();
	s_gridExtraCat1_2->Clear();
	s_gridExtraCat1_3->Clear();
	s_gridExtraCat1_4->Clear();

	for (i = 0; i < EXTRA_INVENTORY_PAGE_SIZE * 1; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat1_1->Put(i, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 1; i < EXTRA_INVENTORY_PAGE_SIZE * 2; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat1_2->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 1, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 2; i < EXTRA_INVENTORY_PAGE_SIZE * 3; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat1_3->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 2, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 3; i < EXTRA_INVENTORY_PAGE_SIZE * 4; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat1_4->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 3, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	CGrid * s_gridExtraCat2_1 = new CGrid(5, 9);
	CGrid * s_gridExtraCat2_2 = new CGrid(5, 9);
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	CGrid * s_gridExtraCat2_3;
	CGrid * s_gridExtraCat2_4;

	int gridextra_size_cat2 = ecs::PointSystem::Get(victimEntity, POINT_EXTRA_INVENTORY2) + 4;
	if (gridextra_size_cat2 >= 9) {
		gridextra_size_cat2 -= 9;
		s_gridExtraCat2_3 = new CGrid(5, 9);
		s_gridExtraCat2_4 = new CGrid(5, gridextra_size_cat2);
	}
	else {
		s_gridExtraCat2_3 = new CGrid(5, gridextra_size_cat2);
		s_gridExtraCat2_4 = new CGrid(5, 0);
	}
#else
	CGrid * s_gridExtraCat2_3 = new CGrid(5, 9);
	CGrid * s_gridExtraCat2_4 = new CGrid(5, 9);
#endif

	s_gridExtraCat2_1->Clear();
	s_gridExtraCat2_2->Clear();
	s_gridExtraCat2_3->Clear();
	s_gridExtraCat2_4->Clear();

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 4; i < EXTRA_INVENTORY_PAGE_SIZE * 5; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat2_1->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 4, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 5; i < EXTRA_INVENTORY_PAGE_SIZE * 6; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat2_2->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 5, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 6; i < EXTRA_INVENTORY_PAGE_SIZE * 7; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat2_3->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 6, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 7; i < EXTRA_INVENTORY_PAGE_SIZE * 8; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat2_4->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 7, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	CGrid * s_gridExtraCat3_1 = new CGrid(5, 9);
	CGrid * s_gridExtraCat3_2 = new CGrid(5, 9);
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	CGrid * s_gridExtraCat3_3;
	CGrid * s_gridExtraCat3_4;

	int gridextra_size_cat3 = ecs::PointSystem::Get(victimEntity, POINT_EXTRA_INVENTORY3) + 4;
	if (gridextra_size_cat3 >= 9) {
		gridextra_size_cat3 -= 9;
		s_gridExtraCat3_3 = new CGrid(5, 9);
		s_gridExtraCat3_4 = new CGrid(5, gridextra_size_cat3);
	}
	else {
		s_gridExtraCat3_3 = new CGrid(5, gridextra_size_cat3);
		s_gridExtraCat3_4 = new CGrid(5, 0);
	}
#else
	CGrid * s_gridExtraCat3_3 = new CGrid(5, 9);
	CGrid * s_gridExtraCat3_4 = new CGrid(5, 9);
#endif

	s_gridExtraCat3_1->Clear();
	s_gridExtraCat3_2->Clear();
	s_gridExtraCat3_3->Clear();
	s_gridExtraCat3_4->Clear();

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 8; i < EXTRA_INVENTORY_PAGE_SIZE * 9; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat3_1->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 8, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 9; i < EXTRA_INVENTORY_PAGE_SIZE * 10; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat3_2->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 9, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 10; i < EXTRA_INVENTORY_PAGE_SIZE * 11; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat3_3->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 10, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 11; i < EXTRA_INVENTORY_PAGE_SIZE * 12; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat3_4->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 11, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	CGrid * s_gridExtraCat4_1 = new CGrid(5, 9);
	CGrid * s_gridExtraCat4_2 = new CGrid(5, 9);
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	CGrid * s_gridExtraCat4_3;
	CGrid * s_gridExtraCat4_4;

	int gridextra_size_cat4 = ecs::PointSystem::Get(victimEntity, POINT_EXTRA_INVENTORY4) + 4;
	if (gridextra_size_cat4 >= 9) {
		gridextra_size_cat4 -= 9;
		s_gridExtraCat4_3 = new CGrid(5, 9);
		s_gridExtraCat4_4 = new CGrid(5, gridextra_size_cat4);
	}
	else {
		s_gridExtraCat4_3 = new CGrid(5, gridextra_size_cat4);
		s_gridExtraCat4_4 = new CGrid(5, 0);
	}
#else
	CGrid * s_gridExtraCat4_3 = new CGrid(5, 9);
	CGrid * s_gridExtraCat4_4 = new CGrid(5, 9);
#endif

	s_gridExtraCat4_1->Clear();
	s_gridExtraCat4_2->Clear();
	s_gridExtraCat4_3->Clear();
	s_gridExtraCat4_4->Clear();

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 12; i < EXTRA_INVENTORY_PAGE_SIZE * 13; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat4_1->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 12, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 13; i < EXTRA_INVENTORY_PAGE_SIZE * 14; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat4_2->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 13, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 14; i < EXTRA_INVENTORY_PAGE_SIZE * 15; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat4_3->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 14, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 15; i < EXTRA_INVENTORY_PAGE_SIZE * 16; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat4_4->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 15, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	CGrid* s_gridExtraCat5_1 = new CGrid(5, 9);
	CGrid* s_gridExtraCat5_2 = new CGrid(5, 9);
	CGrid* s_gridExtraCat5_3 = new CGrid(5, 9);
	CGrid* s_gridExtraCat5_4 = new CGrid(5, 9);

	s_gridExtraCat5_1->Clear();
	s_gridExtraCat5_2->Clear();
	s_gridExtraCat5_3->Clear();
	s_gridExtraCat5_4->Clear();

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 16; i < EXTRA_INVENTORY_PAGE_SIZE * 17; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat5_1->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 16, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 17; i < EXTRA_INVENTORY_PAGE_SIZE * 18; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat5_2->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 17, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 18; i < EXTRA_INVENTORY_PAGE_SIZE * 19; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat5_3->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 18, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 19; i < EXTRA_INVENTORY_PAGE_SIZE * 20; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat5_4->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 19, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	CGrid* s_gridExtraCat6_1 = new CGrid(5, 9);
	CGrid* s_gridExtraCat6_2 = new CGrid(5, 9);
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	CGrid* s_gridExtraCat6_3;
	CGrid* s_gridExtraCat6_4;

	int gridextra_size_cat6 = ecs::PointSystem::Get(victimEntity, POINT_EXTRA_INVENTORY6) + 4;
	if (gridextra_size_cat6 >= 9) {
		gridextra_size_cat6 -= 9;
		s_gridExtraCat6_3 = new CGrid(5, 9);
		s_gridExtraCat6_4 = new CGrid(5, gridextra_size_cat6);
	}
	else {
		s_gridExtraCat6_3 = new CGrid(5, gridextra_size_cat6);
		s_gridExtraCat6_4 = new CGrid(5, 0);
	}
#else
	CGrid* s_gridExtraCat6_3 = new CGrid(5, 9);
	CGrid* s_gridExtraCat6_4 = new CGrid(5, 9);
#endif

	s_gridExtraCat6_1->Clear();
	s_gridExtraCat6_2->Clear();
	s_gridExtraCat6_3->Clear();
	s_gridExtraCat6_4->Clear();

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 20; i < EXTRA_INVENTORY_PAGE_SIZE * 21; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat6_1->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 20, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 21; i < EXTRA_INVENTORY_PAGE_SIZE * 22; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat6_2->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 21, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 22; i < EXTRA_INVENTORY_PAGE_SIZE * 23; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat6_3->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 22, 1, ItemSystem::GetItemSize(occupiedItem));
	}

	for (i = EXTRA_INVENTORY_PAGE_SIZE * 23; i < EXTRA_INVENTORY_PAGE_SIZE * 24; ++i) {
		if (!ItemSystem::IsValidItem(occupiedItem = ItemSystem::GetExtraInventoryItem(victimEntity, i)))
			continue;

		s_gridExtraCat6_4->Put(i - EXTRA_INVENTORY_PAGE_SIZE * 23, 1, ItemSystem::GetItemSize(occupiedItem));
	}
#endif

	static std::vector <uint16_t> s_vDSGrid(DRAGON_SOUL_INVENTORY_MAX_NUM);
	bool bDSInitialized = false;

	for (i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i) {
		const entt::entity itemEntity = m_items[i];
		if (itemEntity == entt::null)
			continue;
		if (!ItemSystem::IsValidItem(itemEntity) || !(item = ResolveLegacyExchangeItem(itemEntity)))
			return false;

		if (item->IsDragonSoul()) {
			if (!bDSInitialized) {
				bDSInitialized = true;
				victim->CopyDragonSoulItemGrid(s_vDSGrid);
			}

			bool bExistEmptySpace = false;
			uint16_t wBasePos = DSManager::instance().GetBasePosition(itemEntity);
			if (wBasePos >= DRAGON_SOUL_INVENTORY_MAX_NUM)
				return false;

			for (int i = 0; i < DRAGON_SOUL_BOX_SIZE; i++) {
				uint16_t wPos = wBasePos + i;
				if (0 == s_vDSGrid[wPos])
				{
					bool bEmpty = true;
					for (int j = 1; j < item->GetSize(); j++) {
						if (s_vDSGrid[wPos + j * DRAGON_SOUL_BOX_COLUMN_NUM]) {
							bEmpty = false;
							break;
						}
					}

					if (bEmpty) {
						for (int j = 0; j < item->GetSize(); j++)
							s_vDSGrid[wPos + j * DRAGON_SOUL_BOX_COLUMN_NUM] =  wPos + 1;

						bExistEmptySpace = true;
						break;
					}
				}

				if (bExistEmptySpace)
					break;
			}
			if (!bExistEmptySpace)
				return false;
		}
#ifdef ENABLE_EXTRA_INVENTORY
		else if (item->IsExtraItem())
		{
			uint8_t category = item->GetExtraCategory();
			if (category == 0) {
				int iPos = s_gridExtraCat1_1->FindBlank(1, item->GetSize());
				if (iPos >= 0) {
					s_gridExtraCat1_1->Put(iPos, 1, item->GetSize());
				}
				else {
					iPos = s_gridExtraCat1_2->FindBlank(1, item->GetSize());
					if (iPos >= 0) {
						s_gridExtraCat1_2->Put(iPos, 1, item->GetSize());
					}
					else {
						iPos = s_gridExtraCat1_3->FindBlank(1, item->GetSize());
						if (iPos >= 0) {
							s_gridExtraCat1_3->Put(iPos, 1, item->GetSize());
						}
						else {
							iPos = s_gridExtraCat1_4->FindBlank(1, item->GetSize());
							if (iPos >= 0) {
								s_gridExtraCat1_4->Put(iPos, 1, item->GetSize());
							}
							else
								return false;
						}
					}
				}
			}
			else if (category == 1) {
				int iPos = s_gridExtraCat2_1->FindBlank(1, item->GetSize());
				if (iPos >= 0) {
					s_gridExtraCat2_1->Put(iPos, 1, item->GetSize());
				}
				else {
					iPos = s_gridExtraCat2_2->FindBlank(1, item->GetSize());
					if (iPos >= 0) {
						s_gridExtraCat2_2->Put(iPos, 1, item->GetSize());
					}
					else {
						iPos = s_gridExtraCat2_3->FindBlank(1, item->GetSize());
						if (iPos >= 0) {
							s_gridExtraCat2_3->Put(iPos, 1, item->GetSize());
						}
						else {
							iPos = s_gridExtraCat2_4->FindBlank(1, item->GetSize());
							if (iPos >= 0) {
								s_gridExtraCat2_4->Put(iPos, 1, item->GetSize());
							}
							else
								return false;
						}
					}
				}
			}
			else if (category == 2) {
				int iPos = s_gridExtraCat3_1->FindBlank(1, item->GetSize());
				if (iPos >= 0) {
					s_gridExtraCat3_1->Put(iPos, 1, item->GetSize());
				}
				else {
					iPos = s_gridExtraCat3_2->FindBlank(1, item->GetSize());
					if (iPos >= 0) {
						s_gridExtraCat3_2->Put(iPos, 1, item->GetSize());
					}
					else {
						iPos = s_gridExtraCat3_3->FindBlank(1, item->GetSize());
						if (iPos >= 0) {
							s_gridExtraCat3_3->Put(iPos, 1, item->GetSize());
						}
						else {
							iPos = s_gridExtraCat3_4->FindBlank(1, item->GetSize());
							if (iPos >= 0) {
								s_gridExtraCat3_4->Put(iPos, 1, item->GetSize());
							}
							else
								return false;
						}
					}
				}
			}
			else if (category == 3) {
				int iPos = s_gridExtraCat4_1->FindBlank(1, item->GetSize());
				if (iPos >= 0) {
					s_gridExtraCat4_1->Put(iPos, 1, item->GetSize());
				}
				else {
					iPos = s_gridExtraCat4_2->FindBlank(1, item->GetSize());
					if (iPos >= 0) {
						s_gridExtraCat4_2->Put(iPos, 1, item->GetSize());
					}
					else {
						iPos = s_gridExtraCat4_3->FindBlank(1, item->GetSize());
						if (iPos >= 0) {
							s_gridExtraCat4_3->Put(iPos, 1, item->GetSize());
						}
						else {
							iPos = s_gridExtraCat4_4->FindBlank(1, item->GetSize());
							if (iPos >= 0) {
								s_gridExtraCat4_4->Put(iPos, 1, item->GetSize());
							}
							else
								return false;
						}
					}
				}
			}
			else if (category == 4) {
				int iPos = s_gridExtraCat5_1->FindBlank(1, item->GetSize());
				if (iPos >= 0) {
					s_gridExtraCat5_1->Put(iPos, 1, item->GetSize());
				}
				else {
					iPos = s_gridExtraCat5_2->FindBlank(1, item->GetSize());
					if (iPos >= 0) {
						s_gridExtraCat5_2->Put(iPos, 1, item->GetSize());
					}
					else {
						iPos = s_gridExtraCat5_3->FindBlank(1, item->GetSize());
						if (iPos >= 0) {
							s_gridExtraCat5_3->Put(iPos, 1, item->GetSize());
						}
						else {
							iPos = s_gridExtraCat5_4->FindBlank(1, item->GetSize());
							if (iPos >= 0) {
								s_gridExtraCat5_4->Put(iPos, 1, item->GetSize());
							}
							else
								return false;
						}
					}
				}
				}
			else if (category == 5) {
					int iPos = s_gridExtraCat6_1->FindBlank(1, item->GetSize());
					if (iPos >= 0) {
						s_gridExtraCat6_1->Put(iPos, 1, item->GetSize());
					}
					else {
						iPos = s_gridExtraCat6_2->FindBlank(1, item->GetSize());
						if (iPos >= 0) {
							s_gridExtraCat6_2->Put(iPos, 1, item->GetSize());
						}
						else {
							iPos = s_gridExtraCat6_3->FindBlank(1, item->GetSize());
							if (iPos >= 0) {
								s_gridExtraCat6_3->Put(iPos, 1, item->GetSize());
							}
							else {
								iPos = s_gridExtraCat6_4->FindBlank(1, item->GetSize());
								if (iPos >= 0) {
									s_gridExtraCat6_4->Put(iPos, 1, item->GetSize());
								}
								else
									return false;
							}
						}
					}
					}
		}
#endif
		else
		{
			int iPos = s_grid1->FindBlank(1, item->GetSize());
			if (iPos >= 0) {
				s_grid1->Put(iPos, 1, item->GetSize());
			}
			else {
				iPos = s_grid2->FindBlank(1, item->GetSize());
				if (iPos >= 0) {
					s_grid2->Put(iPos, 1, item->GetSize());
				}
#ifdef ENABLE_EXTEND_INVEN_SYSTEM
				else {
					iPos = s_grid3->FindBlank(1, item->GetSize());
					if (iPos >= 0) {
						s_grid3->Put(iPos, 1, item->GetSize());
					}
					else {
						iPos = s_grid4->FindBlank(1, item->GetSize());
						if (iPos >= 0) {
							s_grid4->Put(iPos, 1, item->GetSize());
						}
						else
							return false;
					}
				}
#else
				else {
					return false;
				}
#endif
			}
		}
	}

	return true;
}

bool CExchange::Done()
{
	int		empty_pos, i;
	LPITEM	item;

	LPCHARACTER	victim = GetCompany()->GetOwner();
	const entt::entity victimEntity = victim ? victim->GetEntityHandle() : entt::null;


	for (i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		const entt::entity itemEntity = m_items[i];
		if (itemEntity == entt::null)
			continue;
		if (!ItemSystem::IsValidItem(itemEntity) || !(item = ResolveLegacyExchangeItem(itemEntity)))
		{
			m_items[i] = entt::null;
			continue;
		}

		if (item->IsDragonSoul())
			empty_pos = victim->GetEmptyDragonSoulInventory(item);
#ifdef ENABLE_EXTRA_INVENTORY
		else if (item->IsExtraItem())
			empty_pos = victim->GetEmptyExtraInventory(item);
#endif
		else
			empty_pos = victim->GetEmptyInventory(item->GetSize());

		if (empty_pos < 0)
		{
			LOG_ERROR("Exchange::Done : Cannot find blank position in inventory {} <-> {} item {}", ecs::PlayerRuntime::GetName(((m_pOwner) ? (m_pOwner)->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetName(victimEntity).data(), item->GetName());
			continue;
		}

		assert(empty_pos >= 0);

		if (ItemSystem::GetItemVnum(itemEntity) == 90008 || ItemSystem::GetItemVnum(itemEntity) == 90009) // VCARD
		{
			ItemSystem::SetItemExchanging(itemEntity, false);
			m_items[i] = entt::null;
			VCardUse(m_pOwner ? m_pOwner->GetEntityHandle() : entt::null, victimEntity, item ? item->GetEntityHandle() : entt::null);
			continue;
		}

#ifdef ENABLE_EXTRA_INVENTORY
		if (item->IsExtraItem()) {
			m_pOwner->SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, ItemSystem::GetItemCell(itemEntity), 255);
		} else {
			m_pOwner->SyncQuickslot(QUICKSLOT_TYPE_ITEM, ItemSystem::GetItemCell(itemEntity), 255);
		}
#else
		m_pOwner->SyncQuickslot(QUICKSLOT_TYPE_ITEM, ItemSystem::GetItemCell(itemEntity), 255);
#endif

		InventorySystem::RemoveFromCharacter(item->GetEntityHandle());
		if (item->IsDragonSoul())
			InventorySystem::AddToCharacter(item->GetEntityHandle(), victim->GetEntityHandle(), TItemPos(DRAGON_SOUL_INVENTORY, empty_pos));
#ifdef ENABLE_EXTRA_INVENTORY
		else if (item->IsExtraItem())
			InventorySystem::AddToCharacter(item->GetEntityHandle(), victim->GetEntityHandle(), TItemPos(EXTRA_INVENTORY, empty_pos));
#endif
		else
			InventorySystem::AddToCharacter(item->GetEntityHandle(), victim->GetEntityHandle(), TItemPos(INVENTORY, empty_pos));
		ITEM_MANAGER::instance().FlushDelayedSave(item);

		ItemSystem::SetItemExchanging(itemEntity, false);
		{
			char exchange_buf[51];

			snprintf(exchange_buf, sizeof(exchange_buf), "%s %u %u", item->GetName(), ecs::PlayerRuntime::GetPlayerID(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null)), ItemSystem::GetItemCount(itemEntity));
			LogManager::instance().ItemLog(victim, item, "EXCHANGE_TAKE", exchange_buf);

			snprintf(exchange_buf, sizeof(exchange_buf), "%s %u %u", item->GetName(), ecs::PlayerRuntime::GetPlayerID(victimEntity), ItemSystem::GetItemCount(itemEntity));
			LogManager::instance().ItemLog(GetOwner(), item, "EXCHANGE_GIVE", exchange_buf);

			if (ItemSystem::GetItemVnum(itemEntity) >= 80003 && ItemSystem::GetItemVnum(itemEntity) <= 80007)
			{
				LogManager::instance().GoldBarLog(ecs::PlayerRuntime::GetPlayerID(victimEntity), ItemSystem::GetItemID(itemEntity), EXCHANGE_TAKE, "");
				LogManager::instance().GoldBarLog(ecs::PlayerRuntime::GetPlayerID(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null)), ItemSystem::GetItemID(itemEntity), EXCHANGE_GIVE, "");
			}
		}

		m_items[i] = entt::null;
	}

	if (m_lGold)
	{
		ecs::PointSystem::Change(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null), POINT_GOLD, -m_lGold, true);
		ecs::PointSystem::Change(victimEntity, POINT_GOLD, m_lGold, true);

		if (m_lGold > 1000)
		{
			char exchange_buf[51];
			snprintf(exchange_buf, sizeof(exchange_buf), "%u %s", ecs::PlayerRuntime::GetPlayerID(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetName(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null)).data());
			LogManager::instance().CharLog(victimEntity, m_lGold, "EXCHANGE_GOLD_TAKE", exchange_buf);

			snprintf(exchange_buf, sizeof(exchange_buf), "%u %s", ecs::PlayerRuntime::GetPlayerID(victimEntity), ecs::PlayerRuntime::GetName(victimEntity).data());
			LogManager::instance().CharLog(GetOwner() ? GetOwner()->GetEntityHandle() : entt::null, m_lGold, "EXCHANGE_GOLD_GIVE", exchange_buf);
		}
	}

	m_pGrid->Clear();
	return true;
}

// 교환을 동의
bool CExchange::Accept(bool bAccept)
{
	if (m_bAccept == bAccept)
		return true;

	m_bAccept = bAccept;

	// 둘 다 동의 했으므로 교환 성립
	if (m_bAccept && GetCompany()->m_bAccept)
	{
		int	iItemCount;

		LPCHARACTER victim = GetCompany()->GetOwner();
		const entt::entity victimEntity = victim ? victim->GetEntityHandle() : entt::null;


		//PREVENT_PORTAL_AFTER_EXCHANGE
		GetOwner()->SetExchangeTime();
		victim->SetExchangeTime();
		//END_PREVENT_PORTAL_AFTER_EXCHANGE

		// @fixme150 BEGIN
		if (quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null)))->IsRunning() == true || quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(victimEntity))->IsRunning() == true)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 631, "");
			ecs::ChatSystem::SendNew(victimEntity, CHAT_TYPE_INFO, 631, "");
#endif
			goto EXCHANGE_END;
		}
		// @fixme150 END

		// exchange_check 에서는 교환할 아이템들이 제자리에 있나 확인하고,
		// 엘크도 충분히 있나 확인한다, 두번째 인자로 교환할 아이템 개수
		// 를 리턴한다.
		if (!Check(&iItemCount))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 232, "");
#endif
			ecs::ChatSystem::SendNew(victimEntity, CHAT_TYPE_INFO, 274, "%s", ecs::PlayerRuntime::GetName(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null)).data());
			goto EXCHANGE_END;
		}

		// 리턴 받은 아이템 개수로 상대방의 소지품에 남은 자리가 있나 확인한다.
		if (!CheckSpace())
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 365, "%s", ecs::PlayerRuntime::GetName(victimEntity).data());
			ecs::ChatSystem::SendNew(victimEntity, CHAT_TYPE_INFO, 366, "");
#endif
			goto EXCHANGE_END;
		}

		// 상대방도 마찬가지로..
		if (!GetCompany()->Check(&iItemCount))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(victimEntity, CHAT_TYPE_INFO, 232, "");
			ecs::ChatSystem::SendNew(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 274, "%s", ecs::PlayerRuntime::GetName(victimEntity).data());
#endif
			goto EXCHANGE_END;
		}

		if (!GetCompany()->CheckSpace())
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(victimEntity, CHAT_TYPE_INFO, 365, "%s", ecs::PlayerRuntime::GetName(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null)).data());
			ecs::ChatSystem::SendNew(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 366, "");
#endif
			goto EXCHANGE_END;
		}

		if (db_clientdesc->GetSocket() == INVALID_SOCKET)
		{
			LOG_ERROR("Cannot use exchange feature while DB cache connection is dead.");
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(victimEntity, CHAT_TYPE_INFO, 759, "");
			ecs::ChatSystem::SendNew(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 759, "");
#endif
			goto EXCHANGE_END;
		}

		if (Done())
		{
			if (m_lGold) // 돈이 있을 떄만 저장
				GetOwner()->Save();

			if (GetCompany()->Done())
			{
				if (GetCompany()->m_lGold) // 돈이 있을 때만 저장
					victim->Save();

#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 105, "%s", ecs::PlayerRuntime::GetName(victimEntity).data());
				ecs::ChatSystem::SendNew(victimEntity, CHAT_TYPE_INFO, 105, "%s", ecs::PlayerRuntime::GetName(((GetOwner()) ? (GetOwner())->GetEntityHandle() : entt::null)).data());
#endif
			}
		}

EXCHANGE_END:
		Cancel();
		return false;
	}
	else
	{
		// 아니면 accept에 대한 패킷을 보내자.
		exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_ACCEPT, true, m_bAccept, NPOS, 0);
		exchange_packet(GetCompany()->GetOwner(), EXCHANGE_SUBHEADER_GC_ACCEPT, false, m_bAccept, NPOS, 0);
		return true;
	}
}

// 교환 취소
void CExchange::Cancel()
{
	exchange_packet(GetOwner(), EXCHANGE_SUBHEADER_GC_END, 0, 0, NPOS, 0);
	GetOwner()->SetExchange(nullptr);

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (ItemSystem::IsValidItem(m_items[i]))
			ItemSystem::SetItemExchanging(m_items[i], false);
		m_items[i] = entt::null;
	}

	if (GetCompany())
	{
		GetCompany()->SetCompany(nullptr);
		GetCompany()->Cancel();
	}

	M2_DELETE(this);
}
