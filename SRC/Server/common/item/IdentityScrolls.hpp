#pragma once

#include "PersistentItems.hpp"

namespace auriga::item
{
inline constexpr std::uint16_t NameScrollOutcome = 0x3B02;
inline constexpr std::uint16_t EmpireScrollOutcome = 0x3B03;
inline constexpr std::uint16_t SexScrollOutcome = 0x3B04;
inline constexpr std::uint16_t GuildMasterScrollOutcome = 0x3B05;
inline constexpr std::uint16_t DungeonCooldownScrollOutcome = 0x3B06;
inline constexpr std::uint16_t LevelResetScrollOutcome = 0x3B07;
inline constexpr std::uint16_t LevelBeforeReset = 66, LevelAfterReset = 65;
inline constexpr std::int32_t LevelResetMap = 41;
inline constexpr std::array LevelResetEquipmentSlots{
    EquipmentSlot::Feet, EquipmentSlot::Wrist, EquipmentSlot::Neck, EquipmentSlot::Ear};

inline constexpr std::array<std::string_view, 17> CooldownScrollDungeons{
    "owl_zone_razor93", "dungeonorchi", "slime_dungeon", "dungeon_spider",
    "catacombe", "dragonlair_zone", "meleylair_zone", "razador_zone", "nemere_zone",
    "temple_ochao", "enchanted_forest", "nightmare_zone_razor93_prepare",
    "dungeonpyramid_razor93", "tritontemple_razor93", "valentine_dungeon",
    "halloween2022", "vikingdungeon_zone"};
inline constexpr std::array<std::string_view, 4> CooldownScrollFlags{"cooldown", "disconnect", "idx", "ch"};

struct DungeonCooldownScrollChange
{
    std::uint8_t dungeon;
    std::array<std::int64_t, CooldownScrollFlags.size()> before{};
};

struct NameScrollChange
{
    std::string_view before;
    std::string_view after;
};

struct EmpireScrollChange
{
    std::uint32_t account;
    std::uint8_t before;
    std::uint8_t after;
};

struct SexScrollChange
{
    std::uint8_t before;
    std::uint8_t after;
};

struct GuildMasterScrollChange
{
    std::uint32_t guild;
    std::uint32_t successor;
};

// The four player classes occupy the same slots in both halves of the race table.
constexpr std::optional<std::uint8_t> OppositeSexRace(std::uint32_t race)
{
    if (race >= 8) return {};
    return static_cast<std::uint8_t>(race ^ 4);
}

std::optional<NameScrollChange> ReadNameScrollChange(const ItemCommit&);
std::optional<EmpireScrollChange> ReadEmpireScrollChange(const ItemCommit&);
std::optional<SexScrollChange> ReadSexScrollChange(const ItemCommit&);
std::optional<GuildMasterScrollChange> ReadGuildMasterScrollChange(const ItemCommit&);
std::optional<DungeonCooldownScrollChange> ReadDungeonCooldownScrollChange(const ItemCommit&);
bool ValidateQuestScrollWrite(const ItemCatalog&, const ItemCommit&);
std::expected<ItemCommit, ItemPersistenceError> MakeNameScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, NameScrollChange);
std::expected<ItemCommit, ItemPersistenceError> MakeEmpireScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, EmpireScrollChange);
std::expected<ItemCommit, ItemPersistenceError> MakeSexScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, SexScrollChange);
std::expected<ItemCommit, ItemPersistenceError> MakeGuildMasterScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, GuildMasterScrollChange);
std::expected<ItemCommit, ItemPersistenceError> MakeDungeonCooldownScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, DungeonCooldownScrollChange);
bool CanPayLevelReset(const BookCharacter&);
std::expected<ItemCommit, ItemPersistenceError> MakeLevelResetScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, const BookLearning&,
    const BookCharacter&, std::uint16_t randomHP, std::uint16_t randomSP);
}
