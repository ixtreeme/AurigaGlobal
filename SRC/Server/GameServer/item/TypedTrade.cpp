#include "stdafx.h"
#include "TypedTrade.hpp"
#include "TypedRecovery.hpp"
#include "TypedAccessoryClock.hpp"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include "TypedDrops.hpp"
#include "TypedInventorySync.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/components/character_runtime_components.hpp"
#include "../ecs/components/social_components.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../quest/questpc.h"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include "../core/config.h"
#include "../core/utils.h"
#include <item/PlayerTrades.hpp>
#include <Core/Logging.hpp>
#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <random>

namespace
{
namespace items=auriga::item;
namespace store=items::runtime;
using Status=items::PlayerTradeStatus;
using Phase=items::PlayerTradePhase;
struct Ref { std::uint64_t token=0; };
struct RequestState
{
    std::uint64_t sequence=0,revision=0,subject=0;
    std::uint32_t count=0;std::uint8_t action=0;
    Status result=Status::Updated;
};
struct Session
{
    items::PlayerTradeView view;
    std::array<entt::entity,2> entities;
    std::array<std::uint64_t,2> logins;
    std::array<items::persistence::LoadedItemOwner,2> states;
    std::array<RequestState,2> requests;
    std::optional<auriga::resource_admission::Permit> guard;
    std::int64_t expires=0;
    bool joined=false;
    std::array<bool,2> busyOwned{};
};
std::map<std::uint64_t,std::shared_ptr<Session>> sessions;
std::int64_t Now()
{return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
std::array<std::uint8_t,16> Operation()
{
    static std::mt19937_64 rng{std::random_device{}()};std::array<std::uint8_t,16> out{};
    for(auto& b:out)b=static_cast<std::uint8_t>(rng());out[0]|=1;return out;
}
std::shared_ptr<Session> Find(entt::entity e)
{
    const auto* ref=g_registry.valid(e) ? g_registry.try_get<Ref>(e) : nullptr;
    const auto found=ref ? sessions.find(ref->token) : sessions.end();return found==sessions.end() ? nullptr : found->second;
}
int Side(const Session& s,entt::entity e)
{return s.entities[0]==e ? 0 : s.entities[1]==e ? 1 : -1;}
bool Current(const Session& s,int side)
{
    const auto e=s.entities[side];const auto* login=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;
    return login && login->token==s.logins[side] && ecs::PlayerRuntime::GetPlayerID(e)==s.view.actors[side].pid;
}
bool Admitted(entt::entity e)
{
    if(!g_registry.valid(e) || !ecs::PlayerRuntime::IsPC(e) || CombatSystem::IsDead(e) || !InventorySystem::CanHandleItems(e) ||
       ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e) || ExchangeSystem::GetSession(e)!=entt::null ||
       ecs::SocialSystem::GetMyShop(e)!=entt::null || ecs::SocialSystem::GetShopOwner(e)!=entt::null ||
       !g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e))return false;
    const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);
    const auto* quest=g_registry.try_get<ecs::QuestPCState>(e);
    auto* desc=ecs::PlayerRuntime::GetDesc(e);
    if(!desc || !desc->IsPhase(PHASE_GAME) || (flags && (flags->isStunned || flags->isObserverMode)) || !quest || !quest->pc || quest->pc->IsRunning())return false;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    if(ecs::PlayerRuntime::GetGMLevel(e)>GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(e)<GM_IMPLEMENTOR)return false;
#endif
#ifdef ENABLE_PVP_ADVANCED
    if(ecs::PlayerRuntime::GetDuelOption(e,"BlockExchange"))return false;
