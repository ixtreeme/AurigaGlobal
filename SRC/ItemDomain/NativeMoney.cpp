#include <item/NativeMoney.hpp>
#include <item/PersistentItems.hpp>
#include <algorithm>
namespace auriga::item {
namespace {
void Put(std::vector<std::uint8_t>& b,std::uint64_t value,unsigned size){for(unsigned n=0;n<size;++n)b.push_back(static_cast<std::uint8_t>(value>>(8*n)));}
std::uint64_t Get(std::span<const std::uint8_t> b,unsigned at,unsigned size){std::uint64_t value=0;for(unsigned n=0;n<size;++n)value|=std::uint64_t{b[at+n]}<<(8*n);return value;}
}
bool MoneyPassiveAccessoryDebit(const ItemChange& change){
    if(!change.before || !change.before->accessory || !change.after.accessory || !change.before->placement || change.before->placement->kind!=ItemStorageKind::Equipment)return false;
    const auto& before=*change.before->accessory;const auto& after=*change.after.accessory;
    if(before.permanent || after.permanent || !before.activeGrade || after.unlockedCapacity!=before.unlockedCapacity || after.activeGrade>before.activeGrade || (after.activeGrade==before.activeGrade && after.remainingMs>=before.remainingMs))return false;
    auto expected=*change.before;expected.revision=change.after.revision;expected.accessory=after;return expected==change.after;
}
std::expected<ItemCommit,MoneyError> MakeQuestGoldChange(const ItemCatalog& c,std::array<std::uint8_t,16> operation,ItemOwnerFence fence,const ItemWallet& wallet,std::int64_t delta,std::uint64_t cap){
    if(!delta||delta==INT64_MIN||!cap||cap>INT64_MAX||wallet.balance>cap||wallet.owner!=fence.owner||fence.owner.kind!=ItemOwnerKind::Character||wallet.currency!=WalletCurrency::Gold||!wallet.revision||wallet.revision==UINT64_MAX)return std::unexpected(MoneyError::Invalid);
    const auto amount=static_cast<std::uint64_t>(delta<0?-delta:delta);
    if(delta<0?amount>wallet.balance:amount>cap-wallet.balance)return std::unexpected(MoneyError::WalletFull);
    ItemCommit out;out.operationId=operation;out.catalogRevision=c.Revision();out.catalogHash=c.ContentHash();out.kind=ItemOperationKind::Service;out.owners={fence};out.outcomeCode=QuestGoldChangeOutcome;
    auto after=wallet;after.balance=delta<0?wallet.balance-amount:wallet.balance+amount;++after.revision;out.wallets.push_back({wallet,after});out.result={'Q','Y','G',1,static_cast<std::uint8_t>(delta<0)};Put(out.result,amount,8);Put(out.result,cap,8);
    if(!ValidateItemCommit(c,out))return std::unexpected(MoneyError::Invalid);return out;
}
bool ValidQuestGoldChange(const ItemCatalog&,const ItemCommit& out){
    const auto& b=out.result;
    if(out.outcomeCode!=QuestGoldChangeOutcome||out.kind!=ItemOperationKind::Service||out.owners.size()!=1||out.wallets.size()!=1||!std::ranges::all_of(out.items,MoneyPassiveAccessoryDebit)||out.recovery||out.ground||out.learning||out.petStudyCooldown||out.currencyAward||out.currencyDrop||out.privateShop||out.dungeonEntry||b.size()!=21||b[0]!='Q'||b[1]!='Y'||b[2]!='G'||b[3]!=1||b[4]>1)return false;
    const auto amount=Get(b,5,8),cap=Get(b,13,8);const auto& before=out.wallets[0].before;
    if(!amount||amount>INT64_MAX||!cap||cap>INT64_MAX||before.balance>cap||before.owner!=out.owners[0].owner||before.owner.kind!=ItemOwnerKind::Character||before.currency!=WalletCurrency::Gold||!before.revision||before.revision==UINT64_MAX||(b[4]?amount>before.balance:amount>cap-before.balance))return false;
    auto expected=before;expected.balance=b[4]?before.balance-amount:before.balance+amount;++expected.revision;return expected==out.wallets[0].after;
}
std::expected<ItemCommit,MoneyError> MakeQuestGayaChange(const ItemCatalog& c,std::array<std::uint8_t,16> operation,ItemOwnerFence fence,const ItemWallet& wallet,std::int64_t delta,std::uint64_t cap){
    if(!delta||delta==INT64_MIN||!cap||cap>MaxNativeGayaBalance||wallet.balance>cap||wallet.owner!=fence.owner||fence.owner.kind!=ItemOwnerKind::Character||wallet.currency!=WalletCurrency::Gaya||!wallet.revision||wallet.revision==UINT64_MAX)return std::unexpected(MoneyError::Invalid);
    const auto amount=static_cast<std::uint64_t>(delta<0?-delta:delta);
    if(delta<0?amount>wallet.balance:amount>cap-wallet.balance)return std::unexpected(MoneyError::WalletFull);
    ItemCommit out;out.operationId=operation;out.catalogRevision=c.Revision();out.catalogHash=c.ContentHash();out.kind=ItemOperationKind::Service;out.owners={fence};out.outcomeCode=QuestGayaChangeOutcome;
    auto after=wallet;after.balance=delta<0?wallet.balance-amount:wallet.balance+amount;++after.revision;out.wallets.push_back({wallet,after});out.result={'Q','G','Y',1,static_cast<std::uint8_t>(delta<0)};Put(out.result,amount,8);Put(out.result,cap,8);
    if(!ValidateItemCommit(c,out))return std::unexpected(MoneyError::Invalid);return out;
}
bool ValidQuestGayaChange(const ItemCatalog&,const ItemCommit& out){
    const auto& b=out.result;
    if(out.outcomeCode!=QuestGayaChangeOutcome||out.kind!=ItemOperationKind::Service||out.owners.size()!=1||out.wallets.size()!=1||!std::ranges::all_of(out.items,MoneyPassiveAccessoryDebit)||out.recovery||out.ground||out.learning||out.petStudyCooldown||out.currencyAward||out.currencyDrop||out.privateShop||out.dungeonEntry||b.size()!=21||b[0]!='Q'||b[1]!='G'||b[2]!='Y'||b[3]!=1||b[4]>1)return false;
    const auto amount=Get(b,5,8),cap=Get(b,13,8);const auto& before=out.wallets[0].before;
    if(!amount||amount>INT64_MAX||!cap||cap>MaxNativeGayaBalance||before.balance>cap||before.owner!=out.owners[0].owner||before.owner.kind!=ItemOwnerKind::Character||before.currency!=WalletCurrency::Gaya||!before.revision||before.revision==UINT64_MAX||(b[4]?amount>before.balance:amount>cap-before.balance))return false;
    auto expected=before;expected.balance=b[4]?before.balance-amount:before.balance+amount;++expected.revision;return expected==out.wallets[0].after;
}
std::expected<ItemCommit,MoneyError> MakeGroundCurrencyCredit(const ItemCatalog& c,std::array<std::uint8_t,16> operation,ItemOwnerFence fence,const ItemWallet& wallet,ItemKey key,std::uint64_t amount,std::uint64_t cap){
    if(fence.owner.kind!=ItemOwnerKind::Character || !ValidItemOwner(fence.owner) || wallet.owner!=fence.owner || wallet.currency!=WalletCurrency::Gold || !wallet.revision || wallet.revision==UINT64_MAX || !amount || !cap || cap>INT64_MAX || wallet.balance>cap)return std::unexpected(MoneyError::Invalid);
    const auto* entry=c.Find(key);const auto* token=entry ? std::get_if<ItemCurrencyTokenRow>(&entry->category) : nullptr;
    if(!token || c.CurrencyMode(key)!=CurrencyTokenMode::GroundGold || token->currency!=WalletCurrency::Gold || token->amountPerUnit!=1)return std::unexpected(MoneyError::WrongKind);
    if(entry->base.retired)return std::unexpected(MoneyError::Unavailable);
    if(amount>cap-wallet.balance)return std::unexpected(MoneyError::WalletFull);
    ItemCommit out;out.operationId=operation;out.catalogRevision=c.Revision();out.catalogHash=c.ContentHash();out.kind=ItemOperationKind::Service;out.owners={fence};out.outcomeCode=GroundCurrencyCreditOutcome;
    auto after=wallet;after.balance+=amount;++after.revision;out.wallets.push_back({wallet,after});out.result={'Y','N','G',1};Put(out.result,static_cast<unsigned>(key.Category()),2);Put(out.result,key.LocalId(),4);Put(out.result,amount,8);Put(out.result,cap,8);
    if(!ValidateItemCommit(c,out))return std::unexpected(MoneyError::Invalid);return out;
}
bool ValidGroundCurrencyCredit(const ItemCatalog& c,const ItemCommit& out){
    const auto& b=out.result;
    if(out.outcomeCode!=GroundCurrencyCreditOutcome || out.kind!=ItemOperationKind::Service || out.owners.size()!=1 || out.wallets.size()!=1 || !std::ranges::all_of(out.items,MoneyPassiveAccessoryDebit) || out.recovery || out.ground || out.learning || out.petStudyCooldown || b.size()!=26 || b[0]!='Y' || b[1]!='N' || b[2]!='G' || b[3]!=1)return false;
    const auto key=ItemKey::Create(static_cast<std::uint16_t>(Get(b,4,2)),static_cast<std::uint32_t>(Get(b,6,4)));if(!key || c.CurrencyMode(*key)!=CurrencyTokenMode::GroundGold)return false;
    const auto* entry=c.Find(*key);const auto* token=entry ? std::get_if<ItemCurrencyTokenRow>(&entry->category) : nullptr;
    if(!token || entry->base.retired || token->currency!=WalletCurrency::Gold || token->amountPerUnit!=1)return false;
    const auto amount=Get(b,10,8),cap=Get(b,18,8);const auto& change=out.wallets[0];const auto& before=change.before;const auto& after=change.after;
    return amount && cap && cap<=INT64_MAX && before.owner==out.owners[0].owner && before.owner.kind==ItemOwnerKind::Character && before.currency==WalletCurrency::Gold && before.owner==after.owner && before.currency==after.currency && before.revision && before.revision!=UINT64_MAX && after.revision==before.revision+1 && before.balance<=cap && amount<=cap-before.balance && after.balance==before.balance+amount;
}
std::optional<std::string> NormalizeCurrencyAwardLogin(std::string_view login){
    while(!login.empty() && login.front()==' ')login.remove_prefix(1);while(!login.empty() && login.back()==' ')login.remove_suffix(1);
    if(login.empty() || login.size()>32)return {};std::string result;
    for(unsigned char ch:login){if(ch<0x21 || ch>0x7e)return {};result.push_back(ch>='A' && ch<='Z' ? static_cast<char>(ch-'A'+'a') : static_cast<char>(ch));}return result;
}
std::optional<std::uint64_t> CurrencyUnitAmount(const ItemCatalog& c,const PersistentItem& item){
    const auto* entry=c.Find(item.key);const auto* token=entry ? std::get_if<ItemCurrencyTokenRow>(&entry->category) : nullptr;if(!token)return {};
    const auto mode=c.CurrencyMode(item.key);
    if(mode==CurrencyTokenMode::Fixed)return token->amountPerUnit;
    if(mode==CurrencyTokenMode::DynamicGold && item.currencyAmount && *item.currencyAmount && *item.currencyAmount<=MaxTokenUnitAmount)return item.currencyAmount;
    return {};
}
bool ValidCurrencyAwardClaim(const ItemCatalog& c,const ItemCommit& out){
    if(!out.currencyAward)return true;const auto& claim=*out.currencyAward;
    const auto login=NormalizeCurrencyAwardLogin(claim.login);
    if(!login || *login!=claim.login || !claim.id || claim.id>INT64_MAX || !claim.amount || claim.amount>MaxTokenUnitAmount || out.kind!=ItemOperationKind::Grant || out.outcomeCode!=1 || out.owners.size()!=1 || out.owners[0].owner.kind!=ItemOwnerKind::Account || out.items.size()!=1 || out.items[0].before || !out.wallets.empty() || out.recovery || out.ground || out.learning || out.petStudyCooldown)return false;
    const auto& item=out.items[0].after;
    return c.CurrencyMode(item.key)==CurrencyTokenMode::DynamicGold && item.count==1 && item.owner==out.owners[0].owner && item.placement && item.placement->kind==ItemStorageKind::Safebox && item.currencyAmount==claim.amount && out.result.size()==10 && out.result[0]==static_cast<unsigned>(ItemOperationKind::Grant) && out.result[1]==3 && Get(out.result,2,8)==claim.amount;
}
bool ValidCurrencyDropIntent(const CurrencyDropIntent& d){
    return std::ranges::any_of(d.operation,[](auto b){return b!=0;}) && !d.core.empty() && d.core.size()<=64 && std::ranges::all_of(d.core,[](unsigned char ch){return ch>=0x21 && ch<=0x7e;}) && d.run && d.run<=INT64_MAX && d.owner && d.owner<=UINT32_MAX && d.amount && d.amount<=INT64_MAX && d.map>0 && d.createdAtMs && d.createdAtMs<=INT64_MAX && d.expiresAtMs>d.createdAtMs && d.expiresAtMs-d.createdAtMs<=300000 && d.expiresAtMs<=INT64_MAX && d.nowAtMs>=d.createdAtMs && d.nowAtMs<d.expiresAtMs && (!d.create || d.nowAtMs==d.createdAtMs);
}
std::expected<ItemCommit,MoneyError> MakeCurrencyDropCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operation,ItemOwnerFence fence,const ItemWallet& wallet,CurrencyDropIntent intent){
    if(!c.CurrencyKey(CurrencyTokenMode::GroundGold))return std::unexpected(MoneyError::Unavailable);
    if(!intent.create || intent.operation!=operation || !ValidCurrencyDropIntent(intent) || fence.owner.kind!=ItemOwnerKind::Character || fence.owner.id!=intent.owner || wallet.owner!=fence.owner || wallet.currency!=WalletCurrency::Gold || !wallet.revision || wallet.revision==UINT64_MAX || wallet.balance>INT64_MAX || wallet.balance<intent.amount)return std::unexpected(MoneyError::Invalid);
    ItemCommit out;out.operationId=operation;out.catalogRevision=c.Revision();out.catalogHash=c.ContentHash();out.kind=ItemOperationKind::Service;out.owners={fence};out.outcomeCode=GroundCurrencyDropOutcome;out.currencyDrop=intent;
    auto after=wallet;after.balance-=intent.amount;++after.revision;out.wallets.push_back({wallet,after});out.result={'D','Y','G',1};Put(out.result,intent.amount,8);
    if(!ValidateItemCommit(c,out))return std::unexpected(MoneyError::Invalid);return out;
}
bool ValidCurrencyDropWrite(const ItemCatalog& c,const ItemCommit& out){
    if(!out.currencyDrop)return out.outcomeCode!=GroundCurrencyDropOutcome;
    const auto& d=*out.currencyDrop;if(!ValidCurrencyDropIntent(d) || out.owners.size()!=1 || out.owners[0].owner.kind!=ItemOwnerKind::Character || out.wallets.size()!=1 || !std::ranges::all_of(out.items,MoneyPassiveAccessoryDebit) || out.currencyAward || out.recovery || out.ground || out.learning || out.petStudyCooldown || !c.CurrencyKey(CurrencyTokenMode::GroundGold))return false;
    if(!d.create)return out.outcomeCode==GroundCurrencyCreditOutcome && out.result.size()==26 && Get(out.result,10,8)==d.amount;
    if(out.outcomeCode!=GroundCurrencyDropOutcome || out.kind!=ItemOperationKind::Service || d.operation!=out.operationId || d.owner!=out.owners[0].owner.id || out.result.size()!=12 || out.result[0]!='D' || out.result[1]!='Y' || out.result[2]!='G' || out.result[3]!=1 || Get(out.result,4,8)!=d.amount)return false;
    const auto& before=out.wallets[0].before;auto after=before;if(before.owner!=out.owners[0].owner || before.currency!=WalletCurrency::Gold || before.balance>INT64_MAX || before.balance<d.amount || !before.revision || before.revision==UINT64_MAX)return false;after.balance-=d.amount;++after.revision;return after==out.wallets[0].after;
}
}
