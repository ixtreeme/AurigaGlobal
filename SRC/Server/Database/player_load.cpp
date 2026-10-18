#include "stdafx.h"
#include "player_load.h"
#include <algorithm>

void NormalizeSecondarySkills(TPlayerTable& player)
{
    player.skills[123].bLevel = 0; // Retired SKILL_CREATE.
    if (player.level <= 9) {
        player.sub_skill_point = 0;
        return;
    }
    int spent = (std::min)(20, int(player.skills[121].bLevel)) +
        (std::min)(20, int(player.skills[124].bLevel)) +
        (std::min)(10, int(player.skills[131].bLevel)) +
        (std::min)(20, int(player.skills[141].bLevel)) +
        (std::min)(20, int(player.skills[142].bLevel));
#ifdef ENABLE_NEW_SECONDARY_SKILLS
    spent += (std::min)(10, int(player.skills[143].bLevel)) +
        (std::min)(10, int(player.skills[144].bLevel)) +
        (std::min)(10, int(player.skills[145].bLevel)) +
        (std::min)(10, int(player.skills[146].bLevel));
#endif
    player.sub_skill_point = player.level - 9 - spent;
}
