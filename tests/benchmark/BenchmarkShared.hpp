#pragma once

// Shared model for the isolated PvP benchmark.  The benchmark links the real
// combat/battle.cpp, PointSystem.cpp, StatSystem.cpp and constants.cpp, and
// replaces only the live-service leaf systems (network, DB, quests, party,
// affect storage, mount storage access) with deterministic doubles.

#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/entity/char.h"
#include "../../SRC/Server/GameServer/core/constants.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/character_stats_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/vital_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/skill_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/combat_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/item_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace bench
{

struct ItemRow
{
	uint32_t vnum { 0 };
	std::string name;
	int type { 0 };
	int subtype { 0 };
	uint32_t wearFlag { 0 };
	uint32_t antiFlag { 0 };
	uint32_t immuneFlag { 0 };
	int refineSet { 0 };
	int addonType { 0 };
	int limitLevel { 0 };
	int values[ITEM_VALUES_MAX_NUM] {};
	int applyType[ITEM_APPLY_MAX_NUM] {};
	int applyValue[ITEM_APPLY_MAX_NUM] {};
	int source { 0 }; // 0 main, 1 TERA, 2 PvP/PvM variant
	bool eligible { false };
};

struct AttrRow
{
	int set { 0 }; // 0 normal, 1 rare, 2 extra-rare
	std::string apply;
	int applyId { -1 };
	int prob { 0 };
	int values[ITEM_ATTRIBUTE_MAX_LEVEL] {};
	// weapon, body, wrist, foots, neck, head, shield, ear, talisman
	int slot[9] {};
};

struct ExtraRow
{
	uint32_t vnum { 0 };
	int rarity { 0 };
	int type[2] {};
	int value[2] {};
};

struct TalismanRow
{
	uint32_t vnum { 0 };
	int level { 0 };
	int applyAsIs { 0 };
	int valueAsIs { 0 };
	int applyProposed { 0 };
	int valueProposed { 0 };
};

struct SkillRow
{
	uint32_t vnum { 0 };
	std::string name;
	int type { 0 };
	int levelStep { 0 };
	int maxLevel { 0 };
	int levelLimit { 0 };
	std::string pointOn;
};

class Catalog
{
public:
	bool Load(const std::string& directory, const std::string& itemSet, std::string& error);

	const ItemRow* FindItem(uint32_t vnum) const;
	const TItemTable* Proto(uint32_t vnum) const;
	const TItemExtraProto* Extra(uint32_t vnum) const;
	const TalismanRow* FindTalisman(uint32_t vnum) const;
	bool Eligible(uint32_t vnum) const;

	const std::vector<ItemRow>& Items() const { return items_; }
	const std::vector<AttrRow>& Attributes() const { return attributes_; }
	const std::vector<ExtraRow>& Extras() const { return extras_; }
	const std::vector<TalismanRow>& Talismans() const { return talismans_; }
	const std::vector<SkillRow>& Skills() const { return skills_; }

	int ApplyId(const std::string& name) const;

private:
	bool LoadItems(const std::string& path, std::string& error);
	bool LoadAttributes(const std::string& path, int set, std::string& error);
	bool LoadExtras(const std::string& path, std::string& error);
	bool LoadTalismans(const std::string& path, std::string& error);
	bool LoadSkills(const std::string& path, std::string& error);
	void BuildProtos();

	std::vector<ItemRow> items_;
	std::vector<AttrRow> attributes_;
	std::vector<ExtraRow> extras_;
	std::vector<TalismanRow> talismans_;
	std::vector<SkillRow> skills_;
	std::unordered_map<uint32_t, size_t> byVnum_;
	std::unordered_map<uint32_t, bool> eligible_;
	std::unordered_map<uint32_t, TItemTable> protos_;
	std::unordered_map<uint32_t, TItemExtraProto> extraProtos_;
	std::unordered_map<std::string, int> applyByName_;
};

Catalog& GetCatalog();

// ---------------------------------------------------------------------------
// Benchmark world.  These containers stand in for the inventory/equipment
// components only where StatSystem reaches them through ItemSystem accessors.
// ---------------------------------------------------------------------------
extern std::map<std::pair<entt::entity, int>, entt::entity> g_equipment;
extern std::map<std::pair<entt::entity, int>, entt::entity> g_inventory;
extern std::map<entt::entity, std::vector<entt::entity>> g_mountInventory;

struct BenchActor
{
	uint8_t job { JOB_WARRIOR };
	uint8_t skillGroup { 0 };
	uint32_t immune { 0 };
	bool player { true };
};

// Entities may carry a benchmark-local proto override (used to evaluate the
// proposed talisman correction without touching item_proto).
struct BenchProtoOverride
{
	TItemTable proto {};
	bool active { false };
};

struct BenchItemStats
{
	bool rune { false };
};

enum class RngMode
{
	Midpoint,
	Minimum,
	Maximum,
	Random,
};

void SetRng(RngMode mode, uint32_t seed);
uint32_t GetRngSeed();
uint32_t NextRandom();

void ResetWorld();
entt::entity CreateActor(uint8_t job, uint8_t skillGroup, int level);
entt::entity CreateItem(uint32_t vnum, entt::entity owner);
void ComputeActorStats(entt::entity actor);

void Equip(entt::entity owner, int slot, entt::entity item);
void PlaceInInventory(entt::entity owner, int cell, entt::entity item);
void AddMountInventoryItem(entt::entity owner, entt::entity item);
void ClearEquipment(entt::entity owner);

// Attribute construction helpers.  The caller picks the concrete attributes;
// these enforce the ITEM_ATTRIBUTE_* slot layout of the production server.
void ApplyAttribute(entt::entity item, int index, int applyType, int value);
void ApplyMaxAttributes(entt::entity item, const std::vector<const AttrRow*>& normal,
	const std::vector<const AttrRow*>& rare, const std::vector<const AttrRow*>& extraRare,
	int wearSlot);
void ApplyExtraProto(entt::entity item);
void ApplyRefine(entt::entity item, int grade);
void ApplyMetinSockets(entt::entity item, const std::vector<uint32_t>& stones);
void ApplyRuneAttributes(entt::entity item);
void ApplyTalismanOverride(entt::entity item, const TalismanRow& row, bool proposed);

// Damage.  Raw values come from the production battle.cpp.  The post
// processing below is a faithful replica of the normal-damage branch of
// CombatSystem::Damage (block, critical, penetrate, normal-hit modifiers,
// mall attack bonus, victim damage multiplier).
int64_t RawMeleeDamage(entt::entity attacker, entt::entity victim);
int64_t RawArrowDamage(entt::entity attacker, entt::entity victim, entt::entity bow);
int64_t ExpectedMeleeDamage(entt::entity attacker, entt::entity victim);
int64_t ExpectedArrowDamage(entt::entity attacker, entt::entity victim, entt::entity bow);
int64_t SampleMeleeDamage(entt::entity attacker, entt::entity victim);
int64_t SampleArrowDamage(entt::entity attacker, entt::entity victim, entt::entity bow);

struct CombatResult
{
	bool attackerWon { false };
	int rounds { 0 };
	int64_t attackerDamage { 0 };
	int64_t victimDamage { 0 };
	int64_t durationMs { 0 };
};

CombatResult SimulateDuel(entt::entity attacker, entt::entity victim, bool ranged,
	bool trace, std::vector<std::string>* traceLines);

int AttackIntervalMs(entt::entity actor);

} // namespace bench
