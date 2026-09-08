#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/new_switchbot.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/item_proto_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/inventory_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/attr_transfer.h"
#include "../../SRC/Server/GameServer/ecs/detail/ItemAttributeRules.hpp"
#include "../../SRC/Server/GameServer/constants.h"
#include "../../SRC/Server/GameServer/log.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/buffer_manager.h"
#include "../../SRC/Server/GameServer/p2p.h"
#include "../../SRC/Server/GameServer/battle_pass.h"
#include "../../SRC/Server/common/stole_length.h"
#include "../../SRC/Server/common/rune_length.h"
#include "../../SRC/Server/GameServer/DragonSoul.h"
#include "../../SRC/Server/GameServer/dragon_soul_table.h"
#include "../../SRC/Server/GameServer/ecs/systems/DragonSoulSystem.hpp"
#include "../../SRC/Server/GameServer/item_manager.h"
#include <Core/Logging.hpp>
#include <stdexcept>
#include <iostream>
#include <cstdlib>

entt::registry g_registry;
TItemAttrMap g_map_itemAttr;
TItemAttrMap g_map_itemRare;
int g_iDbLogLevel = 0;
int g_bItemCountLimit = 200;
const int aiItemMagicAttributePercentHigh[ITEM_ATTRIBUTE_MAX_LEVEL] = {0, 0, 0, 0, 100};
const int aiItemMagicAttributePercentLow[ITEM_ATTRIBUTE_MAX_LEVEL] = {100, 0, 0, 0, 0};

namespace {
int saves = 0;
int updates = 0;
int randomCalls = 0;
int randomOffset = 0;
int checks = 0;
int payments = 0;
bool rejectPayment = false;
bool rejectGoldPayment = false;
bool transferTest = false;
bool soulStateTest = false;
bool countStateTest = false;
int stackCategoryLookups = 0;
int soulAdds = 0, soulRemoves = 0, soulStarts = 0, soulStops = 0, soulLogs = 0, deckStops = 0;
bool rejectSoulPoints = false, rejectSoulTimer = false;
std::map<entt::entity, int> soulBonus;
std::set<entt::entity> soulTimers;
std::function<void(entt::entity, bool)> onSoulPoints;
std::function<void(entt::entity)> onSoulStart, onSoulStop;
bool extractionTest = false, rejectCreation = false, rejectSocket = false, rejectPlace = false;
bool allowHandling = true, allowUnequip = true, dsHeartOk = true, dsPullOk = true;
int emptyDSCell = -1, extractionLogs = 0;
float pullProbability = 100.f;
uint32_t byProductVnum = 0;
std::vector<float> heartCharges, heartProbabilities;
std::vector<entt::entity> createdOutputs, givenOutputs;
std::function<void(entt::entity)> onCreate, onSocket, onRemove, onPlace, onLog;
TItemTable outputProto {};

int rejectPaymentAt = 0, transferLogs = 0;
std::vector<std::string> transferCommands;
std::function<void()> onPayment;
std::function<void(entt::entity)> onSave, onUpdate, onDestroy;
std::vector<entt::entity> publishedCounts;
std::vector<entt::entity> destroyAttempts;
std::set<entt::entity> rejectDestruction;
struct TransferActor { int32_t x = 0, y = 0, map = 1; entt::entity npc {entt::null}; };
entt::entity watchedItem = entt::null;
std::array<TPlayerItemAttribute, ITEM_ATTRIBUTE_MAX_NUM> beforePayment{};
short lockBeforePayment = -1;
std::map<std::tuple<entt::entity, uint8_t, uint16_t>, entt::entity> inventory;
struct TestPlayer { int64_t gold = 100; int activeDeck = -1; };
struct TestExtraStack {};
DragonSoulTable::TVecApplys dsBasic, dsAdditional;
int dsBasicCount = 3, dsAddMin = 2, dsAddMax = 2;
float dsWeight = 100.f, floatDrawFraction = 0.f;
int floatRandomCalls = 0, dsLiveTables = 0;
bool dsReadOk = true, dsBasicOk = true, dsAdditionalOk = true, dsSettingsOk = true, dsWeightOk = true;
void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

bool EqualAttributes(const auto& left, const auto& right)
{
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        if (left[i].bType != right[i].bType || left[i].sValue != right[i].sValue)
            return false;
    return true;
}

void CheckPaymentOrder()
{
    Check(watchedItem != entt::null && g_registry.valid(watchedItem), "payment has no live target");
    Check(EqualAttributes(g_registry.get<ecs::ItemAttributes>(watchedItem).attrs, beforePayment),
        "attributes changed before payment succeeded");
    Check(ItemSystem::GetItemLockedAttr(watchedItem) == lockBeforePayment,
        "attribute lock changed before payment succeeded");
    Check(saves == 0 && updates == 0, "partial attributes published before payment");
}

[[noreturn]] void UnexpectedSwitchbotService()
{
    std::cerr << "Unexpected timer/UI/manager service call in a transaction test\n";
    std::abort();
}
}

// The entire production new_switchbot.cpp is compiled, not a copied test-only
// slice. These dependencies belong to the timer/UI/manager paths outside the
// transaction tests; fail immediately if the tests accidentally enter them.
int passes_per_sec = 25;
std::shared_ptr<spdlog::logger> logging::GetErrorLogger()
{
    if (!transferTest && !extractionTest && !countStateTest) UnexpectedSwitchbotService();
    static auto logger = std::make_shared<spdlog::logger>("transfer-test", spdlog::sinks_init_list{});
    return logger;
}
void intrusive_ptr_add_ref(EVENT*) { UnexpectedSwitchbotService(); }
void intrusive_ptr_release(EVENT*) { UnexpectedSwitchbotService(); }
LPEVENT event_create_ex(TEVENTFUNC, event_info_data*, int32_t) { UnexpectedSwitchbotService(); }
void event_cancel(LPEVENT*) { UnexpectedSwitchbotService(); }
#ifdef TEXTS_IMPROVEMENT
void ecs::ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, const char*, ...) { if (!transferTest && !extractionTest) UnexpectedSwitchbotService(); }
#endif
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity) { if (!transferTest && !extractionTest) UnexpectedSwitchbotService(); return 42; }
std::string_view ecs::PlayerRuntime::GetName(entt::entity) { UnexpectedSwitchbotService(); }
#ifdef ENABLE_BATTLE_PASS
uint8_t ecs::PlayerRuntime::GetBattlePassId(entt::entity) { UnexpectedSwitchbotService(); }
uint32_t ecs::PlayerRuntime::GetMissionProgress(entt::entity, uint32_t, uint32_t) { UnexpectedSwitchbotService(); }
bool ecs::PlayerRuntime::UpdateMissionProgress(entt::entity, uint32_t, uint32_t, uint32_t, uint32_t, bool) { UnexpectedSwitchbotService(); }
bool CBattlePass::BattlePassMissionGetInfo(uint8_t, uint8_t, uint32_t*, uint32_t*) { UnexpectedSwitchbotService(); }
#endif
#ifdef ENABLE_RANKING
int64_t ecs::PlayerRuntime::GetRankPoints(entt::entity, int) { UnexpectedSwitchbotService(); }
bool ecs::PlayerRuntime::SetRankPoints(entt::entity, int, int64_t) { UnexpectedSwitchbotService(); }
#endif
void DESC::BufferedPacket(const void*, int) { UnexpectedSwitchbotService(); }
void DESC::Packet(const void*, int) { UnexpectedSwitchbotService(); }
void BroadcastNotice(const char*, bool) { UnexpectedSwitchbotService(); }
TEMP_BUFFER::TEMP_BUFFER(int, bool) { UnexpectedSwitchbotService(); }
TEMP_BUFFER::~TEMP_BUFFER() = default; // The fail-fast constructor never creates a buffer.
const void* TEMP_BUFFER::read_peek() { UnexpectedSwitchbotService(); }
void TEMP_BUFFER::write(const void*, int) { UnexpectedSwitchbotService(); }
int TEMP_BUFFER::size() { UnexpectedSwitchbotService(); }
entt::entity CHARACTER_MANAGER::FindEntityByPID(uint32_t) { UnexpectedSwitchbotService(); }
void P2P_MANAGER::Send(const void*, int, LPDESC) { UnexpectedSwitchbotService(); }
entt::entity ItemSystem::FindItemByID(uint32_t) { UnexpectedSwitchbotService(); }
const char* ItemSystem::GetItemName(entt::entity item) {
    Check(extractionTest && ItemSystem::IsValidItem(item), "stale extraction item name");
    return "dragon-soul-test";
}
uint8_t ItemSystem::GetItemSize(entt::entity item) {
    Check(extractionTest && ItemSystem::IsValidItem(item), "stale extraction item size");
    return ItemSystem::GetItemProto(item)->bSize;
}
entt::entity ItemSystem::GetItemOwnerEntity(entt::entity) { UnexpectedSwitchbotService(); }
int ItemSystem::GetItemAttributeType(entt::entity, int) { UnexpectedSwitchbotService(); }
int ItemSystem::GetItemAttributeValue(entt::entity, int) { UnexpectedSwitchbotService(); }

// Compile the complete production DragonSoul.cpp. Unused refinement, timer,
// legacy-character and inventory-movement services must never be entered here.
int MIN(int a, int b) { return std::min(a, b); }
time_t get_global_time() { UnexpectedSwitchbotService(); }
void DragonSoulSystem::DeactivateAll(entt::entity owner) {
    Check(soulStateTest && ecs::PlayerRuntime::IsPC(owner), "invalid deck refresh");
    ++deckStops; g_registry.get<TestPlayer>(owner).activeDeck = -1;
}
bool DragonSoulSystem::CanRefine(entt::entity) { UnexpectedSwitchbotService(); }
int32_t DragonSoulSystem::GetLastRefineTime(entt::entity) { UnexpectedSwitchbotService(); }
void DragonSoulSystem::SetLastRefineTime(entt::entity) { UnexpectedSwitchbotService(); }
ITEM_MANAGER::ITEM_MANAGER() {}
ITEM_MANAGER::~ITEM_MANAGER() {}
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() = default;
CAsyncSQL::~CAsyncSQL() = default;
LogManager::LogManager() : m_bIsConnect(false) {}
LogManager::~LogManager() = default;
void LogManager::ItemLog(entt::entity owner, int, int, const char*, const char*) {
    Check(extractionTest && ecs::PlayerRuntime::IsPC(owner), "extraction log read stale owner");
    ++extractionLogs;
    if (onLog) onLog(owner);
}
entt::entity ITEM_MANAGER::CreateItem(uint32_t vnum, uint32_t count, uint32_t, bool, int, bool) {
    if (!extractionTest) UnexpectedSwitchbotService();
    if (rejectCreation) return entt::null;
    const auto item = g_registry.create();
    g_registry.emplace<ecs::ItemIdentity>(item, ecs::ItemIdentity{500, vnum, vnum});
    g_registry.emplace<ecs::ItemCount>(item, ecs::ItemCount{static_cast<int>(count)});
    g_registry.emplace<ecs::ItemProtoRef>(item).proto = &outputProto;
    g_registry.emplace<ecs::ItemLocation>(item, ecs::ItemLocation{RESERVED_WINDOW, 0});
    createdOutputs.push_back(item);
    if (onCreate) onCreate(item);
    return item;
}
bool DragonSoulTable::GetRefineGradeValues(uint8_t, uint8_t, int&, int&, std::vector<float>&) { UnexpectedSwitchbotService(); }
bool DragonSoulTable::GetRefineStepValues(uint8_t, uint8_t, int&, int&, std::vector<float>&) { UnexpectedSwitchbotService(); }
bool DragonSoulTable::GetRefineStrengthValues(uint8_t, uint8_t, uint8_t, int&, float&) { UnexpectedSwitchbotService(); }
bool DragonSoulTable::GetDragonHeartExtValues(uint8_t, uint8_t, std::vector<float>& charges, std::vector<float>& probabilities) {
    if (!extractionTest) UnexpectedSwitchbotService();
    charges = heartCharges; probabilities = heartProbabilities; return dsHeartOk;
}
bool DragonSoulTable::GetDragonSoulExtValues(uint8_t, uint8_t, float& probability, uint32_t& byproduct) {
    if (!extractionTest) UnexpectedSwitchbotService();
    probability = pullProbability; byproduct = byProductVnum; return dsPullOk;
}
void LogManager::ItemLogEntity(LPCHARACTER, entt::entity, const char*, const char*) { UnexpectedSwitchbotService(); }
entt::entity ItemSystem::GetWearItem(entt::entity owner, uint8_t wear) {
    if (!extractionTest) UnexpectedSwitchbotService();
    return ItemSystem::GetItem(owner, TItemPos(EQUIPMENT, INVENTORY_MAX_NUM + wear));
}
void ItemSystem::AutoGiveItem(entt::entity owner, entt::entity item, bool
#ifdef __HIGHLIGHT_SYSTEM__
    , bool
#endif
) {
    Check(extractionTest && ecs::PlayerRuntime::IsPC(owner) && ItemSystem::IsValidItem(item),
        "extraction output delivered to stale entity");
    givenOutputs.push_back(item);
    g_registry.emplace_or_replace<ecs::ItemOwner>(item, ecs::ItemOwner{owner});
    g_registry.get<ecs::ItemLocation>(item) = {INVENTORY, 100};
    inventory[{owner, INVENTORY, 100}] = item;
}
bool ItemSystem::AutoGiveDS(entt::entity, entt::entity, bool) { UnexpectedSwitchbotService(); }
entt::entity ItemSystem::AutoGiveItemEcs(entt::entity, uint32_t, uint32_t, int, bool) { UnexpectedSwitchbotService(); }
int ItemSystem::GetItemLimitTimerBasedOnWearIndex(entt::entity item) {
    if (!soulStateTest) UnexpectedSwitchbotService();
    Check(ItemSystem::IsValidItem(item), "stale soul lifetime lookup");
    return ItemSystem::GetItemProto(item)->cLimitTimerBasedOnWearIndex;
}
int ItemSystem::GetItemDuration(entt::entity) { return 0; }
bool ItemSystem::DestroyItemEntityEcs(entt::entity item, const char*)
{
    const bool retired = ItemSystem::IsItemConsumptionPending(item) && ItemSystem::GetItemCount(item) == 0;
    Check(countStateTest || ((transferTest || extractionTest) && (retired ||
        (extractionTest && ItemSystem::GetItemOwner(item) == entt::null))),
        "cleanup entered before committed item retirement");
    destroyAttempts.push_back(item);
    if (onDestroy) onDestroy(item);
    if (rejectDestruction.contains(item)) return false;
    if (ItemSystem::IsValidItem(item)) {
        inventory.erase({ItemSystem::GetItemOwner(item), ItemSystem::GetItemWindow(item), ItemSystem::GetItemCell(item)});
        g_registry.destroy(item);
    }
    return true;
}
bool ItemSystem::SetItemSocketEcs(entt::entity item, int index, uint32_t value) {
    if (!extractionTest) UnexpectedSwitchbotService();
    if (rejectSocket) return false;
    g_registry.get_or_emplace<ecs::ItemSockets>(item).sockets[index] = value;
    if (onSocket) onSocket(item);
    return true;
}
bool ItemSystem::CopyItemAttributesEcs(entt::entity, entt::entity) { UnexpectedSwitchbotService(); }
bool ItemSystem::PlaceItemEcs(entt::entity owner, entt::entity item, uint8_t window, uint16_t cell) {
    if (!extractionTest) UnexpectedSwitchbotService();
    if (rejectPlace || !ecs::PlayerRuntime::IsPC(owner) || !IsValidItem(item) ||
        GetItemOwner(item) != entt::null || GetItem(owner, TItemPos(window, cell)) != entt::null) return false;
    g_registry.emplace_or_replace<ecs::ItemOwner>(item, ecs::ItemOwner{owner});
    g_registry.get<ecs::ItemLocation>(item) = {window, cell};
    g_registry.get_or_emplace<ecs::ItemEquipped>(item).equipped = window == EQUIPMENT;
    inventory[{owner, window, cell}] = item;
    if (onPlace) onPlace(item);
    return true;
}
bool ItemSystem::RemoveItemEcs(entt::entity item) {
    Check(extractionTest && IsValidItem(item), "stale extraction removal");
    inventory.erase({GetItemOwner(item), GetItemWindow(item), GetItemCell(item)});
    g_registry.get<ecs::ItemOwner>(item).owner = entt::null;
    g_registry.get<ecs::ItemLocation>(item) = {RESERVED_WINDOW, 0};
    g_registry.get_or_emplace<ecs::ItemEquipped>(item).equipped = false;
    if (onRemove) onRemove(item);
    return true;
}
int ItemSystem::GetEmptyDragonSoulInventory(entt::entity, entt::entity) {
    if (!extractionTest) UnexpectedSwitchbotService();
    return emptyDSCell;
}
bool InventorySystem::CanHandleItems(entt::entity owner, bool, bool) {
    if (!extractionTest) UnexpectedSwitchbotService();
    return allowHandling && ecs::PlayerRuntime::IsPC(owner);
}
bool InventorySystem::CanUnequipNow(entt::entity, entt::entity item, bool) {
    if (!extractionTest) UnexpectedSwitchbotService();
    return allowUnequip && !ItemSystem::IsItemLocked(item) && !ItemSystem::IsItemExchanging(item);
}
bool InventorySystem::IsEmptyItemGrid(entt::entity owner, TItemPos cell, uint8_t size, int) {
    if (!extractionTest) UnexpectedSwitchbotService();
    return size == 1 && cell.cell < DRAGON_SOUL_INVENTORY_MAX_NUM &&
        ItemSystem::GetItem(owner, cell) == entt::null;
}
bool InventorySystem::EquipTo(entt::entity item, entt::entity owner, uint8_t wear) {
    if (!extractionTest) UnexpectedSwitchbotService();
    if (ItemSystem::GetWearItem(owner, wear) != entt::null) return false;
    if (ItemSystem::GetItemOwner(item) != entt::null && !ItemSystem::RemoveItemEcs(item)) return false;
    const bool blocked = rejectPlace; rejectPlace = false;
    const bool result = ItemSystem::PlaceItemEcs(owner, item, EQUIPMENT, INVENTORY_MAX_NUM + wear);
    rejectPlace = blocked;
    return result;
}
bool ItemSystem::ModifyItemPointsEcs(entt::entity item, bool add) {
    Check(soulStateTest && IsValidItem(item) && ecs::PlayerRuntime::IsPC(GetItemOwner(item)),
        "invalid soul point operation");
    if (rejectSoulPoints) return false;
    (add ? soulAdds : soulRemoves)++;
    soulBonus[GetItemOwner(item)] += add ? 1 : -1;
    if (onSoulPoints) onSoulPoints(item, add);
    return true;
}
bool ItemSystem::StartTimerBasedOnWearExpireEventEcs(entt::entity item) {
    Check(soulStateTest && IsValidItem(item), "invalid soul timer start");
    if (rejectSoulTimer) return false;
    ++soulStarts; soulTimers.insert(item);
    if (onSoulStart) onSoulStart(item);
    return true;
}
bool ItemSystem::StopTimerBasedOnWearExpireEventEcs(entt::entity item) {
    Check(soulStateTest && IsValidItem(item), "invalid soul timer stop");
    ++soulStops; soulTimers.erase(item);
    if (onSoulStop) onSoulStop(item);
    return true;
}
bool ItemSystem::SyncItemStateFromLegacy(entt::entity) { UnexpectedSwitchbotService(); }

DragonSoulTable::DragonSoulTable() { ++dsLiveTables; }
DragonSoulTable::~DragonSoulTable() { --dsLiveTables; }
bool DragonSoulTable::ReadDragonSoulTableFile(const char*) { return dsReadOk; }
bool DragonSoulTable::GetBasicApplys(uint8_t, TVecApplys& out) { out = dsBasic; return dsBasicOk; }
bool DragonSoulTable::GetAdditionalApplys(uint8_t, TVecApplys& out) { out = dsAdditional; return dsAdditionalOk; }
bool DragonSoulTable::GetApplyNumSettings(uint8_t, uint8_t, int& basic, int& minimum, int& maximum)
{
    basic = dsBasicCount; minimum = dsAddMin; maximum = dsAddMax; return dsSettingsOk;
}
bool DragonSoulTable::GetWeight(uint8_t, uint8_t, uint8_t, uint8_t, float& weight) { weight = dsWeight; return dsWeightOk; }
float fnumber(float low, float high)
{
    Check(std::isfinite(low) && std::isfinite(high) && low <= high, "invalid floating RNG interval");
    ++floatRandomCalls;
    return low + (high - low) * floatDrawFraction;
}
int DragonSoulSystem::GetActiveDeck(entt::entity owner) { return g_registry.get<TestPlayer>(owner).activeDeck; }
bool DragonSoulSystem::IsDeckActivated(entt::entity owner) { return GetActiveDeck(owner) >= 0; }
bool MakeDistinctRandomNumberSet(std::list<float>, std::vector<int>&);

