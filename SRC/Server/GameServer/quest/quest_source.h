#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct lua_State;

namespace quest::source {
struct Handler {
    std::string target, event, state, argument, condition, code;
};
struct Definition {
    std::string name, file, startCondition, stateCode;
    std::map<std::string, int> states;
    std::vector<Handler> handlers;
};
using Catalog = std::vector<Definition>;

// A parser for the historical quest DSL, independent of Lua's private lexer.
// All Lua bodies retain their original source text and line positions.
Definition ParseLegacy(const std::string& text, const std::string& file);
void Validate(lua_State* L, const Definition& definition);
Catalog Load(lua_State* L, const std::filesystem::path& root);
Catalog LoadFiles(lua_State* L, const std::map<std::string, std::string>& files);
int StateId(const std::string& name);
std::string HandlerCode(const Handler& handler);
}
