#include <item/PrivateShopWire.hpp>
#include <item/TypedInventoryWire.hpp>
#include <item/TypedItemDetails.hpp>
#include <set>
namespace auriga::item {
namespace {
using E=PrivateShopWireError;
struct Writer {std::vector<std::byte> b;void Put(std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(std::byte((v>>(i*8))&255));}void Text(const std::string& s){for(unsigned char c:s)b.push_back(std::byte(c));}};
bool Valid(const PrivateShopRequest& r){
 if(r.action<1 || r.action>7 || r.sign.size()>32 || r.offers.size()>120)return false;
 if(r.action==2){if(r.nonce || r.target || !r.subject || !ValidPrivateShopSign(r.sign) || r.offers.empty())return false;std::set<ItemInstanceId> ids;std::set<unsigned> cells;for(const auto& o:r.offers)if(!o.price || o.price>INT64_MAX || o.cell>=120 || !ids.insert(o.id).second || !cells.insert(o.cell).second)return false;return true;}
 if(!r.sign.empty() || !r.offers.empty())return false;
 switch(r.action){case 1:return !r.nonce && !r.target && r.subject;case 3:return r.nonce && !r.target && !r.subject;case 4:return !r.nonce && !r.target && !r.subject;case 5:return !r.nonce && r.target && !r.subject;case 6:return !r.nonce && r.target && r.subject;case 7:return !r.nonce && !r.target && !r.subject;default:return false;}
}
}
std::expected<std::vector<std::byte>,E> EncodePrivateShopRequest(const PrivateShopRequest& r){
 if(!Valid(r))return std::unexpected(E::Invalid);Writer w;w.Put(PrivateShopRequestHeader,1);w.Put(26+r.sign.size()+18*r.offers.size(),2);w.Put(r.action,1);w.Put(r.nonce,8);w.Put(r.target,4);w.Put(r.subject,8);w.Put(r.sign.size(),1);w.Text(r.sign);w.Put(r.offers.size(),1);for(const auto& o:r.offers){w.Put(o.id.Value(),8);w.Put(o.price,8);w.Put(o.cell,2);}return w.b;
}
std::expected<PrivateShopRequest,E> DecodePrivateShopRequest(std::span<const std::byte> b){
 if(b.size()<PrivateShopRequestMin || b.size()>PrivateShopRequestMax)return std::unexpected(E::Truncated);std::size_t at=0;
 const auto get=[&](unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(std::to_integer<unsigned>(b[at++]))<<(8*i);return v;};
 if(get(1)!=PrivateShopRequestHeader || get(2)!=b.size())return std::unexpected(E::Invalid);
 PrivateShopRequest r;r.action=get(1);r.nonce=get(8);r.target=static_cast<std::uint32_t>(get(4));r.subject=get(8);const auto size=get(1);if(size>32 || at+size+1>b.size())return std::unexpected(E::Truncated);
 for(unsigned n=0;n<size;++n)r.sign.push_back(static_cast<char>(get(1)));const auto count=get(1);if(count>120 || at+18*count!=b.size())return std::unexpected(E::Invalid);
 for(unsigned n=0;n<count;++n){const auto id=ItemInstanceId::Create(get(8));if(!id)return std::unexpected(E::Invalid);const auto price=get(8);const auto cell=static_cast<std::uint16_t>(get(2));r.offers.push_back({*id,price,cell});}if(!Valid(r))return std::unexpected(E::Invalid);return r;
}
std::expected<std::vector<std::byte>,E> EncodePrivateShopSnapshot(const ItemCatalog& c,const PrivateShopSnapshot& s,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items){
 if(s.view>2 || static_cast<unsigned>(s.status)>12 || s.taxPercent>100 || !s.cap || s.cap>INT64_MAX || s.balance>s.cap || s.sign.size()>32 || (!s.sign.empty() && !ValidPrivateShopSign(s.sign)) || s.eligible.size()>4096 || s.offers.size()>120 || s.prices.size()>4096 || (s.status==PrivateShopStatus::Preview && !s.nonce))return std::unexpected(E::Invalid);
 const auto inv=EncodeTypedInventory(c,layouts,{},items),details=EncodeTypedItemDetails(c,items);if(!inv || !details)return std::unexpected(E::Invalid);
 Writer w;w.Put(PrivateShopHeaderByte,1);w.Put(0,4);w.Put(1,1);w.Put(s.view,1);w.Put(static_cast<unsigned>(s.status),1);
 w.Put(s.nonce,8);w.Put(s.subject,8);w.Put(s.target,4);w.Put(s.owner,4);w.Put(s.balance,8);w.Put(s.revision,8);w.Put(s.cap,8);w.Put(s.nowAtMs,8);w.Put(s.taxPercent,1);w.Put(s.remember,1);w.Put(s.sign.size(),1);w.Text(s.sign);
 std::set<std::uint64_t> ids;w.Put(s.eligible.size(),2);for(auto id:s.eligible){if(!id || !ids.insert(id).second)return std::unexpected(E::Invalid);w.Put(id,8);}
 ids.clear();w.Put(s.offers.size(),1);for(const auto& o:s.offers){if(!o.price || o.price>s.cap || o.cell>=120 || !ids.insert(o.id.Value()).second)return std::unexpected(E::Invalid);w.Put(o.id.Value(),8);w.Put(o.price,8);w.Put(o.cell,2);}
 w.Put(s.prices.size(),2);for(const auto& p:s.prices){if(!c.Find(p.key) || p.level>255 || p.unit>INT64_MAX)return std::unexpected(E::Invalid);w.Put(static_cast<unsigned>(p.key.Category()),2);w.Put(p.key.LocalId(),4);w.Put(p.level,2);w.Put(p.unit,8);}
 for(const auto* frame:{&*inv,&*details}){w.Put(frame->size(),4);w.b.insert(w.b.end(),frame->begin(),frame->end());}if(w.b.size()>4*1024*1024)return std::unexpected(E::TooLarge);
 for(unsigned n=0;n<4;++n)w.b[1+n]=std::byte((w.b.size()>>(8*n))&255);return w.b;
}
}
