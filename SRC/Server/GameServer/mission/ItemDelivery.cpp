#include "ItemDelivery.hpp"

namespace auriga::mission
{
Definition ItemDeliveryDefinition(const item::ItemMission& m,std::uint32_t npcRace)
{
    Definition d;d.id=m.code;d.version=m.version;d.root="root";
    Node root;root.id="root";root.kind=NodeKind::Sequence;root.children={"talk","deliver","done"};
    Node talk;talk.id="talk";talk.kind=NodeKind::Objective;talk.objective={EventKind::NpcInteracted,"npc."+std::to_string(npcRace),1,false};
    Node delivery;delivery.id="deliver";delivery.kind=NodeKind::Reward;delivery.reward="items";
    Node done;done.id="done";done.kind=NodeKind::Success;d.nodes={root,talk,delivery,done};return d;
}
ActionKey ItemDeliveryAction(const item::ItemMission& m,CharacterId pid)
{return {pid,m.code,m.version,1,"deliver","items"};}
item::ItemMissionProof ItemDeliveryProof(const item::ItemMission& m,CharacterId pid)
{return {{item::ItemOwnerKind::Character,pid},ActionIdentity(ItemDeliveryAction(m,pid))};}
std::expected<ItemDelivery,std::string> BeginItemDelivery(std::shared_ptr<const DefinitionRegistry> definitions,
    const item::ItemMission& m,CharacterSession owner,std::uint32_t npcRace,Timestamp now)
{
    if(!definitions || !owner.character || !owner.session || !npcRace)return std::unexpected("invalid item delivery identity");
    ItemDelivery delivery;delivery.definitions=std::move(definitions);delivery.action=ItemDeliveryAction(m,owner.character);
    delivery.runtime=std::make_unique<MissionRuntime>(*delivery.definitions,owner,now);
    if(delivery.runtime->Start(m.code,m.version,1,now).code!=ResultCode::Applied)return std::unexpected("item delivery graph could not start");
    GameplayEvent event{owner,{1,1},now,EventKind::NpcInteracted,"npc."+std::to_string(npcRace),1,{}};
    if(delivery.runtime->Dispatch(event,now).code!=ResultCode::Applied || delivery.runtime->PendingActions()!=std::vector<ActionKey>{delivery.action})
        return std::unexpected("item delivery action is not pending");
    return delivery;
}
bool FinishItemDelivery(ItemDelivery& delivery,Timestamp now)
{
    if(!delivery.runtime)return false;
    const auto result=delivery.runtime->Acknowledge(delivery.action,now);
    const auto current=delivery.runtime->Inspect(1);
    return (result.code==ResultCode::Applied || result.code==ResultCode::Duplicate || result.code==ResultCode::NoChange) && current && current->status==MissionStatus::Completed;
}
}
