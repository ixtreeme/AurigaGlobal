#include "SpecialBonusFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <item/EquipmentStats.hpp>
#include <item/TypedInventoryWire.hpp>
#include <algorithm>
#include <set>
#include <iostream>
#include <stdexcept>
#include <source_location>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <item/TypedItemDetails.hpp>
using namespace auriga::item;namespace f=special_bonus_fixture;
namespace {
unsigned checks=0;std::filesystem::path goldenDirectory;void Check(bool v,const char* msg,std::source_location loc=std::source_location::current()){++checks;if(!v)throw std::runtime_error(std::string(msg)+" line="+std::to_string(loc.line()));}
ItemOwner owner{ItemOwnerKind::Character,29};ItemOwnerFence fence{owner,1,1};std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},10,9,1},ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},19,1,1}};std::vector<PersistentItem> items;
void Apply(const ItemCommit& commit){for(const auto& ch:commit.items){auto i=std::ranges::find(items,ch.after.id,&PersistentItem::id);if(i==items.end())items.push_back(ch.after);else *i=ch.after;}++fence.revision;}
PersistentItem& Item(std::uint64_t id){return *std::ranges::find(items,id,[](const auto& i){return i.id.Value();});}
std::expected<ItemCommit,ItemPersistenceError> Service(const ItemCatalog& c,unsigned source,unsigned target,unsigned selection=0,std::uint64_t elapsed=0){return MakeSpecialBonusCommit(c,{static_cast<std::uint8_t>(fence.revision)},fence,layouts,items,Item(source).id,Item(target).id,SpecialBonusAction::Service,static_cast<std::uint8_t>(selection),elapsed,{120,0,0},[](std::uint32_t max){return std::optional<std::uint32_t>{max==UINT32_MAX ? max/2 : max};});}
void Run(){auto built=BuildItemCatalog(f::Catalog());Check(built.has_value(),"special catalog");const auto& c=**built;
    std::uint64_t next=1;const auto grant=[&](ItemKey key,unsigned count=1){ItemGrant g{key};g.count=count;g.createdAtUtcMs=1000;std::array ids{ItemInstanceId::Create(next).value()};auto result=MakeGrantCommit(c,{static_cast<std::uint8_t>(next)},fence,layouts,items,g,ids);Check(result.has_value(),"real grant");Apply(*result);return next++;};
    const auto stole=grant(f::Regalia(1)),enchant=grant(f::Tool(1),3),bottle=grant(f::Tool(2)),reset=grant(f::Tool(3),3),donor=grant(f::Tool(4)),remove=grant(f::Tool(5),3),addon=grant(f::Tool(6),3),costume=grant(f::Costume());
    {   // A stole granted with any instance ID (not only the template placeholder 1) draws for that ID.
        ItemGrant g{f::Regalia(1)};g.createdAtUtcMs=1000;const std::array ids{ItemInstanceId::Create(4242).value()};
        const auto other=MakeGrantCommit(c,{99},fence,layouts,items,g,ids);
        Check(other.has_value() && other->items.size()==1 && other->items[0].after.regalia==InitialRegalia(std::get<ItemRegaliaRow>(c.Find(f::Regalia(1))->category),other->operationId,4242),"stole drawn for its own instance id");
    }
    unsigned randomCalls=0;
    const auto countedRandom=[&](std::uint32_t){++randomCalls;return std::optional<std::uint32_t>{0};};
    Check(!MakeSpecialBonusCommit(c,{91},fence,{},items,Item(enchant).id,Item(stole).id,SpecialBonusAction::Service,0,0,{120,0,0},countedRandom) && !randomCalls,"missing storage denies before RNG");
    Check(!MakeSpecialBonusCommit(c,{92},fence,layouts,items,Item(enchant).id,Item(stole).id,SpecialBonusAction::Service,0,0,{0,9,9},countedRandom) && !randomCalls,"invalid actor denies before RNG");
    Check(ValidRegaliaState(std::get<ItemRegaliaRow>(c.Find(Item(stole).key)->category),*Item(stole).regalia),"factory six values");
    auto reroll=Service(c,enchant,stole);Check(reroll.has_value(),"six-way enchant");auto tampered=*reroll;tampered.items[1].after.regalia->bonuses[0]->valueScaled++;Check(!ValidateItemCommit(c,tampered),"receipt rejects arbitrary enchant");tampered=*reroll;tampered.result.push_back(0);Check(!ValidateItemCommit(c,tampered),"receipt rejects unused random data");tampered=*reroll;tampered.items[0].after.count++;Check(!ValidateItemCommit(c,tampered),"receipt requires exact source debit");Apply(*reroll);for(unsigned j=0;j<6;++j)Check(Item(stole).regalia->bonuses[j]->valueScaled==(j>=4 ? 4000 : 4),"all six selected variants");
    auto equip=MakeEquipCommit(c,{20},fence,layouts,items,Item(stole).id,{120,0,0});Check(equip.has_value(),"stole equips own slot");Apply(*equip);auto stats=BuildEquipmentStats(c,owner,items);Check(stats && stats->Bonus(BonusId::Strength)==4,"stole stat source");Check(!Service(c,enchant,stole),"worn stole cannot enchant");auto off=MakeUnequipCommit(c,{21},fence,layouts,items,Item(stole).id,{ItemStorageKind::Inventory,1,60});Check(off.has_value(),"stole takeoff");Apply(*off);
    Item(costume).attributes.slots[0]=BonusEntry{BonusId::Strength,33}; // Persisted initial nonzero normal row.
    auto add=Service(c,donor,costume);Check(add.has_value(),"instance donor");Apply(*add);Check(Item(costume).attributes.slots[5]==BonusEntry{BonusId::Dexterity,7} && Item(donor).destroyed,"exact donor and atomic consumption");
    auto resetResult=Service(c,reset,costume);Check(resetResult.has_value(),"reset prepares three normal rows");Apply(*resetResult);Check(Item(costume).attributes.slots[0]->valueScaled==200 && Item(costume).attributes.slots[1]->valueScaled==100 && Item(costume).attributes.slots[2]->valueScaled==100 && Item(costume).attributes.slots[5]==BonusEntry{BonusId::Dexterity,7},"per-draw tier and rare preservation");
    auto erase=Service(c,remove,costume);Check(erase.has_value(),"selected rare removal");Apply(*erase);Check(!Item(costume).attributes.slots[5],"rare removed");Check(!Service(c,remove,costume),"empty removal no consumption");
    const auto weapon=grant(item_catalog_fixture::Sword());auto pair=Service(c,addon,weapon);Check(pair.has_value(),"paired Gaussian addon");Apply(*pair);Check(Item(weapon).attributes.slots[0] && Item(weapon).attributes.slots[1],"coupled rows");
    std::vector<unsigned> runes;for(unsigned slot=1;slot<=7;++slot){const auto id=grant(f::Regalia(slot+1));runes.push_back(static_cast<unsigned>(id));auto wear=MakeEquipCommit(c,{static_cast<std::uint8_t>(slot+30)},fence,layouts,items,Item(id).id,{120,0,0});Check(wear.has_value(),"seven independent slots");Apply(*wear);if(slot<7){auto active=MakeSpecialBonusCommit(c,{static_cast<std::uint8_t>(slot+40)},fence,layouts,items,Item(id).id,{},SpecialBonusAction::Activate,0,0,{120,0,0},{});Check(active.has_value(),"activate rune");Apply(*active);}}
    stats=BuildEquipmentStats(c,owner,items);Check(stats && stats->Bonus(BonusId::Strength)==43,"six full runes and gated bonus rune");
    const auto first=runes.front();auto tick=MakeSpecialBonusCommit(c,{70},fence,layouts,items,Item(first).id,{},SpecialBonusAction::Maintain,0,40000,{120,0,0},{});Check(tick.has_value(),"wear time debit");Apply(*tick);stats=BuildEquipmentStats(c,owner,items);Check(stats && stats->Bonus(BonusId::Strength)==39,"tier and set gate update");
    auto charge=Service(c,bottle,first);Check(charge.has_value(),"charge active rune");Apply(*charge);Check(Item(first).regalia->remainingMs==56000 && Item(bottle).destroyed,"charge consumes exact bottle");
    auto stop=MakeSpecialBonusCommit(c,{71},fence,layouts,items,Item(first).id,{},SpecialBonusAction::Deactivate,0,1000,{120,0,0},{});Check(stop.has_value(),"stop charges tail");Apply(*stop);Check(!Item(first).regalia->active && Item(first).regalia->remainingMs==55000,"paused persisted budget");
    auto start=MakeSpecialBonusCommit(c,{72},fence,layouts,items,Item(first).id,{},SpecialBonusAction::Activate,0,0,{120,0,0},{});Check(start.has_value(),"restart");Apply(*start);auto expired=MakeSpecialBonusCommit(c,{73},fence,layouts,items,Item(first).id,{},SpecialBonusAction::Maintain,0,55000,{120,0,0},{});Check(expired.has_value(),"exhaust rune");Apply(*expired);Check(!Item(first).destroyed && !Item(first).regalia->active && !Item(first).regalia->remainingMs,"exhaustion preserves rechargeable identity");
    for(const auto& i:items){if(i.regalia){const auto b=EncodeRegaliaState(*i.regalia);Check(DecodeRegaliaState(b)==i.regalia,"state codec");for(unsigned n=0;n<b.size();++n)Check(!DecodeRegaliaState(std::span(b).first(n)),"state truncation");}}
    if(!goldenDirectory.empty()){grant(f::Tool(2));grant(f::Tool(4));}
    std::vector<PersistentItem> live;for(const auto& i:items)if(!i.destroyed)live.push_back(i);auto wire=EncodeTypedInventory(c,layouts,{},live);Check(wire.has_value() && std::to_integer<unsigned>((*wire)[5])==14,"v14");auto decoded=DecodeTypedInventory(*wire,owner);for(auto& i:live)i.revision=1;Check(decoded && decoded->items==live,"inventory roundtrip");
    if(!goldenDirectory.empty()){std::filesystem::create_directories(goldenDirectory);const auto save=[&](const char* name,const std::vector<std::byte>& data){std::ofstream out(goldenDirectory/name);for(auto b:data)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';Check(out.good(),"golden write");};save("typed_regalia_v14.hex",*wire);const auto details=EncodeTypedItemDetails(c,live,false);Check(details.has_value() && std::to_integer<unsigned>((*details)[5])==3,"v3 details");save("typed_special_details_v3.hex",*details);}
    for(unsigned n=0;n<wire->size();++n)Check(!DecodeTypedInventory(std::span(*wire).first(n),owner),"wire truncation");
}
void RandomGrantPool(){
    auto s=f::Catalog();auto& tool=s.bonusTools[3];tool.payload.reset();
    // Source costumes have a normal reroll profile, without random rare rows.
    std::erase_if(s.attributeProfiles.back().groups,[](const auto& g){return g.pool==AttributePool::Rare;});
    std::erase_if(s.attributeProfiles.back().candidates,[](const auto& c){return c.pool==AttributePool::Rare;});
    for(unsigned i=1;i<=5;++i)tool.choices.push_back({static_cast<std::uint8_t>(i),{static_cast<BonusId>(28+i),static_cast<std::int64_t>(i)}});
    Check(ValidBonusTool(tool),"explicit uniform costume pool validates");const auto c=BuildItemCatalog(s).value();
    Check(auriga::item::sql::DecodeSnapshot(catalog_snapshot_fixture::Snapshot(s),{1,c->ContentHash()}).has_value(),"random choices survive SQL snapshot and digest");
    const ItemOwner own{ItemOwnerKind::Character,88};const ItemOwnerFence fce{own,1,1};const std::array bag{ItemStorageLayout{{own,ItemStorageKind::Inventory,1},5,9,1}};
    std::set<BonusId> reached;
    for(unsigned i=1;i<=128;++i){const std::array<std::uint8_t,16> op{static_cast<std::uint8_t>(i)};const std::array ids{ItemInstanceId::Create(i+4000).value()};
        const auto grant=MakeGrantCommit(*c,op,fce,bag,{},ItemGrant{tool.key},ids);Check(grant.has_value(),"random donor is granted through real factory");const auto state=grant->items[0].after.bonusTool.value();
        Check(state==InitialBonusTool(tool,op,ids[0].Value()) && ValidBonusToolState(tool,state),"server operation and allocated instance determine saved payload");reached.insert(state.payload->bonus);
        auto forged=*grant;forged.items[0].after.bonusTool->payload->valueScaled++;Check(!ValidateItemCommit(*c,forged),"birth proof refuses caller-selected payload");
        const auto bytes=EncodeBonusToolState(state);Check(DecodeBonusToolState(bytes)==state,"persisted payload decodes without drawing again");
    }
    Check(reached.size()==5,"all five configured choices reachable");
    InstanceAttributes donorOnly;donorOnly.slots[5]=tool.choices[0].payload;
    Check(ValidateInstanceAttributes(*c,f::Costume(),donorOnly).has_value(),"explicit donor uses a rare slot without creating a random rare pool");
    donorOnly.slots[6]=tool.choices[0].payload;Check(!ValidateInstanceAttributes(*c,f::Costume(),donorOnly),"donor-only slots still forbid duplicate bonus types");
    auto changed=s;changed.bonusTools[3].choices[0].payload.valueScaled++;Check(BuildItemCatalog(changed).value()->ContentHash()!=c->ContentHash(),"pool values covered by catalog digest");
    std::ranges::reverse(s.bonusTools[3].choices);Check(BuildItemCatalog(s).value()->ContentHash()==c->ContentHash(),"pool input order canonicalized");
    auto bad=[&](auto edit){auto x=changed;edit(x.bonusTools[3]);Check(!BuildItemCatalog(x),"malformed random donor rejected");};
    bad([](auto& t){t.payload=BonusEntry{BonusId::Strength,1};});bad([](auto& t){t.choices[1].payload=t.choices[0].payload;});bad([](auto& t){t.choices[0].choice=0;});bad([](auto& t){t.operation=SpecialBonusOperation::PairedAddon;});
}
void TimedStoles(){
    for(unsigned kind=2;kind<=4;++kind){
        auto source=f::Catalog();ItemDefinitionRules rule{f::Regalia(1)};rule.clock=static_cast<CostumeClock>(kind);rule.durationMs=10000;source.definitionRules.push_back(rule);
        const auto c=BuildItemCatalog(source).value();items.clear();fence={owner,1,1};
        const auto id=ItemInstanceId::Create(9000+kind).value();const std::array ids{id};ItemGrant g{f::Regalia(1)};g.createdAtUtcMs=1000;
        if(kind==2){auto missing=g;missing.createdAtUtcMs=0;Check(!MakeGrantCommit(*c,{77},fence,layouts,items,missing,ids),"creation-clock stole requires actual issuance UTC");}
        Apply(MakeGrantCommit(*c,{78},fence,layouts,items,g,ids).value());
        EquipContext actor{120,0,0,2000};Apply(MakeEquipCommit(*c,{79},fence,layouts,items,id,actor).value());
        const auto saved=Item(id.Value()).costume.value();
        const auto wire=EncodeTypedInventory(*c,layouts,{},items).value();Check(wire[5]==std::byte{24},"timed stole gets explicit inventory v24");
        const auto decoded=DecodeTypedInventory(wire,owner).value();Check(decoded.items[0].costume==saved && decoded.stoleClocks.at(id).clock==rule.clock,"native stole lifetime rules and state roundtrip");
        auto version=wire;version[5]=std::byte{23};Check(!DecodeTypedInventory(version,owner),"old decoder layout cannot consume a stole clock extension");
        const std::array debt{CostumeElapsed{id.Value(),kind==4 ? 400u : 0u}};actor.costumeElapsed=debt;
        Apply(MakeUnequipCommit(*c,{80},fence,layouts,items,id,{ItemStorageKind::Inventory,1,0},actor).value());
        Check(Item(id.Value()).costume->remainingMs==(kind==4 ? 9600u : 0u),"wear-time stole settles exact server debt");
        actor.costumeElapsed={};Apply(MakeEquipCommit(*c,{81},fence,layouts,items,id,actor).value());
        const auto expiry=MakeCostumeClockCommit(*c,{82},fence,layouts,items,id,500000,kind==4 ? 9600 : 0).value();
        Check(expiry.items[0].after.destroyed && expiry.items[0].after.costume->expired,"all finite stole clock modes destroy at expiry");
        auto forged=expiry;forged.items[0].after.costume->expired=false;Check(!ValidateItemCommit(*c,forged),"clock expiry cannot be forged");
    }
}

}
int main(int argc,char** argv){try{if(argc==3 && std::string_view(argv[1])=="--write-golden")goldenDirectory=argv[2];Run();RandomGrantPool();TimedStoles();std::cout<<"SPECIAL_BONUS_TESTS=SUCCESS checks="<<checks<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
