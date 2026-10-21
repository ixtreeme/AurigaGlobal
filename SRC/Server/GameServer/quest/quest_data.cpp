#include "quest_data.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>

namespace quest::data
{
bool ParseConfiguration(const std::string& npcs, const std::string& categories, Configuration& out, std::string& error) {
    try {
        auto parse = [](const std::string& text) {
            std::vector<std::set<std::string>> keys;
            return nlohmann::json::parse(text, [&](int, nlohmann::json::parse_event_t event, nlohmann::json& value) {
                if (event == nlohmann::json::parse_event_t::object_start) keys.emplace_back();
                else if (event == nlohmann::json::parse_event_t::object_end) keys.pop_back();
                else if (event == nlohmann::json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
                    throw std::runtime_error("duplicate quest metadata field");
                return true;
            });
        };
        Configuration candidate;
        if (!ParseNpcs(parse(npcs), candidate.npcs, error) || !ParseCategories(parse(categories), candidate.categories, error)) return false;
        out = std::move(candidate); return true;
    } catch (const std::exception& ex) { error = ex.what(); return false; }
}
namespace
{
using Json = nlohmann::json;

void Object(const Json& value, std::initializer_list<std::string_view> fields, const std::string& where)
{
    if (!value.is_object()) throw std::runtime_error(where + ": expected an object");
    for (const auto field : fields)
        if (!value.contains(std::string(field)))
            throw std::runtime_error(where + ": missing field '" + std::string(field) + "'");
    for (const auto& [key, ignored] : value.items())
        if (std::find(fields.begin(), fields.end(), key) == fields.end())
            throw std::runtime_error(where + ": unknown field '" + key + "'");
}

uint64_t Unsigned(const Json& value, uint64_t maximum, const std::string& where)
{
    if (!value.is_number_integer() ||
        (!value.is_number_unsigned() && value.get<int64_t>() < 0))
        throw std::runtime_error(where + ": expected a nonnegative integer");
    const auto result = value.get<uint64_t>();
    if (result > maximum) throw std::runtime_error(where + ": integer out of range");
    return result;
}

std::string Identifier(const Json& value, const std::string& where)
{
    if (!value.is_string()) throw std::runtime_error(where + ": expected an identifier string");
    auto name = value.get<std::string>();
    const auto letter = [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    if (name.empty() || !letter(name.front()) ||
        !std::all_of(name.begin(), name.end(), [&](unsigned char c) { return letter(c) || (c >= '0' && c <= '9'); }))
        throw std::runtime_error(where + ": expected [A-Za-z_][A-Za-z0-9_]*");
    return name;
}

bool Read(const std::string& path, Json& doc, std::string& error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) { error = path + ": cannot open file"; return false; }
    try
    {
        std::vector<std::set<std::string>> keys;
        doc = Json::parse(input, [&](int, Json::parse_event_t event, Json& value) {
            if (event == Json::parse_event_t::object_start) keys.emplace_back();
            else if (event == Json::parse_event_t::object_end) keys.pop_back();
            else if (event == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
                throw std::runtime_error("duplicate JSON field '" + value.get<std::string>() + "'");
            return true;
        });
        return true;
    }
    catch (const std::exception& ex) { error = path + ": " + ex.what(); return false; }
}
}

bool ParseNpcs(const Json& doc, std::vector<Npc>& out, std::string& error)
{
    try
    {
        Object(doc, {"npcs"}, "$");
        const auto& rows = doc.at("npcs");
        if (!rows.is_array()) throw std::runtime_error("$.npcs: expected an array");
        std::vector<Npc> parsed;
        std::set<uint32_t> vnums;
        std::set<std::string> names;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto where = "$.npcs[" + std::to_string(i) + "]";
            const auto& row = rows[i];
            Object(row, {"vnum", "name"}, where);
            const auto vnum = static_cast<uint32_t>(Unsigned(row.at("vnum"), UINT32_MAX, where + ".vnum"));
            auto name = Identifier(row.at("name"), where + ".name");
            if (vnum == 0 || name == "notarget") throw std::runtime_error(where + ": 0/notarget is reserved");
            if (!vnums.insert(vnum).second) throw std::runtime_error(where + ": duplicate NPC vnum " + std::to_string(vnum));
            if (!names.insert(name).second) throw std::runtime_error(where + ": duplicate NPC name '" + name + "'");
            parsed.push_back({vnum, std::move(name)});
        }
        out = std::move(parsed);
        error.clear();
        return true;
    }
    catch (const std::exception& ex) { error = ex.what(); return false; }
}

bool ParseCategories(const Json& doc, std::map<std::string, uint16_t>& out, std::string& error)
{
    try
    {
        Object(doc, {"categories"}, "$");
        const auto& rows = doc.at("categories");
        if (!rows.is_array()) throw std::runtime_error("$.categories: expected an array");
        std::map<std::string, uint16_t> parsed;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto where = "$.categories[" + std::to_string(i) + "]";
            const auto& row = rows[i];
            Object(row, {"quest", "category"}, where);
            auto name = Identifier(row.at("quest"), where + ".quest");
            const auto category = static_cast<uint16_t>(Unsigned(row.at("category"), UINT16_MAX, where + ".category"));
            if (!parsed.emplace(name, category).second) throw std::runtime_error(where + ": duplicate quest '" + name + "'");
        }
        out = std::move(parsed);
        error.clear();
        return true;
    }
    catch (const std::exception& ex) { error = ex.what(); return false; }
}

bool Load(const std::string& directory, Configuration& out, std::string& error)
{
    Configuration parsed;
    Json doc;
    const auto npcs = directory + "/questnpc.json";
    const auto categories = directory + "/questcategory.json";
    if (!Read(npcs, doc, error)) return false;
    if (!ParseNpcs(doc, parsed.npcs, error)) { error = npcs + ": " + error; return false; }
    if (!Read(categories, doc, error)) return false;
    if (!ParseCategories(doc, parsed.categories, error)) { error = categories + ": " + error; return false; }
    out = std::move(parsed);
    error.clear();
    return true;
}
}
