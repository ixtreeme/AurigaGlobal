#include <item/Crafting.hpp>
#include <item/ItemWireCodec.hpp>
#include <algorithm>
#include <set>
#include <tuple>

namespace auriga::item
{
namespace
{
using E=CraftError;
bool Name(std::string_view value)
{return !value.empty() && value.size()<=128 && std::ranges::none_of(value,[](unsigned char c){return c<32 || c==127;});}
bool RecipeValid(const ItemCatalog& c,const CraftRecipe& recipe)
{
    const auto* d=c.Find(recipe.result.key);
    if(!recipe.id || !Name(recipe.name) || !d || d->base.retired || !SupportsPersistentItem(c,*d) ||
       !c.RefinedBaseStats(recipe.result.key,recipe.result.refinementLevel) || !recipe.result.count || recipe.result.count>d->base.maxStack ||
       recipe.gold>std::uint64_t(INT64_MAX)/MaxCraftBatch || recipe.chance>10000 ||
       recipe.ingredients.empty() || recipe.ingredients.size()>MaxCraftIngredients)return false;
    std::set<std::pair<ItemKey,std::uint16_t>> unique;
    for(const auto& i:recipe.ingredients)
    {
        const auto* material=c.Find(i.key);
        if(!i.count || i.count>UINT32_MAX/MaxCraftBatch || !material || !SupportsPersistentItem(c,*material) ||
           !c.RefinedBaseStats(i.key,i.level) || !unique.emplace(i.key,i.level).second)return false;
    }
    return true;
}
}
bool CraftIngredientUsable(const PersistentItem& item,ItemOwner owner)
{
    return item.owner==owner && !item.destroyed && item.placement && item.placement->kind==ItemStorageKind::Inventory &&
        !item.boundTo && item.attributes==InstanceAttributes{} && item.stones==decltype(item.stones){} &&
        (!item.accessory || *item.accessory==AccessorySocketState{});
}
std::expected<void,E> ValidateCraftWorkshops(const ItemCatalog& c,std::span<const CraftWorkshop> workshops)
{
    if(workshops.size()>MaxCraftWorkshops)return std::unexpected(E::Invalid);
    std::set<std::uint32_t> races;
    for(const auto& shop:workshops)
    {
        if(!shop.npcRace || !races.insert(shop.npcRace).second || !Name(shop.name) || shop.recipes.empty() || shop.recipes.size()>MaxCraftRecipes)return std::unexpected(E::Invalid);
        std::set<std::uint32_t> recipes;
        for(const auto& recipe:shop.recipes)if(!recipes.insert(recipe.id).second || !RecipeValid(c,recipe))return std::unexpected(E::Invalid);
        if(!EncodeCraftWorkshop(c,&shop,1,1,0,CraftStatus::Ready))return std::unexpected(E::Invalid);
    }
    return {};
}
std::expected<CraftPlan,E> MakeCraftCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> inventory,std::span<const ItemWallet> wallets,
    const CraftRecipe& recipe,std::uint32_t attempts,std::span<const std::uint16_t> rolls,std::span<const ItemInstanceId> ids)
{
    if(fence.owner.kind!=ItemOwnerKind::Character || !RecipeValid(c,recipe) || !attempts || attempts>MaxCraftBatch ||
       rolls.size()!=attempts || std::ranges::any_of(rolls,[](auto roll){return roll>=10000;}) || inventory.size()>MaxLoadedOwnerItems)return std::unexpected(E::Invalid);
    const ItemWallet* wallet=nullptr;
    for(const auto& w:wallets)if(w.owner==fence.owner && w.currency==WalletCurrency::Gold)
    {if(wallet || !w.revision || w.revision==UINT64_MAX || w.balance>INT64_MAX)return std::unexpected(E::Invalid);wallet=&w;}
    if(!wallet)return std::unexpected(E::Invalid);
    const auto cost=recipe.gold*attempts;if(cost>wallet->balance)return std::unexpected(E::NoMoney);
    auto working=std::vector<PersistentItem>(inventory.begin(),inventory.end());std::set<ItemInstanceId> unique;
    for(const auto& item:working)if(item.owner!=fence.owner || !unique.insert(item.id).second || !ValidatePersistentItem(c,item))return std::unexpected(E::Invalid);
    std::vector<PersistentItem*> eligible;
    for(auto& item:working)if(CraftIngredientUsable(item,fence.owner))eligible.push_back(&item);
    std::ranges::sort(eligible,{},[](const auto* item){return std::tuple{item->placement->storageId,item->placement->anchor,item->id};});
    for(const auto& ingredient:recipe.ingredients)
    {
        std::uint64_t left=std::uint64_t{ingredient.count}*attempts;
        for(auto* item:eligible)
        {
            if(!left)break;if(item->key!=ingredient.key || item->refinementLevel!=ingredient.level)continue;
            const auto consume=static_cast<std::uint32_t>(std::min(left,std::uint64_t{item->count}));
            item->count-=consume;left-=consume;
            if(!item->count){item->destroyed=true;item->placement.reset();}
        }
        if(left)return std::unexpected(E::NoMaterials);
    }
    CraftPlan plan;plan.successes=static_cast<std::uint32_t>(std::ranges::count_if(rolls,[&](auto roll){return roll<recipe.chance;}));
    std::vector<ItemChange> produced;
    if(plan.successes)
    {
        auto grant=recipe.result;const auto units=std::uint64_t{grant.count}*plan.successes;
        if(units>UINT32_MAX)return std::unexpected(E::Limit);grant.count=static_cast<std::uint32_t>(units);
        const auto stack=c.Find(grant.key)->base.maxStack;
        if((units+stack-1)/stack>MaxItemCommitItems)return std::unexpected(E::Limit);
        const auto result=MakeGrantCommit(c,op,fence,layouts,working,grant,ids);
        if(!result)return std::unexpected(result.error()==ItemPersistenceError::InvalidPlacement ? E::NoRoom : result.error()==ItemPersistenceError::LimitExceeded ? E::Unavailable : E::Invalid);
        produced=result->items;
        for(const auto& change:produced)if(change.before)
        {auto original=std::ranges::find(working,change.after.id,&PersistentItem::id);*original=change.after;}
    }
    auto& out=plan.commit;out.operationId=op;out.catalogRevision=c.Revision();out.catalogHash=c.ContentHash();out.kind=ItemOperationKind::Service;out.owners={fence};out.outcomeCode=plan.successes ? 1 : 2;
    for(std::size_t i=0;i<inventory.size();++i)if(working[i]!=inventory[i])
    {working[i].revision=inventory[i].revision+1;out.items.push_back({inventory[i],working[i]});}
    for(const auto& item:produced)if(!item.before)out.items.push_back(item);
    if(out.items.size()>MaxItemCommitItems)return std::unexpected(E::Limit);
    if(cost){auto after=*wallet;after.balance-=cost;++after.revision;out.wallets.push_back({*wallet,after});}
    for(const auto& change:out.items)for(const auto* item:{change.before ? &*change.before : nullptr,&change.after})if(item && item->placement)
    {
        const ItemStorage storage{item->owner,item->placement->kind,item->placement->storageId};
        if(std::ranges::find(out.storages,storage,&ItemStorageLayout::storage)!=out.storages.end())continue;
        const auto found=std::ranges::find(layouts,storage,&ItemStorageLayout::storage);if(found==layouts.end())return std::unexpected(E::Invalid);out.storages.push_back(*found);
    }
    out.result={'C','R','F',1};const auto put=[&](std::uint64_t value,unsigned n){for(unsigned i=0;i<n;++i)out.result.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    put(recipe.id,4);put(attempts,4);put(plan.successes,4);put(cost,8);for(auto roll:rolls)put(roll,2);
    if(!ValidateItemCommit(c,out))return std::unexpected(E::Invalid);return plan;
}
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeCraftWorkshop(const ItemCatalog& c,const CraftWorkshop* workshop,
    std::uint32_t npc,std::uint64_t session,std::uint64_t request,CraftStatus status,std::uint32_t recipe,std::uint32_t attempts,std::uint32_t successes)
{
    std::vector<std::byte> out;const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)out.push_back(std::byte((v>>(8*i))&255));};
    const auto text=[&](std::string_view value){put(value.size(),2);for(unsigned char b:value)put(b,1);};
    put(230,1);put(0,4);put(1,1);put(static_cast<unsigned>(status),1);put(npc,4);put(session,8);put(request,8);put(recipe,4);put(attempts,4);put(successes,4);
    text(workshop ? std::string_view(workshop->name) : std::string_view{});put(workshop ? workshop->recipes.size() : 0,2);
    std::vector<ItemGrant> extra;
    if(workshop)for(const auto& r:workshop->recipes)
    {
        put(r.id,4);text(r.name);for(auto b:EncodeItemKey(r.result.key))put(std::to_integer<unsigned char>(b),1);
        put(r.result.refinementLevel,2);put(r.result.count,4);put(r.gold,8);put(r.chance,2);put(r.ingredients.size(),1);extra.push_back(r.result);
        for(const auto& i:r.ingredients){for(auto b:EncodeItemKey(i.key))put(std::to_integer<unsigned char>(b),1);put(i.level,2);put(i.count,4);extra.push_back({i.key,i.level,i.count});}
    }
    const auto details=workshop ? EncodeTypedItemDetails(c,{},false,extra) : std::expected<std::vector<std::byte>,TypedInventoryWireError>{std::vector<std::byte>{}};
    if(!details)return std::unexpected(details.error());put(details->size(),4);out.insert(out.end(),details->begin(),details->end());
    if(out.size()>(1u<<20))return std::unexpected(TypedInventoryWireError::TooLarge);
    for(unsigned i=0;i<4;++i)out[1+i]=std::byte((out.size()>>(8*i))&255);return out;
}
}
