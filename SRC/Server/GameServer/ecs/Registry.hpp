#pragma once

#include <entt/entt.hpp>

// Single global registry for the game server.
// Must only be accessed from the main game thread.
extern entt::registry g_registry;
