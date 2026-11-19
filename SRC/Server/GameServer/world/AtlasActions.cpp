#include "stdafx.h"
#include "AtlasActions.hpp"
#include "sectree_manager.h"
#include "../core/AtlasTeleportRules.hpp"
#include "../core/utils.h"
#include "../core/config.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../entity/char_manager.h"
#include "../item/ItemStore.hpp"
#include "../item/TypedInventorySync.hpp"
#include "../item/TypedDrops.hpp"
#include "../item/ItemCatalogRuntime.hpp"
#include "../network/desc.h"
#include <openssl/rand.h>
#include <algorithm>
#include <chrono>
#include <memory>

namespace {
namespace item=auriga::item;
namespace store=item::runtime;
namespace sql=item::persistence;
namespace P=ecs::PlayerRuntime;
struct AtlasCooldown {std::chrono::steady_clock::time_point until;};
int Remaining(entt::entity e)
{
    const auto* timer=g_registry.try_get<AtlasCooldown>(e);
    return timer?static_cast<int>(std::max<std::int64_t>(0,std::chrono::ceil<std::chrono::seconds>(timer->until-std::chrono::steady_clock::now()).count())):0;
}
void StartCooldown(entt::entity e)
{
    g_registry.emplace_or_replace<AtlasCooldown>(e,std::chrono::steady_clock::now()+std::chrono::seconds(auriga::atlas::CooldownSeconds));
    P::SetGoToXYTime(e);
}
void Reply(entt::entity e,const char* status)
{
    if(g_registry.valid(e) && P::GetDesc(e))ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"atlas_warp %s %d",status,Remaining(e));
}
bool Admitted(entt::entity e,int map)
{
    return g_registry.valid(e) && P::GetDesc(e) && P::GetMapIndex(e)==map && map>0 && map<10000 &&
        P::CanWarp(e) && !P::IsObserverMode(e) && !CombatSystem::IsDead(e) && !CombatSystem::IsStun(e);
}
item::ItemCommit WalletCommit(const item::ItemCatalog& c,const sql::LoadedItemOwner& state,
    const item::ItemWallet& wallet,std::array<std::uint8_t,16> operation,bool refund,int map,int x,int y)
{
    item::ItemCommit commit;commit.operationId=operation;commit.catalogRevision=c.Revision();commit.catalogHash=c.ContentHash();
    commit.kind=item::ItemOperationKind::Service;commit.outcomeCode=refund?0x4102:0x4101;commit.owners={state.fence};
    auto after=wallet;++after.revision;after.balance=refund?wallet.balance+auriga::atlas::Fee:wallet.balance-auriga::atlas::Fee;
    commit.wallets.push_back({wallet,after});commit.result={'A','T','L',1,static_cast<std::uint8_t>(refund)};
    for(auto value:{map,x,y})for(unsigned n=0;n<4;n++)commit.result.push_back(static_cast<std::uint32_t>(value)>>(8*n));
    return commit;
}
struct Refund {
    item::ItemOwner owner;std::array<std::uint8_t,16> operation;std::optional<item::ItemCommit> pending;
};
void QueueRefund(std::shared_ptr<Refund> refund)
{
    store::ItemStore::Instance().Submit([refund](CAsyncSQL& db,const item::ItemCatalog& c)->store::ItemStore::Completion {
        bool retry=true;std::optional<sql::LoadedItemOwner> fresh;
        if(!refund->pending){
            auto loaded=sql::LoadCurrentItemOwner(db,c,refund->owner);
            if(loaded){
                auto wallet=std::ranges::find(loaded->wallets,item::WalletCurrency::Gold,&item::ItemWallet::currency);
                if(wallet!=loaded->wallets.end() && wallet->balance<=INT64_MAX-auriga::atlas::Fee)
                    refund->pending=WalletCommit(c,*loaded,*wallet,refund->operation,true,0,0,0);
            }
        }
        if(refund->pending){
            auto result=sql::CommitItems(db,c,*refund->pending);
            if(result){retry=false;auto loaded=sql::LoadCurrentItemOwner(db,c,refund->owner);if(loaded)fresh=std::move(*loaded);}
            else if(result.error()!=item::ItemPersistenceError::CommitUnknown)refund->pending.reset();
        }
        return [refund,retry,fresh=std::move(fresh)]()mutable {
            if(retry){store::ItemStore::Instance().Submit([refund](CAsyncSQL&,const item::ItemCatalog&)->store::ItemStore::Completion{return [refund]{QueueRefund(refund);};},std::chrono::seconds(1));return;}
            if(fresh){NotifyRemoteTypedItemOwner(*fresh);RefreshRemoteTypedItemOwner(static_cast<std::uint32_t>(refund->owner.id),fresh->fence.sessionGeneration,fresh->fence.revision);}
        };
    });
}
}

