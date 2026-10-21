#pragma once

#include <nlohmann/json_fwd.hpp>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace quest::data
{
struct Npc
{
    uint32_t vnum;
    std::string name;
};

struct Configuration
{
    std::vector<Npc> npcs;
    std::map<std::string, uint16_t> categories;
};

// Strict typed input. On failure the destination remains unchanged.
bool ParseNpcs(const nlohmann::json& doc, std::vector<Npc>& out, std::string& error);
bool ParseCategories(const nlohmann::json& doc, std::map<std::string, uint16_t>& out, std::string& error);
bool Load(const std::string& directory, Configuration& out, std::string& error);
bool ParseConfiguration(const std::string& npcs, const std::string& categories, Configuration& out, std::string& error);
}
