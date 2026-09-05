#include "stdafx.h"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include <common/tables.h>
#include "packet.h"
#include "item.h"
#include "char_interface.hpp"
#include "item_manager.h"
#include "desc.h"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/OfflineShopEntityRegistry.hpp"
#include "ecs/services/EntityNetworkDispatch.hpp"
#include "ecs/services/SpatialService.hpp"
#include "ecs/systems/ItemSystem.hpp"

#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"

#ifdef __ENABLE_NEW_OFFLINESHOP__


namespace offlineshop
{
	/*
		structure
		-	contain item informations (offlineshop::SShopItemInfo)
		-	contain price (prezzo)
		-	contain window type (shop/shop safebox)

		methods
		-	constructor using LPITEM, price, window (immediatly set)
		-	constructor default (all to zero, alloc id)
		-	copy constructor (using in vector)

		-	gettable to get prototype table directly (very useful in filtering)
		-	getprice to get the sell price
		-	getinfo to get the item informations

		-	setprice to set sell price
		-	setinfo using LPITEM to get immediatly information from item
		-	setwindow to set the current window (shop/ shop safebox)
		-	getwindow to get the current window ||
	*/


	CShopItem::CShopItem(uint32_t dwID) : m_dwOwnerID(0)
	{
		m_dwID = dwID;
		m_byWindow = 0;

		ZeroObject(m_itemInfo);
		ZeroObject(m_priceInfo);
	}


	CShopItem::CShopItem(const CShopItem& rCopy) : m_dwOwnerID(0)
	{
		m_dwID = rCopy.GetID();
		m_byWindow = rCopy.GetWindow();

		CopyObject(m_itemInfo, *rCopy.GetInfo());
		CopyObject(m_priceInfo, *rCopy.GetPrice());
	}

	CShopItem::CShopItem(LPITEM pItem, const TPriceInfo& sPrice, uint8_t bWindowType, uint32_t dwID): m_dwID(0)
	{
		//setting item window
		SetWindow(bWindowType);


		//setting price info
		CopyObject(m_priceInfo, sPrice);

		//setting info
		if (pItem)
		{
			const entt::entity item = pItem ? pItem->GetEntityHandle() : entt::null;
			//basic info
			m_itemInfo.dwCount	= ItemSystem::GetItemCount(item);
			{
				const entt::entity ownerEntity = ItemSystem::GetItemOwnerEntity(item);
				auto* owner = ecs::LegacyCharOf(ownerEntity);
				m_dwOwnerID			= owner ? ecs::PlayerRuntime::GetPlayerID(((owner) ? (owner)->GetEntityHandle() : entt::null)) : 0;
			}
			m_itemInfo.dwVnum	= ItemSystem::GetItemVnum(item);
			//patch 08-03-2020
			m_itemInfo.expiration = GetItemExpiration(pItem->GetEntityHandle());

			//attributes
			const TPlayerItemAttribute* pAttributes = pItem->GetAttributes();
			memcpy(m_itemInfo.aAttr, pAttributes, sizeof(m_itemInfo.aAttr));

			//sockets
			const int32_t* pSockets = pItem->GetSockets();
			memcpy(m_itemInfo.alSockets, pSockets, sizeof(m_itemInfo.alSockets));

#ifdef __ENABLE_CHANGELOOK_SYSTEM__
			m_itemInfo.dwTransmutation = pItem->GetTransmutation();
#endif
#ifdef ATTR_LOCK
			m_itemInfo.iLockedAttr = pItem->GetLockedAttr();
#endif
		}

		else
		{
			LOG_ERROR("offlineshop::CShopItem - constructor using item/price/window : item == nullptr! ");
		}

		if (dwID != 0)
			m_dwID = dwID;
	}


	CShopItem::~CShopItem()
	{
	}

	bool CShopItem::GetTable(TItemTable ** ppTable) const
	{
		if ((*ppTable = ITEM_MANAGER::instance().GetTable(m_itemInfo.dwVnum)))
			return true;
		return false;
	}

