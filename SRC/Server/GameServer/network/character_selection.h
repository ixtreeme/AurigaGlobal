#pragma once

#include <common/tables.h>
#include <common/Headers/Header_CG.h>
#include <common/Headers/CG_Packet.h>

namespace character_selection {
enum class ChangeNameResult { Ignore, InvalidName, Ready };
ChangeNameResult PrepareChangeName(const TAccountTable& account,
    const TPacketCGChangeName& request, TPacketGDChangeName& output);
}
