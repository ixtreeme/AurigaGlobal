#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/entity/char.h"
#include "../../SRC/Server/GameServer/combat/buff_on_attributes.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>

// Runs the production native buff implementation. Equipment lookup and point
// application are controlled service seams, not copies of the buff algorithm.
entt::registry g_registry;
namespace {
int checks = 0;
struct Equipment { std::map<uint8_t, entt::entity> slots; };
struct TestItem {
    entt::entity owner { entt::null };
    uint16_t cell {};
    int16_t locked { -1 };
    std::vector<TPlayerItemAttribute> attributes;
};
std::map<uint8_t, int> totals;
std::function<void(entt::entity)> afterApply;
void Check(bool ok, const char* reason) {
    ++checks;
    if (!ok) throw std::runtime_error(reason);
}
entt::entity Owner() {
    afterApply = {};
    g_registry.clear();
    totals.clear();
    auto owner = g_registry.create();
    g_registry.emplace<Equipment>(owner);
    return owner;
}
entt::entity Wear(entt::entity owner, uint8_t slot, uint8_t type, int16_t value) {
    auto item = g_registry.create();
    auto& data = g_registry.emplace<TestItem>(item);
    data.owner = owner;
    data.cell = static_cast<uint16_t>(INVENTORY_MAX_NUM + slot);
    TPlayerItemAttribute attribute {};
    attribute.bType = type;
    attribute.sValue = value;
    data.attributes.push_back(attribute);
    g_registry.get<Equipment>(owner).slots[slot] = item;
    return item;
}
void Energy(entt::entity owner, uint8_t previous, uint8_t value) {
    ecs::PlayerRuntime::BuffOnAttr_ValueChange(owner, POINT_ENERGY, previous, value);
}
void AggregationAndEquipmentChanges() {
    auto owner = Owner();
    Wear(owner, WEAR_BODY, APPLY_MAX_HP, 7);
    Wear(owner, WEAR_HEAD, APPLY_MAX_HP, 7);
    Energy(owner, 0, 10);
    Check(totals[APPLY_MAX_HP] == 1, "energy must round the combined equipment total");
    auto extra = Wear(owner, WEAR_WEAPON, APPLY_MAX_HP, 6);
    ecs::PlayerRuntime::BuffOnAttr_AddBuffsFromItem(owner, extra);
    Check(totals[APPLY_MAX_HP] == 2, "adding equipment must apply rounded aggregate delta");
    ecs::PlayerRuntime::BuffOnAttr_RemoveBuffsFromItem(owner, extra);
    g_registry.get<Equipment>(owner).slots.erase(WEAR_WEAPON);
    Check(totals[APPLY_MAX_HP] == 1, "removing equipment must undo rounded aggregate delta");
    Energy(owner, 10, 0);
    Check(totals[APPLY_MAX_HP] == 0, "turning energy off must remove its applied bonus");
    Energy(owner, 0, 0);
    Check(totals[APPLY_MAX_HP] == 0, "turning inactive energy off must be idempotent");
}
void ValueChangeAndPointRecomputation() {
    auto owner = Owner();
    Wear(owner, WEAR_BODY, APPLY_MAX_HP, 100);
    Energy(owner, 0, 10);
    Energy(owner, 10, 20);
    Check(totals[APPLY_MAX_HP] == 0,
        "nonzero percentage change must subtract old bonus without adding the new one");
    Check(g_registry.get<ecs::BuffOnAttrs>(owner).pools.at(POINT_ENERGY).value == 20,
        "percentage change must retain the new pool percentage");
    ecs::PlayerRuntime::BuffOnAttr_ClearAll(owner);
    Check(totals[APPLY_MAX_HP] == 0, "recompute clearing after percentage change must not subtract");
    Energy(owner, 0, 20);
    Check(totals[APPLY_MAX_HP] == 20, "reactivating must aggregate the current equipment");
    ecs::PlayerRuntime::BuffOnAttr_ClearAll(owner);
    Check(totals[APPLY_MAX_HP] == 20, "ClearAll is part of point recomputation and must not subtract");
    totals.clear(); // ComputePoints replaces base point totals before reactivating buffs.
    Energy(owner, 0, 20);
    Check(totals[APPLY_MAX_HP] == 20, "recompute must apply each bonus exactly once");
    ecs::PlayerRuntime::BuffOnAttr_Destroy(owner);
    Check(totals[APPLY_MAX_HP] == 20, "Destroy must release state without changing points");
    ecs::PlayerRuntime::BuffOnAttr_Destroy(owner);
    auto* remaining = g_registry.try_get<ecs::BuffOnAttrs>(owner);
    Check(!remaining || remaining->pools.empty(), "Destroy must leave no owned pools");
}
void CostumeAndSlotIsolation() {
    auto owner = Owner();
    Wear(owner, WEAR_BODY, APPLY_MAX_HP, 100);
    Wear(owner, WEAR_COSTUME_BODY, APPLY_MAX_HP, 80);
    Energy(owner, 0, 10);
    Check(totals[APPLY_MAX_HP] == 10, "energy must not count costume equipment");
    ecs::PlayerRuntime::BuffOnAttr_ValueChange(owner, POINT_COSTUME_ATTR_BONUS, 0, 25);
    Check(totals[APPLY_MAX_HP] == 30, "costume buff must use its own slot set");
    Energy(owner, 10, 0);
    Check(totals[APPLY_MAX_HP] == 20, "turning off energy must preserve costume bonus");
    ecs::PlayerRuntime::BuffOnAttr_ValueChange(owner, POINT_COSTUME_ATTR_BONUS, 25, 0);
    Check(totals[APPLY_MAX_HP] == 0, "costume off must remove only its bonus");
}
void InvalidEntitiesAndUnsupportedType() {
    auto owner = Owner();
    auto item = Wear(owner, WEAR_BODY, APPLY_MAX_HP, 100);
    Energy(owner, 0, 10);
    g_registry.destroy(item);
    ecs::PlayerRuntime::BuffOnAttr_AddBuffsFromItem(owner, item);
    ecs::PlayerRuntime::BuffOnAttr_RemoveBuffsFromItem(owner, item);
    ecs::PlayerRuntime::BuffOnAttr_AddBuffsFromItem(owner, entt::null);
    Check(totals[APPLY_MAX_HP] == 10, "stale and null items must not alter buff state");
    auto count = g_registry.get<ecs::BuffOnAttrs>(owner).pools.size();
    ecs::PlayerRuntime::BuffOnAttr_ValueChange(owner, POINT_LEVEL, 0, 20);
    ecs::PlayerRuntime::BuffOnAttr_ValueChange(owner, POINT_LEVEL, 20, 30);
    Check(g_registry.get<ecs::BuffOnAttrs>(owner).pools.size() == count,
        "unsupported point types must not create invalid pools");
    g_registry.destroy(owner);
    Energy(owner, 0, 10);
    Energy(entt::null, 0, 10);
    ecs::PlayerRuntime::BuffOnAttr_ClearAll(owner);
    ecs::PlayerRuntime::BuffOnAttr_Destroy(owner);
    ecs::PlayerRuntime::BuffOnAttr_RemoveBuffsFromItem(owner, item);
    Check(totals[APPLY_MAX_HP] == 10, "stale owners must not apply or remove points");
}
void CallbackRemoval() {
    auto owner = Owner();
    Wear(owner, WEAR_BODY, APPLY_MAX_HP, 100);
    Wear(owner, WEAR_HEAD, APPLY_MAX_SP, 100);
    int calls = 0;
    afterApply = [&](entt::entity target) {
        ++calls;
        g_registry.remove<ecs::BuffOnAttrs>(target);
    };
    Energy(owner, 0, 10);
    Check(calls == 1, "component removal during point application must stop further writes");
    Check(!g_registry.all_of<ecs::BuffOnAttrs>(owner), "callback-removed state must not be resurrected");
    afterApply = {};
    totals.clear();
    Energy(owner, 0, 10);
    afterApply = [&](entt::entity target) { ++calls; g_registry.destroy(target); };
    Energy(owner, 10, 0);
    Check(!g_registry.valid(owner), "owner destruction during buff removal must be safe");
    afterApply = {};
}
void NestedOffDuringActivation() {
    auto owner = Owner();
    Wear(owner, WEAR_BODY, APPLY_MAX_HP, 100);
    Wear(owner, WEAR_HEAD, APPLY_MAX_SP, 100);
    bool nested = false;
    afterApply = [&](entt::entity target) {
        if (!nested) {
            nested = true;
            Energy(target, 10, 0);
        }
    };
    Energy(owner, 0, 10);
    afterApply = {};
    Check(nested, "activation must reach the controlled point callback");
    Check(totals[APPLY_MAX_HP] == 0 && totals[APPLY_MAX_SP] == 0,
        "nested Off must cancel pending bonuses and remove the already applied delta");
    Check(g_registry.get<ecs::BuffOnAttrs>(owner).pending.empty(),
        "nested buff operation must fully drain pending point deltas");
}
#ifdef ATTR_LOCK
void LockedAttribute() {
    auto owner = Owner();
    auto item = Wear(owner, WEAR_BODY, APPLY_MAX_HP, 100);
    g_registry.get<TestItem>(item).locked = 0;
    Energy(owner, 0, 10);
    Check(totals[APPLY_MAX_HP] == 0, "locked equipment attribute must be excluded on activation");
}
#endif
}

