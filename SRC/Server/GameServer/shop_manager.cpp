#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PointSystem.hpp"
#include <Base/grid.h>
#include "constants.h"
#include "utils.h"
#include "config.h"
#include "shop.h"
#include "desc.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "item.h"
#include "item_manager.h"
#include "buffer_manager.h"
#include "packet.h"
#include "log.h"
#include "db.h"
#include "questmanager.h"
#include "mob_manager.h"
#include "locale_service.h"
#include "desc_client.h"
#include "shop_manager.h"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/Registry.hpp"
#include "ecs/EntityFactory.hpp"
#include "group_text_parse_tree.h"
#include "shopEx.h"
#include <boost/algorithm/string/predicate.hpp>
#include "shop_manager.h"
#include <cctype>
#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif
CShopManager::CShopManager()
{
}

CShopManager::~CShopManager()
{
	Destroy();
}

bool CShopManager::Initialize(TShopTable * table, int size)
{
	if (!m_map_pkShop.empty())
		return false;

	int i;

	for (i = 0; i < size; ++i, ++table)
	{
		LPSHOP shop = M2_NEW CShop;

		if (!shop->Create(table->dwVnum, table->dwNPCVnum, table->items))
		{
			M2_DELETE(shop);
			continue;
		}

		m_map_pkShop.insert(TShopMap::value_type(table->dwVnum, shop));
		m_map_pkShopByNPCVnum.insert(TShopMap::value_type(table->dwNPCVnum, shop));
	}
	char szShopTableExFileName[256];

	snprintf(szShopTableExFileName, sizeof(szShopTableExFileName),
		"%s/shop_table_ex.txt", LocaleService_GetBasePath().c_str());

	return ReadShopTableEx(szShopTableExFileName);
}

void CShopManager::Destroy()
{
	TShopMap::iterator it = m_map_pkShop.begin();

	while (it != m_map_pkShop.end())
	{
		M2_DELETE(it->second);
		++it;
	}

	m_map_pkShop.clear();
}

LPSHOP CShopManager::Get(uint32_t dwVnum)
{
	TShopMap::const_iterator it = m_map_pkShop.find(dwVnum);

	if (it == m_map_pkShop.end())
		return nullptr;

	return (it->second);
}

LPSHOP CShopManager::GetByNPCVnum(uint32_t dwVnum)
{
	TShopMap::const_iterator it = m_map_pkShopByNPCVnum.find(dwVnum);

	if (it == m_map_pkShopByNPCVnum.end())
		return nullptr;

	return (it->second);
}

/*
 * 인터페이스 함수들
 */

// 상점 거래를 시작
bool CShopManager::StartShopping(LPCHARACTER pkChr, LPCHARACTER pkChrShopKeeper, int iShopVnum)
{
	const entt::entity chr = pkChr ? pkChr->GetEntityHandle() : entt::null;
	const entt::entity chrShopKeeper = pkChrShopKeeper ? pkChrShopKeeper->GetEntityHandle() : entt::null;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(chr) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(chr) < GM_IMPLEMENTOR) {
		return false;
	}
