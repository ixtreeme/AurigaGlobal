#include "stdafx.h"
#include <common/tables.h>
#include "packet.h"
#include "item.h"
#include "char_interface.hpp"
#include "item_manager.h"
#include "desc.h"
#include "char_manager.h"

#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"

#ifdef __ENABLE_NEW_OFFLINESHOP__


namespace offlineshop
{


	/*
	structure
	-	vector of items
	-	amount of valute
	-	owner character

	methods

	- 	constructor which set the character owner

	-	setitems to set items when created object
	-	setvalute to set the valute when created object

	-	additem to add an item in the stock
	-	removeitem to remove item in the stock

	-	addvalute to add amount to the valute counts
	-	removevalute to remove amount to the valute counts (return false if val>amount)

	-	getitem to easly find item by id
	-	getvalutes to get the amounts 
	-	getitems to get a pointer to the item vectors

	-	getowner to get the character owner

	*/




	CShopSafebox::CShopSafebox(entt::entity owner)
	{
		ZeroObject(m_valutes);
		m_pkOwner = owner;
	}


	CShopSafebox::CShopSafebox()
	{
		ZeroObject(m_valutes);
		m_pkOwner = entt::null;
	}


	CShopSafebox::CShopSafebox(const CShopSafebox& rCopy)
	{
		CopyObject(m_valutes , rCopy.m_valutes);
		CopyContainer(m_vecItems , rCopy.m_vecItems);

		m_pkOwner = rCopy.m_pkOwner;
	}


	CShopSafebox::~CShopSafebox()
	{
	}


	void CShopSafebox::SetOwner(entt::entity owner)
	{
		m_pkOwner = owner;
	}


	void CShopSafebox::SetItems(VECITEM * pVec)
	{
		CopyContainer(m_vecItems, *pVec);
	}

	void CShopSafebox::SetValuteAmount(SValuteAmount val)
	{
		CopyObject(m_valutes, val);
	}

	bool CShopSafebox::AddItem(CShopItem * pItem)
	{
		CShopItem* pSearch= nullptr;
		if(GetItem(pItem->GetID(), &pSearch))
			return false;

		m_vecItems.push_back(CShopItem(*pItem));
		return true;
	}

	bool CShopSafebox::RemoveItem(uint32_t dwItemID)
	{
		for (VECITEM::iterator it = m_vecItems.begin();
			it != m_vecItems.end();
			it++)
		{
			if (dwItemID == it->GetID())
			{
				m_vecItems.erase(it);
				return true;
			}
		}

		return false;
	}

	void CShopSafebox::AddValute(SValuteAmount val)
	{
		m_valutes+=(val);
		//RefreshValuteToOwner();
	}

	bool CShopSafebox::RemoveValute(SValuteAmount val)
	{
		if(m_valutes.illYang < val.illYang)
			return false;

#ifdef __ENABLE_CHEQUE_SYSTEM__
		if(m_valutes.iCheque < val.iCheque)
			return false;
#endif		


		m_valutes -= val;
		//RefreshValuteToOwner();
		return true;
	}

	CShopSafebox::VECITEM * CShopSafebox::GetItems()
	{
		return &m_vecItems;
	}

	CShopSafebox::SValuteAmount CShopSafebox::GetValutes()
	{
		return m_valutes;
	}

	bool CShopSafebox::GetItem(uint32_t dwItemID, CShopItem ** ppItem)
	{
		for (VECITEM::iterator it = m_vecItems.begin();
			it != m_vecItems.end();
			it++)
		{
			if (dwItemID == it->GetID())
			{
				*ppItem = &(*it);
				return true;
			}
		}

		return false;
	}

	entt::entity CShopSafebox::GetOwner()
	{
		return m_pkOwner;
	}


	//if null auto find via charmanager
	bool CShopSafebox::RefreshToOwner(entt::entity character)
	{
		if(character == entt::null && m_pkOwner == entt::null)
			 return false;

		if(character == entt::null)
			character = m_pkOwner;

		TValutesInfo valute;
		valute.illYang = m_valutes.illYang;
#ifdef __ENABLE_CHEQUE_SYSTEM__
		valute.iCheque = m_valutes.iCheque;
#endif

		OFFSHOP_DEBUG("valute %lld , items count %u", valute.illYang, m_vecItems.size());

		GetManager().SendShopSafeboxRefresh(character, valute, m_vecItems);
		return true;
	}

}



#endif //__ENABLE_NEW_OFFLINESHOP__




