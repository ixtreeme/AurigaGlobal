#include "../../stdafx.h"
#include "PlayerRuntimeSystem.hpp"
#include "../AIHelpers.hpp"

#include "ChatSystem.hpp"

#include "../../buffer_manager.h"
#include "../../char.h"
#include "../../config.h"
#include "../../desc.h"
#include "../../packet.h"
#include "../CharacterAccessors.hpp"
#include <Core/Logging.hpp>

namespace {

int FormatChatMessage(char* buffer, size_t bufferSize, const char* format, va_list args)
{
    if (!buffer || bufferSize == 0 || !format) {
        return -1;
    }

    va_list copy;
    va_copy(copy, args);
    const int len = vsnprintf(buffer, bufferSize, format, copy);
    va_end(copy);

    if (len < 0) {
        return -1;
    }

    return std::min<int>(len, static_cast<int>(bufferSize - 1));
}

} // namespace

namespace ecs {

void ChatSystem::Send(entt::entity e, uint8_t type, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    SendV(e, type, format, args);
    va_end(args);
}

void ChatSystem::SendV(entt::entity e, uint8_t type, const char* format, va_list args)
{
    auto* ch = LegacyCharOf(e);
    if (!ch || !format) {
        return;
    }

    LPDESC d = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));
    if (!d) {
        return;
    }

    char chatbuf[CHAT_MAX_LEN + 1];
    const int len = FormatChatMessage(chatbuf, sizeof(chatbuf), format, args);
    if (len < 0) {
        return;
    }

    packet_chat packChat;
    packChat.header = HEADER_GC_CHAT;
    packChat.size = sizeof(packet_chat) + len;
    packChat.type = type;
    packChat.id = 0;
    packChat.bEmpire = d->GetEmpire();

    TEMP_BUFFER buf;
    buf.write(&packChat, sizeof(packet_chat));
    if (len > 0) {
        buf.write(chatbuf, len);
    }

    d->Packet(buf.read_peek(), buf.size());

    if (type == CHAT_TYPE_COMMAND && test_server) {
        LOG_INFO("SEND_COMMAND {} {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), chatbuf);
    }
}

void ChatSystem::SendNew(entt::entity e, uint8_t type, uint32_t idx, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    SendNewV(e, type, idx, format, args);
    va_end(args);
}

void ChatSystem::SendNewV(entt::entity e, uint8_t type, uint32_t idx, const char* format, va_list args)
{
    auto* ch = LegacyCharOf(e);
    if (!ch || !format) {
        return;
    }

    if (type != CHAT_TYPE_INFO &&
        type != CHAT_TYPE_NOTICE &&
        type != CHAT_TYPE_BIG_NOTICE
#ifdef ENABLE_DICE_SYSTEM
        && type != CHAT_TYPE_DICE_INFO
#endif
#ifdef ENABLE_NEW_CHAT
        && type != CHAT_TYPE_INFO_EXP
        && type != CHAT_TYPE_INFO_ITEM
        && type != CHAT_TYPE_INFO_VALUE
#endif
        && type != CHAT_TYPE_DIALOG) {
        return;
    }

    LPDESC d = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));
    if (!d) {
        return;
    }

    char chatbuf[256];
    const int len = FormatChatMessage(chatbuf, sizeof(chatbuf), format, args);
    if (len < 0) {
        return;
    }

    TPacketGCChatNew packet;
    packet.header = HEADER_GC_CHAT_NEW;
    packet.type = type;
    packet.idx = idx;
    packet.size = sizeof(packet) + len;

    TEMP_BUFFER buf;
    buf.write(&packet, sizeof(packet));
    if (len > 0) {
        buf.write(chatbuf, len);
    }

    d->Packet(buf.read_peek(), buf.size());
}

void ChatSystem::Broadcast(entt::entity source, uint8_t type, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    BroadcastV(source, type, format, args);
    va_end(args);
}

void ChatSystem::BroadcastV(entt::entity source, uint8_t type, const char* format, va_list args)
{
    auto* ch = LegacyCharOf(source);
    if (!ch || !format) {
        return;
    }

    char chatbuf[CHAT_MAX_LEN + 1];
    const int len = FormatChatMessage(chatbuf, sizeof(chatbuf), format, args);
    if (len < 0) {
        return;
    }

    TPacketGCChat packet;
    packet.header = HEADER_GC_CHAT;
    packet.size = sizeof(TPacketGCChat) + len;
    packet.type = type;
    packet.id = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(ch));
    packet.bEmpire = 0;

    TEMP_BUFFER buf;
    buf.write(&packet, sizeof(TPacketGCChat));
    if (len > 0) {
        buf.write(chatbuf, len);
    }

    ch->PacketAround(buf.read_peek(), buf.size());
}

} // namespace ecs
