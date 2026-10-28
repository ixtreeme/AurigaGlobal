#include "ItemCatalogFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace
{
std::size_t checks=0;
void Check(bool ok,const char* why,std::source_location at=std::source_location::current())
{
    ++checks;if(!ok)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));
}
ItemCatalogSource Fixture()
{
    auto s=f::Catalog();
    // Real content may choose any max <=255. The recipe does not create +10.
    auto& p=s.refinements[1];p.maxLevel=30;
    for(std::uint16_t i=10;i<=30;++i)p.levels.push_back({i,{10000LL*i,4000LL*i,0,0},{}});
    s.refinementRecipes.push_back({1,"refine.iron_sword_9_10",f::Sword(),9,f::Sword(),10,5000,7500,
                                  RefinementFailure::Keep,{},{{f::Leather(),0,7},{f::SocketJewel(),0,2}}});
    return s;
}
RefinementUseContext Context() {RefinementUseContext c;c.operationId[0]=1;c.actorId=77;c.walletRevision=8;c.gold=9000;c.allowedByRules=true;return c;}
RefinementItemSnapshot Source() {return {100,5,77,f::Sword(),9,1,false};}
std::vector<RefinementItemSnapshot> Inventory()
{
    return {{103,1,77,f::Leather(),0,4,false},{101,2,77,f::Leather(),0,5,false},
            {105,3,77,f::SocketJewel(),0,2,false},Source()};
}
void PrepareAndBoundaries()
{
    auto s=Fixture();const auto catalog=BuildItemCatalog(s).value();
    auto inventory=Inventory();auto context=Context();
    const auto beforeSource=Source();const auto beforeGold=context.gold;
    auto p=PrepareRefinement(*catalog,1,context,Source(),inventory,7500).value();
    Check(p.success && p.result==f::Sword() && p.resultLevel==10,"+9 to +10 stays one ItemKey and one instance");
    Check(p.sourceInstanceId==100 && p.expectedSourceRevision==5,"source receipt binds instance and revision");
    Check(p.goldCost==5000 && p.expectedGold==9000 && p.expectedWalletRevision==8,"wallet checks and full cost copied");
    Check(p.catalogRevision==catalog->Revision() && p.catalogHash==catalog->ContentHash(),"proposal binds immutable content");
    Check(p.operationId==context.operationId && p.recordedRoll==7500,"server decision and operation identity recorded");
    Check(p.debits.size()==3 && p.debits[0].instanceId==101 && p.debits[0].consumeCount==5 &&
          p.debits[1].instanceId==103 && p.debits[1].consumeCount==2 && p.debits[2].instanceId==105,"stable partial-stack debits");
    Check(p.debits[0].expectedCount==5 && p.debits[0].expectedRevision==2,"material revision and before count guarded");
    Check(context.gold==beforeGold && Source().instanceId==beforeSource.instanceId,"preparation never mutates state");
    for(unsigned i=0;i<100;++i)
    {
        std::ranges::reverse(inventory);
        const auto q=PrepareRefinement(*catalog,1,context,Source(),inventory,7500).value();
        Check(q.debits.size()==p.debits.size() && q.debits[0].instanceId==101 && q.debits[1].consumeCount==2,"input order cannot change stack selection");
    }
    const auto failure=PrepareRefinement(*catalog,1,context,Source(),inventory,7501).value();
    Check(!failure.success && failure.resultLevel==9 && failure.result==f::Sword() && failure.goldCost==5000 && failure.debits.size()==3,
          "failed attempt still charges exactly the specified resources");
    for(auto chance:{std::uint16_t{0},std::uint16_t{10000}})
    {
        s.refinementRecipes[0].successChanceBp=chance;auto c=BuildItemCatalog(s).value();
        Check(PrepareRefinement(*c,1,context,Source(),inventory,0)->success==(chance==10000),"guaranteed decisions need no roll");
        Check(PrepareRefinement(*c,1,context,Source(),inventory,1).error()==RefinementUseError::InvalidDecision,"unneeded RNG not accepted");
    }
    for(auto roll:{std::uint16_t{0},std::uint16_t{10001}})
        Check(PrepareRefinement(*catalog,1,context,Source(),inventory,roll).error()==RefinementUseError::InvalidDecision,"bad decision rejected");
    s=Fixture();s.refinementRecipes[0].failure=RefinementFailure::Destroy;
    auto c=BuildItemCatalog(s).value();auto destroyed=PrepareRefinement(*c,1,context,Source(),inventory,9000).value();
    Check(!destroyed.result && !destroyed.resultLevel && !destroyed.success,"destroy failure is explicit");
    s.refinementRecipes[0].failure=RefinementFailure::SetLevel;s.refinementRecipes[0].failureLevel=8;
    c=BuildItemCatalog(s).value();Check(PrepareRefinement(*c,1,context,Source(),inventory,9000)->resultLevel==8,"downgrade preserves source identity");
    s.refinementRecipes[0].target=f::Armor();s.refinementRecipes[0].targetLevel=2;
    c=BuildItemCatalog(s).value();auto transformed=PrepareRefinement(*c,1,context,Source(),inventory,10).value();
    Check(transformed.result==f::Armor() && transformed.resultLevel==2 && transformed.sourceInstanceId==100,"explicit transformation can change prototype");
}
void RejectRecipes()
{
    auto s=Fixture();auto valid=BuildItemCatalog(s).value();
    const auto reject=[&](auto mutate,RefinementRecipeError expected){auto r=s.refinementRecipes[0];mutate(r);
        auto result=ValidateRefinementRecipe(r,*valid);Check(!result && result.error()==expected,"bad recipe rejected");
        auto data=s;data.refinementRecipes[0]=r;Check(!BuildItemCatalog(data),"builder rejects invalid recipe before publication");};
    reject([](auto& r){r.id=0;},RefinementRecipeError::InvalidIdentity);
    reject([](auto& r){r.code="bad..code";},RefinementRecipeError::InvalidIdentity);
    reject([](auto& r){r.source=ItemKey::Create(1,999).value();},RefinementRecipeError::MissingItem);
    reject([](auto& r){r.target=f::Leather();},RefinementRecipeError::InvalidTransition);
    reject([](auto& r){r.sourceLevel=256;},RefinementRecipeError::InvalidLevel);
    reject([](auto& r){r.targetLevel=31;},RefinementRecipeError::InvalidLevel);
    reject([](auto& r){r.targetLevel=11;},RefinementRecipeError::InvalidTransition);
    reject([](auto& r){r.targetLevel=9;},RefinementRecipeError::InvalidTransition);
    reject([](auto& r){r.goldCost=std::uint64_t{1}<<63;},RefinementRecipeError::InvalidCost);
    reject([](auto& r){r.successChanceBp=10001;},RefinementRecipeError::InvalidChance);
    reject([](auto& r){r.failure=static_cast<RefinementFailure>(0);},RefinementRecipeError::InvalidFailure);
    reject([](auto& r){r.failureLevel=0;},RefinementRecipeError::InvalidFailure);
    reject([](auto& r){r.failure=RefinementFailure::SetLevel;},RefinementRecipeError::InvalidFailure);
    reject([](auto& r){r.failure=RefinementFailure::SetLevel;r.failureLevel=9;},RefinementRecipeError::InvalidFailure);
    reject([](auto& r){r.ingredients[0].count=0;},RefinementRecipeError::InvalidIngredient);
    reject([](auto& r){r.ingredients[0].refinementLevel=1;},RefinementRecipeError::InvalidIngredient);
    reject([](auto& r){r.ingredients.push_back(r.ingredients.front());},RefinementRecipeError::DuplicateIngredient);
    reject([](auto& r){while(r.ingredients.size()<=MaxRefinementIngredients)r.ingredients.push_back(r.ingredients.front());},RefinementRecipeError::TooManyIngredients);
    auto retired=s;retired.items[0].retired=true;retired.refinementRecipes.clear();
    // The loot fixture also references the sword, so remove loot item rewards.
    for(auto& loot:retired.lootProfiles)std::erase_if(loot.entries,[](const auto& e){return e.item==f::Sword();});
    const auto retiredCatalog=BuildItemCatalog(retired).value();
    Check(ValidateRefinementRecipe(s.refinementRecipes[0],*retiredCatalog).error()==RefinementRecipeError::RetiredItem,"retired source cannot refine");
}
void RejectUses()
{
    const auto catalog=BuildItemCatalog(Fixture()).value();
    const auto check=[&](auto mutate,RefinementUseError expected){auto c=Context();auto src=Source();auto inv=Inventory();mutate(c,src,inv);
        auto p=PrepareRefinement(*catalog,1,c,src,inv,5000);Check(!p && p.error()==expected,"invalid use did not fail closed");};
    check([](auto& c,auto&,auto&){c.allowedByRules=false;},RefinementUseError::Forbidden);
    check([](auto& c,auto&,auto&){c.operationId={};},RefinementUseError::InvalidContext);
    check([](auto& c,auto&,auto&){c.actorId=0;},RefinementUseError::InvalidContext);
    check([](auto& c,auto&,auto&){c.walletRevision=0;},RefinementUseError::InvalidContext);
    check([](auto& c,auto&,auto&){c.gold=4999;},RefinementUseError::InsufficientGold);
    check([](auto&,auto& s,auto&){s.locked=true;},RefinementUseError::InvalidSource);
    check([](auto&,auto& s,auto&){s.ownerId=78;},RefinementUseError::InvalidSource);
    check([](auto&,auto& s,auto&){s.count=2;},RefinementUseError::InvalidSource);
    check([](auto&,auto& s,auto&){s.refinementLevel=8;},RefinementUseError::InvalidSource);
    check([](auto&,auto&,auto& i){i.push_back(i.front());},RefinementUseError::InvalidInventory);
    check([](auto&,auto&,auto& i){i.front().ownerId=78;},RefinementUseError::InvalidInventory);
    check([](auto&,auto&,auto& i){i.front().count=201;},RefinementUseError::InvalidInventory);
    check([](auto&,auto&,auto& i){i.front().revision=0;},RefinementUseError::InvalidInventory);
    check([](auto&,auto&,auto& i){i.back().revision=6;},RefinementUseError::InvalidInventory);
    check([](auto&,auto&,auto& i){i.front().locked=true;},RefinementUseError::InsufficientMaterials);
    check([](auto&,auto&,auto& i){i.clear();},RefinementUseError::InsufficientMaterials);
    Check(PrepareRefinement(*catalog,900,Context(),Source(),Inventory(),5000).error()==RefinementUseError::MissingRecipe,"missing recipe is never free");
    auto s=Fixture();s.refinementRecipes[0].ingredients={{f::Sword(),9,1}};auto c=BuildItemCatalog(s).value();
    const auto sourceOnly=std::vector{Source()};
    Check(PrepareRefinement(*c,1,Context(),Source(),sourceOnly,5000).error()==RefinementUseError::InsufficientMaterials,"source cannot pay its own ingredient cost");
}
void HashAndSnapshot()
{
    auto source=Fixture();const auto original=BuildItemCatalog(source).value();
    auto snapshot=catalog_snapshot_fixture::Snapshot(source);
    auto decoded=auriga::item::sql::DecodeSnapshot(snapshot,{88,original->ContentHash()}).value();
    Check(decoded->FindRefinementRecipe(1)->ingredients.size()==2,"recipe and ingredient SQL rows load");
    auto reverse=source;std::ranges::reverse(reverse.refinementRecipes[0].ingredients);
    Check(BuildItemCatalog(reverse).value()->ContentHash()==original->ContentHash(),"recipe ingredient order normalizes");
    for(auto mutate:{0,1,2,3})
    {
        auto s=source;auto& r=s.refinementRecipes[0];
        if(mutate==0)r.goldCost++;if(mutate==1)r.successChanceBp++;if(mutate==2)r.ingredients[0].count++;if(mutate==3)r.failure=RefinementFailure::Destroy;
        Check(BuildItemCatalog(s).value()->ContentHash()!=original->ContentHash(),"economic recipe fields participate in release hash");
    }
    snapshot[auriga::item::sql::Table::RefinementIngredients][0][0]="999";
    Check(!auriga::item::sql::DecodeSnapshot(snapshot,{1,{}}),"orphan ingredient rejected");
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
    try {PrepareAndBoundaries();RejectRecipes();RejectUses();HashAndSnapshot();std::cout<<"Refinement recipe checks passed: "<<checks<<'\n';return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
