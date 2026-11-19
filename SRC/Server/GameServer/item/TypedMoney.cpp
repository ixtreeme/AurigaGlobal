#include "stdafx.h"
#include "TypedMoney.hpp"
#include "TypedLoot.hpp"
#include "TypedDrops.hpp"
#include "TypedInventorySync.hpp"
#include "TypedAccessoryClock.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../quest/questpc.h"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include <SQL/AsyncSQL.h>
#include <Core/Logging.hpp>
#include <openssl/rand.h>
#include <item/TypedGround.hpp>
#include <item/NativeMoneyWire.hpp>
#include <charconv>
#include <chrono>
namespace {
namespace item=auriga::item;namespace store=item::runtime;
bool currencyRecoveryPending=false;
template<class T> bool ReadCurrencyField(const char* text,T& value){if(!text)return false;const std::string_view s(text);const auto parsed=std::from_chars(s.data(),s.data()+s.size(),value);return parsed.ec==std::errc{} && parsed.ptr==s.data()+s.size();}
std::uint64_t Utc(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
std::array<std::uint8_t,16> Operation(){std::array<std::uint8_t,16> op{};if(RAND_bytes(op.data(),op.size())!=1)return {};return op;}
entt::entity Current(std::uint32_t pid,std::uint64_t token){auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* l=g_registry.valid(e)?g_registry.try_get<ecs::TypedItemLogin>(e):nullptr;return l && l->token==token?e:entt::null;}
bool Admitted(entt::entity e){
    if(!g_registry.valid(e) || !ecs::PlayerRuntime::IsPC(e) || CombatSystem::IsDead(e) || !InventorySystem::CanHandleItems(e) ||
       ecs::SocialSystem::HasExchange(e) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e) ||
       ecs::SocialSystem::GetMyShop(e)!=entt::null || ecs::SocialSystem::GetShopOwner(e)!=entt::null || !g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e))return false;
    const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);const auto* q=g_registry.try_get<ecs::QuestPCState>(e);auto* d=ecs::PlayerRuntime::GetDesc(e);
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    if(ecs::PlayerRuntime::GetGMLevel(e)>GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(e)<GM_IMPLEMENTOR)return false;
#endif
    return d && d->IsPhase(PHASE_GAME) && q && q->pc && !q->pc->IsRunning() && !(flags && (flags->isStunned || flags->isObserverMode));
}
const item::ItemWallet* Gold(const item::persistence::LoadedItemOwner& s){for(const auto& w:s.wallets)if(w.owner==s.fence.owner && w.currency==item::WalletCurrency::Gold)return &w;return nullptr;}
enum Status:std::uint8_t {Ready,Applied,Closed,Pending,Invalid,Denied,Stale,Failed};
struct TypedMoneySession {std::uint64_t nonce=0,amount=0,expires=0;item::ItemOwnerFence fence{};std::array<std::uint8_t,16> operation{};Status status=Ready;bool attempted=false,pending=false;};
void Reply(entt::entity e,Status status){
    auto* d=g_registry.valid(e)?ecs::PlayerRuntime::GetDesc(e):nullptr;if(!d || !d->IsPhase(PHASE_GAME))return;
    const auto* s=g_registry.try_get<TypedMoneySession>(e);const auto* state=g_registry.try_get<ecs::TypedItems>(e);const auto* w=state?Gold(state->state):nullptr;
    const auto p=item::EncodeNativeMoney({static_cast<item::NativeMoneyStatus>(status),s?s->nonce:0,s?s->amount:0,w?w->balance:0,state?state->state.fence.revision:0,GOLD_MAX});
    SendTypedInventory(e);d=g_registry.valid(e)?ecs::PlayerRuntime::GetDesc(e):nullptr;if(p && d && d->IsPhase(PHASE_GAME))d->LargePacket(p->data(),static_cast<int>(p->size()));
}
}
void QueueTypedGold(entt::entity e,std::uint64_t amount){
    const auto c=store::CurrentCatalog();const auto key=c?c->CurrencyKey(item::CurrencyTokenMode::GroundGold):std::nullopt;
    if(!key || !amount || amount>INT64_MAX)return;item::ItemGrant grant{*key,0,1};grant.currencyAmount=amount;QueueTypedItemGrant(e,grant);
}
std::expected<void,TypedGrantRefusal> RequestTypedGoldCredit(entt::entity e,std::uint64_t amount,std::optional<item::CurrencyDropIntent> claim,std::function<void(bool)> done){
    auto* typed=g_registry.valid(e)?g_registry.try_get<ecs::TypedItems>(e):nullptr;const auto* login=g_registry.valid(e)?g_registry.try_get<ecs::TypedItemLogin>(e):nullptr;const auto c=store::CurrentCatalog();
    const auto key=c?c->CurrencyKey(item::CurrencyTokenMode::GroundGold):std::nullopt;if(!typed || !login || !key)return std::unexpected(TypedGrantRefusal::NotEnabled);
    if(g_registry.all_of<ecs::TypedItemBusy>(e))return std::unexpected(TypedGrantRefusal::Busy);const auto* wallet=Gold(typed->state);if(!wallet)return std::unexpected(TypedGrantRefusal::Invalid);
    auto commit=item::MakeGroundCurrencyCredit(*c,Operation(),typed->state.fence,*wallet,*key,amount,GOLD_MAX);
    if(!commit)return std::unexpected(commit.error()==item::MoneyError::WalletFull?TypedGrantRefusal::NoRoom:TypedGrantRefusal::Invalid);
    if(claim){claim->create=false;claim->nowAtMs=Utc();commit->currencyDrop=std::move(claim);if(!item::ValidateItemCommit(*c,*commit))return std::unexpected(TypedGrantRefusal::Invalid);}
    const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=login->token;const auto fence=typed->state.fence;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    if(!store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(*commit),[pid,token,done=std::move(done)](store::CommitOutcome result)mutable{
        const auto e=Current(pid,token);if(e!=entt::null){if(result.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*result.state));if(Current(pid,token)==e)g_registry.remove<ecs::TypedItemBusy>(e);if(Current(pid,token)==e)SendTypedInventory(e);}
        if(done)done(result.write.has_value());if(e!=entt::null)PumpTypedItemGrants(e);
    })){g_registry.remove<ecs::TypedItemBusy>(e);return std::unexpected(TypedGrantRefusal::NotEnabled);}return {};
}
bool RequestTypedQuestGoldChange(entt::entity e,std::int64_t delta,std::function<void(bool)> done){
    const auto c=store::CurrentCatalog();const auto* typed=g_registry.valid(e)?g_registry.try_get<ecs::TypedItems>(e):nullptr;const auto* login=g_registry.valid(e)?g_registry.try_get<ecs::TypedItemLogin>(e):nullptr;
    const auto* q=g_registry.valid(e)?g_registry.try_get<ecs::QuestPCState>(e):nullptr;auto* d=g_registry.valid(e)?ecs::PlayerRuntime::GetDesc(e):nullptr;
    if(!c||!typed||!login||!q||!q->pc||!q->pc->IsRunning()||!d||!d->IsPhase(PHASE_GAME)||g_registry.all_of<ecs::TypedItemBusy>(e))return false;
    const auto* wallet=Gold(typed->state);if(!wallet)return false;
    auto commit=item::MakeQuestGoldChange(*c,Operation(),typed->state.fence,*wallet,delta,GOLD_MAX);if(!commit)return false;
    const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=login->token;const auto fence=typed->state.fence;
    auto permission=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permission||!permission->Current()||Current(pid,token)!=e)return false;
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permission));g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    if(!store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(*commit),[pid,token,done=std::move(done),held](store::CommitOutcome result)mutable{
        auto e=Current(pid,token);if(e==entt::null)return;if(result.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*result.state));if(Current(pid,token)!=e)return;
        g_registry.remove<ecs::TypedItemBusy>(e);held.reset();SendTypedInventory(e);if(Current(pid,token)!=e)return;
        if(done)done(result.write.has_value());if(Current(pid,token)==e)PumpTypedItemGrants(e);
    })){if(Current(pid,token)==e)g_registry.remove<ecs::TypedItemBusy>(e);return false;}return true;
}
bool RequestTypedQuestGayaChange(entt::entity e,std::int64_t delta,std::function<void(bool)> done){
    const auto c=store::CurrentCatalog();const auto* typed=g_registry.valid(e)?g_registry.try_get<ecs::TypedItems>(e):nullptr;const auto* login=g_registry.valid(e)?g_registry.try_get<ecs::TypedItemLogin>(e):nullptr;
    const auto* q=g_registry.valid(e)?g_registry.try_get<ecs::QuestPCState>(e):nullptr;auto* d=g_registry.valid(e)?ecs::PlayerRuntime::GetDesc(e):nullptr;
    if(!c||!typed||!login||!q||!q->pc||!q->pc->IsRunning()||!d||!d->IsPhase(PHASE_GAME)||g_registry.all_of<ecs::TypedItemBusy>(e))return false;
    const auto found=std::ranges::find(typed->state.wallets,item::WalletCurrency::Gaya,&item::ItemWallet::currency);if(found==typed->state.wallets.end())return false;const auto* wallet=&*found;
    auto commit=item::MakeQuestGayaChange(*c,Operation(),typed->state.fence,*wallet,delta,GAYA_MAX-1);if(!commit)return false;
    const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=login->token;const auto fence=typed->state.fence;
    auto permission=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permission||!permission->Current()||Current(pid,token)!=e)return false;
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permission));g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    if(!store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(*commit),[pid,token,done=std::move(done),held](store::CommitOutcome result)mutable{
        auto e=Current(pid,token);if(e==entt::null)return;if(result.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*result.state));if(Current(pid,token)!=e)return;
        g_registry.remove<ecs::TypedItemBusy>(e);held.reset();SendTypedInventory(e);if(Current(pid,token)!=e)return;
        if(done)done(result.write.has_value());if(Current(pid,token)==e)PumpTypedItemGrants(e);
    })){if(Current(pid,token)==e)g_registry.remove<ecs::TypedItemBusy>(e);return false;}return true;
}
bool HandleTypedMoneyPacket(entt::entity e,const char* data){
    const auto decoded=item::DecodeNativeMoneyRequest(std::as_bytes(std::span(data,item::NativeMoneyRequestBodySize)));if(!decoded)return false;
    const auto r=*decoded;if(!g_registry.valid(e))return true;auto* s=g_registry.try_get<TypedMoneySession>(e);
    if(s && s->pending){Reply(e,Pending);return true;}
    if(r.action==3){if(!s || r.nonce!=s->nonce || r.amount){Reply(e,Invalid);return true;}g_registry.remove<TypedMoneySession>(e);Reply(e,Closed);return true;}
    if(r.action==1){
        if(r.nonce || !Admitted(e) || g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Denied);return true;}
        const auto& state=g_registry.get<ecs::TypedItems>(e).state;const auto* w=Gold(state);const auto c=store::CurrentCatalog();
        if(!w || !r.amount || r.amount>w->balance || r.amount>GOLD_MAX || !c || !c->CurrencyKey(item::CurrencyTokenMode::GroundGold) || !store::ItemStore::Instance().GroundRun()){Reply(e,Invalid);return true;}
        TypedMoneySession next;next.amount=r.amount;next.fence=state.fence;next.operation=Operation();next.expires=Utc()+30000;
        if(RAND_bytes(reinterpret_cast<unsigned char*>(&next.nonce),8)!=1 || !next.nonce || std::ranges::all_of(next.operation,[](auto b){return !b;})){Reply(e,Failed);return true;}
        g_registry.emplace_or_replace<TypedMoneySession>(e,next);Reply(e,Ready);return true;
    }
    if(r.action!=2 || !s || r.nonce!=s->nonce || r.amount!=s->amount){Reply(e,Stale);return true;}if(s->attempted){Reply(e,s->status);return true;}s->attempted=true;
    if(!Admitted(e) || g_registry.all_of<ecs::TypedItemBusy>(e)){s->status=Denied;Reply(e,Denied);return true;}
    auto permission=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::GroundDrop);
    s=g_registry.valid(e)?g_registry.try_get<TypedMoneySession>(e):nullptr;
    if(!s || s->nonce!=r.nonce)return true;
    if(!permission || !permission->Current() || !Admitted(e) || g_registry.all_of<ecs::TypedItemBusy>(e)){s->status=Denied;Reply(e,Denied);return true;}
    const auto& state=g_registry.get<ecs::TypedItems>(e).state;const auto* w=Gold(state);const auto c=store::CurrentCatalog();const auto run=store::ItemStore::Instance().GroundRun();const auto now=Utc();
    if(state.fence!=s->fence && OnlyTypedAccessoryClockChanges(s->fence,state.fence))s->fence=state.fence;
    if(!w || !c || !run || state.fence!=s->fence || now>=s->expires){s->status=Stale;Reply(e,Stale);return true;}
    item::CurrencyDropIntent intent{s->operation,run->core,run->run,ecs::PlayerRuntime::GetPlayerID(e),s->amount,now,now+item::GroundLifetimeMs,now,static_cast<std::int32_t>(ecs::PlayerRuntime::GetMapIndex(e)),ecs::PlayerRuntime::GetX(e),ecs::PlayerRuntime::GetY(e),ecs::PlayerRuntime::GetZ(e),true};
    auto commit=item::MakeCurrencyDropCommit(*c,s->operation,state.fence,*w,intent);if(!commit){s->status=Invalid;Reply(e,Invalid);return true;}
    const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=g_registry.get<ecs::TypedItemLogin>(e).token;const auto nonce=s->nonce;const auto fence=state.fence;
    static std::unordered_map<std::uint32_t,std::uint64_t> nextDrop;
    if(nextDrop[intent.owner]>now || !ReserveTypedCurrencyDrop(intent)){s->status=Denied;Reply(e,Denied);return true;}nextDrop[intent.owner]=now+250;
    s=Current(pid,token)==e?g_registry.try_get<TypedMoneySession>(e):nullptr;
    const auto* currentState=Current(pid,token)==e?g_registry.try_get<ecs::TypedItems>(e):nullptr;
    if(!s || s->nonce!=nonce || !currentState || currentState->state.fence!=fence || !permission->Current()){CancelTypedCurrencyDrop(intent);return true;}
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permission));s->pending=true;s->status=Pending;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    if(Current(pid,token)!=e || !held->Current()){
        CancelTypedCurrencyDrop(intent);if(Current(pid,token)==e){g_registry.remove<ecs::TypedItemBusy>(e);if(Current(pid,token)==e)if(auto* live=g_registry.try_get<TypedMoneySession>(e);live && live->nonce==nonce){live->pending=false;live->status=Failed;Reply(e,Failed);}}return true;
    }
    if(!store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(*commit),[pid,token,nonce,intent,held](store::CommitOutcome result)mutable{
        if(result.write)PublishTypedCurrencyDrop(intent);else CancelTypedCurrencyDrop(intent);const auto e=Current(pid,token);if(e==entt::null)return;
        if(result.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*result.state));if(Current(pid,token)!=e)return;g_registry.remove<ecs::TypedItemBusy>(e);if(Current(pid,token)!=e)return;
        if(auto* s=g_registry.try_get<TypedMoneySession>(e);s && s->nonce==nonce){s->pending=false;s->status=result.write?Applied:Failed;Reply(e,s->status);}else SendTypedInventory(e);PumpTypedItemGrants(e);
    })){CancelTypedCurrencyDrop(intent);g_registry.remove<ecs::TypedItemBusy>(e);s=g_registry.try_get<TypedMoneySession>(e);if(s){s->pending=false;s->status=Failed;Reply(e,Failed);}}
    return true;
}
void TickTypedMoney(){
    // Reconstruct a committed drop even when its original completion was lost.
    static std::uint64_t next=0;const auto now=Utc();if(currencyRecoveryPending || now<next)return;next=now+1000;
    const auto run=store::ItemStore::Instance().GroundRun();if(!run)return;currencyRecoveryPending=true;
    if(!store::ItemStore::Instance().Submit([run=*run,now](CAsyncSQL& db,const item::ItemCatalog&)->store::ItemStore::Completion{
        std::vector<item::CurrencyDropIntent> drops;
        std::unique_ptr<SQLMsg> rows(db.DirectQueryPrepared("SELECT operation_id,owner_id,amount,map_index,x,y,z,created_at_ms,expires_at_ms FROM player.item_currency_ground WHERE BINARY core_key=? AND run_generation=? AND claimed_operation IS NULL AND expires_at_ms>? ORDER BY created_at_ms LIMIT 4096",{SqlParam::Blob(run.core),SqlParam::UInt(run.run),SqlParam::UInt(now)}));
        bool valid=rows && !rows->uiSQLErrno && rows->Get();
        if(valid)for(auto row=rows->Get()->FetchRow();row;row=rows->Get()->FetchRow()){
            const auto lengths=rows->Get()->FetchLengths();if(!lengths || lengths[0]!=16){valid=false;break;}
            item::CurrencyDropIntent d;std::memcpy(d.operation.data(),row[0],16);d.core=run.core;d.run=run.run;d.nowAtMs=now;
            if(!ReadCurrencyField(row[1],d.owner)||!ReadCurrencyField(row[2],d.amount)||!ReadCurrencyField(row[3],d.map)||!ReadCurrencyField(row[4],d.x)||!ReadCurrencyField(row[5],d.y)||!ReadCurrencyField(row[6],d.z)||!ReadCurrencyField(row[7],d.createdAtMs)||!ReadCurrencyField(row[8],d.expiresAtMs)||!item::ValidCurrencyDropIntent(d)){valid=false;break;}drops.push_back(std::move(d));
        }
        return [valid,drops=std::move(drops)](){currencyRecoveryPending=false;if(!valid){LOG_ERROR("TypedMoney: currency ground recovery query failed");return;}for(const auto& d:drops)PublishTypedCurrencyDrop(d);};
    }))currencyRecoveryPending=false;
}
