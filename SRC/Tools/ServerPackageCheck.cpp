#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "../Server/GameServer/core/lua_incl.h"
#include "../Server/GameServer/core/JsonFile.h"
#include "../Server/GameServer/quest/quest_data.h"
#include "../Server/GameServer/quest/quest_source.h"
#include "../Server/GameServer/quest/quest_snapshot.h"

namespace { std::string dataRoot; }
const std::string& LocaleService_GetBasePath() { return dataRoot; }

int main(int argc, char** argv)
{
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--quest-fingerprint") {
        try {
            quest::RuntimeSnapshot snapshot;
            const auto root = std::filesystem::absolute(argv[2]);
            snapshot.Capture(root.generic_string(), argc == 4 ? argv[3] : (root / "quest").generic_string());
            nlohmann::json files = nlohmann::json::object();
            for (const auto& [path, bytes] : snapshot.files)
                files[path] = {{"sha256", quest::RuntimeSha256(bytes)}, {"size", bytes.size()}};
            std::cout << nlohmann::json({{"revision", snapshot.revision}, {"files", files}}).dump() << '\n'; return 0;
        } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    }
    if (argc == 3 && std::string(argv[1]) == "--check-libraries") {
        quest::RuntimeSnapshot snapshot;
        lua_State* state = luaL_newstate();
        if (!state) return 2;
        try {
            dataRoot = std::filesystem::absolute(argv[2]).generic_string();
            snapshot.originalDataRoot = dataRoot;
            snapshot.originalQuestRoot = (std::filesystem::path(dataRoot) / "quest").generic_string();
            snapshot.Capture(dataRoot, snapshot.originalQuestRoot);
            if (!LuaOpenQuestLibraries(state)) throw std::runtime_error(lua_tostring(state, -1));
            quest::RememberStandardAPIs(state, snapshot);
            // Bootstrap-only test doubles. No player/NPC APIs are executed.
            lua_pushcfunction(state, [](lua_State* L) { lua_pushlstring(L, dataRoot.data(), dataRoot.size()); return 1; });
            lua_setglobal(state, "get_locale_base_path");
            lua_pushcfunction(state, [](lua_State*) { return 0; }); lua_setglobal(state, "add_goto_info");
            lua_pushcfunction(state, [](lua_State*) { return 0; }); lua_setglobal(state, "add_bgm_info");
            lua_pushcfunction(state, [](lua_State*) { return 0; }); lua_setglobal(state, "set_bgm_volume_enable");
            lua_pushcfunction(state, [](lua_State*) { return 0; }); lua_setglobal(state, "add_restart_city_pos");
            for (const auto* name : {"q", "pc", "npc", "d", "game", "item", "party", "guild", "horse", "affect", "arena"}) {
                lua_newtable(state); lua_setglobal(state, name);
            }
            lua_getglobal(state, "arena"); lua_pushcfunction(state, [](lua_State*) { return 0; });
            lua_setfield(state, -2, "add_map"); lua_pop(state, 1);
            lua_getglobal(state, "q"); lua_getglobal(state, "coroutine"); lua_getfield(state, -1, "yield");
            lua_remove(state, -2); lua_setfield(state, -2, "yield"); lua_pop(state, 1);
            snapshot.staging = true;
            quest::GuardRuntimeAPIs(state, snapshot);
            for (const auto* relative : {"settings.lua", "quest/questlib.lua", "translate.lua", "quest/locale.lua"}) {
                const auto path = (std::filesystem::path(dataRoot) / relative).string();
                if (lua_dofile(state, path.c_str()) != LUA_OK) throw std::runtime_error(lua_tostring(state, -1));
                lua_settop(state, 0);
            }
            const auto catalog = quest::source::LoadFiles(state, snapshot.files);
            for (const auto& definition : catalog) {
                if (lua_dobuffer(state, definition.stateCode.data(), definition.stateCode.size(), definition.file.c_str()) != LUA_OK)
                    throw std::runtime_error(lua_tostring(state, -1));
                lua_settop(state, 0);
            }
            auto* dialog = lua_newthread(state);
            const int reference = luaL_ref(state, LUA_REGISTRYINDEX);
            const char* sample = "return select('one','two')";
            if (luaL_loadbufferx(dialog, sample, std::char_traits<char>::length(sample), "@library-dialog-test", "t") != LUA_OK)
                throw std::runtime_error(lua_tostring(dialog, -1));
            int results = 0, status;
            { LuaExecutionBudget budget(dialog); status = lua_resume(dialog, state, 0, &results); }
            if (status != LUA_YIELD || results != 2 || !lua_istable(dialog, -1) || lua_rawlen(dialog, -1) != 2)
                throw std::runtime_error("questlib select() is not Lua 5.5 compatible; run prepare_quests.py");
            lua_closethread(dialog, state); luaL_unref(state, LUA_REGISTRYINDEX, reference);
            quest::CloseLuaRuntime(state); std::cout << "LUA_LIBRARIES_OK bootstrap_only=1\n"; return 0;
        } catch (const std::exception& error) { std::cerr << error.what() << '\n'; quest::CloseLuaRuntime(state); return 1; }
    }
    if (argc == 3 && (std::string(argv[1]) == "--check-quest" || std::string(argv[1]) == "--check-quests" || std::string(argv[1]) == "--inspect-quest" || std::string(argv[1]) == "--inspect-quests")) {
        lua_State* state = luaL_newstate();
        if (!state) return 2;
        try {
            if (std::string(argv[1]) != "--check-quests" && std::string(argv[1]) != "--inspect-quests") {
                std::ifstream input(argv[2], std::ios::binary);
                if (!input) throw std::runtime_error("cannot open quest source");
                const std::string text(std::istreambuf_iterator<char>(input), {});
                auto definition = quest::source::ParseLegacy(text, std::filesystem::path(argv[2]).filename().string());
                quest::source::Validate(state, definition);
                if (std::string(argv[1]) == "--inspect-quest") {
                    nlohmann::json value = {{"id", definition.name}, {"states", definition.states}, {"handlers", definition.handlers.size()}};
                    std::cout << value.dump() << '\n';
                } else std::cout << "QUEST_SYNTAX_OK\n";
            } else {
                auto catalog = quest::source::Load(state, argv[2]);
                if (std::string(argv[1]) == "--inspect-quests") {
                    nlohmann::json rows = nlohmann::json::array();
                    for (const auto& definition : catalog) rows.push_back({{"name", definition.name}, {"states", definition.states}});
                    std::cout << rows.dump() << '\n'; lua_close(state); return 0;
                }
                size_t handlers = 0; for (const auto& q : catalog) handlers += q.handlers.size();
                std::cout << "QUEST_SOURCES_OK quests=" << catalog.size() << " handlers=" << handlers << '\n';
            }
            lua_close(state); return 0;
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n'; lua_close(state); return 1;
        }
    }
    // Parse only. Never open libraries or execute the user's chunk.
    if (argc == 3 && std::string(argv[1]) == "--check-lua") {
        lua_State* state = lua_open();
        if (!state) return 2;
        const int code = luaL_loadfilex(state, argv[2], "t");
        if (code) std::cerr << lua_tostring(state, -1) << '\n';
        else std::cout << "LUA_SYNTAX_OK\n";
        lua_close(state);
        return code ? 1 : 0;
    }
    const bool overrideQuests = argc == 4 && std::string(argv[2]) == "--quest-runtime";
    if (argc != 2 && !overrideQuests) { std::cerr << "Usage: ServerPackageCheck <data-directory> [--quest-runtime <snapshot-data-directory>]\n"; return 2; }
    try {
        const auto root = std::filesystem::absolute(argv[1]).lexically_normal();
        const auto runtimeRoot = overrideQuests ? std::filesystem::absolute(argv[3]).lexically_normal() : root;
        dataRoot = root.generic_string();
        if (!std::filesystem::is_directory(root)) throw std::runtime_error("Data directory missing");
        quest::data::Configuration quests;
        std::string error;
        if (!quest::data::Load((runtimeRoot / "quest").string(), quests, error)) throw std::runtime_error(error);
        struct LuaGuard { lua_State* state = lua_open(); ~LuaGuard() { if (state) lua_close(state); } } lua;
        if (!lua.state) throw std::runtime_error("Cannot initialize Lua parser");
        const auto sources = quest::source::Load(lua.state, runtimeRoot / "quest");
        std::set<std::string> sourceFiles;
        for (const auto& source : sources) sourceFiles.insert("quest/" + source.file);
        size_t jsonCount = 0, luaCount = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;
            const auto relative = entry.path().lexically_relative(root);
            const auto ext = entry.path().extension().string();
            if (ext == ".json" && !(overrideQuests && relative.generic_string().starts_with("quest/"))) {
                nlohmann::json doc;
                if (!JsonFile::Load("data/" + relative.generic_string(), doc))
                    throw std::runtime_error("Invalid JSON: " + relative.generic_string());
                ++jsonCount;
            }
            const auto rel = relative.generic_string();
            const bool object = rel.rfind("quest/object/", 0) == 0;
            const bool runtimeLua = ext == ".lua" && rel.rfind("quest/Quest/", 0) != 0 && rel.rfind("quest/pre_qc/", 0) != 0 && !object;
            if (!runtimeLua || overrideQuests || sourceFiles.contains(rel)) continue;
            const int result = luaL_loadfilex(lua.state, entry.path().string().c_str(), "t");
            if (result != 0) {
                std::cerr << "LUA_INVALID " << rel << ": " << lua_tostring(lua.state, -1) << '\n';
                return 1;
            }
            lua_settop(lua.state, 0);
            ++luaCount;
        }
        if (overrideQuests) {
            quest::RuntimeSnapshot snapshot;
            snapshot.Capture(runtimeRoot.generic_string(), (runtimeRoot / "quest").generic_string());
            for (const auto& [path, bytes] : snapshot.files) {
                if (!path.ends_with(".lua") || path.starts_with("quest/Quest/") || sourceFiles.contains(path)) continue;
                if (snapshot.LoadFile(lua.state, path) != LUA_OK)
                    throw std::runtime_error(lua_tostring(lua.state, -1));
                lua_settop(lua.state, 0); ++luaCount;
            }
        }
        std::cout << "PACKAGE_DATA_OK json=" << jsonCount << " lua=" << luaCount
                  << " quests=" << sources.size()
                  << " quest_npcs=" << quests.npcs.size() << " quest_categories=" << quests.categories.size() << '\n';
        return 0;
    } catch (const std::exception& ex) { std::cerr << "PACKAGE_DATA_INVALID " << ex.what() << '\n'; return 1; }
}
