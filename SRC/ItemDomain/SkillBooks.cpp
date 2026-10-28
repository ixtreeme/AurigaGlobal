#include <item/SkillBooks.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"
#include <algorithm>
#include <array>
#include <limits>

namespace auriga::item
{
bool ValidSkillBookProfile(const SkillBookProfile& p) noexcept
{
    if (!p.id || !detail::ContentCode(p.code) || !p.unitsPerUse || p.targets.size()>MaxSkillBookTargets ||
        p.steps.empty() || p.steps.size()>MaxSkillBookSteps || p.operation<SkillBookOperation::Learn ||
        p.operation>SkillBookOperation::HorsePoint || p.targetMode<SkillBookTargetMode::Fixed ||
        p.targetMode>SkillBookTargetMode::None || p.bonus<SkillBookBonus::Ignore || p.bonus>SkillBookBonus::ApplyAndConsume)
        return false;
    const bool horse=p.operation==SkillBookOperation::HorsePoint, forget=p.operation==SkillBookOperation::Forget;
    if (horse != (p.targetMode==SkillBookTargetMode::None) ||
        (horse ? (!p.targets.empty() || p.allowedMasteries) :
         (p.targets.empty() || !p.allowedMasteries || p.allowedMasteries>7 ||
          (p.targetMode==SkillBookTargetMode::Fixed && p.targets.size()!=1))) ||
        (forget && (p.allowedMasteries!=1 || p.allowDelayBypass)) ||
        ((horse || forget) && p.bonus!=SkillBookBonus::Ignore)) return false;
    std::array<bool,MaxBookSkillId+1> targets{};
    for (const auto& t:p.targets)
    {
        if (!t.skillId || t.skillId>MaxBookSkillId || targets[t.skillId] || t.forbiddenEmpire>3 ||
            (forget ? (t.refundPool<SkillPointPool::Main || t.refundPool>SkillPointPool::Horse || t.forbiddenEmpire) :
             t.refundPool!=SkillPointPool::None)) return false;
        targets[t.skillId]=true;
    }
    std::array<bool,MaxBookSkillLevel> levels{};
    bool anyDelay=false;
    for (const auto& s:p.steps)
    {
        if (s.currentLevel>=MaxBookSkillLevel || levels[s.currentLevel] || s.nextLevel>MaxBookSkillLevel ||
            !s.requiredSuccessfulReads || s.successChanceBp>10000 || s.boostedChanceBp<s.successChanceBp ||
            s.boostedChanceBp>10000 || s.experienceCost>INT64_MAX || s.delayMaximumMs>INT64_MAX ||
            s.delayMinimumMs>s.delayMaximumMs || (!s.delayMinimumMs && s.delayMaximumMs) ||
            (p.bonus==SkillBookBonus::Ignore && s.boostedChanceBp!=s.successChanceBp)) return false;
        levels[s.currentLevel]=true; anyDelay |= s.delayMaximumMs!=0;
        if (forget)
        {
            if (!s.currentLevel || s.currentLevel>=20 || s.nextLevel+1!=s.currentLevel ||
                s.requiredSuccessfulReads!=1 || s.successChanceBp!=10000 || s.experienceCost ||
                s.delayMaximumMs || s.delayOnFailure) return false;
        }
        else if (horse)
        {
            if (s.currentLevel>=20 || s.nextLevel!=s.currentLevel+1 || s.requiredSuccessfulReads!=1 ||
                s.experienceCost) return false;
        }
        else if (s.nextLevel<=s.currentLevel ||
                 !(p.allowedMasteries & (1u << (s.currentLevel>=30?2:s.currentLevel>=20?1:0)))) return false;
    }
    return !p.allowDelayBypass || anyDelay;
}
std::expected<PreparedSkillBookUse, SkillBookError> PrepareSkillBookUse(
    const ItemCatalog& c, ItemKey key, const SkillBookAdmission& a)
{
    using E=SkillBookError;
    const auto* item=c.Find(key);
    if (!item) return std::unexpected(E::MissingItem);
    const auto* child=std::get_if<ItemSkillBookRow>(&item->category);
    if (!child) return std::unexpected(E::WrongCategory);
    const auto& p=*c.FindSkillBook(child->profile);
    if (item->base.retired) return std::unexpected(E::RetiredItem);
    if (a.classIndex>3 || a.sexIndex>1 || !a.empire || a.empire>3 || a.experience>INT64_MAX ||
        a.cooldownRemainingMs>INT64_MAX || a.availableUnits>item->base.maxStack)
        return std::unexpected(E::InvalidState);
    if (!a.skillSystemEnabled) return std::unexpected(E::Disabled);
    if (!a.targetAllowedByRules) return std::unexpected(E::RuleDenied);
    if (!a.alive) return std::unexpected(E::DeadCharacter);
    if (a.polymorphed) return std::unexpected(E::Polymorphed);
    const auto& use=*c.FindUsePolicy(item->base.usePolicy);
    if (a.inCombat && !use.useDuringCombat) return std::unexpected(E::CombatDenied);
    if (a.characterLevel<item->base.minimumLevel) return std::unexpected(E::LevelDenied);
    if (!(use.allowedClasses & (1u<<a.classIndex))) return std::unexpected(E::ClassDenied);
    if (!(use.allowedSexes & (1u<<a.sexIndex))) return std::unexpected(E::SexDenied);
    if (a.availableUnits<p.unitsPerUse) return std::unexpected(E::InsufficientUnits);
    const bool horse=p.operation==SkillBookOperation::HorsePoint, forget=p.operation==SkillBookOperation::Forget;
    SkillBookPlan plan{key,p.id,c.Revision(),p.operation,{}};
    if (horse)
    {
        if (a.skill || a.boundSkillId || !a.horse || a.refundPointBalance ||
            a.horse->unspentPoints>20 || a.horse->allocatedMeleePoints>60 || a.horse->allocatedRangedPoints>60)
            return std::unexpected(E::InvalidState);
        if (a.horse->unspentPoints==20 || a.horse->allocatedMeleePoints==60 || a.horse->allocatedRangedPoints==60)
            return std::unexpected(E::PointLimit);
        plan.previousLevel=a.horse->unspentPoints;
        plan.pointPool=SkillPointPool::Horse;
    }
    else
    {
        if (!a.skill || a.horse || !a.skill->skillId || a.skill->skillId>MaxBookSkillId ||
            a.skill->level>MaxBookSkillLevel || (!forget && a.refundPointBalance))
            return std::unexpected(E::InvalidState);
        std::uint16_t skill=0;
        if (p.targetMode==SkillBookTargetMode::Fixed)
        {
            if (a.boundSkillId) return std::unexpected(E::InvalidState);
            skill=p.targets.front().skillId;
        }
        else
        {
            if (!a.boundSkillId || !*a.boundSkillId || *a.boundSkillId>MaxBookSkillId)
                return std::unexpected(E::InvalidState);
            skill=*a.boundSkillId;
        }
        const auto target=std::ranges::find(p.targets,skill,&SkillBookTarget::skillId);
        if (target==p.targets.end() || a.skill->skillId!=skill) return std::unexpected(E::TargetDenied);
        if (target->forbiddenEmpire==a.empire) return std::unexpected(E::NativeLanguage);
        const auto level=a.skill->level;
        const unsigned mastery=level>=40?3:level>=30?2:level>=20?1:0;
        if (!(p.allowedMasteries & (1u<<mastery))) return std::unexpected(E::MasteryDenied);
        if (forget && a.refundPointBalance==UINT32_MAX) return std::unexpected(E::PointLimit);
        plan.skillId=skill;plan.previousLevel=level;plan.previousReadCount=a.skill->successfulReads;
        plan.pointPool=target->refundPool;
    }
    const auto step=std::ranges::find(p.steps,plan.previousLevel,&SkillBookStep::currentLevel);
    if (step==p.steps.end()) return std::unexpected(E::LevelNotCovered);
    if (a.characterLevel<step->minimumCharacterLevel) return std::unexpected(E::LevelDenied);
    if (!forget && plan.previousReadCount>=step->requiredSuccessfulReads) return std::unexpected(E::InvalidState);
    if (a.experience<step->experienceCost) return std::unexpected(E::InsufficientExperience);
    if (!forget && a.cooldownRemainingMs)
    {
        if (!p.allowDelayBypass || !a.delayBypassAvailable) return std::unexpected(E::OnCooldown);
        plan.consumeDelayBypass=true;
    }
    plan.consumeChanceBonus=p.bonus==SkillBookBonus::ApplyAndConsume && a.chanceBonusAvailable;
    plan.unitsToConsume=p.unitsPerUse;plan.experienceToConsume=step->experienceCost;
    plan.nextLevel=plan.previousLevel;plan.nextReadCount=plan.previousReadCount;
    const auto chance=plan.consumeChanceBonus?step->boostedChanceBp:step->successChanceBp;
    return PreparedSkillBookUse{std::move(plan),*step,chance};
}
std::expected<SkillBookPlan, SkillBookError> ResolveSkillBookUse(
    const PreparedSkillBookUse& r, SkillBookDecisions decisions) noexcept
{
    if ((r.NeedsSuccessRoll() ? (!decisions.successRollBp || decisions.successRollBp>10000) : decisions.successRollBp!=0) ||
        decisions.readDelayMs<r.step_.delayMinimumMs || decisions.readDelayMs>r.step_.delayMaximumMs)
        return std::unexpected(SkillBookError::InvalidDecisions);
    auto plan=r.plan_;
    const bool success=r.chance_==10000 || (r.NeedsSuccessRoll() && decisions.successRollBp<=r.chance_);
    if (success)
    {
        if (plan.operation==SkillBookOperation::Forget)
        {
            plan.outcome=SkillBookOutcome::Forgotten;plan.nextLevel=r.step_.nextLevel;plan.nextReadCount=0;plan.pointsGranted=1;
        }
        else if (plan.operation==SkillBookOperation::HorsePoint)
        {
            plan.outcome=SkillBookOutcome::HorsePoint;plan.nextLevel=r.step_.nextLevel;plan.pointsGranted=1;
        }
        else if (plan.previousReadCount+1>=r.step_.requiredSuccessfulReads)
        {
            plan.outcome=SkillBookOutcome::Learned;plan.nextLevel=r.step_.nextLevel;plan.nextReadCount=0;
        }
        else {plan.outcome=SkillBookOutcome::Progress;++plan.nextReadCount;}
    }
    // Every admitted attempt consumes the declared units/XP/boost tokens, even
    // a failed roll. Admission or decision failure consumes nothing.
    if (decisions.readDelayMs && (success || r.step_.delayOnFailure)) plan.nextReadInMs=decisions.readDelayMs;
    return plan;
}
}
