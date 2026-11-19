#pragma once
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>
namespace auriga::item {
inline constexpr std::uint8_t NativeMoneyHeader=216;
inline constexpr std::size_t NativeMoneyWireSize=47;
inline constexpr std::uint8_t NativeMoneyRequestHeader=218;
// The transport validates one further sequence byte, outside the fixed body.
inline constexpr std::size_t NativeMoneyRequestBodySize=18;
struct NativeMoneyRequest {std::uint8_t action=0;std::uint64_t nonce=0,amount=0;};
enum class NativeMoneyStatus:std::uint8_t {Ready,Applied,Closed,Pending,Invalid,Denied,Stale,Failed};
struct NativeMoneySnapshot {
    NativeMoneyStatus status=NativeMoneyStatus::Closed;
    std::uint64_t nonce=0,amount=0,balance=0,revision=0,cap=0;
    bool operator==(const NativeMoneySnapshot&) const=default;
};
enum class NativeMoneyWireError {Invalid,Truncated};
std::expected<std::vector<std::byte>,NativeMoneyWireError> EncodeNativeMoney(const NativeMoneySnapshot&);
std::expected<NativeMoneySnapshot,NativeMoneyWireError> DecodeNativeMoney(std::span<const std::byte>);
std::expected<NativeMoneyRequest,NativeMoneyWireError> DecodeNativeMoneyRequest(std::span<const std::byte>);
}
