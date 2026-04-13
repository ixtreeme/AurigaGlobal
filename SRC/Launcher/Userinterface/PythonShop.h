#pragma once

#include "Packet.h"

/*
 *	상점 처리
 *
 *	2003-01-16 anoa	일차 완료
 *	2003-12-26 levites 수정
 *
 *	2012-10-29 rtsummit 새로운 화폐 출현 및 tab 기능 추가로 인한 shop 확장.
 *
 */
typedef enum
{
	SHOP_COIN_TYPE_GOLD, // DEFAULT VALUE
	SHOP_COIN_TYPE_SECONDARY_COIN,
} EShopCoinType;

class CPythonShop : public CSingleton<CPythonShop>
{
	public:
		CPythonShop();
		virtual ~CPythonShop();

		void Clear();

		void SetItemData(uint32_t dwIndex, const TShopItemData & c_rShopItemData);
		bool GetItemData(uint32_t dwIndex, const TShopItemData ** c_ppItemData);

		void SetItemData(uint8_t tabIdx, uint32_t dwSlotPos, const TShopItemData & c_rShopItemData);
		bool GetItemData(uint8_t tabIdx, uint32_t dwSlotPos, const TShopItemData ** c_ppItemData);

		void SetTabCount(uint8_t bTabCount) { m_bTabCount = bTabCount; }
		uint8_t GetTabCount() { return m_bTabCount; }

		void SetTabCoinType(uint8_t tabIdx, uint8_t coinType);
		uint8_t GetTabCoinType(uint8_t tabIdx);

		void SetTabName(uint8_t tabIdx, const char* name);
		const char* GetTabName(uint8_t tabIdx);


		//BOOL GetSlotItemID(uint32_t dwSlotPos, uint32_t* pdwItemID);

		void Open(bool isPrivateShop, bool isMainPrivateShop);
		void Close();
		bool IsOpen();
		bool IsPrivateShop();
		bool IsMainPlayerPrivateShop();

		void ClearPrivateShopStock();

		void AddPrivateShopItemStock(TItemPos ItemPos, uint8_t byDisplayPos, int64_t dwPrice);
		int64_t GetPrivateShopItemPrice(TItemPos ItemPos);

		void DelPrivateShopItemStock(TItemPos ItemPos);
		void BuildPrivateShop(const char * c_szName
#ifdef KASMIR_PAKET_SYSTEM
		, uint32_t dwKasmirNpc, uint8_t bKasmirBaslik
#endif
		);

	protected:
		bool	CheckSlotIndex(uint32_t dwIndex);

	protected:
		bool				m_isShoping;
		bool				m_isPrivateShop;
		bool				m_isMainPlayerPrivateShop;

		struct ShopTab
		{
			ShopTab() : items()
			{
				coinType = SHOP_COIN_TYPE_GOLD;
			}
			uint8_t				coinType;
			std::string			name;
			TShopItemData		items[SHOP_HOST_ITEM_MAX_NUM];
		};

		uint8_t m_bTabCount;
		ShopTab m_aShoptabs[SHOP_TAB_COUNT_MAX];

		typedef std::map<TItemPos, TShopItemTable> TPrivateShopItemStock;
		TPrivateShopItemStock	m_PrivateShopItemStock;
};
