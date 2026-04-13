#include "StdAfx.h"
#include "PythonNetworkStream.h"
#include "Packet.h"
#include "PythonOfflineshop.h"

#ifdef __ENABLE_NEW_OFFLINESHOP__
#define SendObj(obj) (Send(sizeof(obj) , &obj))



bool CPythonNetworkStream::RecvOfflineshopPacket()
{
	TPacketGCNewOfflineshop pack;
	if(!Recv(sizeof(pack), &pack))
		return false;

	switch (pack.bSubHeader)
	{
		case offlineshop::SUBHEADER_GC_SHOP_LIST:					return RecvOfflineshopShopList();
		case offlineshop::SUBHEADER_GC_SHOP_OPEN:					return RecvOfflineshopShopOpen();
		case offlineshop::SUBHEADER_GC_SHOP_OPEN_OWNER:				return RecvOfflineshopShopOpenOwner();
		case offlineshop::SUBHEADER_GC_SHOP_OPEN_OWNER_NO_SHOP:		return RecvOfflineshopShopOpenOwnerNoShop();
		case offlineshop::SUBHEADER_GC_SHOP_CLOSE:					return RecvOfflineshopShopClose();
		case offlineshop::SUBHEADER_GC_SHOP_BUY_ITEM_FROM_SEARCH:	return RecvOfflineshopShopBuyItemFromSearch();
		case offlineshop::SUBHEADER_GC_SHOP_FILTER_RESULT:			return RecvOfflineshopShopFilterResult();
		case offlineshop::SUBHEADER_GC_OFFER_LIST:					return RecvOfflineshopOfferList();
		case offlineshop::SUBHEADER_GC_SHOP_SAFEBOX_REFRESH:		return RecvOfflineshopShopSafeboxRefresh();
		case offlineshop::SUBHEADER_GC_AUCTION_LIST:				return RecvOfflineshopAuctionList();
		case offlineshop::SUBHEADER_GC_OPEN_MY_AUCTION:				return RecvOfflineshopOpenMyAuction();
		case offlineshop::SUBHEADER_GC_OPEN_MY_AUCTION_NO_AUCTION:	return RecvOfflineshopOpenMyAuctionNoAuction();
		case offlineshop::SUBHEADER_GC_OPEN_AUCTION:				return RecvOfflineshopOpenAuction();

#ifdef ENABLE_NEW_SHOP_IN_CITIES
		case offlineshop::SUBHEADER_GC_INSERT_SHOP_ENTITY:			return RecvOfflineshopInsertEntity();
		case offlineshop::SUBHEADER_GC_REMOVE_SHOP_ENTITY:			return RecvOfflineshopRemoveEntity();
#endif

		default:
			TraceError("UNKNOWN OFFLINESHOP SUBHEADER : %d ",pack.bSubHeader);
			return false;
	}
}



bool CPythonNetworkStream::RecvOfflineshopShopList()
{
	offlineshop::TSubPacketGCShopList subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	CPythonOfflineshop::instance().ShopListClear();

	offlineshop::TShopInfo shop;
	for (uint32_t i = 0; i < subpack.dwShopCount; i++)
	{
		if(!Recv(sizeof(shop), &shop))
			return false;

		CPythonOfflineshop::instance().ShopListAddItem(shop);
	}

	CPythonOfflineshop::instance().ShopListShow();
	return true;
}



bool CPythonNetworkStream::RecvOfflineshopShopOpen()
{
	offlineshop::TSubPacketGCShopOpen subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	offlineshop::TItemInfo itemInfo;
	std::vector<offlineshop::TItemInfo> items;

	for (uint32_t i = 0; i < subpack.shop.dwCount; i++)
	{
		if(!Recv(sizeof(itemInfo) , &itemInfo))
			return false;

		items.push_back(itemInfo);
	}


	CPythonOfflineshop::Instance().OpenShop(subpack.shop, items);
	return true;
}



