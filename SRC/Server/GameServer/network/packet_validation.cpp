#include "stdafx.h"
#include "packet_validation.h"
#include <Core/packet_string.h>
#include <common/Headers/Header_CG.h>
#include <common/Headers/CG_Packet.h>

namespace {
template <typename Packet, typename Check>
bool Validate(const void* data, std::size_t length, Check check)
{
    return data && length >= sizeof(Packet) && check(*static_cast<const Packet*>(data));
}
}

bool ValidateClientPacketStrings(uint8_t header, const void* data, std::size_t length)
{
    using packet_string::IsTerminated;
    switch (header) {
    case HEADER_CG_LOGIN:
        return Validate<TPacketCGLogin>(data, length, [](const auto& p) { return IsTerminated(p.login) && IsTerminated(p.passwd); });
    case HEADER_CG_LOGIN2:
        return Validate<TPacketCGLogin2>(data, length, [](const auto& p) { return IsTerminated(p.login); });
    case HEADER_CG_LOGIN3:
        return Validate<TPacketCGLogin3>(data, length, [](const auto& p) {
            return IsTerminated(p.login) && IsTerminated(p.passwd)
#ifdef ENABLE_HWID
                && IsTerminated(p.hwid)
#endif
                ;
        });
    case HEADER_CG_CHARACTER_CREATE:
        return Validate<TPacketCGPlayerCreate>(data, length, [](const auto& p) { return IsTerminated(p.name); });
    case HEADER_CG_CHARACTER_DELETE:
        return Validate<TPacketCGPlayerDelete>(data, length, [](const auto& p) { return IsTerminated(p.private_code); });
    case HEADER_CG_CHANGE_NAME:
        return Validate<TPacketCGChangeName>(data, length, [](const auto& p) { return IsTerminated(p.name); });
    case HEADER_CG_CLIENT_VERSION:
        return Validate<TPacketCGClientVersion>(data, length, [](const auto& p) { return IsTerminated(p.filename) && IsTerminated(p.timestamp); });
    case HEADER_CG_CLIENT_VERSION2:
        return Validate<TPacketCGClientVersion2>(data, length, [](const auto& p) { return IsTerminated(p.filename) && IsTerminated(p.timestamp); });
    case HEADER_CG_WHISPER:
        return Validate<TPacketCGWhisper>(data, length, [](const auto& p) { return IsTerminated(p.szNameTo); });
    case HEADER_CG_QUEST_INPUT_STRING:
        return Validate<TPacketCGQuestInputString>(data, length, [](const auto& p) { return IsTerminated(p.msg); });
    case HEADER_CG_ANSWER_MAKE_GUILD:
        return Validate<TPacketCGAnswerMakeGuild>(data, length, [](const auto& p) { return IsTerminated(p.guild_name); });
    case HEADER_CG_HACK:
        return Validate<TPacketCGHack>(data, length, [](const auto& p) { return IsTerminated(p.szBuf); });
    case HEADER_CG_MYSHOP:
        return Validate<TPacketCGMyShop>(data, length, [](const auto& p) { return IsTerminated(p.szSign); });
#ifdef __NEWPET_SYSTEM__
    case HEADER_CG_PetSetName:
        return Validate<TPacketCGRequestPetName>(data, length, [](const auto& p) { return IsTerminated(p.petname); });
#endif
#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
    case HEADER_CG_WHISPER_ADMIN:
        return Validate<TPacketCGWhisperAdmin>(data, length, [](const auto& p) { return IsTerminated(p.szText) && IsTerminated(p.szLang); });
#endif
    default:
        return true;
    }
}
