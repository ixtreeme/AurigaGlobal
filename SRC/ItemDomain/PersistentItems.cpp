#include <item/PersistentItems.hpp>
#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <openssl/evp.h>

namespace auriga::item
{
namespace
{
using E=ItemPersistenceError;
bool StorageOwner(ItemStorage s)
{
    if(!ValidItemOwner(s.owner) || !s.id)return false;
    switch(s.kind)
    {
    case ItemStorageKind::Inventory: case ItemStorageKind::Equipment: case ItemStorageKind::MountInventory:
        return s.owner.kind==ItemOwnerKind::Character;
    case ItemStorageKind::Safebox: case ItemStorageKind::Mall:return s.owner.kind==ItemOwnerKind::Account;
    case ItemStorageKind::GuildStorage:return s.owner.kind==ItemOwnerKind::Guild;
    case ItemStorageKind::TradeEscrow: case ItemStorageKind::Mail: case ItemStorageKind::PendingAward:return true;
    default:return false;
    }
}
bool Wallet(const ItemWallet& w)
{
    return ValidItemOwner(w.owner) && w.revision && w.balance<=INT64_MAX &&
        ((w.currency==WalletCurrency::Gold && w.owner.kind==ItemOwnerKind::Character) ||
         (w.currency==WalletCurrency::DragonCoin && w.owner.kind==ItemOwnerKind::Account));
}
struct Writer
{
    std::vector<std::uint8_t> bytes;
    void Put(std::uint64_t n,unsigned size){for(unsigned i=0;i<size;++i)bytes.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
    void Owner(ItemOwner o){Put(static_cast<std::uint8_t>(o.kind),1);Put(o.id,8);}
    void Key(ItemKey k){Put(static_cast<std::uint16_t>(k.Category()),2);Put(k.LocalId(),4);}
    void Item(const PersistentItem& p)
    {
        Put(p.id.Value(),8);Key(p.key);Owner(p.owner);Put(p.revision,8);Put(p.refinementLevel,2);Put(p.count,4);
        Put(p.boundTo.has_value(),1);if(p.boundTo)Owner(*p.boundTo);Put(p.destroyed,1);
        Put(p.placement.has_value(),1);if(p.placement){Put(static_cast<std::uint8_t>(p.placement->kind),1);Put(p.placement->storageId,8);Put(p.placement->anchor,4);}
        Put(p.attributes.lockedSlot.has_value(),1);if(p.attributes.lockedSlot)Put(*p.attributes.lockedSlot,1);
        for(const auto& e:p.attributes.slots){Put(e.has_value(),1);if(e){Put(static_cast<std::uint32_t>(e->bonus),4);Put(static_cast<std::uint64_t>(e->valueScaled),8);}}
        for(const auto& s:p.stones){Put(static_cast<std::uint8_t>(s.state),1);Put(s.grade,1);Put(s.stone.has_value(),1);if(s.stone)Key(*s.stone);Put(s.refinementLevel,2);}
        Put(p.accessory.has_value(),1);if(p.accessory){Put(p.accessory->unlockedCapacity,1);Put(p.accessory->activeGrade,1);Put(p.accessory->remainingMs,8);Put(p.accessory->permanent,1);}
    }
    void Money(const ItemWallet& w){Owner(w.owner);Put(static_cast<std::uint8_t>(w.currency),1);Put(w.revision,8);Put(w.balance,8);}
};
}
bool ValidItemOwner(ItemOwner o) noexcept
{return o.id && o.kind>=ItemOwnerKind::Character && o.kind<=ItemOwnerKind::Guild;}
bool ValidItemStorage(const ItemStorageLayout& l) noexcept
{
    return StorageOwner(l.storage) && l.columns && l.rows && l.pages &&
        std::uint64_t{l.columns}*l.rows*l.pages<=65536 &&
        (l.storage.kind!=ItemStorageKind::Equipment || (l.rows==1 && l.pages==1));
}
bool SupportsPersistentItem(const ItemCatalog& c,const ItemCatalogEntry& e) noexcept
{
    if(const auto* consumable=std::get_if<ItemConsumableRow>(&e.category))
    {
        if(consumable->socketServiceProfile.has_value() || consumable->attributeServiceProfile.has_value())return true;
        // Instant, reserve and periodic recovery act on the character, not the item.
        const auto* recovery=consumable->recoveryProfile ? c.FindRecovery(consumable->recoveryProfile) : nullptr;
        return recovery && !std::holds_alternative<AutomaticRecovery>(recovery->Data().effect);
    }
    return std::holds_alternative<ItemWeaponRow>(e.category) || std::holds_alternative<ItemArmorRow>(e.category) ||
        std::holds_alternative<ItemAccessoryRow>(e.category) || std::holds_alternative<ItemMaterialRow>(e.category) ||
        std::holds_alternative<ItemSocketStoneRow>(e.category) || std::holds_alternative<ItemCurrencyTokenRow>(e.category) ||
        std::holds_alternative<ItemSkillBookRow>(e.category) || std::holds_alternative<ItemQuestRow>(e.category) ||
        std::holds_alternative<ItemContainerKeyRow>(e.category);
}
std::expected<void,E> ValidatePersistentItem(const ItemCatalog& c,const PersistentItem& p)
{
    const auto* e=c.Find(p.key);if(!e)return std::unexpected(E::MissingItem);
    if(!SupportsPersistentItem(c,*e))return std::unexpected(E::UnsupportedState);
    if(!ValidItemOwner(p.owner) || !p.revision || (p.boundTo && !ValidItemOwner(*p.boundTo)) ||
       (e->base.binding==BindingPolicy::OnAcquire && !p.boundTo) ||
       (p.destroyed ? (p.count || p.placement.has_value()) : (!p.count || p.count>e->base.maxStack || !p.placement)))return std::unexpected(E::InvalidItem);
    if(!c.FindRefinement(e->base.refinementProfile)->FindLevel(p.refinementLevel))return std::unexpected(E::InvalidLevel);
    if(!ValidateInstanceAttributes(c,p.key,p.attributes))return std::unexpected(E::InvalidAttributes);
    const auto* armor=std::get_if<ItemArmorRow>(&e->category);
    if(std::holds_alternative<ItemWeaponRow>(e->category) || (armor && armor->slot==EquipmentSlot::Body))
    {if(!ValidateEquipmentStoneSockets(c,p.key,p.stones))return std::unexpected(E::InvalidSockets);}
    else if(std::ranges::any_of(p.stones,[](const auto& s){return s!=StoneSocketSlot{};}))return std::unexpected(E::InvalidSockets);
    const auto* a=std::get_if<ItemAccessoryRow>(&e->category);
    if(a && a->socketProfile)
    {if(!p.accessory || !ValidateAccessorySocketState(*c.FindAccessorySocket(*a->socketProfile),*p.accessory))return std::unexpected(E::InvalidSockets);}
    else if(p.accessory)return std::unexpected(E::InvalidSockets);
    if(p.placement && !StorageOwner({p.owner,p.placement->kind,p.placement->storageId}))return std::unexpected(E::InvalidPlacement);
    return {};
}
std::expected<std::vector<std::uint32_t>,E> OccupiedItemCells(const ItemCatalog& c,const PersistentItem& p,const ItemStorageLayout& l)
{
    if(!ValidatePersistentItem(c,p) || !ValidItemStorage(l) || p.destroyed || !p.placement ||
       l.storage!=ItemStorage{p.owner,p.placement->kind,p.placement->storageId})return std::unexpected(E::InvalidPlacement);
    const auto& b=c.Find(p.key)->base;
    const auto anchor=p.placement->anchor,pageSize=std::uint32_t{l.columns}*l.rows;
    if(anchor>=std::uint64_t{pageSize}*l.pages)return std::unexpected(E::InvalidPlacement);
    if(l.storage.kind==ItemStorageKind::Equipment)
    {
        std::optional<EquipmentSlot> slot;
        const auto& category=c.Find(p.key)->category;
        if(const auto* w=std::get_if<ItemWeaponRow>(&category))slot=w->slot;
        if(const auto* a=std::get_if<ItemArmorRow>(&category))slot=a->slot;
        if(const auto* a=std::get_if<ItemAccessoryRow>(&category))slot=a->slot;
        if(!slot || anchor+1!=static_cast<unsigned>(*slot))return std::unexpected(E::InvalidPlacement);
        return std::vector<std::uint32_t>{anchor};
    }
    const auto offset=anchor%pageSize;
    if(offset%l.columns+b.width>l.columns || offset/l.columns+b.height>l.rows)return std::unexpected(E::InvalidPlacement);
    std::vector<std::uint32_t> cells;
    for(unsigned y=0;y<b.height;++y)for(unsigned x=0;x<b.width;++x)cells.push_back(anchor+y*l.columns+x);
    return cells;
}
std::expected<void,E> ValidateItemCommit(const ItemCatalog& c,const ItemCommit& r)
{
    if(r.catalogRevision!=c.Revision() || r.catalogHash!=c.ContentHash())return std::unexpected(E::CatalogMismatch);
    if(r.owners.empty() || r.owners.size()>MaxItemCommitOwners || r.items.size()>MaxItemCommitItems ||
       r.storages.size()>MaxItemCommitStorages || r.wallets.size()>MaxItemCommitOwners || r.result.size()>MaxItemCommitResult)return std::unexpected(E::LimitExceeded);
    if((r.items.empty() && r.wallets.empty()) || !r.outcomeCode || r.kind<ItemOperationKind::Grant || r.kind>ItemOperationKind::Trade ||
       std::ranges::all_of(r.operationId,[](auto n){return !n;}))return std::unexpected(E::InvalidOperation);
    std::set<ItemOwner> owners;
    for(const auto& o:r.owners)if(!ValidItemOwner(o.owner) || !o.sessionGeneration || !o.revision || o.revision==UINT64_MAX || !owners.insert(o.owner).second)return std::unexpected(E::InvalidFence);
    std::map<ItemStorage,ItemStorageLayout> layouts;
    for(const auto& l:r.storages)if(!ValidItemStorage(l) || !owners.contains(l.storage.owner) || !layouts.emplace(l.storage,l).second)return std::unexpected(E::InvalidStorage);
    std::set<ItemOwner> usedOwners;std::set<ItemStorage> usedLayouts;
    std::set<std::uint64_t> ids;std::set<std::pair<ItemStorage,std::uint32_t>> occupied;
    for(const auto& change:r.items)
    {
        const auto& p=change.after;
        if(!ids.insert(p.id.Value()).second)return std::unexpected(E::InvalidChange);
        for(const auto* item:{change.before ? &*change.before : nullptr,&p})if(item)
        {
            if(auto valid=ValidatePersistentItem(c,*item);!valid)return valid;
            if(!owners.contains(item->owner))return std::unexpected(E::InvalidFence);
            usedOwners.insert(item->owner);
            if(item->placement)
            {
                const ItemStorage storage{item->owner,item->placement->kind,item->placement->storageId};
                auto layout=layouts.find(storage);if(layout==layouts.end())return std::unexpected(E::InvalidStorage);
                auto cells=OccupiedItemCells(c,*item,layout->second);if(!cells)return std::unexpected(cells.error());usedLayouts.insert(storage);
                if(item==&p)for(auto cell:*cells)if(!occupied.emplace(storage,cell).second)return std::unexpected(E::InvalidPlacement);
            }
        }
        if(change.before)
        {
            const auto& b=*change.before;
            const bool bindingUnchanged=p.boundTo==b.boundTo;
            const bool bindOnEquip=!b.boundTo && p.boundTo==p.owner && b.owner==p.owner && b.key==p.key &&
                c.Find(p.key)->base.binding==BindingPolicy::OnEquip && p.placement && p.placement->kind==ItemStorageKind::Equipment;
            if(b.destroyed || b.id!=p.id || b.revision==UINT64_MAX || p.revision!=b.revision+1 || (!bindingUnchanged && !bindOnEquip) || b==p)return std::unexpected(E::InvalidChange);
        }
        else if(p.destroyed || p.revision!=1)return std::unexpected(E::InvalidChange);
        if((!change.before || change.before->key!=p.key) && c.Find(p.key)->base.retired)return std::unexpected(E::InvalidChange);
    }
    std::set<std::pair<ItemOwner,WalletCurrency>> wallets;
    for(const auto& w:r.wallets)
    {
        if(!Wallet(w.before) || !Wallet(w.after) || w.before.owner!=w.after.owner || w.before.currency!=w.after.currency ||
           w.before.revision==UINT64_MAX || w.after.revision!=w.before.revision+1 || w.before.balance==w.after.balance ||
           !owners.contains(w.before.owner) || !wallets.emplace(w.before.owner,w.before.currency).second)return std::unexpected(E::InvalidWallet);
        usedOwners.insert(w.before.owner);
    }
    if(usedOwners!=owners || usedLayouts.size()!=layouts.size())return std::unexpected(E::InvalidFence);
    return {};
}
std::expected<ItemCatalogDigest,E> ItemCommitDigest(const ItemCatalog& c,const ItemCommit& r)
{
    if(auto v=ValidateItemCommit(c,r);!v)return std::unexpected(v.error());
    Writer w;for(auto b:std::string_view("auriga.item_commit.v1"))w.Put(static_cast<unsigned char>(b),1);
    for(auto b:r.operationId)w.Put(b,1);w.Put(r.catalogRevision,8);for(auto b:r.catalogHash)w.Put(b,1);w.Put(static_cast<std::uint8_t>(r.kind),1);
    auto owners=r.owners;std::ranges::sort(owners,{},&ItemOwnerFence::owner);w.Put(owners.size(),1);
    for(const auto& o:owners){w.Owner(o.owner);w.Put(o.sessionGeneration,8);w.Put(o.revision,8);}
    auto storages=r.storages;std::ranges::sort(storages,{},&ItemStorageLayout::storage);w.Put(storages.size(),1);
    for(const auto& s:storages){w.Owner(s.storage.owner);w.Put(static_cast<std::uint8_t>(s.storage.kind),1);w.Put(s.storage.id,8);w.Put(s.columns,2);w.Put(s.rows,2);w.Put(s.pages,2);}
    auto items=r.items;std::ranges::sort(items,{},[](const auto& i){return i.after.id;});w.Put(items.size(),1);
    for(const auto& i:items){w.Put(i.before.has_value(),1);if(i.before)w.Item(*i.before);w.Item(i.after);}
    auto wallets=r.wallets;std::ranges::sort(wallets,{},[](const auto& v){return std::pair{v.before.owner,v.before.currency};});w.Put(wallets.size(),1);
    for(const auto& v:wallets){w.Money(v.before);w.Money(v.after);}w.Put(r.outcomeCode,2);w.Put(r.result.size(),4);for(auto b:r.result)w.Put(b,1);
    ItemCatalogDigest digest{};unsigned length=0;
    if(EVP_Digest(w.bytes.data(),w.bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)!=1 || length!=digest.size())return std::unexpected(E::HashFailure);
    return digest;
}
std::expected<ItemCatalogDigest,E> PersistentItemDigest(const ItemCatalog& c,const PersistentItem& p)
{
    if(auto v=ValidatePersistentItem(c,p);!v)return std::unexpected(v.error());
    Writer w;for(auto b:std::string_view("auriga.item_instance.v1"))w.Put(static_cast<unsigned char>(b),1);w.Item(p);
    ItemCatalogDigest digest{};unsigned length=0;
    if(EVP_Digest(w.bytes.data(),w.bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)!=1 || length!=digest.size())return std::unexpected(E::HashFailure);
    return digest;
}
std::expected<ItemCommit,E> MakeRefinementCommit(const ItemCatalog& c,const RefinementPlan& p,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> items,const ItemWallet& wallet)
{
    if(p.catalogRevision!=c.Revision() || p.catalogHash!=c.ContentHash())return std::unexpected(E::CatalogMismatch);
    if(owner.owner!=ItemOwner{ItemOwnerKind::Character,p.actorId} || wallet.owner!=owner.owner || wallet.currency!=WalletCurrency::Gold ||
       wallet.revision!=p.expectedWalletRevision || wallet.balance!=p.expectedGold || p.goldCost>wallet.balance)return std::unexpected(E::InvalidWallet);
    const auto* recipe=c.FindRefinementRecipe(p.recipeId);
    if(!recipe || items.size()>MaxRefinementInventory)return std::unexpected(E::InvalidChange);
    std::set<std::uint64_t> ids;std::vector<RefinementItemSnapshot> inventory;
    const PersistentItem* source=nullptr;
    for(const auto& i:items)
    {
        if(!ids.insert(i.id.Value()).second || i.owner!=owner.owner || i.destroyed || !i.placement || i.placement->kind!=ItemStorageKind::Inventory || !ValidatePersistentItem(c,i))return std::unexpected(E::InvalidChange);
        if(i.id.Value()==p.sourceInstanceId)source=&i;
        inventory.push_back({i.id.Value(),i.revision,i.owner.id,i.key,i.refinementLevel,i.count,false});
    }
    if(!source)return std::unexpected(E::InvalidChange);
    // Recompute the pure plan from exact snapshots to reject a forged debit,
    // result, cost or outcome before it reaches SQL.
    auto expected=PrepareRefinement(c,p.recipeId,{p.operationId,p.actorId,wallet.revision,wallet.balance,true},
        {source->id.Value(),source->revision,source->owner.id,source->key,source->refinementLevel,source->count,false},inventory,p.recordedRoll);
    if(!expected || expected->source!=p.source || expected->sourceLevel!=p.sourceLevel || expected->expectedSourceRevision!=p.expectedSourceRevision ||
       expected->goldCost!=p.goldCost || expected->result!=p.result || expected->resultLevel!=p.resultLevel || expected->success!=p.success || expected->debits.size()!=p.debits.size())return std::unexpected(E::InvalidChange);
    for(std::size_t i=0;i<p.debits.size();++i)
    {const auto& a=expected->debits[i];const auto& b=p.debits[i];if(a.instanceId!=b.instanceId || a.expectedRevision!=b.expectedRevision || a.expectedCount!=b.expectedCount || a.consumeCount!=b.consumeCount)return std::unexpected(E::InvalidChange);}
    ItemCommit out;out.operationId=p.operationId;out.catalogRevision=p.catalogRevision;out.catalogHash=p.catalogHash;out.kind=ItemOperationKind::Refine;out.owners={owner};
    auto after=*source;after.revision++;
    if(p.result){after.key=*p.result;after.refinementLevel=*p.resultLevel;}
    else{after.destroyed=true;after.count=0;after.placement.reset();}
    out.items.push_back({*source,after});
    for(const auto& debit:p.debits)
    {
        const auto found=std::ranges::find(items,debit.instanceId,[](const auto& i){return i.id.Value();});
        if(found==items.end() || found->count<debit.consumeCount || found->id==source->id)return std::unexpected(E::InvalidChange);
        auto next=*found;next.revision++;next.count-=debit.consumeCount;if(!next.count){next.destroyed=true;next.placement.reset();}out.items.push_back({*found,next});
    }
    if(p.goldCost){auto next=wallet;next.revision++;next.balance-=p.goldCost;out.wallets.push_back({wallet,next});}
    for(const auto& layout:storages)
    {
        bool used=false;for(const auto& change:out.items)for(const auto* i:{&*change.before,&change.after})
            if(i->placement && layout.storage==ItemStorage{i->owner,i->placement->kind,i->placement->storageId})used=true;
        if(used)out.storages.push_back(layout);
    }
    out.outcomeCode=p.success ? 1 : 2;out.result={static_cast<std::uint8_t>(p.success)};
    if(auto v=ValidateItemCommit(c,out);!v)return std::unexpected(v.error());return out;
}
namespace
{
std::expected<ItemCommit,E> PairCommit(const ItemCatalog& c,const SocketItemPair& pair,std::uint16_t units,
    ItemOwnerFence owner,std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory)
{
    if(owner.owner!=ItemOwner{ItemOwnerKind::Character,pair.context.actorId} || owner.sessionGeneration!=pair.context.sessionGeneration ||
       owner.revision!=pair.context.inventoryRevision || inventory.size()>MaxRefinementInventory)return std::unexpected(E::InvalidFence);
    std::set<std::uint64_t> ids;for(const auto& i:inventory)if(!ids.insert(i.id.Value()).second)return std::unexpected(E::InvalidChange);
    const auto find=[&](const StoneItemSnapshot& snapshot)->const PersistentItem*
    {
        const auto it=std::ranges::find(inventory,snapshot.instanceId,[](const auto& i){return i.id.Value();});
        if(it==inventory.end() || !ValidatePersistentItem(c,*it) || it->destroyed || it->owner!=owner.owner ||
           snapshot.ownerId!=it->owner.id || snapshot.item!=it->key || snapshot.revision!=it->revision || snapshot.refinementLevel!=it->refinementLevel ||
           snapshot.count!=it->count || !it->placement || it->placement->kind!=ItemStorageKind::Inventory || snapshot.locked || snapshot.equipped || snapshot.exchanging)return nullptr;
        return &*it;
    };
    const auto* source=find(pair.source);const auto* target=find(pair.target);
    if(!source || !target || source->id==target->id || units>source->count)return std::unexpected(E::InvalidChange);
    ItemCommit r;r.operationId=pair.context.operationId;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=ItemOperationKind::Service;r.owners={owner};
    auto next=*target;next.revision++;r.items.push_back({*target,next});
    if(units){next=*source;next.revision++;next.count-=units;if(!next.count){next.destroyed=true;next.placement.reset();}r.items.push_back({*source,next});}
    for(const auto& layout:storages)
    {
        bool used=false;for(const auto& change:r.items)for(const auto* i:{&*change.before,&change.after})
            if(i->placement && layout.storage==ItemStorage{i->owner,i->placement->kind,i->placement->storageId})used=true;
        if(used)r.storages.push_back(layout);
    }
    return r;
}
}
std::expected<ItemCommit,E> MakeAttributeCommit(const ItemCatalog& c,const AttributePlan& plan,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory)
{
    if(plan.catalogRevision!=c.Revision() || plan.catalogHash!=c.ContentHash())return std::unexpected(E::CatalogMismatch);
    const auto recalculated=PlanAttributeService(c,plan.expected);
    if(!recalculated || recalculated->after!=plan.after || recalculated->consumeUnits!=plan.consumeUnits || recalculated->succeeded!=plan.succeeded)return std::unexpected(E::InvalidChange);
    auto commit=PairCommit(c,plan.expected.items,plan.consumeUnits,owner,storages,inventory);if(!commit)return commit;
    if(commit->items[0].before->attributes!=plan.expected.state)return std::unexpected(E::InvalidChange);
    commit->items[0].after.attributes=plan.after;commit->outcomeCode=plan.succeeded ? 1 : 2;
    const auto payload=EncodeInstanceAttributes(c,commit->items[0].after.key,plan.after);if(!payload)return std::unexpected(E::InvalidAttributes);
    commit->result=*payload;
    if(auto valid=ValidateItemCommit(c,*commit);!valid)return std::unexpected(valid.error());return commit;
}
std::expected<ItemCommit,E> MakeSocketServiceCommit(const ItemCatalog& c,const SocketServicePlan& plan,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory)
{
    if(plan.catalogRevision!=c.Revision() || plan.catalogHash!=c.ContentHash())return std::unexpected(E::CatalogMismatch);
    // Returned stones require output-ID/placement admission, implemented in a
    // later writer slice. Do not consume their source and silently lose returns.
    if(!plan.returns.empty())return std::unexpected(E::UnsupportedState);
    const auto recalculated=PlanSocketService(c,plan.expected);
    if(!recalculated || !recalculated->returns.empty() || recalculated->after!=plan.after || recalculated->consumeUnits!=plan.consumeUnits)return std::unexpected(E::InvalidChange);
    auto commit=PairCommit(c,plan.expected.items,plan.consumeUnits,owner,storages,inventory);if(!commit)return commit;
    if(commit->items[0].before->stones!=plan.expected.sockets)return std::unexpected(E::InvalidChange);
    commit->items[0].after.stones=plan.after;commit->outcomeCode=1;
    if(auto valid=ValidateItemCommit(c,*commit);!valid)return std::unexpected(valid.error());return commit;
}
namespace
{
// Relocations of placed items (move, equip, swap, unequip): every touched
// storage is re-checked as a whole after all changes, so a swap frees its
// cells before they are taken and no two items ever overlap.
std::expected<ItemCommit,E> Relocate(const ItemCatalog& c,const std::array<std::uint8_t,16>& operationId,const ItemOwnerFence& owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,std::vector<ItemChange> changes)
{
    if(std::ranges::all_of(operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    const auto layoutOf=[&](const ItemPlacement& p)->const ItemStorageLayout*{
        const auto it=std::ranges::find_if(storages,[&](const ItemStorageLayout& l){return l.storage==ItemStorage{owner.owner,p.kind,p.storageId};});
        return it==storages.end()?nullptr:&*it;};
    std::vector<ItemStorageLayout> touched;
    for(const auto& change:changes)
        for(const auto* p:{&*change.before->placement,&*change.after.placement})
        {
            const auto* layout=layoutOf(*p);if(!layout)return std::unexpected(E::InvalidStorage);
            if(std::ranges::find(touched,*layout)==touched.end())touched.push_back(*layout);
        }
    std::set<std::pair<ItemStorage,std::uint32_t>> taken;
    for(const auto& item:inventory)
    {
        if(item.destroyed || !item.placement)continue;
        const auto changed=std::ranges::find_if(changes,[&](const ItemChange& ch){return ch.after.id==item.id;});
        const auto& placed=changed==changes.end()?item:changed->after;
        const auto* layout=layoutOf(*placed.placement);
        if(!layout || std::ranges::find(touched,*layout)==touched.end())continue;
        const auto cells=OccupiedItemCells(c,placed,*layout);if(!cells)return std::unexpected(cells.error());
        for(const auto cell:*cells)if(!taken.emplace(layout->storage,cell).second)return std::unexpected(E::InvalidPlacement);
    }
    ItemCommit r;r.operationId=operationId;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=ItemOperationKind::Move;
    r.owners={owner};r.storages=std::move(touched);r.items=std::move(changes);r.outcomeCode=1;r.result={static_cast<std::uint8_t>(ItemOperationKind::Move)};
    if(auto valid=ValidateItemCommit(c,r);!valid)return std::unexpected(valid.error());
    return r;
}
const PersistentItem* Placed(std::span<const PersistentItem> inventory,const ItemOwnerFence& owner,ItemInstanceId id)
{
    const auto it=std::ranges::find_if(inventory,[&](const PersistentItem& i){return i.id==id && !i.destroyed;});
    return it==inventory.end() || it->owner!=owner.owner || !it->placement?nullptr:&*it;
}
std::optional<EquipmentSlot> SlotOf(const ItemCatalogEntry& entry)
{
    if(const auto* w=std::get_if<ItemWeaponRow>(&entry.category))return w->slot;
    if(const auto* a=std::get_if<ItemArmorRow>(&entry.category))return a->slot;
    if(const auto* a=std::get_if<ItemAccessoryRow>(&entry.category))return a->slot;
    return std::nullopt;
}
}
std::expected<ItemCommit,E> MakeMoveCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operationId,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,ItemInstanceId id,ItemPlacement target)
{
    if(std::ranges::all_of(operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    if(target.kind!=ItemStorageKind::Inventory)return std::unexpected(E::UnsupportedState);
    const auto* source=Placed(inventory,owner,id);if(!source)return std::unexpected(E::MissingItem);
    if(source->placement->kind!=ItemStorageKind::Inventory)return std::unexpected(E::UnsupportedState);
    if(*source->placement==target)return std::unexpected(E::InvalidChange);
    auto moved=*source;moved.revision++;moved.placement=target;
    return Relocate(c,operationId,owner,storages,inventory,{{*source,moved}});
}
std::expected<ItemCommit,E> MakeEquipCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operationId,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,ItemInstanceId id,EquipContext who)
{
    if(std::ranges::all_of(operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    const auto* source=Placed(inventory,owner,id);if(!source)return std::unexpected(E::MissingItem);
    if(source->placement->kind!=ItemStorageKind::Inventory)return std::unexpected(E::UnsupportedState);
    const auto* entry=c.Find(source->key);if(!entry)return std::unexpected(E::MissingItem);
    const auto slot=SlotOf(*entry);if(!slot)return std::unexpected(E::UnsupportedState);
    if(entry->base.minimumLevel>who.level || who.characterClass>3 || who.sex>1)return std::unexpected(E::RequirementNotMet);
    if(entry->base.usePolicy)
    {
        const auto* policy=c.FindUsePolicy(entry->base.usePolicy);if(!policy)return std::unexpected(E::InvalidItem);
        if(!(policy->allowedClasses>>who.characterClass & 1) || !(policy->allowedSexes>>who.sex & 1))return std::unexpected(E::RequirementNotMet);
    }
    const ItemPlacement target{ItemStorageKind::Equipment,1,static_cast<std::uint32_t>(*slot)-1};
    auto equipped=*source;equipped.revision++;equipped.placement=target;
    if(entry->base.binding==BindingPolicy::OnEquip && !equipped.boundTo)equipped.boundTo=owner.owner;
    std::vector<ItemChange> changes{{*source,equipped}};
    const auto occupant=std::ranges::find_if(inventory,[&](const PersistentItem& i){return !i.destroyed && i.placement && *i.placement==target;});
    if(occupant!=inventory.end())
    {
        // The worn item takes the freed place; Relocate refuses it if it does not fit there.
        auto back=*occupant;back.revision++;back.placement=source->placement;
        changes.push_back({*occupant,back});
    }
    return Relocate(c,operationId,owner,storages,inventory,std::move(changes));
}
std::expected<ItemCommit,E> MakeConsumeCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operationId,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,ItemInstanceId id,std::uint32_t units)
{
    if(std::ranges::all_of(operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    const auto* source=Placed(inventory,owner,id);if(!source)return std::unexpected(E::MissingItem);
    if(source->placement->kind!=ItemStorageKind::Inventory)return std::unexpected(E::UnsupportedState);
    if(!units || units>source->count)return std::unexpected(E::InvalidChange);
    const auto layout=std::ranges::find_if(storages,[&](const ItemStorageLayout& l){
        return l.storage==ItemStorage{owner.owner,source->placement->kind,source->placement->storageId};});
    if(layout==storages.end())return std::unexpected(E::InvalidStorage);
    auto after=*source;after.revision++;after.count-=units;
    if(!after.count){after.destroyed=true;after.placement.reset();}
    ItemCommit r;r.operationId=operationId;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=ItemOperationKind::Consume;
    r.owners={owner};r.storages={*layout};r.items={{*source,after}};r.outcomeCode=1;r.result={static_cast<std::uint8_t>(ItemOperationKind::Consume)};
    if(auto valid=ValidateItemCommit(c,r);!valid)return std::unexpected(valid.error());
    return r;
}
std::expected<ItemCommit,E> MakeUnequipCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operationId,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,ItemInstanceId id,ItemPlacement target)
{
    if(std::ranges::all_of(operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    if(target.kind!=ItemStorageKind::Inventory)return std::unexpected(E::UnsupportedState);
    const auto* source=Placed(inventory,owner,id);if(!source)return std::unexpected(E::MissingItem);
    if(source->placement->kind!=ItemStorageKind::Equipment)return std::unexpected(E::UnsupportedState);
    auto taken=*source;taken.revision++;taken.placement=target;
    return Relocate(c,operationId,owner,storages,inventory,{{*source,taken}});
}
} // namespace auriga::item
