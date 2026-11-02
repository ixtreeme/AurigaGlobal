#include "stdafx.h"
#include "TypedTokenRedemption.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../quest/questpc.h"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include <item/TokenRedemption.hpp>
#include <openssl/rand.h>
#include <algorithm>

namespace
{
namespace items=auriga::item;namespace store=items::runtime;using Status=items::TokenRedemptionStatus;
// Unique component name: EnTT hashes names across translation units.
struct TypedTokenRedemptionSession
{
    std::uint64_t nonce=0;items::ItemOwnerFence fence{};std::array<std::uint8_t,16> operation{};
    std::optional<items::PersistentItem> item;items::TokenRedemptionQuote quote;
    bool pending=false,attempted=false;Status status=Status::Ready;
};
std::uint64_t Nonce(){std::uint64_t value=0;while(!value)if(RAND_bytes(reinterpret_cast<unsigned char*>(&value),sizeof(value))!=1)return 0;return value;}
bool Operation(std::array<std::uint8_t,16>& out)
{return RAND_bytes(out.data(),static_cast<int>(out.size()))==1 && std::ranges::any_of(out,[](auto byte){return byte!=0;});}
entt::entity Current(std::uint32_t pid,std::uint64_t token)
{const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* login=e!=entt::null && g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;return login && login->token==token ? e : entt::null;}
bool Admitted(entt::entity e)
{
    if(!g_registry.valid(e) || !ecs::PlayerRuntime::IsPC(e) || CombatSystem::IsDead(e) || !InventorySystem::CanHandleItems(e) ||
       ecs::SocialSystem::HasExchange(e) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e) ||
       ecs::SocialSystem::GetMyShop(e)!=entt::null || ecs::SocialSystem::GetShopOwner(e)!=entt::null || !g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e))return false;
    const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);if(flags && (flags->isObserverMode || flags->isStunned))return false;
    const auto* quest=g_registry.try_get<ecs::QuestPCState>(e);if(!quest || !quest->pc || quest->pc->IsRunning())return false;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    if(ecs::PlayerRuntime::GetGMLevel(e)>GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(e)<GM_IMPLEMENTOR)return false;
