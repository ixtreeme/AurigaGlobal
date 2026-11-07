#include <item/PersistentItems.hpp>
#include <algorithm>

namespace auriga::item {
namespace {
using Error=PetStudyCommitError;
const PersistentItem* Find(std::span<const PersistentItem> items,ItemInstanceId id) {
    const auto it=std::ranges::find(items,id,&PersistentItem::id);
    return it==items.end() ? nullptr : &*it;
}
std::expected<PetStudyWrite,Error> Build(const ItemCatalog& c,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,const PetCareContext& context,
    ItemInstanceId supplyId,ItemInstanceId sealId,const PetServiceTerms& terms) {
    const auto* supply=Find(items,supplyId);const auto* seal=Find(items,sealId);
    const auto* entry=supply ? c.Find(supply->key) : nullptr;
    const auto* row=entry ? std::get_if<ItemPetSupplyRow>(&entry->category) : nullptr;
    const auto* profile=row ? c.FindPetSupplyProfile(row->profile) : nullptr;
    const auto kind=terms.selector==0 ? PetSupplyKind::ExtendLife : terms.selector==1 ? PetSupplyKind::PermanentLife : PetSupplyKind::Enchant;
    if(!supply || !seal || !seal->growthPet || !profile || terms.selector>4 || profile->kind!=kind ||
        owner.owner!=ItemOwner{ItemOwnerKind::Character,context.actorId} || owner.sessionGeneration!=context.sessionGeneration ||
        !PlainInventoryItem(*supply,owner.owner) || supply->refinementLevel || seal->owner!=owner.owner || seal->destroyed ||
        !seal->placement || seal->placement->kind!=ItemStorageKind::Inventory || (seal->boundTo && seal->boundTo!=owner.owner) ||
        (terms.selector<2 && terms.rollBp))return std::unexpected(Error{PetLifecycleError::InvalidSnapshot});
    const bool active=RecallRequested(*seal);
    if(active || terms.elapsedMs)return std::unexpected(Error{PetLifecycleError::WrongTargetState});
    const PetCareItemSnapshot target{seal->key,sealId.Value(),seal->revision,seal->owner.id,seal->count,
        seal->refinementLevel,false,false,false,true,false,0};
    const PetCareItemSnapshot source{supply->key,supplyId.Value(),supply->revision,supply->owner.id,supply->count,supply->refinementLevel};
    const PetLifecycleRequest pet{context,target,seal->revision,terms.nowUtcMs,*seal->growthPet};
    const auto plan=PlanPetSupply(c,{pet,source,{},static_cast<std::uint8_t>(terms.selector>=2 ? terms.selector-2 : 0),terms.rollBp});
    if(!plan)return std::unexpected(Error{plan.error()});
    auto commit=MakeConsumeCommit(c,context.operationId,owner,layouts,items,supplyId,plan->consumeUnits);
    if(!commit)return std::unexpected(Error{commit.error()});
    const auto layout=std::ranges::find(layouts,ItemStorage{owner.owner,seal->placement->kind,seal->placement->storageId},&ItemStorageLayout::storage);
    if(layout==layouts.end())return std::unexpected(Error{ItemPersistenceError::InvalidStorage});
    if(std::ranges::find(commit->storages,*layout)==commit->storages.end())commit->storages.push_back(*layout);
    auto after=*seal;++after.revision;after.growthPet=plan->after;commit->items.push_back({*seal,std::move(after)});
    commit->kind=ItemOperationKind::Service;commit->outcomeCode=0x2608;
    commit->result={'P','S','U',1,terms.selector};
    const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)commit->result.push_back(static_cast<std::uint8_t>(v>>(i*8)));};
    put(terms.nowUtcMs,8);put(terms.elapsedMs,8);put(terms.rollBp,2);put(context.characterLevel,2);
    put(context.characterClass,1);put(context.sex,1);put(context.inCombat,1);
    return PetStudyWrite{*plan,std::move(*commit)};
}
}
std::expected<PetStudyWrite,Error> MakePetServiceCommit(const ItemCatalog& c,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,const PetCareContext& context,
    ItemInstanceId supply,ItemInstanceId seal,const PetServiceTerms& terms) {
    auto write=Build(c,owner,layouts,items,context,supply,seal,terms);
    if(write)if(const auto valid=ValidateItemCommit(c,write->commit);!valid)return std::unexpected(Error{valid.error()});
    return write;
}
bool ValidatePetServiceCommit(const ItemCatalog& c,const ItemCommit& commit) {
    const auto& b=commit.result;
    if(commit.outcomeCode!=0x2608 || commit.kind!=ItemOperationKind::Service || commit.owners.size()!=1 ||
        commit.items.size()!=2 || !commit.wallets.empty() || commit.ground || commit.recovery || commit.learning || commit.petStudyCooldown ||
        b.size()!=28 || b[0]!='P' || b[1]!='S' || b[2]!='U' || b[3]!=1 || b[4]>4 || b[27]>1)return false;
    std::vector<PersistentItem> before;
    for(const auto& change:commit.items){if(!change.before)return false;before.push_back(*change.before);}
    const auto seal=std::ranges::find_if(before,[](const auto& p){return p.growthPet.has_value();});
    if(seal==before.end())return false;
    const auto source=std::ranges::find_if(before,[](const auto& p){return !p.growthPet;});
    if(source==before.end())return false;
    const auto read=[&](unsigned at,unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(b[at+i])<<(8*i);return v;};
    const auto& owner=commit.owners.front();
    const PetCareContext context{commit.operationId,owner.owner.id,owner.sessionGeneration,1,
        static_cast<std::uint16_t>(read(23,2)),b[25],b[26],true,true,true,false,false,b[27]!=0};
    const auto expected=Build(c,owner,commit.storages,before,context,source->id,seal->id,
        {b[4],read(5,8),read(13,8),static_cast<std::uint16_t>(read(21,2))});
    if(!expected || expected->commit.storages!=commit.storages || expected->commit.items.size()!=commit.items.size())return false;
    for(std::size_t i=0;i<commit.items.size();++i)
        if(expected->commit.items[i].before!=commit.items[i].before || expected->commit.items[i].after!=commit.items[i].after)return false;
    return true;
}
}