#endif
    const auto* timing=g_registry.try_get<ecs::WarpBlockState>(e);
    return !timing || std::int64_t(thecore_pulse())-timing->safeboxLoadTime>=PASSES_PER_SEC(g_nPortalLimitTime);
}
bool Near(const Session& s)
{
    if(!Current(s,0) || !Current(s,1))return false;
    const auto a=s.entities[0],b=s.entities[1];
    if(ecs::PlayerRuntime::GetMapIndex(a)!=ecs::PlayerRuntime::GetMapIndex(b))return false;
    const auto dx=std::abs(std::int64_t(ecs::PlayerRuntime::GetX(a))-ecs::PlayerRuntime::GetX(b));
    const auto dy=std::abs(std::int64_t(ecs::PlayerRuntime::GetY(a))-ecs::PlayerRuntime::GetY(b));
    return std::max(dx,dy)+std::min(dx,dy)/2<EXCHANGE_MAX_DISTANCE;
}
bool RebasePassive(const std::shared_ptr<Session>& s)
{
    if(!s->joined)return true;
    for(int side=0;side<2;++side)
    {
        if(!Current(*s,side))return false;
        const auto& now=g_registry.get<ecs::TypedItems>(s->entities[side]).state;
        if(now.fence==s->states[side].fence)continue;
        if(!OnlyTypedAccessoryClockChanges(s->states[side].fence,now.fence))return false;
        s->states[side]=now;s->view.actors[side].offer.owner=now.fence;
    }
    return true;
}
void Terminal(entt::entity e,Status status,std::uint64_t request=0)
{
    if(!g_registry.valid(e))return;const auto catalog=store::CurrentCatalog();auto* desc=ecs::PlayerRuntime::GetDesc(e);
    if(!catalog || !desc || !desc->IsPhase(PHASE_GAME))return;
    const auto wire=items::EncodePlayerTrade(*catalog,nullptr,0,request,status,{},{});
    SendTypedInventory(e);if(wire)desc->LargePacket(wire->data(),static_cast<int>(wire->size()));
}
void Reply(const std::shared_ptr<Session>& s,Status status,int only=-1)
{
    if(!sessions.contains(s->view.token))return;const auto catalog=store::CurrentCatalog();if(!catalog)return;
    auto layouts=s->states[0].storages;layouts.insert(layouts.end(),s->states[1].storages.begin(),s->states[1].storages.end());
    auto allItems=s->states[0].items;allItems.insert(allItems.end(),s->states[1].items.begin(),s->states[1].items.end());
    for(int side=0;side<2;++side)if((only<0 || only==side) && Current(*s,side))
    {
        const auto e=s->entities[side];auto* desc=ecs::PlayerRuntime::GetDesc(e);if(!desc || !desc->IsPhase(PHASE_GAME))continue;
        const auto wire=items::EncodePlayerTrade(*catalog,&s->view,side,s->requests[side].sequence,status,layouts,allItems);
        SendTypedInventory(e);if(wire)desc->LargePacket(wire->data(),static_cast<int>(wire->size()));
        else LOG_ERROR("TypedTrade: token {} response could not be encoded",s->view.token);
    }
}
void Close(const std::shared_ptr<Session>& s,Status status,entt::entity destroying=entt::null)
{
    if(!sessions.erase(s->view.token))return;
    for(int side=0;side<2;++side)if(s->entities[side]!=destroying && Current(*s,side))Terminal(s->entities[side],status,s->requests[side].sequence);
    for(int side=0;side<2;++side)
    {
        const auto e=s->entities[side];if(e==destroying || !Current(*s,side))continue;
        const auto* ref=g_registry.try_get<Ref>(e);
        if(ref && ref->token==s->view.token)
        {
            g_registry.remove<Ref>(e);g_registry.remove<ecs::TypedTradeActive>(e);
            if(s->busyOwned[side])
            {g_registry.get_or_emplace<ecs::WarpBlockState>(e).exchangeTime=thecore_pulse();if(!g_registry.any_of<TypedRecoveryBusy>(e))g_registry.remove<ecs::TypedItemBusy>(e);}
        }
    }
    s->guard.reset();
    for(int side=0;side<2;++side)if(s->entities[side]!=destroying && Current(*s,side))PumpTypedItemGrants(s->entities[side]);
}
void Destroyed(entt::registry&,entt::entity e)
{
    const auto s=Find(e);if(s && s->view.phase!=Phase::Committing)Close(s,Status::Cancelled,e);
}
void ResetApprovals(Session& s)
{++s.view.revision;s.view.readyMask=s.view.confirmedMask=0;s.view.phase=Phase::Editing;}
Status Error(items::PlayerTradeError e)
{
    switch(e){case items::PlayerTradeError::Denied:return Status::Denied;case items::PlayerTradeError::NoMoney:return Status::NoMoney;
    case items::PlayerTradeError::NoRoom:return Status::NoRoom;case items::PlayerTradeError::WalletFull:return Status::WalletFull;
    case items::PlayerTradeError::Unavailable:return Status::Failed;default:return Status::Invalid;}
}
void Commit(const std::shared_ptr<Session>& s)
{
    const auto catalog=store::CurrentCatalog();if(!catalog || !s->guard || !s->guard->Current()){Close(s,Status::Failed);return;}
    auto layouts=s->states[0].storages;layouts.insert(layouts.end(),s->states[1].storages.begin(),s->states[1].storages.end());
    auto allItems=s->states[0].items;allItems.insert(allItems.end(),s->states[1].items.begin(),s->states[1].items.end());
    auto wallets=s->states[0].wallets;wallets.insert(wallets.end(),s->states[1].wallets.begin(),s->states[1].wallets.end());
    const std::array<items::PlayerTradeOffer,2> offers{s->view.actors[0].offer,s->view.actors[1].offer};
    auto commit=items::MakePlayerTradeCommit(*catalog,Operation(),offers,layouts,allItems,wallets,store::PeekInstanceIds(items::MaxItemCommitItems));
    if(!commit){ResetApprovals(*s);Reply(s,Error(commit.error()));return;}
    store::SpendInstanceIds(std::ranges::count_if(commit->items,[](const auto& change){return !change.before;}));
    s->view.phase=Phase::Committing;Reply(s,Status::Pending);
    const bool queued=store::RequestCommitMany({{offers[0].owner.owner,0},{offers[1].owner.owner,0}},std::move(*commit),[s](store::MultiCommitOutcome outcome) {
        for(auto& state:outcome.states)if(state)
        {
            const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(static_cast<std::uint32_t>(state->fence.owner.id));
            auto* current=e!=entt::null && g_registry.valid(e) ? g_registry.try_get<ecs::TypedItems>(e) : nullptr;
            if(current && current->state.fence.sessionGeneration==state->fence.sessionGeneration && current->state.fence.revision<=state->fence.revision)
            {g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*state));SendTypedInventory(e);}
            else if(!current)NotifyRemoteTypedItemOwner(*state);
        }
        Close(s,outcome.write ? Status::Completed : Status::Failed);
    });
    if(!queued)Close(s,Status::Failed);
}
}

