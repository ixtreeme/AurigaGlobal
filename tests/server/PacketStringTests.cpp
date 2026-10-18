#include <Core/stdafx.h>
#include <Core/utils.h>
#include <Core/packet_string.h>
#include <common/tables.h>
#include <common/Headers/Header_CG.h>
#include <common/Headers/CG_Packet.h>
#include "../../SRC/Server/GameServer/network/packet_validation.h"
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
int checks = 0;
void Check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
template <typename Packet, size_t N>
void Field(uint8_t header, char (Packet::*member)[N])
{
    auto storage = std::make_unique<unsigned char[]>(sizeof(Packet));
    auto* packet = reinterpret_cast<Packet*>(storage.get());
    std::memset(storage.get(), 0, sizeof(Packet));
    auto& field = packet->*member;
    std::memset(field, 'a', N);
    Check(!ValidateClientPacketStrings(header, packet, sizeof(Packet)), "unterminated field accepted");
    field[N - 1] = '\0';
    Check(ValidateClientPacketStrings(header, packet, sizeof(Packet)), "last-byte terminator rejected");
    for (size_t length = 0; length < sizeof(Packet); ++length)
        Check(!ValidateClientPacketStrings(header, packet, length), "truncated packet accepted");
    std::memset(storage.get(), 'a', sizeof(Packet));
    Check(!ValidateClientPacketStrings(header, packet, sizeof(Packet)), "packet without any terminator accepted");
}
}
int main()
{
    try {
        Field(HEADER_CG_LOGIN, &TPacketCGLogin::login);
        Field(HEADER_CG_LOGIN, &TPacketCGLogin::passwd);
        Field(HEADER_CG_LOGIN2, &TPacketCGLogin2::login);
        Field(HEADER_CG_LOGIN3, &TPacketCGLogin3::login);
        Field(HEADER_CG_LOGIN3, &TPacketCGLogin3::passwd);
#ifdef ENABLE_HWID
        Field(HEADER_CG_LOGIN3, &TPacketCGLogin3::hwid);
#endif
        Field(HEADER_CG_CHARACTER_CREATE, &TPacketCGPlayerCreate::name);
        Field(HEADER_CG_CHARACTER_DELETE, &TPacketCGPlayerDelete::private_code);
        Field(HEADER_CG_CHANGE_NAME, &TPacketCGChangeName::name);
        Field(HEADER_CG_CLIENT_VERSION, &TPacketCGClientVersion::filename);
        Field(HEADER_CG_CLIENT_VERSION, &TPacketCGClientVersion::timestamp);
        Field(HEADER_CG_CLIENT_VERSION2, &TPacketCGClientVersion2::filename);
        Field(HEADER_CG_CLIENT_VERSION2, &TPacketCGClientVersion2::timestamp);
        Field(HEADER_CG_WHISPER, &TPacketCGWhisper::szNameTo);
        Field(HEADER_CG_QUEST_INPUT_STRING, &TPacketCGQuestInputString::msg);
        Field(HEADER_CG_ANSWER_MAKE_GUILD, &TPacketCGAnswerMakeGuild::guild_name);
        Field(HEADER_CG_HACK, &TPacketCGHack::szBuf);
        Field(HEADER_CG_MYSHOP, &TPacketCGMyShop::szSign);
#ifdef __ENABLE_NEW_OFFLINESHOP__
        auto shop = std::make_unique<TSubPacketCGShopCreate>();
        std::memset(shop.get(), 'a', sizeof(*shop));
        Check(!packet_string::IsTerminated(shop->shop.szName), "unterminated offline shop name accepted");
        shop->shop.szName[sizeof(shop->shop.szName) - 1] = '\0';
        Check(packet_string::IsTerminated(shop->shop.szName), "terminated offline shop name rejected");
        auto rename = std::make_unique<TSubPacketCGShopChangeName>();
        std::memset(rename.get(), 'a', sizeof(*rename));
        Check(!packet_string::IsTerminated(rename->szName), "unterminated offline shop rename accepted");
#endif
#ifdef __NEWPET_SYSTEM__
        Field(HEADER_CG_PetSetName, &TPacketCGRequestPetName::petname);
#endif
#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
        Field(HEADER_CG_WHISPER_ADMIN, &TPacketCGWhisperAdmin::szText);
        Field(HEADER_CG_WHISPER_ADMIN, &TPacketCGWhisperAdmin::szLang);
#endif
        char output[8];
        const char spaces[32] = {' ', ' ', ' '};
        Check(trim_and_lower(spaces, sizeof(spaces), output, sizeof(output)) && !output[0], "all-whitespace normalization failed");
        auto raw = std::make_unique<char[]>(32);
        std::memset(raw.get(), ' ', 32);
        Check(!trim_and_lower(raw.get(), 32, output, sizeof(output)) && !output[0], "unterminated whitespace normalized");
        Check(!trim_and_lower(raw.get(), 0, output, sizeof(output)), "zero-sized source accepted");
        Check(trim_and_lower(" \tAbC\t ", 8, output, sizeof(output)) && std::strcmp(output, "abc") == 0, "normalization changed");
        Check(!trim_and_lower(nullptr, 0, output, sizeof(output)) && !output[0], "null source accepted");
        Check(!trim_and_lower("x", 2, output, 0), "zero-sized destination accepted");
        char overlap[] = "  AB  ";
        Check(trim_and_lower(overlap, sizeof(overlap), overlap, sizeof(overlap)) && std::strcmp(overlap, "ab") == 0, "in-place normalization broken");
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "packet strings: " << checks << " checks passed\n";
}
