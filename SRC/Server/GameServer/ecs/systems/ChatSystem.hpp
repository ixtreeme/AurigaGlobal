#pragma once

#include <cstdint>
#include <cstdarg>

#include <entt/entt.hpp>

#include "../AIHelpers.hpp"
#include "../../typedef.h"

namespace ecs {

class ChatSystem {
public:
    static void Send(entt::entity e, uint8_t type, const char* format, ...);
    static void SendV(entt::entity e, uint8_t type, const char* format, va_list args);

    static void SendNew(entt::entity e, uint8_t type, uint32_t idx, const char* format, ...);
    static void SendNewV(entt::entity e, uint8_t type, uint32_t idx, const char* format, va_list args);

    static void Broadcast(entt::entity source, uint8_t type, const char* format, ...);
    static void BroadcastV(entt::entity source, uint8_t type, const char* format, va_list args);

#if defined(BL_OFFLINE_MESSAGE)
    // Offline messages live in the DB core: a whisper to a player who is not
    // online is stored there, and a player asks for theirs at login.
    static void SendOfflineMessage(entt::entity e, const char* to, const char* message);
    static void ReadOfflineMessages(entt::entity e);
    // When e last stored one; the whisper handler rate-limits on it.
    static uint32_t GetLastOfflineMessageTime(entt::entity e);
#endif
};

} // namespace ecs
