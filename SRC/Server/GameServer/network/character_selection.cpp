#include "stdafx.h"
#include "character_selection.h"
#include "config.h"

character_selection::ChangeNameResult character_selection::PrepareChangeName(
    const TAccountTable& account, const TPacketCGChangeName& request, TPacketGDChangeName& output)
{
    if (!account.id || request.index >= PLAYER_PER_ACCOUNT)
        return ChangeNameResult::Ignore;
    const auto& player = account.players[request.index];
    if (!player.dwID || !player.bChangeName)
        return ChangeNameResult::Ignore;
    if (!std::memchr(request.name, '\0', sizeof(request.name)) || !check_name(request.name))
        return ChangeNameResult::InvalidName;
    output = {};
    output.pid = player.dwID;
    strlcpy(output.name, request.name, sizeof(output.name));
    return ChangeNameResult::Ready;
}
