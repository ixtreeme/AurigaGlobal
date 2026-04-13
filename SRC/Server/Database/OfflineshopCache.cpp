#include "stdafx.h"

#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "Main.h"
#include "Config.h"
#include "DBManager.h"
#include "QID.h"
#include "Peer.h"
#include "ClientManager.h"

#include "OfflineshopCache.h"


namespace offlineshop
{
	//SHOPS
	std::string CreateShopCacheInsertItemQuery(uint32_t dwOwner, const CShopCache::TShopCacheItemInfo& rItem);
	std::string CreateShopCacheUpdateItemQuery(uint32_t dwItemID, const TPriceInfo& rItemPrice);
	std::string CreateShopCacheDeleteShopQuery(uint32_t dwOwner);
	std::string CreateShopCacheDeleteShopItemQuery(uint32_t dwOwner);
	std::string CreateShopCacheInsertShopQuery(uint32_t dwOwnerID, uint32_t dwDuration, const char* name
#ifdef KASMIR_PAKET_SYSTEM
	, uint32_t dwKasmirNpc
#endif
	);
	std::string CreateShopCacheUpdateShopNameQuery(uint32_t dwOwnerID, const char* name);
	std::string CreateShopCacheUpdateDurationQuery(uint32_t dwOwnerID, uint32_t dwDuration);
	std::string CreateShopCacheDeleteItemQuery(uint32_t dwOwnerID, uint32_t dwItemID);
	std::string CreateShopCacheUpdateSoldItemQuery(uint32_t dwOwnerID, uint32_t dwItemID);

	//SAFEBOX
	std::string CreateSafeboxCacheDeleteItemQuery(uint32_t dwItem);
	std::string CreateSafeboxCacheInsertItemQuery(uint32_t dwOwner, const TItemInfoEx& item);
	std::string CreateSafeboxCacheUpdateValutes(uint32_t dwOwner, const TValutesInfo& val);
	std::string CreateSafeboxCacheInsertSafeboxValutesQuery(uint32_t dwOwnerID);
	std::string CreateSafeboxCacheUpdateValutesByAdding(uint32_t dwOwner, const TValutesInfo& val);
	std::string CreateSafeboxCacheLoadItemsQuery(uint32_t dwOwnerID);
	std::string CreateSafeboxCacheLoadValutesQuery(uint32_t dwOwnerID);

	//OFFERS
	std::string CreateOfferCacheInsertOfferQuery(const COfferCache::TOfferCacheInfo& rOffer);
	std::string CreateOfferCacheUpdateNotifiedQuery(uint32_t dwOfferID);
	std::string CreateOfferCacheUpdateAcceptedQuery(uint32_t dwOfferID);
	std::string CreateOfferCacheRemoveOfferByShopOwner(uint32_t dwShopOwner);
	std::string CreateOfferCacheDeleteOfferQuery(uint32_t dwOfferID);
	
	//AUCTION

	std::string CreateAuctionCacheAddAuctionQuery(const TAuctionInfo& auction);
	std::string CreateAuctionCacheAddOfferQuery(const TAuctionOfferInfo& auctionOffer);
	std::string CreateAuctionCacheDeleteAuction(uint32_t dwOwnerID);
	std::string CreateAuctionCacheDeleteAuctionOffers(uint32_t dwOwnerID);
	std::string CreateAuctionCacheUpdateDurationQuery(uint32_t dwOwnerID, uint32_t dwDuration);

	/*
			CSHOPCACHE
	*/

	CShopCache::CShopCache()
	{
	}

	CShopCache::~CShopCache()
	{
	}




	bool CShopCache::Get(uint32_t dwOwnerID, TShopCacheInfo** ppCache) const
	{
		CONST_CACHEITER it = m_shopsMap.find(dwOwnerID);
		if (it == m_shopsMap.end())
			return false;

		*ppCache = (TShopCacheInfo*)&(it->second);
		return true;
	}


