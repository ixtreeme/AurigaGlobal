#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <unordered_set>

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

// The eunhyung power an attack carried out of stealth. OnMove records it when
// an attack breaks eunhyung and drops it on any other attack; the splash
// damage formula reads it as "ek". No component means no eunhyung, so it can
// never be read before it is written - the CHARACTER field it replaces was
// left uninitialised.
struct EunhyungStrike {
    uint32_t power { 0 };
};

// One chain lightning cast in flight: how many jumps have landed and who has
// been hit already. UseSkill removes it when a new cast starts, so an absent
// component is a fresh cast.
struct ChainLightningState {
    int index { 0 };
    std::unordered_set<entt::entity> excepts;
};

struct SkillColorChangeInProgress {};

struct SkillColor {
    uint32_t data[ESkillColorLength::MAX_SKILL_COUNT +
                  ESkillColorLength::MAX_BUFF_COUNT]
                 [ESkillColorLength::MAX_EFFECT_COUNT] {};
};

} // namespace ecs
