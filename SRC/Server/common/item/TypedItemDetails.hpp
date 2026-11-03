#pragma once
#include "TypedInventoryWire.hpp"

namespace auriga::item
{
// GC_TYPED_ITEM_DETAILS (227), v1. Sent immediately before its inventory
// snapshot. All identities are ItemKey/instance IDs; assets are explicit paths.
// LE header:u8,size:u32,version:u8,revision:u64,hash:32;
// definitions:u16 {key,code/name/description/icon/model:text16,width/height:u8,
//   minLevel:u16,classes/sexes/slot/actions:u8};
// states:u16 {key,level:u16,4 stats:i64,bonuses:u8 {id:u32,value:i64}};
// recipes:u16 {id:u32,source:key,level:u16,target:key,level:u16,gold:u64,
//   chance:u16,failure:u8,failureLevel:u16(65535:none),ingredients:u8 {key,level:u16,count:u32}};
// bonus definitions:u8 {id:u32,unit:u8,name:text16}. text16 is UTF-8.
inline constexpr std::uint8_t TypedItemDetailsHeader = 227;
inline constexpr std::uint8_t TypedActionMove = 1, TypedActionEquip = 2,
    TypedActionUse = 4, TypedActionRefine = 8, TypedActionDrop = 16, TypedActionOpen = 32, TypedActionRedeem = 64, TypedActionSocket = 128;
// v2 widens only actions to u16 when an attribute service is present.
inline constexpr std::uint16_t TypedActionAttributes = 256, TypedActionRecovery = 512, TypedActionEffects = 1024, TypedActionBook = 2048;
std::expected<std::vector<std::byte>, TypedInventoryWireError> EncodeTypedItemDetails(
    const ItemCatalog&, std::span<const PersistentItem>, bool ownedDropEnabled = false, std::span<const ItemGrant> extraStates = {});
}
