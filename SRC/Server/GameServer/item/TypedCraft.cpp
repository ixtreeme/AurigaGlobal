#include "stdafx.h"
#include "TypedRecovery.hpp"
#include "TypedCraft.hpp"
#include "TypedDrops.hpp"
#include "TypedInventorySync.hpp"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../quest/questpc.h"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include <item/Crafting.hpp>
#include <item/CraftConfig.hpp>
#include <Core/Logging.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <random>

namespace
{
namespace items=auriga::item;
namespace store=items::runtime;
using Status=items::CraftStatus;
std::map<std::uint32_t,items::CraftWorkshop> workshops;
// Keep the component name unique across translation units: EnTT hashes type
// names, including names inside anonymous namespaces.
struct TypedCraftSession
{
    entt::entity npc=entt::null;
    std::uint32_t vid=0,race=0,recipe=0,attempts=0,successes=0;
    std::uint64_t token=0,request=0,source=0;
    std::uint8_t lastAction=0;std::uint32_t lastRecipe=0,lastCount=0;
    Status status=Status::Ready;
    bool pending=false;
};
std::mt19937_64& Engine(){static std::mt19937_64 engine{std::random_device{}()};return engine;}
std::array<std::uint8_t,16> Operation()
{std::array<std::uint8_t,16> out{};for(auto& b:out)b=static_cast<std::uint8_t>(Engine()());out[0]|=1;return out;}
bool InRange(entt::entity e,entt::entity npc)
{
    if(!g_registry.valid(npc) || !ecs::PlayerRuntime::IsValid(npc) || ecs::PlayerRuntime::IsPC(npc) ||
       ecs::PlayerRuntime::GetMapIndex(e)!=ecs::PlayerRuntime::GetMapIndex(npc))return false;
    const auto dx=std::abs(std::int64_t(ecs::PlayerRuntime::GetX(e))-ecs::PlayerRuntime::GetX(npc));
    const auto dy=std::abs(std::int64_t(ecs::PlayerRuntime::GetY(e))-ecs::PlayerRuntime::GetY(npc));return std::max(dx,dy)+std::min(dx,dy)/2<1000;
}
bool Admitted(entt::entity e,bool write)
{
    if(!g_registry.valid(e) || !ecs::PlayerRuntime::IsPC(e) || CombatSystem::IsDead(e) || !InventorySystem::CanHandleItems(e) ||
       ecs::SocialSystem::HasExchange(e) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SocialSystem::GetMyShop(e)!=entt::null ||
       ecs::SocialSystem::GetShopOwner(e)!=entt::null || !g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e))return false;
    if(ecs::SessionSystem::IsCubeOpen(e) && !g_registry.all_of<TypedCraftSession>(e))return false;
    const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);if(flags && (flags->isObserverMode || flags->isStunned))return false;
    auto* desc=ecs::PlayerRuntime::GetDesc(e);if(!desc || !desc->IsPhase(PHASE_GAME))return false;
    if(write){const auto* quest=g_registry.try_get<ecs::QuestPCState>(e);if(!quest || !quest->pc || quest->pc->IsRunning())return false;}
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    if(ecs::PlayerRuntime::GetGMLevel(e)>GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(e)<GM_IMPLEMENTOR)return false;
#endif
    return true;
}
void Reply(entt::entity e,Status status)
{
    if(!g_registry.valid(e))return;const auto catalog=store::CurrentCatalog();auto* desc=ecs::PlayerRuntime::GetDesc(e);
    if(!catalog || !desc || !desc->IsPhase(PHASE_GAME))return;
    const auto* s=g_registry.try_get<TypedCraftSession>(e);const auto found=s ? workshops.find(s->race) : workshops.end();
    const auto packet=items::EncodeCraftWorkshop(*catalog,found==workshops.end() ? nullptr : &found->second,s ? s->vid : 0,
        s ? s->token : 0,s ? s->request : 0,status,s ? s->recipe : 0,s ? s->attempts : 0,s ? s->successes : 0,s ? s->source : 0);
    SendTypedInventory(e);if(packet)desc->LargePacket(packet->data(),static_cast<int>(packet->size()));else LOG_ERROR("TypedCraft: response could not be encoded");
}
Status Refusal(items::CraftError e)
{
    switch(e){case items::CraftError::NoMaterials:return Status::NoMaterials;case items::CraftError::NoMoney:return Status::NoMoney;
    case items::CraftError::NoRoom:return Status::NoRoom;case items::CraftError::Unavailable:return Status::Failed;
    case items::CraftError::SelectSource:return Status::SelectSource;case items::CraftError::Incompatible:return Status::Incompatible;case items::CraftError::NoGaya:return Status::NoGaya;
    case items::CraftError::Limit:return Status::Limit;default:return Status::Invalid;}
}
}
std::expected<void,std::string> LoadTypedCrafting(const std::string& path)
{
    try
    {
        if(!std::filesystem::exists(path)){workshops.clear();return {};}
        if(std::filesystem::file_size(path)>(1u<<20))return std::unexpected("typed_crafting.json exceeds 1 MiB");
        std::ifstream stream(path);const std::string text{std::istreambuf_iterator<char>{stream},{}};
        const auto catalog=store::CurrentCatalog();if(!catalog)return std::unexpected("item catalog unavailable");
        auto parsed=items::ParseCraftWorkshops(text,*catalog);if(!parsed)return std::unexpected(parsed.error());
        auto definitions=std::move(*parsed);
        std::map<std::uint32_t,items::CraftWorkshop> next;for(auto& w:definitions)next.emplace(w.npcRace,std::move(w));workshops=std::move(next);
        LOG_INFO("TypedCraft: {} workshops loaded",workshops.size());return {};
    }
    catch(const std::exception&){return std::unexpected("cannot parse typed_crafting.json");}
}
bool TryOpenTypedCraft(entt::entity e,entt::entity npc)
{
    if(!g_registry.valid(e) || !g_registry.valid(npc) || !g_registry.all_of<ecs::TypedItems>(e))return false;
    const auto race=ecs::PlayerRuntime::GetRaceNum(npc);if(!workshops.contains(race))return false;
    if(!Admitted(e,false) || !InRange(e,npc) || g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Denied);return true;}
    auto permission=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);
    if(!permission || !permission->Current()){Reply(e,Status::Denied);return true;}
    CloseTypedCraft(e);
    if(!permission->Current() || !Admitted(e,false) || !InRange(e,npc) || g_registry.all_of<ecs::TypedItemBusy>(e))return true;
    const auto op=Operation();std::uint64_t token=0;for(unsigned i=0;i<8;++i)token|=std::uint64_t{op[i]}<<(8*i);
    TypedCraftSession session;session.npc=npc;session.vid=ecs::PlayerRuntime::GetPacketVID(npc);session.race=race;session.token=token;
    g_registry.emplace_or_replace<TypedCraftSession>(e,session);g_registry.emplace_or_replace<ecs::TypedCraftActive>(e);Reply(e,Status::Ready);return true;
}
void CloseTypedCraft(entt::entity e)
{
    if(!g_registry.valid(e))return;const auto* s=g_registry.try_get<TypedCraftSession>(e);if(!s)return;
    if(s->pending){Reply(e,Status::Pending);return;}
    g_registry.remove<TypedCraftSession,ecs::TypedCraftActive>(e);Reply(e,Status::Closed);
}
void TickTypedCrafting()
{
    std::vector<entt::entity> closed;for(auto e:g_registry.view<TypedCraftSession>())
    {const auto& s=g_registry.get<TypedCraftSession>(e);if(!s.pending && (!InRange(e,s.npc) || !Admitted(e,false)))closed.push_back(e);}
    for(auto e:closed)CloseTypedCraft(e);
}
bool HandleTypedCraftPacket(entt::entity e,const char* data)
{
    static_assert(sizeof(TPacketCGTypedCraft)==30);const auto r=*reinterpret_cast<const TPacketCGTypedCraft*>(data);
    if(!g_registry.valid(e))return true;
    if(r.action==1)
    {if(!r.session && !r.request && !r.recipe && !r.count && !TryOpenTypedCraft(e,CHARACTER_MANAGER::instance().FindEntity(r.npc)))Reply(e,Status::Denied);return true;}
    auto* s=g_registry.try_get<TypedCraftSession>(e);
    if(!s || s->token!=r.session || s->vid!=r.npc){Reply(e,Status::Stale);return true;}
    if(r.action==3){CloseTypedCraft(e);return true;}
    if(s->pending){Reply(e,Status::Pending);return true;}
    if(r.action!=2 && r.action!=4){Reply(e,Status::Invalid);return true;}
    if(!r.request || r.request!=s->request+1)
    {Reply(e,r.request==s->request && r.action==s->lastAction && r.recipe==s->lastRecipe && r.count==s->lastCount ? s->status : Status::Stale);return true;}
    s->request=r.request;s->lastAction=r.action;s->lastRecipe=r.recipe;s->lastCount=r.count;s->recipe=r.action==2?r.recipe:0;s->attempts=r.action==2?r.count:0;s->successes=0;s->status=Status::Invalid;
    if(!Admitted(e,true) || !InRange(e,s->npc)){CloseTypedCraft(e);return true;}
    if(g_registry.all_of<ecs::TypedItemBusy>(e)){s->status=Status::Busy;Reply(e,s->status);return true;}
    auto permission=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);
    if(!permission || !permission->Current()){s->status=Status::Denied;Reply(e,s->status);return true;}
    const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);s=g_registry.try_get<TypedCraftSession>(e);
    const auto catalog=store::CurrentCatalog();
    if(!typed || !login || !s || s->token!=r.session || s->request!=r.request || !catalog || !Admitted(e,true) || !InRange(e,s->npc) || g_registry.all_of<ecs::TypedItemBusy>(e))return true;
    const auto& recipes=workshops.at(s->race).recipes;const auto recipe=std::ranges::find(recipes,r.recipe,&items::CraftRecipe::id);
    if(r.action==4) {
        const auto id=std::uint64_t{r.recipe}|(std::uint64_t{r.count}<<32);
        const auto found=std::ranges::find_if(typed->state.items,[&](const auto& item){return item.id.Value()==id;});
        if(!id || (found!=typed->state.items.end() && std::ranges::any_of(recipes,[&](const auto& candidate){return candidate.copySource && items::CraftCopySourceUsable(*found,typed->state.fence.owner,*candidate.copySource);})))
        {s->source=id;s->status=Status::Ready;}else s->status=Status::Invalid;
        Reply(e,s->status);return true;
    }
    if(recipe==recipes.end() || !r.count || r.count>items::MaxCraftBatch){Reply(e,Status::Invalid);return true;}
    std::vector<std::uint16_t> rolls;std::uniform_int_distribution<unsigned> roll(0,9999);for(unsigned i=0;i<r.count;++i)rolls.push_back(static_cast<std::uint16_t>(roll(Engine())));
    const auto& state=typed->state;const auto fence=state.fence;const auto token=login->token;
    auto plan=items::MakeCraftCommit(*catalog,Operation(),fence,state.storages,state.items,state.wallets,*recipe,r.count,rolls,store::PeekInstanceIds(items::MaxItemCommitItems),TypedItemEffectNow(),recipe->copySource?s->source:0);
    if(!plan){s->status=Refusal(plan.error());Reply(e,s->status);return true;}
    const auto successes=plan->successes;const auto pid=ecs::PlayerRuntime::GetPlayerID(e);
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permission));
    if(!held->Current())return true;
    store::SpendInstanceIds(std::ranges::count_if(plan->commit.items,[](const auto& change){return !change.before;}));s->pending=true;s->status=Status::Pending;
    g_registry.insert<ecs::TypedItemBusy>(&e,&e+1);
    const auto* live=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;
    if(!live || live->token!=token || !held->Current())
    {
        if(live && live->token==token)
        {g_registry.remove<ecs::TypedItemBusy>(e);s=g_registry.try_get<TypedCraftSession>(e);if(s && s->token==r.session){s->pending=false;s->status=Status::Failed;Reply(e,s->status);}}
        return true;
    }
    const auto complete=[pid,token,nonce=r.session,request=r.request,successes,held](store::CommitOutcome outcome) {
        const auto current=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* login=current!=entt::null && g_registry.valid(current) ? g_registry.try_get<ecs::TypedItemLogin>(current) : nullptr;
        if(!login || login->token!=token)return;
        if(outcome.state)g_registry.emplace_or_replace<ecs::TypedItems>(current,std::move(*outcome.state));g_registry.remove<ecs::TypedItemBusy>(current);
        auto* s=g_registry.try_get<TypedCraftSession>(current);
        if(s && s->token==nonce && s->request==request)
        {s->pending=false;s->status=outcome.write ? Status::Crafted : Status::Failed;s->successes=outcome.write ? successes : 0;Reply(current,s->status);}
        else SendTypedInventory(current);
        PumpTypedItemGrants(current);
    };
    if(!store::RequestCommit(fence.owner,fence.sessionGeneration,std::move(plan->commit),complete))
    {g_registry.remove<ecs::TypedItemBusy>(e);s=g_registry.try_get<TypedCraftSession>(e);if(s && s->token==r.session){s->pending=false;s->status=Status::Failed;Reply(e,s->status);}}
    return true;
}
