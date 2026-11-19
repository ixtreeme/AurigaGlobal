#include "PersistentItemFixture.hpp"
#include <item/NativeMoney.hpp>
#include <item/NativeMoneyWire.hpp>
#include <item/TokenRedemption.hpp>
#include <item/TypedGround.hpp>
#include <item/TypedInventoryWire.hpp>
#include <item/OwnedGround.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdexcept>
using namespace auriga::item;namespace f=persistent_item_fixture;namespace b=item_catalog_fixture;
namespace {
unsigned checks=0;void Check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
ItemKey Key(unsigned id){return ItemKey::Create(ItemCategory::CurrencyToken,id).value();}
ItemCatalogSource Source(){auto s=f::Catalog();auto row=*std::ranges::find(s.items,b::GoldBar(),&ItemBaseRow::key);
 for(unsigned n=1;n<=2;++n){row.key=Key(90000+n);row.contentCode=n==1?"currency.ground_gold":"currency.guild_payout";row.maxStack=1;s.items.push_back(row);s.currencyTokens.push_back({row.key,WalletCurrency::Gold,n==1?1u:0u});s.currencyTokenPolicies.push_back({row.key,n==1?CurrencyTokenMode::GroundGold:CurrencyTokenMode::DynamicGold});}return s;}
void QuestChanges(){auto c=BuildItemCatalog(Source()).value();const ItemOwnerFence fence{f::Character(),7,1};const auto wallet=f::Gold();constexpr std::uint64_t cap=999000000000ULL;
 for(auto delta:{std::int64_t{-1000},std::int64_t{-1},std::int64_t{1},std::int64_t{7000000001}}){auto commit=MakeQuestGoldChange(*c,{60},fence,wallet,delta,cap);Check(commit.has_value(),"signed native quest change plans");Check(commit->wallets[0].after.balance==static_cast<std::uint64_t>(static_cast<std::int64_t>(wallet.balance)+delta),"quest delta is exact in authoritative64-bit wallet");Check(ValidateItemCommit(*c,*commit).has_value(),"quest delta receipt validates");auto bad=*commit;++bad.wallets[0].after.balance;Check(!ValidateItemCommit(*c,bad),"quest cannot substitute credited or debited value");bad=*commit;bad.result[4]^=1;Check(!ValidateItemCommit(*c,bad),"quest sign is bound to receipt");bad=*commit;bad.result[5]^=1;Check(!ValidateItemCommit(*c,bad),"quest magnitude is bound to receipt");}
 for(auto delta:{std::int64_t{0},INT64_MIN,std::int64_t{-1001},INT64_MAX})Check(!MakeQuestGoldChange(*c,{60},fence,wallet,delta,cap),"zero,overflow,insufficient gold denied without partial debit");
 auto full=wallet;full.balance=cap;Check(!MakeQuestGoldChange(*c,{60},fence,full,1,cap),"full wallet refuses quest reward");Check(MakeQuestGoldChange(*c,{60},fence,full,-1,cap).has_value(),"full wallet can still pay");
 ItemWallet gaya{f::Character(),WalletCurrency::Gaya,1,20};
 for(auto delta:{std::int64_t{-20},std::int64_t{-1},std::int64_t{1},std::int64_t{999978}}){auto commit=MakeQuestGayaChange(*c,{61},fence,gaya,delta,MaxNativeGayaBalance);Check(commit && ValidateItemCommit(*c,*commit).has_value(),"native quest Gaya uses fenced wallet receipt");auto bad=*commit;++bad.wallets[0].after.balance;Check(!ValidateItemCommit(*c,bad),"Gaya credit is bound to exact receipt");}
 Check(!MakeQuestGayaChange(*c,{61},fence,gaya,-21,MaxNativeGayaBalance),"Gaya cannot underflow");Check(!MakeQuestGayaChange(*c,{61},fence,gaya,999979,MaxNativeGayaBalance),"original Gaya cap enforced");
 const auto inventory=EncodeTypedInventory(*c,{},std::array{wallet,gaya},{});const auto read=DecodeTypedInventory(*inventory,f::Character());Check(inventory && (*inventory)[5]==std::byte{32} && read && read->wallets[1].balance==20,"GC225 v32 roundtrips native Gaya");
 if(const char* path=std::getenv("AURIGA_GAYA_INVENTORY_GOLDEN")){const auto bag=f::Inventory();const auto source=f::Sword();const auto frame=EncodeTypedInventory(*c,std::span(&bag,1),std::array{wallet,gaya},std::span(&source,1)).value();Check(DecodeTypedInventory(frame,f::Character()).has_value(),"decorated equipment v32 full-format roundtrip");std::ofstream file(path);for(auto b:frame)file<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);file<<'\n';}
 for(unsigned n=0;n<inventory->size();++n)Check(!DecodeTypedInventory(std::span(*inventory).first(n),f::Character()),"every truncated Gaya inventory refused");

}

