#include <item/ItemCatalog.hpp>
#include "CatalogHash.hpp"
#include "ContentValidation.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>

namespace auriga::item
{
namespace
{
ItemCatalogError Error(ItemCatalogErrorCode code, std::string field, std::optional<ItemKey> key = {}, std::uint32_t id = 0)
{
    return {code, std::move(field), key, id, {}, {}, {}, {}, {}};
}
template <typename T> const T* FindProfile(const std::vector<T>& rows, std::uint32_t id) noexcept
{
    const auto found = std::ranges::lower_bound(rows, id, {}, &T::id);
    return found != rows.end() && found->id == id ? &*found : nullptr;
}
template <typename T, typename Validator>
std::expected<void, ItemCatalogError> Profiles(std::vector<T>& rows, std::string_view field, Validator validate)
{
    std::ranges::sort(rows, {}, &T::id);
    std::set<std::string> codes;
    std::uint32_t previous = 0;
    for (const auto& row : rows)
    {
        if (!row.id || !detail::ContentCode(row.code))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidIdentity, std::string(field), {}, row.id));
        if (previous == row.id || !codes.insert(row.code).second)
            return std::unexpected(Error(ItemCatalogErrorCode::DuplicateProfile, std::string(field), {}, row.id));
        if (!validate(row)) return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, std::string(field), {}, row.id));
        previous = row.id;
    }
    return {};
}
std::expected<void, ItemCatalogError> Budget(const ItemCatalogSource& source)
{
    if (source.petSupplies.size()>ItemCatalogLimits::Items || source.petFeedRules.size()>ItemCatalogLimits::Items*2 || source.items.size() > ItemCatalogLimits::Items || source.weapons.size() > ItemCatalogLimits::Items ||
        source.armors.size() > ItemCatalogLimits::Items || source.accessories.size() > ItemCatalogLimits::Items ||
        source.materials.size() > ItemCatalogLimits::Items || source.consumables.size() > ItemCatalogLimits::Items ||
        source.containers.size() > ItemCatalogLimits::Items || source.containerKeys.size() > ItemCatalogLimits::Items ||
        source.currencyTokens.size() > ItemCatalogLimits::Items || source.skillBooks.size() > ItemCatalogLimits::Items ||
        source.questItems.size() > ItemCatalogLimits::Items || source.socketStones.size()>ItemCatalogLimits::Items ||
        source.companions.size()>ItemCatalogLimits::Items || source.companionForms.size()>ItemCatalogLimits::Items || source.soulItems.size()>ItemCatalogLimits::Items || source.polymorphForms.size()>ItemCatalogLimits::Items || source.polymorphItems.size()>ItemCatalogLimits::Items ||
        source.tools.size()>ItemCatalogLimits::Items || source.fishingItems.size()>ItemCatalogLimits::Items ||
        source.costumes.size()>ItemCatalogLimits::Items || source.costumeVisuals.size()>ItemCatalogLimits::Items*8)
        return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "items"));
    std::size_t textBytes = 0;
    auto add = [&](std::size_t length) {
        if (length > ItemCatalogLimits::TextBytes - textBytes) return false;
        textBytes += length;
        return true;
    };
    auto profiles = [&](const auto& rows) {
        if (rows.size() > ItemCatalogLimits::ProfilesPerKind) return false;
        for (const auto& row : rows) if (!add(row.code.size())) return false;
        return true;
    };
    if (!profiles(source.attributeProfiles) || !profiles(source.attributeServices) || !profiles(source.socketServiceProfiles) || !profiles(source.petTrainingProfiles) || !profiles(source.petSupplyProfiles) || !profiles(source.petHatchProfiles) || !profiles(source.growthPetProfiles) || !profiles(source.soulProfiles) || !profiles(source.polymorphProfiles) || !profiles(source.toolProfiles) || !profiles(source.usePolicies) || !profiles(source.tradePolicies) || !profiles(source.appearances) ||
        !profiles(source.attacks) || !profiles(source.ammoPolicies) || !profiles(source.resistances) || !profiles(source.refinements) ||
        !profiles(source.bonuses) || !profiles(source.accessorySockets) || !profiles(source.recoveries) || !profiles(source.consumableCooldowns) ||
        !profiles(source.lootProfiles) || !profiles(source.timedEffectGroups) || !profiles(source.timedEffects) || !profiles(source.toggleEffectGroups) || !profiles(source.toggleEffects) || !profiles(source.skillBookProfiles))
        return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "profiles"));
    for(const auto& p:source.attributeProfiles){if(p.groups.size()>3 || p.candidates.size()>MaxAttributeCandidates)return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"attribute.profile"));for(const auto& a:p.candidates)if(a.values.size()>MaxAttributeTiers)return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"attribute.values"));}
    for(const auto& p:source.attributeServices)if(p.tiers.size()>MaxAttributeTiers)return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"attribute.tiers"));
    if(source.attributeBindings.size()>ItemCatalogLimits::Items)return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"attribute.bindings"));
    for(const auto& p:source.petTrainingProfiles) {
        if(p.ages.size()>32 || p.slots.size()>4 || p.skills.size()>128)return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"pet.training",{},p.id));
        for(const auto& skill:p.skills)if(skill.ranks.size()>255 || !add(skill.code.size()))return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"pet.skills",{},p.id));
    }
    for(const auto& p:source.growthPetProfiles)
        if(p.levels.size()>255 || p.stages.size()>16)return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"growth_pet.profile",{},p.id));
    for(const auto& f:source.companionForms)
        if(!add(f.code.size()) || !add(f.modelAsset.size()))return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"companion.form",{},f.id));
    for(const auto& p:source.soulProfiles)
        if(p.levels.size()>256)return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"soul.levels",{},p.id));
    for(const auto& p:source.polymorphProfiles)
        if(p.skills.size()>41 || p.masteries.size()>4)return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"polymorph.rules",{},p.id));
    for(const auto& p:source.polymorphForms)
        if(!add(p.code.size()))return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"polymorph.form",{},p.id));
    for(const auto& p:source.toolProfiles)
        if(p.levels.size()>256)return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"tool.levels",{},p.id));
    for (const auto& p : source.skillBookProfiles)
        if (p.targets.size()>MaxSkillBookTargets || p.steps.size()>MaxSkillBookSteps)
            return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "skill_book.children", {}, p.id));
    if (source.refinementRecipes.size() > MaxRefinementRecipes)
        return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "refinement_recipes"));
    for (const auto& recipe : source.refinementRecipes)
        if (recipe.ingredients.size() > MaxRefinementIngredients || !add(recipe.code.size()))
            return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "refinement_recipe.ingredients", {}, recipe.id));
    for (const auto& profile : source.lootProfiles)
        if (profile.entries.size() > MaxLootEntries)
            return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "loot.entries", {}, profile.id));
    for (const auto& profile : source.bonuses)
        if (profile.entries.size() > MaxBonusEntriesPerProfile)
            return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "bonus.entries", {}, profile.id));
    for (const auto& row : source.appearances)
        if (!add(row.modelAsset.size())) return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "appearance"));
    for (const auto& row : source.items)
        if (!add(row.contentCode.size()) || !add(row.nameKey.size()) || !add(row.descriptionKey.size()) || !add(row.iconAsset.size()))
            return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "text"));
    for (const auto& row : source.questItems)
        if (!add(row.groupCode.size())) return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"quest_item.text",row.key));
    for (const auto& row : source.socketStones)
        if (!add(row.primaryGroup.size()) || !add(row.secondaryGroup.size()))
            return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"socket_stone.text",row.key));
    for(const auto& row:source.costumeVisuals)
        if(!add(row.modelAsset.size()))return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded,"costume.visual",row.key));
    return {};
}
ItemBaseStats BaseStats(const ItemWeaponRow& row) { return {row.physicalAttackBaseRaw, row.magicalAttackBaseRaw, 0, 0}; }
ItemBaseStats BaseStats(const ItemArmorRow& row) { return {0, 0, row.physicalDefenseBaseRaw, row.magicalDefenseBaseRaw}; }
ItemBaseStats BaseStats(const ItemAccessoryRow&) { return {}; }
ItemBaseStats BaseStats(const ItemMaterialRow&) { return {}; }
ItemBaseStats BaseStats(const ItemConsumableRow&) { return {}; }
ItemBaseStats BaseStats(const ItemContainerRow&) { return {}; }
ItemBaseStats BaseStats(const ItemContainerKeyRow&) { return {}; }
ItemBaseStats BaseStats(const ItemCurrencyTokenRow&) { return {}; }
ItemBaseStats BaseStats(const ItemSkillBookRow&) { return {}; }
ItemBaseStats BaseStats(const ItemQuestRow&) { return {}; }
ItemBaseStats BaseStats(const ItemSocketStoneRow&) { return {}; }
ItemBaseStats BaseStats(const ItemCostumeRow&) { return {}; }
ItemBaseStats BaseStats(const ItemToolRow&) { return {}; }
ItemBaseStats BaseStats(const ItemFishingRow&) { return {}; }
ItemBaseStats BaseStats(const ItemPolymorphRow&) { return {}; }
ItemBaseStats BaseStats(const ItemSoulRow&) { return {}; }
ItemBaseStats BaseStats(const ItemCompanionRow&) { return {}; }
ItemBaseStats BaseStats(const ItemPetSupplyRow&) { return {}; }
auto VisualKey(const CostumeVisualRow& row) { return std::tuple{row.key,row.characterClass,row.sex}; }
}

