#include "stdafx.h"
#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "Main.h"
#include "Config.h"
#include "DBManager.h"
#include "QID.h"
#include "Peer.h"
#include "OfflineshopCache.h"
#include "ClientManager.h"


#define DIRECT_QUERY(query) CDBManager::instance().DirectQuery((query))

std::string CreateOfflineshopSelectShopItemsQuery();
std::string CreateOfflineshopSelectAuctionsQuery();



template <class T>
const char* Decode(T*& pObj, const char* data){
	pObj = (T*) data;
	return data + sizeof(T);
}




bool CClientManager::InitializeOfflineshopTable()
{
	
	MYSQL_ROW row;
	OFFSHOP_DEBUG("starting to loading offlineshop %s", "table");
	//offlineshop_shops
	{
#ifdef KASMIR_PAKET_SYSTEM
		const char szQuery[] = "SELECT `owner_id` , `name` , `duration`, `npc` FROM `player`.`offlineshop_shops`;";
#else
		const char szQuery[] = "SELECT `owner_id` , `name` , `duration` FROM `player`.`offlineshop_shops`;";
#endif
		std::unique_ptr<SQLMsg> pMsg(DIRECT_QUERY(szQuery));

		if (pMsg->uiSQLErrno != 0)
		{
			LOG_ERROR("CANNOT LOAD offlineshop_shops TABLE , errorcode {} ", pMsg->uiSQLErrno);
			return false;
		}

		if (pMsg->Get())
		{
			while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
			{
				int col=0;
				std::string name="";
				uint32_t dwOwner =0, dwDuration=0;

				str_to_number(dwOwner, row[col++]);
				name = row[col++];
				str_to_number(dwDuration, row[col++]);
#ifdef KASMIR_PAKET_SYSTEM
				uint32_t dwKasmirNpc = 0;
				str_to_number(dwKasmirNpc, row[col++]);
#endif
				OFFSHOP_DEBUG("owner %u , duration %u ",dwOwner, dwDuration);
				if(!m_offlineshopShopCache.PutShop(dwOwner, dwDuration, name.c_str()
#ifdef KASMIR_PAKET_SYSTEM
				, dwKasmirNpc
#endif
				))
					LOG_ERROR("cannot execute putsShop -> double shop id?! {} ", dwOwner);
			}
		}
	}



	//offlineshop_shop_items
	{
		//item_id,owner_id,price_yang,price_cheque,vnum,count,
		//socket0,socket1,socket2,attr0,attrval0,attr1,attrval1,
		//attr2,attrval2,attr3,attrval3,attr4,attrval4,attr5,attrval5,attr6,attrval6

		std::string query = CreateOfflineshopSelectShopItemsQuery();
		std::unique_ptr<SQLMsg> pMsg(DIRECT_QUERY(query.c_str()));

		if (pMsg->uiSQLErrno != 0)
		{
			LOG_ERROR("CANNOT LOAD offlineshop_shop_items TABLE , errorcode {} ", pMsg->uiSQLErrno);
			return false;
		}


		if (pMsg->Get())
		{
			while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
			{
				int col=0;
				uint32_t dwItemID=0, dwOwnerID=0;
				
				offlineshop::
				CShopCache::
				TShopCacheItemInfo item;
				offlineshop::ZeroObject(item);

				str_to_number(dwItemID,				row[col++]);
				str_to_number(dwOwnerID,			row[col++]);
				str_to_number(item.price.illYang,	row[col++]);
#ifdef __ENABLE_CHEQUE_SYSTEM__
				str_to_number(item.price.iCheque, 	row[col++]);
#endif

				str_to_number(item.item.dwVnum,		row[col++]);
				str_to_number(item.item.dwCount,	row[col++]);

				for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
					str_to_number(item.item.alSockets[i], row[col++]);
				
				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
				{
					str_to_number(item.item.aAttr[i].bType,		row[col++]);
					str_to_number(item.item.aAttr[i].sValue,	row[col++]);
				}

				uint8_t bIsSold=0;
				str_to_number(bIsSold , row[col++]);

#ifdef __ENABLE_CHANGELOOK_SYSTEM__
				str_to_number(item.item.dwTransmutation, row[col++]);
#endif
#ifdef ATTR_LOCK
				str_to_number(item.item.iLockedAttr, row[col++]);
#endif
				//patch 08-03-2020
				uint8_t bExpiration = 0;
				str_to_number(bExpiration, row[col++]);
				item.item.expiration = offlineshop::ExpirationType(bExpiration);

				bool isSold = bIsSold==1;

				OFFSHOP_DEBUG("owner %u ,itemid %u ",dwOwnerID, dwItemID);

				if(!m_offlineshopShopCache.PutItem(dwOwnerID, dwItemID, item, isSold))
					LOG_ERROR("cannot execute putitem !? owner {} , item {} , didn't deleted items when closed?", dwOwnerID, dwItemID);
			}
		}
	}


	//offlineshop_offers
	{
		//offer_id, owner_id, offerer_id, item_id, price_yang, price_cheque, is_notified, is_accept
		//offlineshop-updated 03/08/19
		const char szQuery[] = "SELECT `offer_id`, `owner_id`, `offerer_id`, `item_id`, `price_yang`, "/*`price_cheque`, */"`is_notified`, `is_accept`, `buyer_name` FROM `player`.`offlineshop_offers`;";
		std::unique_ptr<SQLMsg> pMsg(DIRECT_QUERY(szQuery));

		if (pMsg->uiSQLErrno != 0)
		{
			LOG_ERROR("CANNOT LOAD offlineshop_offers TABLE , errorcode {} ", pMsg->uiSQLErrno);
			return false;
		}

		if (pMsg->Get())
		{
			while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
			{
				int col = 0;

				uint32_t dwOfferID=0;
				uint8_t byNotified=0, byAccept=0;
				offlineshop::COfferCache::TOfferCacheInfo offer;

				str_to_number(dwOfferID,			row[col++]);
				str_to_number(offer.dwOwnerID,		row[col++]);
				str_to_number(offer.dwOffererID,	row[col++]);
				str_to_number(offer.dwItemID,		row[col++]);
				str_to_number(offer.price.illYang,	row[col++]);

				str_to_number(byNotified,			row[col++]);
				str_to_number(byAccept,				row[col++]);

				//offlineshop-updated 03/08/19
				strncpy(offer.szBuyerName, row[col++], sizeof(offer.szBuyerName));

				offer.bNoticed	= byNotified ==1;
				offer.bAccepted	= byAccept == 1;


				m_offlineshopOfferCache.Puts(dwOfferID, offer);
			}
		}
	}



	//offlineshop_auctions

	{
		//`owner_id`, `duration`, `name`, `init_yang`, `vnum`, `count` socket0	socket1	socket2		attr0 attrval0		attr1 attrval1
		//attr2 attrval2	attr3 attrval3		attr4 attrval4		attr5 attrval5		attr6 attrval6

		std::string query = CreateOfflineshopSelectAuctionsQuery(); 
		std::unique_ptr<SQLMsg> pMsg(DIRECT_QUERY(query.c_str()));

		if (pMsg->uiSQLErrno != 0)
		{
			LOG_ERROR("CANNOT LOAD offlineshop_auctions TABLE , errorcode {} ", pMsg->uiSQLErrno);
			return false;
		}

		if (pMsg->Get())
		{
			while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
			{
				int col = 0;

				offlineshop::TAuctionInfo info;

				str_to_number(info.dwOwnerID ,			row[col++]);
				str_to_number(info.dwDuration,			row[col++]);
				strncpy(info.szOwnerName,				row[col++],	sizeof(info.szOwnerName));
				str_to_number(info.init_price.illYang,	row[col++]);


				str_to_number(info.item.dwVnum,		row[col++]);
				str_to_number(info.item.dwCount,	row[col++]);

				for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
					str_to_number(info.item.alSockets[i], row[col++]);

				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
				{
					str_to_number(info.item.aAttr[i].bType,		row[col++]);
					str_to_number(info.item.aAttr[i].sValue,	row[col++]);
				}

#ifdef __ENABLE_CHANGELOOK_SYSTEM__
				str_to_number(info.item.dwTransmutation , row[col++]);
#endif
#ifdef ATTR_LOCK
				str_to_number(info.item.iLockedAttr, row[col++]);
#endif
				//patch 08-03-2020
				uint8_t bexp = 0;
				str_to_number(bexp, row[col++]);
				info.item.expiration = offlineshop::ExpirationType(bexp);

				m_offlineshopAuctionCache.PutsAuction(info);
			}
		}
	}



	//offlineshop_auction_offers
	{
		//owner_id			buyer_id			buyer_name			yang
		char szQuery[] = "SELECT `owner_id`, `buyer_id`, `buyer_name`, `yang` FROM `player`.`offlineshop_auction_offers`;";
		std::unique_ptr<SQLMsg> pMsg(DIRECT_QUERY(szQuery));


		if (pMsg->uiSQLErrno != 0)
		{
			LOG_ERROR("CANNOT LOAD offlineshop_auctions TABLE , errorcode {} ", pMsg->uiSQLErrno);
			return false;
		}

		if (pMsg->Get())
		{
			while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
			{
				int col = 0;


				offlineshop::TAuctionOfferInfo offer;

				str_to_number(offer.dwOwnerID,		row[col++]);
				str_to_number(offer.dwBuyerID,		row[col++]);
				strncpy(offer.szBuyerName,			row[col++], sizeof(offer.szBuyerName));
				str_to_number(offer.price.illYang,	row[col++]);


				m_offlineshopAuctionCache.AddOffer(offer, false);
			}
		}
	}


	return true;
}


