#include "stdafx.h"
#include "TypedRecovery.hpp"
#include "TypedShop.hpp"
#include "TypedPrivateShops.hpp"
#include "TypedItemOps.hpp"
#include "TypedDrops.hpp"
#include "../social/shop.h"
#include "TypedInventorySync.hpp"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../quest/questpc.h"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include "../core/config.h"
#include <item/NpcShops.hpp>
#include <item/NpcShopConfig.hpp>
#include <Core/Logging.hpp>
#include <Core/shop_limits.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <map>
#include <random>

namespace
{
namespace items=auriga::item;
using Status=items::NpcShopStatus;
std::map<std::uint32_t,items::NpcShop> shops;
std::map<std::pair<std::uint32_t,std::uint32_t>,items::NpcShop> routedShops;
struct Session
{
    entt::entity npc=entt::null;
    std::uint32_t vid=0,race=0;
    std::uint64_t token=0,lastRequest=0;
    Status lastStatus=Status::Ready;
    std::uint8_t lastAction=0; std::uint64_t lastSubject=0; std::uint32_t lastCount=0;
    std::uint32_t route=0;
};
const items::NpcShop* Stock(std::uint32_t race,std::uint32_t route)
{
    if(route){const auto it=routedShops.find({race,route});return it==routedShops.end()?nullptr:&it->second;}
    const auto it=shops.find(race);return it==shops.end()?nullptr:&it->second;
}
std::optional<items::NpcShop> QuotedStock(entt::entity character,const Session& session)
{
    const auto* raw=Stock(session.race,session.route);const auto catalog=items::runtime::CurrentCatalog();
    if(!raw || !catalog || !g_registry.valid(session.npc))return {};
    auto quoted=*raw;
    if(!g_bEmpireShopPriceTripleDisable && ecs::PlayerRuntime::GetEmpire(character)!=ecs::PlayerRuntime::GetEmpire(session.npc))
        for(auto& offer:quoted.offers) {
            const auto price=items::NpcBuyBundlePrice(*catalog,offer);if(!price || *price>std::uint64_t(INT64_MAX)/3)return {};
            offer.bundleGold=*price*3;
        }
    return quoted;
}
std::array<std::uint8_t,16> Operation()
{
    static std::mt19937_64 random{std::random_device{}()};
    std::array<std::uint8_t,16> bytes{};for(auto& b:bytes)b=static_cast<std::uint8_t>(random());bytes[0]|=1;return bytes;
}
bool InRange(entt::entity character,entt::entity npc)
{
    if(!g_registry.valid(npc) || !ecs::PlayerRuntime::IsValid(npc) || ecs::PlayerRuntime::IsPC(npc) ||
       ecs::PlayerRuntime::GetMapIndex(character)!=ecs::PlayerRuntime::GetMapIndex(npc))return false;
    const auto dx=std::abs(std::int64_t(ecs::PlayerRuntime::GetX(character))-ecs::PlayerRuntime::GetX(npc));
    const auto dy=std::abs(std::int64_t(ecs::PlayerRuntime::GetY(character))-ecs::PlayerRuntime::GetY(npc));
    return std::max(dx,dy)+std::min(dx,dy)/2<SHOP_MAX_DISTANCE;
}
bool Admitted(entt::entity e,bool transaction)
{
    if(!g_registry.valid(e) || !ecs::PlayerRuntime::IsPC(e) || CombatSystem::IsDead(e) || ecs::PlayerRuntime::IsObserverMode(e) ||
       !InventorySystem::CanHandleItems(e) || ecs::SocialSystem::HasExchange(e) || ecs::SessionSystem::IsSafeboxOpen(e) ||
       ecs::SessionSystem::IsCubeOpen(e) || ecs::SocialSystem::GetMyShop(e)!=entt::null)return false;
    const auto* s=g_registry.try_get<Session>(e);
    if(ecs::SocialSystem::GetShopOwner(e)!=entt::null && (!s || ecs::SocialSystem::GetShopOwner(e)!=s->npc))return false;
    if(transaction)
    { const auto* q=g_registry.try_get<ecs::QuestPCState>(e);if(!q || !q->pc || q->pc->IsRunning())return false; }
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    if(ecs::PlayerRuntime::GetGMLevel(e)>GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(e)<GM_IMPLEMENTOR)return false;
#endif
    return g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e);
}
void Reply(entt::entity e,Status status,std::uint64_t request=0)
{
    if(!g_registry.valid(e))return;
    auto* desc=ecs::PlayerRuntime::GetDesc(e);const auto catalog=items::runtime::CurrentCatalog();
    const auto* state=g_registry.try_get<ecs::TypedItems>(e);const auto* session=g_registry.try_get<Session>(e);
    if(!desc || !desc->IsPhase(PHASE_GAME) || !catalog || !state)return;
    const auto found=session ? QuotedStock(e,*session) : std::nullopt;
    const auto packet=items::EncodeNpcShop(*catalog,found ? &*found : nullptr,session ? session->vid : 0,
        session ? session->token : 0,request,status,state->state.items);
    SendTypedInventory(e);
    if(packet)desc->LargePacket(packet->data(),static_cast<int>(packet->size()));
    else LOG_ERROR("TypedShop: response encoding failed for pid {}",ecs::PlayerRuntime::GetPlayerID(e));
}
Status Refusal(items::NpcShopError e)
{
    switch(e)
    {
    case items::NpcShopError::Denied:return Status::Denied;
    case items::NpcShopError::NoMoney:return Status::NoMoney;
    case items::NpcShopError::NoMaterials:return Status::NoMaterials;
    case items::NpcShopError::NoRoom:return Status::NoRoom;
    case items::NpcShopError::WalletFull:return Status::WalletFull;
    case items::NpcShopError::Unavailable:return Status::Failed;
    default:return Status::Invalid;
    }
}
}

