#include "ItemCatalogFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <item/BuiltinUtilityRuntime.hpp>
#include <algorithm>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace
{
std::size_t checks=0;
void Check(bool ok,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!ok)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
ItemKey Token(){return ItemKey::Create(ItemCategory::Quest,1).value();}
ItemKey Scroll(){return ItemKey::Create(ItemCategory::Quest,2).value();}
ItemCatalogSource Fixture()
{
    auto s=f::Catalog();
    auto base=*std::ranges::find(s.items,f::Leather(),&ItemBaseRow::key);
    base.key=Token();base.contentCode="quest.soul_orb";base.nameKey="item.soul_orb.name";base.descriptionKey="item.soul_orb.description";
    s.items.push_back(base);s.questItems.push_back({Token(),QuestItemDispatch::Token,"",0,false});
    base.key=Scroll();base.contentCode="quest.horse_scroll";base.nameKey="item.horse_scroll.name";base.descriptionKey="item.horse_scroll.description";
    base.minimumLevel=50;s.items.push_back(base);
    s.questItems.push_back({Scroll(),QuestItemDispatch::FirstHandler,"quest.horse_training",9,false});
    return s;
}
QuestItemUseRequest Request(){return {Scroll(),5,50,QuestItemContext::World,false,false,true,true};}
void Dispatch()
{
    auto source=Fixture();const auto catalog=BuildItemCatalog(source).value();
    auto req=Request();auto plan=PlanQuestItemUse(*catalog,req).value();
    Check(catalog->Size()==f::FixtureItems+2,"quest tokens are native category children");
    Check(plan.item==Scroll() && plan.groupCode=="quest.horse_training" && plan.dispatch==QuestItemDispatch::FirstHandler,"typed quest dispatch identity");
    Check(plan.unitsToConsume==0 && req.stackUnits==5,"dispatch never pre-consumes a quest item");
    Check(plan.catalogRevision==catalog->Revision() && plan.catalogHash==catalog->ContentHash(),"intent retains immutable content identity");
    source.questItems.clear();Check(plan.groupCode=="quest.horse_training","intent owns handler code");
    for(auto context:{QuestItemContext::Duel,QuestItemContext::Arena})
    {
        req=Request();req.context=context;
        Check(PlanQuestItemUse(*catalog,req).error()==QuestItemUseError::Forbidden,"horse-style script is blocked in configured modes");
    }
    req=Request();req.context=QuestItemContext::GuildWar;Check(PlanQuestItemUse(*catalog,req).has_value(),"configured guild-war use");
    req=Request();req.item=Token();Check(PlanQuestItemUse(*catalog,req).error()==QuestItemUseError::NotUsable,"passive soul orb cannot invent a quest use");
    req=Request();req.item=f::Leather();Check(PlanQuestItemUse(*catalog,req).error()==QuestItemUseError::NotQuestItem,"materials are not quest dispatch");
    req=Request();req.item=ItemKey::Create(10,999).value();Check(PlanQuestItemUse(*catalog,req).error()==QuestItemUseError::MissingItem,"missing key rejected");
    for(int kind=0;kind<7;++kind)
    {
        req=Request();auto expected=QuestItemUseError::Forbidden;
        if(kind==0)req.admittedByServer=false;
        if(kind==1)req.observer=true;
        if(kind==2)req.inCombat=true;
        if(kind==3){req.handlerResolvedByServer=false;expected=QuestItemUseError::HandlerUnavailable;}
        if(kind==4){req.characterLevel=49;expected=QuestItemUseError::LevelTooLow;}
        if(kind==5){req.stackUnits=0;expected=QuestItemUseError::InvalidStack;}
        if(kind==6){req.stackUnits=201;expected=QuestItemUseError::InvalidStack;}
        Check(PlanQuestItemUse(*catalog,req).error()==expected,"server admission, handler, level and stack gates");
    }
    for(auto code:{0,3,5,16,255})
    {req=Request();req.context=static_cast<QuestItemContext>(code);Check(PlanQuestItemUse(*catalog,req).error()==QuestItemUseError::InvalidContext,"combined/unknown context cannot bypass policy");}
    source=Fixture();source.questItems[1].dispatch=QuestItemDispatch::AllHandlers;source.questItems[1].groupCode="";
    source.questItems[1].allowObserver=true;source.questItems[1].allowedContexts=15;
    auto all=BuildItemCatalog(source).value();req=Request();req.observer=true;
    auto allPlan=PlanQuestItemUse(*all,req).value();
    Check(allPlan.dispatch==QuestItemDispatch::AllHandlers && allPlan.groupCode.empty(),"explicit all-handlers mode, routed by ItemKey");
    for(auto context:{QuestItemContext::World,QuestItemContext::Duel,QuestItemContext::Arena,QuestItemContext::GuildWar})
    {req.context=context;Check(PlanQuestItemUse(*all,req).has_value(),"all enabled contexts admit");}
    auto retired=Fixture();std::ranges::find(retired.items,Scroll(),&ItemBaseRow::key)->retired=true;
    Check(PlanQuestItemUse(*BuildItemCatalog(retired).value(),Request()).error()==QuestItemUseError::RetiredItem,"retired item never dispatches");
    auto visual=Fixture();std::ranges::find(visual.items,Token(),&ItemBaseRow::key)->appearanceProfile=1;
    auto rendered=BuildItemCatalog(visual).value();
    Check(rendered->FindAppearance(*rendered->Find(Token())->base.appearanceProfile)!=nullptr,"quest ground model does not turn a token into equipment");
}
void Validate()
{
    auto bad=[](auto mutate){auto s=Fixture();mutate(s);Check(!BuildItemCatalog(s),"invalid quest catalogue accepted");};
    bad([](auto& s){s.questItems[0].allowedContexts=1;});
    bad([](auto& s){s.questItems[0].groupCode="quest.invalid";});
    bad([](auto& s){s.questItems[0].allowObserver=true;});
    bad([](auto& s){s.questItems[1].allowedContexts=0;});
    bad([](auto& s){s.questItems[1].allowedContexts=16;});
    bad([](auto& s){s.questItems[1].groupCode="../quest.lua";});
    bad([](auto& s){s.questItems[1].groupCode="quest..invalid";});
    bad([](auto& s){s.questItems[1].groupCode=std::string(97,'a');});
    bad([](auto& s){s.questItems[1].dispatch=static_cast<QuestItemDispatch>(0);});
    bad([](auto& s){s.questItems[1].dispatch=static_cast<QuestItemDispatch>(4);});
    bad([](auto& s){s.questItems.pop_back();});
    bad([](auto& s){s.questItems.push_back(s.questItems[0]);});
    bad([](auto& s){s.questItems[0].key=f::Sword();});
    bad([](auto& s){s.questItems[0].key=ItemKey::Create(10,999).value();});
    bad([](auto& s){std::ranges::find(s.items,Token(),&ItemBaseRow::key)->baseBonusProfile=1;});
    bad([](auto& s){std::ranges::find(s.items,Token(),&ItemBaseRow::key)->refinementProfile=2;});
    bad([](auto& s){std::ranges::find(s.items,Token(),&ItemBaseRow::key)->binding=BindingPolicy::OnEquip;});
}
void BuiltinUtilities()
{
    const ItemOwner owner{ItemOwnerKind::Character,778};const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4}};const ItemOwnerFence fence{owner,1,1};
    for(const auto group:{"cleanse","invisibility","white_flag","courage_combined","firework_random","firework_1","firework_4","firework_7","china_firework","spinning_top","restore_hp_full","restore_sp_full","restore_stamina_full","teleport_ring","self_poison","polymorph_cancel","spouse_warp"}) {
        auto definition=Fixture();definition.questItems[1].groupCode=std::string("utility.")+group;definition.questItems[1].allowedContexts=15;const auto c=BuildItemCatalog(definition).value();const std::array ids{ItemInstanceId::Create(650001).value()};const std::array<std::uint8_t,16> op{0xAB};
        const auto grant=MakeGrantCommit(*c,op,fence,layouts,{},ItemGrant{Scroll(),0,2},ids).value();const auto source=grant.items[0].after;
        BuiltinUtilityFacts facts{{Scroll(),2,50,QuestItemContext::World,false,false,true,false},0,0,21,true,false};
        facts.summonZoneAllowed=facts.warpAllowed=facts.marriageAvailable=true;facts.maximumSp=1003;facts.currentSp=300;
        const auto p=PrepareBuiltinUtility(*c,source,owner,facts).value();Check(p.handler==BuiltinQuestHandler(definition.questItems[1].groupCode),"compiled utility handler resolves without an old item identity");
        Check(p.consume==(p.handler==BuiltinQuestItem::TeleportRing || p.handler==BuiltinQuestItem::PolymorphCancel || p.handler==BuiltinQuestItem::SpouseWarp ? 0 : 1),"native reusable utilities never consume their source");
        if(p.handler==BuiltinQuestItem::SpouseWarp){
            Check(p.spCost==300,"original spouse ring costs floor of30 percent maximum SP");
            for(unsigned gate=0;gate<7;++gate){auto denied=facts;if(gate==0)denied.summonZoneAllowed=false;if(gate==1)denied.warpAllowed=false;if(gate==2)denied.marriageAvailable=false;if(gate==3)denied.marriageArenaBlocked=true;if(gate==4)denied.currentSp=299;if(gate==5)denied.maximumSp=UINT64_MAX;if(gate==6)denied.currentSp=UINT64_MAX;Check(!PrepareBuiltinUtility(*c,source,owner,denied),"spouse warp requires actual zone, portal, marriage, arena and SP facts");}
            for(std::uint64_t maximum:{0ULL,1ULL,3ULL,4ULL,99ULL,100ULL,101ULL,1003ULL,static_cast<unsigned long long>(INT64_MAX)}){auto boundary=facts;boundary.maximumSp=maximum;boundary.currentSp=maximum;const auto cost=PrepareBuiltinUtility(*c,source,owner,boundary).value().spCost;Check(cost==maximum/100*30+maximum%100*30/100,"source SP rounding is exact across signed-pool boundaries");if(cost){boundary.currentSp=cost-1;Check(!PrepareBuiltinUtility(*c,source,owner,boundary),"one SP below exact cost never warps");boundary.currentSp=cost;Check(PrepareBuiltinUtility(*c,source,owner,boundary).has_value(),"exact SP cost is admitted");}}
        }
        if(p.handler==BuiltinQuestItem::Invisibility)Check(p.durationSeconds==300,"source invisibility duration is exactly 300 seconds");
        if(p.handler==BuiltinQuestItem::FireworkRandom)Check(p.firstFirework==1 && p.lastFirework==6,"random source fireworks cover all six original effects");
        if(p.handler==BuiltinQuestItem::Firework7)Check(p.firstFirework==7 && p.lastFirework==7,"source fixed seventh firework is not silently remapped");
        const auto commit=MakeBuiltinUtilityCommit(*c,op,fence,layouts,std::array{source},source.id,facts);
        if(p.consume)Check(commit && commit->items[0].after.count==1 && ValidateItemCommit(*c,*commit).has_value(),"utility uses the authoritative one-unit persistence writer");else Check(!commit,"teleport menu cannot consume its reusable ring");
        for(unsigned gate=0;gate<8;++gate) {auto denied=facts;auto subject=source;if(gate==0)denied.alive=false;if(gate==1)denied.mapIndex=0;if(gate==2)denied.use.admittedByServer=false;if(gate==3)denied.use.observer=true;if(gate==4)denied.use.characterLevel=49;if(gate==5)denied.use.stackUnits=1;if(gate==6)subject.owner.id++;if(gate==7)subject.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};Check(!PrepareBuiltinUtility(*c,subject,owner,denied),"utility source ownership, location and live actor admission cannot be bypassed");}
        if(p.handler==BuiltinQuestItem::CourageCombined){facts.mapIndex=1;Check(!PrepareBuiltinUtility(*c,source,owner,facts),"source courage is forbidden on map one");}
        if(p.handler==BuiltinQuestItem::ChinaFirework || p.handler==BuiltinQuestItem::SpinningTop){facts.use.context=QuestItemContext::Arena;Check(!PrepareBuiltinUtility(*c,source,owner,facts),"source regional firework is forbidden in arena");facts.use.context=QuestItemContext::World;facts.pvpPotionsBlocked=true;Check(!PrepareBuiltinUtility(*c,source,owner,facts),"regional firework observes the source PvP restriction");}
    }
}
void SnapshotAndHash()
{
    auto s=Fixture();const auto catalog=BuildItemCatalog(s).value();auto snapshot=catalog_snapshot_fixture::Snapshot(s);
    auto decoded=auriga::item::sql::DecodeSnapshot(snapshot,{123,catalog->ContentHash()}).value();
    Check(decoded->Find(Token()) && std::get<ItemQuestRow>(decoded->Find(Scroll())->category).groupCode=="quest.horse_training","SQL quest category and empty group token round trip");
    auto reversed=s;std::ranges::reverse(reversed.questItems);Check(BuildItemCatalog(reversed).value()->ContentHash()==catalog->ContentHash(),"quest row order normalizes");
    for(int change=0;change<4;++change)
    {
        auto altered=s;auto& q=altered.questItems[1];
        if(change==0)q.groupCode="quest.other";if(change==1)q.allowedContexts=1;if(change==2)q.allowObserver=true;if(change==3)q.dispatch=QuestItemDispatch::AllHandlers;
        Check(BuildItemCatalog(altered).value()->ContentHash()!=catalog->ContentHash(),"quest dispatch policy contributes to content hash");
    }
    using T=auriga::item::sql::Table;
    snapshot[T::QuestItems][0][3]=std::nullopt;Check(!auriga::item::sql::DecodeSnapshot(snapshot,{1,{}}),"NULL group is distinct from empty self binding");
}
}
int main(int argc,char** argv)
{
    if(argc==2 && std::string_view(argv[1])=="--hash-fixture")
    {
        const auto hash=BuildItemCatalog(Fixture()).value()->ContentHash();
        for(auto b:hash)std::cout<<"0123456789abcdef"[b>>4]<<"0123456789abcdef"[b&15];
        std::cout<<'\n';return 0;
    }
    try {Dispatch();Validate();BuiltinUtilities();SnapshotAndHash();std::cout<<"Quest item checks passed: "<<checks<<'\n';return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
