#include <item/PersistentItems.hpp>
#include <algorithm>

namespace auriga::item {
namespace {
using E=ItemPersistenceError;
std::expected<ItemCommit,E> Build(const ItemCatalog& c,std::array<std::uint8_t,16> operation,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,ItemInstanceId id,
    std::uint64_t now,std::uint64_t elapsed,bool following) {
    const auto it=std::ranges::find(items,id,&PersistentItem::id);
    if(it==items.end() || it->owner!=owner.owner || it->destroyed || !it->petSupply || !it->placement ||
        it->placement->kind!=ItemStorageKind::Inventory || !now || now>INT64_MAX || elapsed>INT64_MAX ||
        (following && !it->petSupply->equippedSeal) || (elapsed && !it->petSupply->equippedSeal))return std::unexpected(E::InvalidItem);
    const auto* row=std::get_if<ItemPetSupplyRow>(&c.Find(it->key)->category);const auto* p=row ? c.FindPetSupplyProfile(row->profile) : nullptr;
    if(!p || !p->skin)return std::unexpected(E::InvalidItem);
    // Elapsed is the settled monotonic budget. It may remain owed after the
    // follower stopped, so wasEquipped derives from the persisted binding.
    const auto time=AdvanceItemClock({p->skin->clock,p->skin->durationMs},it->petSupply->lifetime,{now,elapsed,it->petSupply->equippedSeal!=0,false});
    if(!time || time->after==it->petSupply->lifetime)return std::unexpected(E::RequirementNotMet);
    auto next=*it;++next.revision;next.petSupply->lifetime=time->after;
    if(time->removeItem){next.petSupply->equippedSeal=0;next.count=0;next.destroyed=true;next.placement.reset();}
    const auto layout=std::ranges::find(layouts,ItemStorage{owner.owner,it->placement->kind,it->placement->storageId},&ItemStorageLayout::storage);
    if(layout==layouts.end())return std::unexpected(E::InvalidStorage);
    ItemCommit commit;commit.operationId=operation;commit.catalogRevision=c.Revision();commit.catalogHash=c.ContentHash();
    commit.kind=ItemOperationKind::Service;commit.outcomeCode=0x2609;commit.owners={owner};commit.storages={*layout};commit.items={{*it,next}};
    commit.result={'P','S','K',1};const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)commit.result.push_back(static_cast<std::uint8_t>(v>>(8*i)));};
    put(now,8);put(elapsed,8);put(following,1);return commit;
}
}
std::expected<ItemCommit,ItemPersistenceError> MakePetSkinClockCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operation,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,ItemInstanceId id,std::uint64_t now,std::uint64_t elapsed,bool following) {
    auto commit=Build(c,operation,owner,layouts,items,id,now,elapsed,following);
    if(commit)if(const auto valid=ValidateItemCommit(c,*commit);!valid)return std::unexpected(valid.error());return commit;
}
bool ValidatePetSkinClockCommit(const ItemCatalog& c,const ItemCommit& commit) {
    const auto& b=commit.result;
    if(commit.kind!=ItemOperationKind::Service || commit.outcomeCode!=0x2609 || commit.items.size()!=1 || !commit.items[0].before ||
        commit.owners.size()!=1 || commit.storages.size()!=1 || !commit.wallets.empty() || commit.ground || commit.recovery || commit.learning || commit.petStudyCooldown ||
        b.size()!=21 || b[0]!='P' || b[1]!='S' || b[2]!='K' || b[3]!=1 || b[20]>1)return false;
    const auto get=[&](unsigned at){std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(b[at+i])<<(8*i);return v;};
    const auto expected=Build(c,commit.operationId,commit.owners[0],commit.storages,std::span(&*commit.items[0].before,1),commit.items[0].after.id,get(4),get(12),b[20]!=0);
    return expected && expected->items[0].after==commit.items[0].after && expected->result==commit.result;
}
}