void CClientManager::SendOfflineshopTable(CPeer* peer)
{
	peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, 
		sizeof(TPacketDGNewOfflineShop) + sizeof(offlineshop::TSubPacketDGLoadTables) +
		(sizeof(offlineshop::TShopInfo)+ sizeof(uint32_t))	* m_offlineshopShopCache.GetCount() + sizeof(offlineshop::TItemInfo) * m_offlineshopShopCache.GetItemCount()  +
		sizeof(offlineshop::TOfferInfo)	* m_offlineshopOfferCache.GetCount() +
		sizeof(offlineshop::TAuctionInfo)*m_offlineshopAuctionCache.GetCount() +
		sizeof(offlineshop::TAuctionOfferInfo)*m_offlineshopAuctionCache.GetOffersCount()
	);
	

	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_LOAD_TABLES;
	
	offlineshop::TSubPacketDGLoadTables subPack;
	subPack.dwShopCount		= m_offlineshopShopCache.GetCount();
	subPack.dwOfferCount	= m_offlineshopOfferCache.GetCount();

	subPack.dwAuctionCount		= m_offlineshopAuctionCache.GetCount();
	subPack.dwAuctionOfferCount	= m_offlineshopAuctionCache.GetOffersCount();

	peer->Encode(&pack, sizeof(pack));
	peer->Encode(&subPack, sizeof(subPack));

	OFFSHOP_DEBUG("shop count %u ", subPack.dwShopCount);

	m_offlineshopShopCache.EncodeCache(peer);
	m_offlineshopOfferCache.EncodeCache(peer);
	m_offlineshopAuctionCache.EncodeCache(peer);
}




void CClientManager::RecvOfflineShopPacket(CPeer* peer,const char* data)
{
	TPacketGDNewOfflineShop* pack;
	data = Decode(pack, data);

	bool bRet=false;

	switch (pack->bSubHeader)
	{
	case offlineshop::SUBHEADER_GD_BUY_ITEM:			bRet=RecvOfflineShopBuyItemPacket(data);			break;
	case offlineshop::SUBHEADER_GD_BUY_LOCK_ITEM:		bRet=RecvOfflineShopLockBuyItem(peer,data);			break;
	case offlineshop::SUBHEADER_GD_EDIT_ITEM:			bRet=RecvOfflineShopEditItemPacket(data);			break;
	case offlineshop::SUBHEADER_GD_REMOVE_ITEM:			bRet=RecvOfflineShopRemoveItemPacket(data);			break;
	case offlineshop::SUBHEADER_GD_ADD_ITEM:			bRet=RecvOfflineShopAddItemPacket(data);			break;

	case offlineshop::SUBHEADER_GD_SHOP_FORCE_CLOSE:	bRet=RecvOfflineShopForceClose(data);				break;
	case offlineshop::SUBHEADER_GD_SHOP_CREATE_NEW:		bRet=RecvOfflineShopCreateNew(data);				break;
	case offlineshop::SUBHEADER_GD_SHOP_CHANGE_NAME:	bRet=RecvOfflineShopChangeName(data);				break;

	case offlineshop::SUBHEADER_GD_OFFER_CREATE:		bRet=RecvOfflineShopOfferCreate(data);				break;
	case offlineshop::SUBHEADER_GD_OFFER_NOTIFIED:		bRet=RecvOfflineShopOfferNotified(data);			break;
	case offlineshop::SUBHEADER_GD_OFFER_ACCEPT:		bRet=RecvOfflineShopOfferAccepted(data);			break;
	case offlineshop::SUBHEADER_GD_OFFER_CANCEL:		bRet=RecvOfflineshopOfferCancel(data);				break;

	case offlineshop::SUBHEADER_GD_SAFEBOX_GET_ITEM:	bRet=RecvOfflineShopSafeboxGetItem(data);			break;
	case offlineshop::SUBHEADER_GD_SAFEBOX_GET_VALUTES:	bRet=RecvOfflineShopSafeboxGetValutes(data);		break;
	case offlineshop::SUBHEADER_GD_SAFEBOX_ADD_ITEM:	bRet=RecvOfflineShopSafeboxAddItem(data);			break;

	//AUCTION
	case offlineshop::SUBHEADER_GD_AUCTION_CREATE:		bRet=RecvOfflineShopAuctionCreate(data);			break;
	case offlineshop::SUBHEADER_GD_AUCTION_ADD_OFFER:	bRet=RecvOfflineShopAuctionAddOffer(data);			break;
	case offlineshop::SUBHEADER_GD_AUCTION_CLOSE:	bRet=RecvOfflineShopAuctionClose(data);			break;
	default:
		LOG_ERROR("UNKNOW NEW OFFLINESHOP SUBHEADER GD {}", pack->bSubHeader);
		break;
	}


	if(!bRet)
		LOG_ERROR("maybe some error during recv offline shop subheader {} ", pack->bSubHeader);
}


bool CClientManager::RecvOfflineShopLockBuyItem(CPeer* peer, const char* data)
{
	offlineshop::TSubPacketGDLockBuyItem* subpack;
	data = Decode(subpack, data);

	uint32_t dwGuest = subpack->dwGuestID;
	uint32_t dwOwner = subpack->dwOwnerID;
	uint32_t dwItem  = subpack->dwItemID;

	OFFSHOP_DEBUG("lock request : guest %u , shop %u , item %u ",dwGuest, dwOwner, dwItem);

	if(m_offlineshopShopCache.LockSellItem(dwOwner, dwItem, subpack->TotalPriceSeen))
		SendOfflineShopBuyLockedItemPacket(peer, dwOwner, dwGuest, dwItem);

	else
		LOG_ERROR("cannot find buy target item {} (owner {} , buyer {}) ", dwItem, dwOwner, dwGuest);

	return true;
}


bool CClientManager::RecvOfflineShopCannotBuyLockItem(const char* data) //topatch
{
	offlineshop::TSubPacketGDCannotBuyLockItem* subpack;
	data = Decode(subpack, data);

	uint32_t dwOwner = subpack->dwOwnerID;
	uint32_t dwItem = subpack->dwItemID;

	OFFSHOP_DEBUG("lock unrequired : shop %u , item %u ", dwOwner, dwItem);

	if (!m_offlineshopShopCache.UnlockSellItem(dwOwner, dwItem)) {
		LOG_ERROR("cannot find unlock requested item {} (owner {} ) ", dwItem, dwOwner);
		return false;
	}return true;
}


