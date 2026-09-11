#pragma once

#include <cstdint>
#include <span>

#include <entt/entt.hpp>

#include <common/tables.h>

// The accessory combination and absorption windows. Both the open flags and
// the four material slots live in ecs::AcceWindowComponent; CHARACTER used to
// keep a second copy of the flags.
namespace ecs::AcceSystem {

std::span<entt::entity> GetMaterials(entt::entity e);
bool IsOpened(entt::entity e, bool combination);
bool IsOpen(entt::entity e);
void Open(entt::entity e, bool bCombination);
void Close(entt::entity e);
void ClearMaterials(entt::entity e);
bool IsSameGrade(entt::entity e, int32_t lGrade);
uint32_t GetCombinePrice(entt::entity e, int32_t lGrade
#ifdef ENABLE_STOLE_COSTUME
    , bool isCostume
#endif
    );
uint8_t CheckEmptyMaterialSlot(entt::entity e);
void GetCombineResult(entt::entity e, uint32_t& dwItemVnum, uint32_t& dwMinAbs, uint32_t& dwMaxAbs);
void AddMaterial(entt::entity e, TItemPos tPos, uint8_t bPos);
void RemoveMaterial(entt::entity e, uint8_t bPos);
uint8_t CanRefine(entt::entity e);
void Refine(entt::entity e);

} // namespace ecs::AcceSystem
