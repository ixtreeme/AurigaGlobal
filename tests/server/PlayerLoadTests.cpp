#include <Core/stdafx.h>
#include "../../SRC/Server/Database/player_load.h"
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void Check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
}
int main()
{
    try {
        TPlayerTable player {};
        player.level = 50; player.skills[121].bLevel = 20; player.skills[123].bLevel = 20;
        NormalizeSecondarySkills(player);
        Check(player.sub_skill_point == 21 && player.skills[123].bLevel == 0, "legacy skill cost refunded on load");
#ifdef ENABLE_NEW_SECONDARY_SKILLS
        player.skills[143].bLevel = 10;
        NormalizeSecondarySkills(player);
        Check(player.sub_skill_point == 11, "new skills overwrote legacy cost");
        NormalizeSecondarySkills(player);
        Check(player.sub_skill_point == 11, "repeated load changed the point budget");
#endif
        for (auto& skill : player.skills) skill.bLevel = 255;
        player.level = 255;
        NormalizeSecondarySkills(player);
#ifdef ENABLE_NEW_SECONDARY_SKILLS
        Check(player.sub_skill_point == 116, "skill caps changed during reconstruction");
#else
        Check(player.sub_skill_point == 156, "skill caps changed during reconstruction");
#endif
        for (int level = 1; level <= 10; ++level) {
            player = {}; player.level = uint8_t(level); player.sub_skill_point = 99;
            NormalizeSecondarySkills(player);
            Check(player.sub_skill_point == (level == 10 ? 1 : 0), "low-level point budget incorrect");
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "player load: " << checks << " checks passed\n";
}