std::expected<std::shared_ptr<const ItemCatalog>, ItemCatalogError> BuildItemCatalog(ItemCatalogSource source)
{
    if (source.schemaVersion != 24 || !source.revision) return std::unexpected(Error(ItemCatalogErrorCode::InvalidVersion, "metadata"));
    if (auto result = Budget(source); !result) return std::unexpected(result.error());
    for(auto& p:source.attributeProfiles){std::ranges::sort(p.groups,{},&AttributeGroup::pool);std::ranges::sort(p.candidates,[](const auto& a,const auto& b){return std::pair{a.pool,a.bonus}<std::pair{b.pool,b.bonus};});for(auto& a:p.candidates)std::ranges::sort(a.values,{},&AttributeValue::tier);}
    for(auto& p:source.attributeServices)std::ranges::sort(p.tiers,{},&AttributeTierChance::tier);
    if(auto r=Profiles(source.attributeProfiles,"attribute.profile",ValidAttributeProfile);!r)return std::unexpected(r.error());
    if(auto r=Profiles(source.attributeServices,"attribute.service",ValidAttributeService);!r)return std::unexpected(r.error());
    if(auto r=Profiles(source.socketServiceProfiles,"socket.service",ValidSocketServiceProfile);!r)return std::unexpected(r.error());
    if(auto r=Profiles(source.companionForms,"companion.form",[](const CompanionForm& f){return f.mobTemplateId>0 && detail::AssetReference(f.modelAsset);});!r)return std::unexpected(r.error());
    for(auto& p:source.growthPetProfiles) {
        std::ranges::sort(p.levels,{},&GrowthPetLevel::level);std::ranges::sort(p.stages,{},&GrowthPetStage::stage);
    }
    if(auto r=Profiles(source.growthPetProfiles,"growth_pet.profile",ValidGrowthPetProfile);!r)return std::unexpected(r.error());
    for(const auto& p:source.growthPetProfiles)for(const auto& stage:p.stages)
        if(!FindProfile(source.companionForms,stage.form))return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"growth_pet.form",{},p.id));
    if(auto r=Profiles(source.petHatchProfiles,"pet.hatch",ValidPetHatchProfile);!r)return std::unexpected(r.error());
    std::map<std::uint32_t,std::pair<std::string,PetEffect>> issuedPetSkills;
    std::map<std::string,std::uint32_t> issuedPetSkillCodes;
    for(auto& p:source.petTrainingProfiles) {
        std::ranges::sort(p.ages,{},&PetAgeTier::minimumAgeMs);std::ranges::sort(p.slots,{},&PetSlotRule::slot);
        std::ranges::sort(p.skills,{},&PetSkillDefinition::id);
        std::set<std::string> codes;
        for(auto& s:p.skills) {
            if(!detail::ContentCode(s.code) || !codes.insert(s.code).second)return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"pet.skills",{},p.id));
            std::ranges::sort(s.ranks,{},&PetSkillRank::rank);
            const auto meaning=std::pair{s.code,s.effect};
            if((issuedPetSkills.contains(s.id) && issuedPetSkills.at(s.id)!=meaning) ||
               (issuedPetSkillCodes.contains(s.code) && issuedPetSkillCodes.at(s.code)!=s.id))
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"pet.skills",{},p.id));
            issuedPetSkills.emplace(s.id,meaning);issuedPetSkillCodes.emplace(s.code,s.id);
        }
    }
    if(auto r=Profiles(source.petTrainingProfiles,"pet.training",ValidPetTrainingProfile);!r)return std::unexpected(r.error());
    if(auto r=Profiles(source.petSupplyProfiles,"pet.supply",ValidPetSupplyProfile);!r)return std::unexpected(r.error());
    for(const auto& p:source.petSupplyProfiles) {
        if(p.skin && !FindProfile(source.companionForms,p.skin->form))return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"pet.skin_form",{},p.id));
        if(p.study && !std::ranges::any_of(source.petTrainingProfiles,[&](const auto& t){return std::ranges::any_of(t.skills,[&](const auto& s){return s.id==p.study->skill;});}))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"pet.study_skill",{},p.id));
    }
    std::ranges::sort(source.petSupplies,{},&ItemPetSupplyRow::key);
    std::map<ItemKey,const ItemPetSupplyRow*> petSupplies;
    for(const auto& r:source.petSupplies) {
        if(r.key.Category()!=ItemCategory::Special)return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory,"pet.supply",r.key));
        if(!petSupplies.emplace(r.key,&r).second)return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow,"pet.supply",r.key));
        if(!FindProfile(source.petSupplyProfiles,r.profile))return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"pet.supply",r.key));
    }
    std::ranges::sort(source.companions,{},&ItemCompanionRow::key);
    std::map<ItemKey,const ItemCompanionRow*> companions;
    for(const auto& r:source.companions) {
        if(r.key.Category()!=ItemCategory::Companion)return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory,"companion",r.key));
        if(!companions.emplace(r.key,&r).second)return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow,"companion",r.key));
        if(r.kind==CompanionKind::GrowthPet && r.growthProfile) {
            const auto* p=FindProfile(source.growthPetProfiles,*r.growthProfile);
            if(!p || p->stages.front().form!=r.form)return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"growth_pet.profile",r.key));
        }
        if(r.trainingProfile && (r.kind!=CompanionKind::GrowthPet || !FindProfile(source.petTrainingProfiles,*r.trainingProfile)))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"pet.training",r.key));
        if(!ValidCompanion(r))return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"companion",r.key));
        if(!FindProfile(source.companionForms,r.form) || (r.ridingForm && !FindProfile(source.companionForms,*r.ridingForm)))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"companion.form",r.key));
    }
    for(auto& p:source.soulProfiles)std::ranges::sort(p.levels,{},&SoulLevel::level);
    if(auto r=Profiles(source.soulProfiles,"soul.profile",ValidSoulProfile);!r)return std::unexpected(r.error());
    for(auto& p:source.polymorphProfiles){std::ranges::sort(p.skills,{},&PolymorphSkillRule::skillLevel);std::ranges::sort(p.masteries,{},&PolymorphMasteryRule::mastery);}
    if(auto r=Profiles(source.polymorphProfiles,"polymorph.profile",ValidPolymorphProfile);!r)return std::unexpected(r.error());
    if(auto r=Profiles(source.polymorphForms,"polymorph.form",[](const auto& f){return f.mobTemplate>=8 && f.bookBonus>=PolymorphBonus::None && f.bookBonus<=PolymorphBonus::Movement;});!r)return std::unexpected(r.error());
    for(auto& p:source.toolProfiles)std::ranges::sort(p.levels,{},&ToolLevel::level);
    if(auto r=Profiles(source.toolProfiles,"tool.profile",ValidToolProfile);!r)return std::unexpected(r.error());
    if (auto r = Profiles(source.usePolicies, "use_policy", [](const auto& row) {
        return row.allowedClasses > 0 && row.allowedClasses <= 15 && row.allowedSexes > 0 && row.allowedSexes <= 3;
    }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.tradePolicies, "trade_policy", [](const auto&) { return true; }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.appearances, "appearance", [](const auto& row) { return detail::AssetReference(row.modelAsset); }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.attacks, "attack", [](const auto& row) {
        return row.intervalTicks && row.windupTicks <= row.intervalTicks && row.reachMillimeters && row.maxTargets && row.maxTargets <= 1024;
    }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.ammoPolicies, "ammo", [](const auto& row) {
        return (row.kind == AmmoKind::None && row.unitsPerHit == 0) || (row.kind == AmmoKind::ConsumeStack && row.unitsPerHit > 0);
    }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.resistances, "resistance", [](const auto& row) {
        return std::ranges::all_of(row.Values(), [](auto value) { return value >= -10'000 && value <= 10'000; });
    }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.refinements, "refinement", [](const auto&) { return true; }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.bonuses, "bonus", [](const auto&) { return true; }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.accessorySockets, "accessory_socket", [](const auto&) { return true; }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.recoveries, "recovery", [](const auto&) { return true; }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.consumableCooldowns, "consumable_cooldown", ValidConsumableCooldown); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.lootProfiles, "loot", [](const auto&) { return true; }); !r) return std::unexpected(r.error());

    if (auto r = Profiles(source.timedEffectGroups, "timed_effect_group", ValidTimedEffectGroup); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.timedEffects, "timed_effect", ValidTimedEffectProfile); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.toggleEffectGroups, "toggle_effect_group", ValidToggleEffectGroup); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.toggleEffects, "toggle_effect", ValidToggleEffectProfile); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.skillBookProfiles, "skill_book", ValidSkillBookProfile); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.refinementRecipes, "refinement_recipe", [](const auto&) { return true; }); !r) return std::unexpected(r.error());
    for (auto& recipe : source.refinementRecipes)
        std::ranges::sort(recipe.ingredients, {}, [](const auto& row) { return std::pair{row.key, row.refinementLevel}; });
    for (auto& p : source.skillBookProfiles)
    {
        std::ranges::sort(p.targets, {}, &SkillBookTarget::skillId);
        std::ranges::sort(p.steps, {}, &SkillBookStep::currentLevel);
    }

    auto catalog = std::unique_ptr<ItemCatalog>(new ItemCatalog());
    for (auto& row : source.recoveries)
    {
        const auto id = row.id;
        auto validated = ValidateRecoveryProfile(std::move(row));
        if (!validated)
        {
            auto error = Error(ItemCatalogErrorCode::InvalidRecoveryProfile, "recovery", {}, id);
            error.recoveryError = validated.error();
            return std::unexpected(std::move(error));
        }
        row = validated->Data();
        if (row.cooldownProfile && !FindProfile(source.consumableCooldowns, *row.cooldownProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "recovery.cooldown", {}, *row.cooldownProfile));
        catalog->recoveries_.emplace(id, std::move(*validated));
    }
    for (auto& row : source.lootProfiles)
    {
        const auto id = row.id;
        auto validated = ValidateLootProfile(std::move(row));
        if (!validated)
        {
            auto error = Error(ItemCatalogErrorCode::InvalidLootProfile, "loot", {}, id);
            error.lootError = validated.error();
            return std::unexpected(std::move(error));
        }
        row = validated->Data(); // normalized entry order also feeds canonical hashing
        catalog->loots_.emplace(id, std::move(*validated));
    }
    for (auto& row : source.accessorySockets)
    {
        const auto id = row.id;
        auto validated = ValidateAccessorySocketProfile(std::move(row));
        if (!validated)
        {
            auto error = Error(ItemCatalogErrorCode::InvalidSocketProfile, "accessory_socket", {}, id);
            error.socketError = validated.error();
            return std::unexpected(std::move(error));
        }
        row = validated->Data();
        catalog->accessorySockets_.emplace(row.id, std::move(*validated));
    }
    catalog->bonuses_.reserve(source.bonuses.size());
    for (auto& row : source.bonuses)
    {
        const auto id = row.id;
        auto validated = ValidateBonusProfile(std::move(row));
        if (!validated)
        {
            auto error = Error(ItemCatalogErrorCode::InvalidBonusProfile, "bonus", {}, id);
            error.bonusError = validated.error();
            return std::unexpected(std::move(error));
        }
        row = validated->Data();
        catalog->bonuses_.push_back(std::move(*validated));
    }
    for (const auto& row : source.timedEffects)
    {
        if (!FindProfile(source.timedEffectGroups, row.group))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "timed_effect.group", {}, row.group));
        if (row.cooldownProfile && !FindProfile(source.consumableCooldowns, *row.cooldownProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "timed_effect.cooldown", {}, *row.cooldownProfile));
        const auto* bonus = FindBonusProfile(catalog->bonuses_, row.bonusProfile);
        if (!bonus) return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "timed_effect.bonus", {}, row.bonusProfile));
        if (!AcceptsBonusSource(*bonus, BonusSource::TimedEffect))
            return std::unexpected(Error(ItemCatalogErrorCode::WrongBonusSource, "timed_effect.bonus", {}, row.bonusProfile));
        if (std::ranges::none_of(bonus->Data().entries, [](const auto& b) { return b.valueScaled != 0; }))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "timed_effect.empty_bonus", {}, row.id));
    }
    for (const auto& row : source.toggleEffects)
    {
        if (!FindProfile(source.toggleEffectGroups, row.group))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "toggle_effect.group", {}, row.group));
        if (row.cooldownProfile && !FindProfile(source.consumableCooldowns, *row.cooldownProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "toggle_effect.cooldown", {}, *row.cooldownProfile));
        const auto* bonus = FindBonusProfile(catalog->bonuses_, row.bonusProfile);
        if (!bonus) return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "toggle_effect.bonus", {}, row.bonusProfile));
        if (!AcceptsBonusSource(*bonus, BonusSource::TimedEffect))
            return std::unexpected(Error(ItemCatalogErrorCode::WrongBonusSource, "toggle_effect.bonus", {}, row.bonusProfile));
        if (std::ranges::none_of(bonus->Data().entries, [](const auto& b) { return b.valueScaled != 0; }))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "toggle_effect.empty_bonus", {}, row.id));
    }
    for (auto& row : source.refinements)
    {
        const auto id = row.id;
        auto validated = ValidateRefinementProfile(std::move(row), catalog->bonuses_);
        if (!validated)
        {
            auto error = Error(ItemCatalogErrorCode::InvalidRefinement, "refinement", {}, id);
            error.refinementError = validated.error();
            return std::unexpected(std::move(error));
        }
        row = validated->Data(); // normalized levels also feed canonical hashing
        catalog->refinements_.emplace(row.id, std::move(*validated));
    }
    std::ranges::sort(source.items, {}, &ItemBaseRow::key);
    std::ranges::sort(source.weapons, {}, &ItemWeaponRow::key);
    std::ranges::sort(source.armors, {}, &ItemArmorRow::key);
    std::ranges::sort(source.accessories, {}, &ItemAccessoryRow::key);
    std::ranges::sort(source.materials, {}, &ItemMaterialRow::key);
    std::ranges::sort(source.consumables, {}, &ItemConsumableRow::key);
    std::ranges::sort(source.containers, {}, &ItemContainerRow::key);
    std::ranges::sort(source.containerKeys, {}, &ItemContainerKeyRow::key);
    std::ranges::sort(source.currencyTokens, {}, &ItemCurrencyTokenRow::key);
    std::ranges::sort(source.skillBooks, {}, &ItemSkillBookRow::key);
    std::ranges::sort(source.questItems, {}, &ItemQuestRow::key);
    std::ranges::sort(source.socketStones, {}, &ItemSocketStoneRow::key);
    std::ranges::sort(source.costumes, {}, &ItemCostumeRow::key);
    std::ranges::sort(source.costumeVisuals, {}, VisualKey);
    std::ranges::sort(source.tools,{},&ItemToolRow::key);
    std::ranges::sort(source.fishingItems,{},&ItemFishingRow::key);
    std::ranges::sort(source.polymorphItems,{},&ItemPolymorphRow::key);
    std::ranges::sort(source.soulItems,{},&ItemSoulRow::key);
    std::map<ItemKey,const ItemSoulRow*> soulItems;
    for(const auto& r:source.soulItems)
    {
        if(r.key.Category()!=ItemCategory::Soul)return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory,"soul",r.key));
        if(!soulItems.emplace(r.key,&r).second)return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow,"soul",r.key));
        if(r.kind==SoulKind::Core)
        {
            if(!r.channel || (*r.channel!=SoulChannel::NormalAttack && *r.channel!=SoulChannel::Skill) || !r.profile || r.refineChanceBp)
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"soul",r.key));
            if(!FindProfile(source.soulProfiles,*r.profile))return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"soul",r.key));
        }
        else if(r.kind!=SoulKind::RefinementScroll || r.channel || r.profile || r.refineChanceBp>10000)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"soul",r.key));
    }
    std::map<ItemKey,const ItemPolymorphRow*> polymorphItems;
    for(const auto& r:source.polymorphItems)
    {
        if(r.key.Category()!=ItemCategory::Polymorph)return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory,"polymorph",r.key));
        if(!polymorphItems.emplace(r.key,&r).second)return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow,"polymorph",r.key));
        if(!FindProfile(source.polymorphProfiles,r.profile) || (r.fixedForm && !FindProfile(source.polymorphForms,*r.fixedForm)))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"polymorph",r.key));
    }
    std::map<ItemKey,const ItemToolRow*> tools;
    std::map<ItemKey,const ItemFishingRow*> fishingItems;
    for(const auto& r:source.tools)
    {
        if(r.key.Category()!=ItemCategory::Tool || r.kind<ToolKind::Pick || r.kind>ToolKind::Shears ||
           r.fishingPool<FishingPool::None || r.fishingPool>FishingPool::Second ||
           ((r.kind==ToolKind::Rod)==(r.fishingPool==FishingPool::None)))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"tool",r.key));
        if(!FindProfile(source.toolProfiles,r.profile))return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"tool.profile",r.key,r.profile));
        if(!tools.emplace(r.key,&r).second)return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow,"tool",r.key));
    }
    for(const auto& r:source.fishingItems)
    {
        if(!ValidFishingItem(r))return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"fishing",r.key));
        if(!fishingItems.emplace(r.key,&r).second)return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow,"fishing",r.key));
        if(r.useLootProfile)
        {
            const auto* p=catalog->FindLoot(*r.useLootProfile);
            if(!p)return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"fishing.loot",r.key));
            for(const auto& e:p->Data().entries)if(e.kind!=LootRewardKind::Item || e.rareAttributeChanceBp)
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidLootReward,"fishing.loot",r.key));
        }
    }
    std::set<std::string> codes;
    std::map<ItemKey, const ItemWeaponRow*> weapons;
    std::map<ItemKey, const ItemArmorRow*> armors;
    std::map<ItemKey, const ItemAccessoryRow*> accessories;
    std::map<ItemKey, const ItemMaterialRow*> materials;
    std::map<ItemKey, const ItemConsumableRow*> consumables;
    std::map<ItemKey, const ItemContainerRow*> containers;
    std::map<ItemKey, const ItemContainerKeyRow*> containerKeys;
    std::map<ItemKey, const ItemCurrencyTokenRow*> currencyTokens;
    std::map<ItemKey, const ItemSkillBookRow*> skillBooks;
    std::map<ItemKey, const ItemQuestRow*> questItems;
    std::map<ItemKey, const ItemSocketStoneRow*> socketStones;
    std::map<ItemKey, const ItemCostumeRow*> costumes;
    std::set<std::tuple<ItemKey,std::uint8_t,std::uint8_t>> visuals;
    for(const auto& row:source.costumes)
    {
        if(!ValidCostume(row))return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"costume",row.key));
        if(!costumes.emplace(row.key,&row).second)return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow,"costume",row.key));
    }
    for(const auto& row:source.costumeVisuals)
    {
        if(!costumes.contains(row.key))return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow,"costume.visual",row.key));
        if(row.characterClass<1 || row.characterClass>4 || row.sex<1 || row.sex>2 || !detail::AssetReference(row.modelAsset))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"costume.visual",row.key));
        if(!visuals.insert(VisualKey(row)).second)return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow,"costume.visual",row.key));
    }
    for (const auto& row : source.weapons)
    {
        if (row.key.Category() != ItemCategory::Weapon) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "weapon", row.key));
        if (!weapons.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "weapon", row.key));
        if (row.slot != EquipmentSlot::Weapon || row.subtype < WeaponSubtype::Sword || row.subtype > WeaponSubtype::MountSpear)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "weapon.type", row.key));
        const auto* ammo = FindProfile(source.ammoPolicies, row.ammoPolicy);
        if (!ammo || !FindProfile(source.attacks, row.attackProfile)) return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "weapon", row.key));
        if ((row.subtype == WeaponSubtype::Bow) != (ammo->kind == AmmoKind::ConsumeStack))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "weapon.ammo", row.key));
    }
    for (const auto& row : source.armors)
    {
        if (row.key.Category() != ItemCategory::Armor) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "armor", row.key));
        if (!armors.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "armor", row.key));
        if (row.slot < EquipmentSlot::Body || row.slot > EquipmentSlot::Feet)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "armor.slot", row.key));
        if (!FindProfile(source.resistances, row.resistanceProfile)) return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "armor.resistance", row.key));
    }
    for (const auto& row : source.accessories)
    {
        if (row.key.Category() != ItemCategory::Accessory) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "accessory", row.key));
        if (!accessories.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "accessory", row.key));
        if (row.slot < EquipmentSlot::Wrist || row.slot > EquipmentSlot::Ear)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "accessory.slot", row.key));
        if (!FindProfile(source.resistances, row.resistanceProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "accessory.resistance", row.key));
        if (row.socketProfile && !catalog->FindAccessorySocket(*row.socketProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "accessory.socket", row.key, *row.socketProfile));
    }
    for (const auto& row : source.materials)
    {
        if (row.key.Category() != ItemCategory::Material) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "material", row.key));
        if (!materials.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "material", row.key));
        if (row.kind < MaterialKind::Leather || row.kind > MaterialKind::Jewel)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "material.kind", row.key));
    }
    for (const auto& row : source.consumables)
    {
        if (row.key.Category() != ItemCategory::Consumable) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "consumable", row.key));
        if (!consumables.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "consumable", row.key));
        if (unsigned(row.recoveryProfile != 0) + row.timedEffectProfile.has_value() + row.toggleEffectProfile.has_value() + row.socketServiceProfile.has_value() + row.attributeServiceProfile.has_value() != 1 ||
            row.timedEffectProfile == 0 || row.toggleEffectProfile == 0 || row.socketServiceProfile == 0 || row.attributeServiceProfile == 0)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "consumable.effect", row.key));
        if(row.attributeServiceProfile && !FindProfile(source.attributeServices,*row.attributeServiceProfile))return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"attribute.service",row.key,*row.attributeServiceProfile));
        if (row.socketServiceProfile && !FindProfile(source.socketServiceProfiles,*row.socketServiceProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"socket.service",row.key,*row.socketServiceProfile));
        if (row.recoveryProfile && !catalog->FindRecovery(row.recoveryProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "consumable.recovery", row.key, row.recoveryProfile));
        if (row.timedEffectProfile && !FindProfile(source.timedEffects, *row.timedEffectProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "consumable.timed_effect", row.key, *row.timedEffectProfile));
        if (row.toggleEffectProfile && !FindProfile(source.toggleEffects, *row.toggleEffectProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "consumable.toggle_effect", row.key, *row.toggleEffectProfile));
    }
    for (const auto& row : source.containers)
    {
        if (row.key.Category() != ItemCategory::Container) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "container", row.key));
        if (!containers.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "container", row.key));
        const bool scripted = row.kind == ContainerKind::Scripted || row.kind==ContainerKind::PetEgg;
        if (row.kind < ContainerKind::Use || row.kind > ContainerKind::PetEgg || scripted == row.lootProfile.has_value() ||
            (row.kind==ContainerKind::PetEgg)!=row.hatchProfile.has_value() ||
            (row.kind == ContainerKind::Locked) != (row.lockId > 0))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidContainer, "container.kind", row.key));
        if(row.hatchProfile && !FindProfile(source.petHatchProfiles,*row.hatchProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"pet.hatch",row.key));
        if (row.lootProfile && !catalog->FindLoot(*row.lootProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "container.loot", row.key, *row.lootProfile));
    }
    for (const auto& row : source.containerKeys)
    {
        if (row.key.Category() != ItemCategory::ContainerKey) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "container_key", row.key));
        if (!containerKeys.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "container_key", row.key));
        if (!row.lockId) return std::unexpected(Error(ItemCatalogErrorCode::InvalidContainer, "container_key.lock", row.key));
    }
    for (const auto& row : source.currencyTokens)
    {
        if (row.key.Category() != ItemCategory::CurrencyToken) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "currency_token", row.key));
        if (!currencyTokens.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "currency_token", row.key));
        if (row.currency < WalletCurrency::Gold || row.currency > WalletCurrency::DragonCoin ||
            !row.amountPerUnit || row.amountPerUnit > MaxTokenUnitAmount)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "currency_token", row.key));
    }
    std::optional<ItemKey> previous;
    for (const auto& row : source.skillBooks)
    {
        if (row.key.Category()!=ItemCategory::SkillBook) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory,"skill_book",row.key));
        if (!skillBooks.emplace(row.key,&row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow,"skill_book",row.key));
        if (!FindProfile(source.skillBookProfiles,row.profile)) return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile,"skill_book",row.key,row.profile));
    }
    for (const auto& row : source.questItems)
    {
        if(row.key.Category()!=ItemCategory::Quest)return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory,"quest_item",row.key));
        if(!questItems.emplace(row.key,&row).second)return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow,"quest_item",row.key));
        if(!ValidQuestItem(row))return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"quest_item",row.key));
    }
    for(const auto& row:source.socketStones)
    {
        if(row.key.Category()!=ItemCategory::SocketStone)return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory,"socket_stone",row.key));
        if(!socketStones.emplace(row.key,&row).second)return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow,"socket_stone",row.key));
        if(!ValidSocketStone(row))return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"socket_stone",row.key));
    }
    for (const auto& row : source.items)
    {
        if (previous == row.key) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateItem, "item_base", row.key));
        previous = row.key;
        const auto category = row.key.Category();
        const bool material = category == ItemCategory::Material;
        const bool consumable = category == ItemCategory::Consumable;
        const bool container = category == ItemCategory::Container;
        const bool containerKey = category == ItemCategory::ContainerKey;
        const bool currencyToken = category == ItemCategory::CurrencyToken;
        const bool skillBook = category == ItemCategory::SkillBook;
        const bool questItem = category == ItemCategory::Quest;
        const bool socketStone = category == ItemCategory::SocketStone;
        const bool costume = category == ItemCategory::Costume;
        const bool tool = category == ItemCategory::Tool, fish = category == ItemCategory::Fishing;
        const bool polymorph = category == ItemCategory::Polymorph;
        const bool soul = category == ItemCategory::Soul, companion = category == ItemCategory::Companion;
        const bool petSupply=category==ItemCategory::Special;
        const bool stackable = petSupply || material || consumable || container || containerKey || currencyToken || skillBook || questItem || socketStone || fish || polymorph;
        if (category != ItemCategory::Weapon && category != ItemCategory::Armor && category != ItemCategory::Accessory && !costume && !tool && !soul && !companion && !stackable)
            return std::unexpected(Error(ItemCatalogErrorCode::UnsupportedCategory, "item_base", row.key));
        if (!detail::ContentCode(row.contentCode) || !detail::ContentCode(row.nameKey, 128) ||
            !detail::ContentCode(row.descriptionKey, 128) || !detail::AssetReference(row.iconAsset) ||
            !row.width || !row.height || !row.maxStack || (!stackable && !soul && row.maxStack != 1) ||
            row.binding < BindingPolicy::None || row.binding > BindingPolicy::OnEquip ||
            row.currency != ItemCurrency::Gold || row.buyAmount > INT64_MAX || row.sellAmount > INT64_MAX)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase, "item_base", row.key));
        if (!codes.insert(row.contentCode).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateContentCode, "content_code", row.key));
        // Quest/stone appearance is a ground-item visual only. Stone bonuses
        // are resolved for installed stones; the inventory stone is not gear.
        if ((stackable || soul) && (row.binding == BindingPolicy::OnEquip || (row.appearanceProfile && !questItem && !socketStone) || (row.baseBonusProfile && !socketStone)))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase, "non_equipment.equipment_state", row.key));
        if(companion && (row.appearanceProfile || (row.binding==BindingPolicy::OnEquip && companions.contains(row.key) && companions.at(row.key)->kind!=CompanionKind::Mount)))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase,"companion.equipment_state",row.key));
        if (row.baseBonusProfile)
        {
            const auto* bonus = catalog->FindBonus(*row.baseBonusProfile);
            if (!bonus) return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "item_base.bonus", row.key, *row.baseBonusProfile));
            if (!AcceptsBonusSource(*bonus, BonusSource::ItemBase))
                return std::unexpected(Error(ItemCatalogErrorCode::WrongBonusSource, "item_base.bonus", row.key, *row.baseBonusProfile));
        }
        if (!FindProfile(source.usePolicies, row.usePolicy) || !FindProfile(source.tradePolicies, row.tradePolicy) ||
            (row.appearanceProfile && !FindProfile(source.appearances, *row.appearanceProfile)))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "item_base", row.key));
        const auto* profile = catalog->FindRefinement(row.refinementProfile);
        if (!profile) return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "refinement", row.key, row.refinementProfile));
        const auto expectedKind = category == ItemCategory::Weapon ? RefinementKind::Weapon :
            category == ItemCategory::Armor ? RefinementKind::Armor :
            category == ItemCategory::Accessory ? RefinementKind::Accessory :
            companion ? (companions.contains(row.key) && companions.at(row.key)->kind==CompanionKind::GrowthPet ? RefinementKind::None : RefinementKind::Companion) : socketStone ? RefinementKind::SocketStone : costume ? RefinementKind::Costume : tool ? RefinementKind::Tool : soul && soulItems.contains(row.key) && soulItems.at(row.key)->kind==SoulKind::Core ? RefinementKind::Soul : RefinementKind::None;
        if (profile->Data().kind != RefinementKind::None && profile->Data().kind != expectedKind)
            return std::unexpected(Error(ItemCatalogErrorCode::WrongRefinementKind, "refinement", row.key, row.refinementProfile));
        std::optional<ItemCategoryRow> child;
        if(petSupply && petSupplies.contains(row.key)) {
            const auto* p=FindProfile(source.petSupplyProfiles,petSupplies.at(row.key)->profile);child=*petSupplies.at(row.key);
            if(row.baseBonusProfile || ((p->kind==PetSupplyKind::Carrier || p->kind==PetSupplyKind::Skin) && row.maxStack!=1))
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase,"pet.supply_state",row.key));
        }
        if(companion && companions.contains(row.key))child=*companions.at(row.key);
        if(soul && soulItems.contains(row.key))
        {
            const auto* s=soulItems.at(row.key);child=*s;
            if(s->kind==SoulKind::Core && (row.maxStack!=1 || FindProfile(source.soulProfiles,*s->profile)->levels.size()!=std::size_t(profile->Data().maxLevel)+1))
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"soul.levels",row.key));
            if(s->kind==SoulKind::RefinementScroll && profile->Data().kind!=RefinementKind::None)
                return std::unexpected(Error(ItemCatalogErrorCode::WrongRefinementKind,"soul.scroll",row.key));
        }
        if (category == ItemCategory::Weapon && weapons.contains(row.key)) child = *weapons.at(row.key);
        if (category == ItemCategory::Armor && armors.contains(row.key)) child = *armors.at(row.key);
        if (category == ItemCategory::Accessory && accessories.contains(row.key)) child = *accessories.at(row.key);
        if (costume && costumes.contains(row.key))
        {
            child=*costumes.at(row.key);
            const auto* policy=FindProfile(source.usePolicies,row.usePolicy);
            for(std::uint8_t c=1;c<=4;++c)for(std::uint8_t s=1;s<=2;++s)
                if((policy->allowedClasses&(1u<<(c-1))) && (policy->allowedSexes&(1u<<(s-1))) && !visuals.contains({row.key,c,s}))
                    return std::unexpected(Error(ItemCatalogErrorCode::MissingCategoryRow,"costume.visual",row.key));
        }
        if(tool && tools.contains(row.key))
        {
            child=*tools.at(row.key);
            if(FindProfile(source.toolProfiles,tools.at(row.key)->profile)->levels.size()!=std::size_t(profile->Data().maxLevel)+1)
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"tool.levels",row.key));
        }
        if(fish && fishingItems.contains(row.key))child=*fishingItems.at(row.key);
        if(polymorph && polymorphItems.contains(row.key))
        {
            child=*polymorphItems.at(row.key);
            if(FindProfile(source.polymorphProfiles,polymorphItems.at(row.key)->profile)->kind==PolymorphKind::PracticeBook && row.maxStack!=1)
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase,"polymorph.book_stack",row.key));
        }
        if (material && materials.contains(row.key)) child = *materials.at(row.key);
        if (consumable && consumables.contains(row.key)) child = *consumables.at(row.key);
        if (container && containers.contains(row.key)) child = *containers.at(row.key);
        if (containerKey && containerKeys.contains(row.key)) child = *containerKeys.at(row.key);
        if (currencyToken && currencyTokens.contains(row.key)) child = *currencyTokens.at(row.key);
        if (questItem && questItems.contains(row.key)) child = *questItems.at(row.key);
        if (socketStone && socketStones.contains(row.key))
        {
            child=*socketStones.at(row.key);
            if(socketStones.at(row.key)->kind==SocketStoneKind::BrokenShard && (row.baseBonusProfile || profile->Data().maxLevel))
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase,"socket_stone.broken_effect",row.key));
        }
        if (skillBook && skillBooks.contains(row.key))
        {
            child = *skillBooks.at(row.key);
            if (FindProfile(source.skillBookProfiles,skillBooks.at(row.key)->profile)->unitsPerUse>row.maxStack)
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase,"skill_book.stack_below_use_units",row.key));
        }
        if (!child) return std::unexpected(Error(ItemCatalogErrorCode::MissingCategoryRow, "category", row.key));
        if (consumable)
        {
            const auto& c = *consumables.at(row.key);
            if (c.toggleEffectProfile && row.maxStack != 1)
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase, "consumable.toggle_stack", row.key));
            const auto units = c.timedEffectProfile ? FindProfile(source.timedEffects, *c.timedEffectProfile)->unitsPerUse
                                                   : c.attributeServiceProfile ? FindProfile(source.attributeServices,*c.attributeServiceProfile)->units
                                                   : c.socketServiceProfile ? FindProfile(source.socketServiceProfiles,*c.socketServiceProfile)->unitsPerUse
                                                   : c.toggleEffectProfile ? 1 : catalog->FindRecovery(c.recoveryProfile)->Data().unitsPerUse;
            if (units > row.maxStack) return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase, "consumable.stack_below_use_units", row.key));
            if (c.recoveryProfile && std::holds_alternative<AutomaticRecovery>(catalog->FindRecovery(c.recoveryProfile)->Data().effect) && row.maxStack != 1)
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase, "consumable.automatic_stack", row.key));
        }
        const auto base = std::visit([](const auto& value) { return BaseStats(value); }, *child);
        // Gains are monotone; the maximum level proves the sum bound at all levels.
        if (!ResolveRefinedBaseStats(base, *profile, profile->Data().maxLevel))
            return std::unexpected(Error(ItemCatalogErrorCode::StatOutOfRange, "base_plus_gain", row.key));
        catalog->items_.emplace(row.key, ItemCatalogEntry{row, std::move(*child)});
    }
    for(const auto& row:source.petSupplies)
        if(!catalog->Find(row.key))return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow,"pet.supply",row.key));
    for(const auto& row:source.companions)
        if(!catalog->Find(row.key))return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow,"companion",row.key));
    for(const auto& row:source.soulItems)
        if(!catalog->Find(row.key))return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow,"soul",row.key));
    for (const auto& row : source.weapons)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "weapon", row.key));
    for (const auto& row : source.armors)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "armor", row.key));
    for (const auto& row : source.accessories)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "accessory", row.key));
    for (const auto& row : source.materials)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "material", row.key));
    for (const auto& row : source.consumables)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "consumable", row.key));
    for (const auto& row : source.containers)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "container", row.key));
    for (const auto& row : source.containerKeys)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "container_key", row.key));
    for (const auto& row : source.currencyTokens)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "currency_token", row.key));
    for (const auto& row : source.skillBooks)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow,"skill_book",row.key));
    for (const auto& row : source.questItems)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow,"quest_item",row.key));
    for(const auto& row:source.socketStones)
        if(!catalog->Find(row.key))return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow,"socket_stone",row.key));
    for(const auto& row:source.costumes)
        if(!catalog->Find(row.key))return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow,"costume",row.key));
    for(const auto& row:source.tools)
        if(!catalog->Find(row.key))return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow,"tool",row.key));
    for(const auto& row:source.fishingItems)
    {
        if(!catalog->Find(row.key))return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow,"fishing",row.key));
        if(row.grillResult)
        {
            const auto* target=catalog->Find(*row.grillResult);
            if(!target || target->base.retired || row.grillResult->Category()!=ItemCategory::Consumable)
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"fishing.grill_result",row.key));
        }
    }
    for(const auto& row:source.polymorphItems)
        if(!catalog->Find(row.key))return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow,"polymorph",row.key));
    for(const auto& profile:source.petHatchProfiles) {
        const auto* target=catalog->Find(profile.target);
        const auto* pet=target ? std::get_if<ItemCompanionRow>(&target->category) : nullptr;
        if(!pet || pet->kind!=CompanionKind::GrowthPet || target->base.retired)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"pet.hatch_target",profile.target,profile.id));
    }
    auto foodKey=[](const PetFeedRule& r){return std::pair{r.key,r.service};};
    std::ranges::sort(source.petFeedRules,{},foodKey);
    std::optional<std::pair<ItemKey,PetFeedingService>> previousFood;
    for(const auto& rule:source.petFeedRules) {
        const auto* item=catalog->Find(rule.key);
        if(!item || !ValidPetFeedRule(rule) || (previousFood && *previousFood==foodKey(rule)) ||
           (rule.consumption==PetFoodConsumption::ExactUnits && rule.units>item->base.maxStack) ||
           std::holds_alternative<ItemCompanionRow>(item->category) ||
           (rule.kind==PetFoodKind::EquipmentExperience && rule.key.Category()!=ItemCategory::Weapon && rule.key.Category()!=ItemCategory::Armor))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"pet.feed",rule.key));
        previousFood=foodKey(rule);
    }
    // Item rewards are ordinary, live, stack-sized grants of a published item.
    for (const auto& profile : source.lootProfiles)
        for (const auto& reward : profile.entries)
        {
            if (reward.kind != LootRewardKind::Item) continue;
            const auto* target = catalog->Find(*reward.item);
            if (!target || target->base.retired || reward.amount > target->base.maxStack)
            {
                auto error = Error(ItemCatalogErrorCode::InvalidLootReward, "loot.item", *reward.item, profile.id);
                return std::unexpected(std::move(error));
            }
            if(const auto* poly=std::get_if<ItemPolymorphRow>(&target->category);poly &&
               (!poly->fixedForm || FindProfile(source.polymorphProfiles,poly->profile)->kind==PolymorphKind::PracticeBook))
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidLootReward,"loot.polymorph_binding",*reward.item,profile.id));
        }
    for (const auto& profile : source.accessorySockets)
    {
        for (const auto& key : profile.unlockMaterials)
            if (!materials.contains(key)) return std::unexpected(Error(ItemCatalogErrorCode::InvalidSocketMaterial, "socket.unlock_material", key, profile.id));
        for (const auto& key : profile.chargeMaterials)
            if (!materials.contains(key) || materials.at(key)->kind != MaterialKind::Jewel)
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidSocketMaterial, "socket.charge_material", key, profile.id));
        for (const auto& key : profile.permanentChargeMaterials)
            if (!materials.contains(key) || materials.at(key)->kind != MaterialKind::Jewel)
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidSocketMaterial, "socket.permanent_charge_material", key, profile.id));
    }
    for (const auto& recipe : source.refinementRecipes)
        if (!ValidateRefinementRecipe(recipe, *catalog))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidRefinementRecipe, "refinement_recipe", recipe.source, recipe.id));
    std::ranges::sort(source.attributeBindings,{},&ItemAttributeBinding::key);
    std::optional<ItemKey> priorAttributeKey;
    for(const auto& b:source.attributeBindings){
        const auto* item=catalog->Find(b.key);const auto category=static_cast<unsigned>(b.key.Category());
        if(priorAttributeKey==b.key || !item || category<1 || category>4 || !FindProfile(source.attributeProfiles,b.profile) || item->base.maxStack!=1)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile,"attribute.binding",b.key,b.profile));
        priorAttributeKey=b.key;
    }
    auto digest = detail::CanonicalCatalogHash(source);
    if (!digest) return std::unexpected(Error(digest.error(), "content_hash"));
    catalog->digest_ = *digest;
    catalog->source_ = std::move(source);
    return std::shared_ptr<const ItemCatalog>(std::move(catalog));
}

