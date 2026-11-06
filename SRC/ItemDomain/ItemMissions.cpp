#include <item/ItemMissions.hpp>
#include <openssl/evp.h>
#include <algorithm>
#include <set>
#include <tuple>

namespace auriga::item
{
namespace
{
using E=ItemMissionError;
struct Writer
{
    std::vector<std::uint8_t> bytes;
    void Put(std::uint64_t value,unsigned n){for(unsigned i=0;i<n;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
    void Text(std::string_view value){Put(value.size(),2);bytes.insert(bytes.end(),value.begin(),value.end());}
    void Key(ItemKey key){Put(static_cast<unsigned>(key.Category()),2);Put(key.LocalId(),4);}
    void Amounts(std::vector<MissionItemAmount> values)
    {std::ranges::sort(values,{},[](const auto& v){return std::tuple{v.key,v.level};});Put(values.size(),1);for(const auto& v:values){Key(v.key);Put(v.level,2);Put(v.count,4);}}
};
bool Text(std::string_view s,std::size_t max,bool empty=false)
{return (empty || !s.empty()) && s.size()<=max && std::ranges::none_of(s,[](unsigned char c){return c<32 || c==127;});}
bool Code(std::string_view s){return !s.empty() && s.size()<=96 && s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.")==s.npos;}
bool Shape(const ItemMission& m)
{
    if(!m.id || !m.version || !Code(m.code) || !Text(m.name,128) || !Text(m.description,1024,true) || m.minimumLevel>255 ||
       m.goldCost>INT64_MAX || m.goldReward>INT64_MAX || (m.costs.empty() && m.rewards.empty() && m.goldCost==m.goldReward))return false;
    for(const auto* list:{&m.requirements,&m.costs,&m.rewards})
    {if(list->size()>MaxMissionItemEntries)return false;std::set<std::pair<ItemKey,std::uint16_t>> unique;
     for(const auto& v:*list)if(!v.count || v.count>50000 || !unique.emplace(v.key,v.level).second)return false;}
    return true;
}
std::expected<ItemCatalogDigest,E> Hash(const Writer& w)
{ItemCatalogDigest digest{};unsigned length=0;if(EVP_Digest(w.bytes.data(),w.bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)!=1 || length!=digest.size())return std::unexpected(E::Unavailable);return digest;}
}
bool MissionItemUsable(const PersistentItem& item,ItemOwner owner)
{return PlainInventoryItem(item,owner);}
std::uint64_t CountMissionItems(std::span<const PersistentItem> items,ItemOwner owner,ItemKey key,std::uint16_t level)
{std::uint64_t count=0;for(const auto& i:items)if(i.key==key && i.refinementLevel==level && MissionItemUsable(i,owner))count+=i.count;return count;}
std::expected<void,E> ValidateItemMission(const ItemCatalog& c,const ItemMission& m)
{
    if(!Shape(m))return std::unexpected(E::Invalid);
    for(const auto* list:{&m.requirements,&m.costs,&m.rewards})for(const auto& value:*list)
    {
        const auto* d=c.Find(value.key);
        if(!d || !c.RefinedBaseStats(value.key,value.level))return std::unexpected(E::Invalid);
        const bool supported=list==&m.rewards ? !d->base.retired && SupportsPersistentGrant(c,*d) : SupportsPersistentItem(c,*d);
        if(!supported)return std::unexpected(E::Invalid);
    }
    std::uint64_t stacks=0;for(const auto& reward:m.rewards){const auto stack=c.Find(reward.key)->base.maxStack;stacks+=(std::uint64_t(reward.count)+stack-1)/stack;}
    if(stacks>MaxItemCommitItems)return std::unexpected(E::Limit);return {};
}
std::expected<ItemCatalogDigest,E> ItemMissionDigest(const ItemMission& m)
{
    if(!Shape(m))return std::unexpected(E::Invalid);Writer w;w.Text("ITEM_MISSION_CONTRACT_1");w.Text(m.code);w.Put(m.version,4);w.Put(m.minimumLevel,2);
    w.Amounts(m.requirements);w.Amounts(m.costs);w.Amounts(m.rewards);w.Put(m.goldCost,8);w.Put(m.goldReward,8);return Hash(w);
}
std::expected<std::array<std::uint8_t,16>,E> ItemMissionOperation(ItemMissionProof p)
{
    if(p.owner.kind!=ItemOwnerKind::Character || !ValidItemOwner(p.owner) || std::ranges::all_of(p.action,[](auto b){return b==0;}))return std::unexpected(E::Invalid);
    Writer w;w.Text("ITEM_MISSION_ACTION_1");w.Put(p.owner.id,8);w.bytes.insert(w.bytes.end(),p.action.begin(),p.action.end());auto digest=Hash(w);if(!digest)return std::unexpected(digest.error());
    std::array<std::uint8_t,16> op{};std::ranges::copy(std::span(*digest).first<16>(),op.begin());if(std::ranges::all_of(op,[](auto b){return b==0;}))return std::unexpected(E::Unavailable);return op;
}
std::expected<std::vector<std::uint8_t>,E> ItemMissionReceipt(const ItemMission& m,ItemMissionProof p)
{
    const auto digest=ItemMissionDigest(m);if(!digest || !ItemMissionOperation(p))return std::unexpected(E::Invalid);
    Writer w;w.bytes={'I','M','Q',1};w.Put(static_cast<unsigned>(p.owner.kind),1);w.Put(p.owner.id,8);
    w.bytes.insert(w.bytes.end(),p.action.begin(),p.action.end());w.bytes.insert(w.bytes.end(),digest->begin(),digest->end());return w.bytes;
}
std::expected<ItemCommit,E> MakeItemMissionCommit(const ItemCatalog& c,const ItemMission& m,ItemMissionProof proof,std::uint16_t level,
    ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> inventory,
    std::span<const ItemWallet> wallets,std::span<const ItemInstanceId> ids)
{
    if(!ValidateItemMission(c,m) || proof.owner!=fence.owner || inventory.size()>MaxLoadedOwnerItems)return std::unexpected(E::Invalid);
    if(level<m.minimumLevel)return std::unexpected(E::Level);
    const auto op=ItemMissionOperation(proof);const auto receipt=ItemMissionReceipt(m,proof);if(!op || !receipt)return std::unexpected(E::Invalid);
    std::set<ItemInstanceId> unique;for(const auto& i:inventory)if(i.owner!=fence.owner || !unique.insert(i.id).second || !ValidatePersistentItem(c,i))return std::unexpected(E::Invalid);
    for(const auto* list:{&m.requirements,&m.costs})for(const auto& v:*list)
        if(CountMissionItems(inventory,fence.owner,v.key,v.level)<v.count)return std::unexpected(E::Requirements);
    const ItemWallet* gold=nullptr;for(const auto& w:wallets)if(w.owner==fence.owner && w.currency==WalletCurrency::Gold)
    {if(gold || !w.revision || w.revision==UINT64_MAX || w.balance>INT64_MAX)return std::unexpected(E::Invalid);gold=&w;}
    if((m.goldCost || m.goldReward) && !gold)return std::unexpected(E::Invalid);
    if(gold && gold->balance<m.goldCost)return std::unexpected(E::NoMoney);
    if(gold && m.goldReward>std::uint64_t(INT64_MAX)-(gold->balance-m.goldCost))return std::unexpected(E::Limit);
    std::vector<PersistentItem> working(inventory.begin(),inventory.end());std::vector<std::size_t> eligible;
    for(std::size_t i=0;i<working.size();++i)if(MissionItemUsable(working[i],fence.owner))eligible.push_back(i);
    std::ranges::sort(eligible,{},[&](auto i){const auto& v=working[i];return std::tuple{v.placement->storageId,v.placement->anchor,v.id};});
    for(const auto& cost:m.costs)
    {
        std::uint32_t left=cost.count;for(auto index:eligible)
        {auto& i=working[index];if(!left)break;if(i.key!=cost.key || i.refinementLevel!=cost.level)continue;
         const auto used=std::min(i.count,left);i.count-=used;left-=used;if(!i.count){i.destroyed=true;i.placement.reset();}}
        if(left)return std::unexpected(E::Requirements);
    }
    std::size_t usedIds=0;
    for(const auto& reward:m.rewards)
    {
        const auto grant=MakeGrantCommit(c,*op,fence,layouts,working,{reward.key,reward.level,reward.count},ids.subspan(usedIds));
        if(!grant)return std::unexpected(grant.error()==ItemPersistenceError::InvalidPlacement ? E::NoRoom : grant.error()==ItemPersistenceError::LimitExceeded ? E::Unavailable : E::Invalid);
        for(const auto& change:grant->items)
        {if(change.before)*std::ranges::find(working,change.after.id,&PersistentItem::id)=change.after;else{working.push_back(change.after);++usedIds;}}
    }
    ItemCommit out;out.operationId=*op;out.catalogRevision=c.Revision();out.catalogHash=c.ContentHash();out.kind=ItemOperationKind::Service;
    out.owners={fence};out.outcomeCode=ItemMissionOutcome;out.result=*receipt;
    for(std::size_t i=0;i<inventory.size();++i)if(working[i]!=inventory[i])
    {working[i].revision=inventory[i].revision+1;out.items.push_back({inventory[i],working[i]});}
    for(std::size_t i=inventory.size();i<working.size();++i){working[i].revision=1;out.items.push_back({{},working[i]});}
    if(gold && m.goldCost!=m.goldReward){auto after=*gold;after.balance=gold->balance-m.goldCost+m.goldReward;++after.revision;out.wallets.push_back({*gold,after});}
    if(out.items.size()>MaxItemCommitItems)return std::unexpected(E::Limit);
    for(const auto& change:out.items)for(const auto* i:{change.before ? &*change.before : nullptr,&change.after})if(i && i->placement)
    {const ItemStorage storage{i->owner,i->placement->kind,i->placement->storageId};if(std::ranges::find(out.storages,storage,&ItemStorageLayout::storage)!=out.storages.end())continue;
     const auto found=std::ranges::find(layouts,storage,&ItemStorageLayout::storage);if(found==layouts.end())return std::unexpected(E::Invalid);out.storages.push_back(*found);}
    if(!ValidateItemCommit(c,out))return std::unexpected(E::Invalid);return out;
}
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeItemMissionBoard(const ItemCatalog& c,const ItemMissionBoard* board,
    std::uint32_t npc,std::uint64_t session,std::uint64_t request,ItemMissionStatus status,std::uint32_t selected,std::span<const std::uint32_t> completed)
{
    if(static_cast<unsigned>(status)>static_cast<unsigned>(ItemMissionStatus::Level) ||
       (board ? (!npc || !session) : (npc || session || request || selected || status==ItemMissionStatus::Ready || status==ItemMissionStatus::Completed || status==ItemMissionStatus::AlreadyCompleted || status==ItemMissionStatus::Pending)))return std::unexpected(TypedInventoryWireError::BadValue);
    if(board && (status==ItemMissionStatus::Completed || status==ItemMissionStatus::AlreadyCompleted) &&
       (std::ranges::find(board->missions,selected,&ItemMission::id)==board->missions.end() || std::ranges::find(completed,selected)==completed.end()))return std::unexpected(TypedInventoryWireError::BadValue);
    Writer w;w.Put(232,1);w.Put(0,4);w.Put(1,1);w.Put(static_cast<unsigned>(status),1);w.Put(npc,4);w.Put(session,8);w.Put(request,8);w.Put(selected,4);
    w.Text(board ? board->name : "");w.Put(board ? board->missions.size() : 0,1);std::vector<ItemGrant> states;
    if(board)
    {
        if(!Text(board->name,128) || board->missions.size()>MaxItemMissions)return std::unexpected(TypedInventoryWireError::BadValue);
        for(const auto& m:board->missions)
        {
            if(!ValidateItemMission(c,m))return std::unexpected(TypedInventoryWireError::BadValue);
            w.Put(m.id,4);w.Put(m.version,4);w.Text(m.code);w.Text(m.name);w.Text(m.description);w.Put(m.minimumLevel,2);
            w.Put(std::ranges::find(completed,m.id)!=completed.end(),1);w.Put(m.goldCost,8);w.Put(m.goldReward,8);
            // UI preserves the authored list order; digest normalizes set order.
            for(const auto* list:{&m.requirements,&m.costs,&m.rewards})
            {w.Put(list->size(),1);for(const auto& v:*list){w.Key(v.key);w.Put(v.level,2);w.Put(v.count,4);states.push_back({v.key,v.level,1});}}
        }
    }
    if(states.empty())w.Put(0,4);
    else{const auto details=EncodeTypedItemDetails(c,{},false,states);if(!details)return std::unexpected(details.error());w.Put(details->size(),4);for(auto b:*details)w.bytes.push_back(std::to_integer<std::uint8_t>(b));}
    if(w.bytes.size()>(1u<<20))return std::unexpected(TypedInventoryWireError::BadValue);
    for(unsigned i=0;i<4;++i)w.bytes[1+i]=static_cast<std::uint8_t>(w.bytes.size()>>(8*i));
    std::vector<std::byte> out;for(auto b:w.bytes)out.push_back(std::byte(b));return out;
}
}
