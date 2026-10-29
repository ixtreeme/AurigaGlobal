#pragma once
#include "ItemKey.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace auriga::item {
// Fixed summons only. Growth pets, their eggs/skills/feeding and skins are not
// admitted as fixed companions. A creature template is not an item identifier.
enum class CompanionKind : std::uint8_t { Pet=1, Mount=2 };
enum class CompanionClock : std::uint8_t { Permanent=1, SinceCreation=2, SinceFirstUse=3, ActiveTime=4 };
struct CompanionForm {
    std::uint32_t id=0;
    std::string code;
    std::uint32_t mobTemplateId=0;
    std::string modelAsset;
};
struct ItemCompanionRow {
    ItemKey key;
    CompanionKind kind{};
    std::uint32_t form=0;
    std::optional<std::uint32_t> ridingForm;
    CompanionClock clock=CompanionClock::Permanent;
    std::uint64_t durationMs=0;
    bool dungeonOnlyBonus=false;
};
bool ValidCompanion(const ItemCompanionRow&) noexcept;
struct CompanionLifetime {
    bool started=false,expired=false;
    std::uint64_t deadlineUtcMs=0,remainingMs=0;
    bool operator==(const CompanionLifetime&) const = default;
};
enum class CompanionPhase : std::uint8_t { Dormant=1, Following=2, Riding=3 };
struct CompanionState {
    CompanionPhase phase=CompanionPhase::Dormant;
    std::uint32_t activeFormId=0; // Actual runtime presentation, zero when dormant.
    bool recallRequested=false; // Intent, never proof that an actor exists.
    CompanionLifetime lifetime;
    bool operator==(const CompanionState&) const = default;
};
enum class CompanionAction : std::uint8_t { Summon=1, Dismiss=2, Mount=3, Dismount=4, Maintain=5 };
struct CompanionRequest {
    ItemKey key;
    CompanionState state;
    std::uint64_t instanceId=0,revision=0,ownerId=0,actorId=0,rulesetRevision=0;
    std::array<std::uint8_t,16> operationId{};
    std::uint32_t count=0;
    std::uint16_t refinementLevel=0,characterLevel=0;
    std::uint8_t characterClass=0,sex=0;
    CompanionAction action{};
    std::uint64_t nowUtcMs=0,elapsedOwnerMs=0;
    bool admittedByServer=false,lockedByOther=false,exchanging=false;
    bool ownerAlive=false,polymorphed=false,observer=false,inCombat=false,inDungeon=false;
    bool worldAllowsCompanion=false,worldAllowsRiding=false,channelOccupiedByOther=false;
};
struct CompanionPlan {
    CompanionRequest expected;
    CompanionState after;
    CompanionForm followerForm;
    std::optional<CompanionForm> ridingForm;
    bool spawnFollower=false,despawnFollower=false,changeRider=false,bonusEnabled=false,removeItem=false;
    std::optional<std::uint32_t> baseBonusProfile,refinementBonusProfile;
    std::uint64_t catalogRevision=0;
    std::array<std::uint8_t,32> catalogHash{};
};
enum class CompanionError {
    Forbidden,InvalidOperation,InvalidSnapshot,MissingItem,NotCompanion,InvalidLevel,
    InvalidState,InvalidAction,InvalidTime,TimeOverflow,AlreadyActive,NotFollowing,NotRiding,
    WrongKind,ChannelOccupied,
};
class ItemCatalog;
std::expected<CompanionState,CompanionError> MakeCompanionState(const ItemCatalog&,ItemKey,std::uint64_t createdAtUtcMs);
// Pure, owned intent. The owner validates session/world/actor generations and
// revisions and atomically receipts lifetime, active channel, binding and item
// state. Only then may it spawn/despawn or publish a rider/stat change. A spawn
// failure must leave the item dormant, not lock it behind a fictitious actor.
std::expected<CompanionPlan,CompanionError> PlanCompanion(const ItemCatalog&,const CompanionRequest&);
}
