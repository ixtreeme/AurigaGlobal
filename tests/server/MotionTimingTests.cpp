#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/entity/motion.h"
#include "../../SRC/Server/GameServer/entity/mob_manager.h"
#include "db.h"
#include <cmath>
#include <iostream>
#include <limits>

// SQL/mob boot services are outside this timing-reader fixture. The real
// GetMotionDuration/GetMoveSpeed methods run over the stored DB-value layout.
void CMob::AddSkillSplash(int, uint32_t, uint32_t) {}
SQLMsg* DBManager::DirectQuery(const char*, ...) { return nullptr; }
MYSQL_ROW _SQLResult::FetchRow() { return nullptr; }

struct MotionFixture : CMotionManager {
    void Store(uint32_t race, const SMotionData& row) { m_map_motion[race] = row; }
};

int main()
{
    MotionFixture motions;
    CMotionManager::SMotionData row;
    row.walkSpeed = 125; row.runSpeed = 300;
    row.attackDuration = 1234; // Database values baked by motion_bake.py are milliseconds.
    for (int i = 0; i < MOB_SKILL_MAX_NUM; ++i) row.skillDuration[i] = 500.0f + i * 250;
    motions.Store(101, row);
    auto close = [](float a, float b) { return std::fabs(a - b) < 0.0001f; };
    if (!close(motions.GetMotionDuration(101, MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_NORMAL_ATTACK)), 1.234f)) return 1;
    for (int i = 0; i < MOB_SKILL_MAX_NUM; ++i)
        if (!close(motions.GetMotionDuration(101, MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_SPECIAL_1 + i)), 0.5f + i * 0.25f)) return 2;
    if (motions.GetMoveSpeed(101, true) != 125 || motions.GetMoveSpeed(101, false) != 300) return 3;
    if (motions.GetMotionDuration(999, MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_NORMAL_ATTACK)) != 0) return 4;
    row.attackDuration = -1; row.skillDuration[0] = std::numeric_limits<float>::infinity();
    motions.Store(101, row);
    if (motions.GetMotionDuration(101, MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_NORMAL_ATTACK)) != 0 ||
        motions.GetMotionDuration(101, MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_SPECIAL_1)) != 0) return 5;
    std::cout << "MOTION_TIMING_TESTS=SUCCESS milliseconds -> seconds, skill timing, movement speed, invalid values\n";
    return 0;
}
