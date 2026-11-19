#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace auriga::item {
// Listed instances stay in their original cells, as in the original PC shop.
// Their durable lock prevents every other item writer from spending/moving them.
struct PrivateShopListing {
    std::array<std::uint8_t,16> shop{};
    std::uint64_t price=0;
    std::uint16_t cell=0;
    bool operator==(const PrivateShopListing&) const=default;
};
struct PrivateShopHeader {
    std::uint64_t owner=0,session=0,createdAtMs=0;
    std::array<std::uint8_t,16> operation{};
    std::string sign;
    bool rememberPrices=false;
    bool operator==(const PrivateShopHeader&) const=default;
};
enum class PrivateShopMutationKind:std::uint8_t {Open=1,Buy=2,Close=3,ExpirePermit=4};
struct PrivateShopMutation {
    PrivateShopMutationKind kind{};
    PrivateShopHeader header;
    std::uint64_t subject=0,nowAtMs=0,cap=0,goldBalance=0,goldRevision=0;
    std::uint8_t taxPercent=0;
    bool operator==(const PrivateShopMutation&) const=default;
};
inline constexpr std::size_t MaxPrivateShopListings=120;
inline constexpr std::uint16_t PrivateShopOpenOutcome=0x3501,PrivateShopBuyOutcome=0x3502,
    PrivateShopCloseOutcome=0x3503,PrivateShopPermitExpiryOutcome=0x3504;
}
