#pragma once
#include "Consumables.hpp"
#include <array>
#include <functional>
#include <span>
#include <vector>

namespace auriga::item
{
inline constexpr std::size_t MaxRecoveryDoses=18,MaxRecoveryCooldowns=64,MaxRecoveryStateBytes=4096;
struct RecoveryDose
{
    std::uint64_t id=0;ItemKey source;
    RecoveryDelivery delivery{};RecoveryPool pool{};
    std::int64_t remainingRaw=0,perTickRaw=0;
    std::uint64_t intervalMs=0,nextTickMs=0,remainingMs=0,dueTicks=0;
    std::uint16_t rateBp=0;
    bool operator==(const RecoveryDose&)const=default;
};
struct RecoveryCooldown
{std::uint32_t profile=0;std::uint64_t remainingMs=0;bool operator==(const RecoveryCooldown&)const=default;};
struct RecoverySchedule
{
    std::uint64_t revision=0,generation=0;
    std::array<std::uint16_t,2> creditRaw{};
    std::vector<RecoveryDose> doses;
    std::vector<RecoveryCooldown> cooldowns;
    bool operator==(const RecoverySchedule&)const=default;
};
struct RecoverySchedulePlan
{
    RecoverySchedule after;
    std::int64_t hpRestoredRaw=0,spRestoredRaw=0;
    std::uint16_t units=0,arenaQuota=0;
};
bool ValidateRecoverySchedule(const ItemCatalog&,const RecoverySchedule&);
std::expected<RecoverySchedule,RecoveryUseError> AdvanceRecoverySchedule(const ItemCatalog&,RecoverySchedule,std::uint64_t elapsedMs);
std::uint64_t RecoveryCooldownRemaining(const RecoverySchedule&,std::uint32_t);
std::expected<void,RecoveryUseError> SetRecoveryCooldown(const ItemCatalog&,RecoverySchedule&,std::uint32_t,std::uint64_t);
std::expected<RecoverySchedulePlan,RecoveryUseError> PlanScheduledRecoveryUse(const ItemCatalog&,ItemKey,const InstantRecoveryAdmission&,
    const RecoverySchedule&,std::uint64_t elapsedMs,std::array<std::uint64_t,2> freshIds);
std::expected<RecoverySchedulePlan,RecoveryUseError> PlanRecoveryScheduleTick(const ItemCatalog&,const RecoverySchedule&,std::uint64_t,
    const InstantRecoveryAdmission&,bool dead,bool stunned);
std::expected<RecoverySchedulePlan,RecoveryUseError> PlanRecoveryScheduleCancel(const ItemCatalog&,const RecoverySchedule&,std::uint64_t,
    std::uint64_t generation,std::uint64_t dose); // zero dose: all, including earned sub-point credit
std::expected<std::vector<std::uint8_t>,RecoveryUseError> EncodeRecoverySchedule(const ItemCatalog&,const RecoverySchedule&);
std::expected<RecoverySchedule,RecoveryUseError> DecodeRecoverySchedule(const ItemCatalog&,std::span<const std::uint8_t>);
}
