#include "stdafx.h"
#include "TypedAttributes.hpp"
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
#include <item/AttributeRuntime.hpp>
#include <openssl/rand.h>
#include <algorithm>
#include <tuple>

namespace
{
namespace items=auriga::item;namespace store=items::runtime;using Status=items::AttributeRuntimeStatus;
// Unique component name: EnTT hashes names across translation units.
struct TypedAttributeRuntimeSession
{
    std::uint64_t nonce=0;items::ItemOwnerFence fence{};std::array<std::uint8_t,16> operation{};
    std::optional<items::PersistentItem> source,target;items::AttributeRuntimeQuote quote;std::optional<items::InstanceAttributes> after;std::vector<std::uint64_t> targets;
    bool pending=false,attempted=false;Status status=Status::Ready;
};
std::uint64_t Nonce(){std::uint64_t value=0;while(!value)if(RAND_bytes(reinterpret_cast<unsigned char*>(&value),sizeof(value))!=1)return 0;return value;}
bool Operation(std::array<std::uint8_t,16>& out)
{return RAND_bytes(out.data(),static_cast<int>(out.size()))==1 && std::ranges::any_of(out,[](auto byte){return byte!=0;});}

std::optional<std::uint32_t> Draw(std::uint32_t maximum)
{
    if(!maximum)return {};constexpr auto range=std::uint64_t(UINT32_MAX)+1;const auto limit=range-range%maximum;std::uint32_t value=0;
    do{if(RAND_bytes(reinterpret_cast<unsigned char*>(&value),sizeof(value))!=1)return {};}while(value>=limit);
    return value%maximum+1;
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
items::SocketMaintenanceActor Actor(entt::entity e)
{return {static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetJob(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetSex(e)),ecs::PlayerRuntime::GetPosition(e)==POS_FIGHTING};}

void Reply(entt::entity e,Status status)
{
    if(!g_registry.valid(e))return;const auto c=store::CurrentCatalog();if(!c)return;const auto* s=g_registry.try_get<TypedAttributeRuntimeSession>(e);
    const auto packet=items::EncodeAttributeRuntime(*c,s ? s->nonce : 0,s && s->source ? &*s->source : nullptr,s && s->target ? &*s->target : nullptr,s ? s->fence.revision : 0,status,s ? s->quote : items::AttributeRuntimeQuote{},s && s->after && (status==Status::Applied || status==Status::AttemptFailed) ? &*s->after : nullptr,s ? std::span<const std::uint64_t>(s->targets) : std::span<const std::uint64_t>{});
    SendTypedInventory(e);auto* desc=g_registry.valid(e) ? ecs::PlayerRuntime::GetDesc(e) : nullptr;if(packet && desc && desc->IsPhase(PHASE_GAME))desc->LargePacket(packet->data(),static_cast<int>(packet->size()));
}
void Close(entt::entity e)
{if(!g_registry.valid(e))return;const auto* s=g_registry.try_get<TypedAttributeRuntimeSession>(e);if(s && s->pending){Reply(e,Status::Pending);return;}g_registry.remove<TypedAttributeRuntimeSession>(e);Reply(e,Status::Closed);}
}
void TickTypedAttributes()
{std::vector<entt::entity> closed;for(auto e:g_registry.view<TypedAttributeRuntimeSession>())if(!g_registry.get<TypedAttributeRuntimeSession>(e).pending && !Admitted(e))closed.push_back(e);for(auto e:closed)Close(e);}
bool HandleTypedAttributesPacket(entt::entity e,const char* data)
{
    static_assert(sizeof(TPacketCGTypedAttributes)==26);const auto r=*reinterpret_cast<const TPacketCGTypedAttributes*>(data);if(!g_registry.valid(e))return true;
    auto* s=g_registry.try_get<TypedAttributeRuntimeSession>(e);if(s && s->pending){Reply(e,Status::Pending);return true;}const auto c=store::CurrentCatalog();if(!c)return true;
    if(r.action==1)
    {
        if(r.session){Reply(e,Status::Invalid);return true;}if(!Admitted(e) || g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Denied);return true;}
        const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto id=items::ItemInstanceId::Create(r.source);
        const auto source=id ? std::ranges::find(typed->state.items,*id,&items::PersistentItem::id) : typed->state.items.end();const auto* entry=source!=typed->state.items.end() ? c->Find(source->key) : nullptr;
        const auto* row=entry ? std::get_if<items::ItemConsumableRow>(&entry->category) : nullptr;const auto* service=row && row->attributeServiceProfile ? c->FindAttributeService(*row->attributeServiceProfile) : nullptr;
        if(!service || !items::PlainInventoryItem(*source,typed->state.fence.owner)){g_registry.remove<TypedAttributeRuntimeSession>(e);Reply(e,Status::Invalid);return true;}
        TypedAttributeRuntimeSession next;next.nonce=Nonce();if(!next.nonce || !Operation(next.operation)){g_registry.remove<TypedAttributeRuntimeSession>(e);Reply(e,Status::Failed);return true;}
        next.fence=typed->state.fence;next.source=*source;next.quote.operation=static_cast<std::uint8_t>(service->operation);next.quote.pool=static_cast<std::uint8_t>(service->pool);next.quote.units=service->units;next.quote.available=source->count;next.quote.chance=service->successBp;next.quote.consumeFailure=service->consumeOnFailure;
        std::vector<const items::PersistentItem*> candidates;for(const auto& item:typed->state.items)if(items::AttributeRuntimeTarget(*c,item,next.fence.owner))candidates.push_back(&item);
        std::ranges::sort(candidates,{},[](const auto* i){return std::tuple{i->placement->storageId,i->placement->anchor,i->id};});
        for(const auto* item:candidates)next.targets.push_back(item->id.Value());for(const auto* item:candidates)if((!r.target && !next.target) || item->id.Value()==r.target)next.target=*item;
        next.status=r.target && !next.target ? Status::Invalid : Status::NoTarget;
        if(next.target){next.quote.before=next.target->attributes;const auto quote=items::PrepareAttributeRuntime(*c,next.fence,typed->state.items,source->id,next.target->id,Actor(e));next.status=quote ? Status::Ready : quote.error();if(quote)next.quote=*quote;}
        g_registry.emplace_or_replace<TypedAttributeRuntimeSession>(e,std::move(next));if(auto* current=g_registry.try_get<TypedAttributeRuntimeSession>(e))Reply(e,current->status);return true;
    }
    if(!s || s->nonce!=r.session || !s->source || s->source->id.Value()!=r.source || (s->target ? s->target->id.Value() : 0)!=r.target){Reply(e,Status::Stale);return true;}
    if(r.action==3){Close(e);return true;}if(r.action!=2){Reply(e,Status::Invalid);return true;}if(s->attempted){Reply(e,s->status);return true;}s->attempted=true;
    if(!Admitted(e)){s->status=Status::Denied;Reply(e,s->status);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e)){s->status=Status::Busy;Reply(e,s->status);return true;}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);
    if(!permit || !permit->Current()){if(auto* current=g_registry.try_get<TypedAttributeRuntimeSession>(e)){current->status=Status::Denied;Reply(e,current->status);}return true;}
    s=g_registry.try_get<TypedAttributeRuntimeSession>(e);const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);
    if(!s || s->nonce!=r.session || !typed || !login || !Admitted(e))return true;if(g_registry.all_of<ecs::TypedItemBusy>(e)){s->status=Status::Busy;Reply(e,s->status);return true;}
    if(typed->state.fence!=s->fence)
    {
        const auto source=std::ranges::find(typed->state.items,s->source->id,&items::PersistentItem::id);const auto target=s->target ? std::ranges::find(typed->state.items,s->target->id,&items::PersistentItem::id) : typed->state.items.end();
        if(!OnlyTypedAccessoryClockChanges(s->fence,typed->state.fence) || source==typed->state.items.end() || *source!=*s->source || (s->target && (target==typed->state.items.end() || *target!=*s->target))){s->status=Status::Stale;Reply(e,s->status);return true;}s->fence=typed->state.fence;
    }
    if(!s->target){s->status=Status::NoTarget;Reply(e,s->status);return true;}
    auto plan=items::MakeAttributeRuntimeCommit(*c,s->operation,s->fence,typed->state.storages,typed->state.items,s->source->id,s->target->id,Actor(e),s->quote,Draw);
    if(!plan){s->status=plan.error();Reply(e,s->status);return true;}
    const bool succeeded=plan->succeeded;const auto after=plan->after;const auto fence=s->fence;const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=login->token;const auto nonce=s->nonce;
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));if(!held->Current())return true;s->pending=true;s->status=Status::Pending;g_registry.insert<ecs::TypedItemBusy>(&e,&e+1);
    if(Current(pid,token)!=e || !held->Current()){if(Current(pid,token)==e){g_registry.remove<ecs::TypedItemBusy>(e);if(auto* current=g_registry.try_get<TypedAttributeRuntimeSession>(e);current && current->nonce==nonce){current->pending=false;current->status=Status::Failed;Reply(e,current->status);}}return true;}
    const auto complete=[pid,token,nonce,held,succeeded,after](store::CommitOutcome outcome){
        const auto e=Current(pid,token);if(e==entt::null)return;if(outcome.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*outcome.state));if(Current(pid,token)!=e)return;g_registry.remove<ecs::TypedItemBusy>(e);if(Current(pid,token)!=e)return;
        if(auto* current=g_registry.try_get<TypedAttributeRuntimeSession>(e);current && current->nonce==nonce){current->pending=false;current->status=outcome.write ? (succeeded ? Status::Applied : Status::AttemptFailed) : Status::Failed;if(outcome.write)current->after=after;Reply(e,current->status);}else SendTypedInventory(e);PumpTypedItemGrants(e);
    };
    if(!store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(plan->commit),complete)){g_registry.remove<ecs::TypedItemBusy>(e);if(auto* current=g_registry.try_get<TypedAttributeRuntimeSession>(e);current && current->nonce==nonce){current->pending=false;current->status=Status::Failed;Reply(e,current->status);}}
    return true;
}
