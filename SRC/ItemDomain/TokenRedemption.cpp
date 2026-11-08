#include <item/TokenRedemption.hpp>
#include <algorithm>
#include <set>

namespace auriga::item
{
std::expected<TokenRedemptionPlan,TokenRedemptionError> MakeTokenRedemptionCommit(const ItemCatalog& c,
    std::array<std::uint8_t,16> operation,ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,
    std::span<const PersistentItem> inventory,std::span<const ItemWallet> wallets,ItemInstanceId id,
    std::uint32_t requested,std::uint64_t cap,TokenRedemptionActor actor)
{
    using E=TokenRedemptionError;
    if(fence.owner.kind!=ItemOwnerKind::Character || inventory.size()>MaxLoadedOwnerItems || !requested || !cap || cap>INT64_MAX || actor.characterClass>3 || actor.sex>1)return std::unexpected(E::Invalid);
    std::set<ItemInstanceId> unique;for(const auto& item:inventory)if(item.owner!=fence.owner || !unique.insert(item.id).second || !ValidatePersistentItem(c,item))return std::unexpected(E::Invalid);
    const auto source=std::ranges::find(inventory,id,&PersistentItem::id);
    if(source==inventory.end() || !PlainInventoryItem(*source,fence.owner) || requested>source->count)return std::unexpected(E::Invalid);
    const auto* entry=c.Find(source->key);const auto* token=entry ? std::get_if<ItemCurrencyTokenRow>(&entry->category) : nullptr;
    if(!token)return std::unexpected(E::Invalid);
    // Account vouchers use the shared account-wallet transaction (C33).
    // This character-only gold path must never credit a second balance.
    if(token->currency!=WalletCurrency::Gold)return std::unexpected(E::Unsupported);
    const auto* use=c.FindUsePolicy(entry->base.usePolicy);
    if(entry->base.retired || !use || !(use->allowedClasses&(1u<<actor.characterClass)) || !(use->allowedSexes&(1u<<actor.sex)) || (actor.inCombat && !use->useDuringCombat))return std::unexpected(E::Denied);
    if(actor.level<entry->base.minimumLevel)return std::unexpected(E::Level);
    const ItemWallet* wallet=nullptr;
    for(const auto& candidate:wallets)if(candidate.owner==fence.owner && candidate.currency==WalletCurrency::Gold){if(wallet)return std::unexpected(E::Invalid);wallet=&candidate;}
    if(!wallet || !wallet->revision || wallet->revision==UINT64_MAX || wallet->balance>cap)return std::unexpected(E::Invalid);
    const auto redeem=PlanTokenRedeem(c,{source->key,requested,wallet->balance,cap});
    if(!redeem)return std::unexpected(redeem.error()==TokenRedeemError::WalletFull ? E::WalletFull : E::Invalid);
    auto commit=MakeConsumeCommit(c,operation,fence,layouts,inventory,id,redeem->unitsToConsume);if(!commit)return std::unexpected(E::Invalid);
    TokenRedemptionPlan plan{std::move(*commit),{source->count,requested,redeem->unitsToConsume,token->currency,token->amountPerUnit,redeem->amountToCredit,wallet->balance,cap}};
    auto after=*wallet;after.balance+=redeem->amountToCredit;++after.revision;
    auto& out=plan.commit;out.kind=ItemOperationKind::Service;out.wallets.push_back({*wallet,after});out.outcomeCode=TokenRedemptionOutcome;
    out.result={'T','K','N',1};const auto put=[&](std::uint64_t value,unsigned n){for(unsigned i=0;i<n;++i)out.result.push_back(static_cast<std::uint8_t>(value>>(i*8)));};
    put(id.Value(),8);put(requested,4);put(plan.quote.consumed,4);put(static_cast<unsigned>(token->currency),1);put(plan.quote.unit,8);put(plan.quote.credit,8);put(cap,8);
    if(!ValidateItemCommit(c,out))return std::unexpected(E::Invalid);return plan;
}
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeTokenRedemption(const ItemCatalog& c,
    std::uint64_t session,const PersistentItem* item,std::uint64_t revision,TokenRedemptionStatus status,const TokenRedemptionQuote& q)
{
    const auto* entry=item ? c.Find(item->key) : nullptr;const auto* token=entry ? std::get_if<ItemCurrencyTokenRow>(&entry->category) : nullptr;
    const bool actionable=status==TokenRedemptionStatus::Ready || status==TokenRedemptionStatus::Redeemed;
    if(static_cast<unsigned>(status)>11 || (item ? (!session || !revision || !token || !q.available || q.available!=item->count || q.currency!=token->currency || q.unit!=token->amountPerUnit) : (session || revision || q!=TokenRedemptionQuote{} || actionable || status==TokenRedemptionStatus::Pending)))return std::unexpected(TypedInventoryWireError::BadValue);
    if(actionable && (q.currency!=WalletCurrency::Gold || !q.unit || !q.cap || q.cap>INT64_MAX || q.balance>q.cap || !q.requested || q.requested>q.available || !q.consumed || q.consumed!=std::min<std::uint64_t>(q.requested,(q.cap-q.balance)/q.unit) || q.credit!=std::uint64_t(q.consumed)*q.unit))return std::unexpected(TypedInventoryWireError::BadValue);
    std::vector<std::byte> out;const auto put=[&](std::uint64_t value,unsigned n){for(unsigned i=0;i<n;++i)out.push_back(std::byte((value>>(i*8))&255));};
    put(234,1);put(0,4);put(1,1);put(static_cast<unsigned>(status),1);put(session,8);put(item ? item->id.Value() : 0,8);put(revision,8);
    put(item ? static_cast<unsigned>(item->key.Category()) : 0,2);put(item ? item->key.LocalId() : 0,4);put(item ? item->refinementLevel : 0,2);
    put(static_cast<unsigned>(q.currency),1);put(q.available,4);put(q.requested,4);put(q.consumed,4);put(q.unit,8);put(q.credit,8);put(q.balance,8);put(q.cap,8);
    const auto details=item ? EncodeTypedItemDetails(c,{},false,std::array{ItemGrant{item->key,item->refinementLevel,1}}) : std::expected<std::vector<std::byte>,TypedInventoryWireError>(std::vector<std::byte>{});
    if(!details)return std::unexpected(details.error());put(details->size(),4);out.insert(out.end(),details->begin(),details->end());
    if(out.size()>(1u<<20))return std::unexpected(TypedInventoryWireError::TooLarge);for(unsigned i=0;i<4;++i)out[1+i]=std::byte((out.size()>>(i*8))&255);return out;
}
}