bool CClientManager::RecvOfflineShopBuyItemPacket(const char* data)
{
	offlineshop::TSubPacketGDBuyItem* subpack;
	data = Decode(subpack, data);


	uint32_t dwGuest = subpack->dwGuestID;
	uint32_t dwOwner = subpack->dwOwnerID;
	uint32_t dwItem  = subpack->dwItemID;

	OFFSHOP_DEBUG("guest %u, owner %u, item %u ",dwGuest, dwOwner, dwItem);

	offlineshop::CShopCache::TShopCacheInfo* pCache =nullptr;
	if(!m_offlineshopShopCache.Get(dwOwner, &pCache))
		return false;

	const auto it = pCache->itemsmap.find(dwItem);
	if(pCache->itemsmap.end() == it)
		return false;

	const offlineshop::TPriceInfo& price = it->second.price;
	offlineshop::TValutesInfo valute;
	valute.illYang = price.illYang;
#ifdef __ENABLE_CHEQUE_SYSTEM__
	valute.iCheque = price.iCheque;
#endif

	m_offlineshopSafeboxCache.AddValutes(dwOwner, valute);
	SendOfflineshopSafeboxAddValutes(dwOwner, valute);

	OFFSHOP_DEBUG("added to safebox valute guest %u, owner %u, item %u ",dwGuest, dwOwner, dwItem);

	//check about offers
	std::vector<uint32_t> offerIDs;
	m_offlineshopOfferCache.GetOffersByItemID(subpack->dwItemID, offerIDs);

	if (!offerIDs.empty())
	{
		offlineshop::TValutesInfo valute;

		for (uint32_t i = 0; i < offerIDs.size(); i++)
		{
			offlineshop::COfferCache::TOfferCacheInfo* pOffer = nullptr;
			if (m_offlineshopOfferCache.Get(offerIDs[i], &pOffer) && !pOffer->bAccepted)
			{
#ifdef __ENABLE_CHEQUE_SYSTEM__
				valute.illYang = pOffer->price.illYang%YANG_PER_CHEQUE;
				valute.iCheque = pOffer->price.illYang/YANG_PER_CHEQUE;
#else
				valute.illYang = pOffer->price.illYang;
#endif
				m_offlineshopSafeboxCache.AddValutes(pOffer->dwOffererID, valute);
				SendOfflineshopSafeboxAddValutes(pOffer->dwOffererID, valute);


				SendOfflineShopOfferCancel(pOffer->dwOwnerID, offerIDs[i], true);//offlineshop-updated 05/08/19
				m_offlineshopOfferCache.CancelOffer(offerIDs[i], pOffer->dwOwnerID);
			}
		}
	}


	//offlineshop-updated 04/08/19
	SendOfflineShopBuyItemPacket(dwOwner, dwGuest, dwItem);
	if(!m_offlineshopShopCache.SellItem(dwOwner, dwItem))
		LOG_ERROR("some problem with sell : {} {} ", dwOwner, dwItem);

	return true;
}




bool CClientManager::RecvOfflineShopEditItemPacket(const char* data)
{
	offlineshop::TSubPacketGDEditItem* subpack;
	data = Decode(subpack, data);

	OFFSHOP_DEBUG("%u shop , %u item",subpack->dwOwnerID , subpack->dwItemID);


	if(m_offlineshopShopCache.EditItem(subpack->dwOwnerID, subpack->dwItemID, subpack->priceInfo))
		SendOfflineShopEditItemPacket(subpack->dwOwnerID, subpack->dwItemID, subpack->priceInfo);
	else
		LOG_ERROR("cannot edit item {} , shop {} ", subpack->dwItemID, subpack->dwOwnerID);


	return true;
}




bool CClientManager::RecvOfflineShopRemoveItemPacket(const char* data) //patchme
{
	offlineshop::TSubPacketGDRemoveItem* subpack;
	data = Decode(subpack, data);
	
	offlineshop::CShopCache::TShopCacheInfo* pShop = nullptr;
	if (!m_offlineshopShopCache.Get(subpack->dwOwnerID, &pShop))
	{
		OFFSHOP_DEBUG("cannot found shop owner %u into cache",subpack->dwOwnerID);
		return false;
	}

	const auto it = pShop->itemsmap.find(subpack->dwItemID);
	if (it == pShop->itemsmap.end())
	{
		OFFSHOP_DEBUG("cannot found item id shop owner %u into cache (item id %u) ",subpack->dwOwnerID, subpack->dwItemID);
		return false;
	}

	std::vector<uint32_t> offerIDs;
	m_offlineshopOfferCache.GetOffersByItemID(subpack->dwItemID, offerIDs);

	if (!offerIDs.empty())
	{
		offlineshop::TValutesInfo valute;

		for (uint32_t i = 0; i < offerIDs.size(); i++)
		{
			offlineshop::COfferCache::TOfferCacheInfo* pOffer = nullptr;
			if (m_offlineshopOfferCache.Get(offerIDs[i], &pOffer) && !pOffer->bAccepted)
			{
#ifdef __ENABLE_CHEQUE_SYSTEM__
				valute.illYang = pOffer->price.illYang % YANG_PER_CHEQUE;
				valute.iCheque = pOffer->price.illYang / YANG_PER_CHEQUE;
#else
				valute.illYang = pOffer->price.illYang;
#endif

				m_offlineshopSafeboxCache.AddValutes(pOffer->dwOffererID, valute);
				SendOfflineshopSafeboxAddValutes(pOffer->dwOffererID, valute);


				SendOfflineShopOfferCancel(pOffer->dwOwnerID, offerIDs[i], true);//offlineshop-updated 05/08/19
				m_offlineshopOfferCache.CancelOffer(offerIDs[i], pOffer->dwOwnerID);
			}
		}
	}

	//topatch
	auto copy = it->second.item;
	if (m_offlineshopShopCache.RemoveItem(subpack->dwOwnerID, subpack->dwItemID)) {
		SendOfflineShopRemoveItemPacket(subpack->dwOwnerID, subpack->dwItemID);
		m_offlineshopSafeboxCache.AddItem(subpack->dwOwnerID, copy);
	}

	else
		LOG_ERROR("cannot remove item {} shop {} item ?!", subpack->dwOwnerID, subpack->dwItemID);

	return true;
}




bool CClientManager::RecvOfflineShopAddItemPacket(const char* data)
{
	offlineshop::TSubPacketGDAddItem* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopCache::TShopCacheItemInfo item;
	offlineshop::ZeroObject(item);
	offlineshop::CopyObject(item.item, subpack->itemInfo.item);
	offlineshop::CopyObject(item.price, subpack->itemInfo.price);


	m_offlineshopShopCache.AddItem(subpack->dwOwnerID , item);
	return true;
}





bool CClientManager::RecvOfflineShopForceClose(const char* data)
{
	offlineshop::TSubPacketGDShopForceClose* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopCache::TShopCacheInfo* pInfo=nullptr;
	if(!m_offlineshopShopCache.Get(subpack->dwOwnerID, &pInfo))
		return false;

	//store item in safebox
	offlineshop::CShopCache::ITEM_ITER it = pInfo->itemsmap.begin();
	for(; it!=pInfo->itemsmap.end(); ++it)
		m_offlineshopSafeboxCache.AddItem(subpack->dwOwnerID, it->second.item);

	offlineshop::COfferCache::OFFERIDVEC* pVec;
	if (m_offlineshopOfferCache.GetOffersByShopOwner(subpack->dwOwnerID, &pVec))
	{
		for (uint32_t i = 0; i < pVec->size(); i++)
		{
			uint32_t dwOfferID = pVec->at(i);
			
			offlineshop::COfferCache::TOfferCacheInfo* pOffer=nullptr;
			if (m_offlineshopOfferCache.Get(dwOfferID, &pOffer) && !pOffer->bAccepted)
			{
				offlineshop::TValutesInfo valute;
#ifdef __ENABLE_CHEQUE_SYSTEM__
				valute.illYang = pOffer->price.illYang % YANG_PER_CHEQUE;
				valute.iCheque = pOffer->price.illYang / YANG_PER_CHEQUE;
#else
				valute.illYang = pOffer->price.illYang;
#endif

				m_offlineshopSafeboxCache.AddValutes(pOffer->dwOffererID, valute);
				SendOfflineshopSafeboxAddValutes(pOffer->dwOffererID , valute);
			}
		}
	}

	m_offlineshopOfferCache.RemoveOffersByShopOwner(subpack->dwOwnerID);
	m_offlineshopShopCache.CloseShop(subpack->dwOwnerID);

	SendOfflineShopForceClose(subpack->dwOwnerID);
	return true;
}