#endif
	if (pkChr->GetShopOwner() == pkChrShopKeeper)
		return false;
	// this method is only for NPC

	if (ecs::PlayerRuntime::IsPC(chrShopKeeper))
		return false;

	//PREVENT_TRADE_WINDOW
	if (pkChr->IsOpenSafebox() || ecs::SocialSystem::GetExchange(chr) || pkChr->GetMyShop() || pkChr->IsCubeOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(chr, CHAT_TYPE_INFO, 294, "");
#endif
		return false;
	}
	//END_PREVENT_TRADE_WINDOW

	int32_t distance = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(chr) - ecs::PlayerRuntime::GetX(chrShopKeeper), ecs::PlayerRuntime::GetY(chr) - ecs::PlayerRuntime::GetY(chrShopKeeper));

	if (distance >= SHOP_MAX_DISTANCE)
	{
		LOG_INFO("SHOP: TOO_FAR: {} distance {}", ecs::PlayerRuntime::GetName(chr).data(), distance);
		return false;
	}

	LPSHOP pkShop;

	if (iShopVnum)
		pkShop = Get(iShopVnum);
	else
		pkShop = GetByNPCVnum(ecs::PlayerRuntime::GetRaceNum(chrShopKeeper));

	if (!pkShop)
	{
		LOG_INFO("SHOP: NO SHOP");
		return false;
	}

	bool bOtherEmpire = false;

	if (ecs::PlayerRuntime::GetEmpire(chr) != ecs::PlayerRuntime::GetEmpire(chrShopKeeper))
		bOtherEmpire = true;

	pkShop->AddGuest(pkChr, ecs::PlayerRuntime::GetPacketVID(chrShopKeeper), bOtherEmpire);
	pkChr->SetShopOwner((pkChrShopKeeper ? pkChrShopKeeper->GetEntityHandle() : entt::null));
	LOG_INFO("SHOP: START: {}", ecs::PlayerRuntime::GetName(chr).data());
	return true;
}

LPSHOP CShopManager::FindPCShop(uint32_t dwVID)
{
	TShopMap::iterator it = m_map_pkShopByPC.find(dwVID);

	if (it == m_map_pkShopByPC.end())
		return nullptr;

	return it->second;
}

LPSHOP CShopManager::CreatePCShop(LPCHARACTER ch, TShopItemTable * pTable, uint8_t bItemCount)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	if (FindPCShop(ecs::PlayerRuntime::GetPacketVID(chEntity)))
		return nullptr;

	LPSHOP pkShop = M2_NEW CShop;
	pkShop->SetPCShop(ch);
	pkShop->SetShopItems(pTable, bItemCount);

	m_map_pkShopByPC.insert(TShopMap::value_type(ecs::PlayerRuntime::GetPacketVID(chEntity), pkShop));
	return pkShop;
}

void CShopManager::DestroyPCShop(LPCHARACTER ch)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	LPSHOP pkShop = FindPCShop(ecs::PlayerRuntime::GetPacketVID(chEntity));

	if (!pkShop)
		return;

	//PREVENT_ITEM_COPY;
	ch->SetMyShopTime();
	//END_PREVENT_ITEM_COPY

	m_map_pkShopByPC.erase(ecs::PlayerRuntime::GetPacketVID(chEntity));
	M2_DELETE(pkShop);
}

// 상점 거래를 종료
void CShopManager::StopShopping(LPCHARACTER ch)
{
	LPSHOP shop;

	if (!(shop = ch->GetShop()))
		return;

	//PREVENT_ITEM_COPY;
	ch->SetMyShopTime();
	//END_PREVENT_ITEM_COPY

	shop->RemoveGuest(ch);
	LOG_INFO("SHOP: END: {}", ecs::PlayerRuntime::GetName(((ch) ? (ch)->GetEntityHandle() : entt::null)).data());
}

// 아이템 구입
void CShopManager::Buy(LPCHARACTER ch, uint8_t pos)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(chEntity) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(chEntity) < GM_IMPLEMENTOR) {
		return;
	}
#endif
#ifdef ENABLE_NEWSTUFF
	if (0 != g_BuySellTimeLimitValue)
	{
		if (get_dword_time() < ch->GetLastBuySellTime()+g_BuySellTimeLimitValue)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 510, "");
#endif
			return;
		}
	}

	ch->SetLastBuySellTime(get_dword_time());
#endif
	if (!ch->GetShop())
		return;

	if (ch->GetShopOwner())
	{
		if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(chEntity) - ecs::PlayerRuntime::GetX(((ch->GetShopOwner()) ? (ch->GetShopOwner())->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY(chEntity) - ecs::PlayerRuntime::GetY(((ch->GetShopOwner()) ? (ch->GetShopOwner())->GetEntityHandle() : entt::null))) > 2000)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 381, "");
