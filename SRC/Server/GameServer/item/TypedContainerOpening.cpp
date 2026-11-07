#include "stdafx.h"
#include "TypedRecovery.hpp"
#include "TypedContainerOpening.hpp"
#include "TypedInventorySync.hpp"
#include "TypedAccessoryClock.hpp"
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
#include "../core/config.h"
#include <item/ContainerOpening.hpp>
#include <Core/Logging.hpp>
#include <algorithm>
#include <openssl/rand.h>
#include <tuple>

namespace
{
namespace items=auriga::item;namespace store=items::runtime;using Status=items::ContainerOpeningStatus;
// EnTT component names must remain unique across translation units.
struct TypedContainerOpeningSession
{
    std::uint64_t token=0;items::ItemOwnerFence fence{};
    std::optional<items::PersistentItem> box,key;std::vector<std::uint64_t> keys;std::vector<std::uint16_t> won;
    bool pending=false,attempted=false;Status status=Status::Ready;
};
std::uint64_t Nonce(){std::uint64_t n=0;while(!n)if(RAND_bytes(reinterpret_cast<unsigned char*>(&n),sizeof(n))!=1)return 0;return n;}
std::optional<std::array<std::uint8_t,16>> Operation()
{std::array<std::uint8_t,16> op{};if(RAND_bytes(op.data(),static_cast<int>(op.size()))!=1 || std::ranges::all_of(op,[](auto byte){return byte==0;}))return {};return op;}
std::uint32_t Draw(std::uint32_t bound)
{
    // Rejection sampling avoids modulo bias. Public nonces never expose a
    // predictable PRNG state shared with the loot draw.
    if(!bound)return 0;const auto range=std::uint64_t(UINT32_MAX)+1;const auto limit=range-range%bound;std::uint32_t value=0;
    do{if(RAND_bytes(reinterpret_cast<unsigned char*>(&value),sizeof(value))!=1)return bound;}while(value>=limit);
    return value%bound;
}
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
items::ContainerActor Actor(entt::entity e)
{return {static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetJob(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetSex(e)),ecs::PlayerRuntime::GetPosition(e)==POS_FIGHTING};}
Status Refusal(items::ContainerOpeningError e)
{
    switch(e){case items::ContainerOpeningError::Denied:return Status::Denied;case items::ContainerOpeningError::KeyRequired:return Status::KeyRequired;
    case items::ContainerOpeningError::WrongKey:return Status::WrongKey;case items::ContainerOpeningError::NoRoom:return Status::NoRoom;
    case items::ContainerOpeningError::WalletLimit:return Status::WalletLimit;case items::ContainerOpeningError::Unsupported:return Status::Unsupported;
    case items::ContainerOpeningError::Level:return Status::Level;case items::ContainerOpeningError::Unavailable:return Status::Busy;default:return Status::Invalid;}
}
void Reply(entt::entity e,Status status)
{
    if(!g_registry.valid(e))return;const auto c=store::CurrentCatalog();if(!c)return;const auto* s=g_registry.try_get<TypedContainerOpeningSession>(e);
    const auto packet=items::EncodeContainerOpening(*c,s ? s->token : 0,s && s->box ? &*s->box : nullptr,s && s->key ? &*s->key : nullptr,
        s ? s->fence.revision : 0,status,s ? std::span<const std::uint64_t>(s->keys) : std::span<const std::uint64_t>{},
        s && status==Status::Opened ? std::span<const std::uint16_t>(s->won) : std::span<const std::uint16_t>{});
    SendTypedInventory(e);auto* desc=g_registry.valid(e) ? ecs::PlayerRuntime::GetDesc(e) : nullptr;
    if(desc && desc->IsPhase(PHASE_GAME) && packet)desc->LargePacket(packet->data(),static_cast<int>(packet->size()));
}
void Close(entt::entity e)
{if(!g_registry.valid(e))return;const auto* s=g_registry.try_get<TypedContainerOpeningSession>(e);if(s && s->pending){Reply(e,Status::Pending);return;}g_registry.remove<TypedContainerOpeningSession>(e);Reply(e,Status::Closed);}
}
void TickTypedContainerOpening()
{std::vector<entt::entity> closed;for(auto e:g_registry.view<TypedContainerOpeningSession>())if(!g_registry.get<TypedContainerOpeningSession>(e).pending && !Admitted(e))closed.push_back(e);for(auto e:closed)Close(e);}
bool HandleTypedContainerOpeningPacket(entt::entity e,const char* data)
{
    static_assert(sizeof(TPacketCGTypedContainerOpening)==26);const auto r=*reinterpret_cast<const TPacketCGTypedContainerOpening*>(data);if(!g_registry.valid(e))return true;
    auto* s=g_registry.try_get<TypedContainerOpeningSession>(e);if(s && s->pending){Reply(e,Status::Pending);return true;}
    const auto c=store::CurrentCatalog();if(!c)return true;
    if(r.action==1)
    {
        if(r.session){Reply(e,Status::Invalid);return true;}
        if(!Admitted(e) || g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Denied);return true;}
        const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto id=items::ItemInstanceId::Create(r.box);
        const auto box=id ? std::ranges::find(typed->state.items,*id,&items::PersistentItem::id) : typed->state.items.end();
        const auto* entry=box!=typed->state.items.end() ? c->Find(box->key) : nullptr;const auto* row=entry ? std::get_if<items::ItemContainerRow>(&entry->category) : nullptr;
        if(!row || !items::ContainerItemUsable(*box,typed->state.fence.owner)){g_registry.remove<TypedContainerOpeningSession>(e);Reply(e,Status::Invalid);return true;}
        TypedContainerOpeningSession next;next.token=Nonce();if(!next.token){g_registry.remove<TypedContainerOpeningSession>(e);Reply(e,Status::Failed);return true;}next.fence=typed->state.fence;next.box=*box;
        if(row->kind==items::ContainerKind::Locked)
        {
            std::vector<const items::PersistentItem*> candidates;
            for(const auto& item:typed->state.items)
            {const auto* itemEntry=c->Find(item.key);const auto* key=itemEntry ? std::get_if<items::ItemContainerKeyRow>(&itemEntry->category) : nullptr;
             if(key && key->lockId==row->lockId && items::ContainerItemUsable(item,next.fence.owner) && !itemEntry->base.retired)candidates.push_back(&item);}
            std::ranges::sort(candidates,{},[](const auto* item){return std::tuple{item->placement->storageId,item->placement->anchor,item->id};});
            for(const auto* key:candidates)next.keys.push_back(key->id.Value());
            if(!r.key && !candidates.empty())next.key=*candidates.front();
        }
        if(r.key){const auto keyId=items::ItemInstanceId::Create(r.key);const auto found=keyId ? std::ranges::find(typed->state.items,*keyId,&items::PersistentItem::id) : typed->state.items.end();if(found!=typed->state.items.end())next.key=*found;}
        const auto checked=items::CheckContainerOpening(*c,{*id,next.key ? std::optional(next.key->id) : std::nullopt,Actor(e),TypedItemEffectNow()},next.fence.owner,typed->state.items);
        next.status=checked ? Status::Ready : Refusal(checked.error());
        if(r.key && (!next.key || next.key->id.Value()!=r.key))next.status=Status::WrongKey;
        g_registry.emplace_or_replace<TypedContainerOpeningSession>(e,std::move(next));s=g_registry.try_get<TypedContainerOpeningSession>(e);if(s)Reply(e,s->status);return true;
    }
    if(!s || s->token!=r.session || !s->box || s->box->id.Value()!=r.box || (s->key ? s->key->id.Value() : 0)!=r.key){Reply(e,Status::Stale);return true;}
    if(r.action==3){Close(e);return true;}if(r.action!=2){Reply(e,Status::Invalid);return true;}
    if(s->attempted){Reply(e,s->status);return true;}
    s->attempted=true;
    if(!Admitted(e)){s->status=Status::Denied;Reply(e,s->status);return true;}
    if(g_registry.all_of<ecs::TypedItemBusy>(e)){s->status=Status::Busy;Reply(e,s->status);return true;}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);
    if(!permit || !permit->Current()){s=g_registry.try_get<TypedContainerOpeningSession>(e);if(s){s->status=Status::Denied;Reply(e,s->status);}return true;}
    s=g_registry.try_get<TypedContainerOpeningSession>(e);const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);
    if(!s || s->token!=r.session || !typed || !login || !Admitted(e))return true;
    if(g_registry.all_of<ecs::TypedItemBusy>(e)){s->status=Status::Busy;Reply(e,s->status);return true;}
    if(typed->state.fence!=s->fence)
    {if(!OnlyTypedAccessoryClockChanges(s->fence,typed->state.fence)){s->status=Status::Stale;Reply(e,s->status);return true;}s->fence=typed->state.fence;}
