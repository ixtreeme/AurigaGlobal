// Real GameServer equipment/point/skill setup for logged-in manual PvP tests.
// Do not reuse the isolated tests/benchmark/BenchmarkShared.hpp simulation.
#include "../core/stdafx.h"
#include "../core/constants.h"
#include "../core/config.h"
#include "BenchmarkEquipment.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../item/item_manager.h"
#include "../ecs/systems/MountSystem.hpp"
#include "../pet/PetSystem.h"
#include "../ecs/systems/AffectSystem.hpp"
#include <ctime>
#include <filesystem>
#include <fstream>

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <sstream>
#include <vector>
#include <limits>
#ifdef ENABLE_STOLE_COSTUME
#include <common/stole_length.h> // The live GameServer wing-grading table.
#endif

namespace nexoria::benchmark::equipment {
namespace {

struct SavedCharacter {
    entt::entity entity {entt::null};
    uint32_t pid {0};
    int level {0};
    int randomHp {0};
    int randomSp {0};
    uint32_t realAlignment {0};
    std::array<int64_t, 7> points {};  // ST, HT, DX, IQ, STAT, SKILL, SUB_SKILL
    uint8_t skillGroup {0};
    std::vector<uint8_t> skills;
    std::array<entt::entity, WEAR_MAX_NUM> previousWears {};
    std::vector<entt::entity> temporaryItems;
    std::vector<std::pair<uint32_t, entt::entity>> temporaryMounts;
    uint32_t petMob {0};
};

std::map<uint32_t, SavedCharacter> applied;
#include "BenchmarkActiveMounts.inc"

constexpr std::array<uint8_t, 7> SavedPointTypes {
    POINT_ST, POINT_HT, POINT_DX, POINT_IQ, POINT_STAT, POINT_SKILL, POINT_SUB_SKILL
};

int Resistance(std::string_view kind) {
    if (kind == "twohand") return APPLY_RESIST_TWOHAND;
    if (kind == "sword") return APPLY_RESIST_SWORD;
    if (kind == "dagger") return APPLY_RESIST_DAGGER;
    if (kind == "bow") return APPLY_RESIST_BOW;
    if (kind == "bell") return APPLY_RESIST_BELL;
    if (kind == "fan") return APPLY_RESIST_FAN;
    return APPLY_NONE;
}

uint32_t Weapon(uint8_t job, uint8_t group) {
    switch (job) {
        case JOB_WARRIOR: return group == 2 ? 8509 : 8519;
        case JOB_ASSASSIN: return group == 2 ? 8529 : 8539;
        case JOB_SURA: return 8569;
        case JOB_SHAMAN: return group == 2 ? 8549 : 8559;
        default: return 0;
    }
}

// The /benchmark start weapon argument describes the weapon that must
// actually be used by the attacker. Never label a sword hit as twohand.
uint32_t AttackWeapon(uint8_t job, std::string_view weaponType) {
    switch (job) {
        case JOB_WARRIOR:
            if (weaponType == "twohand") return 8509;
            if (weaponType == "sword") return 8519;
            break;
        case JOB_ASSASSIN:
            if (weaponType == "dagger") return 8539;
            if (weaponType == "bow") return 8529;
            break;
        case JOB_SURA:
            if (weaponType == "sword") return 8569;
            break;
        case JOB_SHAMAN:
            if (weaponType == "bell") return 8559;
            if (weaponType == "fan") return 8549;
            break;
    }
    return 0;
}

uint32_t Body(uint8_t job) {
    switch (job) {
        case JOB_WARRIOR: return 8649;
        case JOB_ASSASSIN: return 8659;
        case JOB_SURA: return 8669;
        case JOB_SHAMAN: return 8679;
        default: return 0;
    }
}

std::vector<int> Priority(bool defense, int resistType) {
    if (!defense)
        return {APPLY_ATTBONUS_HUMAN, APPLY_CRITICAL_PCT,
                APPLY_PENETRATE_PCT, APPLY_STR, APPLY_DEX, APPLY_INT,
                APPLY_ATT_GRADE_BONUS, APPLY_MAX_HP, APPLY_RESIST_MEZZIUOMINI,
                APPLY_SKILL_DAMAGE_BONUS};
    return {resistType, APPLY_RESIST_MEZZIUOMINI, APPLY_MAX_HP,
            APPLY_RESIST_MAGIC, APPLY_BLOCK, APPLY_DODGE,
            APPLY_RESIST_SWORD, APPLY_RESIST_TWOHAND,
            APPLY_RESIST_DAGGER, APPLY_RESIST_BOW,
            APPLY_RESIST_BELL, APPLY_RESIST_FAN,
            APPLY_ATTBONUS_HUMAN, APPLY_SKILL_DEFEND_BONUS,
            APPLY_NORMAL_HIT_DEFEND_BONUS};
}

// The database-loaded item_attr / item_attr_rare / item_attr_rare_8 tables are
// the authority for slot compatibility and maximum *legal* bonus values.
void SetLegalMaximumAttributes(entt::entity item, bool defender, int resistance,
                               bool reserveCostumeScrollSlots = false) {
    const int set = ItemSystem::GetItemAttributeSetIndex(item);
    if (set < 0) return;
    std::array<TPlayerItemAttribute, ITEM_ATTRIBUTE_MAX_NUM> attrs {};
    const auto preferred = Priority(defender, resistance);
    const auto fill = [&](const TItemAttrMap& table, int first, int last, bool shareRare) {
        std::set<int> used;
        if (shareRare)
            for (int i = ITEM_ATTRIBUTE_RARE_START; i < first; ++i)
                if (attrs[i].bType) used.insert(attrs[i].bType);
        int position = first;
        for (const int type : preferred) {
            if (type == APPLY_NONE || position >= last || used.count(type)) continue;
            const auto it = table.find(static_cast<uint32_t>(type));
            if (it == table.end()) continue;
            const auto& row = it->second;
            const int level = std::min<int>(ITEM_ATTRIBUTE_MAX_LEVEL, row.bMaxLevelBySet[set]);
            if (level <= 0) continue;
            int valueIndex = level - 1;
#ifdef ENABLE_ATTR_COSTUMES
            // The normal costume attribute table stores its actual costume
            // tiers AFTER the five regular-item values. Mirror RollNormal().
            if (first == 0 && ItemSystem::GetItemType(item) == ITEM_COSTUME)
                valueIndex += ITEM_ATTRIBUTE_MAX_LEVEL;
#endif
            if (valueIndex < 0 || static_cast<size_t>(valueIndex) >= std::size(row.lValues))
                continue;
            const int value = row.lValues[valueIndex];
            if (value <= 0 || value > INT16_MAX) continue;
            attrs[position++] = {static_cast<uint8_t>(type), static_cast<int16_t>(value)};
            used.insert(type);
        }
    };
    fill(g_map_itemAttr, 0, ITEM_ATTRIBUTE_NORM_NUM, false);
    // 70403 uses the TWO rare costume slots (not the normal or 8th slot).
    // Do not pre-fill them with generic item_attr_rare bonuses.
    if (!reserveCostumeScrollSlots)
        fill(g_map_itemRare, ITEM_ATTRIBUTE_RARE_START, ITEM_ATTRIBUTE_RARE_END, false);
    fill(g_map_itemRare8, ITEM_ATTRIBUTE_EXTRA_RARE_START, ITEM_ATTRIBUTE_EXTRA_RARE_END, true);
    ItemSystem::SetItemAttributes(item, attrs.data());
}

// Exact PvP stone layout requested for the benchmark. These are REAL item_proto
// stone VNUMs, not synthetic character points. Weapon has precisely three
// sockets; armor reserves one socket for 62010 and fills the two remaining
// sockets from legal armor stones, without duplicating stone families.
bool SetLegalStones(entt::entity item, bool defender, std::string& error) {
    const bool weapon = ItemSystem::GetItemType(item) == ITEM_WEAPON;
    const bool armor = ItemSystem::GetItemType(item) == ITEM_ARMOR &&
                       ItemSystem::GetItemSubType(item) == ARMOR_BODY;
    if (!weapon && !armor) {
        error = "Cannot put benchmark stones in non-weapon/non-body-armor item.";
        return false;
    }
    if (ITEM_SOCKET_MAX_NUM != 3) {
        error = "Benchmark requires exactly 3 equipment stone sockets.";
        return false;
    }
    const uint32_t wearMask = weapon ? 16u : 1u;
    const auto stoneFamily = [](uint32_t vnum) -> uint32_t {
        if (vnum >= 28000 && vnum < 28600) return 28000 + (vnum % 100);
        if (vnum >= 59000 && vnum < 59100) return 59000 + (vnum - 59000) / 10 * 10;
        if (vnum >= 60000 && vnum < 60100) return 60000 + (vnum - 60000) / 10 * 10;
        if (vnum >= 62000 && vnum < 62100) return 62000 + (vnum - 62000) / 10 * 10;
        return vnum;
    };
    std::array<uint32_t, 3> selected {};
    if (weapon) {
        // 59006: Warrior/Ninja, 59016: Sura/Shaman, 60022: War Stone +7.
        selected = {59006u, 59016u, 60022u};
    } else {
        // The Divine stone +6 (62010) MUST be in the armor on both PCs.
        selected[0] = 62010u;
        struct Candidate { uint32_t vnum; int score; int apply; };
        std::vector<Candidate> candidates;
        for (const auto& row : ITEM_MANAGER::instance().GetTable()) {
            if (row.bType != ITEM_METIN || (row.dwWearFlags & wearMask) == 0 ||
                row.dwVnum == 62010u || row.dwVnum == 28104 ||
                row.dwVnum == 28112 || row.dwVnum == 28100 ||
                row.dwVnum == 28108) continue;
            int score = 0, best = 0;
            for (const auto& apply : row.aApplies) {
                const int type = apply.bType, amount = apply.lValue;
                if (amount <= 0) continue;
                int importance = 0;
                if (type == APPLY_MAX_HP) importance = 2;
                if (type == APPLY_SKILL_DEFEND_BONUS ||
                    type == APPLY_NORMAL_HIT_DEFEND_BONUS) importance = defender ? 20 : 15;
                if (type == APPLY_RESIST_MEZZIUOMINI) importance = 15;
                if (importance && amount * importance > score) {
                    score = amount * importance;
                    best = type;
                }
            }
            if (score > 0) candidates.push_back({row.dwVnum, score, best});
        }
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.vnum < b.vnum;
        });
        std::set<uint32_t> usedFamilies {stoneFamily(62010u)};
        std::set<int> usedApplies {APPLY_ATTBONUS_HUMAN}; // Divine stone 62010 applytype0=17
        int socket = 1;
        for (const auto& candidate : candidates) {
            if (socket >= ITEM_SOCKET_MAX_NUM) break;
            if (usedFamilies.count(stoneFamily(candidate.vnum)) ||
                usedApplies.count(candidate.apply)) continue;
            selected[socket++] = candidate.vnum;
            usedFamilies.insert(stoneFamily(candidate.vnum));
            usedApplies.insert(candidate.apply);
        }
        if (socket != ITEM_SOCKET_MAX_NUM) {
            error = "Not enough distinct legal armor stones alongside 62010.";
            return false;
        }
    }
    // Validate ALL three VNUMs before changing the item's sockets.
    std::set<uint32_t> families;
    for (uint32_t vnum : selected) {
        const auto* proto = ITEM_MANAGER::instance().GetTable(vnum);
        if (!proto || proto->bType != ITEM_METIN || (proto->dwWearFlags & wearMask) == 0 ||
            !families.insert(stoneFamily(vnum)).second) {
            error = "Missing, incompatible or duplicate benchmark stone VNUM " +
                    std::to_string(vnum) + ".";
            return false;
        }
    }
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
        if (!ItemSystem::SetItemSocket(item, i, selected[i], false) ||
            ItemSystem::GetItemSocket(item, i) != selected[i]) {
            error = "Stone socket write/verification failed at index " + std::to_string(i) + ".";
            return false;
        }
    }
    return true;
}

