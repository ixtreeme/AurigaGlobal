#pragma once

#include <common/tables.h>

bool ValidateLoginByKey(const TPacketGDLoginByKey& request, const TAccountTable& account,
    uint32_t loginKey, const uint32_t* clientKey);
