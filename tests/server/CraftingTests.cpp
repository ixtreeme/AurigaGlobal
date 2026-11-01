#include "PersistentItemFixture.hpp"
#include <item/Crafting.hpp>
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
void Check(bool okay,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!okay)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
std::vector<std::byte> Run()
{
    const auto catalog=BuildItemCatalog(f::Catalog());Check(catalog.has_value(),"catalog");const auto& c=**catalog;
    const auto bag=f::Inventory();const auto wallet=f::Gold();const ItemOwnerFence owner{f::Character(),7,1};
    CraftRecipe recipe{7,"Iron sword",{f::Sword().key,9,1},25,5000,{{f::Leather().key,0,2}}};
    CraftWorkshop shop{20016,"Workshop",{recipe}};Check(ValidateCraftWorkshops(c,std::span(&shop,1)).has_value(),"typed workshop");
    auto material=f::Leather();material.count=10;material.placement->anchor=0;
    const std::vector<ItemInstanceId> ids{ItemInstanceId::Create(UINT64_MAX-20).value(),ItemInstanceId::Create(UINT64_MAX-21).value()};
    std::array<std::uint8_t,16> op{};op[0]=1;std::vector<std::uint16_t> rolls{0,4999,5000};
    auto craft=MakeCraftCommit(c,op,owner,std::span(&bag,1),std::span(&material,1),std::span(&wallet,1),recipe,3,rolls,ids);
    Check(craft && craft->successes==2 && craft->commit.items.size()==3,"server rolls choose mixed outcomes in one batch");
    Check(craft->commit.wallets[0].after.balance==925 && craft->commit.items[0].after.count==4,"all attempts consume exact gold and materials");
    Check(craft->commit.items[1].after.refinementLevel==9 && craft->commit.items[1].after.id==ids[0],"fresh result identity and explicit level");
    Check(craft->commit.result.size()==30 && ValidateItemCommit(c,craft->commit).has_value(),"audit payload includes every roll");
    auto poor=wallet;poor.balance=74;
    Check(MakeCraftCommit(c,op,owner,std::span(&bag,1),std::span(&material,1),std::span(&poor,1),recipe,3,rolls,ids).error()==CraftError::NoMoney,"no money leaves ingredients unchanged");
    auto small=material;small.count=5;
    Check(MakeCraftCommit(c,op,owner,std::span(&bag,1),std::span(&small,1),std::span(&wallet,1),recipe,3,rolls,ids).error()==CraftError::NoMaterials,"missing ingredients never partly consume");
    auto bound=material;bound.boundTo=owner.owner;Check(!CraftIngredientUsable(bound,owner.owner),"bound instance is not an ingredient");
    auto custom=f::Sword();Check(!CraftIngredientUsable(custom,owner.owner),"bonus/socket state is not automatically consumed");
    auto worn=material;worn.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};Check(!CraftIngredientUsable(worn,owner.owner),"equipped material excluded");
    auto failure=recipe;failure.chance=0;std::vector<std::uint16_t> three{0,0,0};
    auto failed=MakeCraftCommit(c,op,owner,std::span(&bag,1),std::span(&material,1),std::span(&wallet,1),failure,3,three,{});
    Check(failed && !failed->successes && failed->commit.items.size()==1 && failed->commit.items[0].after.count==4 && failed->commit.wallets[0].after.balance==925,"failed attempts atomically pay without results");
    auto tight=bag;tight.columns=tight.rows=tight.pages=1;auto compact=recipe;compact.chance=10000;
    Check(MakeCraftCommit(c,op,owner,std::span(&tight,1),std::span(&material,1),std::span(&wallet,1),compact,1,std::span(three).first(1),ids).error()==CraftError::NoRoom,"no room rolls back every input");
    compact.result={material.key,0,1};compact.ingredients[0].count=10;
    const auto freed=MakeCraftCommit(c,op,owner,std::span(&tight,1),std::span(&material,1),std::span(&wallet,1),compact,1,std::span(three).first(1),ids);
    Check(freed && freed->commit.items.size()==2 && freed->commit.items[0].after.destroyed && freed->commit.items[1].after.placement->anchor==0,"fully consumed input frees output footprint");
    compact.ingredients[0].count=2;
    const auto net=MakeCraftCommit(c,op,owner,std::span(&bag,1),std::span(&material,1),std::span(&wallet,1),compact,1,std::span(three).first(1),{});
    Check(net && net->commit.items.size()==1 && net->commit.items[0].before->count==10 && net->commit.items[0].after.count==9 && net->commit.items[0].after.revision==2,"same input/output stack has one write against original snapshot");
    auto split=material;split.id=ItemInstanceId::Create(22).value();split.count=1;split.placement->anchor=1;small.count=1;
    const std::vector<PersistentItem> stacks{small,split};
    Check(MakeCraftCommit(c,op,owner,std::span(&bag,1),stacks,std::span(&wallet,1),recipe,1,std::span(three).first(1),ids).has_value(),"ingredient can span multiple stacks");
    rolls[0]=10000;Check(!MakeCraftCommit(c,op,owner,std::span(&bag,1),std::span(&material,1),std::span(&wallet,1),recipe,3,rolls,ids),"roll bounds checked");
    Check(!MakeCraftCommit(c,op,owner,std::span(&bag,1),std::span(&material,1),std::span(&wallet,1),recipe,101,{},ids),"batch bound checked");
    auto invalid=shop;invalid.recipes[0].ingredients.push_back(invalid.recipes[0].ingredients[0]);Check(!ValidateCraftWorkshops(c,std::span(&invalid,1)),"duplicate ingredient cannot be underquoted");
    invalid=shop;invalid.recipes[0].gold=INT64_MAX;Check(!ValidateCraftWorkshops(c,std::span(&invalid,1)),"batch cost overflow refused at boot");
    invalid=shop;invalid.name=std::string(129,'x');Check(!ValidateCraftWorkshops(c,std::span(&invalid,1)),"name size bound");
    auto spacious=bag;spacious.pages=4;auto many=material;many.count=32;auto bulk=recipe;bulk.chance=10000;bulk.ingredients[0].count=1;
    std::vector<ItemInstanceId> reserved;for(unsigned i=0;i<32;++i)reserved.push_back(ItemInstanceId::Create(1000+i).value());std::vector<std::uint16_t> batch(32,0);
    Check(MakeCraftCommit(c,op,owner,std::span(&spacious,1),std::span(&many,1),std::span(&wallet,1),bulk,32,batch,reserved).error()==CraftError::Limit,"input and output together respect 32-change transaction bound");
    Check(MakeCraftCommit(c,op,owner,std::span(&spacious,1),std::span(&many,1),std::span(&wallet,1),bulk,31,std::span(batch).first(31),reserved).has_value(),"largest fitting weapon batch plus input commits together");
    many.count=33;batch.push_back(0);
    Check(MakeCraftCommit(c,op,owner,std::span(&spacious,1),std::span(&many,1),std::span(&wallet,1),bulk,33,batch,reserved).error()==CraftError::Limit,"oversized output reports transaction limit rather than inventory space");
    const auto wire=EncodeCraftWorkshop(c,&shop,12345,0x0123456789abcdefULL,3,CraftStatus::Crafted,7,3,2);
    Check(wire && std::to_integer<unsigned>((*wire)[0])==230,"native golden wire");
    const auto closed=EncodeCraftWorkshop(c,nullptr,0,0,0,CraftStatus::Closed);Check(closed && closed->size()==47,"terminal wire");return *wire;
}
}
int main(int argc,char** argv)
{
    try
    {
        const auto wire=Run();
        if(argc==3 && std::string_view(argv[1])=="--write-golden")
        {std::ofstream out(argv[2]);unsigned n=0;for(auto b:wire){if(n)out<<(n%32 ? ' ' : '\n');out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);++n;}out<<'\n';}
        std::cout<<checks<<" crafting checks passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
