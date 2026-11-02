#include "PersistentItemFixture.hpp"
#include <item/TokenRedemption.hpp>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;namespace f=persistent_item_fixture;namespace base=item_catalog_fixture;
namespace
{
unsigned checks=0;
void Check(bool okay,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!okay)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
std::vector<std::byte> Run()
{
    const auto catalog=BuildItemCatalog(f::Catalog()).value();const auto& c=*catalog;const auto owner=f::Character();const auto fence=ItemOwnerFence{owner,7,1};
    PersistentItem token{ItemInstanceId::Create(10).value(),base::GoldBar(),owner};token.count=10;token.placement=ItemPlacement{ItemStorageKind::Inventory,1,0};
    std::vector<PersistentItem> items{token};const std::vector<ItemStorageLayout> layouts{f::Inventory()};auto wallets=std::vector{f::Gold()};TokenRedemptionActor actor{100,0,0,false};
    auto plan=MakeTokenRedemptionCommit(c,{14},fence,layouts,items,wallets,token.id,3,INT64_MAX,actor);
    Check(plan && plan->quote.requested==3 && plan->quote.consumed==3 && plan->quote.credit==15000000,"explicit selected whole units are quoted");
    Check(plan->commit.items.size()==1 && plan->commit.items[0].after.id==token.id && plan->commit.items[0].after.count==7 && plan->commit.items[0].after.revision==2,"partial stack preserves identity and changes revision once");
    Check(plan->commit.wallets.size()==1 && plan->commit.wallets[0].after.balance==15001000 && plan->commit.wallets[0].after.revision==2 && ValidateItemCommit(c,plan->commit).has_value(),"token and exact wallet credit share one valid commit");
    const auto same=MakeTokenRedemptionCommit(c,{14},fence,layouts,items,wallets,token.id,3,INT64_MAX,actor);
    Check(same && ItemCommitDigest(c,same->commit)==ItemCommitDigest(c,plan->commit),"same before image and request reproduce exact receipt digest");
    auto all=MakeTokenRedemptionCommit(c,{15},fence,layouts,items,wallets,token.id,10,INT64_MAX,actor);
    Check(all && all->commit.items[0].after.destroyed && !all->commit.items[0].after.placement && all->commit.items[0].after.count==0,"full redemption destroys only the redeemed stack");
    wallets[0].balance=std::uint64_t(INT64_MAX)-12000000;
    auto partial=MakeTokenRedemptionCommit(c,{16},fence,layouts,items,wallets,token.id,10,INT64_MAX,actor);
    Check(partial && partial->quote.consumed==2 && partial->quote.credit==10000000 && partial->commit.items[0].after.count==8 && partial->commit.wallets[0].after.balance==std::uint64_t(INT64_MAX)-2000000,"wallet cap leaves every uncredited token intact");
    wallets[0].balance=std::uint64_t(INT64_MAX)-4999999;
    Check(MakeTokenRedemptionCommit(c,{17},fence,layouts,items,wallets,token.id,10,INT64_MAX,actor).error()==TokenRedemptionError::WalletFull,"less than one whole unit never consumes a token");
    wallets[0]=f::Gold();
    Check(!MakeTokenRedemptionCommit(c,{18},fence,layouts,items,wallets,token.id,0,INT64_MAX,actor),"zero request denied");
    Check(!MakeTokenRedemptionCommit(c,{19},fence,layouts,items,wallets,token.id,11,INT64_MAX,actor),"request cannot exceed source count");
    Check(!MakeTokenRedemptionCommit(c,{},fence,layouts,items,wallets,token.id,1,INT64_MAX,actor),"zero operation identity denied");
    Check(!MakeTokenRedemptionCommit(c,{20},fence,layouts,items,wallets,token.id,1,999,actor),"balance above supplied cap denied");
    Check(!MakeTokenRedemptionCommit(c,{21},fence,layouts,items,wallets,token.id,1,UINT64_MAX,actor),"writer never exceeds persistent signed 64-bit balance");
    auto foreign=wallets;foreign[0].owner.id++;
    Check(!MakeTokenRedemptionCommit(c,{22},fence,layouts,items,foreign,token.id,1,INT64_MAX,actor),"foreign wallet cannot receive credit");
    foreign=wallets;foreign.push_back(foreign[0]);Check(!MakeTokenRedemptionCommit(c,{23},fence,layouts,items,foreign,token.id,1,INT64_MAX,actor),"duplicate wallets denied");
    foreign=wallets;foreign[0].revision=UINT64_MAX;Check(!MakeTokenRedemptionCommit(c,{24},fence,layouts,items,foreign,token.id,1,INT64_MAX,actor),"wallet revision cannot wrap");
    items[0].placement=ItemPlacement{ItemStorageKind::TradeEscrow,1,0};Check(!MakeTokenRedemptionCommit(c,{25},fence,layouts,items,wallets,token.id,1,INT64_MAX,actor),"escrow token never redeems");items[0]=token;
    items[0].boundTo=ItemOwner{ItemOwnerKind::Character,43};Check(!MakeTokenRedemptionCommit(c,{26},fence,layouts,items,wallets,token.id,1,INT64_MAX,actor),"foreign-bound token denied");
    auto source=f::Catalog();std::ranges::find(source.items,token.key,&ItemBaseRow::key)->binding=BindingPolicy::OnAcquire;const auto bound=BuildItemCatalog(source).value();items[0].boundTo=owner;
    Check(MakeTokenRedemptionCommit(*bound,{27},fence,layouts,items,wallets,token.id,1,INT64_MAX,actor).has_value(),"own bound token remains usable");items[0]=token;
    items[0].attributes.lockedSlot=1;Check(!MakeTokenRedemptionCommit(c,{28},fence,layouts,items,wallets,token.id,1,INT64_MAX,actor),"customized token cannot be automatically destroyed");items[0]=token;
    items[0].revision=UINT64_MAX;Check(!MakeTokenRedemptionCommit(c,{29},fence,layouts,items,wallets,token.id,1,INT64_MAX,actor),"item revision cannot wrap");items[0]=token;
    source=f::Catalog();std::ranges::find(source.items,token.key,&ItemBaseRow::key)->minimumLevel=101;const auto high=BuildItemCatalog(source).value();
    Check(MakeTokenRedemptionCommit(*high,{30},fence,layouts,items,wallets,token.id,1,INT64_MAX,actor).error()==TokenRedemptionError::Level,"minimum level enforced");
    auto retired=source;std::ranges::find(retired.items,token.key,&ItemBaseRow::key)->retired=true;const auto old=BuildItemCatalog(retired).value();
    Check(MakeTokenRedemptionCommit(*old,{31},fence,layouts,items,wallets,token.id,1,INT64_MAX,actor).error()==TokenRedemptionError::Denied,"retired token denied");
    actor.characterClass=4;Check(!MakeTokenRedemptionCommit(c,{32},fence,layouts,items,wallets,token.id,1,INT64_MAX,actor),"invalid actor class denied");actor.characterClass=0;
    auto voucher=token;voucher.key=base::CoinVoucher();items={voucher};
    Check(MakeTokenRedemptionCommit(c,{33},fence,layouts,items,wallets,voucher.id,1,INT64_MAX,actor).error()==TokenRedemptionError::Unsupported,"account coin voucher never credits character Gold");
    items={f::Leather()};Check(!MakeTokenRedemptionCommit(c,{34},fence,layouts,items,wallets,items[0].id,1,INT64_MAX,actor),"non-token denied");
    source=f::Catalog();source.currencyTokens[0].amountPerUnit=INT64_MAX;const auto huge=BuildItemCatalog(source).value();items={token};wallets[0].balance=0;
    auto large=MakeTokenRedemptionCommit(*huge,{35},fence,layouts,items,wallets,token.id,10,INT64_MAX,actor);
    Check(large && large->quote.consumed==1 && large->quote.credit==std::uint64_t(INT64_MAX) && large->commit.items[0].after.count==9,"huge unit value credits exactly one without overflow");
    const auto packet=EncodeTokenRedemption(c,0x123456789abcdefULL,&token,1,TokenRedemptionStatus::Ready,partial->quote);
    Check(packet && EncodeTokenRedemption(c,0,nullptr,0,TokenRedemptionStatus::Closed,{})->size()==88,"native partial quote and terminal wire envelope");
    auto bad=partial->quote;bad.credit++;Check(!EncodeTokenRedemption(c,1,&token,1,TokenRedemptionStatus::Ready,bad),"incorrect quoted amount cannot be encoded as actionable");
    return *packet;
}
}
int main(int argc,char** argv)
{try{const auto bytes=Run();if(argc==3 && std::string_view(argv[1])=="--write-golden"){std::ofstream out(argv[2]);for(auto byte:bytes)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(byte);out<<'\n';if(!out)throw std::runtime_error("golden write failed");}std::cout<<checks<<" token redemption checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
