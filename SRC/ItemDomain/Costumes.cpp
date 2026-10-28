#include <item/Costumes.hpp>
#include <item/ItemCatalog.hpp>
#include <limits>

namespace auriga::item
{
namespace
{
constexpr std::uint64_t MaxTime=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
bool ValidState(const ItemCostumeRow& definition,const CostumeLifetimeState& state,bool equipped)
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
bool ValidCostume(const ItemCostumeRow& row) noexcept
{
    if(row.key.Category()!=ItemCategory::Costume || row.kind<CostumeKind::Body || row.kind>CostumeKind::Weapon)return false;
    if(row.kind==CostumeKind::Weapon)
    {if(!row.weapon || *row.weapon<WeaponSubtype::Sword || *row.weapon>WeaponSubtype::MountSpear)return false;}
    else if(row.weapon)return false;
    if(row.clock<CostumeClock::Permanent || row.clock>CostumeClock::EquippedTime || row.durationMs>MaxTime)return false;
    return row.clock==CostumeClock::Permanent ? row.durationMs==0 : row.durationMs>0;
}
std::expected<CostumeLifetimeState,CostumeLifetimeError> MakeCostumeLifetime(
    const ItemCostumeRow& definition,std::uint64_t created)
{
    if(!ValidCostume(definition))return std::unexpected(CostumeLifetimeError::InvalidDefinition);
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
    const auto* visual=catalog.FindCostumeVisual(request.item,request.characterClass,request.sex);
    if(!visual)return std::unexpected(E::MissingVisual);
    return CostumeEquipPlan{request.item,costume->kind,request.refinementLevel,visual->modelAsset,*lifetime,catalog.Revision(),catalog.ContentHash()};
}
}
