#pragma once
#include <item/EquipmentStats.hpp>
#include <item/PendantElement.hpp>
#include "../ecs/Registry.hpp"
#include "../entity/char.h"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include <algorithm>
#include "EquipmentPointBindings.hpp"

namespace ecs
{
// Derived view only; the persisted TypedItems snapshot remains the source.
struct TypedEquipmentProjection
{
    auriga::item::ItemOwnerFence fence;
    std::optional<auriga::item::EquipmentStats> stats;
    std::uint64_t revision=0;
    std::uint64_t collectionRevision=0;
    bool applied=false;
    auriga::item::TargetElement targetElement = auriga::item::TargetElement::None;
};
// Clones already copy the player's effective points. The weapon facts are
// used for motion and skills without applying those stats a second time.
struct ClonedWeaponStats
{
    auriga::item::WeaponSubtype subtype;
    std::int64_t physicalAttackRaw;
    std::int64_t magicalAttackRaw;
};
}
inline bool HasTypedEquipment(entt::entity e)
{return g_registry.valid(e) && g_registry.all_of<ecs::TypedEquipmentProjection>(e);}
inline std::optional<auriga::item::EquipmentStats> TypedEquipmentStats(entt::entity e)
{const auto* p=g_registry.valid(e) ? g_registry.try_get<ecs::TypedEquipmentProjection>(e) : nullptr;return p ? p->stats : std::optional<auriga::item::EquipmentStats>{};}
inline const auriga::item::EquipmentStats* TypedEquipmentView(entt::entity e)
{const auto* p=g_registry.valid(e) ? g_registry.try_get<ecs::TypedEquipmentProjection>(e) : nullptr;return p && p->stats ? &*p->stats : nullptr;}
inline std::int64_t TypedEquipmentBonus(entt::entity e,auriga::item::BonusId id)
{const auto* p=g_registry.valid(e) ? g_registry.try_get<ecs::TypedEquipmentProjection>(e) : nullptr;return p && p->stats ? p->stats->Bonus(id) : 0;}
inline std::int64_t TypedPetBonus(entt::entity e,auriga::item::PetEffect effect)
{const auto* stats=TypedEquipmentView(e);return stats ? stats->petBonuses.Bonus(effect) : 0;}

