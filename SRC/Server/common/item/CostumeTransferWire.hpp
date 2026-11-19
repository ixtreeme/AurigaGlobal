#pragma once
#include "CostumeTransfer.hpp"
#include "TypedItemDetails.hpp"
#include "TypedInventoryWire.hpp"
namespace auriga::item {
enum class CostumeTransferStatus : std::uint8_t { Ready=0,Applied,Closed,Pending,Invalid,Denied,Stale,Failed,Selecting };
struct CostumeTransferRuntimeView {
    CostumeTransferStatus status=CostumeTransferStatus::Closed;
    std::uint64_t nonce=0,scroll=0,receiver=0,donor=0;
    const CostumeTransferQuote* quote=nullptr;
};
// GC220 v3: header,size:u32,version,status,nonce:u64,three instance IDs:u64,
// kind:u8,previewSize:u32,GC225 receiver preview,detailsSize:u32,GC227.
// Preview's nonce field in fixed28-byte CG special-bonus operation4 is the
// selected donor identity. Confirmation uses the opaque server quote nonce.
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeCostumeTransferRuntime(const ItemCatalog&,
    const CostumeTransferRuntimeView&,std::span<const ItemStorageLayout>,std::span<const PersistentItem>);
}