// Deterministic I/O doubles. The system under test never constructs CItem or
// CHARACTER, and fixtures deliberately contain no LegacyItemPtr component.
int number_ex(int low, int high, const char*, int)
{
    Check(low <= high, "invalid random interval");
    ++randomCalls;
    Check(randomOffset >= 0 && randomOffset <= high - low, "scripted random offset out of range");
    return low + randomOffset;
}
float gauss_random(float, float) { return 4.0f; }
void LogManager::ItemLog(uint32_t, uint32_t, uint32_t, uint32_t, const char*, const char*, const char*, uint32_t) {}
void LogManager::ItemLogEntity(entt::entity owner, entt::entity item, const char*, const char*) {
    if (soulStateTest) {
        Check(ecs::PlayerRuntime::IsPC(owner) && ItemSystem::IsValidItem(item), "stale soul log");
        ++soulLogs;
        if (onLog) onLog(owner);
    }
}
LPDESC ecs::PlayerRuntime::GetDesc(entt::entity) { return nullptr; }
bool ecs::PlayerRuntime::IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e); }
bool ecs::PlayerRuntime::IsPC(entt::entity e) { return IsValid(e) && g_registry.all_of<TestPlayer>(e); }
CShop* ecs::SocialSystem::GetMyShop(entt::entity e)
{
    const auto* shop = g_registry.try_get<ecs::ShopState>(e);
    return shop ? shop->myShop : nullptr;
}
CShop* ecs::SocialSystem::GetShop(entt::entity e)
{
    const auto* shop = g_registry.try_get<ecs::ShopState>(e);
    return shop ? shop->currentShop : nullptr;
}
entt::entity ecs::SocialSystem::GetShopOwner(entt::entity e)
{
    const auto* shop = g_registry.try_get<ecs::ShopState>(e);
    return shop ? shop->shopOwner : entt::entity{entt::null};
}
bool ecs::SocialSystem::HasExchange(entt::entity e)
{
    const auto* ref = g_registry.try_get<ecs::ExchangeRef>(e);
    const auto* session = ref && g_registry.valid(ref->session)
        ? g_registry.try_get<ecs::ExchangeSession>(ref->session) : nullptr;
    return session && (session->offers[0].owner == e || session->offers[1].owner == e);
}
int32_t ecs::PlayerRuntime::GetX(entt::entity e) { return g_registry.get<TransferActor>(e).x; }
int32_t ecs::PlayerRuntime::GetY(entt::entity e) { return g_registry.get<TransferActor>(e).y; }
int32_t ecs::PlayerRuntime::GetMapIndex(entt::entity e) { return g_registry.get<TransferActor>(e).map; }
entt::entity ecs::PlayerRuntime::GetQuestNPC(entt::entity e) { return g_registry.get<TransferActor>(e).npc; }
void ecs::ChatSystem::Send(entt::entity, uint8_t type, const char* message, ...)
{
    Check(transferTest && type == CHAT_TYPE_COMMAND, "unexpected chat service");
    transferCommands.emplace_back(message);
}
void LogManager::AttrTransferLog(uint32_t pid, uint32_t, uint32_t, uint32_t)
{
    Check(transferTest && pid == 42 && saves == 1 && updates == 1, "transfer logged before complete publish");
    ++transferLogs;
}
int64_t ecs::PointSystem::GetGold(entt::entity e) { return g_registry.get<TestPlayer>(e).gold; }
void ecs::PointSystem::Change(entt::entity e, uint8_t type, int64_t amount, bool, bool
#ifdef __ENABLE_BLOCK_EXP__
    , bool
#endif
)
{
    CheckPaymentOrder();
    Check(type == POINT_GOLD && amount < 0, "unexpected currency operation");
    ++payments;
    if (!rejectGoldPayment)
        g_registry.get<TestPlayer>(e).gold += amount;
}
void ecs::ItemNetworkSystem::SendItemUpdate(entt::registry&, entt::entity item)
{
    if (!transferTest || item == watchedItem) ++updates;
    else publishedCounts.push_back(item);
    if (onUpdate) onUpdate(item);
}

namespace ItemSystem {
bool IsValidItem(entt::entity item)
{
    return item != entt::null && g_registry.valid(item) && g_registry.all_of<ecs::ItemIdentity>(item);
}
const TItemTable* GetItemProto(entt::entity item)
{
    const auto* ref = g_registry.try_get<ecs::ItemProtoRef>(item);
    return ref ? ref->proto : nullptr;
}
uint8_t GetItemType(entt::entity item) { const auto* p = GetItemProto(item); return p ? p->bType : 0; }
uint8_t GetItemSubType(entt::entity item) { const auto* p = GetItemProto(item); return p ? p->bSubType : 0; }
bool IsDragonSoulItem(entt::entity item) { return IsValidItem(item) && GetItemType(item) == ITEM_DS; }
bool IsRuneItem(entt::entity item) {
    return IsValidItem(item) && GetItemType(item) == ITEM_COSTUME &&
        GetItemSubType(item) >= RUNE_SLOT1 && GetItemSubType(item) <= RUNE_SLOT7;
}
bool IsExtraItem(entt::entity item) { ++stackCategoryLookups; return IsValidItem(item) && g_registry.all_of<TestExtraStack>(item); }
uint32_t GetItemWearFlags(entt::entity item) { const auto* p = GetItemProto(item); return p ? p->dwWearFlags : 0; }
int32_t GetItemValue(entt::entity item, uint32_t index)
{
    const auto* proto = GetItemProto(item);
    return proto && index < ITEM_VALUES_MAX_NUM ? proto->alValues[index] : 0;
}
uint32_t GetItemVnum(entt::entity item) { return g_registry.get<ecs::ItemIdentity>(item).vnum; }
uint32_t GetItemOriginalVnum(entt::entity item) { return GetItemVnum(item); }
uint32_t GetItemID(entt::entity item) { return g_registry.get<ecs::ItemIdentity>(item).id; }
entt::entity GetItemOwner(entt::entity item)
{
    const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
    return owner ? owner->owner : entt::entity{entt::null};
}
entt::entity GetItem(entt::entity owner, TItemPos pos)
{
    const auto found = inventory.find({owner, pos.window_type, pos.cell});
    return found != inventory.end() ? found->second : entt::entity{entt::null};
}
entt::entity GetInventoryItem(entt::entity owner, uint16_t cell) { return GetItem(owner, TItemPos(INVENTORY, cell)); }
uint8_t GetItemWindow(entt::entity item) { return g_registry.get<ecs::ItemLocation>(item).window; }
uint16_t GetItemCell(entt::entity item) { return g_registry.get<ecs::ItemLocation>(item).cell; }
int GetItemAttributeCount(entt::entity item)
{
    return ecs::item_attributes::Count(g_registry.get<ecs::ItemAttributes>(item).attrs, 0, ITEM_ATTRIBUTE_NORM_NUM);
}
bool IsItemEquipped(entt::entity item)
{
    const auto* equipped = g_registry.try_get<ecs::ItemEquipped>(item);
    return equipped && equipped->equipped;
}
bool IsItemExchanging(entt::entity item)
{
    const auto* flags = g_registry.try_get<ecs::ItemFlags>(item);
    return flags && flags->exchanging;
}
bool IsItemLocked(entt::entity item)
{
    const auto* flags = g_registry.try_get<ecs::ItemFlags>(item);
    return IsItemConsumptionPending(item) || (flags && flags->isLocked);
}
bool ConsumeItemEcs(entt::entity item, uint32_t amount)
{
    Check(!transferTest && !extractionTest, "batch fell back to sequential consumption");
    CheckPaymentOrder();
    ++payments;
    if (rejectPayment || rejectPaymentAt == payments)
        return false;
    if (onPayment) onPayment();
    Check(IsValidItem(item) && amount > 0 && GetItemCount(item) >= amount, "invalid material debit");
    if (GetItemCount(item) == amount) {
        inventory.erase({GetItemOwner(item), GetItemWindow(item), GetItemCell(item)});
        g_registry.destroy(item);
    } else {
        g_registry.get<ecs::ItemCount>(item).count -= static_cast<int>(amount);
    }
    return true;
}
uint32_t GetItemSocket(entt::entity item, int index)
{
    const auto* sockets = g_registry.try_get<ecs::ItemSockets>(item);
    return sockets && index >= 0 && index < ITEM_SOCKET_MAX_NUM ? static_cast<uint32_t>(sockets->sockets[index]) : 0;
}
TItemExtraProto* GetItemExtraProto(entt::entity item)
{
    const auto* ref = g_registry.try_get<ecs::ItemExtraProtoRef>(item);
    return ref ? ref->proto : nullptr;
}
void SaveItem(entt::entity item)
{
    if (!transferTest || item == watchedItem) ++saves;
    if (onSave) onSave(item);
}
TPlayerItemAttribute GetItemAttribute(entt::entity item, int index)
{
    return g_registry.get<ecs::ItemAttributes>(item).attrs[index];
}
void SetItemAttributes(entt::entity item, const TPlayerItemAttribute* attrs)
{
    auto& component = g_registry.get<ecs::ItemAttributes>(item);
    std::copy_n(attrs, ITEM_ATTRIBUTE_MAX_NUM, component.attrs.begin());
    ++saves;
}
}

namespace {
struct Fixture {
    TItemTable proto{};
    entt::entity item;
    Fixture()
    {
        g_registry.clear();
        g_map_itemAttr.clear();
        g_map_itemRare.clear();
        saves = updates = randomCalls = 0;
        randomOffset = 0;
        floatDrawFraction = 0.f;
        floatRandomCalls = 0;
        payments = 0;
        rejectPayment = rejectGoldPayment = false;
        transferTest = extractionTest = soulStateTest = countStateTest = false;
        g_bItemCountLimit = 200;
        stackCategoryLookups = 0;
        onSoulPoints = {}; onSoulStart = onSoulStop = {};
        onCreate = onSocket = onRemove = onPlace = onLog = {};
        rejectPaymentAt = transferLogs = 0;
        transferCommands.clear();
        onPayment = {};
        onSave = onUpdate = onDestroy = {};
        publishedCounts.clear();
        destroyAttempts.clear();
        rejectDestruction.clear();
        ItemSystem::ProcessPendingItemConsumptions(); // discard retired handles from the previous fixture
        watchedItem = entt::null;
        inventory.clear();
        proto.bType = ITEM_WEAPON;
        proto.bSubType = WEAPON_SWORD;
        item = g_registry.create();
        g_registry.emplace<ecs::ItemIdentity>(item, ecs::ItemIdentity{1, 1000, 1000});
        g_registry.emplace<ecs::ItemAttributes>(item);
        auto& ref = g_registry.emplace<ecs::ItemProtoRef>(item);
        ref.proto = &proto;
        Check(!g_registry.any_of<ecs::LegacyItemPtr>(item), "fixture must be entity-only");
    }
    auto& Attrs() { return g_registry.get<ecs::ItemAttributes>(item).attrs; }
    void Rows(int count, bool rare = false)
    {
        for (int i = 1; i <= count; ++i) {
            TItemAttrTable row;
            row.dwApplyIndex = i;
            row.dwProb = 1;
            std::fill(std::begin(row.bMaxLevelBySet), std::end(row.bMaxLevelBySet), 5);
            for (size_t level = 0; level < std::size(row.lValues); ++level)
                row.lValues[level] = static_cast<int32_t>(10 + level);
            (rare ? g_map_itemRare : g_map_itemAttr).emplace(i, row);
        }
    }
};

struct PaidFixture : Fixture {
    entt::entity owner;
    entt::entity material;
    PaidFixture()
    {
        owner = g_registry.create();
        g_registry.emplace<TestPlayer>(owner);
        Place(item, SWITCHBOT, 0);
        material = Material(c_arSwitchingItems[0], 2, 0);
        Rows(5);
        Attrs()[0] = {20, 200};
        Attrs()[1] = {21, 300};
        Watch();
    }
    void Place(entt::entity e, uint8_t window, uint16_t cell)
    {
        std::erase_if(inventory, [e](const auto& entry) { return entry.second == e; });
        g_registry.emplace_or_replace<ecs::ItemOwner>(e, ecs::ItemOwner{owner});
        g_registry.emplace_or_replace<ecs::ItemLocation>(e, ecs::ItemLocation{window, cell});
        inventory[{owner, window, cell}] = e;
    }
    entt::entity Material(uint32_t vnum, int count, uint16_t cell)
    {
        const auto e = g_registry.create();
        g_registry.emplace<ecs::ItemIdentity>(e, ecs::ItemIdentity{uint32_t(100 + cell), vnum, vnum});
        g_registry.emplace<ecs::ItemCount>(e, ecs::ItemCount{count});
        Place(e, INVENTORY, cell);
        return e;
    }
    void Watch()
    {
        watchedItem = item;
        beforePayment = Attrs();
        lockBeforePayment = ItemSystem::GetItemLockedAttr(item);
        saves = updates = payments = 0;
    }
};

struct AttributeItemFixture : PaidFixture {
    TItemTable materialProto{};
    explicit AttributeItemFixture(uint8_t operation)
    {
        Place(item, INVENTORY, 5);
        materialProto.bType = ITEM_USE;
        materialProto.bSubType = operation;
        g_registry.emplace<ecs::ItemProtoRef>(material).proto = &materialProto;
        for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
            Attrs()[i] = {static_cast<uint8_t>(20 + i), static_cast<int16_t>(200 + i)};
        Check(!g_registry.any_of<ecs::LegacyCharPtr>(owner), "attribute item owner must be entity-only");
        Watch();
    }
    void Unchanged()
    {
        Check(EqualAttributes(Attrs(), beforePayment) && saves == 0 && updates == 0 &&
            ItemSystem::GetItemLockedAttr(item) == lockBeforePayment,
            "failed attribute item operation published changes");
        Check(ItemSystem::GetItemCount(material) == 2, "failed attribute item operation consumed material");
    }
};

// Each operation must enforce these guards itself, not rely on a legacy caller.
template <class Use>
void AttributeItemGuards(AttributeItemFixture& f, Use use)
{
    Check(!use(entt::null, f.item, f.material), "null owner accepted");
    Check(!use(f.owner, entt::null, f.material), "null target accepted");
    Check(!use(f.owner, f.item, entt::null), "null material accepted");
    Check(!use(f.owner, f.item, f.item), "target accepted as material");
    const auto stranger = g_registry.create();
    g_registry.emplace<TestPlayer>(stranger);
    Check(!use(stranger, f.item, f.material), "foreign owner accepted");
    inventory.erase({f.owner, INVENTORY, 5});
    Check(!use(f.owner, f.item, f.material), "detached target accepted");
    f.Place(f.item, INVENTORY, 5);
    for (const auto entity : {f.item, f.material}) {
        g_registry.emplace<ecs::ItemFlags>(entity).isLocked = true;
        Check(!use(f.owner, f.item, f.material), "locked target/material accepted");
        auto& flags = g_registry.get<ecs::ItemFlags>(entity);
        flags.isLocked = false;
        flags.exchanging = true;
        Check(!use(f.owner, f.item, f.material), "exchanging target/material accepted");
        flags.exchanging = false;
        g_registry.emplace<ecs::ItemEquipped>(entity).equipped = true;
        Check(!use(f.owner, f.item, f.material), "equipped target/material accepted");
        g_registry.get<ecs::ItemEquipped>(entity).equipped = false;
    }
    g_registry.get<ecs::ItemOwner>(f.material).owner = stranger;
    Check(!use(f.owner, f.item, f.material), "foreign material accepted");
    f.Place(f.material, INVENTORY, 0);
    inventory.erase({f.owner, INVENTORY, 0});
    Check(!use(f.owner, f.item, f.material), "detached material accepted");
    f.Place(f.material, INVENTORY, 0);
    for (const int count : {0, -1}) {
        g_registry.get<ecs::ItemCount>(f.material).count = count;
        Check(!use(f.owner, f.item, f.material), "empty/negative material stack accepted");
    }
    g_registry.get<ecs::ItemCount>(f.material).count = 2;
    f.materialProto.bType = ITEM_WEAPON;
    Check(!use(f.owner, f.item, f.material), "non-consumable material accepted");
    f.materialProto.bType = ITEM_USE;
    const auto operation = f.materialProto.bSubType;
    f.materialProto.bSubType = USE_POTION;
    Check(!use(f.owner, f.item, f.material), "unrelated consumable accepted");
    f.materialProto.bSubType = operation;
    Check(payments == 0 && randomCalls == 0, "invalid request attempted payment or RNG");
    f.Unchanged();
}

void EntityAndTableValidation()
{
    Fixture f;
    Check(!ItemSystem::AddItemAttributeEcs(entt::null), "null entity accepted");
    const auto foreign = g_registry.create();
    Check(!ItemSystem::AddItemRareAttributeEcs(foreign), "non-item entity accepted");
    f.proto.bSubType = WEAPON_ARROW;
    f.Rows(2, true);
    Check(!ItemSystem::AddItemRareAttributeEcs(f.item), "arrow must not index attribute set -1");
    Check(randomCalls == 0 && saves == 0 && updates == 0, "invalid item caused side effects");
    f.proto.bSubType = WEAPON_SWORD;
    Check(!ItemSystem::AddItemAttributeEcs(f.item), "empty table accepted");
    Check(g_map_itemAttr.empty(), "read inserted rows into the attribute table");
    f.Rows(1);
    g_map_itemAttr.begin()->second.dwProb = 0;
    Check(!ItemSystem::AddItemAttributeEcs(f.item), "zero-weight table accepted");
    Check(saves == 0 && updates == 0, "failed roll saved item");
    g_registry.destroy(f.item);
    Check(!ItemSystem::ChangeItemAttributeEcs(f.item), "stale entity accepted");
}

void LockedSlotAndRarePreservation()
{
    Fixture f;
    f.Rows(10);
    for (int i = 0; i < 5; ++i)
        f.Attrs()[i] = {static_cast<uint8_t>(i + 1), static_cast<int16_t>(100 + i)};
    f.Attrs()[5] = {40, 70};
    f.Attrs()[6] = {41, 80};
    g_registry.emplace<ecs::ItemLockedAttribute>(f.item, ecs::ItemLockedAttribute{2});
    Check(ItemSystem::ChangeItemAttributeEcs(f.item), "locked normal reroll failed");
    Check(f.Attrs()[2].bType == 3 && f.Attrs()[2].sValue == 102, "locked slot changed");
    Check(ecs::item_attributes::Count(f.Attrs(), 0, 5) == 5, "reroll lost a normal bonus");
    Check(f.Attrs()[5].bType == 40 && f.Attrs()[6].bType == 41, "normal reroll touched rare slots");
    Check(saves == 1 && updates == 1, "reroll must publish once after success");
    Check(!ItemSystem::AddItemAttributeEcs(f.item), "full attributes accepted another bonus");
    Check(ItemSystem::ClearNormalItemAttributes(f.item), "clear normal failed");
    Check(f.Attrs()[2].sValue == 102 && f.Attrs()[5].sValue == 70, "clear destroyed protected bonus");
}

void FailureIsAtomic()
{
    Fixture f;
    f.Rows(1);
    f.Attrs()[0] = {20, 100};
    f.Attrs()[1] = {21, 200};
    const auto before = f.Attrs();
    Check(!ItemSystem::ChangeItemAttributeEcs(f.item), "insufficient distinct bonuses accepted");
    Check(f.Attrs()[0].bType == before[0].bType && f.Attrs()[1].sValue == before[1].sValue,
        "failed reroll changed original attributes");
    Check(saves == 0 && updates == 0, "failed reroll published partial state");
    const int badProbabilities[5] = {0, 0, 0, 0, 0};
    Check(!ItemSystem::ChangeItemAttributeEcs(f.item, badProbabilities), "invalid level probabilities accepted");
}

void RareHolesAndFailure()
{
    Fixture f;
    f.Rows(3, true);
    f.Attrs()[6] = {3, 333};
    Check(ItemSystem::AddItemRareAttributeEcs(f.item), "rare gap could not be filled");
    Check(f.Attrs()[5].bType == 1 && f.Attrs()[6].sValue == 333, "rare add overwrote occupied slot");
    Check(!ItemSystem::AddItemRareAttributeEcs(f.item), "rare slots overflowed");
    const auto before = f.Attrs();
    g_map_itemRare.erase(2);
    g_map_itemRare.erase(3);
    saves = updates = 0;
    Check(!ItemSystem::ChangeItemRareAttributeEcs(f.item), "rare reroll accepted too few types");
    Check(f.Attrs()[5].bType == before[5].bType && f.Attrs()[6].sValue == before[6].sValue,
        "failed rare reroll lost bonuses");
    Check(saves == 0 && updates == 0, "failed rare reroll published state");
}

void AddonRemovalAndDuplicates()
{
    Fixture f;
    f.Attrs()[0] = {APPLY_NORMAL_HIT_DAMAGE_BONUS, 20};
    f.Attrs()[1] = {APPLY_SKILL_DAMAGE_BONUS, -5};
    f.Attrs()[2] = {APPLY_MAX_HP, 500};
    f.Attrs()[5] = {40, 444};
    Check(ItemSystem::RemoveItemAttributeType(f.item, APPLY_SKILL_DAMAGE_BONUS), "addon removal failed");
    Check(f.Attrs()[1].bType == APPLY_MAX_HP && f.Attrs()[5].sValue == 444, "addon removal moved wrong slots");
    Check(ItemSystem::ApplyItemAddon(f.item, -1), "entity-only addon failed");
    Check(ecs::item_attributes::Count(f.Attrs(), 0, 5) == 3, "addon did not replace bonus pair");
    Check(!ItemSystem::AddItemAttribute(f.item, APPLY_MAX_HP, 1000), "duplicate normal bonus accepted");
    Check(f.Attrs()[5].sValue == 444, "addon touched rare attribute");
}

void WeightedBoundaries()
{
    Fixture f;
    f.Rows(2);
    g_map_itemAttr.at(1).dwProb = 2;
    g_map_itemAttr.at(2).dwProb = 3;
    auto eligible = [](auto, const auto&) { return true; };
    for (int roll = 1; roll <= 5; ++roll) {
        const auto found = ecs::item_attributes::Select(g_map_itemAttr, ATTRIBUTE_SET_WEAPON, true,
            eligible, [=](int, int) { return roll; });
        Check(found != g_map_itemAttr.end() && found->first == (roll <= 2 ? 1 : 2), "weighted boundary changed");
    }
    auto noRandom = [](int, int) { throw std::runtime_error("invalid table reached RNG"); return 0; };
    const int partialProbabilities[5] = {0, 0, 30, 40, 3};
    Check(ecs::item_attributes::RollLevel(partialProbabilities, 5, [](int, int) { return 30; }) == 3,
        "partial probability table rejected a covered roll");
    Check(ecs::item_attributes::RollLevel(partialProbabilities, 5, [](int, int) { return 73; }) == 5,
        "partial probability table last boundary changed");
    Check(ecs::item_attributes::RollLevel(partialProbabilities, 5, [](int, int) { return 74; }) == 0,
        "unassigned roll was silently renormalized");
    Check(ecs::item_attributes::Select(g_map_itemAttr, -1, true, eligible, noRandom) == g_map_itemAttr.end(),
        "negative attribute set accepted");
    Check(ecs::item_attributes::Select(g_map_itemAttr, ATTRIBUTE_SET_MAX_NUM, true, eligible, noRandom) == g_map_itemAttr.end(),
        "attribute set overflow accepted");
    g_map_itemAttr.at(1).dwProb = UINT32_MAX;
    Check(ecs::item_attributes::Select(g_map_itemAttr, ATTRIBUTE_SET_WEAPON, true, eligible, noRandom) == g_map_itemAttr.end(),
        "probability sum overflow accepted");
}

void ProtoAndCostumeRules()
{
    Fixture f;
    f.Rows(3);
    f.proto.aApplies[0].bType = 1;
    Check(ItemSystem::AddItemAttributeEcs(f.item), "base apply exclusion blocked all bonuses");
    Check(f.Attrs()[0].bType == 2, "normal roll duplicated a base apply");
    f.Attrs().fill({});
    g_registry.get<ecs::ItemIdentity>(f.item).vnum = 300;
    Check(ItemSystem::HasItemAttribute(f.item, 1), "base-apply query lost the Zodiac base bonus");
    Check(ItemSystem::AddItemAttributeEcs(f.item), "zodiac roll failed");
#ifndef DISABLE_ZODIAC_ATT
    Check(f.Attrs()[0].bType == 1, "zodiac base-apply exception lost");
#endif
#ifdef ENABLE_ATTR_COSTUMES
    f.Attrs().fill({});
    f.proto.aApplies[0] = {};
    f.proto.bType = ITEM_COSTUME;
    f.proto.bSubType = COSTUME_BODY;
    Check(ItemSystem::AddItemAttributeEcs(f.item), "costume roll failed");
    Check(f.Attrs()[0].sValue == 15, "costume used non-costume level values");
    f.Attrs().fill({});
    Check(ItemSystem::AlterItemToMagicItem(f.item), "entity-only magic generation failed");
    Check(ecs::item_attributes::Count(f.Attrs(), 0, 5) == 3 && f.Attrs()[0].sValue == 19,
        "costume magic count or high-level value changed");
#endif
    saves = updates = 0;
    Check(!ItemSystem::SetItemForceAttributeEcs(f.item, -1, 1, 10), "negative force-attribute index accepted");
    Check(!ItemSystem::SetItemForceAttributeEcs(f.item, ITEM_ATTRIBUTE_MAX_NUM, 1, 10), "force-attribute index overflow");
    Check(saves == 0 && updates == 0, "invalid force write caused side effects");
    Check(ItemSystem::SetItemForceAttributeEcs(f.item, 6, 2, -30), "signed force attribute failed");
    Check(f.Attrs()[6].sValue == -30 && saves == 1 && updates == 1, "force write did not update client and save");
}

void PaidRerollTransactions()
{
    PaidFixture f;
    g_map_itemAttr.erase(2);
    g_map_itemAttr.erase(3);
    g_map_itemAttr.erase(4);
    g_map_itemAttr.erase(5);
    Check(!ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "incomplete paid roll accepted");
    Check(payments == 0 && ItemSystem::GetItemCount(f.material) == 2 && EqualAttributes(f.Attrs(), beforePayment),
        "failed preparation consumed material or changed bonuses");
    f.Rows(5);
    rejectPayment = true;
    Check(!ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "failed payment accepted");
    Check(payments == 1 && saves == 0 && updates == 0 && EqualAttributes(f.Attrs(), beforePayment),
        "failed payment published a reroll");
    rejectPayment = false;
    f.Watch();
    Check(ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "paid reroll failed");
    Check(payments == 1 && ItemSystem::GetItemCount(f.material) == 1 && saves == 1 && updates == 1,
        "paid reroll did not charge and publish exactly once");
    f.Watch();
    Check(ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "last material failed");
    Check(!g_registry.valid(f.material) && g_registry.valid(f.item) && saves == 1 && updates == 1,
        "last-unit consumption invalidated target or missed commit");
    f.Watch();
    Check(!ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material) && payments == 0,
        "stale material was reused");
}

