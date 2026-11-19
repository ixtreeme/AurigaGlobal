#pragma once
#include "ItemKey.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace auriga::item {
// External SQL reservations preserve the complete concrete item and its occupied
// cells until the party's final debit. No escrow or reconstructed refund item.
struct DungeonEntryTicket {
 ItemKey key;std::uint16_t level=0;std::uint64_t owner=0,session=0,instance=0,revision=0,createdAtMs=0;
 std::uint32_t count=0,units=0;std::array<std::uint8_t,32> stateHash{};
 bool operator==(const DungeonEntryTicket&) const=default;
};
enum class DungeonEntryMutationKind:std::uint8_t {Reserve=1,Consume=2,Release=3};
struct DungeonEntryMutation {
 DungeonEntryMutationKind kind=DungeonEntryMutationKind::Reserve;
 std::array<std::uint8_t,16> reservation{};std::string gate;std::vector<DungeonEntryTicket> tickets;
};
inline constexpr std::uint16_t DungeonEntryReserveOutcome=0x3601,DungeonEntryConsumeOutcome=0x3602,DungeonEntryReleaseOutcome=0x3603;
}
