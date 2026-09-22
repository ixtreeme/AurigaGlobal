#ifndef __INC_METIN_II_GAME_SHOP_H__
#define __INC_METIN_II_GAME_SHOP_H__

// Shops are ECS state now: ecs::ShopData on a registry-owned shop entity. The
// NPC shop table and the personal shop of a player differ only by their owner
// and their item source; the extended (tabbed) shops carry their tabs in the
// same component. There is no heap CShop object and no raw shop pointer on the
// character; ShopState holds generation-checked entity handles.

#include <entt/entt.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <Base/grid.h>
#include <common/tables.h>

#include "../ecs/Registry.hpp"

enum
{
	SHOP_MAX_DISTANCE = 1000
};

struct SShopTable;
typedef struct SShopTableEx : SShopTable
{
	std::string name;
	EShopCoinType coinType;
} TShopTableEx;

namespace ecs
{
	// One row of the shop window. For an NPC shop the item is a prototype
	// description (vnum/count); for a personal shop it points at the player's
	// own item entity.
	struct ShopItem
	{
		uint32_t		vnum { 0 };
		int64_t			price { 0 };
#ifdef ENABLE_NEW_STACK_LIMIT
		int				count { 0 };
#else
		uint8_t			count { 0 };
#endif
		entt::entity	item { entt::null };
		int				itemid { 0 };
#ifdef ENABLE_BUY_WITH_ITEM
		TShopItemPrice	itemprice[MAX_SHOP_PRICES] {};
#endif
	};

	// Authoritative state of one shop: an NPC shop from the table, an extended
	// (tabbed) NPC shop, or a player's personal shop. The durable identity of
	// the static shops is the shop vnum (and the NPC vnum); the personal shop
	// is indexed by the owner's packet VID in CShopManager.
	struct ShopData
	{
		ShopData();
		~ShopData();

		uint32_t		vnum { 0 };
		uint32_t		npcVnum { 0 };

		std::unique_ptr<CGrid>	grid;
		std::unordered_map<entt::entity, bool> guests;
		std::vector<ShopItem>	items;

		// The player whose personal shop this is, null for an NPC shop.
		entt::entity	owner { entt::null };

		// Extended (tabbed) NPC shop state.
		bool			extended { false };
		std::vector<TShopTableEx> tabs;
	};
}

// Native API over ecs::ShopData. Every entry point validates the shop entity
// before use, so a retired or recycled handle is a no-op. The shop is always
// the first parameter.
namespace ShopSystem
{
	// Defined here so the free functions below resolve the state inline.
	inline ecs::ShopData* Find(entt::entity shop)
	{
		if (shop == entt::null || !g_registry.valid(shop))
			return nullptr;

		return g_registry.try_get<ecs::ShopData>(shop);
	}

	bool IsValid(entt::entity shop);

	// Lifecycle. Destroy tells every guest the window closed and releases the
	// guest relation before the entity goes.
	void Initialize(entt::entity shop);
	void Destroy(entt::entity shop);

	bool Create(entt::entity shop, uint32_t dwVnum, uint32_t dwNPCVnum, TShopItemTable* pItemTable);
	void SetShopItems(entt::entity shop, TShopItemTable* pItemTable, uint8_t bItemCount);
	void SetOwner(entt::entity shop, entt::entity owner);
	bool IsPCShop(entt::entity shop);
	uint32_t GetVnum(entt::entity shop);
	uint32_t GetNPCVnum(entt::entity shop);

	bool AddShopTable(entt::entity shop, TShopTableEx& shopTable);
	size_t GetTabCount(entt::entity shop);

	// Guests (the players currently browsing this shop).
	bool AddGuest(entt::entity shop, entt::entity guest, uint32_t owner_vid, bool bOtherEmpire);
	void RemoveGuest(entt::entity shop, entt::entity guest);

	int64_t Buy(entt::entity shop, entt::entity ch, uint8_t pos
#ifdef ENABLE_BUY_STACK_FROM_SHOP
		, bool multiple = false
#endif
	);
#ifdef ENABLE_BUY_STACK_FROM_SHOP
	uint8_t MultipleBuy(entt::entity shop, entt::entity ch, uint8_t p, uint8_t c);
#endif

	void BroadcastUpdateItem(entt::entity shop, uint8_t pos);
	int GetNumberByVnum(entt::entity shop, uint32_t dwVnum);
	bool IsSellingItem(entt::entity shop, uint32_t itemID);
}

#endif
