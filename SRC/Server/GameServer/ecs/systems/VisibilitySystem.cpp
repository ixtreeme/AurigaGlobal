#include "../../stdafx.h"

#include "VisibilitySystem.hpp"

#include "../EventDispatcher.hpp"
#include "../events.hpp"

#include <Core/Logging.hpp>

namespace ecs::VisibilitySystem {

namespace {
    bool g_initialized = false;

    // Phase 15E-final.LPENTITY.4-architect.D.3 skeleton handler.
    //
    // D.4 will replace this body with:
    //   1. Compute oldViewers via sectree query at (oldX, oldY, oldMapIndex).
    //   2. Compute newViewers via sectree query at (newX, newY, newMapIndex).
    //   3. Symmetric-diff: leaving = oldViewers \ newViewers,
    //                     entering = newViewers \ oldViewers.
    //   4. For each leaving viewer: bidirectional ViewMap/ViewerMap erase
    //      + EntityNetworkDispatch::SendRemove.
    //   5. For each entering viewer: bidirectional ViewMap/ViewerMap insert
    //      + EntityNetworkDispatch::SendInsert.
    //
    // Spawn case (oldMapIndex == 0): skip the leaving step (no prior
    // viewers existed) and fall straight through to the entering step
    // computed against the new position.
    //
    // For D.3 the handler is a no-op so we can validate that:
    //  - The dispatcher accepts the connection at Init time.
    //  - The trigger sites in D.2 fire without crashing.
    //  - g_dispatcher.update() in main.cpp processes the queue cleanly.
    void OnPositionChanged(const ecs::PositionChangedEvent& ev)
    {
        (void)ev;
    }
}

void Init(entt::registry& /*reg*/)
{
    if (g_initialized)
        return;

    g_dispatcher.sink<ecs::PositionChangedEvent>()
        .connect<&OnPositionChanged>();

    g_initialized = true;
    LOG_INFO("[VISIBILITY] Init: PositionChangedEvent handler connected (D.3 skeleton)");
}

void Shutdown(entt::registry& /*reg*/)
{
    if (!g_initialized)
        return;

    g_dispatcher.sink<ecs::PositionChangedEvent>()
        .disconnect<&OnPositionChanged>();

    g_initialized = false;
}

} // namespace ecs::VisibilitySystem