#endif
    const auto* desc=ecs::PlayerRuntime::GetDesc(e);return desc && desc->IsPhase(PHASE_GAME);
}
items::TokenRedemptionActor Actor(entt::entity e)
{return {static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetJob(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetSex(e)),ecs::PlayerRuntime::GetPosition(e)==POS_FIGHTING};}
Status Refusal(items::TokenRedemptionError error)
{switch(error){case items::TokenRedemptionError::Denied:return Status::Denied;case items::TokenRedemptionError::WalletFull:return Status::WalletFull;case items::TokenRedemptionError::Unsupported:return Status::Unsupported;case items::TokenRedemptionError::Stale:return Status::Stale;case items::TokenRedemptionError::Level:return Status::Level;default:return Status::Invalid;}}
void Reply(entt::entity e,Status status)
{
    if(!g_registry.valid(e))return;const auto c=store::CurrentCatalog();if(!c)return;const auto* s=g_registry.try_get<TypedTokenRedemptionSession>(e);
    const auto packet=items::EncodeTokenRedemption(*c,s ? s->nonce : 0,s && s->item ? &*s->item : nullptr,s ? s->fence.revision : 0,status,s ? s->quote : items::TokenRedemptionQuote{});
    SendTypedInventory(e);auto* desc=g_registry.valid(e) ? ecs::PlayerRuntime::GetDesc(e) : nullptr;if(packet && desc && desc->IsPhase(PHASE_GAME))desc->LargePacket(packet->data(),static_cast<int>(packet->size()));
}
void Close(entt::entity e)
{if(!g_registry.valid(e))return;const auto* s=g_registry.try_get<TypedTokenRedemptionSession>(e);if(s && s->pending){Reply(e,Status::Pending);return;}g_registry.remove<TypedTokenRedemptionSession>(e);Reply(e,Status::Closed);}
}
void TickTypedTokenRedemption()
{std::vector<entt::entity> closed;for(auto e:g_registry.view<TypedTokenRedemptionSession>())if(!g_registry.get<TypedTokenRedemptionSession>(e).pending && !Admitted(e))closed.push_back(e);for(auto e:closed)Close(e);}
bool HandleTypedTokenRedemptionPacket(entt::entity e,const char* data)
{
    static_assert(sizeof(TPacketCGTypedTokenRedemption)==22);const auto r=*reinterpret_cast<const TPacketCGTypedTokenRedemption*>(data);if(!g_registry.valid(e))return true;
    auto* s=g_registry.try_get<TypedTokenRedemptionSession>(e);if(s && s->pending){Reply(e,Status::Pending);return true;}const auto c=store::CurrentCatalog();if(!c)return true;
    if(r.action==1)
    {
        if(r.session){Reply(e,Status::Invalid);return true;}if(!Admitted(e) || g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Denied);return true;}
        const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto id=items::ItemInstanceId::Create(r.item);const auto item=id ? std::ranges::find(typed->state.items,*id,&items::PersistentItem::id) : typed->state.items.end();
        const auto* entry=item!=typed->state.items.end() ? c->Find(item->key) : nullptr;const auto* token=entry ? std::get_if<items::ItemCurrencyTokenRow>(&entry->category) : nullptr;
        if(!token || !items::PlainInventoryItem(*item,typed->state.fence.owner)){g_registry.remove<TypedTokenRedemptionSession>(e);Reply(e,Status::Invalid);return true;}
        TypedTokenRedemptionSession next;next.nonce=Nonce();if(!next.nonce || !Operation(next.operation)){g_registry.remove<TypedTokenRedemptionSession>(e);Reply(e,Status::Failed);return true;}
        next.fence=typed->state.fence;next.item=*item;next.quote={item->count,r.units ? r.units : item->count,0,token->currency,token->amountPerUnit,0,0,token->currency==items::WalletCurrency::Gold ? std::uint64_t(INT64_MAX) : 0};
        if(token->currency==items::WalletCurrency::Gold)for(const auto& wallet:typed->state.wallets)if(wallet.owner==next.fence.owner && wallet.currency==token->currency)next.quote.balance=wallet.balance;
        const auto plan=items::MakeTokenRedemptionCommit(*c,next.operation,next.fence,typed->state.storages,typed->state.items,typed->state.wallets,*id,next.quote.requested,INT64_MAX,Actor(e));
        next.status=plan ? Status::Ready : Refusal(plan.error());if(plan)next.quote=plan->quote;
        g_registry.emplace_or_replace<TypedTokenRedemptionSession>(e,std::move(next));if(auto* s=g_registry.try_get<TypedTokenRedemptionSession>(e))Reply(e,s->status);return true;
    }
    if(!s || s->nonce!=r.session || !s->item || s->item->id.Value()!=r.item || (r.action==2 && r.units!=s->quote.requested)){Reply(e,Status::Stale);return true;}
    if(r.action==3){if(r.units){Reply(e,Status::Invalid);return true;}Close(e);return true;}if(r.action!=2){Reply(e,Status::Invalid);return true;}
    if(s->attempted){Reply(e,s->status);return true;}s->attempted=true;
    if(!Admitted(e)){s->status=Status::Denied;Reply(e,s->status);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e)){s->status=Status::Busy;Reply(e,s->status);return true;}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);
    if(!permit || !permit->Current()){if(auto* s=g_registry.try_get<TypedTokenRedemptionSession>(e)){s->status=Status::Denied;Reply(e,s->status);}return true;}
    s=g_registry.try_get<TypedTokenRedemptionSession>(e);const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);
    if(!s || s->nonce!=r.session || !typed || !login || !Admitted(e))return true;
    if(g_registry.all_of<ecs::TypedItemBusy>(e)){s->status=Status::Busy;Reply(e,s->status);return true;}
    if(typed->state.fence!=s->fence){s->status=Status::Stale;Reply(e,s->status);return true;}
    auto plan=items::MakeTokenRedemptionCommit(*c,s->operation,s->fence,typed->state.storages,typed->state.items,typed->state.wallets,s->item->id,s->quote.requested,INT64_MAX,Actor(e));
    if(!plan){s->status=Refusal(plan.error());Reply(e,s->status);return true;}
    if(plan->quote!=s->quote){s->status=Status::Stale;Reply(e,s->status);return true;}
    const auto fence=s->fence;const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=login->token;const auto nonce=s->nonce;
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));if(!held->Current())return true;s->pending=true;s->status=Status::Pending;g_registry.insert<ecs::TypedItemBusy>(&e,&e+1);
    if(Current(pid,token)!=e || !held->Current()){if(Current(pid,token)==e){g_registry.remove<ecs::TypedItemBusy>(e);if(auto* s=g_registry.try_get<TypedTokenRedemptionSession>(e);s && s->nonce==nonce){s->pending=false;s->status=Status::Failed;Reply(e,s->status);}}return true;}
    const auto complete=[pid,token,nonce,held](store::CommitOutcome outcome) {
        const auto e=Current(pid,token);if(e==entt::null)return;if(outcome.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*outcome.state));if(Current(pid,token)!=e)return;g_registry.remove<ecs::TypedItemBusy>(e);if(Current(pid,token)!=e)return;
        if(auto* s=g_registry.try_get<TypedTokenRedemptionSession>(e);s && s->nonce==nonce){s->pending=false;s->status=outcome.write ? Status::Redeemed : Status::Failed;Reply(e,s->status);}else SendTypedInventory(e);PumpTypedItemGrants(e);
    };
    if(!store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(plan->commit),complete)){g_registry.remove<ecs::TypedItemBusy>(e);if(auto* s=g_registry.try_get<TypedTokenRedemptionSession>(e);s && s->nonce==nonce){s->pending=false;s->status=Status::Failed;Reply(e,s->status);}}
    return true;
}
