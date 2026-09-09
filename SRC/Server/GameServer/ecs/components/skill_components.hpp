#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <unordered_map>

#include <common/tables.h>

#include "../../char.h"

namespace ecs {

struct SkillLevels {
    TPlayerSkill* levels { nullptr };
    uint8_t group { 0 };
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
