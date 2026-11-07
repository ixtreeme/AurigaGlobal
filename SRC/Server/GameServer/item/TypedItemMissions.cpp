#include "stdafx.h"
#include "TypedRecovery.hpp"
#include "TypedItemMissions.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../mission/ItemDelivery.hpp"
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
#include <Core/Logging.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <random>
#include <set>

namespace
{
namespace items=auriga::item;
namespace store=items::runtime;
namespace missions=auriga::mission;
using Status=items::ItemMissionStatus;
std::map<std::uint32_t,std::shared_ptr<const items::ItemMissionBoard>> boards;
std::shared_ptr<const missions::DefinitionRegistry> definitions;
struct TypedItemMissionSession
{
    entt::entity npc=entt::null;std::uint32_t vid=0,race=0,selected=0;
    std::uint64_t token=0,request=0;bool pending=false;Status status=Status::Ready;
    std::vector<std::uint32_t> completed;
};
std::int64_t Now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
std::uint64_t Nonce(){static std::mt19937_64 engine{std::random_device{}()};std::uint64_t n=0;while(!n)n=engine();return n;}
bool InRange(entt::entity e,entt::entity npc,std::uint32_t race=0)
{
    if(!g_registry.valid(e) || !g_registry.valid(npc) || !ecs::PlayerRuntime::IsValid(npc) || ecs::PlayerRuntime::IsPC(npc) ||
       ecs::PlayerRuntime::GetMapIndex(e)!=ecs::PlayerRuntime::GetMapIndex(npc))return false;
    if(race && ecs::PlayerRuntime::GetRaceNum(npc)!=race)return false;
    const auto x=std::abs(std::int64_t(ecs::PlayerRuntime::GetX(e))-ecs::PlayerRuntime::GetX(npc));
    const auto y=std::abs(std::int64_t(ecs::PlayerRuntime::GetY(e))-ecs::PlayerRuntime::GetY(npc));return std::max(x,y)+std::min(x,y)/2<1000;
}
bool Admitted(entt::entity e)
{
    if(!g_registry.valid(e) || !ecs::PlayerRuntime::IsPC(e) || CombatSystem::IsDead(e) || !InventorySystem::CanHandleItems(e) ||
       ecs::SocialSystem::HasExchange(e) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e) ||
       ecs::SocialSystem::GetMyShop(e)!=entt::null || ecs::SocialSystem::GetShopOwner(e)!=entt::null ||
       !g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e))return false;
    const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);if(flags && (flags->isObserverMode || flags->isStunned))return false;
    const auto* quest=g_registry.try_get<ecs::QuestPCState>(e);if(!quest || !quest->pc || quest->pc->IsRunning())return false;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    if(ecs::PlayerRuntime::GetGMLevel(e)>GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(e)<GM_IMPLEMENTOR)return false;