void Run(){QuestChanges();
 auto c=BuildItemCatalog(Source()).value();const ItemOwnerFence fence{f::Character(),7,1};const std::array layouts{f::Inventory()};const std::array ids{ItemInstanceId::Create(100).value()};
 constexpr std::uint64_t amount=7000000001ULL,cap=999000000000ULL;
 ItemGrant grant{Key(90002),0,1};grant.currencyAmount=amount;const auto made=MakeGrantCommit(*c,{54},fence,layouts,{},grant,ids);Check(made.has_value(),"native issuer creates a variable pouch");const auto pouch=made->items[0].after;
 Check(pouch.currencyAmount==amount && CurrencyUnitAmount(*c,pouch)==amount,"stored value exceeds 32 bits without truncation");Check(ValidateItemCommit(*c,*made).has_value(),"native pouch creation fully validates");
 auto bad=*made;bad.items[0].after.currencyAmount=amount+1;Check(!ValidateItemCommit(*c,bad),"pouch value is bound to the actual issuance receipt");
 bad=*made;bad.result[2]^=1;Check(!ValidateItemCommit(*c,bad),"issuance receipt cannot substitute another amount");
 grant.currencyAmount=0;Check(!MakeGrantCommit(*c,{54},fence,layouts,{},grant,ids),"zero-valued payout is refused");grant.currencyAmount=amount;grant.count=2;Check(!MakeGrantCommit(*c,{54},fence,layouts,{},grant,ids),"variable pouches cannot stack");grant.count=1;
 grant.key=Key(90001);Check(!MakeGrantCommit(*c,{54},fence,layouts,{},grant,ids),"ground Yang can never be materialized in inventory");grant.key=Key(90002);
 const std::array inventory{pouch};const auto move=MakeMoveCommit(*c,{54,1},fence,layouts,inventory,pouch.id,{ItemStorageKind::Inventory,1,8});Check(move && move->items[0].after.currencyAmount==amount,"movement retains exact stored value");bad=*move;++*bad.items[0].after.currencyAmount;Check(!ValidateItemCommit(*c,bad),"movement cannot mint currency");
 const std::array wallets{f::Gold()};const TokenRedemptionActor actor{100,0,0,false};const auto redeem=MakeTokenRedemptionCommit(*c,{54,2},fence,layouts,inventory,wallets,pouch.id,1,cap,actor);
 Check(redeem && redeem->quote.unit==amount && redeem->quote.credit==amount && redeem->commit.wallets[0].after.balance==amount+1000 && redeem->commit.items[0].after.destroyed,"actual redemption atomically consumes the pouch and credits its own 64-bit value");
 bad=redeem->commit;++bad.wallets[0].after.balance;Check(!ValidateItemCommit(*c,bad),"redemption cannot credit a cent more than its receipt");bad=redeem->commit;++bad.result[29];Check(!ValidateItemCommit(*c,bad),"modified redemption receipt is refused");
 auto full=wallets;full[0].balance=cap-amount+1;Check(MakeTokenRedemptionCommit(*c,{54,3},fence,layouts,inventory,full,pouch.id,1,cap,actor).error()==TokenRedemptionError::WalletFull,"insufficient room for the whole payout preserves it intact");
 const auto quote=EncodeTokenRedemption(*c,42,&pouch,1,TokenRedemptionStatus::Ready,redeem->quote);Check(quote && (*quote)[5]==std::byte{2},"dynamic redemption quote is explicitly versioned");
 const auto wire=EncodeTypedInventory(*c,layouts,wallets,inventory).value();const auto read=DecodeTypedInventory(wire,fence.owner);Check(wire[5]==std::byte{30} && read && read->items[0].currencyAmount==amount,"GC225v30 round-trips the actual variable amount");
 for(unsigned n=0;n<wire.size();++n)Check(!DecodeTypedInventory(std::span(wire).first(n),fence.owner),"every truncated money inventory is refused");
 auto credit=MakeGroundCurrencyCredit(*c,{54,4},fence,wallets[0],Key(90001),amount,cap);Check(credit && credit->items.empty() && credit->wallets[0].after.balance==amount+1000,"ground pickup credits wallet without an inventory item or free cell");
 Check(!MakeGroundCurrencyCredit(*c,{54,4},fence,full[0],Key(90001),amount,cap),"ground pickup observes the real wallet cap");bad=*credit;++bad.wallets[0].after.balance;Check(!ValidateItemCommit(*c,bad),"ground credit receipt binds the exact value");bad=*credit;bad.items.push_back({{},pouch});Check(!ValidateItemCommit(*c,bad),"a ground credit cannot also mint a pouch");
 const ItemOwner account{ItemOwnerKind::Account,9};const ItemOwnerFence accountFence{account,1,1};const std::array bank{ItemStorageLayout{{account,ItemStorageKind::Safebox,1},5,9,1}};
 auto award=MakeGrantCommit(*c,{54,5},accountFence,bank,{},grant,ids,ItemStorageKind::Safebox);Check(award.has_value(),"original account payout is delivered straight into account safebox");award->currencyAward=CurrencyAwardClaim{1,amount,"sample"};Check(ValidateItemCommit(*c,*award).has_value(),"award claim and one exact safebox pouch form a valid atomic write");
 bad=*award;++bad.currencyAward->amount;Check(!ValidateItemCommit(*c,bad),"award amount cannot diverge from pouch value");bad=*award;bad.currencyAward->login="Sample";Check(!ValidateItemCommit(*c,bad),"claim must use normalized authenticated login");
 Check(NormalizeCurrencyAwardLogin(" SAMPLE ")=="sample" && !NormalizeCurrencyAwardLogin("a\tb") && !NormalizeCurrencyAwardLogin(""),"native award account normalization is strict");
 auto funded=wallets[0];funded.balance=amount+1000;CurrencyDropIntent intent{{54,6},"test-core",1,fence.owner.id,amount,1000000,1300000,1000000,1,100,200,0,true};
 auto drop=MakeCurrencyDropCommit(*c,intent.operation,fence,funded,intent);Check(drop && drop->items.empty() && drop->wallets[0].after.balance==1000,"gold drop is exact wallet debit and durable ground intent in one commit");
 bad=*drop;++bad.currencyDrop->amount;Check(!ValidateItemCommit(*c,bad),"drop row and wallet debit cannot disagree");bad=*drop;bad.currencyDrop->owner++;Check(!ValidateItemCommit(*c,bad),"drop cannot impersonate another owner");
 intent.create=false;intent.nowAtMs++;credit->currencyDrop=intent;Check(ValidateItemCommit(*c,*credit).has_value(),"pickup atomically claims the original durable drop and credits its value");bad=*credit;++bad.currencyDrop->amount;Check(!ValidateItemCommit(*c,bad),"claim cannot alter original drop amount");bad=*credit;bad.currencyDrop->nowAtMs=bad.currencyDrop->expiresAtMs;Check(!ValidateItemCommit(*c,bad),"expired currency intent cannot be claimed");
 const auto ground=EncodeTypedGround({12,100,200,0,Key(90001),0,1,"currency.ground_gold",amount});Check(ground.has_value(),"ground Yang can be encoded");const auto decoded=DecodeTypedGround(*ground);Check((*ground)[5]==std::byte{2} && decoded && decoded->goldAmount==amount,"GC226v2 preserves 64-bit ground Yang value");
 for(unsigned n=0;n<ground->size();++n)Check(!DecodeTypedGround(std::span(*ground).first(n)),"every truncated money ground frame refused");
 const NativeMoneySnapshot snapshot{NativeMoneyStatus::Ready,42,amount,amount+1000,9,cap};const auto money=EncodeNativeMoney(snapshot).value();Check(DecodeNativeMoney(money)==snapshot,"actual drop quote shares its precise native and Unity wire contract");
 for(unsigned n=0;n<money.size();++n)Check(!DecodeNativeMoney(std::span(money).first(n)),"every truncated Yang quote refused");
 std::array<std::byte,19> request{};request[0]=std::byte{218};request[1]=std::byte{1};for(unsigned n=0;n<8;++n)request[10+n]=std::byte((amount>>(n*8))&255);request[18]=std::byte{56};
 Check(DecodeNativeMoneyRequest(std::span(request).first(NativeMoneyRequestBodySize))->amount==amount,"actual fixed request body excludes the separately validated sequence byte");Check(!DecodeNativeMoneyRequest(request),"sequence is not accidentally counted in the fixed body");
 for(unsigned n=0;n<NativeMoneyRequestBodySize;++n)Check(!DecodeNativeMoneyRequest(std::span(request).first(n)),"all truncated64-bit requests refused");
 request[1]=std::byte{2};Check(!DecodeNativeMoneyRequest(std::span(request).first(NativeMoneyRequestBodySize)),"confirmation must include opaque quote nonce");request[2]=std::byte{1};Check(DecodeNativeMoneyRequest(std::span(request).first(NativeMoneyRequestBodySize)).has_value(),"nonce-bound confirmation accepted");request[1]=std::byte{3};Check(!DecodeNativeMoneyRequest(std::span(request).first(NativeMoneyRequestBodySize)),"close cannot carry a spendable amount");
 auto source=Source();source.currencyTokens.back().amountPerUnit=1;Check(!BuildItemCatalog(source),"dynamic catalog cannot smuggle fixed token value");source=Source();source.currencyTokenPolicies.push_back(source.currencyTokenPolicies.back());Check(!BuildItemCatalog(source),"duplicate money policies refused");
 std::deque<ItemGrant> queue;grant.currencyAmount=amount;Check(QueueGrant(queue,grant,200),"queue accepts first concrete payout");grant.currencyAmount=amount+1;Check(QueueGrant(queue,grant,200) && queue.size()==2,"queue never merges different stored currency values");
}
}
int main(){try{Run();std::cout<<checks<<" native money checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