void SendAtlasInfo(entt::entity e)
{
    if(!g_registry.valid(e) || !P::GetDesc(e))return;
    const int map=P::GetMapIndex(e);const auto* region=SECTREE_MANAGER::instance().GetMapRegion(map>=10000?map/10000:map);
    if(!region)return;
    auto* tree=P::GetSectree(e);const bool safe=tree && tree->IsAttr(P::GetX(e),P::GetY(e),ATTR_BANPK);
    ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"atlas_info %d %d %d %d %d %llu %d %d",map,region->sx,region->sy,
        region->ex-region->sx,region->ey-region->sy,static_cast<unsigned long long>(auriga::atlas::Fee),Remaining(e),safe?1:0);
}

void RequestAtlasTeleport(entt::entity e,const char* arguments)
{
    if(!g_registry.valid(e))return;
    char first[256],second[256],extra[256];
    const char* tail=two_arguments(arguments,first,sizeof(first),second,sizeof(second));one_argument(tail,extra,sizeof(extra));
    int x=0,y=0;
    if(*extra || !auriga::atlas::ParseCoordinate(first,x) || !auriga::atlas::ParseCoordinate(second,y)){Reply(e,"invalid");return;}
    const int map=P::GetMapIndex(e);
    if(!Admitted(e,map)){Reply(e,"blocked");return;}
    if(Remaining(e)){Reply(e,"cooldown");return;}
    if(g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,"busy");return;}
    const auto* region=SECTREE_MANAGER::instance().GetMapRegion(map);int gx=0,gy=0;
    if(!region || !auriga::atlas::Destination(x,y,region->sx,region->sy,region->ex,region->ey,gx,gy) ||
       !SECTREE_MANAGER::instance().IsMovablePosition(map,gx,gy)){Reply(e,"invalid");return;}
    auto* typed=g_registry.try_get<ecs::TypedItems>(e);
    const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);
    if(!typed){Reply(e,login && login->mode==ecs::TypedItemLogin::Mode::Pending?"busy":"failed");return;}
    const auto catalog=store::CurrentCatalog();
    if(!catalog || !login || login->mode!=ecs::TypedItemLogin::Mode::Native){Reply(e,"failed");return;}
    const auto wallet=std::ranges::find(typed->state.wallets,item::WalletCurrency::Gold,&item::ItemWallet::currency);
    if(wallet==typed->state.wallets.end() || wallet->balance<auriga::atlas::Fee){Reply(e,"money");return;}
    std::array<std::uint8_t,16> op{};if(RAND_bytes(op.data(),static_cast<int>(op.size()))!=1){Reply(e,"failed");return;}
    auto commit=WalletCommit(*catalog,typed->state,*wallet,op,false,map,x,y);
    const auto pid=P::GetPlayerID(e);const auto token=login->token;const auto owner=typed->state.fence.owner;
    g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);Reply(e,"pending");
    if(!store::RequestCommit(owner,typed->state.fence.sessionGeneration,std::move(commit),[=](store::CommitOutcome out){
        auto current=CHARACTER_MANAGER::instance().FindEntityByPID(pid);
        auto* active=current!=entt::null && g_registry.valid(current)?g_registry.try_get<ecs::TypedItemLogin>(current):nullptr;
        bool connected=active && active->token==token;
        if(connected){
            g_registry.remove<ecs::TypedItemBusy>(current);
            if(out.state)g_registry.emplace_or_replace<ecs::TypedItems>(current,std::move(*out.state));
        }
        if(out.write){
            if(connected && Admitted(current,map) && ecs::MovementSystem::Show(current,map,gx,gy)){
                ecs::MovementSystem::Stop(current);StartCooldown(current);Reply(current,"ok");
            }else{
                auto refund=std::make_shared<Refund>();refund->owner=owner;refund->operation=op;refund->operation[0]^=0x80;
                QueueRefund(refund);if(connected)Reply(current,"failed");
            }
        }else if(connected)Reply(current,"failed");
        if(connected){SendTypedInventory(current);PumpTypedItemGrants(current);}
    })){g_registry.remove<ecs::TypedItemBusy>(e);Reply(e,"failed");}
}