void PaymentValidation()
{
    PaidFixture f;
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.item), "target accepted as material");
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.material, 0), "zero cost accepted");
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.material, 3), "material stack underflow accepted");
    g_registry.get<ecs::ItemCount>(f.material).count = -1;
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.material), "negative stack wrapped into an affordable count");
    g_registry.get<ecs::ItemCount>(f.material).count = 2;
    auto& flags = g_registry.emplace<ecs::ItemFlags>(f.material);
    flags.exchanging = true;
    Check(!ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "exchanging material consumed");
    flags.exchanging = false;
    flags.isLocked = true;
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.material), "locked material accepted");
    flags.isLocked = false;
    auto& equipped = g_registry.emplace<ecs::ItemEquipped>(f.item);
    equipped.equipped = true;
    Check(!ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "equipped target changed");
    equipped.equipped = false;
    const auto stranger = g_registry.create();
    g_registry.get<ecs::ItemOwner>(f.material).owner = stranger;
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.material), "foreign material accepted");
    g_registry.get<ecs::ItemOwner>(f.material).owner = f.owner;
    inventory.erase({f.owner, INVENTORY, 0});
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.material), "detached material accepted");
    Check(payments == 0 && saves == 0 && updates == 0, "invalid payment caused side effects");
    f.Place(f.material, INVENTORY, 0);
    g_registry.destroy(f.owner);
    Check(!ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "stale owner accepted");
}

void GoldTransactions()
{
    PaidFixture f;
    Check(!ItemSystem::ChangeItemAttributeWithGoldCost(f.item, 101), "insufficient yang accepted");
    Check(!ItemSystem::ChangeItemAttributeWithGoldCost(f.item, 0), "free yang reroll accepted");
    Check(!ItemSystem::ChangeItemAttributeWithGoldCost(f.item, -1), "negative price accepted");
    g_map_itemAttr.clear();
    Check(!ItemSystem::ChangeItemAttributeWithGoldCost(f.item, 10), "invalid roll charged yang");
    Check(payments == 0 && ecs::PointSystem::GetGold(f.owner) == 100, "failed preparation changed wallet");
    f.Rows(5);
    rejectGoldPayment = true;
    Check(!ItemSystem::ChangeItemAttributeWithGoldCost(f.item, 10), "rejected wallet debit accepted");
    Check(saves == 0 && updates == 0 && EqualAttributes(f.Attrs(), beforePayment), "rejected debit changed attributes");
    rejectGoldPayment = false;
    f.Watch();
    Check(ItemSystem::ChangeItemAttributeWithGoldCost(f.item, 100), "exact yang payment failed");
    Check(ecs::PointSystem::GetGold(f.owner) == 0 && payments == 1 && saves == 1 && updates == 1,
        "yang debit and commit were not applied exactly once");
}

void CostumeResetTransactions()
{
#ifdef ENABLE_ATTR_COSTUMES
    PaidFixture f;
    f.proto.bType = ITEM_COSTUME;
    f.proto.bSubType = COSTUME_BODY;
    f.Attrs()[5] = {40, 400};
    f.Attrs()[6] = {41, 500};
    f.Watch();
    g_map_itemAttr.clear();
    f.Rows(2);
    Check(!ItemSystem::ResetCostumeAttributesWithItemCost(f.item, f.material), "partial costume reset accepted");
    Check(EqualAttributes(f.Attrs(), beforePayment) && saves == 0 && updates == 0 && payments == 0,
        "failed reset cleared original bonuses or consumed reset item");
    f.Rows(5);
    rejectPayment = true;
    Check(!ItemSystem::ResetCostumeAttributesWithItemCost(f.item, f.material), "unpaid costume reset accepted");
    Check(EqualAttributes(f.Attrs(), beforePayment) && saves == 0 && updates == 0,
        "rejected reset payment changed attributes");
    rejectPayment = false;
    f.Watch();
    g_registry.get<ecs::ItemCount>(f.material).count = 1;
    Check(ItemSystem::ResetCostumeAttributesWithItemCost(f.item, f.material), "costume reset failed");
    Check(ItemSystem::GetItemAttributeCount(f.item) == 3 && f.Attrs()[0].sValue == 19 && f.Attrs()[1].sValue == 15,
        "costume reset changed three-bonus level rules");
    Check(f.Attrs()[5].sValue == 400 && f.Attrs()[6].sValue == 500 && !g_registry.valid(f.material),
        "costume reset damaged rare bonuses or failed to consume last material");
    Check(saves == 1 && updates == 1 && payments == 1, "costume reset published intermediate empty attributes");
    f.material = f.Material(39028, 2, 0);
    f.Attrs()[2] = {30, 777};
    g_registry.emplace<ecs::ItemLockedAttribute>(f.item, ecs::ItemLockedAttribute{2});
    f.Watch();
    Check(ItemSystem::ResetCostumeAttributesWithItemCost(f.item, f.material), "locked costume reset failed");
    Check(f.Attrs()[2].bType == 30 && f.Attrs()[2].sValue == 777, "reset lost locked slot");
    f.proto.bType = ITEM_WEAPON;
    f.Watch();
    Check(!ItemSystem::ResetCostumeAttributesWithItemCost(f.item, f.material) && payments == 0,
        "costume reset accepted weapon");
#endif
}

#ifdef ENABLE_ATTR_COSTUMES
struct CostumeFixture : PaidFixture {
    TItemTable materialProto{};
    explicit CostumeFixture(uint8_t operation)
    {
        proto.bType = ITEM_COSTUME;
        proto.bSubType = COSTUME_BODY;
        Place(item, INVENTORY, 5);
        materialProto.bType = ITEM_USE;
        materialProto.bSubType = operation;
        g_registry.emplace<ecs::ItemProtoRef>(material).proto = &materialProto;
        g_registry.emplace<ecs::ItemSockets>(material).sockets = {40, 100};
        Check(!g_registry.any_of<ecs::LegacyCharPtr>(owner), "costume owner must be entity-only");
        Watch();
    }
    auto Use() { return ItemSystem::UseCostumeAttributeItem(owner, item, material); }
    void Unchanged()
    {
        Check(EqualAttributes(Attrs(), beforePayment) && saves == 0 && updates == 0,
            "rejected costume operation published attributes");
        Check(ItemSystem::GetItemCount(material) == 2, "rejected costume operation consumed material");
    }
};
#endif

void CostumeSelectionValidation()
{
#ifdef ENABLE_ATTR_COSTUMES
    using Result = ItemSystem::CostumeAttributeResult;
    CostumeFixture f(USE_REMOVE_ATTR_COSTUME);
    f.Attrs()[5] = {40, 400};
    f.Attrs()[6] = {41, 500};
    f.Watch();
    Check(!ItemSystem::SelectCostumeAttributeToRemove(entt::null, "0"), "null dialog owner accepted");
    Check(!ItemSystem::SelectCostumeAttributeToRemove(f.item, "0"), "non-player dialog owner accepted");
    for (const auto input : {"", "-1", "-5", "-6", "2", "garbage", "1suffix", "01", "+1", " 1", "999999999999999999999"}) {
        Check(ItemSystem::SelectCostumeAttributeToRemove(f.owner, "1"), "valid dialog selection failed");
        Check(!ItemSystem::SelectCostumeAttributeToRemove(f.owner, input), "malformed selection accepted");
        Check(f.Use() == Result::InvalidSelection, "invalid command reused previous selection");
        f.Unchanged();
    }
    // Defend the operation as well as the command boundary against bad state.
    for (const int index : {INT_MIN, -6, -5, -1, 2, INT_MAX}) {
        g_registry.get<ecs::CostumeAttributeSelection>(f.owner).rareSlot = index;
        Check(f.Use() == Result::InvalidSelection, "out-of-range rare index accepted");
        f.Unchanged();
    }
    Check(payments == 0, "invalid selection attempted payment");
    g_registry.destroy(f.owner);
    Check(!ItemSystem::SelectCostumeAttributeToRemove(f.owner, "0"), "stale dialog owner accepted");
    Check(f.Use() == Result::InvalidTarget, "stale owner used costume item");
#endif
}

void CostumeAdditionTransactions()
{
#ifdef ENABLE_ATTR_COSTUMES
    using Result = ItemSystem::CostumeAttributeResult;
    for (const uint8_t subtype : {COSTUME_BODY, COSTUME_HAIR, COSTUME_WEAPON})
    for (const uint8_t operation : {USE_ADD_ATTR_COSTUME1, USE_ADD_ATTR_COSTUME2}) {
        CostumeFixture f(operation);
        f.proto.bSubType = subtype;
        g_registry.get<ecs::ItemSockets>(f.material).sockets[1] = -123;
        g_registry.get<ecs::ItemCount>(f.material).count = 1;
        Check(f.Use() == Result::Success, "costume addition failed");
        Check(f.Attrs()[5].bType == 40 && f.Attrs()[5].sValue == -123 && f.Attrs()[6].bType == 0,
            "costume addition changed signed payload or rare slot order");
        Check(f.Attrs()[0].sValue == 200 && f.Attrs()[1].sValue == 300 && !g_registry.valid(f.material),
            "costume addition damaged normal bonuses or missed last-unit debit");
        Check(saves == 1 && updates == 1 && payments == 1 && randomCalls == 0,
            "explicit costume addition was not a single deterministic transaction");
        f.Watch();
        Check(f.Use() == Result::InvalidMaterial && payments == 0, "stale costume material reused");
    }
    CostumeFixture f(USE_ADD_ATTR_COSTUME1);
    f.Attrs()[6] = {40, 500};
    f.Watch();
    Check(f.Use() == Result::DuplicateAttribute, "duplicate rare bonus accepted across a gap");
    f.Unchanged();
    auto& sockets = g_registry.get<ecs::ItemSockets>(f.material).sockets;
    for (const int32_t type : {0, -1, 256, static_cast<int>(MAX_APPLY_NUM), INT32_MAX}) {
        sockets[0] = type;
        Check(f.Use() == Result::InvalidMaterial, "invalid costume apply narrowed to a byte");
        f.Unchanged();
    }
    sockets[0] = 20; // A normal-slot duplicate is allowed by the existing costume rules.
    for (const int32_t value : {0, INT16_MIN - 1, INT16_MAX + 1, INT32_MIN, INT32_MAX}) {
        sockets[1] = value;
        Check(f.Use() == Result::InvalidMaterial, "invalid costume value narrowed to a short");
        f.Unchanged();
    }
    sockets[1] = INT16_MIN;
    rejectPayment = true;
    Check(f.Use() == Result::Failed, "unpaid costume addition accepted");
    f.Unchanged();
    rejectPayment = false;
    f.Watch();
    Check(f.Use() == Result::Success, "costume rare-gap addition failed");
    Check(f.Attrs()[5].bType == 20 && f.Attrs()[5].sValue == INT16_MIN && f.Attrs()[6].sValue == 500,
        "rare-gap addition lost other bonuses");
    f.Watch();
    Check(f.Use() == Result::SlotsFull && payments == 0 && saves == 0, "full rare slots accepted addition");
    f.Attrs()[5] = {};
    f.Watch();
    g_registry.remove<ecs::ItemSockets>(f.material);
    Check(f.Use() == Result::InvalidMaterial && payments == 0, "missing material sockets accepted");
#endif
}

void CostumeRemovalTransactions()
{
#ifdef ENABLE_ATTR_COSTUMES
    using Result = ItemSystem::CostumeAttributeResult;
    for (const int selected : {0, 1}) {
        CostumeFixture f(USE_REMOVE_ATTR_COSTUME);
        f.Attrs()[5] = {40, 400};
        f.Attrs()[6] = {41, 500};
        f.Watch();
        // No component retains the legacy default of slot zero.
        if (selected == 1)
            Check(ItemSystem::SelectCostumeAttributeToRemove(f.owner, "1"), "select second rare slot failed");
        rejectPayment = true;
        Check(f.Use() == Result::Failed, "unpaid costume removal accepted");
        f.Unchanged();
        rejectPayment = false;
        f.Watch();
        g_registry.get<ecs::ItemCount>(f.material).count = 1;
        Check(f.Use() == Result::Success, "costume removal failed");
        Check(f.Attrs()[5].bType == (selected == 0 ? 41 : 40) &&
            f.Attrs()[5].sValue == (selected == 0 ? 500 : 400) && f.Attrs()[6].bType == 0,
            "costume removal compacted wrong slots");
        Check(f.Attrs()[0].sValue == 200 && f.Attrs()[1].sValue == 300,
            "costume removal changed normal bonuses");
        Check(!g_registry.valid(f.material) && saves == 1 && updates == 1 && payments == 1,
            "costume removal published intermediate state or missed last-unit debit");
    }
    CostumeFixture f(USE_REMOVE_ATTR_COSTUME);
    Check(f.Use() == Result::NoRareAttributes, "empty costume rare slots consumed remover");
    f.Attrs()[6] = {41, 500};
    f.Watch();
    Check(f.Use() == Result::NoRareAttributes, "empty selected slot consumed remover");
    f.Unchanged();
    Check(ItemSystem::SelectCostumeAttributeToRemove(f.owner, "1"), "select rare slot after gap failed");
    Check(f.Use() == Result::Success && f.Attrs()[6].bType == 0, "rare-slot gap blocked valid removal");
#endif
}