bool CPythonNetworkStream::RecvOfflineshopShopOpenOwner()
{
	offlineshop::TSubPacketGCShopOpenOwner subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	OFFSHOP_DEBUG("owner id %u , count %u , duration %u , name %s , solds %u , offers%u", subpack.shop.dwOwnerID , subpack.shop.dwCount , subpack.shop.dwDuration, subpack.shop.szName, subpack.dwSoldCount, subpack.dwOfferCount);

	//offlineshop::TItemInfo itemInfo;
	std::vector<offlineshop::TItemInfo> items, solds;
	std::vector<offlineshop::TOfferInfo> offers;

	items.resize(subpack.shop.dwCount);
	solds.resize(subpack.dwSoldCount);
	offers.resize(subpack.dwOfferCount);


	if(subpack.shop.dwCount!=0)
		if(!Recv(sizeof(offlineshop::TItemInfo)*subpack.shop.dwCount, &items[0]))
			return false;

	OFFSHOP_DEBUG("recv %d item info for sale", subpack.shop.dwCount);

	if(subpack.dwSoldCount !=0)
		if(!Recv(sizeof(offlineshop::TItemInfo)*subpack.dwSoldCount, &solds[0]))
			return false;
	
	OFFSHOP_DEBUG("recv %d item info sold", subpack.dwSoldCount );

	if(subpack.dwOfferCount!=0)
		if(!Recv(sizeof(offlineshop::TOfferInfo)*subpack.dwOfferCount, &offers[0]))
			return false;

	OFFSHOP_DEBUG("recv %d offer info", subpack.dwOfferCount );

	CPythonOfflineshop::Instance().OpenShopOwner(subpack.shop, items, solds, offers);
	return true;
}



bool CPythonNetworkStream::RecvOfflineshopShopOpenOwnerNoShop()
{
	CPythonOfflineshop::instance().OpenShopOwnerNoShop();
	return true;
}



bool CPythonNetworkStream::RecvOfflineshopShopBuyItemFromSearch()
{
	offlineshop::TSubPacketGCShopBuyItemFromSearch subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	CPythonOfflineshop::Instance().BuyFromSearch(subpack.dwOwnerID, subpack.dwItemID);
	return true;
}



bool CPythonNetworkStream::RecvOfflineshopShopClose()
{
	CPythonOfflineshop::instance().ShopClose();
	return true;
}



bool CPythonNetworkStream::RecvOfflineshopShopFilterResult()
{
	offlineshop::TSubPacketGCShopFilterResult subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	offlineshop::TItemInfo itemInfo;
	std::vector<offlineshop::TItemInfo> items;

	for (uint32_t i = 0; i < subpack.dwCount; i++)
	{
		if(!Recv(sizeof(itemInfo) , &itemInfo))
			return false;

		items.push_back(itemInfo);
	}

	CPythonOfflineshop::Instance().ShopFilterResult(items);
	return true;
}





bool CPythonNetworkStream::RecvOfflineshopOfferList() //offlineshop-updated 03/08/19
{
	offlineshop::TSubPacketGCShopOfferList subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	
	std::vector<offlineshop::TOfferInfo> vec;
	std::vector<offlineshop::TMyOfferExtraInfo> extra;

	vec.resize(subpack.dwOfferCount);
	extra.resize(subpack.dwOfferCount);

	if (subpack.dwOfferCount != 0)
	{
		if (!Recv(sizeof(offlineshop::TOfferInfo)*subpack.dwOfferCount, &vec[0]))
			return false;

		//offlineshop-updated 04/08/19
		if (!Recv(sizeof(offlineshop::TMyOfferExtraInfo)*subpack.dwOfferCount, &extra[0]))
			return false;
	}

	CPythonOfflineshop::instance().OfferListReceived(vec, extra);
	return true;
}





bool CPythonNetworkStream::RecvOfflineshopShopSafeboxRefresh()
{
	offlineshop::TSubPacketGCShopSafeboxRefresh subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	std::vector<uint32_t> ids;
	std::vector<offlineshop::TItemInfoEx> items;

	ids.resize(subpack.dwItemCount);
	items.resize(subpack.dwItemCount);

	OFFSHOP_DEBUG("item count %u ", subpack.dwItemCount);

	for (uint32_t i = 0; i < subpack.dwItemCount; i++)
	{
		if(!Recv(sizeof(uint32_t), &ids[i]))
			return false;

		if(!Recv(sizeof(offlineshop::TItemInfoEx) , &items[i]))
			return false;
	}
	

	CPythonOfflineshop::instance().SafeboxRefresh(subpack.valute, ids, items);
	return true;
}




