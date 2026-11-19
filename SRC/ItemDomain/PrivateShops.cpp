#include <item/PrivateShops.hpp>
#include <algorithm>
#include <set>
namespace auriga::item {
namespace {
using E=PrivateShopError;
bool Operation(const std::array<std::uint8_t,16>& op){return std::ranges::any_of(op,[](auto v){return v!=0;});}
const ItemOwnerFence* Fence(const ItemCommit& r,std::uint64_t pid){for(const auto& f:r.owners)if(f.owner==ItemOwner{ItemOwnerKind::Character,pid})return &f;return nullptr;}
bool Extra(const ItemChange& ch){return MoneyPassiveAccessoryDebit(ch);}
std::uint64_t Net(std::uint64_t price,std::uint8_t tax){return price-((price/100)*tax+(price%100)*tax/100);}
void Layouts(ItemCommit& r,std::span<const ItemStorageLayout> all){
 for(const auto& l:all)if(std::ranges::any_of(r.items,[&](const auto& ch){const auto matches=[&](const auto& p){return p.placement && l.storage==ItemStorage{p.owner,p.placement->kind,p.placement->storageId};};return matches(ch.after) || ch.before && matches(*ch.before);}))r.storages.push_back(l);
}
ItemCommit Base(const ItemCatalog& c,std::array<std::uint8_t,16> op,PrivateShopMutation mutation,std::vector<ItemOwnerFence> owners){
 ItemCommit r;r.operationId=op;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=mutation.kind==PrivateShopMutationKind::Buy?ItemOperationKind::Trade:ItemOperationKind::Service;r.owners=std::move(owners);r.outcomeCode=PrivateShopOpenOutcome+static_cast<unsigned>(mutation.kind)-1;r.result={'P','S',static_cast<std::uint8_t>(mutation.kind),1};r.privateShop=std::move(mutation);return r;
}
std::expected<ItemCommit,E> Finish(const ItemCatalog& c,ItemCommit r,std::span<const ItemStorageLayout> all){Layouts(r,all);if(!ValidateItemCommit(c,r))return std::unexpected(E::Invalid);return r;}
bool UniqueItems(std::span<const PersistentItem> items){std::set<ItemInstanceId> ids;return std::ranges::all_of(items,[&](const auto& i){return ids.insert(i.id).second;});}
const ItemWallet* Wallet(std::span<const ItemWallet> all,ItemOwner owner){const ItemWallet* result=nullptr;for(const auto& w:all)if(w.owner==owner && w.currency==WalletCurrency::Gold){if(result)return nullptr;result=&w;}return result;}
bool ValidGold(const ItemWallet& w,ItemOwner owner,std::uint64_t cap){return w.owner==owner && w.currency==WalletCurrency::Gold && w.revision && w.revision!=UINT64_MAX && cap && cap<=INT64_MAX && w.balance<=cap;}
std::optional<ItemPlacement> FreePlace(const ItemCatalog& c,const PersistentItem& incoming,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> all){
 for(const auto& bag:layouts){if(bag.storage.owner!=incoming.owner || bag.storage.kind!=ItemStorageKind::Inventory)continue;std::set<std::uint32_t> used;
  for(const auto& item:all)if(!item.destroyed && item.owner==incoming.owner && item.placement && item.placement->kind==bag.storage.kind && item.placement->storageId==bag.storage.id){const auto cells=OccupiedItemCells(c,item,bag);if(!cells)return {};used.insert(cells->begin(),cells->end());}
  for(std::uint32_t at=0;at<std::uint64_t(bag.columns)*bag.rows*bag.pages;++at){auto candidate=incoming;candidate.placement=ItemPlacement{bag.storage.kind,bag.storage.id,at};const auto cells=OccupiedItemCells(c,candidate,bag);if(cells && std::ranges::none_of(*cells,[&](auto cell){return used.contains(cell);}))return candidate.placement;}
 }return {};
}
}
PrivateShopPermitKind PrivateShopPermit(const ItemCatalog& c,ItemKey key){const auto* e=c.Find(key);const auto* q=e?std::get_if<ItemQuestRow>(&e->category):nullptr;if(!q || q->dispatch!=QuestItemDispatch::FirstHandler)return PrivateShopPermitKind::None;
 switch(BuiltinQuestHandler(q->groupCode)){case BuiltinQuestItem::PrivateShopBundle:return PrivateShopPermitKind::Consumed;case BuiltinQuestItem::PrivateShopSilk:return PrivateShopPermitKind::Silk;default:return PrivateShopPermitKind::None;}}
std::uint64_t PrivateShopPermitDuration(const ItemCatalog& c,ItemKey key){const auto* r=c.FindDefinitionRules(key);return PrivateShopPermit(c,key)==PrivateShopPermitKind::Silk && r && r->clock==CostumeClock::SinceCreation?r->durationMs:0;}
bool ValidPrivateShopSign(std::string_view text){
 if(text.empty() || text.size()>32 || std::ranges::all_of(text,[](char ch){return ch==' ';}))return false;
 for(std::size_t at=0;at<text.size();){const auto lead=static_cast<unsigned char>(text[at++]);std::uint32_t value=lead;unsigned more=0;
  if(lead>=0xc2 && lead<=0xdf){value=lead&31;more=1;}else if(lead>=0xe0 && lead<=0xef){value=lead&15;more=2;}else if(lead>=0xf0 && lead<=0xf4){value=lead&7;more=3;}else if(lead>=0x80)return false;
  if(at+more>text.size())return false;for(unsigned n=0;n<more;++n){const auto next=static_cast<unsigned char>(text[at++]);if((next&0xc0)!=0x80)return false;value=(value<<6)|(next&63);}
  if((more==1 && value<0x80) || (more==2 && value<0x800) || (more==3 && value<0x10000) || value>0x10ffff || (value>=0xd800 && value<=0xdfff) || value<32 || (value>=127 && value<=159) || value==0x2028 || value==0x2029)return false;
 }return true;
}
bool ValidPrivateShopHeader(const PrivateShopHeader& h){return h.owner && h.owner<=UINT32_MAX && h.session && h.createdAtMs && h.createdAtMs<=INT64_MAX && Operation(h.operation) && ValidPrivateShopSign(h.sign);}
bool ValidPrivateShopListing(const PrivateShopListing& s){return Operation(s.shop) && s.price && s.price<=INT64_MAX && s.cell<MaxPrivateShopListings;}
std::optional<std::vector<std::uint16_t>> PrivateShopDisplayCells(const ItemCatalog& c,ItemKey key,std::uint16_t cell){const auto* e=c.Find(key);if(!e || cell>=MaxPrivateShopListings || cell%40%5+e->base.width>5 || cell%40/5+e->base.height>8)return {};std::vector<std::uint16_t> out;for(unsigned y=0;y<e->base.height;++y)for(unsigned x=0;x<e->base.width;++x)out.push_back(static_cast<std::uint16_t>(cell+y*5+x));return out;}
std::uint64_t PrivateShopItemDeadline(const PersistentItem& item){std::uint64_t deadline=0;const auto add=[&](std::uint64_t value){if(value && (!deadline || value<deadline))deadline=value;};if(item.costume)add(item.costume->deadlineUtcMs);if(item.companion)add(item.companion->lifetime.deadlineUtcMs);if(item.petSupply)add(item.petSupply->lifetime.deadlineUtcMs);return deadline;}
bool PrivateShopListable(const ItemCatalog& c,const PersistentItem& item,ItemOwner owner,std::uint64_t now,bool allowListed){
 const auto* e=c.Find(item.key);const auto* policy=e?c.FindTradePolicy(e->base.tradePolicy):nullptr;
 return e && !e->base.retired && policy && policy->allowTrade && item.owner==owner && owner.kind==ItemOwnerKind::Character && !item.boundTo && !item.destroyed && item.count && item.placement && item.placement->kind==ItemStorageKind::Inventory && (allowListed || !item.privateShop) && PrivateShopPermit(c,item.key)==PrivateShopPermitKind::None &&
  (!item.automatic || !item.automatic->active) && (!item.toggle || !item.toggle->active) && (!item.soul || !item.soul->active) && !RecallRequested(item) && !item.growthPet && (!item.petSupply || (!item.petSupply->containedSeal && !item.petSupply->equippedSeal)) && (!item.costume || !item.costume->expired) && (!item.companion || !item.companion->lifetime.expired) && (!item.petSupply || !item.petSupply->lifetime.expired) && (!PrivateShopItemDeadline(item) || PrivateShopItemDeadline(item)>now);
}
std::vector<std::uint8_t> EncodePrivateShopMutation(const PrivateShopMutation& m){std::vector<std::uint8_t> b{'P','S','M',1};const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(static_cast<std::uint8_t>(v>>(i*8)));};put(static_cast<unsigned>(m.kind),1);put(m.header.owner,8);put(m.header.session,8);put(m.header.createdAtMs,8);for(auto ch:m.header.operation)b.push_back(ch);put(m.header.sign.size(),1);b.insert(b.end(),m.header.sign.begin(),m.header.sign.end());put(m.header.rememberPrices,1);for(auto v:{m.subject,m.nowAtMs,m.cap,m.goldBalance,m.goldRevision})put(v,8);put(m.taxPercent,1);return b;}
bool ValidPrivateShopWrite(const ItemCatalog& c,const ItemCommit& r){
 if(!r.privateShop)return r.outcomeCode<PrivateShopOpenOutcome || r.outcomeCode>PrivateShopPermitExpiryOutcome;const auto& m=*r.privateShop;
 if(m.kind<PrivateShopMutationKind::Open || m.kind>PrivateShopMutationKind::ExpirePermit || r.outcomeCode!=PrivateShopOpenOutcome+static_cast<unsigned>(m.kind)-1 || r.result!=std::vector<std::uint8_t>{'P','S',static_cast<std::uint8_t>(m.kind),1} || r.kind!=(m.kind==PrivateShopMutationKind::Buy?ItemOperationKind::Trade:ItemOperationKind::Service) || r.currencyAward || r.currencyDrop || r.ground || r.recovery || r.learning || r.petStudyCooldown)return false;
 if(m.kind==PrivateShopMutationKind::ExpirePermit){
  if(m.header!=PrivateShopHeader{} || m.cap || m.goldBalance || m.goldRevision || m.taxPercent || !m.subject || !m.nowAtMs || m.nowAtMs>INT64_MAX || r.owners.size()!=1 || !r.wallets.empty())return false;unsigned found=0;
  for(const auto& ch:r.items){if(ch.after.id.Value()!=m.subject){if(!Extra(ch))return false;continue;}++found;if(!ch.before || ch.before->privateShop || PrivateShopPermit(c,ch.before->key)!=PrivateShopPermitKind::Silk || !ch.before->shopPermitDeadline || *ch.before->shopPermitDeadline>m.nowAtMs || ch.before->destroyed || ch.before->owner!=r.owners[0].owner)return false;auto expected=*ch.before;++expected.revision;expected.destroyed=true;expected.count=0;expected.placement.reset();if(expected!=ch.after)return false;}return found==1;
 }
 const auto* seller=Fence(r,m.header.owner);if(!ValidPrivateShopHeader(m.header) || !seller || (m.kind==PrivateShopMutationKind::Close?seller->sessionGeneration<m.header.session:seller->sessionGeneration!=m.header.session))return false;
 if(m.kind==PrivateShopMutationKind::Close){if(r.owners.size()!=1 || !r.wallets.empty() || m.subject || m.nowAtMs || m.cap || m.goldBalance || m.goldRevision || m.taxPercent)return false;
  for(const auto& ch:r.items){if(!ch.before || !ch.before->privateShop){if(!Extra(ch))return false;continue;}if(ch.before->owner!=seller->owner || ch.before->privateShop->shop!=m.header.operation)return false;auto expected=*ch.before;++expected.revision;expected.privateShop.reset();if(expected!=ch.after)return false;}return true;
 }
 if(!m.cap || m.cap>INT64_MAX || !m.goldRevision || m.goldRevision==UINT64_MAX || m.goldBalance>m.cap || !m.nowAtMs || m.nowAtMs>INT64_MAX || !m.subject)return false;
 if(m.kind==PrivateShopMutationKind::Open){
  if(r.owners.size()!=1 || !r.wallets.empty() || m.header.operation!=r.operationId || m.header.createdAtMs!=m.nowAtMs || m.taxPercent)return false;unsigned permits=0,count=0;std::set<std::uint16_t> cells;std::uint64_t total=m.goldBalance;
  for(const auto& ch:r.items){
   if(ch.after.id.Value()==m.subject){++permits;if(!ch.before || !PlainInventoryItem(*ch.before,seller->owner) || ch.before->revision==UINT64_MAX)return false;const auto kind=PrivateShopPermit(c,ch.before->key);auto expected=*ch.before;++expected.revision;
    if(kind==PrivateShopPermitKind::Consumed){if(m.header.rememberPrices)return false;if(!--expected.count){expected.destroyed=true;expected.placement.reset();}}
    else if(kind==PrivateShopPermitKind::Silk){if(!m.header.rememberPrices || !ch.before->shopPermitDeadline || *ch.before->shopPermitDeadline<=m.nowAtMs)return false;}else return false;
    if(expected!=ch.after)return false;continue;
   }
   if(!ch.after.privateShop){if(!Extra(ch))return false;continue;}
   if(!ch.before || !PrivateShopListable(c,*ch.before,seller->owner,m.nowAtMs) || !ValidPrivateShopListing(*ch.after.privateShop) || ch.after.privateShop->shop!=m.header.operation || !cells.insert(ch.after.privateShop->cell).second || ++count>MaxPrivateShopListings || ch.after.privateShop->price>=m.cap-total)return false;
   const auto footprint=PrivateShopDisplayCells(c,ch.after.key,ch.after.privateShop->cell);if(!footprint)return false;cells.erase(ch.after.privateShop->cell);for(auto cell:*footprint)if(!cells.insert(cell).second)return false;
   total+=ch.after.privateShop->price;auto expected=*ch.before;++expected.revision;expected.privateShop=ch.after.privateShop;if(expected!=ch.after)return false;
  }return permits==1 && count>0 && total<m.cap;
 }
 if(r.owners.size()!=2 || m.taxPercent>100 || r.wallets.empty())return false;const auto buyer=std::ranges::find_if(r.owners,[&](const auto& f){return f.owner!=seller->owner;});if(buyer==r.owners.end() || buyer->owner.kind!=ItemOwnerKind::Character)return false;
 const auto changed=std::ranges::find_if(r.items,[&](const auto& ch){return ch.after.id.Value()==m.subject;});if(changed==r.items.end() || !changed->before || !changed->before->privateShop || changed->before->privateShop->shop!=m.header.operation || !PrivateShopListable(c,*changed->before,seller->owner,m.nowAtMs,true))return false;
 const auto price=changed->before->privateShop->price,net=Net(price,m.taxPercent);if(net>m.cap-m.goldBalance)return false;auto expected=*changed->before;++expected.revision;expected.owner=buyer->owner;expected.placement=changed->after.placement;expected.privateShop.reset();if(expected!=changed->after || !expected.placement || expected.placement->kind!=ItemStorageKind::Inventory)return false;
 bool buyerWallet=false,sellerWallet=false;
 for(const auto& w:r.wallets){auto after=w.before;++after.revision;if(w.before.currency!=WalletCurrency::Gold || !w.before.revision || w.before.revision==UINT64_MAX || w.before.balance>m.cap)return false;
  if(w.before.owner==seller->owner){if(sellerWallet || !net || w.before.balance!=m.goldBalance || w.before.revision!=m.goldRevision)return false;sellerWallet=true;after.balance+=net;}
  else if(w.before.owner==buyer->owner){if(buyerWallet || w.before.balance<price)return false;buyerWallet=true;after.balance-=price;}else return false;if(after!=w.after)return false;
 }return buyerWallet && sellerWallet==(net!=0) && std::ranges::all_of(r.items,[&](const auto& ch){return ch.after.id.Value()==m.subject || Extra(ch);});
}
std::expected<ItemCommit,E> MakePrivateShopOpen(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,const ItemWallet& wallet,ItemInstanceId permitId,std::string_view sign,std::span<const PrivateShopOffer> offers,std::uint64_t now,std::uint64_t cap){
 if(!ValidGold(wallet,fence.owner,cap) || fence.owner.kind!=ItemOwnerKind::Character || !UniqueItems(items) || offers.empty() || offers.size()>MaxPrivateShopListings || !now || now>INT64_MAX || !ValidPrivateShopSign(sign))return std::unexpected(E::Invalid);
 auto permit=std::ranges::find(items,permitId,&PersistentItem::id);if(permit==items.end() || !PlainInventoryItem(*permit,fence.owner))return std::unexpected(E::Denied);const auto originalKind=PrivateShopPermit(c,permit->key);if(originalKind==PrivateShopPermitKind::None)return std::unexpected(E::Denied);
 if(originalKind==PrivateShopPermitKind::Consumed)for(auto it=items.begin();it!=items.end();++it)if(PrivateShopPermit(c,it->key)==PrivateShopPermitKind::Silk && PlainInventoryItem(*it,fence.owner) && it->shopPermitDeadline && *it->shopPermitDeadline>now && (PrivateShopPermit(c,permit->key)!=PrivateShopPermitKind::Silk || it->id<permit->id))permit=it;
 const bool silk=PrivateShopPermit(c,permit->key)==PrivateShopPermitKind::Silk;if(silk && (!permit->shopPermitDeadline || *permit->shopPermitDeadline<=now))return std::unexpected(E::Expired);
 PrivateShopMutation mutation;mutation.kind=PrivateShopMutationKind::Open;mutation.header={fence.owner.id,fence.sessionGeneration,now,op,std::string(sign),silk};mutation.subject=permit->id.Value();mutation.nowAtMs=now;mutation.cap=cap;mutation.goldBalance=wallet.balance;mutation.goldRevision=wallet.revision;auto r=Base(c,op,mutation,{fence});
 auto after=*permit;++after.revision;if(!silk && !--after.count){after.destroyed=true;after.placement.reset();}r.items.push_back({*permit,after});std::set<ItemInstanceId> ids;std::set<std::uint16_t> cells;std::uint64_t total=wallet.balance;
 for(const auto& offer:offers){const auto source=std::ranges::find(items,offer.id,&PersistentItem::id);if(source==items.end() || source==permit || !ids.insert(offer.id).second || !cells.insert(offer.cell).second || !PrivateShopListable(c,*source,fence.owner,now) || !offer.price || offer.price>=cap-total || offer.cell>=MaxPrivateShopListings)return std::unexpected(E::Denied);total+=offer.price;after=*source;++after.revision;after.privateShop=PrivateShopListing{op,offer.price,offer.cell};r.items.push_back({*source,after});}
 return Finish(c,std::move(r),layouts);
}
std::expected<ItemCommit,E> MakePrivateShopBuy(const ItemCatalog& c,std::array<std::uint8_t,16> op,const PrivateShopHeader& header,ItemOwnerFence seller,ItemOwnerFence buyer,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,std::span<const ItemWallet> wallets,ItemInstanceId itemId,std::uint8_t tax,std::uint64_t now,std::uint64_t cap){
 if(seller.owner==buyer.owner || seller.owner!=ItemOwner{ItemOwnerKind::Character,header.owner} || seller.sessionGeneration!=header.session || buyer.owner.kind!=ItemOwnerKind::Character || !UniqueItems(items) || tax>100)return std::unexpected(E::Invalid);
 const auto source=std::ranges::find(items,itemId,&PersistentItem::id);if(source==items.end() || !source->privateShop || source->privateShop->shop!=header.operation || !PrivateShopListable(c,*source,seller.owner,now,true))return std::unexpected(E::Unavailable);
 const auto* from=Wallet(wallets,buyer.owner);const auto* to=Wallet(wallets,seller.owner);if(!from || !to || !ValidGold(*from,buyer.owner,cap) || !ValidGold(*to,seller.owner,cap))return std::unexpected(E::Invalid);
 const auto price=source->privateShop->price,net=Net(price,tax);if(from->balance<price)return std::unexpected(E::NoMoney);if(net>cap-to->balance)return std::unexpected(E::WalletFull);
 auto transferred=*source;++transferred.revision;transferred.owner=buyer.owner;transferred.privateShop.reset();transferred.placement=FreePlace(c,transferred,layouts,items);if(!transferred.placement)return std::unexpected(E::NoRoom);
 PrivateShopMutation mutation;mutation.kind=PrivateShopMutationKind::Buy;mutation.header=header;mutation.subject=itemId.Value();mutation.nowAtMs=now;mutation.cap=cap;mutation.goldBalance=to->balance;mutation.goldRevision=to->revision;mutation.taxPercent=tax;auto r=Base(c,op,mutation,{seller,buyer});r.items.push_back({*source,transferred});
 auto after=*from;++after.revision;after.balance-=price;r.wallets.push_back({*from,after});if(net){after=*to;++after.revision;after.balance+=net;r.wallets.push_back({*to,after});}return Finish(c,std::move(r),layouts);
}
std::expected<ItemCommit,E> MakePrivateShopClose(const ItemCatalog& c,std::array<std::uint8_t,16> op,const PrivateShopHeader& header,ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items){
 PrivateShopMutation mutation;mutation.kind=PrivateShopMutationKind::Close;mutation.header=header;auto r=Base(c,op,mutation,{fence});for(const auto& item:items)if(item.privateShop && item.privateShop->shop==header.operation){auto after=item;++after.revision;after.privateShop.reset();r.items.push_back({item,after});}return Finish(c,std::move(r),layouts);
}
std::expected<ItemCommit,E> MakePrivateShopPermitExpiry(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,ItemInstanceId id,std::uint64_t now){
 const auto source=std::ranges::find(items,id,&PersistentItem::id);if(source==items.end())return std::unexpected(E::Unavailable);PrivateShopMutation mutation;mutation.kind=PrivateShopMutationKind::ExpirePermit;mutation.subject=id.Value();mutation.nowAtMs=now;auto r=Base(c,op,mutation,{fence});auto after=*source;++after.revision;after.destroyed=true;after.count=0;after.placement.reset();r.items.push_back({*source,after});return Finish(c,std::move(r),layouts);
}
}
