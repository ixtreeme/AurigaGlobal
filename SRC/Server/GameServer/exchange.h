#ifndef __INC_METIN_II_GAME_EXCHANGE_H__
#define __INC_METIN_II_GAME_EXCHANGE_H__

#include <array>
#include <entt/entity/entity.hpp>

class CGrid;

enum EExchangeValues
{
#ifdef __NEW_EXCHANGE_WINDOW__
	EXCHANGE_ITEM_MAX_NUM = 24,
#else
	EXCHANGE_ITEM_MAX_NUM = 12,
#endif
	EXCHANGE_MAX_DISTANCE	= 1000
};

class CExchange
{
	public:
		CExchange(LPCHARACTER pOwner);
		~CExchange();

		bool		Accept(bool bIsAccept = true);
		void		Cancel();

		bool		AddGold(int64_t lGold);
		bool		AddItem(TItemPos item_pos, uint8_t display_pos);
		bool		RemoveItem(uint8_t pos);

		LPCHARACTER	GetOwner()	{ return m_pOwner;	}
		CExchange *	GetCompany()	{ return m_pCompany;	}

		bool		GetAcceptStatus() { return m_bAccept; }

		void		SetCompany(CExchange * pExchange)	{ m_pCompany = pExchange; }

	private:
		bool		Done();
		bool		Check(int * piItemCount);
		bool		CheckSpace();

	private:
		CExchange *	m_pCompany;	// 상대방의 CExchange 포인터

		LPCHARACTER	m_pOwner;

		TItemPos		m_aItemPos[EXCHANGE_ITEM_MAX_NUM];
		std::array<entt::entity, EXCHANGE_ITEM_MAX_NUM> m_items;
		uint8_t		m_abItemDisplayPos[EXCHANGE_ITEM_MAX_NUM];

		bool 		m_bAccept;

		int64_t		m_lGold;

		CGrid *		m_pGrid;

};

#endif
