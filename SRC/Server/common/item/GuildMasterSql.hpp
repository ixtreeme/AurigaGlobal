#pragma once

#include <SQL/PreparedTransaction.h>
#include <cstdint>
#include <string>
#include <string_view>

namespace auriga::item
{
inline void WriteGuildMasterChange(SqlTransaction& transaction, std::uint32_t guild,
    std::uint32_t from, std::uint32_t to, std::string_view tablePostfix = {})
{
    const std::string suffix(tablePostfix);
    using P = SqlParam;
    transaction.push_back({"UPDATE player.guild" + suffix + " SET master=? WHERE id=? AND master=?",
        {P::UInt(to), P::UInt(guild), P::UInt(from)}, 1, {}});
    transaction.push_back({"UPDATE player.guild_member" + suffix + " SET grade=CASE WHEN pid=? THEN 1 ELSE 15 END "
        "WHERE guild_id=? AND ((pid=? AND grade=1) OR (pid=? AND grade BETWEEN 2 AND 15))",
        {P::UInt(to), P::UInt(guild), P::UInt(from), P::UInt(to)}, 2, {}});
}
}
