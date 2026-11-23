#include "PersistentItemFixture.hpp"
#include <item/DungeonEntry.hpp>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;namespace f=persistent_item_fixture;
namespace {
unsigned checks=0;void Check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
void Run(){
 auto source=f::Catalog();const std::array codes{"dungeon.rune.fragment","dungeon.rune.floor_key","catalog.10.ticket_of_the_runes_78bafde9234a","dungeon.orcs.key"};std::vector<ItemKey> purgeKeys;
 for(unsigned n=0;n<codes.size();++n){auto base=*std::ranges::find(source.items,f::Leather().key,&ItemBaseRow::key);base.key=ItemKey::Create(ItemCategory::Quest,98001+n).value();base.contentCode=codes[n];source.items.push_back(base);source.questItems.push_back({base.key,QuestItemDispatch::Token,"",0});purgeKeys.push_back(base.key);}
 const auto c=BuildItemCatalog(source).value();std::vector<ItemOwnerFence> owners;std::vector<ItemStorageLayout> layouts;std::vector<PersistentItem> items;
 for(unsigned n=0;n<8;++n){const ItemOwner owner{ItemOwnerKind::Character,700+n};owners.push_back({owner,10+n,20+n});layouts.push_back(f::Inventory(owner));auto item=f::Leather(7000+n);item.owner=owner;item.count=n+1;items.push_back(item);}
 const auto reserve=MakeDungeonEntryReservation(*c,{58,1},"dungeon.test.entry",items[0].key,0,1,100000,owners,items);
 Check(reserve && reserve->items.empty() && reserve->wallets.empty() && reserve->owners.size()==8 && reserve->dungeonEntry->tickets.size()==8,"all8 party tickets reserve without spending or relocating any instance");
 for(unsigned n=0;n<8;++n){const auto& t=reserve->dungeonEntry->tickets[n];Check(t.owner==owners[n].owner.id && t.instance==items[n].id.Value() && t.count==items[n].count && t.units==1 && t.stateHash==PersistentItemDigest(*c,items[n]).value(),"reservation binds exact owner/session/concrete before-image");}
 auto current=owners;for(auto& f:current)++f.revision;
 const auto consume=MakeDungeonEntryResolution(*c,{58,2},*reserve->dungeonEntry,true,current,layouts,items);Check(consume && consume->items.size()==8 && consume->owners.size()==8 && consume->storages.size()==8,"party debit is one bounded atomic native commit");
 for(unsigned n=0;n<8;++n){const auto& ch=consume->items[n];Check(ch.before->id==items[n].id && ch.after.count==n && ch.after.destroyed==(n==0) && ch.after.placement.has_value()==(n!=0) && ch.after.attributes==items[n].attributes && ch.after.stones==items[n].stones,"confirmed consumption preserves rich state and tombstones only each last unit");}
 const auto release=MakeDungeonEntryResolution(*c,{58,3},*reserve->dungeonEntry,false,current,layouts,items);Check(release && release->items.empty() && release->storages.empty(),"cancelled preflight releases only external reservation without reconstructing rewards");
 auto newLogin=current;for(auto& f:newLogin)++f.sessionGeneration;Check(MakeDungeonEntryResolution(*c,{58,4},*reserve->dungeonEntry,false,newLogin,{},{}).has_value(),"new authenticated session can release stale reservations before inventory publication");Check(!MakeDungeonEntryResolution(*c,{58,5},*reserve->dungeonEntry,true,newLogin,layouts,items),"new session cannot spend a previous session's entry reservation");
 auto bad=*consume;++bad.items[1].after.count;Check(!ValidateItemCommit(*c,bad),"forged retained unit rejected");bad=*consume;bad.items[1].after.owner=owners[0].owner;Check(!ValidateItemCommit(*c,bad),"entry debit cannot transfer ownership");bad=*reserve;bad.dungeonEntry->tickets[0].units=0;Check(!ValidateItemCommit(*c,bad),"zero unit reservation rejected");bad=*reserve;bad.dungeonEntry->tickets[1]=bad.dungeonEntry->tickets[0];Check(!ValidateItemCommit(*c,bad),"duplicate party owner or instance rejected");bad=*reserve;bad.dungeonEntry->gate="dungeon';DROP";Check(!ValidateItemCommit(*c,bad),"untrusted gate syntax rejected");bad=*reserve;bad.dungeonEntry->reservation={};Check(!ValidateItemCommit(*c,bad),"zero reservation identity rejected");bad=*consume;bad.dungeonEntry->tickets[0].stateHash[0]^=1;Check(!ValidateItemCommit(*c,bad),"changed concrete before hash rejected");
 auto changed=items;++changed[0].revision;Check(!MakeDungeonEntryResolution(*c,{58,6},*reserve->dungeonEntry,true,current,layouts,changed),"stale instance revision cannot consume");changed=items;changed[0].count=0;Check(!MakeDungeonEntryReservation(*c,{58,7},"dungeon.test.entry",items[0].key,0,1,100000,owners,changed),"one missing participant ticket refuses entire reservation");changed=items;changed[0].privateShop=PrivateShopListing{{58,9},1,0};Check(!MakeDungeonEntryReservation(*c,{58,8},"dungeon.test.entry",items[0].key,0,1,100000,owners,changed),"listed ticket cannot also be reserved for entry");
 auto larger=owners;larger.push_back({{ItemOwnerKind::Character,900},1,1});auto ninth=items.front();ninth.id=ItemInstanceId::Create(9000).value();ninth.owner=larger.back().owner;auto nineItems=items;nineItems.push_back(ninth);Check(MakeDungeonEntryReservation(*c,{58,9},"dungeon.test.entry",items[0].key,0,1,100000,larger,nineItems).error()==ItemPersistenceError::LimitExceeded,"ninth owner cannot create an oversized party operation");
 auto reordered=*reserve;std::ranges::reverse(reordered.dungeonEntry->tickets);Check(ItemCommitDigest(*c,reordered)==ItemCommitDigest(*c,*reserve),"reservation digest is stable across equivalent participant ordering");
 auto mixedTickets=items;auto preferred=items.front();preferred.key=purgeKeys.front();
 preferred.id=ItemInstanceId::Create(98001).value();preferred.placement->anchor=4;
 mixedTickets.push_back(preferred);const std::array choices{preferred.key,items.front().key};
 const auto mixed=MakeDungeonEntryReservation(*c,{58,10},"dungeon.catacombe.entry",choices,0,1,100000,owners,mixedTickets);
 Check(mixed && mixed->dungeonEntry->tickets.front().instance==preferred.id.Value() &&
    mixed->dungeonEntry->tickets[1].key==items[1].key,"two ticket kinds are selected independently with the original alternate preferred");
 const auto mixedDebit=MakeDungeonEntryResolution(*c,{58,11},*mixed->dungeonEntry,true,current,layouts,mixedTickets);
 Check(mixedDebit && mixedDebit->items.size()==8 &&
    std::ranges::none_of(mixedDebit->items,[&](const auto& change){return change.after.id==items[0].id;}),
    "only one of the two carried tickets is debited for that party member");
 for(unsigned action=1;action<=3;++action){auto forged=*reserve;forged.dungeonEntry->kind=static_cast<DungeonEntryMutationKind>(action);forged.outcomeCode=DungeonEntryReserveOutcome+action-1;Check(action==1 || !ValidateItemCommit(*c,forged),"receipt cannot impersonate a different reservation transition");}
 auto withLeftovers=items;for(unsigned n=0;n<3;++n){auto item=items.front();item.key=purgeKeys[n];item.id=ItemInstanceId::Create(10000+n).value();item.count=3;item.placement->anchor=2+n;withLeftovers.push_back(item);}
 const auto rune=MakeDungeonEntryReservation(*c,{58,20},"dungeon.rune.entry",items[0].key,0,1,100000,owners,withLeftovers).value();const auto runeDebit=MakeDungeonEntryResolution(*c,{58,21},*rune.dungeonEntry,true,current,layouts,withLeftovers);Check(runeDebit && runeDebit->items.size()==11 && std::ranges::all_of(std::span(runeDebit->items).last(3),[](const auto& ch){return ch.after.destroyed && !ch.after.count;}),"Rune entry atomically removes all3 original leftover kinds with the8 party tickets");
 auto pyramidItems=items;for(unsigned n=0;n<2;++n){auto item=items.front();item.key=purgeKeys.back();item.id=ItemInstanceId::Create(11000+n).value();item.count=200;item.placement->anchor=2+n;pyramidItems.push_back(item);}const auto pyramid=MakeDungeonEntryReservation(*c,{58,22},"dungeon.pyramid.entry",items[0].key,0,1,100000,owners,pyramidItems).value();auto pyramidDebit=MakeDungeonEntryResolution(*c,{58,23},*pyramid.dungeonEntry,true,current,layouts,pyramidItems);Check(pyramidDebit && pyramidDebit->items.size()==10 && pyramidDebit->items[8].after.count==0 && pyramidDebit->items[9].after.count==145,"Pyramid preserves original255-unit cleanup cap across multiple concrete stacks");

 const auto greenKey = purgeKeys.back();
 auto orcItems = pyramidItems; // The leader carries 400 old keys across two stacks.
 auto memberKey = orcItems.back();
 memberKey.id = ItemInstanceId::Create(11002).value();
 memberKey.owner = owners[1].owner;
 orcItems.push_back(memberKey);
 const auto orcs = MakeDungeonEntryReservation(*c, {58, 24}, "dungeon.orcs.entry", items.front().key,
     0, 1, 100000, owners, orcItems);
 Check(orcs && orcs->items.empty(), "Orc entry reservation preserves tickets and green keys");
 const auto orcsDebit = MakeDungeonEntryResolution(*c, {58, 25}, *orcs->dungeonEntry, true, current, layouts, orcItems);
 Check(orcsDebit && orcsDebit->items.size() == 11 &&
     std::ranges::count_if(orcsDebit->items, [&](const auto& change) {
         return change.before->key == items.front().key && change.after.count + 1 == change.before->count;
     }) == 8 &&
     std::ranges::count_if(orcsDebit->items, [&](const auto& change) {
         return change.before->key == greenKey && change.after.destroyed && change.after.count == 0;
     }) == 3, "Orc entry debits one ticket per member and fully clears every green-key stack");
 const auto orcsRelease = MakeDungeonEntryResolution(*c, {58, 26}, *orcs->dungeonEntry, false, current, layouts, orcItems);
 Check(orcsRelease && orcsRelease->items.empty(), "Cancelled Orc entry does not spend tickets or clear green keys");

 auto forgedPurge=*pyramidDebit;--forgedPurge.items.back().after.count;Check(!ValidateItemCommit(*c,forgedPurge),"cleanup cannot destroy one extra unit beyond authored cap");auto unrelated=items[0];unrelated.id=ItemInstanceId::Create(12000).value();unrelated.placement->anchor=20;auto removed=unrelated;++removed.revision;removed.destroyed=true;removed.count=0;removed.placement.reset();forgedPurge=*consume;forgedPurge.items.push_back({unrelated,removed});Check(!ValidateItemCommit(*c,forgedPurge),"entry receipt cannot smuggle destruction of an unrelated item");
}
}
int main(){try{Run();std::cout<<checks<<" dungeon entry checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
