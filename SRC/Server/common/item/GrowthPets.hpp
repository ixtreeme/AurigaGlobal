#pragma once
#include "ItemKey.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace auriga::item {
enum class GrowthExpPolicy : std::uint8_t { MobOnly=1, MobAndItem=2 };
struct GrowthPetLevel {
    std::uint16_t level=0;
    std::uint64_t mobExperience=0,itemExperience=0;
};
struct GrowthPetStage {
    std::uint8_t stage=0;
    std::uint16_t maximumLevel=0;
    std::uint32_t form=0;
};
struct GrowthPetProfile {
    std::uint32_t id=0;
    std::string code;
    std::uint16_t maximumLevel=0;
    GrowthExpPolicy experiencePolicy{};
    std::vector<GrowthPetLevel> levels;
    std::vector<GrowthPetStage> stages;
};
bool ValidGrowthPetProfile(const GrowthPetProfile&) noexcept;
// A disjoint part of pet instance state. Never overwrite name, lifetime, age,
// skills, bonuses, recall or transport data when committing this progression.
struct GrowthPetProgress {
    std::uint16_t level=0;
    std::uint8_t stage=0;
    std::uint64_t mobExperience=0,itemExperience=0;
    bool operator==(const GrowthPetProgress&) const=default;
};
bool ValidGrowthPetProgress(const GrowthPetProfile&,const GrowthPetProgress&) noexcept;
enum class GrowthPetAction : std::uint8_t { AwardMobExperience=1,AwardItemExperience=2,Evolve=3 };
struct GrowthPetRequest {
    ItemKey key;
    GrowthPetProgress progress;
    std::uint64_t instanceId=0,revision=0,progressRevision=0,ownerId=0,actorId=0,rulesetRevision=0;
    std::array<std::uint8_t,16> operationId{};
    std::uint32_t count=0;
    std::uint16_t refinementLevel=0;
    GrowthPetAction action{};
    std::uint64_t experience=0;
    bool admittedByServer=false,activePet=false,lockedByOther=false,exchanging=false;
};
struct GrowthPetPlan {
    GrowthPetRequest expected;
    GrowthPetProgress after;
    std::uint32_t beforeForm=0,afterForm=0;
    bool levelled=false,evolved=false,atEvolutionGate=false,atMaximum=false;
    std::uint64_t acceptedExperience=0,discardedExperience=0;
    std::uint64_t catalogRevision=0;
    std::array<std::uint8_t,32> catalogHash{};
};
enum class GrowthPetError {
    Forbidden,InvalidOperation,MissingItem,NotGrowthPet,InvalidSnapshot,
    InvalidProgress,InvalidAction,InvalidExperience,NotAtEvolutionGate,
};
class ItemCatalog;
std::expected<GrowthPetProgress,GrowthPetError> MakeGrowthPetProgress(const ItemCatalog&,ItemKey);
// No resource mutations. Owner must receipt all expected revisions and recheck
// the live actor generation before publishing a level/form change.
std::expected<GrowthPetPlan,GrowthPetError> PlanGrowthPetProgress(const ItemCatalog&,const GrowthPetRequest&);
}
