#include "stdafx.h"
#include "TypedBooks.hpp"
#include "../core/config.h"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "TypedRecovery.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/skill_components.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../combat/skill.h"
#include "../quest/questpc.h"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include <item/BookRuntime.hpp>
#include <openssl/rand.h>
namespace
{
namespace item=auriga::item;namespace store=item::runtime;
enum class Status:std::uint8_t{Closed,Ready,Pending,Applied,Invalid,Denied,Busy,Stale,Failed};
struct TypedBookSession
{
    std::uint64_t control=0,preview=0;std::optional<item::PersistentItem> source;std::array<std::uint8_t,16> operation{};
    item::BookCharacter before;std::uint64_t revision=0;bool attempted=false,pending=false;Status status=Status::Closed;std::uint8_t error=255;
    std::optional<item::SkillBookPlan> result;std::optional<item::BookAidPlan> aidResult;item::SkillBookDecisions decisions;
};
struct Pending
{
    std::uint32_t pid=0;std::uint64_t login=0;
    std::vector<std::function<void()>> mutations,waiters;
};
struct PendingBook{std::shared_ptr<Pending> value;};struct ApplyingBook{};
std::map<std::uint32_t,std::shared_ptr<Pending>> pending;
std::uint64_t Nonce(){std::uint64_t n=0;while(!n)if(RAND_bytes(reinterpret_cast<unsigned char*>(&n),8)!=1)return 0;return n;}
std::optional<std::uint64_t> Draw(std::uint64_t maximum)
{if(!maximum || maximum>INT64_MAX)return {};const auto cut=(UINT64_MAX-maximum+1)%maximum;std::uint64_t n;do{if(RAND_bytes(reinterpret_cast<unsigned char*>(&n),8)!=1)return {};}while(n<cut);return n%maximum;}
bool InGame(entt::entity e){const auto* d=g_registry.valid(e) ? ecs::PlayerRuntime::GetDesc(e) : nullptr;return d && d->IsPhase(PHASE_GAME);}
std::optional<item::BookCharacter> Character(entt::entity e)
{
    const auto* skills=g_registry.try_get<ecs::SkillLevels>(e);if(!skills || !skills->loaded)return {};item::BookCharacter result;result.experience=ecs::PlayerRuntime::GetExp(e);
    const auto stat=ecs::PointSystem::Get(e,POINT_STAT);const auto hp=ecs::PointSystem::GetRandomHP(e);const auto sp=ecs::PointSystem::GetRandomSP(e);const auto step=ecs::PointSystem::Get(e,POINT_LEVEL_STEP);
    if(stat<0 || stat>INT16_MAX || hp<0 || hp>INT16_MAX || sp<0 || sp>INT16_MAX || step<0 || step>3)return {};
    result.progression=item::CharacterProgression{uint16_t(ecs::PointSystem::GetLevel(e)),uint16_t(step),uint16_t(stat),uint16_t(hp),uint16_t(sp)};
    unsigned i=0;for(auto type:{POINT_SKILL,POINT_SUB_SKILL,POINT_HORSE_SKILL}){const auto p=ecs::PointSystem::Get(e,type);if(p<0 || p>INT16_MAX)return {};result.points[i++]=static_cast<std::uint16_t>(p);}
    static_assert(sizeof(TPlayerSkill)==10 && SKILL_MAX_NUM==255);
    for(unsigned n=0;n<255;++n){const auto& p=skills->levels[n];result.skills[n]={p.bMasterType,p.bLevel,p.tNextRead};}if(!item::ValidBookCharacter(result))return {};return result;
}
void Apply(entt::entity e,const item::BookLearning& state,bool notify=false)
{
    const auto oldLevel=ecs::PointSystem::GetLevel(e);const auto oldStep=ecs::PointSystem::Get(e,POINT_LEVEL_STEP);
    g_registry.emplace_or_replace<ApplyingBook>(e);std::array<TPlayerSkill,255> skills{};
    for(unsigned n=0;n<255;++n){const auto& p=state.character.skills[n];skills[n]={p.mastery,p.level,p.nextReadSeconds};}
    SkillSystem::LoadSkillLevels(e,skills.data(),SkillSystem::GetSkillGroup(e));ecs::PlayerRuntime::SetExp(e,state.character.experience);
    unsigned i=0;for(auto type:{POINT_SKILL,POINT_SUB_SKILL,POINT_HORSE_SKILL}){ecs::PointSystem::Set(e,type,state.character.points[i]);ecs::PointSystem::SetReal(e,type,state.character.points[i++]);}
    if(state.character.progression){const auto& p=*state.character.progression;if(notify && p.level>oldLevel)ecs::PointSystem::Change(e,POINT_LEVEL,p.level-oldLevel,false,true);else ecs::PlayerRuntime::SetLevel(e,uint8_t(p.level));ecs::PointSystem::SetRandomHP(e,p.randomHP);ecs::PointSystem::SetRandomSP(e,p.randomSP);ecs::PointSystem::Set(e,POINT_LEVEL_STEP,p.step);ecs::PointSystem::SetReal(e,POINT_LEVEL_STEP,p.step);ecs::PointSystem::Set(e,POINT_STAT,p.statPoints);ecs::PointSystem::SetReal(e,POINT_STAT,p.statPoints);}
    g_registry.emplace_or_replace<ecs::TypedBookRevision>(e,state.revision);ecs::PointSystem::Compute(e);if(notify && state.character.progression && (state.character.progression->level>oldLevel || state.character.progression->step>oldStep)){ecs::PointSystem::Change(e,POINT_HP,int64_t(ecs::PointSystem::GetMaxHP(e))-ecs::PointSystem::Get(e,POINT_HP));ecs::PointSystem::Change(e,POINT_SP,int64_t(ecs::PointSystem::GetMaxSP(e))-ecs::PointSystem::Get(e,POINT_SP));ecs::PointSystem::Change(e,POINT_STAMINA,int64_t(ecs::PlayerRuntime::GetMaxStamina(e))-ecs::PlayerRuntime::GetStamina(e));}g_registry.remove<ApplyingBook>(e);SkillSystem::SendSkillLevelPacket(e);NetworkSyncSystem::PointsPacket(e);
}
item::SkillBookAdmission Facts(entt::entity e,const item::PersistentItem& source)
{
    item::SkillBookAdmission a;const auto c=store::CurrentCatalog();const auto* p=c ? item::BookItemProfile(*c,source.key) : nullptr;const auto actor=TypedRecoveryAdmission(e,{},0);
    a.characterLevel=actor.level;a.classIndex=actor.classIndex;a.sexIndex=actor.sexIndex;a.empire=ecs::PlayerRuntime::GetEmpire(e);a.alive=!CombatSystem::IsDead(e) && actor.pools.hpRaw>0;a.polymorphed=AffectSystem::IsPolymorphed(e);a.inCombat=actor.inCombat;a.skillSystemEnabled=!g_bSkillDisable && SkillSystem::HasSkillLevels(e);
    a.targetAllowedByRules=(p || (c && item::BookAidItemProfile(*c,source.key))) && (actor.context==item::RecoveryContext::World || actor.context==item::RecoveryContext::Duel || actor.context==item::RecoveryContext::Arena || actor.context==item::RecoveryContext::GuildWar);
    if(!p || p->operation==item::SkillBookOperation::HorsePoint)return a;
    const auto skill=p->targetMode==item::SkillBookTargetMode::Fixed ? p->targets.front().skillId : source.bookSkill.value_or(0);const auto* proto=CSkillManager::instance().Get(skill);
    if(!proto || !skill || skill>=255){a.targetAllowedByRules=false;return a;}
    const auto level=SkillSystem::GetSkillLevel(e,skill);const auto step=std::ranges::find(p->steps,level,&item::SkillBookStep::currentLevel);
    if((step!=p->steps.end() && step->nextLevel>item::BookRuntimeSkillCap(proto->dwType,proto->bMaxLevel)) || a.characterLevel<proto->bLevelLimit)a.targetAllowedByRules=false;
    if(proto->preSkillVnum && SkillSystem::GetSkillLevel(e,proto->preSkillVnum)<proto->preSkillLevel && SkillSystem::GetSkillMasterType(e,proto->preSkillVnum)==SKILL_NORMAL)a.targetAllowedByRules=false;
    if(proto->dwType>=1 && proto->dwType<=4){const auto job=ecs::PlayerRuntime::GetJob(e);const auto group=SkillSystem::GetSkillGroup(e);const unsigned first=1+job*30+(group ? group-1 : 0)*15;if(proto->dwType!=job+1 || group<1 || group>2 || skill<first || skill>first+5)a.targetAllowedByRules=false;}
    else if(proto->dwType==5){if(skill==SKILL_HORSE_WILDATTACK_RANGE && ecs::PlayerRuntime::GetJob(e)!=JOB_ASSASSIN)a.targetAllowedByRules=false;}
    else if(proto->dwType!=0 && proto->dwType!=6)a.targetAllowedByRules=false;
#ifdef ENABLE_NEW_PASSIVE_SKILLS
    if(p->operation==item::SkillBookOperation::Learn && !SkillSystem::CanIncreaseSkill(e,skill,true))a.targetAllowedByRules=false;
#endif
#ifdef ENABLE_NEW_SECONDARY_SKILLS
    if(p->operation==item::SkillBookOperation::Learn && step!=p->steps.end() && step->nextLevel>10 && (skill==NEW_SUPPORT_SKILL_ATTACK || skill==NEW_SUPPORT_SKILL_YANG || skill==NEW_SUPPORT_SKILL_MONSTERS || skill==NEW_SUPPORT_SKILL_HP))a.targetAllowedByRules=false;
#endif
    if(p->operation==item::SkillBookOperation::Forget){const auto target=std::ranges::find(p->targets,skill,&item::SkillBookTarget::skillId);const auto pool=proto->dwType==0 ? item::SkillPointPool::Secondary : proto->dwType==5 ? item::SkillPointPool::Horse : proto->dwType<=4 ? item::SkillPointPool::Main : item::SkillPointPool::None;if(target==p->targets.end() || pool==item::SkillPointPool::None || target->refundPool!=pool)a.targetAllowedByRules=false;}
    return a;
}
bool Admitted(entt::entity e)
{if(!InGame(e) || !InventorySystem::CanHandleItems(e) || ecs::SocialSystem::HasExchange(e) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e) || ecs::SocialSystem::GetShopOwner(e)!=entt::null || ecs::SocialSystem::GetMyShop(e)!=entt::null)return false;const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);if(flags && (flags->isObserverMode || flags->isStunned))return false;const auto* q=g_registry.try_get<ecs::QuestPCState>(e);return q && q->pc && !q->pc->IsRunning();}
TypedBookSession* State(entt::entity e){auto* s=g_registry.try_get<TypedBookSession>(e);if(!s){TypedBookSession next;next.control=Nonce();if(!next.control)return nullptr;s=&g_registry.emplace<TypedBookSession>(e,std::move(next));}return s;}
void Reply(entt::entity e,Status status,std::uint8_t error=255)
{
    if(!InGame(e))return;auto* s=g_registry.try_get<TypedBookSession>(e);const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto c=store::CurrentCatalog();const auto character=Character(e);if(!s || !typed || !c || !character){LOG_ERROR("TypedBooks: reply state unavailable pid {} status {}",ecs::PlayerRuntime::GetPlayerID(e),static_cast<unsigned>(status));return;}s->status=status;s->error=error;
    const auto found=s->source ? std::ranges::find(typed->state.items,s->source->id,&item::PersistentItem::id) : typed->state.items.end();const auto* source=found!=typed->state.items.end() ? &*found : s->source ? &*s->source : nullptr;
    auto actor=source ? Facts(e,*source) : item::SkillBookAdmission{};std::string name;
    if(source)if(const auto* p=item::BookItemProfile(*c,source->key)){const auto skill=p->targetMode==item::SkillBookTargetMode::Fixed ? p->targets.front().skillId : source->bookSkill.value_or(0);if(const auto* proto=CSkillManager::instance().Get(skill))name=proto->szName;}
    const auto bytes=item::EncodeBookRuntime(*c,{static_cast<std::uint8_t>(status),error,s->control,source ? s->preview : 0,TypedItemEffectNow(),source,&typed->state.learning,*character,actor,name,s->result ? &*s->result : nullptr,s->decisions,s->aidResult ? &*s->aidResult : nullptr});if(bytes)ecs::PlayerRuntime::GetDesc(e)->LargePacket(bytes->data(),static_cast<int>(bytes->size()));else LOG_ERROR("TypedBooks: reply encode failed pid {} status {} error {}",ecs::PlayerRuntime::GetPlayerID(e),static_cast<unsigned>(status),static_cast<unsigned>(bytes.error()));
}
}
bool DeferTypedBookMutation(entt::entity e,std::function<void()> action)
{if(!g_registry.valid(e) || g_registry.all_of<ApplyingBook>(e))return false;const auto* p=g_registry.try_get<PendingBook>(e);if(!p)return false;p->value->mutations.push_back(std::move(action));return true;}
bool DeferTypedBookLogin(std::uint32_t pid,std::function<void()> action)
{const auto f=pending.find(pid);if(f==pending.end())return false;f->second->waiters.push_back(std::move(action));return true;}
bool ApplyTypedBookLogin(entt::entity e)
{
    const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto* revision=g_registry.try_get<ecs::TypedBookRevision>(e);if(!typed || !revision || revision->value>typed->state.learning.revision)return false;
    if(revision->value<typed->state.learning.revision)Apply(e,typed->state.learning);return true;
}
std::optional<item::BookCharacter> TypedBookCharacter(entt::entity e){return Character(e);}
std::function<void(bool,bool)> HoldTypedBookCheckpoint(entt::entity e)
{
    const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);const auto pid=ecs::PlayerRuntime::GetPlayerID(e);
    if(!login || !Character(e) || pending.contains(pid) || g_registry.all_of<PendingBook>(e))return {};
    auto work=std::make_shared<Pending>();work->pid=pid;work->login=login->token;pending[pid]=work;g_registry.emplace<PendingBook>(e,work);
    return [work](bool apply,bool replay) {
        const auto entity=CHARACTER_MANAGER::instance().FindEntityByPID(work->pid);
        const auto* login=g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItemLogin>(entity) : nullptr;
        if(login && login->token==work->login) {
            if(apply)if(const auto* t=g_registry.try_get<ecs::TypedItems>(entity))Apply(entity,t->state.learning,true);
            g_registry.remove<PendingBook>(entity);
            if(replay)for(auto& mutation:work->mutations)if(g_registry.valid(entity))mutation();
        }
        pending.erase(work->pid);for(auto& waiter:work->waiters)waiter();
    };
}
bool HandleTypedBooksPacket(entt::entity e,const char* data)
{
    static_assert(sizeof(TPacketCGTypedRecovery)==26);const auto r=*reinterpret_cast<const TPacketCGTypedRecovery*>(data);if(!InGame(e))return true;auto* s=State(e);const auto c=store::CurrentCatalog();const auto* typed=g_registry.try_get<ecs::TypedItems>(e);if(!s || !c || !typed)return true;
    if(s->pending){Reply(e,Status::Pending);return true;}
    if(r.action==3){if(r.session!=s->control || r.source || r.target){Reply(e,Status::Stale);return true;}s->source.reset();s->preview=0;s->result.reset();s->aidResult.reset();Reply(e,Status::Closed);return true;}
    if(!Admitted(e)){Reply(e,Status::Denied);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Busy);return true;}
    const auto character=Character(e);if(!character){Reply(e,Status::Denied,3);return true;}
    if(r.action==1)
    {
        if(r.session || r.target || !r.source){Reply(e,Status::Invalid);return true;}const auto source=std::ranges::find_if(typed->state.items,[&](const auto& i){return i.id.Value()==r.source;});if(source==typed->state.items.end() || (!item::BookItemProfile(*c,source->key) && !item::BookAidItemProfile(*c,source->key)) || !item::PlainInventoryItem(*source,typed->state.fence.owner)){Reply(e,Status::Invalid);return true;}
        s->source=*source;s->preview=Nonce();s->before=*character;s->revision=typed->state.learning.revision;s->attempted=false;s->result.reset();s->aidResult.reset();s->decisions={};if(!s->preview || RAND_bytes(s->operation.data(),16)!=1){Reply(e,Status::Failed);return true;}
        if(item::BookAidItemProfile(*c,source->key)){const auto quote=item::PrepareBookAidUse(*c,*source,typed->state.learning,*character,Facts(e,*source));Reply(e,quote ? Status::Ready : Status::Denied,quote ? 255 : static_cast<std::uint8_t>(quote.error()));return true;}
    const auto actor=item::BookRuntimeAdmission(*c,*source,typed->state.learning,*character,Facts(e,*source),TypedItemEffectNow());const auto quote=actor ? item::PrepareSkillBookUse(*c,source->key,*actor) : std::expected<item::PreparedSkillBookUse,item::SkillBookError>{std::unexpected(actor.error())};Reply(e,quote ? Status::Ready : Status::Denied,quote ? 255 : static_cast<std::uint8_t>(quote.error()));return true;
    }
    if(r.action!=2){Reply(e,Status::Invalid);return true;}
    if(!s->source || r.session!=s->preview || r.source!=s->source->id.Value() || r.target){Reply(e,Status::Stale);return true;}if(s->attempted){Reply(e,s->status,s->error);return true;}s->attempted=true;
    const auto source=std::ranges::find(typed->state.items,s->source->id,&item::PersistentItem::id);if(source==typed->state.items.end() || *source!=*s->source || *character!=s->before || typed->state.learning.revision!=s->revision){Reply(e,Status::Stale);return true;}
    std::optional<item::ItemCommit> commit;std::optional<item::SkillBookPlan> bookResult;std::optional<item::BookAidPlan> aidResult;item::SkillBookDecisions decisions;
    if(item::BookAidItemProfile(*c,source->key))
    {
        auto plan=item::MakeBookAidRuntimeCommit(*c,s->operation,typed->state.fence,typed->state.storages,typed->state.items,typed->state.learning,*character,source->id,Facts(e,*source));
        if(!plan){Reply(e,Status::Denied,static_cast<std::uint8_t>(plan.error()));return true;}commit=std::move(plan->commit);aidResult=plan->result;
    }
    else
    {
    const auto actor=item::BookRuntimeAdmission(*c,*source,typed->state.learning,*character,Facts(e,*source),TypedItemEffectNow());const auto quote=actor ? item::PrepareSkillBookUse(*c,source->key,*actor) : std::expected<item::PreparedSkillBookUse,item::SkillBookError>{std::unexpected(actor.error())};if(!quote){Reply(e,Status::Denied,static_cast<std::uint8_t>(quote.error()));return true;}
    if(quote->NeedsSuccessRoll()){const auto n=Draw(10000);if(!n){Reply(e,Status::Failed);return true;}decisions.successRollBp=static_cast<std::uint16_t>(*n+1);}const auto width=quote->MaximumDelayMs()-quote->MinimumDelayMs();const auto delay=width ? Draw(width+1) : std::optional<std::uint64_t>{0};if(!delay){Reply(e,Status::Failed);return true;}decisions.readDelayMs=quote->MinimumDelayMs()+*delay;
    auto plan=item::MakeBookRuntimeCommit(*c,s->operation,typed->state.fence,typed->state.storages,typed->state.items,typed->state.learning,*character,source->id,Facts(e,*source),TypedItemEffectNow(),decisions);if(!plan){LOG_INFO("TypedBooks: plan denied pid {} error {}",ecs::PlayerRuntime::GetPlayerID(e),static_cast<unsigned>(plan.error()));Reply(e,Status::Denied,static_cast<std::uint8_t>(plan.error()));return true;}
    commit=std::move(plan->commit);bookResult=plan->result;
    }
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);if(!permit || !permit->Current() || !login){Reply(e,Status::Denied);return true;}
    auto work=std::make_shared<Pending>();work->pid=ecs::PlayerRuntime::GetPlayerID(e);work->login=login->token;pending[work->pid]=work;g_registry.emplace<PendingBook>(e,work);g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);s->pending=true;s->status=Status::Pending;s->result=bookResult;s->aidResult=aidResult;s->decisions=decisions;
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));const auto owner=typed->state.fence;
    const bool queued=store::RequestCommit(owner.owner,owner.sessionGeneration,std::move(*commit),[work,held](store::CommitOutcome result)
    {
        LOG_INFO("TypedBooks: commit finished pid {} result {}",work->pid,result.write ? 0 : static_cast<unsigned>(result.write.error()));
        const auto entity=CHARACTER_MANAGER::instance().FindEntityByPID(work->pid);const auto* current=entity!=entt::null && g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItemLogin>(entity) : nullptr;
        if(current && current->token==work->login && !result.state){LOG_ERROR("TypedBooks: authoritative reload failed pid {} error {}",work->pid,static_cast<unsigned>(result.state.error()));g_registry.remove<PendingBook,ecs::TypedItemBusy>(entity);if(auto* desc=ecs::PlayerRuntime::GetDesc(entity))desc->SetPhase(PHASE_CLOSE);pending.erase(work->pid);for(auto& action:work->waiters)action();return;}
        if(current && current->token==work->login){if(result.state)g_registry.emplace_or_replace<ecs::TypedItems>(entity,std::move(*result.state));if(result.write)Apply(entity,g_registry.get<ecs::TypedItems>(entity).state.learning);g_registry.remove<PendingBook,ecs::TypedItemBusy>(entity);if(auto* session=g_registry.try_get<TypedBookSession>(entity)){session->pending=false;if(!result.write){session->result.reset();session->aidResult.reset();}}for(auto& action:work->mutations)if(g_registry.valid(entity))action();SendTypedInventory(entity);Reply(entity,result.write ? Status::Applied : Status::Failed);PumpTypedItemGrants(entity);}
        pending.erase(work->pid);for(auto& action:work->waiters)action();
    });
    if(!queued){pending.erase(work->pid);g_registry.remove<PendingBook,ecs::TypedItemBusy>(e);s->pending=false;s->result.reset();s->aidResult.reset();Reply(e,Status::Failed);}return true;
}
