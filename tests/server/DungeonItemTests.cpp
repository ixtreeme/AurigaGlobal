#include "PersistentItemFixture.hpp"
#include <item/DungeonItems.hpp>
#include <item/TypedInventoryWire.hpp>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;
namespace {
unsigned checks=0;void Check(bool v,const char* message){++checks;if(!v)throw std::runtime_error(message);}
ItemKey Key(unsigned n){return ItemKey::Create(10,63000+n).value();}
std::shared_ptr<const ItemCatalog> Catalog(){auto s=persistent_item_fixture::Catalog();const auto base=*std::ranges::find(s.items,item_catalog_fixture::InstantPotion(),&ItemBaseRow::key);
    for(unsigned n=1;n<=3;++n){auto b=base;b.key=Key(n);b.contentCode=std::string(std::array{RuneFragmentCode,RuneFloorKeyCode,OrcKeyCode}[n-1]);b.minimumLevel=0;b.maxStack=7;s.items.push_back(b);
        s.questItems.push_back({Key(n),n==3 ? QuestItemDispatch::Token : QuestItemDispatch::FirstHandler,n==3 ? "" : b.contentCode,static_cast<std::uint8_t>(n==3 ? 0 : 1),false});}for(unsigned n=0;n<DungeonUseBindings.size();++n){auto b=base;b.key=Key(4+n);b.contentCode=std::string(DungeonUseBindings[n].code);b.minimumLevel=0;b.maxStack=7;s.items.push_back(b);s.questItems.push_back({b.key,QuestItemDispatch::Token,"",0,false});}return BuildItemCatalog(std::move(s)).value();}
struct Owner {
    std::shared_ptr<const ItemCatalog> catalog=Catalog();ItemOwnerFence fence{persistent_item_fixture::Character(),7,1};std::array<ItemStorageLayout,1> layouts{persistent_item_fixture::Inventory()};std::vector<PersistentItem> items;std::uint64_t serial=0,identity=800000;
    auto Op(){return std::array<std::uint8_t,16>{0x52,static_cast<std::uint8_t>(++serial),static_cast<std::uint8_t>(serial>>8)};}
    auto Id(){return ItemInstanceId::Create(++identity).value();}
    void Apply(const ItemCommit& c){Check(ValidateItemCommit(*catalog,c).has_value(),"complete native dungeon commit");for(const auto& change:c.items){auto i=std::ranges::find(items,change.after.id,&PersistentItem::id);if(change.after.destroyed){if(i!=items.end())items.erase(i);}else if(i==items.end())items.push_back(change.after);else *i=change.after;}++fence.revision;Check(EncodeTypedInventory(*catalog,layouts,{},items).has_value(),"post-operation state serializes");}
    ItemInstanceId Grant(unsigned k,unsigned count){const auto id=Id();std::array ids{id,Id(),Id()};Apply(MakeGrantCommit(*catalog,Op(),fence,layouts,items,{Key(k),0,count},ids).value());return std::ranges::find_if(items,[&](const auto& i){return i.key==Key(k);})->id;}
    RuneKeyFacts Facts(){RuneKeyFacts f;f.admitted=f.dungeon=f.leader=true;f.floor=2;f.type=1;f.level=100;const auto* p=catalog->FindUsePolicy(catalog->Find(Key(1))->base.usePolicy);while(!(p->allowedClasses&(1u<<f.characterClass)))++f.characterClass;while(!(p->allowedSexes&(1u<<f.sex)))++f.sex;return f;}
    const PersistentItem& Find(ItemInstanceId id){return *std::ranges::find(items,id,&PersistentItem::id);}
    auto Use(ItemInstanceId id,RuneKeyFacts facts,std::span<const ItemInstanceId> ids={}){return MakeRuneKeyCommit(*catalog,Op(),fence,layouts,items,id,facts,ids,100000);}
    auto Count(unsigned n){return CountMissionItems(items,fence.owner,Key(n),0);}
};
void Exchange(){Owner o;auto id=o.Grant(1,9);const auto facts=o.Facts();Check(o.Use(id,facts).error()==ItemMissionError::Requirements,"nine fragments cannot exchange");o.Grant(1,2);const auto before=o.items;Check(!o.Use(id,facts),"fresh key identity is required");Check(o.items==before,"failed exchange consumes nothing");std::array ids{o.Id()};auto commit=o.Use(id,facts,ids).value();Check(commit.result==std::vector<std::uint8_t>({'D','R','K',1,1}),"exchange receipt identifies exact action");o.Apply(commit);Check(o.Count(1)==1 && o.Count(2)==1,"ten fragments across stacks produce one key atomically");Check(DungeonItemKey(*o.catalog,RuneFragmentCode)==Key(1),"runtime lookup by native content code");Check(!RuneKeyItem(*o.catalog,Key(3)),"passive orc key has no fabricated use");}
void Admission(){Owner o;const auto id=o.Grant(1,10);const auto source=o.Find(id);auto f=o.Facts();
    for(unsigned floor=0;floor<=8;++floor)for(unsigned type=0;type<=9;++type)for(bool leader:{false,true}){f.floor=floor;f.type=type;f.leader=leader;Check(bool(QuoteRuneKey(*o.catalog,source,o.fence.owner,f))==(floor>=2 && floor<=4 && type==1 && leader),"fragment gate matrix uses real rune floors and leader");}
    f=o.Facts();for(unsigned missing=0;missing<2;++missing){auto bad=f;if(missing)bad.admitted=false;else bad.dungeon=false;Check(!QuoteRuneKey(*o.catalog,source,o.fence.owner,bad),"unavailable session or dungeon denied");}
    auto other=o.fence.owner;++other.id;Check(!QuoteRuneKey(*o.catalog,source,other,f),"foreign source denied");f.characterClass=4;Check(!QuoteRuneKey(*o.catalog,source,o.fence.owner,f),"out of range character class denied");
}
void Keys(){Owner o;const auto id=o.Grant(2,7);o.Grant(2,3);auto f=o.Facts();auto c=o.Use(id,f).value();Check(c.items.size()==1 && c.items[0].after.id==id,"normal use debits the exact selected key");Check(c.result.back()==2,"normal key advances floor");o.Apply(c);Check(o.Count(2)==9,"advance spends exactly one");f.floor=5;f.type=6;f.leader=false;Check(!o.Use(id,f),"offering requires an actual nearby pedestal");f.pedestal=true;c=o.Use(id,f).value();Check(c.result.back()==3,"any admitted party member can offer on floor five");o.Apply(c);Check(o.Count(2)==8,"offering spends exactly one");f.type=7;Check(!o.Use(id,f),"completed altar phase cannot spend another key");}
void Cleanup(){Owner o;o.Grant(3,7);o.Grant(3,3);auto c=MakeDungeonCleanupCommit(*o.catalog,o.Op(),o.fence,o.layouts,o.items,Key(3),3).value();o.Apply(c);Check(o.Count(3)==7,"bounded cleanup preserves exact remaining units");c=MakeDungeonCleanupCommit(*o.catalog,o.Op(),o.fence,o.layouts,o.items,Key(3)).value();o.Apply(c);Check(!o.Count(3),"full native cleanup leaves no live key");Check(!MakeDungeonCleanupCommit(*o.catalog,o.Op(),o.fence,o.layouts,o.items,Key(3)),"empty cleanup has no fabricated write");}
void StageActions(){
 for(unsigned n=0;n<DungeonUseBindings.size();++n){Owner o;const auto id=o.Grant(4+n,3);const auto source=o.Find(id);DungeonUseFacts f;f.admitted=f.stageReady=true;const auto rune=o.Facts();f.character.level=255;f.character.characterClass=rune.characterClass;f.character.sex=rune.sex;
  Check(DungeonItemUseRole(*o.catalog,source.key)==DungeonUseBindings[n].role,"all authored stage roles resolve by native content code");
  const auto plan=[&](DungeonUseFacts facts){return MakeDungeonUseCommit(*o.catalog,o.Op(),o.fence,o.layouts,o.items,id,facts);};
  auto bad=f;bad.admitted=false;Check(!plan(bad),"unadmitted dungeon use never debits");bad=f;bad.stageReady=false;Check(!plan(bad),"unready or wrong NPC stage never debits");bad=f;bad.character.level=0;Check(!plan(bad),"missing native character denied");bad=f;bad.character.characterClass=4;Check(!plan(bad),"invalid class denied");bad=f;bad.character.sex=2;Check(!plan(bad),"invalid sex denied");
  auto commit=plan(f).value();Check(commit.items.size()==1&&commit.items[0].before->id==id&&commit.items[0].after.count==2&&commit.result.back()==static_cast<unsigned>(DungeonUseBindings[n].role),"stage commit spends exact selected native identity and records role");
  auto foreign=o.fence;++foreign.owner.id;Check(!MakeDungeonUseCommit(*o.catalog,o.Op(),foreign,o.layouts,o.items,id,f),"foreign stage item denied");
  auto listed=o.items;listed.front().privateShop=PrivateShopListing{{1},1,0};Check(!MakeDungeonUseCommit(*o.catalog,o.Op(),o.fence,o.layouts,listed,id,f),"shop-listed stage item cannot be offered");
  o.Apply(commit);Check(o.Count(4+n)==2,"confirmed stage debit preserves remaining stack");
 }
}

}
int main(){try{Exchange();Admission();Keys();Cleanup();StageActions();std::cout<<checks<<" Dungeon item checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