#endif
			return;
		}
	}

	CShop* pkShop = ch->GetShop();
	//PREVENT_ITEM_COPY
	ch->SetMyShopTime();
	//END_PREVENT_ITEM_COPY

	int ret = pkShop->Buy(ch, pos);

	if (SHOP_SUBHEADER_GC_OK != ret) // 문제가 있었으면 보낸다.
	{
		TPacketGCShop pack;

		pack.header	= HEADER_GC_SHOP;
		pack.subheader	= ret;
		pack.size	= sizeof(TPacketGCShop);

		ecs::PlayerRuntime::GetDesc(chEntity)->Packet(&pack, sizeof(pack));
	}
}

#ifdef ENABLE_BUY_STACK_FROM_SHOP
void CShopManager::MultipleBuy(LPCHARACTER ch, uint8_t p, uint8_t c) {
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(chEntity) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(chEntity) < GM_IMPLEMENTOR) {
		return;
	}
#endif

	if (!ch->GetShop()) {
		return;
	}

	if (ch->GetShopOwner()) {
		if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(chEntity) - ecs::PlayerRuntime::GetX(((ch->GetShopOwner()) ? (ch->GetShopOwner())->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY(chEntity) - ecs::PlayerRuntime::GetY(((ch->GetShopOwner()) ? (ch->GetShopOwner())->GetEntityHandle() : entt::null))) > 2000) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 381, "");
#endif
			return;
		}
	}

	CShop* pkShop = ch->GetShop();
	//PREVENT_ITEM_COPY
	ch->SetMyShopTime();
	//END_PREVENT_ITEM_COPY

	int ret = pkShop->MultipleBuy(ch, p, c);
	if (SHOP_SUBHEADER_GC_OK != ret) {
		TPacketGCShop pack;
		pack.header = HEADER_GC_SHOP;
		pack.subheader = ret;
		pack.size = sizeof(TPacketGCShop);

		ecs::PlayerRuntime::GetDesc(chEntity)->Packet(&pack, sizeof(pack));
	}
}
#endif

#ifdef ENABLE_EXTRA_INVENTORY
void CShopManager::Sell(LPCHARACTER ch, TItemPos Cell,
#ifdef ENABLE_NEW_STACK_LIMIT
uint16_t bCount
#else
uint8_t bCount
#endif
)
#else
void CShopManager::Sell(LPCHARACTER ch, uint8_t bCell,
#ifdef ENABLE_NEW_STACK_LIMIT
uint16_t bCount
#else
uint8_t bCount
#endif
)
#endif
{

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(((ch) ? (ch)->GetEntityHandle() : entt::null)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(((ch) ? (ch)->GetEntityHandle() : entt::null)) < GM_IMPLEMENTOR) {
		return;
	}
#endif
#ifdef ENABLE_NEWSTUFF
	if (0 != g_BuySellTimeLimitValue)
	{
		if (get_dword_time() < ch->GetLastBuySellTime()+g_BuySellTimeLimitValue)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 510, "");
#endif
			return;
		}
	}

	ch->SetLastBuySellTime(get_dword_time());
#endif
	if (!ch->GetShop())
		return;

	if (!ch->GetShopOwner())
		return;

	if (!ch->CanHandleItem())
		return;

	if (ch->GetShop()->IsPCShop())
		return;

	/*
	if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(((ch) ? (ch)->GetEntityHandle() : entt::null))-ecs::PlayerRuntime::GetX(((ch->GetShopOwner()) ? (ch->GetShopOwner())->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY(((ch) ? (ch)->GetEntityHandle() : entt::null))-ecs::PlayerRuntime::GetY(((ch->GetShopOwner()) ? (ch->GetShopOwner())->GetEntityHandle() : entt::null)))>2000)
	{
		return;
	}
	*/

	const entt::entity owner = ((ch) ? (ch)->GetEntityHandle() : entt::null);
#ifdef ENABLE_EXTRA_INVENTORY
	const entt::entity itemEntity = ItemSystem::GetItem(owner, Cell);
#else
	const entt::entity itemEntity = ItemSystem::GetInventoryItem(owner, bCell);
