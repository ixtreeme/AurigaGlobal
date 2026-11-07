#pragma once
#include "PersistentItems.hpp"
#include <functional>

namespace auriga::item {
inline constexpr std::uint16_t SpecialBonusOutcome=0x2901;
enum class SpecialBonusAction : std::uint8_t { Activate=1,Deactivate=2,Service=3,Maintain=4 };
using SpecialBonusRandom=std::function<std::optional<std::uint32_t>(std::uint32_t)>; // 0..inclusive maximum
std::expected<ItemCommit,ItemPersistenceError> MakeSpecialBonusCommit(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId source,std::optional<ItemInstanceId> target,
    SpecialBonusAction,std::uint8_t selection,std::uint64_t elapsed,EquipContext,const SpecialBonusRandom&);
bool ValidSpecialBonusWrite(const ItemCatalog&,const ItemCommit&);
std::uint64_t SpecialBonusElapsed(const ItemCommit&,std::uint64_t id);
}
