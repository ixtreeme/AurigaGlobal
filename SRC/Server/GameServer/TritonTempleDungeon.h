#pragma once

#include <cstdint>

#include <entt/entt.hpp>

class CHARACTER;

// C++ replacement for quest: tritontemple_razor93
class CTritonTempleDungeon
{
public:
    static CTritonTempleDungeon& instance();

    bool IsTritonTempleMap(int32_t mapIndex) const;

    // Hooks (call from existing server flow)
    void OnPlayerDisconnect(entt::entity character);
    void OnPlayerLogin(entt::entity character);
    void OnMobKilled(entt::entity killer, entt::entity victim);

    // Trigger from ON_CLICK_TRITON_TEMPLE
    bool OnClickNpc(entt::entity character);
};
