#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <entt/entity/entity.hpp>

#include "../../char.h"
#include "../../typedef.h"
#include "../../questpc.h"

namespace ecs {

struct ClickTrigger {
    uint8_t type { 0 };
    int (*callback)(entt::entity, entt::entity) { nullptr };
};

struct QuestContext {
    // Runtime references must keep their generation; a wire VID/PID can be reused.
    entt::entity npc { entt::null };
    entt::entity lockOwner { entt::null };
    uint32_t byVnum { 0 };
    entt::entity questItem { entt::null };
};

// The quest runtime this character owns (flags, quest states and timers). The
// PC keeps its heap address while the entity lives, so the running-state and
// timer pointers stay stable; this component is the single owner and is
// destroyed with the character entity, which also cancels the PC's timers.
struct QuestPCState {
    std::unique_ptr<quest::PC> pc;
};

struct ItemAward {
    uint32_t vnum { 0 };
    std::string command;
};

struct RankPoints {
    int64_t points[RANKING_MAX_CATEGORIES] {};
};

struct AlignBonuses {
    int32_t hp, monster, human, metin, boss, pvm, normal, skill;
    int32_t appliedHp, appliedMonster, appliedHuman, appliedMetin;
    int32_t appliedBoss, appliedPvm, appliedNormal, appliedSkill;
    uint8_t lastGrade;
};

} // namespace ecs
