#include "../../SRC/Server/GameServer/raid/icc/SpellRecords.generated.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
int main(){
 uint32_t prior=0;for(auto& record:SpellRecords){Require(record.id>prior,"sorted unique catalog");prior=record.id;Require(Spells().Find(record.id)==&record,"lookup every record");}
 for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25}){
  Require(ParseMode(ModeName(mode))==mode,"difficulty round trip");
  Require(PlayerLimit(mode)==(Raid25(mode)?25:10),"actual WoW raid size");
  auto slice=Spells().Resolve(69055,mode);Require(slice&&slice->id==(Raid25(mode)?70814:69055),"heroic bone slice uses normal same-size fallback");
  Require(slice->maximumTargets==(Raid25(mode)?3:2),"resolved slice cap");
  Require(Spells().Resolve(69075,mode)->id==RaidMode<uint32_t>(mode,69075,70834,70835,70836),"all four bone storm damage spells");
  Require(Spells().Resolve(72219,mode)->id==RaidMode<uint32_t>(mode,72219,72551,72552,72553),"all four gastric bloat spells");
  Require(Spells().Resolve(26662,mode)->id==26662,"single-version berserk unchanged");
 }
 Require(!ParseMode("30_heroic")&&!ParseMode("25")&&!ParseMode(""),"invalid modes not silently accepted");
 Require(Spells().Find(71356)&&Spells().Find(3617),"server-only spells resolved from upstream SQL rather than fabricated DBC records");
 Require(!Spells().Resolve(99999999,Difficulty::Heroic25),"genuinely absent source spell stays missing");
 Require(Spells().Find(69076)->castTime==3000,"bone storm cast has windup");
 Require(Spells().Find(69065)->duration==-1,"impale persists until released");
 Require(Spells().Find(69065)->effects[1].aura==89&&Spells().Find(69065)->effects[1].period==1000,"impale is periodic percent health, not flat damage");
 Require(Spells().Find(73799)->effects[0].trigger==-1,"signed source sentinel preserved, never converted to a huge trigger ID");
 const SpellRecord sparse[]={{1,{{1,2,3,4}}},{2}};SpellCatalog missingVariant(sparse,2);
 Require(missingVariant.ResolveID(1,Difficulty::Heroic25)==4,"ID lookup follows source even if record absent");
 Require(missingVariant.Resolve(1,Difficulty::Heroic25)->id==1,"missing variant record falls back to original SpellInfo");
 Require(Spells().Find(69146)->radius[0]==5&&Spells().Find(69778)->radius[0]==6,"effect-indexed radii retain source geometry");
 std::cout<<"ICC_SPELL_CATALOG_PASS "<<SpellRecords.size()<<"_records four_modes heroic_fallback server_sql effect_radii unresolved_guard\n";
}
