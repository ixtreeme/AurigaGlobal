#include "CatalogHash.hpp"
#include <item/ItemWireCodec.hpp>
#include <memory>
#include <string_view>
#include <type_traits>
#include <openssl/evp.h>

namespace auriga::item::detail
{
namespace
{
class Writer
{
public:
    Writer() : context_(EVP_MD_CTX_new(), EVP_MD_CTX_free)
    {
        good_ = context_ && EVP_DigestInit_ex(context_.get(), EVP_sha256(), nullptr) == 1;
    }
    void Bytes(const void* data, std::size_t size)
    {
        if (good_ && size) good_ = EVP_DigestUpdate(context_.get(), data, size) == 1;
    }
    template <typename T> void Integer(T value)
    {
        static_assert(std::is_unsigned_v<T>);
        std::array<std::uint8_t, sizeof(T)> bytes {};
        for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<std::uint8_t>(value >> (i * 8));
        Bytes(bytes.data(), bytes.size());
    }
    void Boolean(bool value) { Integer<std::uint8_t>(value ? 1 : 0); }
    void Text(const std::string_view value)
    {
        Integer<std::uint32_t>(static_cast<std::uint32_t>(value.size()));
        Bytes(value.data(), value.size());
    }
    void Optional(std::optional<std::uint32_t> value)
    {
        Boolean(value.has_value());
        if (value) Integer(*value);
    }
    void Key(ItemKey key)
    {
        const auto bytes = EncodeItemKey(key);
        Bytes(bytes.data(), bytes.size());
    }
    void Table(std::uint8_t tag, std::size_t count)
    {
        Integer(tag);
        Integer<std::uint32_t>(static_cast<std::uint32_t>(count));
    }
    void Identity(std::uint32_t id, std::string_view code) { Integer(id); Text(code); }
    std::expected<ItemCatalogDigest, ItemCatalogErrorCode> Finish()
    {
        ItemCatalogDigest digest {};
        unsigned int size = 0;
        if (!good_ || EVP_DigestFinal_ex(context_.get(), digest.data(), &size) != 1 || size != digest.size())
            return std::unexpected(ItemCatalogErrorCode::HashFailure);
        return digest;
    }
private:
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context_;
    bool good_ = false;
};
}

std::expected<ItemCatalogDigest, ItemCatalogErrorCode> CanonicalCatalogHash(const ItemCatalogSource& source)
{
    // Vectors and refinement levels are already validated and canonically sorted.
    // Revision is separate release identity; unchanged content retains its digest.
    Writer out;
    out.Text("auriga.item_catalog.v14");
    out.Integer(source.schemaVersion);
    out.Table(1, source.usePolicies.size());
    for (const auto& row : source.usePolicies)
    {
        out.Identity(row.id, row.code); out.Integer(row.allowedClasses); out.Integer(row.allowedSexes); out.Boolean(row.useDuringCombat);
    }
    out.Table(2, source.tradePolicies.size());
    for (const auto& row : source.tradePolicies)
    {
        out.Identity(row.id, row.code); out.Boolean(row.allowTrade); out.Boolean(row.allowDrop); out.Boolean(row.allowSell);
    }
    out.Table(3, source.appearances.size());
    for (const auto& row : source.appearances) { out.Identity(row.id, row.code); out.Text(row.modelAsset); }
    out.Table(4, source.attacks.size());
    for (const auto& row : source.attacks)
    {
        out.Identity(row.id, row.code); out.Integer(row.intervalTicks); out.Integer(row.windupTicks);
        out.Integer(row.reachMillimeters); out.Integer(row.maxTargets);
    }
    out.Table(5, source.ammoPolicies.size());
    for (const auto& row : source.ammoPolicies)
    {
        out.Identity(row.id, row.code); out.Integer(static_cast<std::uint8_t>(row.kind)); out.Integer(row.unitsPerHit);
    }
    out.Table(6, source.resistances.size());
    for (const auto& row : source.resistances)
    {
        out.Identity(row.id, row.code);
        for (auto value : row.Values()) out.Integer(static_cast<std::uint32_t>(value));
    }
    out.Table(7, source.refinements.size());
    for (const auto& row : source.refinements)
    {
        out.Identity(row.id, row.code); out.Integer(static_cast<std::uint8_t>(row.kind)); out.Integer(row.maxLevel);
        out.Integer<std::uint32_t>(static_cast<std::uint32_t>(row.levels.size()));
        for (const auto& step : row.levels)
        {
            out.Integer(step.level);
            for (auto value : step.gain.Values()) out.Integer(static_cast<std::uint64_t>(value));
            out.Optional(step.extraBonusProfile);
        }
    }
    out.Table(8, source.items.size());
    for (const auto& row : source.items)
    {
        out.Key(row.key); out.Text(row.contentCode); out.Text(row.nameKey); out.Text(row.descriptionKey); out.Text(row.iconAsset);
        out.Integer(row.width); out.Integer(row.height); out.Integer(row.maxStack); out.Integer(row.weightGrams); out.Integer(row.minimumLevel);
        out.Integer(static_cast<std::uint8_t>(row.binding)); out.Integer(row.usePolicy); out.Integer(row.tradePolicy); out.Integer(row.refinementProfile);
        out.Optional(row.appearanceProfile); out.Optional(row.baseBonusProfile); out.Integer(static_cast<std::uint8_t>(row.currency));
        out.Integer(row.buyAmount); out.Integer(row.sellAmount); out.Boolean(row.retired);
    }
    out.Table(9, source.weapons.size());
    for (const auto& row : source.weapons)
    {
        out.Key(row.key); out.Integer(static_cast<std::uint8_t>(row.subtype)); out.Integer(static_cast<std::uint8_t>(row.slot));
        out.Integer(static_cast<std::uint64_t>(row.physicalAttackBaseRaw)); out.Integer(static_cast<std::uint64_t>(row.magicalAttackBaseRaw));
        out.Integer(row.attackProfile); out.Integer(row.ammoPolicy);
    }
    out.Table(10, source.armors.size());
    for (const auto& row : source.armors)
    {
        out.Key(row.key); out.Integer(static_cast<std::uint8_t>(row.slot));
        out.Integer(static_cast<std::uint64_t>(row.physicalDefenseBaseRaw)); out.Integer(static_cast<std::uint64_t>(row.magicalDefenseBaseRaw));
        out.Integer(row.resistanceProfile);
    }
    // Registry semantics are content too. A BonusId whose unit/stage/cap/source
    // changes must never silently retain an earlier catalog's identity.
    out.Integer(BonusRegistrySchemaVersion); out.Text(BonusRegistryStatus);
    out.Table(11, BonusDefinitions.size());
    for (const auto& row : BonusDefinitions)
    {
        out.Identity(row.id, row.code); out.Text(row.displayKey);
        out.Integer(static_cast<std::uint8_t>(row.unit)); out.Integer(static_cast<std::uint8_t>(row.target));
        out.Integer(static_cast<std::uint8_t>(row.stage)); out.Integer(static_cast<std::uint8_t>(row.channel));
        out.Integer(static_cast<std::uint8_t>(row.action)); out.Integer(static_cast<std::uint8_t>(row.mode));
        out.Integer(static_cast<std::uint16_t>(row.family)); out.Integer(static_cast<std::uint16_t>(row.rank));
        out.Integer(static_cast<std::uint8_t>(row.element)); out.Integer(static_cast<std::uint8_t>(row.stacking));
        out.Integer(static_cast<std::uint8_t>(row.aggregateCap)); out.Integer(row.allowedSources);
        out.Integer(static_cast<std::uint64_t>(row.minimum)); out.Integer(static_cast<std::uint64_t>(row.maximum));
    }
    out.Table(12, source.bonuses.size());
    for (const auto& row : source.bonuses)
    {
        out.Identity(row.id, row.code); out.Integer(static_cast<std::uint8_t>(row.source));
        out.Integer<std::uint32_t>(static_cast<std::uint32_t>(row.entries.size()));
        for (const auto& entry : row.entries)
        {
            out.Integer(static_cast<std::uint32_t>(entry.bonus));
            out.Integer(static_cast<std::uint64_t>(entry.valueScaled));
        }
    }
    out.Table(13, source.accessories.size());
    for (const auto& row : source.accessories)
    {
        out.Key(row.key); out.Integer(static_cast<std::uint8_t>(row.slot)); out.Integer(row.resistanceProfile); out.Optional(row.socketProfile);
    }
    out.Table(14, source.materials.size());
    for (const auto& row : source.materials)
    {
        out.Key(row.key); out.Integer(static_cast<std::uint8_t>(row.kind));
    }
    out.Table(15, source.accessorySockets.size());
    for (const auto& row : source.accessorySockets)
    {
        out.Identity(row.id, row.code); out.Integer(row.maxGrade);
        out.Integer(row.unlockUnitsPerAttempt); out.Integer(row.chargeUnitsPerAttempt);
        out.Integer<std::uint32_t>(static_cast<std::uint32_t>(row.grades.size()));
        for (const auto& grade : row.grades)
        {
            out.Integer(grade.grade); out.Integer(grade.durationMs); out.Integer(grade.bonusIncreaseBp);
            out.Integer(static_cast<std::uint64_t>(grade.minimumRaw));
            out.Integer(static_cast<std::uint32_t>(grade.minimumBp)); out.Integer(static_cast<std::uint32_t>(grade.minimumInteger));
            out.Integer(grade.unlockChanceBp); out.Integer(grade.chargeChanceBp);
        }
        out.Integer<std::uint32_t>(static_cast<std::uint32_t>(row.unlockMaterials.size()));
        for (auto key : row.unlockMaterials) out.Key(key);
        out.Integer<std::uint32_t>(static_cast<std::uint32_t>(row.chargeMaterials.size()));
        for (auto key : row.chargeMaterials) out.Key(key);
        out.Integer<std::uint32_t>(static_cast<std::uint32_t>(row.permanentChargeMaterials.size()));
        for (auto key : row.permanentChargeMaterials) out.Key(key);
    }
    out.Table(16, source.consumableCooldowns.size());
    for (const auto& row : source.consumableCooldowns) { out.Identity(row.id, row.code); out.Integer(row.durationMs); }
    out.Table(17, source.recoveries.size());
    for (const auto& row : source.recoveries)
    {
        out.Identity(row.id, row.code); out.Integer(row.unitsPerUse); out.Integer(row.allowedContexts);
        out.Boolean(row.allowDuringPvp); out.Optional(row.cooldownProfile); out.Integer(row.arenaQuotaCost); out.Integer(static_cast<std::uint8_t>(row.Delivery()));
        std::visit([&](const auto& effect) {
            using T = std::decay_t<decltype(effect)>;
            if constexpr (std::is_same_v<T, AutomaticRecovery>)
            {
                out.Integer(static_cast<std::uint8_t>(effect.pool)); out.Integer(static_cast<std::uint8_t>(effect.supply));
                out.Integer(static_cast<std::uint64_t>(effect.capacityRaw)); out.Integer(effect.tickIntervalMs);
                out.Integer(effect.maxPoolPerTickBp); out.Integer(effect.activeMaxPoolBonusBp);
            }
            else if constexpr (std::is_same_v<T, PeriodicRecovery>)
            {
                out.Integer(static_cast<std::uint64_t>(effect.hpPerTickRaw)); out.Integer(static_cast<std::uint64_t>(effect.spPerTickRaw));
                out.Integer(effect.tickIntervalMs); out.Integer(effect.durationMs);
            }
            else
            {
                out.Integer(static_cast<std::uint64_t>(effect.hpRaw)); out.Integer(static_cast<std::uint64_t>(effect.spRaw));
                if constexpr (std::is_same_v<T, InstantRecovery>) { out.Integer(effect.maxHpBp); out.Integer(effect.maxSpBp); out.Integer(effect.flatBonusCapBp); }
                else { out.Integer(effect.flatBonusCapBp); out.Integer(effect.tickIntervalMs); out.Integer(effect.maxPoolPerTickBp); }
            }
        }, row.effect);
    }
    out.Table(18, source.consumables.size());
    for (const auto& row : source.consumables) { out.Key(row.key); out.Optional(row.recoveryProfile ? std::optional{row.recoveryProfile} : std::nullopt); out.Optional(row.timedEffectProfile); out.Optional(row.toggleEffectProfile); }
    out.Table(19, source.lootProfiles.size());
    for (const auto& row : source.lootProfiles)
    {
        out.Identity(row.id, row.code); out.Integer(static_cast<std::uint8_t>(row.mode));
        out.Integer<std::uint32_t>(static_cast<std::uint32_t>(row.entries.size()));
        for (const auto& entry : row.entries)
        {
            out.Integer(entry.entry); out.Integer(static_cast<std::uint8_t>(entry.kind));
            out.Boolean(entry.item.has_value());
            if (entry.item) out.Key(*entry.item);
            out.Integer(entry.amount); out.Integer(entry.weight); out.Integer(entry.rareAttributeChanceBp);
        }
    }
    out.Table(20, source.containers.size());
    for (const auto& row : source.containers)
    {
        out.Key(row.key); out.Integer(static_cast<std::uint8_t>(row.kind)); out.Optional(row.lootProfile); out.Integer(row.lockId);
    }
    out.Table(21, source.containerKeys.size());
    for (const auto& row : source.containerKeys) { out.Key(row.key); out.Integer(row.lockId); }
    out.Table(22, source.currencyTokens.size());
    for (const auto& row : source.currencyTokens)
    {
        out.Key(row.key); out.Integer(static_cast<std::uint8_t>(row.currency)); out.Integer(row.amountPerUnit);
    }
    out.Table(23, source.timedEffectGroups.size());
    for (const auto& row : source.timedEffectGroups)
    {
        out.Identity(row.id, row.code); out.Integer(static_cast<std::uint8_t>(row.reapply));
        out.Integer(static_cast<std::uint8_t>(row.clock)); out.Boolean(row.removeOnDeath);
    }
    out.Table(24, source.timedEffects.size());
    for (const auto& row : source.timedEffects)
    {
        out.Identity(row.id, row.code); out.Integer(row.group); out.Integer(row.bonusProfile);
        out.Integer(row.durationMs); out.Integer(row.unitsPerUse); out.Integer(row.allowedContexts);
        out.Boolean(row.allowDuringPvp); out.Optional(row.cooldownProfile);
    }
    out.Table(25, source.toggleEffectGroups.size());
    for (const auto& r : source.toggleEffectGroups) { out.Identity(r.id, r.code); out.Boolean(r.removeOnDeath); }
    out.Table(26, source.toggleEffects.size());
    for (const auto& r : source.toggleEffects)
    {
        out.Identity(r.id, r.code); out.Integer(r.group); out.Integer(r.bonusProfile); out.Integer(r.variant);
        out.Integer(r.durationMs); out.Integer(r.requirements); out.Integer(r.allowedContexts);
        out.Boolean(r.allowDuringPvp); out.Optional(r.cooldownProfile);
    }
    out.Table(27, source.skillBookProfiles.size());
    for (const auto& p : source.skillBookProfiles)
    {
        out.Identity(p.id,p.code); out.Integer(static_cast<std::uint8_t>(p.operation));
        out.Integer(static_cast<std::uint8_t>(p.targetMode));out.Integer(p.unitsPerUse);out.Integer(p.allowedMasteries);
        out.Boolean(p.allowDelayBypass);out.Integer(static_cast<std::uint8_t>(p.bonus));
        out.Integer(static_cast<std::uint32_t>(p.targets.size()));
        for (const auto& t:p.targets) {out.Integer(t.skillId);out.Integer(static_cast<std::uint8_t>(t.refundPool));out.Integer(t.forbiddenEmpire);}
        out.Integer(static_cast<std::uint32_t>(p.steps.size()));
        for (const auto& s:p.steps)
        {
            out.Integer(s.currentLevel);out.Integer(s.nextLevel);out.Integer(s.minimumCharacterLevel);
            out.Integer(s.requiredSuccessfulReads);out.Integer(s.successChanceBp);out.Integer(s.boostedChanceBp);
            out.Integer(s.experienceCost);out.Integer(s.delayMinimumMs);out.Integer(s.delayMaximumMs);out.Boolean(s.delayOnFailure);
        }
    }
    out.Table(28, source.skillBooks.size());
    for (const auto& row:source.skillBooks) {out.Key(row.key);out.Integer(row.profile);}
    out.Table(29, source.refinementRecipes.size());
    for (const auto& r : source.refinementRecipes)
    {
        out.Identity(r.id,r.code);out.Key(r.source);out.Integer(r.sourceLevel);out.Key(r.target);out.Integer(r.targetLevel);
        out.Integer(r.goldCost);out.Integer(r.successChanceBp);out.Integer(static_cast<std::uint8_t>(r.failure));
        out.Boolean(r.failureLevel.has_value());if(r.failureLevel)out.Integer(*r.failureLevel);
        out.Integer(static_cast<std::uint32_t>(r.ingredients.size()));
        for(const auto& i:r.ingredients){out.Key(i.key);out.Integer(i.refinementLevel);out.Integer(i.count);}
    }
    out.Table(30,source.questItems.size());
    for(const auto& r:source.questItems){out.Key(r.key);out.Integer(static_cast<std::uint8_t>(r.dispatch));out.Text(r.groupCode);out.Integer(r.allowedContexts);out.Boolean(r.allowObserver);}
    out.Table(31,source.socketStones.size());
    for(const auto& r:source.socketStones){out.Key(r.key);out.Integer(static_cast<std::uint8_t>(r.kind));out.Integer(r.allowedTargets);out.Integer(r.minimumSocketGrade);out.Text(r.primaryGroup);out.Text(r.secondaryGroup);}
    return out.Finish();
}
} // namespace auriga::item::detail
