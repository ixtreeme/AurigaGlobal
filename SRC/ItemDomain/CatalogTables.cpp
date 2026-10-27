#include "CatalogSnapshot.hpp"
#include <algorithm>

namespace auriga::item::sql
{
namespace
{
constexpr auto Profiles = ItemCatalogLimits::ProfilesPerKind;
constexpr auto Items = ItemCatalogLimits::Items;
constexpr std::array<TableSpec, TableCount> Specs {{
    {Table::Format, "catalog_format", "singleton,schema_version,canonical_domain", 1},
    {Table::Categories, "item_category", "category_id,code,prototype_table", ItemCategories.size()},
    {Table::BonusDefinitions, "bonus_definition", "bonus_id,code,display_key,unit_kind,target_kind,stage_kind,channel_filter,action_filter,mode_filter,family_filter,rank_filter,element_filter,stack_policy,aggregate_cap,allowed_sources,minimum_value,maximum_value", BonusDefinitions.size()},
    {Table::UsePolicies, "item_use_policy", "policy_id,code,allowed_classes,allowed_sexes,use_during_combat", Profiles},
    {Table::TradePolicies, "item_trade_policy", "policy_id,code,allow_trade,allow_drop,allow_sell", Profiles},
    {Table::Appearances, "item_appearance_profile", "profile_id,code,model_asset", Profiles},
    {Table::Attacks, "item_attack_profile", "profile_id,code,interval_ticks,windup_ticks,reach_millimeters,max_targets", Profiles},
    {Table::AmmoPolicies, "item_ammo_policy", "policy_id,code,ammo_kind,units_per_hit", Profiles},
    {Table::Resistances, "item_resistance_profile", "profile_id,code,physical_bp,magical_bp,sword_bp,dagger_bp,bow_bp,two_handed_bp,bell_bp,fan_bp,mount_spear_bp,fire_bp,electric_bp,ice_bp,wind_bp,earth_bp,dark_bp", Profiles},
    {Table::BonusProfiles, "bonus_profile", "profile_id,code,source_kind", Profiles},
    {Table::BonusEntries, "bonus_profile_entry", "profile_id,bonus_id,value_scaled", Profiles * MaxBonusEntriesPerProfile},
    {Table::Refinements, "refinement_profile", "profile_id,code,profile_kind,max_level", Profiles},
    {Table::RefinementLevels, "refinement_level", "profile_id,level,physical_attack_gain_raw,magical_attack_gain_raw,physical_defense_gain_raw,magical_defense_gain_raw,extra_bonus_profile_id", Profiles * 256},
    {Table::Items, "item_base", "category_id,local_id,content_code,name_key,description_key,icon_asset,inventory_width,inventory_height,max_stack,weight_grams,min_level,binding_policy,use_policy_id,trade_policy_id,refinement_profile_id,appearance_profile_id,base_bonus_profile_id,currency_kind,buy_amount,sell_amount,retired", Items},
    {Table::Weapons, "item_weapon", "category_id,local_id,weapon_subtype,equip_slot,physical_attack_base_raw,magical_attack_base_raw,attack_profile_id,ammo_policy_id", Items},
    {Table::Armors, "item_armor", "category_id,local_id,equip_slot,physical_defense_base_raw,magical_defense_base_raw,resistance_profile_id", Items},
    {Table::Accessories, "item_accessory", "category_id,local_id,equip_slot,resistance_profile_id,socket_profile_id", Items},
    {Table::Materials, "item_material", "category_id,local_id,material_kind", Items},
    {Table::SocketProfiles, "accessory_socket_profile", "profile_id,code,max_grade,unlock_units_per_attempt,charge_units_per_attempt", Profiles},
    {Table::SocketGrades, "accessory_socket_grade", "profile_id,grade,duration_ms,bonus_increase_bp,minimum_raw,minimum_bp,minimum_integer,unlock_chance_bp,charge_chance_bp", Profiles * 256},
    {Table::SocketMaterials, "accessory_socket_material", "profile_id,operation_kind,category_id,local_id", Profiles * MaxAccessorySocketMaterials * 3},
    {Table::Cooldowns, "consumable_cooldown_profile", "profile_id,code,duration_ms", Profiles},
    {Table::Recoveries, "recovery_profile", "profile_id,code,delivery_kind,units_per_use,allowed_contexts,allow_during_pvp,cooldown_profile_id", Profiles},
    {Table::Instant, "instant_recovery_profile", "profile_id,delivery_kind,hp_raw,sp_raw,max_hp_bp,max_sp_bp,flat_bonus_cap_bp", Profiles},
    {Table::Reserve, "reserve_recovery_profile", "profile_id,delivery_kind,hp_raw,sp_raw,flat_bonus_cap_bp,tick_interval_ms,max_pool_per_tick_bp", Profiles},
    {Table::Periodic, "periodic_recovery_profile", "profile_id,delivery_kind,hp_per_tick_raw,sp_per_tick_raw,tick_interval_ms,duration_ms", Profiles},
    {Table::Consumables, "item_consumable", "category_id,local_id,recovery_profile_id", Items},
    {Table::LootProfiles, "loot_profile", "profile_id,code,pick_mode", Profiles},
    {Table::LootEntries, "loot_entry", "profile_id,entry_no,reward_kind,item_category_id,item_local_id,amount,weight,rare_attribute_chance_bp", Profiles * MaxLootEntries},
    {Table::Containers, "item_container", "category_id,local_id,container_kind,loot_profile_id,lock_id", Items},
    {Table::ContainerKeys, "item_container_key", "category_id,local_id,lock_id", Items},
}};
static_assert([] { for (std::size_t i = 0; i < Specs.size(); ++i) if (static_cast<std::size_t>(Specs[i].table) != i) return false; return true; }());
}
std::span<const TableSpec, TableCount> Tables() noexcept { return Specs; }
std::size_t ColumnCount(const TableSpec& spec) noexcept { return 1 + std::ranges::count(spec.columns, ','); }
} // namespace auriga::item::sql
