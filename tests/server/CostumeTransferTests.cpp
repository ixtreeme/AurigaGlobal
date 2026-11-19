#include "SpecialBonusFixture.hpp"
#include <item/CostumeTransferWire.hpp>
#include <item/EquipmentStats.hpp>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;namespace f=special_bonus_fixture;
namespace {
unsigned checks=0;void Check(bool v,const char* message){++checks;if(!v)throw std::runtime_error(message);}
ItemKey Scroll(){return f::Tool(8);}
ItemKey Hair(){return ItemKey::Create(ItemCategory::Costume,64001).value();}
std::shared_ptr<const ItemCatalog> Catalog(){auto s=f::Catalog();auto tool=*std::ranges::find(s.items,f::Tool(3),&ItemBaseRow::key);tool.key=Scroll();tool.contentCode="transfer.scroll";tool.maxStack=10;tool.minimumLevel=0;s.items.push_back(tool);s.bonusTools.push_back({Scroll(),SpecialBonusOperation::CostumeTransfer,1});
    auto hair=*std::ranges::find(s.items,f::Costume(),&ItemBaseRow::key);hair.key=Hair();hair.contentCode="transfer.hair";s.items.push_back(hair);s.costumes.push_back({Hair(),CostumeKind::Hair,{},CostumeClock::Permanent,0});for(unsigned c=1;c<=4;++c)for(unsigned sex=1;sex<=2;++sex)s.costumeVisuals.push_back({Hair(),static_cast<std::uint8_t>(c),static_cast<std::uint8_t>(sex),"models/costume.model"});
    s.attributeBindings.push_back({Hair(),880});for(auto& p:s.attributeProfiles)if(p.id==880)p.allowLock=true;
    s.costumeTransferTargets={f::Costume(),Hair(),f::Regalia(1)};return BuildItemCatalog(std::move(s)).value();}
struct Owner {
    std::shared_ptr<const ItemCatalog> c=Catalog();ItemOwnerFence fence{{ItemOwnerKind::Character,77},1,1};std::array<ItemStorageLayout,1> layouts{ItemStorageLayout{{fence.owner,ItemStorageKind::Inventory,1},10,9,1}};std::vector<PersistentItem> items;unsigned serial=0;std::uint64_t next=700000;
    auto Id(){return ItemInstanceId::Create(++next).value();}auto Op(){return std::array<std::uint8_t,16>{0x54,static_cast<std::uint8_t>(++serial),static_cast<std::uint8_t>(serial>>8)};}
    CostumeTransferActor Actor(){CostumeTransferActor a;a.admitted=a.alive=a.nearNpc=true;a.equipment={120,0,0};a.equipment.nowUtcMs=100000;return a;}
    PersistentItem& Find(ItemInstanceId id){return *std::ranges::find(items,id,&PersistentItem::id);}
    void Apply(const ItemCommit& r){Check(ValidateItemCommit(*c,r).has_value(),"complete authenticated three-item native write");for(const auto& ch:r.items){auto i=std::ranges::find(items,ch.after.id,&PersistentItem::id);if(ch.after.destroyed){if(i!=items.end())items.erase(i);}else if(i==items.end())items.push_back(ch.after);else *i=ch.after;}++fence.revision;Check(EncodeTypedInventory(*c,layouts,{},items).has_value(),"native post-transfer inventory serializes");}
    ItemInstanceId Grant(ItemKey key,unsigned count=1){std::array ids{Id(),Id(),Id(),Id()};Apply(MakeGrantCommit(*c,Op(),fence,layouts,items,{key,0,count,{},{},{},1000},ids).value());return ids[0];}
    auto Quote(ItemInstanceId a,ItemInstanceId b,ItemInstanceId d,CostumeTransferActor actor){return PrepareCostumeTransfer(*c,fence.owner,items,a,b,d,actor);}
    auto Commit(ItemInstanceId a,ItemInstanceId b,ItemInstanceId d){return MakeCostumeTransferCommit(*c,Op(),fence,layouts,items,a,b,d,Actor());}
};
void Ordinary(){Owner o;auto scroll=o.Grant(Scroll(),2),target=o.Grant(f::Costume()),donor=o.Grant(f::Costume());auto& d=o.Find(donor).attributes;d.slots[0]=BonusEntry{BonusId::Strength,100};d.slots[1]=BonusEntry{BonusId::Dexterity,200};d.slots[5]=BonusEntry{BonusId::Constitution,100};d.slots[6]=BonusEntry{BonusId::Intelligence,200};d.slots[7]=BonusEntry{BonusId::NormalOutgoing,100};d.lockedSlot=1;
    auto& t=o.Find(target).attributes;t.slots[0]=BonusEntry{BonusId::Strength,200};t.lockedSlot=0;const auto before=o.Find(target);const auto q=o.Quote(scroll,target,donor,o.Actor()).value();
    Check(q.kind==1 && q.attributes.slots[0]==d.slots[0] && q.attributes.slots[1]==d.slots[1],"normal donor slots copied exactly");Check(!q.attributes.slots[5] && !q.attributes.slots[6] && q.attributes.slots[7]==d.slots[7],"original two rare slots clear and extra-rare slot7 survives");Check(q.attributes.lockedSlot==0,"receiver lock retained rather than donor lock");
    auto r=o.Commit(scroll,target,donor).value();for(unsigned part=0;part<3;++part){auto bad=r;if(part==0)++bad.items[0].after.count;else if(part==1)++bad.items[1].after.attributes.slots[0]->valueScaled;else bad.items[2].after.destroyed=false;Check(!ValidateItemCommit(*o.c,bad),"receipt rejects extra consumption, forged bonuses or retained donor");}
    o.Apply(r);Check(o.Find(scroll).count==1 && std::ranges::find(o.items,donor,&PersistentItem::id)==o.items.end(),"one scroll and entire donor consumed together");Check(o.Find(target).id==before.id && o.Find(target).key==before.key && o.Find(target).costume==before.costume && o.Find(target).attributes==q.attributes,"receiver identity and lifetime preserved with exact copied state");
    auto packet=EncodeTypedInventory(*o.c,o.layouts,{},o.items).value();Check(packet[5]==std::byte{28} && DecodeTypedInventory(packet,o.fence.owner).has_value(),"transfer tool uses explicit native GC225v28");packet[5]=std::byte{27};Check(!DecodeTypedInventory(packet,o.fence.owner),"old inventory versions reject the new operation");
    const auto wire=EncodeCostumeTransferRuntime(*o.c,{CostumeTransferStatus::Applied,123,scroll.Value(),target.Value(),donor.Value(),&q},o.layouts,o.items);Check(wire.has_value() && (*wire)[5]==std::byte{3},"actual native after-preview uses shared GC220v3 encoder");
}
void Admission(){Owner o;const auto scroll=o.Grant(Scroll()),target=o.Grant(f::Costume()),donor=o.Grant(f::Costume()),hair=o.Grant(Hair());o.Find(donor).attributes.slots[0]=BonusEntry{BonusId::Strength,100};auto a=o.Actor();
    for(unsigned flag=0;flag<3;++flag){auto bad=a;if(flag==0)bad.admitted=false;else if(flag==1)bad.alive=false;else bad.nearNpc=false;Check(!o.Quote(scroll,target,donor,bad),"session, death and actual NPC gate all required");}
    Check(!o.Quote(scroll,target,target,a) && !o.Quote(scroll,target,hair,a),"three distinct owned items and matching original subtype required");
    const auto keep=o.items;o.Find(donor).owner.id++;Check(!o.Quote(scroll,target,donor,a),"foreign donor refused");o.items=keep;
    o.Find(donor).attributes.slots[0].reset();Check(!o.Quote(scroll,target,donor,a),"empty donor does not consume anything");o.items=keep;
    Check(!MakeSpecialBonusCommit(*o.c,o.Op(),o.fence,o.layouts,o.items,scroll,target,SpecialBonusAction::Service,0,0,a.equipment,{}),"two-item service cannot consume a three-item transfer scroll");
    const auto q=o.Quote(scroll,target,donor,a).value();Check(EncodeCostumeTransferRuntime(*o.c,{CostumeTransferStatus::Ready,77,scroll.Value(),target.Value(),donor.Value(),&q},o.layouts,o.items).has_value(),"real before-state builds exact receiver preview");Check(!EncodeCostumeTransferRuntime(*o.c,{CostumeTransferStatus::Ready,77,scroll.Value(),target.Value(),donor.Value(),nullptr},o.layouts,o.items),"ready without a preview refused");
}
void Stole(){Owner o;const auto scroll=o.Grant(Scroll()),target=o.Grant(f::Regalia(1)),donor=o.Grant(f::Regalia(1));const auto source=*o.Find(donor).regalia;const auto q=o.Quote(scroll,target,donor,o.Actor()).value();Check(q.kind==4 && q.stole && !q.stole->bonuses[5],"original rare index5 is cleared in native six-bonus stole state");for(unsigned i=0;i<5;++i)Check(q.stole->bonuses[i]==source.bonuses[i],"five original stole bonuses copied without rerolling");o.Apply(o.Commit(scroll,target,donor).value());
    const auto& row=std::get<ItemRegaliaRow>(o.c->Find(f::Regalia(1))->category);Check(RegaliaContributions(row,*o.Find(target).regalia).size()==5,"cleared sixth bonus makes no equipment contribution");auto bad=*o.Find(target).regalia;bad.bonuses[0].reset();Check(!ValidRegaliaState(row,bad),"other mandatory stole slots cannot disappear");
    auto packet=EncodeTypedInventory(*o.c,o.layouts,{},o.items).value();Check(packet[5]==std::byte{28} && DecodeTypedInventory(packet,o.fence.owner).has_value(),"post-transfer five-slot stole roundtrips in v28");packet[5]=std::byte{27};Check(!DecodeTypedInventory(packet,o.fence.owner),"old versions require original six-slot stole shape");
    const auto reroll=o.Grant(f::Tool(1));const auto r=MakeSpecialBonusCommit(*o.c,o.Op(),o.fence,o.layouts,o.items,reroll,target,SpecialBonusAction::Service,0,0,o.Actor().equipment,[](std::uint32_t){return std::optional<std::uint32_t>{0};});Check(r.has_value(),"existing stole reroll accepts a transferred stole");o.Apply(*r);Check(o.Find(target).regalia->bonuses[5].has_value(),"normal reroll restores all six original bonuses");
}
}
int main(){try{Ordinary();Admission();Stole();std::cout<<checks<<" Costume transfer checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
