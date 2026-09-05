#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "../../typedef.h"

namespace DragonSoulSystem {

void Initialize(entt::entity owner);
int GetActiveDeck(entt::entity owner);
bool IsDeckActivated(entt::entity owner);
bool ActivateDeck(entt::entity owner, int deckIdx);
void DeactivateAll(entt::entity owner);
void CleanUp(entt::entity owner);
bool OpenRefineWindow(entt::entity owner, entt::entity opener);
bool CloseRefineWindow(entt::entity owner);
bool CanRefine(entt::entity owner);
int32_t GetLastRefineTime(entt::entity owner);
void SetLastRefineTime(entt::entity owner);
entt::entity GetRefineWindowOpener(entt::entity owner);

} // namespace DragonSoulSystem
