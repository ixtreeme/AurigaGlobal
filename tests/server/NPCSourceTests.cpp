#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/quest/questmanager.h"
#include <stdexcept>
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/core/constants.h"

namespace { [[noreturn]] void Unexpected() { throw std::runtime_error("unused NPC test service called"); } }
int test_server = 0;
std::string m_closeText[LANGUAGE_MAX_NUM];
void intrusive_ptr_release(event*) { Unexpected(); }
namespace ecs::PlayerRuntime {
DESC* GetDesc(entt::entity) { Unexpected(); }
std::string_view GetName(entt::entity) { Unexpected(); }
bool IsValid(entt::entity) { Unexpected(); }
}

// Only the services needed by the real NPC::Set are doubled. Event maps and
// script assembly are the production implementation from questnpc.cpp.
namespace quest {
CQuestManager::CQuestManager() : L(nullptr) {}
CQuestManager::~CQuestManager() {}
void CQuestManager::SwitchRuntime(const std::shared_ptr<RuntimeGeneration>&) noexcept { std::abort(); }
unsigned int CQuestManager::GetQuestIndexByName(const std::string& name) { return name == "routing" ? 71 : 0; }
const std::string& CQuestManager::GetQuestNameByIndex(unsigned int) { Unexpected(); }
entt::entity CQuestManager::GetCurrentPCEntity() const { Unexpected(); }
void CQuestManager::CloseState(QuestState&, bool) const { Unexpected(); }
bool CQuestManager::CanStartQuest(unsigned int) { Unexpected(); }
bool CQuestManager::CanStartQuest(unsigned int, const PC&) { Unexpected(); }
bool CQuestManager::ExecuteQuestScript(PC&, const std::string&, int, const char*, int, std::vector<AArgScript*>*, bool) { Unexpected(); }
bool CQuestManager::ExecuteQuestScript(PC&, uint32_t, int, const char*, int, std::vector<AArgScript*>*, bool) { Unexpected(); }
bool IsScriptTrue(const char*, int) { Unexpected(); }
std::string ScriptToString(const std::string&) { Unexpected(); }
void PC::EndRunning() { Unexpected(); }
const std::string& PC::GetCurrentQuestName() const { Unexpected(); }
}
namespace {
class InspectNPC : public quest::NPC {
public:
    using NPC::m_mapOwnQuest;
    using NPC::m_mapOwnArgQuest;
};
}
int CheckNPCSources(lua_State* L) {
    using namespace quest;
    int checks = 0;
    auto check = [&](bool value, const char* error) { ++checks; if (!value) throw std::runtime_error(error); };
    quest::CQuestManager manager;
    manager.m_mapEventName = {{"login", QUEST_LOGIN_EVENT}, {"chat", QUEST_CHAT_EVENT},
        {"click", QUEST_CLICK_EVENT}, {"server_timer", QUEST_SERVER_TIMER_EVENT}};
    auto definition = quest::source::ParseLegacy(R"q(quest routing begin
        state start begin
            when login begin result=1 end
            when 20016.chat."numeric" begin result=2 end
            when blacksmith.chat."alias" begin result=3 end
            when blacksmith.click with false begin result=4 end
            when blacksmith.click with true begin result=5 return end
            when blacksmith.click begin result=6 end
            when scheduled.server_timer begin result=7 end
        end
        state running begin when 20016.chat."running" begin result=8 end end
    end)q", "routing.quest");
    quest::source::Validate(L, definition);
    const_cast<quest::source::Catalog&>(manager.GetQuestSources()).push_back(std::move(definition));
    InspectNPC npc;
    npc.Set(20016, "blacksmith");
    auto& chats = npc.m_mapOwnArgQuest[QUEST_CHAT_EVENT].at(71).at(0);
    check(chats.size() == 2 && chats[0].arg == "\"numeric\"" && chats[1].arg == "\"alias\"", "numeric/alias chat routing changed");
    check(chats[0].quest_index == 71 && chats[0].state_index == 0, "chat routing IDs changed");
    check(npc.m_mapOwnArgQuest[QUEST_CHAT_EVENT].at(71).contains(quest::source::StateId("running")), "non-start state handler missing");
    check(npc.m_mapOwnQuest[QUEST_LOGIN_EVENT].empty(), "notarget event leaked onto NPC");
    auto& script = npc.m_mapOwnQuest[QUEST_CLICK_EVENT].at(71).at(0);
    check(lua_dobuffer(L, script.GetCode(), script.GetSize(), "@assembled") == LUA_OK, "assembled NPC event failed");
    lua_getglobal(L, "result"); check(lua_tointeger(L, -1) == 5, "conditional handler order/return behavior changed"); lua_pop(L, 1);
    npc.Set(20016, "blacksmith");
    check(npc.m_mapOwnArgQuest[QUEST_CHAT_EVENT].at(71).at(0).size() == 2, "repeated Set duplicated handlers");
    InspectNPC noTarget;
    noTarget.Set(0, "notarget");
    check(noTarget.m_mapOwnQuest[QUEST_LOGIN_EVENT].at(71).contains(0), "global event missing without object directory");
    check(noTarget.m_mapOwnArgQuest[QUEST_CHAT_EVENT].empty(), "NPC chat leaked onto global events");
    InspectNPC timer;
    timer.Set(UINT_MAX, "scheduled");
    check(timer.m_mapOwnQuest[QUEST_SERVER_TIMER_EVENT].at(71).contains(0), "named server timer missing");
    return checks;
}
