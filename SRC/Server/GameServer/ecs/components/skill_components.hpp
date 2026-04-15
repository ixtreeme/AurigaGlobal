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
    uint32_t lastSkillTime;
    bool disableCooltime;
};

struct SkillDamageBonus {
    std::unordered_map<uint8_t, int> bySkill;
    std::map<int, TSkillUseInfo> useInfo;
};

struct SkillColor {
    uint32_t data[ESkillColorLength::MAX_SKILL_COUNT +
                  ESkillColorLength::MAX_BUFF_COUNT]
                 [ESkillColorLength::MAX_EFFECT_COUNT] {};
};

} // namespace ecs
