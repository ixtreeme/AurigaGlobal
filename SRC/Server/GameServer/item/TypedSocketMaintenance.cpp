#include "stdafx.h"
#include "TypedSocketMaintenance.hpp"
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
#include <item/SocketMaintenance.hpp>
#include <openssl/rand.h>
#include <algorithm>
#include <tuple>

namespace
{
namespace items=auriga::item;namespace store=items::runtime;using Status=items::SocketMaintenanceStatus;
// Unique component name: EnTT hashes names across translation units.
struct TypedSocketMaintenanceSession
{
    std::uint64_t nonce=0;items::ItemOwnerFence fence{};std::array<std::uint8_t,16> operation{};
    std::optional<items::PersistentItem> source,target;items::SocketMaintenanceQuote quote;std::vector<std::uint64_t> targets;
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
items::SocketMaintenanceActor Actor(entt::entity e)
{return {static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetJob(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetSex(e)),ecs::PlayerRuntime::GetPosition(e)==POS_FIGHTING};}
void Reply(entt::entity e,Status status)
{
    if(!g_registry.valid(e))return;const auto c=store::CurrentCatalog();if(!c)return;const auto* s=g_registry.try_get<TypedSocketMaintenanceSession>(e);
    const auto packet=items::EncodeSocketMaintenance(*c,s ? s->nonce : 0,s && s->source ? &*s->source : nullptr,s && s->target ? &*s->target : nullptr,s ? s->fence.revision : 0,status,s ? s->quote : items::SocketMaintenanceQuote{},s ? std::span<const std::uint64_t>(s->targets) : std::span<const std::uint64_t>{});
    SendTypedInventory(e);auto* desc=g_registry.valid(e) ? ecs::PlayerRuntime::GetDesc(e) : nullptr;if(packet && desc && desc->IsPhase(PHASE_GAME))desc->LargePacket(packet->data(),static_cast<int>(packet->size()));
}
void Close(entt::entity e)
{if(!g_registry.valid(e))return;const auto* s=g_registry.try_get<TypedSocketMaintenanceSession>(e);if(s && s->pending){Reply(e,Status::Pending);return;}g_registry.remove<TypedSocketMaintenanceSession>(e);Reply(e,Status::Closed);}
}
void TickTypedSocketMaintenance()
{std::vector<entt::entity> closed;for(auto e:g_registry.view<TypedSocketMaintenanceSession>())if(!g_registry.get<TypedSocketMaintenanceSession>(e).pending && !Admitted(e))closed.push_back(e);for(auto e:closed)Close(e);}
bool HandleTypedSocketMaintenancePacket(entt::entity e,const char* data)
{
    static_assert(sizeof(TPacketCGTypedSocketMaintenance)==26);const auto r=*reinterpret_cast<const TPacketCGTypedSocketMaintenance*>(data);if(!g_registry.valid(e))return true;
    auto* s=g_registry.try_get<TypedSocketMaintenanceSession>(e);if(s && s->pending){Reply(e,Status::Pending);return true;}const auto c=store::CurrentCatalog();if(!c)return true;
    if(r.action==1)
    {
        if(r.session){Reply(e,Status::Invalid);return true;}if(!Admitted(e) || g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Denied);return true;}
        const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto id=items::ItemInstanceId::Create(r.source);
        const auto source=id ? std::ranges::find(typed->state.items,*id,&items::PersistentItem::id) : typed->state.items.end();
        const auto* entry=source!=typed->state.items.end() ? c->Find(source->key) : nullptr;const auto* row=entry ? std::get_if<items::ItemConsumableRow>(&entry->category) : nullptr;
        const auto* profile=row && row->socketServiceProfile ? c->FindSocketService(*row->socketServiceProfile) : nullptr;
        if(!profile || !items::PlainInventoryItem(*source,typed->state.fence.owner)){g_registry.remove<TypedSocketMaintenanceSession>(e);Reply(e,Status::Invalid);return true;}
        TypedSocketMaintenanceSession next;next.nonce=Nonce();if(!next.nonce || !Operation(next.operation)){g_registry.remove<TypedSocketMaintenanceSession>(e);Reply(e,Status::Failed);return true;}
        next.fence=typed->state.fence;next.source=*source;next.quote.kind=static_cast<std::uint8_t>(profile->kind);next.quote.units=profile->unitsPerUse;next.quote.available=source->count;
        std::vector<const items::PersistentItem*> candidates;for(const auto& item:typed->state.items)if(items::SocketMaintenanceTarget(*c,item,next.fence.owner))candidates.push_back(&item);
        std::ranges::sort(candidates,{},[](const auto* i){return std::tuple{i->placement->storageId,i->placement->anchor,i->id};});
        for(const auto* item:candidates)next.targets.push_back(item->id.Value());
        for(const auto* item:candidates)if((!r.target && !next.target) || item->id.Value()==r.target)next.target=*item;
        next.status=r.target && !next.target ? Status::Invalid : Status::NoTarget;
        if(next.target)
        {
            next.quote.before=next.quote.after=next.target->stones;
            const auto plan=items::MakeSocketMaintenanceCommit(*c,next.operation,next.fence,typed->state.storages,typed->state.items,source->id,next.target->id,Actor(e),store::PeekInstanceIds(3));
            next.status=plan ? Status::Ready : plan.error();if(plan)next.quote=plan->quote;
        }
        g_registry.emplace_or_replace<TypedSocketMaintenanceSession>(e,std::move(next));if(auto* s=g_registry.try_get<TypedSocketMaintenanceSession>(e))Reply(e,s->status);return true;
    }
    if(!s || s->nonce!=r.session || !s->source || s->source->id.Value()!=r.source || (s->target ? s->target->id.Value() : 0)!=r.target){Reply(e,Status::Stale);return true;}
    if(r.action==3){Close(e);return true;}if(r.action!=2){Reply(e,Status::Invalid);return true;}
    if(s->attempted){Reply(e,s->status);return true;}s->attempted=true;
    if(!Admitted(e)){s->status=Status::Denied;Reply(e,s->status);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e)){s->status=Status::Busy;Reply(e,s->status);return true;}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);
    if(!permit || !permit->Current()){if(auto* s=g_registry.try_get<TypedSocketMaintenanceSession>(e)){s->status=Status::Denied;Reply(e,s->status);}return true;}
    s=g_registry.try_get<TypedSocketMaintenanceSession>(e);const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);
    if(!s || s->nonce!=r.session || !typed || !login || !Admitted(e))return true;
    if(g_registry.all_of<ecs::TypedItemBusy>(e)){s->status=Status::Busy;Reply(e,s->status);return true;}
    if(typed->state.fence!=s->fence){s->status=Status::Stale;Reply(e,s->status);return true;}
    if(!s->target){s->status=Status::NoTarget;Reply(e,s->status);return true;}
    auto plan=items::MakeSocketMaintenanceCommit(*c,s->operation,s->fence,typed->state.storages,typed->state.items,s->source->id,s->target->id,Actor(e),store::PeekInstanceIds(3));
    if(!plan){s->status=plan.error();Reply(e,s->status);return true;}
    if(plan->quote!=s->quote){s->status=Status::Stale;Reply(e,s->status);return true;}
    const auto fence=s->fence;const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=login->token;const auto nonce=s->nonce;
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));if(!held->Current())return true;store::SpendInstanceIds(plan->quote.returns.size());s->pending=true;s->status=Status::Pending;g_registry.insert<ecs::TypedItemBusy>(&e,&e+1);
    if(Current(pid,token)!=e || !held->Current()){if(Current(pid,token)==e){g_registry.remove<ecs::TypedItemBusy>(e);if(auto* s=g_registry.try_get<TypedSocketMaintenanceSession>(e);s && s->nonce==nonce){s->pending=false;s->status=Status::Failed;Reply(e,s->status);}}return true;}
    const auto complete=[pid,token,nonce,held](store::CommitOutcome outcome) {
        const auto e=Current(pid,token);if(e==entt::null)return;if(outcome.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*outcome.state));if(Current(pid,token)!=e)return;g_registry.remove<ecs::TypedItemBusy>(e);if(Current(pid,token)!=e)return;
        if(auto* s=g_registry.try_get<TypedSocketMaintenanceSession>(e);s && s->nonce==nonce){s->pending=false;s->status=outcome.write ? Status::Applied : Status::Failed;Reply(e,s->status);}else SendTypedInventory(e);PumpTypedItemGrants(e);
    };
    if(!store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(plan->commit),complete)){g_registry.remove<ecs::TypedItemBusy>(e);if(auto* s=g_registry.try_get<TypedSocketMaintenanceSession>(e);s && s->nonce==nonce){s->pending=false;s->status=Status::Failed;Reply(e,s->status);}}
    return true;
}
