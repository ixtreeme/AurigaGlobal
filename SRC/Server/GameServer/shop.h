#ifndef __INC_METIN_II_GAME_SHOP_H__
#define __INC_METIN_II_GAME_SHOP_H__

#include <entt/entity/entity.hpp>

enum
{
	SHOP_MAX_DISTANCE = 1000
};

class CGrid;

/* ---------------------------------------------------------------------------------- */
class CShop
{
	public:
		typedef struct shop_item
		{
			uint32_t			vnum;

			int64_t		price;

#ifdef ENABLE_NEW_STACK_LIMIT
			int				count;
#else
			uint8_t			count;
#endif
			entt::entity		pkItem;
			int				itemid;
#ifdef ENABLE_BUY_WITH_ITEM
			TShopItemPrice	itemprice[MAX_SHOP_PRICES];
#endif
			shop_item()
			{
				vnum = 0;
				price = 0;
				count = 0;
				itemid = 0;
#ifdef ENABLE_BUY_WITH_ITEM
				memset(itemprice, 0, sizeof(itemprice));
#endif
				pkItem = entt::null;
			}
		} SHOP_ITEM;

		CShop();
		virtual ~CShop(); // @fixme139 (+virtual)

		bool	Create(uint32_t dwVnum, uint32_t dwNPCVnum, TShopItemTable * pItemTable);
		void	SetShopItems(TShopItemTable * pItemTable, uint8_t bItemCount);

		virtual void	SetPCShop(LPCHARACTER ch);
		virtual bool	IsPCShop()	{ return m_pkPC ? true : false; }

		// 게스트 추가/삭제
		virtual bool	AddGuest(entt::entity guest, uint32_t owner_vid, bool bOtherEmpire);
		void	RemoveGuest(entt::entity guest);


		virtual int64_t	Buy(LPCHARACTER ch, uint8_t pos
#ifdef ENABLE_BUY_STACK_FROM_SHOP
, bool multiple = false
#endif
);

#ifdef ENABLE_BUY_STACK_FROM_SHOP
		virtual uint8_t MultipleBuy(LPCHARACTER ch, uint8_t p, uint8_t c);
#endif
		// 게스트에게 패킷을 보냄
		void	BroadcastUpdateItem(uint8_t pos);

		// 판매중인 아이템의 갯수를 알려준다.
		int		GetNumberByVnum(uint32_t dwVnum);

		// 아이템이 상점에 등록되어 있는지 알려준다.
		virtual bool	IsSellingItem(uint32_t itemID);

		uint32_t	GetVnum() { return m_dwVnum; }
		uint32_t	GetNPCVnum() { return m_dwNPCVnum; }

	protected:
		void	Broadcast(const void * data, int bytes);

	protected:
		uint32_t				m_dwVnum;
		uint32_t				m_dwNPCVnum;

		CGrid *				m_pGrid;

		typedef std::unordered_map<entt::entity, bool> GuestMapType;
		GuestMapType m_map_guest;
		std::vector<SHOP_ITEM>		m_itemVector;	// 이 상점에서 취급하는 물건들

		LPCHARACTER			m_pkPC;
};

#endif
