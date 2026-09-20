#ifndef __HEADER_PET_SYSTEM__
#define __HEADER_PET_SYSTEM__

// Regular pets are ECS state now: PetRuntime/PetActorState on the owner, with
// free functions below. There is no CPetActor/CPetSystem instance layer.

#include <cstddef>
#include <cstdint>

#include <entt/entt.hpp>

#include "ecs/components/pet_mount_components.hpp"

namespace PetSystem {

enum EPetOptions
{
    EPetOption_Followable = 1 << 0,
    EPetOption_Mountable = 1 << 1,
    EPetOption_Summonable = 1 << 2,
    EPetOption_Combatable = 1 << 3,
};

constexpr uint32_t DefaultPetOptions = EPetOption_Followable | EPetOption_Summonable;

bool HasOption(const ecs::PetActorState& actor, uint32_t option);
bool IsSummoned(const ecs::PetActorState& actor);

ecs::PetActorState* FindActor(entt::entity owner, uint32_t vnum);
ecs::PetActorState* FindActorByVID(entt::entity owner, uint32_t vid);
bool IsPetSummoned(entt::entity owner, uint32_t vnum);
size_t CountSummoned(entt::entity owner);

ecs::PetActorState* Summon(entt::entity owner, uint32_t vnum, entt::entity item,
    const char* petName, bool spawnFar, uint32_t options = DefaultPetOptions);
void Unsummon(entt::entity owner, uint32_t vnum, bool deleteFromList = false);
void UnsummonAll(entt::entity owner);
void DeleteActor(entt::entity owner, uint32_t vnum);
void DestroyRuntime(entt::entity owner);

bool Update(entt::entity owner, uint32_t deltaTime);
void SetUpdatePeriod(entt::entity owner, uint32_t ms);
void RefreshBuff(entt::entity owner);
void UpdatePetSkin(entt::entity owner);

bool Mount(entt::entity owner, uint32_t vnum);
void Unmount(entt::entity owner, uint32_t vnum);

} // namespace PetSystem

#endif