	bool CShopCache::AddItem(uint32_t dwOwnerID, const TShopCacheItemInfo& rItem)
	{
		TShopCacheInfo* pCache;
		if (!Get(dwOwnerID, &pCache))
			return false;

		SQueryInfoAddItem* qi = new SQueryInfoAddItem;
		qi->dwOwnerID = dwOwnerID;
		CopyObject(qi->item, rItem);

		std::string query = CreateShopCacheInsertItemQuery(dwOwnerID, rItem);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_ADD_ITEM, 0, qi);
		return true;
	}
	
	
	bool CShopCache::RemoveItem(uint32_t dwOwnerID, uint32_t dwItemID)
	{
		OFFSHOP_DEBUG("owner %u , item id %u ", dwOwnerID, dwItemID);

		TShopCacheInfo* pCache;
		if (!Get(dwOwnerID, &pCache))
			return false;

		OFFSHOP_DEBUG("found successful (shop %u)",dwOwnerID);

		std::map<uint32_t, TShopCacheItemInfo>::iterator it = pCache->itemsmap.find(dwItemID);
		if (it == pCache->itemsmap.end())
			return false;

		OFFSHOP_DEBUG("found successful (item %u)",dwItemID);

		if(it->second.bLock)
			return false;

		OFFSHOP_DEBUG("is not locked (item %u)",dwItemID);

		pCache->itemsmap.erase(it);

		std::string query = CreateShopCacheDeleteItemQuery(dwOwnerID, dwItemID);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_REMOVE_ITEM, 0, nullptr);

		if(pCache->itemsmap.empty())
			CClientManager::instance().OfflineshopExpiredShop(dwOwnerID);

		return true;
	}


	bool CShopCache::SellItem(uint32_t dwOwnerID, uint32_t dwItemID)
	{
		TShopCacheInfo* pCache;
		if (!Get(dwOwnerID, &pCache))
			return false;

		std::map<uint32_t, TShopCacheItemInfo>::iterator it = pCache->itemsmap.find(dwItemID);
		if (it == pCache->itemsmap.end())
			return false;

		OFFSHOP_DEBUG("found item %u ",dwItemID);

		if(!it->second.bLock)
			return false;

		OFFSHOP_DEBUG("inserted into soldsmap %u ",dwItemID);

		pCache->soldsmap.insert(std::make_pair(it->first, it->second));
		pCache->itemsmap.erase(it);

		std::string query = CreateShopCacheUpdateSoldItemQuery(dwOwnerID, dwItemID);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_UPDATE_SOLD_ITEM, 0, nullptr);

		//offlineshop-updated 03/08/19
		if(pCache->itemsmap.empty())
			CClientManager::instance().OfflineshopExpiredShop(dwOwnerID);
		//end

		return true;
	}



	bool CShopCache::LockSellItem(uint32_t dwOwnerID, uint32_t dwItemID, int64_t TotalPriceSeen) //patch seen price check
	{
		TShopCacheInfo* pCache;
		if (!Get(dwOwnerID, &pCache))
			return false;

		std::map<uint32_t, TShopCacheItemInfo>::iterator it = pCache->itemsmap.find(dwItemID);
		if (it == pCache->itemsmap.end())
			return false;

		if(it->second.bLock)
			return false;

		//patch seen price check
		if (it->second.price.GetTotalYangAmount() != TotalPriceSeen)
			return false;

		OFFSHOP_DEBUG("locked success %u ",dwItemID);
		it->second.bLock = true;
		return true;
	}

	bool CShopCache::UnlockSellItem(uint32_t dwOwnerID, uint32_t dwItemID)//topatch
	{
		TShopCacheInfo* pCache;
		if (!Get(dwOwnerID, &pCache))
			return false;

		std::map<uint32_t, TShopCacheItemInfo>::iterator it = pCache->itemsmap.find(dwItemID);
		if (it == pCache->itemsmap.end())
			return false;

		if (!it->second.bLock)
			return false;

		OFFSHOP_DEBUG("Unlocked success %u ", dwItemID);
		it->second.bLock = false;
		return true;
	}


	bool CShopCache::EditItem(uint32_t dwOwnerID, uint32_t dwItemID, const TPriceInfo& rItemPrice)
	{
		TShopCacheInfo* pCache;
		if (!Get(dwOwnerID, &pCache))
			return false;

		std::map<uint32_t, TShopCacheItemInfo>::iterator it = pCache->itemsmap.find(dwItemID);
		if (it == pCache->itemsmap.end())
			return false;

		if(it->second.bLock)
			return false;

		TShopCacheItemInfo& rItem = it->second;
		CopyObject(rItem.price, rItemPrice);

		std::string query = CreateShopCacheUpdateItemQuery(dwItemID, rItemPrice);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_EDIT_ITEM, 0, nullptr);
		return true;
	}


	bool CShopCache::CloseShop(uint32_t dwOwnerID)
	{
		CACHEITER it = m_shopsMap.find(dwOwnerID);
		if (it == m_shopsMap.end())
			return false;

		m_shopsMap.erase(it);

		std::string query = CreateShopCacheDeleteShopQuery(dwOwnerID);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_DELETE_SHOP, 0, nullptr);

		query = CreateShopCacheDeleteShopItemQuery(dwOwnerID);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_DELETE_SHOP_ITEM, 0, nullptr);
		return true;
	}

	bool CShopCache::CreateShop(uint32_t dwOwnerID, uint32_t dwDuration, const char* szName, std::vector<TShopCacheItemInfo>& items
#ifdef KASMIR_PAKET_SYSTEM
	, uint32_t dwKasmirNpc
#endif
	)
	{
		CACHEITER it = m_shopsMap.find(dwOwnerID);
		if (it != m_shopsMap.end())
			return false;

		SQueryInfoCreateShop* qi = new SQueryInfoCreateShop;
		qi->dwOwnerID = dwOwnerID;
		qi->dwDuration = dwDuration;
#ifdef KASMIR_PAKET_SYSTEM
		qi->dwKasmirNpc = dwKasmirNpc;
#endif
		strncpy(qi->szName, szName, sizeof(qi->szName));
		CopyContainer(qi->items, items);

		std::string query = CreateShopCacheInsertShopQuery(dwOwnerID, dwDuration, szName
#ifdef KASMIR_PAKET_SYSTEM
		, dwKasmirNpc
#endif
		);
		
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_CREATE_SHOP, 0, qi);
		
		OFFSHOP_DEBUG("Sent query %s", query.c_str());
		return true;
	}

	bool CShopCache::CreateShopAddItem(SQueryInfoCreateShop* qi, const TShopCacheItemInfo& rItem)
	{
		CACHEITER it = m_shopsMap.find(qi->dwOwnerID);
		if (it == m_shopsMap.end())
			return false;

		std::string query = CreateShopCacheInsertItemQuery(qi->dwOwnerID, rItem);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_CREATE_SHOP_ADD_ITEM, 0, qi);
		OFFSHOP_DEBUG("Sent query %s", query.c_str());
		return true;
	}

	bool CShopCache::ChangeShopName(uint32_t dwOwnerID, const char* szName)
	{
		CACHEITER it = m_shopsMap.find(dwOwnerID);
		if (it == m_shopsMap.end())
			return false;

		TShopCacheInfo& rShop = it->second;
		strncpy(rShop.szName, szName, sizeof(rShop.szName));

		std::string query = CreateShopCacheUpdateShopNameQuery(dwOwnerID, szName);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_SHOP_CHANGE_NAME, 0, nullptr);
		return true;
	}


	bool CShopCache::PutItem(uint32_t dwOwnerID, uint32_t dwItemID, const TShopCacheItemInfo& rItem, bool isSold)
	{
		CACHEITER it = m_shopsMap.find(dwOwnerID);
		if (it == m_shopsMap.end())
			return false;

		TShopCacheInfo& rShop	= it->second;
		SHOPCACHE_MAP& rMap		= isSold ? rShop.soldsmap : rShop.itemsmap;

		if (rMap.find(dwItemID) != rMap.end())
			return false;

		rMap.insert(std::make_pair(dwItemID, rItem));
		return true;
	}


	bool CShopCache::PutShop(uint32_t dwOwnerID, uint32_t dwDuration, const char* szName
#ifdef KASMIR_PAKET_SYSTEM
	, uint32_t dwKasmirNpc
#endif
	)
	{
		CACHEITER it = m_shopsMap.find(dwOwnerID);
		if (it != m_shopsMap.end())
			return false;

		TShopCacheInfo sShop;
		sShop.dwDuration = dwDuration;
		strncpy(sShop.szName, szName, sizeof(sShop.szName));
#ifdef KASMIR_PAKET_SYSTEM
		sShop.dwKasmirNpc = dwKasmirNpc;
#endif
		m_shopsMap.insert(std::make_pair(dwOwnerID, sShop));
		return true;
	}


	void CShopCache::EncodeCache(CPeer* peer) const
	{
		TShopInfo shopInfo;
		auto it=m_shopsMap.begin();

		while (it != m_shopsMap.end())
		{
			uint32_t dwOwnerID				= it->first;
			const TShopCacheInfo& rShop	= it->second;

			
			strncpy(shopInfo.szName, rShop.szName, sizeof(shopInfo.szName));
			shopInfo.dwDuration = rShop.dwDuration;

			shopInfo.dwOwnerID	= dwOwnerID;
			shopInfo.dwCount	= rShop.itemsmap.size();
#ifdef KASMIR_PAKET_SYSTEM
			shopInfo.dwKasmirNpc = rShop.dwKasmirNpc;
#endif
			peer->Encode(&shopInfo, sizeof(shopInfo));
			peer->EncodeDWORD(rShop.soldsmap.size());

			OFFSHOP_DEBUG("encoding shop %u %s (solds %u) ", shopInfo.dwOwnerID, shopInfo.szName);

			auto itItem= rShop.itemsmap.begin();
			TItemInfo itemInfo;

			for (; itItem != rShop.itemsmap.end(); itItem++)
			{

				uint32_t dwItemID					= itItem->first;			
				const TShopCacheItemInfo& rItem = itItem->second;

				
				itemInfo.dwOwnerID = dwOwnerID;
				itemInfo.dwItemID  = dwItemID;
				
				CopyObject(itemInfo.item , rItem.item);
				CopyObject(itemInfo.price, rItem.price);

				OFFSHOP_DEBUG("encoding for sale item %u ",itemInfo.dwItemID);
				peer->Encode(&itemInfo, sizeof(itemInfo));
			}

			for (itItem = rShop.soldsmap.begin(); itItem != rShop.soldsmap.end(); itItem++)
			{

				uint32_t dwItemID					= itItem->first;			
				const TShopCacheItemInfo& rItem = itItem->second;


				itemInfo.dwOwnerID = dwOwnerID;
				itemInfo.dwItemID  = dwItemID;

				CopyObject(itemInfo.item , rItem.item);
				CopyObject(itemInfo.price, rItem.price);

				OFFSHOP_DEBUG("encoding sold item %u ",itemInfo.dwItemID);
				peer->Encode(&itemInfo, sizeof(itemInfo));
			}


			it++;
		}
	}


	uint32_t CShopCache::GetItemCount() const
	{
		uint32_t dwItemCount=0;
		CONST_CACHEITER it = m_shopsMap.begin();
		for (; it != m_shopsMap.end(); ++it)
		{
			dwItemCount += it->second.itemsmap.size();
			dwItemCount += it->second.soldsmap.size();
		}

		return dwItemCount;
	}

	//patch 08-03-2020
	void CShopCache::ShopDurationProcess()
	{
		CACHEITER it = m_shopsMap.begin();
		for (; it != m_shopsMap.end(); ++it)
			if(--it->second.dwDuration!=0 && it->second.dwDuration % 5 == 0)
				UpdateDurationQuery(it->first, it->second);

		//expired check
		std::vector<uint32_t> vec;

		//item expired check
		std::vector<std::pair<uint32_t,uint32_t>> item_vec;
		const time_t now = time(nullptr);
#ifdef ENABLE_OFFLINESHOP_CLEAR_CHACHE
		static int s_iLastSoldClearDay = -1;

		tm tmNow;
		localtime_r(&now, &tmNow);

		// napi 1x, ejfel utan toroljuk az eladott itemeket
		if (tmNow.tm_hour == 0 && s_iLastSoldClearDay != tmNow.tm_yday)
		{
			s_iLastSoldClearDay = tmNow.tm_yday;

			// memoria cache torles
			CACHEITER itClear = m_shopsMap.begin();
			for (; itClear != m_shopsMap.end(); ++itClear)
				itClear->second.soldsmap.clear();

			// adatbazis torles
			CDBManager::instance().AsyncQuery(
				"DELETE FROM `player`.`offlineshop_shop_items` WHERE `is_sold` = 1;"
			);
		}
#endif
		it = m_shopsMap.begin();
		for (; it != m_shopsMap.end(); ++it)
		{
			CShopCache::TShopCacheInfo& shop = it->second;

			if (shop.dwDuration == 0) {
				vec.push_back(it->first);
				continue;
			}

			auto it_item  = shop.itemsmap.begin();
			auto end_item = shop.itemsmap.end();

			for (; it_item != end_item; it_item++) {
				TItemInfoEx& item_info = it_item->second.item;
				if (item_info.expiration == offlineshop::ExpirationType::EXPIRE_REAL_TIME) {
					if (now > item_info.alSockets[0])
						item_vec.push_back(std::make_pair(it->first, it_item->first));
				}
				else if (item_info.expiration == offlineshop::ExpirationType::EXPIRE_REAL_TIME_FIRST_USE) {
					if (item_info.alSockets[1] != 0 && item_info.alSockets[0] < now)
						item_vec.push_back(std::make_pair(it->first, it_item->first));
				}
			}
		}

		for(uint32_t i=0; i < vec.size(); i++)
			CClientManager::instance().OfflineshopExpiredShop(vec[i]);

		auto item_it  = item_vec.begin();
		auto item_end = item_vec.end();
		for (; item_it != item_end; item_it++) {
			CClientManager::Instance().SendOfflineShopRemoveItemPacket(item_it->first, item_it->second);
			//patch offerlist loading fix
			CClientManager::Instance().RemoveOfferOnShopItem(item_it->first);

			RemoveItem(item_it->first, item_it->second);
		}
	}


	void CShopCache::UpdateDurationQuery(uint32_t dwOwnerID, const TShopCacheInfo& rShop)
	{
		std::string query = CreateShopCacheUpdateDurationQuery(dwOwnerID, rShop.dwDuration);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_SHOP_UPDATE_DURATION, 0 , nullptr);
	}




	//SAFEBOX CHACHE
	CSafeboxCache::CSafeboxCache()
	{
	}

	CSafeboxCache::~CSafeboxCache()
	{
	}

	bool CSafeboxCache::Get(uint32_t dwOwnerID, TSafeboxCacheInfo** ppSafebox) const
	{
		CHACHECONSTITER it = m_safeboxMap.find(dwOwnerID);
		if (it == m_safeboxMap.end())
			return false;

		*ppSafebox = (TSafeboxCacheInfo*)&(it->second);
		return true;
	}



	bool CSafeboxCache::PutSafebox(uint32_t dwOwnerID, const TSafeboxCacheInfo& rSafebox)
	{
		CHACHECONSTITER it = m_safeboxMap.find(dwOwnerID);
		if (it != m_safeboxMap.end())
			return false;
		
		m_safeboxMap.insert(std::make_pair(dwOwnerID, rSafebox));
		return true;
	}

	bool CSafeboxCache::PutItem(uint32_t dwOwnerID, uint32_t dwItem, const TItemInfoEx& item)
	{
		TSafeboxCacheInfo* pSafebox = nullptr;
		if (!Get(dwOwnerID, &pSafebox))
			return false;

		std::map<uint32_t, TItemInfoEx>::iterator it = pSafebox->itemsmap.find(dwItem);
		if (it != pSafebox->itemsmap.end())
			return false;

		pSafebox->itemsmap.insert(std::make_pair(dwItem, item));
		return true;
	}


	bool CSafeboxCache::RemoveItem(uint32_t dwOwnerID, uint32_t dwItemID)
	{
		TSafeboxCacheInfo* pSafebox = nullptr;
		if (!Get(dwOwnerID, &pSafebox))
			return false;

		std::map<uint32_t, TItemInfoEx>::iterator it = pSafebox->itemsmap.find(dwItemID);
		if (it == pSafebox->itemsmap.end())
			return false;

		pSafebox->itemsmap.erase(it);

		std::string query = CreateSafeboxCacheDeleteItemQuery(dwItemID);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_SAFEBOX_DELETE_ITEM, 0, nullptr);
		return true;
	}

	bool CSafeboxCache::AddItem(uint32_t dwOwnerID, const TItemInfoEx& item)
	{
		//TSafeboxCacheInfo* pSafebox = nullptr;
		//offlineshop-updated 04/08/19
		/*
		if (!Get(dwOwnerID, &pSafebox))
			return false;
		*/

		SQueryInfoSafeboxAddItem* qi = new SQueryInfoSafeboxAddItem;
		qi->dwOwnerID = dwOwnerID;
		CopyObject(qi->item, item);

		std::string query = CreateSafeboxCacheInsertItemQuery(dwOwnerID, item);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_SAFEBOX_ADD_ITEM, 0, qi);
		return true;
	}


	bool CSafeboxCache::AddValutes(uint32_t dwOwnerID, const TValutesInfo& val)
	{
		TSafeboxCacheInfo* pSafebox = nullptr;
		if (!Get(dwOwnerID, &pSafebox))
		{
			AddValutesAsQuery(dwOwnerID, val);
			return true;
		}

		pSafebox->valutes += val;

		std::string query = CreateSafeboxCacheUpdateValutes(dwOwnerID, pSafebox->valutes);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_SAFEBOX_UPDATE_VALUTES, 0, nullptr);
		return true;
	}


	void CSafeboxCache::AddValutesAsQuery(uint32_t dwOwnerID, const TValutesInfo& val)
	{
		std::string query=CreateSafeboxCacheUpdateValutesByAdding(dwOwnerID,val);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_SAFEBOX_UPDATE_VALUTES_ADDING, 0, nullptr);
	}



	bool CSafeboxCache::RemoveValutes(uint32_t dwOwnerID, const TValutesInfo& val)
	{
		TSafeboxCacheInfo* pSafebox = nullptr;
		if (!Get(dwOwnerID, &pSafebox))
			return false;

		pSafebox->valutes -= val;

		std::string query = CreateSafeboxCacheUpdateValutes(dwOwnerID, pSafebox->valutes);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_SAFEBOX_UPDATE_VALUTES, 0, nullptr);
		return true;
	}



	CSafeboxCache::TSafeboxCacheInfo* CSafeboxCache::CreateSafebox(uint32_t dwOwnerID)
	{
		if (!PutSafebox(dwOwnerID, TSafeboxCacheInfo()))
			return nullptr;

		std::string query = CreateSafeboxCacheInsertSafeboxValutesQuery(dwOwnerID);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_SAFEBOX_INSERT_VALUTES, 0, nullptr);

		CSafeboxCache::TSafeboxCacheInfo* pInfo=nullptr;
		Get(dwOwnerID, &pInfo);

		return pInfo;
	}




	CSafeboxCache::TSafeboxCacheInfo* CSafeboxCache::LoadSafebox(uint32_t dwPID)
	{
		TSafeboxCacheInfo* pSafebox = nullptr;
		if (Get(dwPID, &pSafebox))
			return pSafebox;

		TSafeboxCacheInfo safebox;
		MYSQL_ROW row;

		{
			std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(CreateSafeboxCacheLoadValutesQuery(dwPID).c_str()));
			if(pMsg->Get()->uiAffectedRows == 0)
				return CreateSafebox(dwPID);

			if (pMsg->Get()->uiAffectedRows != 1)
			{
				sys_err("multiple safebox rows for id %d ",dwPID);
				return nullptr;
			}

			if ((row = mysql_fetch_row(pMsg->Get()->pSQLResult))) {
				str_to_number(safebox.valutes.illYang, row[0]);
#ifdef __ENABLE_CHEQUE_SYSTEM__
				str_to_number(safebox.valutes.iCheque , row[1]);
#endif
			}
			
			else
			{
				sys_err("cannot fetch safebox row for id %d ",dwPID);
				return nullptr;
			}
		}
		
		{
			std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(CreateSafeboxCacheLoadItemsQuery(dwPID).c_str()));
			uint32_t dwItemID =0;
			TItemInfoEx item;

			while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
			{
				//item_id, vnum , count , sockets, attrs

				int col=0;

				str_to_number(dwItemID ,		row[col++]);
				str_to_number(item.dwVnum,		row[col++]);
				str_to_number(item.dwCount,		row[col++]);
				
				for(int i=0; i < ITEM_SOCKET_MAX_NUM; i++)
					str_to_number(item.alSockets[i] , row[col++]);

				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
				{
					str_to_number(item.aAttr[i].bType , row[col++]);
					str_to_number(item.aAttr[i].sValue, row[col++]);
				}
				
#ifdef __ENABLE_CHANGELOOK_SYSTEM__
				str_to_number(item.dwTransmutation, row[col++]);
#endif
#ifdef ATTR_LOCK
				str_to_number(item.iLockedAttr, row[col++]);
#endif
				//patch 08-03-2020
				uint8_t exp = 0;
				str_to_number(exp, row[col++]);
				item.expiration = offlineshop::ExpirationType(exp);

				safebox.itemsmap.insert(std::make_pair(dwItemID, item));
			}
		}


		CACHEMAP::iterator it= m_safeboxMap.insert(std::make_pair(dwPID , safebox)).first;
		return &it->second;
	}




	uint32_t CSafeboxCache::GetItemCount() const
	{
		uint32_t dwItemCount=0;
		CACHEMAP::const_iterator it = m_safeboxMap.begin();
		for (; it != m_safeboxMap.end(); it++)
			dwItemCount+= it->second.itemsmap.size();
		
		return dwItemCount;
	}

	//patch 08-03-2020
	void CSafeboxCache::ItemExpirationProcess() {
		std::vector<std::pair<uint32_t, uint32_t>> vec;

		//if u are getting error here it's possible u are not using c++11.
		//what you need to do is to comment the part under c++11 tag end to remove
		//the comment under c++03 tag, good luck! -ikarus

		//c++11 
		const auto now = time(nullptr);
		for (auto& iter : m_safeboxMap) {
			auto& info = iter.second;
			
			for (auto& item : info.itemsmap) {
				auto& item_info = item.second;
				if (item_info.expiration == offlineshop::ExpirationType::EXPIRE_REAL_TIME) {
					if (now > item_info.alSockets[0])
						vec.emplace_back(std::make_pair(iter.first , item.first));
				} else if(item_info.expiration == offlineshop::ExpirationType::EXPIRE_REAL_TIME_FIRST_USE){
					if (item_info.alSockets[1] != 0 && item_info.alSockets[0] < now)
						vec.emplace_back(std::make_pair(iter.first, item.first));
				}
			}
		}

		for (const auto& data : vec) {
			RemoveItem(data.first, data.second);
			CClientManager::instance().SendOfflineshopSafeboxExpiredItem(data.first, data.second);
		}



		//c++03
		/*const time_t now = time(0);
		itertype(m_safeboxMap) it = m_safeboxMap.begin();
		itertype(m_safeboxMap) end = m_safeboxMap.end();
		while(it != end){
			TSafeboxCacheInfo& info = it->second;

			itertype(info.itemsmap) item_it  = info.itemsmap.begin();
			itertype(info.itemsmap) item_end = info.itemsmap.end();

			while (item_it != item_end) {
				TItemInfoEx& item_info = item_it->second;
				if (item_info.expiration == offlineshop::ExpirationType::EXPIRE_REAL_TIME) {
					if (now > item_info.alSockets[0])
						vec.push_back(std::make_pair(it->first, item_it->first));
				}
				else if (item_info.expiration == offlineshop::ExpirationType::EXPIRE_REAL_TIME_FIRST_USE) {
					if (item_info.alSockets[1] != 0 && item_info.alSockets[0] < now)
						vec.push_back(std::make_pair(it->first, item_it->first));
				} item_it++;
			} it++;
		}


		itertype(vec) vec_it = vec.begin();
		itertype(vec) vec_end = vec.end();
		while(vec_it != vec_end) {
			RemoveItem(vec_it->first, vec_it->second);
			CClientManager::instance().SendOfflineshopSafeboxExpiredItem(vec_it->first, vec_it->second);
			vec_it++;
		}*/
	}

	


	//OFFER CACHE
	COfferCache::COfferCache()
	{
	}


	COfferCache::~COfferCache()
	{
	}



	bool COfferCache::Puts(uint32_t dwOfferID, const TOfferCacheInfo& rOffer)
	{
		CONST_CACHEITER it = m_mapOffer.find(dwOfferID);
		if (it != m_mapOffer.end())
			return false;

		m_mapOffer.insert(std::make_pair(dwOfferID, rOffer));
		
		//offlineshop-updated 04/08/19
		OFFERIDVEC& vec = m_findOffersByShop[rOffer.dwOwnerID];
		vec.push_back(dwOfferID);
		
		return true;
	}


	bool COfferCache::Get(uint32_t dwOfferID, TOfferCacheInfo** ppOffer) const
	{
		CONST_CACHEITER it = m_mapOffer.find(dwOfferID);
		if (it == m_mapOffer.end())
			return false;

		*ppOffer = (TOfferCacheInfo*)&(it->second);
		return true;
	}



	bool COfferCache::AddOffer(const TOfferCacheInfo& rOffer)
	{
		SQueryInfoOfferAdd* qi = new SQueryInfoOfferAdd;
		CopyObject(qi->offer, rOffer);

		std::string query = CreateOfferCacheInsertOfferQuery(rOffer);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_OFFER_ADD, 0, qi);
		return true;
	}



	bool COfferCache::NoticedOffer(uint32_t dwOfferID)
	{
		TOfferCacheInfo* pOffer = nullptr;
		if (!Get(dwOfferID, &pOffer))
			return false;

		pOffer->bNoticed = true;
		std::string query = CreateOfferCacheUpdateNotifiedQuery(dwOfferID);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_OFFER_UPDATE_NOTIFIED, 0, nullptr);
		return true;
	}



	bool COfferCache::CancelOffer(uint32_t dwOfferID, uint32_t dwOwnerID) //offlineshop-updated 04/08/19 reminder
	{
		TOfferCacheInfo* pOffer = nullptr;
		if (!Get(dwOfferID, &pOffer))
			return false;


		FINDOFFERBYSHOP::iterator it=m_findOffersByShop.find(dwOwnerID);
		if(it==m_findOffersByShop.end())
			return false;

		OFFERIDVEC& vec= it->second;
		for(OFFERIDVEC::iterator itID = vec.begin() ; itID!= vec.end(); itID++)
		{
			if (*itID == dwOfferID)
			{
				vec.erase(itID);
				break;
			}
		}

		if(vec.empty())
			m_findOffersByShop.erase(it);
		m_mapOffer.erase(m_mapOffer.find(dwOfferID));


		std::string query = CreateOfferCacheDeleteOfferQuery(dwOfferID);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_OFFER_DELETE, 0, nullptr);
		return true;
	}


	bool COfferCache::AcceptOffer(uint32_t dwOfferID)
	{
		TOfferCacheInfo* pOffer = nullptr;
		if (!Get(dwOfferID, &pOffer))
			return false;

		pOffer->bAccepted = true;
		std::string query = CreateOfferCacheUpdateAcceptedQuery(dwOfferID);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_OFFER_UPDATE_ACCEPTED, 0, nullptr);
		return true;
	}



	bool COfferCache::RemoveOffersByShopOwner(uint32_t dwShopOwnerID)
	{
		FINDOFFERBYSHOP::iterator it=m_findOffersByShop.find(dwShopOwnerID);
		if(it==m_findOffersByShop.end())
			return false;

		OFFERIDVEC& vec= it->second;
		for(OFFERIDVEC::iterator itID = vec.begin() ; itID!= vec.end(); itID++)
		{
			CACHEITER itCache = m_mapOffer.find(*itID);
			if(itCache!=m_mapOffer.end())
				m_mapOffer.erase(itCache);
		}


		m_findOffersByShop.erase(it);
		std::string query = CreateOfferCacheRemoveOfferByShopOwner(dwShopOwnerID);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_OFFER_DELETE_BY_SHOP, 0, nullptr);
		return true;
	}

	void COfferCache::EncodeCache(CPeer* peer) const
	{
		CONST_CACHEITER it = m_mapOffer.begin();
		TOfferInfo offer;

		for (; it != m_mapOffer.end(); it++)
		{
			uint32_t dwOfferID					= it->first;
			const TOfferCacheInfo &	rOffer	= it->second;

			CopyObject(offer.price, rOffer.price);
			
			offer.dwOfferID		= dwOfferID;
			offer.dwOffererID	= rOffer.dwOffererID;
			offer.dwOwnerID		= rOffer.dwOwnerID;
			offer.bNoticed		= rOffer.bNoticed;
			offer.dwItemID		= rOffer.dwItemID;
			offer.bAccepted		= rOffer.bAccepted;
			offer.bNoticed		= rOffer.bNoticed;

			//offlineshop-updated 03/08/19
			strncpy(offer.szBuyerName, rOffer.szBuyerName, sizeof(offer.szBuyerName));

			peer->Encode(&offer, sizeof(offer));
		}
	}


	bool COfferCache::GetOffersByShopOwner(uint32_t dwOwnerID , COfferCache::OFFERIDVEC** ppVec)
	{
		FINDOFFERBYSHOP::iterator it =  m_findOffersByShop.find(dwOwnerID);
		if(it == m_findOffersByShop.end())
			return false;

		*ppVec = &(it->second);
		return true;
	}


	bool COfferCache::GetOffersByItemID(uint32_t dwItemID, std::vector<uint32_t>& offerIDs)
	{
		offerIDs.clear();
		auto it = m_mapOffer.begin();
		for (; it != m_mapOffer.end(); it++)
		{
			if(it->second.dwItemID == dwItemID)
				offerIDs.push_back(it->first);
		}

		return !offerIDs.empty();
	}







	//AUCTION
	CAuctionCache::CAuctionCache() 
	{
	}


	CAuctionCache::~CAuctionCache()
	{
	}


	bool CAuctionCache::PutsAuction(const TAuctionInfo& auction)
	{
		m_mapAuction[auction.dwOwnerID]			= auction;
		m_mapAuctionOffer[auction.dwOwnerID]	= std::vector<TAuctionOfferInfo>();

		return true;
	}




	bool CAuctionCache::PutsAuctionOffer(const TAuctionOfferInfo& auctionOffer)
	{
		AUCTIONOFFERVEC& vec= m_mapAuctionOffer[auctionOffer.dwOwnerID];
		vec.push_back(auctionOffer);
		return true;
	}



	bool CAuctionCache::AddAuction(const TAuctionInfo& auction)
	{
		OFFSHOP_DEBUG("auction %u , %u min",auction.dwOwnerID, auction.dwDuration);

		std::string query = CreateAuctionCacheAddAuctionQuery(auction);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_AUCTION_INSERT, 0, nullptr);

		OFFSHOP_DEBUG("query:%s", query.c_str());

		PutsAuction(auction);
		return true;
	}



	bool CAuctionCache::AddOffer(const TAuctionOfferInfo& auctionOffer, bool quering)
	{
		if (quering)
		{
			std::string query = CreateAuctionCacheAddOfferQuery(auctionOffer);
			CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_AUCTION_INSERT_OFFER, 0 , nullptr);
			if (m_mapAuction[auctionOffer.dwOwnerID].dwDuration == 0) // +
				m_mapAuction[auctionOffer.dwOwnerID].dwDuration = 1; // +
		}
		

		PutsAuctionOffer(auctionOffer);
		return true;
	}




	bool CAuctionCache::ExpiredAuction(uint32_t dwOwnerID)
	{
		
		auto		it		= m_mapAuction.find(dwOwnerID);
		auto	itOff	= m_mapAuctionOffer.find(dwOwnerID);

		if(it !=m_mapAuction.end())
			m_mapAuction.erase(it);

		if(itOff !=m_mapAuctionOffer.end())
			m_mapAuctionOffer.erase(itOff);
		

		CDBManager::instance().ReturnQuery(CreateAuctionCacheDeleteAuction(dwOwnerID).c_str(), QID_OFFLINESHOP_AUCTION_DELETE, 0, nullptr);
		CDBManager::instance().ReturnQuery(CreateAuctionCacheDeleteAuctionOffers(dwOwnerID).c_str(), QID_OFFLINESHOP_AUCTION_DELETE_OFFERS, 0 , nullptr);
		return true;
	}



	//patch 08-03-2020
	bool CAuctionCache::AuctionDurationProcess()
	{
		if(m_mapAuction.empty())
			return false;

		std::vector<uint32_t> owners;

		//item expiration
		std::vector<uint32_t> items_expired;
		const time_t now = time(nullptr);

		auto it = m_mapAuction.begin();
		for (; it != m_mapAuction.end(); it++)
		{
			TAuctionInfo& auction = it->second;

			//check about the item expiration
			TItemInfoEx& itemInfo = auction.item;
			if (itemInfo.expiration == offlineshop::ExpirationType::EXPIRE_REAL_TIME) {
				if (now > itemInfo.alSockets[0]) {
					items_expired.push_back(it->first);
					continue;
				}
			}
			else if (itemInfo.expiration == offlineshop::ExpirationType::EXPIRE_REAL_TIME_FIRST_USE) {
				if (itemInfo.alSockets[1] != 0 && itemInfo.alSockets[0] < now) {
					items_expired.push_back(it->first);
					continue;
				}
			}



			if (auction.dwDuration != 0)
			{
				if(--auction.dwDuration%5==0 && auction.dwDuration!=0)
					__UpdateDuration(auction);
			}


			else
			{
				owners.push_back(it->first);
			}
		}


		for(uint32_t i=0; i < owners.size(); i++)
			CClientManager::instance().OfflineshopExpiredAuction(owners[i]);

		auto exp_it = items_expired.begin();
		auto end_it = items_expired.end();

		for (; exp_it != end_it; exp_it++) {
			CClientManager::instance().OfflineshopExpiredAuctionItem(*exp_it);
		}


		return true;
	}



	void CAuctionCache::__UpdateDuration(const TAuctionInfo & auction)
	{	
		std::string query = CreateAuctionCacheUpdateDurationQuery(auction.dwOwnerID, auction.dwDuration);
		CDBManager::instance().ReturnQuery(query.c_str(), QID_OFFLINESHOP_AUCTION_UPDATE_DURATION, 0 , nullptr);
	}






	uint32_t CAuctionCache::GetBestBuyer(uint32_t dwOwnerID, TAuctionOfferInfo** ppOffer)
	{
		*ppOffer = nullptr;
		AUCTIONOFFERVEC& vec = m_mapAuctionOffer[dwOwnerID];

		if(vec.empty())
			return 0;

		uint32_t dwBest=0;
		TPriceInfo* pBestPrice=nullptr;
		
		for (auto it = vec.begin(); it != vec.end(); it++)
		{
			if (!pBestPrice || *pBestPrice < it->price)
			{
				pBestPrice	= &it->price;
				dwBest		= it->dwBuyerID;
				*ppOffer	= &(*it);
				continue;
			}
		}

		return dwBest;
	}



	bool CAuctionCache::Get(uint32_t dwOwnerID, TAuctionInfo** ppInfo)
	{
		*ppInfo=nullptr;

		if(const auto it = m_mapAuction.find(dwOwnerID); it!=m_mapAuction.end())
			*ppInfo = &(it->second);

		return *ppInfo != nullptr;
	}



	bool CAuctionCache::GetOffers(uint32_t dwOwnerID, std::vector<TAuctionOfferInfo>** ppVec)
	{
		*ppVec=nullptr;

		if(const auto it = m_mapAuctionOffer.find(dwOwnerID); it !=m_mapAuctionOffer.end())
			*ppVec = &(it->second);

		return *ppVec != nullptr;
	}


	uint32_t CAuctionCache::GetCount()
	{
		return m_mapAuction.size();
	}



	uint32_t CAuctionCache::GetOffersCount()
	{
		uint32_t dwCount =0;
		for(auto it = m_mapAuctionOffer.begin(); it != m_mapAuctionOffer.end(); ++it)
			dwCount += it->second.size();

		return dwCount;
	}




	void CAuctionCache::EncodeCache(CPeer* peer)
	{
		for(auto it = m_mapAuction.begin(); it != m_mapAuction.end(); ++it)
			peer->Encode(&it->second, sizeof(it->second));

		for(auto it = m_mapAuctionOffer.begin(); it != m_mapAuctionOffer.end(); ++it)
			if(!it->second.empty())
				peer->Encode(&it->second[0], sizeof(it->second[0])* it->second.size());
	}


















	// QUERY MAKE
	std::string CreateShopCacheInsertItemQuery(uint32_t dwOwner, const CShopCache::TShopCacheItemInfo& rItem)
	{
		//item_id, owner_id, price_yang, price_cheque, vnum, count,	socket0, socket1, socket2, attr0, attrval0,	attr1, attrval1
		//attr2, attrval2,attr3,attrval3,attr4,attrval4,attr5,attrval5,attr6,attrval6

		char szQuery[1024] = "INSERT INTO `player`.`offlineshop_shop_items` (`item_id`, `owner_id`, `price_yang`, "
#ifdef __ENABLE_CHEQUE_SYSTEM__
			"`price_cheque`,"
#endif
			" `vnum`, `count` ";
		size_t len = strlen(szQuery);

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				",`socket%d` ", i);

		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				",`attr%d` , `attrval%d` ", i, i);

		len += snprintf(szQuery + len, sizeof(szQuery) - len, "%s", " "
#ifdef __ENABLE_CHANGELOOK_SYSTEM__
		", `trans` "
#endif
#ifdef ATTR_LOCK
		", `locked_attr`"
#endif
		//patch 08-03-2020
		", expiration "

		") VALUES (");
		len += snprintf(szQuery + len, sizeof(szQuery) - len, "0, %u, %lld,"
#ifdef __ENABLE_CHEQUE_SYSTEM__
			" %d, "
#endif
			" %u, %u ",
			dwOwner, rItem.price.illYang,
#ifdef __ENABLE_CHEQUE_SYSTEM__
			rItem.price.iCheque,
#endif
			rItem.item.dwVnum, rItem.item.dwCount

		);


		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				",%ld ", rItem.item.alSockets[i]);

		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				", %d , %d ", rItem.item.aAttr[i].bType, rItem.item.aAttr[i].sValue);

