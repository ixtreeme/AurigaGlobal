#include "../../SRC/Server/GameServer/quest/quest_data.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
int checks = 0;
void Check(bool okay, const char* why)
{
    ++checks;
    if (!okay) throw std::runtime_error(why);
}

struct Directory
{
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("auriga-quest-data-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Directory() { if (!std::filesystem::create_directory(path)) throw std::runtime_error("fixture directory already exists"); }
    ~Directory()
    {
        std::error_code ignored;
        std::filesystem::remove(path / "questnpc.json", ignored);
        std::filesystem::remove(path / "questcategory.json", ignored);
        std::filesystem::remove(path, ignored);
    }
    void Write(const char* file, const std::string& text) { std::ofstream(path / file, std::ios::binary) << text; }
};

void TypedRecords()
{
    using Json = nlohmann::json;
    std::string error;
    std::vector<quest::data::Npc> npcs;
    std::map<std::string, uint16_t> categories;
    Check(quest::data::ParseNpcs(Json::parse(R"({"npcs":[{"vnum":1,"name":"blacksmith"},{"vnum":4294967295,"name":"Guard_2"}]})"), npcs, error), "valid NPC list rejected");
    Check(npcs.size() == 2 && npcs[0].vnum == 1 && npcs[1].vnum == UINT32_MAX && npcs[1].name == "Guard_2", "NPC directory association changed");
    Check(quest::data::ParseCategories(Json::parse(R"({"categories":[{"quest":"main","category":0},{"quest":"guild_war_join","category":3},{"quest":"edge","category":65535}]})"), categories, error), "valid category list rejected");
    Check(categories.size() == 3 && categories.at("main") == 0 && categories.at("guild_war_join") == 3 && categories.at("edge") == UINT16_MAX, "category value changed");

    for (const auto* invalid : {
        R"([])", R"({})", R"({"npcs":{}})", R"({"npcs":null})", R"({"npcs":[null]})",
        R"({"npcs":[{"vnum":1}]})", R"({"npcs":[{"vnum":1,"name":"a","typo":0}]})",
        R"({"npcs":[{"vnum":0,"name":"a"}]})", R"({"npcs":[{"vnum":1,"name":"notarget"}]})",
        R"({"npcs":[{"vnum":-1,"name":"a"}]})", R"({"npcs":[{"vnum":4294967296,"name":"a"}]})",
        R"({"npcs":[{"vnum":1.0,"name":"a"}]})", R"({"npcs":[{"vnum":true,"name":"a"}]})",
        R"({"npcs":[{"vnum":"1","name":"a"}]})", R"({"npcs":[{"vnum":1,"name":""}]})",
        R"({"npcs":[{"vnum":1,"name":"../outside"}]})", R"({"npcs":[{"vnum":1,"name":"12"}]})",
        R"({"npcs":[{"vnum":1,"name":"a\u0000b"}]})",
        R"({"npcs":[{"vnum":1,"name":"a"},{"vnum":1,"name":"b"}]})",
        R"({"npcs":[{"vnum":1,"name":"a"},{"vnum":2,"name":"a"}]})"})
    {
        Check(!quest::data::ParseNpcs(Json::parse(invalid), npcs, error) && !error.empty(), "invalid NPC data accepted");
        Check(npcs.size() == 2 && npcs.back().vnum == UINT32_MAX, "failed NPC validation replaced active data");
    }
    for (const auto* invalid : {
        R"({"categories":[{}]})", R"({"categories":[{"quest":"main"}]})",
        R"({"categories":[{"quest":"main","category":-1}]})",
        R"({"categories":[{"quest":"main","category":65536}]})",
        R"({"categories":[{"quest":"main","category":18446744073709551615}]})",
        R"({"categories":[{"quest":"main","category":1.5}]})",
        R"({"categories":[{"quest":"main","category":false}]})",
        R"({"categories":[{"quest":"main","category":"3"}]})",
        R"({"categories":[{"quest":4,"category":3}]})",
        R"({"categories":[{"quest":"a b","category":3}]})",
        R"({"categories":[{"quest":"main","category":3},{"quest":"main","category":4}]})"})
    {
        Check(!quest::data::ParseCategories(Json::parse(invalid), categories, error) && !error.empty(), "invalid category data accepted");
        Check(categories.size() == 3 && categories.at("guild_war_join") == 3, "failed category validation replaced active data");
    }
    Check(error.find("$.categories[1]") != std::string::npos && error.find("duplicate quest") != std::string::npos, "duplicate diagnostic lacks row and reason");
    Check(quest::data::ParseNpcs(Json::parse(R"({"npcs":[]})"), npcs, error) && npcs.empty(), "empty NPC list rejected");
    Check(quest::data::ParseCategories(Json::parse(R"({"categories":[]})"), categories, error) && categories.empty(), "empty category list rejected");
}

void Files()
{
    Directory dir;
    quest::data::Configuration data;
    data.npcs.push_back({9, "old"}); data.categories.emplace("old", 9);
    std::string error;
    Check(!quest::data::Load(dir.path.string(), data, error) && error.find("questnpc.json") != std::string::npos, "missing file not reported");
    dir.Write("questnpc.json", R"({"npcs":[{"vnum":1,"name":"new"}]})");
    dir.Write("questcategory.json", R"({"categories":[{"quest":"main"}]})");
    Check(!quest::data::Load(dir.path.string(), data, error) && error.find("questcategory.json: $.categories[0]") != std::string::npos, "second-file failure not reported");
    Check(data.npcs[0].name == "old" && data.categories.at("old") == 9, "pair of files loaded partially");
    dir.Write("questcategory.json", R"({"categories":[],"categories":[]})");
    Check(!quest::data::Load(dir.path.string(), data, error) && error.find("duplicate JSON field") != std::string::npos, "duplicate JSON field accepted");
    dir.Write("questcategory.json", R"({"categories":[{"quest":"main","category":1,"category":2}]})");
    Check(!quest::data::Load(dir.path.string(), data, error), "nested duplicate JSON field accepted");
    dir.Write("questcategory.json", std::string(R"({"categories":[]})") + " trailing");
    Check(!quest::data::Load(dir.path.string(), data, error), "trailing JSON garbage accepted");
    dir.Write("questcategory.json", R"({"categories":[]})");
    const auto loaded = quest::data::Load(dir.path.string(), data, error);
    Check(loaded, error.c_str());
    Check(data.npcs.size() == 1 && data.npcs[0].name == "new" && data.categories.empty() && error.empty(), "valid file pair did not replace snapshot");
}
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--dump")
        {
            quest::data::Configuration data;
            std::string error;
            if (!quest::data::Load(argv[2], data, error)) throw std::runtime_error(error);
            nlohmann::json doc = {{"npcs", nlohmann::json::array()}, {"categories", nlohmann::json::array()}};
            for (const auto& npc : data.npcs) doc["npcs"].push_back({{"vnum", npc.vnum}, {"name", npc.name}});
            for (const auto& [name, category] : data.categories) doc["categories"].push_back({{"quest", name}, {"category", category}});
            std::cout << doc.dump() << '\n';
            return 0;
        }
        if (argc != 1) throw std::runtime_error("usage: QuestDataTests [--dump directory]");
        TypedRecords(); Files();
        std::cout << "Quest data: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