	TPriceInfo* CShopItem::GetPrice() const
	{
		return const_cast<TPriceInfo*>(&m_priceInfo);
	}

	LPITEM CShopItem::CreateItem() const
	{
		LPITEM item = ITEM_MANAGER::instance().CreateItem(m_itemInfo.dwVnum, m_itemInfo.dwCount);
		if (!item)
			return item;


		item->SetAttributes(m_itemInfo.aAttr);
		item->SetSockets(m_itemInfo.alSockets);
#ifdef __ENABLE_CHANGELOOK_SYSTEM__
		item->SetTransmutation(m_itemInfo.dwTransmutation);
#endif
#ifdef ATTR_LOCK
		item->SetLockedAttr(m_itemInfo.iLockedAttr);
#endif


		return item;
	}

	TItemInfoEx* CShopItem::GetInfo() const
	{
		return const_cast<TItemInfoEx*>(&m_itemInfo);
	}

	void CShopItem::SetInfo(LPITEM pItem)
	{
		if (pItem)
		{
			const entt::entity item = pItem ? pItem->GetEntityHandle() : entt::null;
			//basic info
			m_itemInfo.dwCount	= ItemSystem::GetItemCount(item);
			{
				const entt::entity ownerEntity = ItemSystem::GetItemOwnerEntity(item);
				auto* owner = ecs::LegacyCharOf(ownerEntity);
				m_dwOwnerID			= owner ? ecs::PlayerRuntime::GetPlayerID(((owner) ? (owner)->GetEntityHandle() : entt::null)) : 0;
			}
			m_itemInfo.dwVnum	= ItemSystem::GetItemVnum(item);

			//attributes
			const TPlayerItemAttribute* pAttributes = pItem->GetAttributes();
			memcpy(m_itemInfo.aAttr, pAttributes, sizeof(m_itemInfo.aAttr));

			//sockets
			const int32_t* pSockets = pItem->GetSockets();
			memcpy(m_itemInfo.alSockets, pSockets, sizeof(m_itemInfo.alSockets));
#ifdef __ENABLE_CHANGELOOK_SYSTEM__
			m_itemInfo.dwTransmutation = pItem->GetTransmutation();
#endif
#ifdef ATTR_LOCK
			m_itemInfo.iLockedAttr = pItem->GetLockedAttr();
#endif
		}

		else
		{
			LOG_ERROR("offlineshop::CShopItem - SetInfo: item == nullptr! ");
		}
	}


	void CShopItem::SetInfo(const TItemInfoEx& info)
	{
		CopyObject(m_itemInfo, info);
	}


	void CShopItem::SetPrice(const TPriceInfo& sPrice)
	{
		CopyObject(m_priceInfo, sPrice);
	}

	void CShopItem::SetWindow(uint8_t byWin)
	{
		m_byWindow = byWin;
	}

	uint8_t CShopItem::GetWindow() const
	{
		return m_byWindow;
	}

	uint32_t CShopItem::GetID() const
	{
		return m_dwID;
	}


	void CShopItem::SetOwnerID(uint32_t dwOwnerID)
	{
		m_dwOwnerID = dwOwnerID;
	}



	bool CShopItem::CanBuy(LPCHARACTER ch)
	{
		if(!ch)
			return false;

		if(m_priceInfo.illYang > ecs::PointSystem::GetGold(((ch) ? (ch)->GetEntityHandle() : entt::null)))
			return false;
#ifdef __ENABLE_CHEQUE_SYSTEM__
		if(m_priceInfo.iCheque > ch->GetCheque())
			return false;
#endif
		return true;
	}


	void CShopItem::operator=(const CShopItem & rItem)
	{
		m_dwID = rItem.GetID();
		m_byWindow = rItem.GetWindow();

		CopyObject(m_itemInfo, *rItem.GetInfo());
		CopyObject(m_priceInfo, *rItem.GetPrice());
	}