// Resolve actual GameServer costume slots. These are separate from equipment
// and from the seven rune subtypes that also use ITEM_COSTUME.
int CostumeSlot(uint8_t subType) {
    switch (subType) {
        case COSTUME_BODY: return WEAR_COSTUME_BODY;
        case COSTUME_HAIR: return WEAR_COSTUME_HAIR;
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
        case COSTUME_MOUNT: return WEAR_COSTUME_MOUNT;
#endif
#ifdef ENABLE_ACCE_SYSTEM
        case COSTUME_ACCE: return WEAR_COSTUME_ACCE_SLOT;
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
        case COSTUME_WEAPON: return WEAR_COSTUME_WEAPON;
#endif
#ifdef ENABLE_STOLE_COSTUME
        case COSTUME_STOLE: return WEAR_COSTUME_ACCE;
#endif
#ifdef ENABLE_COSTUME_PET
        case COSTUME_PET_SKIN: return WEAR_COSTUME_PET_SKIN;
#endif
#ifdef ENABLE_COSTUME_MOUNT
        case COSTUME_MOUNT_SKIN: return WEAR_COSTUME_MOUNT_SKIN;
#endif
#ifdef ENABLE_COSTUME_EFFECT
        case COSTUME_EFFECT_BODY: return WEAR_COSTUME_EFFECT_BODY;
        case COSTUME_EFFECT_WEAPON: return WEAR_COSTUME_EFFECT_WEAPON;
#endif
        default: return -1; // In particular, NOT costume-typed runes.
    }
}

bool CostumeAllowed(const TItemTable& proto, entt::entity pc, uint8_t weaponSubType) {
    if (proto.bType != ITEM_COSTUME || CostumeSlot(proto.bSubType) < 0) return false;
    const uint32_t anti = proto.dwAntiFlags;
    const auto job = ecs::PlayerRuntime::GetJob(pc);
    const auto sex = ecs::PlayerRuntime::GetSex(pc);
    if ((sex == SEX_MALE && (anti & ITEM_ANTIFLAG_MALE)) ||
        (sex == SEX_FEMALE && (anti & ITEM_ANTIFLAG_FEMALE))) return false;
    switch (job) {
        case JOB_WARRIOR: if (anti & ITEM_ANTIFLAG_WARRIOR) return false; break;
        case JOB_ASSASSIN: if (anti & ITEM_ANTIFLAG_ASSASSIN) return false; break;
        case JOB_SURA: if (anti & ITEM_ANTIFLAG_SURA) return false; break;
        case JOB_SHAMAN: if (anti & ITEM_ANTIFLAG_SHAMAN) return false; break;
        default: return false;
    }
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
    // Normal equip explicitly checks the actual weapon subtype. Direct
    // benchmark EquipTo would bypass that check unless done here.
    if (proto.bSubType == COSTUME_WEAPON && proto.alValues[3] != weaponSubType)
        return false;
#endif
    for (const auto& limit : proto.aLimits)
        if (limit.bType == LIMIT_LEVEL &&
            ecs::PointSystem::GetLevel(pc) < limit.lValue) return false;
    return true;
}

