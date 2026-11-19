#include "PersistentItemFixture.hpp"
#include <item/NpcShops.hpp>
#include <item/NpcShopConfig.hpp>
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
std::vector<std::byte> barterGolden;
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
    auto exchange=shop.offers[1];exchange.bundleGold=7;exchange.costs={{leather.key,0,3}};
    NpcShop barter{20017,"Native barter",{exchange}};
    Check(ValidateNpcShops(c,std::span(&barter,1)).has_value(),"authored item and bundle-gold shop is valid");
    const auto paid=MakeNpcBuyCommit(c,op,fence,layouts,inventory,wallets,exchange,2,ids);
    Check(paid && paid->wallets.front().after.balance==986,"bundle price is independent of prototype unit price and refinement");
    Check(paid && std::ranges::any_of(paid->items,[&](const auto& change){return change.before && change.after.id==leather.id && change.after.count==192;}),"barter debits exact native material units in purchase commit");
    Check(paid && ValidateItemCommit(c,*paid).has_value(),"barter payment and grant form one valid atomic commit");
    auto bundle=shop.offers.front();bundle.bundleGold=7;const auto bundled=MakeNpcBuyCommit(c,op,fence,layouts,inventory,wallets,bundle,3,ids);
    Check(bundled && bundled->wallets.front().after.balance==979,"explicit price applies once per two-unit bundle, without division or unit multiplication");
    auto absent=exchange;absent.costs.front().count=199;
    Check(MakeNpcBuyCommit(c,op,fence,layouts,inventory,wallets,absent,1,ids).error()==NpcShopError::NoMaterials,"missing payment items cannot produce a reward");
    auto duplicate=barter;duplicate.offers.front().costs.push_back(exchange.costs.front());
    Check(!ValidateNpcShops(c,std::span(&duplicate,1)),"duplicate currency lanes are refused");
    auto free=shop.offers.front();free.bundleGold=0;
    Check(MakeNpcBuyCommit(c,op,fence,layouts,inventory,wallets,free,1,ids)->wallets.empty(),"explicitly free source shop does not mutate gold");
    auto overflow=exchange;overflow.bundleGold=INT64_MAX;
    Check(!MakeNpcBuyCommit(c,op,fence,layouts,inventory,wallets,overflow,2,ids),"bundle multiplication cannot overflow wallet bounds");
    auto huge=exchange;huge.bundleGold=0;huge.costs.front().count=UINT32_MAX;
    Check(MakeNpcBuyCommit(c,op,fence,layouts,inventory,wallets,huge,10000,ids).error()==NpcShopError::NoMaterials,"large currency quantity does not wrap into an affordable price");
    const auto extended=EncodeNpcShop(c,&barter,12345,123,0,NpcShopStatus::Ready,inventory);
    Check(extended && std::to_integer<unsigned>((*extended)[5])==2,"barter stock uses GC228 v2");
    barterGolden=*extended;
    Check(std::to_integer<unsigned>((*bytes)[5])==1,"ordinary legacy-compatible native stock retains GC228 v1");
    const std::string manifest=R"({"version":2,"shops":[{"npc_race":20017,"name":"Original stock","offers":[{"id":1,"category":6,"local":1,"level":0,"count":2,"gold":0,"try_magic":true,"costs":[{"category":6,"local":1,"level":0,"count":3}]}]}],"routes":[]})";
    const auto parsed=ParseNpcShopManifest(manifest,c);
    Check(parsed && parsed->shops.size()==1 && parsed->shops[0].offers[0].bundleGold==0 && parsed->shops[0].offers[0].grant.tryMagic,"offline and game manifest parser retains explicit free price and original magic policy");
    auto negative=manifest;negative.replace(negative.find("\"gold\":0"),8,"\"gold\":-1");Check(!ParseNpcShopManifest(negative,c),"negative authored price fails closed without narrowing");
    auto oversized=manifest;oversized.replace(oversized.find("\"gold\":0"),8,"\"gold\":18446744073709551615");Check(!ParseNpcShopManifest(oversized,c),"oversized authored price fails closed");
    Check(!ParseNpcShopManifest(manifest.substr(0,manifest.size()-1),c),"incomplete native stock cannot publish a partial list");
    return *bytes;
}
}
int main(int argc,char** argv)
{
    try
    {
        const auto bytes=Run();
        if(argc==3 && (std::string_view(argv[1])=="--write-golden" || std::string_view(argv[1])=="--write-barter-golden"))
        {const auto& wire=std::string_view(argv[1])=="--write-barter-golden"?barterGolden:bytes;std::ofstream out(argv[2]);unsigned n=0;for(auto b:wire){if(n)out<<(n%32 ? ' ' : '\n');out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);++n;}out<<'\n';}
        std::cout<<checks<<" NPC shop checks passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<" after checks="<<checks<<'\n';return 1;}
}
