#pragma once

#include <cstdint>
#include <string>
#include <entt/entity/entity.hpp>

#include "../../char.h"
#include "../../typedef.h"

namespace ecs {

struct QuestContext {
    uint32_t npcVID;
    uint32_t byVnum;
    entt::entity questItem { entt::null };
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
