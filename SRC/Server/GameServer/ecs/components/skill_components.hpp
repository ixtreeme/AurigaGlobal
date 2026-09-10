#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <unordered_map>

#include <common/tables.h>

#include "../../char.h"

namespace ecs {

// Every skill's level, master type and next-read time. This held a raw
// pointer to an array the component allocated itself, while CHARACTER kept a
// second array of its own loaded from the same row - so a level-up wrote here
// and the save read there. The table is owned here now, and there is one copy.
struct SkillLevels {
    std::array<TPlayerSkill, SKILL_MAX_NUM> levels {};
    uint8_t group { 0 };
    bool loaded { false };
};

struct SkillCooldowns {
    std::array<uint32_t, MOB_SKILL_MAX_NUM> mob {};
    // When a skill was last used. The field was declared here and left
    // uninitialised while CHARACTER kept the live copy.
    uint32_t lastSkillTime { 0 };
    bool disableCooltime;
};

// Mob skill hits that have been scheduled but not landed yet, keyed by the
// splash index so a repeat use of the same slot cancels the one in flight.
struct MobSkillEvents {
    std::map<int, LPEVENT> pending;
};

struct SkillDamageBonus {
    std::unordered_map<uint8_t, int> bySkill;
    std::map<int, TSkillUseInfo> useInfo;
};

struct SkillColorChangeInProgress {};

struct SkillColor {
    uint32_t data[ESkillColorLength::MAX_SKILL_COUNT +
                  ESkillColorLength::MAX_BUFF_COUNT]
                 [ESkillColorLength::MAX_EFFECT_COUNT] {};
};

} // namespace ecs