void CostumeOperationValidation()
{
#ifdef ENABLE_ATTR_COSTUMES
    using Result = ItemSystem::CostumeAttributeResult;
    CostumeFixture f(USE_CHANGE_ATTR_COSTUME);
    const auto stranger = g_registry.create();
    g_registry.emplace<TestPlayer>(stranger);
    Check(ItemSystem::UseCostumeAttributeItem(stranger, f.item, f.material) == Result::InvalidTarget,
        "costume operation accepted foreign owner");
    Check(ItemSystem::UseCostumeAttributeItem(f.owner, entt::null, f.material) == Result::InvalidTarget,
        "costume operation accepted null target");
    inventory.erase({f.owner, INVENTORY, 5});
    Check(f.Use() == Result::InvalidTarget, "costume operation accepted detached target");
    f.Place(f.item, INVENTORY, 5);
    auto& flags = g_registry.emplace<ecs::ItemFlags>(f.item);
    flags.isLocked = true;
    Check(f.Use() == Result::InvalidTarget, "costume operation accepted locked target");
    flags.isLocked = false;
    flags.exchanging = true;
    Check(f.Use() == Result::InvalidTarget, "costume operation accepted exchanging target");
    flags.exchanging = false;
    g_registry.emplace<ecs::ItemEquipped>(f.item).equipped = true;
    Check(f.Use() == Result::InvalidTarget, "costume operation accepted equipped target");
    g_registry.get<ecs::ItemEquipped>(f.item).equipped = false;
    f.proto.bType = ITEM_WEAPON;
    Check(f.Use() == Result::InvalidTarget, "costume operation accepted weapon");
    f.proto.bType = ITEM_COSTUME;
    f.proto.bSubType = COSTUME_ACCE;
    Check(f.Use() == Result::InvalidTarget, "costume operation accepted sash");
    f.proto.bSubType = COSTUME_BODY;
    f.materialProto.bType = ITEM_WEAPON;
    Check(f.Use() == Result::InvalidMaterial, "non-consumable costume material accepted");
    f.materialProto.bType = ITEM_USE;
    f.materialProto.bSubType = USE_POTION;
    Check(f.Use() == Result::InvalidMaterial, "unrelated consumable accepted");
    f.materialProto.bSubType = USE_CHANGE_ATTR_COSTUME;
    g_registry.get<ecs::ItemOwner>(f.material).owner = stranger;
    Check(f.Use() == Result::InvalidMaterial, "foreign costume material accepted");
    g_registry.get<ecs::ItemOwner>(f.material).owner = f.owner;
    inventory.erase({f.owner, INVENTORY, 0});
    Check(f.Use() == Result::InvalidMaterial, "detached costume material accepted");
    f.Place(f.material, INVENTORY, 0);
    g_registry.emplace<ecs::ItemFlags>(f.material).isLocked = true;
    Check(f.Use() == Result::InvalidMaterial, "locked costume material accepted");
    g_registry.get<ecs::ItemFlags>(f.material).isLocked = false;
    Check(payments == 0, "invalid costume request attempted payment");
    f.Unchanged();
    f.Attrs()[0] = f.Attrs()[1] = {};
    f.Watch();
    Check(f.Use() == Result::NoAttributes && payments == 0, "empty costume rerolled");
    f.Attrs()[0] = {20, 200};
    f.Attrs()[1] = {21, 300};
    f.Attrs()[5] = {40, 400};
    f.Attrs()[6] = {41, 500};
    f.Watch();
    g_map_itemAttr.clear();
    Check(f.Use() == Result::Failed && payments == 0, "empty table charged costume changer");
    f.Unchanged();
    f.Rows(5);
    rejectPayment = true;
    Check(f.Use() == Result::Failed, "unpaid costume reroll accepted");
    f.Unchanged();
    rejectPayment = false;
    f.Watch();
    Check(f.Use() == Result::Success && saves == 1 && updates == 1 && payments == 1, "costume reroll transaction failed");
    Check(f.Attrs()[5].sValue == 400 && f.Attrs()[6].sValue == 500 && ItemSystem::GetItemAttributeCount(f.item) == 2,
        "costume changer altered rare bonuses or normal count");
    f.Watch();
    Check(f.Use() == Result::Success && !g_registry.valid(f.material) && saves == 1 && updates == 1,
        "costume changer failed last-unit debit");
    f.Watch();
    g_registry.destroy(f.item);
    Check(f.Use() == Result::InvalidTarget && payments == 0, "stale costume target accepted");
#endif
}

void StoleEnchantTransactions()
{
#ifdef ENABLE_STOLE_COSTUME
    // Check every variant at every supported grade, plus the legacy high-grade
    // cap without uint8_t wrapping. The seventh bonus must remain untouched.
    for (const int grade : {1, 2, 3, 4, 5, 256, INT32_MAX})
    for (int variant = 0; variant < MAX_VAR_ATTR; ++variant) {
        AttributeItemFixture f(USE_ENCHANT_STOLE);
        f.proto.bType = ITEM_COSTUME;
        f.proto.bSubType = COSTUME_STOLE;
        f.proto.alValues[0] = grade;
        randomOffset = variant;
        g_registry.get<ecs::ItemCount>(f.material).count = 1;
        Check(ItemSystem::EnchantStoleWithItemCost(f.owner, f.item, f.material), "stole enchant failed");
        const int column = (std::min(grade, 4) - 1) * MAX_VAR_ATTR + 1 + variant;
        for (int i = 0; i < MAX_ATTR; ++i)
            Check(f.Attrs()[i].bType == stoleInfoTable[i][0] && f.Attrs()[i].sValue == stoleInfoTable[i][column],
                "stole grade/variant table value changed");
        Check(f.Attrs()[6].bType == beforePayment[6].bType && f.Attrs()[6].sValue == beforePayment[6].sValue,
            "stole enchant overwrote seventh bonus");
        Check(randomCalls == MAX_ATTR && payments == 1 && saves == 1 && updates == 1 && !g_registry.valid(f.material),
            "stole enchant was not one six-bonus commit with last-unit debit");
        f.Watch();
        Check(!ItemSystem::EnchantStoleWithItemCost(f.owner, f.item, f.material) && payments == 0,
            "stale stole material reused");
    }
    AttributeItemFixture f(USE_ENCHANT_STOLE);
    f.proto.bType = ITEM_COSTUME;
    f.proto.bSubType = COSTUME_STOLE;
    f.proto.alValues[0] = 2;
    AttributeItemGuards(f, ItemSystem::EnchantStoleWithItemCost);
    for (const int grade : {0, -1, -256, INT32_MIN}) {
        f.proto.alValues[0] = grade;
        Check(!ItemSystem::EnchantStoleWithItemCost(f.owner, f.item, f.material), "invalid stole grade accepted");
        f.Unchanged();
    }
    Check(randomCalls == 0 && payments == 0, "invalid grade rolled bonuses or charged material");
    f.proto.alValues[0] = 2;
    f.proto.bSubType = COSTUME_BODY;
    Check(!ItemSystem::EnchantStoleWithItemCost(f.owner, f.item, f.material), "body costume accepted stole enchant");
    f.proto.bSubType = COSTUME_STOLE;
    rejectPayment = true;
    Check(!ItemSystem::EnchantStoleWithItemCost(f.owner, f.item, f.material), "unpaid stole enchant accepted");
    f.Unchanged();
    rejectPayment = false;
    f.Watch();
#ifdef ENABLE_EXTRA_INVENTORY
    f.Place(f.material, EXTRA_INVENTORY, 7);
#endif
    Check(ItemSystem::EnchantStoleWithItemCost(f.owner, f.item, f.material) &&
        ItemSystem::GetItemCount(f.material) == 1 && payments == 1 && saves == 1 && updates == 1,
        "stole enchant did not debit one unit of a stack");
    f.Watch();
    g_registry.destroy(f.item);
    Check(!ItemSystem::EnchantStoleWithItemCost(f.owner, f.item, f.material) && payments == 0,
        "stale stole target accepted");
#endif
}

void AttributeLockTransactions()
{
#ifdef ATTR_LOCK
    using Result = ItemSystem::AttributeLockResult;
    for (const uint8_t operation : {USE_ADD_ATTRIBUTE_LOCK, USE_CHANGE_ATTRIBUTE_LOCK, USE_DELETE_ATTRIBUTE_LOCK}) {
        AttributeItemFixture f(operation);
        if (operation != USE_ADD_ATTRIBUTE_LOCK)
            g_registry.emplace<ecs::ItemLockedAttribute>(f.item, ecs::ItemLockedAttribute{0});
        f.Watch();
        AttributeItemGuards(f, [](auto owner, auto item, auto material) {
            return ItemSystem::UseItemAttributeLock(owner, item, material) == Result::Success;
        });
        rejectPayment = true;
        Check(ItemSystem::UseItemAttributeLock(f.owner, f.item, f.material) == Result::Failed,
            "unpaid lock operation accepted");
        f.Unchanged();
        rejectPayment = false;
        f.Watch();
        g_registry.get<ecs::ItemCount>(f.material).count = 1;
        Check(ItemSystem::UseItemAttributeLock(f.owner, f.item, f.material) == Result::Success,
            "lock operation failed");
        const int expected = operation == USE_ADD_ATTRIBUTE_LOCK ? 0 : operation == USE_CHANGE_ATTRIBUTE_LOCK ? 1 : -1;
        Check(ItemSystem::GetItemLockedAttr(f.item) == expected && EqualAttributes(f.Attrs(), beforePayment),
            "lock operation changed bonuses or selected wrong slot");
        Check(payments == 1 && saves == 1 && updates == 1 && !g_registry.valid(f.material),
            "lock operation was not a single commit with last-unit debit");
        f.Watch();
        Check(ItemSystem::UseItemAttributeLock(f.owner, f.item, f.material) == Result::InvalidMaterial && payments == 0,
            "stale lock material reused");
    }
    // Enumerate every possible random choice. Moving a lock must never pick
    // its current slot, including when the lock is at either boundary.
    for (int current = -1; current < ITEM_ATTRIBUTE_NORM_NUM; ++current)
    for (int choice = 0; choice < ITEM_ATTRIBUTE_NORM_NUM - (current >= 0); ++choice) {
        AttributeItemFixture f(current < 0 ? USE_ADD_ATTRIBUTE_LOCK : USE_CHANGE_ATTRIBUTE_LOCK);
        g_registry.emplace<ecs::ItemLockedAttribute>(f.item, ecs::ItemLockedAttribute{static_cast<short>(current)});
        f.Watch();
        randomOffset = choice;
        Check(ItemSystem::UseItemAttributeLock(f.owner, f.item, f.material) == Result::Success,
            "bounded lock choice failed");
        const int expected = current < 0 || choice < current ? choice : choice + 1;
        Check(ItemSystem::GetItemLockedAttr(f.item) == expected && randomCalls == 1,
            "lock RNG retried or mapped the choice incorrectly");
    }
#endif
}

void AttributeLockEdgeCases()
{
#ifdef ATTR_LOCK
    using Result = ItemSystem::AttributeLockResult;
    AttributeItemFixture f(USE_ADD_ATTRIBUTE_LOCK);
    auto use = [&] { return ItemSystem::UseItemAttributeLock(f.owner, f.item, f.material); };
    f.Attrs()[2] = {};
    f.Watch();
    Check(use() == Result::NotEnoughAttributes, "lock added to fewer than five normal bonuses");
    f.Attrs()[2] = {22, 202};
    f.Watch();
    f.proto.dwWearFlags = WEARABLE_PENDANT | WEARABLE_BODY;
    Check(use() == Result::InvalidTarget, "combined pendant wear flags bypassed lock restriction");
    f.proto.dwWearFlags = 0;
    for (const uint8_t type : {ITEM_COSTUME, ITEM_DS}) {
        f.proto.bType = type;
        Check(use() == Result::InvalidTarget, "costume/dragon soul accepted attribute lock");
    }
    f.proto.bType = ITEM_WEAPON;
    g_registry.emplace<ecs::ItemLockedAttribute>(f.item, ecs::ItemLockedAttribute{2});
    f.Watch();
    Check(use() == Result::AlreadyLocked, "already-locked item accepted add-lock material");
    f.materialProto.bSubType = USE_CHANGE_ATTRIBUTE_LOCK;
    for (const short index : {short(-1), short(-2), short(5), short(6), short(INT16_MAX)}) {
        g_registry.get<ecs::ItemLockedAttribute>(f.item).index = index;
        f.Watch();
        Check(use() == (index == -1 ? Result::NotLocked : Result::InvalidLock), "invalid lock index changed");
        f.Unchanged();
    }
    g_registry.get<ecs::ItemLockedAttribute>(f.item).index = 2;
    f.Attrs()[2] = {};
    f.Watch();
    Check(use() == Result::InvalidLock, "lock on an empty bonus moved");
    f.Attrs()[2] = {22, 202};
    f.Attrs()[0] = f.Attrs()[1] = f.Attrs()[3] = f.Attrs()[4] = {};
    f.Watch();
    Check(use() == Result::NoAlternative, "one-bonus lock consumed a changer without changing slot");
    Check(payments == 0 && randomCalls == 0, "invalid lock state attempted payment or RNG");
    f.Unchanged();
    f.Attrs()[4] = {24, 204};
    f.Watch();
    Check(use() == Result::Success && ItemSystem::GetItemLockedAttr(f.item) == 4 && randomCalls == 1,
        "lock change did not skip empty slots");
    Check(EqualAttributes(f.Attrs(), beforePayment), "lock change modified normal/rare bonuses");

    // Deleting a malformed lock repairs it without indexing the attribute array.
    for (const short index : {short(-2), short(2), short(5), short(INT16_MAX)}) {
        AttributeItemFixture broken(USE_DELETE_ATTRIBUTE_LOCK);
        g_registry.emplace<ecs::ItemLockedAttribute>(broken.item, ecs::ItemLockedAttribute{index});
        broken.Watch();
        Check(ItemSystem::UseItemAttributeLock(broken.owner, broken.item, broken.material) == Result::Success &&
            ItemSystem::GetItemLockedAttr(broken.item) == -1 && randomCalls == 0,
            "remover failed to clear malformed lock");
        broken.Watch();
        Check(ItemSystem::UseItemAttributeLock(broken.owner, broken.item, broken.material) == Result::NotLocked && payments == 0,
            "remover charged an unlocked item");
    }
#endif
}

#ifdef ENABLE_DS_ENCHANT
struct DragonSoulFixture : AttributeItemFixture {
    DSManager manager;
    explicit DragonSoulFixture(bool load = true) : AttributeItemFixture(USE_DS_ENCHANT)
    {
        proto.bType = ITEM_DS;
        proto.bSubType = 0;
        g_registry.get<ecs::ItemIdentity>(item).vnum = 110000 + Grade() * 1000 + DRAGON_SOUL_STEP_HIGHEST * 100;
        dsReadOk = dsBasicOk = dsAdditionalOk = dsSettingsOk = dsWeightOk = true;
        dsBasicCount = 3; dsAddMin = dsAddMax = 2; dsWeight = 100.f;
        dsBasic = {{APPLY_MAX_HP, 100}, {APPLY_ATT_GRADE_BONUS, 20}, {APPLY_DEF_GRADE_BONUS, 30}};
        dsAdditional = {{APPLY_CRITICAL_PCT, 5, 0.f}, {APPLY_PENETRATE_PCT, 7, 1.f},
            {APPLY_ATTBONUS_MONSTER, 9, 3.f}, {APPLY_ATTBONUS_HUMAN, 11, 1.f}};
        if (load)
            Check(manager.ReadDragonSoulTableFile("headless-table"), "DS fixture table load failed");
        Watch();
    }
    static int Grade()
    {
#ifdef ENABLE_DS_GRADE_MYTH
        return DRAGON_SOUL_GRADE_MYTH;
#else
        return DRAGON_SOUL_GRADE_LEGENDARY;
#endif
    }
    auto Use() { return manager.EnchantWithItemCost(owner, item, material); }
};
#endif

void DragonSoulSampling()
{
    Fixture f;
    for (const float fraction : {0.f, 0.25f, 1.f}) {
        floatDrawFraction = fraction;
        std::vector<int> selected(2, -1);
        Check(MakeDistinctRandomNumberSet({0.f, 1.f, 0.f, 3.f}, selected), "DS weighted selection failed");
        Check(selected[0] != selected[1] &&
            ((selected[0] == 1 && selected[1] == 3) || (selected[0] == 3 && selected[1] == 1)),
            "DS selector picked a zero-weight/duplicate row");
    }
    for (const auto weights : {std::list<float>{}, {0.f, 0.f}, {-1.f, 3.f},
        {std::numeric_limits<float>::quiet_NaN(), 1.f}, {std::numeric_limits<float>::infinity(), 1.f},
        {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()}, {1.f, 0.f}}) {
        std::vector<int> selected(2, -7);
        Check(!MakeDistinctRandomNumberSet(weights, selected), "invalid DS probability table accepted");
        Check(selected == std::vector<int>(2, -7), "failed DS sampling published partial indices");
    }
    floatDrawFraction = std::numeric_limits<float>::quiet_NaN();
    std::vector<int> selected(1, -7);
    Check(!MakeDistinctRandomNumberSet({1.f}, selected) && selected[0] == -7, "NaN RNG output accepted");
}

void DragonSoulPreparation()
{
#ifdef ENABLE_DS_ENCHANT
    using Result = DSManager::EnchantResult;
    DragonSoulFixture f(false);
    Check(f.Use() == Result::Failed && !f.manager.PutAttributes(f.item), "DS operation accepted missing table");
    f.Unchanged();
    Check(dsLiveTables == 0, "unexpected DS table lifetime");
    Check(f.manager.ReadDragonSoulTableFile("headless-table") && dsLiveTables == 1, "DS table load failed");
    dsReadOk = false;
    Check(!f.manager.ReadDragonSoulTableFile("bad-table") && dsLiveTables == 1, "failed reload leaked/replaced table");
    Check(f.manager.PutAttributes(f.item) && saves == 1 && updates == 1, "failed reload lost usable table");
    dsReadOk = true;
    Check(f.manager.ReadDragonSoulTableFile("new-table") && dsLiveTables == 1, "successful reload leaked old table");
    f.Watch();
    auto reject = [&] {
        Check(f.Use() == Result::Failed, "invalid DS preparation succeeded");
        f.Unchanged();
        Check(payments == 0, "DS preparation failure attempted payment");
    };
    for (bool* available : {&dsBasicOk, &dsAdditionalOk, &dsSettingsOk, &dsWeightOk}) {
        *available = false; reject(); *available = true;
    }
    for (const int count : {-1, 4, INT_MAX}) { dsBasicCount = count; reject(); }
    dsBasicCount = 3;
    const auto savedBasic = dsBasic;
    dsBasic.pop_back(); reject(); dsBasic = savedBasic;
    for (const auto bounds : {std::pair{-1, 2}, std::pair{3, 2}, std::pair{0, 5}}) {
        dsAddMin = bounds.first; dsAddMax = bounds.second; reject();
    }
    dsAddMin = dsAddMax = 2;
    for (const float weight : {-1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max()}) { dsWeight = weight; reject(); }
    dsWeight = 100.f;
    dsBasic[0].apply_value = INT_MAX; reject(); dsBasic = savedBasic;
    dsBasic[0].apply_type = static_cast<EApplyTypes>(MAX_APPLY_NUM); reject(); dsBasic = savedBasic;
    const auto savedAdditional = dsAdditional;
    dsAdditional.clear(); reject(); dsAdditional = savedAdditional;
    for (auto& apply : dsAdditional) apply.prob = 0.f;
    reject(); dsAdditional = savedAdditional;
    dsAdditional[1].prob = -1.f; reject(); dsAdditional = savedAdditional;
    dsAdditional[1].apply_value = INT_MIN; reject(); dsAdditional = savedAdditional;
    auto& vnum = g_registry.get<ecs::ItemIdentity>(f.item).vnum;
    const auto oldVnum = vnum;
    vnum += DRAGON_SOUL_STRENGTH_MAX * 10; reject(); vnum = oldVnum;
    const auto attributes = g_registry.get<ecs::ItemAttributes>(f.item);
    g_registry.remove<ecs::ItemAttributes>(f.item);
    Check(f.Use() == Result::Failed && payments == 0, "DS item without attributes accepted");
    g_registry.emplace<ecs::ItemAttributes>(f.item, attributes);
    dsWeight = 12.5f;
    Check(f.manager.DragonSoulItemInitialize(f.item), "entity-only DS initialization failed");
    Check(f.Attrs()[0].sValue == 13 && f.Attrs()[1].sValue == 3 && f.Attrs()[3].sValue == 1,
        "DS scaled-value rounding changed");
    Check(saves == 1 && updates == 1 && payments == 0 && f.Attrs()[5].bType == 0 && f.Attrs()[6].bType == 0,
        "DS initialization published partial/stale bonus slots");
    f.Watch();
    const auto chosenType = f.Attrs()[3].bType;
    dsWeight = 200.f;
    Check(f.manager.RefreshItemAttributes(f.item), "DS attribute refresh failed");
    Check(f.Attrs()[0].sValue == 200 && f.Attrs()[3].bType == chosenType && f.Attrs()[3].sValue == 14,
        "DS refresh rerolled types or scaled incorrectly");
    Check(saves == 1 && updates == 1 && payments == 0, "DS refresh published intermediate attributes");
    f.Watch();
    dsAdditional.clear();
    Check(!f.manager.RefreshItemAttributes(f.item), "DS refresh accepted unknown stored bonus");
    f.Unchanged();
    dsAdditional = savedAdditional;
    f.Watch();
    g_registry.remove<ecs::ItemOwner>(f.item);
    Check(f.manager.DragonSoulItemInitialize(f.item) && saves == 1 && updates == 1 && payments == 0,
        "new ownerless DS initialization failed");
#endif
}

