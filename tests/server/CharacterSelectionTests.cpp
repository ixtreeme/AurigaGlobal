#include <Core/stdafx.h>
#include "../../SRC/Server/GameServer/network/character_selection.h"
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0, nameChecks = 0;
void Check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
int ValidateName(const char* name) { ++nameChecks; return std::strcmp(name, "ValidName") == 0; }
}
int (*check_name)(const char*) = ValidateName;

int main()
{
    try {
        using character_selection::PrepareChangeName;
        using Result = character_selection::ChangeNameResult;
        TAccountTable account {};
        account.id = 7;
        for (auto& player : account.players) { player.dwID = 42; player.bChangeName = true; }
        TPacketCGChangeName request {};
        std::strcpy(request.name, "ValidName");
        TPacketGDChangeName output {};
        for (int i = 0; i < 256; ++i) {
            request.index = uint8_t(i);
            const auto result = PrepareChangeName(account, request, output);
            Check(result == (i < PLAYER_PER_ACCOUNT ? Result::Ready : Result::Ignore), "invalid character index accepted");
        }
        Check(nameChecks == PLAYER_PER_ACCOUNT, "invalid index reached name validation");
        Check(output.pid == 42 && std::string(output.name) == "ValidName", "valid request lost player or name");
        request.index = 4;
        account.players[4].bChangeName = false;
        Check(PrepareChangeName(account, request, output) == Result::Ignore, "unrequested rename allowed");
        account.players[4].bChangeName = true; account.players[4].dwID = 0;
        Check(PrepareChangeName(account, request, output) == Result::Ignore, "empty slot accepted");
        account.players[4].dwID = 42;
        std::memset(request.name, 'x', sizeof(request.name));
        const auto before = nameChecks;
        Check(PrepareChangeName(account, request, output) == Result::InvalidName && nameChecks == before,
            "unterminated name reached C-string validation");
        request.name[sizeof(request.name) - 1] = 0;
        Check(PrepareChangeName(account, request, output) == Result::InvalidName, "name policy was skipped");
        account.id = 0;
        Check(PrepareChangeName(account, request, output) == Result::Ignore, "unauthenticated account allowed");
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "character selection: " << checks << " checks passed\n";
}
