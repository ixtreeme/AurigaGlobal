#pragma once
#include "ItemKey.hpp"
#include <cstdint>

namespace auriga::item
{
enum class CharacterServiceKind : std::uint8_t { Alignment=1, StatResetCredit=2, HairDye=3 };
struct ItemCharacterServiceRow
{
    ItemKey key;
    CharacterServiceKind kind{};
    std::uint16_t units=0;
    std::uint32_t amount=0,minimum=0,maximum=0;
};
inline bool ValidCharacterService(const ItemCharacterServiceRow& row)
{
    const bool kind=row.kind==CharacterServiceKind::Alignment ||
        (row.kind==CharacterServiceKind::StatResetCredit && row.amount==1 && row.minimum==0 && row.maximum==INT16_MAX) ||
        (row.kind==CharacterServiceKind::HairDye && row.amount>=1 && row.amount<=6 && row.minimum==0 && row.maximum==1001 && row.units==1);
    return row.key.Category()==ItemCategory::Consumable && kind &&
        row.units && row.amount && row.amount<=50000000 && row.minimum<row.maximum && row.maximum<=50000000;
}
}
