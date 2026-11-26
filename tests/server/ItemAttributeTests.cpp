#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/social/new_switchbot.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/spatial_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/item_proto_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/inventory_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/item/attr_transfer.h"
#include "../../SRC/Server/GameServer/item/TypedCostumeTransfer.hpp"
#include "../../SRC/Server/GameServer/ecs/detail/ItemAttributeRules.hpp"
#include "../../SRC/Server/GameServer/core/constants.h"
#include "../../SRC/Server/GameServer/core/log.h"
#include "../../SRC/Server/GameServer/entity/char.h"
#include "../../SRC/Server/GameServer/entity/char_manager.h"
#include "../../SRC/Server/GameServer/network/desc.h"
#include "../../SRC/Server/GameServer/core/buffer_manager.h"
#include "../../SRC/Server/GameServer/network/p2p.h"
#include "../../SRC/Server/common/stole_length.h"
#include "../../SRC/Server/common/rune_length.h"
#include "../../SRC/Server/GameServer/item/item_manager.h"
#include <Core/Logging.hpp>
#include <stdexcept>
#include <iostream>
#include <cstdlib>

entt::registry g_registry;
TItemAttrMap g_map_itemAttr;
TItemAttrMap g_map_itemRare;
TItemAttrMap g_map_itemRare8;
int g_iDbLogLevel = 0;
int g_bItemCountLimit = 200;
const int aiItemMagicAttributePercentHigh[ITEM_ATTRIBUTE_MAX_LEVEL] = {0, 0, 0, 0, 100};
const int aiItemMagicAttributePercentLow[ITEM_ATTRIBUTE_MAX_LEVEL] = {100, 0, 0, 0, 0};

namespace {
int updates = 0;
int randomCalls = 0;
int randomOffset = 0;
float gaussianValue = 4.f;
int checks = 0;
int payments = 0;
bool rejectPayment = false;
bool rejectGoldPayment = false;
bool transferTest = false;
bool countStateTest = false;
bool runeTest = false;
std::map<entt::entity, std::array<int, RUNE_ATTR_EACH>> runePoints;
std::vector<std::pair<entt::entity, bool>> runePointCalls;
std::map<std::pair<entt::entity, uint32_t>, CAffect> runeAffects;
std::vector<uint32_t> runeMessages;
std::function<void(entt::entity, bool)> onRunePoints;
std::function<void(entt::entity)> onRuneAffect, onRuneChat;
bool rejectRuneAffect = false;
bool runeChargeTest = false;
int runeEmptyCell = 1;
int stackCategoryLookups = 0;
int wearTimerStarts = 0;
bool rejectWearTimer = false;
std::set<entt::entity> wearTimers;
std::function<void(entt::entity)> onWearTimerStart;
bool rejectCreation = false, rejectPlace = false;
bool allowHandling = true;
std::vector<entt::entity> createdOutputs;
std::function<void(entt::entity)> onCreate, onPlace;
TItemTable outputProto {};

int rejectPaymentAt = 0, transferLogs = 0;
std::vector<std::string> transferCommands;
std::function<void()> onPayment;
std::function<void(entt::entity)> onUpdate, onDestroy;
std::vector<entt::entity> publishedCounts;
std::vector<entt::entity> destroyAttempts;
std::set<entt::entity> rejectDestruction;
struct TransferActor { int32_t x = 0, y = 0, map = 1; entt::entity npc {entt::null}; };
entt::entity watchedItem = entt::null;
std::array<TPlayerItemAttribute, ITEM_ATTRIBUTE_MAX_NUM> beforePayment{};
short lockBeforePayment = -1;
std::map<std::tuple<entt::entity, uint8_t, uint16_t>, entt::entity> inventory;
struct TestPlayer { int64_t gold = 100; };
struct TestExtraStack {};
float floatDrawFraction = 0.f;
int floatRandomCalls = 0;
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
    Check(updates == 0, "partial attributes published before payment");
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
    if (!transferTest && !countStateTest && !runeTest) UnexpectedSwitchbotService();
    static auto logger = std::make_shared<spdlog::logger>("transfer-test", spdlog::sinks_init_list{});
    return logger;
}
void intrusive_ptr_add_ref(EVENT*) { UnexpectedSwitchbotService(); }
void intrusive_ptr_release(EVENT*) { UnexpectedSwitchbotService(); }
LPEVENT event_create_ex(TEVENTFUNC, event_info_data*, int32_t) { UnexpectedSwitchbotService(); }
void event_cancel(LPEVENT*) { UnexpectedSwitchbotService(); }
#ifdef TEXTS_IMPROVEMENT
void ecs::ChatSystem::SendNew(entt::entity owner, uint8_t, uint32_t message, const char*, ...) {
    if (runeTest) {
        Check(ecs::PlayerRuntime::IsPC(owner), "rune message used a stale owner");
        runeMessages.push_back(message);
        if (onRuneChat) onRuneChat(owner);
    } else if (!transferTest) UnexpectedSwitchbotService();
}
#endif
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity) { if (!transferTest) UnexpectedSwitchbotService(); return 42; }
std::string_view ecs::PlayerRuntime::GetName(entt::entity) { UnexpectedSwitchbotService(); }
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
    Check((runeTest) && ItemSystem::IsValidItem(item), "stale item name");
    return "rune-test";
}
const char* ItemSystem::GetItemName(entt::entity item, uint8_t) {
    Check(runeTest && ItemSystem::IsValidItem(item), "stale localized rune name");
    return "rune-test";
}
uint8_t ItemSystem::GetItemSize(entt::entity item) {
    Check((runeChargeTest) && ItemSystem::IsValidItem(item), "stale item size");
    return ItemSystem::GetItemProto(item)->bSize;
}
entt::entity ItemSystem::GetItemOwnerEntity(entt::entity) { UnexpectedSwitchbotService(); }
int ItemSystem::GetItemAttributeType(entt::entity, int) { UnexpectedSwitchbotService(); }
double ItemSystem::GetItemAttributeValue(entt::entity, int) { UnexpectedSwitchbotService(); }

