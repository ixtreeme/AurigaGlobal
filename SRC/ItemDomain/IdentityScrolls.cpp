#include <item/IdentityScrolls.hpp>
#include <length.h>
#include <algorithm>

namespace auriga::item
{
namespace
{
constexpr std::string_view NameScrollCode = "catalog.10.tincture_of_the_name_2cbd51b0aa94";

bool ValidNames(NameScrollChange names)
{
    const auto valid = [](std::string_view name) {
        return !name.empty() && name.size() <= CHARACTER_NAME_MAX_LEN && name.find('\0') == name.npos;
    };
    return valid(names.before) && valid(names.after) && names.before != names.after;
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

bool ValidateNameScrollWrite(const ItemCatalog& catalog, const ItemCommit& write)
{
    if (write.outcomeCode != NameScrollOutcome) return true;
    if (!ReadNameScrollChange(write) || write.kind != ItemOperationKind::Consume ||
        write.owners.size() != 1 || write.owners.front().owner.kind != ItemOwnerKind::Character ||
        write.owners.front().owner.id > UINT32_MAX || write.items.size() != 1 ||
        !write.wallets.empty() || write.ground || write.recovery || write.learning || write.petStudyCooldown ||
        write.currencyAward || write.nativeAward || write.currencyDrop || write.privateShop || write.dungeonEntry)
        return false;

    const auto& item = write.items.front();
    if (!item.before || !PlainInventoryItem(*item.before, write.owners.front().owner)) return false;
    const auto* definition = catalog.Find(item.before->key);
    if (!definition || definition->base.retired || definition->base.contentCode != NameScrollCode ||
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

    auto write = MakeConsumeCommit(catalog, operation, fence, storages, inventory, source, 1);
    if (!write) return std::unexpected(write.error());
    if (write->items.front().before->revision != revision)
        return std::unexpected(ItemPersistenceError::Conflict);

    write->outcomeCode = NameScrollOutcome;
    write->result = {'N', 'A', 'M', 1, static_cast<std::uint8_t>(names.before.size()), static_cast<std::uint8_t>(names.after.size())};
    write->result.insert(write->result.end(), names.before.begin(), names.before.end());
    write->result.insert(write->result.end(), names.after.begin(), names.after.end());
    if (auto valid = ValidateItemCommit(catalog, *write); !valid) return std::unexpected(valid.error());
    return write;
}
}
