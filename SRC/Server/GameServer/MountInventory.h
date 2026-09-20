#pragma once

// Kept as the single source for the account mount-inventory width used by
// packet and gameplay code. Runtime state is an ECS component declared in
// ecs/components/inventory_components.hpp.
constexpr int MOUNT_INVENTORY_WIDTH = 12;
constexpr int MOUNT_INVENTORY_MAX_HEIGHT = 16;
