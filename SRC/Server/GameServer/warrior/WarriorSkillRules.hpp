#pragma once
#include "../../common/WarriorSkillIds.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
namespace WarriorExpansion
{
constexpr std::array<int, 5> UnlockLevels{10, 30, 50, 70, 90};
using Choices = std::array<uint16_t, 5>;
constexpr int BooksRequired(int absoluteLevel)
{
    return absoluteLevel >= 20 && absoluteLevel <= 29 ? absoluteLevel - 18 : 0;
}
enum class ChoiceError
{
    None,
    Class,
    Group,
    Level,
    Tier,
    Spent,
    Category,
    Duplicate
};
inline ChoiceError ValidateChoice(unsigned job, unsigned group, int level, const Choices &selected, unsigned tier,
                                  uint32_t skill)
{
    if (job > 3)
        return ChoiceError::Class;
    if (group != 1 && group != 2)
        return ChoiceError::Group;
    if (tier >= selected.size())
        return ChoiceError::Tier;
    if (level < UnlockLevels[tier])
        return ChoiceError::Level;
    if (selected[tier] != 0)
        return ChoiceError::Spent;
    if ((tier == 0 && !BuffForGroup(skill, group, job)) || (tier >= 1 && tier <= 3 && !IsRanged(skill)) ||
        (tier == 4 && !IsArea(skill)))
        return ChoiceError::Category;
    if (std::find(selected.begin(), selected.end(), skill) != selected.end())
        return ChoiceError::Duplicate;
    return ChoiceError::None;
}
constexpr int AvailablePoints(int level, int spent)
{
    return std::max(0, std::max(0, level - 9) - spent);
}
struct ReadProgress
{
    uint8_t level = 20;
    uint16_t successes = 0;
};
inline ReadProgress AdvanceReading(ReadProgress before, bool success)
{
    if (!BooksRequired(before.level) || !success)
        return before;
    if (++before.successes >= BooksRequired(before.level))
    {
        ++before.level;
        before.successes = 0;
    }
    return before;
}
struct ImpactGate
{
    bool alive = true, connected = true, sameMap = true, weaponUnchanged = true, learned = true, targetValid = true;
    double distance = 0, range = 0;
};
inline double Progress(unsigned level) { return std::clamp((int(level)-1)/39.0, 0.0, 1.0); }
inline int BeamRange(unsigned level) { return int(1200+2800*Progress(level)); }
inline int EffectDuration(uint32_t id, unsigned level)
{ return int((id==198 ? 4 : 2)+(id==198 ? 16 : 8)*Progress(level)); }
inline int TargetCount(uint32_t id, unsigned level)
{ return 1+int((id==195 ? 9 : 11)*Progress(level)); }
inline bool InsideBeam(double dx, double dy, double ux, double uy, double length, double halfWidth)
{ const double along=dx*ux+dy*uy; return along>=0 && along<=length && std::abs(dx*uy-dy*ux)<=halfWidth; }
inline bool CanImpact(const ImpactGate &g)
{
    return g.alive && g.connected && g.sameMap && g.weaponUnchanged && g.learned && g.targetValid && g.distance >= 0 &&
           g.distance <= g.range;
}
} // namespace WarriorExpansion
