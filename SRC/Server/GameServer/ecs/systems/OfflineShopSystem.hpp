#pragma once

#include <entt/entt.hpp>

namespace offlineshop {
class CShop;
class CAuction;
class CShopSafebox;
}

// The offline shop, its safebox and the auction. CHARACTER held seven fields
// for these; three of them were mirrors of ecs::ShopState, written on both
// sides by the same setters, and the other four had no component at all.
#ifdef __ENABLE_NEW_OFFLINESHOP__
namespace ecs::OfflineShopSystem {

offlineshop::CShop* GetOfflineShop(entt::entity e);
void SetOfflineShop(entt::entity e, offlineshop::CShop* shop);

offlineshop::CShop* GetOfflineShopGuest(entt::entity e);
void SetOfflineShopGuest(entt::entity e, offlineshop::CShop* shop);

offlineshop::CShopSafebox* GetShopSafebox(entt::entity e);
void SetShopSafebox(entt::entity e, offlineshop::CShopSafebox* safebox);

offlineshop::CAuction* GetAuction(entt::entity e);
void SetAuction(entt::entity e, offlineshop::CAuction* auction);

offlineshop::CAuction* GetAuctionGuest(entt::entity e);
void SetAuctionGuest(entt::entity e, offlineshop::CAuction* auction);

int GetUseTime(entt::entity e);
void SetUseTime(entt::entity e);

bool IsLookingOfferList(entt::entity e);
void SetLookingOfferList(entt::entity e, bool value);

} // namespace ecs::OfflineShopSystem
#endif