bool CClientManager::RecvOfflineShopCreateNew(const char* data)
{
	offlineshop::TSubPacketGDShopCreateNew* subpack;
	data = Decode(subpack, data);

	std::vector<offlineshop::CShopCache::TShopCacheItemInfo> vec;
	offlineshop::CShopCache::TShopCacheItemInfo itemInfo;
	offlineshop::ZeroObject(itemInfo);

	vec.reserve(subpack->shop.dwCount);

	for (uint32_t i = 0; i < subpack->shop.dwCount; i++)
	{
		offlineshop::TItemInfo* pItemInfo;
		data = Decode(pItemInfo, data);

		offlineshop::CopyObject(itemInfo.item , pItemInfo->item);
		offlineshop::CopyObject(itemInfo.price, pItemInfo->price);

		vec.push_back(itemInfo);
	}

	OFFSHOP_DEBUG(" id %u , duration %u , name %s , count %u ",subpack->shop.dwOwnerID , subpack->shop.dwDuration , subpack->shop.szName, vec.size());
	m_offlineshopShopCache.CreateShop(subpack->shop.dwOwnerID , subpack->shop.dwDuration , subpack->shop.szName, vec
#ifdef KASMIR_PAKET_SYSTEM
	, subpack->shop.dwKasmirNpc
#endif
	);
	
	return true;
}




bool CClientManager::RecvOfflineShopChangeName(const char* data)
{
	offlineshop::TSubPacketGDShopChangeName* subpack;
	data = Decode(subpack, data);

	if(m_offlineshopShopCache.ChangeShopName(subpack->dwOwnerID, subpack->szName))
		SendOfflineShopChangeName(subpack->dwOwnerID, subpack->szName);
	return true;
}







bool CClientManager::RecvOfflineShopOfferCreate(const char* data)
{
	offlineshop::TSubPacketGDOfferCreate* subpack;
	data = Decode(subpack, data);

	offlineshop::COfferCache::TOfferCacheInfo info;
	info.bAccepted	= false;
	info.bNoticed	= false;
	info.dwItemID	= subpack->dwItemID;
	info.dwOffererID= subpack->offer.dwOffererID;
	info.dwOwnerID	= subpack->offer.dwOwnerID;
	offlineshop::CopyObject(info.price, subpack->offer.price);

	//offlineshop-updated 03/08/19
	strncpy(info.szBuyerName, subpack->offer.szBuyerName, sizeof(info.szBuyerName));

#ifdef ENABLE_NEW_OFFLINESHOP_LOGS
	OfflineshopLog(subpack->dwOwnerID, subpack->dwItemID, "created a new offer from %u  (name %s) for the item. %lld yang "
#ifdef __ENABLE_CHEQUE_SYSTEM__
		" %d iCheque "
#endif
		, subpack->offer.dwOffererID, subpack->offer.szBuyerName, subpack->offer.price.illYang

#ifdef __ENABLE_CHEQUE_SYSTEM__
		, subpack->offer.price.iCheque
#endif

		
		);
#endif

	if(m_offlineshopOfferCache.AddOffer(info))
		return true;

	return false;
}




bool CClientManager::RecvOfflineShopOfferNotified(const char* data)
{
	offlineshop::TSubPacketGDOfferNotified* subpack;
	data = Decode(subpack, data);

	if(m_offlineshopOfferCache.NoticedOffer(subpack->dwOfferID))
		SendOfflineShopOfferNotified(subpack->dwOwnerID, subpack->dwOfferID);
	return true;
}




bool CClientManager::RecvOfflineShopOfferAccepted(const char* data)
{
	offlineshop::TSubPacketGDOfferAccept* subpack;
	data = Decode(subpack, data);

	offlineshop::COfferCache::TOfferCacheInfo* pOffer=nullptr;
	m_offlineshopOfferCache.Get(subpack->dwOfferID,&pOffer);

	if(pOffer->bAccepted)
		return true;

	if(!pOffer)
		return true;

	//check about offers
	std::vector<uint32_t> offerIDs;
	m_offlineshopOfferCache.GetOffersByItemID(pOffer->dwItemID, offerIDs);

	if (!offerIDs.empty())
	{
		offlineshop::TValutesInfo valute;

		for (uint32_t i = 0; i < offerIDs.size(); i++)
		{
			if(offerIDs[i] == subpack->dwOfferID)
				continue;

			offlineshop::COfferCache::TOfferCacheInfo* pOfferToRemove = nullptr;
			if (m_offlineshopOfferCache.Get(offerIDs[i], &pOfferToRemove) && !pOfferToRemove->bAccepted)
			{
#ifdef __ENABLE_CHEQUE_SYSTEM__
				valute.iCheque = pOfferToRemove->price.illYang/YANG_PER_CHEQUE;
				valute.illYang = pOfferToRemove->price.illYang%YANG_PER_CHEQUE;
#else
				valute.illYang = pOfferToRemove->price.illYang;
#endif

				m_offlineshopSafeboxCache.AddValutes(pOfferToRemove->dwOffererID, valute);
				SendOfflineshopSafeboxAddValutes(pOfferToRemove->dwOffererID, valute);


				SendOfflineShopOfferCancel(pOfferToRemove->dwOwnerID, offerIDs[i], true);//offlineshop-updated 05/08/19
				m_offlineshopOfferCache.CancelOffer(offerIDs[i], pOfferToRemove->dwOwnerID);
			}
		}
	}



	if(m_offlineshopOfferCache.AcceptOffer(subpack->dwOfferID))
		SendOfflineShopOfferAccepted(subpack->dwOwnerID, subpack->dwOfferID);

	





	offlineshop::CShopCache::TShopCacheInfo* pShop=nullptr;


	if (m_offlineshopShopCache.Get(pOffer->dwOwnerID, &pShop))
	{
		if (const auto it = pShop->itemsmap.find(pOffer->dwItemID); it != pShop->itemsmap.end())
		{
			m_offlineshopShopCache.EditItem(pOffer->dwOwnerID, pOffer->dwItemID, pOffer->price);
			SendOfflineShopEditItemPacket(pOffer->dwOwnerID, pOffer->dwItemID, pOffer->price);

			it->second.bLock = true;
			m_offlineshopSafeboxCache.AddItem(pOffer->dwOffererID, it->second.item);
		}

		else
			LOG_ERROR("cannot find item in shop {} item {} ", pOffer->dwOwnerID, pOffer->dwItemID);
	}

	else
		LOG_ERROR("cannot find shop {} , offer {} ", pOffer->dwOwnerID, subpack->dwOfferID);

	//offlineshop-updated 03/08/19
	offlineshop::TValutesInfo valute;
#ifdef __ENABLE_CHEQUE_SYSTEM__
	//converting as possible yang to cheque
	valute.iCheque = pOffer->price.illYang/YANG_PER_CHEQUE;
	valute.illYang = pOffer->price.illYang%YANG_PER_CHEQUE;
#else
	valute.illYang = pOffer->price.illYang;
#endif


	m_offlineshopSafeboxCache.AddValutes(pOffer->dwOwnerID, valute);
	SendOfflineshopSafeboxAddValutes(pOffer->dwOwnerID, valute);


	if(!m_offlineshopShopCache.SellItem(pOffer->dwOwnerID,pOffer->dwItemID))
		LOG_ERROR("cannot sell item {}, shop {} ", pOffer->dwItemID, pOffer->dwOwnerID);

	return true;
}







bool CClientManager::RecvOfflineshopOfferCancel(const char* data)
{
	offlineshop::TSubPacketGDOfferCancel* subpack;
	data = Decode(subpack, data);

	OFFSHOP_DEBUG("%u offer, %u owner ",subpack->dwOfferID, subpack->dwOwnerID);

	offlineshop::COfferCache::TOfferCacheInfo* pOffer=nullptr;
	if(!m_offlineshopOfferCache.Get(subpack->dwOfferID, &pOffer))
		return false;

	uint32_t dwBuyer = pOffer->dwOffererID;
	offlineshop::TValutesInfo valute;
#ifdef __ENABLE_CHEQUE_SYSTEM__
	valute.illYang = pOffer->price.illYang % YANG_PER_CHEQUE;
	valute.iCheque = pOffer->price.illYang / YANG_PER_CHEQUE;
#else
	valute.illYang = pOffer->price.illYang;
#endif


	m_offlineshopSafeboxCache.AddValutes(dwBuyer, valute);
	SendOfflineshopSafeboxAddValutes(dwBuyer, valute);


	//offlineshop-updated 05/08/19
	if(m_offlineshopOfferCache.CancelOffer(subpack->dwOfferID, subpack->dwOwnerID))
		SendOfflineShopOfferCancel(subpack->dwOwnerID, subpack->dwOfferID, false);
	
	else
		LOG_ERROR("some problems with cache cancel offer");

	return true;
}




