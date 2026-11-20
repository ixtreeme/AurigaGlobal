#include <item/IdentityScrolls.hpp>
#include <length.h>
#include <algorithm>
#include <bit>

namespace auriga::item
{
namespace
{
constexpr std::string_view NameScrollCode = "catalog.10.tincture_of_the_name_2cbd51b0aa94";
constexpr std::string_view EmpireScrollCode = "catalog.10.tincture_of_kingdoms_83e4f6b6bb83";
constexpr std::string_view SexScrollCode = "catalog.10.modification_charm_679127259da2";
constexpr std::string_view GuildMasterScrollCode = "catalog.10.ring_of_successor_3b1ff932cf1c";
constexpr std::string_view DungeonCooldownScrollCode = "catalog.10.dungeon_cd_reset_0f8f1b52bcef";
constexpr std::string_view LevelResetScrollCode = "catalog.10.level_reset_66_to_65_c0f8a197800f";

bool ValidNames(NameScrollChange names)
{
    const auto valid = [](std::string_view name) {
        return !name.empty() && name.size() <= CHARACTER_NAME_MAX_LEN && name.find('\0') == name.npos;
    };
    return valid(names.before) && valid(names.after) && names.before != names.after;
}

bool ValidEmpires(EmpireScrollChange change)
{
    return change.account && change.before >= 1 && change.before <= 3 &&
        change.after >= 1 && change.after <= 3 && change.before != change.after;
}

std::expected<ItemCommit, ItemPersistenceError> MakeScrollCommit(const ItemCatalog& catalog,
    std::array<std::uint8_t, 16> operation, ItemOwnerFence fence, std::span<const ItemStorageLayout> storages,
    std::span<const PersistentItem> inventory, ItemInstanceId source, std::uint64_t revision,
    std::uint16_t outcome, std::vector<std::uint8_t> receipt, std::optional<ItemLearningChange> learning = {})
{
    auto write = MakeConsumeCommit(catalog, operation, fence, storages, inventory, source, 1);
    if (!write) return std::unexpected(write.error());
    if (write->items.front().before->revision != revision)
        return std::unexpected(ItemPersistenceError::Conflict);

    write->outcomeCode = outcome;
    write->result = std::move(receipt);
    write->learning = std::move(learning);
    if (auto valid = ValidateItemCommit(catalog, *write); !valid) return std::unexpected(valid.error());
    return write;
}

bool ValidLevelReset(const ItemCommit& write)
{
    const auto& data = write.result;
    if (data.size() != 11 || data[0] != 'L' || data[1] != 'R' || data[2] != 'S' || data[3] != 1 ||
        data[4] > 3 || !write.learning || write.owners.size() != 1)
        return false;

    const auto& change = *write.learning;
    const auto& character = change.after.character;
    if (change.owner != write.owners.front().owner || !character.progression ||
        change.before.chanceTokens != change.after.chanceTokens ||
        change.before.bypassTokens != change.after.bypassTokens || change.before.progress != change.after.progress)
        return false;

    const auto word = [&](std::size_t offset) { return unsigned(data[offset]) | (unsigned(data[offset + 1]) << 8); };
    const auto& progress = *character.progression;
    return progress.level == LevelAfterReset && progress.step == data[4] &&
        unsigned(character.points[0]) + 1 == word(5) && unsigned(character.points[1]) + 1 == word(7) &&
        unsigned(progress.statPoints) + 3 - progress.step == word(9);
}
}

std::optional<NameScrollChange> ReadNameScrollChange(const ItemCommit& write)
{
    const auto& data = write.result;
    if (write.outcomeCode != NameScrollOutcome || data.size() < 6 ||
        data[0] != 'N' || data[1] != 'A' || data[2] != 'M' || data[3] != 1 ||
        data.size() != std::size_t{6} + data[4] + data[5])
        return {};

    const char* names = reinterpret_cast<const char*>(data.data() + 6);
    const NameScrollChange change{{names, data[4]}, {names + data[4], data[5]}};
    if (!ValidNames(change)) return {};
    return change;
}

std::optional<EmpireScrollChange> ReadEmpireScrollChange(const ItemCommit& write)
{
    const auto& data = write.result;
    if (write.outcomeCode != EmpireScrollOutcome || data.size() != 10 ||
        data[0] != 'E' || data[1] != 'M' || data[2] != 'P' || data[3] != 1)
        return {};

    EmpireScrollChange change{0, data[4], data[5]};
    for (unsigned byte = 0; byte < 4; ++byte)
        change.account |= std::uint32_t{data[6 + byte]} << (8 * byte);
    if (!ValidEmpires(change)) return {};
    return change;
}

std::optional<SexScrollChange> ReadSexScrollChange(const ItemCommit& write)
{
    const auto& data = write.result;
    if (write.outcomeCode != SexScrollOutcome || data.size() != 6 ||
        data[0] != 'S' || data[1] != 'E' || data[2] != 'X' || data[3] != 1 ||
        OppositeSexRace(data[4]) != data[5])
        return {};
    return SexScrollChange{data[4], data[5]};
}

std::optional<GuildMasterScrollChange> ReadGuildMasterScrollChange(const ItemCommit& write)
{
    const auto& data = write.result;
    if (write.outcomeCode != GuildMasterScrollOutcome || data.size() != 12 ||
        data[0] != 'G' || data[1] != 'M' || data[2] != 'S' || data[3] != 1)
        return {};

    GuildMasterScrollChange change{};
    for (unsigned byte = 0; byte < 4; ++byte)
    {
        change.guild |= std::uint32_t{data[4 + byte]} << (8 * byte);
        change.successor |= std::uint32_t{data[8 + byte]} << (8 * byte);
    }
    if (!change.guild || change.guild > INT32_MAX || !change.successor) return {};
    return change;
}

std::optional<DungeonCooldownScrollChange> ReadDungeonCooldownScrollChange(const ItemCommit& write)
{
    const auto& data = write.result;
    if (write.outcomeCode != DungeonCooldownScrollOutcome || data.size() != 5 + 8 * CooldownScrollFlags.size() ||
        data[0] != 'D' || data[1] != 'C' || data[2] != 'R' || data[3] != 1 ||
        data[4] >= CooldownScrollDungeons.size())
        return {};

    DungeonCooldownScrollChange change{data[4]};
    for (std::size_t flag = 0; flag < change.before.size(); ++flag)
    {
        std::uint64_t value = 0;
        for (unsigned byte = 0; byte < 8; ++byte)
            value |= std::uint64_t{data[5 + 8 * flag + byte]} << (8 * byte);
        change.before[flag] = std::bit_cast<std::int64_t>(value);
    }
    if (change.before.front() <= 0) return {};
    return change;
}

bool ValidateQuestScrollWrite(const ItemCatalog& catalog, const ItemCommit& write)
{
    std::string_view contentCode;
    switch (write.outcomeCode)
    {
    case NameScrollOutcome:
        if (!ReadNameScrollChange(write)) return false;
        contentCode = NameScrollCode;
        break;
    case EmpireScrollOutcome:
        if (!ReadEmpireScrollChange(write)) return false;
        contentCode = EmpireScrollCode;
        break;
    case SexScrollOutcome:
        if (!ReadSexScrollChange(write)) return false;
        contentCode = SexScrollCode;
        break;
    case GuildMasterScrollOutcome:
    {
        const auto change = ReadGuildMasterScrollChange(write);
        if (!change || write.owners.size() != 1 || change->successor == write.owners.front().owner.id)
            return false;
        contentCode = GuildMasterScrollCode;
        break;
    }
    case DungeonCooldownScrollOutcome:
        if (!ReadDungeonCooldownScrollChange(write)) return false;
        contentCode = DungeonCooldownScrollCode;
        break;
    case LevelResetScrollOutcome:
        if (!ValidLevelReset(write)) return false;
        contentCode = LevelResetScrollCode;
        break;
    default:
        return true;
    }

    if (write.kind != ItemOperationKind::Consume ||
        write.owners.size() != 1 || write.owners.front().owner.kind != ItemOwnerKind::Character ||
        write.owners.front().owner.id > UINT32_MAX || write.items.size() != 1 ||
        !write.wallets.empty() || write.ground || write.recovery || write.petStudyCooldown ||
        (write.learning && write.outcomeCode != LevelResetScrollOutcome) ||
        write.currencyAward || write.nativeAward || write.currencyDrop || write.privateShop || write.dungeonEntry)
        return false;

    const auto& item = write.items.front();
    if (!item.before || !PlainInventoryItem(*item.before, write.owners.front().owner)) return false;
    const auto* definition = catalog.Find(item.before->key);
    if (!definition || definition->base.retired || definition->base.contentCode != contentCode ||
        item.before->refinementLevel || !item.before->count || item.before->revision == UINT64_MAX)
        return false;

    auto consumed = *item.before;
    ++consumed.revision;
    if (--consumed.count == 0)
    {
        consumed.destroyed = true;
        consumed.placement.reset();
    }
    return consumed == item.after;
}

std::expected<ItemCommit, ItemPersistenceError> MakeNameScrollCommit(const ItemCatalog& catalog,
    std::array<std::uint8_t, 16> operation, ItemOwnerFence fence, std::span<const ItemStorageLayout> storages,
    std::span<const PersistentItem> inventory, ItemInstanceId source, std::uint64_t revision, NameScrollChange names)
{
    if (!ValidNames(names)) return std::unexpected(ItemPersistenceError::InvalidChange);
    std::vector<std::uint8_t> receipt{
        'N', 'A', 'M', 1, static_cast<std::uint8_t>(names.before.size()), static_cast<std::uint8_t>(names.after.size())};
    receipt.insert(receipt.end(), names.before.begin(), names.before.end());
    receipt.insert(receipt.end(), names.after.begin(), names.after.end());
    return MakeScrollCommit(catalog, operation, fence, storages, inventory, source, revision,
        NameScrollOutcome, std::move(receipt));
}

std::expected<ItemCommit, ItemPersistenceError> MakeEmpireScrollCommit(const ItemCatalog& catalog,
    std::array<std::uint8_t, 16> operation, ItemOwnerFence fence, std::span<const ItemStorageLayout> storages,
    std::span<const PersistentItem> inventory, ItemInstanceId source, std::uint64_t revision, EmpireScrollChange change)
{
    if (!ValidEmpires(change)) return std::unexpected(ItemPersistenceError::InvalidChange);
    std::vector<std::uint8_t> receipt{'E', 'M', 'P', 1, change.before, change.after};
    for (unsigned byte = 0; byte < 4; ++byte)
        receipt.push_back(static_cast<std::uint8_t>(change.account >> (8 * byte)));
    return MakeScrollCommit(catalog, operation, fence, storages, inventory, source, revision,
        EmpireScrollOutcome, std::move(receipt));
}

std::expected<ItemCommit, ItemPersistenceError> MakeSexScrollCommit(const ItemCatalog& catalog,
    std::array<std::uint8_t, 16> operation, ItemOwnerFence fence, std::span<const ItemStorageLayout> storages,
    std::span<const PersistentItem> inventory, ItemInstanceId source, std::uint64_t revision, SexScrollChange change)
{
    return MakeScrollCommit(catalog, operation, fence, storages, inventory, source, revision,
        SexScrollOutcome, {'S', 'E', 'X', 1, change.before, change.after});
}

std::expected<ItemCommit, ItemPersistenceError> MakeGuildMasterScrollCommit(const ItemCatalog& catalog,
    std::array<std::uint8_t, 16> operation, ItemOwnerFence fence, std::span<const ItemStorageLayout> storages,
    std::span<const PersistentItem> inventory, ItemInstanceId source, std::uint64_t revision, GuildMasterScrollChange change)
{
    std::vector<std::uint8_t> receipt{'G', 'M', 'S', 1};
    for (const auto value : {change.guild, change.successor})
        for (unsigned byte = 0; byte < 4; ++byte)
            receipt.push_back(static_cast<std::uint8_t>(value >> (8 * byte)));
    return MakeScrollCommit(catalog, operation, fence, storages, inventory, source, revision,
        GuildMasterScrollOutcome, std::move(receipt));
}

std::expected<ItemCommit, ItemPersistenceError> MakeDungeonCooldownScrollCommit(const ItemCatalog& catalog,
    std::array<std::uint8_t, 16> operation, ItemOwnerFence fence, std::span<const ItemStorageLayout> storages,
    std::span<const PersistentItem> inventory, ItemInstanceId source, std::uint64_t revision, DungeonCooldownScrollChange change)
{
    std::vector<std::uint8_t> receipt{'D', 'C', 'R', 1, change.dungeon};
    for (const auto value : change.before)
        for (unsigned byte = 0; byte < 8; ++byte)
            receipt.push_back(static_cast<std::uint8_t>(std::uint64_t(value) >> (8 * byte)));
    return MakeScrollCommit(catalog, operation, fence, storages, inventory, source, revision,
        DungeonCooldownScrollOutcome, std::move(receipt));
}

bool CanPayLevelReset(const BookCharacter& live)
{
    if (!live.progression || live.progression->step > 3) return false;
    return live.points[0] >= 1 && live.points[1] >= 1 && live.progression->statPoints >= 3 - live.progression->step;
}

std::expected<ItemCommit, ItemPersistenceError> MakeLevelResetScrollCommit(const ItemCatalog& catalog,
    std::array<std::uint8_t, 16> operation, ItemOwnerFence fence, std::span<const ItemStorageLayout> storages,
    std::span<const PersistentItem> inventory, ItemInstanceId source, std::uint64_t revision, const BookLearning& before,
    const BookCharacter& live, std::uint16_t randomHP, std::uint16_t randomSP)
{
    if (!ValidBookLearning(before) || before.revision == UINT64_MAX || !ValidBookCharacter(live) ||
        !live.progression || live.progression->level != LevelBeforeReset || !CanPayLevelReset(live))
        return std::unexpected(ItemPersistenceError::RequirementNotMet);

    auto after = before;
    ++after.revision;
    after.character = live;
    --after.character.points[0];
    --after.character.points[1];
    auto& progress = *after.character.progression;
    progress.level = LevelAfterReset;
    progress.statPoints -= static_cast<std::uint16_t>(3 - progress.step);
    progress.randomHP = randomHP;
    progress.randomSP = randomSP;

    std::vector<std::uint8_t> receipt{'L', 'R', 'S', 1, static_cast<std::uint8_t>(progress.step)};
    for (const auto value : {live.points[0], live.points[1], live.progression->statPoints})
    {
        receipt.push_back(static_cast<std::uint8_t>(value));
        receipt.push_back(static_cast<std::uint8_t>(value >> 8));
    }
    return MakeScrollCommit(catalog, operation, fence, storages, inventory, source, revision,
        LevelResetScrollOutcome, std::move(receipt), ItemLearningChange{fence.owner, before, std::move(after)});
}
}