	/*
	CShop

	structure
	-	item pointers vector
	-	owner's player id
	-	duration
	-	offers vector
	-	gusts list
	-	shop virtual id

	metodi
	-	getitems to get a pointer to items vector
	-	getoffers to get a pointer to offers vector
	-	getguests to get a pointer to the guests list
	-	notify offers to notify to character new offer

	-	getduration to know duration of the shop
	-	setduration used to set the initi duration a the boot
	-	decreaseduration to get and decrease duration sametime

	-	setownerpid to set the owner's pid
	-	getownerpid to get the owner's pid

	-	addguest to add a new guest to the shop
	-	removeguest to remove a guest to the shop (close board or logout)

	-	setitems used to set initial item on boot
	-	modifyitem to modify an item and refresh to guest
	-	buyitem guest , used to buy an item when you are looking the shop
	-	buyitem character, used to buy item when you are looking a filtered search
	-	removeitem to remove an item and send refresh to uests
	-	additem to add an item to the shop and send refresh to guests
	-	clear to delete the item pointer in vector item and remove all element into containers
	-	getitem to find item by virtualid
	-	findowner to use find by pid (char manager) to search owner id

	-	addoffer to add an offer and send notification at the owner (if online, otherwise it will recv it when login)
	-	(private) refresh list item to a guest or all (args != null -> send to one, otherwise send to all)
	-	(private) notifyoffer to send notify packet

	*/



	CShop::CShop()
	{
		m_dwPID = 0;
		m_dwDuration = 0;
		m_stName.clear();
#ifdef KASMIR_PAKET_SYSTEM
		m_Race = 0;
#endif
	}


	CShop::CShop(const CShop& rCopy)
	{
		CopyContainer(m_listGuests, *rCopy.GetGuests());
		CopyContainer(m_vecItems,   *rCopy.GetItems());
		CopyContainer(m_vecOffers,  *rCopy.GetOffers());

		m_dwPID			= rCopy.GetOwnerPID();
		m_dwDuration	= rCopy.GetDuration();
		m_stName		= rCopy.GetName();
#ifdef KASMIR_PAKET_SYSTEM
		m_Race = rCopy.GetRace();
#endif
	}


	CShop::~CShop()
	{
	}




	CShop::VECSHOPITEM * CShop::GetItems() const
	{
		return (CShop::VECSHOPITEM *)&m_vecItems;
	}

	CShop::VECSHOPITEM* CShop::GetItemsSold() const
	{
		return (CShop::VECSHOPITEM *)&m_vecItemSold;
	}


	CShop::VECSHOPOFFER * CShop::GetOffers() const
	{
		return (CShop::VECSHOPOFFER *)&m_vecOffers;
	}

	CShop::LISTGUEST * CShop::GetGuests() const
	{
		return (CShop::LISTGUEST *)&m_listGuests;
	}


	void CShop::SetDuration(uint32_t dwDuration)
	{
		m_dwDuration=dwDuration;
	}

	uint32_t CShop::DecreaseDuration()
	{
		return --m_dwDuration;
	}

	uint32_t CShop::GetDuration() const
	{
		return m_dwDuration;
	}



	void CShop::SetOwnerPID(uint32_t dwOwnerPID)
	{
		m_dwPID = dwOwnerPID;
	}



	uint32_t CShop::GetOwnerPID() const
	{
		return m_dwPID;
	}



	bool CShop::AddGuest(LPCHARACTER ch)
	{
		for (LISTGUEST::iterator it = m_listGuests.begin(); it != m_listGuests.end(); it++)
			if (*it == AS_GUESTID(ch))
				return false;

		m_listGuests.push_back(AS_GUESTID(ch));
		//__RefreshItems(ch);
		return true;
	}



	bool CShop::RemoveGuest(LPCHARACTER ch)
	{
		for (LISTGUEST::iterator it = m_listGuests.begin(); it != m_listGuests.end(); it++)
		{
			if (*it == AS_GUESTID(ch))
			{
				m_listGuests.erase(it);
				return true;
			}
		}

		return false;
	}