bool CClientManager::RecvOfflineShopSafeboxGetItem(const char* data)
{
	offlineshop::TSubPacketGDSafeboxGetItem* subpack;
	data = Decode(subpack,data);

	if(m_offlineshopSafeboxCache.RemoveItem(subpack->dwOwnerID , subpack->dwItemID))
		return true;
	return false;
}


bool CClientManager::RecvOfflineShopSafeboxGetValutes(const char* data)
{
	offlineshop::TSubPacketGDSafeboxGetValutes* subpack;
	data = Decode(subpack,data);

	if(m_offlineshopSafeboxCache.RemoveValutes(subpack->dwOwnerID , subpack->valute))
		return true;
	return false;
}



bool CClientManager::RecvOfflineShopSafeboxAddItem(const char* data) {
	offlineshop::TSubPacketGDSafeboxAddItem* subpack;
	data = Decode(subpack,data);

	if(!m_offlineshopSafeboxCache.AddItem(subpack->dwOwnerID , subpack->item))
		return false;
	return true;
}



//AUCTION
bool CClientManager::RecvOfflineShopAuctionCreate(const char* data)
{
	offlineshop::TSubPacketGDAuctionCreate* subpack;
	data = Decode(subpack,data);

	OFFSHOP_DEBUG("auction %u , %s , %u ",subpack->auction.dwOwnerID , subpack->auction.szOwnerName, subpack->auction.dwDuration);

	if(m_offlineshopAuctionCache.AddAuction(subpack->auction))
		SendOfflineshopAuctionCreate(subpack->auction);


	else
		LOG_ERROR("cannot create auction: {} , {} , {} ", subpack->auction.dwOwnerID, subpack->auction.dwDuration, subpack->auction.szOwnerName);

	return true;
}



bool CClientManager::RecvOfflineShopAuctionAddOffer(const char* data)
{
	offlineshop::TSubPacketGDAuctionAddOffer* subpack;
	data = Decode(subpack,data);

	offlineshop::TAuctionOfferInfo* pOffer=nullptr;
	if (m_offlineshopAuctionCache.GetBestBuyer(subpack->offer.dwOwnerID, &pOffer) != 0 && pOffer)
	{
		offlineshop::TValutesInfo valute;
#ifdef __ENABLE_CHEQUE_SYSTEM__
		valute.illYang = pOffer->price.illYang % YANG_PER_CHEQUE;
		valute.iCheque = pOffer->price.illYang / YANG_PER_CHEQUE;
#else
		valute.illYang = pOffer->price.illYang;
#endif


		m_offlineshopSafeboxCache.AddValutes(pOffer->dwBuyerID, valute);
		SendOfflineshopSafeboxAddValutes(pOffer->dwBuyerID, valute);
	}


	if(m_offlineshopAuctionCache.AddOffer(subpack->offer))
		SendOfflineshopAuctionAddOffer(subpack->offer);

	return true;
}

bool CClientManager::RecvOfflineShopAuctionClose(const char* data)
{
	offlineshop::TSubPacketGDAuctionClose* subpack;
	data = Decode(subpack, data);
	
	
	offlineshop::TAuctionInfo* pAuction=nullptr;
	if(!m_offlineshopAuctionCache.Get(subpack->dwOwnerID, &pAuction))
		return true;
	
	offlineshop::TAuctionOfferInfo* pOffer = nullptr;
	uint32_t dwNewOwner = m_offlineshopAuctionCache.GetBestBuyer(subpack->dwOwnerID, &pOffer);
	if ((pOffer) && (dwNewOwner != 0)) {
		offlineshop::TValutesInfo valute;
#ifdef __ENABLE_CHEQUE_SYSTEM__
		valute.illYang = pOffer->price.illYang % YANG_PER_CHEQUE;
		valute.iCheque = pOffer->price.illYang / YANG_PER_CHEQUE;
#else
		valute.illYang = pOffer->price.illYang;
#endif
		OFFSHOP_DEBUG("found valute best offer %lld", pOffer->price.illYang);

		m_offlineshopSafeboxCache.AddValutes(dwNewOwner, valute);
		SendOfflineshopSafeboxAddValutes(dwNewOwner, valute);
	}
	
	m_offlineshopSafeboxCache.AddItem(subpack->dwOwnerID , pAuction->item);
	m_offlineshopAuctionCache.ExpiredAuction(subpack->dwOwnerID);
	SendOfflineshopAuctionExpired(subpack->dwOwnerID);
	return true;
}

bool CClientManager::SendOfflineShopBuyLockedItemPacket(CPeer* peer, uint32_t dwOwner, uint32_t dwGuest, uint32_t dwItem)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_LOCKED_BUY_ITEM;

	offlineshop::TSubPacketDGLockedBuyItem subpack;
	subpack.dwOwnerID	= dwOwner;
	subpack.dwBuyerID	= dwGuest;
	subpack.dwItemID	= dwItem;

	peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack)+sizeof(subpack));
	peer->Encode(&pack, sizeof(pack));
	peer->Encode(&subpack, sizeof(subpack));

	OFFSHOP_DEBUG("sent locked item packet %u ",dwItem);
	return true;
}


bool CClientManager::SendOfflineShopBuyItemPacket(uint32_t dwOwner, uint32_t dwGuest, uint32_t dwItem)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_BUY_ITEM;

	offlineshop::TSubPacketDGBuyItem subpack;
	subpack.dwOwnerID	= dwOwner;
	subpack.dwBuyerID	= dwGuest;
	subpack.dwItemID	= dwItem;

	
	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}

	OFFSHOP_DEBUG("owner %u , guest %u , item %u ", dwOwner, dwGuest, dwItem);
	return true;
}




bool CClientManager::SendOfflineShopEditItemPacket(uint32_t dwOwner, uint32_t dwItem, const offlineshop::TPriceInfo& price)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_EDIT_ITEM;

	offlineshop::TSubPacketDGEditItem subpack;
	subpack.dwOwnerID	= dwOwner;
	subpack.dwItemID	= dwItem;
	offlineshop::CopyObject(subpack.price, price);

	OFFSHOP_DEBUG("owner %u , item %u ",dwOwner, dwItem);

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}

	return true;
}




bool CClientManager::SendOfflineShopRemoveItemPacket(uint32_t dwOwner, uint32_t dwItem)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_REMOVE_ITEM;

	offlineshop::TSubPacketDGRemoveItem subpack;
	subpack.dwOwnerID	= dwOwner;
	subpack.dwItemID	= dwItem;
	
	


	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}

	return true;
}




bool CClientManager::SendOfflineShopAddItemPacket(uint32_t dwOwner, uint32_t dwItemID, const offlineshop::TItemInfo& rInfo)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_ADD_ITEM;

	offlineshop::TSubPacketDGAddItem subpack;
	subpack.dwOwnerID	= dwOwner;
	subpack.dwItemID	= dwItemID;
	offlineshop::CopyObject(subpack.item, rInfo);




	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}

	return true;
}





bool CClientManager::SendOfflineShopForceClose(uint32_t dwOwnerID)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_SHOP_FORCE_CLOSE;

	offlineshop::TSubPacketDGShopForceClose subpack;
	subpack.dwOwnerID	= dwOwnerID;




	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}

	return true;
}




bool CClientManager::SendOfflineShopCreateNew(const offlineshop::TShopInfo& shop, const std::vector<offlineshop::TItemInfo>& vec)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_SHOP_CREATE_NEW;

	offlineshop::TSubPacketDGShopCreateNew subpack;
	offlineshop::CopyObject(subpack.shop, shop);
	
	
	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack) + (sizeof(offlineshop::TItemInfo) * vec.size()));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));

		if(!vec.empty())
			peer->Encode(&vec[0], sizeof(offlineshop::TItemInfo)*vec.size());
	}

	OFFSHOP_DEBUG("Sent offlineshop create packet (forward) for shop %s , shop id %u ", shop.szName, shop.dwOwnerID);
#ifdef ENABLE_OFFLINESHOP_DEBUG
	for(uint32_t i=0; i < vec.size(); i++)
		OFFSHOP_DEBUG("item %u , vnum %u , count %u ", vec[i].dwItemID , vec[i].item.dwVnum , vec[i].item.dwCount);
#endif
	return true;
}




bool CClientManager::SendOfflineShopChangeName(uint32_t dwOwnerID, const char* szName)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_SHOP_CHANGE_NAME;

	offlineshop::TSubPacketDGShopChangeName subpack;
	subpack.dwOwnerID	= dwOwnerID;
	strncpy(subpack.szName, szName, sizeof(subpack.szName));


	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack) );
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}

	return true;
}



