#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/warrior/WarriorSkillSystem.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
namespace
{
std::string reply;
}
namespace ecs
{
void ChatSystem::Send(entt::entity, uint8_t, const char *text, ...)
{
    reply = text ? text : "";
}
} // namespace ecs
int main()
{
    auto player = static_cast<entt::entity>(1);
    uint8_t level = 20;
    if (WarriorExpansion::IsCasting(player) || WarriorExpansion::IsEnabled() ||
        WarriorExpansion::AllowBuff(player, 180) || WarriorExpansion::EnsureLoaded(player) ||
        WarriorExpansion::IsChosen(player, 180) || WarriorExpansion::CommitLevel(player, 180, level) ||
        WarriorExpansion::ReadBook(player, 180, 0) || WarriorExpansion::ValidateCast(player, 184, player))
        throw std::runtime_error("Feature remains active without compile macro");
    WarriorExpansion::Command(player, "state");
    if (reply != "wex_disabled")
        throw std::runtime_error("Macro-off capability response missing");
    std::cout << "PASS warrior expansion macro-off gates\n";
}
