#pragma once
#include "PersistentItems.hpp"
namespace auriga::item {
inline constexpr std::uint16_t ForgeOutcome=0x3201;
struct ForgeContext {EquipContext actor;bool admitted=false,alive=true,npcInRange=false;};
struct ForgeSelection {ItemInstanceId target;std::uint64_t scroll=0;std::array<std::uint64_t,3> boosters{};std::uint32_t recipe=0;};
struct ForgeTerms {std::uint16_t chanceBp=0;RefinementFailure failure=RefinementFailure::Keep;std::uint16_t failureLevel=0,targetLevel=0;std::uint64_t gold=0;bool operator==(const ForgeTerms&) const=default;};
struct ForgeCommit {ItemCommit commit;ForgeTerms terms;bool success=false;};
// No roll is an admission/quote probe. Real attempts pass the server's exact roll.
std::expected<ForgeCommit,ItemPersistenceError> MakeForgeCommit(const ItemCatalog&,std::array<std::uint8_t,16>,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,const ItemWallet&,
    const ForgeSelection&,ForgeContext,std::optional<std::uint16_t> roll={});
bool ValidForgeWrite(const ItemCatalog&,const ItemCommit&);
}
