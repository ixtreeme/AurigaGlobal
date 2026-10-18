#include <Core/stdafx.h>
#include "../../SRC/Server/Database/login_validation.h"
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void Check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
}
int main()
{
    try {
        TAccountTable account {};
        strlcpy(account.login, "player", sizeof(account.login));
        TPacketGDLoginByKey request {};
        strlcpy(request.szLogin, "player", sizeof(request.szLogin));
        strlcpy(request.szIP, "127.0.0.1", sizeof(request.szIP));
        request.dwLoginKey = 123;
        const uint32_t clientKey[4] = { 1, 2, 3, 4 };
        std::memcpy(request.adwClientKey, clientKey, sizeof(clientKey));
        auto valid = [&] { return ValidateLoginByKey(request, account, 123, clientKey); };
        Check(valid(), "matching credentials rejected");
        strlcpy(request.szLogin, "other", sizeof(request.szLogin));
        Check(!valid(), "different login accepted with a valid key");
        strlcpy(request.szLogin, " \tPlAyEr\t ", sizeof(request.szLogin));
        Check(valid(), "normalized login rejected");
        ++request.dwLoginKey;
        Check(!valid(), "wrong login key accepted");
        --request.dwLoginKey;
        for (auto& part : request.adwClientKey) {
            ++part;
            Check(!valid(), "wrong client key accepted");
            --part;
        }
        std::memset(request.szLogin, ' ', sizeof(request.szLogin));
        Check(!valid(), "unterminated whitespace login accepted");
        request.szLogin[0] = '\0';
        Check(!valid(), "empty login accepted");
        strlcpy(request.szLogin, "player", sizeof(request.szLogin));
        std::memset(request.szIP, 'x', sizeof(request.szIP));
        Check(!valid(), "unterminated IP accepted");
        request.szIP[0] = '\0';
        std::memset(account.login, 'x', sizeof(account.login));
        Check(!valid(), "unterminated registered login accepted");
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "login by key: " << checks << " checks passed\n";
}