namespace ItemSystem {
bool IsValidItem(entt::entity item) {
    return g_registry.valid(item) && g_registry.all_of<TestItem>(item);
}
entt::entity GetWearItem(entt::entity owner, uint8_t slot) {
    const auto* equipment = g_registry.valid(owner) ? g_registry.try_get<Equipment>(owner) : nullptr;
    if (!equipment) return entt::null;
    auto found = equipment->slots.find(slot);
    return found == equipment->slots.end() ? entt::null : found->second;
}
uint16_t GetItemCell(entt::entity item) { return g_registry.get<TestItem>(item).cell; }
int GetItemAttributeCount(entt::entity item) {
    return static_cast<int>(g_registry.get<TestItem>(item).attributes.size());
}
TPlayerItemAttribute GetItemAttribute(entt::entity item, int index) {
    return g_registry.get<TestItem>(item).attributes.at(index);
}
int16_t GetItemLockedAttributeIndex(entt::entity item) { return g_registry.get<TestItem>(item).locked; }
entt::entity GetItemOwnerEntity(entt::entity item) { return g_registry.get<TestItem>(item).owner; }
const char* GetItemName(entt::entity) { return "buff-test-item"; }
uint32_t GetItemVnum(entt::entity) { return 1000; }
}
void ecs::PointSystem::ApplyPoint(entt::entity owner, uint8_t type, int amount) {
    Check(g_registry.valid(owner), "point application must not receive a stale owner");
    totals[type] += amount;
    if (afterApply) afterApply(owner);
}
void ecs::ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, const char*, ...) {}
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("buff-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }

int main() {
    try {
        AggregationAndEquipmentChanges();
        ValueChangeAndPointRecomputation();
        CostumeAndSlotIsolation();
        InvalidEntitiesAndUnsupportedType();
        CallbackRemoval();
        NestedOffDuringActivation();
#ifdef ATTR_LOCK
        LockedAttribute();
#endif
        afterApply = {};
        g_registry.clear();
        std::cout << "Buff attribute tests passed: " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Buff attribute test failure: " << error.what() << '\n';
        return 1;
    }
}
