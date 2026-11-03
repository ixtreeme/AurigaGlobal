#include <item/EquipmentStats.hpp>
#include <algorithm>
#include <limits>
#include <set>

namespace auriga::item
{
namespace
{
using E=EquipmentStatsError;
bool Add(std::int64_t& to,std::int64_t value)
{
    if((value>0 && to>INT64_MAX-value) || (value<0 && to<INT64_MIN-value))return false;
    to+=value;return true;
}
}
std::expected<std::int64_t,E> AddEquipmentValue(std::int64_t a,std::int64_t b)
{if(!Add(a,b))return std::unexpected(E::Overflow);return a;}
std::expected<std::int64_t,E> ScaleEquipmentValue(std::int64_t value,std::int64_t numerator,std::int64_t denominator)
{
    if(denominator<=0 || numerator<0 || value==INT64_MIN)return std::unexpected(E::Overflow);
    const bool negative=value<0;const auto n=static_cast<std::uint64_t>(negative ? -value : value);
    const auto factor=static_cast<std::uint64_t>(numerator),div=static_cast<std::uint64_t>(denominator);
    const auto q=n/div,r=n%div;
    if((q && factor>std::uint64_t(INT64_MAX)/q) || (r && factor>UINT64_MAX/r))return std::unexpected(E::Overflow);
    const auto whole=q*factor,part=r*factor;
    const auto fraction=part/div+(part%div>=div/2+div%2 ? 1 : 0);
    if(fraction>std::uint64_t(INT64_MAX)-whole)return std::unexpected(E::Overflow);
    const auto result=static_cast<std::int64_t>(whole+fraction);return negative ? -result : result;
}
std::expected<EquipmentStats,E> BuildEquipmentStats(const ItemCatalog& c,ItemOwner owner,std::span<const PersistentItem> inventory,std::span<const BonusEntry> effects)
{
    EquipmentStats out;std::set<ItemInstanceId> ids;std::set<unsigned> slots;
    std::set<RecoveryPool> automaticPools;
    for(const auto& item:inventory)if(!item.destroyed && item.automatic && item.automatic->active)
    {
        if(item.owner!=owner || !ValidatePersistentItem(c,item))return std::unexpected(E::InvalidItem);
        const auto& row=std::get<ItemConsumableRow>(c.Find(item.key)->category);const auto& a=std::get<AutomaticRecovery>(c.FindRecovery(row.recoveryProfile)->Data().effect);
        if(!automaticPools.insert(a.pool).second)return std::unexpected(E::Duplicate);
        (a.pool==RecoveryPool::Hp ? out.automaticHpBonusBp : out.automaticSpBonusBp)=a.activeMaxPoolBonusBp;
    }
    std::vector<const PersistentItem*> equipped;
    for(const auto& item:inventory)
    {
        if(item.destroyed || !item.placement || item.placement->kind!=ItemStorageKind::Equipment)continue;
        if(item.owner!=owner || !ValidatePersistentItem(c,item) || !ids.insert(item.id).second)return std::unexpected(E::InvalidItem);
        if(!slots.insert(item.placement->anchor).second)return std::unexpected(E::Duplicate);
        equipped.push_back(&item);
    }
    std::ranges::sort(equipped,{},[](const auto* item){return item->placement->anchor;});
    const auto profiles=[&](ItemKey key,std::uint16_t level,const AccessorySocketGrade* grade)->bool {
        const auto p=c.BonusProfilesAt(key,level);if(!p)return false;
        std::map<BonusId,std::int64_t> combined;
        for(const auto* profile:{p->base,p->refinement})if(profile)for(const auto& b:profile->Data().entries)
            if(!FindBonusDefinition(b.bonus) || !Add(combined[b.bonus],b.valueScaled))return false;
        for(auto [id,value]:combined)
        {
            if(grade && value)
            {
                if(value==INT64_MIN)return false;
                const auto* definition=FindBonusDefinition(id);const auto magnitude=value<0 ? -value : value;
                const auto delta=ScaleEquipmentValue(magnitude,grade->bonusIncreaseBp);if(!delta)return false;
                const std::int64_t minimum=definition->unit==BonusUnit::Raw ? grade->minimumRaw : definition->unit==BonusUnit::BasisPoints ? grade->minimumBp : grade->minimumInteger;
                const auto boosted=AddEquipmentValue(magnitude,std::max(*delta,minimum));if(!boosted)return false;
                value=value<0 ? -*boosted : *boosted;
            }
            if(!Add(out.bonuses[static_cast<unsigned>(id)],value))return false;
        }
        return true;
    };
    const auto base=[&](ItemKey key,std::uint16_t level,const AccessorySocketGrade* grade=nullptr)->bool {
        const auto s=c.RefinedBaseStats(key,level);return s && Add(out.flat.physicalAttackRaw,s->physicalAttackRaw) && Add(out.flat.magicalAttackRaw,s->magicalAttackRaw) &&
            Add(out.flat.physicalDefenseRaw,s->physicalDefenseRaw) && Add(out.flat.magicalDefenseRaw,s->magicalDefenseRaw) && profiles(key,level,grade);
    };
    for(const auto* item:equipped)
    {
        const auto* d=c.Find(item->key);if(!d)return std::unexpected(E::InvalidItem);
        unsigned slot=0;std::uint32_t resistance=0;
        std::visit([&](const auto& row){if constexpr(requires{row.slot;})slot=static_cast<unsigned>(row.slot);if constexpr(requires{row.resistanceProfile;})resistance=row.resistanceProfile;},d->category);
        if(!slot || slot>8 || item->placement->anchor!=slot-1)return std::unexpected(E::InvalidSlot);
        const AccessorySocketGrade* grade=nullptr;
        if(item->accessory && item->accessory->activeGrade)
        {
            const auto* accessory=std::get_if<ItemAccessoryRow>(&d->category);
            const auto* profile=accessory && accessory->socketProfile ? c.FindAccessorySocket(*accessory->socketProfile) : nullptr;
            if(!profile || !(grade=profile->FindGrade(item->accessory->activeGrade)))return std::unexpected(E::InvalidItem);
        }
        if(const auto* weapon=std::get_if<ItemWeaponRow>(&d->category))
        {
            const auto* ammo=c.FindAmmoPolicy(weapon->ammoPolicy);const auto* attack=c.FindAttack(weapon->attackProfile);
            if(!ammo || ammo->kind!=AmmoKind::None || !attack)return std::unexpected(E::UnsupportedState);
            out.weapon=weapon->subtype;out.reachMillimeters=attack->reachMillimeters;
        }
        const auto* appearance=d->base.appearanceProfile ? c.FindAppearance(*d->base.appearanceProfile) : nullptr;
        out.equipped.push_back({static_cast<std::uint8_t>(slot),item->id,item->key,item->refinementLevel,appearance ? appearance->modelAsset : std::string{}});
        if(!base(item->key,item->refinementLevel,grade))return std::unexpected(E::Overflow);
        for(const auto& entry:item->attributes.slots)if(entry && !Add(out.bonuses[static_cast<unsigned>(entry->bonus)],entry->valueScaled))return std::unexpected(E::Overflow);
        for(const auto& stone:item->stones)if(stone.state==StoneSocketState::Occupied)
            if(!stone.stone || !base(*stone.stone,stone.refinementLevel))return std::unexpected(E::InvalidItem);
        if(resistance)
        {
            const auto* r=c.FindResistance(resistance);if(!r)return std::unexpected(E::InvalidItem);const auto values=r->Values();
            for(std::size_t i=0;i<values.size();++i)if(!Add(out.resistances[i],values[i]))return std::unexpected(E::Overflow);
        }
    }
    for(const auto& effect:effects)if(!FindBonusDefinition(effect.bonus) || !Add(out.bonuses[static_cast<unsigned>(effect.bonus)],effect.valueScaled))return std::unexpected(E::Overflow);
    if(!Add(out.flat.physicalAttackRaw,out.Bonus(BonusId::PhysicalAttackFlat)) || !Add(out.flat.magicalAttackRaw,out.Bonus(BonusId::MagicalAttackFlat)) ||
       !Add(out.flat.physicalDefenseRaw,out.Bonus(BonusId::PhysicalDefenseFlat)) || !Add(out.flat.magicalDefenseRaw,out.Bonus(BonusId::MagicalDefenseFlat)))return std::unexpected(E::Overflow);
    for(const auto [flat,bp]:{std::pair{out.flat.physicalAttackRaw,out.Bonus(BonusId::PhysicalAttackPercent)},std::pair{out.flat.magicalAttackRaw,out.Bonus(BonusId::MagicalAttackPercent)}})
    {
        const auto maximum=AddEquipmentValue(std::max<std::int64_t>(0,flat),std::int64_t(INT32_MAX)*1000);
        if(!maximum || !ScaleEquipmentValue(*maximum,std::max<std::int64_t>(0,10000+bp)))return std::unexpected(E::Overflow);
    }
    return out;
}
std::vector<std::byte> EncodeEquipmentStats(const EquipmentStats* stats,std::uint32_t vid,std::uint64_t revision,
    std::uint64_t inventoryRevision,const std::array<std::int64_t,13>& points)
{
    const EquipmentStats empty;const auto& s=stats ? *stats : empty;
    std::vector<std::byte> bytes;const auto put=[&](std::uint64_t value,unsigned size){for(unsigned i=0;i<size;++i)bytes.push_back(std::byte((value>>(8*i))&255));};
    put(231,1);put(0,4);put(1,1);put(stats ? 0 : 1,1);put(vid,4);put(revision,8);put(inventoryRevision,8);
    put(s.weapon ? static_cast<unsigned>(*s.weapon) : 0,1);for(auto value:s.flat.Values())put(static_cast<std::uint64_t>(value),8);
    for(auto value:points)put(static_cast<std::uint64_t>(value),8);
    put(std::ranges::count_if(s.bonuses,[](auto value){return value!=0;}),1);
    for(unsigned id=1;id<s.bonuses.size();++id)if(s.bonuses[id]){put(id,4);put(static_cast<std::uint64_t>(s.bonuses[id]),8);}
    for(auto value:s.resistances)put(static_cast<std::uint64_t>(value),8);
    for(unsigned i=0;i<4;++i)bytes[1+i]=std::byte((bytes.size()>>(8*i))&255);return bytes;
}
}
