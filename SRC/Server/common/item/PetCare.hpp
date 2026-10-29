#pragma once
#include "CompanionItems.hpp"
#include "GrowthPets.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace auriga::item {
struct PetBirthRange {
    std::uint16_t minimum=0,maximum=0,step=1;
};
struct PetBirthTraits {
    // Stored tenths of percent. Applying these to stats is a separate contract.
    std::uint16_t maximumHpPercentTenths=0,monsterDamagePercentTenths=0,pveNormalDamagePercentTenths=0;
    bool operator==(const PetBirthTraits&) const=default;
};
struct PetHatchProfile {
    std::uint32_t id=0;
    std::string code;
    ItemKey target;
    std::uint64_t goldCost=0;
    PetBirthRange maximumHp,monsterDamage,pveNormalDamage;
};
bool ValidPetHatchProfile(const PetHatchProfile&) noexcept;
enum class PetFeedingService : std::uint8_t { Cube=1,Direct=2 };
enum class PetFoodKind : std::uint8_t { LifetimeFraction=1,RefillOrRevive=2,EquipmentExperience=3 };
enum class PetFoodConsumption : std::uint8_t { ExactUnits=1,WholeStack=2 };
struct PetFeedRule {
    ItemKey key;
    PetFeedingService service{};
    PetFoodKind kind{};
    PetFoodConsumption consumption{};
    std::uint32_t units=0;
    std::uint16_t lifetimeFractionBp=0,deadRecoveryBp=0;
    std::uint32_t experiencePriceDivisor=0;
};
bool ValidPetFeedRule(const PetFeedRule&) noexcept;

struct GrowthPetLife {
    CompanionClock clock{};
    std::uint64_t capacityMs=0,remainingMs=0;
    bool operator==(const GrowthPetLife&) const=default;
};
struct PetCareItemSnapshot {
    ItemKey key;
    std::uint64_t instanceId=0,revision=0,ownerId=0;
    std::uint32_t count=0;
    std::uint16_t refinementLevel=0;
    bool lockedByOther=false,exchanging=false,equipped=false,hasPetState=false,active=false;
    std::uint64_t actorGeneration=0;
};
struct PetCareContext {
    std::array<std::uint8_t,16> operationId{};
    std::uint64_t actorId=0,sessionGeneration=0,rulesetRevision=0;
    std::uint16_t characterLevel=0;
    std::uint8_t characterClass=0,sex=0;
    bool admittedByServer=false,canUseItems=false,ownerAlive=false,observer=false,polymorphed=false,inCombat=false;
};
struct PetHatchRequest {
    PetCareContext context;
    PetCareItemSnapshot egg;
    std::uint64_t walletRevision=0,gold=0,newInstanceId=0,bornAtUtcMs=0;
    std::string name;
    PetBirthTraits recordedTraits;
    bool nameAcceptedByServer=false,newInstanceReservedByServer=false;
};
struct PetHatchPlan {
    PetHatchRequest expected;
    ItemKey target;
    GrowthPetProgress progress;
    GrowthPetLife life;
    std::uint32_t consumeEggUnits=1;
    std::uint32_t newItemCount=1;
    std::uint16_t refinementLevel=0;
    std::uint8_t initiallyLockedSkillSlots=4;
    std::uint64_t goldCost=0,catalogRevision=0;
    std::array<std::uint8_t,32> catalogHash{};
    // Name/time/traits are the exact validated owned values in expected.
    // The complete F3 birth factory must initialize all four skill slots locked.
};
struct PetLifeRequest {
    PetCareContext context;
    PetCareItemSnapshot target;
    GrowthPetLife life;
    std::uint64_t petRevision=0,elapsedOwnerMs=0;
};
struct PetLifePlan {
    PetLifeRequest expected;
    GrowthPetLife after;
    std::uint64_t consumedMs=0,catalogRevision=0;
    std::array<std::uint8_t,32> catalogHash{};
    bool expired=false,despawnFollower=false;
    // Expiry never removes the seal or any other pet instance state.
};
struct PetFeedRequest {
    PetCareContext context;
    PetCareItemSnapshot food,target;
    GrowthPetLife life;
    std::uint64_t petRevision=0;
    PetFeedingService service{};
    std::optional<GrowthPetProgress> progress;
};
struct PetFeedPlan {
    PetFeedRequest expected;
    GrowthPetLife after;
    std::optional<GrowthPetPlan> progress;
    std::uint32_t consumeFoodUnits=0;
    std::uint64_t restoredMs=0,catalogRevision=0;
    std::array<std::uint8_t,32> catalogHash{};
    bool revived=false;
};
enum class PetCareError {
    Forbidden,InvalidOperation,MissingItem,InvalidSnapshot,NotGrowthPet,NotPetEgg,
    InvalidLife,InvalidTime,InvalidName,InvalidBirthDecision,InsufficientGold,
    MissingFeedRule,InvalidService,WrongTargetState,NoEffect,PetStateSource,InvalidProgress,
};
class ItemCatalog;
std::expected<GrowthPetLife,PetCareError> MakeGrowthPetLife(const ItemCatalog&,ItemKey);
std::expected<PetHatchPlan,PetCareError> PlanPetHatch(const ItemCatalog&,const PetHatchRequest&);
std::expected<PetLifePlan,PetCareError> PlanPetLife(const ItemCatalog&,const PetLifeRequest&);
std::expected<PetFeedPlan,PetCareError> PlanPetFeed(const ItemCatalog&,const PetFeedRequest&);
}
