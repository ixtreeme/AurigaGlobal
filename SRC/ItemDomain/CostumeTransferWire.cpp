#include <item/CostumeTransferWire.hpp>
#include <algorithm>
namespace auriga::item {
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeCostumeTransferRuntime(const ItemCatalog& c,const CostumeTransferRuntimeView& v,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items){
    using E=TypedInventoryWireError;if(v.status>CostumeTransferStatus::Selecting)return std::unexpected(E::BadValue);
    std::vector<std::byte> preview,details;
    if(v.quote){
        const auto& q=*v.quote;if(!v.nonce || !v.scroll || !v.receiver || !v.donor || v.scroll==v.receiver || v.scroll==v.donor || v.receiver==v.donor || q.scroll!=v.scroll || q.receiver!=v.receiver || q.donor!=v.donor || !q.kind || q.kind>4 || (v.status!=CostumeTransferStatus::Ready && v.status!=CostumeTransferStatus::Applied && v.status!=CostumeTransferStatus::Pending))return std::unexpected(E::BadValue);
        const auto found=std::ranges::find(items,v.receiver,[](const auto& p){return p.id.Value();});if(found==items.end() || found->destroyed)return std::unexpected(E::BadValue);
        auto after=*found;after.attributes=q.attributes;if(q.stole)after.regalia=q.stole;
        const auto inventory=EncodeTypedInventory(c,layouts,{},std::array{after});const auto definitions=EncodeTypedItemDetails(c,std::array{after});if(!inventory || !definitions)return std::unexpected(E::BadValue);preview=*inventory;details=*definitions;
    }else if(v.status==CostumeTransferStatus::Ready || v.status==CostumeTransferStatus::Applied)return std::unexpected(E::BadValue);
    std::vector<std::byte> bytes;const auto put=[&](std::uint64_t x,unsigned n){for(unsigned i=0;i<n;++i)bytes.push_back(static_cast<std::byte>(x>>(i*8)));};
    put(220,1);put(0,4);put(3,1);put(static_cast<unsigned>(v.status),1);put(v.nonce,8);put(v.scroll,8);put(v.receiver,8);put(v.donor,8);put(v.quote ? v.quote->kind : 0,1);
    put(preview.size(),4);bytes.insert(bytes.end(),preview.begin(),preview.end());put(details.size(),4);bytes.insert(bytes.end(),details.begin(),details.end());if(bytes.size()>65535)return std::unexpected(E::BadValue);
    for(unsigned i=0;i<4;++i)bytes[1+i]=static_cast<std::byte>(bytes.size()>>(i*8));return bytes;
}
}
