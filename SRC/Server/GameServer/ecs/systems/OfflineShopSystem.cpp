#include "../../stdafx.h"

#include "OfflineShopSystem.hpp"
#include "../Registry.hpp"
#include "../CharacterAccessors.hpp"
#include "../components/social_components.hpp"
#include "../components/dirty_components.hpp"

#include "../../char.h"
#include "../../new_offlineshop.h"

#ifdef __ENABLE_NEW_OFFLINESHOP__
namespace ecs::OfflineShopSystem {

namespace {

ecs::ShopState* StateOf(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;
    return &g_registry.get_or_emplace<ecs::ShopState>(e);
}

const ecs::ShopState* ReadStateOf(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;
    return g_registry.try_get<ecs::ShopState>(e);
}

void Touch(entt::entity e)
{
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

} // namespace

offlineshop::CShop* GetOfflineShop(entt::entity e)
{
    const auto* state = ReadStateOf(e);
    return state ? state->offlineShop : nullptr;
}

void SetOfflineShop(entt::entity e, offlineshop::CShop* shop)
{
    auto* state = StateOf(e);
    if (!state)
        return;

    state->offlineShop = shop;
    Touch(e);
}

offlineshop::CShop* GetOfflineShopGuest(entt::entity e)
{
    const auto* state = ReadStateOf(e);
    return state ? state->offlineShopGuest : nullptr;
}

void SetOfflineShopGuest(entt::entity e, offlineshop::CShop* shop)
{
    auto* state = StateOf(e);
    if (!state)
        return;

    state->offlineShopGuest = shop;
    Touch(e);
}

offlineshop::CShopSafebox* GetShopSafebox(entt::entity e)
{
    const auto* state = ReadStateOf(e);
    return state ? state->shopSafebox : nullptr;
}

// The safebox and its owner point at each other, so handing one over clears
// the old back pointer first.
void SetShopSafebox(entt::entity e, offlineshop::CShopSafebox* safebox)
{
    auto* state = StateOf(e);
    if (!state)
        return;

    // CShopSafebox::SetOwner takes the character; that is its own migration.
    LPCHARACTER self = ecs::LegacyCharOf(e);

    if (state->shopSafebox && safebox == nullptr)
        state->shopSafebox->SetOwner(nullptr);

    else if (state->shopSafebox == nullptr && safebox)
        safebox->SetOwner(self);

    state->shopSafebox = safebox;
    Touch(e);
}

offlineshop::CAuction* GetAuction(entt::entity e)
{
    const auto* state = ReadStateOf(e);
    return state ? state->auction : nullptr;
}

void SetAuction(entt::entity e, offlineshop::CAuction* auction)
{
    auto* state = StateOf(e);
    if (!state)
        return;

    state->auction = auction;
    Touch(e);
}

offlineshop::CAuction* GetAuctionGuest(entt::entity e)
{
    const auto* state = ReadStateOf(e);
    return state ? state->auctionGuest : nullptr;
}

void SetAuctionGuest(entt::entity e, offlineshop::CAuction* auction)
{
    auto* state = StateOf(e);
    if (!state)
        return;

    state->auctionGuest = auction;
    Touch(e);
}

// When the character last touched the offline shop, which the warp check reads.
int GetUseTime(entt::entity e)
{
    const auto* state = ReadStateOf(e);
    return state ? state->offlineShopUseTime : 0;
}

void SetUseTime(entt::entity e)
{
    auto* state = StateOf(e);
    if (!state)
        return;

    state->offlineShopUseTime = thecore_pulse();
    Touch(e);
}

bool IsLookingOfferList(entt::entity e)
{
    const auto* state = ReadStateOf(e);
    return state && state->lookingOfferList;
}

void SetLookingOfferList(entt::entity e, bool value)
{
    auto* state = StateOf(e);
    if (!state)
        return;

    state->lookingOfferList = value;
    Touch(e);
}

} // namespace ecs::OfflineShopSystem
#endif
