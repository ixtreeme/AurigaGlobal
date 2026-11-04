#include "PolymorphRuntimeFixture.hpp"
#include <item/EffectRuntime.hpp>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;namespace f=polymorph_runtime_fixture;namespace pf=persistent_item_fixture;
unsigned checks=0;void Check(bool b,const char* message){++checks;if(!b)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
 auto c=BuildItemCatalog(f::Catalog()).value();std::vector<PersistentItem> items;for(unsigned n=0;n<4;++n)items.push_back(f::Item(n));std::array layouts{pf::Inventory()};ItemOwnerFence fence{pf::Character(),7,1};RecoverySchedule state;auto a=f::Facts();
 auto use=[&](unsigned n,std::uint64_t now=1000,std::uint16_t roll=0){return MakePolymorphRuntimeCommit(*c,{24,1},fence,layouts,items,state,items[n].id,a,0,now,roll);};
 for(const auto& i:items)Check(ValidatePersistentItem(*c,i).has_value(),"polymorph payload validated");auto bad=items[0];bad.polymorph.reset();Check(!ValidatePersistentItem(*c,bad),"missing payload refused");bad=items[0];bad.polymorph->boundForm=24001;Check(!ValidatePersistentItem(*c,bad),"fixed form cannot carry alternative binding");bad=items[1];bad.polymorph->boundForm=999;Check(!ValidatePersistentItem(*c,bad),"unknown bound form refused");
 auto grant=MakeGrantCommit(*c,{24,0,1},fence,layouts,{},ItemGrant{f::Key(0),0,3},std::array{ItemInstanceId::Create(9000).value()});Check(grant && grant->items[0].after.polymorph==PolymorphInstanceState{},"fixed orb grant initializes payload");Check(!MakeGrantCommit(*c,{24,0,2},fence,layouts,{},ItemGrant{f::Key(1),0,3},std::array{ItemInstanceId::Create(9001).value()}),"bound orb needs explicit payload");
 std::deque<ItemGrant> queue;Check(QueueGrant(queue,{f::Key(1),0,1,{},PolymorphInstanceState{24001,0,0}},20) && QueueGrant(queue,{f::Key(1),0,1,{},PolymorphInstanceState{24002,0,0}},20) && queue.size()==2,"different bound forms do not merge");
 auto first=use(0);Check(first && first->commit.items[0].after.count==4 && first->commit.recovery->after.polymorph->effect.expiresAtUtcMs==3000,"orb debit and UTC effect together");state=first->commit.recovery->after;Check(use(0).error()==PolymorphError::Forbidden,"already transformed blocks another orb");
 auto bytes=EncodeRecoverySchedule(*c,state).value();Check(std::string(bytes.begin(),bytes.begin()+4)=="RCS3" && DecodeRecoverySchedule(*c,bytes).value()==state,"complete RCS3 state roundtrip");for(std::size_t n=0;n<bytes.size();++n)Check(!DecodeRecoverySchedule(*c,std::span(bytes).first(n)),"RCS3 truncation");bytes.push_back(0);Check(!DecodeRecoverySchedule(*c,bytes),"RCS3 trailing byte");
 auto cancel=MakePolymorphCancel(*c,{24,2},fence,state,0,1500,state.generation);Check(cancel && !cancel->recovery->after.polymorph && cancel->recovery->after.generation==state.generation+1,"explicit cancel fences active generation");Check(!MakePolymorphCancel(*c,{24,3},fence,state,0,1500,state.generation+1),"stale cancel rejected");
 auto expired=state;NormalizeTimedItems(*c,expired,3000);Check(!expired.polymorph,"exact UTC deadline expires");auto dead=state;ItemEffectFacts facts;facts.alive=false;facts.allowed=true;NormalizeTimedItems(*c,dead,1500,&facts);Check(!dead.polymorph,"death removes morph");auto cleared=state;facts.alive=true;facts.polymorphAllowed=false;NormalizeTimedItems(*c,cleared,1500,&facts);Check(!cleared.polymorph,"legacy clear request projected before persistence");
 state={};auto failed=use(3);Check(failed && !failed->result.success && !failed->commit.recovery->after.polymorph && failed->commit.items[0].after.polymorph==items[3].polymorph,"failed practice keeps exact payload and no effect");auto book=use(2);Check(book && book->result.success && book->commit.items[0].after.polymorph->practiceRemaining==1 && book->result.effect->bonusMagnitude==21,"success decrements one practice and computes additive movement points");state=book->commit.recovery->after;auto bonusExpired=state;NormalizeTimedItems(*c,bonusExpired,4000);Check(bonusExpired.polymorph && bonusExpired.polymorph->effect.bonus==PolymorphBonus::None,"bonus expires before the shape without ending transformation");
 state={};a.action=PolymorphAction::UpgradeBook;Check(use(2).error()==PolymorphError::NotReady,"upgrade requires complete practice");items[2].polymorph->practiceRemaining=0;auto upgraded=use(2);Check(upgraded && upgraded->result.after.bookGrade==2 && upgraded->result.after.practiceRemaining==2 && !upgraded->commit.recovery->after.polymorph,"upgrade changes book only and starts exact practice");a.action=PolymorphAction::Use;
 auto quote=QuotePolymorphRuntime(*c,items[1],state,a,1000);Check(quote && quote->effect->formId==24001,"bound form resolved from trusted instance");auto actor=a;actor.mobTemplate=102;Check(!QuotePolymorphRuntime(*c,items[1],state,actor,1000),"mob snapshot cannot replace bound form");
 const auto packet=EncodePolymorphRuntime(*c,{1,255,1,0,0,123,456,1000,&items[1],&state,&a,"Wolf",""});Check(packet.has_value(),"native polymorph quote golden");const auto inventory=EncodeTypedInventory(*c,layouts,{},items).value();Check(std::to_integer<unsigned>(inventory[5])==5 && DecodeTypedInventory(inventory,pf::Character())->items[1].polymorph==items[1].polymorph,"inventory v5 exact payload");
 if(argc==3 && std::string_view(argv[1])=="--golden-output")
 {
  const auto write=[](const std::filesystem::path& path,const std::vector<std::byte>& bytes){std::ofstream file(path);for(auto b:bytes)file<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);file<<'\n';};
  const std::filesystem::path path=argv[2];write(path,*packet);write(path.parent_path()/"typed_polymorph_inventory_v5.txt",inventory);
  const auto active=first->commit.recovery->after;const auto source=first->commit.items[0].after;
  write(path.parent_path()/"typed_polymorph_active_v1.txt",EncodePolymorphRuntime(*c,{3,255,1,2,0,123,456,1500,&source,&active,&a,"Wolf","Wolf"}).value());
  write(path.parent_path()/"typed_polymorph_book_v1.txt",EncodePolymorphRuntime(*c,{1,255,2,0,0,123,456,1500,&items[2],&state,&a,"Wolf",""}).value());
  write(path.parent_path()/"typed_polymorph_cancel_v1.txt",EncodePolymorphRuntime(*c,{1,255,3,0,0,123,456,1500,nullptr,&active,nullptr,"","Wolf"}).value());
 }
 std::cout<<"Polymorph runtime: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
