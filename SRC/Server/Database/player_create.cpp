#include <Core/stdafx.h>
#include "player_create.h"

PlayerCreateQuery BuildPlayerCreateQuery(const char* tablePostfix, uint32_t accountId,
    const TPlayerTable& player)
{
    PlayerCreateQuery query;
    query.sql = std::string("INSERT INTO player") + tablePostfix + " (";
    std::string values;
    auto add = [&](const char* column, SqlParam value) {
        if (!query.params.empty()) {
            query.sql += ", ";
            values += ", ";
        }
        query.sql += column;
        values += '?';
        query.params.push_back(std::move(value));
    };
    add("id", SqlParam::UInt(0));
    add("account_id", SqlParam::UInt(accountId));
    add("name", SqlParam::Text(player.name));
    add("level", SqlParam::UInt(player.level));
    add("st", SqlParam::UInt(player.st));
    add("ht", SqlParam::UInt(player.ht));
    add("dx", SqlParam::UInt(player.dx));
    add("iq", SqlParam::UInt(player.iq));
    add("job", SqlParam::UInt(player.job));
    add("voice", SqlParam::UInt(player.voice));
    add("dir", SqlParam::UInt(player.dir));
    add("x", SqlParam::Int(player.x));
    add("y", SqlParam::Int(player.y));
    add("z", SqlParam::Int(player.z));
    add("hp", SqlParam::UInt(player.hp));
    add("mp", SqlParam::UInt(player.sp));
    add("random_hp", SqlParam::Int(player.sRandomHP));
    add("random_sp", SqlParam::Int(player.sRandomSP));
    add("stat_point", SqlParam::Int(player.stat_point));
    add("stamina", SqlParam::Int(player.stamina));
    add("part_base", SqlParam::UInt(player.part_base));
    add("part_main", SqlParam::UInt(player.parts[PART_MAIN] ? player.parts[PART_MAIN] : player.part_base));
    add("part_hair", SqlParam::UInt(player.parts[PART_HAIR]));
    add("skill_group", SqlParam::UInt(player.skill_group));
    add("skill_point", SqlParam::Int(player.skill_point));
#ifdef ENABLE_ACCE_SYSTEM
    add("part_acce", SqlParam::UInt(0));
#endif
    add("gold", SqlParam::Int(player.gold));
#ifdef ENABLE_GAYA_SYSTEM
    add("gaya", SqlParam::Int(player.gaya));
#endif
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
    add("envanter", SqlParam::Int(player.envanter));
#endif
    add("playtime", SqlParam::UInt(0));
    add("skill_level", SqlParam::Blob(std::string(reinterpret_cast<const char*>(player.skills), sizeof(player.skills))));
    add("quickslot", SqlParam::Blob(std::string(reinterpret_cast<const char*>(player.quickslot), sizeof(player.quickslot))));
    query.sql += ") VALUES (" + values + ")";
    return query;
}
