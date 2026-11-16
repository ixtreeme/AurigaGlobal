#pragma once
#include <cstdint>
namespace WarriorExpansion
{
constexpr uint32_t FirstSkill = 180, LastSkill = 204, FirstBook = 650000;
constexpr uint32_t SkillCount = LastSkill - FirstSkill + 1;
constexpr bool IsSkill(uint32_t id)
{
    return id >= FirstSkill && id <= LastSkill;
}
constexpr bool IsBuff(uint32_t id)
{
    return (id >= 180 && id <= 183) || id == 194 || (id >= 200 && id <= 204);
}
constexpr bool IsArea(uint32_t id)
{
    return (id >= 190 && id <= 193) || (id >= 197 && id <= 199);
}
constexpr bool IsRanged(uint32_t id)
{
    return (id >= 184 && id <= 189) || id == 195 || id == 196;
}
constexpr int BuffGroup(uint32_t id)
{
    return id == 180 || id == 181 ? 1 : id == 182 || id == 183 ? 2 : 0;
}
constexpr bool CanShare(uint32_t id) { return id == 200 || (id >= 202 && id <= 204); }
// Skill IDs 200+ overlap native potion affect IDs. New buffs use a separate affect namespace.
constexpr uint32_t BuffAffect(uint32_t id) { return id<193?id:2600+id-193; }
constexpr uint32_t SkillFromAffect(uint32_t type) { return type>=2600&&type<=2611?type-2600+193:type<193?type:0; }
constexpr bool IsBuffAffect(uint32_t type) { return IsBuff(SkillFromAffect(type)); }
constexpr bool IsSelfArea(uint32_t id) { return id == 193 || (id >= 197 && id <= 199); }
constexpr bool BuffForGroup(uint32_t id, unsigned group, unsigned job = 0)
{ return IsBuff(id) && (job > 0 || BuffGroup(id) == 0 || BuffGroup(id) == int(group)); }
} // namespace WarriorExpansion
