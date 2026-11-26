#include "stdafx.h"
#include "TypedPrivateShops.hpp"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include "TypedInventorySync.hpp"
#include "TypedRecovery.hpp"
#include "TypedDrops.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/social_components.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PlayerItemActions.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/ViewSystem.hpp"
#include "../social/shop.h"
#include "../quest/questmanager.h"
#include "../quest/questpc.h"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include "banword.h"
#include <PrivateShopsSql.hpp>
#include <item/PrivateShopWire.hpp>
#include <openssl/rand.h>
#include <Core/Logging.hpp>
bool IS_BOTARYABLE_ZONE(int);
namespace {
namespace it=auriga::item;namespace rt=it::runtime;using Status=it::PrivateShopStatus;
struct Market {it::PrivateShopHeader header;entt::entity marker=entt::null;bool closing=false,pending=false;std::uint64_t retryAt=0;};
struct Window {it::PrivateShopSnapshot snapshot;entt::entity seller=entt::null;std::optional<it::ItemCommit> quote;std::uint64_t expires=0;bool attempted=false,pending=false;};
std::uint64_t Now(){return TypedItemEffectNow();}
std::array<std::uint8_t,16> Op(){std::array<std::uint8_t,16> p{};if(RAND_bytes(p.data(),p.size())!=1)p={};return p;}
std::uint64_t Nonce(){std::uint64_t v=0;if(RAND_bytes(reinterpret_cast<unsigned char*>(&v),8)!=1)return 0;return v;}
entt::entity Current(std::uint32_t pid,std::uint64_t token){const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* login=g_registry.valid(e)?g_registry.try_get<ecs::TypedItemLogin>(e):nullptr;return login && login->token==token?e:entt::null;}
bool Live(entt::entity e){auto* d=g_registry.valid(e)?ecs::PlayerRuntime::GetDesc(e):nullptr;return d && d->IsPhase(PHASE_GAME) && ecs::PlayerRuntime::IsPC(e) && !CombatSystem::IsDead(e) && !ecs::PlayerRuntime::IsObserverMode(e) && g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e);}
bool Admitted(entt::entity e){
 if(!Live(e) || !PlayerItemActions::CanHandleItems(e) || ecs::SocialSystem::HasExchange(e) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e))return false;
 const auto* q=g_registry.try_get<ecs::QuestPCState>(e);const auto* f=g_registry.try_get<ecs::StatusFlags>(e);if(!q || !q->pc || q->pc->IsRunning() || f && f->isStunned)return false;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
 if(ecs::PlayerRuntime::GetGMLevel(e)>GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(e)<GM_IMPLEMENTOR)return false;