// Unused services must never be entered by these headless tests.
int MIN(int a, int b) { return std::min(a, b); }
time_t get_global_time() { UnexpectedSwitchbotService(); }
ITEM_MANAGER::ITEM_MANAGER() {}
ITEM_MANAGER::~ITEM_MANAGER() {}
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() : m_queueLane(0) {}
CAsyncSQL::~CAsyncSQL() = default;
LogManager::LogManager() : m_bIsConnect(false) {}
LogManager::~LogManager() = default;
void LogManager::ItemLog(entt::entity owner, int, int, const char*, const char*) { UnexpectedSwitchbotService(); }
entt::entity ITEM_MANAGER::CreateItem(uint32_t vnum, uint32_t count, uint32_t, bool, int, bool) {
    if (!runeChargeTest) UnexpectedSwitchbotService();
    if (rejectCreation) return entt::null;
    const auto item = g_registry.create();
    g_registry.emplace<ecs::ItemIdentity>(item, ecs::ItemIdentity{500, vnum, vnum});
    g_registry.emplace<ecs::ItemCount>(item, ecs::ItemCount{static_cast<int>(count)});
    g_registry.emplace<ecs::ItemProtoRef>(item).proto = &outputProto;
    g_registry.emplace<ecs::ItemLocation>(item, ecs::ItemLocation{RESERVED_WINDOW, 0});
    if (runeChargeTest) {
        g_registry.emplace<ecs::ItemOwner>(item);
        g_registry.emplace<ecs::ItemSockets>(item);
        g_registry.emplace<ecs::ItemAttributes>(item);
    }
    createdOutputs.push_back(item);
    if (onCreate) onCreate(item);
    return item;
}
entt::entity ItemSystem::GetWearItem(entt::entity owner, uint8_t wear) {
    if (!runeTest) UnexpectedSwitchbotService();
    return ItemSystem::GetItem(owner, TItemPos(EQUIPMENT, INVENTORY_MAX_NUM + wear));
}
void ItemSystem::AutoGiveItem(entt::entity owner, entt::entity item, bool
#ifdef __HIGHLIGHT_SYSTEM__
    , bool
#endif
) { UnexpectedSwitchbotService(); }
entt::entity ItemSystem::AutoGiveItemEcs(entt::entity, uint32_t, uint32_t, int, bool, bool) { UnexpectedSwitchbotService(); }
int ItemSystem::GetItemLimitTimerBasedOnWearIndex(entt::entity item) { UnexpectedSwitchbotService(); }
int ItemSystem::GetItemDuration(entt::entity) { return 0; }
bool ItemSystem::DestroyItemEntityEcs(entt::entity item, const char*)
{
    const bool retired = ItemSystem::IsItemConsumptionPending(item) && ItemSystem::GetItemCount(item) == 0;
    Check(countStateTest || ((transferTest || runeChargeTest) && (retired ||
        ((runeChargeTest) && ItemSystem::GetItemOwner(item) == entt::null))),
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
bool ItemSystem::SetItemSocketEcs(entt::entity item, int index, uint32_t value) { UnexpectedSwitchbotService(); }
bool ItemSystem::CopyItemAttributesEcs(entt::entity, entt::entity) { UnexpectedSwitchbotService(); }
bool ItemSystem::PlaceItemEcs(entt::entity owner, entt::entity item, uint8_t window, uint16_t cell) { UnexpectedSwitchbotService(); }
bool ItemSystem::RemoveItemEcs(entt::entity item) { UnexpectedSwitchbotService(); }
bool InventorySystem::CanHandleItems(entt::entity owner, bool, bool) {
    if (!runeChargeTest) UnexpectedSwitchbotService();
    return allowHandling && ecs::PlayerRuntime::IsPC(owner);
}
int InventorySystem::GetEmptyInventory(entt::entity owner, uint8_t size) {
    Check(runeChargeTest && ecs::PlayerRuntime::IsPC(owner) && size > 0, "invalid rune inventory query");
    return runeEmptyCell;
}
bool InventorySystem::SplitItemWithCommit(entt::entity owner, entt::entity source, int count,
    TItemPos destination, const std::function<bool(entt::entity)>& commit)
{
    Check(runeChargeTest && count == 1 && destination.window_type == INVENTORY, "unexpected rune split request");
    const auto output = ITEM_MANAGER::instance().CreateItem(ItemSystem::GetItemVnum(source), count);
    if (!ItemSystem::IsValidItem(output)) return false;
    bool committed = false;
    struct Cleanup {
        entt::entity item;
        bool& committed;
        ~Cleanup() {
            if (!committed && ItemSystem::IsValidItem(item) && ItemSystem::GetItemOwner(item) == entt::null)
                ItemSystem::DestroyItemEntityEcs(item, "SPLIT_ABORT");
        }
    } cleanup {output, committed};
    if (rejectPlace || !ItemSystem::IsValidItem(source) || !ecs::PlayerRuntime::IsPC(owner) ||
        ItemSystem::GetItemOwner(output) != entt::null || ItemSystem::GetItem(owner, destination) != entt::null)
        return false;
    g_registry.get<ecs::ItemSockets>(output) = g_registry.get<ecs::ItemSockets>(source);
    g_registry.get<ecs::ItemAttributes>(output) = g_registry.get<ecs::ItemAttributes>(source);
    if (!commit(output)) return false;
    --g_registry.get<ecs::ItemCount>(source).count;
    g_registry.get<ecs::ItemOwner>(output).owner = owner;
    g_registry.get<ecs::ItemLocation>(output) = {destination.window_type, destination.cell};
    inventory[{owner, destination.window_type, static_cast<uint16_t>(destination.cell)}] = output;
    committed = true;
    if (onPlace) onPlace(output);
    return true;
}
bool InventorySystem::CanUnequipNow(entt::entity, entt::entity item, bool) { UnexpectedSwitchbotService(); }
bool InventorySystem::IsEmptyItemGrid(entt::entity owner, TItemPos cell, uint8_t size, int) { UnexpectedSwitchbotService(); }
bool InventorySystem::EquipTo(entt::entity item, entt::entity owner, uint8_t wear) { UnexpectedSwitchbotService(); }
bool ItemSystem::ModifyItemPointsEcs(entt::entity item, bool add) { UnexpectedSwitchbotService(); }



float fnumber(float low, float high)
{
    Check(std::isfinite(low) && std::isfinite(high) && low <= high, "invalid floating RNG interval");
    ++floatRandomCalls;
    return low + (high - low) * floatDrawFraction;
}
bool MakeDistinctRandomNumberSet(std::list<float>, std::vector<int>&);

// Deterministic I/O doubles. The system under test never constructs CItem or
// CHARACTER; item fixtures contain only native components.
int number_ex(int low, int high, const char*, int)
{
    Check(low <= high, "invalid random interval");
    ++randomCalls;
    Check(randomOffset >= 0 && randomOffset <= high - low, "scripted random offset out of range");
    return low + randomOffset;
}
float gauss_random(float, float) { return gaussianValue; }
void LogManager::ItemLog(uint32_t, uint32_t, uint32_t, uint32_t, const char*, const char*, const char*, uint32_t) {}
void LogManager::ItemLogEntity(entt::entity owner, entt::entity item, const char*, const char*) { UnexpectedSwitchbotService(); }
LPDESC ecs::PlayerRuntime::GetDesc(entt::entity) { return nullptr; }
bool ecs::PlayerRuntime::IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e); }
bool ecs::PlayerRuntime::IsPC(entt::entity e) { return IsValid(e) && g_registry.all_of<TestPlayer>(e); }
namespace ShopSystem {
bool IsSellingItem(entt::entity, uint32_t) { return false; }
}
entt::entity ecs::SocialSystem::GetMyShop(entt::entity e)
{
    const auto* shop = g_registry.try_get<ecs::ShopState>(e);
    return shop ? shop->myShop : entt::null;
}
entt::entity ecs::SocialSystem::GetShop(entt::entity e)
{
    const auto* shop = g_registry.try_get<ecs::ShopState>(e);
    return shop ? shop->currentShop : entt::null;
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
    Check(transferTest && pid == 42 && updates == 1, "transfer logged before complete publish");
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
    if (runeTest) Check(ItemSystem::IsValidItem(item), "rune update used a stale entity");
    if (!transferTest || item == watchedItem) ++updates;
    else publishedCounts.push_back(item);
    if (onUpdate) onUpdate(item);
}

