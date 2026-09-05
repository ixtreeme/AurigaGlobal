#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
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
    if (!transferTest) UnexpectedSwitchbotService();
    static auto logger = std::make_shared<spdlog::logger>("transfer-test", spdlog::sinks_init_list{});
    return logger;
}
void intrusive_ptr_add_ref(EVENT*) { UnexpectedSwitchbotService(); }
void intrusive_ptr_release(EVENT*) { UnexpectedSwitchbotService(); }
LPEVENT event_create_ex(TEVENTFUNC, event_info_data*, int32_t) { UnexpectedSwitchbotService(); }
void event_cancel(LPEVENT*) { UnexpectedSwitchbotService(); }
#ifdef TEXTS_IMPROVEMENT
void ecs::ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, const char*, ...) { if (!transferTest) UnexpectedSwitchbotService(); }
#endif
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity) { if (!transferTest) UnexpectedSwitchbotService(); return 42; }
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
const char* ItemSystem::GetItemName(entt::entity) { UnexpectedSwitchbotService(); }
entt::entity ItemSystem::GetItemOwnerEntity(entt::entity) { UnexpectedSwitchbotService(); }
int ItemSystem::GetItemAttributeType(entt::entity, int) { UnexpectedSwitchbotService(); }
int ItemSystem::GetItemAttributeValue(entt::entity, int) { UnexpectedSwitchbotService(); }

// Compile the complete production DragonSoul.cpp. Unused refinement, timer,
// legacy-character and inventory-movement services must never be entered here.
int MIN(int a, int b) { return std::min(a, b); }
time_t get_global_time() { UnexpectedSwitchbotService(); }
void DragonSoulSystem::DeactivateAll(entt::entity) { UnexpectedSwitchbotService(); }
bool DragonSoulSystem::CanRefine(entt::entity) { UnexpectedSwitchbotService(); }
int32_t DragonSoulSystem::GetLastRefineTime(entt::entity) { UnexpectedSwitchbotService(); }
void DragonSoulSystem::SetLastRefineTime(entt::entity) { UnexpectedSwitchbotService(); }
entt::entity ITEM_MANAGER::CreateItem(uint32_t, uint32_t, uint32_t, bool, int, bool) { UnexpectedSwitchbotService(); }
bool DragonSoulTable::GetRefineGradeValues(uint8_t, uint8_t, int&, int&, std::vector<float>&) { UnexpectedSwitchbotService(); }
bool DragonSoulTable::GetRefineStepValues(uint8_t, uint8_t, int&, int&, std::vector<float>&) { UnexpectedSwitchbotService(); }
bool DragonSoulTable::GetRefineStrengthValues(uint8_t, uint8_t, uint8_t, int&, float&) { UnexpectedSwitchbotService(); }
bool DragonSoulTable::GetDragonHeartExtValues(uint8_t, uint8_t, std::vector<float>&, std::vector<float>&) { UnexpectedSwitchbotService(); }
bool DragonSoulTable::GetDragonSoulExtValues(uint8_t, uint8_t, float&, uint32_t&) { UnexpectedSwitchbotService(); }
void LogManager::ItemLogEntity(LPCHARACTER, entt::entity, const char*, const char*) { UnexpectedSwitchbotService(); }
entt::entity ItemSystem::GetWearItem(entt::entity, uint8_t) { UnexpectedSwitchbotService(); }
void ItemSystem::AutoGiveItem(entt::entity, entt::entity, bool
#ifdef __HIGHLIGHT_SYSTEM__
    , bool
#endif
) { UnexpectedSwitchbotService(); }
bool ItemSystem::AutoGiveDS(entt::entity, entt::entity, bool) { UnexpectedSwitchbotService(); }
entt::entity ItemSystem::AutoGiveItemEcs(entt::entity, uint32_t, uint32_t, int, bool) { UnexpectedSwitchbotService(); }
int ItemSystem::GetItemLimitTimerBasedOnWearIndex(entt::entity) { UnexpectedSwitchbotService(); }
int ItemSystem::GetItemDuration(entt::entity) { return 0; }
bool ItemSystem::DestroyItemEntityEcs(entt::entity item, const char*)
{
    Check(transferTest && ItemSystem::IsItemConsumptionPending(item) && ItemSystem::GetItemCount(item) == 0,
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
bool ItemSystem::SetItemSocketEcs(entt::entity, int, uint32_t) { UnexpectedSwitchbotService(); }
bool ItemSystem::CopyItemAttributesEcs(entt::entity, entt::entity) { UnexpectedSwitchbotService(); }
bool ItemSystem::PlaceItemEcs(entt::entity, entt::entity, uint8_t, uint16_t) { UnexpectedSwitchbotService(); }
bool ItemSystem::RemoveItemEcs(entt::entity) { UnexpectedSwitchbotService(); }
int ItemSystem::GetEmptyDragonSoulInventory(entt::entity, entt::entity) { UnexpectedSwitchbotService(); }
bool ItemSystem::ModifyItemPointsEcs(entt::entity, bool) { UnexpectedSwitchbotService(); }
bool ItemSystem::StartTimerBasedOnWearExpireEventEcs(entt::entity) { UnexpectedSwitchbotService(); }
bool ItemSystem::StopTimerBasedOnWearExpireEventEcs(entt::entity) { UnexpectedSwitchbotService(); }
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
void LogManager::ItemLogEntity(entt::entity, entt::entity, const char*, const char*) {}
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
CExchange* ecs::SocialSystem::GetExchange(entt::entity e)
{
    const auto* exchange = g_registry.try_get<ecs::ExchangeRef>(e);
    return exchange ? exchange->exchange : nullptr;
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
uint32_t GetItemCount(entt::entity item) { return g_registry.get<ecs::ItemCount>(item).count; }
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
    Check(!transferTest, "batch transfer fell back to sequential consumption");
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
        transferTest = false;
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
            case 11: g_registry.emplace<ecs::ExchangeRef>(f.owner).exchange = reinterpret_cast<CExchange*>(1); break;
            case 12: g_registry.emplace<ecs::AcceWindowComponent>(f.owner).absorptionOpen = true; break;
            case 13: location.x = INT_MIN; g_registry.get<TransferActor>(f.npc).x = INT_MAX; break;
            case 14: g_registry.emplace<ecs::ShopState>(f.owner).myShop = reinterpret_cast<CShop*>(1); break;
            case 15: g_registry.emplace<ecs::CubeWindowComponent>(f.owner).pNpc = reinterpret_cast<CHARACTER*>(1); break;
        }
        Check(!AttrTransfer_make(f.owner) && payments == 0, "blocked transfer context accepted");
        f.Unchanged();
        AttrTransfer_close(f.owner);
        Check(!AttrTransfer_is_open(f.owner), "blocked context prevented close");
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
        SwitchbotTransactions();
        SwitchbotMaterialSelection();
        TransferWindowAndCommands();
        TransferContextGuards();
        TransferItemGuards();
        TransferPaymentAndCommit();
        BatchCostValidation();
        BatchReentrancyAndRetirement();
        std::cout << "Item attribute regression checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