bool CPythonNetworkStream::RecvOfflineshopAuctionList()
{
	offlineshop::TSubPacketGCAuctionList subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	std::vector<offlineshop::TAuctionListElement> vector;
	vector.resize(subpack.dwCount);

	if(subpack.dwCount!=0)
		if(!Recv(sizeof(vector[0]) * vector.size(), &vector[0]))
			return false;

	CPythonOfflineshop::instance().AuctionList(vector, subpack.bOwner);
	return true;
}



bool CPythonNetworkStream::RecvOfflineshopOpenMyAuction()
{
	offlineshop::TSubPacketGCAuctionOpen subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	std::vector<offlineshop::TAuctionOfferInfo> offers;
	offers.resize(subpack.dwOfferCount);

	if(subpack.dwOfferCount!= 0)
		if(!Recv(sizeof(offers[0]) * offers.size() , &offers[0]))
			return false;

	CPythonOfflineshop::instance().MyAuctionOpen(subpack.auction, offers);
	return true;
}



bool CPythonNetworkStream::RecvOfflineshopOpenMyAuctionNoAuction()
{
	CPythonOfflineshop::instance().OpenMyAuctionNoAuction();
	return true;
}



bool CPythonNetworkStream::RecvOfflineshopOpenAuction()
{
	offlineshop::TSubPacketGCAuctionOpen subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	std::vector<offlineshop::TAuctionOfferInfo> offers;
	offers.resize(subpack.dwOfferCount);

	if(subpack.dwOfferCount!= 0)
		if(!Recv(sizeof(offers[0]) * offers.size() , &offers[0]))
			return false;

	CPythonOfflineshop::instance().AuctionOpen(subpack.auction, offers);
	return true;
}


#ifdef ENABLE_NEW_SHOP_IN_CITIES
bool CPythonNetworkStream::RecvOfflineshopInsertEntity()
{
	offlineshop::TSubPacketGCInsertShopEntity subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	OFFSHOP_DEBUG("inserting shop instance %u %s ",subpack.dwVID, subpack.szName);
	CPythonOfflineshop::Instance().InsertEntity(subpack.dwVID, subpack.iType, subpack.szName, subpack.x, subpack.y, subpack.z
#ifdef KASMIR_PAKET_SYSTEM
	, subpack.dwKasmirNpc
#endif
	);
	return true;
}




bool CPythonNetworkStream::RecvOfflineshopRemoveEntity()
{
	offlineshop::TSubPacketGCRemoveShopEntity subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	OFFSHOP_DEBUG("removing shop entity %u ",subpack.dwVID);

	CPythonOfflineshop::Instance().RemoveEntity(subpack.dwVID);
	return true;
}


void CPythonNetworkStream::SendOfflineshopOnClickShopEntity(uint32_t dwPickedShopVID)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader = HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader = offlineshop::SUBHEADER_CG_CLICK_ENTITY;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGShopClickEntity);

	offlineshop::TSubPacketCGShopClickEntity subpack;
	subpack.dwShopVID = dwPickedShopVID;

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}



	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}
	
}
#endif













void CPythonNetworkStream::SendOfflineshopShopCreate(const offlineshop::TShopInfo& shopInfo, const std::vector<offlineshop::TShopItemInfo>& items)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_CREATE_NEW;
	pack.wSize		= (WORD)(sizeof(pack) + sizeof(offlineshop::TSubPacketCGShopCreate) + items.size()*sizeof(offlineshop::TShopItemInfo));
	
	offlineshop::TSubPacketCGShopCreate subpack;
	offlineshop::CopyObject(subpack.shop, shopInfo);
	subpack.shop.dwCount = items.size();
	
	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}


	
	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}


	for (uint32_t i = 0; i < items.size(); i++)
	{
		const offlineshop::TShopItemInfo& info = items[i];
		if (!SendObj(info))
		{
			TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - items forloop", pack.bSubHeader);
			return;
		}
	}

	
}