void ItemSystem::ModifyPoints(entt::entity item, bool add)
{
    Check(runeTest && ItemSystem::IsRuneItem(item) &&
        ecs::PlayerRuntime::IsPC(ItemSystem::GetItemOwner(item)), "rune points used a stale item/owner");
    const auto attrs = g_registry.get<ecs::ItemAttributes>(item).attrs;
    for (int index = 0; index < RUNE_ATTR_EACH; ++index)
        runePoints[item][index] += (add ? 1 : -1) * attrs[index].sValue;
    runePointCalls.emplace_back(item, add);
    if (onRunePoints) onRunePoints(item, add);
}

namespace AffectSystem {
CAffect* FindAffect(entt::entity owner, uint32_t type, uint8_t)
{
    Check(runeTest && ecs::PlayerRuntime::IsPC(owner), "rune affect lookup used a stale owner");
    const auto found = runeAffects.find({owner, type});
    return found == runeAffects.end() ? nullptr : &found->second;
}
bool AddAffect(entt::entity owner, uint32_t type, uint8_t apply, double value,
    uint32_t flag, int32_t duration, int32_t cost, bool overwrite, bool)
{
    Check(runeTest && ecs::PlayerRuntime::IsPC(owner), "rune affect add used a stale owner");
    Check(apply == APPLY_NONE && value == 0 && flag == 0 && duration == INFINITE_AFFECT_DURATION &&
        cost == 0 && !overwrite, "rune visual affect parameters changed");
    if (rejectRuneAffect) return false;
    runeAffects[{owner, type}] = {};
    if (onRuneAffect) onRuneAffect(owner);
    return true;
}
bool RemoveAffect(entt::entity owner, uint32_t type)
{
    Check(runeTest && ecs::PlayerRuntime::IsPC(owner), "rune affect remove used a stale owner");
    const bool removed = runeAffects.erase({owner, type}) != 0;
    if (onRuneAffect) onRuneAffect(owner);
    return removed;
}
}

// This fixture exercises the in-memory attribute path without a typed item session.
bool OpenTypedCostumeTransfer(entt::entity) { return false; }
bool CloseTypedCostumeTransfer(entt::entity) { return false; }