// The benchmark uses the player's requested fixed Necromancer outfit, not
// the old unrestricted "best scoring costume" search.
// VNUMs are taken from the loaded November item_proto and item_extra_proto.
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
uint32_t NecromancerWeaponCostume(uint8_t weaponSubType) {
    switch (weaponSubType) {
        case WEAPON_SWORD: return 41615;
        case WEAPON_DAGGER: return 41614;
        case WEAPON_BOW: return 41613;
        case WEAPON_TWO_HANDED: return 41612;
        case WEAPON_BELL: return 41610;
        case WEAPON_FAN: return 41611;
        default: return 0;
    }
}
#endif

// Deterministic benchmark-only equivalent of two successful VNUM 70403
// uses. The real 70403 item is classified as an EXTRA_INVENTORY consumable,
// and creating it only to consume it requires available extra-inventory slots.
// Benchmark costumes are newly created temporary items that are not yet
// equipped or owned by the player; set their two rare attributes directly
// using the existing GameServer item API BEFORE normal equipment applies them.
// This does not create, consume or grant any 70403 scrolls and never adds
// PvP points manually. Both values are legal 70403 socket payloads.
#ifdef ENABLE_ATTR_COSTUMES
bool ApplyNecromancerScrollBonuses(entt::entity costume, std::string& error) {
    if (!ItemSystem::IsValidItem(costume) ||
        ItemSystem::GetItemType(costume) != ITEM_COSTUME ||
        ItemSystem::GetItemAttributeSetIndex(costume) < 0) {
        error = "Invalid Necromancer costume for 70403 benchmark bonuses.";
        return false;
    }
    const auto subtype = ItemSystem::GetItemSubType(costume);
    if (subtype != COSTUME_BODY && subtype != COSTUME_HAIR &&
        subtype != COSTUME_WEAPON) {
        error = "70403 benchmark bonuses require body, hair or weapon costume.";
        return false;
    }
    if (ITEM_ATTRIBUTE_RARE_END - ITEM_ATTRIBUTE_RARE_START != 2) {
        error = "Unexpected rare costume attribute slot count.";
        return false;
    }
    // Never overwrite pre-existing attributes. The caller deliberately
    // reserves these two slots via SetLegalMaximumAttributes(..., true).
    for (int slot = ITEM_ATTRIBUTE_RARE_START; slot < ITEM_ATTRIBUTE_RARE_END; ++slot) {
        if (ItemSystem::GetItemAttribute(costume, slot).bType != APPLY_NONE) {
            error = "Rare costume attribute slot was not reserved for 70403.";
            return false;
        }
    }
    if (!ItemSystem::SetItemAttribute(costume, ITEM_ATTRIBUTE_RARE_START,
                                       APPLY_ATTBONUS_HUMAN, 10) ||
        !ItemSystem::SetItemAttribute(costume, ITEM_ATTRIBUTE_RARE_START + 1,
                                       APPLY_RESIST_MEZZIUOMINI, 10)) {
        error = "Could not write 70403-equivalent rare costume bonuses.";
        return false;
    }
    const auto human = ItemSystem::GetItemAttribute(costume, ITEM_ATTRIBUTE_RARE_START);
    const auto resist = ItemSystem::GetItemAttribute(costume, ITEM_ATTRIBUTE_RARE_START + 1);
    if (human.bType != APPLY_ATTBONUS_HUMAN || human.sValue != 10 ||
        resist.bType != APPLY_RESIST_MEZZIUOMINI || resist.sValue != 10) {
        error = "70403-equivalent HUMAN+10 / RESIST+10 attributes failed verification.";
        return false;
    }
    return true;
}
#endif

// Necromancer wings (85153) are grade-four COSTUME_STOLE items, NOT normal
// body/hair/weapon costumes. ITEM_MANAGER::CreateItem generates six bonuses
// from stoleInfoTable. The generic costume roll would erase those six fixed
// bonuses and commonly leave only ATT_GRADE_BONUS +50. Recreate the six highest
// *actually obtainable* grade-four outcomes, using exactly the GameServer
// source-of-truth table and the same six slots as USE_ENCHANT_STOLE.
#ifdef ENABLE_STOLE_COSTUME
bool SetMaxNecromancerWingBonuses(entt::entity wing, std::string& error) {
    if (!ItemSystem::IsValidItem(wing) ||
        ItemSystem::GetItemVnum(wing) != 85153u ||
        ItemSystem::GetItemType(wing) != ITEM_COSTUME ||
        ItemSystem::GetItemSubType(wing) != COSTUME_STOLE) {
        error = "85153 is not a valid Necromancer wing.";
        return false;
    }
    const int grade = ItemSystem::GetItemValue(wing, 0);
    if (grade != 4) {
        error = "Necromancer wing must have proto grade 4; actual=" + std::to_string(grade);
        return false;
    }
    static_assert(MAX_ATTR == 6 && MAX_ATTR <= ITEM_ATTRIBUTE_MAX_NUM);
    static_assert(std::size(stoleInfoTable) == MAX_ATTR);
    static_assert(std::size(stoleInfoTable[0]) == 1 + 4 * MAX_VAR_ATTR);
    constexpr int maximumRoll = 4 * MAX_VAR_ATTR; // Grade 4, highest of 4 rolls.

    // Only a freshly-created temporary benchmark wing is passed here. Never
    // overwrite a real player's items or add a second set of bonus points.
    if (!ItemSystem::ClearItemAttributesEcs(wing)) {
        error = "Cannot reset temporary wing attributes.";
        return false;
    }
    for (int slot = 0; slot < MAX_ATTR; ++slot) {
        const int type = stoleInfoTable[slot][0];
        const int amount = stoleInfoTable[slot][maximumRoll];
        if (type <= APPLY_NONE || type > UINT8_MAX ||
            amount <= 0 || amount > INT16_MAX) {
            error = "Invalid grade-four wing bonus in stoleInfoTable slot " + std::to_string(slot);
            return false;
        }
        if (!ItemSystem::SetItemForceAttributeEcs(
                wing, slot, static_cast<uint8_t>(type), static_cast<int16_t>(amount))) {
            error = "Could not write wing bonus at slot " + std::to_string(slot);
            return false;
        }
        const auto actual = ItemSystem::GetItemAttribute(wing, slot);
        if (actual.bType != type || actual.sValue != amount) {
            error = "Wing bonus verification failed at slot " + std::to_string(slot);
            return false;
        }
    }
    // Grade-four stole enchantment defines exactly six bonuses; 70403 is NOT
    // legal for a stole and the normal/8th item-attribute tables do not apply.
    for (int slot = MAX_ATTR; slot < ITEM_ATTRIBUTE_MAX_NUM; ++slot) {
        const auto attr = ItemSystem::GetItemAttribute(wing, slot);
        if (attr.bType != APPLY_NONE || attr.sValue != 0) {
            error = "Unexpected seventh/eighth wing bonus after initialization.";
            return false;
        }
    }
    return true;
}
#endif

std::vector<uint8_t> CostumeSlots() {
    std::set<uint8_t> slots;
    for (const auto& proto : ITEM_MANAGER::instance().GetTable())
        if (proto.bType == ITEM_COSTUME) {
            const int slot = CostumeSlot(proto.bSubType);
            if (slot >= 0 && slot < WEAR_MAX_NUM) slots.insert(static_cast<uint8_t>(slot));
        }
    return {slots.begin(), slots.end()};
}