void DragonSoulTransactions()
{
#ifdef ENABLE_DS_ENCHANT
    using Result = DSManager::EnchantResult;
    Check(dsLiveTables == 0, "DS manager destruction leaked table");
    DragonSoulFixture f;
    AttributeItemGuards(f, [&](auto owner, auto item, auto material) {
        return f.manager.EnchantWithItemCost(owner, item, material) == Result::Success;
    });
    rejectPayment = true;
    Check(f.Use() == Result::Failed, "unpaid DS reroll accepted");
    f.Unchanged();
    rejectPayment = false;
    f.Watch();
    f.Place(f.item, DRAGON_SOUL_INVENTORY, 300);
#ifdef ENABLE_EXTRA_INVENTORY
    f.Place(f.material, EXTRA_INVENTORY, 7);
#endif
    Check(f.Use() == Result::Success && ItemSystem::GetItemCount(f.material) == 1,
        "DS inventory/extra-inventory transaction failed");
    Check(saves == 1 && updates == 1 && payments == 1 && f.Attrs()[3].bType == APPLY_PENETRATE_PCT &&
        f.Attrs()[4].bType == APPLY_ATTBONUS_MONSTER && f.Attrs()[5].bType == 0 && f.Attrs()[6].bType == 0,
        "DS reroll published partial state or retained old additional bonuses");
    f.Watch();
    floatDrawFraction = 1.f;
    Check(f.Use() == Result::Success && !g_registry.valid(f.material) && saves == 1 && updates == 1,
        "DS last-unit consumption failed");
    Check(f.Attrs()[3].bType == APPLY_ATTBONUS_HUMAN && f.Attrs()[4].bType == APPLY_ATTBONUS_MONSTER,
        "DS upper RNG endpoint selected duplicate or wrong rows");
    f.Watch();
    Check(f.Use() == Result::InvalidMaterial && payments == 0, "stale DS material reused");
    g_registry.destroy(f.item);
    Check(f.Use() == Result::InvalidTarget && payments == 0, "stale DS target accepted");
#endif
}

void DragonSoulEquipmentRules()
{
#ifdef ENABLE_DS_ENCHANT
    using Result = DSManager::EnchantResult;
    DragonSoulFixture f;
    auto& vnum = g_registry.get<ecs::ItemIdentity>(f.item).vnum;
    const auto originalVnum = vnum;
    vnum -= 1000;
    Check(f.Use() == Result::InvalidGrade, "lower-grade DS accepted enchant");
    vnum = originalVnum - 100;
    Check(f.Use() == Result::InvalidGrade, "lower-step DS accepted enchant");
    vnum = originalVnum;
    g_registry.emplace<ecs::ItemSockets>(f.item).sockets[ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX] = 1;
    Check(f.Use() == Result::Active, "active DS socket accepted reroll");
    g_registry.get<ecs::ItemSockets>(f.item).sockets[ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX] = 0;
    g_registry.emplace<ecs::ItemEquipped>(f.item).equipped = true;
    f.Place(f.item, EQUIPMENT, DRAGON_SOUL_EQUIP_SLOT_START);
    Check(f.Use() == Result::InvalidTarget, "detached equipped DS accepted");
    inventory[{f.owner, INVENTORY, DRAGON_SOUL_EQUIP_SLOT_START}] = f.item;
    g_registry.get<TestPlayer>(f.owner).activeDeck = 0;
    Check(f.Use() == Result::Active, "equipped DS with active deck accepted enchant");
    g_registry.get<TestPlayer>(f.owner).activeDeck = -1;
    f.Unchanged();
    Check(payments == 0, "rejected equipment request charged material");
    Check(f.Use() == Result::Success && saves == 1 && updates == 1 && payments == 1,
        "inactive equipped DS lost the existing enchant allowance");
    f.Watch();
    g_registry.get<ecs::ItemLocation>(f.item).cell = DRAGON_SOUL_EQUIP_SLOT_START - INVENTORY_MAX_NUM;
    Check(f.Use() == Result::InvalidTarget && payments == 0, "relative equipment cell accepted as stored absolute cell");
#endif
}

struct TransferFixture : PaidFixture {
    TItemTable scrollProto{}, donorProto{};
    entt::entity donor, npc;
    TransferFixture()
    {
        transferTest = true;
        proto.bType = ITEM_COSTUME;
        proto.bSubType = COSTUME_BODY;
        scrollProto.bType = ITEM_TRANSFER_SCROLL;
        scrollProto.bSubType = 255; // Not a costume subtype: scroll validation is independent.
        donorProto = proto;
        Place(item, INVENTORY, 5);
        g_registry.emplace<ecs::ItemCount>(item, 1);
        g_registry.emplace<ecs::ItemProtoRef>(material).proto = &scrollProto;
        donor = Material(40001, 1, 6);
        g_registry.emplace<ecs::ItemProtoRef>(donor).proto = &donorProto;
        auto& attributes = g_registry.emplace<ecs::ItemAttributes>(donor).attrs;
        for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
            attributes[i] = {static_cast<uint8_t>(i + 1), static_cast<int16_t>(10 + i)};
        npc = g_registry.create();
        g_registry.emplace<TransferActor>(npc);
        g_registry.emplace<TransferActor>(owner).npc = npc;
        Check(!g_registry.any_of<ecs::LegacyCharPtr>(owner) && !g_registry.any_of<ecs::LegacyCharPtr>(npc),
            "transfer fixture must have no legacy characters");
        Watch();
    }
    void Select()
    {
        AttrTransfer_command(owner, "open");
        AttrTransfer_command(owner, "add 0 0");
        AttrTransfer_command(owner, "add 2 6");
        AttrTransfer_command(owner, "add 1 5");
        Check(Window().items == std::array{material, item, donor}, "transfer selection failed");
    }
    auto& Window() { return g_registry.get<ecs::AttrTransferWindowComponent>(owner); }
    void Unchanged()
    {
        Check(EqualAttributes(Attrs(), beforePayment) && saves == 0 && updates == 0 && transferLogs == 0,
            "rejected transfer changed target or published success");
    }
};

void TransferWindowAndCommands()
{
    TransferFixture f;
    AttrTransfer_command(entt::null, "open");
    AttrTransfer_close(entt::null);
    AttrTransfer_clean_item(entt::null);
    AttrTransfer_add_item(entt::null, 0, 0);
    AttrTransfer_delete_item(entt::null, 0);
    Check(!AttrTransfer_make(entt::null) && !AttrTransfer_is_open(entt::null), "null transfer owner accepted");
    for (const auto input : {"", "opensesame", "open extra", "OPEN", "makejunk", "a 0", "close extra"})
        AttrTransfer_command(f.owner, input);
    Check(!AttrTransfer_is_open(f.owner) && transferCommands.empty(), "malformed command opened transfer");
    AttrTransfer_open(f.owner);
    const auto commandCount = transferCommands.size();
    AttrTransfer_open(f.owner);
    Check(AttrTransfer_is_open(f.owner) && transferCommands.size() == commandCount, "duplicate open reset window");
    for (const auto input : {"add -1 0", "add +0 0", "add 0 0junk", "add 0 2147483648", "add 3 0",
        "add 0 -1", "add 0 999999999999999999999", "add 0 0 extra", "add 0x0 0", "delete 0junk"})
        AttrTransfer_command(f.owner, input);
    Check(f.Window().items[0] == entt::null && payments == 0, "malformed index accepted");
    AttrTransfer_add_item(f.owner, INT_MIN, INT_MAX);
    AttrTransfer_add_item(f.owner, 0, INVENTORY_MAX_NUM);
    AttrTransfer_delete_item(f.owner, INT_MAX);
    AttrTransfer_add_item(f.owner, 1, 5);
    Check(f.Window().items[1] == entt::null, "target accepted before scroll/donor");
    f.Select();
    AttrTransfer_add_item(f.owner, 1, 6);
    Check(f.Window().items[1] == f.item && f.Window().items[2] == f.donor, "same item selected twice");
    for (const auto input : {"del 1", "d 2", "delete 0"}) AttrTransfer_command(f.owner, input);
    Check(f.Window().items == std::array<entt::entity,3>{entt::null,entt::null,entt::null}, "client del aliases broken");
    AttrTransfer_command(f.owner, "a 0 0");
    Check(f.Window().items[0] == f.material, "short add alias broken");
    g_registry.emplace<ecs::StatusFlags>(f.owner).isDead = true;
    AttrTransfer_command(f.owner, "close");
    Check(!AttrTransfer_is_open(f.owner) && f.Window().items[0] == entt::null, "dead owner cannot close window");
    const auto stale = f.owner;
    g_registry.destroy(stale);
    const auto replacement = g_registry.create();
    Check(entt::to_entity(stale) == entt::to_entity(replacement) && stale != replacement, "owner generation not recycled");
    AttrTransfer_command(stale, "open");
    Check(!AttrTransfer_is_open(stale), "retired owner accepted");
}

void TransferContextGuards()
{
    for (int scenario = 0; scenario < 16; ++scenario)
    {
        TransferFixture f;
        f.Select();
        auto& location = g_registry.get<TransferActor>(f.owner);
        switch (scenario)
        {
            case 0: location.x = ATTR_TRANSFER_MAX_DISTANCE; break;
            case 1: location.map = 2; break;
            case 2: location.npc = entt::null; break;
            case 3: g_registry.destroy(f.npc); (void)g_registry.create(); break;
            case 4: g_registry.emplace<ecs::StatusFlags>(f.owner).isDead = true; break;
            case 5: g_registry.emplace<ecs::StatusFlags>(f.owner).isObserverMode = true; break;
            case 6: g_registry.emplace<ecs::StatusFlags>(f.owner).isStunned = true; break;
            case 7: g_registry.emplace<ecs::SafeboxRef>(f.owner).isOpening = true; break;
            case 8: g_registry.emplace<ecs::ShopState>(f.owner).underRefine = true; break;
            case 9: g_registry.emplace<ecs::ShopState>(f.owner).shopOwner = f.npc; break;
            case 10: g_registry.emplace<ecs::ShopState>(f.owner).currentShop = reinterpret_cast<CShop*>(1); break;
            case 11: {
                const auto session = g_registry.create();
                g_registry.emplace<ecs::ExchangeSession>(session).offers[0].owner = f.owner;
                g_registry.emplace<ecs::ExchangeRef>(f.owner).session = session;
                break;
            }
            case 12: g_registry.emplace<ecs::AcceWindowComponent>(f.owner).absorptionOpen = true; break;
            case 13: location.x = INT_MIN; g_registry.get<TransferActor>(f.npc).x = INT_MAX; break;
            case 14: g_registry.emplace<ecs::ShopState>(f.owner).myShop = reinterpret_cast<CShop*>(1); break;
            case 15: g_registry.emplace<ecs::CubeWindowComponent>(f.owner).npc = f.npc; break;
        }
        Check(!AttrTransfer_make(f.owner) && payments == 0, "blocked transfer context accepted");
        f.Unchanged();
        AttrTransfer_close(f.owner);
        Check(!AttrTransfer_is_open(f.owner), "blocked context prevented close");
    }
    {
        TransferFixture f;
        const auto cubeNpc = g_registry.create();
        g_registry.emplace<ecs::CubeWindowComponent>(f.owner).npc = cubeNpc;
        AttrTransfer_open(f.owner);
        Check(!AttrTransfer_is_open(f.owner), "live cube NPC did not block transfer");
        g_registry.destroy(cubeNpc);
        const auto replacement = g_registry.create();
        Check(entt::to_entity(cubeNpc) == entt::to_entity(replacement) && cubeNpc != replacement,
            "cube NPC index not recycled");
        AttrTransfer_open(f.owner);
        Check(AttrTransfer_is_open(f.owner), "stale cube NPC inherited replacement window");
        AttrTransfer_close(f.owner);
        g_registry.get<ecs::CubeWindowComponent>(f.owner).npc = replacement;
        AttrTransfer_open(f.owner);
        Check(!AttrTransfer_is_open(f.owner), "replacement cube NPC not recognized when explicitly assigned");
    }
    TransferFixture f;
    g_registry.get<TransferActor>(f.npc).x = ATTR_TRANSFER_MAX_DISTANCE;
    AttrTransfer_open(f.owner);
    Check(!AttrTransfer_is_open(f.owner), "far-away NPC opened window");
    g_registry.get<TransferActor>(f.npc).x = ATTR_TRANSFER_MAX_DISTANCE - 1;
    AttrTransfer_open(f.owner);
    Check(AttrTransfer_is_open(f.owner), "valid distance boundary rejected");
}

void TransferItemGuards()
{
    for (int slot = 0; slot < 3; ++slot)
        for (int scenario = 0; scenario < 8; ++scenario)
        {
            TransferFixture f;
            f.Select();
            const auto selected = f.Window().items[slot];
            switch (scenario)
            {
                case 0: g_registry.emplace<ecs::ItemFlags>(selected).isLocked = true; break;
                case 1: g_registry.emplace<ecs::ItemFlags>(selected).exchanging = true; break;
                case 2: g_registry.emplace<ecs::ItemEquipped>(selected).equipped = true; break;
                case 3: g_registry.get<ecs::ItemOwner>(selected).owner = f.npc; break;
                case 4: inventory.erase({f.owner, INVENTORY, ItemSystem::GetItemCell(selected)}); break;
                case 5: f.Place(selected, INVENTORY, 9); break;
                case 6: g_registry.get<ecs::ItemCount>(selected).count = 0; break;
                case 7: g_registry.get<ecs::ItemLocation>(selected).window = SAFEBOX; break;
            }
            Check(!AttrTransfer_make(f.owner) && payments == 0, "invalid selected item accepted");
            f.Unchanged();
        }
    for (int slot = 0; slot < 3; ++slot)
    {
        TransferFixture f;
        f.Select();
        const auto old = f.Window().items[slot];
        const auto cell = ItemSystem::GetItemCell(old);
        g_registry.destroy(old);
        const auto replacement = f.Material(40001, 1, cell);
        Check(entt::to_entity(old) == entt::to_entity(replacement) && old != replacement, "item generation not recycled");
        Check(!AttrTransfer_make(f.owner) && payments == 0 && ItemSystem::GetItemCount(replacement) == 1,
            "replacement item consumed through stale selection");
    }
    for (int scenario = 0; scenario < 8; ++scenario)
    {
        TransferFixture f;
        f.Select();
        switch (scenario)
        {
            case 0: f.donorProto.bSubType = COSTUME_HAIR; break;
            case 1: g_registry.get<ecs::ItemIdentity>(f.donor).vnum = 73001; break;
            case 2: g_registry.get<ecs::ItemAttributes>(f.donor).attrs.fill({}); break;
            case 3: g_registry.remove<ecs::ItemAttributes>(f.donor); break;
            case 4: g_registry.get<ecs::ItemCount>(f.donor).count = 2; break;
            case 5: g_registry.get<ecs::ItemCount>(f.item).count = 2; break;
            case 6: g_registry.get<ecs::ItemAttributes>(f.donor).attrs[0].bType = MAX_APPLY_NUM; break;
            case 7:
                g_registry.get<ecs::ItemAttributes>(f.donor).attrs.fill({});
                g_registry.get<ecs::ItemAttributes>(f.donor).attrs[6] = {1, 10};
                break;
        }
        Check(!AttrTransfer_make(f.owner) && payments == 0, "malformed costume accepted");
        f.Unchanged();
    }
}

void TransferPaymentAndCommit()
{
    for (const int count : {1, 3})
    {
        TransferFixture f;
        g_registry.get<ecs::ItemCount>(f.material).count = count;
        f.Select();
        auto expected = g_registry.get<ecs::ItemAttributes>(f.donor).attrs;
#ifdef ENABLE_ATTR_COSTUMES
        expected[5] = {}; expected[6] = {};
#endif
        onSave = [&](entt::entity published) {
            if (published != f.item) return;
            Check(EqualAttributes(f.Attrs(), expected) && ItemSystem::GetItemCount(f.material) == count - 1 &&
                ItemSystem::GetItemCount(f.donor) == 0, "first callback observed a partial batch");
            Check(f.Window().busy && !AttrTransfer_make(f.owner), "reentrant transfer accepted");
            AttrTransfer_close(f.owner);
            AttrTransfer_add_item(f.owner, 1, 6);
        };
        onDestroy = [&](entt::entity retired) {
            Check(ItemSystem::IsItemConsumptionPending(retired) && ItemSystem::IsItemLocked(retired) &&
                !ItemSystem::CanConsumeOwnedItem(f.owner, retired), "retired payment remained spendable");
            ItemSystem::ProcessPendingItemConsumptions(); // Must not recursively destroy this item.
        };
        Check(AttrTransfer_make(f.owner), "valid transfer failed");
        Check(EqualAttributes(f.Attrs(), expected) && !g_registry.valid(f.donor) &&
            payments == 0 && saves == 1 && updates == 1 && transferLogs == 1 && !f.Window().busy,
            "batch transfer did not publish exactly once");
        Check(count == 1 ? !g_registry.valid(f.material) : ItemSystem::GetItemCount(f.material) == count - 1,
            "wrong transfer scroll count");
        Check(destroyAttempts.size() == (count == 1 ? 2u : 1u), "cleanup repeated a debit or destruction");
        Check(publishedCounts.size() == (count == 1 ? 0u : 1u), "surviving stack count was not published");
        Check(!AttrTransfer_make(f.owner) && payments == 0, "repeated transfer charged twice");
    }
    TransferFixture f;
    f.Select();
    rejectDestruction.insert(f.donor);
    Check(AttrTransfer_make(f.owner) && !f.Window().busy, "committed batch reported a failed payment");
    Check(ItemSystem::GetItemCount(f.material) == 1 && ItemSystem::GetItemCount(f.donor) == 0 &&
        ItemSystem::IsItemConsumptionPending(f.donor) && !ItemSystem::CanConsumeOwnedItem(f.owner, f.donor),
        "failed cleanup left a partial debit or spendable donor");
    const auto committed = f.Attrs();
    rejectDestruction.clear();
    ItemSystem::ProcessPendingItemConsumptions();
    Check(!g_registry.valid(f.donor) && EqualAttributes(f.Attrs(), committed) && ItemSystem::GetItemCount(f.material) == 1 &&
        saves == 1 && updates == 1 && transferLogs == 1, "cleanup retry recharged or republished transaction");
}

void BatchCostValidation()
{
    using Cost = ItemSystem::ItemCost;
    for (int scenario = 0; scenario < 12; ++scenario)
    {
        TransferFixture f;
        auto desired = g_registry.get<ecs::ItemAttributes>(f.donor);
        std::vector<Cost> costs {{f.material, 1}, {f.donor, 1}};
        auto owner = f.owner;
        switch (scenario) {
            case 0: costs.clear(); break;
            case 1: costs[0].amount = 0; break;
            case 2: costs[1].amount = 2; break;
            case 3: costs[1].amount = UINT32_MAX; break;
            case 4: costs[1].item = f.material; break;
            case 5: costs[1].item = f.item; break;
            case 6: owner = entt::null; break;
            case 7: costs.resize(65, Cost{f.material, 1}); break;
            case 8: desired.attrs[0].bType = MAX_APPLY_NUM; break;
            case 9: g_registry.get<ecs::ItemOwner>(f.donor).owner = f.npc; break;
            case 10: g_registry.emplace<ecs::ItemFlags>(f.material).isLocked = true; break;
            case 11: g_registry.remove<ecs::ItemCount>(f.donor); break;
        }
        Check(!ItemSystem::SetItemAttributesWithItemCosts(owner, f.item, desired, costs), "invalid batch accepted");
        f.Unchanged();
        Check(ItemSystem::GetItemCount(f.material) == 2 && destroyAttempts.empty() && publishedCounts.empty() &&
            !ItemSystem::IsItemConsumptionPending(f.material) && !ItemSystem::IsItemConsumptionPending(f.donor),
            "rejected batch changed costs or retirement state");
    }
    TransferFixture f;
    std::vector<Cost> costs;
    for (int i = 0; i < 64; ++i)
        costs.push_back({f.Material(50000 + i, 3, static_cast<uint16_t>(20 + i)), 2});
    const auto desired = g_registry.get<ecs::ItemAttributes>(f.donor);
    onSave = [&](entt::entity published) {
        if (published != f.item) return;
        for (const auto cost : costs)
            Check(ItemSystem::GetItemCount(cost.item) == 1, "64-cost batch partly visible");
        Check(EqualAttributes(f.Attrs(), desired.attrs), "target not committed with all costs");
    };
    Check(ItemSystem::SetItemAttributesWithItemCosts(f.owner, f.item, desired, costs), "64 valid costs rejected");
    Check(publishedCounts.size() == 64 && saves == 1 && updates == 1 && destroyAttempts.empty(), "64-cost publication incorrect");
}

