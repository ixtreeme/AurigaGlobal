#include "stdafx.h"
#include "TypedDungeonItems.hpp"
#include "TypedDungeonActions.hpp"
#include "TypedRecovery.hpp"
#include "TypedGathering.hpp"
#include "TypedTools.hpp"
#include "TypedFishProcessing.hpp"
#include "../ecs/SpatialHelpers.hpp"
#include "../world/sectree_manager.h"
#include <cmath>
#include "../dungeon/VikingDungeon.h"
#include "../dungeon/Halloween2022Dungeon.h"
#include "../dungeon/LostCastleDungeon.h"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/CharacterAccessors.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../quest/questpc.h"
#include "../network/desc.h"
#include "../dungeon/dungeon.h"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "../dungeon/RuneDungeon.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../entity/char_manager.h"
#include <openssl/rand.h>
#include <deque>
namespace {
namespace item=auriga::item;
struct Cleanup {std::string code;std::uint32_t maximum;};
struct PendingCleanup {std::deque<Cleanup> entries;};
void Pump(entt::entity e){
    if(!g_registry.valid(e) || g_registry.all_of<ecs::TypedItemBusy>(e))return;
    auto* pending=g_registry.try_get<PendingCleanup>(e);auto* typed=g_registry.try_get<ecs::TypedItems>(e);auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);
    const auto catalog=item::runtime::CurrentCatalog();if(!pending || !typed || !login || !catalog)return;
    while(!pending->entries.empty()){
        const auto request=pending->entries.front();const auto key=item::DungeonItemKey(*catalog,request.code);
        if(!key || !item::CountMissionItems(typed->state.items,typed->state.fence.owner,*key,0)){pending->entries.pop_front();continue;}
        std::array<std::uint8_t,16> op{};if(RAND_bytes(op.data(),op.size())!=1)return;
        auto commit=item::MakeDungeonCleanupCommit(*catalog,op,typed->state.fence,typed->state.storages,typed->state.items,*key,request.maximum);if(!commit)return;
        std::uint32_t removed=0;for(const auto& change:commit->items)removed+=(change.before?change.before->count:0)-change.after.count;
        const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=login->token;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
        if(!item::runtime::RequestCommit(typed->state.fence.owner,typed->state.fence.sessionGeneration,std::move(*commit),[pid,token,removed](item::runtime::CommitOutcome out){
            const auto ch=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* l=g_registry.valid(ch) ? g_registry.try_get<ecs::TypedItemLogin>(ch) : nullptr;if(!l || l->token!=token)return;
            g_registry.remove<ecs::TypedItemBusy>(ch);if(out.state)g_registry.emplace_or_replace<ecs::TypedItems>(ch,std::move(*out.state));
            if(out.write){if(auto* q=g_registry.try_get<PendingCleanup>(ch);q && !q->entries.empty()){auto& request=q->entries.front();const auto used=std::min<std::uint32_t>(request.maximum,removed);request.maximum-=used;if(!request.maximum)q->entries.pop_front();}}
            SendTypedInventory(ch);PumpTypedItemGrants(ch);
        }))g_registry.remove<ecs::TypedItemBusy>(e);
        return;
    }
    g_registry.remove<PendingCleanup>(e);
}
}
bool TryUseTypedDungeonKey(entt::entity e,const item::PersistentItem& source,const item::ItemCatalog& catalog){
 if(item::RuneKeyItem(catalog,source.key)){CRuneDungeon::instance().OnUseTypedKey(e,source,catalog);return true;}
 const auto role=item::DungeonItemUseRole(catalog,source.key);if(role==item::DungeonUseRole::None)return false;
 if(role>=item::DungeonUseRole::VikingFire)CVikingDungeon::instance().OnUseTypedItem(e,source,catalog);
 else if(role>=item::DungeonUseRole::CathedralCross)CHalloween2022Dungeon::instance().OnUseTypedItem(e,source,catalog);
 else CLostCastleDungeon::instance().OnUseTypedItem(e,source,catalog);return true;
}
std::optional<item::ItemKey> FindTypedDungeonUseKey(item::DungeonUseRole role)
{
    const auto catalog = item::runtime::CurrentCatalog();
    if (!catalog)
        return {};
    const auto binding = std::ranges::find(item::DungeonUseBindings, role, &item::DungeonUseBinding::role);
    if (binding == item::DungeonUseBindings.end())
        return {};
    const auto key = item::DungeonItemKey(*catalog, binding->code);
    const auto* definition = key ? catalog->Find(*key) : nullptr;
    return definition && !definition->base.retired ? key : std::nullopt;
}

void QueueTypedDungeonCleanup(entt::entity e,std::string_view code,std::uint32_t maximum){if(!maximum || !g_registry.valid(e) || !g_registry.all_of<ecs::TypedItems>(e))return;auto& q=g_registry.get_or_emplace<PendingCleanup>(e).entries;if(q.size()<16)q.push_back({std::string(code),maximum});Pump(e);}
void TickTypedDungeonCleanup(){std::vector<entt::entity> owners;for(auto e:g_registry.view<PendingCleanup>())owners.push_back(e);for(auto e:owners)Pump(e);}