inline const ecs::ClonedWeaponStats* ClonedWeaponView(entt::entity e)
{
    return g_registry.valid(e) ? g_registry.try_get<ecs::ClonedWeaponStats>(e) : nullptr;
}
inline int CombatWeaponSubtype(entt::entity e)
{
    const auto* clone = ClonedWeaponView(e);
    if (clone || HasTypedEquipment(e))
    {
        const auto* stats = TypedEquipmentView(e);
        const auto subtype = clone ? std::optional{clone->subtype} : (stats ? stats->weapon : std::nullopt);
        if (!subtype) return -1;
        switch(*subtype){case auriga::item::WeaponSubtype::Sword:return WEAPON_SWORD;case auriga::item::WeaponSubtype::Dagger:return WEAPON_DAGGER;
        case auriga::item::WeaponSubtype::Bow:return WEAPON_BOW;case auriga::item::WeaponSubtype::TwoHanded:return WEAPON_TWO_HANDED;
        case auriga::item::WeaponSubtype::Bell:return WEAPON_BELL;case auriga::item::WeaponSubtype::Fan:return WEAPON_FAN;
        case auriga::item::WeaponSubtype::MountSpear:return WEAPON_MOUNT_SPEAR;}return -1;
    }
    return -1;
}
inline bool HasCombatWeapon(entt::entity e){return CombatWeaponSubtype(e)>=0;}
inline int EquipmentLegacyInt(std::int64_t value)
{return static_cast<int>(std::clamp<std::int64_t>(value,INT_MIN,INT_MAX));}
inline std::int64_t EquipmentScale(std::int64_t value,std::int64_t factor,std::int64_t divisor=10000)
{return auriga::item::ScaleEquipmentValue(value,std::max<std::int64_t>(0,factor),divisor).value_or(0);}
inline std::int64_t EquipmentEffectiveDefense(entt::entity attacker,std::int64_t defense,bool magical=false)
{
    using B=auriga::item::BonusId;
    const auto flat=EquipmentScale(TypedEquipmentBonus(attacker,magical ? B::MagicalFlatPenetration : B::PhysicalFlatPenetration),1,1000);
    const auto bp=std::clamp<std::int64_t>(TypedEquipmentBonus(attacker,magical ? B::MagicalPercentPenetration : B::PhysicalPercentPenetration),0,10000);
    return EquipmentScale(std::max<std::int64_t>(0,defense-flat),10000-bp);
}
inline std::int64_t EquipmentDamageModifiers(entt::entity attacker,entt::entity victim,std::int64_t damage,std::uint8_t type,bool pvp,bool critical)
{
    using B=auriga::item::BonusId;
    const bool normal=type==DAMAGE_TYPE_NORMAL || type==DAMAGE_TYPE_NORMAL_RANGE;
    const bool magical=type==DAMAGE_TYPE_MAGIC || type==DAMAGE_TYPE_FIRE || type==DAMAGE_TYPE_ICE || type==DAMAGE_TYPE_ELEC;
    const bool skill=type==DAMAGE_TYPE_MELEE || type==DAMAGE_TYPE_RANGE || magical;
    if(!normal && !skill)return damage;
    const auto apply=[&](std::int64_t factor,std::int64_t divisor=10000){damage=std::clamp<std::int64_t>(EquipmentScale(damage,factor,divisor),0,INT_MAX);};
    apply(10000+TypedEquipmentBonus(attacker,normal ? B::NormalOutgoing : B::SkillOutgoing));
    apply(10000+TypedEquipmentBonus(attacker,pvp ? B::PvpOutgoing : B::PveOutgoing));
    if(critical)apply(std::max<std::int64_t>(10000,20000+TypedEquipmentBonus(attacker,B::CritMultiplierExtra)),20000);
    const auto* target=TypedEquipmentView(victim);
    const auto channel=target ? target->resistances[magical ? 1 : 0] : 0;
    apply(10000-channel-TypedEquipmentBonus(victim,magical ? B::MagicalResistance : B::PhysicalResistance));
    if(target)
    {
        if(magical)
        {const int element=type==DAMAGE_TYPE_FIRE ? 9 : type==DAMAGE_TYPE_ELEC ? 10 : type==DAMAGE_TYPE_ICE ? 11 : -1;if(element>=0)apply(10000-target->resistances[element]);}
        else
        {const int subtype=CombatWeaponSubtype(attacker);int index=subtype==WEAPON_SWORD ? 2 : subtype==WEAPON_DAGGER ? 3 : subtype==WEAPON_BOW ? 4 : subtype==WEAPON_TWO_HANDED ? 5 : subtype==WEAPON_BELL ? 6 : subtype==WEAPON_FAN ? 7 : subtype==WEAPON_MOUNT_SPEAR ? 8 : -1;if(index>=0)apply(10000-target->resistances[index]);}
    }
    apply(10000-TypedEquipmentBonus(victim,normal ? B::NormalIncoming : B::SkillIncoming));
    apply(10000-TypedEquipmentBonus(victim,pvp ? B::PvpIncoming : B::PveIncoming));
    if(!magical){apply(10000-TypedEquipmentBonus(victim,B::BlockReduction));apply(10000-TypedEquipmentBonus(victim,B::Evasion));}
    apply(10000+TypedEquipmentBonus(attacker,B::GlobalOutgoing));
    const auto* source=TypedEquipmentView(attacker);
    if(damage>0 && ((source && source->petBonuses.seal) || (target && target->petBonuses.seal)))
    {
        const auto combatant=[](entt::entity e) {
            const bool player=ecs::PlayerRuntime::IsPC(e);
            return auriga::item::PetCombatant{player,ecs::PlayerRuntime::IsMonster(e),ecs::PlayerRuntime::IsStone(e),
                !player && ecs::PlayerRuntime::GetMobRank(e)>=MOB_RANK_BOSS,
                static_cast<std::uint8_t>(player ? ecs::PlayerRuntime::GetJob(e) : 0)};
        };
        const auriga::item::PetBonusValues empty{};
        const auto result=auriga::item::ApplyPetDamage(source ? source->petBonuses.bonuses.total : empty,
            target ? target->petBonuses.bonuses.total : empty,damage,combatant(attacker),combatant(victim),normal,!magical);
        damage=result ? std::clamp<std::int64_t>(*result,0,INT_MAX) : 0;
    }
    return damage;
}
inline bool IsEquipmentDerivedPoint(std::uint8_t type)
{
    if(EquipmentPointBindingFor(type))return true;
    switch(type){case POINT_ST:case POINT_HT:case POINT_DX:case POINT_IQ:case POINT_ATT_GRADE:case POINT_MAGIC_ATT_GRADE:
    case POINT_DEF_GRADE:case POINT_CLIENT_DEF_GRADE:case POINT_MAGIC_DEF_GRADE:case POINT_MOV_SPEED:case POINT_ATT_SPEED:case POINT_CASTING_SPEED:return true;default:return false;}
}
inline std::int64_t EquipmentPointValue(entt::entity e,std::uint8_t type,std::int64_t base)
{
    using namespace auriga::item;
    const auto* p=g_registry.try_get<ecs::TypedEquipmentProjection>(e);if(!p || !p->stats || !IsEquipmentDerivedPoint(type))return base;
    const auto& s=*p->stats;std::int64_t flat=0,bp=0;bool raw=false;
    const auto binding=EquipmentPointBindingFor(type);
    const auto property=binding ? s.Bonus(binding->bonus)/binding->divisor : 0;
    switch(type)
    {
    case POINT_ST:flat=s.Bonus(BonusId::Strength);break;
    case POINT_DX:flat=s.Bonus(BonusId::Dexterity);break;
    case POINT_HT:flat=s.Bonus(BonusId::Constitution);break;
    case POINT_IQ:flat=s.Bonus(BonusId::Intelligence);break;
    case POINT_ATT_GRADE:flat=s.flat.physicalAttackRaw;bp=s.Bonus(BonusId::PhysicalAttackPercent);raw=true;break;
    case POINT_MAGIC_ATT_GRADE:flat=s.flat.magicalAttackRaw;bp=s.Bonus(BonusId::MagicalAttackPercent);raw=true;break;
    case POINT_DEF_GRADE:case POINT_CLIENT_DEF_GRADE:flat=s.flat.physicalDefenseRaw-s.Bonus(BonusId::ArmorPhysicalDefense);raw=true;break;
    case POINT_MAGIC_DEF_GRADE:flat=s.flat.magicalDefenseRaw;raw=true;break;
    case POINT_MOV_SPEED:flat=property;bp=s.Bonus(BonusId::MovementSpeedPercent);break;
    case POINT_ATT_SPEED:flat=property;bp=s.Bonus(BonusId::AttackSpeedPercent);break;
    case POINT_CASTING_SPEED:flat=property;bp=s.Bonus(BonusId::CastSpeedPercent);break;
    default:return std::clamp<std::int64_t>(auriga::item::AddEquipmentValue(base,property).value_or(0),INT_MIN,INT_MAX);
    }
    // This boundary feeds the existing whole-point formulas, whose public
    // interfaces are int32. Aggregate in raw units first, round only once.
    const auto value=AddEquipmentValue(std::clamp<std::int64_t>(base,INT_MIN,INT_MAX)*(raw ? 1000LL : 1LL),flat);
    if(!value)return 0;
    const auto scaled=ScaleEquipmentValue(std::max<std::int64_t>(0,*value),std::max<std::int64_t>(0,10000+bp));if(!scaled)return 0;
    const auto result=raw ? ScaleEquipmentValue(*scaled,1,1000).value_or(0) : *scaled;
    return std::clamp<std::int64_t>(result,0,INT_MAX);
}
void RefreshTypedEquipment(entt::entity);
void SendTypedEquipment(entt::entity);
void TickTypedEquipment();
