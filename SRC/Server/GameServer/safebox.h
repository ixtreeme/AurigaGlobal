#ifndef __INC_METIN_II_GAME_SAFEBOX_H__
#define __INC_METIN_II_GAME_SAFEBOX_H__

// Safebox and mall storage are ECS state now: a SafeboxStorageComponent on a
// registry-owned entity, published by SafeboxRef on the owner. There is no
// shared_ptr<CSafebox> ownership layer; every entry point revalidates the
// storage entity after callbacks, so a nested Close retires it safely.

#include <cstdint>

#include <entt/entt.hpp>

#include "ecs/components/inventory_components.hpp"

namespace SafeboxSystem {

// The storage published for this owner and window, or null.
entt::entity Get(entt::entity owner, uint8_t window);
entt::entity Open(entt::entity owner, uint8_t window, int height, uint32_t gold = 0);
void Close(entt::entity owner, uint8_t window, bool save = true);

// Storage operations. They take the storage entity, not the owner, so a
// replacement container cannot silently receive an old operation's result.
bool Add(entt::entity storage, uint32_t cell, entt::entity item);
entt::entity GetItem(entt::entity storage, uint32_t cell);
entt::entity Remove(entt::entity storage, uint32_t cell);
bool MoveItem(entt::entity storage, uint32_t cell, uint32_t destCell, uint32_t count);
bool IsEmpty(entt::entity storage, uint32_t cell, uint8_t size);
bool IsValidPosition(entt::entity storage, uint32_t cell);
void ChangeSize(entt::entity storage, int size);
void SetWindowMode(entt::entity storage, uint8_t window);
void Save(entt::entity storage);

// Retires the storage and its contents without saving. Reentrant calls and
// calls after retirement are no-ops.
void Destroy(entt::entity storage);

} // namespace SafeboxSystem

#endif
