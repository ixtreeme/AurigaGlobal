#include "quest_snapshot.h"
#include <filesystem>
#include <vector>
#include <stdexcept>
#include <openssl/evp.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <nlohmann/json.hpp>

namespace quest {
namespace {
char stagingKey;
char bootstrapKey;
std::string Hash(const std::string& bytes) {
    unsigned char digest[EVP_MAX_MD_SIZE]; unsigned int length = 0;
    if (!EVP_Digest(bytes.data(), bytes.size(), digest, &length, EVP_sha256(), nullptr))
        throw std::runtime_error("quest snapshot SHA256 failed");
    std::ostringstream out;
    for (unsigned int i = 0; i < length; ++i) out << std::hex << std::setfill('0') << std::setw(2) << unsigned(digest[i]);
    return out.str();
}
std::string Read(const std::filesystem::path& path) {
    if (std::filesystem::file_size(path) > 8 * 1024 * 1024) throw std::runtime_error("quest file too large: " + path.generic_string());
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read quest file: " + path.generic_string());
    std::string bytes(std::istreambuf_iterator<char>(input), {});
    if (bytes.size() > 8 * 1024 * 1024) throw std::runtime_error("quest file changed while capturing");
    return bytes;
}
bool Within(const std::filesystem::path& path, const std::filesystem::path& root) {
    const auto relative = path.lexically_relative(root);
    return !relative.empty() && *relative.begin() != "..";
}
int FrozenLoadFile(lua_State* L) {
    auto* runtime = static_cast<RuntimeSnapshot*>(lua_touserdata(L, lua_upvalueindex(1)));
    const char* path = luaL_checkstring(L, 1);
    const char* mode = luaL_optstring(L, 2, "t");
    if (std::strcmp(mode, "t")) { lua_pushnil(L); lua_pushliteral(L, "only text Lua is allowed"); return 2; }
    try {
        const int result = runtime->LoadFile(L, path);
        if (result == LUA_OK) {
            if (!lua_isnoneornil(L, 3)) { lua_pushvalue(L, 3); lua_setupvalue(L, -2, 1); }
            return 1;
        }
        lua_pushnil(L); lua_insert(L, -2); return 2;
    } catch (const std::exception& error) {
        lua_pushnil(L); lua_pushstring(L, error.what()); return 2;
    }
}
struct NativeFunction { lua_CFunction function; int policy; };
void RecordSetup(lua_State* L, const char* name) {
    lua_rawgetp(L, LUA_REGISTRYINDEX, &bootstrapKey);
    const bool bootstrap = lua_toboolean(L, -1); lua_pop(L, 1);
    if (!bootstrap) return;
    std::string call = name; call += '\0';
    const int count = lua_gettop(L);
    for (int i = 1; i <= count; ++i) {
        const int type = lua_type(L, i); call += static_cast<char>(type);
        if (type == LUA_TSTRING) {
            size_t size = 0; const char* text = lua_tolstring(L, i, &size);
            call += std::to_string(size) + ':'; call.append(text, size);
        } else if (type == LUA_TNUMBER) {
            // These transcripts are compared inside a single core/ABI only.
            const auto number = lua_tonumber(L, i);
            call.append(reinterpret_cast<const char*>(&number), sizeof(number));
        } else if (type == LUA_TBOOLEAN) call += lua_toboolean(L, i) ? '1' : '0';
        else if (type != LUA_TNIL && RuntimeIsStaging(L)) luaL_error(L, "non-scalar bootstrap registration requires a server restart");
    }
    lua_getfield(L, LUA_REGISTRYINDEX, "auriga.setupcalls");
    size_t size = 0; const char* previous = lua_tolstring(L, -1, &size);
    std::string chain(previous ? previous : "", size); lua_pop(L, 1);
    chain += Hash(call);
    lua_pushlstring(L, chain.data(), chain.size()); lua_setfield(L, LUA_REGISTRYINDEX, "auriga.setupcalls");
}
int GuardedCall(lua_State* L) {
    const auto* original = static_cast<NativeFunction*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (original->policy == 1) RecordSetup(L, lua_tostring(L, lua_upvalueindex(2)));
    if (RuntimeIsStaging(L)) {
        if (original->policy == 1) return 0;
        if (!original->policy) return luaL_error(L, "native API '%s' cannot run while preparing a quest generation", lua_tostring(L, lua_upvalueindex(2)));
    }
    return original->function(L);
}
void VisitFunctions(lua_State* L, int table, std::set<const void*>& visited, RuntimeSnapshot& runtime,
                    const std::string& prefix, bool wrap, int depth = 0) {
    table = lua_absindex(L, table);
    if (depth > 8 || !visited.insert(lua_topointer(L, table)).second) return;
    lua_pushnil(L);
    while (lua_next(L, table)) {
        std::string name = prefix;
        if (lua_type(L, -2) == LUA_TSTRING) name += lua_tostring(L, -2);
        if (lua_istable(L, -1)) VisitFunctions(L, -1, visited, runtime, name + '.', wrap, depth + 1);
        else if (lua_iscfunction(L, -1)) {
            const auto function = lua_tocfunction(L, -1);
            if (!wrap) runtime.standardFunctions.insert(function);
            else if (function != GuardedCall && !runtime.standardFunctions.contains(function)) {
                int policy = 0;
                if (name == "add_goto_info" || name == "add_bgm_info" || name == "set_bgm_volume_enable" ||
                    name == "add_restart_city_pos" || name == "arena.add_map") policy = 1;
                if (name == "get_locale_base_path" || name == "get_locale" || name == "get_channel_id") policy = 2;
                auto* value = static_cast<NativeFunction*>(lua_newuserdatauv(L, sizeof(NativeFunction), 0));
                *value = {function, policy};
                lua_pushlstring(L, name.data(), name.size());
                lua_pushcclosure(L, GuardedCall, 2);
                lua_pushvalue(L, -3); lua_pushvalue(L, -2); lua_rawset(L, table);
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }
}
}

std::string RuntimeFingerprint(const std::map<std::string, std::string>& files) {
    std::string manifest;
    for (const auto& [path, bytes] : files) { manifest += path; manifest += '\0'; manifest += Hash(bytes); manifest += '\n'; }
    return Hash(manifest);
}
std::string RuntimeSha256(const std::string& bytes) { return Hash(bytes); }
void RuntimeSnapshot::Capture(const std::string& dataRoot, const std::string& questRoot) {
    namespace fs = std::filesystem;
    const auto base = fs::canonical(dataRoot), quests = fs::canonical(questRoot);
    std::map<std::string, std::string> captured;
    size_t total = 0;
    auto add = [&](const fs::path& path, const std::string& key) {
        if (fs::is_symlink(path) || !fs::is_regular_file(path)) throw std::runtime_error("linked or non-file quest resource: " + key);
        auto bytes = Read(path); total += bytes.size();
        if (total > 64 * 1024 * 1024 || captured.size() >= 8192) throw std::runtime_error("quest snapshot exceeds resource limit");
        captured.emplace(key, std::move(bytes));
    };
    for (auto it = fs::recursive_directory_iterator(base); it != fs::recursive_directory_iterator(); ++it) {
        if (fs::is_symlink(it->symlink_status())) throw std::runtime_error("linked data resource rejected");
        if (it->path() == quests) { it.disable_recursion_pending(); continue; }
        if (it->is_regular_file() && it->path().extension() == ".lua") add(it->path(), it->path().lexically_relative(base).generic_string());
    }
    for (auto it = fs::recursive_directory_iterator(quests); it != fs::recursive_directory_iterator(); ++it) {
        if (fs::is_symlink(it->symlink_status())) throw std::runtime_error("linked quest resource rejected");
        const auto name = it->path().filename().string();
        if (it->is_directory() && (name == "object" || name == "pre_qc" || name == ".git" || name == ".plastic")) { it.disable_recursion_pending(); continue; }
        const auto relative = it->path().lexically_relative(quests).generic_string();
        if (it->is_regular_file() && (it->path().extension() == ".lua" || it->path().extension() == ".quest" ||
            relative == "quest_list" || relative == "questnpc.json" || relative == "questcategory.json")) add(it->path(), "quest/" + relative);
    }
    // quest_list may refer to extensionless sources too.
    auto listBytes = captured.at("quest/quest_list");
    if (listBytes.starts_with("\xef\xbb\xbf")) listBytes.erase(0, 3);
    std::istringstream list(listBytes); std::string row;
    while (std::getline(list, row)) {
        const auto first = row.find_first_not_of(" \t\r");
        if (first == std::string::npos || row[first] == '#') continue;
        row = row.substr(first, row.find_last_not_of(" \t\r") - first + 1);
        const auto relative = fs::path(row).lexically_normal();
        if (relative.is_absolute() || relative.has_root_name() || *relative.begin() == "..") throw std::runtime_error("quest_list path outside quest root");
        const auto path = fs::canonical(quests / relative);
        if (!Within(path, quests) || path != quests / relative) throw std::runtime_error("quest_list linked source rejected");
        const auto key = "quest/" + relative.generic_string();
        if (!captured.contains(key)) add(path, key);
    }
    files = std::move(captured); inputRoot = base.generic_string(); revision = RuntimeFingerprint(files);
}
std::string RuntimeSnapshot::FileKey(const std::string& value) const {
    namespace fs = std::filesystem;
    fs::path path(value);
    if (path.is_absolute()) {
        path = path.lexically_normal();
        for (const auto& [prefix, replacement] : std::vector<std::pair<fs::path, std::string>>{
            {originalQuestRoot, "quest/"}, {originalDataRoot, ""}, {inputRoot, ""}}) {
            if (!prefix.empty() && Within(path, prefix)) return replacement + path.lexically_relative(prefix).generic_string();
        }
        throw std::runtime_error("Lua path outside captured data roots");
    }
    auto key = path.lexically_normal().generic_string();
    if (key.starts_with("data/")) key.erase(0, 5);
    if (key.empty() || key == ".." || key.starts_with("../") || path.has_root_name()) throw std::runtime_error("Lua path outside captured data roots");
    return key;
}
int RuntimeSnapshot::LoadFile(lua_State* L, const std::string& path) const {
    const auto key = FileKey(path);
    const auto found = files.find(key);
    if (found == files.end()) throw std::runtime_error("Lua file missing from generation: " + key);
    const auto& bytes = found->second;
    const size_t offset = bytes.starts_with("\xef\xbb\xbf") ? 3 : 0;
    return luaL_loadbufferx(L, bytes.data() + offset, bytes.size() - offset, ("@" + key).c_str(), "t");
}
void SetRuntimeStaging(lua_State* L, bool staging) { lua_pushboolean(L, staging); lua_rawsetp(L, LUA_REGISTRYINDEX, &stagingKey); }
bool RuntimeIsStaging(lua_State* L) { lua_rawgetp(L, LUA_REGISTRYINDEX, &stagingKey); const bool value = lua_toboolean(L, -1); lua_pop(L, 1); return value; }
void RememberStandardAPIs(lua_State* L, RuntimeSnapshot& runtime) {
    std::set<const void*> visited; lua_pushglobaltable(L); VisitFunctions(L, -1, visited, runtime, "", false); lua_pop(L, 1);
}
void GuardRuntimeAPIs(lua_State* L, RuntimeSnapshot& runtime) {
    lua_pushboolean(L, true); lua_rawsetp(L, LUA_REGISTRYINDEX, &bootstrapKey);
    lua_pushliteral(L, ""); lua_setfield(L, LUA_REGISTRYINDEX, "auriga.setupcalls");
    std::set<const void*> visited; lua_pushglobaltable(L); VisitFunctions(L, -1, visited, runtime, "", true); lua_pop(L, 1);
    lua_pushlightuserdata(L, &runtime); lua_pushcclosure(L, FrozenLoadFile, 1);
    lua_pushvalue(L, -1); lua_setfield(L, LUA_REGISTRYINDEX, "auriga.frozen_loadfile"); lua_setglobal(L, "loadfile");
    SetRuntimeStaging(L, runtime.staging);
}
void FinishRuntimeBootstrap(lua_State* L) { lua_pushboolean(L, false); lua_rawsetp(L, LUA_REGISTRYINDEX, &bootstrapKey); }
std::string RuntimeSetupFingerprint(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "auriga.setupcalls");
    size_t size = 0; const char* text = lua_tolstring(L, -1, &size);
    const auto fingerprint = Hash(std::string(text ? text : "", size)); lua_pop(L, 1); return fingerprint;
}
void CloseLuaRuntime(lua_State* L) {
    SetRuntimeStaging(L, true);
    LuaExecutionBudget budget(L); lua_close(L); budget.Detach();
}
}