#endif

	if (!ItemSystem::IsValidItem(itemEntity))
		return;

	if (ItemSystem::IsItemEquipped(itemEntity) == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 541, "");
#endif
		return;
	}

	if (ItemSystem::IsItemLocked(itemEntity))
	{
		return;
	}

	if (IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_SELL))
		return;

	const uint32_t itemCount = ItemSystem::GetItemCount(itemEntity);
	if (itemCount == 0)
		return;
	if (bCount == 0 || bCount > itemCount)
		bCount = static_cast<decltype(bCount)>(itemCount);

	int64_t dwPrice = ItemSystem::GetItemShopBuyPrice(itemEntity);

	if (IS_SET(ItemSystem::GetItemFlags(itemEntity), ITEM_FLAG_COUNT_PER_1GOLD))
	{
		if (dwPrice == 0)
			dwPrice = bCount;
		else
			dwPrice = bCount / dwPrice;
	}
	else
		dwPrice *= bCount;

/* 	dwPrice /= 5;

	//세금 계산
	uint32_t dwTax = 0;
	int iVal = 3;

	{
		dwTax = dwPrice * iVal/100;
		dwPrice -= dwTax;
	} */

	if (test_server)
		LOG_INFO("Sell Item price id {} {} itemid {}", ecs::PlayerRuntime::GetPlayerID(((ch) ? (ch)->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetName(((ch) ? (ch)->GetEntityHandle() : entt::null)).data(), ItemSystem::GetItemID(itemEntity));

	const int64_t currentGold = ecs::PointSystem::GetGold(owner);
	if (dwPrice < 0 || currentGold >= GOLD_MAX || dwPrice >= GOLD_MAX - currentGold)
	{
		const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
		LOG_ERROR("[OVERFLOW_GOLD] id {} name {} gold {}", ecs::PlayerRuntime::GetPlayerID(chEntity), ecs::PlayerRuntime::GetName(chEntity).data(), ecs::PointSystem::GetGold(chEntity));
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 226,
		"%lld"

		, GOLD_MAX);
#endif
		return;
	}

	DBManager::instance().SendMoneyLog(MONEY_LOG_SHOP, ItemSystem::GetItemVnum(itemEntity), dwPrice);
#ifdef ENABLE_BATTLE_PASS
	uint8_t bBattlePassId = ch->GetBattlePassId();
	if(bBattlePassId)
	{
		uint32_t dwItemVnum, dwSellCount;
		if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, SELL_ITEM, &dwItemVnum, &dwSellCount))
		{
			if(dwItemVnum == ItemSystem::GetItemVnum(itemEntity) && ch->GetMissionProgress(SELL_ITEM, bBattlePassId) < dwSellCount)
				ch->UpdateMissionProgress(SELL_ITEM, bBattlePassId, bCount, dwSellCount);
		}
	}
#endif
	const bool sold = (bCount == itemCount)
		? ItemSystem::DestroyItemEntityEcs(itemEntity, "SELL")
		: ItemSystem::ConsumeItemEcs(itemEntity, bCount);
	if (!sold)
		return;

	ecs::PointSystem::Change(owner, POINT_GOLD, dwPrice, false);
}

bool CompareShopItemName(const SShopItemTable& lhs, const SShopItemTable& rhs)
{
	TItemTable* lItem = ITEM_MANAGER::instance().GetTable(lhs.vnum);
	TItemTable* rItem = ITEM_MANAGER::instance().GetTable(rhs.vnum);
	if (lItem && rItem)
#ifdef ENABLE_MULTI_NAMES
		return strcmp(lItem->szLocaleName[DEFAULT_LANGUAGE], rItem->szLocaleName[DEFAULT_LANGUAGE]) < 0;
#else
		return strcmp(lItem->szLocaleName, rItem->szLocaleName) < 0;
#endif
	else
		return true;
}

