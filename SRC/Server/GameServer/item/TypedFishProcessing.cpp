#include "stdafx.h"
#include "TypedFishProcessing.hpp"
#include "TypedTools.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/SpatialHelpers.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../quest/questpc.h"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include "../core/config.h"
#include "../world/fishing.h"
#include "../world/arena.h"
#include "../combat/pvp.h"
#include <item/FishProcessing.hpp>
#include <openssl/rand.h>

namespace
{
namespace item=auriga::item;namespace store=item::runtime;using Status=item::FishProcessingStatus;
struct TypedFishProcessingSession
{
    std::uint64_t nonce=0,ownerSession=0;std::array<std::uint8_t,16> operation{};
    item::ItemCatalogDigest catalog{};std::uint64_t catalogRevision=0;
    std::optional<item::PersistentItem> source;item::FishAction action{};
    entt::entity fire=entt::null;std::uint32_t fireVid=0;
    item::FishProcessingQuote quote;std::vector<item::FishingYield> yields;
    bool pending=false,attempted=false;Status status=Status::Ready;std::uint8_t error=255;
};
std::uint64_t Nonce(){std::uint64_t n=0;while(!n)if(RAND_bytes(reinterpret_cast<unsigned char*>(&n),8)!=1)return 0;return n;}
std::uint32_t Draw(std::uint32_t bound)
{if(!bound)return 0;const auto range=std::uint64_t(UINT32_MAX)+1,limit=range-range%bound;std::uint32_t n;do{if(RAND_bytes(reinterpret_cast<unsigned char*>(&n),4)!=1)return bound;}while(n>=limit);return n%bound;}
bool Admitted(entt::entity e)
{
    if(!g_registry.valid(e) || !ecs::PlayerRuntime::IsPC(e) || !g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e) ||
       CombatSystem::IsDead(e) || !InventorySystem::CanHandleItems(e) || ecs::SocialSystem::HasExchange(e) ||
       ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e) ||
       ecs::SocialSystem::GetMyShop(e)!=entt::null || ecs::SocialSystem::GetShopOwner(e)!=entt::null)return false;
    const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);if(flags && (flags->isObserverMode || flags->isStunned))return false;
    const auto* quest=g_registry.try_get<ecs::QuestPCState>(e);if(!quest || !quest->pc || quest->pc->IsRunning())return false;
    if(CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(e)))return false;
#ifdef ENABLE_NEWSTUFF
    if(g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e)))return false;
