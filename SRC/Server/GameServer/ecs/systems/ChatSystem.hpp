#pragma once

#include <cstdint>
#include <cstdarg>

#include <entt/entt.hpp>

namespace ecs {

class ChatSystem {
public:
    static void Send(entt::entity e, uint8_t type, const char* format, ...);
    static void SendV(entt::entity e, uint8_t type, const char* format, va_list args);

    static void SendNew(entt::entity e, uint8_t type, uint32_t idx, const char* format, ...);
    static void SendNewV(entt::entity e, uint8_t type, uint32_t idx, const char* format, va_list args);

    static void Broadcast(entt::entity source, uint8_t type, const char* format, ...);
    static void BroadcastV(entt::entity source, uint8_t type, const char* format, va_list args);
};

} // namespace ecs
