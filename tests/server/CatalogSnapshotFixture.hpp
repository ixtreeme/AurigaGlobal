#pragma once
#include "ItemCatalogFixture.hpp"
#include <CatalogSnapshot.hpp>
#include <type_traits>

namespace catalog_snapshot_fixture
{
using namespace auriga::item;
namespace db = auriga::item::sql;
inline db::Cell Cell(std::string_view text) { return std::string(text); }
inline db::Cell Cell(const std::string& text) { return text; }
inline db::Cell Cell(const char* text) { return std::string(text); }
template <typename T> db::Cell Cell(T value) {
    if constexpr (std::is_enum_v<T>) return std::to_string(static_cast<std::underlying_type_t<T>>(value));
    else return std::to_string(value);
}
inline db::Cell Cell(std::optional<std::uint32_t> value) { return value ? Cell(*value) : std::nullopt; }
template <typename... T> void Add(db::Snapshot& s, db::Table table, T... values) { s[table].push_back({Cell(values)...}); }
inline db::Snapshot Snapshot(const ItemCatalogSource& source = item_catalog_fixture::Catalog())
{
    db::Snapshot s;
    using T = db::Table;
    Add(s,T::Format,1,5,db::CanonicalDomain);
    for (const auto& r : ItemCategories) Add(s,T::Categories,r.category,r.code,r.table);
    for (const auto& d : BonusDefinitions)
        Add(s,T::BonusDefinitions,d.id,d.code,d.displayKey,d.unit,d.target,d.stage,d.channel,d.action,d.mode,d.family,d.rank,
            d.element,d.stacking,d.aggregateCap,d.allowedSources,d.minimum,d.maximum);
    for (const auto& r : source.usePolicies) Add(s,T::UsePolicies,r.id,r.code,r.allowedClasses,r.allowedSexes,r.useDuringCombat);
    for (const auto& r : source.tradePolicies) Add(s,T::TradePolicies,r.id,r.code,r.allowTrade,r.allowDrop,r.allowSell);
    for (const auto& r : source.appearances) Add(s,T::Appearances,r.id,r.code,r.modelAsset);
    for (const auto& r : source.attacks) Add(s,T::Attacks,r.id,r.code,r.intervalTicks,r.windupTicks,r.reachMillimeters,r.maxTargets);
    for (const auto& r : source.ammoPolicies) Add(s,T::AmmoPolicies,r.id,r.code,r.kind,r.unitsPerHit);
    for (const auto& r : source.resistances) {
        db::Row row{Cell(r.id),Cell(r.code)};
        for (auto value : r.Values()) row.push_back(Cell(value));
        s[T::Resistances].push_back(std::move(row));
    }
    for (const auto& r : source.bonuses) {
        Add(s,T::BonusProfiles,r.id,r.code,r.source);
        for (const auto& e : r.entries) Add(s,T::BonusEntries,r.id,e.bonus,e.valueScaled);
    }
    for (const auto& r : source.refinements) {
        Add(s,T::Refinements,r.id,r.code,r.kind,r.maxLevel);
        for (const auto& l : r.levels) Add(s,T::RefinementLevels,r.id,l.level,l.gain.physicalAttackRaw,l.gain.magicalAttackRaw,
            l.gain.physicalDefenseRaw,l.gain.magicalDefenseRaw,l.extraBonusProfile);
    }
    for (const auto& r : source.items) Add(s,T::Items,r.key.Category(),r.key.LocalId(),r.contentCode,r.nameKey,r.descriptionKey,
        r.iconAsset,r.width,r.height,r.maxStack,r.weightGrams,r.minimumLevel,r.binding,r.usePolicy,r.tradePolicy,
        r.refinementProfile,r.appearanceProfile,r.baseBonusProfile,r.currency,r.buyAmount,r.sellAmount,r.retired);
    for (const auto& r : source.weapons) Add(s,T::Weapons,r.key.Category(),r.key.LocalId(),r.subtype,r.slot,
        r.physicalAttackBaseRaw,r.magicalAttackBaseRaw,r.attackProfile,r.ammoPolicy);
    for (const auto& r : source.armors) Add(s,T::Armors,r.key.Category(),r.key.LocalId(),r.slot,r.physicalDefenseBaseRaw,r.magicalDefenseBaseRaw,r.resistanceProfile);
    for (const auto& r : source.accessories) Add(s,T::Accessories,r.key.Category(),r.key.LocalId(),r.slot,r.resistanceProfile,r.socketProfile);
    for (const auto& r : source.materials) Add(s,T::Materials,r.key.Category(),r.key.LocalId(),r.kind);
    for (const auto& r : source.accessorySockets) {
        Add(s,T::SocketProfiles,r.id,r.code,r.maxGrade,r.unlockUnitsPerAttempt,r.chargeUnitsPerAttempt);
        for (const auto& g : r.grades) Add(s,T::SocketGrades,r.id,g.grade,g.durationMs,g.bonusIncreaseBp,g.minimumRaw,g.minimumBp,g.minimumInteger,g.unlockChanceBp,g.chargeChanceBp);
        for (auto key : r.unlockMaterials) Add(s,T::SocketMaterials,r.id,1,key.Category(),key.LocalId());
        for (auto key : r.chargeMaterials) Add(s,T::SocketMaterials,r.id,2,key.Category(),key.LocalId());
        for (auto key : r.permanentChargeMaterials) Add(s,T::SocketMaterials,r.id,3,key.Category(),key.LocalId());
    }
    for (const auto& r : source.consumableCooldowns) Add(s,T::Cooldowns,r.id,r.code,r.durationMs);
    for (const auto& r : source.recoveries) {
        const auto delivery = std::holds_alternative<InstantRecovery>(r.effect) ? 1 : std::holds_alternative<ReserveRecovery>(r.effect) ? 2 : 3;
        Add(s,T::Recoveries,r.id,r.code,delivery,r.unitsPerUse,r.allowedContexts,r.allowDuringPvp,r.cooldownProfile);
        std::visit([&](const auto& e) {
            using E = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<E,InstantRecovery>) Add(s,T::Instant,r.id,1,e.hpRaw,e.spRaw,e.maxHpBp,e.maxSpBp,e.flatBonusCapBp);
            else if constexpr (std::is_same_v<E,ReserveRecovery>) Add(s,T::Reserve,r.id,2,e.hpRaw,e.spRaw,e.flatBonusCapBp,e.tickIntervalMs,e.maxPoolPerTickBp);
            else Add(s,T::Periodic,r.id,3,e.hpPerTickRaw,e.spPerTickRaw,e.tickIntervalMs,e.durationMs);
        },r.effect);
    }
    for (const auto& r : source.consumables) Add(s,T::Consumables,r.key.Category(),r.key.LocalId(),r.recoveryProfile);
    return s;
}
} // namespace catalog_snapshot_fixture
