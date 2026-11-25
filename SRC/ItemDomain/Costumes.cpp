#include <item/Costumes.hpp>
#include <item/ItemCatalog.hpp>
#include <limits>

namespace auriga::item
{
std::optional<std::vector<std::uint8_t>> EncodeCostumeAppearance(const CostumeAppearance& p,std::uint8_t version)
{
    if(version<1 || version>5 || !p.vid || !p.revision || p.weapon>7 || p.tool>4 || (p.weapon && p.tool) || p.runeMode>2 || (!p.weapon && (!p.models[4].empty() || p.weaponEffect)))return {};
    std::vector<std::uint8_t> b;
    const auto put=[&](std::uint64_t value,unsigned size){for(unsigned n=0;n<size;++n)b.push_back(static_cast<std::uint8_t>(value>>(8*n)));};
    put(241,1);put(0,4);put(version,1);put(p.vid,4);put(p.revision,8);put(p.weapon,1);put(p.tool,1);
    for(unsigned n=0;n<(version==1 ? 5u : 6u);++n) {
        const auto& text=p.models[n];
        if(text.size()>512 || text.starts_with('/') || text.find("..")!=text.npos || text.find(':')!=text.npos || text.find('\\')!=text.npos || text.find('\0')!=text.npos)return {};
        put(text.size(),2);b.insert(b.end(),text.begin(),text.end());
    }
    if(version>=2)put(p.runeMode,1);if(version>=3)put(p.bodyEffect,2);if(version>=4)put(p.weaponEffect,2);
    if(version>=5){const auto& text=p.absorptionSash;if(text.size()>512 || text.starts_with('/') || text.find("..")!=text.npos || text.find(':')!=text.npos || text.find('\\')!=text.npos || text.find('\0')!=text.npos)return {};put(text.size(),2);b.insert(b.end(),text.begin(),text.end());}
    for(unsigned n=0;n<4;++n)b[1+n]=static_cast<std::uint8_t>(b.size()>>(8*n));
    return b;
}
namespace
{
constexpr std::uint64_t MaxTime=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
bool ValidClock(CostumeClockDefinition definition)
{
    return definition.clock>=CostumeClock::Permanent && definition.clock<=CostumeClock::EquippedTime &&
        definition.durationMs<=MaxTime && (definition.clock==CostumeClock::Permanent ? !definition.durationMs : definition.durationMs>0);
}
bool ValidState(CostumeClockDefinition definition,const CostumeLifetimeState& state,bool equipped)
{
    if(state.deadlineUtcMs>MaxTime || state.remainingMs>MaxTime)return false;
    if(definition.clock==CostumeClock::Permanent)
        return !state.started && !state.expired && !state.deadlineUtcMs && !state.remainingMs;
    if(definition.clock==CostumeClock::EquippedTime)
        // Remaining time is instance-owned. A shorter new prototype duration
        // does not erase or refill an already issued instance's budget.
        return state.started && !state.deadlineUtcMs && (!state.expired || !state.remainingMs);
    if(!state.started)
        return definition.clock==CostumeClock::SinceFirstUse && !equipped && !state.deadlineUtcMs && !state.remainingMs && !state.expired;
    return state.deadlineUtcMs && !state.remainingMs;
}
}
std::optional<CostumeClockDefinition> FindWearableClock(const ItemCatalog& catalog,ItemKey key)
{
    const auto* entry=catalog.Find(key);if(!entry)return {};
    if(const auto* row=std::get_if<ItemCostumeRow>(&entry->category))return CostumeClockDefinition{row->clock,row->durationMs};
    const auto* accessory=std::get_if<ItemAccessoryRow>(&entry->category);
    const auto* regalia=std::get_if<ItemRegaliaRow>(&entry->category);
    if((regalia && regalia->kind==RegaliaKind::Stole) ||
        (accessory && (accessory->slot==EquipmentSlot::Unique || accessory->slot==EquipmentSlot::UniqueAlternate))){
        const auto* rule=catalog.FindDefinitionRules(key);
        return CostumeClockDefinition{rule ? rule->clock : CostumeClock::Permanent,rule ? rule->durationMs : 0};
    }
    return {};
}
std::optional<CostumeClockDefinition> FindTimedQuestPetSealClock(const ItemCatalog& catalog,ItemKey key)
{
    const auto* entry=key.Category()==ItemCategory::Quest ? catalog.Find(key) : nullptr;
    const auto* quest=entry ? std::get_if<ItemQuestRow>(&entry->category) : nullptr;
    if(!quest || quest->dispatch!=QuestItemDispatch::FirstHandler || entry->base.maxStack!=1 ||
       !quest->groupCode.starts_with("companion.timed."))return {};
    const auto* rule=catalog.FindDefinitionRules(key,0);
    if(!rule || (rule->clock!=CostumeClock::SinceCreation && rule->clock!=CostumeClock::SinceFirstUse &&
       !(quest->groupCode==PetChoiceGroup && rule->clock==CostumeClock::EquippedTime)) ||
       !rule->durationMs)return {};
    return CostumeClockDefinition{rule->clock,rule->durationMs};
}
std::optional<CostumeClockDefinition> FindPersistentItemClock(const ItemCatalog& catalog,ItemKey key)
{
    return key.Category()==ItemCategory::Quest ? FindTimedQuestPetSealClock(catalog,key) : FindWearableClock(catalog,key);
}
bool ValidItemClock(CostumeClockDefinition definition,const CostumeLifetimeState& state,bool equipped) noexcept
{
    return ValidClock(definition) && ValidState(definition,state,equipped) &&
        (definition.clock!=CostumeClock::EquippedTime || state.expired==!state.remainingMs);
}
bool ValidCostume(const ItemCostumeRow& row) noexcept
{
    if(row.key.Category()!=ItemCategory::Costume || row.kind<CostumeKind::Body || row.kind>CostumeKind::WeaponEffect)return false;
    if(row.kind==CostumeKind::Weapon)
    {if(!row.weapon || *row.weapon<WeaponSubtype::Sword || *row.weapon>WeaponSubtype::MountSpear)return false;}
    else if(row.weapon)return false;
    if((row.kind==CostumeKind::PetSkin || row.kind==CostumeKind::MountSkin) ? (!row.companionForm || !*row.companionForm) : row.companionForm.has_value())return false;
    if((row.kind==CostumeKind::BodyEffect)!=(row.bodyEffect!=0))return false;
    for(const auto effect:row.weaponEffects)if((row.kind==CostumeKind::WeaponEffect)!=(effect!=0))return false;
    if(row.clock<CostumeClock::Permanent || row.clock>CostumeClock::EquippedTime || row.durationMs>MaxTime)return false;
    return row.clock==CostumeClock::Permanent ? row.durationMs==0 : row.durationMs>0;
}
bool ValidCostumeLifetime(const ItemCostumeRow& row,const CostumeLifetimeState& state,bool equipped) noexcept
{return ValidCostume(row) && ValidState({row.clock,row.durationMs},state,equipped) && (row.clock!=CostumeClock::EquippedTime || state.expired==!state.remainingMs);}
std::array<std::uint8_t,22> EncodeCostumeLifetime(const CostumeLifetimeState& t)
{
    std::array<std::uint8_t,22> b{'C','T','L',1,static_cast<std::uint8_t>(t.started),static_cast<std::uint8_t>(t.expired)};
    for(unsigned n=0;n<8;++n){b[6+n]=static_cast<std::uint8_t>(t.deadlineUtcMs>>(8*n));b[14+n]=static_cast<std::uint8_t>(t.remainingMs>>(8*n));}return b;
}
std::optional<CostumeLifetimeState> DecodeCostumeLifetime(std::span<const std::uint8_t> b)
{
    if(b.size()!=22 || b[0]!='C' || b[1]!='T' || b[2]!='L' || b[3]!=1 || b[4]>1 || b[5]>1)return {};
    CostumeLifetimeState t{b[4]!=0,b[5]!=0};
    for(unsigned n=0;n<8;++n){t.deadlineUtcMs|=std::uint64_t{b[6+n]}<<(8*n);t.remainingMs|=std::uint64_t{b[14+n]}<<(8*n);}
    if(t.deadlineUtcMs>MaxTime || t.remainingMs>MaxTime)return {};return t;
}
std::expected<CostumeLifetimeState,CostumeLifetimeError> MakeCostumeLifetime(
    const ItemCostumeRow& definition,std::uint64_t created)
{
    if(!ValidCostume(definition))return std::unexpected(CostumeLifetimeError::InvalidDefinition);
    if(created>MaxTime)return std::unexpected(CostumeLifetimeError::TimeOverflow);
    return MakeItemClock({definition.clock,definition.durationMs},created);
}
std::expected<CostumeLifetimeState,CostumeLifetimeError> MakeItemClock(CostumeClockDefinition definition,std::uint64_t created)
{
    if(!ValidClock(definition))return std::unexpected(CostumeLifetimeError::InvalidDefinition);
    if(created>MaxTime)return std::unexpected(CostumeLifetimeError::TimeOverflow);
    CostumeLifetimeState state;
    if(definition.clock==CostumeClock::SinceCreation)
    {
        if(definition.durationMs>MaxTime-created)return std::unexpected(CostumeLifetimeError::TimeOverflow);
        state.started=true;state.deadlineUtcMs=created+definition.durationMs;
    }
    if(definition.clock==CostumeClock::EquippedTime){state.started=true;state.remainingMs=definition.durationMs;}
    return state;
}
std::expected<CostumeLifetimePlan,CostumeLifetimeError> AdvanceCostumeLifetime(
    const ItemCostumeRow& definition,const CostumeLifetimeState& before,CostumeClockStep step)
{
    if(!ValidCostume(definition))return std::unexpected(CostumeLifetimeError::InvalidDefinition);
    return AdvanceItemClock({definition.clock,definition.durationMs},before,step);
}
std::expected<CostumeLifetimePlan,CostumeLifetimeError> AdvanceItemClock(
    CostumeClockDefinition definition,const CostumeLifetimeState& before,CostumeClockStep step)
{
    if(!ValidClock(definition))return std::unexpected(CostumeLifetimeError::InvalidDefinition);
    if(step.nowUtcMs>MaxTime || step.elapsedMs>MaxTime)return std::unexpected(CostumeLifetimeError::TimeOverflow);
    if(!ValidState(definition,before,step.wasEquipped))return std::unexpected(CostumeLifetimeError::InvalidState);
    auto state=before;
    if(!state.expired && definition.clock!=CostumeClock::Permanent)
    {
        if(definition.clock==CostumeClock::EquippedTime)
        {
            if(step.wasEquipped)state.remainingMs=step.elapsedMs>=state.remainingMs ? 0 : state.remainingMs-step.elapsedMs;
            if(!state.remainingMs)state.expired=true;
        }
        else
        {
            if(!state.started && step.useRequested)
            {
                if(definition.durationMs>MaxTime-step.nowUtcMs)return std::unexpected(CostumeLifetimeError::TimeOverflow);
                state.started=true;state.deadlineUtcMs=step.nowUtcMs+definition.durationMs;
            }
            if(state.started && step.nowUtcMs>=state.deadlineUtcMs)state.expired=true;
        }
    }
    return CostumeLifetimePlan{state,state.expired,state.expired && step.wasEquipped};
}
std::expected<CostumeEquipPlan,CostumeEquipError> PlanCostumeEquip(const ItemCatalog& catalog,const CostumeEquipRequest& request)
{
    using E=CostumeEquipError;
    if(!request.admittedByServer)return std::unexpected(E::Forbidden);
    const auto* item=catalog.Find(request.item);
    if(!item)return std::unexpected(E::MissingItem);
    const auto* costume=std::get_if<ItemCostumeRow>(&item->category);
    if(!costume)return std::unexpected(E::NotCostume);
    if(item->base.retired)return std::unexpected(E::RetiredItem);
    if(request.stackUnits!=1)return std::unexpected(E::InvalidStack);
    if(request.characterClass<1 || request.characterClass>4 || request.sex<1 || request.sex>2)
        return std::unexpected(E::InvalidCharacter);
    const auto* policy=catalog.FindUsePolicy(item->base.usePolicy);
    if(!(policy->allowedClasses&(1u<<(request.characterClass-1))) || !(policy->allowedSexes&(1u<<(request.sex-1))) ||
       (request.inCombat && !policy->useDuringCombat))return std::unexpected(E::Forbidden);
    if(request.characterLevel<item->base.minimumLevel)return std::unexpected(E::LevelTooLow);
    if(!catalog.FindRefinement(item->base.refinementProfile)->FindLevel(request.refinementLevel))return std::unexpected(E::InvalidRefinement);
    if(costume->kind==CostumeKind::Weapon)
    {
        if(!request.equippedWeapon)return std::unexpected(E::WeaponRequired);
        if(request.equippedWeapon!=costume->weapon)return std::unexpected(E::WeaponMismatch);
    }
    auto step=request.time;step.useRequested=true;
    auto lifetime=AdvanceCostumeLifetime(*costume,request.lifetime,step);
    if(!lifetime)return std::unexpected(E::InvalidLifetime);
    if(lifetime->removeItem)
        return CostumeEquipPlan{request.item,costume->kind,request.refinementLevel,"",*lifetime,catalog.Revision(),catalog.ContentHash(),false};
    if(costume->companionForm) {
        const auto* form=catalog.FindCompanionForm(*costume->companionForm);
        if(!form)return std::unexpected(E::MissingVisual);
        return CostumeEquipPlan{request.item,costume->kind,request.refinementLevel,form->modelAsset,*lifetime,catalog.Revision(),catalog.ContentHash()};
    }
    if(costume->bodyEffect || costume->kind==CostumeKind::WeaponEffect)return CostumeEquipPlan{request.item,costume->kind,request.refinementLevel,"",*lifetime,catalog.Revision(),catalog.ContentHash()};
    const auto* visual=catalog.FindCostumeVisual(request.item,request.characterClass,request.sex);
    if(!visual)return std::unexpected(E::MissingVisual);
    return CostumeEquipPlan{request.item,costume->kind,request.refinementLevel,visual->modelAsset,*lifetime,catalog.Revision(),catalog.ContentHash()};
}
}
