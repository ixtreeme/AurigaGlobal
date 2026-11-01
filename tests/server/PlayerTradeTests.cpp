#include "PersistentItemFixture.hpp"
#include <item/PlayerTrades.hpp>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;
namespace f=persistent_item_fixture;
namespace
{
unsigned checks=0;
void Check(bool ok,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!ok)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
std::vector<std::byte> Run()
{
    const auto built=BuildItemCatalog(f::Catalog());Check(built.has_value(),"catalog");const auto& c=**built;
    const ItemOwner a=f::Character(),b{ItemOwnerKind::Character,43};
    const std::vector<ItemStorageLayout> bags{f::Inventory(a),f::Inventory(b)};
    const std::vector<ItemWallet> wallets{f::Gold(),{b,WalletCurrency::Gold,1,500}};
    auto sword=f::Sword();auto leather=f::Leather();leather.owner=b;leather.count=10;leather.placement->anchor=0;
    const std::vector<PersistentItem> inventory{sword,leather};
    std::array<std::uint8_t,16> op{};op.fill(0xAB);
    std::array<PlayerTradeOffer,2> offers{{{{a,7,1},{{sword.id,0}},100},{{b,9,1},{{leather.id,4}},40}}};
    const std::vector<ItemInstanceId> ids{ItemInstanceId::Create(1001).value()};
    auto commit=MakePlayerTradeCommit(c,op,offers,bags,inventory,wallets,ids);
    Check(commit && commit->owners.size()==2 && commit->items.size()==3,"one atomic two-owner transfer");
    const auto moved=std::ranges::find(commit->items,sword.id,[](const auto& i){return i.after.id;});
    Check(moved!=commit->items.end() && moved->after.owner==b && moved->after.id==sword.id,"whole instance identity retained");
    Check(moved->after.attributes==sword.attributes && moved->after.stones==sword.stones && moved->after.refinementLevel==9,"full weapon state retained");
    Check(commit->wallets[0].after.balance==940 && commit->wallets[1].after.balance==560,"both gold offers net atomically");
    Check(commit->items[1].after.count==6 && commit->items[1].after.id==leather.id && commit->items[2].after.count==4 && commit->items[2].after.owner==a,"split retains remainder and transfers fresh ID");
    Check(ValidateItemCommit(c,*commit).has_value(),"valid complete write set");
    auto noMoney=offers;noMoney[0].gold=1001;
    Check(MakePlayerTradeCommit(c,op,noMoney,bags,inventory,wallets,ids).error()==PlayerTradeError::NoMoney,"cannot offer anticipated incoming money");
    auto rich=wallets;rich[1].balance=INT64_MAX;auto gift=offers;gift[1].gold=0;
    Check(MakePlayerTradeCommit(c,op,gift,bags,inventory,rich,ids).error()==PlayerTradeError::WalletFull,"credit overflow rejected");
    auto bound=inventory;bound[0].boundTo=a;
    Check(MakePlayerTradeCommit(c,op,offers,bags,bound,wallets,ids).error()==PlayerTradeError::Denied,"bound item refused");
    auto worn=inventory;worn[0].placement=ItemPlacement{ItemStorageKind::Equipment,1,0};
    Check(!MakePlayerTradeCommit(c,op,offers,bags,worn,wallets,ids),"equipment cannot be offered");
    auto duplicate=offers;duplicate[0].items.push_back(duplicate[0].items.front());
    Check(!MakePlayerTradeCommit(c,op,duplicate,bags,inventory,wallets,ids),"duplicate identity refused");
    Check(!MakePlayerTradeCommit(c,op,offers,bags,inventory,wallets,{}),"split requires reserved identity");
    auto changed=offers;changed[0].items[0].count=2;
    Check(!MakePlayerTradeCommit(c,op,changed,bags,inventory,wallets,ids),"over-offer refused");
    auto otherSword=sword;otherSword.owner=b;otherSword.id=ItemInstanceId::Create(20).value();
    const std::vector<PersistentItem> swords{sword,otherSword};
    auto tight=bags;for(auto& bag:tight){bag.columns=1;bag.rows=3;bag.pages=1;}
    std::array<PlayerTradeOffer,2> swap{{{{a,7,1},{{sword.id,0}},0},{{b,9,1},{{otherSword.id,0}},0}}};
    Check(MakePlayerTradeCommit(c,op,swap,tight,swords,wallets,{}).has_value(),"two full inventories free both outgoing footprints before placement");
    auto tooSmall=bags;tooSmall[1].columns=tooSmall[1].rows=tooSmall[1].pages=1;
    Check(MakePlayerTradeCommit(c,op,offers,tooSmall,inventory,wallets,ids).error()==PlayerTradeError::NoRoom,"failure does not partially transfer");
    auto equal=swap;equal[0].gold=equal[1].gold=100;
    const auto sameGold=MakePlayerTradeCommit(c,op,equal,bags,swords,wallets,{});
    Check(sameGold && sameGold->wallets.empty(),"equal gold offers do not create invalid unchanged wallet writes");
    std::array<PlayerTradeOffer,2> money{{{{a,7,1},{},50},{{b,9,1},{},0}}};
    Check(MakePlayerTradeCommit(c,op,money,bags,inventory,wallets,{}).has_value(),"gold-only gift");
    money[0].gold=0;Check(!MakePlayerTradeCommit(c,op,money,bags,inventory,wallets,{}),"empty trade refused");
    std::vector<PersistentItem> stacks;std::vector<ItemInstanceId> splitIds;
    std::array<PlayerTradeOffer,2> maximum{{{{a,7,1},{},0},{{b,9,1},{},0}}};
    for(unsigned side=0;side<2;++side)for(unsigned i=0;i<MaxPlayerTradeItems;++i)
    {
        auto stack=f::Leather(100+side*20+i);stack.owner=side ? b : a;stack.count=2;stack.placement->anchor=i;
        stacks.push_back(stack);maximum[side].items.push_back({stack.id,1});splitIds.push_back(ItemInstanceId::Create(1000+splitIds.size()).value());
    }
    const auto full=MakePlayerTradeCommit(c,op,maximum,bags,stacks,wallets,splitIds);
    Check(full && full->items.size()==32,"maximum sixteen partial transfers fit one commit");
    auto denied=f::Catalog();denied.tradePolicies[0].allowTrade=false;const auto restrictive=BuildItemCatalog(denied).value();
    Check(MakePlayerTradeCommit(*restrictive,op,offers,bags,inventory,wallets,ids).error()==PlayerTradeError::Denied,"catalog GIVE prohibition");
    PlayerTradeView view;view.token=0x0123456789abcdefULL;view.revision=7;view.phase=PlayerTradePhase::Confirming;view.readyMask=3;view.confirmedMask=1;
    view.actors={PlayerTradeActorView{100,42,"First",offers[0]},PlayerTradeActorView{101,43,"Second",offers[1]}};
    auto wire=EncodePlayerTrade(c,&view,0,5,PlayerTradeStatus::Ready,bags,inventory);
    Check(wire && std::to_integer<unsigned>((*wire)[0])==229,"active trade wire");
    auto closed=EncodePlayerTrade(c,nullptr,0,0,PlayerTradeStatus::Cancelled,{},{});
    Check(closed && closed->size()==35,"terminal trade wire has no private data");
    return *wire;
}
}
int main(int argc,char** argv)
{
    try
    {
        const auto wire=Run();
        if(argc==3 && std::string_view(argv[1])=="--write-golden")
        {std::ofstream out(argv[2]);unsigned n=0;for(auto b:wire){if(n)out<<(n%32 ? ' ' : '\n');out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);++n;}out<<'\n';}
        std::cout<<checks<<" player trade checks passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
