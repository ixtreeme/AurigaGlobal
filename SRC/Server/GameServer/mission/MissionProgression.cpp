#include "MissionProgression.hpp"
#include <algorithm>

namespace auriga::mission
{
std::expected<item::BookCharacter, std::string> RewardProgression(
    const item::BookCharacter &before, const RewardBundle &reward, const ProgressionRules &rules)
{
    if (!item::ValidBookCharacter(before) || !before.progression ||
        !ValidRewardBundle(reward) || reward.schema != 3 ||
        !rules.maximumLevel || rules.maximumLevel > 255 ||
        before.progression->level > rules.maximumLevel)
        return std::unexpected("Invalid progression snapshot or rules");
    auto after = before;
    auto &p = *after.progression;
    uint64_t xp = reward.experience;
    unsigned levels = reward.levels, drawn = 0;
    if (p.level + levels > rules.maximumLevel)
        return std::unexpected("Direct level reward exceeds the level cap");
    while (levels || xp)
    {
        if (p.level >= rules.maximumLevel || p.level >= rules.nextExperience.size())
            return std::unexpected("Experience reward exceeds the level cap");
        const uint64_t next = rules.nextExperience[p.level];
        if (next < 4 || after.experience >= next)
            return std::unexpected("Invalid current level experience");
        const uint64_t amount = levels ? next - after.experience : std::min(xp, next - after.experience);
        const uint64_t total = after.experience + amount;
        if (!levels) xp -= amount;
        const unsigned steps = total >= next ? 4 : std::min<uint64_t>(3, total / (next / 4));
        // Existing saved steps can exceed the current XP after an XP cost.
        for (unsigned step = p.step + 1; step <= steps; ++step)
        {
            if (step < 4)
            {
                if (rules.cappedStats344 ? p.level < 115 || (p.level == 115 && step < 3)
                                        : p.level <= rules.statLevelLimit)
                    ++p.statPoints;
            }
            else
            {
                if (drawn >= rules.gains.size())
                    return std::unexpected("Missing server level growth draw");
                const auto [hp, sp] = rules.gains[drawn++];
                if (hp > INT16_MAX - p.randomHP || sp > INT16_MAX - p.randomSP)
                    return std::unexpected("Level growth overflow");
                p.randomHP += hp;
                p.randomSP += sp;
                if (rules.skillGroup)
                {
                    if (p.level >= 5) ++after.points[0];
                    if (p.level >= 9) ++after.points[1];
                }
            }
        }
        if (total == next)
        {
            ++p.level;
            p.step = 0;
            after.experience = 0;
            if (levels) --levels;
        }
        else
        {
            p.step = std::max<unsigned>(p.step, steps);
            after.experience = static_cast<uint32_t>(total);
        }
        if (!item::ValidBookCharacter(after))
            return std::unexpected("Progression point overflow");
    }
    return after;
}
}