void CPythonNetworkStream::SendOfflineshopChangeName(const char* szName)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_CHANGE_NAME;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGShopChangeName);

	offlineshop::TSubPacketCGShopChangeName subpack;
	strncpy(subpack.szName , szName, sizeof(subpack.szName));


	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}
}



void CPythonNetworkStream::SendOfflineshopForceCloseShop()
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_FORCE_CLOSE;
	pack.wSize		= sizeof(pack);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}
}




void CPythonNetworkStream::SendOfflineshopRequestShopList()
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_REQUEST_SHOPLIST;
	pack.wSize		= sizeof(pack);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

}




void CPythonNetworkStream::SendOfflineshopOpenShop(uint32_t dwOwnerID)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_OPEN;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGShopOpen);

	offlineshop::TSubPacketCGShopOpen subpack;
	subpack.dwOwnerID = dwOwnerID;

	OFFSHOP_DEBUG("shop id %u ",dwOwnerID);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}

}



void CPythonNetworkStream::SendOfflineshopOpenShopOwner()
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_OPEN_OWNER;
	pack.wSize		= sizeof(pack);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

}




void CPythonNetworkStream::SendOfflineshopBuyItem(uint32_t dwOwnerID, uint32_t dwItemID, bool isSearch, long long TotalPriceSeen)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_BUY_ITEM;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGShopBuyItem);

	offlineshop::TSubPacketCGShopBuyItem subpack;
	subpack.dwItemID	= dwItemID;
	subpack.dwOwnerID	= dwOwnerID;
	subpack.bIsSearch	= isSearch;
	subpack.TotalPriceSeen = TotalPriceSeen;

	OFFSHOP_DEBUG("shop owner %u , item id %u ",dwOwnerID , dwItemID);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}

}




void CPythonNetworkStream::SendOfflineshopAddItem(offlineshop::TShopItemInfo& itemInfo)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_ADD_ITEM;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGAddItem);

	offlineshop::TSubPacketCGAddItem subpack;
	offlineshop::CopyObject(subpack.pos , itemInfo.pos);
	offlineshop::CopyObject(subpack.price , itemInfo.price);


	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}

}



void CPythonNetworkStream::SendOfflineshopRemoveItem(uint32_t dwItemID)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_REMOVE_ITEM;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGRemoveItem);

	offlineshop::TSubPacketCGRemoveItem subpack;
	subpack.dwItemID	= dwItemID;

	OFFSHOP_DEBUG("sending remove item %u ",dwItemID);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}

}



void CPythonNetworkStream::SendOfflineShopEditItem(uint32_t dwItemID, const offlineshop::TPriceInfo& price)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_EDIT_ITEM;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGEditItem);

	offlineshop::TSubPacketCGEditItem subpack;
	subpack.dwItemID	= dwItemID;
	offlineshop::CopyObject(subpack.price , price);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}

}




void CPythonNetworkStream::SendOfflineshopFilterRequest(const offlineshop::TFilterInfo& filter)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_FILTER_REQUEST;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGFilterRequest);

	offlineshop::TSubPacketCGFilterRequest subpack;
	offlineshop::CopyObject(subpack.filter, filter);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}


}




void CPythonNetworkStream::SendOfflineshopOfferCreate(const offlineshop::TOfferInfo& offer)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_OFFER_CREATE;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGOfferCreate);

	offlineshop::TSubPacketCGOfferCreate subpack;
	offlineshop::CopyObject(subpack.offer, offer);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}

}



void CPythonNetworkStream::SendOfflineshopOfferAccept(uint32_t dwOfferID)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_OFFER_ACCEPT;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGOfferAccept);

	offlineshop::TSubPacketCGOfferAccept subpack;
	subpack.dwOfferID	= dwOfferID;

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}


}




void CPythonNetworkStream::SendOfflineshopOfferCancel(uint32_t dwOfferID, uint32_t dwOwnerID)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_OFFER_CANCEL;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGOfferCancel);

	offlineshop::TSubPacketCGOfferCancel subpack;
	subpack.dwOfferID	= dwOfferID;
	subpack.dwOwnerID	= dwOwnerID;

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}
}






void CPythonNetworkStream::SendOfflineshopOfferListRequest()
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_REQUEST_OFFER_LIST;
	pack.wSize		= sizeof(pack);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

}