#endif
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    if(ecs::PlayerRuntime::GetGMLevel(e)>GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(e)<GM_IMPLEMENTOR)return false;
#endif
    const auto* d=ecs::PlayerRuntime::GetDesc(e);return d && d->IsPhase(PHASE_GAME);
}
bool Campfire(entt::entity e,entt::entity fire)
{
    if(!g_registry.valid(fire) || !ecs::PlayerRuntime::IsValid(fire) || ecs::PlayerRuntime::IsPC(fire) ||
       ecs::PlayerRuntime::GetRaceNum(fire)!=fishing::CAMPFIRE_MOB || CombatSystem::IsDead(fire) ||
       ecs::PlayerRuntime::GetMapIndex(e)!=ecs::PlayerRuntime::GetMapIndex(fire))return false;
    const auto x=std::abs(std::int64_t(ecs::PlayerRuntime::GetX(e))-ecs::PlayerRuntime::GetX(fire));
    const auto y=std::abs(std::int64_t(ecs::PlayerRuntime::GetY(e))-ecs::PlayerRuntime::GetY(fire));return std::max(x,y)+std::min(x,y)/2<500;
}
item::FishProcessingActor Actor(entt::entity e,entt::entity fire)
{return {static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetJob(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetSex(e)),Admitted(e),ecs::PlayerRuntime::GetPosition(e)==POS_FIGHTING,Campfire(e,fire)};}
entt::entity Current(std::uint32_t pid,std::uint64_t token)
{const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* l=e!=entt::null && g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;return l && l->token==token ? e : entt::null;}
void Reply(entt::entity e,Status status,std::uint8_t error=255)
{
    const auto c=store::CurrentCatalog();if(!g_registry.valid(e) || !c)return;auto* s=g_registry.try_get<TypedFishProcessingSession>(e);if(s){s->status=status;s->error=error;}
    const auto packet=item::EncodeFishProcessing(*c,status,error,s ? s->nonce : 0,s && s->source ? &*s->source : nullptr,s ? s->action : item::FishAction{},s ? s->fireVid : 0,s ? s->quote : item::FishProcessingQuote{},s && status==Status::Applied ? std::span<const item::FishingYield>(s->yields) : std::span<const item::FishingYield>{});
    SendTypedInventory(e);auto* d=ecs::PlayerRuntime::GetDesc(e);if(packet && d && d->IsPhase(PHASE_GAME))d->LargePacket(packet->data(),static_cast<int>(packet->size()));
}
}
bool TypedGatheringAdmitted(entt::entity e){return Admitted(e);}
bool HandleTypedFishProcessingPacket(entt::entity e,const char* bytes)
{
    if(static_cast<unsigned char>(bytes[1])>=4)return HandleTypedToolsPacket(e,bytes);
    static_assert(sizeof(TPacketCGTypedRecovery)==26);const auto r=*reinterpret_cast<const TPacketCGTypedRecovery*>(bytes);
    if(!g_registry.valid(e))return true;auto* s=g_registry.try_get<TypedFishProcessingSession>(e);if(s && s->pending){Reply(e,Status::Pending);return true;}
    const auto c=store::CurrentCatalog();if(!c)return true;
    if(r.action==1)
    {
        if(r.session || r.target || !r.source){Reply(e,Status::Invalid);return true;}
        if(!Admitted(e)){Reply(e,Status::Denied);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Busy);return true;}
        const auto& t=g_registry.get<ecs::TypedItems>(e).state;const auto source=std::ranges::find_if(t.items,[&](const auto& i){return i.id.Value()==r.source;});
        const auto* row=source==t.items.end() ? nullptr : std::get_if<item::ItemFishingRow>(&c->Find(source->key)->category);
        if(!row || !item::PlainInventoryItem(*source,t.fence.owner)){g_registry.remove<TypedFishProcessingSession>(e);Reply(e,Status::Invalid);return true;}
        TypedFishProcessingSession next;next.nonce=Nonce();if(!next.nonce || RAND_bytes(next.operation.data(),16)!=1 || std::ranges::all_of(next.operation,[](auto b){return b==0;})){Reply(e,Status::Failed);return true;}
        next.ownerSession=t.fence.sessionGeneration;next.source=*source;next.catalog=c->ContentHash();next.catalogRevision=c->Revision();next.quote.kind=row->kind;
        next.action=row->kind==item::FishingKind::DeadFish ? item::FishAction::Grill : item::FishAction::Use;
        if(next.action==item::FishAction::Grill)
        {
            ecs::ForEachAround(g_registry,e,[&](entt::entity fire){if(Campfire(e,fire) && (next.fire==entt::null || ecs::PlayerRuntime::GetPacketVID(fire)<next.fireVid)){next.fire=fire;next.fireVid=ecs::PlayerRuntime::GetPacketVID(fire);}});
        }
        auto quote=item::CheckFishProcessing(*c,t.fence.owner,t.items,{source->id,next.action,Actor(e,next.fire)});
        next.status=quote ? Status::Ready : Status::Denied;next.error=quote ? 255 : static_cast<std::uint8_t>(quote.error());if(quote)next.quote=std::move(*quote);
        g_registry.emplace_or_replace<TypedFishProcessingSession>(e,std::move(next));s=g_registry.try_get<TypedFishProcessingSession>(e);Reply(e,s->status,s->error);return true;
    }
    if(!s || !s->source || r.session!=s->nonce || r.source!=s->source->id.Value() || r.target){Reply(e,Status::Stale);return true;}
    if(r.action==3){g_registry.remove<TypedFishProcessingSession>(e);Reply(e,Status::Closed);return true;}
    if(r.action!=2){Reply(e,Status::Invalid);return true;}if(s->attempted){Reply(e,s->status,s->error);return true;}
    if(s->status!=Status::Ready){Reply(e,Status::Denied,s->error);return true;}
    if(!Admitted(e)){Reply(e,Status::Denied);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Busy);return true;}
    s->attempted=true;
    const auto& t=g_registry.get<ecs::TypedItems>(e).state;const auto found=std::ranges::find(t.items,s->source->id,&item::PersistentItem::id);
    if(t.fence.sessionGeneration!=s->ownerSession || c->Revision()!=s->catalogRevision || c->ContentHash()!=s->catalog || found==t.items.end() || *found!=*s->source){Reply(e,Status::Stale);return true;}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permit || !permit->Current()){Reply(e,Status::Denied);return true;}
    auto plan=item::MakeFishProcessingCommit(*c,s->operation,t.fence,t.storages,t.items,{s->source->id,s->action,Actor(e,s->fire)},store::PeekInstanceIds(item::MaxItemCommitItems),Draw);
    if(!plan){Reply(e,Status::Denied,static_cast<std::uint8_t>(plan.error()));return true;}
    const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=g_registry.get<ecs::TypedItemLogin>(e).token;const auto owner=t.fence;
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));s->pending=true;s->yields=plan->yields;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    store::SpendInstanceIds(std::ranges::count_if(plan->commit.items,[](const auto& i){return !i.before;}));
    const bool queued=store::RequestCommit(owner.owner,owner.sessionGeneration,std::move(plan->commit),[pid,token,held](store::CommitOutcome out)
    {
        const auto e=Current(pid,token);if(e==entt::null)return;
        if(!out.state){LOG_ERROR("TypedFishProcessing: authoritative reload failed pid {}",pid);if(auto* d=ecs::PlayerRuntime::GetDesc(e))d->SetPhase(PHASE_CLOSE);return;}
        g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*out.state));g_registry.remove<ecs::TypedItemBusy>(e);
        if(auto* s=g_registry.try_get<TypedFishProcessingSession>(e)){s->pending=false;if(!out.write)s->yields.clear();}
        Reply(e,out.write ? Status::Applied : Status::Failed);PumpTypedItemGrants(e);
    });
    if(!queued){g_registry.remove<ecs::TypedItemBusy>(e);s->pending=false;s->yields.clear();Reply(e,Status::Failed);}return true;
}