bool ConvertToShopItemTable(IN CGroupNode* pNode, OUT TShopTableEx& shopTable)
{
	if (!pNode->GetValue("vnum", 0, shopTable.dwVnum))
	{
		LOG_ERROR("Group {} does not have vnum.", pNode->GetNodeName().c_str());
		return false;
	}

	if (!pNode->GetValue("name", 0, shopTable.name))
	{
		LOG_ERROR("Group {} does not have name.", pNode->GetNodeName().c_str());
		return false;
	}

	if (shopTable.name.length() >= SHOP_TAB_NAME_MAX)
	{
		LOG_ERROR("Shop name length must be less than {}. Error in Group {}, name {}", SHOP_TAB_NAME_MAX, pNode->GetNodeName().c_str(), shopTable.name.c_str());
		return false;
	}

	std::string stCoinType;
	if (!pNode->GetValue("cointype", 0, stCoinType))
	{
		stCoinType = "Gold";
	}

	if (boost::iequals(stCoinType, "Gold"))
	{
		shopTable.coinType = SHOP_COIN_TYPE_GOLD;
	}
	else if (boost::iequals(stCoinType, "SecondaryCoin"))
	{
		shopTable.coinType = SHOP_COIN_TYPE_SECONDARY_COIN;
	}
	else
	{
		LOG_ERROR("Group {} has undefine cointype({}).", pNode->GetNodeName().c_str(), stCoinType.c_str());
		return false;
	}

	CGroupNode* pItemGroup = pNode->GetChildNode("items");
	if (!pItemGroup)
	{
		LOG_ERROR("Group {} does not have 'group items'.", pNode->GetNodeName().c_str());
		return false;
	}

	int itemGroupSize = pItemGroup->GetRowCount();
	std::vector <TShopItemTable> shopItems(itemGroupSize);
	if (itemGroupSize >= SHOP_HOST_ITEM_MAX_NUM)
	{
		LOG_ERROR("count({}) of rows of group items of group {} must be smaller than {}", itemGroupSize, pNode->GetNodeName().c_str(), SHOP_HOST_ITEM_MAX_NUM);
		return false;
	}

	for (int i = 0; i < itemGroupSize; i++)
	{
		if (!pItemGroup->GetValue(i, "vnum", shopItems[i].vnum))
		{
			LOG_ERROR("row({}) of group items of group {} does not have vnum column", i, pNode->GetNodeName().c_str());
			return false;
		}

		if (!pItemGroup->GetValue(i, "count", shopItems[i].count))
		{
			LOG_ERROR("row({}) of group items of group {} does not have count column", i, pNode->GetNodeName().c_str());
			return false;
		}
		if (!pItemGroup->GetValue(i, "price", shopItems[i].price))
		{
			LOG_ERROR("row({}) of group items of group {} does not have price column", i, pNode->GetNodeName().c_str());
			return false;
		}
	}
	std::string stSort;
	if (!pNode->GetValue("sort", 0, stSort))
	{
		stSort = "None";
	}

	if (boost::iequals(stSort, "Asc"))
	{
		std::sort(shopItems.begin(), shopItems.end(), CompareShopItemName);
	}
	else if(boost::iequals(stSort, "Desc"))
	{
		std::sort(shopItems.rbegin(), shopItems.rend(), CompareShopItemName);
	}
#ifdef ENABLE_120_SHOP_SLOT_RAZOR93
	CGrid grid = CGrid(15, 9);
#else
	CGrid grid = CGrid(5, 9);
#endif
	int iPos;

	memset(&shopTable.items[0], 0, sizeof(shopTable.items));

	for (size_t i = 0; i < shopItems.size(); i++)
	{
		TItemTable * item_table = ITEM_MANAGER::instance().GetTable(shopItems[i].vnum);
		if (!item_table)
		{
			LOG_ERROR("vnum({}) of group items of group {} does not exist", shopItems[i].vnum, pNode->GetNodeName().c_str());
			return false;
		}

		iPos = grid.FindBlank(1, item_table->bSize);

		grid.Put(iPos, 1, item_table->bSize);
		shopTable.items[iPos] = shopItems[i];
	}

	shopTable.byItemCount = shopItems.size();
	return true;
}

