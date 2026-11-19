#pragma once
#include "PersistentItems.hpp"
namespace auriga::item {
inline constexpr std::uint16_t CostumeTransferOutcome=0x2C01;
struct CostumeTransferActor {EquipContext equipment;bool admitted=false,alive=false,nearNpc=false;};
struct CostumeTransferQuote {
    std::uint64_t scroll=0,receiver=0,donor=0;
    std::uint8_t kind=0; // Body1,Hair2,Weapon3,Stole4; weapon subtype does not restrict transfer.
    InstanceAttributes attributes;
    std::optional<RegaliaState> stole;
};
std::expected<CostumeTransferQuote,ItemPersistenceError> PrepareCostumeTransfer(const ItemCatalog&,ItemOwner,
    std::span<const PersistentItem>,ItemInstanceId scroll,ItemInstanceId receiver,ItemInstanceId donor,const CostumeTransferActor&);
std::expected<ItemCommit,ItemPersistenceError> MakeCostumeTransferCommit(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId,ItemInstanceId,ItemInstanceId,const CostumeTransferActor&);
bool ValidCostumeTransferWrite(const ItemCatalog&,const ItemCommit&);
}