bool CClientManager::SendOfflineshopShopExpired(uint32_t dwOwnerID)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_SHOP_EXPIRED;

	offlineshop::TSubPacketDGShopExpired subpack;
	subpack.dwOwnerID	= dwOwnerID;

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack) );
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}

	return true;
}




bool CClientManager::SendOfflineShopOfferCreate(const offlineshop::TOfferInfo& offer)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_OFFER_CREATE;

	offlineshop::TSubPacketDGOfferCreate subpack;
	subpack.dwItemID	= offer.dwItemID;
	subpack.dwOwnerID	= offer.dwOwnerID;
	offlineshop::CopyObject(subpack.offer, offer);


	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}

	return true;
}




bool CClientManager::SendOfflineShopOfferNotified(uint32_t dwOwnerID, uint32_t dwOfferID)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_OFFER_NOTIFIED;

	offlineshop::TSubPacketDGOfferNotified subpack;
	subpack.dwOfferID = dwOfferID;
	subpack.dwOwnerID = dwOwnerID;


	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}

	return true;
}




bool CClientManager::SendOfflineShopOfferAccepted(uint32_t dwOwnerID, uint32_t dwOfferID)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_OFFER_ACCEPT;

	offlineshop::TSubPacketDGOfferAccept subpack;
	subpack.dwOfferID = dwOfferID;
	subpack.dwOwnerID = dwOwnerID;


	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}

	return true;
}


//offlineshop-updated 05/08/19
void CClientManager::SendOfflineShopOfferCancel(uint32_t dwOwnerID, uint32_t dwOfferID, bool isRemovingItem) 
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_OFFER_CANCEL;

	offlineshop::TSubPacketDGOfferCancel subpack;
	subpack.dwOfferID = dwOfferID;
	subpack.dwOwnerID = dwOwnerID;
	subpack.IsRemovingItem = isRemovingItem;
	

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}

	OFFSHOP_DEBUG("%u offer, %u owner",dwOfferID, dwOwnerID);
}





void CClientManager::SendOfflineshopSafeboxAddItem(uint32_t dwOwnerID, uint32_t dwItem, const offlineshop::TItemInfoEx& item)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_SAFEBOX_ADD_ITEM;

	offlineshop::TSubPacketDGSafeboxAddItem subpack;
	subpack.dwOwnerID	= dwOwnerID;
	subpack.dwItemID	= dwItem;

	offlineshop::CopyObject(subpack.item , item);

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}
}



void CClientManager::SendOfflineshopSafeboxAddValutes(uint32_t dwOwnerID, const offlineshop::TValutesInfo& valute)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_SAFEBOX_ADD_VALUTES;

	offlineshop::TSubPacketDGSafeboxAddValutes subpack;
	subpack.dwOwnerID	= dwOwnerID;
	offlineshop::CopyObject(subpack.valute , valute);


	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}
}




void CClientManager::SendOfflineshopSafeboxLoad(CPeer* peer ,uint32_t dwOwnerID, const offlineshop::TValutesInfo& valute, const std::vector<offlineshop::TItemInfoEx>& items , const std::vector<uint32_t>& ids)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_SAFEBOX_LOAD;

	offlineshop::TSubPacketDGSafeboxLoad subpack;
	subpack.dwOwnerID	= dwOwnerID;
	subpack.dwItemCount	= items.size();
	offlineshop::CopyObject(subpack.valute , valute);

	OFFSHOP_DEBUG("player %u , count %u , valutes yang %lld ",dwOwnerID, items.size(), valute.illYang);

	peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack) + (sizeof(offlineshop::TItemInfoEx)*items.size()) + (sizeof(uint32_t)*ids.size()));
	peer->Encode(&pack, sizeof(pack));
	peer->Encode(&subpack, sizeof(subpack));

	for (uint32_t i = 0; i < items.size(); i++)
	{
		peer->EncodeDWORD(ids[i]);
		peer->Encode(&items[i] , sizeof(offlineshop::TItemInfoEx));
	}
}




//patch 08-03-2020
void CClientManager::SendOfflineshopSafeboxExpiredItem(uint32_t dwOwnerID, uint32_t itemID) {
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader = offlineshop::SUBHEADER_DG_SAFEBOX_EXPIRED_ITEM;

	offlineshop::TSubPacketDGSafeboxExpiredItem subpack;
	subpack.dwOwnerID = dwOwnerID;
	subpack.dwItemID = itemID;

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;
		if (peer->GetChannel() == 0)
			continue;
		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}
}



//AUCTION
void CClientManager::SendOfflineshopAuctionCreate(const offlineshop::TAuctionInfo& auction)
{
	OFFSHOP_DEBUG("auction id %u , duration %u ",auction.dwOwnerID, auction.dwDuration);

	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_AUCTION_CREATE;

	offlineshop::TSubPacketDGAuctionCreate subpack;
	CopyObject(subpack.auction, auction);

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer= *it;

		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}
}



void CClientManager::SendOfflineshopAuctionAddOffer(const offlineshop::TAuctionOfferInfo& offer)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_AUCTION_ADD_OFFER;

	offlineshop::TSubPacketDGAuctionAddOffer subpack;
	CopyObject(subpack.offer, offer);

	for (auto it = m_peerList.begin(); it != m_peerList.end(); it++)
	{
		CPeer* peer= *it;

		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}
}





void CClientManager::SendOfflineshopAuctionExpired(uint32_t dwOwnerID)
{
	TPacketDGNewOfflineShop pack;
	pack.bSubHeader	= offlineshop::SUBHEADER_DG_AUCTION_EXPIRED;

	offlineshop::TSubPacketDGAuctionExpired subpack;
	subpack.dwOwnerID = dwOwnerID;

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer= *it;

		peer->EncodeHeader(HEADER_DG_NEW_OFFLINESHOP, 0, sizeof(pack) + sizeof(subpack));
		peer->Encode(&pack, sizeof(pack));
		peer->Encode(&subpack, sizeof(subpack));
	}
}




void CClientManager::OfflineShopResultAddItemQuery(CPeer* peer, SQLMsg* msg, CQueryInfo* pQueryInfo)
{
	offlineshop::SQueryInfoAddItem* qi = (offlineshop::SQueryInfoAddItem*) pQueryInfo->pvData;


	uint32_t dwItemID = msg->Get()->uiInsertID;

	offlineshop::TItemInfo info;
	offlineshop::CopyObject(info.item, qi->item.item);
	offlineshop::CopyObject(info.price,qi->item.price);

	info.dwItemID = dwItemID;
	info.dwOwnerID= qi->dwOwnerID;

	
	m_offlineshopShopCache.PutItem(qi->dwOwnerID, dwItemID, qi->item);
	SendOfflineShopAddItemPacket(qi->dwOwnerID, dwItemID, info);


	delete(qi);
}



void CClientManager::OfflineShopResultCreateShopQuery(CPeer* peer, SQLMsg* msg, CQueryInfo* pQueryInfo)
{
	offlineshop::SQueryInfoCreateShop* qi = (offlineshop::SQueryInfoCreateShop*) pQueryInfo->pvData;
	if(!m_offlineshopShopCache.PutShop(qi->dwOwnerID, qi->dwDuration, qi->szName
#ifdef KASMIR_PAKET_SYSTEM
	, qi->dwKasmirNpc
#endif
	))
		LOG_ERROR("cannot insert new shop , id {} ", qi->dwOwnerID);

	if (!qi->items.empty())
	{
		OFFSHOP_DEBUG("shop %s , Item insert %d", qi->szName, 0);

		qi->dwItemIndex	= 0;
		offlineshop::CShopCache::TShopCacheItemInfo & itemInfo = qi->items[qi->dwItemIndex];
		if(!m_offlineshopShopCache.CreateShopAddItem(qi, itemInfo))
			LOG_ERROR("some problem during shop puts? cannot find the shop in the cache");
	}

	else
	{
		offlineshop::TShopInfo shop;
		shop.dwOwnerID	= qi->dwOwnerID;
		shop.dwCount	= 0;
		strncpy(shop.szName, qi->szName, sizeof(shop.szName));
		
		std::vector<offlineshop::TItemInfo> vec;
		SendOfflineShopCreateNew(shop, vec);
	}
}



