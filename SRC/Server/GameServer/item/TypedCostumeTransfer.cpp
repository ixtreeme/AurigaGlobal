#include "stdafx.h"
#include "TypedCostumeTransfer.hpp"
#include "TypedInventorySync.hpp"
#include "TypedRecovery.hpp"
#include "TypedFishProcessing.hpp"
#include "TypedTools.hpp"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../quest/questpc.h"
#include <item/CostumeTransferWire.hpp>
#include <openssl/rand.h>
#include <chrono>
namespace {
namespace item=auriga::item;namespace store=item::runtime;namespace P=ecs::PlayerRuntime;using Status=item::CostumeTransferStatus;using Clock=std::chrono::steady_clock;
#pragma pack(push,1)
struct Request {std::uint8_t header,action,operation;std::uint64_t nonce,source,target;std::uint8_t selection;};
#pragma pack(pop)
static_assert(sizeof(Request)==28);
struct TypedCostumeTransferSession {
    std::uint64_t nonce=0,scroll=0,receiver=0,donor=0,token=0;
    entt::entity npc=entt::null;item::ItemOwnerFence fence;Clock::time_point deadline;
    std::array<std::optional<item::PersistentItem>,3> before;
    std::optional<item::CostumeTransferQuote> quote;
    std::array<std::uint8_t,16> operation{};
    bool pending=false,attempted=false;Status status=Status::Selecting;
};
bool Native(entt::entity e){return g_registry.valid(e) && g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e);}
void Tell(entt::entity e,const char* text){ecs::ChatSystem::Send(e,CHAT_TYPE_INFO,"%s",text);}
bool Near(entt::entity e,entt::entity npc){if(!g_registry.valid(npc) || P::IsPC(npc) || P::GetMapIndex(e)!=P::GetMapIndex(npc) || P::GetQuestNPC(e)!=npc)return false;const auto dx=std::abs(std::int64_t(P::GetX(e))-P::GetX(npc)),dy=std::abs(std::int64_t(P::GetY(e))-P::GetY(npc));return std::max(dx,dy)+std::min(dx,dy)/2<1000;}
bool Admitted(entt::entity e,bool opening=false){
    if(!Native(e) || !P::IsPC(e) || !P::GetDesc(e) || !P::GetDesc(e)->IsPhase(PHASE_GAME) || CombatSystem::IsDead(e) || P::IsWarping(e) || !InventorySystem::CanHandleItems(e) || TypedGatheringActive(e) || g_registry.all_of<ecs::TypedItemBusy>(e))return false;
    const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);if(flags && (flags->isObserverMode || flags->isStunned))return false;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    if(P::GetGMLevel(e)>GM_PLAYER && P::GetGMLevel(e)<GM_IMPLEMENTOR)return false;