	void CShop::SetItems(VECSHOPITEM * pVec)
	{
		CopyContainer(m_vecItems, *pVec);
		for(uint32_t i=0; i < m_vecItems.size(); i++)
			m_vecItems[i].SetWindow(NEW_OFFSHOP);


		if (!m_listGuests.empty())
			__RefreshItems();
	}


	bool CShop::AddItem(CShopItem & rItem)
	{
		rItem.SetWindow(NEW_OFFSHOP);
		m_vecItems.push_back(rItem);
		__RefreshItems();
		return true;
	}

	bool CShop::AddItemSold(CShopItem & rItem)
	{
		rItem.SetWindow(NEW_OFFSHOP);
		m_vecItemSold.push_back(rItem);
		__RefreshItems();
		return true;
	}



	bool CShop::RemoveItem(uint32_t dwItemID)
	{
		for (VECSHOPITEM::iterator it = m_vecItems.begin();
			it != m_vecItems.end();
			it++)
		{
			if (dwItemID == it->GetID())
			{
				m_vecItems.erase(it);
				__RefreshItems();
				return true;
			}
		}

		return false;
	}



	bool CShop::ModifyItem(uint32_t dwItemID, CShopItem & rItem)
	{
		if (rItem.GetID() != dwItemID)
		{
			LOG_ERROR("have you forgot to set item id ? {} - {} don't match ", rItem.GetID(), dwItemID);
			return false;
		}


		CShopItem* pItem = nullptr;
		if (!GetItem(dwItemID, &pItem))
			return false;

		*pItem = rItem;
		pItem->SetWindow(NEW_OFFSHOP);
		__RefreshItems();
		return true;
	}




	bool CShop::BuyItem(uint32_t dwItem)
	{
		CShopItem* pItem = nullptr;
		if (!GetItem(dwItem, &pItem))
			return false;


		m_vecItemSold.emplace_back(*pItem);
		RemoveItem(dwItem);
		return true;
	}



	bool CShop::GetItem(uint32_t dwItem, CShopItem** ppItem)
	{
		for (auto it = m_vecItems.begin();
			it != m_vecItems.end();
			++it)
		{
			if (dwItem == it->GetID())
			{
				*ppItem = &(*it);
				return true;
			}
		}

		return false;
	}



	bool CShop::GetItemSold(uint32_t dwItem, CShopItem** ppItem)
	{
		for (VECSHOPITEM::iterator it = m_vecItemSold.begin();
			it != m_vecItemSold.end();
			it++)
		{
			if (dwItem == it->GetID())
			{
				*ppItem = &(*it);
				return true;
			}
		}

		return false;
	}


	bool CShop::AddOffer(const TOfferInfo* pOfferInfo)
	{

		CShopItem* pitem = nullptr;
		if (!GetItem(pOfferInfo->dwItemID, &pitem))
			return false;

		m_vecOffers.push_back(*pOfferInfo);

		if (!pOfferInfo->bNoticed && !pOfferInfo->bAccepted)
		{
			//offlineshop-updated 04/08/19 reminder
			LPCHARACTER pkOwner = FindOwnerCharacter();
			if (pkOwner)
			{
				if (pkOwner->GetOfflineShop() && pkOwner->GetOfflineShop() == pkOwner->GetOfflineShopGuest())
				{
					NotifyOffers(pkOwner);
					GetManager().SendShopOpenMyShopClientPacket(((pkOwner) ? (pkOwner)->GetEntityHandle() : entt::null));
				}
			}
		}

		return true;
	}



	bool CShop::AcceptOffer(const TOfferInfo* pOffer)
	{
		CShopItem* pkItem = nullptr;
		if(!GetItem(pOffer->dwItemID, &pkItem))
			return false;

		TPriceInfo* pPrice = pkItem->GetPrice();
		CopyObject(*pPrice, pOffer->price);
		BuyItem(pkItem->GetID());



		for (uint32_t i = 0; i < m_vecOffers.size(); i++)
		{
			TOfferInfo& offer = m_vecOffers.at(i);
			if (offer.dwOfferID == offer.dwOwnerID)
			{
				offer.bAccepted = true;
				
				/*
				offer.bNoticed = false;
				
				LPCHARACTER pkOwner = FindOwnerCharacter();
				if (pkOwner)
					NotifyAcceptedOffers(pkOwner);
				*/
				return true;
			}
		}

		

		return false;
	}