void CClientManager::OfflineShopResultCreateShopAddItemQuery(CPeer* peer, SQLMsg* msg, CQueryInfo* pQueryInfo)
{
	offlineshop::SQueryInfoCreateShop* qi = (offlineshop::SQueryInfoCreateShop*) pQueryInfo->pvData;

	
	const offlineshop::CShopCache::TShopCacheItemInfo& item = qi->items[qi->dwItemIndex++];
	m_offlineshopShopCache.PutItem(qi->dwOwnerID  , msg->Get()->uiInsertID,  item);
	qi->ids.push_back(msg->Get()->uiInsertID);

	OFFSHOP_DEBUG("shop %s ", qi->szName);

	if (!qi->items.empty() && qi->dwItemIndex != qi->items.size())
	{
		offlineshop::CShopCache::TShopCacheItemInfo & itemInfo = qi->items[qi->dwItemIndex];
		m_offlineshopShopCache.CreateShopAddItem(qi, itemInfo);

		OFFSHOP_DEBUG("shop %s , Item insert %d", qi->szName, qi->dwItemIndex);
	}


	else
	{
		offlineshop::TShopInfo shopInfo;
		shopInfo.dwOwnerID	= qi->dwOwnerID;
		shopInfo.dwDuration	= qi->dwDuration;
#ifdef KASMIR_PAKET_SYSTEM
		shopInfo.dwKasmirNpc = qi->dwKasmirNpc;
#endif
		shopInfo.dwCount	= qi->items.size();
		strncpy(shopInfo.szName , qi->szName,  sizeof(shopInfo.szName));

		offlineshop::TItemInfo temp;
		std::vector<offlineshop::TItemInfo> vec;
		vec.reserve(qi->items.size());

		OFFSHOP_DEBUG("shop making a new vector TItemInfo , reserved %d ",qi->items.size());

		for (uint32_t i = 0; i < qi->items.size(); i++)
		{
			offlineshop::CShopCache::TShopCacheItemInfo& itemInfo= qi->items[i];

			temp.dwItemID	= qi->ids[i];
			temp.dwOwnerID	= qi->dwOwnerID;
			offlineshop::CopyObject(temp.item, itemInfo.item);
			offlineshop::CopyObject(temp.price, itemInfo.price);

			vec.push_back(temp);
		}

		SendOfflineShopCreateNew(shopInfo, vec);
		OFFSHOP_DEBUG("shop %s sending forward packet (items count %d) ", shopInfo.szName, shopInfo.dwCount);
		delete(qi);
	}
}



void CClientManager::OfflineShopResultOfferAddQuery(CPeer* peer, SQLMsg* msg, CQueryInfo* pQueryInfo)
{
	offlineshop::SQueryInfoOfferAdd* qi = (offlineshop::SQueryInfoOfferAdd*) pQueryInfo->pvData;
	m_offlineshopOfferCache.Puts(msg->Get()->uiInsertID, qi->offer);


	offlineshop::TOfferInfo offerInfo;
	offerInfo.bAccepted		= qi->offer.bAccepted;
	offerInfo.bNoticed		= qi->offer.bNoticed;
	offerInfo.dwItemID		= qi->offer.dwItemID;
	offerInfo.dwOffererID	= qi->offer.dwOffererID;
	offerInfo.dwOfferID		= msg->Get()->uiInsertID;
	offerInfo.dwOwnerID		= qi->offer.dwOwnerID;

	offlineshop::CopyObject(offerInfo.price, qi->offer.price);
	//offlineshop-updated 04/08/19 
	strncpy(offerInfo.szBuyerName, qi->offer.szBuyerName, sizeof(offerInfo.szBuyerName));

	SendOfflineShopOfferCreate(offerInfo);

	delete(qi);
}

void CClientManager::OfflineShopResultSafeboxAddItemQuery(CPeer* peer, SQLMsg* msg, CQueryInfo* pQueryInfo)
{
	offlineshop::SQueryInfoSafeboxAddItem* qi = (offlineshop::SQueryInfoSafeboxAddItem*) pQueryInfo->pvData;
	
	m_offlineshopSafeboxCache.PutItem(qi->dwOwnerID , msg->Get()->uiInsertID , qi->item);
	SendOfflineshopSafeboxAddItem(qi->dwOwnerID , msg->Get()->uiInsertID, qi->item);
	
	delete(qi);
}



void CClientManager::OfflineShopResultQuery(CPeer* peer, SQLMsg* msg, CQueryInfo* pQueryInfo)
{
	OFFSHOP_DEBUG("query type %d ",pQueryInfo->iType);

	switch (pQueryInfo->iType)
	{
	case QID_OFFLINESHOP_ADD_ITEM:
		OfflineShopResultAddItemQuery(peer, msg, pQueryInfo);
		break;

	case QID_OFFLINESHOP_CREATE_SHOP:
		OfflineShopResultCreateShopQuery(peer, msg, pQueryInfo);
		break;

	case QID_OFFLINESHOP_CREATE_SHOP_ADD_ITEM:
		OfflineShopResultCreateShopAddItemQuery(peer, msg, pQueryInfo);
		break;

	case QID_OFFLINESHOP_SAFEBOX_ADD_ITEM:
		OfflineShopResultSafeboxAddItemQuery(peer, msg, pQueryInfo);
		break;

	case QID_OFFLINESHOP_OFFER_ADD:
		OfflineShopResultOfferAddQuery(peer, msg, pQueryInfo);
		break;

	case QID_OFFLINESHOP_EDIT_ITEM:
	case QID_OFFLINESHOP_REMOVE_ITEM:
	case QID_OFFLINESHOP_DELETE_SHOP:
	case QID_OFFLINESHOP_DELETE_SHOP_ITEM:
	case QID_OFFLINESHOP_SHOP_CHANGE_NAME:
	case QID_OFFLINESHOP_SAFEBOX_DELETE_ITEM:
	case QID_OFFLINESHOP_SAFEBOX_UPDATE_VALUTES:
	case QID_OFFLINESHOP_SAFEBOX_INSERT_VALUTES:
	case QID_OFFLINESHOP_SAFEBOX_UPDATE_VALUTES_ADDING:
	case QID_OFFLINESHOP_OFFER_UPDATE_NOTIFIED:
	case QID_OFFLINESHOP_OFFER_UPDATE_ACCEPTED:
	case QID_OFFLINESHOP_OFFER_DELETE_BY_SHOP:
	case QID_OFFLINESHOP_UPDATE_SOLD_ITEM:
	case QID_OFFLINESHOP_SHOP_UPDATE_DURATION:
	case QID_OFFLINESHOP_AUCTION_INSERT:
	case QID_OFFLINESHOP_AUCTION_INSERT_OFFER:
	case QID_OFFLINESHOP_AUCTION_DELETE:
	case QID_OFFLINESHOP_AUCTION_DELETE_OFFERS:
	case QID_OFFLINESHOP_AUCTION_UPDATE_DURATION:
		break;

	default:
		LOG_ERROR("Unknown offshop query type {} ", pQueryInfo->iType);
		break;

	}
}




void CClientManager::OfflineshopDurationProcess()
{
	m_offlineshopShopCache.ShopDurationProcess();
	m_offlineshopAuctionCache.AuctionDurationProcess();
	//patch 08-03-2020
	m_offlineshopSafeboxCache.ItemExpirationProcess();
}


void CClientManager::OfflineshopExpiredShop(uint32_t dwID)
{
	offlineshop::CShopCache::TShopCacheInfo* pInfo=nullptr;
	if(!m_offlineshopShopCache.Get(dwID, &pInfo))
		return;

	//store item in safebox
	offlineshop::CShopCache::ITEM_ITER it = pInfo->itemsmap.begin();
	for(; it!=pInfo->itemsmap.end(); it++)
		m_offlineshopSafeboxCache.AddItem(dwID, it->second.item);

	//store back offer valutes in safebox
	offlineshop::COfferCache::OFFERIDVEC* pVec=nullptr;
	offlineshop::COfferCache::TOfferCacheInfo* pOffer=nullptr;

	if (m_offlineshopOfferCache.GetOffersByShopOwner(dwID, &pVec))
	{
		for (uint32_t i = 0; i < pVec->size(); i++)
		{
			if (m_offlineshopOfferCache.Get(pVec->at(i), &pOffer) && !pOffer->bAccepted)
			{
				offlineshop::TValutesInfo valutes;
#ifdef __ENABLE_CHEQUE_SYSTEM__
				valutes.illYang = pOffer->price.illYang % YANG_PER_CHEQUE;
				valutes.iCheque = pOffer->price.illYang / YANG_PER_CHEQUE;
#else
				valutes.illYang = pOffer->price.illYang;
#endif

				m_offlineshopSafeboxCache.AddValutes(pOffer->dwOffererID, valutes);
				SendOfflineshopSafeboxAddValutes(pOffer->dwOffererID , valutes);
			}
		}
	}


	//close shop 
	m_offlineshopShopCache.CloseShop(dwID);
	m_offlineshopOfferCache.RemoveOffersByShopOwner(dwID);

	SendOfflineshopShopExpired(dwID);
}




