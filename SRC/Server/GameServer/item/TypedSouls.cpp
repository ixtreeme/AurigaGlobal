#include "stdafx.h"
#include "TypedSouls.hpp"
#include "TypedSoulClock.hpp"
#include "TypedInventorySync.hpp"
#include "TypedRecovery.hpp"
#include "TypedDrops.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../quest/questpc.h"
#include "ItemCatalogRuntime.hpp"
#include <item/SoulRuntime.hpp>
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/components/status_components.hpp"
#include "../network/desc.h"
#include "../entity/char_manager.h"
#include <openssl/rand.h>

namespace {
namespace item=auriga::item;namespace store=item::runtime;
#pragma pack(push,1)
struct Request {std::uint8_t header,action,operation;std::uint64_t nonce,source,target;std::uint8_t selection;};
#pragma pack(pop)
static_assert(sizeof(Request)==28);
static_assert(HEADER_CG_TYPED_SOUL!=HEADER_CG_CLIENT_VERSION2 &&
    HEADER_CG_TYPED_SOUL!=HEADER_CG_HANDSHAKE && HEADER_CG_TYPED_SOUL!=HEADER_CG_TIME_SYNC);
enum class Status : std::uint8_t {Ready=0,Applied=1,Denied=2,Stale=3,Invalid=4,Pending=5,Closed=6,Failed=7,AttemptFailed=8};
// EnTT hashes component type names: never share an anonymous Session name with shops.
struct TypedSoulSession {std::uint64_t nonce=0;Request request{};std::optional<item::PersistentItem> source,target;std::array<std::uint8_t,16> operation{};bool attempted=false,pending=false;Status status=Status::Invalid;};
entt::entity Current(std::uint32_t pid,std::uint64_t token){const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* login=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;return login && login->token==token ? e : entt::null;}
bool Admitted(entt::entity e){if(!g_registry.valid(e) || !ecs::PlayerRuntime::IsPC(e) || CombatSystem::IsDead(e) || !InventorySystem::CanHandleItems(e) || ecs::SocialSystem::HasExchange(e) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e) || ecs::SocialSystem::GetMyShop(e)!=entt::null || ecs::SocialSystem::GetShopOwner(e)!=entt::null || !g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e))return false;const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);if(flags && (flags->isObserverMode || flags->isStunned))return false;const auto* quest=g_registry.try_get<ecs::QuestPCState>(e);if(!quest || !quest->pc || quest->pc->IsRunning())return false;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
if(ecs::PlayerRuntime::GetGMLevel(e)>GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(e)<GM_IMPLEMENTOR)return false;
#endif
auto* d=ecs::PlayerRuntime::GetDesc(e);return d && d->IsPhase(PHASE_GAME);}
item::EquipContext Actor(entt::entity e){item::EquipContext a{static_cast<std::uint16_t>(std::max(0,ecs::PointSystem::GetLevel(e))),ecs::PlayerRuntime::GetJob(e),ecs::PlayerRuntime::GetSex(e)};a.inCombat=TypedItemEffectFacts(e).actor.inCombat;a.nowUtcMs=TypedItemEffectNow();return a;}
std::uint64_t Elapsed(entt::entity e,const Request& r){return TypedSoulElapsed(e,r.operation==4 ? r.target : r.source);}
std::optional<std::uint32_t> Random(std::uint32_t maximum){std::uint32_t value;if(maximum==UINT32_MAX){if(RAND_bytes(reinterpret_cast<unsigned char*>(&value),4)!=1)return {};return value;}const auto bound=maximum+1;const auto threshold=static_cast<std::uint32_t>((std::uint64_t{1}<<32)%bound);do{if(RAND_bytes(reinterpret_cast<unsigned char*>(&value),4)!=1)return {};}while(value<threshold);return value%bound;}
void Reply(entt::entity e,Status status,const Request* request=nullptr){if(!g_registry.valid(e))return;const auto* s=g_registry.try_get<TypedSoulSession>(e);std::vector<std::uint8_t> b;const auto put=[&](std::uint64_t v,unsigned n){for(unsigned j=0;j<n;++j)b.push_back(static_cast<std::uint8_t>(v>>(8*j)));};put(219,1);put(35,4);put(1,1);put(static_cast<unsigned>(status),1);put(request ? request->nonce : s ? s->nonce : 0,8);put(request ? request->source : s ? s->request.source : 0,8);put(request ? request->target : s ? s->request.target : 0,8);put(request ? std::min<unsigned>(request->operation,4) : s ? s->request.operation : 0,1);put(request ? std::min<unsigned>(request->selection,1) : s ? s->request.selection : 0,1);put(0,2);SendTypedInventory(e);auto* d=ecs::PlayerRuntime::GetDesc(e);if(d && d->IsPhase(PHASE_GAME))d->Packet(b.data(),b.size());}
std::expected<item::SoulCommit,item::ItemPersistenceError> Plan(entt::entity e,const TypedSoulSession& s,const std::function<std::optional<std::uint32_t>(std::uint32_t)>& random){const auto c=store::CurrentCatalog();const auto* t=g_registry.try_get<ecs::TypedItems>(e);if(!c || !t || s.request.selection || s.request.operation==3)return std::unexpected(item::ItemPersistenceError::InvalidItem);const auto core=item::ItemInstanceId::Create(s.request.operation==4 ? s.request.target : s.request.source);if(!core)return std::unexpected(item::ItemPersistenceError::InvalidItem);std::uint16_t roll=0;if(s.request.operation==4){const auto source=std::ranges::find(t->state.items,s.request.source,[](const auto& i){return i.id.Value();});const auto* row=source!=t->state.items.end() ? std::get_if<item::ItemSoulRow>(&c->Find(source->key)->category) : nullptr;if(!row || row->kind!=item::SoulKind::RefinementScroll)return std::unexpected(item::ItemPersistenceError::InvalidItem);if(row->refineChanceBp && row->refineChanceBp!=10000){const auto v=random(9999);if(!v)return std::unexpected(item::ItemPersistenceError::InvalidChange);roll=static_cast<std::uint16_t>(*v+1);}}item::SoulContext context;context.actor=Actor(e);return item::MakeSoulCommit(*c,s.operation,t->state.fence,t->state.storages,t->state.items,*core,s.request.operation==4 ? std::optional{item::ItemInstanceId::Create(s.request.source).value()} : std::nullopt,s.request.operation==4 ? item::SoulAction::Refine : static_cast<item::SoulAction>(s.request.operation),Elapsed(e,s.request),roll,context);}
bool SameQuote(const item::PersistentItem& old,const item::PersistentItem& current){auto expected=old;expected.revision=current.revision;if(old.soul && current.soul && old.soul->active==current.soul->active && old.soul->strikes==current.soul->strikes && old.soul->charge<=current.soul->charge)expected.soul=current.soul;return expected==current;}

}
bool HandleTypedSoulsPacket(entt::entity e,const char* bytes){
    const auto r=*reinterpret_cast<const Request*>(bytes);if(!g_registry.valid(e))return true;auto* session=g_registry.try_get<TypedSoulSession>(e);
    if(session && session->pending){Reply(e,Status::Pending);return true;}
    if(r.action==3){g_registry.remove<TypedSoulSession>(e);Reply(e,Status::Closed);return true;}
    // A completed request remains replayable even if its tool was consumed or
    // another clock has since entered item admission. This only returns a result.
    if(r.action==2 && session && session->attempted && session->nonce==r.nonce && session->request.source==r.source && session->request.target==r.target && session->request.operation==r.operation && session->request.selection==r.selection){Reply(e,session->status);return true;}
    if(!Admitted(e) || g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Denied,&r);return true;}
    auto* t=g_registry.try_get<ecs::TypedItems>(e);
    if(r.action==1){
        if(r.nonce || r.operation<1 || r.operation>4 || !r.source || (r.operation==4)!=bool(r.target)){Reply(e,Status::Invalid,&r);return true;}
        TypedSoulSession next;next.request=r;next.nonce=0;if(RAND_bytes(reinterpret_cast<unsigned char*>(&next.nonce),8)!=1 || !next.nonce || RAND_bytes(next.operation.data(),16)!=1){Reply(e,Status::Failed,&r);return true;}
        const auto source=std::ranges::find(t->state.items,r.source,[](const auto& i){return i.id.Value();});const auto target=std::ranges::find(t->state.items,r.target,[](const auto& i){return i.id.Value();});if(source==t->state.items.end()){Reply(e,Status::Invalid,&r);return true;}next.source=*source;if(target!=t->state.items.end())next.target=*target;
        const auto planned=Plan(e,next,[](std::uint32_t){return std::optional<std::uint32_t>{0};});next.status=planned ? Status::Ready : Status::Invalid;g_registry.emplace_or_replace<TypedSoulSession>(e,std::move(next));Reply(e,g_registry.get<TypedSoulSession>(e).status);return true;
    }
    if(r.action!=2 || !session || session->nonce!=r.nonce || session->request.source!=r.source || session->request.target!=r.target || session->request.operation!=r.operation || session->request.selection!=r.selection){Reply(e,Status::Stale,&r);return true;}
    if(session->attempted){Reply(e,session->status);return true;}session->attempted=true;
    const auto source=std::ranges::find(t->state.items,r.source,[](const auto& i){return i.id.Value();});const auto target=std::ranges::find(t->state.items,r.target,[](const auto& i){return i.id.Value();});
    // Rebase unrelated clocks only; a changing source or target needs a fresh preview.
    if(!session->source || source==t->state.items.end() || !SameQuote(*session->source,*source) || (r.target && (!session->target || target==t->state.items.end() || !SameQuote(*session->target,*target)))){session->status=Status::Stale;Reply(e,session->status);return true;}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permit || !permit->Current()){session->status=Status::Denied;Reply(e,session->status);return true;}
    session=g_registry.try_get<TypedSoulSession>(e);t=g_registry.try_get<ecs::TypedItems>(e);if(!session || session->nonce!=r.nonce || !t || !Admitted(e)){Reply(e,Status::Denied,&r);return true;}
    const auto probe=Plan(e,*session,[](std::uint32_t){return std::optional<std::uint32_t>{0};});if(!probe){session->status=Status::Invalid;Reply(e,session->status);return true;}auto planned=Plan(e,*session,Random);if(!planned){session->status=Status::Invalid;Reply(e,session->status);return true;}
    const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=g_registry.get<ecs::TypedItemLogin>(e).token;const auto nonce=session->nonce;auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));session->pending=true;session->status=Status::Pending;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    const bool succeeded=planned->success;const auto fence=t->state.fence;const auto complete=[pid,token,nonce,held,succeeded](store::CommitOutcome result){const auto e=Current(pid,token);if(e==entt::null)return;if(result.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*result.state));g_registry.remove<ecs::TypedItemBusy>(e);auto* s=g_registry.try_get<TypedSoulSession>(e);if(s && s->nonce==nonce){s->pending=false;s->status=result.write ? (succeeded ? Status::Applied : Status::AttemptFailed) : Status::Failed;Reply(e,s->status);}else SendTypedInventory(e);PumpTypedItemGrants(e);};
    if(!store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(planned->commit),complete)){g_registry.remove<ecs::TypedItemBusy>(e);session=g_registry.try_get<TypedSoulSession>(e);if(session){session->pending=false;session->status=Status::Failed;}Reply(e,Status::Failed);}return true;
}
