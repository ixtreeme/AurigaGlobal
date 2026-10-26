#pragma once

#include <common/tables.h>

// Normalize the legacy skill slot and reconstruct the unspent secondary
// budget from every supported secondary skill when loading a player.
void NormalizeSecondarySkills(TPlayerTable& player);

struct PlayerLoadContext
{
    uint32_t peer = 0, accountPeer = 0;
    bool authenticated = false, loggedOn = false, playing = false, deleted = false;
};
bool AuthorizePlayerLoad(const TAccountTable& account, const TPlayerLoadPacket& request,
    const PlayerLoadContext& context);