void BatchReentrancyAndRetirement()
{
    using Cost = ItemSystem::ItemCost;
    {
        TransferFixture f;
        g_registry.get<ecs::ItemCount>(f.material).count = 5;
        const std::array outerCosts {Cost{f.material, 1}, Cost{f.donor, 1}};
        auto desired = g_registry.get<ecs::ItemAttributes>(f.donor);
        auto nestedAttributes = desired;
        nestedAttributes.attrs[0].sValue = 321;
        bool nested = false;
        onSave = [&](entt::entity published) {
            if (published != f.item || nested) return;
            nested = true;
            Check(ItemSystem::GetItemCount(f.material) == 4 && ItemSystem::GetItemCount(f.donor) == 0,
                "nested transaction saw partial outer payment");
            const std::array nestedCosts {Cost{f.material, 2}};
            Check(ItemSystem::SetItemAttributesWithItemCosts(f.owner, f.item, nestedAttributes, nestedCosts), "nested batch rejected");
        };
        Check(ItemSystem::SetItemAttributesWithItemCosts(f.owner, f.item, desired, outerCosts), "outer batch failed");
        Check(EqualAttributes(f.Attrs(), nestedAttributes.attrs) && ItemSystem::GetItemCount(f.material) == 2 &&
            !g_registry.valid(f.donor), "outer publication restored an obsolete snapshot");
    }
    {
        TransferFixture f;
        const std::array costs {Cost{f.material, 2}, Cost{f.donor, 1}};
        const auto desired = g_registry.get<ecs::ItemAttributes>(f.donor);
        rejectDestruction.insert(f.donor);
        Check(ItemSystem::SetItemAttributesWithItemCosts(f.owner, f.item, desired, costs), "retirement fixture commit failed");
        const auto old = f.donor;
        g_registry.destroy(old);
        const auto replacement = f.Material(49999, 7, 6);
        Check(entt::to_entity(old) == entt::to_entity(replacement) && old != replacement, "retired generation not recycled");
        const auto attempts = destroyAttempts.size();
        ItemSystem::ProcessPendingItemConsumptions();
        Check(g_registry.valid(replacement) && ItemSystem::GetItemCount(replacement) == 7 &&
            !ItemSystem::IsItemConsumptionPending(replacement) && destroyAttempts.size() == attempts,
            "retirement queue destroyed the replacement generation");
    }
    {
        TransferFixture f;
        const auto desired = g_registry.get<ecs::ItemAttributes>(f.donor);
        const std::array costs {Cost{f.material, 1}, Cost{f.donor, 1}};
        const auto third = f.Material(49000, 1, 8);
        const std::array nestedCosts {Cost{third, 1}};
        bool appended = false;
        onDestroy = [&](entt::entity) {
            if (appended) return;
            appended = true;
            Check(ItemSystem::SetItemAttributesWithItemCosts(f.owner, f.item, desired, nestedCosts), "batch during cleanup rejected");
        };
        Check(ItemSystem::SetItemAttributesWithItemCosts(f.owner, f.item, desired, costs), "cleanup append fixture failed");
        Check(ItemSystem::IsItemConsumptionPending(third), "new retirement was not queued during cleanup");
        ItemSystem::ProcessPendingItemConsumptions();
        Check(!g_registry.valid(third) && destroyAttempts.size() == 2, "appended retirement skipped or processed twice");
    }
    {
        TransferFixture f;
        const auto desired = g_registry.get<ecs::ItemAttributes>(f.donor);
        const std::array costs {Cost{f.material, 1}, Cost{f.donor, 1}};
        entt::entity replacement {entt::null};
        onSave = [&](entt::entity published) {
            if (published != f.item) return;
            Check(ItemSystem::GetItemCount(f.material) == 1 && ItemSystem::GetItemCount(f.donor) == 0,
                "post-commit callback observed partial costs");
            g_registry.destroy(f.item);
            replacement = f.Material(48000, 1, 5);
            g_registry.emplace<ecs::ItemAttributes>(replacement).attrs[0] = {1, 999};
        };
        Check(ItemSystem::SetItemAttributesWithItemCosts(f.owner, f.item, desired, costs),
            "post-commit target disappearance was reported as an uncommitted payment");
        Check(replacement != f.item && entt::to_entity(replacement) == entt::to_entity(f.item) &&
            g_registry.get<ecs::ItemAttributes>(replacement).attrs[0].sValue == 999 && updates == 0 &&
            ItemSystem::GetItemCount(f.material) == 1 && !g_registry.valid(f.donor),
            "post-commit replacement overwritten, refunded or published as old target");
    }
}


struct ExtractionFixture : PaidFixture {
    DSManager manager;
    TItemTable extractorProto {};
    TItemPos destination;
    explicit ExtractionFixture(bool equipped = false, bool loadTable = true)
    {
        extractionTest = true;
        rejectCreation = rejectSocket = rejectPlace = false;
        allowHandling = allowUnequip = dsReadOk = dsHeartOk = dsPullOk = true;
        createdOutputs.clear(); givenOutputs.clear(); extractionLogs = 0;
        heartCharges = {50.f}; heartProbabilities = {100.f};
        pullProbability = 100.f; byProductVnum = 0;
        proto.bType = ITEM_DS; proto.bSubType = 0; proto.bSize = 1;
        g_registry.get<ecs::ItemIdentity>(item).vnum = 110000;
        g_registry.emplace<ecs::ItemCount>(item, ecs::ItemCount{1});
        extractorProto.bType = ITEM_EXTRACT;
        extractorProto.bSubType = equipped ? EXTRACT_DRAGON_SOUL : EXTRACT_DRAGON_HEART;
        extractorProto.alValues[0] = 50;
        g_registry.emplace<ecs::ItemProtoRef>(material).proto = &extractorProto;
        outputProto = {}; outputProto.bSize = 1;
        if (loadTable) Check(manager.ReadDragonSoulTableFile("extraction-test-table"), "extraction table failed");
        emptyDSCell = manager.GetBasePosition(item);
        destination = TItemPos(DRAGON_SOUL_INVENTORY, emptyDSCell);
        Place(item, equipped ? EQUIPMENT : DRAGON_SOUL_INVENTORY,
            equipped ? INVENTORY_MAX_NUM + WEAR_MAX_NUM : emptyDSCell);
        g_registry.emplace<ecs::ItemEquipped>(item).equipped = equipped;
        Check(!g_registry.any_of<ecs::LegacyCharPtr>(owner) &&
            !g_registry.any_of<ecs::LegacyItemPtr>(material), "extraction fixtures must be entity-only");
    }
    bool Run(bool equipped)
    {
        return equipped ? manager.PullOutEcs(owner, destination, item, material) :
            manager.ExtractDragonHeartEcs(owner, item, material);
    }
    void Intact(bool equipped)
    {
        Check(ItemSystem::IsValidItem(item) && ItemSystem::GetItemCount(item) == 1 &&
            ItemSystem::GetItemCount(material) == 2 && ItemSystem::IsItemEquipped(item) == equipped,
            "rejected extraction altered inputs");
        Check(givenOutputs.empty() && extractionLogs == 0 && payments == 0,
            "rejected extraction delivered/logged/charged");
    }
};

void ExtractionInputGuards()
{
    for (bool equipped : {false, true})
    {
        ExtractionFixture f(equipped);
        const auto run = [&] { return f.Run(equipped); };
        allowHandling = false;
        Check(!run(), "extraction while inventory blocked");
        allowHandling = true;
        const auto stranger = g_registry.create();
        g_registry.emplace<TestPlayer>(stranger);
        for (const auto entity : {f.item, f.material})
        {
            g_registry.get<ecs::ItemOwner>(entity).owner = stranger;
            Check(!run(), "foreign extraction input accepted");
            g_registry.get<ecs::ItemOwner>(entity).owner = f.owner;
            auto& flags = g_registry.emplace<ecs::ItemFlags>(entity);
            flags.isLocked = true; Check(!run(), "locked extraction input accepted");
            flags.isLocked = false; flags.exchanging = true;
            Check(!run(), "exchanging extraction input accepted"); flags.exchanging = false;
        }
        const auto originalMaterial = f.material;
        f.material = f.item;
        Check(!run(), "soul accepted as its own extractor");
        f.material = originalMaterial;
        f.extractorProto.bSubType = equipped ? EXTRACT_DRAGON_HEART : EXTRACT_DRAGON_SOUL;
        Check(!run(), "wrong extractor subtype accepted");
        f.extractorProto.bSubType = equipped ? EXTRACT_DRAGON_SOUL : EXTRACT_DRAGON_HEART;
        inventory.erase({f.owner, INVENTORY, 0});
        Check(!run(), "unanchored extractor accepted");
        f.Place(f.material, INVENTORY, 0);
        Check(randomCalls == 0 && floatRandomCalls == 0, "bad inputs reached RNG");
        f.Intact(equipped);
        g_registry.destroy(f.material);
        Check(!run(), "stale extractor accepted");
        g_registry.destroy(f.owner);
        Check(!run(), "stale owner accepted");
    }
    ExtractionFixture f;
    f.Place(f.item, INVENTORY, 5);
    Check(!f.Run(false), "heart accepted DS outside its inventory");
    f.Intact(false);
}

void HeartExtractionTransactions()
{
    {
        ExtractionFixture f;
        rejectCreation = true;
        Check(!f.Run(false), "missing heart output accepted"); f.Intact(false);
        rejectCreation = false; rejectSocket = true;
        Check(!f.Run(false), "failed output socket accepted"); f.Intact(false);
        Check(createdOutputs.size() == 1 && !g_registry.valid(createdOutputs.back()), "unused heart leaked");
    }
    for (bool duringSocket : {false, true})
    {
        ExtractionFixture f;
        const auto change = [&](entt::entity) {
            g_registry.get<ecs::ItemFlags>(f.material).isLocked = true;
        };
        g_registry.emplace<ecs::ItemFlags>(f.material);
        if (duringSocket) onSocket = change; else onCreate = change;
        Check(!f.Run(false), "changed material accepted after output callback");
        f.Intact(false);
        Check(!g_registry.valid(createdOutputs.back()), "aborted output leaked");
    }
    {
        ExtractionFixture f;
        const auto source = f.item;
        onSave = [&](entt::entity) {
            Check(ItemSystem::IsItemConsumptionPending(source) && ItemSystem::GetItemCount(source) == 0 &&
                ItemSystem::GetItemCount(f.material) == 1, "heart published a partial debit");
            Check(!f.Run(false), "recursive heart extraction was allowed");
        };
        Check(f.Run(false), "entity-only heart extraction failed");
        Check(!g_registry.valid(source) && ItemSystem::GetItemCount(f.material) == 1 &&
            givenOutputs.size() == 1 && extractionLogs == 1, "heart debit/reward count wrong");
        Check(ItemSystem::GetItemSocket(givenOutputs.front(), ITEM_SOCKET_CHARGING_AMOUNT_IDX) ==
#ifdef ENABLE_DS_EDITS
            50,
#else
            75,
#endif
            "heart charging policy changed");
        Check(!f.Run(false) && givenOutputs.size() == 1, "consumed heart source reused");
    }
    {
        ExtractionFixture f;
        f.extractorProto.alValues[0] = 0; heartCharges = {0};
        Check(!f.Run(false), "zero-charge extraction reported success");
        Check(!g_registry.valid(f.item) && ItemSystem::GetItemCount(f.material) == 1 &&
            givenOutputs.empty() && extractionLogs == 1, "failed roll did not commit both costs");
    }
    {
        ExtractionFixture f;
        const auto source = f.item;
        rejectDestruction.insert(source);
        Check(f.Run(false), "committed heart extraction rolled back on cleanup failure");
        Check(ItemSystem::IsItemConsumptionPending(source) && ItemSystem::GetItemCount(source) == 0 &&
            givenOutputs.size() == 1, "retired DS remained spendable");
        Check(!f.Run(false) && givenOutputs.size() == 1, "pending cleanup duplicated heart");
        rejectDestruction.clear(); ItemSystem::ProcessPendingItemConsumptions();
        Check(!g_registry.valid(source), "DS retirement cleanup did not retry");
    }
    {
        ExtractionFixture f;
        onLog = [&](entt::entity owner) { g_registry.destroy(owner); };
        Check(f.Run(false), "committed debit changed result after owner destruction");
        Check(givenOutputs.empty() && !g_registry.valid(createdOutputs.back()), "orphaned output after owner teardown");
    }
}

void ExtractionTableValidation()
{
    for (bool equipped : {false, true})
    {
        ExtractionFixture f(equipped, false);
        Check(!f.Run(equipped), "extraction without initialized table accepted");
        f.Intact(equipped);
    }
    for (int scenario = 0; scenario < 7; ++scenario)
    {
        ExtractionFixture f;
        switch (scenario) {
        case 0: dsHeartOk = false; break;
        case 1: heartProbabilities.clear(); break;
        case 2: heartProbabilities = {0}; break;
        case 3: heartProbabilities = {-1}; break;
        case 4: heartProbabilities = {std::numeric_limits<float>::infinity()}; break;
        case 5: heartCharges = {std::numeric_limits<float>::quiet_NaN()}; break;
        case 6: heartCharges = {-1}; break;
        }
        Check(!f.Run(false), "invalid heart table accepted");
        Check(floatRandomCalls == 0, "invalid table reached RNG"); f.Intact(false);
    }
    for (float probability : {-1.f, 101.f, std::numeric_limits<float>::quiet_NaN()})
    {
        ExtractionFixture f(true); pullProbability = probability;
        Check(!f.Run(true), "invalid pull probability accepted"); f.Intact(true);
    }
    {
        ExtractionFixture f;
        // A zero-weight leading row must not be selected by a zero RNG draw.
        heartCharges = {0, 50}; heartProbabilities = {0, 100};
        Check(f.Run(false) && givenOutputs.size() == 1, "zero-weight heart row selected");
    }
}

void PullOutTransactions()
{
    {
        ExtractionFixture f(true);
        randomOffset = 0; pullProbability = 0; // Extractor replaces the base probability.
        Check(f.Run(true), "extractor-assisted pull failed");
        Check(ItemSystem::GetItem(f.owner, f.destination) == f.item && !ItemSystem::IsItemEquipped(f.item) &&
            ItemSystem::GetItemCount(f.material) == 1 && givenOutputs.empty(), "successful pull state incorrect");
        Check(!f.Run(true), "already unequipped stone pulled twice");
    }
    {
        ExtractionFixture f(true);
        dsPullOk = false; // Missing row retains the legacy free, guaranteed pull.
        Check(f.manager.PullOutEcs(f.owner, NPOS, f.item, f.material), "fallback destination/missing-row pull failed");
        Check(ItemSystem::GetItemCount(f.material) == 2 && floatRandomCalls == 0 && extractionLogs == 0,
            "missing extraction row charged/rolled");
    }
    {
        ExtractionFixture f(true);
        const auto blocker = f.Material(99, 1, 20);
        f.Place(blocker, DRAGON_SOUL_INVENTORY, f.destination.cell);
        Check(!f.Run(true), "occupied destination accepted"); f.Intact(true);
        Check(ItemSystem::GetItem(f.owner, f.destination) == blocker, "destination overwritten");
    }
    {
        ExtractionFixture f(true);
        emptyDSCell = -1;
        Check(!f.manager.PullOutEcs(f.owner, NPOS, f.item, f.material), "full DS inventory accepted");
        f.Intact(true);
    }
    {
        ExtractionFixture f(true);
        rejectPlace = true;
        Check(!f.Run(true), "rejected placement accepted"); f.Intact(true);
        Check(ItemSystem::GetWearItem(f.owner, WEAR_MAX_NUM) == f.item, "failed pull lost original wear anchor");
    }
    {
        ExtractionFixture f(true);
        g_registry.emplace<ecs::ItemFlags>(f.material);
        onPlace = [&](entt::entity) { g_registry.get<ecs::ItemFlags>(f.material).isLocked = true; };
        Check(!f.Run(true), "invalidated extractor charged after placement"); f.Intact(true);
    }
    {
        ExtractionFixture f(true);
        onRemove = [&](entt::entity soul) { g_registry.destroy(soul); };
        Check(!f.Run(true), "removed stale DS accepted");
        Check(ItemSystem::GetItemCount(f.material) == 2 && givenOutputs.empty(), "lost DS callback charged material");
    }
    {
        ExtractionFixture f(true);
        // Failed roll retires both inputs before any destruction callback.
        randomOffset = 99; byProductVnum = 999;
        g_registry.get<ecs::ItemCount>(f.material).count = 1;
        const auto source = f.item, extractor = f.material;
        rejectDestruction.insert(source); rejectDestruction.insert(extractor);
        onDestroy = [&](entt::entity) {
            Check(ItemSystem::GetItemCount(source) == 0 && ItemSystem::GetItemCount(extractor) == 0 &&
                ItemSystem::IsItemConsumptionPending(source) && ItemSystem::IsItemConsumptionPending(extractor),
                "pull published a partial retirement");
            auto retry = source;
            Check(!f.manager.PullOutEcs(f.owner, f.destination, retry, extractor), "recursive retired pull accepted");
        };
        Check(!f.Run(true) && f.item == entt::null, "failed pull did not retire caller handle");
        Check(givenOutputs.size() == 1 && extractionLogs == 1, "failed pull byproduct missing/duplicated");
        onDestroy = {}; rejectDestruction.clear(); ItemSystem::ProcessPendingItemConsumptions();
        Check(!g_registry.valid(source) && !g_registry.valid(extractor) && givenOutputs.size() == 1,
            "retirement retry recreated reward");
    }
    {
        ExtractionFixture f(true);
        randomOffset = 99; byProductVnum = 999; rejectCreation = true;
        Check(!f.Run(true), "missing byproduct output accepted"); f.Intact(true);
    }
}


void ExtractionCallbacksAndCostPolicy()
{
    for (bool equipped : {false, true})
    {
        ExtractionFixture f(equipped);
        const auto block = [](entt::entity) { allowHandling = false; };
        if (equipped) onRemove = block; else onSocket = block;
        Check(!f.Run(equipped), "extraction continued after a conflicting window opened");
        f.Intact(equipped);
    }
    {
        ExtractionFixture f;
        const ItemSystem::ItemCost ordinary[] = {{f.item, 1}};
        Check(!ItemSystem::ConsumeOwnedItemCosts(f.owner, ordinary), "ordinary cost silently accepted DS inventory");
        using Storage = ItemSystem::ItemCostStorage;
        const ItemSystem::ItemCost duplicate[] = {{f.item, 1, Storage::DragonSoulInventory}, {f.item, 1, Storage::DragonSoulInventory}};
        Check(!ItemSystem::ConsumeOwnedItemCosts(f.owner, duplicate), "duplicate DS cost accepted");
        const ItemSystem::ItemCost insufficient[] = {{f.item, 1, Storage::DragonSoulInventory}, {f.material, 3}};
        Check(!ItemSystem::ConsumeOwnedItemCosts(f.owner, insufficient), "partly affordable DS batch accepted");
        f.Intact(false);
        Check(f.manager.ExtractDragonHeartEcs(f.owner, f.item) ==
#ifdef ENABLE_DS_EDITS
            false,
#else
            true,
#endif
            "extractor-free heart rule changed");
        Check(!g_registry.valid(f.item) && ItemSystem::GetItemCount(f.material) == 2,
            "extractor-free heart consumed an unrelated material");
    }
    for (bool success : {false, true})
    {
        ExtractionFixture f(true);
        floatDrawFraction = 0.5f; pullProbability = success ? 100.f : 0.f;
        Check(f.manager.PullOutEcs(f.owner, f.destination, f.item) == success,
            "extractor-free pull result wrong");
        Check(ItemSystem::GetItemCount(f.material) == 2 && givenOutputs.empty(),
            "extractor-free pull charged unrelated material or created byproduct");
        if (!success) Check(f.item == entt::null, "extractor-free failure did not retire source");
    }
}


struct SoulStateFixture : ExtractionFixture {
    SoulStateFixture() : ExtractionFixture(true) {
        soulStateTest = true;
        soulAdds = soulRemoves = soulStarts = soulStops = soulLogs = deckStops = 0;
        rejectSoulPoints = rejectSoulTimer = false;
        soulBonus.clear(); soulTimers.clear();
        g_registry.get<TestPlayer>(owner).activeDeck = 0;
        proto.cLimitTimerBasedOnWearIndex = 0;
        g_registry.emplace<ecs::ItemSockets>(item).sockets[ITEM_SOCKET_REMAIN_SEC] = 120;
    }
    bool Active() { return manager.IsActiveDragonSoul(item); }
};

