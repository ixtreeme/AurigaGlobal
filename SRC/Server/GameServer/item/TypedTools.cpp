#include "stdafx.h"
#include "TypedTools.hpp"
#include "TypedGathering.hpp"
#include "TypedFishProcessing.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/SpatialHelpers.hpp"
#include "../ecs/components/activity_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include "../item/refine.h"
#include "../world/fishing.h"
#include <item/ToolRuntime.hpp>
#include <openssl/rand.h>
namespace
{
namespace item=auriga::item;namespace store=item::runtime;
enum class Status:std::uint8_t{Closed,Ready,Pending,Applied,Invalid,Denied,Busy,Stale,Failed};
struct TypedToolsSession
{
    std::uint64_t nonce=0,ownerSession=0,catalogRevision=0;item::ItemCatalogDigest catalog{};std::array<std::uint8_t,16> operation{};
    std::optional<item::PersistentItem> source,tool;item::ToolRuntimeAction action{};
    entt::entity npc=entt::null;std::uint32_t npcVid=0,roll=0;std::uint8_t outcome=0,error=255;
    std::uint16_t afterLevel=0;item::ToolInstanceState after;Status status=Status::Ready;bool pending=false,attempted=false;
};
std::uint64_t Nonce(){std::uint64_t n=0;while(!n)if(RAND_bytes(reinterpret_cast<unsigned char*>(&n),8)!=1)return 0;return n;}
std::optional<std::uint32_t> Roll(std::uint32_t bound)
{if(!bound)return {};const auto range=std::uint64_t(UINT32_MAX)+1,limit=range-range%bound;std::uint32_t n;do{if(RAND_bytes(reinterpret_cast<unsigned char*>(&n),4)!=1)return {};}while(n>=limit);return n%bound+1;}
const item::ItemToolRow* ToolRow(const item::ItemCatalog& c,const item::PersistentItem& i)
{const auto* e=c.Find(i.key);return e ? std::get_if<item::ItemToolRow>(&e->category) : nullptr;}
bool Service(entt::entity e,entt::entity npc,item::ToolKind kind)
{
    if(!g_registry.valid(npc) || !ecs::PlayerRuntime::IsValid(npc) || ecs::PlayerRuntime::IsPC(npc) || CombatSystem::IsDead(npc) || ecs::PlayerRuntime::GetMapIndex(e)!=ecs::PlayerRuntime::GetMapIndex(npc))return false;
    const auto race=ecs::PlayerRuntime::GetRaceNum(npc);
    if(kind==item::ToolKind::Rod ? race!=fishing::FISHER_MOB : race!=BLACKSMITH_MOB && race!=BLACKSMITH2_MOB)return false;
    const auto x=std::abs(std::int64_t(ecs::PlayerRuntime::GetX(e))-ecs::PlayerRuntime::GetX(npc));const auto y=std::abs(std::int64_t(ecs::PlayerRuntime::GetY(e))-ecs::PlayerRuntime::GetY(npc));return std::max(x,y)+std::min(x,y)/2<1000;
}
item::ToolRuntimeActor Actor(entt::entity e,const TypedToolsSession& s)
{
    const auto c=store::CurrentCatalog();const auto* row=c && s.tool ? ToolRow(*c,*s.tool) : nullptr;
    return {static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetJob(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetSex(e)),TypedGatheringAdmitted(e),ecs::PlayerRuntime::GetPosition(e)==POS_FIGHTING,TypedGatheringActive(e),row && Service(e,s.npc,row->kind),false};
}
void Reply(entt::entity e,Status status,std::uint8_t error=255)
{
    const auto c=store::CurrentCatalog();if(!g_registry.valid(e) || !c)return;auto* s=g_registry.try_get<TypedToolsSession>(e);if(s){s->status=status;s->error=error;}
    const bool applied=status==Status::Applied;
    const auto packet=item::EncodeToolRuntime(*c,{static_cast<std::uint8_t>(status),error,applied && s ? s->outcome : std::uint8_t{},s ? s->nonce : 0,s && s->source ? &*s->source : nullptr,s && s->tool ? &*s->tool : nullptr,s ? s->action : item::ToolRuntimeAction{},s ? s->npcVid : 0,applied && s ? s->roll : 0,applied && s ? s->afterLevel : std::uint16_t{},applied && s ? s->after : item::ToolInstanceState{}});
    SendTypedInventory(e);auto* d=ecs::PlayerRuntime::GetDesc(e);if(packet && d && d->IsPhase(PHASE_GAME))d->LargePacket(packet->data(),static_cast<int>(packet->size()));
}
entt::entity Current(std::uint32_t pid,std::uint64_t token)
{const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* l=e!=entt::null && g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;return l && l->token==token ? e : entt::null;}
}
bool TypedGatheringActive(entt::entity e)
{
    if(!g_registry.valid(e))return false;const auto* fishing=g_registry.try_get<ecs::FishingState>(e);const auto* mining=g_registry.try_get<ecs::MiningState>(e);
    return TypedResourceActive(e) || g_registry.all_of<ecs::FishingActiveTag>(e) || (fishing && fishing->fishingNewEvent) || (mining && mining->event) || ecs::PlayerRuntime::GetCharEvent(e,ecs::PlayerRuntime::CharEvent::Fishing);
}
bool HandleTypedToolsPacket(entt::entity e,const char* bytes)
{
    if(!g_registry.valid(e))return true;const auto r=*reinterpret_cast<const TPacketCGTypedRecovery*>(bytes);const auto c=store::CurrentCatalog();if(!c)return true;
    auto* s=g_registry.try_get<TypedToolsSession>(e);if(s && s->pending){Reply(e,Status::Pending);return true;}
    if(r.action==4 || r.action==5 || r.action==7)
    {
        if(r.session || !r.source || (r.action!=7 && r.target)){Reply(e,Status::Invalid);return true;}
        g_registry.remove<TypedToolsSession>(e);
        if(!TypedGatheringAdmitted(e)){Reply(e,Status::Denied);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Busy);return true;}
        const auto& state=g_registry.get<ecs::TypedItems>(e).state;const auto source=std::ranges::find_if(state.items,[&](const auto& i){return i.id.Value()==r.source;});
        if(source==state.items.end() || source->owner!=state.fence.owner){g_registry.remove<TypedToolsSession>(e);Reply(e,Status::Invalid);return true;}
        const auto* fish=std::get_if<item::ItemFishingRow>(&c->Find(source->key)->category);
        if(r.action==7 ? !fish || fish->kind!=item::FishingKind::Bait : !ToolRow(*c,*source))
        {g_registry.remove<TypedToolsSession>(e);Reply(e,Status::Invalid);return true;}
        TypedToolsSession next;next.nonce=Nonce();if(!next.nonce || RAND_bytes(next.operation.data(),16)!=1 || std::ranges::all_of(next.operation,[](auto b){return b==0;})){Reply(e,Status::Failed);return true;}
        next.ownerSession=state.fence.sessionGeneration;next.catalog=c->ContentHash();next.catalogRevision=c->Revision();next.source=*source;next.action=r.action==7 ? item::ToolRuntimeAction::Bait : r.action==5 ? item::ToolRuntimeAction::Refine : item::ToolRuntimeAction::Info;
        if(r.action==7)
        {
            const auto rod=std::ranges::find_if(state.items,[&](const auto& i){const auto* row=ToolRow(*c,i);return row && row->kind==item::ToolKind::Rod && i.placement && *i.placement==item::ItemPlacement{item::ItemStorageKind::Equipment,1,0} && (!r.target || i.id.Value()==r.target);});
            if(rod!=state.items.end())next.tool=*rod;
        }
        else if(ToolRow(*c,*source))next.tool=*source;
        if(next.tool && r.action!=7)
        {
            const auto kind=ToolRow(*c,*next.tool)->kind;
            ecs::ForEachAround(g_registry,e,[&](entt::entity npc){if(Service(e,npc,kind) && (next.npc==entt::null || ecs::PlayerRuntime::GetPacketVID(npc)<next.npcVid)){next.npc=npc;next.npcVid=ecs::PlayerRuntime::GetPacketVID(npc);}});
        }
        const auto checked=item::CheckToolRuntime(*c,state.fence.owner,state.items,{source->id,r.action==7 && next.tool ? std::optional(next.tool->id) : std::nullopt,next.action,Actor(e,next)});
        next.status=checked ? Status::Ready : Status::Denied;next.error=checked ? 255 : static_cast<std::uint8_t>(checked.error());g_registry.emplace_or_replace<TypedToolsSession>(e,std::move(next));s=g_registry.try_get<TypedToolsSession>(e);Reply(e,s->status,s->error);return true;
    }
    if(!s || !s->source || r.session!=s->nonce || r.source!=s->source->id.Value() || r.target!=(s->action==item::ToolRuntimeAction::Bait && s->tool ? s->tool->id.Value() : 0)){Reply(e,Status::Stale);return true;}
    if(r.action==9){g_registry.remove<TypedToolsSession>(e);Reply(e,Status::Closed);return true;}
    if((r.action!=6 || s->action!=item::ToolRuntimeAction::Refine) && (r.action!=8 || s->action!=item::ToolRuntimeAction::Bait)){Reply(e,Status::Invalid);return true;}
    if(s->attempted){Reply(e,s->status,s->error);return true;}if(s->status!=Status::Ready){Reply(e,Status::Denied,s->error);return true;}
    if(!TypedGatheringAdmitted(e)){Reply(e,Status::Denied);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Busy);return true;}
    s->attempted=true;const auto& state=g_registry.get<ecs::TypedItems>(e).state;
    const auto same=[&](const std::optional<item::PersistentItem>& expected){if(!expected)return true;const auto current=std::ranges::find(state.items,expected->id,&item::PersistentItem::id);return current!=state.items.end() && *current==*expected;};
    if(state.fence.sessionGeneration!=s->ownerSession || c->Revision()!=s->catalogRevision || c->ContentHash()!=s->catalog || !same(s->source) || !same(s->tool)){Reply(e,Status::Stale);return true;}
    item::ToolRuntimeRequest request{s->source->id,s->action==item::ToolRuntimeAction::Bait && s->tool ? std::optional(s->tool->id) : std::nullopt,s->action,Actor(e,*s)};
    const auto checked=item::CheckToolRuntime(*c,state.fence.owner,state.items,request);if(!checked){Reply(e,Status::Denied,static_cast<std::uint8_t>(checked.error()));return true;}
    if(s->action==item::ToolRuntimeAction::Refine)
    {const auto* row=ToolRow(*c,*s->tool);const auto chance=c->FindToolProfile(row->profile)->levels[s->tool->refinementLevel].refineChanceBp;if(chance && chance<10000){const auto roll=Roll(10000);if(!roll){Reply(e,Status::Failed);return true;}s->roll=*roll;request.roll=*roll;}}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permit || !permit->Current()){Reply(e,Status::Denied);return true;}
    auto plan=item::MakeToolRuntimeCommit(*c,s->operation,state.fence,state.storages,state.items,request);if(!plan){Reply(e,Status::Denied,static_cast<std::uint8_t>(plan.error()));return true;}
    const auto owner=state.fence;const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=g_registry.get<ecs::TypedItemLogin>(e).token;auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
    s->outcome=plan->outcome;s->afterLevel=plan->level;s->after=plan->state;s->pending=true;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    const bool queued=store::RequestCommit(owner.owner,owner.sessionGeneration,std::move(plan->commit),[pid,token,held](store::CommitOutcome out)
    {
        const auto e=Current(pid,token);if(e==entt::null)return;if(!out.state){LOG_ERROR("TypedTools: authoritative reload failed pid {}",pid);if(auto* d=ecs::PlayerRuntime::GetDesc(e))d->SetPhase(PHASE_CLOSE);return;}
        g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*out.state));g_registry.remove<ecs::TypedItemBusy>(e);if(auto* s=g_registry.try_get<TypedToolsSession>(e)){s->pending=false;if(!out.write){s->outcome=0;s->roll=0;}}
        Reply(e,out.write ? Status::Applied : Status::Failed);PumpTypedItemGrants(e);
    });
    if(!queued){g_registry.remove<ecs::TypedItemBusy>(e);s->pending=false;s->outcome=0;s->roll=0;Reply(e,Status::Failed);}return true;
}
