#pragma once
#include "PetCare.hpp"
#include "Costumes.hpp"
#include <span>
#include <variant>
#include <vector>

namespace auriga::item {
// Domain effects with fixed units: MaximumHpFlat is integer HP, all others bp.
// They are not legacy APPLY codes. Runtime stat publication replaces one entire
// source bundle; it must never append a second copy on refresh.
enum class PetEffect : std::uint8_t {
    MaximumHpFlat=1,MaximumHpPercent=2,MonsterDamagePercent=3,NormalPveDamagePercent=4,
    MonsterResistancePercent=5,HumanResistancePercent=6,WarriorResistancePercent=7,
    SuraResistancePercent=8,NinjaResistancePercent=9,ShamanResistancePercent=10,
    BossDamagePercent=11,MetinDamagePercent=12,PveCriticalChance=13,ExperiencePercent=14,
    GoldDropPercent=15,AttackBonusPercent=16,BlockChance=17,DodgeArrowChance=18,
};
struct PetAgeTier {
    std::uint64_t minimumAgeMs=0;
    std::uint32_t maximumHpFlat=0;
    std::uint16_t monsterResistanceBp=0,humanResistanceBp=0;
};
struct PetSlotRule {
    std::uint8_t slot=0,minimumStage=0;
    std::uint64_t minimumAgeMs=0;
};
struct PetSkillRank {
    std::uint16_t rank=0;
    std::int64_t value=0;
};
struct PetSkillDefinition {
    std::uint32_t id=0;
    std::string code;
    PetEffect effect{};
    std::vector<PetSkillRank> ranks;
};
struct PetTrainingProfile {
    std::uint32_t id=0;
    std::string code;
    std::uint16_t hpEvolutionPeriod=0,monsterEvolutionPeriod=0,pveEvolutionPeriod=0;
    std::uint16_t evolutionMinimum=1,evolutionMaximum=6;
    std::vector<PetAgeTier> ages;
    std::vector<PetSlotRule> slots;
    std::vector<PetSkillDefinition> skills;
};
bool ValidPetTrainingProfile(const PetTrainingProfile&) noexcept;
struct PetSkillSlot {
    bool unlocked=false;
    std::uint32_t skill=0;
    std::uint16_t rank=0;
    bool operator==(const PetSkillSlot&) const=default;
};
struct GrowthPetState {
    std::string name;
    std::uint64_t bornAtUtcMs=0,ageMs=0;
    GrowthPetProgress progress;
    GrowthPetLife life;
    PetBirthTraits traits;
    std::array<PetSkillSlot,4> skills{};
    // Zero means the seal is in its ordinary item location. Otherwise the
    // unchanged seal instance is contained in this exact carrier instance.
    std::uint64_t carrierInstanceId=0;
    bool everSummoned=false,recallRequested=false;
    bool operator==(const GrowthPetState&) const=default;
};
enum class PetSupplyKind : std::uint8_t {
    Study=1,ResetOne=2,ResetAll=3,ExtendLife=4,PermanentLife=5,Enchant=6,Carrier=7,Skin=8,
};
struct PetStudyRule {
    std::uint32_t skill=0;
    bool selectedAllowed=false,knownAllowed=false;
    std::uint16_t knownSuccessBp=0;
    std::uint64_t knownCooldownMs=0;
};
struct PetExtensionRule { std::uint64_t addedMs=0,maximumCapacityMs=0; };
struct PetEnchantRule {
    std::uint16_t successBp=0,step=0;
    PetBirthTraits limits;
};
struct PetSkinRule {
    std::uint32_t form=0;
    CostumeClock clock{};
    std::uint64_t durationMs=0;
};
struct PetSupplyProfile {
    std::uint32_t id=0;
    std::string code;
    PetSupplyKind kind{};
    std::optional<PetStudyRule> study;
    std::optional<PetExtensionRule> extension;
    std::optional<PetEnchantRule> enchant;
    std::optional<PetSkinRule> skin;
};
bool ValidPetSupplyProfile(const PetSupplyProfile&) noexcept;
struct ItemPetSupplyRow { ItemKey key; std::uint32_t profile=0; };
enum class PetLifecycleError {
    Forbidden,InvalidOperation,InvalidSnapshot,MissingItem,NotGrowthPet,MissingTraining,
    InvalidState,InvalidTime,WrongSupply,InvalidSkill,LockedSlot,DuplicateSkill,MaximumRank,
    Cooldown,InvalidDecision,NoEffect,WrongTargetState,InvalidPlacement,InvalidCodec,
};
class ItemCatalog;
std::expected<void,PetLifecycleError> ValidateGrowthPetState(const ItemCatalog&,ItemKey,const GrowthPetState&);
std::expected<GrowthPetState,PetLifecycleError> MakeGrowthPetState(const ItemCatalog&,const PetHatchPlan&);

struct PetLifecycleRequest {
    PetCareContext context;
    PetCareItemSnapshot seal;
    std::uint64_t petRevision=0,nowUtcMs=0;
    GrowthPetState state;
};
struct PetLifecyclePlan {
    PetLifecycleRequest expected;
    GrowthPetState after;
    std::uint64_t catalogRevision=0;
    std::array<std::uint8_t,32> catalogHash{};
};
std::expected<PetLifecyclePlan,PetLifecycleError> PlanPetAge(const ItemCatalog&,const PetLifecycleRequest&);
struct PetMaintenancePlan {
    PetLifecyclePlan pet;
    std::uint64_t elapsedOwnerMs=0;
    bool expired=false,despawnFollower=false;
};
std::expected<PetMaintenancePlan,PetLifecycleError> PlanPetMaintenance(const ItemCatalog&,const PetLifecycleRequest&,std::uint64_t elapsedOwnerMs);
struct PetSummonPlan {
    PetLifecyclePlan pet;
    std::uint32_t form=0;
    bool spawnFollower=false,despawnFollower=false;
};
std::expected<PetSummonPlan,PetLifecycleError> PlanGrowthPetSummon(const ItemCatalog&,const PetLifecycleRequest&,
    bool summonRequested,bool worldAllowsPet,bool channelOccupiedByOther);
std::expected<PetLifecyclePlan,PetLifecycleError> PlanPetEvolution(const ItemCatalog&,const PetLifecycleRequest&,PetBirthTraits recordedGrowth);
struct PetSupplyRequest {
    PetLifecycleRequest pet;
    PetCareItemSnapshot supply;
    std::optional<std::uint8_t> selectedSlot;
    std::uint8_t traitIndex=0;
    std::uint16_t recordedRollBp=0;
    std::uint64_t ownerCooldownRevision=0,ownerSkillNotBeforeUtcMs=0,bypassEffectRevision=0;
    bool bypassAvailable=false;
};
struct PetSupplyPlan {
    PetSupplyRequest expected;
    GrowthPetState after;
    std::uint32_t consumeUnits=1;
    bool succeeded=false,consumeBypass=false,writeOwnerCooldown=false;
    std::uint32_t cooldownSkill=0;
    std::uint64_t ownerSkillNotBeforeUtcMs=0,catalogRevision=0;
    std::array<std::uint8_t,32> catalogHash{};
};
std::expected<PetSupplyPlan,PetLifecycleError> PlanPetSupply(const ItemCatalog&,const PetSupplyRequest&);
enum class PetBonusSource : std::uint8_t { Age=1,Trait=2,Skill=3 };
struct PetBonusContribution {
    PetEffect effect{};
    std::int64_t value=0;
    PetBonusSource source{};
    std::uint8_t slot=0;
};
struct PetBonusPlan {
    PetLifecyclePlan pet;
    std::vector<PetBonusContribution> contributions;
    bool replaceSource=true;
};
std::expected<PetBonusPlan,PetLifecycleError> PlanPetBonuses(const ItemCatalog&,const PetLifecycleRequest&,bool worldAllowsPet,bool inDungeon);
struct PetTransportRequest {
    PetLifecycleRequest pet;
    PetCareItemSnapshot carrier;
    bool unpack=false,placementReservedByServer=false;
    std::uint64_t carrierRevision=0,containedSealInstanceId=0;
};
struct PetTransportPlan {
    PetTransportRequest expected;
    GrowthPetState after;
    bool consumeCarrier=false;
    std::uint64_t catalogRevision=0;
    std::array<std::uint8_t,32> catalogHash{};
};
std::expected<PetTransportPlan,PetLifecycleError> PlanPetTransport(const ItemCatalog&,const PetTransportRequest&);
struct PetAppearanceRequest {
    PetLifecycleRequest pet;
    PetCareItemSnapshot skin;
    CostumeLifetimeState lifetime;
    CostumeClockStep time;
    bool equipRequested=false,worldAllowsPet=false;
};
struct PetAppearancePlan {
    PetAppearanceRequest expected;
    CostumeLifetimePlan lifetime;
    std::uint32_t form=0;
    bool equip=false;
    std::uint64_t catalogRevision=0;
    std::array<std::uint8_t,32> catalogHash{};
};
std::expected<PetAppearancePlan,PetLifecycleError> PlanPetAppearance(const ItemCatalog&,const PetAppearanceRequest&);
// Explicit little-endian v1 envelope, exact EOF and bounded fields; never
// serializes struct memory. This is not a runtime vnum conversion or packet.
std::expected<std::vector<std::uint8_t>,PetLifecycleError> EncodeGrowthPetState(const ItemCatalog&,ItemKey,const GrowthPetState&);
std::expected<GrowthPetState,PetLifecycleError> DecodeGrowthPetState(const ItemCatalog&,ItemKey,std::span<const std::uint8_t>);
}