void CPythonNetworkStream::SendOfflineshopSafeboxOpen()
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_SAFEBOX_OPEN;
	pack.wSize		= sizeof(pack);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

}





void CPythonNetworkStream::SendOfflineshopSafeboxGetItem(uint32_t dwItemID)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_SAFEBOX_GET_ITEM;

	offlineshop::TSubPacketCGShopSafeboxGetItem subpack;
	subpack.dwItemID = dwItemID;

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}

}



void CPythonNetworkStream::SendOfflineshopSafeboxGetValutes(const offlineshop::TValutesInfo& valutes)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_SAFEBOX_GET_VALUTES;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGShopSafeboxGetValutes);

	offlineshop::TSubPacketCGShopSafeboxGetValutes subpack;
	offlineshop::CopyObject(subpack.valutes, valutes);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	if(!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.bSubHeader);
		return;
	}

}



void CPythonNetworkStream::SendOfflineshopSafeboxClose()
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_SHOP_SAFEBOX_CLOSE;
	pack.wSize		= sizeof(pack);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

}





void CPythonNetworkStream::SendOfflineshopAuctionListRequest()
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_AUCTION_LIST_REQUEST;
	pack.wSize		= sizeof(pack);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}
}



void CPythonNetworkStream::SendOfflineshopAuctionOpen(uint32_t dwOwnerID)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_AUCTION_OPEN_REQUEST;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGAuctionOpenRequest);

	offlineshop::TSubPacketCGAuctionOpenRequest subpack;
	subpack.dwOwnerID = dwOwnerID;


	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}

	
	if (!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack",pack.bSubHeader);
		return;
	}

}



void CPythonNetworkStream::SendOfflineshopAuctionAddOffer(uint32_t dwOwnerID, const offlineshop::TPriceInfo& price)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_AUCTION_ADD_OFFER;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGAuctionAddOffer);

	offlineshop::TSubPacketCGAuctionAddOffer subpack;
	subpack.dwOwnerID = dwOwnerID;
	offlineshop::CopyObject(subpack.price, price);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}


	if (!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack",pack.bSubHeader);
		return;
	}

}



void CPythonNetworkStream::SendOfflineshopAuctionExitFrom(uint32_t dwOwnerID)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_EXIT_FROM_AUCTION;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGAuctionExitFrom);

	offlineshop::TSubPacketCGAuctionExitFrom subpack;
	subpack.dwOwnerID = dwOwnerID;
	
	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}


	if (!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack",pack.bSubHeader);
		return;
	}
}



void CPythonNetworkStream::SendOfflineshopAuctionCreate(const TItemPos& pos, const offlineshop::TPriceInfo& price, uint32_t dwDuration)
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_CREATE_AUCTION;
	pack.wSize		= sizeof(pack) + sizeof(offlineshop::TSubPacketCGAuctionCreate);

	offlineshop::TSubPacketCGAuctionCreate subpack;
	subpack.dwDuration	= dwDuration;
	offlineshop::CopyObject(subpack.pos, pos);
	offlineshop::CopyObject(subpack.init_price , price);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}


	if (!SendObj(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack",pack.bSubHeader);
		return;
	}
}



void CPythonNetworkStream::SendOfflineshopAuctionOpenMy()
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_MY_AUCTION_OPEN_REQUEST;
	pack.wSize		= sizeof(pack);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}
}

void CPythonNetworkStream::SendOfflineshopCloseMyAuction()
{
	TPacketCGNewOfflineShop pack;
	pack.bHeader = HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader = offlineshop::SUBHEADER_CG_AUCTION_CLOSE;
	pack.wSize = sizeof(pack);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}
}

void CPythonNetworkStream::SendOfflineshopCloseBoard()
{
	if(m_strPhase != "Game")
		return;

	TPacketCGNewOfflineShop pack;
	pack.bHeader	= HEADER_CG_NEW_OFFLINESHOP;
	pack.bSubHeader	= offlineshop::SUBHEADER_CG_CLOSE_BOARD;
	pack.wSize		= sizeof(pack);

	if (!SendObj(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.bSubHeader);
		return;
	}
}

#endif
