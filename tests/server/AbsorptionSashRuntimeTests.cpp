#include "AbsorptionSashFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <item/EquipmentStats.hpp>
#include <item/TypedInventoryWire.hpp>
#include <item/AbsorptionSashWire.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;namespace f=absorption_sash_fixture;namespace pf=persistent_item_fixture;
namespace {
unsigned checks=0;
void Check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
struct Fixture {
    std::shared_ptr<const ItemCatalog> c=BuildItemCatalog(f::Catalog()).value();
    ItemOwnerFence fence{pf::Character(),7,1};std::array<ItemStorageLayout,1> layouts{pf::Inventory()};
    std::vector<PersistentItem> items;std::vector<ItemWallet> wallets{{pf::Character(),WalletCurrency::Gold,1,30000000}};unsigned serial=0;std::uint64_t next=60000;
    std::array<std::uint8_t,16> Op(){return {0xAC,static_cast<std::uint8_t>(++serial)};}
    ItemInstanceId Grant(ItemKey key,unsigned count=1){const auto id=ItemInstanceId::Create(++next).value();const auto r=MakeGrantCommit(*c,Op(),fence,layouts,items,{key,0,count},std::array{id}).value();Check(r.items.size()==1,"fixture grants one actual owned source");items.push_back(r.items[0].after);return id;}
    PersistentItem& Find(ItemInstanceId id){return *std::ranges::find(items,id,&PersistentItem::id);}
    std::expected<AbsorptionSashWrite,ItemPersistenceError> Use(ItemInstanceId a,ItemInstanceId b,AbsorptionSashOperation mode,AbsorptionSashDecision d={}){return MakeAbsorptionSashCommit(*c,Op(),fence,layouts,items,wallets,a,b,mode,f::Actor(),d);}
};
void Combine(){
    Fixture t;const auto a=t.Grant(f::Sash(1)),b=t.Grant(f::Sash(1,1)),fresh=ItemInstanceId::Create(70000).value();
    const auto quote=PrepareAbsorptionSash(*t.c,t.fence.owner,t.items,t.wallets,a,b,AbsorptionSashOperation::Combine,f::Actor()).value();Check(quote.goldCost==1000000 && quote.successBp==10000 && quote.minimumPercent==5 && quote.maximumPercent==5 && quote.resultKey==f::Sash(2),"cross-family combination preserves first-family next grade and source price");
    const auto write=t.Use(a,b,AbsorptionSashOperation::Combine,{1,5,fresh}).value();Check(write.success && write.commit.items.size()==3 && write.commit.wallets[0].after.balance==29000000,"successful combine debits both inputs and exact gold atomically");
    const auto result=std::ranges::find(write.commit.items,fresh,[](const auto& x){return x.after.id;});Check(result!=write.commit.items.end() && !result->before && result->after.key==f::Sash(2) && result->after.placement==t.Find(a).placement && result->after.sash->percent==5,"combination creates a new native instance in the first source cell");
    for(unsigned forge=0;forge<5;++forge){auto r=write.commit;if(forge==0)r.wallets[0].after.balance++;if(forge==1)r.items.back().after.sash->percent=6;if(forge==2)r.items[0].after=t.Find(b);if(forge==3)r.result[31]=6;if(forge==4)r.items.back().after.boundTo=t.fence.owner;Check(!ValidateItemCommit(*t.c,r),"forged sash price, percentage, debit or binding rejects");}
    Fixture failed;const auto x=failed.Grant(f::Sash(2)),y=failed.Grant(f::Sash(2));const auto loss=failed.Use(x,y,AbsorptionSashOperation::Combine,{8001,0,{}}).value();Check(!loss.success && loss.commit.items.size()==2 && loss.commit.wallets[0].after.balance==27000000,"original grade-two failure consumes second sash and price");const auto kept=std::ranges::find(loss.commit.items,x,[](const auto& p){return p.after.id;});auto expected=failed.Find(x);++expected.revision;Check(kept->after==expected,"failure retains first sash state and binds its expected revision");
    Check(!failed.Use(x,y,AbsorptionSashOperation::Combine,{8001,5,fresh}),"failed draw cannot smuggle a result");
    Fixture third;const auto g=third.Grant(f::Sash(3)),h=third.Grant(f::Sash(3));Check(!third.Use(g,h,AbsorptionSashOperation::Combine,{1,11,fresh}) && third.Use(g,h,AbsorptionSashOperation::Combine,{1,12,fresh}).has_value() && third.Use(g,h,AbsorptionSashOperation::Combine,{1,19,fresh}).has_value(),"source third-to-fourth draw is exactly 12..19");
    Fixture last;const auto m=last.Grant(f::Sash(4)),n=last.Grant(f::Sash(4));last.Find(m).sash->percent=24;const auto capped=last.Use(m,n,AbsorptionSashOperation::Combine,{1,25,fresh}).value();Check(capped.quote.minimumPercent==25 && capped.quote.maximumPercent==25,"last-grade improvement cannot exceed 25 percent");last.Find(m).sash->percent=25;Check(!last.Use(m,n,AbsorptionSashOperation::Combine,{1,25,fresh}),"capped sash refuses without consumption");
    Fixture poor;const auto pa=poor.Grant(f::Sash(1)),pb=poor.Grant(f::Sash(1));poor.wallets[0].balance=999999;Check(!poor.Use(pa,pb,AbsorptionSashOperation::Combine,{1,5,fresh}),"insufficient gold refuses before a result is admitted");
}
void AbsorbAndClear(){
    Fixture t;const auto sash=t.Grant(f::Sash(4)),weapon=t.Grant(item_catalog_fixture::Sword()),tool=t.Grant(f::Clear(),2);
    t.Find(weapon).attributes=attribute_fixture::PayloadFixture();t.Find(weapon).boundTo=t.fence.owner;
    const auto attempt=t.Use(sash,weapon,AbsorptionSashOperation::Absorb);if(!attempt)throw std::runtime_error("Absorb failure "+std::to_string(static_cast<unsigned>(attempt.error())));const auto write=*attempt;Check(write.commit.wallets.empty() && write.commit.items.size()==2,"absorption has no invented gold fee");const auto changed=std::ranges::find(write.commit.items,sash,[](const auto& p){return p.after.id;});const auto donor=changed->after.sash->donor;
    Check(donor && donor->key==item_catalog_fixture::Sword() && donor->refinement==0 && !donor->attributes[0] && donor->attributes[2]==t.Find(weapon).attributes.slots[2] && donor->attributes[5]==t.Find(weapon).attributes.slots[5],"absorption copies original positive slots and clamps negative contributions to zero");Check(changed->after.boundTo==t.fence.owner,"bound donor cannot be laundered into a tradable sash");
    auto fake=write.commit;fake.items.back().after.sash->donor->attributes[2]->valueScaled++;Check(!ValidateItemCommit(*t.c,fake),"forged copied donor bonus rejects");
    t.Find(sash)=changed->after;t.items.erase(std::ranges::find(t.items,weapon,&PersistentItem::id));const auto cleared=t.Use(tool,sash,AbsorptionSashOperation::Clear).value();const auto clearResult=std::ranges::find(cleared.commit.items,sash,[](const auto& p){return p.after.id;});Check(!clearResult->after.sash->donor && clearResult->after.sash->percent==changed->after.sash->percent && clearResult->after.boundTo==t.fence.owner,"clear removes donor while retaining percentage and owner binding");Check(cleared.commit.items[0].after.count==1,"clear consumes one real tool unit");
    t.Find(sash)=clearResult->after;Check(!t.Use(tool,sash,AbsorptionSashOperation::Clear),"already clear sash cannot waste the tool");
    Fixture invalid;const auto a=invalid.Grant(f::Sash(1)),material=invalid.Grant(item_catalog_fixture::Leather());Check(!invalid.Use(a,material,AbsorptionSashOperation::Absorb),"non-equipment donor rejected");
    auto actor=f::Actor();actor.alive=false;Check(!PrepareAbsorptionSash(*t.c,t.fence.owner,t.items,t.wallets,sash,tool,AbsorptionSashOperation::Absorb,actor),"dead actor cannot invoke sash services");
}
void Wire(){
    Fixture t;const auto a=t.Grant(f::Sash(4)),b=t.Grant(item_catalog_fixture::Sword());t.Find(a).sash->donor=AbsorbedSashDonor{t.Find(b).key,0};
    const auto bytes=EncodeTypedInventory(*t.c,t.layouts,t.wallets,t.items).value();const auto view=DecodeTypedInventory(bytes,t.fence.owner);
    Check(std::to_integer<unsigned>(bytes[5])==26 && view && view->items[0].sash==t.Find(a).sash && view->sashGrades.at(a)==4 && view->sashContributions.at(a).size()==4,"GC225 v26 carries the exact donor state and original separately rounded contributions");
    for(unsigned n=0;n<bytes.size();++n)Check(!DecodeTypedInventory(std::span(bytes).first(n),t.fence.owner),"truncated sash packet rejects as a whole");
    const auto q=PrepareAbsorptionSash(*t.c,t.fence.owner,t.items,t.wallets,a,b,AbsorptionSashOperation::Clear,f::Actor());Check(!q,"sash cannot impersonate a clearing tool");
    const auto denied=EncodeAbsorptionSashReply(*t.c,2,0,a.Value(),b.Value(),2);Check(denied.size()==36 && denied[5]==2 && denied[35]==0,"GC220 v2 denied reply retains complete identity without a fake quote");
}
void Catalog(){auto source=f::Catalog();const auto c=BuildItemCatalog(source).value();const auto snapshot=catalog_snapshot_fixture::Snapshot(source);Check(sql::DecodeSnapshot(snapshot,{source.revision,c->ContentHash()}).has_value(),"sash catalog and donor components survive native SQL snapshot codec");std::ranges::reverse(source.absorptionSashes);std::ranges::reverse(source.sashDonors[0].components);Check(BuildItemCatalog(source).value()->ContentHash()==c->ContentHash(),"sash canonical hash normalizes profile and component ordering");source.absorptionSashes.pop_back();Check(!BuildItemCatalog(source),"missing sash profile cannot leave a usable orphan regalia");}
}
int main(){try{Catalog();Combine();AbsorbAndClear();Wire();std::cout<<"Absorption sash runtime checks passed: "<<checks<<'\n';return 0;}catch(const std::exception& e){std::cerr<<"Check "<<checks<<": "<<e.what()<<'\n';return 1;}}
