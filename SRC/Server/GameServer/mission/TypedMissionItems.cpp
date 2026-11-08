#include "TypedMissionItems.hpp"
#include <algorithm>
#include <stdexcept>

namespace auriga::mission
{
std::expected<item::ItemMission, std::string> TypedItemContract(const item::ItemCatalog &catalog, const PinnedReward &pin)
{
    const auto &b=pin.Payload();
    if(b.schema!=2 || !ValidRewardBundle(b)) return std::unexpected("The Mission action needs a typed item payload");
    const auto identity=ActionIdentity(pin.Action());std::string code="typed.";
    for(auto byte:identity){code+="0123456789abcdef"[byte>>4];code+="0123456789abcdef"[byte&15];}
    item::ItemMission m; m.id=1;m.version=pin.Action().version;m.code=code;m.name="Mission";m.goldCost=b.goldCost;m.goldReward=b.gold;
    const auto convert=[](const auto &source,auto &target)->bool{for(const auto &[key,count]:source){const auto typed=item::ItemKey::Create(key.category,key.local);if(!typed)return false;target.push_back({*typed,key.level,count});}return true;};
    if(!convert(b.typedItems,m.rewards) || !convert(b.typedConsumeItems,m.costs) || !convert(b.typedRequirements,m.requirements) || !item::ValidateItemMission(catalog,m))return std::unexpected("Unknown or unsupported typed Mission item contract");
    return m;
}
void FenceTypedMissionPlan(persistence::Plan &plan,item::ItemOwnerFence fence)
{
    if(fence.owner.kind!=item::ItemOwnerKind::Character || !fence.sessionGeneration || fence.owner.id!=plan.character){plan.errors.push_back({"session","Mission/native login differs"});return;}
    plan.statements.insert(plan.statements.begin(),{"SELECT 1 FROM information_schema.TABLES WHERE TABLE_SCHEMA='player' AND TABLE_NAME IN ('item_mission_watch','item_mission_event') HAVING COUNT(*)=2 AND SUM(ENGINE='InnoDB')=2",{},{},1});
    plan.statements.insert(plan.statements.begin(),{"SELECT state_revision FROM player.item_owner_fence WHERE owner_kind=1 AND owner_id=? AND session_generation=? LOCK IN SHARE MODE",{SqlParam::UInt(fence.owner.id),SqlParam::UInt(fence.sessionGeneration)},{},1});
}
std::expected<TypedMissionWrite,std::string> PrepareTypedMissionWrite(const item::ItemCatalog &catalog,
    const DefinitionRegistry &registry,const MissionRuntime::Checkpoint &before,persistence::Owner owner,
    uint64_t revision,const ActionKey &action,const item::persistence::LoadedItemOwner &state,
    uint16_t level,std::span<const item::ItemInstanceId> ids,Timestamp now)
{
    try{
        if(before.character!=owner.character || action.character!=owner.character || state.fence.owner!=item::ItemOwner{item::ItemOwnerKind::Character,owner.character})return std::unexpected("Mission owner differs");
        const auto graph=registry.Get(action.mission,action.version);if(!graph)return std::unexpected("Missing exact Mission definition");
        const auto pin=PinReward(action,FingerprintDefinition(*graph),*graph);if(!pin)return std::unexpected("Invalid reward pin");
        const auto contract=TypedItemContract(catalog,*pin.reward);if(!contract)return std::unexpected(contract.error());
        MissionRuntime runtime(registry,{owner.character,1},before.now);
        const auto restored=runtime.Restore(before,before.now);const auto pending=runtime.PendingActions();
        if(restored.code!=ResultCode::Applied || std::ranges::find(pending,action)==pending.end())return std::unexpected("Mission action is not pending");
        const auto encoded=EncodeCheckpoint(before,registry);if(!encoded)return std::unexpected("Invalid pending checkpoint");
        auto commit=item::MakeItemMissionCommit(catalog,*contract,{state.fence.owner,ActionIdentity(action)},level,state.fence,state.storages,state.items,state.wallets,ids,now);
        if(!commit)return std::unexpected("Typed item requirements, balance or free space are insufficient");
        if(runtime.Acknowledge(action,std::max(now,before.now)).code!=ResultCode::Applied)return std::unexpected("Mission acknowledgement failed");
        auto checkpoint=runtime.Save();auto save=persistence::Save(checkpoint,registry,owner,revision,persistence::NewToken());
        auto entitlement=persistence::RecordEntitlement(owner,*pin.reward,revision,encoded.metadata.digest,persistence::NewToken());
        auto items=item::persistence::PrepareItemWrite(catalog,*commit);
        if(!save || !entitlement || !items)return std::unexpected("Mission/item SQL plan rejected");
        SqlTransaction all=std::move(entitlement.statements);all.insert(all.end(),items->begin(),items->end());all.insert(all.end(),save.statements.begin(),save.statements.end());save.statements=std::move(all);
        FenceTypedMissionPlan(save,state.fence);
        return TypedMissionWrite{std::move(save),std::move(*commit),std::move(checkpoint)};
    }catch(const std::exception &e){return std::unexpected(std::string(e.what()));}
}
}
