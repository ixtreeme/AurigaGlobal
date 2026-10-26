#pragma once
#include <cstddef>
#include "tables.h"

namespace time64 {
// Fixed payloads affected by the shared 64-bit time layout. Reject a mismatched
// peer before a handler casts its shorter/older frame to the current structure.
inline bool ValidDatabasePacketSize(uint8_t header, size_t size)
{
    switch (header) {
    case HEADER_DG_PLAYER_SESSION_HELLO: return size == 24;
    case HEADER_DG_PLAYER_CACHE_HELLO: return size == 24;
    case HEADER_DG_PLAYER_CACHE_RECEIPT: return size == 176;
    case HEADER_DG_PLAYER_SESSION_REPLY: return size >= 80 && size <= 80 + 3 * 1024 * 1024;
    case HEADER_DG_SET_EVENT_FLAG: return size == sizeof(TPacketSetEventFlag);
    case HEADER_DG_PLAYER_LOAD_SUCCESS: return size == sizeof(TPlayerTable);
    case HEADER_DG_MARRIAGE_ADD: return size == sizeof(TPacketMarriageAdd);
    case HEADER_DG_CHANGE_GUILD_PRIV: return size == sizeof(TPacketDGChangeGuildPriv);
    case HEADER_DG_CHANGE_EMPIRE_PRIV: return size == sizeof(TPacketDGChangeEmpirePriv);
    default: return true;
    }
}
} // namespace time64
