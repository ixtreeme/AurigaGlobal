#include "stdafx.h"
#include "TypedRecovery.hpp"
#include "TypedShop.hpp"
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
#include <item/NpcShops.hpp>
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
struct Session
{
    entt::entity npc=entt::null;
    std::uint32_t vid=0,race=0;
    std::uint64_t token=0,lastRequest=0;
    Status lastStatus=Status::Ready;
    std::uint8_t lastAction=0; std::uint64_t lastSubject=0; std::uint32_t lastCount=0;
};
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
    const auto found=session ? shops.find(session->race) : shops.end();
    const auto packet=items::EncodeNpcShop(*catalog,found==shops.end() ? nullptr : &found->second,session ? session->vid : 0,
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
    case items::NpcShopError::NoRoom:return Status::NoRoom;
    case items::NpcShopError::WalletFull:return Status::WalletFull;
    case items::NpcShopError::Unavailable:return Status::Failed;
    default:return Status::Invalid;
    }
}
}

std::expected<void,std::string> LoadTypedShops(const std::string& file)
{
    try
    {
        std::map<std::uint32_t,items::NpcShop> next;
        if(!std::filesystem::exists(file)){shops.clear();return {};}
        if(std::filesystem::file_size(file)>(1u<<20))return std::unexpected("typed_shops.json exceeds 1 MiB");
        std::ifstream stream(file);const auto json=nlohmann::json::parse(stream);
        if(!json.is_object() || json.size()!=2 || json.at("version")!=1 || !json.at("shops").is_array() ||
           json.at("shops").size()>items::MaxNpcShops)return std::unexpected("invalid typed shop document");
        const auto readNumber=[](const nlohmann::json& row,const char* key,std::uint64_t max)->std::uint32_t {
            const auto& value=row.at(key);
            if(!value.is_number_unsigned() || value.get<std::uint64_t>()>max)throw std::runtime_error("invalid shop integer");
            return value.get<std::uint32_t>();
        };
        std::vector<items::NpcShop> definitions;
        for(const auto& row:json.at("shops"))
        {
            if(!row.is_object() || row.size()!=3 || !row.at("offers").is_array() || row.at("offers").size()>items::MaxNpcShopOffers)
                return std::unexpected("invalid typed shop listing");
            items::NpcShop shop{readNumber(row,"npc_race",UINT32_MAX),row.at("name").get<std::string>(),{}};
            for(const auto& offer:row.at("offers"))
            {
                if(!offer.is_object() || offer.size()!=5)return std::unexpected("invalid typed shop offer");
                const auto key=items::ItemKey::Create(readNumber(offer,"category",UINT16_MAX),readNumber(offer,"local",UINT32_MAX));
                if(!key)return std::unexpected("invalid typed shop ItemKey");
                shop.offers.push_back({readNumber(offer,"id",UINT32_MAX),{*key,static_cast<std::uint16_t>(readNumber(offer,"level",UINT16_MAX)),readNumber(offer,"count",UINT32_MAX)}});
            }
            definitions.push_back(std::move(shop));
        }
        const auto catalog=items::runtime::CurrentCatalog();
        if(!catalog || !items::ValidateNpcShops(*catalog,definitions))return std::unexpected("typed shop listing does not match the item catalog");
        for(auto& shop:definitions)next.emplace(shop.npcRace,std::move(shop));
        shops=std::move(next);LOG_INFO("TypedShop: {} NPC listings loaded",shops.size());return {};
    }
    catch(const std::exception&){return std::unexpected("cannot parse typed_shops.json");}
}
bool TryOpenTypedShop(entt::entity character,entt::entity npc)
{
    if(!g_registry.valid(character) || !g_registry.valid(npc) || !g_registry.all_of<ecs::TypedItems>(character))return false;
    const auto race=ecs::PlayerRuntime::GetRaceNum(npc);
    // A native owner must not enter a legacy shop when this NPC has no native stock.
    if(!shops.contains(race)){Reply(character,Status::Denied);return true;}
    if(!Admitted(character,false) || !InRange(character,npc) || g_registry.all_of<ecs::TypedItemBusy>(character))
    {Reply(character,Status::Denied);return true;}
    const auto permission=ecs::resource_admission::Enter({character},auriga::resource_admission::Operation::Shop);
    if(!permission || !permission->Current())return true;
    CloseTypedShop(character);
    if(!permission->Current() || !Admitted(character,false) || !InRange(character,npc))return true;
    auto op=Operation();std::uint64_t token=0;for(unsigned i=0;i<8;++i)token|=std::uint64_t{op[i]}<<(8*i);
    g_registry.emplace<Session>(character,npc,ecs::PlayerRuntime::GetPacketVID(npc),race,token);
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
        const auto& offers=shops.at(session->race).offers;
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
