#pragma once
#include "PersistentItems.hpp"

namespace auriga::item {
inline constexpr std::uint16_t SoulOutcome=0x3001;
struct SoulContext {
    EquipContext actor;
    bool alive=true,polymorphed=false,targetIsNpc=false;
    SoulChannel channel=SoulChannel::NormalAttack;
    std::uint64_t event=0,target=0,targetRevision=0;
    std::int64_t damage=0;
};
struct SoulCommit {ItemCommit commit;std::int64_t damage=0;bool success=false;};
bool ValidSoulState(const ItemCatalog&,ItemKey,std::uint16_t,const SoulState&);
std::vector<std::uint8_t> EncodeSoulState(const SoulState&);
std::optional<SoulState> DecodeSoulState(std::span<const std::uint8_t>);
std::expected<SoulCommit,ItemPersistenceError> MakeSoulCommit(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId core,std::optional<ItemInstanceId> scroll,
    SoulAction,std::uint64_t elapsed,std::uint16_t roll,SoulContext);
bool ValidSoulWrite(const ItemCatalog&,const ItemCommit&);
std::uint64_t SoulElapsed(const ItemCommit&,std::uint64_t);
}
