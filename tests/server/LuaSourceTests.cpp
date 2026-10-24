#include "../../SRC/Server/GameServer/core/lua_incl.h"
#include "../../SRC/Server/GameServer/quest/quest_source.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>

int CheckNPCSources(lua_State* L);
int CheckDungeonQuests(lua_State* L);

namespace {
int checks = 0;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
void Execute(lua_State* L, const std::string& code) {
    if (lua_dobuffer(L, code.data(), code.size(), "@test") != LUA_OK)
        throw std::runtime_error(lua_tostring(L, -1));
}
template<class F> void Reject(F action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Check(rejected, "invalid quest accepted");
}
void Parser(lua_State* L) {
    auto q = quest::source::ParseLegacy(R"q(quest sample with !false begin
        state start begin
            function helper(x) if x != 2 then return "begin != end" end return x end
            when login or levelup with true begin
                -- begin end ! false
                local s = [=[begin end !=]=]
                local modern = "a\z
                    b"
                assert(modern == "ab")
                begin if !false then result = sample.helper(2) end end
            end
            when 20016.chat."Hello" with true begin return end
            when marker.target.click begin result = 3 end
        end
        state start begin when leave begin result = 4 end end
        state running begin when timer begin repeat result = 5 until result == 5 end end
    end)q", "sample.quest");
    Check(q.name == "sample" && q.states.size() == 2 && q.handlers.size() == 6, "event/state parsing changed");
    Check(q.states.at("start") == 0 && q.states.at("running") == quest::source::StateId("running"), "state IDs changed");
    Check(q.handlers[2].target == "20016" && q.handlers[2].event == "chat" && q.handlers[2].argument == "\"Hello\"", "chat selector changed");
    Check(q.handlers[3].event == "target" && q.handlers[3].argument == "marker.click", "target selector changed");
    quest::source::Validate(L, q);
    Execute(L, q.stateCode);
    Execute(L, q.handlers[0].code);
    lua_getglobal(L, "result"); Check(lua_tointeger(L, -1) == 2, "legacy operators/functions failed"); lua_pop(L, 1);
    Check(q.handlers[0].code.find("[=[begin end !=]=]") != std::string::npos, "normalizer rewrote a string");
    Reject([&] { quest::source::ParseLegacy("quest x begin state start begin when login begin", "bad.quest"); });
    Reject([&] { auto bad = quest::source::ParseLegacy("quest x begin state start begin when login begin if then end end end end", "bad.quest"); quest::source::Validate(L, bad); });
    Reject([&] { quest::source::ParseLegacy("quest x begin state start begin when login begin local s='bad\n' end end end", "bad.quest"); });
    Reject([&] { quest::source::ParseLegacy("quest x begin state other begin end end", "bad.quest"); });
}
void Coroutines(lua_State* L) {
    const std::string code = R"lua(
        local selected = coroutine.yield("select", {"one", "two"})
        assert(selected == 2)
        coroutine.yield("wait")
        assert(coroutine.yield("input") == "answer")
        assert(coroutine.yield("confirm", "ok", 1, 30) == 1)
        assert(coroutine.yield("select_item") == 42)
        return 9007199254740993
    )lua";
    auto* co = lua_newthread(L);
    const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    Check(luaL_loadbufferx(co, code.data(), code.size(), "@dialog", "t") == LUA_OK, "dialog did not compile");
    int results = 0;
    auto resume = [&](int args, const char* action) {
        LuaExecutionBudget budget(co);
        Check(lua_resume(co, L, args, &results) == LUA_YIELD, "dialog failed to yield");
        Check(std::string(lua_tostring(co, 1)) == action, "wrong dialog payload");
        lua_settop(co, 0);
    };
    resume(0, "select"); lua_pushinteger(co, 2);
    resume(1, "wait"); resume(0, "input"); lua_pushstring(co, "answer");
    resume(1, "confirm"); lua_pushinteger(co, 1);
    resume(1, "select_item"); lua_pushinteger(co, 42);
    { LuaExecutionBudget budget(co); Check(lua_resume(co, L, 1, &results) == LUA_OK, "dialog failed to finish"); }
    Check(results == 1 && lua_isinteger(co, -1) && lua_tointeger(co, -1) == 9007199254740993LL, "64-bit integer lost precision");
    Check(lua_closethread(co, L) == LUA_OK, "coroutine close failed");
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
    Execute(L, "closed=false; function scoped() local v <close> = setmetatable({}, {__close=function() closed=true end}); coroutine.yield('wait') end");
    co = lua_newthread(L); const int closeRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getglobal(co, "scoped");
    Check(lua_resume(co, L, 0, &results) == LUA_YIELD, "close fixture failed");
    Check(lua_closethread(co, L) == LUA_OK, "suspended close failed");
    luaL_unref(L, LUA_REGISTRYINDEX, closeRef);
    lua_getglobal(L, "closed"); Check(lua_toboolean(L, -1), "to-be-closed variable leaked"); lua_pop(L, 1);
    const std::string loop = "while true do end";
    Check(lua_dobuffer(L, loop.data(), loop.size(), "@loop") == LUA_ERRRUN, "runaway script was not stopped");
    Check(std::string(lua_tostring(L, -1)).find("budget exceeded") != std::string::npos, "wrong budget error"); lua_pop(L, 1);
    Execute(L, "assert(table.getn({1,2})==2 and math.mod(5,2)==1 and unpack({7})==7); assert(io==nil and debug==nil and package==nil and os.execute==nil)");
    Execute(L, "local f=assert(load('return 42')); assert(f()==42); assert(not load(string.dump(f))); assert(not load(string.dump(f), 'x', 'b')); assert(not loadfile(nil))");
    // A child created under a budget may outlive that stack frame.
    Execute(L, "saved=coroutine.create(function() coroutine.yield(1); return 2 end); assert(coroutine.resume(saved))");
    Execute(L, "local ok,value=coroutine.resume(saved); assert(ok and value==2)");
    const std::string binary("\x1bLua\0", 5);
    Check(lua_dobuffer(L, binary.data(), binary.size(), "@binary") != LUA_OK, "binary bytecode accepted"); lua_pop(L, 1);
}
void Lists(lua_State* L) {
    const auto root = std::filesystem::temp_directory_path() / ("auriga-lua-source-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if (!std::filesystem::create_directory(root)) throw std::runtime_error("fixture directory already exists");
    struct Cleanup { std::filesystem::path root; ~Cleanup() { std::filesystem::remove_all(root); } } cleanup {root};
    auto write = [&](const char* file, const char* value) { std::ofstream(root / file, std::ios::binary) << value; };
    write("a.quest", "quest existing begin state start begin when login begin return end end end");
    write("quest_list", "# enabled sources\na.quest\n");
    Check(quest::source::Load(L, root).size() == 1, "source load failed without object directory");
    write("quest_list", "a.quest\na.quest\n"); Reject([&] { quest::source::Load(L, root); });
    write("b.quest", "quest existing begin state start begin end end");
    write("quest_list", "a.quest\nb.quest\n"); Reject([&] { quest::source::Load(L, root); });
    write("quest_list", "../outside.quest\n"); Reject([&] { quest::source::Load(L, root); });
    write("quest_list", "missing.quest\n"); Reject([&] { quest::source::Load(L, root); });
}
}
int main() {
    lua_State* L = luaL_newstate();
    if (!L) return 2;
    try {
        Check(LUA_VERSION_NUM == 505, "wrong Lua runtime linked");
        Check(LuaOpenQuestLibraries(L), "quest libraries failed to open");
        Parser(L); Coroutines(L); Lists(L);
        checks += CheckNPCSources(L);
        checks += CheckDungeonQuests(L);
        Check(lua_gettop(L) == 0, "Lua stack leaked");
        lua_close(L);
        std::cout << checks << " Lua/source checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; lua_close(L); return 1;
    }
}