SavedCharacter Snapshot(entt::entity character) {
    SavedCharacter saved;
    saved.entity = character;
    saved.pid = ecs::PlayerRuntime::GetPlayerID(character);
    saved.level = ecs::PointSystem::GetLevel(character);
    saved.randomHp = ecs::PointSystem::GetRandomHP(character);
    saved.randomSp = ecs::PointSystem::GetRandomSP(character);
    saved.realAlignment = CombatSystem::GetRealAlignment(character);
    for (size_t n = 0; n < SavedPointTypes.size(); ++n)
        saved.points[n] = ecs::PointSystem::GetReal(character, SavedPointTypes[n]);
    saved.skillGroup = SkillSystem::GetSkillGroup(character);
    for (int i = 0; i < SKILL_MAX_NUM; ++i)
        saved.skills.push_back(static_cast<uint8_t>(SkillSystem::GetSkillLevel(character, i)));
    for (int slot = 0; slot < WEAR_MAX_NUM; ++slot)
        saved.previousWears[slot] = ItemSystem::GetWearItem(character, slot);
    return saved;
}

void Undo(SavedCharacter& saved, std::ostringstream& errors) {
    const entt::entity character = saved.entity;
    if (!ecs::PlayerRuntime::IsPC(character) ||
        ecs::PlayerRuntime::GetPlayerID(character) != saved.pid) {
        errors << "Character disconnected; state requires recovery. ";
        return;
    }
    if (saved.petMob)
        PetSystem::Unsummon(character, saved.petMob, true);
    for (const auto& [pos, item] : saved.temporaryMounts) {
        const auto removed = MountSystem::RemoveMountInventoryItem(character, pos, true);
        if (removed != item) {
            errors << "Could not safely remove temporary mount at " << pos << ". ";
        } else if (ItemSystem::IsValidItem(removed)) {
            ItemSystem::DestroyItemEntityEcs(removed, "BENCHMARK_MOUNT_RESTORE");
        }
    }
    for (const auto item : saved.temporaryItems) {
        if (!ItemSystem::IsValidItem(item)) continue;
        if (ItemSystem::GetItemOwner(item) == character &&
            ItemSystem::IsItemEquipped(item))
            if (!ItemSystem::UnequipItemEcs(character, item)) {
                errors << "Temporary item could not be unequipped: "
                       << ItemSystem::GetItemVnum(item) << ". ";
                continue;
            }
        if (ItemSystem::IsValidItem(item) &&
            !ItemSystem::DestroyItemEntityEcs(item, "BENCHMARK_RESTORE"))
            errors << "Temporary item cleanup failed. ";
    }
    // Restore exactly the previous *item entities*, not copies of their VNUMs.
    for (int slot = 0; slot < WEAR_MAX_NUM; ++slot) {
        const auto original = saved.previousWears[slot];
        if (ItemSystem::IsValidItem(original) &&
            ItemSystem::GetWearItem(character, slot) == entt::null &&
            !InventorySystem::EquipTo(original, character, static_cast<uint8_t>(slot)))
            errors << "Could not restore original slot " << slot << ". ";
    }
    ecs::PointSystem::SetLevelFromQuest(character, saved.level);
    ecs::PointSystem::SetRandomHP(character, saved.randomHp);
    ecs::PointSystem::SetRandomSP(character, saved.randomSp);
    for (size_t n = 0; n < SavedPointTypes.size(); ++n)
        ecs::PointSystem::SetReal(character, SavedPointTypes[n], saved.points[n]);
    // Alignment contributes real PvP bonuses. Restore through the canonical
    // GameServer API, which recalculates the alignment bonus grade and syncs it.
    CombatSystem::UpdateAlignment(character,
        static_cast<int64_t>(saved.realAlignment) -
        static_cast<int64_t>(CombatSystem::GetRealAlignment(character)));
    SkillSystem::SetSkillGroup(character, saved.skillGroup);
    for (size_t i = 0; i < saved.skills.size(); ++i)
        SkillSystem::SetSkillLevel(character, static_cast<uint32_t>(i), saved.skills[i]);
    ecs::PointSystem::Compute(character);
    ecs::PointSystem::Change(character, POINT_HP,
        static_cast<int64_t>(ecs::PointSystem::GetMaxHP(character)) - ecs::PointSystem::Get(character, POINT_HP));
    ecs::PointSystem::Change(character, POINT_SP,
        static_cast<int64_t>(ecs::PointSystem::GetMaxSP(character)) - ecs::PointSystem::Get(character, POINT_SP));
    NetworkSyncSystem::PointsPacket(character);
    SkillSystem::SendSkillLevelPacket(character);
}

} // namespace

bool IsActive(entt::entity character) {
    if (!ecs::PlayerRuntime::IsPC(character)) return false;
    return applied.count(ecs::PlayerRuntime::GetPlayerID(character)) != 0;
}

