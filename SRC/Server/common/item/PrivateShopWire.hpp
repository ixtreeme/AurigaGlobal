#pragma once
#include "PrivateShops.hpp"
#include <cstddef>
namespace auriga::item {
inline constexpr std::uint8_t PrivateShopRequestHeader=217,PrivateShopHeaderByte=213;
// Unity writers append the legacy sequence placeholder; SendTypedGameAsync
// trims it before TLS. The declared body is exactly the server's wire length.
inline constexpr std::size_t PrivateShopRequestMin=26,PrivateShopRequestMax=26+32+120*18;
struct PrivateShopRequest {std::uint8_t action=0;std::uint64_t nonce=0,subject=0;std::uint32_t target=0;std::string sign;std::vector<PrivateShopOffer> offers;};
enum class PrivateShopStatus:std::uint8_t {Ready,Preview,Applied,Closed,Pending,Denied,Stale,Failed,Invalid,NoMoney,NoRoom,WalletFull,Expired};
struct PrivateShopPriceMemory {ItemKey key;std::uint16_t level=0;std::uint64_t unit=0;};
struct PrivateShopSnapshot {
 std::uint8_t view=0;PrivateShopStatus status=PrivateShopStatus::Closed;
 std::uint64_t nonce=0,subject=0,balance=0,revision=0,cap=0,nowAtMs=0;
 std::uint32_t target=0,owner=0;std::uint8_t taxPercent=0;bool remember=false;std::string sign;
 std::vector<std::uint64_t> eligible;std::vector<PrivateShopOffer> offers;std::vector<PrivateShopPriceMemory> prices;
};
enum class PrivateShopWireError {Invalid,Truncated,TooLarge};
std::expected<PrivateShopRequest,PrivateShopWireError> DecodePrivateShopRequest(std::span<const std::byte>);
std::expected<std::vector<std::byte>,PrivateShopWireError> EncodePrivateShopRequest(const PrivateShopRequest&);
std::expected<std::vector<std::byte>,PrivateShopWireError> EncodePrivateShopSnapshot(const ItemCatalog&,const PrivateShopSnapshot&,
 std::span<const ItemStorageLayout>,std::span<const PersistentItem>);
}