#endif
 const auto* w=g_registry.try_get<Window>(e);return ecs::SocialSystem::GetShopOwner(e)==entt::null || w && ecs::SocialSystem::GetShopOwner(e)==w->seller;
}
bool OpenZone(entt::entity e){
 if(!IS_BOTARYABLE_ZONE(ecs::PlayerRuntime::GetMapIndex(e)))return false;
#ifndef ENABLE_OPEN_SHOP_WITH_ARMOR
 if(ecs::PlayerRuntime::GetPart(e,PART_MAIN)>2)return false;
#endif
 return true;
}
bool Range(entt::entity viewer,entt::entity seller){if(!Live(viewer)||!Live(seller)||viewer==seller || ecs::PlayerRuntime::GetMapIndex(viewer)!=ecs::PlayerRuntime::GetMapIndex(seller))return false;const auto dx=std::abs(std::int64_t(ecs::PlayerRuntime::GetX(viewer))-ecs::PlayerRuntime::GetX(seller)),dy=std::abs(std::int64_t(ecs::PlayerRuntime::GetY(viewer))-ecs::PlayerRuntime::GetY(seller));return std::max(dx,dy)+std::min(dx,dy)/2<1000;}
const it::ItemWallet* Gold(const it::persistence::LoadedItemOwner& s){for(const auto& w:s.wallets)if(w.owner==s.fence.owner && w.currency==it::WalletCurrency::Gold)return &w;return nullptr;}
Status Refuse(it::PrivateShopError e){switch(e){case it::PrivateShopError::Expired:return Status::Expired;case it::PrivateShopError::NoMoney:return Status::NoMoney;case it::PrivateShopError::NoRoom:return Status::NoRoom;case it::PrivateShopError::WalletFull:return Status::WalletFull;case it::PrivateShopError::Denied:return Status::Denied;case it::PrivateShopError::Unavailable:return Status::Stale;default:return Status::Invalid;}}
std::uint8_t Tax(){return static_cast<std::uint8_t>(std::clamp<std::int64_t>(quest::CQuestManager::instance().GetEventFlag("personal_shop"),0,100));}
void Reply(entt::entity e,Status result){
 if(!g_registry.valid(e))return;auto* w=g_registry.try_get<Window>(e);const auto* t=g_registry.try_get<ecs::TypedItems>(e);auto* d=ecs::PlayerRuntime::GetDesc(e);const auto c=rt::CurrentCatalog();if(!w || !t || !d || !d->IsPhase(PHASE_GAME) || !c)return;
 w->snapshot.status=result;auto s=w->snapshot;s.nowAtMs=Now();s.revision=t->state.fence.revision;s.balance=Gold(t->state)?Gold(t->state)->balance:0;s.cap=GOLD_MAX;s.taxPercent=w->quote && w->quote->privateShop && w->quote->privateShop->kind==it::PrivateShopMutationKind::Buy && (result==Status::Preview || result==Status::Pending || result==Status::Applied)?w->quote->privateShop->taxPercent:Tax();
 const auto* source=&t->state;
 if(s.view==2){const auto* seller=g_registry.valid(w->seller)?g_registry.try_get<ecs::TypedItems>(w->seller):nullptr;const auto* market=g_registry.valid(w->seller)?g_registry.try_get<Market>(w->seller):nullptr;if(!seller || !market || market->closing){s.status=Status::Closed;}else{source=&seller->state;s.sign=market->header.sign;s.remember=market->header.rememberPrices;}}
 else if(const auto* m=g_registry.try_get<Market>(e)){s.sign=m->header.sign;s.remember=m->header.rememberPrices;}
 std::vector<it::PersistentItem> items;for(const auto& i:source->items)if(s.view==0 || i.privateShop)items.push_back(i);
 const auto packet=it::EncodePrivateShopSnapshot(*c,s,source->storages,items);SendTypedInventory(e);d=g_registry.valid(e)?ecs::PlayerRuntime::GetDesc(e):nullptr;if(packet && d && d->IsPhase(PHASE_GAME))d->LargePacket(packet->data(),static_cast<int>(packet->size()));
}
void CloseWindow(entt::entity e){if(!g_registry.valid(e))return;const auto* w=g_registry.try_get<Window>(e);if(w && w->pending)return;if(w && ecs::SocialSystem::GetShopOwner(e)==w->seller)ecs::SocialSystem::SetShopOwner(e,entt::null);Reply(e,Status::Closed);g_registry.remove<Window>(e);}
void Sign(entt::entity e,const std::string& text){if(!g_registry.valid(e))return;g_registry.get_or_emplace<ecs::ShopState>(e).shopSign=text;TPacketGCShopSign p{};p.bHeader=HEADER_GC_SHOP_SIGN;p.dwVID=ecs::PlayerRuntime::GetPacketVID(e);strlcpy(p.szSign,text.c_str(),sizeof(p.szSign));ecs::ViewSystem::PacketView(e,&p,sizeof(p));}
void RemoveMarket(entt::entity e){if(!g_registry.valid(e))return;auto* m=g_registry.try_get<Market>(e);if(!m)return;const auto marker=m->marker;g_registry.remove<Market>(e);if(!g_registry.valid(e))return;g_registry.get_or_emplace<ecs::ShopState>(e).myShop=entt::null;Sign(e,"");if(g_registry.valid(marker))g_registry.destroy(marker);if(g_registry.valid(e) && AffectSystem::GetPolymorphVnum(e)==30000)AffectSystem::SetPolymorph(e,0,true);
 std::vector<entt::entity> guests;for(auto viewer:g_registry.view<Window>())if(g_registry.get<Window>(viewer).seller==e)guests.push_back(viewer);for(auto viewer:guests)CloseWindow(viewer);
}
void ShowMarket(entt::entity e,const it::PrivateShopHeader& h){if(!Live(e))return;const auto marker=g_registry.create();g_registry.emplace<ecs::ShopData>(marker).owner=e;g_registry.emplace_or_replace<Market>(e,h,marker);if(!g_registry.valid(e)){g_registry.destroy(marker);return;}g_registry.get_or_emplace<ecs::ShopState>(e).myShop=marker;Sign(e,h.sign);
 // The explicit clear request survives even a very quick shop close. Merely
 // blocking polymorph while the market is visible would revive the old effect.
 RequestTypedPolymorphClear(e);AffectSystem::RemoveAffect(e,AFFECT_POLYMORPH);
 MountSystem::SummonHorse(e,false,true);AffectSystem::RemoveAffect(e,AFFECT_MOUNT);AffectSystem::RemoveAffect(e,AFFECT_MOUNT_BONUS);AffectSystem::SetPolymorph(e,30000,true);
}
void Notify(entt::entity seller){std::vector<entt::entity> guests;for(auto e:g_registry.view<Window>())if(e==seller || g_registry.get<Window>(e).seller==seller)guests.push_back(e);for(auto e:guests)if(g_registry.valid(e))Reply(e,Status::Ready);}
bool QueueClose(entt::entity e){auto* m=g_registry.valid(e)?g_registry.try_get<Market>(e):nullptr;const auto* t=g_registry.valid(e)?g_registry.try_get<ecs::TypedItems>(e):nullptr;const auto* l=g_registry.valid(e)?g_registry.try_get<ecs::TypedItemLogin>(e):nullptr;if(!m || !t || !l)return false;if(m->pending)return true;m->closing=m->pending=true;const auto owner=t->state.fence.owner;const auto session=t->state.fence.sessionGeneration;const auto token=l->token;
 const bool submitted=rt::ItemStore::Instance().Submit([owner,session,token](CAsyncSQL& sql,const it::ItemCatalog& c)->rt::ItemStore::Completion{const auto closed=it::persistence::RecoverPrivateShop(sql,c,owner,session);auto state=it::persistence::LoadItemOwner(sql,c,owner,session);return [owner,token,closed,state=std::move(state)]()mutable{const auto e=Current(static_cast<std::uint32_t>(owner.id),token);if(e==entt::null)return;auto* m=g_registry.try_get<Market>(e);if(!m)return;m->pending=false;m->retryAt=Now()+1000;if(closed && state){if(!g_registry.all_of<ecs::TypedItemBusy>(e))g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*state));else RefreshRemoteTypedItemOwner(static_cast<std::uint32_t>(owner.id),state->fence.sessionGeneration,state->fence.revision);RemoveMarket(e);SendTypedInventory(e);Reply(e,Status::Closed);}else LOG_ERROR("TypedPrivateShop: closing pid {} awaits durable recovery",owner.id);};});
 if(!submitted){if(auto* current=g_registry.try_get<Market>(e))current->pending=false;}return submitted;
}
}
bool HasTypedPrivateShop(entt::entity e){return g_registry.valid(e) && g_registry.all_of<Market>(e);}
bool CloseTypedPrivateShop(entt::entity e){if(!HasTypedPrivateShop(e))return false;QueueClose(e);return true;}
bool OpenTypedPrivateShopComposer(entt::entity e,it::ItemInstanceId permit){
 if(!Admitted(e) || !OpenZone(e) || g_registry.all_of<ecs::TypedItemBusy>(e))return false;const auto c=rt::CurrentCatalog();const auto& state=g_registry.get<ecs::TypedItems>(e).state;const auto source=std::ranges::find(state.items,permit,&it::PersistentItem::id);if(!c || source==state.items.end() || !it::PlainInventoryItem(*source,state.fence.owner) || it::PrivateShopPermit(*c,source->key)==it::PrivateShopPermitKind::None)return false;
 const auto* entry=c->Find(source->key);const auto* policy=entry?c->FindUsePolicy(entry->base.usePolicy):nullptr;const auto facts=TypedRecoveryAdmission(e,{},source->count);if(!policy || !(policy->allowedClasses&(1u<<ecs::PlayerRuntime::GetJob(e))) || !(policy->allowedSexes&(1u<<ecs::PlayerRuntime::GetSex(e))) || !it::PlanQuestItemUse(*c,{source->key,source->count,static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(e)),static_cast<it::QuestItemContext>(facts.context),false,facts.inCombat,true,true}))return false;
 CloseWindow(e);Window w;w.snapshot.view=0;w.snapshot.subject=permit.Value();w.snapshot.owner=static_cast<std::uint32_t>(state.fence.owner.id);w.expires=Nonce();w.snapshot.remember=std::ranges::any_of(state.items,[&](const auto& i){return it::PrivateShopPermit(*c,i.key)==it::PrivateShopPermitKind::Silk && it::PlainInventoryItem(i,state.fence.owner) && i.shopPermitDeadline && *i.shopPermitDeadline>Now();});for(const auto& i:state.items)if(it::PrivateShopListable(*c,i,state.fence.owner,Now()))w.snapshot.eligible.push_back(i.id.Value());
 const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=g_registry.get<ecs::TypedItemLogin>(e).token;const auto read=w.expires;const auto remember=w.snapshot.remember;g_registry.emplace_or_replace<Window>(e,std::move(w));if(!remember){Reply(e,Status::Ready);return true;}
 if(!rt::ItemStore::Instance().Submit([pid,token,read](CAsyncSQL& sql,const it::ItemCatalog& c)->rt::ItemStore::Completion{auto prices=it::persistence::LoadPrivateShopPrices(sql,c,pid);return [pid,token,read,prices=std::move(prices)](){const auto e=Current(pid,token);const auto* current=e!=entt::null?g_registry.try_get<Window>(e):nullptr;if(!current || current->expires!=read || current->snapshot.view!=0)return;auto& w=g_registry.get<Window>(e);if(prices)for(const auto& p:*prices)w.snapshot.prices.push_back({p.key,p.level,p.unit});Reply(e,prices?Status::Ready:Status::Failed);};})){Reply(e,Status::Failed);}return true;
}
bool TryOpenTypedPrivateShop(entt::entity e,entt::entity seller){if(!HasTypedPrivateShop(seller))return false;if(e==seller && Live(e)){CloseWindow(e);Window owner;owner.snapshot.view=1;owner.snapshot.owner=ecs::PlayerRuntime::GetPlayerID(e);g_registry.emplace_or_replace<Window>(e,std::move(owner));Reply(e,Status::Ready);return true;}const auto* m=g_registry.try_get<Market>(seller);if(!m || m->closing || !Admitted(e) || !Range(e,seller) || g_registry.all_of<ecs::TypedItemBusy>(e))return true;CloseWindow(e);Window w;w.seller=seller;w.snapshot.view=2;w.snapshot.target=ecs::PlayerRuntime::GetPacketVID(seller);w.snapshot.owner=ecs::PlayerRuntime::GetPlayerID(seller);g_registry.emplace_or_replace<Window>(e,std::move(w));ecs::SocialSystem::SetShopOwner(e,seller);Reply(e,Status::Ready);return true;}
int HandleTypedPrivateShopPacket(entt::entity e,const char* data,std::size_t available){
 const auto size=static_cast<unsigned char>(data[1])|(static_cast<unsigned>(static_cast<unsigned char>(data[2]))<<8);auto* desc=ecs::PlayerRuntime::GetDesc(e);if(size<it::PrivateShopRequestMin || size>it::PrivateShopRequestMax){if(desc)desc->SetPhase(PHASE_CLOSE);return -1;}if(available<size)return -1;
 const auto parsed=it::DecodePrivateShopRequest(std::as_bytes(std::span(data,size)));if(!parsed){if(desc)desc->SetPhase(PHASE_CLOSE);return -1;}const auto& r=*parsed;const auto extra=static_cast<int>(size-it::PrivateShopRequestMin);
 if(r.action==1){if(const auto id=it::ItemInstanceId::Create(r.subject))OpenTypedPrivateShopComposer(e,*id);return extra;}if(r.action==5){TryOpenTypedPrivateShop(e,CHARACTER_MANAGER::instance().FindEntity(r.target));return extra;}auto* w=g_registry.valid(e)?g_registry.try_get<Window>(e):nullptr;
 if(r.action==4){CloseWindow(e);return extra;}if(!w){return extra;}if(w->pending){Reply(e,Status::Pending);return extra;}
 const auto c=rt::CurrentCatalog();const auto* t=g_registry.try_get<ecs::TypedItems>(e);if(!c || !t)return extra;
 if(r.action==7){if(!HasTypedPrivateShop(e) || !Live(e)){Reply(e,Status::Denied);return extra;}auto& m=g_registry.get<Market>(e);w->quote=it::MakePrivateShopClose(*c,Op(),m.header,t->state.fence,t->state.storages,t->state.items).value_or(it::ItemCommit{});w->snapshot.view=1;w->seller=entt::null;}
 else if(r.action==2){if(w->snapshot.view!=0 || !Admitted(e) || !OpenZone(e) || g_registry.all_of<ecs::TypedItemBusy>(e) || r.subject!=w->snapshot.subject || CBanwordManager::instance().CheckString(r.sign.c_str(),r.sign.size()) || !Gold(t->state)){Reply(e,Status::Denied);return extra;}
  auto commit=it::MakePrivateShopOpen(*c,Op(),t->state.fence,t->state.storages,t->state.items,*Gold(t->state),it::ItemInstanceId::Create(r.subject).value(),r.sign,r.offers,Now(),GOLD_MAX);if(!commit){Reply(e,Refuse(commit.error()));return extra;}w->quote=std::move(*commit);w->snapshot.sign=r.sign;w->snapshot.offers=r.offers;w->snapshot.remember=w->quote->privateShop->header.rememberPrices;
 }else if(r.action==6){const auto seller=CHARACTER_MANAGER::instance().FindEntity(r.target);const auto* m=g_registry.valid(seller)?g_registry.try_get<Market>(seller):nullptr;const auto* s=g_registry.valid(seller)?g_registry.try_get<ecs::TypedItems>(seller):nullptr;if(w->snapshot.view!=2 || w->seller!=seller || !m || m->closing || !s || !Admitted(e) || !Range(e,seller) || g_registry.any_of<ecs::TypedItemBusy>(e) || g_registry.any_of<ecs::TypedItemBusy>(seller)){Reply(e,Status::Denied);return extra;}
  auto items=s->state.items;items.insert(items.end(),t->state.items.begin(),t->state.items.end());auto layouts=s->state.storages;layouts.insert(layouts.end(),t->state.storages.begin(),t->state.storages.end());auto wallets=s->state.wallets;wallets.insert(wallets.end(),t->state.wallets.begin(),t->state.wallets.end());const auto id=it::ItemInstanceId::Create(r.subject);auto commit=id?it::MakePrivateShopBuy(*c,Op(),m->header,s->state.fence,t->state.fence,layouts,items,wallets,*id,Tax(),Now(),GOLD_MAX):std::expected<it::ItemCommit,it::PrivateShopError>{std::unexpected(it::PrivateShopError::Invalid)};if(!commit){Reply(e,Refuse(commit.error()));return extra;}w->quote=std::move(*commit);w->snapshot.subject=r.subject;
 }else if(r.action!=3){Reply(e,Status::Invalid);return extra;}
 if(r.action!=3){if(!w->quote || !it::ValidateItemCommit(*c,*w->quote)){w->quote.reset();Reply(e,Status::Invalid);return extra;}w->snapshot.nonce=Nonce();w->expires=Now()+30000;w->attempted=false;Reply(e,w->snapshot.nonce?Status::Preview:Status::Failed);return extra;}
 if(!w->quote || r.nonce!=w->snapshot.nonce){Reply(e,Status::Stale);return extra;}if(w->attempted){Reply(e,w->snapshot.status);return extra;}w->attempted=true;
 const auto kind=w->quote->privateShop->kind;if(kind==it::PrivateShopMutationKind::Open && !OpenZone(e)){Reply(e,Status::Denied);return extra;}if(kind==it::PrivateShopMutationKind::Buy && w->quote->privateShop->taxPercent!=Tax()){Reply(e,Status::Stale);return extra;}const auto seller=kind==it::PrivateShopMutationKind::Buy?w->seller:e;
 if(Now()>=w->expires || !Live(e) || !Live(seller) || kind!=it::PrivateShopMutationKind::Close && (!Admitted(e) || kind==it::PrivateShopMutationKind::Buy && !Range(e,seller)) || g_registry.all_of<ecs::TypedItemBusy>(e) || g_registry.all_of<ecs::TypedItemBusy>(seller)){Reply(e,Status::Denied);return extra;}
 auto permission=kind==it::PrivateShopMutationKind::Buy?ecs::resource_admission::Enter({e,seller},auriga::resource_admission::Operation::Shop):ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::Shop);
 w=g_registry.valid(e)?g_registry.try_get<Window>(e):nullptr;if(!permission || !permission->Current() || !w || w->snapshot.nonce!=r.nonce || !w->quote){return extra;}
 std::vector<rt::OwnerRead> reads;std::vector<std::pair<std::uint32_t,std::uint64_t>> actors;for(const auto& fence:w->quote->owners){const auto actor=CHARACTER_MANAGER::instance().FindEntityByPID(static_cast<std::uint32_t>(fence.owner.id));const auto* state=g_registry.valid(actor)?g_registry.try_get<ecs::TypedItems>(actor):nullptr;const auto* login=g_registry.valid(actor)?g_registry.try_get<ecs::TypedItemLogin>(actor):nullptr;if(!state || !login || state->state.fence!=fence || g_registry.all_of<ecs::TypedItemBusy>(actor)){Reply(e,Status::Stale);return extra;}reads.push_back({fence.owner,fence.sessionGeneration});actors.push_back({static_cast<std::uint32_t>(fence.owner.id),login->token});}
 const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=g_registry.get<ecs::TypedItemLogin>(e).token;const auto header=w->quote->privateShop->header;const auto nonce=w->snapshot.nonce;auto commit=*w->quote;auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permission));w->pending=true;for(const auto& [id,key]:actors)if(const auto actor=Current(id,key);actor!=entt::null)g_registry.emplace_or_replace<ecs::TypedItemBusy>(actor);
 auto completion=[pid,token,nonce,actors,header,kind,seller,held](rt::MultiCommitOutcome result)mutable{
  for(unsigned n=0;n<actors.size();++n){const auto actor=Current(actors[n].first,actors[n].second);if(actor==entt::null)continue;if(n<result.states.size() && result.states[n])g_registry.emplace_or_replace<ecs::TypedItems>(actor,std::move(*result.states[n]));if(Current(actors[n].first,actors[n].second)==actor)g_registry.remove<ecs::TypedItemBusy>(actor);}
  const auto e=Current(pid,token);if(result.write && kind==it::PrivateShopMutationKind::Open){if(e!=entt::null){ShowMarket(e,header);if(auto* w=g_registry.try_get<Window>(e))w->snapshot.view=1;}else rt::ItemStore::Instance().Submit([header](CAsyncSQL& sql,const it::ItemCatalog& c)->rt::ItemStore::Completion{it::persistence::RecoverPrivateShop(sql,c,{it::ItemOwnerKind::Character,header.owner},header.session);return {};});}
  if(result.write && kind==it::PrivateShopMutationKind::Close && e!=entt::null)RemoveMarket(e);
  if(result.write && kind==it::PrivateShopMutationKind::Buy && !actors.empty() && Current(actors[0].first,actors[0].second)==seller && HasTypedPrivateShop(seller) && g_registry.get<Market>(seller).header.operation==header.operation){const auto* s=g_registry.try_get<ecs::TypedItems>(seller);if(s && std::ranges::none_of(s->state.items,[](const auto& i){return i.privateShop.has_value();}))RemoveMarket(seller);else Notify(seller);}
  if(e!=entt::null){if(auto* w=g_registry.try_get<Window>(e);w && w->snapshot.nonce==nonce){w->pending=false;Reply(e,result.write?Status::Applied:Status::Stale);}}
  for(const auto& [id,key]:actors)if(const auto actor=Current(id,key);actor!=entt::null){SendTypedInventory(actor);PumpTypedItemGrants(actor);}
 };
 bool current=held->Current();for(unsigned n=0;n<actors.size();++n){const auto actor=Current(actors[n].first,actors[n].second);const auto* state=actor!=entt::null?g_registry.try_get<ecs::TypedItems>(actor):nullptr;current=current && state && state->state.fence==commit.owners[n];}
 if(!current || !rt::RequestCommitMany(std::move(reads),std::move(commit),completion)){rt::MultiCommitOutcome failed{std::unexpected(it::ItemPersistenceError::SqlFailure),{}};completion(std::move(failed));}else Reply(e,Status::Pending);return extra;
}
void TickTypedPrivateShops(){
 std::vector<entt::entity> markets;for(auto e:g_registry.view<Market>())markets.push_back(e);for(auto e:markets){if(!g_registry.valid(e))continue;auto* m=g_registry.try_get<Market>(e);if(!m)continue;const auto* stock=g_registry.try_get<ecs::TypedItems>(e);if(!Live(e) || m->closing || stock && std::ranges::none_of(stock->state.items,[](const auto& i){return i.privateShop.has_value();})){if(!m->pending && Now()>=m->retryAt)QueueClose(e);continue;}if(AffectSystem::GetPolymorphVnum(e)!=30000)AffectSystem::SetPolymorph(e,30000,true);}
 std::vector<entt::entity> windows;for(auto e:g_registry.view<Window>())if(g_registry.get<Window>(e).seller!=entt::null && (!Admitted(e) || !Range(e,g_registry.get<Window>(e).seller) || !HasTypedPrivateShop(g_registry.get<Window>(e).seller)))windows.push_back(e);for(auto e:windows)CloseWindow(e);
 static std::uint64_t next=0;if(Now()<next)return;next=Now()+1000;const auto c=rt::CurrentCatalog();if(!c)return;std::vector<entt::entity> owners;for(auto e:g_registry.view<ecs::TypedItems,ecs::TypedItemLogin>(entt::exclude<ecs::TypedItemBusy>))owners.push_back(e);
 for(auto e:owners){if(!g_registry.valid(e))continue;const auto& s=g_registry.get<ecs::TypedItems>(e).state;for(const auto& i:s.items)if(i.shopPermitDeadline && *i.shopPermitDeadline<=Now()){auto commit=it::MakePrivateShopPermitExpiry(*c,Op(),s.fence,s.storages,s.items,i.id,Now());if(!commit)break;const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=g_registry.get<ecs::TypedItemLogin>(e).token;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);if(!rt::RequestCommit(s.fence.owner,s.fence.sessionGeneration,std::move(*commit),[pid,token](rt::CommitOutcome out){const auto e=Current(pid,token);if(e==entt::null)return;if(out.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*out.state));g_registry.remove<ecs::TypedItemBusy>(e);SendTypedInventory(e);PumpTypedItemGrants(e);})){g_registry.remove<ecs::TypedItemBusy>(e);}break;}}
}
