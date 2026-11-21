#include "stdafx.h"
#include "wheel_of_destiny.h"
#include "TypedQuestItems.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "utils.h"

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
namespace {
namespace item = auriga::item;
constexpr std::uint64_t WheelPrice=1;
constexpr std::string_view TicketCode="wheel.fortune_ticket";

struct Prize {
    std::string_view code;
    unsigned visual;
    std::uint32_t count;
    unsigned minimumChance;
};

// The legacy image identifiers keep the wheel artwork and animation positions.
constexpr Prize prizes[] = {
    {"catalog.10.gaya_4dbbae9647e8",39066,10,0},
    {"catalog.10.voucher_100_dc_03622683353d",39067,10,25},
    {"catalog.22.mystic_blessing_scroll_b8dbc49e4791",70027,1,0},
    {"catalog.16.voucher_1000_dc_e316f2658298",89007,1,0},
    {"catalog.10.dungeon_point_10af51240afa",99999,200,0},
    {"catalog.22.mystic_blessing_scroll_b8dbc49e4791",70027,3,0},
    {TicketCode,70610,3,0},
    {"catalog.5.golden_fruit_31fe36387997",72100,10,0},
    {"catalog.15.hunting_dog_summoner_273f32f08a28",611586,1,25},
    {"catalog.5.legendary_bonus_043af960ba20",30617,1,0},
    {"catalog.5.legendary_changer_8e172122f0ec",30618,1,0},
    {"catalog.16.gold_ingot_400_million_101fc6bbeec7",80008,5,0},
    {"catalog.16.voucher_500_dc_c5cab7c40efd",89006,1,0},
    {"catalog.5.fruit_of_life_135bea6f7abb",71107,5,0},
    {"material.sarkany_karom",71129,5,0},
    {"material.sarkany_pikkely",71123,5,0},
};

const item::ItemCatalogEntry* Resolve(const item::ItemCatalog& catalog,std::string_view code)
{
    const auto& entries=catalog.Entries();
    const auto at=std::ranges::find_if(entries,[&](const auto& entry){return entry.second.base.contentCode==code;});
    return at!=entries.end() && !at->second.base.retired ? &at->second : nullptr;
}
}

CWheelDestiny::CWheelDestiny(entt::entity character) : owner(character)
{
    for(unsigned i=0;i<std::size(prizes);++i)
        ecs::ChatSystem::Send(owner,CHAT_TYPE_COMMAND,"BINARY_WHEEL_ICON %u %u %u",prizes[i].visual,prizes[i].count,i);
    ecs::ChatSystem::Send(owner,CHAT_TYPE_COMMAND,"BINARY_WHEEL_OPEN %u %d",static_cast<unsigned>(WheelPrice),
        ecs::PlayerRuntime::GetWheelFreeCount(owner));
}

void CWheelDestiny::TurnWheel()
{
    if(!CanClose())return;
    const auto catalog=item::runtime::CurrentCatalog();
    const auto* ticket=catalog ? Resolve(*catalog,TicketCode) : nullptr;
    if(!ticket)
    {
        ecs::ChatSystem::Send(owner,CHAT_TYPE_INFO,"A Szerencsekerék jelenleg nem érhető el.");
        return;
    }

    const unsigned chance=turns>=40 ? 255 : turns>=25 ? 50 : turns>=10 ? 25 : 0;
    std::vector<unsigned> eligible;
    for(unsigned i=0;i<std::size(prizes);++i)
        if(prizes[i].minimumChance<=chance && Resolve(*catalog,prizes[i].code)) eligible.push_back(i);
    if(eligible.empty())
    {
        ecs::ChatSystem::Send(owner,CHAT_TYPE_INFO,"A Szerencsekerék jutalmai jelenleg nem érhetők el.");
        return;
    }
    const auto index=eligible[number(0,static_cast<int>(eligible.size())-1)];
    const auto prize=prizes[index];
    const auto* definition=Resolve(*catalog,prize.code);
    const bool free=ecs::PlayerRuntime::GetWheelFreeCount(owner)>0;

    pending=true;
    const std::weak_ptr<CWheelDestiny> wheel=ecs::PlayerRuntime::GetWheelDestiny(owner);
    const auto queued=RequestTypedGameplayItemExchange(owner,{{ticket->base.key,0,1}},{{definition->base.key,0,prize.count}},
        free ? 0 : WheelPrice,[wheel,free,index,prize](bool written){
            const auto session=wheel.lock();
            if(!session)return;
            session->pending=false;
            if(!written)
            {
                ecs::ChatSystem::Send(session->owner,CHAT_TYPE_INFO,"A pörgetés sikertelen. A jegy és a Yang visszamaradt.");
                return;
            }
            if(free)
            {
                const int remaining=ecs::PlayerRuntime::GetWheelFreeCount(session->owner);
                if(remaining>0)ecs::PlayerRuntime::SetWheelFreeCount(session->owner,remaining-1);
            }
            if(session->turns<40)++session->turns;
            session->showingResult=true;
            ecs::ChatSystem::Send(session->owner,CHAT_TYPE_COMMAND,"BINARY_WHEEL_TURN %u %u %d %u",
                prize.visual,prize.count,number(1,8),index);
        });
    if(!queued)
    {
        pending=false;
        ecs::ChatSystem::Send(owner,CHAT_TYPE_INFO,"A pörgetéshez ellenőrizd a jegyet, a Yangot és a szabad leltárhelyet.");
    }
}
#endif
