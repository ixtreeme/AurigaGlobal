#include <item/TypedItemDetails.hpp>
#include <item/ItemWireCodec.hpp>
#include <item/BonusRegistry.hpp>
#include <limits>
#include <set>
#include <stdexcept>

namespace auriga::item
{
namespace
{
struct TooLarge {};
struct Writer
{
    std::vector<std::byte> bytes;
    void Put(std::uint64_t value, unsigned count)
    {
        if(bytes.size()+count > (1u<<20)) throw TooLarge{};
        for(unsigned i=0;i<count;++i) bytes.push_back(std::byte((value>>(8*i))&255));
    }
    void Text(std::string_view text)
    {
        if(text.size()>65535) throw TooLarge{};
        Put(text.size(),2); for(unsigned char c:text) Put(c,1);
    }
    void Key(ItemKey key) { for(auto b:EncodeItemKey(key)) Put(std::to_integer<unsigned char>(b),1); }
};
}
std::expected<std::vector<std::byte>, TypedInventoryWireError> EncodeTypedItemDetails(
    const ItemCatalog& c,std::span<const PersistentItem> items,bool ownedDropEnabled,std::span<const ItemGrant> extraStates)
{
    try
    {
        std::set<ItemKey> keys;
        std::set<std::pair<ItemKey,std::uint16_t>> states;
        std::map<std::uint32_t,const RefinementRecipe*> recipes;
        std::set<std::pair<ItemKey,std::uint16_t>> sources;
        std::set<ItemKey> refinable;
        for(const auto& item:items)
        {
            keys.insert(item.key); states.emplace(item.key,item.refinementLevel);
            for(const auto& stone:item.stones) if(stone.stone)
            { keys.insert(*stone.stone); states.emplace(*stone.stone,stone.refinementLevel); }
            sources.emplace(item.key,item.refinementLevel);
        }
        for(const auto& extra:extraStates)
        { keys.insert(extra.key); states.emplace(extra.key,extra.refinementLevel); sources.emplace(extra.key,extra.refinementLevel); }
        for(const auto& [key,level]:sources)
            for(const auto* r:c.RefinementRecipesFrom(key,level))
            {
                refinable.insert(key); recipes.emplace(r->id,r); keys.insert(r->target); states.emplace(r->target,r->targetLevel);
                for(const auto& ingredient:r->ingredients)
                { keys.insert(ingredient.key); states.emplace(ingredient.key,ingredient.refinementLevel); }
            }
        if(keys.size()>65535 || states.size()>65535 || recipes.size()>65535) throw TooLarge{};
        Writer w; w.Put(TypedItemDetailsHeader,1); w.Put(0,4); w.Put(1,1); w.Put(c.Revision(),8);
        for(auto b:c.ContentHash()) w.Put(b,1);
        w.Put(keys.size(),2);
        for(auto key:keys)
        {
            const auto* entry=c.Find(key);
            if(!entry) return std::unexpected(TypedInventoryWireError::MissingCatalogItem);
            const auto& b=entry->base;
            const auto* use=c.FindUsePolicy(b.usePolicy);
            const auto* trade=c.FindTradePolicy(b.tradePolicy);
            const auto* appearance=b.appearanceProfile ? c.FindAppearance(*b.appearanceProfile) : nullptr;
            unsigned slot=0,actions=TypedActionMove;
            std::visit([&](const auto& row) {
                if constexpr(requires { row.slot; }) slot=static_cast<unsigned>(row.slot);
                if constexpr(std::is_same_v<std::decay_t<decltype(row)>,ItemConsumableRow>)
                {
                    const auto* recovery=c.FindRecovery(row.recoveryProfile);
                    if(recovery && recovery->Data().Delivery()==RecoveryDelivery::Instant) actions|=TypedActionUse;
                }
            },entry->category);
            if(const auto* box=std::get_if<ItemContainerRow>(&entry->category);box && (box->kind==ContainerKind::Use || box->kind==ContainerKind::Locked))actions|=TypedActionOpen;
            if(std::holds_alternative<ItemCurrencyTokenRow>(entry->category))actions|=TypedActionRedeem;
            if(slot && slot<=8) actions|=TypedActionEquip;
            if(refinable.contains(key)) actions|=TypedActionRefine;
            if(ownedDropEnabled && trade && trade->allowDrop && trade->allowTrade) actions|=TypedActionDrop;
            if(b.retired) actions=0;
            w.Key(key); w.Text(b.contentCode); w.Text(b.nameKey); w.Text(b.descriptionKey);
            w.Text(b.iconAsset); w.Text(appearance ? appearance->modelAsset : "");
            w.Put(b.width,1); w.Put(b.height,1); w.Put(b.minimumLevel,2);
            w.Put(use ? use->allowedClasses : 0,1); w.Put(use ? use->allowedSexes : 0,1);
            w.Put(slot,1); w.Put(actions,1);
        }
        w.Put(states.size(),2);
        for(const auto& [key,level]:states)
        {
            const auto stats=c.RefinedBaseStats(key,level);
            const auto profiles=c.BonusProfilesAt(key,level);
            if(!stats || !profiles) return std::unexpected(TypedInventoryWireError::BadValue);
            w.Key(key); w.Put(level,2);
            for(auto stat:stats->Values()) w.Put(static_cast<std::uint64_t>(stat),8);
            std::map<BonusId,std::int64_t> bonuses;
            for(const auto* profile:{profiles->base,profiles->refinement}) if(profile)
                for(const auto& bonus:profile->Data().entries) bonuses[bonus.bonus]+=bonus.valueScaled;
            if(bonuses.size()>255) throw TooLarge{};
            w.Put(bonuses.size(),1);
            for(const auto& [id,value]:bonuses) { w.Put(static_cast<std::uint32_t>(id),4); w.Put(static_cast<std::uint64_t>(value),8); }
        }
        w.Put(recipes.size(),2);
        for(const auto& [id,r]:recipes)
        {
            w.Put(id,4); w.Key(r->source); w.Put(r->sourceLevel,2); w.Key(r->target); w.Put(r->targetLevel,2);
            w.Put(r->goldCost,8); w.Put(r->successChanceBp,2); w.Put(static_cast<unsigned>(r->failure),1);
            w.Put(r->failureLevel.value_or(65535),2);
            if(r->ingredients.size()>255) throw TooLarge{};
            w.Put(r->ingredients.size(),1);
            for(const auto& ingredient:r->ingredients) { w.Key(ingredient.key); w.Put(ingredient.refinementLevel,2); w.Put(ingredient.count,4); }
        }
        w.Put(BonusDefinitions.size(),1);
        for(const auto& bonus:BonusDefinitions) { w.Put(bonus.id,4); w.Put(static_cast<unsigned>(bonus.unit),1); w.Text(bonus.displayKey); }
        for(unsigned i=0;i<4;++i) w.bytes[1+i]=std::byte((w.bytes.size()>>(i*8))&255);
        return std::move(w.bytes);
    }
    catch(const TooLarge&) { return std::unexpected(TypedInventoryWireError::TooLarge); }
}
}
