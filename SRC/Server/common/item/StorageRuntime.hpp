#pragma once
#include "PersistentItems.hpp"
#include "TypedInventoryWire.hpp"
namespace auriga::item {
inline constexpr std::uint16_t StorageOutcome=0x3301,AccountWalletOutcome=0x3302;
enum class StorageRoute:std::uint8_t {Bank=1,Mall=2,Guild=3,Mail=4,Awards=5,Wallet=6};
enum class StorageCommand:std::uint8_t {Deposit=1,Withdraw=2,Rearrange=3,Send=4,Redeem=5,Mint=6};
struct StorageActor {
    std::uint32_t pid=0,account=0,guild=0;
    EquipContext use;
    bool admitted=false,bankInRange=false,guildMember=false,guildLeader=false;
};
struct StorageChoice {
    StorageRoute route=StorageRoute::Bank;StorageCommand command=StorageCommand::Deposit;
    std::uint64_t subject=0;std::uint32_t units=0,cell=UINT32_MAX,recipient=0;
    bool operator==(const StorageChoice&)const=default;
};
struct StoragePlan {ItemCommit commit;std::uint64_t amount=0;std::uint32_t units=0;};
// Every shared-owner write includes the authenticated character's session fence.
// Shared account/guild generations are not claimed by individual character logins.
std::expected<StoragePlan,ItemPersistenceError> MakeStorageCommit(const ItemCatalog&,std::array<std::uint8_t,16>,
    std::span<const ItemOwnerFence>,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,
    std::span<const ItemWallet>,StorageActor,StorageChoice,std::span<const ItemInstanceId> reservedIds={});
bool ValidStorageWrite(const ItemCatalog&,const ItemCommit&);
struct StorageAuthorization {StorageActor actor;StorageChoice choice;};
std::optional<StorageAuthorization> StorageWriteAuthorization(const ItemCommit&);
// Native nested inventory uses an inventory projection; actual owner/storage/anchor
// remain in the outer packet. No projected state is ever accepted for a write.
struct StorageSnapshot {
    StorageActor actor;StorageRoute route=StorageRoute::Bank;std::uint8_t status=0,command=0;
    std::uint64_t nonce=0,subject=0,amount=0;std::uint32_t units=0,cell=UINT32_MAX,recipient=0;
    ItemOwner owner;std::vector<ItemStorageLayout> layouts;std::vector<PersistentItem> items;
    std::uint64_t coins=0,walletRevision=0;bool canDeposit=false,canWithdraw=false;
    std::string message;
};
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeStorageSnapshot(const ItemCatalog&,const StorageSnapshot&);
}
