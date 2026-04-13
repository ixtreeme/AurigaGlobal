#pragma once

#include <cstdint>

class CHARACTER;

// C++ replacement for quest: tritontemple_razor93
class CTritonTempleDungeon
{
public:
    static CTritonTempleDungeon& instance();

    bool IsTritonTempleMap(int32_t mapIndex) const;

    // Hooks (call from existing server flow)
    void OnPlayerDisconnect(CHARACTER* ch);
    void OnPlayerLogin(CHARACTER* ch);
    void OnMobKilled(CHARACTER* killer, CHARACTER* victim);

    // Trigger from ON_CLICK_TRITON_TEMPLE
    bool OnClickNpc(CHARACTER* ch);
};
