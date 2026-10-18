#include <Core/stdafx.h>
#include <Core/packet_string.h>
#include <Core/utils.h>
#include "login_validation.h"

bool ValidateLoginByKey(const TPacketGDLoginByKey& request, const TAccountTable& account,
    uint32_t loginKey, const uint32_t* clientKey)
{
    if (!loginKey || request.dwLoginKey != loginKey || !clientKey ||
        !packet_string::IsTerminated(request.szLogin) ||
        !packet_string::IsTerminated(request.szIP) ||
        !packet_string::IsTerminated(account.login))
        return false;

    char requested[LOGIN_MAX_LEN + 1];
    char registered[LOGIN_MAX_LEN + 1];
    trim_and_lower(request.szLogin, sizeof(request.szLogin), requested, sizeof(requested));
    trim_and_lower(account.login, sizeof(account.login), registered, sizeof(registered));
    return requested[0] && std::strcmp(requested, registered) == 0 &&
        std::memcmp(clientKey, request.adwClientKey, sizeof(request.adwClientKey)) == 0;
}