bool Apply(entt::entity character, int tier, std::string_view defenseWeapon,
           bool defender, std::string& message) {
    // Only the final Zodiac gear profile (minimum equipment level 158)
    // has been verified. The GameServer maximum comes from its live config.
    if (!test_server || !ecs::PlayerRuntime::IsPC(character) || tier < 158 ||
        (defender && Resistance(defenseWeapon) == APPLY_NONE)) {
        message = "Final PvP set requires level 158 or higher and a test-server PC.";
        return false;
    }
    const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(character);
    if (applied.count(pid)) {
        message = "This PC already has a benchmark profile; stop previous session first.";
        return false;
    }
    // Preflight MUST run before SetLevelFromQuest: the quest setter can return
    // true even when Change(POINT_LEVEL) silently rejects a level above the
    // configured GameServer maximum. Otherwise we can mutate skills/stats and
    // attempt to equip level-158 items on a lower-level character.
    if (gPlayerMaxLevel < tier) {
        message = "Benchmark requires server.max_level >= " + std::to_string(tier) +
                  "; running GameServer max_level=" + std::to_string(gPlayerMaxLevel) +
                  ". Update the GameServer configuration and restart it before testing.";
        return false;
    }
    if (MountSystem::GetMountInventory(character) == entt::null) {
        message = "Mount inventory has not loaded; retry after login sync.";
        return false;
    }
    const int mountWidth = MountSystem::GetMountInventoryWidth(character);
    const int mountHeight = MountSystem::GetMountInventorySize(character);
    const int mountCapacity = mountWidth * mountHeight;
    if (mountWidth <= 0 || mountHeight <= 0) {
        message = "Mount inventory dimensions are invalid.";
        return false;
    }
    // Active live mounts are all size-one items in the validated item_proto.
    // Count *actual* free cells and missing VNUMs before touching character
    // points/equipment or allocating temporary items.
    std::set<uint32_t> mountVnumsAlreadyOwned;
    int emptyMountCells = 0;
    for (int cell = 0; cell < mountCapacity; ++cell) {
        const auto existing = MountSystem::GetMountInventoryItem(character, cell);
        if (ItemSystem::IsValidItem(existing))
            mountVnumsAlreadyOwned.insert(ItemSystem::GetItemVnum(existing));
        else if (MountSystem::IsMountInventoryPositionEmpty(character, cell, 1))
            ++emptyMountCells;
    }
    int missingMounts = 0;
    for (const auto mountVnum : kActiveMounts) {
        const auto* proto = ITEM_MANAGER::instance().GetTable(mountVnum);
        if (!proto) {
            message = "Required mount VNUM missing from loaded item_proto: " +
                      std::to_string(mountVnum);
            return false;
        }
        if (proto->bSize != 1) {
            message = "Unexpected non-single-cell mount VNUM " +
                      std::to_string(mountVnum) + "; cannot safely auto-equip.";
            return false;
        }
        if (!mountVnumsAlreadyOwned.count(mountVnum)) ++missingMounts;
    }
    if (missingMounts > emptyMountCells) {
        message = "Mount inventory needs " + std::to_string(missingMounts) +
                  " free cells but has " + std::to_string(emptyMountCells) +
                  " (capacity " + std::to_string(mountCapacity) +
                  " = " + std::to_string(mountWidth) + "x" +
                  std::to_string(mountHeight) + ").";
        return false;
    }
    // UpdateAlignment uses the real value as its authority, so a character
    // with a divergent visible/real rank cannot be safely restored by the
    // public API. Refuse before modifying any character data.
    if (CombatSystem::GetAlignment(character) != CombatSystem::GetRealAlignment(character)) {
        message = "Visible and real alignment differ; cannot safely snapshot rank for benchmark.";
        return false;
    }
    if (PetSystem::CountSummoned(character) != 0) {
        message = "Unsummon the existing pet before using benchmark.";
        return false;
    }
    for (int cell = BELT_INVENTORY_SLOT_START; cell < BELT_INVENTORY_SLOT_END; ++cell) {
        if (ItemSystem::IsValidItem(ItemSystem::GetInventoryItem(character, cell))) {
            message = "Please empty belt inventory before benchmarking.";
            return false;
        }
    }
    // Verify the live database-backed costume extra_proto data before any
    // level/stat/item change. If it is missing, no benchmark profile is made.
#ifdef ENABLE_ITEM_EXTRA_PROTO
#ifdef ENABLE_NEW_EXTRA_BONUS
    bool hasLoadedCostumeExtras = false;
    for (const auto& proto : ITEM_MANAGER::instance().GetTable()) {
        if (proto.bType != ITEM_COSTUME || CostumeSlot(proto.bSubType) < 0) continue;
        if (const auto* extra = ITEM_MANAGER::instance().GetExtraProto(proto.dwVnum)) {
            for (const auto& apply : extra->ExtraBonus)
                if (apply.bType != APPLY_NONE && apply.lValue != 0)
                    hasLoadedCostumeExtras = true;
        }
        if (hasLoadedCostumeExtras) break;
    }
    if (!hasLoadedCostumeExtras) {
        message = "No costume extra_proto bonuses loaded; cannot verify PvP costume setup.";
        return false;
    }
#endif
#endif
    // Validate the real 70403 material before changing the character.
#if !defined(ENABLE_ATTR_COSTUMES) || !defined(ENABLE_WEAPON_COSTUME_SYSTEM) || !defined(ENABLE_STOLE_COSTUME)
    message = "Necromancer benchmark needs costume attributes, weapon costumes and stole enabled.";
    return false;
#else
    const auto* costumeScrollProto = ITEM_MANAGER::instance().GetTable(70403);
    if (!costumeScrollProto || costumeScrollProto->bType != ITEM_USE ||
        costumeScrollProto->bSubType != USE_ADD_ATTR_COSTUME2) {
        message = "70403 is missing or is not a 10% special costume bonus scroll.";
        return false;
    }
    for (const auto vnum : {41610u, 41611u, 41612u, 41613u, 41614u, 41615u,
                            41616u, 41617u, 41618u, 41619u, 85153u, 88027u}) {
        const auto* costumeProto = ITEM_MANAGER::instance().GetTable(vnum);
        if (!costumeProto || costumeProto->bType != ITEM_COSTUME) {
            message = "Missing Necromancer costume VNUM " + std::to_string(vnum) + ".";
            return false;
        }
    }
#endif
#ifndef ENABLE_COSTUME_PET
    message = "Benchmark requires ENABLE_COSTUME_PET for Baashido costume 88027.";
    return false;
#else
    const auto* petCostumeProto = ITEM_MANAGER::instance().GetTable(88027u);
    if (!petCostumeProto || petCostumeProto->bType != ITEM_COSTUME ||
        petCostumeProto->bSubType != COSTUME_PET_SKIN ||
        CostumeSlot(petCostumeProto->bSubType) != WEAR_COSTUME_PET_SKIN) {
        message = "Pet costume 88027 is missing or has invalid pet-costume type/slot.";
        return false;
    }
#endif
    // Reject an incomplete/mismatched live item_proto BEFORE touching level,
    // stats, rank, equipment or the benchmark's restore snapshot.
    for (const auto [vnum, wear] : std::array<std::pair<uint32_t, uint32_t>, 4>{{
            {59006u, 16u}, {59016u, 16u}, {60022u, 16u}, {62010u, 1u}}}) {
        const auto* stone = ITEM_MANAGER::instance().GetTable(vnum);
        if (!stone || stone->bType != ITEM_METIN || (stone->dwWearFlags & wear) == 0) {
            message = "Benchmark stone VNUM " + std::to_string(vnum) +
                      " missing or has incompatible wearflag in live item_proto.";
            return false;
        }
    }
    SavedCharacter saved = Snapshot(character);
    const uint8_t job = ecs::PlayerRuntime::GetJob(character);
    const uint8_t group = SkillSystem::GetSkillGroup(character) == 2 ? 2 : 1;
    // Both roles must wear a shield. Defenders retain the one-handed weapon
    // selection used by the existing defense profile; attackers retain the
    // actual weapon requested by /benchmark start (including twohand).
    const uint32_t profileWeapon = defender ? Weapon(job, 1) : AttackWeapon(job, defenseWeapon);
    if (!profileWeapon || !Body(job)) {
        message = defender ? "Unsupported defender benchmark class."
                           : "Requested attack weapon cannot be used by this character class.";
        return false;
    }
    std::ostringstream errors;
    const auto fail = [&](const std::string& why) {
        errors << why << " ";
        Undo(saved, errors);
        message = errors.str();
        return false;
    };
    // The regular level-change path updates HP/SP, network packets and dirty
    // tracking. The four real base stats each reach their in-game cap of 90.
    if (!ecs::PointSystem::SetLevelFromQuest(character, tier))
        return fail("Could not level PC to " + std::to_string(tier) + ".");
    if (ecs::PointSystem::GetLevel(character) != tier)
        return fail("Level change did not reach " + std::to_string(tier) + " (actual=" +
                    std::to_string(ecs::PointSystem::GetLevel(character)) + ").");
    for (const uint8_t point : {POINT_ST, POINT_HT, POINT_DX, POINT_IQ})
        if (!ecs::PointSystem::SetReal(character, point, 90))
            return fail("Could not maximize base points.");
    ecs::PointSystem::SetReal(character, POINT_STAT, 0);
    // MAX_ALIGNMENT is the real server cap (50,000,000 raw / 5,000,000
    // displayed). Never add hand-crafted rank bonus points: StatSystem
    // already derives all rank bonuses from the true alignment grade.
    CombatSystem::UpdateAlignment(character,
        static_cast<int64_t>(CombatSystem::MAX_ALIGNMENT) -
        static_cast<int64_t>(CombatSystem::GetRealAlignment(character)));
    if (CombatSystem::GetRealAlignment(character) != CombatSystem::MAX_ALIGNMENT)
        return fail("Failed to apply maximum alignment rank.");
    ecs::PointSystem::Compute(character);
    if (SkillSystem::GetSkillGroup(character) == 0) SkillSystem::SetSkillGroup(character, group);
    for (int skill = 1; skill < SKILL_MAX_NUM; ++skill) {
        if (SkillSystem::CanUseSkill(character, skill))
            SkillSystem::SetSkillLevel(character, skill, 40);
    }
    SkillSystem::SendSkillLevelPacket(character);

    // Remove old equipment with the original item entity retained in normal
    // inventory, so it can be restored at /benchmark stop.
    constexpr std::array<uint8_t, 10> slots {
        WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD, WEAR_WRIST,
        WEAR_FOOTS, WEAR_NECK, WEAR_EAR, WEAR_PENDANT, WEAR_ARROW
    };
    for (const auto slot : slots) {
        const auto old = ItemSystem::GetWearItem(character, slot);
        if (old != entt::null && !ItemSystem::UnequipItemEcs(character, old))
            return fail("Cannot preserve equipped originals; check inventory space.");
    }
    // Save all original costumes as real item entities, just like ordinary
    // armor. Snapshot() already holds their original wear slots.
    const auto costumeSlots = CostumeSlots();
    for (const auto slot : costumeSlots) {
        const auto old = ItemSystem::GetWearItem(character, slot);
        if (old != entt::null && !ItemSystem::UnequipItemEcs(character, old))
            return fail("Cannot preserve original costume in slot " + std::to_string(slot) +
                        "; check free inventory cells.");
    }
#ifdef ENABLE_RUNE_SYSTEM
    for (int i = 0; i < 7; ++i) {
        const auto old = ItemSystem::GetWearItem(character, WEAR_RUNE1 + i);
        if (old != entt::null && !ItemSystem::UnequipItemEcs(character, old))
            return fail("Cannot preserve old rune; check inventory space.");
    }
#endif
    const std::array<std::pair<uint8_t, uint32_t>, 9> gear {{
        {WEAR_WEAPON, profileWeapon}, {WEAR_BODY, Body(job)},
        {WEAR_HEAD, 8689}, {WEAR_SHIELD, 8699}, {WEAR_WRIST, 8709},
        {WEAR_FOOTS, 8719}, {WEAR_NECK, 8729}, {WEAR_EAR, 8739},
        {WEAR_PENDANT, 10830}
    }};
    int equipped = 0;
    for (auto [slot, vnum] : gear) {
        if (!ITEM_MANAGER::instance().GetTable(vnum))
            return fail("Unknown VNUM " + std::to_string(vnum));
        // Equip the shield for both attacker and target. Unlike the old benchmark
        // restriction, this GameServer's CanEquipNow / EquipTo / FindEquipCell
        // do not prohibit the shield slot for two-handed weapons or bows.
        // Keep the actual requested attacking weapon (e.g. 8509 for twohand).
        const auto item = ITEM_MANAGER::instance().CreateItem(vnum, 1, 0, false, -1, true);
        if (!ItemSystem::IsValidItem(item))
            return fail("CreateItem failed for " + std::to_string(vnum));
        saved.temporaryItems.push_back(item);
        SetLegalMaximumAttributes(item, defender, Resistance(defenseWeapon));
        if (slot == WEAR_WEAPON || slot == WEAR_BODY) {
            std::string stoneError;
            if (!SetLegalStones(item, defender, stoneError))
                return fail("VNUM " + std::to_string(vnum) + " stones: " + stoneError);
        }
        if (slot == WEAR_WRIST || slot == WEAR_NECK || slot == WEAR_EAR) {
            ItemSystem::SetItemAccessorySocketMaxGrade(item, 3);
            ItemSystem::SetItemAccessorySocketGrade(item, 3);
        }
        // Do not silently skip the shield for any benchmark role. If a future
        // GameServer changes its equip rules, fail visibly and roll back instead.
        if (!InventorySystem::CanEquipNow(character, item) ||
            !InventorySystem::EquipTo(item, character, slot))
            return fail("Failed to equip verified PvP item " + std::to_string(vnum) +
                        " (actual level=" + std::to_string(ecs::PointSystem::GetLevel(character)) +
                        ", max_level=" + std::to_string(gPlayerMaxLevel) + ").");
        ++equipped;
    }
    const auto equippedShield = ItemSystem::GetWearItem(character, WEAR_SHIELD);
    if (!ItemSystem::IsValidItem(equippedShield) ||
        ItemSystem::GetItemVnum(equippedShield) != 8699)
        return fail(defender ? "Defender shield missing after PvP gear setup."
                             : "Attacker shield missing after PvP gear setup.");
    // Strict Necromancer costume set for BOTH attacker and defender.
    // The 70403 material works on body/hair/weapon, not on the stole/wing.
    const auto* weaponProto = ITEM_MANAGER::instance().GetTable(profileWeapon);
    if (!weaponProto || weaponProto->bType != ITEM_WEAPON)
        return fail("Cannot resolve benchmark weapon subtype for Necromancer costume.");
    std::vector<std::pair<uint8_t, uint32_t>> costumeGear;
#if defined(ENABLE_WEAPON_COSTUME_SYSTEM) && defined(ENABLE_STOLE_COSTUME) && defined(ENABLE_ATTR_COSTUMES)
    const auto sex = ecs::PlayerRuntime::GetSex(character);
    if (sex != SEX_MALE && sex != SEX_FEMALE)
        return fail("Character has invalid sex for Necromancer costume variants.");
    const bool male = sex == SEX_MALE;
    const uint32_t necroWeapon = NecromancerWeaponCostume(weaponProto->bSubType);
    if (!necroWeapon)
        return fail("No Necromancer weapon costume for weapon subtype " +
                    std::to_string(weaponProto->bSubType) + ".");
    const std::array<std::pair<uint8_t, uint32_t>, 4> necromancer {{
        {WEAR_COSTUME_BODY, male ? 41616u : 41617u},
        {WEAR_COSTUME_HAIR, male ? 41618u : 41619u},
        {WEAR_COSTUME_WEAPON, necroWeapon},
        {WEAR_COSTUME_ACCE, 85153u} // Necromancer wings: COSTUME_STOLE (subtype 5)
    }};
    for (const auto [slot, vnum] : necromancer) {
        const auto* proto = ITEM_MANAGER::instance().GetTable(vnum);
        if (!proto || !CostumeAllowed(*proto, character, weaponProto->bSubType) ||
            CostumeSlot(proto->bSubType) != slot)
            return fail("Necromancer costume VNUM " + std::to_string(vnum) +
                        " is missing, illegal for this PC/weapon, or has wrong wear slot.");
#if defined(ENABLE_ITEM_EXTRA_PROTO) && defined(ENABLE_NEW_EXTRA_BONUS)
        // The three wearable costumes really do have their own extra_proto
        // bonuses (e.g. Necromancer human resistance / block). Never apply
        // them manually: EquipTo/ModifyPoints does that already.
        if (slot != WEAR_COSTUME_ACCE && !ITEM_MANAGER::instance().GetExtraProto(vnum))
            return fail("Necromancer extra_proto missing for VNUM " + std::to_string(vnum));
#endif
        const auto costume = ITEM_MANAGER::instance().CreateItem(vnum, 1, 0, false, -1, true);
        if (!ItemSystem::IsValidItem(costume))
            return fail("CreateItem failed for Necromancer costume " + std::to_string(vnum));
        saved.temporaryItems.push_back(costume);
        const bool useSpecialScroll = slot != WEAR_COSTUME_ACCE;
        if (useSpecialScroll) {
            SetLegalMaximumAttributes(costume, defender, Resistance(defenseWeapon), true);
            std::string scrollError;
            if (!ApplyNecromancerScrollBonuses(costume, scrollError))
                return fail("Costume VNUM " + std::to_string(vnum) + ": " + scrollError);
        } else {
            // A grade-four Necromancer stole uses its dedicated six-bonus
            // table. NEVER run SetLegalMaximumAttributes on it: that would
            // replace the legal wing bonuses with a generic +50 attack roll.
            std::string wingError;
            if (!SetMaxNecromancerWingBonuses(costume, wingError))
                return fail("Necromancer wing 85153: " + wingError);
        }
        if (!InventorySystem::CanEquipNow(character, costume) ||
            !InventorySystem::IsEquipmentSexAllowed(character, costume) ||
            ItemSystem::FindEquipCell(character, costume) != slot ||
            !InventorySystem::EquipTo(costume, character, slot) ||
            ItemSystem::GetWearItem(character, slot) != costume)
            return fail("Could not equip Necromancer costume " + std::to_string(vnum) +
                        " at slot " + std::to_string(slot) + ".");
        costumeGear.emplace_back(slot, vnum);
        ++equipped;
    }
#else
    return fail("Necromancer benchmark requires weapon costumes, stole and costume attributes enabled.");
#endif
    if (costumeGear.size() != 4)
        return fail("Necromancer outfit incomplete; expected body, hair, weapon and wings.");
#ifdef ENABLE_COSTUME_PET
    // Baashido Pet Costume (88027), one for EACH benchmark PC. The real
    // pet-costume item_proto carries five base bonuses (including HUMAN+30,
    // MAX_HP+10000). Do not duplicate those bonuses with extra points or
    // generic attribute rolling. The normal EquipTo -> ModifyPoints path
    // applies them exactly once, and updates the summoned pet's appearance.
    {
        constexpr uint32_t petSkinVnum = 88027u;
        const auto* proto = ITEM_MANAGER::instance().GetTable(petSkinVnum);
        if (!proto || !CostumeAllowed(*proto, character, weaponProto->bSubType) ||
            proto->bSubType != COSTUME_PET_SKIN ||
            CostumeSlot(proto->bSubType) != WEAR_COSTUME_PET_SKIN)
            return fail("Baashido pet costume 88027 cannot be equipped on this PC.");
        const auto skin = ITEM_MANAGER::instance().CreateItem(petSkinVnum, 1, 0, false, -1, true);
        if (!ItemSystem::IsValidItem(skin))
            return fail("CreateItem failed for Baashido pet costume 88027.");
        saved.temporaryItems.push_back(skin);
        if (!InventorySystem::CanEquipNow(character, skin) ||
            !InventorySystem::IsEquipmentSexAllowed(character, skin) ||
            ItemSystem::FindEquipCell(character, skin) != WEAR_COSTUME_PET_SKIN ||
            !InventorySystem::EquipTo(skin, character, WEAR_COSTUME_PET_SKIN) ||
            ItemSystem::GetWearItem(character, WEAR_COSTUME_PET_SKIN) != skin)
            return fail("Could not equip Baashido pet costume 88027 in pet skin slot.");
        costumeGear.emplace_back(WEAR_COSTUME_PET_SKIN, petSkinVnum);
        ++equipped;
    }
#else
    return fail("Baashido pet costume 88027 requires ENABLE_COSTUME_PET.");
#endif
    // Account mount inventory is account-shared. Add only missing VNUMs and
    // use the inventory's no-save mode. Every inserted cell is remembered so
    // it can be detached without deleting a live account-mount DB row.
    for (const uint32_t mountVnum : kActiveMounts) {
        if (!ITEM_MANAGER::instance().GetTable(mountVnum))
            return fail("Missing active mount VNUM " + std::to_string(mountVnum));
        bool owned = false;
        const int capacity = mountCapacity; // Width * height, NOT height alone.
        for (int cell = 0; cell < capacity; ++cell) {
            const auto old = MountSystem::GetMountInventoryItem(character, cell);
            if (ItemSystem::IsValidItem(old) && ItemSystem::GetItemVnum(old) == mountVnum) {
                owned = true;
                break;
            }
        }
        if (owned) continue;
        const entt::entity item = ITEM_MANAGER::instance().CreateItem(mountVnum, 1, 0, false, -1, true);
        if (!ItemSystem::IsValidItem(item))
            return fail("Could not create mount " + std::to_string(mountVnum));
        // Prevent the helper's first-use initialization from writing a row.
        // The whole item is temporary and skipSave=true.
        ItemSystem::SetItemSocket(item, 0, static_cast<uint32_t>(time(nullptr)) + 86400 * 30, false);
        ItemSystem::SetItemSocket(item, 1, 1, false);
        bool added = false;
        for (int cell = 0; cell < capacity; ++cell) {
            if (MountSystem::IsMountInventoryPositionEmpty(character, cell, ItemSystem::GetItemSize(item)) &&
                MountSystem::AddMountInventoryItem(character, cell, item, true)) {
                saved.temporaryMounts.emplace_back(static_cast<uint32_t>(cell), item);
                added = true;
                break;
            }
        }
        if (!added) {
            ItemSystem::DestroyItemEntityEcs(item, "BENCHMARK_MOUNT_NO_SPACE");
            return fail("Mount insertion failed despite successful capacity preflight: VNUM " +
                        std::to_string(mountVnum) + ".");
        }
    }
    // The 16 real belt-family maxima are registered in the special inventory
    // range 500..659, where StatSystem::ApplyBeltPoints actually reads them.
    for (int family = 0; family < 16; ++family) {
        // Some live families do NOT have a +9 (e.g. 18149 is absent).
        // Resolve the highest real proto in each 10-VNUM family.
        uint32_t beltVnum = 0;
        for (int grade = 9; grade >= 0; --grade) {
            const uint32_t candidate = 18000 + family * 10 + grade;
            const auto* proto = ITEM_MANAGER::instance().GetTable(candidate);
            if (proto && proto->bType == ITEM_BELT) {
                beltVnum = candidate;
                break;
            }
        }
        if (!beltVnum)
            return fail("Missing usable belt family " + std::to_string(family));
        const auto belt = ITEM_MANAGER::instance().CreateItem(beltVnum, 1, 0, false, -1, true);
        if (!ItemSystem::IsValidItem(belt))
            return fail("Cannot create belt " + std::to_string(beltVnum));
        saved.temporaryItems.push_back(belt);
        if (!InventorySystem::AddToCharacter(belt, character,
                TItemPos(INVENTORY, static_cast<uint16_t>(BELT_INVENTORY_SLOT_START + 10 * family))))
            return fail("Cannot add max belt to special inventory cell.");
    }
#ifdef ENABLE_RUNE_SYSTEM
    // All seven runes at full 86400-second charge, using the real activation
    // prerequisite checks. The seventh is activated after the first six.
    for (int i = 0; i < 7; ++i) {
        const uint32_t runeVnum = 88970 + i;
        if (!ITEM_MANAGER::instance().GetTable(runeVnum))
            return fail("Missing rune VNUM " + std::to_string(runeVnum));
        const auto rune = ITEM_MANAGER::instance().CreateItem(runeVnum, 1, 0, false, -1, true);
        if (!ItemSystem::IsValidItem(rune))
            return fail("Cannot create rune.");
        saved.temporaryItems.push_back(rune);
        ItemSystem::SetItemSocket(rune, 0, 86400, false);
        if (!ItemSystem::InitializeRuneItem(rune) ||
            !InventorySystem::EquipTo(rune, character, WEAR_RUNE1 + i))
            return fail("Rune initialization/equip failed.");
        const bool activated = i == 6 ? ItemSystem::ActivateRuneBonus(rune) : ItemSystem::ActivateRune(rune);
        if (!activated) return fail("Rune activation rejected by real GameServer.");
    }
#endif
    // Exactly one pet. The real pet system reads the summon mob from item
    // value1 and applies the normal pet buff logic, including extra proto.
    {
        const auto petItem = ITEM_MANAGER::instance().CreateItem(49010, 1, 0, false, -1, true);
        if (!ItemSystem::IsValidItem(petItem)) return fail("Cannot create 49010 PvP pet.");
        saved.temporaryItems.push_back(petItem);
        const int cell = InventorySystem::GetEmptyInventory(character, ItemSystem::GetItemSize(petItem));
        if (cell < 0 || !InventorySystem::AddToCharacter(petItem, character,
                TItemPos(INVENTORY, static_cast<uint16_t>(cell))))
            return fail("No inventory space for PvP pet.");
        saved.petMob = static_cast<uint32_t>(ItemSystem::GetItemValue(petItem, 1));
        if (saved.petMob == 0 ||
            !PetSystem::Summon(character, saved.petMob, petItem, "", false)) {
            saved.petMob = 0;
            return fail("PvP pet summon failed.");
        }
    }
    ecs::PointSystem::Compute(character);
    // Apply only self-cast buffs through real skill use/affect logic; never
    // fake external buffs by writing points directly. Unavailable buffs remain
    // explicitly unrepresented in this build.
    for (const uint32_t buff : {3u, 4u, 19u, 49u, 64u, 65u,
                                79u, 94u, 95u, 96u, 110u, 111u})
        if (SkillSystem::CanUseSkill(character, buff))
            SkillSystem::UseSkill(character, buff, character);
    ecs::PointSystem::Change(character, POINT_HP,
        static_cast<int64_t>(ecs::PointSystem::GetMaxHP(character)) - ecs::PointSystem::Get(character, POINT_HP));
    ecs::PointSystem::Change(character, POINT_SP,
        static_cast<int64_t>(ecs::PointSystem::GetMaxSP(character)) - ecs::PointSystem::Get(character, POINT_SP));
    NetworkSyncSystem::PointsPacket(character);
    // An independent, server-side receipt makes it possible to verify that
    // the client really received all profile points/items rather than relying
    // only on a GM-chat success message.
    {
        std::error_code ec;
        std::filesystem::create_directories("benchmark_logs", ec);
        if (!ec) {
            std::ofstream receipt(std::filesystem::path("benchmark_logs") /
                ("profile_" + std::to_string(pid) + ".txt"), std::ios::trunc);
            if (receipt) {
                receipt << "mode=" << (defender ? "DEFENDER" : "ATTACKER")
                        << " tier=" << tier << " vs=" << defenseWeapon << '\n'
                        << "PID=" << pid << " level=" << ecs::PointSystem::GetLevel(character)
                        << " ST=" << ecs::PointSystem::Get(character, POINT_ST)
                        << " HT=" << ecs::PointSystem::Get(character, POINT_HT)
                        << " DX=" << ecs::PointSystem::Get(character, POINT_DX)
                        << " IQ=" << ecs::PointSystem::Get(character, POINT_IQ)
                        << " HP=" << ecs::PointSystem::GetMaxHP(character)
                        << " TWOHAND_RESIST=" << ecs::PointSystem::Get(character, POINT_RESIST_TWOHAND)
                        << " HUMAN_ATTACK=" << ecs::PointSystem::Get(character, POINT_ATTBONUS_HUMAN)
                        << " ALIGNMENT_RAW=" << CombatSystem::GetRealAlignment(character)
                        << " ALIGNMENT_DISPLAY=" << CombatSystem::GetRealAlignment(character) / 10
                        << " ALIGNMENT_GRADE=" << int(CombatSystem::GetAlignmentGrade(character))
                        << " ATTACK_WEAPON_VNUM=" << profileWeapon
                        << " SHIELD_VNUM=" << ([&]() -> uint32_t {
                              const auto shield = ItemSystem::GetWearItem(character, WEAR_SHIELD);
                              return ItemSystem::IsValidItem(shield) ? ItemSystem::GetItemVnum(shield) : 0;
                          }())
                        << " MOUNTS=" << MountSystem::GetMountCount(character)
                        << " PETS=" << PetSystem::CountSummoned(character) << '\n';
                receipt << "costume_count=" << costumeGear.size() << "\n";
                receipt << "necromancer_70403_equivalent=human_attack_10,human_resistance_10"
                        << " costume_slots=body,hair,weapon method=pre_equip_temporary_attributes_no_scroll\n";
                receipt << "weapon_stones=59006,59016,60022 armor_required_stone=62010\n";
#ifdef ENABLE_COSTUME_PET
                receipt << "pet_costume=88027 slot=" << int(WEAR_COSTUME_PET_SKIN)
                        << " source=item_proto five base applies; item_extra_proto=normal equip only\n";
#endif
                receipt << "necromancer_wing=85153 grade=4 slots=6 source=stoleInfoTable"
                        << " roll=max_grade4 70403=not_allowed\n";
                for (const auto [slot, vnum] : costumeGear) {
                    const auto item = ItemSystem::GetWearItem(character, slot);
                    receipt << "costume_slot=" << int(slot) << " vnum=" << vnum;
                    if (ItemSystem::IsValidItem(item)) {
                        if (const auto* proto = ItemSystem::GetItemProto(item))
                            for (const auto& apply : proto->aApplies)
                                if (apply.bType != APPLY_NONE && apply.lValue != 0)
                                    receipt << " base=" << int(apply.bType) << ':' << apply.lValue;
#ifdef ENABLE_ITEM_EXTRA_PROTO
#ifdef ENABLE_NEW_EXTRA_BONUS
                        if (const auto* extra = ItemSystem::GetItemExtraProto(item))
                            for (const auto& apply : extra->ExtraBonus)
                                if (apply.bType != APPLY_NONE && apply.lValue != 0)
                                    receipt << " extra=" << int(apply.bType) << ':' << apply.lValue;
#endif
#endif
                        for (int a = 0; a < ITEM_ATTRIBUTE_MAX_NUM; ++a) {
                            const auto attr = ItemSystem::GetItemAttribute(item, a);
                            if (attr.bType) receipt << " attr=" << int(attr.bType) << ':' << attr.sValue;
                        }
                    }
                    receipt << '\n';
                }
                for (const auto [slot, vnum] : gear) {
                    const auto current = ItemSystem::GetWearItem(character, slot);
                    receipt << "slot=" << int(slot) << " vnum="
                            << (ItemSystem::IsValidItem(current) ? ItemSystem::GetItemVnum(current) : 0)
                            << " attrs=";
                    if (ItemSystem::IsValidItem(current))
                        for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i) {
                            const auto attr = ItemSystem::GetItemAttribute(current, i);
                            if (attr.bType) receipt << int(attr.bType) << ':' << attr.sValue << ';';
                        }
                    receipt << " sockets=";
                    if (ItemSystem::IsValidItem(current))
                        for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
                            const uint32_t socketVnum = ItemSystem::GetItemSocket(current, i);
                            if (socketVnum) receipt << i << ':' << socketVnum << ';';
                        }
                    receipt << '\n';
                }
            }
        }
    }
    applied.emplace(pid, std::move(saved));
    std::ostringstream reply;
    reply << "PC leveled to " << tier << ", STR/DEX/INT/VIT=90, usable skills P(40); "
          << "alignment=max (" << CombatSystem::GetRealAlignment(character) / 10
          << " displayed), " << equipped
          << " final Zodiac PvP items equipped (shield VNUM 8699 verified), legal attributes / sockets; "
          << "weapon stones 59006/59016/60022, armor includes 62010. "
          << (defender ? "Defense against " : "Attacker profile; ") << defenseWeapon
          << ". Four Necromancer pieces and one Baashido pet costume equipped ("
          << costumeGear.size() << " costume slots); "
          << "70403-equivalent HUMAN+10/RESIST+10 on body, hair and weapon (no scroll consumed); "
          << "Necromancer wing 85153: all six grade-four stole bonuses maximized. "
          << "Baashido pet costume 88027 equipped in pet-skin slot (base bonuses via EquipTo). "
          << "item_extra_proto applied by real equip logic. "
          << "Live mount whitelist (123), 16 max belts, 7 runes and one PvP pet applied; self-buffs only.";
    message = reply.str();
    return true;
}

void Restore(entt::entity character, std::string& message) {
    if (!ecs::PlayerRuntime::IsPC(character)) { message = "Offline PC."; return; }
    const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(character);
    const auto it = applied.find(pid);
    if (it == applied.end()) { message = "No benchmark equipment profile to restore."; return; }
    std::ostringstream issues;
    Undo(it->second, issues);
    message = issues.str().empty() ? "Original character state restored." : issues.str();
    if (issues.str().empty()) applied.erase(it);
}

} // namespace nexoria::benchmark::equipment
