#pragma once
#include "MissionActions.hpp"
#include <item/ItemMissions.hpp>

namespace auriga::mission
{
// Deliberately restricted item hand-in graph: NPC objective -> one durable
// item effect -> success. Its single action receipt is the complete persisted
// progress. This does not activate general multi-stage Mission/resource state.
Definition ItemDeliveryDefinition(const item::ItemMission&,std::uint32_t npcRace);
ActionKey ItemDeliveryAction(const item::ItemMission&,CharacterId);
item::ItemMissionProof ItemDeliveryProof(const item::ItemMission&,CharacterId);
struct ItemDelivery
{
    std::shared_ptr<const DefinitionRegistry> definitions;
    std::unique_ptr<MissionRuntime> runtime;
    ActionKey action;
};
std::expected<ItemDelivery,std::string> BeginItemDelivery(std::shared_ptr<const DefinitionRegistry>,
    const item::ItemMission&,CharacterSession,std::uint32_t npcRace,Timestamp now);
bool FinishItemDelivery(ItemDelivery&,Timestamp now);
}
