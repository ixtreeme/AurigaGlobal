#ifndef __INC_METIN_II_GAME_SHOP_MANAGER_H__
#define __INC_METIN_II_GAME_SHOP_MANAGER_H__

#include <entt/entity/entity.hpp>

class CShop;
typedef class CShop * LPSHOP;

class CShopManager : public singleton<CShopManager>
{
public:
	typedef std::map<uint32_t, CShop *> TShopMap;

public:
	CShopManager();
	virtual ~CShopManager();

	bool	Initialize(TShopTable * table, int size);
	void	Destroy();

	LPSHOP	Get(uint32_t dwVnum);
	LPSHOP	GetByNPCVnum(uint32_t dwVnum);

	bool	StartShopping(entt::entity pkChr, entt::entity pkShopKeeper, int iShopVnum = 0);
	void	StopShopping(entt::entity ch);

	void	Buy(entt::entity ch, uint8_t pos);
#ifdef ENABLE_BUY_STACK_FROM_SHOP
	void MultipleBuy(entt::entity ch, uint8_t p, uint8_t c);
#endif
#ifdef ENABLE_EXTRA_INVENTORY
	void	Sell(entt::entity ch, TItemPos Cell,
#ifdef ENABLE_NEW_STACK_LIMIT
	uint16_t bCount = 0
#else
	uint8_t bCount = 0
#endif
	);
#else
	void	Sell(entt::entity ch, uint8_t bCell,
#ifdef ENABLE_NEW_STACK_LIMIT
	uint16_t bCount = 0
#else
	uint8_t bCount = 0
#endif
	);
#endif


	LPSHOP	CreatePCShop(entt::entity ch, TShopItemTable * pTable, uint8_t bItemCount);
	LPSHOP	FindPCShop(uint32_t dwVID);
	void	DestroyPCShop(entt::entity ch);

private:
	TShopMap	m_map_pkShop;
	TShopMap	m_map_pkShopByNPCVnum;
	TShopMap	m_map_pkShopByPC;

	bool	ReadShopTableEx(const char* stFileName);
};

#endif
