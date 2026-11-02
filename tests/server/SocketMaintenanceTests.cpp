#include "PersistentItemFixture.hpp"
#include <item/SocketMaintenance.hpp>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;namespace f=persistent_item_fixture;namespace sf=socket_service_fixture;
namespace
{
unsigned checks=0;
void Check(bool good,const char* why,std::source_location at=std::source_location::current()){++checks;if(!good)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
std::vector<std::byte> Run()
{
    const auto catalog=BuildItemCatalog(f::Catalog()).value();const auto& c=*catalog;auto target=f::Sword();target.stones[2]={StoneSocketState::Occupied,2,sf::Stone(2),0};
    auto tool=f::Leather(70);tool.key=sf::Tool(2);tool.count=2;std::vector<PersistentItem> items{target,tool};const std::vector layouts{f::Inventory()};const ItemOwnerFence fence{f::Character(),7,2};
    const std::array ids{ItemInstanceId::Create(100).value(),ItemInstanceId::Create(101).value(),ItemInstanceId::Create(102).value()};const SocketMaintenanceActor actor{100,1,1,false};
    auto extract=MakeSocketMaintenanceCommit(c,{15},fence,layouts,items,tool.id,target.id,actor,ids);
    Check(extract && extract->quote.returns.size()==2 && extract->quote.units==1,"all occupied stones returned in one plan");
    Check(extract->commit.items.size()==4 && extract->commit.items[0].after.count==1 && extract->commit.items[0].after.id==tool.id,"partial source identity preserved");
    const auto& after=extract->commit.items[1].after;Check(after.id==target.id && after.revision==target.revision+1 && after.attributes==target.attributes && after.refinementLevel==target.refinementLevel,"complete target identity and bonuses preserved");
    Check(after.stones[0]==StoneSocketSlot{StoneSocketState::Broken,1} && after.stones[2]==StoneSocketSlot{StoneSocketState::Broken,2} && after.stones[1]==target.stones[1],"all extraction preserves grades and existing broken slots");
    Check(extract->commit.items[2].after.key==sf::Stone() && extract->commit.items[2].after.refinementLevel==20 && extract->commit.items[3].after.key==sf::Stone(2) && extract->commit.items[3].after.refinementLevel==0,"returns preserve exact key and per-instance level");
    Check(ValidateItemCommit(c,extract->commit).has_value() && extract->commit.wallets.empty(),"entire bundle is one valid commit with no money change");
    auto same=MakeSocketMaintenanceCommit(c,{15},fence,layouts,items,tool.id,target.id,actor,ids);Check(same && ItemCommitDigest(c,same->commit)==ItemCommitDigest(c,extract->commit),"deterministic source and output placement digest");
    items[1].key=sf::Tool(3);auto last=MakeSocketMaintenanceCommit(c,{16},fence,layouts,items,tool.id,target.id,actor,ids);
    Check(last && last->quote.returns.size()==1 && last->quote.returns[0].socket==2 && last->quote.after[0]==target.stones[0] && last->quote.after[2]==StoneSocketSlot{StoneSocketState::Empty,2},"last extraction selects highest occupied slot without compacting");
    items[1].key=sf::Tool(1);auto clean=MakeSocketMaintenanceCommit(c,{17},fence,layouts,items,tool.id,target.id,actor,{});
    Check(clean && clean->quote.returns.empty() && clean->quote.after[1]==StoneSocketSlot{StoneSocketState::Empty,1} && clean->quote.after[0]==target.stones[0] && clean->quote.after[2]==target.stones[2],"cleaning needs no output IDs and only cleans broken slot");
    auto noEffect=items;noEffect[0].stones[1]={StoneSocketState::Empty,1};Check(MakeSocketMaintenanceCommit(c,{18},fence,layouts,noEffect,tool.id,target.id,actor,{}).error()==SocketMaintenanceStatus::NoEffect,"no effect consumes nothing");
    auto bad=items;bad[1].count=0;Check(!MakeSocketMaintenanceCommit(c,{19},fence,layouts,bad,tool.id,target.id,actor,ids),"invalid source state denied");
    bad=items;bad[1].revision=UINT64_MAX;Check(!MakeSocketMaintenanceCommit(c,{20},fence,layouts,bad,tool.id,target.id,actor,ids),"source revision cannot wrap");
    bad=items;bad[0].revision=UINT64_MAX;Check(!MakeSocketMaintenanceCommit(c,{21},fence,layouts,bad,tool.id,target.id,actor,ids),"target revision cannot wrap");
    bad=items;bad[1].boundTo=ItemOwner{ItemOwnerKind::Character,43};Check(!MakeSocketMaintenanceCommit(c,{22},fence,layouts,bad,tool.id,target.id,actor,ids),"foreign-bound source denied");
    bad=items;bad[0].boundTo=ItemOwner{ItemOwnerKind::Character,43};Check(!MakeSocketMaintenanceCommit(c,{23},fence,layouts,bad,tool.id,target.id,actor,ids),"foreign-bound target denied");
    bad=items;bad[0].boundTo=fence.owner;Check(MakeSocketMaintenanceCommit(c,{24},fence,layouts,bad,tool.id,target.id,actor,ids).has_value(),"own bound target is maintained without losing binding");
    bad=items;bad[0].placement=ItemPlacement{ItemStorageKind::Equipment,1,0};Check(!MakeSocketMaintenanceCommit(c,{25},fence,layouts,bad,tool.id,target.id,actor,ids),"equipped target must be taken off first");
    Check(!MakeSocketMaintenanceCommit(c,{},fence,layouts,items,tool.id,target.id,actor,ids),"zero operation denied");
    Check(!MakeSocketMaintenanceCommit(c,{26},fence,layouts,items,target.id,target.id,actor,ids),"same source and target denied");
    auto denied=actor;denied.characterClass=4;Check(!MakeSocketMaintenanceCommit(c,{27},fence,layouts,items,tool.id,target.id,denied,ids),"invalid actor denied");
    auto high=f::Catalog();std::ranges::find(high.items,sf::Tool(1),&ItemBaseRow::key)->minimumLevel=101;const auto highCatalog=BuildItemCatalog(high).value();Check(MakeSocketMaintenanceCommit(*highCatalog,{28},fence,layouts,items,tool.id,target.id,actor,ids).error()==SocketMaintenanceStatus::Level,"source minimum level enforced");
    items[1]=tool;Check(MakeSocketMaintenanceCommit(c,{29},fence,layouts,items,tool.id,target.id,actor,{}).error()==SocketMaintenanceStatus::Busy,"missing output IDs refuses whole extraction");
    const std::array duplicate{ids[0],ids[0]};Check(!MakeSocketMaintenanceCommit(c,{30},fence,layouts,items,tool.id,target.id,actor,duplicate),"duplicate output IDs denied");
    const std::array reused{target.id,ids[1]};Check(!MakeSocketMaintenanceCommit(c,{31},fence,layouts,items,tool.id,target.id,actor,reused),"input identity cannot be reused");
    auto retired=f::Catalog();std::ranges::find(retired.items,sf::Stone(),&ItemBaseRow::key)->retired=true;const auto retiredCatalog=BuildItemCatalog(retired).value();auto recovery=MakeSocketMaintenanceCommit(*retiredCatalog,{32},fence,layouts,items,tool.id,target.id,actor,ids);Check(recovery.has_value(),"known retired embedded stone remains recoverable");
    auto reordered=recovery->commit;std::ranges::reverse(reordered.items);Check(ValidateItemCommit(*retiredCatalog,reordered).has_value() && ItemCommitDigest(*retiredCatalog,reordered)==ItemCommitDigest(*retiredCatalog,recovery->commit),"recovery exception respects canonical write-set ordering");
    auto forged=recovery->commit;forged.items[2].after.count=2;Check(!ValidateItemCommit(*retiredCatalog,forged),"retired recovery cannot create an extra stone");
    forged=recovery->commit;forged.items[0].after.count++;Check(!ValidateItemCommit(*retiredCatalog,forged),"retired recovery requires exact tool consumption");
    forged=recovery->commit;forged.items[1].after.stones=target.stones;Check(!ValidateItemCommit(*retiredCatalog,forged),"retired recovery must remove every returned embedded stone");
    forged=recovery->commit;forged.items[1].before->stones[0].stone.reset();std::ranges::reverse(forged.items);Check(!ValidateItemCommit(*retiredCatalog,forged),"malformed later before image cannot authorize a retired return");
    auto invalidLayouts=layouts;invalidLayouts[0].pages=UINT16_MAX;Check(!MakeSocketMaintenanceCommit(c,{36},fence,invalidLayouts,items,tool.id,target.id,actor,ids),"invalid output grid rejected before allocation");
    invalidLayouts=layouts;invalidLayouts.push_back(layouts[0]);Check(!MakeSocketMaintenanceCommit(c,{37},fence,invalidLayouts,items,tool.id,target.id,actor,ids),"duplicate output grid denied");
    // Fill every footprint, including tall target. Two returns cannot fit in
    // the single cell freed by consuming the complete source.
    auto full=items;full[1].count=1;
    for(unsigned cell=0;cell<90;++cell)if(cell!=0 && cell!=1 && cell!=5 && cell!=10){auto material=f::Leather(200+cell);material.placement->anchor=cell;full.push_back(material);}
    Check(MakeSocketMaintenanceCommit(c,{33},fence,layouts,full,tool.id,target.id,actor,ids).error()==SocketMaintenanceStatus::NoRoom,"not enough room for all returns leaves whole operation uncommitted");
    full[1].key=sf::Tool(3);auto freed=MakeSocketMaintenanceCommit(c,{34},fence,layouts,full,tool.id,target.id,actor,ids);Check(freed && freed->commit.items[0].after.destroyed && freed->commit.items[2].after.placement->anchor==1,"last source cell can hold exactly one returned stone");
    full[1].key=sf::Tool(1);Check(MakeSocketMaintenanceCommit(c,{35},fence,layouts,full,tool.id,target.id,actor,{}).has_value(),"cleaning works in a full inventory");
    const auto packet=EncodeSocketMaintenance(c,0x123456789abcdefULL,&tool,&target,2,SocketMaintenanceStatus::Ready,extract->quote,std::array{target.id.Value()});
    Check(packet && EncodeSocketMaintenance(c,0,nullptr,nullptr,0,SocketMaintenanceStatus::Closed,{},{})->size()==162,"native quote and terminal envelope encode");return *packet;
}
}
int main(int argc,char** argv){try{const auto bytes=Run();if(argc==3 && std::string_view(argv[1])=="--write-golden"){std::ofstream out(argv[2]);for(auto b:bytes)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';if(!out)throw std::runtime_error("golden write failed");}std::cout<<checks<<" socket maintenance checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