std::expected<void,std::string> LoadTypedShops(const std::string& file)
{
    try {
        if(!std::filesystem::exists(file)){shops.clear();routedShops.clear();return {};}
        if(std::filesystem::file_size(file)>(1u<<20))return std::unexpected("typed_shops.json exceeds 1 MiB");
        std::ifstream stream(file);const std::string text{std::istreambuf_iterator<char>(stream),{}};
        const auto catalog=items::runtime::CurrentCatalog();if(!catalog)return std::unexpected("Native shop catalog unavailable");
        auto parsed=items::ParseNpcShopManifest(text,*catalog);if(!parsed)return std::unexpected(parsed.error());
        std::map<std::uint32_t,items::NpcShop> next;std::map<std::pair<std::uint32_t,std::uint32_t>,items::NpcShop> routes;
        for(auto& shop:parsed->shops)next.emplace(shop.npcRace,std::move(shop));
        for(auto& route:parsed->routes)routes.emplace(std::pair{route.stock.npcRace,route.source},std::move(route.stock));
        shops=std::move(next);routedShops=std::move(routes);LOG_INFO("TypedShop: {} NPC listings, {} authored routes loaded",shops.size(),routedShops.size());return {};
    }catch(const std::exception& error){return std::unexpected(error.what());}
}
bool TryOpenTypedShop(entt::entity character,entt::entity npc,std::uint32_t authoredRoute)
{
    if(!g_registry.valid(character) || !g_registry.valid(npc) || !g_registry.all_of<ecs::TypedItems>(character))return false;
    if(TryOpenTypedPrivateShop(character,npc))return true;
    const auto race=ecs::PlayerRuntime::GetRaceNum(npc);
    // A native owner must not enter a legacy shop when this NPC has no native stock.
    if(!Stock(race,authoredRoute)){Reply(character,Status::Denied);return true;}
    if(!Admitted(character,false) || !InRange(character,npc) || g_registry.all_of<ecs::TypedItemBusy>(character))
    {Reply(character,Status::Denied);return true;}
    const auto permission=ecs::resource_admission::Enter({character},auriga::resource_admission::Operation::Shop);
    if(!permission || !permission->Current())return true;
    CloseTypedShop(character);
    if(!permission->Current() || !Admitted(character,false) || !InRange(character,npc))return true;
    auto op=Operation();std::uint64_t token=0;for(unsigned i=0;i<8;++i)token|=std::uint64_t{op[i]}<<(8*i);
    g_registry.emplace<Session>(character,npc,ecs::PlayerRuntime::GetPacketVID(npc),race,token);
    g_registry.get<Session>(character).route=authoredRoute;
    ecs::SocialSystem::SetShopOwner(character,npc);
    Reply(character,Status::Ready);return true;
}
void CloseTypedShop(entt::entity character)
{
    if(!g_registry.valid(character))return;
    const auto* session=g_registry.try_get<Session>(character);if(!session)return;
    if(ecs::SocialSystem::GetShopOwner(character)==session->npc)ecs::SocialSystem::SetShopOwner(character,entt::null);
    g_registry.remove<Session>(character);Reply(character,Status::Closed);
}
void TickTypedShops()
{
    std::vector<entt::entity> closed;
    for(auto e:g_registry.view<Session>())
    {const auto& session=g_registry.get<Session>(e);if(!InRange(e,session.npc) || !Admitted(e,false))closed.push_back(e);}
    for(auto e:closed)CloseTypedShop(e);
}
bool HandleTypedShopPacket(entt::entity character,const char* data)
{
    static_assert(sizeof(TPacketCGTypedShop)==34);
    const auto& request=*reinterpret_cast<const TPacketCGTypedShop*>(data);
    if(!g_registry.valid(character))return true;
    if(request.action==1)
    {
        if(request.session || request.request || request.subject || request.count)return true;
        const auto npc=CHARACTER_MANAGER::instance().FindEntity(request.npc);
        if(!TryOpenTypedShop(character,npc))Reply(character,Status::Denied);
        return true;
    }
    auto* session=g_registry.try_get<Session>(character);
    if(!session || session->token!=request.session || session->vid!=request.npc){Reply(character,Status::Stale,request.request);return true;}
    if(request.action==4){CloseTypedShop(character);return true;}
    if(request.action!=2 && request.action!=3){Reply(character,Status::Invalid,request.request);return true;}
    if(!request.request || request.request!=session->lastRequest+1)
    {
        const bool same=request.request==session->lastRequest && request.action==session->lastAction && request.subject==session->lastSubject && request.count==session->lastCount;
        Reply(character,same ? session->lastStatus : Status::Stale,request.request);return true;
    }
    session->lastRequest=request.request;session->lastStatus=Status::Invalid;
    session->lastAction=request.action;session->lastSubject=request.subject;session->lastCount=request.count;
    if(!Admitted(character,true) || !InRange(character,session->npc)){CloseTypedShop(character);return true;}
    if(g_registry.all_of<ecs::TypedItemBusy>(character)){session->lastStatus=Status::Busy;Reply(character,Status::Busy,request.request);return true;}
    const auto permission=ecs::resource_admission::Enter({character},auriga::resource_admission::Operation::Shop);
    if(!permission || !permission->Current()){session->lastStatus=Status::Denied;Reply(character,Status::Denied,request.request);return true;}
    auto* typed=g_registry.try_get<ecs::TypedItems>(character);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(character);
    const auto catalog=items::runtime::CurrentCatalog();session=g_registry.try_get<Session>(character);
    if(!typed || !login || !session || !catalog || session->token!=request.session || session->vid!=request.npc ||
       session->lastRequest!=request.request || !Admitted(character,true) || !InRange(character,session->npc))return true;
    const auto& state=typed->state;const auto token=login->token;const auto nonce=session->token;
    std::expected<items::ItemCommit,items::NpcShopError> commit=std::unexpected(items::NpcShopError::Invalid);
    if(request.action==2)
    {
        const auto stock=QuotedStock(character,*session);if(!stock){Reply(character,Status::Stale,request.request);return true;}
        const auto& offers=stock->offers;
        const auto offer=std::ranges::find(offers,request.subject,[](const auto& o){return std::uint64_t{o.id};});
        if(offer!=offers.end())commit=items::MakeNpcBuyCommit(*catalog,Operation(),state.fence,state.storages,state.items,state.wallets,*offer,request.count,items::runtime::PeekInstanceIds(items::MaxItemCommitItems),TypedItemEffectNow());
    }
    else if(const auto id=items::ItemInstanceId::Create(request.subject))
        commit=items::MakeNpcSellCommit(*catalog,Operation(),state.fence,state.storages,state.items,state.wallets,*id,request.count);
    if(!commit){session->lastStatus=Refusal(commit.error());Reply(character,session->lastStatus,request.request);return true;}
    if(!permission->Current())return true;
    const auto ids=std::ranges::count_if(commit->items,[](const auto& change){return !change.before;});
    items::runtime::SpendInstanceIds(ids);
    session->lastStatus=Status::Busy;
    const auto pid=ecs::PlayerRuntime::GetPlayerID(character);const auto done=request.action==2 ? Status::Bought : Status::Sold;
    g_registry.emplace_or_replace<ecs::TypedItemBusy>(character);
    const auto complete=[pid,token,nonce,sequence=request.request,done](items::runtime::CommitOutcome outcome) {
        const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);
        const auto* login=e!=entt::null && g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;
        if(!login || login->token!=token)return;
        g_registry.remove<ecs::TypedItemBusy>(e);
        if(outcome.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*outcome.state));
        auto* s=g_registry.try_get<Session>(e);
        if(s && s->token==nonce)
        {const auto status=outcome.write ? done : Status::Failed;if(s->lastRequest==sequence)s->lastStatus=status;Reply(e,status,sequence);}
        else SendTypedInventory(e);
        PumpTypedItemGrants(e);
    };
    if(!items::runtime::RequestCommit(state.fence.owner,state.fence.sessionGeneration,std::move(*commit),complete))
    {g_registry.remove<ecs::TypedItemBusy>(character);session->lastStatus=Status::Failed;Reply(character,Status::Failed,request.request);}
    return true;
}