void SoulStateValidation()
{
    SoulStateFixture f;
    Check(!f.manager.ActivateDragonSoul(entt::null) && !f.manager.DeactivateDragonSoul(entt::null), "null soul toggle");
    Check(!f.manager.IsActiveDragonSoul(entt::null) && !f.manager.IsTimeLeftDragonSoul(entt::null), "null soul queries");
    Check(!f.manager.DeactivateDragonSoul(f.item), "inactive soul removed points");
    for (int deck : std::array<int, 4>{-1, 1, DRAGON_SOUL_DECK_MAX_NUM, INT_MAX}) {
        g_registry.get<TestPlayer>(f.owner).activeDeck = deck;
        Check(!f.manager.ActivateDragonSoul(f.item), "soul activated outside its deck");
    }
    g_registry.get<TestPlayer>(f.owner).activeDeck = 0;
    for (int remaining : {0, -1, INT_MIN}) {
        g_registry.get<ecs::ItemSockets>(f.item).sockets[ITEM_SOCKET_REMAIN_SEC] = remaining;
        Check(!f.manager.IsTimeLeftDragonSoul(f.item) && !f.manager.ActivateDragonSoul(f.item), "expired/negative lifetime accepted");
    }
    g_registry.get<ecs::ItemSockets>(f.item).sockets[ITEM_SOCKET_REMAIN_SEC] = 120;
    const auto wearCell = ItemSystem::GetItemCell(f.item);
    inventory.erase({f.owner, EQUIPMENT, wearCell});
    Check(!f.manager.ActivateDragonSoul(f.item), "unanchored soul activated");
    f.Place(f.item, EQUIPMENT, wearCell);
    const auto stranger = g_registry.create(); g_registry.emplace<TestPlayer>(stranger);
    g_registry.get<ecs::ItemOwner>(f.item).owner = stranger;
    Check(!f.manager.ActivateDragonSoul(f.item), "foreign soul activated");
    g_registry.get<ecs::ItemOwner>(f.item).owner = f.owner;
    g_registry.get<ecs::ItemEquipped>(f.item).equipped = false;
    Check(!f.manager.ActivateDragonSoul(f.item), "unequipped soul activated");
    g_registry.get<ecs::ItemEquipped>(f.item).equipped = true;
    Check(soulAdds == 0 && soulRemoves == 0 && soulStarts == 0 && soulLogs == 0, "invalid toggle caused side effects");
    f.proto.cLimitTimerBasedOnWearIndex = -1;
    Check(f.manager.LeftTime(f.item) == INT_MAX && f.manager.IsTimeLeftDragonSoul(f.item), "permanent soul expired");
    g_registry.destroy(f.item);
    Check(!f.manager.IsActiveDragonSoul(f.item) && !f.manager.ActivateDragonSoul(f.item), "stale soul accepted");
}

void SoulStateTransitions()
{
    {
        SoulStateFixture f;
        Check(f.manager.ActivateDragonSoul(f.item) && f.Active(), "entity-only soul activation failed");
        Check(f.manager.ActivateDragonSoul(f.item) && soulAdds == 1 && soulStarts == 1, "activation applied twice");
        g_registry.get<ecs::ItemSockets>(f.item).sockets[ITEM_SOCKET_REMAIN_SEC] = 0;
        Check(f.manager.DeactivateDragonSoul(f.item) && !f.Active(), "expired soul was not deactivated");
        Check(!f.manager.DeactivateDragonSoul(f.item) && soulRemoves == 1 && soulBonus[f.owner] == 0 &&
            soulTimers.empty() && deckStops == 1, "deactivation repeated or skipped cleanup");
    }
    {
        SoulStateFixture f;
        rejectSoulPoints = true;
        Check(!f.manager.ActivateDragonSoul(f.item) && !f.Active() && soulStarts == 0, "failed points activated soul");
        rejectSoulPoints = false; rejectSoulTimer = true;
        Check(!f.manager.ActivateDragonSoul(f.item) && !f.Active() && soulBonus[f.owner] == 0,
            "failed timer left soul bonus active");
    }
    for (int callback = 0; callback < 4; ++callback) {
        SoulStateFixture f;
        bool once = false;
        const auto deactivate = [&](entt::entity) {
            if (once) return; once = true;
            Check(f.manager.DeactivateDragonSoul(f.item, true), "nested deactivation rejected");
        };
        if (callback == 0) onSoulStart = deactivate;
        else if (callback == 1) onSave = deactivate;
        else if (callback == 2) onUpdate = deactivate;
        else onLog = deactivate;
        Check(!f.manager.ActivateDragonSoul(f.item) && !f.Active() && soulBonus[f.owner] == 0 &&
            soulAdds == 1 && soulRemoves == 1 && soulTimers.empty(), "activation overwrote callback deactivation");
    }
    {
        SoulStateFixture f;
        onSoulPoints = [&](entt::entity item, bool add) {
            Check(!f.manager.ActivateDragonSoul(item), "recursive activation allowed");
            if (!add) Check(!f.manager.DeactivateDragonSoul(item), "recursive deduction allowed");
        };
        Check(f.manager.ActivateDragonSoul(f.item), "guard blocked outer activation");
        Check(f.manager.DeactivateDragonSoul(f.item, true) && soulBonus[f.owner] == 0 && deckStops == 0,
            "guard blocked outer deactivation or ignored skip refresh");
    }
    {
        SoulStateFixture f;
        onSoulPoints = [&](entt::entity, bool add) { if (add) g_registry.get<TestPlayer>(f.owner).activeDeck = 1; };
        Check(!f.manager.ActivateDragonSoul(f.item) && !f.Active() && soulBonus[f.owner] == 0,
            "deck changed during point application but old soul stayed active");
        Check(g_registry.get<TestPlayer>(f.owner).activeDeck == 1, "old activation overwrote new deck");
    }
}

void SoulStateCallbacks()
{
    {
        SoulStateFixture f;
        Check(f.manager.ActivateDragonSoul(f.item), "teardown setup");
        onSoulStop = [&](entt::entity item) { g_registry.destroy(item); };
        Check(f.manager.DeactivateDragonSoul(f.item) && soulBonus[f.owner] == 0 && deckStops == 1,
            "deactivation read destroyed item or missed owner refresh");
    }
    for (bool destroyOwner : {false, true}) {
        SoulStateFixture f;
        entt::entity replacement = entt::null;
        onSoulStart = [&](entt::entity item) {
            if (destroyOwner) g_registry.destroy(f.owner);
            else {
                Check(f.manager.DeactivateDragonSoul(item, true), "teardown deactivation failed");
                g_registry.destroy(item);
            }
            replacement = g_registry.create();
        };
        Check(!f.manager.ActivateDragonSoul(f.item), "destroyed binding reported active");
        Check(replacement != entt::null && !g_registry.any_of<ecs::ItemSockets>(replacement),
            "recycled entity inherited active socket");
    }
    {
        SoulStateFixture f;
        Check(f.manager.ActivateDragonSoul(f.item), "refresh setup");
        const auto second = f.Material(110000, 1, 20);
        g_registry.emplace<ecs::ItemProtoRef>(second).proto = &f.proto;
        g_registry.emplace<ecs::ItemSockets>(second).sockets[ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX] = 1;
        g_registry.emplace<ecs::ItemEquipped>(second).equipped = true;
        f.Place(second, EQUIPMENT, DRAGON_SOUL_EQUIP_SLOT_START + 1);
        Check(f.manager.DeactivateDragonSoul(f.item) && deckStops == 0, "refresh disabled another active soul");
    }
}


struct CountFixture : PaidFixture {
    CountFixture()
    {
        countStateTest = true;
        Place(item, INVENTORY, 5);
        g_registry.emplace<ecs::ItemCount>(item, ecs::ItemCount{3});
    }
};

struct CountSignals {
    int calls {0};
    void Changed(entt::registry&, entt::entity) { ++calls; }
};

void CountValidationAndLimits()
{
    for (int scenario = 0; scenario < 5; ++scenario) {
        CountFixture f;
        auto invalid = f.item;
        switch (scenario) {
            case 0: invalid = entt::null; break;
            case 1: g_registry.destroy(invalid); break;
            case 2: invalid = g_registry.create(); break;
            case 3: g_registry.remove<ecs::ItemIdentity>(invalid); break;
            case 4: g_registry.remove<ecs::ItemCount>(invalid); break;
        }
        for (auto amount : {0u, 1u, UINT32_MAX})
            Check(!ItemSystem::SetItemCountEcs(invalid, amount), "invalid stack write accepted");
        Check(!ItemSystem::AddItemCountEcs(invalid, -1) && !ItemSystem::AddItemCountEcs(invalid, 1),
            "invalid stack delta accepted");
        if (scenario == 0 || scenario == 1 || scenario == 2 || scenario == 4)
            Check(ItemSystem::GetItemCount(invalid) == 0, "missing/stale stack read nonzero");
        Check(saves == 0 && updates == 0 && destroyAttempts.empty(), "rejected count write published");
        if (scenario == 4) Check(!g_registry.any_of<ecs::ItemCount>(invalid), "count setter created missing component");
    }
    {
        CountFixture f;
        CountSignals signals;
        entt::scoped_connection countConstruct = g_registry.on_construct<ecs::ItemCount>().connect<&CountSignals::Changed>(signals);
        entt::scoped_connection countUpdate = g_registry.on_update<ecs::ItemCount>().connect<&CountSignals::Changed>(signals);
        Check(ItemSystem::SetItemCountEcs(f.item, 4) && ItemSystem::GetItemCount(f.item) == 4,
            "positive stack update failed");
        Check(saves == 1 && updates == 1, "positive stack publication not exactly once");
        ItemSystem::SetItemCount(f.item, 8);
        Check(ItemSystem::GetItemCount(f.item) == 8 && saves == 2 && updates == 2, "void setter bypassed core");
        for (auto amount : {201u, UINT32_MAX})
            Check(ItemSystem::SetItemCountEcs(f.item, amount) && ItemSystem::GetItemCount(f.item) == 200,
                "stack limit/unsigned input narrowed before clamping");
        for (int limit : {0, -1}) {
            g_bItemCountLimit = limit;
            f.Watch();
            Check(!ItemSystem::SetItemCountEcs(f.item, 1) && ItemSystem::GetItemCount(f.item) == 200 &&
                saves == 0 && updates == 0, "invalid cap created zero/negative stack");
        }
        f.proto.bType = ITEM_ELK;
        Check(ItemSystem::SetItemCountEcs(f.item, UINT32_MAX) && ItemSystem::GetItemCount(f.item) == INT_MAX,
            "gold stack was not clamped independently to INT_MAX");
        Check(ItemSystem::AddItemCountEcs(f.item, INT_MAX) && ItemSystem::GetItemCount(f.item) == INT_MAX,
            "gold delta overflowed signed arithmetic");
        Check(ItemSystem::AddItemCountEcs(f.item, -1) && ItemSystem::GetItemCount(f.item) == INT_MAX - 1u,
            "negative delta did not use current component count");
        g_registry.get<ecs::ItemCount>(f.item).count = -10;
        Check(ItemSystem::GetItemCount(f.item) == 0, "negative component wrapped to unsigned count");
        Check(ItemSystem::SetItemCountEcs(f.item, 1) && ItemSystem::GetItemCount(f.item) == 1,
            "explicit valid count did not replace corrupt negative state");
        Check(signals.calls == 0, "count commit emitted registry signals");
    }
}

void CountDestructionAndCallbacks()
{
    for (bool delta : {false, true}) {
        CountFixture f;
        rejectDestruction.insert(f.item);
        const auto remove = [&] { return delta ? ItemSystem::AddItemCountEcs(f.item, INT_MIN) :
            ItemSystem::SetItemCountEcs(f.item, 0); };
        Check(!remove() && ItemSystem::GetItemCount(f.item) == 3 && saves == 0 && updates == 0,
            "failed zero-count removal reported success or changed count");
        rejectDestruction.clear();
        Check(remove() && !g_registry.valid(f.item) && saves == 0 && updates == 0,
            "zero-count removal did not retire the item");
    }
    {
        CountFixture f;
        bool nested = false;
        onSave = [&](entt::entity e) {
            Check(e == f.item && ItemSystem::GetItemCount(e) == (nested ? 9u : 5u),
                "save observed uncommitted stack");
            if (!nested) {
                nested = true;
                Check(ItemSystem::SetItemCountEcs(e, 9), "nested count update rejected");
            }
        };
        onUpdate = [&](entt::entity e) { Check(ItemSystem::GetItemCount(e) == 9, "outer packet restored stale count"); };
        Check(ItemSystem::SetItemCountEcs(f.item, 5) && ItemSystem::GetItemCount(f.item) == 9 &&
            saves == 2 && updates == 2, "nested count commit was overwritten");
    }
    for (int scenario = 0; scenario < 4; ++scenario) {
        CountFixture f;
        entt::entity replacement {entt::null};
        onSave = [&](entt::entity e) {
            Check(ItemSystem::GetItemCount(e) == 5, "save called before positive commit");
            if (scenario == 0) {
                g_registry.destroy(e);
                replacement = f.Material(50001, 37, 5);
                Check(entt::to_entity(replacement) == entt::to_entity(e) && replacement != e,
                    "fixture did not recycle entity index");
            } else if (scenario == 1) g_registry.remove<ecs::ItemCount>(e);
            else if (scenario == 2) g_registry.get<ecs::ItemCount>(e).count = 0;
            else Check(ItemSystem::SetItemCountEcs(e, 0), "nested zero-count removal failed");
        };
        Check(ItemSystem::SetItemCountEcs(f.item, 5) && saves == 1 && updates == 0,
            "post-save removal still published count or hid successful commit");
        if (scenario == 0) Check(ItemSystem::GetItemCount(replacement) == 37, "stale publication touched replacement item");
    }
    {
        CountFixture f;
        onUpdate = [&](entt::entity e) { g_registry.destroy(e); };
        Check(ItemSystem::SetItemCountEcs(f.item, 6) && !g_registry.valid(f.item) && saves == 1 && updates == 1,
            "packet callback destruction invalidated the commit result");
    }
    {
        CountFixture f;
        const std::array costs {ItemSystem::ItemCost{f.material, 2}};
        rejectDestruction.insert(f.material);
        Check(ItemSystem::ConsumeOwnedItemCosts(f.owner, costs) && ItemSystem::IsItemConsumptionPending(f.material),
            "fixture did not retain committed zero-count retirement");
        for (auto amount : {0u, 1u, UINT32_MAX})
            Check(!ItemSystem::SetItemCountEcs(f.material, amount), "count setter resurrected retired stack");
        Check(!ItemSystem::AddItemCountEcs(f.material, 1) && !ItemSystem::AddItemCountEcs(f.material, -1),
            "count delta bypassed retirement guard");
        Check(ItemSystem::GetItemCount(f.material) == 0 && saves == 0 && updates == 0,
            "retired stack was published");
        rejectDestruction.clear();
        ItemSystem::ProcessPendingItemConsumptions();
        Check(!g_registry.valid(f.material), "retired stack did not finish cleanup");
    }
}

struct StackFixture : CountFixture {
    StackFixture()
    {
        proto.bType = ITEM_USE;
        proto.bSubType = USE_POTION;
        Configure(item, 40);
        Configure(material, 180);
    }
    void Configure(entt::entity e, int count)
    {
        auto& identity = g_registry.get<ecs::ItemIdentity>(e);
        identity.vnum = identity.originalVnum = 1000;
        g_registry.emplace_or_replace<ecs::ItemCount>(e, ecs::ItemCount{count});
        g_registry.emplace_or_replace<ecs::ItemProtoRef>(e).proto = &proto;
        g_registry.emplace_or_replace<ecs::ItemPrototypeMeta>(e, ecs::ItemPrototypeMeta{proto.bType, proto.bSubType});
        g_registry.emplace_or_replace<ecs::ItemFlags>(e).flags = ITEM_FLAG_STACKABLE;
        g_registry.emplace_or_replace<ecs::ItemSockets>(e);
        g_registry.emplace_or_replace<ecs::ItemAttributes>(e);
    }
    entt::entity Target(int count, uint16_t cell)
    {
        const auto e = Material(1000, count, cell);
        Configure(e, count);
        return e;
    }
    void Detach()
    {
        std::erase_if(inventory, [&](const auto& entry) { return entry.second == item; });
        g_registry.get<ecs::ItemOwner>(item) = {};
        g_registry.get<ecs::ItemLocation>(item) = {RESERVED_WINDOW, 0};
    }
};

void StackMergeBoundaries()
{
    for (int limit = 1; limit <= 5; ++limit)
        for (int source = 1; source <= 7; ++source)
            for (int target = 1; target <= 6; ++target)
                for (uint32_t request = 0; request <= 8; ++request) {
                    StackFixture f;
                    g_bItemCountLimit = limit;
                    g_registry.get<ecs::ItemCount>(f.item).count = source;
                    g_registry.get<ecs::ItemCount>(f.material).count = target;
                    const int expected = request > uint32_t(source) || target >= limit ? 0 :
                        std::min(request ? int(request) : source, limit - target);
                    CountSignals signals;
                    entt::scoped_connection constructed = g_registry.on_construct<ecs::ItemCount>().connect<&CountSignals::Changed>(signals);
                    entt::scoped_connection updated = g_registry.on_update<ecs::ItemCount>().connect<&CountSignals::Changed>(signals);
                    const auto observe = [&](entt::entity) {
                        Check(ItemSystem::GetItemCount(f.item) == uint32_t(source - expected) &&
                            ItemSystem::GetItemCount(f.material) == uint32_t(target + expected),
                            "callback observed only half of stack transfer");
                    };
                    onSave = onUpdate = onDestroy = observe;
                    const auto result = ItemSystem::MergeItemStacksEcs(f.owner, f.item, f.material, request);
                    Check(result.transferred == uint32_t(expected) && result.sourceDepleted == (expected == source),
                        "stack transfer amount/depletion boundary incorrect");
                    Check(ItemSystem::GetItemCount(f.item) + ItemSystem::GetItemCount(f.material) == uint32_t(source + target),
                        "stack merge created or lost units");
                    Check(signals.calls == 0, "stack commit emitted registry signals");
                    if (expected == 0) Check(saves == 0 && updates == 0 && destroyAttempts.empty(), "failed merge published changes");
                    else if (expected == source) Check(!g_registry.valid(f.item) && saves == 1 && updates == 1 &&
                        destroyAttempts.size() == 1, "depleted stack not retired exactly once");
                    else Check(saves == 2 && updates == 2 && destroyAttempts.empty(), "partial merge publication incorrect");
                }
}

void StackMergeGuards()
{
    for (int scenario = 0; scenario < 37; ++scenario) {
        StackFixture f;
        auto owner = f.owner, source = f.item, target = f.material;
        auto storage = ItemSystem::StackSource::Inventory;
        uint32_t amount = 0;
        switch (scenario) {
            case 0: owner = entt::null; break;
            case 1: g_registry.destroy(owner); break;
            case 2: source = entt::null; break;
            case 3: target = entt::null; break;
            case 4: g_registry.destroy(source); break;
            case 5: g_registry.destroy(target); break;
            case 6: target = source; break;
            case 7: amount = UINT32_MAX; break;
            case 8: g_bItemCountLimit = 0; break;
            case 9: g_bItemCountLimit = -1; break;
            case 10: g_registry.remove<ecs::ItemCount>(source); break;
            case 11: g_registry.remove<ecs::ItemCount>(target); break;
            case 12: g_registry.get<ecs::ItemCount>(source).count = -1; break;
            case 13: g_registry.get<ecs::ItemCount>(target).count = 0; break;
            case 14: g_registry.get<ecs::ItemOwner>(target).owner = g_registry.create(); break;
            case 15: g_registry.get<ecs::ItemOwner>(source).owner = g_registry.create(); break;
            case 16: inventory.erase({owner, INVENTORY, 0}); break;
            case 17: inventory.erase({owner, INVENTORY, 5}); break;
            case 18: f.Place(source, MOUNT_INVENTORY, 0); break;
            case 19: f.Place(target, SAFEBOX, 0); break;
            case 20: f.Place(target, INVENTORY, INVENTORY_MAX_NUM); break;
            case 21: g_registry.emplace<ecs::ItemEquipped>(source).equipped = true; break;
            case 22: g_registry.emplace<ecs::ItemEquipped>(target).equipped = true; break;
            case 23: g_registry.get<ecs::ItemFlags>(source).exchanging = true; break;
            case 24: g_registry.get<ecs::ItemFlags>(target).exchanging = true; break;
            case 25: g_registry.get<ecs::ItemFlags>(source).isLocked = true; break;
            case 26: g_registry.get<ecs::ItemFlags>(target).isLocked = true; break;
            case 27: storage = static_cast<ItemSystem::StackSource>(99); break;
            case 28: storage = ItemSystem::StackSource::DetachedReward; break; // still owned
            case 29: f.Detach(); break; // requires explicit reward opt-in
            case 30: f.Detach(); storage = ItemSystem::StackSource::DetachedReward;
                g_registry.get<ecs::ItemLocation>(source).window = GROUND; break;
            case 31: f.Detach(); storage = ItemSystem::StackSource::DetachedReward;
                g_registry.get<ecs::ItemOwner>(source).ownerPID = 42; break;
            case 32: f.Detach(); storage = ItemSystem::StackSource::DetachedReward;
                g_registry.get<ecs::ItemLocation>(source).cell = 5; break;
            case 33: g_registry.remove<ecs::ItemIdentity>(target); break;
            case 34: g_registry.remove<ecs::ItemOwner>(source); break;
            case 35: g_registry.remove<ecs::ItemLocation>(target); break;
            case 36: f.Place(source, INVENTORY, BELT_INVENTORY_SLOT_START); break;
        }
        const auto beforeSource = ItemSystem::GetItemCount(source), beforeTarget = ItemSystem::GetItemCount(target);
        Check(ItemSystem::MergeItemStacksEcs(owner, source, target, amount, storage).transferred == 0,
            "invalid stack merge accepted");
        Check(ItemSystem::GetItemCount(source) == beforeSource && ItemSystem::GetItemCount(target) == beforeTarget &&
            saves == 0 && updates == 0 && destroyAttempts.empty(), "rejected merge changed state");
    }
    for (int scenario = 0; scenario < 21; ++scenario) {
        StackFixture f;
        const auto e = f.material;
        auto& id = g_registry.get<ecs::ItemIdentity>(e);
        switch (scenario) {
            case 0: ++id.vnum; break;
            case 1: ++id.originalVnum; break;
            case 2: ++id.maskVnum; break;
            case 3: ++id.sigVnum; break;
            case 4: ++id.specialGroup; break;
            case 5: ++id.transmutationVnum; break;
            case 6: g_registry.get<ecs::ItemSockets>(e).sockets.back() = 1; break;
            case 7: g_registry.get<ecs::ItemAttributes>(e).attrs.back() = {APPLY_MAX_HP, 100}; break;
            case 8: g_registry.get<ecs::ItemFlags>(e).flags = 0; break;
            case 9: g_registry.get<ecs::ItemProtoRef>(e).anti_flags = ITEM_ANTIFLAG_STACK; break;
            case 10: g_registry.emplace<ecs::ItemLockedAttribute>(e).index = 0; break;
            case 11: g_registry.get<ecs::ItemPrototypeMeta>(e).subType = USE_ABILITY_UP; break;
            case 12: g_registry.remove<ecs::ItemPrototypeMeta>(e); break;
            case 13: g_registry.remove<ecs::ItemProtoRef>(e); break;
            case 14: g_registry.remove<ecs::ItemFlags>(e); break;
            case 15: g_registry.remove<ecs::ItemSockets>(e); break;
            case 16: g_registry.remove<ecs::ItemAttributes>(e); break;
            case 17: g_registry.get<ecs::ItemPrototypeMeta>(e).type = ITEM_DS; break;
            case 18: g_registry.get<ecs::ItemPrototypeMeta>(e).type = ITEM_SPECIAL_DS; break;
            case 19: g_registry.get<ecs::ItemPrototypeMeta>(e).type = ITEM_ELK; break;
            case 20: g_registry.emplace<ecs::ItemExtraProtoRef>(e).proto = reinterpret_cast<TItemExtraProto*>(1); break;
        }
        Check(ItemSystem::MergeItemStacksEcs(f.owner, f.item, e).transferred == 0 &&
            ItemSystem::GetItemCount(f.item) == 40 && ItemSystem::GetItemCount(e) == 180 && saves == 0,
            "incompatible payload merged/lost metadata");
    }
}

