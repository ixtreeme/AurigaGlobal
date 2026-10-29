#include "ItemCatalogFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <item/SoulItems.hpp>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace db=auriga::item::sql;
namespace
{
unsigned checks=0;
void Check(bool v,const char* m){++checks;if(!v)throw std::runtime_error(m);}
ItemKey Key(unsigned n){return ItemKey::Create(14,n).value();}
ItemCatalogSource Fixture()
{
    auto s=f::Catalog();RefinementProfile refinement{5,"refine.soul",RefinementKind::Soul,30,{}};
    SoulProfile profile{1,"soul.standard",{}};
    for(std::uint16_t i=0;i<=30;++i){refinement.levels.push_back({i,{},{}});profile.levels.push_back({i,180,60,500,0,500,60000,15000,18000,25000});}
    s.refinements.push_back(refinement);s.soulProfiles.push_back(profile);
    for(unsigned i=1;i<=3;++i)
    {auto base=s.items[3];base.key=Key(i);base.contentCode="soul.item_"+std::to_string(i);base.nameKey="item.soul_"+std::to_string(i)+".name";base.descriptionKey="item.soul_"+std::to_string(i)+".description";
     base.maxStack=i==3?200:1;base.usePolicy=1;base.refinementProfile=i==3?1:5;s.items.push_back(base);}
    s.soulItems={{Key(1),SoulKind::Core,SoulChannel::NormalAttack,1,0},{Key(2),SoulKind::Core,SoulChannel::Skill,1,0},{Key(3),SoulKind::RefinementScroll,{},{},7500}};
    return s;
}
SoulRequest Request()
{
    SoulRequest r{Key(1)};r.state={180,500,0,true};r.instanceId=10;r.revision=5;r.actorId=r.ownerId=7;r.rulesetRevision=2;r.operationId[0]=1;
    r.count=1;r.characterLevel=50;r.characterClass=r.sex=1;r.action=SoulAction::Strike;r.damageChannel=SoulChannel::NormalAttack;
    r.inputDamageRaw=10001;r.damageEventId=20;r.targetId=100;r.targetRevision=5;r.admittedByServer=r.actorIsPlayer=r.targetIsNpc=true;return r;
}
void Actions()
{
    auto source=Fixture();auto c=BuildItemCatalog(source).value();auto r=Request();
    Check(MakeSoulState(*c,Key(1),30).value()==SoulState{0,500,0,false},"typed factory initializes charge and strikes");
    Check(MakeSoulState(*c,Key(3),0).error()==SoulError::NotCore,"scroll has no core state");
    Check(MakeSoulState(*c,Key(1),31).error()==SoulError::InvalidLevel,"factory checks configured maximum");
    auto p=PlanSoul(*c,r).value();Check(p.outputDamageRaw==25002 && p.additionalDamageRaw==15001 && p.after.strikes==499,"exact integer tier-3 damage and one strike");
    for(auto [charge,multiplier]:{std::pair{60u,15000u},std::pair{119u,15000u},std::pair{120u,18000u},std::pair{179u,18000u},std::pair{180u,25000u}})
    {r.state.charge=charge;Check(PlanSoul(*c,r)->multiplierBp==multiplier,"explicit charge tier boundaries");}
    r=Request();r.state={60,1,0,true};p=PlanSoul(*c,r).value();Check(p.after==SoulState{0,500,0,false},"last cycle strike deactivates and retains item");
    r.state={120,1,1200,true};p=PlanSoul(*c,r).value();Check(p.after==SoulState{60,500,1200,true},"charge cycle keeps partial clock and remaining active tier");
    r=Request();r.key=Key(2);r.damageChannel=SoulChannel::Skill;Check(PlanSoul(*c,r)->success,"blue soul serves skill channel");
    r.damageChannel=SoulChannel::NormalAttack;Check(PlanSoul(*c,r).error()==SoulError::IneligibleTarget,"blue cannot debit on normal hit");
    r=Request();r.action=SoulAction::Recharge;r.inputDamageRaw=0;r.state={0,500,59999,false};r.elapsedOwnerMs=1;
    p=PlanSoul(*c,r).value();Check(p.after==SoulState{1,500,0,false},"one owner-clock millisecond completes partial recharge");
    r.state={179,1,0,true};r.elapsedOwnerMs=UINT64_C(9223372036854775807);p=PlanSoul(*c,r).value();Check(p.after==SoulState{180,1,0,true},"large elapsed recharge caps without strike refill");
    r.state={180,500,0,false};Check(PlanSoul(*c,r)->after.rechargeRemainderMs==0,"full core has no accumulating clock");
    r.elapsedOwnerMs=UINT64_MAX;Check(PlanSoul(*c,r).error()==SoulError::InvalidOperation,"invalid elapsed interval rejected");
    r=Request();r.inputDamageRaw=0;r.action=SoulAction::Activate;r.state.active=false;
    Check(PlanSoul(*c,r)->after.active,"activation locks owning channel without consume");
    r.state.charge=59;Check(PlanSoul(*c,r).error()==SoulError::NotReady,"cannot activate below one charge cycle");
    r.state.charge=60;r.channelOccupiedByOther=true;Check(PlanSoul(*c,r).error()==SoulError::ChannelOccupied,"one active core per channel");
    r=Request();r.action=SoulAction::Deactivate;r.inputDamageRaw=0;r.characterLevel=0;Check(!PlanSoul(*c,r)->after.active,"cleanup independent of changed use policy");
    source.items[source.items.size()-3].retired=true;auto retired=BuildItemCatalog(source).value();Check(!PlanSoul(*retired,r)->after.active,"retired core can deactivate");
    r=Request();r.inputDamageRaw=INT64_MAX;Check(PlanSoul(*c,r).error()==SoulError::DamageOverflow,"damage multiplication cannot overflow");
    r.inputDamageRaw=0;Check(PlanSoul(*c,r).error()==SoulError::InvalidDamage,"zero damage never consumes strike");
    r=Request();r.action=SoulAction::Refine;r.inputDamageRaw=0;r.state.active=false;r.refinementLevel=10;r.recordedRoll=7500;r.scroll=SoulScrollSnapshot{Key(3),20,8,7,10,false};
    p=PlanSoul(*c,r).value();Check(p.success && p.afterLevel==11 && p.consumeScrollUnits==1 && p.after==SoulState{0,500,0,false},"same prototype +10 to +11 resets explicit initial state");
    r.recordedRoll=7501;p=PlanSoul(*c,r).value();Check(!p.success && p.afterLevel==10 && p.after==r.state && p.consumeScrollUnits==1,"failed scroll preserves entire core state");
    r.refinementLevel=30;Check(PlanSoul(*c,r).error()==SoulError::AtMaximum,"level maximum controlled by refinement profile");
    r.refinementLevel=10;r.scroll->instanceId=10;Check(PlanSoul(*c,r).error()==SoulError::InvalidScroll,"core cannot consume itself");
    r.scroll->instanceId=20;r.scroll->ownerId=9;Check(PlanSoul(*c,r).error()==SoulError::InvalidScroll,"foreign scroll rejected");
    for(auto chance:{0u,10000u}){auto s=Fixture();s.soulItems[2].refineChanceBp=static_cast<std::uint16_t>(chance);auto fixed=BuildItemCatalog(s).value();r.scroll->ownerId=7;r.recordedRoll=0;Check(PlanSoul(*fixed,r)->success==(chance==10000),"guaranteed scroll does not draw RNG");}
    c.reset();source={};Check(p.expected.state.strikes==500 && p.catalogRevision==1,"plan owns expected state after catalog destruction");
}
void Rejections()
{
    auto s=Fixture();const auto c=BuildItemCatalog(s).value();
    for(unsigned i=0;i<21;++i){auto r=Request();switch(i){
        case 0:r.admittedByServer=false;break;case 1:r.ownerId=8;break;case 2:r.revision=UINT64_MAX;break;
        case 3:r.operationId={};break;case 4:r.count=2;break;case 5:r.lockedByOther=true;break;case 6:r.exchanging=true;break;
        case 7:r.equipped=true;break;case 8:r.state.charge=181;break;case 9:r.state.strikes=0;break;
        case 10:r.targetIsNpc=false;break;case 11:r.polymorphed=true;break;case 12:r.actorIsPlayer=false;break;
        case 13:r.elapsedOwnerMs=1;break;case 14:r.state.rechargeRemainderMs=1;break;case 15:r.recordedRoll=1;break;
        case 16:r.damageEventId=0;break;case 17:r.targetId=0;break;case 18:r.targetRevision=0;break;case 19:r.targetRevision=UINT64_MAX;break;case 20:r.channelOccupiedByOther=true;break;}
        Check(!PlanSoul(*c,r),"invalid input never debits or changes damage");}
    auto bad=[&](auto change){auto x=s;change(x);Check(!BuildItemCatalog(x),"invalid soul cannot publish");};
    bad([](auto& x){x.soulProfiles[0].levels.pop_back();});bad([](auto& x){x.soulProfiles[0].levels[0].cycleChargeCost=0;});
    bad([](auto& x){x.soulProfiles[0].levels[0].strikesPerCycle=0;});bad([](auto& x){x.soulProfiles[0].levels[0].initialStrikes=501;});
    bad([](auto& x){x.soulProfiles[0].levels[0].initialCharge=181;});bad([](auto& x){x.soulProfiles[0].levels[0].chargeCapacity=61;});
    bad([](auto& x){x.soulProfiles[0].levels[0].rechargeIntervalMs=0;});bad([](auto& x){x.soulProfiles[0].levels[0].thirdMultiplierBp=9999;});
    bad([](auto& x){x.soulItems[0].channel.reset();});bad([](auto& x){x.soulItems[2].profile=1;});
    bad([](auto& x){x.items[x.items.size()-3].maxStack=2;});bad([](auto& x){x.refinements.back().levels[1].extraBonusProfile=2;});
    auto snapshot=catalog_snapshot_fixture::Snapshot(s);Check(db::DecodeSnapshot(snapshot,{1,c->ContentHash()}).has_value(),"snapshot roundtrip preserves typed soul rules");
    std::ranges::reverse(s.soulProfiles[0].levels);Check(BuildItemCatalog(s).value()->ContentHash()==c->ContentHash(),"levels canonically sorted");
    s=Fixture();s.soulProfiles[0].levels[11].initialStrikes=499;Check(BuildItemCatalog(s).value()->ContentHash()!=c->ContentHash(),"initial state is covered by digest");
    Check(!ValidateRefinementRecipe({1,"soul.bypass",Key(1),10,Key(1),11,0,10000,RefinementFailure::Keep,{},{}},*c),"ordinary recipe cannot lose Soul state");
}
}
int main(int argc,char** argv)
{
    try{if(argc==2 && std::string_view(argv[1])=="--hash-fixture"){const auto c=BuildItemCatalog(Fixture()).value();for(auto b:c->ContentHash())std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b);std::cout<<'\n';return 0;}
        Actions();Rejections();std::cout<<checks<<" Soul checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