#ifdef __ENABLE_CHANGELOOK_SYSTEM__
		len += snprintf(szQuery + len, sizeof(szQuery) - len, " , %u ", rItem.item.dwTransmutation );
#endif
#ifdef ATTR_LOCK
		len += snprintf(szQuery + len, sizeof(szQuery) - len, " , %d ", rItem.item.iLockedAttr);
#endif
		//patch 08-03-2020
		len += snprintf(szQuery + len, sizeof(szQuery) - len, ", %u ", (uint8_t)rItem.item.expiration);


		std::string query = szQuery;
		query += ");";

		return query;
	}


	std::string CreateShopCacheUpdateItemQuery(uint32_t dwItemID, const TPriceInfo& rItemPrice)
	{
		static char szQuery[256];
		snprintf(szQuery, sizeof(szQuery), "UPDATE `player`.`offlineshop_shop_items` SET `price_yang` = %lld "
#ifdef __ENABLE_CHEQUE_SYSTEM__
			", `price_cheque` = %d "
#endif
			" WHERE `item_id` = %u;",
			rItemPrice.illYang,
#ifdef __ENABLE_CHEQUE_SYSTEM__
			rItemPrice.iCheque,
#endif

			dwItemID);

		return szQuery;
	}

	std::string CreateShopCacheDeleteShopQuery(uint32_t dwOwner)
	{
		static char szQuery[128];
		snprintf(szQuery, sizeof(szQuery), "DELETE from `player`.`offlineshop_shops` WHERE `owner_id` = %d;", dwOwner);
		return szQuery;
	}


	std::string CreateShopCacheDeleteShopItemQuery(uint32_t dwOwner)
	{
		static char szQuery[128];
		snprintf(szQuery, sizeof(szQuery), "DELETE from `player`.`offlineshop_shop_items` WHERE `owner_id` = %d;", dwOwner);
		return szQuery;
	}


	std::string CreateShopCacheInsertShopQuery(uint32_t dwOwnerID, uint32_t dwDuration, const char* name
#ifdef KASMIR_PAKET_SYSTEM
	, uint32_t dwKasmirNpc
#endif
	)
	{
		static char szQuery[256];
		char szEscapeString[OFFLINE_SHOP_NAME_MAX_LEN + 32];

		CDBManager::instance().EscapeString(szEscapeString, name, strlen(name));
#ifdef KASMIR_PAKET_SYSTEM
		snprintf(szQuery, sizeof(szQuery), "INSERT INTO `player`.`offlineshop_shops` (`owner_id`, `duration`, `name`, `npc`) VALUES(%u, %u, '%s', %u);", dwOwnerID, dwDuration, szEscapeString, dwKasmirNpc);
#else
		snprintf(szQuery, sizeof(szQuery), "INSERT INTO `player`.`offlineshop_shops` (`owner_id`, `duration`, `name`) VALUES(%u, %u, '%s');", dwOwnerID, dwDuration, szEscapeString);
#endif
		return szQuery;
	}


	std::string CreateShopCacheUpdateShopNameQuery(uint32_t dwOwnerID, const char* name)
	{
		static char szQuery[256];
		static char szEscapeString[OFFLINE_SHOP_NAME_MAX_LEN + 32];
		CDBManager::instance().EscapeString(szEscapeString, name, strlen(name));

		snprintf(szQuery, sizeof(szQuery), "UPDATE `player`.`offlineshop_shops` SET `name` = '%s' WHERE `owner_id` = %u;", szEscapeString, dwOwnerID);
		return szQuery;
	}



	std::string CreateShopCacheUpdateDurationQuery(uint32_t dwOwnerID, uint32_t dwDuration)
	{
		static char szQuery[256];
		snprintf(szQuery, sizeof(szQuery), "UPDATE `player`.`offlineshop_shops` SET `duration` = '%d' WHERE `owner_id` = %u;", dwDuration, dwOwnerID);
		return szQuery;
	}


	std::string CreateSafeboxCacheDeleteItemQuery(uint32_t dwItem)
	{
		static char szQuery[128];
		snprintf(szQuery, sizeof(szQuery), "DELETE from `player`.`offlineshop_safebox_items` WHERE `item_id` = %d;", dwItem);
		return szQuery;
	}


	std::string CreateSafeboxCacheInsertItemQuery(uint32_t dwOwner, const TItemInfoEx& item)
	{
		char szQuery[1024] = "INSERT INTO `player`.`offlineshop_safebox_items` (`item_id`, `owner_id`, `vnum`, `count` ";
		size_t len = strlen(szQuery);

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				",`socket%d` ", i);

		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				",`attr%d` , `attrval%d` ", i, i);

		len += snprintf(szQuery + len, sizeof(szQuery) - len, "%s", "  "
#ifdef __ENABLE_CHANGELOOK_SYSTEM__
		" , `trans` "
#endif
#ifdef ATTR_LOCK
		", `locked_attr`"
#endif
		//patch 08-03-2020
		", expiration "

		") VALUES (");
		len += snprintf(szQuery + len, sizeof(szQuery) - len, "0, %u, %u, %u ",
			dwOwner, item.dwVnum, item.dwCount
		);


		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				",%ld ", item.alSockets[i]);

		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				", %d , %d ", item.aAttr[i].bType, item.aAttr[i].sValue);

