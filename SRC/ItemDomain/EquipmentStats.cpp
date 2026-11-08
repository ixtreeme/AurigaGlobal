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
std::expected<FixedCompanionSource,E> CollectFixedCompanionBonuses(const ItemCatalog& c,const PersistentItem& seal,
    bool active,bool riding,bool inDungeon,std::uint64_t now,std::uint64_t elapsed)
{
    if(!ValidatePersistentItem(c,seal) || !seal.companion)return std::unexpected(E::InvalidItem);
    const auto& row=std::get<ItemCompanionRow>(c.Find(seal.key)->category);
    if(!active || !seal.companion->recallRequested || (row.kind==CompanionKind::Mount)!=riding || (row.dungeonOnlyBonus && !inDungeon))return FixedCompanionSource{};
    CompanionRequest request{seal.key};request.state=*seal.companion;request.state.phase=riding ? CompanionPhase::Riding : CompanionPhase::Following;
    request.state.activeFormId=riding ? *row.ridingForm : row.form;request.instanceId=seal.id.Value();request.revision=seal.revision;
    request.ownerId=request.actorId=seal.owner.id;request.rulesetRevision=1;request.operationId={1};request.count=seal.count;request.refinementLevel=seal.refinementLevel;
    request.action=CompanionAction::Maintain;request.nowUtcMs=now;request.elapsedOwnerMs=elapsed;request.elapsedWasActive=elapsed!=0;
    request.admittedByServer=request.ownerAlive=request.worldAllowsCompanion=true;request.worldAllowsRiding=riding;request.inDungeon=inDungeon;
    const auto plan=PlanCompanion(c,request);if(!plan)return std::unexpected(E::InvalidItem);if(!plan->bonusEnabled)return FixedCompanionSource{};
    FixedCompanionSource source{seal.id.Value(),row.kind,riding};std::map<BonusId,std::int64_t> values;
    const auto profiles=c.BonusProfilesAt(seal.key,seal.refinementLevel);if(!profiles)return std::unexpected(E::InvalidItem);
    for(const auto* profile:{profiles->base,profiles->refinement})if(profile)for(const auto& bonus:profile->Data().entries)
        if(!Add(values[bonus.bonus],bonus.valueScaled))return std::unexpected(E::Overflow);
    for(const auto& slot:seal.attributes.slots)if(slot && !Add(values[slot->bonus],slot->valueScaled))return std::unexpected(E::Overflow);
    for(const auto [id,value]:values)if(value)source.bonuses.push_back({id,value});return source;
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
        if(const auto* tool=std::get_if<ItemToolRow>(&d->category)){slot=1;out.tool=tool->kind;}
        if(const auto* costume=std::get_if<ItemCostumeRow>(&d->category)){slot=CostumeSlot(costume->kind);if(costume->kind==CostumeKind::Weapon && out.weapon!=costume->weapon)continue;}
        if(const auto* regalia=std::get_if<ItemRegaliaRow>(&d->category))slot=RegaliaSlot(*regalia);
        if(std::holds_alternative<ItemAmmoRow>(d->category))slot=20;
        if(!slot || slot>20 || item->placement->anchor!=slot-1)return std::unexpected(E::InvalidSlot);
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
            if(!ammo || !attack)return std::unexpected(E::UnsupportedState);
            out.weapon=weapon->subtype;out.reachMillimeters=attack->reachMillimeters;
        }
        const auto* appearance=d->base.appearanceProfile ? c.FindAppearance(*d->base.appearanceProfile) : nullptr;
        out.equipped.push_back({static_cast<std::uint8_t>(slot),item->id,item->key,item->refinementLevel,appearance ? appearance->modelAsset : std::string{}});
        if(const auto* regalia=std::get_if<ItemRegaliaRow>(&d->category)){
            bool enabled=true;
            if(regalia->kind==RegaliaKind::Rune && regalia->slot==7)for(unsigned rune=1;rune<7;++rune){const auto found=std::ranges::find_if(equipped,[&](const auto* p){const auto* r=std::get_if<ItemRegaliaRow>(&c.Find(p->key)->category);return r && r->kind==RegaliaKind::Rune && r->slot==rune;});if(found==equipped.end() || !(*found)->regalia->active || (*found)->regalia->remainingMs<((*found)->regalia->capacityMs/2+(*found)->regalia->capacityMs%2)){enabled=false;break;}}
            if(enabled)for(const auto& b:RegaliaContributions(*regalia,*item->regalia))if(!Add(out.bonuses[static_cast<unsigned>(b.bonus)],b.valueScaled))return std::unexpected(E::Overflow);
        }
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
    const auto nonzero=[](const auto& values){return std::ranges::any_of(values,[](auto v){return v!=0;});};
    const bool natural=s.petBonuses.seal && (nonzero(s.petBonuses.bonuses.age) || nonzero(s.petBonuses.bonuses.traits));
    const bool fixed=s.fixedCompanion.seal!=0;
    put(231,1);put(0,4);put(fixed ? 5 : s.petBonuses.seal ? natural ? 4 : 3 : s.tool ? 2 : 1,1);put(stats ? 0 : 1,1);put(vid,4);put(revision,8);put(inventoryRevision,8);
    put(s.weapon ? static_cast<unsigned>(*s.weapon) : 0,1);for(auto value:s.flat.Values())put(static_cast<std::uint64_t>(value),8);
    for(auto value:points)put(static_cast<std::uint64_t>(value),8);
    put(std::ranges::count_if(s.bonuses,[](auto value){return value!=0;}),1);
    for(unsigned id=1;id<s.bonuses.size();++id)if(s.bonuses[id]){put(id,4);put(static_cast<std::uint64_t>(s.bonuses[id]),8);}
    for(auto value:s.resistances)put(static_cast<std::uint64_t>(value),8);
    if(s.tool || s.petBonuses.seal || fixed)put(s.tool ? static_cast<unsigned>(*s.tool) : 0,1);
    if(s.petBonuses.seal || fixed)
    {
        put(s.petBonuses.seal,8);
        const auto effects=[&](const PetBonusValues& values) {
            put(std::ranges::count_if(values,[](auto value){return value!=0;}),1);
            for(unsigned effect=1;effect<values.size();++effect)if(values[effect])
            {put(effect,1);put(static_cast<std::uint64_t>(values[effect]),8);}
        };
        effects(s.petBonuses.bonuses.total);
        if(natural || fixed)
        {
            put(s.petBonuses.bonuses.ageTierMinimumMs,8);
            effects(s.petBonuses.bonuses.age);effects(s.petBonuses.bonuses.traits);effects(s.petBonuses.bonuses.skills);
        }
    }
    if(fixed) {
        put(s.fixedCompanion.seal,8);put(static_cast<unsigned>(s.fixedCompanion.kind),1);put(s.fixedCompanion.riding,1);
        put(s.fixedCompanion.bonuses.size(),1);
        for(const auto& bonus:s.fixedCompanion.bonuses){put(static_cast<unsigned>(bonus.bonus),4);put(static_cast<std::uint64_t>(bonus.valueScaled),8);}
    }
    for(unsigned i=0;i<4;++i)bytes[1+i]=std::byte((bytes.size()>>(8*i))&255);return bytes;
}
}
