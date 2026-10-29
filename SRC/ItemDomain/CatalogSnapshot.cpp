#include "CatalogSnapshot.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
#include <type_traits>

namespace auriga::item::sql
{
namespace
{
struct Reader
{
    Table table;
    std::size_t index;
    const Row& row;
    [[noreturn]] void Fail(ReadErrorCode code, std::size_t column = 0) const { throw ReadError{code, table, index, column}; }
    const std::string& Text(std::size_t column, std::size_t limit = 192) const
    {
        if (!row[column]) Fail(ReadErrorCode::NullValue, column);
        const auto& text = *row[column];
        if (text.size() > limit || text.find('\0') != text.npos) Fail(ReadErrorCode::InvalidText, column);
        return text;
    }
    template <typename T> T Number(std::size_t column) const
    {
        const auto& text = Text(column, 20);
        T value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        auto digits = std::string_view(text);
        if constexpr (std::is_signed_v<T>) if (digits.starts_with('-')) digits.remove_prefix(1);
        if (error != std::errc{} || end != text.data() + text.size() || text.empty() ||
            (digits.size() > 1 && digits.front() == '0') || (value == 0 && text.front() == '-'))
            Fail(ReadErrorCode::InvalidNumber, column);
        return value;
    }
    std::uint8_t U8(std::size_t c) const { return Number<std::uint8_t>(c); }
    std::uint16_t U16(std::size_t c) const { return Number<std::uint16_t>(c); }
    std::uint32_t U32(std::size_t c) const { return Number<std::uint32_t>(c); }
    std::uint64_t U64(std::size_t c) const { return Number<std::uint64_t>(c); }
    std::int64_t I64(std::size_t c) const { return Number<std::int64_t>(c); }
    std::int32_t I32(std::size_t c) const { return Number<std::int32_t>(c); }
    bool Bool(std::size_t c) const { const auto value = U8(c); if (value > 1) Fail(ReadErrorCode::InvalidNumber, c); return value == 1; }
    std::optional<std::uint32_t> OptionalId(std::size_t c) const { return row[c] ? std::optional{U32(c)} : std::nullopt; }
    ItemKey Key(std::size_t c = 0) const
    {
        const auto key = ItemKey::Create(static_cast<ItemCategory>(U16(c)), U32(c + 1));
        if (!key) Fail(ReadErrorCode::InvalidKey, c);
        return *key;
    }
};
template <typename F> void Each(const Snapshot& source, Table table, F apply)
{
    const auto& rows = source[table];
    for (std::size_t index = 0; index < rows.size(); ++index) apply(Reader{table, index, rows[index]});
}
template <typename T> std::map<std::uint32_t, std::size_t> Index(const std::vector<T>& rows, Table table)
{
    std::map<std::uint32_t, std::size_t> index;
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (!index.emplace(rows[i].id, i).second) throw ReadError{ReadErrorCode::DuplicateRow, table, i};
    return index;
}
template <typename T> T& Parent(std::vector<T>& rows, const std::map<std::uint32_t, std::size_t>& index, const Reader& row)
{
    const auto found = index.find(row.U32(0));
    if (found == index.end()) row.Fail(ReadErrorCode::OrphanRow);
    return rows[found->second];
}
void ValidateShape(const Snapshot& source)
{
    std::size_t budget = 0;
    const auto account = [&](std::size_t bytes, Table table, std::size_t row, std::size_t column) {
        if (bytes > CapturedByteBudget - budget) throw ReadError{ReadErrorCode::ByteBudget, table, row, column};
        budget += bytes;
    };
    for (const auto& spec : Tables())
    {
        const auto& rows = source[spec.table];
        if (rows.size() > spec.maximumRows) throw ReadError{ReadErrorCode::RowLimit, spec.table};
        const auto columns = ColumnCount(spec);
        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            const auto& row = rows[index];
            if (row.size() != columns) throw ReadError{ReadErrorCode::ColumnCount, spec.table, index};
            account(row.size() * CellBudgetOverhead, spec.table, index, 0);
            for (std::size_t column = 0; column < row.size(); ++column)
                if (row[column]) account(row[column]->size(), spec.table, index, column);
        }
    }
}
void Registries(const Snapshot& source)
{
    if (source[Table::Format].size() != 1) throw ReadError{ReadErrorCode::FormatMismatch, Table::Format};
    Each(source, Table::Format, [](const Reader& r) {
        if (r.U8(0) != 1 || r.U32(1) != CatalogSchemaVersion || r.Text(2) != CanonicalDomain) r.Fail(ReadErrorCode::FormatMismatch);
    });
    if (source[Table::Categories].size() != ItemCategories.size()) throw ReadError{ReadErrorCode::RegistryMismatch, Table::Categories};
    std::set<std::uint16_t> categories;
    Each(source, Table::Categories, [&](const Reader& r) {
        const auto id = r.U16(0);
        const auto found = std::ranges::find_if(ItemCategories, [id](const auto& c) { return static_cast<std::uint16_t>(c.category) == id; });
        if (found == ItemCategories.end() || !categories.insert(id).second || r.Text(1) != found->code || r.Text(2) != found->table)
            r.Fail(ReadErrorCode::RegistryMismatch);
    });
    if (source[Table::BonusDefinitions].size() != BonusDefinitions.size()) throw ReadError{ReadErrorCode::RegistryMismatch, Table::BonusDefinitions};
    std::set<std::uint32_t> bonuses;
    Each(source, Table::BonusDefinitions, [&](const Reader& r) {
        const auto* d = FindBonusDefinition(r.U32(0));
        if (!d || !bonuses.insert(d->id).second || r.Text(1) != d->code || r.Text(2) != d->displayKey ||
            r.U8(3) != static_cast<std::uint8_t>(d->unit) || r.U8(4) != static_cast<std::uint8_t>(d->target) ||
            r.U8(5) != static_cast<std::uint8_t>(d->stage) || r.U8(6) != static_cast<std::uint8_t>(d->channel) ||
            r.U8(7) != static_cast<std::uint8_t>(d->action) || r.U8(8) != static_cast<std::uint8_t>(d->mode) ||
            r.U16(9) != static_cast<std::uint16_t>(d->family) || r.U16(10) != static_cast<std::uint16_t>(d->rank) ||
            r.U8(11) != static_cast<std::uint8_t>(d->element) || r.U8(12) != static_cast<std::uint8_t>(d->stacking) ||
            r.U8(13) != static_cast<std::uint8_t>(d->aggregateCap) || r.U8(14) != d->allowedSources ||
            r.I64(15) != d->minimum || r.I64(16) != d->maximum)
            r.Fail(ReadErrorCode::RegistryMismatch);
    });
}
ItemCatalogSource Decode(const Snapshot& source, std::uint64_t revision)
{
    Registries(source);
    ItemCatalogSource s; s.schemaVersion = CatalogSchemaVersion; s.revision = revision;
    Each(source, Table::UsePolicies, [&](const Reader& r) { s.usePolicies.push_back({r.U32(0), r.Text(1,96), r.U8(2), r.U8(3), r.Bool(4)}); });
    Each(source, Table::TradePolicies, [&](const Reader& r) { s.tradePolicies.push_back({r.U32(0), r.Text(1,96), r.Bool(2), r.Bool(3), r.Bool(4)}); });
    Each(source, Table::Appearances, [&](const Reader& r) { s.appearances.push_back({r.U32(0), r.Text(1,96), r.Text(2)}); });
    Each(source, Table::Attacks, [&](const Reader& r) { s.attacks.push_back({r.U32(0), r.Text(1,96), r.U32(2), r.U32(3), r.U32(4), r.U16(5)}); });
    Each(source, Table::AmmoPolicies, [&](const Reader& r) { s.ammoPolicies.push_back({r.U32(0), r.Text(1,96), static_cast<AmmoKind>(r.U8(2)), r.U16(3)}); });
    Each(source, Table::Resistances, [&](const Reader& r) {
        s.resistances.push_back({r.U32(0), r.Text(1,96), r.I32(2), r.I32(3), r.I32(4), r.I32(5), r.I32(6), r.I32(7),
            r.I32(8), r.I32(9), r.I32(10), r.I32(11), r.I32(12), r.I32(13), r.I32(14), r.I32(15), r.I32(16)});
    });
    Each(source, Table::BonusProfiles, [&](const Reader& r) { s.bonuses.push_back({r.U32(0), r.Text(1,96), static_cast<BonusSource>(r.U8(2)), {}}); });
    const auto bonuses = Index(s.bonuses, Table::BonusProfiles);
    Each(source, Table::BonusEntries, [&](const Reader& r) { Parent(s.bonuses, bonuses, r).entries.push_back({static_cast<BonusId>(r.U32(1)), r.I64(2)}); });
    Each(source, Table::Refinements, [&](const Reader& r) { s.refinements.push_back({r.U32(0), r.Text(1,96), static_cast<RefinementKind>(r.U8(2)), r.U16(3), {}}); });
    const auto refinements = Index(s.refinements, Table::Refinements);
    Each(source, Table::RefinementLevels, [&](const Reader& r) { Parent(s.refinements, refinements, r).levels.push_back({r.U16(1), {r.I64(2),r.I64(3),r.I64(4),r.I64(5)},r.OptionalId(6)}); });
    Each(source, Table::Items, [&](const Reader& r) {
        ItemBaseRow base{r.Key()};
        base.contentCode=r.Text(2,96); base.nameKey=r.Text(3,128); base.descriptionKey=r.Text(4,128); base.iconAsset=r.Text(5);
        base.width=r.U8(6); base.height=r.U8(7); base.maxStack=r.U32(8); base.weightGrams=r.U32(9); base.minimumLevel=r.U16(10);
        base.binding=static_cast<BindingPolicy>(r.U8(11)); base.usePolicy=r.U32(12); base.tradePolicy=r.U32(13); base.refinementProfile=r.U32(14);
        base.appearanceProfile=r.OptionalId(15); base.baseBonusProfile=r.OptionalId(16); base.currency=static_cast<ItemCurrency>(r.U8(17));
        base.buyAmount=r.U64(18); base.sellAmount=r.U64(19); base.retired=r.Bool(20); s.items.push_back(std::move(base));
    });
    Each(source, Table::Weapons, [&](const Reader& r) { s.weapons.push_back({r.Key(), static_cast<WeaponSubtype>(r.U8(2)), static_cast<EquipmentSlot>(r.U8(3)), r.I64(4), r.I64(5), r.U32(6), r.U32(7)}); });
    Each(source, Table::Armors, [&](const Reader& r) { s.armors.push_back({r.Key(), static_cast<EquipmentSlot>(r.U8(2)), r.I64(3), r.I64(4), r.U32(5)}); });
    Each(source, Table::Accessories, [&](const Reader& r) { s.accessories.push_back({r.Key(), static_cast<EquipmentSlot>(r.U8(2)), r.U32(3), r.OptionalId(4)}); });
    Each(source, Table::Materials, [&](const Reader& r) { s.materials.push_back({r.Key(), static_cast<MaterialKind>(r.U8(2))}); });
    Each(source, Table::SocketProfiles, [&](const Reader& r) { s.accessorySockets.push_back({r.U32(0), r.Text(1,96), r.U8(2), r.U16(3), r.U16(4), {}, {}, {}, {}}); });
    const auto sockets = Index(s.accessorySockets, Table::SocketProfiles);
    Each(source, Table::SocketGrades, [&](const Reader& r) { Parent(s.accessorySockets, sockets, r).grades.push_back({r.U8(1),r.U64(2),r.U32(3),r.I64(4),r.I32(5),r.I32(6),r.U16(7),r.U16(8)}); });
    Each(source, Table::SocketMaterials, [&](const Reader& r) {
        auto& parent = Parent(s.accessorySockets, sockets, r); const auto key = r.Key(2);
        switch (r.U8(1)) {
        case 1: parent.unlockMaterials.push_back(key); break;
        case 2: parent.chargeMaterials.push_back(key); break;
        case 3: parent.permanentChargeMaterials.push_back(key); break;
        default: r.Fail(ReadErrorCode::InvalidChild, 1);
        }
    });
    Each(source, Table::Cooldowns, [&](const Reader& r) { s.consumableCooldowns.push_back({r.U32(0),r.Text(1,96),r.U64(2)}); });
    std::map<std::uint32_t, std::uint8_t> delivery;
    Each(source, Table::Recoveries, [&](const Reader& r) {
        const auto id = r.U32(0);
        if (!delivery.emplace(id, r.U8(2)).second) r.Fail(ReadErrorCode::DuplicateRow);
        s.recoveries.push_back({id,r.Text(1,96),r.U16(3),r.U8(4),r.Bool(5),r.OptionalId(6),InstantRecovery{},r.U16(7)});
    });
    const auto recoveries = Index(s.recoveries, Table::Recoveries);
    std::set<std::uint32_t> children;
    const auto recoveryChild = [&](const Reader& r, std::uint8_t kind) -> RecoveryProfile& {
        auto& parent = Parent(s.recoveries, recoveries, r);
        if (r.U8(1) != kind || delivery.at(parent.id) != kind) r.Fail(ReadErrorCode::InvalidChild,1);
        if (!children.insert(parent.id).second) r.Fail(ReadErrorCode::DuplicateRow);
        return parent;
    };
    Each(source, Table::Instant, [&](const Reader& r) { recoveryChild(r,1).effect=InstantRecovery{r.I64(2),r.I64(3),r.U16(4),r.U16(5),r.U32(6)}; });
    Each(source, Table::Reserve, [&](const Reader& r) { recoveryChild(r,2).effect=ReserveRecovery{r.I64(2),r.I64(3),r.U32(4),r.U64(5),r.U16(6)}; });
    Each(source, Table::Periodic, [&](const Reader& r) { recoveryChild(r,3).effect=PeriodicRecovery{r.I64(2),r.I64(3),r.U64(4),r.U64(5)}; });
    Each(source, Table::Automatic, [&](const Reader& r) { recoveryChild(r,4).effect=AutomaticRecovery{static_cast<RecoveryPool>(r.U8(2)),static_cast<RecoverySupply>(r.U8(3)),r.I64(4),r.U64(5),r.U16(6),r.U16(7)}; });
    if (children.size() != s.recoveries.size()) throw ReadError{ReadErrorCode::InvalidChild,Table::Recoveries};
    Each(source,Table::SocketServices,[&](const Reader& r){s.socketServiceProfiles.push_back({r.U32(0),r.Text(1,96),static_cast<SocketServiceKind>(r.U8(2)),r.U16(3)});});
    Each(source, Table::Consumables, [&](const Reader& r) {
        if (r.row[2] && r.U32(2) == 0) r.Fail(ReadErrorCode::InvalidNumber, 2);
        s.consumables.push_back({r.Key(),r.OptionalId(2).value_or(0),r.OptionalId(3),r.OptionalId(4),r.OptionalId(5),r.OptionalId(6)});
    });
    Each(source, Table::LootProfiles, [&](const Reader& r) { s.lootProfiles.push_back({r.U32(0),r.Text(1,96),static_cast<LootPickMode>(r.U8(2)),{}}); });
    const auto loots = Index(s.lootProfiles, Table::LootProfiles);
    Each(source, Table::LootEntries, [&](const Reader& r) {
        // The reward item is one optional composite key: both columns or neither.
        if (r.row[3].has_value() != r.row[4].has_value()) r.Fail(ReadErrorCode::InvalidKey, 3);
        std::optional<ItemKey> item;
        if (r.row[3]) item = r.Key(3);
        Parent(s.lootProfiles, loots, r).entries.push_back({r.U16(1),static_cast<LootRewardKind>(r.U8(2)),item,r.U64(5),r.U32(6),r.U16(7)});
    });
    Each(source, Table::Containers, [&](const Reader& r) {
        s.containers.push_back({r.Key(),static_cast<ContainerKind>(r.U8(2)),r.OptionalId(3),r.U16(4),r.OptionalId(5)});
    });
    Each(source, Table::ContainerKeys, [&](const Reader& r) { s.containerKeys.push_back({r.Key(),r.U16(2)}); });
    Each(source, Table::CurrencyTokens, [&](const Reader& r) {
        s.currencyTokens.push_back({r.Key(),static_cast<WalletCurrency>(r.U8(2)),r.U64(3)});
    });
    Each(source, Table::TimedEffectGroups, [&](const Reader& r) {
        s.timedEffectGroups.push_back({r.U32(0),r.Text(1,96),static_cast<TimedEffectReapply>(r.U8(2)),static_cast<TimedEffectClock>(r.U8(3)),r.Bool(4)});
    });
    Each(source, Table::TimedEffects, [&](const Reader& r) {
        s.timedEffects.push_back({r.U32(0),r.Text(1,96),r.U32(2),r.U32(3),r.U64(4),r.U16(5),r.U8(6),r.Bool(7),r.OptionalId(8)});
    });
    Each(source, Table::ToggleEffectGroups, [&](const Reader& r) {
        s.toggleEffectGroups.push_back({r.U32(0),r.Text(1,96),r.Bool(2)});
    });
    Each(source, Table::ToggleEffects, [&](const Reader& r) {
        s.toggleEffects.push_back({r.U32(0),r.Text(1,96),r.U32(2),r.U32(3),r.U16(4),r.U64(5),r.U8(6),r.U8(7),r.Bool(8),r.OptionalId(9)});
    });
    Each(source, Table::BookProfiles, [&](const Reader& r) {
        s.skillBookProfiles.push_back({r.U32(0),r.Text(1,96),static_cast<SkillBookOperation>(r.U8(2)),static_cast<SkillBookTargetMode>(r.U8(3)),r.U16(4),r.U8(5),r.Bool(6),static_cast<SkillBookBonus>(r.U8(7)),{}, {}});
    });
    const auto books=Index(s.skillBookProfiles,Table::BookProfiles);
    Each(source, Table::BookTargets, [&](const Reader& r) {
        Parent(s.skillBookProfiles,books,r).targets.push_back({r.U16(1),static_cast<SkillPointPool>(r.U8(2)),r.U8(3)});
    });
    Each(source, Table::BookSteps, [&](const Reader& r) {
        Parent(s.skillBookProfiles,books,r).steps.push_back({r.U8(1),r.U8(2),r.U16(3),r.U16(4),r.U16(5),r.U16(6),r.U64(7),r.U64(8),r.U64(9),r.Bool(10)});
    });
    Each(source, Table::SkillBooks, [&](const Reader& r) {s.skillBooks.push_back({r.Key(),r.U32(2)});});
    Each(source, Table::RefinementRecipes, [&](const Reader& r) {
        std::optional<std::uint16_t> failureLevel;
        if(r.row[11])failureLevel=r.U16(11);
        s.refinementRecipes.push_back({r.U32(0),r.Text(1,96),r.Key(2),r.U16(4),r.Key(5),r.U16(7),r.U64(8),r.U16(9),
                                      static_cast<RefinementFailure>(r.U8(10)),failureLevel,{}});
    });
    const auto recipes=Index(s.refinementRecipes,Table::RefinementRecipes);
    Each(source, Table::RefinementIngredients, [&](const Reader& r) {
        Parent(s.refinementRecipes,recipes,r).ingredients.push_back({r.Key(1),r.U16(3),r.U32(4)});
    });
    Each(source,Table::QuestItems,[&](const Reader& r){s.questItems.push_back({r.Key(),static_cast<QuestItemDispatch>(r.U8(2)),r.Text(3,96),r.U8(4),r.Bool(5)});});
    Each(source,Table::Costumes,[&](const Reader& r){s.costumes.push_back({r.Key(),static_cast<CostumeKind>(r.U8(2)),r.row[3] ? std::optional{static_cast<WeaponSubtype>(r.U8(3))} : std::nullopt,static_cast<CostumeClock>(r.U8(4)),r.U64(5)});});
    Each(source,Table::CostumeVisuals,[&](const Reader& r){s.costumeVisuals.push_back({r.Key(),r.U8(2),r.U8(3),r.Text(4)});});
    Each(source,Table::SocketStones,[&](const Reader& r){s.socketStones.push_back({r.Key(),static_cast<SocketStoneKind>(r.U8(2)),r.U8(3),r.U8(4),r.Text(5,96),r.Text(6,96)});});
    Each(source,Table::ToolProfiles,[&](const Reader& r){s.toolProfiles.push_back({r.U32(0),r.Text(1,96),{}});});
    const auto toolIndex=Index(s.toolProfiles,Table::ToolProfiles);
    Each(source,Table::ToolLevels,[&](const Reader& r){Parent(s.toolProfiles,toolIndex,r).levels.push_back({r.U16(1),r.U32(2),r.U16(3),r.U32(4),r.U32(5),r.U32(6),r.U16(7),static_cast<ToolFailure>(r.U8(8)),r.row[9] ? std::optional{r.U16(9)} : std::nullopt,r.U16(10)});});
    Each(source,Table::Tools,[&](const Reader& r){s.tools.push_back({r.Key(),static_cast<ToolKind>(r.U8(2)),r.U32(3),static_cast<FishingPool>(r.U8(4))});});
    Each(source,Table::FishingItems,[&](const Reader& r){
        if(r.row[4].has_value()!=r.row[5].has_value())r.Fail(ReadErrorCode::InvalidChild,4);
        s.fishingItems.push_back({r.Key(),static_cast<FishingKind>(r.U8(2)),r.OptionalId(3),r.row[4] ? std::optional{r.Key(4)} : std::nullopt,r.U32(6)});
    });
    Each(source,Table::PolymorphForms,[&](const Reader& r){s.polymorphForms.push_back({r.U32(0),r.Text(1,96),r.U32(2),static_cast<PolymorphBonus>(r.U8(3))});});
    Each(source,Table::PolymorphProfiles,[&](const Reader& r){s.polymorphProfiles.push_back({r.U32(0),r.Text(1,96),static_cast<PolymorphKind>(r.U8(2)),r.U16(3),r.U16(4),r.U16(5),r.U16(6),r.U32(7),r.Number<std::int16_t>(8),{},{}});});
    const auto polyIndex=Index(s.polymorphProfiles,Table::PolymorphProfiles);
    Each(source,Table::PolymorphSkills,[&](const Reader& r){Parent(s.polymorphProfiles,polyIndex,r).skills.push_back({r.U8(1),r.U64(2),r.U32(3),r.U16(4)});});
    Each(source,Table::PolymorphMasteries,[&](const Reader& r){Parent(s.polymorphProfiles,polyIndex,r).masteries.push_back({r.U8(1),r.U64(2),r.U64(3)});});
    Each(source,Table::PolymorphItems,[&](const Reader& r){s.polymorphItems.push_back({r.Key(),r.U32(2),r.OptionalId(3)});});
    Each(source,Table::CompanionForms,[&](const Reader& r){s.companionForms.push_back({r.U32(0),r.Text(1,96),r.U32(2),r.Text(3,192)});});
    Each(source,Table::Companions,[&](const Reader& r){s.companions.push_back({r.Key(),static_cast<CompanionKind>(r.U8(2)),r.U32(3),r.OptionalId(4),static_cast<CompanionClock>(r.U8(5)),r.U64(6),r.Bool(7),r.OptionalId(8),r.OptionalId(9)});});
    Each(source,Table::GrowthPetProfiles,[&](const Reader& r){s.growthPetProfiles.push_back({r.U32(0),r.Text(1,96),r.U16(2),static_cast<GrowthExpPolicy>(r.U8(3)),{}, {}});});
    const auto growthIndex=Index(s.growthPetProfiles,Table::GrowthPetProfiles);
    Each(source,Table::GrowthPetLevels,[&](const Reader& r){Parent(s.growthPetProfiles,growthIndex,r).levels.push_back({r.U16(1),r.U64(2),r.U64(3)});});
    Each(source,Table::GrowthPetStages,[&](const Reader& r){Parent(s.growthPetProfiles,growthIndex,r).stages.push_back({r.U8(1),r.U16(2),r.U32(3)});});
    Each(source,Table::SoulProfiles,[&](const Reader& r){s.soulProfiles.push_back({r.U32(0),r.Text(1,96),{}});});
    const auto soulIndex=Index(s.soulProfiles,Table::SoulProfiles);
    Each(source,Table::SoulLevels,[&](const Reader& r){Parent(s.soulProfiles,soulIndex,r).levels.push_back({r.U16(1),r.U32(2),r.U32(3),r.U32(4),r.U32(5),r.U32(6),r.U64(7),r.U32(8),r.U32(9),r.U32(10)});});
    Each(source,Table::SoulItems,[&](const Reader& r){s.soulItems.push_back({r.Key(),static_cast<SoulKind>(r.U8(2)),r.row[3] ? std::optional{static_cast<SoulChannel>(r.U8(3))} : std::nullopt,r.OptionalId(4),r.U16(5)});});
    Each(source,Table::PetHatchProfiles,[&](const Reader& r){s.petHatchProfiles.push_back({r.U32(0),r.Text(1,96),r.Key(2),r.U64(4),{r.U16(5),r.U16(6),r.U16(7)},{r.U16(8),r.U16(9),r.U16(10)},{r.U16(11),r.U16(12),r.U16(13)}});});
    Each(source,Table::PetFeedRules,[&](const Reader& r){s.petFeedRules.push_back({r.Key(),static_cast<PetFeedingService>(r.U8(2)),static_cast<PetFoodKind>(r.U8(3)),static_cast<PetFoodConsumption>(r.U8(4)),r.U32(5),r.U16(6),r.U16(7),r.U32(8)});});
    Each(source,Table::PetTrainingProfiles,[&](const Reader& r){s.petTrainingProfiles.push_back({r.U32(0),r.Text(1,96),r.U16(2),r.U16(3),r.U16(4),r.U16(5),r.U16(6),{},{},{}});});
    const auto trainingIndex=Index(s.petTrainingProfiles,Table::PetTrainingProfiles);
    Each(source,Table::PetAgeTiers,[&](const Reader& r){Parent(s.petTrainingProfiles,trainingIndex,r).ages.push_back({r.U64(1),r.U32(2),r.U16(3),r.U16(4)});});
    Each(source,Table::PetSlotRules,[&](const Reader& r){Parent(s.petTrainingProfiles,trainingIndex,r).slots.push_back({r.U8(1),r.U8(2),r.U64(3)});});
    Each(source,Table::PetSkillDefinitions,[&](const Reader& r){Parent(s.petTrainingProfiles,trainingIndex,r).skills.push_back({r.U32(1),r.Text(2,96),static_cast<PetEffect>(r.U8(3)),{}});});
    Each(source,Table::PetSkillRanks,[&](const Reader& r){
        auto& skills=Parent(s.petTrainingProfiles,trainingIndex,r).skills;
        auto skill=std::ranges::find(skills,r.U32(1),&PetSkillDefinition::id);
        if(skill==skills.end())r.Fail(ReadErrorCode::InvalidChild,1);
        skill->ranks.push_back({r.U16(2),r.Number<std::int64_t>(3)});
    });
    Each(source,Table::PetSupplyProfiles,[&](const Reader& r){s.petSupplyProfiles.push_back({r.U32(0),r.Text(1,96),static_cast<PetSupplyKind>(r.U8(2))});});
    const auto supplyIndex=Index(s.petSupplyProfiles,Table::PetSupplyProfiles);
    Each(source,Table::PetStudyRules,[&](const Reader& r){if(r.U8(1)!=1)r.Fail(ReadErrorCode::InvalidChild,1);Parent(s.petSupplyProfiles,supplyIndex,r).study=PetStudyRule{r.U32(2),r.Bool(3),r.Bool(4),r.U16(5),r.U64(6)};});
    Each(source,Table::PetExtensionRules,[&](const Reader& r){if(r.U8(1)!=4)r.Fail(ReadErrorCode::InvalidChild,1);Parent(s.petSupplyProfiles,supplyIndex,r).extension=PetExtensionRule{r.U64(2),r.U64(3)};});
    Each(source,Table::PetEnchantRules,[&](const Reader& r){if(r.U8(1)!=6)r.Fail(ReadErrorCode::InvalidChild,1);Parent(s.petSupplyProfiles,supplyIndex,r).enchant=PetEnchantRule{r.U16(2),r.U16(3),{r.U16(4),r.U16(5),r.U16(6)}};});
    Each(source,Table::PetSkinRules,[&](const Reader& r){if(r.U8(1)!=8)r.Fail(ReadErrorCode::InvalidChild,1);Parent(s.petSupplyProfiles,supplyIndex,r).skin=PetSkinRule{r.U32(2),static_cast<CostumeClock>(r.U8(3)),r.U64(4)};});
    Each(source,Table::PetSupplies,[&](const Reader& r){if(r.U8(2)!=1)r.Fail(ReadErrorCode::InvalidChild,2);s.petSupplies.push_back({r.Key(),r.U32(3)});});
    Each(source,Table::AttributeProfiles,[&](const Reader& r){s.attributeProfiles.push_back({r.U32(0),r.Text(1,96),r.Bool(2),{},{}});});
    const auto attributeIndex=Index(s.attributeProfiles,Table::AttributeProfiles);
    Each(source,Table::AttributeGroups,[&](const Reader& r){Parent(s.attributeProfiles,attributeIndex,r).groups.push_back({static_cast<AttributePool>(r.U8(1)),r.U8(2),r.Bool(3)});});
    Each(source,Table::AttributeCandidates,[&](const Reader& r){Parent(s.attributeProfiles,attributeIndex,r).candidates.push_back({static_cast<AttributePool>(r.U8(1)),static_cast<BonusId>(r.U32(2)),r.U32(3),{}});});
    Each(source,Table::AttributeValues,[&](const Reader& r){auto& rows=Parent(s.attributeProfiles,attributeIndex,r).candidates;auto it=std::ranges::find_if(rows,[&](const auto& a){return a.pool==static_cast<AttributePool>(r.U8(1)) && a.bonus==static_cast<BonusId>(r.U32(2));});if(it==rows.end())r.Fail(ReadErrorCode::InvalidChild,2);it->values.push_back({r.U8(3),r.I64(4)});});
    Each(source,Table::AttributeBindings,[&](const Reader& r){s.attributeBindings.push_back({r.Key(),r.U32(2)});});
    Each(source,Table::AttributeServices,[&](const Reader& r){s.attributeServices.push_back({r.U32(0),r.Text(1,96),static_cast<AttributeOperation>(r.U8(2)),static_cast<AttributePool>(r.U8(3)),r.U16(4),r.U16(5),r.U8(6),r.U8(7),r.U8(8),r.U8(9),r.U16(10),r.Bool(11),{}});});
    const auto attributeServiceIndex=Index(s.attributeServices,Table::AttributeServices);
    Each(source,Table::AttributeTiers,[&](const Reader& r){Parent(s.attributeServices,attributeServiceIndex,r).tiers.push_back({r.U8(1),r.U16(2)});});
    return s;
}
}

std::expected<std::shared_ptr<const ItemCatalog>, ReadError> DecodeSnapshot(Snapshot source, ReadPin pin)
{
    if (!pin.revision) return std::unexpected(ReadError{ReadErrorCode::InvalidPin,{}});
    try
    {
        ValidateShape(source);
        auto catalog = BuildItemCatalog(Decode(source, pin.revision));
        if (!catalog)
        {
            ReadError error{ReadErrorCode::InvalidCatalog,{}}; error.catalogError = catalog.error();
            return std::unexpected(std::move(error));
        }
        if (pin.expectedHash && (*catalog)->ContentHash() != *pin.expectedHash)
            return std::unexpected(ReadError{ReadErrorCode::HashMismatch,{}});
        return *catalog;
    }
    catch (ReadError& error) { return std::unexpected(std::move(error)); }
}
} // namespace auriga::item::sql