#ifdef __ENABLE_CHANGELOOK_SYSTEM__
		len += snprintf(szQuery + len, sizeof(szQuery) - len,
			", %u ", item.dwTransmutation );
#endif
#ifdef ATTR_LOCK
		len += snprintf(szQuery + len, sizeof(szQuery) - len,
			", %d ", item.iLockedAttr );
#endif
		//patch 08-03-2020
		len += snprintf(szQuery + len, sizeof(szQuery) - len, ", %u ", (uint8_t)item.expiration);

		std::string query = szQuery;
		query += ");";

		return query;
	}


	std::string CreateSafeboxCacheUpdateValutes(uint32_t dwOwner, const TValutesInfo& val)
	{
		static char szQuery[256];
		snprintf(szQuery, sizeof(szQuery), "UPDATE `player`.`offlineshop_safebox_valutes` SET `yang` = %lld "
#ifdef __ENABLE_CHEQUE_SYSTEM__
			", `cheque` = %d "
#endif
			" WHERE `owner_id` = %u ;", 
			val.illYang,
#ifdef __ENABLE_CHEQUE_SYSTEM__
			val.iCheque,
#endif
			dwOwner);

		return szQuery;
	}


	std::string CreateSafeboxCacheUpdateValutesByAdding(uint32_t dwOwner, const TValutesInfo& val)
	{
		static char szQuery[256];
		snprintf(szQuery, sizeof(szQuery), "UPDATE `player`.`offlineshop_safebox_valutes` SET `yang` = `yang`+%lld "
#ifdef __ENABLE_CHEQUE_SYSTEM__
			", `cheque` = `cheque`+%d "
#endif
			" WHERE `owner_id` = %u ;", val.illYang,
#ifdef __ENABLE_CHEQUE_SYSTEM__
			val.iCheque,
#endif
			dwOwner);

		return szQuery;
	}

	std::string CreateSafeboxCacheInsertSafeboxValutesQuery(uint32_t dwOwnerID)
	{
		static char szQuery[256];
		snprintf(szQuery, sizeof(szQuery), "INSERT INTO `player`.`offlineshop_safebox_valutes` (`owner_id`,`yang`,`cheque`) VALUES( %u , 0,0);", dwOwnerID);
		return szQuery;
	}



	std::string CreateSafeboxCacheLoadValutesQuery(uint32_t dwOwnerID)
	{
		static char szQuery[256];
		snprintf(szQuery, sizeof(szQuery), "SELECT `yang` "
#ifdef __ENABLE_CHEQUE_SYSTEM__
			", `cheque` "
#endif
			" from `player`.`offlineshop_safebox_valutes` WHERE `owner_id` = %d;", dwOwnerID);
		return szQuery;
	}

	std::string CreateSafeboxCacheLoadItemsQuery(uint32_t dwOwnerID)
	{
		char szQuery[1024] = "SELECT `item_id`, `vnum`, `count` ";
		size_t len = strlen(szQuery);

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				",`socket%d` ", i);

		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				",`attr%d` , `attrval%d` ", i, i);

		len += snprintf(szQuery + len, sizeof(szQuery) - len, "%s%u;", " "

#ifdef __ENABLE_CHANGELOOK_SYSTEM__
		" , `trans` "
#endif
#ifdef ATTR_LOCK
		" , `locked_attr` "
#endif
		//patch 08-03-2020
		", expiration "

		" from `player`.`offlineshop_safebox_items` WHERE `owner_id`=", dwOwnerID);
		
		return szQuery;
	}




	//OFFERCHACHE
	std::string CreateOfferCacheInsertOfferQuery(const COfferCache::TOfferCacheInfo& rOffer)
	{
		//offlineshop-updated 03/08/19
		static char szEscapeString[CHARACTER_NAME_MAX_LEN + 32];
		CDBManager::instance().EscapeString(szEscapeString, rOffer.szBuyerName, strnlen(rOffer.szBuyerName, sizeof(rOffer.szBuyerName)));
		// offer_id,owner_id,offerer_id,price_yang,price_cheque,is_notified,is_accept

		static char szQuery[256];
		snprintf(szQuery, sizeof(szQuery), "INSERT INTO `player`.`offlineshop_offers` (`offer_id`,`owner_id`,`offerer_id`, `item_id`, `price_yang`,"/*`price_cheque`,*/"`is_notified`,`is_accept`, `buyer_name`) VALUES"
											"(0, %u, %u, %u, %lld, %u, %u, '%s')",
											rOffer.dwOwnerID, rOffer.dwOffererID, rOffer.dwItemID, rOffer.price.illYang, rOffer.bNoticed?1:0, rOffer.bAccepted?1:0, szEscapeString
		);

		return szQuery;
	}


	std::string CreateOfferCacheUpdateNotifiedQuery(uint32_t dwOfferID)
	{
		static char szQuery[128];
		snprintf(szQuery, sizeof(szQuery),"UPDATE `player`.`offlineshop_offers` SET `is_notified` = 1 WHERE `offer_id` = %u ;",dwOfferID);
		return szQuery;
	}


	std::string CreateOfferCacheUpdateAcceptedQuery(uint32_t dwOfferID)
	{
		static char szQuery[128];
		snprintf(szQuery, sizeof(szQuery),"UPDATE `player`.`offlineshop_offers` SET `is_accept` = 1 WHERE `offer_id` = %u ;",dwOfferID);
		return szQuery;
	}



	std::string CreateOfferCacheRemoveOfferByShopOwner(uint32_t dwShopOwner)
	{
		static char szQuery[128];
		snprintf(szQuery, sizeof(szQuery),"DELETE from `player`.`offlineshop_offers`  WHERE `owner_id` = %u ;",dwShopOwner);
		return szQuery;
	}




	std::string CreateOfferCacheDeleteOfferQuery(uint32_t dwOfferID)
	{
		static char szQuery[128];
		snprintf(szQuery, sizeof(szQuery),"DELETE from `player`.`offlineshop_offers`  WHERE `offer_id` = %u ;",dwOfferID);
		return szQuery;
	}




	std::string CreateShopCacheDeleteItemQuery(uint32_t dwOwnerID, uint32_t dwItemID)
	{
		static char szQuery[128];
		snprintf(szQuery, sizeof(szQuery),"DELETE from `player`.`offlineshop_shop_items`  WHERE `owner_id` = %u AND item_id = '%u' ;",dwOwnerID, dwItemID);
		return szQuery;
	}


	std::string CreateShopCacheUpdateSoldItemQuery(uint32_t dwOwnerID, uint32_t dwItemID)
	{
		static char szQuery[128];
		snprintf(szQuery, sizeof(szQuery),"UPDATE `player`.`offlineshop_shop_items` SET `is_sold` = 1  WHERE `owner_id` = %u AND item_id = '%u' ;",dwOwnerID, dwItemID);
		return szQuery;
	}





	std::string CreateAuctionCacheAddAuctionQuery(const TAuctionInfo& auction)
	{
		static char szQuery[1024];
		size_t len = snprintf(szQuery, sizeof(szQuery), "%s" , "INSERT INTO `player`.`offlineshop_auctions` (`owner_id`, `duration`, `name` , `vnum`, `count` , `init_yang` ");


		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				",`socket%d` ", i);

		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				",`attr%d` , `attrval%d` ", i, i);

		static char szEscapeName[CHARACTER_NAME_MAX_LEN+32];
		CDBManager::instance().EscapeString(szEscapeName, auction.szOwnerName, strnlen(auction.szOwnerName , sizeof(auction.szOwnerName)));

		
		len += snprintf(szQuery + len, sizeof(szQuery)-len, "  "
#ifdef __ENABLE_CHANGELOOK_SYSTEM__
			" , `trans` "
#endif
#ifdef ATTR_LOCK
			" , `locked_attr`"
#endif
			//patch 08-03-2020
			", expiration "

			") VALUES( %u, %u, '%s', %u, %u ,%lld ",
			auction.dwOwnerID, auction.dwDuration, szEscapeName, auction.item.dwVnum, auction.item.dwCount, auction.init_price.illYang

		);


		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				",%ld ", auction.item.alSockets[i]);

		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
			len += snprintf(szQuery + len, sizeof(szQuery) - len,
				",%u , %d ", auction.item.aAttr[i].bType, auction.item.aAttr[i].sValue);
		