#endif
    const auto* quest=g_registry.try_get<ecs::QuestPCState>(e);return quest && quest->pc && (opening || !quest->pc->IsRunning()) && (opening || TypedGatheringAdmitted(e));
}
item::CostumeTransferActor Actor(entt::entity e,entt::entity npc){item::CostumeTransferActor actor;const auto facts=TypedRecoveryAdmission(e,{},1);actor.equipment={facts.level,facts.classIndex,facts.sexIndex};actor.equipment.inCombat=facts.inCombat;actor.equipment.nowUtcMs=TypedItemEffectNow();actor.admitted=Admitted(e);actor.alive=!CombatSystem::IsDead(e);actor.nearNpc=Near(e,npc);return actor;}
const item::PersistentItem* Find(std::span<const item::PersistentItem> items,std::uint64_t id){const auto i=std::ranges::find(items,id,[](const auto& p){return p.id.Value();});return i==items.end() ? nullptr : &*i;}
void ReplyView(entt::entity e,item::CostumeTransferRuntimeView view){
    if(!Native(e))return;const auto c=store::CurrentCatalog();if(!c)return;const auto& state=g_registry.get<ecs::TypedItems>(e).state;
    auto bytes=item::EncodeCostumeTransferRuntime(*c,view,state.storages,state.items);if(!bytes)bytes=item::EncodeCostumeTransferRuntime(*c,{Status::Invalid,0,view.scroll,view.receiver,view.donor,nullptr},state.storages,state.items);
    SendTypedInventory(e);if(auto* d=P::GetDesc(e);bytes && d && d->IsPhase(PHASE_GAME))d->LargePacket(bytes->data(),static_cast<int>(bytes->size()));
}
void Reply(entt::entity e,Status status){const auto* s=g_registry.try_get<TypedCostumeTransferSession>(e);const bool preview=s && s->quote && (status==Status::Ready || status==Status::Pending || status==Status::Applied);ReplyView(e,{status,s ? s->nonce : 0,s ? s->scroll : 0,s ? s->receiver : 0,s ? s->donor : 0,preview ? &*s->quote : nullptr});}
}
bool OpenTypedCostumeTransfer(entt::entity e){
    if(!Native(e))return false;if(!Admitted(e,true) || !Near(e,P::GetQuestNPC(e))){Tell(e,"A kosztümbónusz átviteléhez állj az NPC mellé.");return true;}
    auto* old=g_registry.try_get<TypedCostumeTransferSession>(e);if(old && old->pending){Reply(e,Status::Pending);return true;}
    const auto c=store::CurrentCatalog();if(!c)return true;const auto& state=g_registry.get<ecs::TypedItems>(e).state;const item::PersistentItem* scroll=nullptr;
    for(const auto& p:state.items){const auto* def=c->Find(p.key);const auto* tool=def ? std::get_if<item::ItemBonusToolRow>(&def->category) : nullptr;if(tool && tool->operation==item::SpecialBonusOperation::CostumeTransfer && item::PlainInventoryItem(p,state.fence.owner) && (!scroll || p.id<scroll->id))scroll=&p;}
    if(!scroll){Tell(e,"Öltözékbónusz-átvivőre van szükséged. A leltárból nyisd meg az átvivőt.");return true;}
    TypedCostumeTransferSession s;s.scroll=scroll->id.Value();s.npc=P::GetQuestNPC(e);s.deadline=Clock::now()+std::chrono::minutes(2);s.token=g_registry.get<ecs::TypedItemLogin>(e).token;g_registry.emplace_or_replace<TypedCostumeTransferSession>(e,std::move(s));Reply(e,Status::Selecting);return true;
}
bool CloseTypedCostumeTransfer(entt::entity e){if(!Native(e))return false;const auto* s=g_registry.try_get<TypedCostumeTransferSession>(e);if(s && s->pending){Reply(e,Status::Pending);return true;}const auto scroll=s ? s->scroll : 0;g_registry.remove<TypedCostumeTransferSession>(e);ReplyView(e,{Status::Closed,0,scroll});return true;}
bool HandleTypedCostumeTransferPacket(entt::entity e,const char* data){
    const auto r=*reinterpret_cast<const Request*>(data);if(r.operation!=4)return false;if(!Native(e))return true;
    const auto refuse=[&](Status status){ReplyView(e,{status,0,r.source,r.target,r.action==1 ? r.nonce : 0,nullptr});};
    auto* s=g_registry.try_get<TypedCostumeTransferSession>(e);if(s && s->pending){if(r.source && r.source!=s->scroll)refuse(Status::Denied);else Reply(e,Status::Pending);return true;}if(r.action==3)return CloseTypedCostumeTransfer(e);
    if(r.action==2 && s && s->attempted && s->nonce==r.nonce && s->scroll==r.source && s->receiver==r.target && !r.selection){Reply(e,s->status);return true;}
    if(!Admitted(e)){refuse(Status::Denied);return true;}const auto c=store::CurrentCatalog();if(!c)return true;const auto& state=g_registry.get<ecs::TypedItems>(e).state;
    if(r.action==1){
        const auto a=item::ItemInstanceId::Create(r.source),b=item::ItemInstanceId::Create(r.target),d=item::ItemInstanceId::Create(r.nonce);if(!a || !b || !d || r.selection){refuse(Status::Invalid);return true;}
        TypedCostumeTransferSession next;next.scroll=r.source;next.receiver=r.target;next.donor=r.nonce;next.npc=P::GetQuestNPC(e);next.token=g_registry.get<ecs::TypedItemLogin>(e).token;next.fence=state.fence;next.deadline=Clock::now()+std::chrono::minutes(2);
        if(RAND_bytes(reinterpret_cast<unsigned char*>(&next.nonce),8)!=1 || !next.nonce || RAND_bytes(next.operation.data(),next.operation.size())!=1 || std::ranges::all_of(next.operation,[](auto v){return v==0;})){refuse(Status::Failed);return true;}
        const auto actor=Actor(e,next.npc);const auto q=item::PrepareCostumeTransfer(*c,state.fence.owner,state.items,*a,*b,*d,actor);next.status=q ? Status::Ready : !actor.nearNpc ? Status::Denied : Status::Invalid;
        if(q){next.quote=*q;for(unsigned i=0;i<3;++i){const auto* value=Find(state.items,std::array{next.scroll,next.receiver,next.donor}[i]);if(value)next.before[i]=*value;}}
        g_registry.emplace_or_replace<TypedCostumeTransferSession>(e,std::move(next));Reply(e,g_registry.get<TypedCostumeTransferSession>(e).status);return true;
    }
    if(r.action!=2 || !s || !s->quote || !r.nonce || s->nonce!=r.nonce || s->scroll!=r.source || s->receiver!=r.target || r.selection || s->token!=g_registry.get<ecs::TypedItemLogin>(e).token || s->fence.sessionGeneration!=state.fence.sessionGeneration || Clock::now()>s->deadline || !Near(e,s->npc)){refuse(Status::Stale);return true;}
    for(unsigned i=0;i<3;++i){const auto* now=Find(state.items,std::array{s->scroll,s->receiver,s->donor}[i]);if(!s->before[i] || !now || *now!=*s->before[i]){s->status=Status::Stale;Reply(e,s->status);return true;}}
    const auto a=item::ItemInstanceId::Create(s->scroll),b=item::ItemInstanceId::Create(s->receiver),d=item::ItemInstanceId::Create(s->donor);const auto planned=item::MakeCostumeTransferCommit(*c,s->operation,state.fence,state.storages,state.items,*a,*b,*d,Actor(e,s->npc));if(!planned){s->status=Status::Invalid;Reply(e,s->status);return true;}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permit || !permit->Current()){Reply(e,Status::Denied);return true;}
    const auto pid=P::GetPlayerID(e);const auto nonce=s->nonce;const auto token=s->token;auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));s->attempted=s->pending=true;s->status=Status::Pending;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    const bool queued=store::RequestCommit(state.fence.owner,state.fence.sessionGeneration,*planned,[pid,token,nonce,held](store::CommitOutcome out){const auto ch=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* login=g_registry.valid(ch) ? g_registry.try_get<ecs::TypedItemLogin>(ch) : nullptr;if(!login || login->token!=token)return;g_registry.remove<ecs::TypedItemBusy>(ch);if(out.state)g_registry.emplace_or_replace<ecs::TypedItems>(ch,std::move(*out.state));if(auto* current=g_registry.try_get<TypedCostumeTransferSession>(ch);current && current->nonce==nonce){current->pending=false;current->status=out.write ? Status::Applied : Status::Failed;Reply(ch,current->status);}else SendTypedInventory(ch);PumpTypedItemGrants(ch);});
    if(!queued){g_registry.remove<ecs::TypedItemBusy>(e);s=g_registry.try_get<TypedCostumeTransferSession>(e);if(s){s->pending=false;s->status=Status::Failed;}Reply(e,Status::Failed);}return true;
}
