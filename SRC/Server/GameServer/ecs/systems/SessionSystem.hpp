#pragma once

#include <entt/entt.hpp>
#include "../components/transform_components.hpp"

class CSafebox;

namespace ecs::SessionSystem {

bool WarpToPID(entt::entity e, uint32_t dwPID);
CSafebox* GetSafebox(entt::entity e);
CSafebox* GetMall(entt::entity e);
void CloseMall(entt::entity e);
void QuerySafeboxSize(entt::entity e);
void ChangeSafeboxSize(entt::entity e, uint8_t bSize);
void ReqSafeboxLoad(entt::entity e, const char* pszPassword);
void LoadSafebox(entt::entity e, int iSize, uint32_t dwGold, int iItemCount, TPlayerItem* pItems);
void LoadMall(entt::entity e, int iItemCount, TPlayerItem* pItems);
void CloseSafebox(entt::entity e);
bool IsSafeboxLoading(entt::entity e);
void SetSafeboxLoading(entt::entity e, bool loading);
void Disconnect(entt::entity e, const char* c_pszReason);
bool GetSkipSave(entt::entity e);
void SetSkipSave(entt::entity e, bool value);
void Save(entt::entity e);
void SaveReal(entt::entity e);
void StartSaveEvent(entt::entity e);
void FlushDelayedSaveItem(entt::entity e);
bool IsSafeboxOpen(entt::entity character);
void SetSafeboxOpen(entt::entity character, bool open);
bool IsCubeOpen(entt::entity character);
void SetCubeNPC(entt::entity character, entt::entity npc);

int GetSafeboxSize(entt::entity character);
bool SetSafeboxSize(entt::entity character, int size);
bool SetSafeboxOpenPosition(entt::entity character);
float GetDistanceFromSafeboxOpen(entt::entity character);

} // namespace ecs::SessionSystem