#ifdef ENABLE_NEWSTUFF
    const bool plain=std::get<items::ItemContainerRow>(c->Find(s->box->key)->category).kind==items::ContainerKind::Use;
    if(plain && g_BoxUseTimeLimitValue && std::uint32_t(get_dword_time()-ecs::PlayerRuntime::GetBoxUseTime(e))<g_BoxUseTimeLimitValue){s->status=Status::Cooldown;Reply(e,s->status);return true;}
#endif
    const auto operation=Operation();if(!operation){s->status=Status::Failed;Reply(e,s->status);return true;}
    auto plan=items::MakeContainerOpeningCommit(*c,*operation,s->fence,typed->state.storages,typed->state.items,typed->state.wallets,
        {s->box->id,s->key ? std::optional(s->key->id) : std::nullopt,Actor(e),TypedItemEffectNow()},store::PeekInstanceIds(items::MaxItemCommitItems),
        Draw);
    if(!plan){s->status=Refusal(plan.error());Reply(e,s->status);return true;}
    const auto fence=s->fence;const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=login->token;const auto nonce=s->token;
    const bool gift=std::get<items::ItemContainerRow>(c->Find(s->box->key)->category).kind==items::ContainerKind::Use;
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));if(!held->Current())return true;
    store::SpendInstanceIds(std::ranges::count_if(plan->commit.items,[](const auto& change){return !change.before;}));
    s->pending=true;s->status=Status::Pending;s->won=plan->won;g_registry.insert<ecs::TypedItemBusy>(&e,&e+1);
    if(Current(pid,token)!=e || !held->Current()){if(Current(pid,token)==e){g_registry.remove<ecs::TypedItemBusy>(e);if(auto* s=g_registry.try_get<TypedContainerOpeningSession>(e);s && s->token==nonce){s->pending=false;s->status=Status::Failed;Reply(e,s->status);}}return true;}
    const auto complete=[pid,token,nonce,held,gift](store::CommitOutcome outcome) {
        const auto e=Current(pid,token);if(e==entt::null)return;
        if(outcome.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*outcome.state));if(Current(pid,token)!=e)return;g_registry.remove<ecs::TypedItemBusy>(e);if(Current(pid,token)!=e)return;
        if(outcome.write && gift)
        {
#ifdef ENABLE_NEWSTUFF
            ecs::PlayerRuntime::SetBoxUseTime(e,get_dword_time());
#endif
#ifdef ENABLE_RANKING
            const auto points=ecs::PlayerRuntime::GetRankPoints(e,17);if(points<INT64_MAX)ecs::PlayerRuntime::SetRankPoints(e,17,points+1);
#endif
        }
        if(Current(pid,token)!=e)return;
        if(auto* s=g_registry.try_get<TypedContainerOpeningSession>(e);s && s->token==nonce){s->pending=false;s->status=outcome.write ? Status::Opened : Status::Failed;Reply(e,s->status);}else SendTypedInventory(e);PumpTypedItemGrants(e);
    };
    if(!store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(plan->commit),complete))
    {g_registry.remove<ecs::TypedItemBusy>(e);if(auto* s=g_registry.try_get<TypedContainerOpeningSession>(e);s && s->token==nonce){s->pending=false;s->status=Status::Failed;Reply(e,s->status);}}
    return true;
}
