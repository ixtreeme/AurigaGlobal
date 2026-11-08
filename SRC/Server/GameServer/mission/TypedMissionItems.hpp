#pragma once
#include "MissionPersistence.hpp"
#include "MissionProgression.hpp"
#include <ItemPersistenceSql.hpp>

namespace auriga::mission
{
// Explicit bridge from the immutable Mission action to the typed item writer.
// Neither a client nor Lua can submit an item amount or a completion receipt.
std::expected<item::ItemMission, std::string> TypedItemContract(const item::ItemCatalog &, const PinnedReward &);
struct MissionRewardFacts
{
    item::BookCharacter character;
    ProgressionRules rules;
    uint32_t accountId=0;
    std::optional<item::persistence::LoadedItemOwner> account;
};
struct TypedMissionWrite
{
    persistence::Plan plan;
    item::ItemCommit items;
    MissionRuntime::Checkpoint checkpoint;
};
std::expected<TypedMissionWrite, std::string> PrepareTypedMissionWrite(
    const item::ItemCatalog &, const DefinitionRegistry &, const MissionRuntime::Checkpoint &,
    persistence::Owner, uint64_t storageRevision, const ActionKey &, const item::persistence::LoadedItemOwner &,
    uint16_t level, std::span<const item::ItemInstanceId>, Timestamp now, const MissionRewardFacts *facts=nullptr);
// All Mission writes also carry the actual typed login generation. Claiming a
// Mission owner can therefore never resurrect an older character login.
void FenceTypedMissionPlan(persistence::Plan &, item::ItemOwnerFence);
}