namespace { struct TestGroundReservation { entt::entity owner {entt::null}; }; }
namespace ItemSystem {
bool IsOwnership(entt::entity item, entt::entity owner) {
    if (!IsValidItem(item) || !ecs::PlayerRuntime::IsPC(owner)) return false;
    const auto* reservation = g_registry.try_get<TestGroundReservation>(item);
    return !reservation || reservation->owner == owner;
}
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
    const uint16_t cell = pos.cell;
    const auto found = inventory.find({owner, pos.window_type, cell});
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
    Check(!transferTest, "batch fell back to sequential consumption");
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
TPlayerItemAttribute GetItemAttribute(entt::entity item, int index)
{
    return g_registry.get<ecs::ItemAttributes>(item).attrs[index];
}
void SetItemAttributes(entt::entity item, const TPlayerItemAttribute* attrs)
{
    auto& component = g_registry.get<ecs::ItemAttributes>(item);
    std::copy_n(attrs, ITEM_ATTRIBUTE_MAX_NUM, component.attrs.begin());
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
        updates = randomCalls = 0;
        randomOffset = 0;
        gaussianValue = 4.f;
        floatDrawFraction = 0.f;
        floatRandomCalls = 0;
        payments = 0;
        rejectPayment = rejectGoldPayment = false;
        transferTest = countStateTest = runeTest = runeChargeTest = false;
        runeEmptyCell = 1;
        runePoints.clear(); runePointCalls.clear(); runeAffects.clear(); runeMessages.clear();
        onRunePoints = {}; onRuneAffect = onRuneChat = {}; rejectRuneAffect = false;
        g_bItemCountLimit = 200;
        stackCategoryLookups = 0;
        onWearTimerStart = {};
        onCreate = onPlace = {};
        rejectPaymentAt = transferLogs = 0;
        transferCommands.clear();
        onPayment = {};
        onUpdate = onDestroy = {};
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
    }
    auto& Attrs() { return g_registry.get<ecs::ItemAttributes>(item).attrs; }
    void Rows(int count, bool rare = false)
    {
        for (int i = 1; i <= count; ++i) {
            TItemAttrTable row;
            row.dwApplyIndex = i;
            row.dwProb = 1;
            std::fill(std::begin(row.bMaxLevelBySet), std::end(row.bMaxLevelBySet), 5);
            for (size_t level = 0; level < (sizeof(row.lValues) / sizeof(row.lValues[0])); ++level)
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
        updates = payments = 0;
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
            Attrs()[i] = {static_cast<uint8_t>(20 + i), static_cast<double>(200 + i)};
        Watch();
    }
    void Unchanged()
    {
        Check(EqualAttributes(Attrs(), beforePayment) && updates == 0 &&
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
    Check(randomCalls == 0 && updates == 0, "invalid item caused side effects");
    f.proto.bSubType = WEAPON_SWORD;
    Check(!ItemSystem::AddItemAttributeEcs(f.item), "empty table accepted");
    Check(g_map_itemAttr.empty(), "read inserted rows into the attribute table");
    f.Rows(1);
    g_map_itemAttr.begin()->second.dwProb = 0;
    Check(!ItemSystem::AddItemAttributeEcs(f.item), "zero-weight table accepted");
    Check(updates == 0, "failed roll published an item update");
    g_registry.destroy(f.item);
    Check(!ItemSystem::ChangeItemAttributeEcs(f.item), "stale entity accepted");
}

void LockedSlotAndRarePreservation()
{
    Fixture f;
    f.Rows(10);
    for (int i = 0; i < 5; ++i)
        f.Attrs()[i] = {static_cast<uint8_t>(i + 1), static_cast<double>(100 + i)};
    f.Attrs()[5] = {40, 70};
    f.Attrs()[6] = {41, 80};
    g_registry.emplace<ecs::ItemLockedAttribute>(f.item, ecs::ItemLockedAttribute{2});
    Check(ItemSystem::ChangeItemAttributeEcs(f.item), "locked normal reroll failed");
    Check(f.Attrs()[2].bType == 3 && f.Attrs()[2].sValue == 102, "locked slot changed");
    Check(ecs::item_attributes::Count(f.Attrs(), 0, 5) == 5, "reroll lost a normal bonus");
    Check(f.Attrs()[5].bType == 40 && f.Attrs()[6].bType == 41, "normal reroll touched rare slots");
    Check(updates == 1, "reroll must publish once after success");
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
    Check(updates == 0, "failed reroll published partial state");
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
    updates = 0;
    Check(!ItemSystem::ChangeItemRareAttributeEcs(f.item), "rare reroll accepted too few types");
    Check(f.Attrs()[5].bType == before[5].bType && f.Attrs()[6].sValue == before[6].sValue,
        "failed rare reroll lost bonuses");
    Check(updates == 0, "failed rare reroll published state");
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
    updates = 0;
    Check(!ItemSystem::SetItemForceAttributeEcs(f.item, -1, 1, 10), "negative force-attribute index accepted");
    Check(!ItemSystem::SetItemForceAttributeEcs(f.item, ITEM_ATTRIBUTE_MAX_NUM, 1, 10), "force-attribute index overflow");
    Check(updates == 0, "invalid force write caused side effects");
    Check(ItemSystem::SetItemForceAttributeEcs(f.item, 6, 2, -30), "signed force attribute failed");
    Check(f.Attrs()[6].sValue == -30 && updates == 1, "force write did not update the client");
    Check(ItemSystem::SetItemForceAttributeEcs(f.item, 6, 2, 1.113) && f.Attrs()[6].sValue == 1.113 && updates == 2, "fractional attribute was not committed and published");
    Check(!ItemSystem::SetItemForceAttributeEcs(f.item, 6, 2, std::numeric_limits<double>::quiet_NaN()) && updates == 2, "non-finite attribute caused side effects");
}

void HitDamageScrollTransactions()
{
#ifdef ENABLE_CHANGE_NORMAL_HIT_RAZOR93
    const auto setup = [](AttributeItemFixture& f) {
        transferTest = true;
        g_registry.emplace<ecs::ItemCount>(f.item).count = 1;
        g_registry.get<ecs::ItemIdentity>(f.material).vnum = 70251;
        f.Attrs()[0] = {APPLY_NORMAL_HIT_DAMAGE_BONUS, 1};
        f.Attrs()[1] = {APPLY_SKILL_DAMAGE_BONUS, 2};
        f.Watch();
    };
    {
        AttributeItemFixture f(USE_CHANGE_ATTRIBUTE);
        setup(f);
        const auto before = f.Attrs();
        Check(ItemSystem::ChangeItemHitDamageBonuses(f.owner, f.item, f.material), "native hit reroll failed");
        Check(f.Attrs()[0].sValue == 9 && f.Attrs()[1].sValue == 4, "hit/skill distribution changed");
        for (int i = 2; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
            Check(f.Attrs()[i].bType == before[i].bType && f.Attrs()[i].sValue == before[i].sValue,
                "hit reroll overwrote an unrelated or rare attribute");
        Check(ItemSystem::GetItemCount(f.material) == 1, "hit reroll did not charge exactly one scroll");
    }
    {
        AttributeItemFixture f(USE_CHANGE_ATTRIBUTE);
        setup(f);
        gaussianValue = 30.f;
        Check(ItemSystem::ChangeItemHitDamageBonuses(f.owner, f.item, f.material),
            "negative hit bonus reroll failed");
        Check(f.Attrs()[0].sValue == -59 && f.Attrs()[1].sValue == 30,
            "negative hit bonus narrowed to an unsigned attribute level");
    }
    for (int mode = 0; mode < 8; ++mode)
    {
        AttributeItemFixture f(USE_CHANGE_ATTRIBUTE);
        setup(f);
        switch (mode) {
        case 0: f.Attrs()[0].bType = 20; f.Attrs()[1].bType = 21; break;
        case 1: g_registry.emplace<ecs::ItemLockedAttribute>(f.item).index = 0; break;
        case 2: g_registry.emplace<ecs::ItemFlags>(f.material).isLocked = true; break;
        case 3: g_registry.get<ecs::ItemOwner>(f.material).owner = entt::null; break;
        case 4: g_registry.get<ecs::ItemIdentity>(f.material).vnum = 1; break;
        case 5: f.Attrs()[2].bType = APPLY_SKILL_DAMAGE_BONUS; break;
        case 6: g_registry.emplace<ecs::ItemEquipped>(f.item).equipped = true; break;
        case 7:
            f.Attrs()[0] = {}; f.Attrs()[1] = {};
            g_registry.emplace<ecs::ItemLockedAttribute>(f.item).index = 0;
            break;
        }
        f.Watch();
        Check(!ItemSystem::ChangeItemHitDamageBonuses(f.owner, f.item, f.material), "invalid hit reroll accepted");
        f.Unchanged();
        Check(randomCalls == 0, "rejected hit reroll used randomness");
    }
    {
        AttributeItemFixture f(USE_CHANGE_ATTRIBUTE);
        setup(f);
        f.Attrs()[0] = {}; f.Attrs()[1] = {};
        Check(ItemSystem::ChangeItemHitDamageBonuses(f.owner, f.item, f.material),
            "empty normal slots could not accept hit bonuses");
        Check(f.Attrs()[0].bType == APPLY_NORMAL_HIT_DAMAGE_BONUS &&
            f.Attrs()[1].bType == APPLY_SKILL_DAMAGE_BONUS, "hit bonus types lost");
    }
    {
        AttributeItemFixture f(USE_CHANGE_ATTRIBUTE);
        setup(f);
        g_registry.get<ecs::ItemCount>(f.material).count = 1;
        bool observed = false;
        onUpdate = [&](entt::entity item) {
            if (item != f.item) return;
            observed = true;
            Check(ItemSystem::GetItemCount(f.material) == 0 &&
                ItemSystem::IsItemConsumptionPending(f.material), "attributes published before the scroll debit");
            Check(!ItemSystem::ChangeItemHitDamageBonuses(f.owner, f.item, f.material),
                "callback reused the last scroll");
            g_registry.destroy(item);
        };
        Check(ItemSystem::ChangeItemHitDamageBonuses(f.owner, f.item, f.material) && observed,
            "committed hit reroll failed after callback destruction");
        Check(!g_registry.valid(f.item) && !g_registry.valid(f.material), "retired hit reroll entities survived");
    }
#endif
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
    Check(payments == 1 && updates == 0 && EqualAttributes(f.Attrs(), beforePayment),
        "failed payment published a reroll");
    rejectPayment = false;
    f.Watch();
    Check(ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "paid reroll failed");
    Check(payments == 1 && ItemSystem::GetItemCount(f.material) == 1 && updates == 1,
        "paid reroll did not charge and publish exactly once");
    f.Watch();
    Check(ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "last material failed");
    Check(!g_registry.valid(f.material) && g_registry.valid(f.item) && updates == 1,
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
    Check(payments == 0 && updates == 0, "invalid payment caused side effects");
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
    Check(updates == 0 && EqualAttributes(f.Attrs(), beforePayment), "rejected debit changed attributes");
    rejectGoldPayment = false;
    f.Watch();
    Check(ItemSystem::ChangeItemAttributeWithGoldCost(f.item, 100), "exact yang payment failed");
    Check(ecs::PointSystem::GetGold(f.owner) == 0 && payments == 1 && updates == 1,
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
    Check(EqualAttributes(f.Attrs(), beforePayment) && updates == 0 && payments == 0,
        "failed reset cleared original bonuses or consumed reset item");
    f.Rows(5);
    rejectPayment = true;
    Check(!ItemSystem::ResetCostumeAttributesWithItemCost(f.item, f.material), "unpaid costume reset accepted");
    Check(EqualAttributes(f.Attrs(), beforePayment) && updates == 0,
        "rejected reset payment changed attributes");
    rejectPayment = false;
    f.Watch();
    g_registry.get<ecs::ItemCount>(f.material).count = 1;
    Check(ItemSystem::ResetCostumeAttributesWithItemCost(f.item, f.material), "costume reset failed");
    Check(ItemSystem::GetItemAttributeCount(f.item) == 3 && f.Attrs()[0].sValue == 19 && f.Attrs()[1].sValue == 15,
        "costume reset changed three-bonus level rules");
    Check(f.Attrs()[5].sValue == 400 && f.Attrs()[6].sValue == 500 && !g_registry.valid(f.material),
        "costume reset damaged rare bonuses or failed to consume last material");
    Check(updates == 1 && payments == 1, "costume reset published intermediate empty attributes");
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
        Watch();
    }
    auto Use() { return ItemSystem::UseCostumeAttributeItem(owner, item, material); }
    void Unchanged()
    {
        Check(EqualAttributes(Attrs(), beforePayment) && updates == 0,
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
        Check(updates == 1 && payments == 1 && randomCalls == 0,
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
    Check(f.Use() == Result::SlotsFull && payments == 0 && updates == 0, "full rare slots accepted addition");
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
        Check(!g_registry.valid(f.material) && updates == 1 && payments == 1,
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
    Check(f.Use() == Result::Success && updates == 1 && payments == 1, "costume reroll transaction failed");
    Check(f.Attrs()[5].sValue == 400 && f.Attrs()[6].sValue == 500 && ItemSystem::GetItemAttributeCount(f.item) == 2,
        "costume changer altered rare bonuses or normal count");
    f.Watch();
    Check(f.Use() == Result::Success && !g_registry.valid(f.material) && updates == 1,
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
        Check(randomCalls == MAX_ATTR && payments == 1 && updates == 1 && !g_registry.valid(f.material),
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
        ItemSystem::GetItemCount(f.material) == 1 && payments == 1 && updates == 1,
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
        Check(payments == 1 && updates == 1 && !g_registry.valid(f.material),
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
    for (const uint8_t type : {ITEM_COSTUME, ITEM_RETIRED_29}) {
        f.proto.bType = type;
        Check(use() == Result::InvalidTarget, "costume/retired item accepted attribute lock");
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
            attributes[i] = {static_cast<uint8_t>(i + 1), static_cast<double>(10 + i)};
        npc = g_registry.create();
        g_registry.emplace<TransferActor>(npc);
        g_registry.emplace<TransferActor>(owner).npc = npc;
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
    ecs::AttrTransferWindowComponent& Window() { return g_registry.get<ecs::AttrTransferWindowComponent>(owner); }
    void Unchanged()
    {
        Check(EqualAttributes(Attrs(), beforePayment) && updates == 0 && transferLogs == 0,
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
    for (int scenario = 0; scenario < 15; ++scenario)
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
            case 10: g_registry.emplace<ecs::ShopState>(f.owner).currentShop = static_cast<entt::entity>(1); break;
            case 11: {
                const auto session = g_registry.create();
                g_registry.emplace<ecs::ExchangeSession>(session).offers[0].owner = f.owner;
                g_registry.emplace<ecs::ExchangeRef>(f.owner).session = session;
                break;
            }
            case 12: location.x = INT_MIN; g_registry.get<TransferActor>(f.npc).x = INT_MAX; break;
            case 13: g_registry.emplace<ecs::ShopState>(f.owner).myShop = static_cast<entt::entity>(1); break;
            case 14: g_registry.emplace<ecs::CubeWindowComponent>(f.owner).npc = f.npc; break;
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
        onUpdate = [&](entt::entity published) {
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
            payments == 0 && updates == 1 && transferLogs == 1 && !f.Window().busy,
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
        updates == 1 && transferLogs == 1, "cleanup retry recharged or republished transaction");
}

void CompanionCostCommit()
{
    using Cost = ItemSystem::ItemCost;
    for (bool accept : {false, true}) {
        TransferFixture f;
        const std::array costs{Cost{f.material, 1}, Cost{f.donor, 1}};
        int wallet = 100, publications = 0;
        const auto observe = [&](entt::entity) {
            ++publications;
            Check(wallet == 75 && ItemSystem::GetItemCount(f.material) == 1 &&
                (!g_registry.valid(f.donor) || ItemSystem::GetItemCount(f.donor) == 0),
                "cost companion published partial resource state");
        };
        onUpdate = onDestroy = observe;
        Check(ItemSystem::ConsumeOwnedItemCostsWithCommit(f.owner, costs, [&] {
            Check(ItemSystem::GetItemCount(f.material) == 2 && ItemSystem::GetItemCount(f.donor) == 1 && wallet == 100,
                "cost companion ran after debit");
            if (!accept) return false;
            wallet = 75; return true;
        }, [&] { observe(f.material); }) == accept, "cost companion result lost");
        Check(wallet == (accept ? 75 : 100) && (accept ? publications > 1 : publications == 0),
            "cost companion rejection changed resources");
        if (!accept) Check(ItemSystem::GetItemCount(f.material) == 2 && ItemSystem::GetItemCount(f.donor) == 1 &&
            !ItemSystem::IsItemConsumptionPending(f.donor), "rejected companion left a pending cost");
    }
    {
        TransferFixture f; int writes = 0;
        Check(ItemSystem::ConsumeOwnedItemCostsWithCommit(f.owner, {}, [&] { ++writes; return true; }, [&] {
            Check(writes == 1, "empty-cost companion published before commit");
        }) && writes == 1, "pure wallet companion rejected");
        Check(!ItemSystem::ConsumeOwnedItemCostsWithCommit(f.owner, {}, {}, [] {}) &&
            !ItemSystem::ConsumeOwnedItemCostsWithCommit(f.owner, {}, [] { return true; }, {}),
            "empty callbacks accepted");
    }
    {
        TransferFixture f; int writes = 0;
        const auto detached = f.Material(50001, 4, 8);
        inventory.erase({f.owner, INVENTORY, 8});
        g_registry.get<ecs::ItemOwner>(detached) = {};
        g_registry.get<ecs::ItemLocation>(detached) = {RESERVED_WINDOW, 0};
        rejectDestruction.insert(detached);
        const Cost cost{f.material, 1};
        Check(ItemSystem::ConsumeOwnedItemCostsWithCommit(f.owner, std::span(&cost, 1), [&] {
            ++writes; return true;
        }, [&] {
            Check(writes == 1 && ItemSystem::GetItemCount(f.material) == 1 && ItemSystem::GetItemCount(detached) == 0 &&
                ItemSystem::IsItemConsumptionPending(detached), "detached retirement was not part of commit");
        }, std::span(&detached, 1)), "detached companion retirement failed");
        Check(ItemSystem::IsItemConsumptionPending(detached), "failed detached cleanup lost pending entry");
        rejectDestruction.clear(); ItemSystem::ProcessPendingItemConsumptions();
        Check(!g_registry.valid(detached) && writes == 1 && ItemSystem::GetItemCount(f.material) == 1,
            "detached retirement retry repeated debit");
    }
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
    onUpdate = [&](entt::entity published) {
        if (published != f.item) return;
        for (const auto cost : costs)
            Check(ItemSystem::GetItemCount(cost.item) == 1, "64-cost batch partly visible");
        Check(EqualAttributes(f.Attrs(), desired.attrs), "target not committed with all costs");
    };
    Check(ItemSystem::SetItemAttributesWithItemCosts(f.owner, f.item, desired, costs), "64 valid costs rejected");
    Check(publishedCounts.size() == 64 && updates == 1 && destroyAttempts.empty(), "64-cost publication incorrect");
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
        onUpdate = [&](entt::entity published) {
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
        onUpdate = [&](entt::entity published) {
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
            g_registry.get<ecs::ItemAttributes>(replacement).attrs[0].sValue == 999 && updates == 1 &&
            ItemSystem::GetItemCount(f.material) == 1 && !g_registry.valid(f.donor),
            "post-commit replacement overwritten, refunded or published as old target");
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
        Check(updates == 0 && destroyAttempts.empty(), "rejected count write published");
        if (scenario == 4) Check(!g_registry.any_of<ecs::ItemCount>(invalid), "count setter created missing component");
    }
    {
        CountFixture f;
        CountSignals signals;
        entt::scoped_connection countConstruct = g_registry.on_construct<ecs::ItemCount>().connect<&CountSignals::Changed>(signals);
        entt::scoped_connection countUpdate = g_registry.on_update<ecs::ItemCount>().connect<&CountSignals::Changed>(signals);
        Check(ItemSystem::SetItemCountEcs(f.item, 4) && ItemSystem::GetItemCount(f.item) == 4,
            "positive stack update failed");
        Check(updates == 1, "positive stack publication not exactly once");
        ItemSystem::SetItemCount(f.item, 8);
        Check(ItemSystem::GetItemCount(f.item) == 8 && updates == 2, "void setter bypassed core");
        for (auto amount : {201u, UINT32_MAX})
            Check(ItemSystem::SetItemCountEcs(f.item, amount) && ItemSystem::GetItemCount(f.item) == 200,
                "stack limit/unsigned input narrowed before clamping");
        for (int limit : {0, -1}) {
            g_bItemCountLimit = limit;
            f.Watch();
            Check(!ItemSystem::SetItemCountEcs(f.item, 1) && ItemSystem::GetItemCount(f.item) == 200 &&
                updates == 0, "invalid cap created zero/negative stack");
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
        Check(!remove() && ItemSystem::GetItemCount(f.item) == 3 && updates == 0,
            "failed zero-count removal reported success or changed count");
        rejectDestruction.clear();
        Check(remove() && !g_registry.valid(f.item) && updates == 0,
            "zero-count removal did not retire the item");
    }
    {
        CountFixture f;
        bool nested = false;
        onUpdate = [&](entt::entity e) {
            Check(e == f.item && ItemSystem::GetItemCount(e) == (nested ? 9u : 5u),
                "packet observed uncommitted stack");
            if (!nested) {
                nested = true;
                Check(ItemSystem::SetItemCountEcs(e, 9), "nested count update rejected");
            }
        };
        Check(ItemSystem::SetItemCountEcs(f.item, 5) && ItemSystem::GetItemCount(f.item) == 9 &&
            updates == 2, "nested count commit was overwritten");
    }
    for (int scenario = 0; scenario < 4; ++scenario) {
        CountFixture f;
        entt::entity replacement {entt::null};
        onUpdate = [&](entt::entity e) {
            Check(ItemSystem::GetItemCount(e) == 5, "packet sent before positive commit");
            if (scenario == 0) {
                g_registry.destroy(e);
                replacement = f.Material(50001, 37, 5);
                Check(entt::to_entity(replacement) == entt::to_entity(e) && replacement != e,
                    "fixture did not recycle entity index");
            } else if (scenario == 1) g_registry.remove<ecs::ItemCount>(e);
            else if (scenario == 2) g_registry.get<ecs::ItemCount>(e).count = 0;
            else Check(ItemSystem::SetItemCountEcs(e, 0), "nested zero-count removal failed");
        };
        Check(ItemSystem::SetItemCountEcs(f.item, 5) && updates == 1,
            "publication-time removal hid a committed write or published again");
        if (scenario == 0) Check(ItemSystem::GetItemCount(replacement) == 37, "stale publication touched replacement item");
    }
    {
        CountFixture f;
        onUpdate = [&](entt::entity e) { g_registry.destroy(e); };
        Check(ItemSystem::SetItemCountEcs(f.item, 6) && !g_registry.valid(f.item) && updates == 1,
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
        Check(ItemSystem::GetItemCount(f.material) == 0 && updates == 0,
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
                    onUpdate = onDestroy = observe;
                    const auto result = ItemSystem::MergeItemStacksEcs(f.owner, f.item, f.material, request);
                    Check(result.transferred == uint32_t(expected) && result.sourceDepleted == (expected == source),
                        "stack transfer amount/depletion boundary incorrect");
                    Check(ItemSystem::GetItemCount(f.item) + ItemSystem::GetItemCount(f.material) == uint32_t(source + target),
                        "stack merge created or lost units");
                    Check(signals.calls == 0, "stack commit emitted registry signals");
                    if (expected == 0) Check(updates == 0 && destroyAttempts.empty(), "failed merge published changes");
                    else if (expected == source) Check(!g_registry.valid(f.item) && updates == 1 &&
                        destroyAttempts.size() == 1, "depleted stack not retired exactly once");
                    else Check(updates == 2 && destroyAttempts.empty(), "partial merge publication incorrect");
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
            updates == 0 && destroyAttempts.empty(), "rejected merge changed state");
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
            case 17: g_registry.get<ecs::ItemPrototypeMeta>(e).type = ITEM_RETIRED_29; break;
            case 18: g_registry.get<ecs::ItemPrototypeMeta>(e).type = ITEM_RETIRED_30; break;
            case 19: g_registry.get<ecs::ItemPrototypeMeta>(e).type = ITEM_ELK; break;
            case 20: g_registry.emplace<ecs::ItemExtraProtoRef>(e).proto = reinterpret_cast<TItemExtraProto*>(1); break;
        }
        Check(ItemSystem::MergeItemStacksEcs(f.owner, f.item, e).transferred == 0 &&
            ItemSystem::GetItemCount(f.item) == 40 && ItemSystem::GetItemCount(e) == 180 && updates == 0,
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
        onUpdate = [&](entt::entity) {
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
        onUpdate = [&](entt::entity) {
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
        onUpdate = [&](entt::entity) { throw std::runtime_error("publication failure"); };
        bool threw = false;
        try { ItemSystem::MergeItemStacksEcs(f.owner, f.item, f.material); }
        catch (const std::runtime_error&) { threw = true; }
        Check(threw && ItemSystem::GetItemCount(f.item) == 20 && ItemSystem::GetItemCount(f.material) == 200,
            "publication exception rolled back only one count");
    }
}


void GroundStackMerges()
{
    const auto ground = [](StackFixture& f) {
        f.Detach();
        g_registry.get<ecs::ItemLocation>(f.item).window = GROUND;
        g_registry.emplace<ecs::SpatialEntity>(f.item);
        g_registry.emplace<ecs::SectorPlacement>(f.item);
    };
    for (uint32_t count : {255u, 256u, 700u, 1000u}) {
        StackFixture f; ground(f); g_bItemCountLimit = 1000;
        g_registry.get<ecs::ItemCount>(f.item).count = count;
        const auto observe = [&](entt::entity) {
            const uint32_t moved = std::min(count, 820u);
            Check(ItemSystem::GetItemCount(f.item) == count - moved &&
                ItemSystem::GetItemCount(f.material) == 180 + moved, "ground pickup exposed half a transfer");
        };
        onUpdate = onDestroy = observe;
        const auto result = ItemSystem::MergeItemStacksEcs(f.owner, f.item, f.material, 0, ItemSystem::StackSource::GroundPickup);
        Check(result.transferred == std::min(count, 820u) && result.sourceDepleted == (count <= 820),
            "ground pickup narrowed counts to eight bits");
    }
    for (int invalid = 0; invalid < 10; ++invalid) {
        StackFixture f; ground(f);
        if (invalid == 0) g_registry.remove<ecs::SpatialEntity>(f.item);
        if (invalid == 1) g_registry.remove<ecs::SectorPlacement>(f.item);
        if (invalid == 2) g_registry.get<ecs::ItemLocation>(f.item).window = RESERVED_WINDOW;
        if (invalid == 3) g_registry.get<ecs::ItemOwner>(f.item).owner = f.owner;
        if (invalid == 4) g_registry.get<ecs::ItemOwner>(f.item).ownerPID = 123;
        if (invalid == 5) g_registry.get<ecs::ItemFlags>(f.item).isLocked = true;
        if (invalid == 6) g_registry.get<ecs::ItemFlags>(f.item).exchanging = true;
        if (invalid == 7) g_registry.emplace_or_replace<ecs::ItemEquipped>(f.item).equipped = true;
        if (invalid == 8) {
            g_registry.emplace<TestGroundReservation>(f.item);
        }
        if (invalid == 9) g_registry.get<ecs::ItemAttributes>(f.item).attrs[0] = {APPLY_MAX_HP, 123};
        Check(ItemSystem::MergeItemStacksEcs(f.owner, f.item, f.material, 0,
            ItemSystem::StackSource::GroundPickup).transferred == 0 &&
            ItemSystem::GetItemCount(f.item) == 40 && ItemSystem::GetItemCount(f.material) == 180 && updates == 0,
            "ground pickup accepted an invalid/reserved/foreign payload");
    }
    {
        StackFixture f; ground(f);
        g_registry.get<ecs::ItemCount>(f.item).count = 10;
        rejectDestruction.insert(f.item);
        const auto result = ItemSystem::MergeItemStacksEcs(f.owner, f.item, f.material, 0, ItemSystem::StackSource::GroundPickup);
        Check(result.transferred == 10 && result.sourceDepleted && ItemSystem::IsItemConsumptionPending(f.item),
            "failed ground cleanup hid a committed pickup");
        Check(ItemSystem::MergeItemStacksEcs(f.owner, f.item, f.material, 0,
            ItemSystem::StackSource::GroundPickup).transferred == 0, "pending ground item was picked twice");
        rejectDestruction.clear(); ItemSystem::ProcessPendingItemConsumptions();
        Check(!g_registry.valid(f.item) && ItemSystem::GetItemCount(f.material) == 190,
            "ground retirement retry credited the target twice");
    }
}

void AutomaticStackMerges()
{
    {
        StackFixture f;
        f.Detach();
        g_registry.get<ecs::ItemFlags>(f.item).flags = 0;
        Check(ItemSystem::MergeItemIntoInventoryEcs(f.owner, f.item) == f.item &&
            stackCategoryLookups == 0 && updates == 0,
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
        onUpdate = [&](entt::entity) {
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
        onUpdate = [&](entt::entity) {
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
        onUpdate = [&](entt::entity e) { g_registry.destroy(e); };
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
    Check(payments == 1 && updates == 1, "switchbot charged or committed more than once");
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



        EntityAndTableValidation();
        LockedSlotAndRarePreservation();
        FailureIsAtomic();
        RareHolesAndFailure();
        AddonRemovalAndDuplicates();
        WeightedBoundaries();
        ProtoAndCostumeRules();
        HitDamageScrollTransactions();
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
        SwitchbotTransactions();
        SwitchbotMaterialSelection();
        TransferWindowAndCommands();
        TransferContextGuards();
        TransferItemGuards();
        TransferPaymentAndCommit();
        BatchCostValidation();
        CompanionCostCommit();
        BatchReentrancyAndRetirement();
        CountValidationAndLimits(); CountDestructionAndCallbacks();
        StackMergeBoundaries(); StackMergeGuards(); StackMergeCallbacks(); GroundStackMerges(); AutomaticStackMerges();
        std::cout << "Item attribute regression checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
