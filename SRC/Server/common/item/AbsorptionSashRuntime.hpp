#pragma once
#include "PersistentItems.hpp"
namespace auriga::item {
inline constexpr std::uint16_t AbsorptionSashOutcome=0x2902;
enum class AbsorptionSashOperation : std::uint8_t { Combine=1,Absorb=2,Clear=3 };
struct AbsorptionSashActor {
    EquipContext equipment;
    bool admitted=false,alive=false,observer=false,polymorphed=false;
};
struct AbsorptionSashQuote {
    AbsorptionSashOperation operation{};
    ItemInstanceId source,target;
    std::optional<ItemKey> resultKey;
    std::uint8_t grade=0,currentPercent=0,minimumPercent=0,maximumPercent=0;
    std::uint16_t successBp=0;
    std::uint64_t goldCost=0;
    AbsorptionSashState previewMinimum,previewMaximum;
};
struct AbsorptionSashDecision {
    std::uint16_t successRoll=0; // Inclusive 1..10000, only for combination.
    std::uint8_t resultPercent=0;
    std::optional<ItemInstanceId> newInstance;
};
struct AbsorptionSashWrite {
    ItemCommit commit;
    AbsorptionSashQuote quote;
    bool success=false;
    std::optional<ItemInstanceId> resultInstance;
};
std::expected<AbsorptionSashQuote,ItemPersistenceError> PrepareAbsorptionSash(const ItemCatalog&,ItemOwner,
    std::span<const PersistentItem>,std::span<const ItemWallet>,ItemInstanceId,ItemInstanceId,
    AbsorptionSashOperation,const AbsorptionSashActor&);
std::expected<AbsorptionSashWrite,ItemPersistenceError> MakeAbsorptionSashCommit(const ItemCatalog&,
    std::array<std::uint8_t,16>,ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,
    std::span<const ItemWallet>,ItemInstanceId,ItemInstanceId,AbsorptionSashOperation,
    const AbsorptionSashActor&,const AbsorptionSashDecision&);
bool ValidAbsorptionSashWrite(const ItemCatalog&,const ItemCommit&);
}
