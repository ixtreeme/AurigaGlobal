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

bool AuthorizePlayerLoad(const TAccountTable& account, const TPlayerLoadPacket& request,
                         const PlayerLoadContext& context)
{
    return context.authenticated && context.loggedOn && !context.playing && !context.deleted &&
           context.peer && context.peer == context.accountPeer && account.id && account.id == request.account_id &&
           request.account_index < PLAYER_PER_ACCOUNT && request.player_id &&
           account.players[request.account_index].dwID == request.player_id;
}
