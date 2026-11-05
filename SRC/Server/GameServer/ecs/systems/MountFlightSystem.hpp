#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "../Registry.hpp"
#include "../NetworkService.hpp"
#include "../components/visibility_components.hpp"
#include "MountSystem.hpp"
#include "ItemSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "MovementSystem.hpp"
#include "CombatSystem.hpp"
#include "AffectSystem.hpp"
#include "../../core/packet.h"
#include "../../core/utils.h"

namespace ecs::MountFlightSystem {
struct Supported {};
struct State {
    bool ready = false, active = false, anchored = false;
    int32_t altitude = 0, map = 0;
    int32_t landingX = 0, landingY = 0;
    uint32_t updated = 0;
    int8_t axis = 0;
    float credit = 200.f;
};
inline bool CanFly(entt::entity e) {
    if(!PlayerRuntime::IsPC(e)||!MountSystem::GetMountVnum(e)||CombatSystem::IsDead(e)||PlayerRuntime::IsObserverMode(e))return false;
    // Login restores mount affects after entering the game phase.
    if (!AffectSystem::IsLoaded(e)) return false;
    auto item=ItemSystem::GetWearItem(e,WEAR_COSTUME_MOUNT);
    return ItemSystem::IsValidItem(item)&&ItemSystem::GetItemValue(item,5)==1&&
        ItemSystem::GetItemValue(item,1)==MountSystem::GetMountVnum(e);
}
inline void Reset(entt::entity e) { if(auto* s=g_registry.try_get<State>(e)){const auto map=s->map;const bool ready=s->ready;*s=State{};s->map=map;s->ready=ready;} }
inline void Send(entt::entity source,entt::entity viewer) {
    if(!g_registry.valid(source)||!g_registry.valid(viewer)||!g_registry.all_of<Supported>(viewer)||!PlayerRuntime::IsPC(source))return;
    auto& state=g_registry.get_or_emplace<State>(source);
    bool capable=CanFly(source);
    if(state.map!=PlayerRuntime::GetMapIndex(source))state.ready=false;
    if(!capable||!state.ready) {state.active=false;state.axis=0;}
    TPacketGCMountFlight p{};p.header=HEADER_GC_MOUNT_FLIGHT;p.vid=PlayerRuntime::GetPacketVID(source);
    p.flags=(capable?1:0)|(state.active?2:0);p.altitude=state.altitude;p.axis=state.axis;
    NetworkService::Send(viewer,&p,sizeof(p));
}
inline void Broadcast(entt::entity e) {
    Send(e,e);
    if(const auto* view=g_registry.try_get<ViewerMap>(e))for(const auto& pair:view->viewers)Send(e,pair);
}
inline void Request(entt::entity e,const TPacketCGMountFlight& p) {
    if(!PlayerRuntime::IsPC(e)||p.action>2)return;
    if(p.action==0) {
        g_registry.get_or_emplace<Supported>(e);
        auto& own=g_registry.get_or_emplace<State>(e);
        if(own.map!=PlayerRuntime::GetMapIndex(e))own=State{};
        own.ready=true;own.map=PlayerRuntime::GetMapIndex(e);Send(e,e);
        for(auto source:g_registry.view<State>()) {
            const auto* viewers=g_registry.try_get<ViewerMap>(source);
            if(source!=e&&viewers&&viewers->viewers.count(e))Send(source,e);
        }
        return;
    }
    if(!g_registry.all_of<Supported>(e))return;
    auto& state=g_registry.get_or_emplace<State>(e);
    if(!CanFly(e)||!MovementSystem::CanMove(e)) {Reset(e);Broadcast(e);return;}
    uint32_t now=get_dword_time();
    if(!state.ready||state.map!=PlayerRuntime::GetMapIndex(e)){state=State{};Broadcast(e);return;}
    if(p.altitude < -1000000 || p.altitude > 1000000) {Send(e,e);return;}
    if(!state.anchored&&p.action!=1){Send(e,e);return;}
    // The map has no server-side terrain height. A mount that moved on the
    // ground can start from the new sampled height. Stationary takeoff keeps
    // the last altitude and budget, including immediately after landing.
    const bool newGround = !state.active && p.action == 1 &&
        (state.landingX != PlayerRuntime::GetX(e) || state.landingY != PlayerRuntime::GetY(e));
    if(state.anchored && !newGround) {
        const float elapsed=std::min(1.0f,(now-state.updated)*.001f);
        // Shared 6 m/s vertical budget, capped at 2 m of network jitter.
        // A packet cannot obtain a fresh allowance just by arriving more often.
        const float allowance=std::min(200.f,state.credit+600.f*elapsed);
        if(std::abs(double(p.altitude)-state.altitude)>allowance){Send(e,e);return;}
        state.credit=allowance-float(std::abs(double(p.altitude)-state.altitude));
        state.axis=p.altitude>state.altitude+1?1:p.altitude<state.altitude-1?-1:0;
    } else {
        state.credit = 200.f;
        state.axis = 1;
    }
    state.anchored=true;state.active=p.action==1;state.altitude=p.altitude;state.updated=now;state.map=PlayerRuntime::GetMapIndex(e);
    if (!state.active) {
        state.axis = 0;
        state.landingX = PlayerRuntime::GetX(e);
        state.landingY = PlayerRuntime::GetY(e);
    }
    Broadcast(e);
}
static_assert(sizeof(TPacketCGMountFlight)==10,"Flight request wire layout");
static_assert(sizeof(TPacketGCMountFlight)==11,"Flight state wire layout");
} // namespace ecs::MountFlightSystem