	void CShop::__RefreshItems(LPCHARACTER ch)
	{
		if(m_listGuests.empty())
			return;

		if (!ch)
		{
			for (auto it = m_listGuests.begin(); it != m_listGuests.end(); ++it)
			{
				LPCHARACTER ch = AS_LPGUEST(*it);
				if (!ch)
					continue;

				if ((ecs::PlayerRuntime::GetPlayerID(((ch) ? (ch)->GetEntityHandle() : entt::null))) == m_dwPID)
					GetManager().SendShopOpenMyShopClientPacket(((ch) ? (ch)->GetEntityHandle() : entt::null));

				else
					GetManager().SendShopOpenClientPacket(((ch) ? (ch)->GetEntityHandle() : entt::null), this);
			}
		}


		else
		{
			if((ecs::PlayerRuntime::GetPlayerID(((ch) ? (ch)->GetEntityHandle() : entt::null))) == m_dwPID)
				GetManager().SendShopOpenMyShopClientPacket(((ch) ? (ch)->GetEntityHandle() : entt::null));

			else
				GetManager().SendShopOpenClientPacket(((ch) ? (ch)->GetEntityHandle() : entt::null), this);
		}
	}



	void CShop::Clear()
	{
		m_vecItems.clear();
		m_vecOffers.clear();
		m_listGuests.clear();
	}


	LPCHARACTER CShop::FindOwnerCharacter()
	{
		return CHARACTER_MANAGER::instance().FindByPID(GetOwnerPID());
	}


	void CShop::NotifyOffers(LPCHARACTER ch)
	{
		for (VECSHOPOFFER::iterator it = m_vecOffers.begin();
			it != m_vecOffers.end();
			it++)
		{
			if (!it->bNoticed && !it->bAccepted)
			{
				it->bNoticed = true;
				//__SendOfferNotify(ch, &(*it));
				GetManager().SendShopOfferNotifiedDBPacket(it->dwOfferID, it->dwOwnerID);
			}
		}
	}

	void  CShop::NotifyAcceptedOffers(LPCHARACTER ch) //UNUSED
	{
		for (VECSHOPOFFER::iterator it = m_vecOffers.begin();
			it != m_vecOffers.end();
			it++)
		{
			if (!it->bNoticed && it->bAccepted)
			{
				it->bNoticed = true;
				//__SendOfferNotify(ch, &(*it));
				GetManager().SendShopOfferNotifiedDBPacket(it->dwOfferID, it->dwOwnerID);
			}
		}
	}



	void CShop::__SendOfferNotify(entt::entity ch, TOfferInfo* pOffer)
	{
		//TODO : add send packet
	}



	const char* CShop::GetName() const
	{
		return m_stName.c_str();
	}


	void CShop::SetName(const char* pcszName)
	{
		m_stName = pcszName;
	}

#ifdef KASMIR_PAKET_SYSTEM
	void CShop::SetRace(uint32_t dwRace) {
		m_Race = dwRace;
	}
	
	uint32_t CShop::GetRace() const {
		return m_Race;
	}
#endif

	void CShop::RefreshToOwner()
	{
		LPCHARACTER ch = FindOwnerCharacter();
		if(!ch)
			return;

		GetManager().SendShopOpenMyShopClientPacket(((ch) ? (ch)->GetEntityHandle() : entt::null));
	}




	//AUCTION
	CAuction::CAuction()
	{
		ZeroObject(m_info);
		ZeroObject(m_bestOffer);
		m_dwBestBuyer=0;
	}


	CAuction::~CAuction()
	{
	}


	void CAuction::SetInfo(const TAuctionInfo& auction)
	{
		CopyObject(m_info, auction);
	}



	void CAuction::SetOffers(const std::vector<TAuctionOfferInfo>& vec)
	{
		CopyContainer(m_offersVec, vec);
	}




