#pragma once
#include "ItemKey.hpp"
#include "WeaponTypes.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace auriga::item
{
// Sashes, summons, runes and visual effects require their own behavior. They
// cannot be published as these ordinary body/hair/weapon costumes.
enum class CostumeKind : std::uint8_t { Body = 1, Hair = 2, Weapon = 3 };
enum class CostumeClock : std::uint8_t { Permanent = 1, SinceCreation = 2, SinceFirstUse = 3, EquippedTime = 4 };
struct ItemCostumeRow
{
    ItemKey key;
    CostumeKind kind{};
    std::optional<WeaponSubtype> weapon;
    CostumeClock clock = CostumeClock::Permanent;
    std::uint64_t durationMs = 0;
};
struct CostumeVisualRow
{
    ItemKey key;
    std::uint8_t characterClass = 0, sex = 0; // Classes 1..4, male=1/female=2.
    std::string modelAsset;
};
bool ValidCostume(const ItemCostumeRow&) noexcept;
struct CostumeLifetimeState
{
    bool started = false, expired = false;
    std::uint64_t deadlineUtcMs = 0, remainingMs = 0;
    bool operator==(const CostumeLifetimeState&) const = default;
};
struct CostumeClockStep
{
    std::uint64_t nowUtcMs = 0, elapsedMs = 0; // Server UTC and monotonic delta.
    bool wasEquipped = false, useRequested = false; // Admitted use, even if equipment later fails.
};
// Shared item-clock mathematics; domain-specific callers validate their own
// content identity. No synthetic costume prototype is needed for a pet skin.
struct CostumeClockDefinition { CostumeClock clock; std::uint64_t durationMs; };
struct CostumeLifetimePlan
{
    CostumeLifetimeState after;
    bool removeItem = false, unequip = false;
};
enum class CostumeLifetimeError { InvalidDefinition, InvalidState, TimeOverflow };
std::expected<CostumeLifetimeState,CostumeLifetimeError> MakeItemClock(CostumeClockDefinition,std::uint64_t createdAtUtcMs);
std::expected<CostumeLifetimePlan,CostumeLifetimeError> AdvanceItemClock(CostumeClockDefinition,const CostumeLifetimeState&,CostumeClockStep);
// Factory only: never reconstruct a saved instance with this function, since
// that would refill its lifetime. The offline converter preserves saved state.
std::expected<CostumeLifetimeState,CostumeLifetimeError> MakeCostumeLifetime(
    const ItemCostumeRow&,std::uint64_t createdAtUtcMs);
std::expected<CostumeLifetimePlan,CostumeLifetimeError> AdvanceCostumeLifetime(
    const ItemCostumeRow&,const CostumeLifetimeState&,CostumeClockStep);
struct CostumeEquipRequest
{
    ItemKey item;
    std::uint32_t stackUnits = 0;
    std::uint16_t characterLevel = 0, refinementLevel = 0;
    std::uint8_t characterClass = 0, sex = 0;
    std::optional<WeaponSubtype> equippedWeapon;
    CostumeLifetimeState lifetime;
    CostumeClockStep time;
    bool admittedByServer = false, inCombat = false;
};
struct CostumeEquipPlan
{
    ItemKey item;
    CostumeKind slot;
    std::uint16_t refinementLevel = 0;
    std::string modelAsset;
    CostumeLifetimePlan lifetime;
    std::uint64_t catalogRevision = 0;
    std::array<std::uint8_t,32> catalogHash{};
    // Expiry is a commit-able outcome: an attempted equip must not discard the
    // expired latch and let a later UTC rollback revive the same instance.
    bool equip = true;
};
enum class CostumeEquipError
{
    Forbidden, MissingItem, NotCostume, RetiredItem, InvalidStack, InvalidCharacter,
    LevelTooLow, InvalidRefinement, WeaponRequired, WeaponMismatch, InvalidLifetime, MissingVisual,
};
class ItemCatalog;
// Owning appearance/lifetime intent, not an equipment or persistence writer.
// The owner binds concrete instance/revision/session, checks locks/location and
// the displaced item, then atomically commits the swap and lifetime change.
// SinceFirstUse starts before equipment-specific rejection in the source game.
// The item-use owner must separately commit AdvanceCostumeLifetime on admitted
// use, even when this equipment plan rejects (e.g. wrong/missing real weapon).
// Retry this planner with that committed state; never reset the saved deadline.
std::expected<CostumeEquipPlan,CostumeEquipError> PlanCostumeEquip(const ItemCatalog&,const CostumeEquipRequest&);
}
