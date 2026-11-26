#pragma once

#include <entt/entt.hpp>
#include "../components/transform_components.hpp"

namespace ecs::SessionSystem {

bool WarpToPID(entt::entity e, uint32_t dwPID);
bool IsSafeboxLoading(entt::entity e);
void SetSafeboxLoading(entt::entity e, bool loading);
void CreatePlayerProto(entt::entity e, TPlayerTable& tab);
void Disconnect(entt::entity e, const char* c_pszReason);
bool GetSkipSave(entt::entity e);
void SetSkipSave(entt::entity e, bool value);
void Save(entt::entity e);
void SaveReal(entt::entity e);
void StartSaveEvent(entt::entity e);
bool IsSafeboxOpen(entt::entity character);
void SetSafeboxOpen(entt::entity character, bool open);
bool IsCubeOpen(entt::entity character);


} // namespace ecs::SessionSystem
