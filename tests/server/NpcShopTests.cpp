#include "PersistentItemFixture.hpp"
#include <item/NpcShops.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;
namespace f=persistent_item_fixture;
namespace
{
unsigned checks=0;
void Check(bool value,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!value)throw std::runtime_error(std::string(why)+" line "+std::to_string(at.line()));}
std::vector<std::byte> Run()
{
    auto source=f::Catalog();
    for(auto& item:source.items)if(item.key==item_catalog_fixture::Leather()){item.buyAmount=3;item.sellAmount=1;}
    const auto built=BuildItemCatalog(source);Check(built.has_value(),"catalog");const auto& c=**built;
    const auto fence=ItemOwnerFence{f::Character(),7,1};
    const std::vector<ItemStorageLayout> layouts{f::Inventory()};
    const std::vector<ItemWallet> wallets{f::Gold()};
    auto leather=f::Leather();leather.count=198;
    const std::vector<PersistentItem> inventory{f::Sword(),leather};
    std::array<std::uint8_t,16> op{};op.fill(0xAD);
    const std::vector<ItemInstanceId> ids{ItemInstanceId::Create(1001).value(),ItemInstanceId::Create(1002).value()};
    NpcShop shop{20016,"Item shop",{{7,{leather.key,0,2}},{8,{f::Sword().key,9,1}}}};
    Check(ValidateNpcShops(c,std::span(&shop,1)).has_value(),"valid offers");
    auto invalid=shop;invalid.name.push_back(char{});Check(!ValidateNpcShops(c,std::span(&invalid,1)),"control character in shop name refused");
    invalid=shop;invalid.offers.push_back(invalid.offers.front());Check(!ValidateNpcShops(c,std::span(&invalid,1)),"duplicate offer refused");
    invalid=shop;invalid.offers[0].grant.refinementLevel=20;Check(!ValidateNpcShops(c,std::span(&invalid,1)),"unknown level refused");
    auto buy=MakeNpcBuyCommit(c,op,fence,layouts,inventory,wallets,shop.offers[0],3,ids);
    Check(buy && buy->wallets[0].after.balance==982,"three bundles cost 18");
    Check(buy->items.size()==2 && buy->items[0].after.id==leather.id && buy->items[0].after.count==200 && buy->items[1].after.count==4,"top up plus a new stack");
    Check(ValidateItemCommit(c,*buy).has_value(),"complete atomic purchase");
    auto poor=wallets;poor[0].balance=17;
    Check(MakeNpcBuyCommit(c,op,fence,layouts,inventory,poor,shop.offers[0],3,ids).error()==NpcShopError::NoMoney,"no money");
    auto narrow=layouts;narrow[0].columns=narrow[0].rows=narrow[0].pages=1;
    auto full=leather;full.count=200;full.placement->anchor=0;
    Check(MakeNpcBuyCommit(c,op,fence,narrow,std::span(&full,1),wallets,shop.offers[0],1,ids).error()==NpcShopError::NoRoom,"full inventory");
    Check(!MakeNpcBuyCommit(c,op,fence,layouts,inventory,wallets,shop.offers[0],0,ids),"zero quantity");
    Check(!MakeNpcBuyCommit(c,op,fence,layouts,inventory,wallets,shop.offers[0],UINT32_MAX,ids),"huge quantity");
    auto sold=MakeNpcSellCommit(c,op,fence,layouts,inventory,wallets,leather.id,2);
    Check(sold && sold->wallets[0].after.balance==1002 && sold->items[0].after.count==196 && sold->items[0].after.id==leather.id,"partial sale preserves identity");
    auto whole=MakeNpcSellCommit(c,op,fence,layouts,inventory,wallets,f::Sword().id,0);
    Check(whole && whole->items[0].after.destroyed && !whole->items[0].after.placement,"whole sale tombstone");
    Check(whole->items[0].after.attributes==f::Sword().attributes && whole->items[0].after.stones==f::Sword().stones && whole->items[0].after.refinementLevel==9,"complete tombstone retained");
    auto rich=wallets;rich[0].balance=INT64_MAX;
    Check(MakeNpcSellCommit(c,op,fence,layouts,inventory,rich,leather.id,1).error()==NpcShopError::WalletFull,"wallet overflow refused");
    Check(!MakeNpcSellCommit(c,op,fence,layouts,inventory,wallets,leather.id,199),"over-selling refused");
    auto worn=f::Sword();worn.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};
    Check(!MakeNpcSellCommit(c,op,fence,layouts,std::span(&worn,1),wallets,worn.id,0),"worn item refused");
    auto denied=source;denied.tradePolicies[0].allowSell=false;
    const auto deniedBuild=BuildItemCatalog(denied);Check(deniedBuild.has_value(),"no-sale catalog");const auto noSell=*deniedBuild;
    Check(MakeNpcSellCommit(*noSell,op,fence,layouts,inventory,wallets,leather.id,1).error()==NpcShopError::Denied,"catalog sale policy");
    auto retired=source;retired.items[0].retired=true;retired.refinementRecipes.clear();for(auto& loot:retired.lootProfiles)std::erase_if(loot.entries,[](const auto& e){return e.item==f::Sword().key;});const auto retiredBuild=BuildItemCatalog(retired);Check(retiredBuild.has_value(),"retired catalog");const auto old=*retiredBuild;
    Check(!MakeNpcBuyCommit(*old,op,fence,layouts,inventory,wallets,shop.offers[1],1,ids),"retired purchase denied");
    Check(MakeNpcSellCommit(*old,op,fence,layouts,inventory,wallets,f::Sword().id,1).has_value(),"retired owned item remains sellable");
    const auto bytes=EncodeNpcShop(c,&shop,12345,0x0123456789abcdefULL,3,NpcShopStatus::Ready,inventory);
    Check(bytes && std::to_integer<unsigned>((*bytes)[0])==228,"shop wire");
    const auto nameOf=[](const std::vector<std::byte>& wire,std::size_t size) {
        std::string text;for(std::size_t i=29;i<29+size;++i)text.push_back(static_cast<char>(std::to_integer<unsigned char>(wire[i])));return text;
    };
    Check(nameOf(*bytes,shop.name.size())==shop.name,"short shop name survives encoding");
    auto named=shop;named.name="Vegyeskeresked\xC3\xA9s - ItemKey NPC shop";
    auto namedBytes=EncodeNpcShop(c,&named,12345,123,0,NpcShopStatus::Ready,inventory);
    Check(namedBytes && nameOf(*namedBytes,named.name.size())==named.name,"long UTF-8 shop name survives encoding");
    const auto closed=EncodeNpcShop(c,nullptr,0,0,0,NpcShopStatus::Closed,{});
    Check(closed && closed->size()==37,"empty close wire");
    return *bytes;
}
}
int main(int argc,char** argv)
{
    try
    {
        const auto bytes=Run();
        if(argc==3 && std::string_view(argv[1])=="--write-golden")
        {std::ofstream out(argv[2]);unsigned n=0;for(auto b:bytes){if(n)out<<(n%32 ? ' ' : '\n');out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);++n;}out<<'\n';}
        std::cout<<checks<<" NPC shop checks passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<" after checks="<<checks<<'\n';return 1;}
}
