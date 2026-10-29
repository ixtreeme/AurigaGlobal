#pragma once
#include <item/ItemCatalog.hpp>
#include <array>
#include <optional>
#include <string_view>

namespace auriga::item::sql
{
// Static schema identifiers only. These are never supplied by a profile/user.
enum class Table : std::uint8_t
{
    Format, Categories, BonusDefinitions, UsePolicies, TradePolicies, Appearances,
    Attacks, AmmoPolicies, Resistances, BonusProfiles, BonusEntries, Refinements,
    RefinementLevels, Items, Weapons, Armors, Accessories, Materials, SocketProfiles,
    SocketGrades, SocketMaterials, Cooldowns, Recoveries, Instant, Reserve, Periodic,
    Consumables, LootProfiles, LootEntries, Containers, ContainerKeys, CurrencyTokens, TimedEffectGroups, TimedEffects, Automatic, ToggleEffectGroups, ToggleEffects, BookProfiles, BookTargets, BookSteps, SkillBooks, RefinementRecipes, RefinementIngredients, QuestItems, SocketStones, Costumes, CostumeVisuals, ToolProfiles, ToolLevels, Tools, FishingItems, PolymorphForms, PolymorphProfiles, PolymorphSkills, PolymorphMasteries, PolymorphItems, Count,
};
struct TableSpec
{
    Table table;
    std::string_view name, columns;
    std::size_t maximumRows;
};
inline constexpr std::size_t TableCount = static_cast<std::size_t>(Table::Count);
inline constexpr std::size_t CapturedByteBudget = 256 * 1024 * 1024;
inline constexpr std::size_t CellBudgetOverhead = 160;
inline constexpr std::uint32_t CatalogSchemaVersion = 17;
inline constexpr std::string_view CanonicalDomain = "auriga.item_catalog.v17";
std::span<const TableSpec, TableCount> Tables() noexcept;
std::size_t ColumnCount(const TableSpec& spec) noexcept;

using Cell = std::optional<std::string>; // SQL NULL remains distinct from ""/"0".
using Row = std::vector<Cell>;
using Rows = std::vector<Row>;
struct Snapshot
{
    std::array<Rows, TableCount> tables;
    Rows& operator[](Table table) noexcept { return tables[static_cast<std::size_t>(table)]; }
    const Rows& operator[](Table table) const noexcept { return tables[static_cast<std::size_t>(table)]; }
};
struct ReadPin
{
    std::uint64_t revision = 0; // Explicit builder input, not a DB release allocator.
    std::optional<ItemCatalogDigest> expectedHash;
};
enum class ReadErrorCode
{
    InvalidPin, RowLimit, ByteBudget, ColumnCount, NullValue, InvalidNumber,
    InvalidText, InvalidKey, RegistryMismatch, FormatMismatch, OrphanRow,
    DuplicateRow, InvalidChild, InvalidCatalog, HashMismatch, SqlFailure,
    IsolationRequired, SnapshotUnconfirmed, ResultShape,
};
struct ReadError
{
    ReadErrorCode code;
    std::optional<Table> table;
    std::size_t row = 0, column = 0;
    std::uint32_t sqlError = 0;
    std::size_t failedStep = static_cast<std::size_t>(-1);
    std::optional<ItemCatalogError> catalogError;
};
// Decode one completed, owned snapshot. No database, registry, mutation or lazy
// lookup. A failure never returns a partial catalogue. Registry rows must match
// the compiled registries exactly, including unused definitions/categories.
std::expected<std::shared_ptr<const ItemCatalog>, ReadError> DecodeSnapshot(Snapshot source, ReadPin pin);
} // namespace auriga::item::sql
