#pragma once
#include "MissionPassRules.hpp"
#include <nlohmann/json.hpp>

namespace AurigaMissionPass {
// Strict reader shared by offline validation and the future runtime adapter.
// A malformed config must not partially replace the running season.
inline Season ReadConfig(const nlohmann::json& doc) {
    const auto readUnsigned = [](const nlohmann::json& value) -> uint32_t {
        if ((!value.is_number_unsigned() && !value.is_number_integer()) ||
            (value.is_number_integer() && value.get<int64_t>() < 0) ||
            value.get<uint64_t>() > UINT32_MAX) throw std::invalid_argument("Invalid unsigned field");
        return value.get<uint32_t>();
    };
    const auto timestamp = [](const nlohmann::json& value) -> int64_t {
        if ((!value.is_number_unsigned() && !value.is_number_integer()) ||
            (value.is_number_unsigned() && value.get<uint64_t>() > INT64_MAX) ||
            value.get<int64_t>() < 0) throw std::invalid_argument("Invalid UTC timestamp");
        return value.get<int64_t>();
    };
    if (doc.at("schema_version") != 1 || doc.at("premium_purchase_enabled") != false)
        throw std::invalid_argument("Unsupported schema or premium purchasing enabled");
    Season s;
    s.id = readUnsigned(doc.at("season_id"));
    s.enabled = doc.at("enabled").get<bool>();
    s.titleKey = doc.at("title_key").get<std::string>();
    s.startsAt = timestamp(doc.at("starts_at_utc"));
    s.endsAt = timestamp(doc.at("ends_at_utc"));
    s.levels = readUnsigned(doc.at("levels"));
    s.xpPerLevel = readUnsigned(doc.at("xp_per_level"));
    const std::map<std::string, Kind> kinds {
        {"mob_kill",Kind::MobKill},{"boss_kill",Kind::BossKill},{"metin_kill",Kind::MetinKill},
        {"dungeon_complete",Kind::DungeonComplete},{"wood_gather",Kind::WoodGather},
        {"tree_felled",Kind::TreeFelled},{"herb_gather",Kind::HerbGather},
        {"ore_gather",Kind::OreGather},{"fish_catch",Kind::FishCatch},
        {"tool_training",Kind::ToolTraining},{"tool_upgrade",Kind::ToolUpgrade},
        {"explore",Kind::Explore},{"active_minute",Kind::ActiveMinute},{"guild_exp",Kind::GuildExp}};
    const std::map<std::string, Cadence> cadences {
        {"daily",Cadence::Daily},{"weekly",Cadence::Weekly},{"season",Cadence::Season}};
    const std::map<std::string, Group> groups {
        {"any",Group::Any},{"solo",Group::Solo},{"party",Group::Party},{"guild_party",Group::GuildParty}};
    if (!doc.at("missions").is_array() || !doc.at("rewards").is_array())
        throw std::invalid_argument("Missions and rewards must be arrays");
    for (const auto& row : doc.at("missions")) {
        Mission m;
        m.id = readUnsigned(row.at("id"));
        m.nameKey = row.at("name_key").get<std::string>();
        m.descriptionKey = row.at("description_key").get<std::string>();
        m.cadence = cadences.at(row.at("cadence").get<std::string>());
        m.kind = kinds.at(row.at("event").get<std::string>());
        m.group = groups.at(row.at("group").get<std::string>());
        auto counting = row.at("counting").get<std::string>();
        if (counting != "quantity" && counting != "unique_subject") throw std::invalid_argument("Unknown counting mode");
        m.counting = counting == "quantity" ? Counting::Quantity : Counting::UniqueSubject;
        m.goal = readUnsigned(row.at("goal")); m.xp = readUnsigned(row.at("xp"));
        m.minimumLevel = readUnsigned(row.at("minimum_level"));
        m.maximumLevelGap = readUnsigned(row.at("maximum_level_gap"));
        if (!row.at("subjects").is_array()) throw std::invalid_argument("Subjects must be an array");
        for (const auto& subject : row.at("subjects")) m.subjects.insert(readUnsigned(subject));
        s.missions.push_back(std::move(m));
    }
    for (const auto& row : doc.at("rewards")) {
        auto track = row.at("track").get<std::string>();
        if (track != "free" && track != "premium") throw std::invalid_argument("Unknown reward track");
        s.rewards.push_back({readUnsigned(row.at("level")), track == "free" ? Track::Free : Track::Premium,
            readUnsigned(row.at("item")), readUnsigned(row.at("count"))});
    }
    Validate(s);
    return s;
}
}
