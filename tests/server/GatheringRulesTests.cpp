#include "ToolRuntimeFixture.hpp"
#include <item/ResourceGathering.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;using J=nlohmann::json;unsigned checks=0;void Check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
int main(){try{auto c=BuildItemCatalog(tool_runtime_fixture::Catalog()).value();J row={{"id",1u},{"map",41u},{"race",9009u},{"name","Helyi tó"},{"kind",2u},{"pool",2u},{"range",400u},{"duration_ms",15000u},{"minimum_power",1u},{"power_step",1u},{"base_chance_bp",10000u},{"power_chance_bp",0u},{"bait_chance_bp",0u},{"rewards",J::array({{{"category",12u},{"local",6601u},{"count",2u},{"weight",1u}}})}};J doc={{"version",1u},{"sites",J::array({row})}};
Check(ParseGatheringSites(doc.dump(),*c).has_value(),"node world document");doc["version"]=2;doc["sites"][0]["race"]=0;Check(ParseGatheringSites(doc.dump(),*c)->front().race==0,"v2 water world document");
for(const auto& key:{"id","map","race","name","kind","pool","range","duration_ms","minimum_power","power_step","base_chance_bp","power_chance_bp","bait_chance_bp","rewards"}){auto bad=doc;bad["sites"][0].erase(key);Check(!ParseGatheringSites(bad.dump(),*c),"every required site field");}
for(auto bad:J::array({J{{"version",3u},{"sites",J::array()}},J{{"version",2.0},{"sites",J::array()}},J{{"version",2u},{"sites",false}},J{{"version",2u},{"sites",J::array()},{"extra",1}}}))Check(!ParseGatheringSites(bad.dump(),*c),"strict document shape");
for(const auto& [field,value]:std::vector<std::pair<std::string,J>>{{"id",-1},{"range",0},{"range",1001},{"power_step",0},{"base_chance_bp",10001},{"duration_ms",4999},{"kind",1},{"pool",1},{"name",""},{"map","41"},{"minimum_power",2147483648ull}}){auto bad=doc;bad["sites"][0][field]=value;Check(!ParseGatheringSites(bad.dump(),*c),"strict site values");}
auto bad=doc;bad["version"]=1;Check(!ParseGatheringSites(bad.dump(),*c),"v1 cannot silently become water");bad=doc;bad["sites"].push_back(bad["sites"][0]);Check(!ParseGatheringSites(bad.dump(),*c),"duplicate site");bad=doc;bad["sites"][0]["rewards"][0]["local"]=999999u;Check(!ParseGatheringSites(bad.dump(),*c),"catalog reference");
Check(!ParseGatheringSites("{\"version\":1,\"version\":2,\"sites\":[]}",*c),"duplicate JSON field");Check(!ParseGatheringSites(std::string(1048577,' '),*c),"byte budget");Check(ParseGatheringSites("{\"version\":1,\"sites\":[]}",*c)->empty(),"empty file disables sites");
std::cout<<"Gathering rules: "<<checks<<" checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
