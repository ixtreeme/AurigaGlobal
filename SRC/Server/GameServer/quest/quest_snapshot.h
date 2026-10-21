#pragma once
#include "../core/lua_incl.h"
#include <map>
#include <set>
#include <string>

namespace quest {
struct RuntimeSnapshot {
    std::map<std::string, std::string> files;
    std::set<lua_CFunction> standardFunctions;
    std::string originalDataRoot, originalQuestRoot, inputRoot, revision;
    bool staging = false;
    void Capture(const std::string& dataRoot, const std::string& questRoot);
    std::string FileKey(const std::string& path) const;
    int LoadFile(lua_State* L, const std::string& path) const;
};
void GuardRuntimeAPIs(lua_State* L, RuntimeSnapshot& runtime);
void RememberStandardAPIs(lua_State* L, RuntimeSnapshot& runtime);
void SetRuntimeStaging(lua_State* L, bool staging);
bool RuntimeIsStaging(lua_State* L);
void CloseLuaRuntime(lua_State* L);
void FinishRuntimeBootstrap(lua_State* L);
std::string RuntimeSetupFingerprint(lua_State* L);
std::string RuntimeFingerprint(const std::map<std::string, std::string>& files);
std::string RuntimeSha256(const std::string& bytes);
}