#ifdef __ENABLE_CHANGELOOK_SYSTEM__
		len += snprintf(szQuery + len, sizeof(szQuery)-len, " , %u ",auction.item.dwTransmutation);
#endif
#ifdef ATTR_LOCK
		len += snprintf(szQuery + len, sizeof(szQuery) - len, " , %d ", auction.item.iLockedAttr);
#endif
		//patch 08-03-2020
		len += snprintf(szQuery + len, sizeof(szQuery) - len, ", %u ", (uint8_t)auction.item.expiration);
		
		len += snprintf(szQuery + len, sizeof(szQuery)-len, "%s",");");
		return szQuery;
	}


	std::string CreateAuctionCacheAddOfferQuery(const TAuctionOfferInfo& auctionOffer)
	{
		static char szEscapeName[CHARACTER_NAME_MAX_LEN+32];
		CDBManager::instance().EscapeString(szEscapeName, auctionOffer.szBuyerName, strnlen(auctionOffer.szBuyerName , sizeof(auctionOffer.szBuyerName)));


		//owner_id 		buyer_id			buyer_name			yang
		static char szQuery[512];
		snprintf(szQuery, sizeof(szQuery), "INSERT INTO `player`.`offlineshop_auction_offers` (`owner_id`, `buyer_id`, `buyer_name` , `yang`) "
																"VALUES( %u, %u, '%s' , %lld );",
																auctionOffer.dwOwnerID, auctionOffer.dwBuyerID, szEscapeName, auctionOffer.price.illYang
		);
		
		return szQuery;
	}


	std::string CreateAuctionCacheDeleteAuction(uint32_t dwOwnerID)
	{
		static char szQuery[128];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM `player`.`offlineshop_auctions` WHERE `owner_id` = %u;",dwOwnerID);
		return szQuery;
	}


	std::string CreateAuctionCacheDeleteAuctionOffers(uint32_t dwOwnerID)
	{
		static char szQuery[128];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM `player`.`offlineshop_auction_offers` WHERE `owner_id` = %u;",dwOwnerID);
		return szQuery;
	}



	std::string CreateAuctionCacheUpdateDurationQuery(uint32_t dwOwnerID, uint32_t dwDuration)
	{
		static char szQuery[128];
		snprintf(szQuery, sizeof(szQuery), "UPDATE `player`.`offlineshop_auctions` SET `duration` = %u WHERE `owner_id` = %u;",dwDuration, dwOwnerID);
		return szQuery;
	}

}


#endif


