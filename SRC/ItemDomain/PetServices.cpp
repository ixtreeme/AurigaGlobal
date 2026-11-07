#include <item/PersistentItems.hpp>
#include <algorithm>
#include <set>

namespace auriga::item {
namespace {
using Error=PetStudyCommitError;
const PersistentItem* Find(std::span<const PersistentItem> items,ItemInstanceId id) {
    const auto it=std::ranges::find(items,id,&PersistentItem::id);
    return it==items.end() ? nullptr : &*it;
}
void Receipt(ItemCommit& commit,const PetCareContext& context,const PetServiceTerms& terms,
    std::optional<ItemPlacement> placement={}) {
    commit.result={'P','S','U',static_cast<std::uint8_t>(placement ? 2 : 1),terms.selector};
    const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)commit.result.push_back(static_cast<std::uint8_t>(v>>(i*8)));};
    put(terms.nowUtcMs,8);put(terms.elapsedMs,8);put(terms.rollBp,2);put(context.characterLevel,2);
    put(context.characterClass,1);put(context.sex,1);put(context.inCombat,1);
    if(placement){put(static_cast<unsigned>(placement->kind),1);put(placement->storageId,8);put(placement->anchor,4);}
    if(terms.selector>=7){commit.result[3]=3;put(terms.following,1);}
}
std::expected<PetStudyWrite,Error> Skin(const ItemCatalog& c,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,const PetCareContext& context,
    const PersistentItem& skin,const PersistentItem& seal,const PetServiceTerms& terms) {
    const bool equip=terms.selector==7;
    if(!skin.petSupply || !seal.growthPet || skin.destroyed || seal.destroyed || terms.selector>8 || terms.rollBp || terms.placement ||
        skin.owner!=owner.owner || seal.owner!=owner.owner || !skin.placement || !seal.placement ||
        skin.placement->kind!=ItemStorageKind::Inventory || seal.placement->kind!=ItemStorageKind::Inventory ||
        (skin.boundTo && skin.boundTo!=owner.owner) || (seal.boundTo && seal.boundTo!=owner.owner) ||
        (equip ? skin.petSupply->equippedSeal || !terms.following || !RecallRequested(seal) : skin.petSupply->equippedSeal!=seal.id.Value()) ||
        (terms.elapsedMs && !skin.petSupply->equippedSeal))return std::unexpected(Error{PetLifecycleError::WrongTargetState});
    for(const auto& other:items)if(equip && !other.destroyed && other.petSupply && other.petSupply->equippedSeal==seal.id.Value())
        return std::unexpected(Error{PetLifecycleError::WrongTargetState});
    const PetCareItemSnapshot target{seal.key,seal.id.Value(),seal.revision,seal.owner.id,seal.count,seal.refinementLevel,
        false,false,false,true,terms.following,terms.following ? seal.id.Value() : 0};
    const PetLifecycleRequest pet{context,target,seal.revision,terms.nowUtcMs,*seal.growthPet};
    PetCareItemSnapshot source{skin.key,skin.id.Value(),skin.revision,skin.owner.id,skin.count,skin.refinementLevel};
    source.equipped=skin.petSupply->equippedSeal!=0;
    const auto plan=PlanPetAppearance(c,{pet,source,skin.petSupply->lifetime,{terms.nowUtcMs,terms.elapsedMs,source.equipped,equip},equip,true});
    if(!plan || (equip && !plan->equip))return std::unexpected(Error{plan ? PetLifecycleError::WrongTargetState : plan.error()});
    auto next=skin;++next.revision;next.petSupply->lifetime=plan->lifetime.after;next.petSupply->equippedSeal=plan->equip ? seal.id.Value() : 0;
    if(plan->lifetime.removeItem){next.count=0;next.destroyed=true;next.placement.reset();}
    auto guarded=seal;++guarded.revision;
    ItemCommit commit;commit.operationId=context.operationId;commit.catalogRevision=c.Revision();commit.catalogHash=c.ContentHash();
    commit.kind=ItemOperationKind::Service;commit.outcomeCode=0x2608;commit.owners={owner};commit.items={{skin,next},{seal,guarded}};
    for(const auto* item:{&skin,&seal}) {
        const auto layout=std::ranges::find(layouts,ItemStorage{owner.owner,item->placement->kind,item->placement->storageId},&ItemStorageLayout::storage);
        if(layout==layouts.end())return std::unexpected(Error{ItemPersistenceError::InvalidStorage});
        if(std::ranges::find(commit.storages,*layout)==commit.storages.end())commit.storages.push_back(*layout);
    }
    Receipt(commit,context,terms);PetSupplyPlan result{{pet,source},*seal.growthPet};result.consumeUnits=next.destroyed ? 1 : 0;result.succeeded=true;
    result.catalogRevision=c.Revision();result.catalogHash=c.ContentHash();return PetStudyWrite{result,std::move(commit)};
}
std::expected<PetStudyWrite,Error> Transport(const ItemCatalog& c,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,const PetCareContext& context,
    const PersistentItem& carrier,const PersistentItem& seal,const PetServiceTerms& terms) {
    const bool unpack=terms.selector==6;
    if(std::ranges::any_of(items,[&](const auto& i){return !i.destroyed && i.petSupply && i.petSupply->equippedSeal==seal.id.Value();}))
        return std::unexpected(Error{PetLifecycleError::WrongTargetState});
    const auto* entry=c.Find(carrier.key);const auto* row=entry ? std::get_if<ItemPetSupplyRow>(&entry->category) : nullptr;
    const auto* profile=row ? c.FindPetSupplyProfile(row->profile) : nullptr;
    if(!profile || profile->kind!=PetSupplyKind::Carrier || !carrier.petSupply || !seal.growthPet ||
        carrier.owner!=owner.owner || seal.owner!=owner.owner || carrier.destroyed || seal.destroyed ||
        !carrier.placement || carrier.placement->kind!=ItemStorageKind::Inventory || !seal.placement ||
        carrier.count!=1 || carrier.refinementLevel || (carrier.boundTo && carrier.boundTo!=owner.owner) ||
        (seal.boundTo && seal.boundTo!=owner.owner) || RecallRequested(seal) || terms.elapsedMs || terms.rollBp ||
        (!unpack && seal.placement->kind!=ItemStorageKind::Inventory) ||
        (unpack && (seal.placement->kind!=ItemStorageKind::PetCarrier || seal.placement->storageId!=carrier.id.Value())))
        return std::unexpected(Error{PetLifecycleError::WrongTargetState});
    const PetCareItemSnapshot target{seal.key,seal.id.Value(),seal.revision,seal.owner.id,seal.count,seal.refinementLevel,false,false,false,true};
    const PetLifecycleRequest pet{context,target,seal.revision,terms.nowUtcMs,*seal.growthPet};
    PetCareItemSnapshot box{carrier.key,carrier.id.Value(),carrier.revision,carrier.owner.id,carrier.count,carrier.refinementLevel};
    box.containsPet=carrier.petSupply->containedSeal!=0;
    const auto plan=PlanPetTransport(c,{pet,box,unpack,true,carrier.revision,carrier.petSupply->containedSeal});
    if(!plan)return std::unexpected(Error{plan.error()});
    auto afterSeal=seal;auto afterCarrier=carrier;++afterSeal.revision;++afterCarrier.revision;
    afterSeal.growthPet=plan->after;
    ItemCommit commit;commit.operationId=context.operationId;commit.catalogRevision=c.Revision();commit.catalogHash=c.ContentHash();
    commit.kind=ItemOperationKind::Service;commit.outcomeCode=0x2608;commit.owners={owner};
    const auto addLayout=[&](const ItemStorageLayout& layout){if(std::ranges::find(commit.storages,layout)==commit.storages.end())commit.storages.push_back(layout);};
    for(const auto* item:{&carrier,&seal}) {
        const auto at=std::ranges::find(layouts,ItemStorage{item->owner,item->placement->kind,item->placement->storageId},&ItemStorageLayout::storage);
        if(at==layouts.end())return std::unexpected(Error{ItemPersistenceError::InvalidStorage});addLayout(*at);
    }
    if(!unpack) {
        afterCarrier.petSupply->containedSeal=seal.id.Value();afterSeal.placement=ItemPlacement{ItemStorageKind::PetCarrier,carrier.id.Value(),0};
        addLayout({{owner.owner,ItemStorageKind::PetCarrier,carrier.id.Value()},1,1,1});
    } else {
        afterCarrier.count=0;afterCarrier.destroyed=true;afterCarrier.placement.reset();
        std::set<std::pair<std::uint64_t,std::uint32_t>> taken;
        for(const auto& item:items)if(item.id!=carrier.id && item.id!=seal.id && !item.destroyed && item.owner==owner.owner && item.placement && item.placement->kind==ItemStorageKind::Inventory) {
            const auto layout=std::ranges::find(layouts,ItemStorage{owner.owner,ItemStorageKind::Inventory,item.placement->storageId},&ItemStorageLayout::storage);
            if(layout==layouts.end())return std::unexpected(Error{ItemPersistenceError::InvalidStorage});
            const auto cells=OccupiedItemCells(c,item,*layout);if(!cells)return std::unexpected(Error{cells.error()});
            for(auto cell:*cells)taken.emplace(item.placement->storageId,cell);
        }
        bool placed=false;
        for(const auto& bag:layouts)if(!placed && bag.storage.owner==owner.owner && bag.storage.kind==ItemStorageKind::Inventory)
            for(std::uint32_t anchor=0;anchor<std::uint32_t(bag.columns)*bag.rows*bag.pages;++anchor) {
                const ItemPlacement candidate{ItemStorageKind::Inventory,bag.storage.id,anchor};
                if(terms.placement && candidate!=*terms.placement)continue;
                afterSeal.placement=candidate;
                const auto cells=OccupiedItemCells(c,afterSeal,bag);
                if(cells && std::ranges::none_of(*cells,[&](auto cell){return taken.contains({bag.storage.id,cell});})) {
                    addLayout(bag);placed=true;break;
                }
            }
        if(!placed)return std::unexpected(Error{ItemPersistenceError::InvalidPlacement});
    }
    commit.items={{carrier,std::move(afterCarrier)},{seal,std::move(afterSeal)}};
    Receipt(commit,context,terms,commit.items[1].after.placement);
    PetSupplyPlan result{{pet,box},plan->after};result.consumeUnits=unpack ? 1 : 0;result.succeeded=true;
    result.catalogRevision=c.Revision();result.catalogHash=c.ContentHash();return PetStudyWrite{result,std::move(commit)};
}
std::expected<PetStudyWrite,Error> Build(const ItemCatalog& c,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,const PetCareContext& context,
    ItemInstanceId supplyId,ItemInstanceId sealId,const PetServiceTerms& terms) {
    const auto* supply=Find(items,supplyId);const auto* seal=Find(items,sealId);
    if(terms.selector>=5) {
        if(!supply || !seal || owner.owner!=ItemOwner{ItemOwnerKind::Character,context.actorId} || owner.sessionGeneration!=context.sessionGeneration)
            return std::unexpected(Error{PetLifecycleError::InvalidSnapshot});
        return terms.selector>=7 ? Skin(c,owner,layouts,items,context,*supply,*seal,terms) : Transport(c,owner,layouts,items,context,*supply,*seal,terms);
    }
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
    Receipt(*commit,context,terms);
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
        b.size()<28 || b[0]!='P' || b[1]!='S' || b[2]!='U' ||
        (b[3]==1 ? b.size()!=28 || b[4]>4 : b[3]==2 ? b.size()!=41 || b[4]<5 || b[4]>6 : b[3]!=3 || b.size()!=29 || b[4]<7 || b[4]>8 || b[28]>1) || b[27]>1)return false;
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
    PetServiceTerms terms{b[4],read(5,8),read(13,8),static_cast<std::uint16_t>(read(21,2))};
    if(b[3]==2)terms.placement=ItemPlacement{static_cast<ItemStorageKind>(b[28]),read(29,8),static_cast<std::uint32_t>(read(37,4))};
    if(b[3]==3)terms.following=b[28]!=0;
    const auto expected=Build(c,owner,commit.storages,before,context,source->id,seal->id,terms);
    if(!expected || expected->commit.storages!=commit.storages || expected->commit.result!=commit.result || expected->commit.items.size()!=commit.items.size())return false;
    for(std::size_t i=0;i<commit.items.size();++i)
        if(expected->commit.items[i].before!=commit.items[i].before || expected->commit.items[i].after!=commit.items[i].after)return false;
    return true;
}
}
