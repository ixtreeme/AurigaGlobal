#pragma once

#include <common/tables.h>
#include <SQL/AsyncSQL.h>

struct PlayerCreateQuery
{
    std::string sql;
    SqlParams params;
};

PlayerCreateQuery BuildPlayerCreateQuery(const char* tablePostfix, uint32_t accountId,
    const TPlayerTable& player);
