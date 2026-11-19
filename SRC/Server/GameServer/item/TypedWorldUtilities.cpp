#include "stdafx.h"
#include "TypedWorldUtilities.hpp"
#include "TypedRecovery.hpp"
#include "TypedTools.hpp"
#include "TypedFishProcessing.hpp"
#include "TypedGathering.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/SpatialHelpers.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../world/sectree_manager.h"
#include "../world/map_location.h"
#include "../world/arena.h"
#include "../world/fishing.h"
#include "../world/event.h"
#include "../quest/questpc.h"
#include "../combat/pvp.h"
#include "../core/utils.h"
#include "../core/vector.h"
#include "../core/config.h"
#include <item/WorldUtilityRuntime.hpp>
#include <openssl/rand.h>
#include <cmath>

bool IS_SUMMONABLE_ZONE(int);
extern int32_t kill_campfire_event(LPEVENT,int32_t);
namespace {
namespace item=auriga::item;namespace store=item::runtime;namespace P=ecs::PlayerRuntime;
void Tell(entt::entity e,const char* text){ecs::ChatSystem::Send(e,CHAT_TYPE_INFO,"%s",text);}
bool Operation(item::ItemCommit& c){return RAND_bytes(c.operationId.data(),16)==1 && std::ranges::any_of(c.operationId,[](auto b){return b!=0;});}
entt::entity Current(std::uint32_t pid,std::uint64_t token){const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* l=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;return l && l->token==token ? e : entt::null;}
bool Route(const item::RecallPosition& p,entt::entity e){std::int32_t map=0;std::uint32_t address=0;std::uint16_t port=0;
#ifdef ENABLE_GENERAL_CH
    const auto* desc=P::GetDesc(e);if(!desc)return false;
    const bool found=CMapLocation::instance().Get(desc->GetAccountTable().bChannel,p.x,p.y,map,address,port);
#else
    const bool found=CMapLocation::instance().Get(p.x,p.y,map,address,port);
#endif
    return found && map==p.map && address && port;
}
struct PreparedCampfire {
    entt::entity entity=entt::null;std::int32_t map=0,x=0,y=0;bool published=false;
    ~PreparedCampfire(){if(!published && g_registry.valid(entity))CHARACTER_MANAGER::instance().DestroyCharacter(entity);}
    void Publish(){if(published || !g_registry.valid(entity))return;published=true;ecs::MovementSystem::Show(entity,map,x,y,0);if(!g_registry.valid(entity))return;
        auto* info=AllocEventInfo<char_event_info>();info->ch=entity;P::SetCharEvent(entity,P::CharEvent::Mining,event_create(kill_campfire_event,info,PASSES_PER_SEC(40)));}
};
struct WarpAttempt {item::WorldWarpIntent intent;bool issued=false;};
item::WorldUtilityFacts Facts(entt::entity e,const item::PersistentItem& source){
    item::WorldUtilityFacts f;const auto admission=TypedRecoveryAdmission(e,{},source.count);
    f.character={admission.level,admission.classIndex,admission.sexIndex};f.character.inCombat=admission.inCombat;f.context=static_cast<item::QuestItemContext>(admission.context);
    f.position={P::GetMapIndex(e),P::GetX(e),P::GetY(e)};f.empire=P::GetEmpire(e);f.hp=std::max<std::int64_t>(0,P::GetHP(e));f.maximumHp=std::max<std::int64_t>(0,ecs::PointSystem::GetMaxHP(e));
    const auto* q=g_registry.try_get<ecs::QuestPCState>(e);auto* d=P::GetDesc(e);
    f.admitted=d && d->IsPhase(PHASE_GAME) && TypedGatheringAdmitted(e) && !TypedGatheringActive(e) && !g_registry.all_of<ecs::TypedItemBusy>(e) && q && q->pc && !q->pc->IsRunning();
    f.alive=!CombatSystem::IsDead(e) && f.hp>0;f.mapLoaded=ecs::GetMap(f.position.map)!=nullptr;f.arena=CArenaManager::instance().IsArenaMap(f.position.map);
    f.inDungeon=ecs::SocialSystem::GetDungeon(e)!=entt::null;f.warping=P::IsWarping(e);f.warpReady=P::CanWarp(e);f.summonZoneAllowed=IS_SUMMONABLE_ZONE(f.position.map);
#ifdef ENABLE_NEWSTUFF
    f.pvpPotionsBlocked=g_NoPotionsOnPVP && admission.inPvp;
#endif
    PIXEL_POSITION town{};if(ecs::GetRecallPosition(f.position.map,f.empire,town)){item::RecallPosition p{ecs::MapIndexAt(town.x,town.y),town.x,town.y};if(Route(p,e))f.town=p;}
    if(source.recall && source.recall->destination)f.savedDestinationAvailable=Route(*source.recall->destination,e);
    if(source.tool){std::map<std::uint32_t,entt::entity> stones;auto collect=[&](entt::entity stone){if(ecs::IsCharacter(stone) && P::IsStone(stone))stones.emplace(P::GetPacketVID(stone),stone);};if(auto* map=ecs::GetMap(f.position.map))map->for_each(collect);
        std::uint64_t nearest=UINT64_MAX;entt::entity target=entt::null;for(const auto& [vid,stone]:stones){const auto dx=std::int64_t(f.position.x)-P::GetX(stone),dy=std::int64_t(f.position.y)-P::GetY(stone);const auto distance=static_cast<std::uint64_t>(std::hypot(double(dx),double(dy)));if(distance && distance<nearest){nearest=distance;target=stone;f.stoneVid=vid;}}
        if(target!=entt::null){f.stoneRange=nearest<10000 ? 2 : nearest<70000 ? 1 : 3;f.stoneBearing=static_cast<std::uint16_t>(static_cast<int>(GetDegreeFromPositionXY(f.position.x,P::GetY(target),P::GetX(target),f.position.y))%360);}}
    return f;
}
}
bool TryUseTypedWorldUtility(entt::entity e,const item::PersistentItem& source,const item::ItemCatalog& catalog){
    if(!item::StatefulWorldUtility(catalog,source.key))return false;
    auto facts=Facts(e,source);const auto& state=g_registry.get<ecs::TypedItems>(e).state;const auto elapsed=TypedRecoveryElapsed(e),now=TypedItemEffectNow();
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permit || !permit->Current()){Tell(e,"A tárgy most nem használható.");return true;}
    std::shared_ptr<PreparedCampfire> fire;
    const auto* entry=catalog.Find(source.key);const auto* quest=entry ? std::get_if<item::ItemQuestRow>(&entry->category) : nullptr;
    if(quest && quest->groupCode=="utility.campfire"){
        float dx=0,dy=0;GetDeltaByDegree(P::GetRotation(e),100.0f,&dx,&dy);const auto x=static_cast<std::int32_t>(facts.position.x+dx),y=static_cast<std::int32_t>(facts.position.y+dy);auto* sector=ecs::SectorAt(facts.position.map,x,y);
        facts.campfireSpotAllowed=sector && !sector->IsAttr(x,y,ATTR_WATER);
        const auto quote=item::QuoteWorldUtility(catalog,state.fence.owner,source,state.recovery,facts,now);if(!quote){Tell(e,"Itt még nem állítható tábortűz (hely vagy újrahasználati idő).");return true;}
        fire=std::make_shared<PreparedCampfire>();fire->map=facts.position.map;fire->x=x;fire->y=y;
        fire->entity=CHARACTER_MANAGER::instance().SpawnMobEntity(fishing::CAMPFIRE_MOB,fire->map,x,y,0,false,number(0,359),false);facts.campfirePrepared=g_registry.valid(fire->entity);
    }
    const auto ids=store::PeekInstanceIds(1);item::ItemCommit operation;if(!Operation(operation)){Tell(e,"A tárgyművelet nem indítható.");return true;}
    const auto plan=item::MakeWorldUtilityCommit(catalog,operation.operationId,state.fence,state.storages,state.items,state.recovery,source.id,facts,elapsed,now,ids.empty() ? std::nullopt : std::optional(ids[0]));
    if(!plan || !plan->commit){Tell(e,"A tárgy itt vagy ebben az állapotban nem használható.");return true;}
    const auto pid=P::GetPlayerID(e);const auto token=g_registry.get<ecs::TypedItemLogin>(e).token;auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    const bool queued=store::RequestCommit(state.fence.owner,state.fence.sessionGeneration,*plan->commit,[pid,token,held,fire,quote=plan->quote](store::CommitOutcome out){
        if(out.write && fire)fire->Publish();const auto ch=Current(pid,token);if(ch==entt::null)return;
        g_registry.remove<ecs::TypedItemBusy>(ch);if(out.state)g_registry.emplace_or_replace<ecs::TypedItems>(ch,std::move(*out.state));
        if(out.write && out.state){
            if(quote.action==item::WorldUtilityAction::Detect){ecs::ChatSystem::Send(ch,CHAT_TYPE_COMMAND,"StoneDetect %u %u %u",P::GetPacketVID(ch),quote.stoneRange,quote.stoneBearing);if(!quote.stoneVid && quote.remainingUses)Tell(ch,"Nem található kő a térképen.");}
            if(quote.destination)ResumeTypedWorldWarp(ch);else if(quote.hpCost)ecs::PointSystem::Change(ch,POINT_HP,-static_cast<std::int64_t>(std::min<std::uint64_t>(quote.hpCost,std::max<std::int64_t>(0,P::GetHP(ch)))),false);
        }else Tell(ch,"A tárgyművelet mentése nem sikerült.");
        SendTypedInventory(ch);PumpTypedItemGrants(ch);
    });
    if(queued){if(plan->quote.split)store::SpendInstanceIds(1);}else{g_registry.remove<ecs::TypedItemBusy>(e);Tell(e,"A tárgytároló nem elérhető.");}return true;
}
bool ResumeTypedWorldWarp(entt::entity e){
    auto* t=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItems>(e) : nullptr;const auto* login=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;
    if(!t || !login || !t->state.recovery.worldWarp)return false;auto* d=P::GetDesc(e);if(!d || !d->IsPhase(PHASE_GAME))return true;
    const auto intent=*t->state.recovery.worldWarp;const auto& p=intent.destination;
    if(g_registry.all_of<ecs::TypedItemBusy>(e))return true;
    if(!g_registry.all_of<WarpAttempt>(e)){
        g_registry.emplace<WarpAttempt>(e,intent);
        const auto hp=std::max<std::int64_t>(0,P::GetHP(e));if(std::uint64_t(hp)>intent.hpCeiling)ecs::PointSystem::Change(e,POINT_HP,-static_cast<std::int64_t>(hp-intent.hpCeiling),false);
    }
    const bool arrived=P::GetMapIndex(e)==p.map && P::GetX(e)==p.x && P::GetY(e)==p.y;
    if(!arrived){if(!P::IsWarping(e) && !g_registry.get<WarpAttempt>(e).issued && Route(p,e)){
            g_registry.get<WarpAttempt>(e).issued=true;if(ecs::MovementSystem::WarpSet(e,p.x,p.y,p.map))ecs::SessionSystem::Save(e);else g_registry.get<WarpAttempt>(e).issued=false;
        }return true;}
    const auto catalog=store::CurrentCatalog();if(!catalog)return true;const auto elapsed=TypedRecoveryElapsed(e);auto next=item::AdvanceRecoverySchedule(*catalog,t->state.recovery,elapsed);if(!next || next->revision==UINT64_MAX)return true;
    next->worldWarp.reset();++next->revision;item::ItemCommit commit;if(!Operation(commit))return true;commit.kind=item::ItemOperationKind::Service;commit.catalogRevision=catalog->Revision();commit.catalogHash=catalog->ContentHash();commit.owners={t->state.fence};commit.outcomeCode=0x2003;commit.result={'R','T','M',1};commit.recovery=item::ItemRecoveryChange{t->state.fence.owner,t->state.recovery,*next,elapsed};
    const auto pid=P::GetPlayerID(e);const auto token=login->token;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    if(!store::RequestCommit(t->state.fence.owner,t->state.fence.sessionGeneration,std::move(commit),[pid,token](store::CommitOutcome out){const auto ch=Current(pid,token);if(ch==entt::null)return;g_registry.remove<ecs::TypedItemBusy>(ch);if(out.state)g_registry.emplace_or_replace<ecs::TypedItems>(ch,std::move(*out.state));if(out.write)g_registry.remove<WarpAttempt>(ch);SendTypedInventory(ch);PumpTypedItemGrants(ch);})){g_registry.remove<ecs::TypedItemBusy>(e);}return true;
}
