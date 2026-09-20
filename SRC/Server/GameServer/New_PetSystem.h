#ifndef	__HEADER_NEWPET_SYSTEM__
#define	__HEADER_NEWPET_SYSTEM__

// Growth ("new") pets are ECS state now: NewPetRuntime/NewPetActorState on the
// owner, with free functions below. There is no CNewPetActor/CNewPetSystem
// instance layer.

#include <cstddef>
#include <cstdint>

#include <entt/entt.hpp>

#include "ecs/components/pet_mount_components.hpp"

namespace NewPetSystem {

enum ENewPetOptions
{
    EPetOption_Followable = 1 << 0,
    EPetOption_Mountable = 1 << 1,
    EPetOption_Summonable = 1 << 2,
    EPetOption_Combatable = 1 << 3,
};

constexpr uint32_t DefaultPetOptions = EPetOption_Followable | EPetOption_Summonable;

bool HasOption(const ecs::NewPetActorState& actor, uint32_t option);
bool IsSummoned(const ecs::NewPetActorState& actor);
bool HasValidSummon(entt::entity owner, const ecs::NewPetActorState& actor);

ecs::NewPetActorState* FindActor(entt::entity owner, uint32_t vnum);
ecs::NewPetActorState* FindActorByVID(entt::entity owner, uint32_t vid);
bool IsActivePet(entt::entity owner);
size_t CountSummoned(entt::entity owner);

ecs::NewPetActorState* Summon(entt::entity owner, uint32_t vnum, entt::entity item,
    const char* petName, bool spawnFar, uint32_t options = DefaultPetOptions);
void Unsummon(entt::entity owner, uint32_t vnum, bool deleteFromList = false);
void UnsummonAll(entt::entity owner);
void DeletePet(entt::entity owner, uint32_t vnum);
void DestroyRuntime(entt::entity owner);

bool Update(entt::entity owner, uint32_t deltaTime);
void UpdateTime(entt::entity owner, bool now = false);
void SetUpdatePeriod(entt::entity owner, uint32_t ms);
void RefreshBuff(entt::entity owner);
void UpdatePetSkin(entt::entity owner);
void ChangeName(entt::entity owner, const char* name);

bool Mount(entt::entity owner, uint32_t vnum);
void Unmount(entt::entity owner, uint32_t vnum);

bool IncreasePetEvolution(entt::entity owner);
void SetExp(entt::entity owner, int exp, int mode);
int GetEvolution(entt::entity owner);
int GetLevel(entt::entity owner);
int GetExp(entt::entity owner);
int GetLevelStep(entt::entity owner);
#ifdef ENABLE_NEW_PET_EDITS
int GetNextExpFromMob(entt::entity owner);
int ResetSkills(entt::entity owner);
int ResetSkill(entt::entity owner, int type);
bool IncreasePetSkill(entt::entity owner, int slot, int type);
bool IncreasePetSkillByBook(entt::entity owner, entt::entity bookItem);
#else
bool IncreasePetSkill(entt::entity owner, int skill);
#endif
void SetItemCube(entt::entity owner, int pos, int invpos);
void ItemCubeFeed(entt::entity owner, int type);
void DoPetSkill(entt::entity owner, int skillslot);
uint32_t GetNewPetItemID(entt::entity owner);

} // namespace NewPetSystem

#endif	//__HEADER_NEWPET_SYSTEM__