namespace {
struct DungeonActionPending {};
struct DungeonActionGuard {entt::entity lock=entt::null;bool completed=false;~DungeonActionGuard(){if(g_registry.valid(lock))g_registry.remove<DungeonActionPending>(lock);}};
}
bool TypedDungeonActionPending(entt::entity e){return g_registry.valid(e)&&g_registry.all_of<DungeonActionPending>(e);}
bool UseTypedDungeonAction(entt::entity e,const item::PersistentItem& source,const item::ItemCatalog& catalog,TypedDungeonPlanner planner,bool targetRequired){
 namespace P=ecs::PlayerRuntime;
 const auto refuse=[&]{ecs::ChatSystem::Send(e,CHAT_TYPE_INFO,"Ez a tárgy most nem használható. Állj a megfelelő kazamataobjektum mellé.");return false;};
 if(!g_registry.valid(e)||!g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e))return false;
 const auto map=P::GetMapIndex(e);const auto dungeon=CDungeonManager::instance().FindByMapIndex(map);const auto lock=g_registry.valid(dungeon)?dungeon:e;
 const auto* quest=g_registry.try_get<ecs::QuestPCState>(e);auto* desc=P::GetDesc(e);
 const bool admitted=desc&&desc->IsPhase(PHASE_GAME)&&!CombatSystem::IsDead(e)&&!P::IsWarping(e)&&TypedGatheringAdmitted(e)&&!TypedGatheringActive(e)&&!g_registry.all_of<ecs::TypedItemBusy>(e)&&quest&&quest->pc&&!quest->pc->IsRunning();
 if(!admitted||TypedDungeonActionPending(lock))return refuse();
 entt::entity target=entt::null;std::optional<TypedDungeonAction> plan;
 if(!targetRequired)plan=planner(e,target);
 else if(auto* world=ecs::GetMap(map)){
  std::pair<double,std::uint32_t> nearest{501.,UINT32_MAX};
  auto find=[&](entt::entity npc){if(!ecs::IsCharacter(npc)||P::IsPC(npc)||CombatSystem::IsDead(npc))return;
   const auto distance=std::hypot(double(P::GetX(npc))-P::GetX(e),double(P::GetY(npc))-P::GetY(e));const auto order=std::pair{distance,P::GetPacketVID(npc)};
   if(distance<=500&&order<nearest)if(auto candidate=planner(e,npc)){nearest=order;target=npc;plan=std::move(candidate);}};world->for_each(find);
 }
 if(!plan||!plan->publish)return refuse();
 const auto admission=TypedRecoveryAdmission(e,{},source.count);item::DungeonUseFacts facts;
 facts.admitted=admitted;facts.stageReady=true;facts.character.level=admission.level;facts.character.characterClass=admission.classIndex;facts.character.sex=admission.sexIndex;facts.character.inCombat=admission.inCombat;facts.character.nowUtcMs=TypedItemEffectNow();
 const auto& state=g_registry.get<ecs::TypedItems>(e).state;std::array<std::uint8_t,16> op{};if(RAND_bytes(op.data(),op.size())!=1)return refuse();
 auto commit=item::MakeDungeonUseCommit(catalog,op,state.fence,state.storages,state.items,source.id,facts);if(!commit)return refuse();
 const auto fence=state.fence;const auto pid=P::GetPlayerID(e);const auto token=g_registry.get<ecs::TypedItemLogin>(e).token;
 auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permit||!permit->Current()||!g_registry.valid(e))return false;
 const auto* currentLogin=g_registry.try_get<ecs::TypedItemLogin>(e);const auto* currentItems=g_registry.try_get<ecs::TypedItems>(e);if(!currentLogin||currentLogin->token!=token||!currentItems||currentItems->state.fence!=fence)return false;
 auto guard=std::make_shared<DungeonActionGuard>();guard->lock=lock;g_registry.emplace<DungeonActionPending>(lock);
 auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
 g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
 const bool queued=item::runtime::RequestCommit(fence.owner,fence.sessionGeneration,std::move(*commit),[pid,token,map,target,targetRequired,plan=std::move(*plan),planner=std::move(planner),guard,held](item::runtime::CommitOutcome out)mutable{
  const auto ch=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* login=g_registry.valid(ch)?g_registry.try_get<ecs::TypedItemLogin>(ch):nullptr;if(!login||login->token!=token)return;
  if(out.state)g_registry.emplace_or_replace<ecs::TypedItems>(ch,std::move(*out.state));
  if(out.write&&!guard->completed){guard->completed=true;const bool targetNear=!targetRequired||(g_registry.valid(target)&&P::GetMapIndex(target)==map&&!CombatSystem::IsDead(target)&&std::hypot(double(P::GetX(target))-P::GetX(ch),double(P::GetY(target))-P::GetY(ch))<=500);
   auto current=held->Current()&&P::GetMapIndex(ch)==map&&targetNear?planner(ch,target):std::nullopt;
   if(current&&current->stamp==plan.stamp)plan.publish();else ecs::ChatSystem::Send(ch,CHAT_TYPE_INFO,"A kazamata állapota közben megváltozott.");
  }
  if(!out.write)ecs::ChatSystem::Send(ch,CHAT_TYPE_INFO,"A tárgy levonásának mentése nem sikerült.");
  if(g_registry.valid(ch)){g_registry.remove<ecs::TypedItemBusy>(ch);held.reset();SendTypedInventory(ch);PumpTypedItemGrants(ch);}
 });
 if(!queued){g_registry.remove<ecs::TypedItemBusy>(e);return refuse();}return true;
}
