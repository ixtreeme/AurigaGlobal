#include <item/WorldUtilityRuntime.hpp>
#include <item/EffectRuntime.hpp>
#include <algorithm>
#include <cmath>
#include <set>

namespace auriga::item {
namespace {
using E=WorldUtilityError;
BuiltinQuestItem Handler(const ItemCatalog& c,ItemKey key){const auto* e=c.Find(key);const auto* q=e ? std::get_if<ItemQuestRow>(&e->category) : nullptr;return q ? BuiltinQuestHandler(q->groupCode) : BuiltinQuestItem::None;}
const ItemToolRow* Detector(const ItemCatalog& c,ItemKey key){const auto* e=c.Find(key);const auto* t=e ? std::get_if<ItemToolRow>(&e->category) : nullptr;return t && t->kind==ToolKind::Detector ? t : nullptr;}
int EmpireFor(int map){if(map<=0 || map>=10000 || map==66 || map==216)return -1;return map<20 ? 1 : map<40 ? 2 : map<60 ? 3 : 0;}
bool EmpireAllowed(int map,unsigned empire){const auto target=EmpireFor(map);return target>=0 && (!target || unsigned(target)==empire);}
std::uint64_t Percent(std::uint64_t n,unsigned percent){return n/100*percent+n%100*percent/100;}
bool Near(RecallPosition a,RecallPosition b){return std::hypot(double(a.x)-b.x,double(a.y)-b.y)<5000.;}
std::vector<std::uint8_t> Receipt(const WorldUtilityQuote& q,ItemInstanceId source,std::uint64_t now,std::optional<ItemInstanceId> split){
    std::vector<std::uint8_t> out{'W','U','R',1,static_cast<std::uint8_t>(q.action)};
    const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)out.push_back(static_cast<std::uint8_t>(v>>(8*i)));};
    put(source.Value(),8);put(split ? split->Value() : 0,8);put(now,8);put(q.hpCost,8);put(q.remainingUses,2);put(q.stoneVid,4);put(q.stoneBearing,2);put(q.stoneRange,1);
    for(const auto& pos:{q.destination,q.stamp}){put(pos.has_value(),1);if(pos){put(static_cast<std::uint32_t>(pos->map),4);put(static_cast<std::uint32_t>(pos->x),4);put(static_cast<std::uint32_t>(pos->y),4);}}
    return out;
}
}
bool StatefulWorldUtility(const ItemCatalog& c,ItemKey key){return Detector(c,key) || RecallItemMode(c,key)!=RecallMode::None || Handler(c,key)==BuiltinQuestItem::Campfire;}
std::uint64_t WorldUtilityCooldownRemaining(const ItemCatalog& c,const RecoverySchedule& state,std::uint64_t now){
    const auto* profile=c.FindConsumableCooldownByCode("utility.campfire");if(!profile)return 0;
    for(const auto& cd:state.worldCooldowns)if(cd.profile==profile->id)return cd.expiresAtUtcMs-std::min(now,cd.expiresAtUtcMs);
    return 0;
}
std::expected<WorldUtilityQuote,E> QuoteWorldUtility(const ItemCatalog& c,ItemOwner owner,const PersistentItem& source,
    const RecoverySchedule& schedule,const WorldUtilityFacts& f,std::uint64_t now)
{
    if(!StatefulWorldUtility(c,source.key))return std::unexpected(E::NotStateful);
    if(!PlainInventoryItem(source,owner) || !ValidatePersistentItem(c,source) || !ValidateRecoverySchedule(c,schedule) || now>INT64_MAX)return std::unexpected(E::InvalidSource);
    const auto* entry=c.Find(source.key);const auto* policy=c.FindUsePolicy(entry->base.usePolicy);
    const auto* rules=c.FindDefinitionRules(source.key,source.refinementLevel);const auto* quest=std::get_if<ItemQuestRow>(&entry->category);
    if(entry->base.retired || (rules && !rules->available) || (quest && !(quest->allowedContexts&static_cast<unsigned>(f.context))) ||
       !f.admitted || !f.alive || !f.mapLoaded || f.position.map<=0 || !f.character.level || f.character.level>255 || f.character.characterClass>3 || f.character.sex>1 || !policy ||
       f.character.level<entry->base.minimumLevel || !(policy->allowedClasses&(1u<<f.character.characterClass)) || !(policy->allowedSexes&(1u<<f.character.sex)) || (f.character.inCombat && !policy->useDuringCombat))return std::unexpected(E::Forbidden);
    WorldUtilityQuote q;
    if(Detector(c,source.key)){
        if(f.stoneBearing>=360 || f.stoneRange>3 || (f.stoneVid && !f.stoneRange) || (!f.stoneVid && (f.stoneBearing || f.stoneRange)))return std::unexpected(E::InvalidState);
        q.action=WorldUtilityAction::Detect;q.remainingUses=*source.tool->remainingUses-1;q.consume=q.remainingUses==0;
        if(!q.consume){q.stoneVid=f.stoneVid;q.stoneBearing=f.stoneBearing;q.stoneRange=f.stoneRange;}return q;
    }
    if(Handler(c,source.key)==BuiltinQuestItem::Campfire){
        const auto* cd=c.FindConsumableCooldownByCode("utility.campfire");
        if(!cd || !cd->durationMs || cd->durationMs>INT64_MAX-now || !f.campfireSpotAllowed)return std::unexpected(E::Forbidden);
        if(WorldUtilityCooldownRemaining(c,schedule,now))return std::unexpected(E::OnCooldown);
        q.action=WorldUtilityAction::Campfire;q.consume=true;return q;
    }
    const auto mode=RecallItemMode(c,source.key);
    if(f.arena || f.warping || !f.warpReady || f.pvpPotionsBlocked || f.position.map==200 || f.position.map==113)return std::unexpected(E::ZoneDenied);
    if(!f.maximumHp || f.maximumHp>INT64_MAX || f.hp>INT64_MAX || f.hp<Percent(f.maximumHp,30))return std::unexpected(E::HealthDenied);
    const auto fee=std::min(Percent(f.maximumHp,30),f.hp-std::min(f.hp,Percent(f.maximumHp,10)));
    const bool gates=RecallItemSummonGates(c,source.key);
    if(gates && !f.summonZoneAllowed)return std::unexpected(E::ZoneDenied);
    if(!source.recall->destination){
        if(mode==RecallMode::Memory && f.inDungeon)return std::unexpected(E::DungeonDenied);
        if(mode==RecallMode::Town){
            if(!f.town || !EmpireAllowed(f.town->map,f.empire))return std::unexpected(E::TargetUnavailable);
            if(gates && Near(f.position,*f.town))return std::unexpected(E::TooNear);
            q.action=WorldUtilityAction::WarpTown;q.destination=f.town;q.hpCost=fee;
        }else q.action=WorldUtilityAction::Stamp;
        if(!f.inDungeon){if(!EmpireAllowed(f.position.map,f.empire))return std::unexpected(E::ZoneDenied);q.stamp=f.position;q.split=source.count>1;}
        // A dungeon exit keeps its unstamped source but still persists its warp intent.
        return q;
    }
    const auto target=*source.recall->destination;
    if(!f.savedDestinationAvailable)return std::unexpected(E::TargetUnavailable);
    if(target.map>=301 && target.map<=304 && f.character.level<90)return std::unexpected(E::LevelDenied);
    if(!EmpireAllowed(target.map,f.empire)){q.action=WorldUtilityAction::ClearStamp;q.hpCost=mode==RecallMode::Memory ? fee : 0;return q;}
    // The original22011/22020 branches accidentally compared an all-zero
    // distance. Resolve the actual saved destination for every memory variant.
    if(gates && Near(f.position,target))return std::unexpected(E::TooNear);
    q.action=WorldUtilityAction::WarpRecall;q.destination=target;q.consume=true;q.hpCost=mode==RecallMode::Memory ? fee : 0;return q;
}
std::expected<WorldUtilityPlan,E> MakeWorldUtilityCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> inventory,const RecoverySchedule& before,ItemInstanceId id,
    const WorldUtilityFacts& facts,std::uint64_t elapsed,std::uint64_t now,std::optional<ItemInstanceId> splitId)
{
    const auto source=std::ranges::find(inventory,id,&PersistentItem::id);if(source==inventory.end())return std::unexpected(E::InvalidSource);
    auto next=AdvanceRecoverySchedule(c,before,elapsed);if(!next)return std::unexpected(E::InvalidState);NormalizeTimedItems(c,*next,now);
    if(next->worldWarp)return std::unexpected(E::Forbidden);
    const auto quote=QuoteWorldUtility(c,fence.owner,*source,*next,facts,now);if(!quote)return std::unexpected(quote.error());
    WorldUtilityPlan out{*quote,{}};
    if(quote->action==WorldUtilityAction::Campfire && !facts.campfirePrepared)return std::unexpected(E::SpawnUnprepared);
    ItemCommit commit;commit.operationId=op;commit.catalogRevision=c.Revision();commit.catalogHash=c.ContentHash();commit.kind=ItemOperationKind::Service;commit.owners={fence};
    const auto layout=std::ranges::find(layouts,ItemStorage{fence.owner,source->placement->kind,source->placement->storageId},&ItemStorageLayout::storage);
    if(layout==layouts.end())return std::unexpected(E::InvalidState);commit.storages={*layout};
    if(quote->consume){
        const auto consumed=MakeConsumeCommit(c,op,fence,layouts,inventory,id,1);if(!consumed)return std::unexpected(E::InvalidState);commit=*consumed;commit.kind=ItemOperationKind::Service;
    }else{
        auto after=*source;++after.revision;commit.items={{*source,std::move(after)}};
    }
    auto& changed=commit.items[0].after;
    if(quote->action==WorldUtilityAction::Detect)changed.tool->remainingUses=quote->remainingUses;
    if(quote->action==WorldUtilityAction::ClearStamp)changed.recall->destination.reset();
    if(quote->stamp){
        if(!quote->split)changed.recall->destination=quote->stamp;
        else{
            if(!splitId || *splitId==source->id || std::ranges::any_of(inventory,[&](const auto& i){return i.id==*splitId;}))return std::unexpected(E::NoIds);
            auto fresh=*source;fresh.id=*splitId;fresh.revision=1;fresh.count=1;fresh.recall->destination=quote->stamp;--changed.count;
            std::optional<ItemStorageLayout> selected;
            for(const auto& bag:layouts){
                if(bag.storage.owner!=fence.owner || bag.storage.kind!=ItemStorageKind::Inventory)continue;
                std::set<std::uint32_t> occupied;
                for(const auto& i:inventory)if(!i.destroyed && i.owner==fence.owner && i.placement && i.placement->kind==bag.storage.kind && i.placement->storageId==bag.storage.id){const auto cells=OccupiedItemCells(c,i,bag);if(!cells)return std::unexpected(E::InvalidState);occupied.insert(cells->begin(),cells->end());}
                for(std::uint32_t cell=0;cell<std::uint32_t{bag.columns}*bag.rows*bag.pages;++cell){fresh.placement=ItemPlacement{bag.storage.kind,bag.storage.id,cell};const auto cells=OccupiedItemCells(c,fresh,bag);if(cells && std::ranges::none_of(*cells,[&](auto v){return occupied.contains(v);})){selected=bag;break;}}
                if(selected)break;
            }
            if(!selected)return std::unexpected(E::NoRoom);
            if(selected->storage!=layout->storage)commit.storages.push_back(*selected);
            commit.items.push_back({{},std::move(fresh)});
        }
    }
    if(quote->action==WorldUtilityAction::Campfire){
        const auto* cd=c.FindConsumableCooldownByCode("utility.campfire");std::erase_if(next->worldCooldowns,[&](const auto& row){return row.profile==cd->id;});
        next->worldCooldowns.push_back({cd->id,now+cd->durationMs});std::ranges::sort(next->worldCooldowns,{},&WorldItemCooldown::profile);
    }
    if(quote->destination)next->worldWarp=WorldWarpIntent{*quote->destination,facts.hp-quote->hpCost};
    if(*next!=before){if(before.revision==UINT64_MAX)return std::unexpected(E::InvalidState);next->revision=before.revision+1;commit.recovery=ItemRecoveryChange{fence.owner,before,*next,elapsed};}
    commit.outcomeCode=WorldUtilityOutcome;commit.result=Receipt(*quote,id,now,quote->split ? splitId : std::nullopt);
    if(!ValidateItemCommit(c,commit))return std::unexpected(E::InvalidState);out.commit=std::move(commit);return out;
}
}
