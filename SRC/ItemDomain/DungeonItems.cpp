#include <item/DungeonItems.hpp>
#include <algorithm>
namespace auriga::item {
std::optional<ItemKey> DungeonItemKey(const ItemCatalog& c,std::string_view code){
    std::optional<ItemKey> key;for(const auto& [k,e]:c.Entries())if(e.base.contentCode==code){if(key)return {};key=k;}return key;
}
bool RuneKeyItem(const ItemCatalog& c,ItemKey key){const auto* e=c.Find(key);const auto* q=e ? std::get_if<ItemQuestRow>(&e->category) : nullptr;return q && (q->groupCode==RuneFragmentCode || q->groupCode==RuneFloorKeyCode);}
DungeonUseRole DungeonItemUseRole(const ItemCatalog& c,ItemKey key){const auto* e=c.Find(key);if(e)for(const auto& b:DungeonUseBindings)if(e->base.contentCode==b.code)return b.role;return DungeonUseRole::None;}
std::expected<ItemCommit,ItemMissionError> MakeDungeonUseCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> inventory,ItemInstanceId id,const DungeonUseFacts& f){
    const auto i=std::ranges::find(inventory,id,&PersistentItem::id);
    if(i==inventory.end() || !PlainInventoryItem(*i,fence.owner) || !ValidatePersistentItem(c,*i))return std::unexpected(ItemMissionError::Invalid);
    const auto role=DungeonItemUseRole(c,i->key);const auto* e=c.Find(i->key);const auto* p=e?c.FindUsePolicy(e->base.usePolicy):nullptr;
    const auto* rules=c.FindDefinitionRules(i->key,i->refinementLevel);const auto* profile=e?c.FindRefinement(e->base.refinementProfile):nullptr;const auto* level=profile?profile->FindLevel(i->refinementLevel):nullptr;
    if(role==DungeonUseRole::None || !p || !level || e->base.retired || (rules&&!rules->available) || !f.admitted || !f.stageReady || !f.character.level || f.character.level>255 || f.character.characterClass>3 || f.character.sex>1 ||
       f.character.level<std::max(e->base.minimumLevel,level->minimumLevel.value_or(0)) || !(p->allowedClasses&(1u<<f.character.characterClass)) || !(p->allowedSexes&(1u<<f.character.sex)) || (f.character.inCombat&&!p->useDuringCombat))return std::unexpected(ItemMissionError::Requirements);
    if(i->costume){const auto clock=FindWearableClock(c,i->key);if(!clock||!f.character.nowUtcMs)return std::unexpected(ItemMissionError::Requirements);const auto time=AdvanceItemClock(*clock,*i->costume,{f.character.nowUtcMs,0,false,true});if(!time||time->removeItem)return std::unexpected(ItemMissionError::Requirements);}
    auto out=MakeConsumeCommit(c,op,fence,layouts,inventory,id,1);if(!out)return std::unexpected(ItemMissionError::Invalid);
    out->outcomeCode=0x3701;out->result={'D','U','S',1,static_cast<std::uint8_t>(role)};return *out;
}
std::expected<RuneKeyAction,ItemMissionError> QuoteRuneKey(const ItemCatalog& c,const PersistentItem& source,ItemOwner owner,const RuneKeyFacts& f){
    if(!RuneKeyItem(c,source.key) || !PlainInventoryItem(source,owner) || !f.admitted || !f.dungeon || f.characterClass>3 || f.sex>1)return std::unexpected(ItemMissionError::Invalid);
    const auto* e=c.Find(source.key);const auto* p=c.FindUsePolicy(e->base.usePolicy);
    if(!p || !(p->allowedClasses&(1u<<f.characterClass)) || !(p->allowedSexes&(1u<<f.sex)) ||
       !PlanQuestItemUse(c,{source.key,source.count,f.level,QuestItemContext::World,false,f.inCombat,true,true}))return std::unexpected(ItemMissionError::Requirements);
    const auto& q=std::get<ItemQuestRow>(e->category);
    if(f.floor>=2 && f.floor<=4 && f.type==1 && f.leader)return q.groupCode==RuneFragmentCode ? RuneKeyAction::Exchange : RuneKeyAction::Advance;
    if(f.floor==5 && f.type==6 && f.pedestal && q.groupCode==RuneFloorKeyCode)return RuneKeyAction::Offer;
    return std::unexpected(ItemMissionError::Requirements);
}
namespace {
ItemMissionProof Proof(ItemOwner owner,std::array<std::uint8_t,16> op){ItemMissionProof p{owner};std::copy(op.begin(),op.end(),p.action.begin());std::copy(op.begin(),op.end(),p.action.begin()+16);return p;}
void Receipt(ItemCommit& c,std::array<std::uint8_t,16> op,std::uint8_t action){c.operationId=op;c.outcomeCode=RuneKeyOutcome;c.result={'D','R','K',1,action};}
}
std::expected<ItemCommit,ItemMissionError> MakeRuneKeyCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> inventory,ItemInstanceId id,const RuneKeyFacts& facts,std::span<const ItemInstanceId> ids,std::uint64_t now){
    const auto found=std::ranges::find(inventory,id,&PersistentItem::id);if(found==inventory.end())return std::unexpected(ItemMissionError::Invalid);
    const auto action=QuoteRuneKey(c,*found,fence.owner,facts);if(!action)return std::unexpected(action.error());
    if(*action!=RuneKeyAction::Exchange){auto out=MakeConsumeCommit(c,op,fence,layouts,inventory,id,1);if(!out)return std::unexpected(ItemMissionError::Invalid);Receipt(*out,op,static_cast<std::uint8_t>(*action));return out.value();}
    const auto key=DungeonItemKey(c,RuneFloorKeyCode);if(!key)return std::unexpected(ItemMissionError::Unavailable);
    ItemMission exchange;exchange.id=1;exchange.version=1;exchange.code="dungeon.rune.exchange";exchange.name="Rúna lélekkulcs";exchange.costs={{found->key,0,10}};exchange.rewards={{*key,0,1}};
    auto out=MakeItemMissionCommit(c,exchange,Proof(fence.owner,op),facts.level,fence,layouts,inventory,{},ids,now);if(!out)return out;Receipt(*out,op,static_cast<std::uint8_t>(*action));if(!ValidateItemCommit(c,*out))return std::unexpected(ItemMissionError::Invalid);return out;
}
std::expected<ItemCommit,ItemMissionError> MakeDungeonCleanupCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> inventory,ItemKey key,std::uint32_t maximum){
    const auto count=std::min<std::uint64_t>({CountMissionItems(inventory,fence.owner,key,0),maximum,50000});if(!count)return std::unexpected(ItemMissionError::Requirements);
    ItemMission cleanup;cleanup.id=1;cleanup.version=1;cleanup.code="dungeon.cleanup";cleanup.name="Kazamata tárgyainak leadása";cleanup.costs={{key,0,static_cast<std::uint32_t>(count)}};
    auto out=MakeItemMissionCommit(c,cleanup,Proof(fence.owner,op),255,fence,layouts,inventory,{},{});if(!out)return out;Receipt(*out,op,0);return out;
}
}
