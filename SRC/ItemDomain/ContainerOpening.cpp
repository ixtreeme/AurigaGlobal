#include <item/ContainerOpening.hpp>
#include <algorithm>
#include <set>

namespace auriga::item
{
namespace
{
using E=ContainerOpeningError;
const PersistentItem* Find(std::span<const PersistentItem> items,ItemInstanceId id)
{const auto found=std::ranges::find(items,id,&PersistentItem::id);return found==items.end() ? nullptr : &*found;}
std::expected<void,E> Allowed(const ItemCatalog& c,const PersistentItem& item,ItemOwner owner,ContainerActor actor)
{
    if(!ContainerItemUsable(item,owner) || !ValidatePersistentItem(c,item) || actor.characterClass>3 || actor.sex>1)return std::unexpected(E::Invalid);
    const auto& b=c.Find(item.key)->base;const auto* use=c.FindUsePolicy(b.usePolicy);
    if(b.retired || !use || !(use->allowedClasses&(1u<<actor.characterClass)) || !(use->allowedSexes&(1u<<actor.sex)) || (actor.inCombat && !use->useDuringCombat))return std::unexpected(E::Denied);
    if(actor.level<b.minimumLevel)return std::unexpected(E::Level);return {};
}
std::expected<ContainerOpeningPlan,E> Build(const ItemCatalog& c,std::array<std::uint8_t,16> operation,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> inventory,std::span<const ItemWallet> wallets,
    ContainerOpeningRequest request,std::span<const ItemInstanceId> ids,std::span<const LootEntry> rewards)
{
    std::vector<PersistentItem> working(inventory.begin(),inventory.end());
    for(auto id:{std::optional(request.box),request.key})if(id)
    {auto& item=*std::ranges::find(working,*id,&PersistentItem::id);if(!--item.count){item.destroyed=true;item.placement.reset();}}
    ContainerOpeningPlan plan;auto& out=plan.commit;std::size_t usedIds=0;
    for(const auto& reward:rewards)
    {
        plan.won.push_back(reward.entry);
        if(reward.kind==LootRewardKind::Gold)
        {if(reward.amount>std::uint64_t(INT64_MAX)-plan.gold)return std::unexpected(E::WalletLimit);plan.gold+=reward.amount;continue;}
        const auto grant=MakeGrantCommit(c,operation,fence,layouts,working,{*reward.item,0,static_cast<std::uint32_t>(reward.amount),{},{},{},request.nowUtcMs},ids.subspan(usedIds));
        if(!grant)return std::unexpected(grant.error()==ItemPersistenceError::InvalidPlacement ? E::NoRoom : grant.error()==ItemPersistenceError::LimitExceeded ? E::Unavailable : E::Invalid);
        for(const auto& change:grant->items)
        {if(change.before)*std::ranges::find(working,change.after.id,&PersistentItem::id)=change.after;else{working.push_back(change.after);++usedIds;}}
    }
    out.operationId=operation;out.catalogRevision=c.Revision();out.catalogHash=c.ContentHash();out.kind=ItemOperationKind::Service;out.owners={fence};out.outcomeCode=ContainerOpeningOutcome;
    out.result={'B','O','X',1};const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)out.result.push_back(static_cast<std::uint8_t>(v>>(i*8)));};
    put(request.box.Value(),8);put(request.key ? request.key->Value() : 0,8);put(plan.won.size(),2);for(auto entry:plan.won)put(entry,2);put(plan.gold,8);
    for(std::size_t i=0;i<inventory.size();++i)if(working[i]!=inventory[i]){working[i].revision=inventory[i].revision+1;out.items.push_back({inventory[i],working[i]});}
    for(std::size_t i=inventory.size();i<working.size();++i){working[i].revision=1;out.items.push_back({{},working[i]});}
    if(plan.gold)
    {
        const ItemWallet* gold=nullptr;for(const auto& wallet:wallets)if(wallet.owner==fence.owner && wallet.currency==WalletCurrency::Gold){if(gold)return std::unexpected(E::Invalid);gold=&wallet;}
        if(!gold || !gold->revision || gold->revision==UINT64_MAX || gold->balance>INT64_MAX)return std::unexpected(E::Invalid);
        if(plan.gold>std::uint64_t(INT64_MAX)-gold->balance)return std::unexpected(E::WalletLimit);
        auto after=*gold;after.balance+=plan.gold;++after.revision;out.wallets.push_back({*gold,after});
    }
    if(out.items.size()>MaxItemCommitItems)return std::unexpected(E::Unsupported);
    for(const auto& change:out.items)for(const auto* item:{change.before ? &*change.before : nullptr,&change.after})if(item && item->placement)
    {const ItemStorage storage{item->owner,item->placement->kind,item->placement->storageId};if(std::ranges::find(out.storages,storage,&ItemStorageLayout::storage)!=out.storages.end())continue;
     const auto layout=std::ranges::find(layouts,storage,&ItemStorageLayout::storage);if(layout==layouts.end())return std::unexpected(E::Invalid);out.storages.push_back(*layout);}
    if(!ValidateItemCommit(c,out))return std::unexpected(E::Invalid);return plan;
}
}
bool ContainerItemUsable(const PersistentItem& item,ItemOwner owner)
{return PlainInventoryItem(item,owner);}
std::expected<void,E> ValidateContainerContents(const ItemCatalog& c,ItemKey key)
{
    const auto* entry=c.Find(key);const auto* box=entry ? std::get_if<ItemContainerRow>(&entry->category) : nullptr;
    if(!box)return std::unexpected(E::Invalid);
    if((box->kind!=ContainerKind::Use && box->kind!=ContainerKind::Locked) || !box->lootProfile)return std::unexpected(E::Unsupported);
    const auto* profile=c.FindLoot(*box->lootProfile);if(!profile)return std::unexpected(E::Invalid);
    std::size_t uncertain=0,items=0;
    for(const auto& reward:profile->Data().entries)
    {
        if(reward.kind==LootRewardKind::Gold)continue;
        if(reward.kind!=LootRewardKind::Item || reward.rareAttributeChanceBp || !reward.item)return std::unexpected(E::Unsupported);
        const auto* target=c.Find(*reward.item);if(!target || target->base.retired || !SupportsPersistentGrant(c,*target) || !c.RefinedBaseStats(*reward.item,0))return std::unexpected(E::Unsupported);
        ++items;if(reward.weight<10000)++uncertain;
    }
    if(profile->Data().mode==LootPickMode::IndependentChance && (uncertain>MaxUncertainContainerItems || items+2>MaxItemCommitItems))return std::unexpected(E::Unsupported);
    return {};
}
std::expected<ContainerOpenRequest,E> CheckContainerOpening(const ItemCatalog& c,ContainerOpeningRequest request,ItemOwner owner,std::span<const PersistentItem> inventory)
{
    const auto* box=Find(inventory,request.box);if(!box)return std::unexpected(E::Invalid);
    if(const auto allowed=Allowed(c,*box,owner,request.actor);!allowed)return std::unexpected(allowed.error());
    if(const auto supported=ValidateContainerContents(c,box->key);!supported)return std::unexpected(supported.error());
    const auto& row=std::get<ItemContainerRow>(c.Find(box->key)->category);
    const auto* key=request.key ? Find(inventory,*request.key) : nullptr;
    if(row.kind==ContainerKind::Locked && !request.key)return std::unexpected(E::KeyRequired);
    if(row.kind==ContainerKind::Use && request.key)return std::unexpected(E::WrongKey);
    if(request.key)
    {
        if(!key || *request.key==request.box)return std::unexpected(E::WrongKey);
        if(const auto allowed=Allowed(c,*key,owner,request.actor);!allowed)return std::unexpected(allowed.error());
        const auto* keyRow=std::get_if<ItemContainerKeyRow>(&c.Find(key->key)->category);if(!keyRow || keyRow->lockId!=row.lockId)return std::unexpected(E::WrongKey);
    }
    return ContainerOpenRequest{box->key,key ? std::optional(key->key) : std::nullopt,box->count,key ? key->count : 0};
}
std::expected<ContainerOpeningPlan,E> MakeContainerOpeningCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operation,
    ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> inventory,
    std::span<const ItemWallet> wallets,ContainerOpeningRequest request,std::span<const ItemInstanceId> ids,const LootDraw& draw)
{
    if(fence.owner.kind!=ItemOwnerKind::Character || inventory.size()>MaxLoadedOwnerItems)return std::unexpected(E::Invalid);
    std::set<ItemInstanceId> unique;for(const auto& item:inventory)if(item.owner!=fence.owner || !unique.insert(item.id).second || !ValidatePersistentItem(c,item))return std::unexpected(E::Invalid);
    const auto checked=CheckContainerOpening(c,request,fence.owner,inventory);if(!checked)return std::unexpected(checked.error());
    const auto& row=std::get<ItemContainerRow>(c.Find(checked->container)->category);const auto& profile=c.FindLoot(*row.lootProfile)->Data();
    // Admit every possible item result BEFORE the caller's RNG is touched.
    // Checking only the sampled result permits inventory-based reward rerolls.
    if(profile.mode==LootPickMode::WeightedOne)
    {for(const auto& reward:profile.entries){const auto candidate=Build(c,operation,fence,layouts,inventory,wallets,request,ids,{&reward,1});if(!candidate)return std::unexpected(candidate.error());}}
    else
    {
        std::size_t uncertain=0;for(const auto& reward:profile.entries)if(reward.kind==LootRewardKind::Item && reward.weight<10000)++uncertain;
        for(std::size_t mask=0;mask<(std::size_t{1}<<uncertain);++mask)
        {
            std::vector<LootEntry> possible;std::size_t bit=0;
            for(const auto& reward:profile.entries)if(reward.kind==LootRewardKind::Gold || reward.weight==10000 || (mask&(std::size_t{1}<<bit++)))possible.push_back(reward);
            const auto candidate=Build(c,operation,fence,layouts,inventory,wallets,request,ids,possible);if(!candidate)return std::unexpected(candidate.error());
        }
    }
    const auto picked=PlanContainerOpen(c,*checked,draw);if(!picked)return std::unexpected(E::Invalid);
    return Build(c,operation,fence,layouts,inventory,wallets,request,ids,picked->rewards);
}
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeContainerOpening(const ItemCatalog& c,
    std::uint64_t session,const PersistentItem* box,const PersistentItem* key,std::uint64_t revision,ContainerOpeningStatus status,
    std::span<const std::uint64_t> keys,std::span<const std::uint16_t> won)
{
    const auto* entry=box ? c.Find(box->key) : nullptr;const auto* row=entry ? std::get_if<ItemContainerRow>(&entry->category) : nullptr;
    const auto* profile=row && row->lootProfile ? c.FindLoot(*row->lootProfile) : nullptr;
    if(static_cast<unsigned>(status)>15 || (box ? (!session || !revision || !row) : (session || key || revision || !keys.empty() || !won.empty() || status==ContainerOpeningStatus::Ready || status==ContainerOpeningStatus::Opened || status==ContainerOpeningStatus::Pending)) || keys.size()>MaxLoadedOwnerItems || won.size()>MaxLootEntries)return std::unexpected(TypedInventoryWireError::BadValue);
    std::vector<std::byte> out;const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)out.push_back(std::byte((v>>(i*8))&255));};
    const auto putKey=[&](const PersistentItem* item){put(item ? static_cast<unsigned>(item->key.Category()) : 0,2);put(item ? item->key.LocalId() : 0,4);put(item ? item->refinementLevel : 0,2);};
    put(233,1);put(0,4);put(1,1);put(static_cast<unsigned>(status),1);put(session,8);put(box ? box->id.Value() : 0,8);put(key ? key->id.Value() : 0,8);put(revision,8);putKey(box);putKey(key);
    put(row ? static_cast<unsigned>(row->kind) : 0,1);put(row ? row->lockId : 0,2);put(profile ? static_cast<unsigned>(profile->Data().mode) : 0,1);
    put(keys.size(),2);for(auto id:keys)put(id,8);put(profile ? profile->Data().entries.size() : 0,2);
    std::vector<ItemGrant> extra;if(box)extra.push_back({box->key,box->refinementLevel,1});if(key)extra.push_back({key->key,key->refinementLevel,1});
    if(profile)for(const auto& reward:profile->Data().entries)
    {put(reward.entry,2);put(static_cast<unsigned>(reward.kind),1);put(reward.item ? static_cast<unsigned>(reward.item->Category()) : 0,2);put(reward.item ? reward.item->LocalId() : 0,4);put(reward.amount,8);put(reward.weight,4);put(reward.rareAttributeChanceBp,2);if(reward.item)extra.push_back({*reward.item,0,1});}
    put(won.size(),2);for(auto entry:won){if(!profile || !entry || entry>profile->Data().entries.size())return std::unexpected(TypedInventoryWireError::BadValue);put(entry,2);}
    const auto details=extra.empty() ? std::expected<std::vector<std::byte>,TypedInventoryWireError>(std::vector<std::byte>{}) : EncodeTypedItemDetails(c,{},false,extra);
    if(!details)return std::unexpected(details.error());put(details->size(),4);out.insert(out.end(),details->begin(),details->end());
    if(out.size()>(1u<<20))return std::unexpected(TypedInventoryWireError::TooLarge);for(unsigned i=0;i<4;++i)out[1+i]=std::byte((out.size()>>(i*8))&255);return out;
}
}