#endif
    auto* desc=ecs::PlayerRuntime::GetDesc(e);return desc && desc->IsPhase(PHASE_GAME);
}
entt::entity Current(std::uint32_t pid,std::uint64_t token)
{
    const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);
    const auto* login=e!=entt::null && g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;
    return login && login->token==token ? e : entt::null;
}
void Reply(entt::entity e,Status status)
{
    if(!g_registry.valid(e))return;const auto c=store::CurrentCatalog();if(!c)return;
    const auto* session=g_registry.try_get<TypedItemMissionSession>(e);const auto found=session ? boards.find(session->race) : boards.end();
    const auto packet=items::EncodeItemMissionBoard(*c,found==boards.end() ? nullptr : found->second.get(),session ? session->vid : 0,
        session ? session->token : 0,session ? session->request : 0,status,session ? session->selected : 0,session ? std::span<const std::uint32_t>(session->completed) : std::span<const std::uint32_t>{});
    SendTypedInventory(e);
    auto* desc=g_registry.valid(e) ? ecs::PlayerRuntime::GetDesc(e) : nullptr;
    if(desc && desc->IsPhase(PHASE_GAME) && packet)desc->LargePacket(packet->data(),static_cast<int>(packet->size()));
}
void Close(entt::entity e)
{
    if(!g_registry.valid(e))return;const auto* s=g_registry.try_get<TypedItemMissionSession>(e);if(!s)return;
    if(s->pending){Reply(e,Status::Pending);return;}g_registry.remove<TypedItemMissionSession>(e);Reply(e,Status::Closed);
}
Status Refusal(items::ItemMissionError e)
{
    switch(e){case items::ItemMissionError::Requirements:return Status::Requirements;case items::ItemMissionError::NoMoney:return Status::NoMoney;
    case items::ItemMissionError::NoRoom:return Status::NoRoom;case items::ItemMissionError::Level:return Status::Level;
    case items::ItemMissionError::Invalid:return Status::Invalid;default:return Status::Failed;}
}
bool Open(entt::entity e,entt::entity npc)
{
    const auto race=g_registry.valid(npc) ? ecs::PlayerRuntime::GetRaceNum(npc) : 0;const auto board=boards.find(race);
    if(board==boards.end() || !Admitted(e) || !InRange(e,npc,race) || g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Denied);return true;}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);
    if(!permit || !permit->Current()){Reply(e,Status::Denied);return true;}
    const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);
    if(!typed || !login || !Admitted(e) || !InRange(e,npc,race) || g_registry.all_of<ecs::TypedItemBusy>(e))return true;
    const auto fence=typed->state.fence;const auto token=login->token;const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto nonce=Nonce();
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
    TypedItemMissionSession value;value.npc=npc;value.vid=ecs::PlayerRuntime::GetPacketVID(npc);value.race=race;value.token=nonce;value.pending=true;
    g_registry.emplace_or_replace<TypedItemMissionSession>(e,std::move(value));g_registry.insert<ecs::TypedItemBusy>(&e,&e+1);
    if(Current(pid,token)!=e || !held->Current())
    {if(Current(pid,token)==e){g_registry.remove<ecs::TypedItemBusy>(e);if(auto* s=g_registry.try_get<TypedItemMissionSession>(e);s && s->token==nonce){s->pending=false;Reply(e,Status::Failed);}}return true;}
    const bool queued=store::ItemStore::Instance().Submit([fence,pid,token,nonce,board=board->second,held](CAsyncSQL& sql,const items::ItemCatalog& c)->store::ItemStore::Completion {
        std::vector<std::uint32_t> completed;bool valid=true;
        auto state=items::persistence::LoadItemOwner(sql,c,fence.owner,fence.sessionGeneration);
        if(state)for(const auto& m:board->missions)
        {const auto receipt=items::persistence::FindItemMissionReceipt(sql,c,fence,m,missions::ItemDeliveryProof(m,pid));if(!receipt){valid=false;break;}if(*receipt)completed.push_back(m.id);}
        return [pid,token,nonce,held,state=std::move(state),completed=std::move(completed),valid]() mutable {
            const auto e=Current(pid,token);if(e==entt::null)return;
            if(state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*state));
            if(Current(pid,token)!=e)return;g_registry.remove<ecs::TypedItemBusy>(e);if(Current(pid,token)!=e)return;auto* s=g_registry.try_get<TypedItemMissionSession>(e);
            if(s && s->token==nonce){s->pending=false;s->completed=std::move(completed);s->status=state && valid ? Status::Ready : Status::Failed;Reply(e,s->status);}
            PumpTypedItemGrants(e);
        };
    });
    if(!queued)
    {g_registry.remove<ecs::TypedItemBusy>(e);if(auto* s=g_registry.try_get<TypedItemMissionSession>(e);s && s->token==nonce){s->pending=false;s->status=Status::Failed;Reply(e,s->status);}}
    return true;
}
struct DeliveryResult
{
    Status status=Status::Failed;bool committed=false;
    std::expected<items::persistence::LoadedItemOwner,items::ItemPersistenceError> state=std::unexpected(items::ItemPersistenceError::SqlFailure);
};
}
std::expected<void,std::string> LoadTypedItemMissions(const std::string& file)
{
    try
    {
        if(!std::filesystem::exists(file)){boards.clear();definitions.reset();return {};}
        if(std::filesystem::file_size(file)>(1u<<20))return std::unexpected("typed_item_missions.json exceeds 1 MiB");
        std::ifstream stream(file);const auto document=nlohmann::json::parse(stream);
        if(!document.is_object() || document.size()!=2 || document.at("version")!=1 || !document.at("boards").is_array() || document.at("boards").size()>items::MaxItemMissionBoards)return std::unexpected("invalid item mission document");
        const auto integer=[](const nlohmann::json& j,const char* key,std::uint64_t max)->std::uint64_t
        {const auto& v=j.at(key);if(!v.is_number_unsigned() || v.get<std::uint64_t>()>max)throw std::runtime_error("invalid item mission number");return v.get<std::uint64_t>();};
        const auto amounts=[&](const nlohmann::json& row,const char* key) {
            const auto& list=row.at(key);if(!list.is_array() || list.size()>items::MaxMissionItemEntries)throw std::runtime_error("invalid item list");std::vector<items::MissionItemAmount> out;
            for(const auto& v:list)
            {if(!v.is_object() || v.size()!=4)throw std::runtime_error("invalid item amount");const auto item=items::ItemKey::Create(static_cast<std::uint16_t>(integer(v,"category",UINT16_MAX)),static_cast<std::uint32_t>(integer(v,"local",UINT32_MAX)));if(!item)throw std::runtime_error("invalid ItemKey");out.push_back({*item,static_cast<std::uint16_t>(integer(v,"level",255)),static_cast<std::uint32_t>(integer(v,"count",50000))});}return out;};
        const auto c=store::CurrentCatalog();if(!c)return std::unexpected("item catalog not loaded");
        std::map<std::uint32_t,std::shared_ptr<const items::ItemMissionBoard>> next;auto registry=std::make_shared<missions::DefinitionRegistry>();std::set<std::string> codes;
        for(const auto& b:document.at("boards"))
        {
            if(!b.is_object() || b.size()!=3 || !b.at("missions").is_array() || b.at("missions").empty() || b.at("missions").size()>items::MaxItemMissions)return std::unexpected("invalid item mission board");
            auto board=std::make_shared<items::ItemMissionBoard>();board->npcRace=static_cast<std::uint32_t>(integer(b,"npc_race",UINT32_MAX));board->name=b.at("name").get<std::string>();std::set<std::uint32_t> ids;
            if(!board->npcRace || next.contains(board->npcRace))return std::unexpected("duplicate or invalid NPC race");
            for(const auto& value:b.at("missions"))
            {
                if(!value.is_object() || value.size()!=11)return std::unexpected("invalid item mission");
                items::ItemMission m{static_cast<std::uint32_t>(integer(value,"id",UINT32_MAX)),static_cast<std::uint32_t>(integer(value,"version",UINT32_MAX)),value.at("code").get<std::string>(),value.at("name").get<std::string>(),value.at("description").get<std::string>(),static_cast<std::uint16_t>(integer(value,"min_level",255)),amounts(value,"requirements"),amounts(value,"costs"),amounts(value,"rewards"),integer(value,"gold_cost",INT64_MAX),integer(value,"gold_reward",INT64_MAX)};
                if(!ids.insert(m.id).second || !codes.insert(m.code).second || !items::ValidateItemMission(*c,m) || registry->Publish(missions::ItemDeliveryDefinition(m,board->npcRace)).status!=missions::Publication::Status::Published)return std::unexpected("invalid, duplicated or unsupported item mission contract");board->missions.push_back(std::move(m));
            }
            if(!items::EncodeItemMissionBoard(*c,board.get(),1,1,0,Status::Ready,0,{}))return std::unexpected("item mission board cannot be encoded");next.emplace(board->npcRace,std::move(board));
        }
        boards=std::move(next);definitions=std::move(registry);LOG_INFO("TypedItemMissions: {} boards loaded",boards.size());return {};
    }
    catch(const std::exception&){return std::unexpected("cannot parse typed_item_missions.json");}
}
void TickTypedItemMissions()
{
    std::vector<entt::entity> close;for(auto e:g_registry.view<TypedItemMissionSession>())
    {const auto& s=g_registry.get<TypedItemMissionSession>(e);if(!s.pending && (!Admitted(e) || !InRange(e,s.npc,s.race)))close.push_back(e);}
    for(auto e:close)Close(e);
}
bool HandleTypedItemMissionPacket(entt::entity e,const char* data)
{
    static_assert(sizeof(TPacketCGTypedItemMission)==26);const auto r=*reinterpret_cast<const TPacketCGTypedItemMission*>(data);
    if(!g_registry.valid(e))return true;
    if(r.action==1){if(!r.session && !r.request && !r.mission)Open(e,CHARACTER_MANAGER::instance().FindEntity(r.npc));else Reply(e,Status::Invalid);return true;}
    auto* s=g_registry.try_get<TypedItemMissionSession>(e);
    if(!s || s->token!=r.session || s->vid!=r.npc){Reply(e,Status::Stale);return true;}
    if(s->pending){Reply(e,Status::Pending);return true;}
    if(r.action==3 && !r.request && !r.mission){Close(e);return true;}
    if(r.action!=2){Reply(e,Status::Invalid);return true;}
    if(!r.request || r.request!=s->request+1)
    {Reply(e,r.request==s->request && r.mission==s->selected ? s->status : Status::Stale);return true;}
    s->request=r.request;s->selected=r.mission;s->status=Status::Invalid;
    if(!Admitted(e) || !InRange(e,s->npc,s->race)){Close(e);return true;}
    if(g_registry.all_of<ecs::TypedItemBusy>(e)){s->status=Status::Busy;Reply(e,s->status);return true;}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);
    if(!permit || !permit->Current()){Reply(e,Status::Denied);return true;}
    s=g_registry.try_get<TypedItemMissionSession>(e);const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);
    if(!s || s->token!=r.session || !typed || !login || !Admitted(e) || !InRange(e,s->npc,s->race) || g_registry.all_of<ecs::TypedItemBusy>(e))return true;
    const auto board=boards.at(s->race);const auto selected=std::ranges::find(board->missions,r.mission,&items::ItemMission::id);
    if(selected==board->missions.end()){Reply(e,Status::Invalid);return true;}
    const auto fence=typed->state.fence;const auto token=login->token;const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto level=ecs::PointSystem::GetLevel(e);
    const auto begun=Now();auto delivery=missions::BeginItemDelivery(definitions,*selected,{pid,token},s->race,begun);
    if(!delivery){Reply(e,Status::Failed);return true;}
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));auto flow=std::make_shared<missions::ItemDelivery>(std::move(*delivery));const auto mission=*selected;
    const auto finish=std::make_shared<std::function<void(DeliveryResult)>>([pid,token,nonce=r.session,request=r.request,id=r.mission,held,flow,begun](DeliveryResult result) mutable {
        const auto e=Current(pid,token);if(e==entt::null)return;
        if(result.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*result.state));if(Current(pid,token)!=e)return;
        g_registry.remove<ecs::TypedItemBusy>(e);if(Current(pid,token)!=e)return;
        if(result.committed && !missions::FinishItemDelivery(*flow,std::max(begun,Now())))
        {LOG_ERROR("TypedItemMissions: confirmed item receipt could not acknowledge its pinned Mission action for pid {}",pid);result.status=Status::Failed;}
        auto* s=g_registry.try_get<TypedItemMissionSession>(e);if(s && s->token==nonce && s->request==request)
        {s->pending=false;s->status=result.status;if(result.committed && std::ranges::find(s->completed,id)==s->completed.end())s->completed.push_back(id);Reply(e,s->status);}else SendTypedInventory(e);
        PumpTypedItemGrants(e);
    });
    s->pending=true;s->status=Status::Pending;g_registry.insert<ecs::TypedItemBusy>(&e,&e+1);
    if(Current(pid,token)!=e || !held->Current()){(*finish)({});return true;}
    const auto proof=missions::ItemDeliveryProof(mission,pid);
    const bool queued=store::ItemStore::Instance().Submit([fence,mission,proof,level,finish,held,pid,token](CAsyncSQL& sql,const items::ItemCatalog& c)->store::ItemStore::Completion {
        DeliveryResult result;result.state=items::persistence::LoadItemOwner(sql,c,fence.owner,fence.sessionGeneration);
        if(!result.state)return [finish,result=std::move(result)]() mutable {(*finish)(std::move(result));};
        const auto receipt=items::persistence::FindItemMissionReceipt(sql,c,fence,mission,proof);
        if(!receipt)return [finish,result=std::move(result)]() mutable {(*finish)(std::move(result));};
        if(*receipt){result.status=Status::AlreadyCompleted;result.committed=true;return [finish,result=std::move(result)]() mutable {(*finish)(std::move(result));};}
        if(result.state->fence!=fence){result.status=Status::Stale;return [finish,result=std::move(result)]() mutable {(*finish)(std::move(result));};}
        std::vector<items::ItemInstanceId> ids;
        if(!mission.rewards.empty())
        {
            const auto reserved=items::persistence::ReserveItemInstanceIds(sql,c,items::MaxItemCommitItems);
            if(!reserved)return [finish,result=std::move(result)]() mutable {(*finish)(std::move(result));};
            for(auto id=reserved->first;id<reserved->end;++id)ids.push_back(items::ItemInstanceId::Create(id).value());
        }
        auto commit=items::MakeItemMissionCommit(c,mission,proof,static_cast<std::uint16_t>(level),fence,result.state->storages,result.state->items,result.state->wallets,ids,TypedItemEffectNow());
        if(!commit){result.status=Refusal(commit.error());return [finish,result=std::move(result)]() mutable {(*finish)(std::move(result));};}
        return [finish,held,pid,token,fence,commit=std::move(*commit),state=std::move(result.state)]() mutable {
            if(Current(pid,token)==entt::null || !held->Current()){(*finish)({Status::Failed,false,std::move(state)});return;}
            if(!store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(commit),[finish](store::CommitOutcome outcome) mutable {
                (*finish)({outcome.write ? Status::Completed : Status::Failed,outcome.write.has_value(),std::move(outcome.state)});
            }))(*finish)({Status::Failed,false,std::move(state)});
        };
    });
    if(!queued)(*finish)({});return true;
}
