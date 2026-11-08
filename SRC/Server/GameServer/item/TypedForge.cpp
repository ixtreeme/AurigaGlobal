#include "stdafx.h"
#include "TypedForge.hpp"
#include "TypedInventorySync.hpp"
#include "TypedRecovery.hpp"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/components/status_components.hpp"
#include "../quest/questpc.h"
#include "../quest/questmanager.h"
#include "../network/desc.h"
#include "../entity/char_manager.h"
#include "../core/utils.h"
#include "refine.h"
#include <item/ForgeRuntime.hpp>
#include <openssl/rand.h>
namespace {
namespace item=auriga::item;namespace store=item::runtime;
#pragma pack(push,1)
struct Request {std::uint8_t header,action;std::uint64_t nonce,target,scroll;std::array<std::uint64_t,3> boosters;std::uint32_t recipe,npc;};
#pragma pack(pop)
static_assert(sizeof(Request)==58);
enum class Status:std::uint8_t{Ready=0,Applied=1,Denied=2,Stale=3,Invalid=4,Pending=5,Closed=6,Failed=7,AttemptFailed=8};
struct TypedForgeSession {Request request{};std::uint64_t nonce=0;std::array<std::uint8_t,16> operation{};std::optional<item::ForgeCommit> quote;bool attempted=false,pending=false;Status status=Status::Invalid;};
entt::entity Current(std::uint32_t pid,std::uint64_t token){const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* login=g_registry.valid(e)?g_registry.try_get<ecs::TypedItemLogin>(e):nullptr;return login && login->token==token?e:entt::null;}
bool Admitted(entt::entity e){if(!g_registry.valid(e) || !ecs::PlayerRuntime::IsPC(e) || CombatSystem::IsDead(e) || !InventorySystem::CanHandleItems(e) || ecs::SocialSystem::HasExchange(e) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e) || ecs::SocialSystem::GetMyShop(e)!=entt::null || ecs::SocialSystem::GetShopOwner(e)!=entt::null || !g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e))return false;const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);if(flags && (flags->isObserverMode || flags->isStunned))return false;const auto* quest=g_registry.try_get<ecs::QuestPCState>(e);if(!quest || !quest->pc || quest->pc->IsRunning())return false;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
if(ecs::PlayerRuntime::GetGMLevel(e)>GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(e)<GM_IMPLEMENTOR)return false;
#endif
    const auto updated=quest::CQuestManager::instance().GetEventFlag("update_refine_time");if(updated && get_global_time()<updated+300)return false;auto* d=ecs::PlayerRuntime::GetDesc(e);return d && d->IsPhase(PHASE_GAME);
}
bool Smith(entt::entity e,std::uint32_t vid){const auto n=CHARACTER_MANAGER::instance().FindEntity(vid);if(!g_registry.valid(n) || ecs::PlayerRuntime::IsPC(n) || ecs::PlayerRuntime::GetMapIndex(e)!=ecs::PlayerRuntime::GetMapIndex(n))return false;const auto race=ecs::PlayerRuntime::GetRaceNum(n);if(race!=BLACKSMITH_MOB && race!=BLACKSMITH2_MOB)return false;const auto dx=std::abs(std::int64_t(ecs::PlayerRuntime::GetX(e))-ecs::PlayerRuntime::GetX(n)),dy=std::abs(std::int64_t(ecs::PlayerRuntime::GetY(e))-ecs::PlayerRuntime::GetY(n));return dx<=2000 && dy<=2000 && DISTANCE_APPROX(static_cast<int>(dx),static_cast<int>(dy))<=2000;}
std::expected<item::ForgeCommit,item::ItemPersistenceError> Plan(entt::entity e,const TypedForgeSession& s,std::optional<std::uint16_t> roll={}){const auto c=store::CurrentCatalog();const auto* t=g_registry.try_get<ecs::TypedItems>(e);const auto id=item::ItemInstanceId::Create(s.request.target);if(!c || !t || !id)return std::unexpected(item::ItemPersistenceError::InvalidItem);const auto wallet=std::ranges::find(t->state.wallets,item::WalletCurrency::Gold,&item::ItemWallet::currency);if(wallet==t->state.wallets.end())return std::unexpected(item::ItemPersistenceError::InvalidWallet);item::ForgeContext context;context.admitted=Admitted(e);context.alive=!CombatSystem::IsDead(e);context.npcInRange=Smith(e,s.request.npc);context.actor={static_cast<std::uint16_t>(std::max(0,ecs::PointSystem::GetLevel(e))),ecs::PlayerRuntime::GetJob(e),ecs::PlayerRuntime::GetSex(e)};context.actor.inCombat=TypedItemEffectFacts(e).actor.inCombat;return item::MakeForgeCommit(*c,s.operation,t->state.fence,t->state.storages,t->state.items,*wallet,{*id,s.request.scroll,s.request.boosters,s.request.recipe},context,roll);}
bool Same(const Request& a,const Request& b){return a.target==b.target && a.scroll==b.scroll && a.boosters==b.boosters && a.recipe==b.recipe && a.npc==b.npc;}
bool Fresh(const item::ItemCommit& quote,const item::persistence::LoadedItemOwner& state){for(const auto& ch:quote.items){const auto i=std::ranges::find(state.items,ch.before->id,&item::PersistentItem::id);if(i==state.items.end() || *i!=*ch.before)return false;}for(const auto& w:quote.wallets){const auto i=std::ranges::find(state.wallets,w.before.currency,&item::ItemWallet::currency);if(i==state.wallets.end() || *i!=w.before)return false;}return true;}
void Reply(entt::entity e,Status status,const Request* echo=nullptr){if(!g_registry.valid(e))return;const auto* s=g_registry.try_get<TypedForgeSession>(e);const Request empty{};const auto& r=echo?*echo:s?s->request:empty;const auto terms=s && s->quote && !echo?s->quote->terms:item::ForgeTerms{};std::vector<std::uint8_t> b;const auto put=[&](std::uint64_t v,unsigned n){for(unsigned j=0;j<n;++j)b.push_back(static_cast<std::uint8_t>(v>>(j*8)));};put(212,1);put(80,4);put(1,1);put(unsigned(status),1);put(s && !echo?s->nonce:r.nonce,8);put(r.target,8);put(r.scroll,8);for(auto id:r.boosters)put(id,8);put(r.recipe,4);put(r.npc,4);put(terms.chanceBp,2);put(unsigned(terms.failure),1);put(terms.failureLevel,2);put(terms.gold,8);put(s && s->quote && !echo?s->quote->commit.items[0].before->refinementLevel:0,2);put(terms.targetLevel,2);assert(b.size()==80);SendTypedInventory(e);if(auto* d=ecs::PlayerRuntime::GetDesc(e))d->Packet(b.data(),static_cast<int>(b.size()));}
std::optional<std::uint16_t> Roll(std::uint16_t chance){if(!chance || chance==10000)return 0;std::uint32_t v;const auto threshold=static_cast<std::uint32_t>((std::uint64_t{1}<<32)%10000);do{if(RAND_bytes(reinterpret_cast<unsigned char*>(&v),4)!=1)return {};}while(v<threshold);return static_cast<std::uint16_t>(v%10000+1);}
}
bool HandleTypedForgePacket(entt::entity e,const char* bytes){const auto r=*reinterpret_cast<const Request*>(bytes);if(!g_registry.valid(e))return true;auto* s=g_registry.try_get<TypedForgeSession>(e);if(s && s->pending){Reply(e,Status::Pending);return true;}if(r.action==3){g_registry.remove<TypedForgeSession>(e);Reply(e,Status::Closed,&r);return true;}
    if(r.action==2 && s && s->attempted && s->nonce==r.nonce && Same(s->request,r)){Reply(e,s->status);return true;}
    if(!Admitted(e) || g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Denied,&r);return true;}
    if(r.action==1){if(r.nonce || !r.target || !r.recipe){Reply(e,Status::Invalid,&r);return true;}TypedForgeSession next;next.request=r;if(RAND_bytes(reinterpret_cast<unsigned char*>(&next.nonce),8)!=1 || !next.nonce || RAND_bytes(next.operation.data(),16)!=1){Reply(e,Status::Failed,&r);return true;}auto p=Plan(e,next);if(p){next.quote=std::move(*p);next.status=Status::Ready;}g_registry.emplace_or_replace<TypedForgeSession>(e,std::move(next));Reply(e,g_registry.get<TypedForgeSession>(e).status);return true;}
    if(r.action!=2 || !s || s->nonce!=r.nonce || !Same(s->request,r)){Reply(e,Status::Stale,&r);return true;}if(s->attempted){Reply(e,s->status);return true;}s->attempted=true;
    auto* t=g_registry.try_get<ecs::TypedItems>(e);if(!s->quote || !t || !Fresh(s->quote->commit,t->state)){s->status=Status::Stale;Reply(e,s->status);return true;}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permit || !permit->Current()){s->status=Status::Denied;Reply(e,s->status);return true;}
    s=g_registry.try_get<TypedForgeSession>(e);t=g_registry.try_get<ecs::TypedItems>(e);if(!s || !t || s->nonce!=r.nonce || !Admitted(e) || !Fresh(s->quote->commit,t->state)){Reply(e,Status::Stale,&r);return true;}
    const auto probe=Plan(e,*s);if(!probe || probe->terms!=s->quote->terms){s->status=Status::Stale;Reply(e,s->status);return true;}const auto roll=Roll(probe->terms.chanceBp);if(!roll){s->status=Status::Failed;Reply(e,s->status);return true;}auto planned=Plan(e,*s,*roll);if(!planned){s->status=Status::Invalid;Reply(e,s->status);return true;}
    const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=g_registry.get<ecs::TypedItemLogin>(e).token;const auto nonce=s->nonce;const auto f=t->state.fence;const bool success=planned->success;auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));s->pending=true;s->status=Status::Pending;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    if(!store::RequestCommit(f.owner,f.sessionGeneration,std::move(planned->commit),[pid,token,nonce,success,held](store::CommitOutcome result){const auto e=Current(pid,token);if(e==entt::null)return;if(result.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*result.state));g_registry.remove<ecs::TypedItemBusy>(e);auto* s=g_registry.try_get<TypedForgeSession>(e);if(s && s->nonce==nonce){s->pending=false;s->status=result.write?(success?Status::Applied:Status::AttemptFailed):Status::Failed;Reply(e,s->status);}else SendTypedInventory(e);PumpTypedItemGrants(e);})){g_registry.remove<ecs::TypedItemBusy>(e);s=g_registry.try_get<TypedForgeSession>(e);if(s){s->pending=false;s->status=Status::Failed;}Reply(e,Status::Failed);}return true;
}