void StackMergeCallbacks()
{
    {
        StackFixture f;
        g_registry.get<ecs::ItemCount>(f.item).count = 10;
        rejectDestruction.insert(f.item);
        onDestroy = [&](entt::entity e) {
            Check(e == f.item && ItemSystem::IsItemConsumptionPending(e) && ItemSystem::GetItemCount(e) == 0 &&
                ItemSystem::GetItemCount(f.material) == 190, "retirement preceded complete stack commit");
            Check(!ItemSystem::SetItemCountEcs(e, 10) &&
                ItemSystem::MergeItemStacksEcs(f.owner, e, f.material).transferred == 0, "retired source spent twice");
        };
        const auto result = ItemSystem::MergeItemStacksEcs(f.owner, f.item, f.material);
        Check(result.transferred == 10 && result.sourceDepleted && g_registry.valid(f.item), "cleanup failure hid committed merge");
        Check(ItemSystem::MergeItemStacksEcs(f.owner, f.material, f.item).transferred == 0, "retired target resurrected");
        rejectDestruction.clear();
        ItemSystem::ProcessPendingItemConsumptions();
        Check(!g_registry.valid(f.item) && ItemSystem::GetItemCount(f.material) == 190, "retry credited destination twice");
    }
    {
        StackFixture f;
        const auto third = f.Target(100, 1);
        bool nested = false;
        onSave = [&](entt::entity) {
            if (nested) return;
            nested = true;
            Check(ItemSystem::GetItemCount(f.item) == 20 && ItemSystem::GetItemCount(f.material) == 200,
                "nested merge entered partially committed state");
            Check(ItemSystem::MergeItemStacksEcs(f.owner, f.item, third, 5).transferred == 5, "nested merge rejected");
        };
        Check(ItemSystem::MergeItemStacksEcs(f.owner, f.item, f.material).transferred == 20 &&
            ItemSystem::GetItemCount(f.item) == 15 && ItemSystem::GetItemCount(third) == 105 &&
            ItemSystem::GetItemCount(f.material) == 200, "outer publication restored obsolete counts");
    }
    for (bool removeTarget : {false, true}) {
        StackFixture f;
        entt::entity replacement {entt::null};
        bool removed = false;
        onSave = [&](entt::entity) {
            if (removed) return;
            removed = true;
            const auto victim = removeTarget ? f.material : f.item;
            g_registry.destroy(victim);
            replacement = f.Target(77, removeTarget ? 0 : 5);
            Check(replacement != victim && entt::to_entity(replacement) == entt::to_entity(victim), "index not recycled");
        };
        Check(ItemSystem::MergeItemStacksEcs(f.owner, f.item, f.material).transferred == 20 &&
            ItemSystem::GetItemCount(replacement) == 77, "stale merge touched recycled entity");
    }
    {
        StackFixture f;
        onSave = [&](entt::entity) { throw std::runtime_error("publication failure"); };
        bool threw = false;
        try { ItemSystem::MergeItemStacksEcs(f.owner, f.item, f.material); }
        catch (const std::runtime_error&) { threw = true; }
        Check(threw && ItemSystem::GetItemCount(f.item) == 20 && ItemSystem::GetItemCount(f.material) == 200,
            "publication exception rolled back only one count");
    }
}

void AutomaticStackMerges()
{
    {
        StackFixture f;
        f.Detach();
        g_registry.get<ecs::ItemFlags>(f.item).flags = 0;
        Check(ItemSystem::MergeItemIntoInventoryEcs(f.owner, f.item) == f.item &&
            stackCategoryLookups == 0 && saves == 0 && updates == 0,
            "nonstackable reward entered inventory search");
    }
    for (bool blend : {false, true}) {
        StackFixture f;
        f.Detach();
        if (blend) {
            f.proto.bType = ITEM_BLEND;
            for (auto e : {f.item, f.material}) {
                g_registry.get<ecs::ItemPrototypeMeta>(e).type = ITEM_BLEND;
                g_registry.get<ecs::ItemFlags>(e).flags = 0;
            }
        }
        Check(ItemSystem::MergeItemIntoInventoryEcs(f.owner, f.item) == f.item &&
            ItemSystem::GetItemCount(f.item) == 20 && ItemSystem::GetItemCount(f.material) == 200 &&
            destroyAttempts.empty(), "automatic merge discarded stack-limit remainder");
    }
    {
        StackFixture f;
        f.Detach();
        const auto second = f.Target(180, 1);
        bool nested = false;
        onSave = [&](entt::entity) {
            if (nested) return;
            nested = true;
            g_registry.get<ecs::ItemFlags>(f.item).flags = 0;
            Check(ItemSystem::MergeItemIntoInventoryEcs(f.owner, f.item) == entt::null,
                "reentrant reward delivery accepted");
            g_registry.get<ecs::ItemFlags>(f.item).flags = ITEM_FLAG_STACKABLE;
        };
        Check(ItemSystem::MergeItemIntoInventoryEcs(f.owner, f.item) == second && !g_registry.valid(f.item) &&
            ItemSystem::GetItemCount(f.material) == 200 && ItemSystem::GetItemCount(second) == 200,
            "multi-stack reward distribution or recursive guard failed");
    }
    for (int scenario = 0; scenario < 7; ++scenario) {
        StackFixture f;
        f.Detach();
        const auto second = f.Target(100, 1);
        entt::entity replacement {entt::null};
        bool acted = false;
        onSave = [&](entt::entity) {
            if (acted) return;
            acted = true;
            if (scenario == 0) { g_registry.destroy(second); replacement = f.Target(77, 1); }
            if (scenario == 1) f.Place(second, INVENTORY, 2);
            if (scenario == 2) g_registry.get<ecs::ItemFlags>(second).isLocked = true;
            if (scenario == 3) g_registry.destroy(f.owner);
            if (scenario == 4) f.Place(f.item, INVENTORY, 8);
            if (scenario == 5) { g_registry.destroy(f.item); replacement = f.Target(77, 8); }
            if (scenario == 6) {
                g_registry.get<ecs::ItemLocation>(second).cell = 2;
                inventory[{f.owner, INVENTORY, 2}] = second; // leave a stale old-slot alias
            }
        };
        const auto result = ItemSystem::MergeItemIntoInventoryEcs(f.owner, f.item);
        Check(result == (scenario >= 3 && scenario <= 5 ? entt::entity{entt::null} : f.item),
            "callback change did not stop/skip automatic merge");
        if (scenario == 0 || scenario == 5) Check(ItemSystem::GetItemCount(replacement) == 77, "automatic merge touched replacement");
        else Check(ItemSystem::GetItemCount(second) == 100, "changed candidate still received stack units");
    }
    {
        StackFixture f;
        f.Detach();
        g_registry.get<ecs::ItemFlags>(f.material).isLocked = true;
        const auto second = f.Target(100, 1);
        Check(ItemSystem::MergeItemIntoInventoryEcs(f.owner, f.item) == second &&
            ItemSystem::GetItemCount(f.material) == 180 && ItemSystem::GetItemCount(second) == 140,
            "locked candidate prevented valid fallback");
    }
#ifdef ENABLE_EXTRA_INVENTORY
    {
        StackFixture f;
        f.Detach();
        g_registry.emplace<TestExtraStack>(f.item);
        const auto extra = f.Target(100, 1);
        f.Place(extra, EXTRA_INVENTORY, 1);
        Check(ItemSystem::MergeItemIntoInventoryEcs(f.owner, f.item) == extra &&
            ItemSystem::GetItemCount(f.material) == 180 && ItemSystem::GetItemCount(extra) == 140,
            "extra-inventory reward merged into normal inventory");
    }
#endif
    {
        StackFixture f;
        f.Detach();
        g_registry.get<ecs::ItemCount>(f.item).count = 10;
        onSave = [&](entt::entity e) { g_registry.destroy(e); };
        Check(ItemSystem::MergeItemIntoInventoryEcs(f.owner, f.item) == entt::null && !g_registry.valid(f.item),
            "destroyed destination returned as live reward or source resurrected");
    }
}

void SwitchbotTransactions()
{
#if defined(ENABLE_SWITCHBOT)
    using Result = SwitchbotHelper::Result;
    PaidFixture f;
    const auto stranger = g_registry.create();
    g_registry.emplace<TestPlayer>(stranger);
    Check(SwitchbotHelper::TrySwitch(stranger, f.item, 0).result == Result::InvalidTarget, "switchbot accepted foreign target");
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 1).result == Result::InvalidTarget, "switchbot accepted wrong slot");
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, SWITCHBOT_SLOT_COUNT).result == Result::InvalidTarget, "switchbot slot overflow");
    inventory.erase({f.owner, SWITCHBOT, 0});
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::InvalidTarget, "switchbot accepted detached target");
    f.Place(f.item, SWITCHBOT, 0);
    g_map_itemAttr.clear();
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::RollFailed, "switchbot ignored failed reroll");
    Check(payments == 0 && ItemSystem::GetItemCount(f.material) == 2, "switchbot consumed item for failed roll");
    f.Rows(5);
    const auto outcome = SwitchbotHelper::TrySwitch(f.owner, f.item, 0);
    Check(outcome.result == Result::Success && outcome.materialVnum == c_arSwitchingItems[0], "switchbot transaction failed");
    Check(payments == 1 && saves == 1 && updates == 1, "switchbot charged or committed more than once");
    f.Watch();
    g_registry.get<ecs::ItemCount>(f.material).count = 0;
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::NoPayment && payments == 0, "empty material stack accepted");

    const auto limited = f.Material(71151, 2, 1);
    f.proto.aLimits[0] = {LIMIT_LEVEL, 31};
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::NoPayment, "level-limited changer used above level 30");
    f.proto.aLimits[0].lValue = 30;
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::Success && ItemSystem::GetItemCount(limited) == 1,
        "level-30 limited changer rejected");
    f.Watch();
#ifdef DISABLE_ZODIAC_ATT
    g_registry.get<ecs::ItemIdentity>(f.item).vnum = 12314141;
#else
    g_registry.get<ecs::ItemIdentity>(f.item).vnum = 300;
#endif
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::NoPayment, "zodiac accepted normal changer");
    const auto zodiac = f.Material(86060, 1, 2);
    const auto zodiacOutcome = SwitchbotHelper::TrySwitch(f.owner, f.item, 0);
    Check(zodiacOutcome.result == Result::Success && zodiacOutcome.materialVnum == 86060 && !g_registry.valid(zodiac),
        "zodiac payment or destroyed-material ID lost");
    f.Watch();
    g_registry.destroy(f.item);
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::InvalidTarget && payments == 0, "stale switchbot entity accepted");
#endif
}

void RuneInitializationAndBoundaries()
{
#ifdef ENABLE_RUNE_SYSTEM
    for (int subtype = RUNE_SLOT1; subtype <= RUNE_SLOT7; ++subtype) {
        for (int duration : {100, 199, 10000, INT_MAX}) {
            Fixture f;
            f.proto.bType = ITEM_COSTUME; f.proto.bSubType = static_cast<uint8_t>(subtype);
            f.proto.alValues[0] = duration;
            g_registry.emplace<ecs::ItemSockets>(f.item);
            f.Attrs()[3] = {APPLY_MAX_SP, 23};
            for (int remaining : {INT_MIN, -1, 0, 5, 6, 10, 11, 20, 21, 40, 41, 60, 61, 80, 81, 100, INT_MAX}) {
                // Input is seconds, preserving the original integer-percent rounding.
                const int percent = remaining / (duration / 100);
                const int tier = percent >= 81 ? 7 : percent >= 61 ? 6 : percent >= 41 ? 5 :
                    percent >= 21 ? 4 : percent >= 11 ? 3 : percent >= 6 ? 2 : 1;
                g_registry.get<ecs::ItemSockets>(f.item).sockets[0] = remaining;
                Check(ItemSystem::InitializeRuneItem(f.item), "valid rune initialization rejected");
                for (int slot = 0; slot < RUNE_ATTR_EACH; ++slot) {
                    const auto& row = aApplyRuneInfo[(subtype - RUNE_SLOT1) * RUNE_ATTR_EACH + slot];
                    Check(ItemSystem::GetRuneAttributeType(f.item, slot) == row[0], "rune subtype/type mapping changed");
                    Check(ItemSystem::GetRuneAttributeValue(f.item, slot, remaining) == row[tier], "rune duration tier changed");
                    Check(f.Attrs()[slot].bType == row[0] && f.Attrs()[slot].sValue == row[tier], "rune initialization lost attributes");
                }
                Check(f.Attrs()[3].bType == APPLY_MAX_SP && f.Attrs()[3].sValue == 23, "rune overwrote unrelated attribute");
            }
            Check(saves == 0 && updates == 0 && randomCalls == 0, "rune setup published partial item");
        }
    }
    for (int duration : {INT_MIN, -100, -1, 0, 1, 99}) {
        Fixture f; f.proto.bType = ITEM_COSTUME; f.proto.bSubType = RUNE_SLOT1; f.proto.alValues[0] = duration;
        g_registry.emplace<ecs::ItemSockets>(f.item);
        f.Attrs()[0] = {APPLY_MAX_HP, 17}; const auto before = f.Attrs();
        Check(ItemSystem::GetRuneAttributeValue(f.item, 0, INT_MAX) == 0, "invalid rune duration divided by zero");
        Check(!ItemSystem::InitializeRuneItem(f.item) && EqualAttributes(f.Attrs(), before), "invalid rune modified attributes");
    }
    {
        Fixture f; g_registry.emplace<ecs::ItemSockets>(f.item);
        Check(ItemSystem::InitializeRuneItem(f.item), "ordinary item rejected by rune initializer");
        Check(ItemSystem::GetRuneAttributeType(f.item, 0) == 0 && ItemSystem::GetRuneAttributeValue(f.item, 0, 100) == 0,
            "non-rune acquired rune bonus");
        f.proto.bType = ITEM_USE; f.proto.bSubType = USE_RUNE_PERC_CHARGE; f.proto.alValues[0] = 30;
        Check(ItemSystem::InitializeRuneItem(f.item) && g_registry.get<ecs::ItemSockets>(f.item).sockets[0] == 30,
            "rune charge potion lost payload");
        f.proto.bType = ITEM_COSTUME; f.proto.bSubType = RUNE_SLOT7; f.proto.alValues[0] = 100;
        for (int index : {INT_MIN, -1, 2, INT_MAX})
            Check(ItemSystem::GetRuneAttributeType(f.item, index) == 0 && ItemSystem::GetRuneAttributeValue(f.item, index, 100) == 0,
                "rune slot bounds were not checked");
        g_registry.remove<ecs::ItemSockets>(f.item);
        Check(!ItemSystem::InitializeRuneItem(f.item) && !g_registry.any_of<ecs::ItemSockets>(f.item), "rune setup fabricated missing sockets");
        g_registry.emplace<ecs::ItemSockets>(f.item); g_registry.remove<ecs::ItemAttributes>(f.item);
        Check(!ItemSystem::InitializeRuneItem(f.item) && !g_registry.any_of<ecs::ItemAttributes>(f.item), "rune setup fabricated missing attributes");
        g_registry.destroy(f.item); const auto replacement = g_registry.create();
        Check(!ItemSystem::InitializeRuneItem(f.item) && !ItemSystem::InitializeRuneItem(entt::null) &&
            ItemSystem::GetRuneAttributeValue(f.item, 0, 100) == 0 && g_registry.valid(replacement), "rune setup accepted stale entity");
    }
#endif
}
void SwitchbotMaterialSelection()
{
#ifdef ENABLE_SWITCHBOT
    using Result = SwitchbotHelper::Result;
    PaidFixture f;
    g_registry.emplace<ecs::ItemFlags>(f.material).isLocked = true;
    const auto fallback = f.Material(c_arSwitchingItems[0], 1, 1);
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::Success, "locked first stack blocked valid fallback");
    Check(ItemSystem::GetItemCount(f.material) == 2 && !g_registry.valid(fallback), "wrong material stack consumed");
#ifdef ENABLE_EXTRA_INVENTORY
    g_registry.get<ecs::ItemFlags>(f.material).isLocked = false;
    f.Place(f.material, EXTRA_INVENTORY, 5);
    f.Watch();
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::Success && ItemSystem::GetItemCount(f.material) == 1,
        "extra-inventory material was not consumed");
#endif
#endif
}
}

int main()
{
    try {
        ITEM_MANAGER itemManager;
        LogManager logManager;
        RuneInitializationAndBoundaries();
        EntityAndTableValidation();
        LockedSlotAndRarePreservation();
        FailureIsAtomic();
        RareHolesAndFailure();
        AddonRemovalAndDuplicates();
        WeightedBoundaries();
        ProtoAndCostumeRules();
        PaidRerollTransactions();
        PaymentValidation();
        GoldTransactions();
        CostumeResetTransactions();
        CostumeSelectionValidation();
        CostumeAdditionTransactions();
        CostumeRemovalTransactions();
        CostumeOperationValidation();
        StoleEnchantTransactions();
        AttributeLockTransactions();
        AttributeLockEdgeCases();
        DragonSoulSampling();
        DragonSoulPreparation();
        DragonSoulTransactions();
        DragonSoulEquipmentRules();
        ExtractionInputGuards();
        ExtractionTableValidation();
        HeartExtractionTransactions();
        PullOutTransactions();
        ExtractionCallbacksAndCostPolicy();
        SoulStateValidation(); SoulStateTransitions(); SoulStateCallbacks();
        SwitchbotTransactions();
        SwitchbotMaterialSelection();
        TransferWindowAndCommands();
        TransferContextGuards();
        TransferItemGuards();
        TransferPaymentAndCommit();
        BatchCostValidation();
        BatchReentrancyAndRetirement();
        CountValidationAndLimits(); CountDestructionAndCallbacks();
        StackMergeBoundaries(); StackMergeGuards(); StackMergeCallbacks(); AutomaticStackMerges();
        std::cout << "Item attribute regression checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