bool CShopManager::ReadShopTableEx(const char* stFileName)
{
	// file 유무 체크.
	// 없는 경우는 에러로 처리하지 않는다.
	FILE* fp = fopen(stFileName, "rb");
	if (nullptr == fp)
		return true;
	fclose(fp);

	CGroupTextParseTreeLoader loader;
	if (!loader.Load(stFileName))
	{
		LOG_ERROR("{} Load fail.", stFileName);
		return false;
	}

	CGroupNode* pShopNPCGroup = loader.GetGroup("shopnpc");
	if (nullptr == pShopNPCGroup)
	{
		LOG_ERROR("Group ShopNPC is not exist.");
		return false;
	}

	typedef std::multimap <uint32_t, TShopTableEx> TMapNPCshop;
	TMapNPCshop map_npcShop;
	for (int i = 0; i < pShopNPCGroup->GetRowCount(); i++)
	{
		uint32_t npcVnum;
		std::string shopName;
		if (!pShopNPCGroup->GetValue(i, "npc", npcVnum) || !pShopNPCGroup->GetValue(i, "group", shopName))
		{
			LOG_ERROR("Invalid row({}). Group ShopNPC rows must have 'npc', 'group' columns", i);
			return false;
		}
		std::transform(shopName.begin(), shopName.end(), shopName.begin(), (int(*)(int))std::tolower);
		CGroupNode* pShopGroup = loader.GetGroup(shopName.c_str());
		if (!pShopGroup)
		{
			LOG_ERROR("Group {} is not exist.", shopName.c_str());
			return false;
		}
		TShopTableEx table;
		if (!ConvertToShopItemTable(pShopGroup, table))
		{
			LOG_ERROR("Cannot read Group {}.", shopName.c_str());
			return false;
		}
		if (m_map_pkShopByNPCVnum.find(npcVnum) != m_map_pkShopByNPCVnum.end())
		{
			LOG_ERROR("{} cannot have both original shop and extended shop", npcVnum);
			return false;
		}

		map_npcShop.insert(TMapNPCshop::value_type(npcVnum, table));
	}

	for (TMapNPCshop::iterator it = map_npcShop.begin(); it != map_npcShop.end(); ++it)
	{
		uint32_t npcVnum = it->first;
		TShopTableEx& table = it->second;
		if (m_map_pkShop.find(table.dwVnum) != m_map_pkShop.end())
		{
			LOG_ERROR("Shop vnum({}) already exists", table.dwVnum);
			return false;
		}
		TShopMap::iterator shop_it = m_map_pkShopByNPCVnum.find(npcVnum);

		LPSHOPEX pkShopEx = nullptr;
		if (m_map_pkShopByNPCVnum.end() == shop_it)
		{
			pkShopEx = M2_NEW CShopEx;
			pkShopEx->Create(0, npcVnum);
			m_map_pkShopByNPCVnum.insert(TShopMap::value_type(npcVnum, pkShopEx));
		}
		else
		{
			pkShopEx = dynamic_cast <CShopEx*> (shop_it->second);
			if (nullptr == pkShopEx)
			{
				LOG_ERROR("WTF!!! It can't be happend. NPC({}) Shop is not extended version.", shop_it->first);
				return false;
			}
		}

		if (pkShopEx->GetTabCount() >= SHOP_TAB_COUNT_MAX)
		{
			LOG_ERROR("ShopEx cannot have tab more than {}", SHOP_TAB_COUNT_MAX);
			return false;
		}

		if (pkShopEx->GetVnum() != 0 && m_map_pkShop.find(pkShopEx->GetVnum()) != m_map_pkShop.end())
		{
			LOG_ERROR("Shop vnum({}) already exist.", pkShopEx->GetVnum());
			return false;
		}
		m_map_pkShop.insert(TShopMap::value_type (pkShopEx->GetVnum(), pkShopEx));
		pkShopEx->AddShopTable(table);
	}

	return true;
}