const ItemCatalogEntry* ItemCatalog::Find(ItemKey key) const noexcept
{
    const auto found = items_.find(key);
    return found == items_.end() ? nullptr : &found->second;
}
const CostumeVisualRow* ItemCatalog::FindCostumeVisual(ItemKey key,std::uint8_t characterClass,std::uint8_t sex) const noexcept
{
    const auto wanted=std::tuple{key,characterClass,sex};
    const auto it=std::ranges::lower_bound(source_.costumeVisuals,wanted,{},VisualKey);
    return it!=source_.costumeVisuals.end() && VisualKey(*it)==wanted ? &*it : nullptr;
}
const ValidatedRefinementProfile* ItemCatalog::FindRefinement(std::uint32_t id) const noexcept
{
    const auto found = refinements_.find(id);
    return found == refinements_.end() ? nullptr : &found->second;
}
const PetTrainingProfile* ItemCatalog::FindPetTrainingProfile(std::uint32_t id) const noexcept {return FindProfile(source_.petTrainingProfiles,id);}
const PetSupplyProfile* ItemCatalog::FindPetSupplyProfile(std::uint32_t id) const noexcept {return FindProfile(source_.petSupplyProfiles,id);}
const PetHatchProfile* ItemCatalog::FindPetHatchProfile(std::uint32_t id) const noexcept { return FindProfile(source_.petHatchProfiles,id); }
const PetFeedRule* ItemCatalog::FindPetFeedRule(ItemKey key,PetFeedingService service) const noexcept {
    const auto wanted=std::pair{key,service};
    const auto projection=[](const PetFeedRule& r){return std::pair{r.key,r.service};};
    const auto found=std::ranges::lower_bound(source_.petFeedRules,wanted,{},projection);
    return found!=source_.petFeedRules.end() && projection(*found)==wanted ? &*found : nullptr;
}
const GrowthPetProfile* ItemCatalog::FindGrowthPetProfile(std::uint32_t id) const noexcept { return FindProfile(source_.growthPetProfiles,id); }
const CompanionForm* ItemCatalog::FindCompanionForm(std::uint32_t id) const noexcept { return FindProfile(source_.companionForms,id); }
const SoulProfile* ItemCatalog::FindSoulProfile(std::uint32_t id) const noexcept { return FindProfile(source_.soulProfiles,id); }
const PolymorphForm* ItemCatalog::FindPolymorphForm(std::uint32_t id) const noexcept { return FindProfile(source_.polymorphForms,id); }
const PolymorphProfile* ItemCatalog::FindPolymorphProfile(std::uint32_t id) const noexcept { return FindProfile(source_.polymorphProfiles,id); }
const ToolProfile* ItemCatalog::FindToolProfile(std::uint32_t id) const noexcept { return FindProfile(source_.toolProfiles,id); }
const ItemUsePolicy* ItemCatalog::FindUsePolicy(std::uint32_t id) const noexcept { return FindProfile(source_.usePolicies, id); }
const RefinementRecipe* ItemCatalog::FindRefinementRecipe(std::uint32_t id) const noexcept { return FindProfile(source_.refinementRecipes, id); }
const ItemTradePolicy* ItemCatalog::FindTradePolicy(std::uint32_t id) const noexcept { return FindProfile(source_.tradePolicies, id); }
const ItemAppearanceProfile* ItemCatalog::FindAppearance(std::uint32_t id) const noexcept { return FindProfile(source_.appearances, id); }
const ItemAttackProfile* ItemCatalog::FindAttack(std::uint32_t id) const noexcept { return FindProfile(source_.attacks, id); }
const ItemAmmoPolicy* ItemCatalog::FindAmmoPolicy(std::uint32_t id) const noexcept { return FindProfile(source_.ammoPolicies, id); }
const ItemResistanceProfile* ItemCatalog::FindResistance(std::uint32_t id) const noexcept { return FindProfile(source_.resistances, id); }
const ValidatedBonusProfile* ItemCatalog::FindBonus(std::uint32_t id) const noexcept
{
    // The builder has already established a sorted, unique, nonempty profile set.
    const auto found = std::ranges::lower_bound(bonuses_, id, {}, [](const auto& row) { return row.Data().id; });
    return found != bonuses_.end() && found->Data().id == id ? &*found : nullptr;
}
const ValidatedAccessorySocketProfile* ItemCatalog::FindAccessorySocket(std::uint32_t id) const noexcept
{
    const auto found = accessorySockets_.find(id);
    return found == accessorySockets_.end() ? nullptr : &found->second;
}
const AttributeProfile* ItemCatalog::FindItemAttributes(ItemKey key) const noexcept
{
    const auto it=std::ranges::lower_bound(source_.attributeBindings,key,{},&ItemAttributeBinding::key);
    return it==source_.attributeBindings.end() || it->key!=key ? nullptr : FindProfile(source_.attributeProfiles,it->profile);
}
const AttributeServiceProfile* ItemCatalog::FindAttributeService(std::uint32_t id) const noexcept {return FindProfile(source_.attributeServices,id);}
const SocketServiceProfile* ItemCatalog::FindSocketService(std::uint32_t id) const noexcept
{ return FindProfile(source_.socketServiceProfiles,id); }
const ValidatedRecoveryProfile* ItemCatalog::FindRecovery(std::uint32_t id) const noexcept
{
    const auto found = recoveries_.find(id);
    return found == recoveries_.end() ? nullptr : &found->second;
}
const ConsumableCooldownProfile* ItemCatalog::FindConsumableCooldown(std::uint32_t id) const noexcept
{
    return FindProfile(source_.consumableCooldowns, id);
}
const ValidatedLootProfile* ItemCatalog::FindLoot(std::uint32_t id) const noexcept
{
    const auto found = loots_.find(id);
    return found == loots_.end() ? nullptr : &found->second;
}
const SkillBookProfile* ItemCatalog::FindSkillBook(std::uint32_t id) const noexcept
{
    return FindProfile(source_.skillBookProfiles,id);
}
const ToggleEffectGroup* ItemCatalog::FindToggleEffectGroup(std::uint32_t id) const noexcept
{
    return FindProfile(source_.toggleEffectGroups, id);
}
const ToggleEffectProfile* ItemCatalog::FindToggleEffect(std::uint32_t id) const noexcept
{
    return FindProfile(source_.toggleEffects, id);
}
const TimedEffectGroup* ItemCatalog::FindTimedEffectGroup(std::uint32_t id) const noexcept
{
    return FindProfile(source_.timedEffectGroups, id);
}
const TimedEffectProfile* ItemCatalog::FindTimedEffect(std::uint32_t id) const noexcept
{
    return FindProfile(source_.timedEffects, id);
}
std::expected<ItemBonusProfiles, ItemCatalogLookupError> ItemCatalog::BonusProfilesAt(ItemKey key, std::uint16_t level) const noexcept
{
    const auto* item = Find(key);
    if (!item) return std::unexpected(ItemCatalogLookupError::MissingItem);
    const auto* step = FindRefinement(item->base.refinementProfile)->FindLevel(level);
    if (!step) return std::unexpected(ItemCatalogLookupError::InvalidRefinementLevel);
    return ItemBonusProfiles{
        item->base.baseBonusProfile ? FindBonus(*item->base.baseBonusProfile) : nullptr,
        step->extraBonusProfile ? FindBonus(*step->extraBonusProfile) : nullptr};
}
std::expected<ItemBaseStats, ItemCatalogLookupError> ItemCatalog::RefinedBaseStats(ItemKey key, std::uint16_t level) const noexcept
{
    const auto* item = Find(key);
    if (!item) return std::unexpected(ItemCatalogLookupError::MissingItem);
    const auto* profile = FindRefinement(item->base.refinementProfile);
    const auto base = std::visit([](const auto& row) { return BaseStats(row); }, item->category);
    const auto result = ResolveRefinedBaseStats(base, *profile, level);
    if (!result) return std::unexpected(ItemCatalogLookupError::InvalidRefinementLevel);
    return *result;
}
} // namespace auriga::item