	bool CAuction::AddOffer(const TAuctionOfferInfo& offer)
	{
		m_offersVec.push_back(offer);
		__SetBestOffer();
		__RefreshToGuests();
		return true;
	}



	bool CAuction::AddGuest(LPCHARACTER ch)
	{
		for (auto it = m_guestsList.begin(); it != m_guestsList.end(); ++it)
		{
			if (AS_GUESTID(ch) == *it)
				return false;
		}

		m_guestsList.push_back(AS_GUESTID(ch));
		ch->SetAuctionGuest(this);
		GetManager().SendAuctionOpenAuctionClientPacket(((ch) ? (ch)->GetEntityHandle() : entt::null), m_info, m_offersVec);
		return true;
	}




	bool CAuction::RemoveGuest(LPCHARACTER ch)
	{
		for (auto it = m_guestsList.begin(); it != m_guestsList.end(); ++it)
		{
			if (AS_GUESTID(ch) == *it)
			{
				m_guestsList.erase(it);
				ch->SetAuctionGuest(nullptr);
				return true;
			}
		}

		return false;
	}




	void CAuction::DecreaseDuration()
	{
		if(m_info.dwDuration!=0)
			m_info.dwDuration--;
	}

	void CAuction::IncreaseDuration()
	{
		m_info.dwDuration++;
	}

	const TAuctionInfo& CAuction::GetInfo() const
	{
		return m_info;
	}


	const CAuction::AUCTION_OFFERVEC& CAuction::GetOffers() const
	{
		return m_offersVec;
	}


	const TPriceInfo& CAuction::GetBestOffer() const
	{
		return m_bestOffer;
	}



	CShop::LISTGUEST& CAuction::GetGuests()
	{
		return m_guestsList;
	}



	const uint32_t CAuction::GetBestBuyer() const
	{
		return m_dwBestBuyer;
	}



	void CAuction::__RefreshToGuests()
	{
		for (auto it=m_guestsList.begin(); it != m_guestsList.end(); ++it) {
			LPCHARACTER ch = AS_LPGUEST(*it);
			if (!ch)
				continue;

			GetManager().SendAuctionOpenAuctionClientPacket(((ch) ? (ch)->GetEntityHandle() : entt::null), m_info, m_offersVec);
		}
	}




	bool CAuction::__SetBestOffer()
	{
		TPriceInfo* pInfo=nullptr;
		for (auto it = m_offersVec.begin(); it != m_offersVec.end(); ++it)
		{
			if (!pInfo)
			{
				pInfo = &(it->price);
				m_dwBestBuyer = it->dwBuyerID;
				continue;
			}

			if (pInfo->illYang < it->price.illYang)
			{
				pInfo = &it->price;
				m_dwBestBuyer = it->dwBuyerID;
			}

		}

		if(pInfo)
			CopyObject(m_bestOffer, *pInfo);


		return pInfo != nullptr;
	}



#ifdef ENABLE_NEW_SHOP_IN_CITIES

	ShopEntity::ShopEntity()
	{
		m_dwVID	= AllocID();
		m_pkShop= nullptr;
		CEntity::Initialize(ENTITY_NEWSHOPS);
	}


	//NEW ENTITY




	const char* ShopEntity::GetShopName() const
	{
		return m_szName;
	}



	void ShopEntity::SetShopName(const char* name)
	{
		strncpy(m_szName, name , sizeof(m_szName));
	}

#ifdef KASMIR_PAKET_SYSTEM
	void ShopEntity::SetShopRace(uint32_t dwRace) {
		m_Race = dwRace;
	}
	
	uint32_t ShopEntity::GetShopRace() {
		return m_Race;
	}
#endif

	int ShopEntity::GetShopType()
	{
		return m_iType;
	}


	void ShopEntity::SetShopType(int iType)
	{
		m_iType=iType;
	}


	void ShopEntity::SetShop(offlineshop::CShop* pOfflineShop)
	{
		m_pkShop=pOfflineShop;
	}

#endif
}



#endif //__ENABLE_NEW_OFFLINESHOP__







