#include "stdafx.h"
#include "TypedPolymorph.hpp"
#include "TypedRecovery.hpp"
#include "TypedInventorySync.hpp"
#include "TypedEquipment.hpp"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/components/character_stats_components.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../entity/char_manager.h"
#include "../entity/mob_manager.h"
#include "../quest/questpc.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include "../core/config.h"
#include <item/PolymorphRuntime.hpp>
#include <openssl/rand.h>
namespace
{
namespace item=auriga::item;namespace store=item::runtime;
enum class Status:std::uint8_t{Closed,Ready,Pending,Applied,Invalid,Denied,Busy,Stale,Failed};
struct TypedPolymorphSession
{
    std::uint64_t control=0,preview=0,generation=0,mobs=0;
    std::optional<item::PersistentItem> source;std::array<std::uint8_t,16> operation{};
    std::uint8_t purpose=0,outcome=0,skill=0,mastery=0;std::uint16_t level=0,roll=0;
    bool pending=false,attempted=false,visible=false;Status status=Status::Closed;std::uint8_t error=255;
};
struct UpdatingTypedPolymorph{};
std::uint64_t Nonce(){std::uint64_t n=0;while(!n)if(RAND_bytes(reinterpret_cast<unsigned char*>(&n),8)!=1)return 0;return n;}
std::optional<std::uint16_t> Roll(){std::uint32_t n;do{if(RAND_bytes(reinterpret_cast<unsigned char*>(&n),4)!=1)return {};}while(n<(UINT32_MAX-10000u+1u)%10000u);return static_cast<std::uint16_t>(n%10000+1);}
bool InGame(entt::entity e){const auto* d=g_registry.valid(e) ? ecs::PlayerRuntime::GetDesc(e) : nullptr;return d && d->IsPhase(PHASE_GAME);}
bool Admitted(entt::entity e,bool cancel=false)
{
    if(!InGame(e) || (!cancel && (!InventorySystem::CanHandleItems(e) || CombatSystem::IsDead(e))) || ecs::SocialSystem::HasExchange(e) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e) || ecs::SocialSystem::GetShopOwner(e)!=entt::null || ecs::SocialSystem::GetMyShop(e)!=entt::null)return false;
    const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);const auto* q=g_registry.try_get<ecs::QuestPCState>(e);return q && q->pc && !q->pc->IsRunning() && (!flags || (!flags->isObserverMode && (cancel || !flags->isStunned)));
}
std::string Name(std::uint32_t mob)
{const auto* p=CMobManager::instance().Get(mob);return p ? std::string(p->m_table.szName) : std::string{};}
item::PolymorphRequest Facts(entt::entity e,const item::PersistentItem& source,std::uint8_t purpose,std::array<std::uint8_t,16> operation)
{
    item::PolymorphRequest a{source.key};const auto c=store::CurrentCatalog();const auto* row=c ? item::PolymorphItem(*c,source.key) : nullptr;
    const auto* form=row ? c->FindPolymorphForm(row->fixedForm.value_or(source.polymorph ? source.polymorph->boundForm.value_or(0) : 0)) : nullptr;const auto* mob=form ? CMobManager::instance().Get(form->mobTemplate) : nullptr;
    const auto f=TypedItemEffectFacts(e);a.actorId=ecs::PlayerRuntime::GetPlayerID(e);a.rulesetRevision=1;a.operationId=operation;a.action=purpose==2 ? item::PolymorphAction::UpgradeBook : item::PolymorphAction::Use;
    a.characterLevel=f.actor.level;a.characterClass=f.actor.classIndex+1;a.sex=f.actor.sexIndex+1;a.inCombat=f.actor.inCombat;a.skillLevel=SkillSystem::GetSkillLevel(e,SKILL_POLYMORPH);a.mastery=SkillSystem::GetSkillMasterType(e,SKILL_POLYMORPH);
    a.admittedByServer=f.allowed && f.alive && !g_bSkillDisable && SkillSystem::HasSkillLevels(e);a.riding=MountSystem::IsRiding(e);a.alreadyPolymorphed=AffectSystem::IsPolymorphed(e);
#ifdef ENABLE_PVP_ADVANCED
    a.duelBlocksPolymorph=ecs::PlayerRuntime::GetDuelOption(e,"BlockPoly");
#endif
    if(mob){a.mobTemplate=form->mobTemplate;a.mobLevel=mob->m_table.bLevel;a.mobCatalogRevision=CMobManager::instance().Revision();a.formAllowedByServer=mob->m_table.bType==CHAR_TYPE_MONSTER && form->mobTemplate>=JOB_MAX_NUM && form->mobTemplate<=UINT16_MAX;}
    return a;
}
TypedPolymorphSession* Session(entt::entity e)
{auto* s=g_registry.try_get<TypedPolymorphSession>(e);if(!s){TypedPolymorphSession next;next.control=Nonce();if(!next.control)return nullptr;s=&g_registry.emplace<TypedPolymorphSession>(e,next);}return s;}
void Reply(entt::entity e,Status status,std::uint8_t error=255)
{
    auto* s=g_registry.try_get<TypedPolymorphSession>(e);const auto* t=g_registry.try_get<ecs::TypedItems>(e);const auto c=store::CurrentCatalog();if(!InGame(e) || !s || !t || !c)return;s->status=status;s->error=error;
    auto state=item::AdvanceRecoverySchedule(*c,t->state.recovery,TypedRecoveryElapsed(e));if(!state)return;const auto facts=TypedItemEffectFacts(e);item::NormalizeTimedItems(*c,*state,TypedItemEffectNow(),&facts);
    const auto found=s->source ? std::ranges::find(t->state.items,s->source->id,&item::PersistentItem::id) : t->state.items.end();const auto* source=found!=t->state.items.end() ? &*found : s->source ? &*s->source : nullptr;
    std::optional<item::PolymorphRequest> actor;if(source)actor=Facts(e,*source,s->purpose,s->operation);
    const auto packet=item::EncodePolymorphRuntime(*c,{static_cast<std::uint8_t>(status),error,s->purpose,s->outcome,s->roll,s->control,s->preview,TypedItemEffectNow(),source,&*state,actor ? &*actor : nullptr,actor ? Name(actor->mobTemplate) : "",state->polymorph ? Name(state->polymorph->effect.mobTemplate) : ""});
    if(packet)ecs::PlayerRuntime::GetDesc(e)->LargePacket(packet->data(),static_cast<int>(packet->size()));else LOG_ERROR("TypedPolymorph: encode failed pid {} error {}",ecs::PlayerRuntime::GetPlayerID(e),static_cast<unsigned>(packet.error()));
}
}
void SendTypedPolymorph(entt::entity e)
{if(const auto* s=g_registry.try_get<TypedPolymorphSession>(e);s && s->visible)Reply(e,s->status,s->error);}
void RefreshTypedPolymorph(entt::entity e)
{
    if(!g_registry.valid(e) || g_registry.all_of<UpdatingTypedPolymorph>(e))return;
    const auto* t=g_registry.try_get<ecs::TypedItems>(e);const auto c=store::CurrentCatalog();if(!t || !c)return;
    const auto* points=g_registry.try_get<ecs::CharacterStatsComponent>(e);if(points && points->recomputing)return;
    auto state=t->state.recovery;const auto facts=TypedItemEffectFacts(e);item::NormalizeTimedItems(*c,state,TypedItemEffectNow(),&facts);
    ecs::TypedPolymorphProjection next;
    if(state.polymorph){const auto& p=state.polymorph->effect;const auto* mob=CMobManager::instance().Get(p.mobTemplate);if(mob && mob->m_table.bType==CHAR_TYPE_MONSTER && p.mobTemplate<=UINT16_MAX)next={p.mobTemplate,p.powerBp,static_cast<std::uint8_t>(p.bonus),p.bonusMagnitude};}
    const auto* old=g_registry.try_get<ecs::TypedPolymorphProjection>(e);if(old && *old==next)return;
    const auto oldRace=old ? old->race : 0;g_registry.emplace_or_replace<ecs::TypedPolymorphProjection>(e,next);
    if(!oldRace && !next.race)return;
    g_registry.emplace<UpdatingTypedPolymorph>(e);
    AffectSystem::SetPolymorph(e,next.race);ecs::PointSystem::Compute(e);NetworkSyncSystem::PointsPacket(e);g_registry.remove<UpdatingTypedPolymorph>(e);SendTypedPolymorph(e);
}
bool HandleTypedPolymorphPacket(entt::entity e,const char* data)
{
    static_assert(sizeof(TPacketCGTypedRecovery)==26);if(!InGame(e))return true;RefreshTypedPolymorph(e);
    const auto r=*reinterpret_cast<const TPacketCGTypedRecovery*>(data);auto* s=Session(e);const auto* t=g_registry.try_get<ecs::TypedItems>(e);const auto c=store::CurrentCatalog();if(!s || !t || !c)return true;s->visible=true;
    if(s->pending){Reply(e,Status::Pending);return true;}
    if(r.action==3){if(r.session!=s->control || r.source || r.target){Reply(e,Status::Stale);return true;}s->source.reset();s->preview=0;s->purpose=s->outcome=0;s->roll=0;s->visible=false;Reply(e,Status::Closed);return true;}
    if(r.action==8){if(r.session || r.source || r.target){Reply(e,Status::Invalid);return true;}s->source.reset();s->preview=0;s->purpose=s->outcome=0;s->roll=0;Reply(e,Status::Ready);return true;}
    if(!Admitted(e,r.action==6 || r.action==7)){Reply(e,Status::Denied);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Busy);return true;}
    if(r.action==1 || r.action==4 || r.action==6)
    {
        const bool cancel=r.action==6;if(r.target || (cancel ? r.session!=s->control || r.source!=0 : r.session!=0 || r.source==0)){Reply(e,Status::Invalid);return true;}
        const auto source=std::ranges::find_if(t->state.items,[&](const auto& i){return i.id.Value()==r.source;});if(!cancel && (source==t->state.items.end() || !item::PolymorphItem(*c,source->key) || !item::PlainInventoryItem(*source,t->state.fence.owner))){Reply(e,Status::Invalid);return true;}
        s->source=cancel ? std::nullopt : std::optional{*source};s->purpose=cancel ? 3 : r.action==4 ? 2 : 1;s->preview=Nonce();s->generation=t->state.recovery.generation;s->mobs=CMobManager::instance().Revision();s->attempted=false;s->outcome=0;s->roll=0;
        if(!s->preview || RAND_bytes(s->operation.data(),16)!=1){Reply(e,Status::Failed);return true;}
        auto state=item::AdvanceRecoverySchedule(*c,t->state.recovery,TypedRecoveryElapsed(e));if(!state){Reply(e,Status::Failed);return true;}const auto f=TypedItemEffectFacts(e);item::NormalizeTimedItems(*c,*state,TypedItemEffectNow(),&f);
        if(cancel){Reply(e,state->polymorph ? Status::Ready : Status::Denied,state->polymorph ? 255 : static_cast<std::uint8_t>(item::PolymorphError::NotReady));return true;}
        const auto a=Facts(e,*source,s->purpose,s->operation);s->level=a.characterLevel;s->skill=a.skillLevel;s->mastery=a.mastery;
        const auto quote=item::QuotePolymorphRuntime(*c,*source,*state,a,TypedItemEffectNow());Reply(e,quote ? Status::Ready : Status::Denied,quote ? 255 : static_cast<std::uint8_t>(quote.error()));return true;
    }
    const std::uint8_t purpose=r.action==2 ? 1 : r.action==5 ? 2 : r.action==7 ? 3 : 0;
    if(!purpose || purpose!=s->purpose || r.session!=s->preview || !s->preview || r.target || r.source!=(s->source ? s->source->id.Value() : 0)){Reply(e,Status::Stale);return true;}
    if(s->attempted){Reply(e,s->status,s->error);return true;}s->attempted=true;
    if(t->state.recovery.generation!=s->generation || CMobManager::instance().Revision()!=s->mobs){Reply(e,Status::Stale);return true;}
    std::expected<item::ItemCommit,item::PolymorphError> commit=std::unexpected(item::PolymorphError::InvalidState);
    if(purpose==3)commit=item::MakePolymorphCancel(*c,s->operation,t->state.fence,t->state.recovery,TypedRecoveryElapsed(e),TypedItemEffectNow(),s->generation);
    else
    {
        const auto source=std::ranges::find(t->state.items,s->source->id,&item::PersistentItem::id);if(source==t->state.items.end() || *source!=*s->source){Reply(e,Status::Stale);return true;}
        const auto a=Facts(e,*source,purpose,s->operation);if(a.characterLevel!=s->level || a.skillLevel!=s->skill || a.mastery!=s->mastery){Reply(e,Status::Stale);return true;}
        auto state=item::AdvanceRecoverySchedule(*c,t->state.recovery,TypedRecoveryElapsed(e));if(!state){Reply(e,Status::Failed);return true;}const auto f=TypedItemEffectFacts(e);item::NormalizeTimedItems(*c,*state,TypedItemEffectNow(),&f);
        const auto quote=item::QuotePolymorphRuntime(*c,*source,*state,a,TypedItemEffectNow());if(!quote){Reply(e,Status::Denied,static_cast<std::uint8_t>(quote.error()));return true;}
        if(quote->chanceBp>0 && quote->chanceBp<10000){const auto roll=Roll();if(!roll){Reply(e,Status::Failed);return true;}s->roll=*roll;}
        auto plan=item::MakePolymorphRuntimeCommit(*c,s->operation,t->state.fence,t->state.storages,t->state.items,t->state.recovery,source->id,a,TypedRecoveryElapsed(e),TypedItemEffectNow(),s->roll);
        if(plan){s->outcome=purpose==2 ? 3 : plan->result.success ? 2 : 1;commit=std::move(plan->commit);}else commit=std::unexpected(plan.error());
    }
    if(!commit){Reply(e,Status::Denied,static_cast<std::uint8_t>(commit.error()));return true;}if(purpose==3)s->outcome=4;
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);if(!permit || !permit->Current() || !login){Reply(e,Status::Denied);return true;}
    const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=login->token;const auto owner=t->state.fence;auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));s->pending=true;s->status=Status::Pending;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    const bool queued=store::RequestCommit(owner.owner,owner.sessionGeneration,std::move(*commit),[pid,token,held](store::CommitOutcome out)
    {
        const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* login=e!=entt::null && g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;if(!login || login->token!=token)return;
        if(!out.state){LOG_ERROR("TypedPolymorph: authoritative reload failed pid {}",pid);if(auto* d=ecs::PlayerRuntime::GetDesc(e))d->SetPhase(PHASE_CLOSE);return;}
        g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*out.state));g_registry.remove<ecs::TypedItemBusy>(e);if(auto* s=g_registry.try_get<TypedPolymorphSession>(e)){s->pending=false;if(!out.write){s->outcome=0;s->roll=0;}}
        SendTypedInventory(e);SendTypedEquipment(e);Reply(e,out.write ? Status::Applied : Status::Failed);PumpTypedItemGrants(e);
    });
    if(!queued){g_registry.remove<ecs::TypedItemBusy>(e);s->pending=false;s->outcome=0;s->roll=0;Reply(e,Status::Failed);}return true;
}
