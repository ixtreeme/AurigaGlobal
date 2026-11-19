#include "CatalogSnapshotFixture.hpp"
static void TimedUtilities(){
 for(const auto utility:{TimedEffectUtility::FishMind,TimedEffectUtility::NoDeathPenalty}){
  auto source=f::Catalog();auto& p=*std::ranges::find(source.timedEffects,21101u,&TimedEffectProfile::id);p.utility=utility;p.bonusProfile=0;p.durationMs=utility==TimedEffectUtility::FishMind ? 1200000ULL : 536870911000ULL;
  auto& group=*std::ranges::find(source.timedEffectGroups,21101u,&TimedEffectGroup::id);group.removeOnDeath=utility==TimedEffectUtility::FishMind;
  const auto built=BuildItemCatalog(source);if(!built)std::cerr<<"Timed utility catalog rejected: "<<built.error().field<<'\n';Check(built.has_value(),"timed utility publishes without a fictitious stat profile");const auto& c=**built;
  const auto snapshot=catalog_snapshot_fixture::Snapshot(source);Check(sql::DecodeSnapshot(snapshot,{source.revision,c.ContentHash()}).has_value(),"nullable bonus and utility kind round-trip in native SQL catalog codec");
  const std::array layouts{pf::Inventory()};const std::array items{f::Item(c,1,71)};const ItemOwnerFence fence{pf::Character(),7,1};const auto facts=f::Facts();RecoverySchedule empty;
  const auto used=MakeItemEffectUse(c,{0xB7,static_cast<std::uint8_t>(utility)},fence,layouts,items,empty,items[0].id,facts,0,1000,900).value();const auto state=used.recovery->after;
  Check(used.items[0].after.count==4 && state.effects.size()==1 && state.effects[0].remainingMs==p.durationMs && !state.effects[0].expiresAtMs,"utility debit and original online lifetime are one commit");
  Check(FindTimedUtility(c,state,utility,1000)==900 && !FindTimedUtility(c,state,TimedEffectUtility::None,1000),"typed utility lookup is explicit, never inferred from empty bonuses");
  Check(CollectItemEffectBonuses(c,state,items).value().empty(),"non-stat utility changes no equipment or combat bonuses");
  Check(!MakeItemEffectUse(c,{0xB7,10},fence,layouts,items,state,items[0].id,facts,0,1000,901),"duplicate utility refuses without another debit");
  auto legacy=facts;legacy.existingUtilityMask=1u<<(static_cast<unsigned>(utility)-1);Check(!MakeItemEffectUse(c,{0xB7,13},fence,layouts,items,empty,items[0].id,legacy,0,1000,901),"existing server-granted legacy utility rejects a duplicate native item without debit");
  const auto bytes=EncodeRecoverySchedule(c,state).value();Check(DecodeRecoverySchedule(c,bytes).value()==state,"existing durable schedule preserves utility identity across relog");
  const auto wire=EncodeItemEffectsRuntime(c,{1,255,123,456,&items[0],&state,items,1000}).value();Check(std::to_integer<unsigned>(wire[5])==3,"utility quote and active state use explicit GC243 v3");
  auto advanced=AdvanceRecoverySchedule(c,state,123).value();Check(advanced.effects[0].remainingMs==p.durationMs-123 && FindTimedUtility(c,advanced,utility,UINT64_MAX)==900,"only online elapsed consumes online utility lifetime");
  auto death=state;auto dead=facts;dead.alive=false;NormalizeTimedItems(c,death,1000,&dead);Check(death.effects.empty()==(utility==TimedEffectUtility::FishMind),"fish mind clears on death while medal survives until eligible penalty");
  const auto cancel=MakeItemEffectCancel(c,{0xB7,11},fence,state,0,1000,state.generation,900).value();Check(cancel.recovery->after.effects.empty() && cancel.items.empty(),"consuming exact utility protection never consumes another inventory object");Check(!MakeItemEffectCancel(c,{0xB7,12},fence,state,0,1000,state.generation+1,900),"stale utility protection generation cannot be spent");
  auto expired=AdvanceRecoverySchedule(c,state,p.durationMs).value();Check(!FindTimedUtility(c,expired,utility,1000),"expired utility cannot authorize a gameplay benefit");
  auto invalid=source;std::ranges::find(invalid.timedEffects,21101u,&TimedEffectProfile::id)->bonusProfile=7;Check(!BuildItemCatalog(invalid),"utility cannot smuggle a stat profile");
  invalid=source;std::ranges::find(invalid.timedEffectGroups,21101u,&TimedEffectGroup::id)->clock=TimedEffectClock::WallTime;Check(!BuildItemCatalog(invalid),"original utility cannot silently switch to offline wall time");
  invalid=source;std::ranges::find(invalid.timedEffects,21101u,&TimedEffectProfile::id)->utility=TimedEffectUtility::None;Check(!BuildItemCatalog(invalid),"ordinary timed effect still requires a real bonus profile");
 }
}