void CClientManager::OfflineshopExpiredAuction(uint32_t dwID)
{
	//searching auction into cache
	offlineshop::TAuctionInfo* pAuction=nullptr;
	if(!m_offlineshopAuctionCache.Get(dwID, &pAuction))
		return;

	//get best offer info
	offlineshop::TAuctionOfferInfo* pOffer=nullptr;
	uint32_t dwNewOwner = m_offlineshopAuctionCache.GetBestBuyer(dwID, &pOffer);
	
	//no offer? back to owner
	if(dwNewOwner ==0)
		dwNewOwner = dwID;

	//insert item into safebox
	m_offlineshopSafeboxCache.AddItem(dwNewOwner , pAuction->item);


	//if best offer exists insert money in safebox
	if (pOffer)
	{
		offlineshop::TValutesInfo valute;
#ifdef __ENABLE_CHEQUE_SYSTEM__
		valute.illYang = pOffer->price.illYang % YANG_PER_CHEQUE;
		valute.iCheque = pOffer->price.illYang / YANG_PER_CHEQUE;
#else
		valute.illYang = pOffer->price.illYang;
#endif


		OFFSHOP_DEBUG("found valute best offer %lld", pOffer->price.illYang);

		m_offlineshopSafeboxCache.AddValutes(dwID,valute);
		SendOfflineshopSafeboxAddValutes(dwID, valute);
	}

#ifdef ENABLE_OFFLINESHOP_DEBUG
	else
		OFFSHOP_DEBUG("cannot found best offer pointer %u ",dwID);
#endif

	m_offlineshopAuctionCache.ExpiredAuction(dwID);
	SendOfflineshopAuctionExpired(dwID);
}





void CClientManager::OfflineshopLoadShopSafebox(CPeer* peer, uint32_t dwID)
{
	offlineshop::CSafeboxCache::TSafeboxCacheInfo* pSafebox=nullptr;
	
	
	if (!m_offlineshopSafeboxCache.Get(dwID, &pSafebox))
		pSafebox = m_offlineshopSafeboxCache.LoadSafebox(dwID);

	if (!pSafebox)
	{
		LOG_ERROR("cannot load shop safebox for pid {} ", dwID);
		return;
	}

	std::vector<offlineshop::TItemInfoEx> items;
	std::vector<uint32_t> ids;

	items.reserve(pSafebox->itemsmap.size());
	ids.reserve(pSafebox->itemsmap.size());

	for (auto it = pSafebox->itemsmap.begin(); it != pSafebox->itemsmap.end(); ++it)
	{
		ids.push_back(it->first);
		items.push_back(it->second);
	}

	SendOfflineshopSafeboxLoad(peer, dwID, pSafebox->valutes , items, ids);
}


//patch 08-03-2020
void CClientManager::OfflineshopExpiredAuctionItem(uint32_t dwOwnerID) {
	//searching auction into cache
	offlineshop::TAuctionInfo* pAuction = nullptr;
	if (!m_offlineshopAuctionCache.Get(dwOwnerID, &pAuction))
		return;

	//get best offer info
	offlineshop::TAuctionOfferInfo* pOffer = nullptr;
	uint32_t dwNewOwner = m_offlineshopAuctionCache.GetBestBuyer(dwOwnerID, &pOffer);

	//no offer? back to owner
	if (dwNewOwner != 0 && pOffer){
		offlineshop::TValutesInfo valute;
#ifdef __ENABLE_CHEQUE_SYSTEM__
		valute.illYang = pOffer->price.illYang % YANG_PER_CHEQUE;
		valute.iCheque = pOffer->price.illYang / YANG_PER_CHEQUE;
#else
		valute.illYang = pOffer->price.illYang;
#endif
		OFFSHOP_DEBUG("found valute best offer %lld", pOffer->price.illYang);

		m_offlineshopSafeboxCache.AddValutes(dwNewOwner, valute);
		SendOfflineshopSafeboxAddValutes(dwNewOwner, valute);
	}


#ifdef ENABLE_OFFLINESHOP_DEBUG
	else
		OFFSHOP_DEBUG("cannot found best offer pointer %u ", dwOwnerID);
#endif

	m_offlineshopAuctionCache.ExpiredAuction(dwOwnerID);
	SendOfflineshopAuctionExpired(dwOwnerID);
}





//updated 15-01-2020
bool CClientManager::IsUsingOfflineshopSystem(uint32_t dwID)
{
	offlineshop::CShopCache::TShopCacheInfo* pInfo = nullptr;
	if (m_offlineshopShopCache.Get(dwID, &pInfo))
		return true;

	else {
		static char query[256];
		snprintf(query, sizeof(query), "SELECT owner_id FROM player.offlineshop_shops WHERE owner_id ='%u' UNION SELECT owner_id FROM player.offlineshop_auctions WHERE owner_id ='%u' ;",
			dwID, dwID);

		std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(query));
		return pMsg->Get()->uiAffectedRows != 0;
	}
}





std::string CreateOfflineshopSelectShopItemsQuery()
{
	char szQuery[512] = "SELECT `item_id`, `owner_id`, `price_yang`, "
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

	len += snprintf(szQuery + len, sizeof(szQuery) - len, "%s", " ,`is_sold` "
#ifdef __ENABLE_CHANGELOOK_SYSTEM__
	", trans "
#endif
#ifdef ATTR_LOCK
	", locked_attr"
#endif
	//patch 08-03-2020
	", expiration "

	" FROM  `player`.`offlineshop_shop_items`;");
	return szQuery;
}

void CClientManager::RemoveOfferOnShopItem(uint32_t dwItemID)
{
	std::vector<uint32_t> offerIDs;
	m_offlineshopOfferCache.GetOffersByItemID(dwItemID, offerIDs);

	if (!offerIDs.empty())
	{
		offlineshop::TValutesInfo valute;

		for (uint32_t i = 0; i < offerIDs.size(); i++)
		{
			offlineshop::COfferCache::TOfferCacheInfo* pOffer = nullptr;
			if (m_offlineshopOfferCache.Get(offerIDs[i], &pOffer) && !pOffer->bAccepted)
			{
#ifdef __ENABLE_CHEQUE_SYSTEM__
				valute.illYang = pOffer->price.illYang % YANG_PER_CHEQUE;
				valute.iCheque = pOffer->price.illYang / YANG_PER_CHEQUE;
#else
				valute.illYang = pOffer->price.illYang;
#endif

				m_offlineshopSafeboxCache.AddValutes(pOffer->dwOffererID, valute);
				SendOfflineshopSafeboxAddValutes(pOffer->dwOffererID, valute);


				SendOfflineShopOfferCancel(pOffer->dwOwnerID, offerIDs[i], true);//offlineshop-updated 05/08/19
				m_offlineshopOfferCache.CancelOffer(offerIDs[i], pOffer->dwOwnerID);
			}
		}
	}
}

std::string CreateOfflineshopSelectAuctionsQuery()
{
	char szQuery[512] = "SELECT `owner_id`, `duration`, `name`, `init_yang`, `vnum`, `count` ";
	size_t len = strlen(szQuery);

	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
		len += snprintf(szQuery + len, sizeof(szQuery) - len,
			",`socket%d` ", i);

	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
		len += snprintf(szQuery + len, sizeof(szQuery) - len,
			",`attr%d` , `attrval%d` ", i, i);

	len += snprintf(szQuery + len, sizeof(szQuery) - len, "%s", " "
#ifdef __ENABLE_CHANGELOOK_SYSTEM__
	" , `trans` "
#endif
#ifdef ATTR_LOCK
	", locked_attr"
#endif
	//patch 08-03-2020
	", expiration "

	" FROM  `player`.`offlineshop_auctions`;");
	return szQuery;
}

#endif