bool TryStartTypedTrade(entt::entity first,entt::entity second)
{
    const bool a=g_registry.valid(first) && g_registry.all_of<ecs::TypedItems>(first);
    const bool b=g_registry.valid(second) && g_registry.all_of<ecs::TypedItems>(second);
    if(!a && !b)return false;
    const auto denied=[&]{if(const auto existing=Find(first))Reply(existing,Status::Denied,Side(*existing,first));else Terminal(first,Status::Denied);return true;};
    if(!a || !b || first==second || !Admitted(first) || !Admitted(second) || Find(first) || Find(second) ||
       g_registry.all_of<ecs::TypedItemBusy>(first) || g_registry.all_of<ecs::TypedItemBusy>(second) || ecs::PlayerRuntime::IsBlockMode(second,BLOCK_EXCHANGE))return denied();
    static std::map<std::uint32_t,std::int64_t> cooldown;
    const auto pid=ecs::PlayerRuntime::GetPlayerID(first);if(cooldown[pid]>Now())return denied();cooldown[pid]=Now()+1000;
    auto permission=ecs::resource_admission::Enter({first,second},auriga::resource_admission::Operation::Exchange);
    if(!permission || !permission->Current() || !Admitted(first) || !Admitted(second) || Find(first) || Find(second) ||
       g_registry.all_of<ecs::TypedItemBusy>(first) || g_registry.all_of<ecs::TypedItemBusy>(second))return denied();
    struct Hooks{};if(!g_registry.ctx().contains<Hooks>()){g_registry.on_destroy<Ref>().connect<&Destroyed>();g_registry.ctx().emplace<Hooks>();}
    auto s=std::make_shared<Session>();s->entities={first,second};s->expires=Now()+30000;s->view.revision=1;
    do{const auto op=Operation();s->view.token=0;for(unsigned i=0;i<8;++i)s->view.token|=std::uint64_t{op[i]}<<(8*i);}while(sessions.contains(s->view.token));
    for(int side=0;side<2;++side)
    {
        const auto e=s->entities[side];s->logins[side]=g_registry.get<ecs::TypedItemLogin>(e).token;s->states[side]=g_registry.get<ecs::TypedItems>(e).state;
        auto& actor=s->view.actors[side];actor.vid=ecs::PlayerRuntime::GetPacketVID(e);actor.pid=ecs::PlayerRuntime::GetPlayerID(e);actor.name=ecs::PlayerRuntime::GetName(e);actor.offer.owner=s->states[side].fence;
    }
    if(!Near(*s))return denied();
    sessions.emplace(s->view.token,s);
    for(auto e:s->entities)
    {
        if(!g_registry.valid(e)){Close(s,Status::Cancelled);return true;}g_registry.emplace_or_replace<Ref>(e,s->view.token);
        if(!permission->Current() || !sessions.contains(s->view.token)){Close(s,Status::Cancelled);return true;}
    }
    Reply(s,Status::Invited);return true;
}
void CancelTypedTrade(entt::entity e)
{if(const auto s=Find(e)){if(s->view.phase==Phase::Committing)Reply(s,Status::Pending,Side(*s,e));else Close(s,Status::Cancelled);}}
bool TypedTradeAllowsRecoveryTick(entt::entity e){const auto s=Find(e);return s && s->joined && s->view.phase!=Phase::Committing;}
void TickTypedTrades()
{
    std::vector<std::shared_ptr<Session>> current;for(const auto& [token,s]:sessions)current.push_back(s);
    for(const auto& s:current)
    {
        if(s->view.phase==Phase::Committing)continue;
        bool valid=s->expires>Now() && Near(*s) && Admitted(s->entities[0]) && Admitted(s->entities[1]) && (!s->guard || s->guard->Current());
        if(valid && s->joined)valid=RebasePassive(s) && !g_registry.any_of<ecs::TypedItemRefresh>(s->entities[0]) && !g_registry.any_of<ecs::TypedItemRefresh>(s->entities[1]);
        if(!valid)Close(s,Status::Cancelled);
    }
}
bool HandleTypedTradePacket(entt::entity e,const char* data)
{
    static_assert(sizeof(TPacketCGTypedTrade)==42);const auto& r=*reinterpret_cast<const TPacketCGTypedTrade*>(data);
    if(!g_registry.valid(e))return true;
    if(r.action==1)
    {if(!r.session && !r.request && !r.revision && !r.subject && !r.count && !TryStartTypedTrade(e,CHARACTER_MANAGER::instance().FindEntity(r.peer)))Terminal(e,Status::Denied);return true;}
    const auto s=Find(e);const int side=s ? Side(*s,e) : -1;
    if(!s || side<0 || !Current(*s,side)){Terminal(e,Status::Stale,r.request);return true;}
    if(s->view.token!=r.session || s->view.actors[1-side].vid!=r.peer){Reply(s,Status::Stale,side);return true;}
    if(r.action==8){CancelTypedTrade(e);return true;}
    if(!Near(*s) || !Admitted(s->entities[0]) || !Admitted(s->entities[1])){if(s->view.phase!=Phase::Committing)Close(s,Status::Cancelled);return true;}
    if(s->view.phase==Phase::Committing){Reply(s,Status::Pending,side);return true;}
    if(g_registry.any_of<TypedRecoveryBusy>(s->entities[0]) || g_registry.any_of<TypedRecoveryBusy>(s->entities[1])){Reply(s,Status::Stale,side);return true;}
    if(!RebasePassive(s)){Close(s,Status::Cancelled);return true;}
    auto& last=s->requests[side];
    if(!r.request || r.request!=last.sequence+1)
    {
        const bool same=r.request==last.sequence && r.action==last.action && r.subject==last.subject && r.count==last.count && r.revision==last.revision;
        Reply(s,same ? last.result : Status::Stale,side);return true;
    }
    last={r.request,r.revision,r.subject,r.count,r.action,Status::Invalid};
    if(r.revision!=s->view.revision){last.result=Status::Stale;Reply(s,last.result,side);return true;}
    if(s->view.phase==Phase::Invited)
    {
        if(r.action!=2 || side!=1 || g_registry.all_of<ecs::TypedItemBusy>(s->entities[0]) || g_registry.all_of<ecs::TypedItemBusy>(s->entities[1])){Close(s,Status::Denied);return true;}
        s->guard=ecs::resource_admission::Enter({s->entities[0],s->entities[1]},auriga::resource_admission::Operation::Exchange);
        if(!s->guard || !s->guard->Current() || !Near(*s) || !Admitted(s->entities[0]) || !Admitted(s->entities[1])){Close(s,Status::Denied);return true;}
        s->joined=true;
        for(int i=0;i<2;++i)
        {
            if(!sessions.contains(s->view.token) || !Current(*s,i) || !s->guard || !s->guard->Current()){Close(s,Status::Cancelled);return true;}
            if(g_registry.all_of<ecs::TypedItemBusy>(s->entities[i])){Close(s,Status::Denied);return true;}
            s->states[i]=g_registry.get<ecs::TypedItems>(s->entities[i]).state;s->view.actors[i].offer.owner=s->states[i].fence;
            s->busyOwned[i]=true;g_registry.emplace_or_replace<ecs::TypedItemBusy>(s->entities[i]);
            if(!sessions.contains(s->view.token) || !Current(*s,i)){Close(s,Status::Cancelled);return true;}
            g_registry.emplace_or_replace<ecs::TypedTradeActive>(s->entities[i]);
        }
        if(!sessions.contains(s->view.token) || !Near(*s) || !s->guard || !s->guard->Current()){Close(s,Status::Cancelled);return true;}
        s->expires=Now()+300000;ResetApprovals(*s);last.result=Status::Updated;Reply(s,last.result);return true;
    }
    if(!s->guard || !s->guard->Current()){Close(s,Status::Denied);return true;}
    auto& offer=s->view.actors[side].offer;bool changed=false;
    if(r.action==3)
    {
        const auto id=items::ItemInstanceId::Create(r.subject);const auto& carried=s->states[side].items;
        const auto found=id ? std::ranges::find(carried,*id,&items::PersistentItem::id) : carried.end();
        if(found!=carried.end() && !found->destroyed && found->placement && found->placement->kind==items::ItemStorageKind::Inventory)
        {
            const auto catalog=store::CurrentCatalog();const auto* d=catalog ? catalog->Find(found->key) : nullptr;const auto* policy=d ? catalog->FindTradePolicy(d->base.tradePolicy) : nullptr;
            const auto count=r.count ? r.count : found->count;
            if(found->boundTo || !policy || !policy->allowTrade)last.result=Status::Denied;
            else if(count && count<=found->count)
            {
                auto current=std::ranges::find(offer.items,*id,&items::TradeSelection::id);
                if(current!=offer.items.end()){changed=current->count!=count;current->count=count;}
                else if(offer.items.size()<items::MaxPlayerTradeItems){offer.items.push_back({*id,count});changed=true;}
            }
        }
    }
    else if(r.action==4)
    {const auto before=offer.items.size();std::erase_if(offer.items,[&](const auto& item){return item.id.Value()==r.subject;});changed=before!=offer.items.size();}
    else if(r.action==5)
    {
        const auto& wallets=s->states[side].wallets;const auto wallet=std::ranges::find(wallets,items::WalletCurrency::Gold,&items::ItemWallet::currency);
        if(wallet==wallets.end() || r.subject>wallet->balance)last.result=Status::NoMoney;
        else{changed=offer.gold!=r.subject;offer.gold=r.subject;}
    }
    else if(r.action==9){changed=true;}
    else if(r.action==6)
    {
        if(!s->view.actors[0].offer.items.empty() || !s->view.actors[1].offer.items.empty() || s->view.actors[0].offer.gold || s->view.actors[1].offer.gold)
        {s->view.readyMask|=1u<<side;if(s->view.readyMask==3)s->view.phase=Phase::Confirming;last.result=Status::Ready;Reply(s,last.result);return true;}
    }
    else if(r.action==7 && s->view.phase==Phase::Confirming && s->view.readyMask==3)
    {
        s->view.confirmedMask|=1u<<side;last.result=Status::Ready;
        if(s->view.confirmedMask==3)Commit(s);else Reply(s,last.result);return true;
    }
    if(changed){ResetApprovals(*s);last.result=Status::Updated;}
    Reply(s,last.result,changed ? -1 : side);return true;
}
