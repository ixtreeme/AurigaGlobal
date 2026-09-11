#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/exchange.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/inventory_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/character_runtime_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SessionSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/QuestSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/desc_client.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/DragonSoul.h"
#include "../../SRC/Server/GameServer/dragon_soul_table.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/log.h"
#include "../../SRC/Server/GameServer/db.h"
#include "../../SRC/Server/GameServer/packet.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <climits>

// The complete production exchange.cpp is linked. Fixtures contain only
// entities/components and real inert descriptors, never CHARACTER or CItem.
entt::registry g_registry;
int passes_per_sec = 25, g_nPortalLimitTime = 10;
std::string g_stHostname = "exchange-test";
LPCLIENT_DESC db_clientdesc = nullptr;
namespace {
int checks = 0, saves = 0, characterSaves = 0, itemLogs = 0, goldLogs = 0;
int pulse = 100000;
bool noQuest = false, questConsumes = false;
std::function<void()> onQuest, onFlag, onSave, onQuickslot;
std::function<void(entt::entity, uint8_t, uint8_t)> onPacket;
std::vector<std::unique_ptr<DESC>> descriptors;
struct Actor {
    DESC* desc = nullptr;
    uint32_t pid = 0;
    int32_t x = 100, y = 100, map = 1;
    int inventorySize = INVENTORY_MAX_NUM, unlocked = 1000;
    bool pc = true, canHandle = true, safebox = false, cube = false, block = false;
    uint8_t gm = GM_PLAYER;
    std::string name = "player";
};
struct ItemMeta {
    uint8_t size = 1, category = 0;
    bool dragon = false, extra = false;
    uint16_t dragonBase = 0;
    uint32_t anti = 0;
};
struct WirePacket { entt::entity recipient; uint8_t header, sub; };
std::vector<WirePacket> sent;
std::vector<std::pair<entt::entity, uint8_t>> quickDeletes;
void Check(bool value, const char* message) {
    ++checks; if (!value) throw std::runtime_error(message);
}
[[noreturn]] void Unexpected(const char* message) { throw std::runtime_error(message); }
void Call(std::function<void()>& callback) {
    if (callback) { auto current = callback; current(); }
}
Actor& A(entt::entity e) {
    Check(g_registry.valid(e) && g_registry.all_of<Actor>(e), "stale character service");
    return g_registry.get<Actor>(e);
}
ItemMeta& I(entt::entity e) {
    Check(g_registry.valid(e) && g_registry.all_of<ItemMeta>(e), "stale item service");
    return g_registry.get<ItemMeta>(e);
}
template<class F> decltype(auto) Inventory(entt::entity owner, uint8_t window, F&& f) {
#ifdef ENABLE_EXTRA_INVENTORY
    if (window == EXTRA_INVENTORY) return f(g_registry.get<ecs::ExtraInventoryRuntimeComponent>(owner));
#endif
    if (window == DRAGON_SOUL_INVENTORY) return f(g_registry.get<ecs::DragonSoulInventoryComponent>(owner));
    return f(g_registry.get<ecs::MainInventoryRuntimeComponent>(owner));
}
entt::entity ActorEntity() {
    const auto e = g_registry.create();
    auto desc = std::make_unique<DESC>(); desc->SetEntity(e);
    auto& actor = g_registry.emplace<Actor>(e);
    actor.desc = desc.get(); actor.pid = entt::to_integral(e) + 100;
    actor.name += std::to_string(actor.pid);
    std::strcpy(desc->GetAccountTable().login, "account");
    descriptors.push_back(std::move(desc));
    g_registry.emplace<ecs::MainInventoryRuntimeComponent>(e);
    g_registry.emplace<ecs::DragonSoulInventoryComponent>(e);
#ifdef ENABLE_EXTRA_INVENTORY
    g_registry.emplace<ecs::ExtraInventoryRuntimeComponent>(e);
#endif
    g_registry.emplace<ecs::GoldAmount>(e, 10000);
    g_registry.emplace<ecs::QuickSlots>(e);
    g_registry.emplace<ecs::StatusFlags>(e);
    g_registry.emplace<ecs::WarpBlockState>(e);
    return e;
}
entt::entity Item(entt::entity owner, int cell = 0, uint8_t size = 1, uint8_t window = INVENTORY) {
    const auto e = g_registry.create();
    auto& meta = g_registry.emplace<ItemMeta>(e); meta.size = size;
    meta.dragon = window == DRAGON_SOUL_INVENTORY;
#ifdef ENABLE_EXTRA_INVENTORY
    meta.extra = window == EXTRA_INVENTORY;
    if (meta.extra) meta.category = cell / EXTRA_INVENTORY_CATEGORY_MAX_NUM;
#endif
    auto& identity = g_registry.emplace<ecs::ItemIdentity>(e);
    identity.id = entt::to_integral(e) + 100; identity.vnum = 100;
    auto& ownership = g_registry.emplace<ecs::ItemOwner>(e);
    ownership.owner = owner; ownership.ownerPID = A(owner).pid;
    g_registry.emplace<ecs::ItemLocation>(e, window, static_cast<uint16_t>(cell));
    g_registry.emplace<ecs::ItemFlags>(e);
    g_registry.emplace<ecs::ItemCount>(e, 1);
    g_registry.emplace<ecs::ItemEquipped>(e);
    g_registry.emplace<ecs::ItemSockets>(e);
    g_registry.emplace<ecs::ItemAttributes>(e);
    g_registry.emplace<ecs::ItemLockedAttribute>(e);
    const int columns = window == DRAGON_SOUL_INVENTORY ? DRAGON_SOUL_BOX_COLUMN_NUM : INVENTORY_PAGE_COLUMN;
    Inventory(owner, window, [&](auto& inv) {
        Check(cell >= 0 && cell + (size - 1) * columns < inv.items.size(), "fixture bounds");
        inv.items[cell] = e;
        for (int row = 0; row < size; ++row) inv.itemGrid[cell + row * columns] = cell + 1;
    });
    return e;
}
void Reset() {
    onQuest = onFlag = onSave = onQuickslot = {};
    onPacket = {};
    g_registry.clear();
    descriptors.clear(); sent.clear(); quickDeletes.clear();
    saves = characterSaves = itemLogs = goldLogs = 0;
    noQuest = questConsumes = false;
}
void Offer(entt::entity owner, entt::entity item, int display = 0) {
    const auto pos = g_registry.get<ecs::ItemLocation>(item);
    Check(ExchangeSystem::AddItem(owner, TItemPos(pos.window, pos.cell), display), "offer succeeds");
}
bool Finish(entt::entity first, entt::entity second) {
    Check(ExchangeSystem::Accept(first), "first accepts");
    return ExchangeSystem::Accept(second);
}
void AssertClosed(entt::entity first, entt::entity second) {
    Check(!ExchangeSystem::IsActive(first) && !ExchangeSystem::IsActive(second), "both reservations released");
    Check(g_registry.view<ecs::ExchangeSession>().empty(), "no session leaked");
}
}

namespace logging {
std::shared_ptr<spdlog::logger> GetLogger() { static auto log = std::make_shared<spdlog::logger>("exchange"); return log; }
std::shared_ptr<spdlog::logger> GetErrorLogger() { return GetLogger(); }
}
int thecore_pulse() { return pulse; }
void intrusive_ptr_release(EVENT*) { Unexpected("unexpected live event in exchange fixture"); }
DESC::DESC() { m_sock = 1; m_entity = entt::null; m_accountTable = {}; }
DESC::~DESC() {}
void DESC::Destroy() { Unexpected(__func__); }
void DESC::SetPhase(int) { Unexpected(__func__); }
CLIENT_DESC::CLIENT_DESC() {}
CLIENT_DESC::~CLIENT_DESC() {}
void CLIENT_DESC::Destroy() { Unexpected(__func__); }
void CLIENT_DESC::SetPhase(int) { Unexpected(__func__); }
CInputProcessor::CInputProcessor() {}
bool CInputProcessor::Process(DESC*, const void*, int, int&) { Unexpected(__func__); }
void CInputProcessor::Handshake(DESC*, const char*) { Unexpected(__func__); }
CInputHandshake::CInputHandshake() {}
CInputHandshake::~CInputHandshake() {}
int CInputHandshake::Analyze(DESC*, uint8_t, const char*) { Unexpected(__func__); }
int CInputLogin::Analyze(DESC*, uint8_t, const char*) { Unexpected(__func__); }
int CInputMain::Analyze(DESC*, uint8_t, const char*) { Unexpected(__func__); }
int CInputDead::Analyze(DESC*, uint8_t, const char*) { Unexpected(__func__); }
int CInputDB::Analyze(DESC*, uint8_t, const char*) { Unexpected(__func__); }
bool CInputDB::Process(DESC*, const void*, int, int&) { Unexpected(__func__); }
CInputP2P::CInputP2P() {}
CInputAuth::CInputAuth() {}
int CInputP2P::Analyze(DESC*, uint8_t, const char*) { Unexpected(__func__); }
int CInputAuth::Analyze(DESC*, uint8_t, const char*) { Unexpected(__func__); }
CPacketInfo::CPacketInfo() : m_pCurrentPacket(nullptr), m_dwStartTime(0) {}
CPacketInfo::~CPacketInfo() {}
CPacketInfoCG::CPacketInfoCG() {}
CPacketInfoGG::CPacketInfoGG() {}
CPacketInfoCG::~CPacketInfoCG() {}
CPacketInfoGG::~CPacketInfoGG() {}
Cipher::Cipher() : activated_(false), encoder_(nullptr), decoder_(nullptr), key_agreement_(nullptr) {}
Cipher::~Cipher() {}
void DESC::Packet(const void* data, int size) {
    const auto header = *static_cast<const uint8_t*>(data);
    uint8_t sub = 0;
    if (header == HEADER_GC_EXCHANGE) {
        Check(size == sizeof(packet_exchange), "exchange wire size");
        sub = static_cast<const packet_exchange*>(data)->sub_header;
    }
    sent.push_back({GetEntity(), header, sub});
    if (onPacket) { auto callback = onPacket; callback(GetEntity(), header, sub); }
}
void CLIENT_DESC::DBPacket(uint8_t, uint32_t, const void*, uint32_t) {
    Unexpected("exchange unexpectedly sent a direct DB credit packet");
}
CHARACTER_MANAGER::CHARACTER_MANAGER() = default;
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
void CHARACTER_MANAGER::DelayedSave(entt::entity e) { A(e); ++characterSaves; }
DSManager::DSManager() = default;
DSManager::~DSManager() = default;
DragonSoulTable::~DragonSoulTable() = default;
uint16_t DSManager::GetBasePosition(entt::entity e) const { return I(e).dragonBase; }
quest::CQuestManager::CQuestManager() = default;
quest::CQuestManager::~CQuestManager() = default;
quest::NPC::NPC() = default;
quest::NPC::~NPC() = default;
quest::PC::PC() : m_RunningQuestState(nullptr) {}
quest::PC::~PC() = default;
quest::PC* quest::CQuestManager::GetPCForce(unsigned int) { static quest::PC pc; return noQuest ? nullptr : &pc; }
bool quest::CQuestManager::GiveItemToPC(unsigned int, entt::entity target) { A(target); Call(onQuest); return questConsumes; }
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() = default;
CAsyncSQL::~CAsyncSQL() = default;
LogManager::LogManager() : m_bIsConnect(false) {}
LogManager::~LogManager() = default;
void LogManager::ItemLog(entt::entity e, int, int, const char*, const char*) { A(e); ++itemLogs; }
void LogManager::CharLog(entt::entity e, uint32_t, const char*, const char*) { A(e); ++goldLogs; }
void LogManager::GoldBarLog(uint32_t, uint32_t, GOLDBAR_HOW, const char*) { ++goldLogs; }
namespace ecs::PlayerRuntime {
bool IsValid(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<Actor>(e); }
bool IsPC(entt::entity e) { return IsValid(e) && A(e).pc; }
LPDESC GetDesc(entt::entity e) { return A(e).desc; }
int32_t GetX(entt::entity e) { return A(e).x; }
int32_t GetY(entt::entity e) { return A(e).y; }
int32_t GetMapIndex(entt::entity e) { return A(e).map; }
uint32_t GetPlayerID(entt::entity e) { return A(e).pid; }
uint32_t GetPacketVID(entt::entity e) { return A(e).pid + 100; }
std::string_view GetName(entt::entity e) { return A(e).name; }
uint8_t GetGMLevel(entt::entity e) { return A(e).gm; }
int GetDuelOption(entt::entity e, const char*) { return A(e).block; }
// exchange.cpp used to read the flags component here itself; case 11 sets it.
bool IsBlockMode(entt::entity e, uint8_t flag)
{
    const auto* flags = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e);
    return flags && (flags->blockMode & flag) != 0;
}
}
namespace ecs::SessionSystem {
bool IsSafeboxOpen(entt::entity e) { return A(e).safebox; }
bool IsCubeOpen(entt::entity e) { return A(e).cube; }
}
namespace ecs::QuestSystem {
int32_t GetFlag(entt::entity e, std::string_view) { const int result = A(e).unlocked; Call(onFlag); return result; }
}
void ecs::ChatSystem::SendNew(entt::entity e, uint8_t, uint32_t, const char*, ...) { A(e); }
namespace ecs::PointSystem {
int64_t GetGold(entt::entity e) { A(e); return g_registry.get<ecs::GoldAmount>(e).amount; }
}
namespace InventorySystem {
bool CanHandleItems(entt::entity e, bool, bool) { return A(e).canHandle; }
int GetInventorySize(entt::entity e) { return A(e).inventorySize; }
}
namespace NetworkSyncSystem {
void SendQuickslotDelete(entt::entity e, uint8_t slot) { A(e); quickDeletes.emplace_back(e, slot); Call(onQuickslot); }
}
namespace ItemSystem {
bool IsValidItem(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<ItemMeta>(e); }
entt::entity GetItem(entt::entity e, TItemPos p) {
    A(e);
    return Inventory(e, p.window_type, [&](auto& inv) { return p.cell < inv.items.size() ? inv.items[p.cell] : entt::null; });
}
entt::entity GetItemOwner(entt::entity e) { I(e); return g_registry.get<ecs::ItemOwner>(e).owner; }
uint8_t GetItemWindow(entt::entity e) { I(e); return g_registry.get<ecs::ItemLocation>(e).window; }
uint16_t GetItemCell(entt::entity e) { I(e); return g_registry.get<ecs::ItemLocation>(e).cell; }
uint32_t GetItemID(entt::entity e) { I(e); return g_registry.get<ecs::ItemIdentity>(e).id; }
uint32_t GetItemVnum(entt::entity e) { I(e); return g_registry.get<ecs::ItemIdentity>(e).vnum; }
const char* GetItemName(entt::entity e) { I(e); return "item"; }
uint32_t GetItemCount(entt::entity e) { I(e); return g_registry.get<ecs::ItemCount>(e).count; }
uint8_t GetItemSize(entt::entity e) { return I(e).size; }
bool IsItemEquipped(entt::entity e) { I(e); return g_registry.get<ecs::ItemEquipped>(e).equipped; }
bool IsItemLocked(entt::entity e) { I(e); return g_registry.get<ecs::ItemFlags>(e).isLocked; }
bool IsItemExchanging(entt::entity e) { I(e); return g_registry.get<ecs::ItemFlags>(e).exchanging; }
bool GetItemSkipSave(entt::entity e) { I(e); return g_registry.get<ecs::ItemFlags>(e).skipSave; }
int32_t GetItemFlags(entt::entity e) { I(e); return g_registry.get<ecs::ItemFlags>(e).flags; }
uint32_t GetItemAntiFlag(entt::entity e) { return I(e).anti; }
uint32_t GetItemSocket(entt::entity e, int index) { I(e); return g_registry.get<ecs::ItemSockets>(e).sockets.at(index); }
TPlayerItemAttribute GetItemAttribute(entt::entity e, int index) { I(e); return g_registry.get<ecs::ItemAttributes>(e).attrs.at(index); }
int16_t GetItemLockedAttributeIndex(entt::entity e) { I(e); return g_registry.get<ecs::ItemLockedAttribute>(e).index; }
bool IsDragonSoulItem(entt::entity e) { return I(e).dragon; }
bool IsExtraItem(entt::entity e) { return I(e).extra; }
uint8_t GetItemExtraCategory(entt::entity e) { return I(e).category; }
bool SaveItemEcs(entt::entity e, bool flush) { I(e); Check(flush, "immediate item persistence"); ++saves; Call(onSave); return true; }

}
namespace {
void LifecycleTests() {
    Reset(); auto a = ActorEntity(), b = ActorEntity(), c = ActorEntity(); auto item = Item(a);
    Check(!ExchangeSystem::Start(a, a), "no self trade");
    Check(!ExchangeSystem::Start(a, entt::null), "no null target");
    Check(ExchangeSystem::Start(a, b), "native start");
    auto session = ExchangeSystem::GetSession(a);
    Check(session == ExchangeSystem::GetSession(b), "one shared session");
    Check(!ExchangeSystem::Start(a, c), "one trade per player"); Offer(a, item);
    onPacket = [&](auto, auto h, auto sub) {
        if (h == HEADER_GC_EXCHANGE && sub == EXCHANGE_SUBHEADER_GC_END) {
            Check(!ExchangeSystem::Start(a, c), "end callback cannot replace reserved trade");
            ExchangeSystem::Cancel(b);
        }
    };
    ExchangeSystem::Cancel(a); onPacket = {};
    AssertClosed(a, b); Check(!ItemSystem::IsItemExchanging(item), "cancel unlocks item");
    ExchangeSystem::Cancel(a); Check(ExchangeSystem::Start(a, b), "restart");
    Offer(a, item); g_registry.destroy(b); AssertClosed(a, b);
    Check(!ItemSystem::IsItemExchanging(item), "participant destruction unlocks");
    b = ActorEntity(); Check(ExchangeSystem::Start(a, b), "recycled participant starts fresh");
    Offer(a, item); g_registry.destroy(ExchangeSystem::GetSession(a)); AssertClosed(a, b);
    Check(!ItemSystem::IsItemExchanging(item), "direct session destruction unlocks");
    Check(ExchangeSystem::Start(a, b), "start before ref removal");
    g_registry.remove<ecs::ExchangeRef>(a); AssertClosed(a, b);
}
void StartGuards() {
    for (int scenario = 0; scenario < 12; ++scenario) {
        Reset(); auto a = ActorEntity(), b = ActorEntity();
        switch (scenario) {
        case 0: A(b).pc = false; break;
        case 1: A(b).desc = nullptr; break;
        case 2: A(b).desc->SetEntity(a); break;
        case 3: A(b).canHandle = false; break;
        case 4: A(b).safebox = true; break;
        case 5: A(b).cube = true; break;
        case 6: A(b).x = A(a).x + EXCHANGE_MAX_DISTANCE; break;
        case 7: A(b).map = 2; break;
        case 8: g_registry.get<ecs::StatusFlags>(b).isDead = true; break;
        case 9: g_registry.get<ecs::StatusFlags>(b).isStunned = true; break;
        case 10: g_registry.get<ecs::WarpBlockState>(b).safeboxLoadTime = pulse; break;
        case 11: g_registry.emplace<ecs::CharacterRuntimeFlagsComponent>(b).blockMode = BLOCK_EXCHANGE; break;
        }
        Check(!ExchangeSystem::Start(a, b), "start guard"); AssertClosed(a, b);
    }
    Reset(); auto a = ActorEntity(), b = ActorEntity();
    onQuest = [&] { g_registry.destroy(b); };
    Check(!ExchangeSystem::Start(a, b), "quest callback destroys target safely"); AssertClosed(a, b);
    onQuest = {}; b = ActorEntity(); questConsumes = true;
    Check(!ExchangeSystem::Start(a, b), "quest handles gift instead of trade"); AssertClosed(a, b);
}
void OffersAndValidation() {
    for (int scenario = 0; scenario < 14; ++scenario) {
        Reset(); auto a = ActorEntity(), b = ActorEntity(); auto item = Item(a);
        Check(ExchangeSystem::Start(a, b), "validation start");
        if (scenario < 6) {
            switch (scenario) {
            case 0: I(item).anti = ITEM_ANTIFLAG_GIVE; break;
            case 1: g_registry.get<ecs::ItemFlags>(item).isLocked = true; break;
            case 2: g_registry.get<ecs::ItemFlags>(item).skipSave = true; break;
            case 3: g_registry.get<ecs::ItemEquipped>(item).equipped = true; break;
            case 4: g_registry.get<ecs::ItemCount>(item).count = 0; break;
            case 5: I(item).size = 0; break;
            }
            Check(!ExchangeSystem::AddItem(a, TItemPos(INVENTORY, 0), 0), "invalid offer rejected");
            ExchangeSystem::Cancel(a); continue;
        }
        Offer(a, item);
        Check(!ExchangeSystem::AddItem(a, TItemPos(INVENTORY, 0), 1), "duplicate reservation rejected");
        switch (scenario) {
        case 6: g_registry.get<ecs::ItemCount>(item).count = 2; break;
        case 7: g_registry.get<ecs::ItemSockets>(item).sockets[0] = 1; break;
        case 8: g_registry.get<ecs::ItemAttributes>(item).attrs[0].sValue = 1; break;
        case 9: g_registry.get<ecs::ItemLockedAttribute>(item).index = 1; break;
        case 10: g_registry.get<ecs::ItemOwner>(item).owner = b; break;
        case 11: g_registry.destroy(item); break;
        case 12: g_registry.get<ecs::MainInventoryRuntimeComponent>(a).itemGrid[0] = 0; break;
        case 13: A(b).map = 2; break;
        }
        Check(!Finish(a, b), "changed offer aborts"); AssertClosed(a, b);
        Check(saves == 0 && ecs::PointSystem::GetGold(a) == 10000, "failed trade has no transfer effects");
    }
    Reset(); auto a = ActorEntity(), b = ActorEntity(); auto tall = Item(a, 0, 3), other = Item(a, 1);
    Check(ExchangeSystem::Start(a, b), "display start");
    Check(!ExchangeSystem::AddItem(a, TItemPos(INVENTORY, 0), UINT32_MAX), "huge display rejected");
    Check(!ExchangeSystem::AddItem(a, TItemPos(INVENTORY, 0), EXCHANGE_ITEM_MAX_NUM - 1), "vertical overflow rejected");
    Offer(a, tall);
#ifdef __NEW_EXCHANGE_WINDOW__
    constexpr int columns = 6;
#else
    constexpr int columns = 4;
#endif
    Check(!ExchangeSystem::AddItem(a, TItemPos(INVENTORY, 1), columns), "overlapping display rejected");
    Check(!ExchangeSystem::RemoveItem(a, UINT32_MAX), "huge remove index rejected");
    Check(ExchangeSystem::RemoveItem(a, 0), "remove offer");
    Check(!ItemSystem::IsItemExchanging(tall), "remove unlocks");
    Offer(a, other); Check(ExchangeSystem::Accept(b), "other accepts");
    Check(!ExchangeSystem::RemoveItem(a, 0), "accepted other prevents change");
    ExchangeSystem::Cancel(b);
}
void TransferTests() {
    Reset(); auto a = ActorEntity(), b = ActorEntity();
    auto x = Item(a, 0, 2), y = Item(b, 0, 2);
    auto& quick = g_registry.get<ecs::QuickSlots>(a); quick.slots[0] = {QUICKSLOT_TYPE_ITEM, 0};
    Check(ExchangeSystem::Start(a, b), "transfer start"); Offer(a, x); Offer(b, y);
    Check(ecs::PointSystem::GetGold(a) == 10000 && ecs::PointSystem::GetGold(b) == 10000, "fixture gold initialized");
    Check(ExchangeSystem::AddGold(a, 2000), "first gold offer");
    Check(ExchangeSystem::AddGold(b, 3000), "second gold offer");
    auto verify = [&] {
        Check(ItemSystem::GetItemOwner(x) == b && ItemSystem::GetItemOwner(y) == a, "callbacks see both committed owners");
        Check(ecs::PointSystem::GetGold(a) == 11000 && ecs::PointSystem::GetGold(b) == 9000, "callbacks see both balances");
        Check(ItemSystem::GetItem(a, TItemPos(INVENTORY, 0)) == y, "incoming reused outgoing cell");
        Check(g_registry.get<ecs::QuickSlots>(a).slots[0].type == QUICKSLOT_TYPE_NONE, "old quickslot removed before callback");
    };
    onSave = verify; onQuickslot = verify;
    Check(Finish(a, b), "complete swap"); verify(); AssertClosed(a, b);
    Check(saves == 2 && characterSaves == 2 && itemLogs == 4 && goldLogs == 4, "persistence and logs");
    Check(quickDeletes.size() == 1, "quickslot publication");
    Check(g_registry.get<ecs::ItemOwner>(x).ownerPID == A(b).pid &&
        g_registry.get<ecs::ItemOwner>(x).lastOwnerPID == A(b).pid, "persistent owner metadata");

    Reset(); a = ActorEntity(); b = ActorEntity();
    std::vector<entt::entity> left, right;
    for (int cell = 0; cell < INVENTORY_MAX_NUM; ++cell) { left.push_back(Item(a, cell)); right.push_back(Item(b, cell)); }
    Check(ExchangeSystem::Start(a, b), "full inventories start"); Offer(a, left[0]); Offer(b, right[0]);
    Check(Finish(a, b), "two full inventories swap using freed cells");
    Check(ItemSystem::GetItemOwner(left[0]) == b && ItemSystem::GetItemOwner(right[0]) == a, "full swap owners");
    AssertClosed(a, b);

    Reset(); a = ActorEntity(); b = ActorEntity(); x = Item(a);
    for (int cell = 0; cell < INVENTORY_MAX_NUM; ++cell) Item(b, cell);
    Check(ExchangeSystem::Start(a, b), "no space start"); Offer(a, x);
    Check(ExchangeSystem::AddGold(b, 1), "gold for no-space item");
    Check(!Finish(a, b), "no space abort");
    Check(ItemSystem::GetItemOwner(x) == a && ecs::PointSystem::GetGold(a) == 10000 && saves == 0, "no partial gold/item transfer");
    AssertClosed(a, b);
}
void GoldTests() {
    Reset(); auto a = ActorEntity(), b = ActorEntity(); Check(ExchangeSystem::Start(a, b), "gold start");
    for (const int64_t value : {INT64_MIN, int64_t(-1), int64_t(0), int64_t(GOLD_MAX), INT64_MAX})
        Check(!ExchangeSystem::AddGold(a, value), "gold bounds");
    Check(!ExchangeSystem::AddGold(a, 10001), "insufficient gold");
    g_registry.get<ecs::GoldAmount>(b).amount = GOLD_MAX - 5;
    Check(!ExchangeSystem::AddGold(a, 5), "recipient cap");
    g_registry.get<ecs::GoldAmount>(b).amount = 10000;
    Check(ExchangeSystem::AddGold(a, 5000), "valid gold");
    Check(!ExchangeSystem::AddGold(a, 1), "cannot overwrite gold offer");
    g_registry.get<ecs::GoldAmount>(a).amount = 4999;
    Check(!Finish(a, b), "balance changed after offer");
    Check(ecs::PointSystem::GetGold(b) == 10000 && saves == 0, "failed gold untouched"); AssertClosed(a, b);
}
void InventoryKinds() {
    // A tall item must not straddle pages, even if the corresponding cells
    // happen to be free. An extension changes capacity, not page geometry.
    Reset(); auto left = ActorEntity(), right = ActorEntity();
    auto tall = Item(left, 0, 2);
    for (int cell = 0; cell < INVENTORY_PAGE_SIZE; ++cell)
        if (cell != INVENTORY_PAGE_SIZE - 1) Item(right, cell);
    Check(ExchangeSystem::Start(left, right), "page edge start"); Offer(left, tall);
    Check(Finish(left, right), "page edge transfer");
    Check(ItemSystem::GetItemCell(tall) == INVENTORY_PAGE_SIZE, "tall item skips cross-page hole");
    Check(ExchangeSystem::GetLastExchangePulse(left) == pulse, "native exchange cooldown");

    Reset(); left = ActorEntity(); right = ActorEntity(); tall = Item(left, 0, 2);
    A(right).inventorySize = INVENTORY_PAGE_SIZE;
    for (int cell = 0; cell < INVENTORY_PAGE_SIZE; ++cell)
        if (cell != INVENTORY_PAGE_SIZE - 1) Item(right, cell);
    Check(ExchangeSystem::Start(left, right), "capacity edge start"); Offer(left, tall);
    Check(!Finish(left, right), "locked main pages cannot receive item");
    Check(ItemSystem::GetItemOwner(tall) == left, "main limit leaves owner unchanged");

#ifdef ENABLE_EXTRA_INVENTORY
    for (int category = 0; category < 6; ++category) {
        Reset(); const auto a = ActorEntity(), b = ActorEntity();
        const auto item = Item(a, category * EXTRA_INVENTORY_CATEGORY_MAX_NUM, 2, EXTRA_INVENTORY);
        Check(ExchangeSystem::Start(a, b), "extra start"); Offer(a, item);
        Check(Finish(a, b), "extra transfer"); Check(ItemSystem::GetItemOwner(item) == b, "extra owner");
        Check(ItemSystem::GetItemCell(item) == category * EXTRA_INVENTORY_CATEGORY_MAX_NUM, "extra category retained");
        Check(ItemSystem::GetItemWindow(item) == EXTRA_INVENTORY, "extra window retained");
    }
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
    Reset(); auto a = ActorEntity(), b = ActorEntity(); A(b).unlocked = 0;
    auto item = Item(a, 0, 1, EXTRA_INVENTORY);
    for (int cell = 0; cell < EXTRA_INVENTORY_PAGE_SIZE * 2 + 20; ++cell) Item(b, cell, 1, EXTRA_INVENTORY);
    Check(ExchangeSystem::Start(a, b), "locked extra start"); Offer(a, item);
    Check(!Finish(a, b), "locked extra slots not usable");
    Check(ItemSystem::GetItemOwner(item) == a, "locked failure retains owner"); AssertClosed(a, b);
#endif
#endif
    Reset(); auto a2 = ActorEntity(), b2 = ActorEntity();
    auto dragon = Item(a2, 0, 1, DRAGON_SOUL_INVENTORY); I(dragon).dragonBase = DRAGON_SOUL_BOX_SIZE;
    Check(ExchangeSystem::Start(a2, b2), "dragon start"); Offer(a2, dragon);
    Check(Finish(a2, b2), "dragon transfer");
    Check(ItemSystem::GetItemWindow(dragon) == DRAGON_SOUL_INVENTORY &&
        ItemSystem::GetItemCell(dragon) == DRAGON_SOUL_BOX_SIZE, "dragon box selection");
}
void ReentrancyTests() {
    Reset(); auto a = ActorEntity(), b = ActorEntity(); auto x = Item(a), y = Item(b);
    Check(ExchangeSystem::Start(a, b), "reentrant start"); Offer(a, x); Offer(b, y);
    bool called = false;
    onSave = [&] {
        if (called) return; called = true;
        Check(ItemSystem::GetItemOwner(x) == b && ItemSystem::GetItemOwner(y) == a, "atomic before first save");
        g_registry.destroy(b);
    };
    Check(Finish(a, b), "committed trade survives participant destruction");
    AssertClosed(a, b); Check(called, "destruction callback exercised");

    Reset(); a = ActorEntity(); b = ActorEntity(); x = Item(a);
    onPacket = [&](auto, auto h, auto sub) {
        if (h == HEADER_GC_EXCHANGE && sub == EXCHANGE_SUBHEADER_GC_START) ExchangeSystem::Cancel(a);
    };
    Check(!ExchangeSystem::Start(a, b), "start callback cancellation"); AssertClosed(a, b);
    onPacket = {};
    Check(ExchangeSystem::Start(a, b), "offer cancel start");
    onPacket = [&](auto, auto h, auto sub) {
        if (h == HEADER_GC_EXCHANGE && sub == EXCHANGE_SUBHEADER_GC_ACCEPT) ExchangeSystem::Cancel(a);
    };
    Offer(a, x); AssertClosed(a, b); Check(!ItemSystem::IsItemExchanging(x), "offer callback unlock");

#ifdef ENABLE_EXTRA_INVENTORY
    Reset(); a = ActorEntity(); b = ActorEntity(); x = Item(a, 0, 1, EXTRA_INVENTORY);
    entt::entity replacement = entt::null;
    Check(ExchangeSystem::Start(a, b), "plan callback start"); Offer(a, x);
    onFlag = [&] { if (replacement == entt::null) replacement = Item(b, 20); };
    Check(!Finish(a, b), "inventory changed while planning aborts");
    Check(ItemSystem::GetItem(b, TItemPos(INVENTORY, 20)) == replacement, "plan cannot overwrite callback item");
    Check(ItemSystem::GetItemOwner(x) == a, "planned item stays with owner");
#endif
}
void RetiredCardItemsAreOrdinary() {
    // Account trading was removed upstream. Existing item vnums must transfer
    // as ordinary items, without retirement or a credit/account DB request.
    for (const uint32_t vnum : {90008u, 90009u}) {
        Reset();
        const auto a = ActorEntity(), b = ActorEntity(), item = Item(a);
        g_registry.get<ecs::ItemIdentity>(item).vnum = vnum;
        g_registry.get<ecs::ItemSockets>(item).sockets[0] = 42;
        Check(ExchangeSystem::Start(a, b), "retired card item trade start"); Offer(a, item);
        Check(Finish(a, b), "retired card item trade");
        Check(g_registry.valid(item) && ItemSystem::GetItemOwner(item) == b,
            "removed card behavior consumed the transferred item");
        Check(g_registry.get<ecs::ItemSockets>(item).sockets[0] == 42, "ordinary item socket changed");
        AssertClosed(a, b);
    }
}

void AdditionalFailureTests() {
    for (int scenario = 0; scenario < 6; ++scenario) {
        Reset(); auto a = ActorEntity(), b = ActorEntity(), item = Item(a);
        Check(ExchangeSystem::Start(a, b), "late validation start"); Offer(a, item);
        auto* database = db_clientdesc;
        switch (scenario) {
        case 0: noQuest = true; break;
        case 1: db_clientdesc = nullptr; break;
        case 2: g_registry.remove<ecs::DragonSoulInventoryComponent>(b); break;
        case 3: g_registry.get<ecs::WarpBlockState>(b).safeboxLoadTime = pulse; break;
        case 4: A(b).canHandle = false; break;
        case 5: g_registry.get<ecs::MainInventoryRuntimeComponent>(a).items[10] = item; break;
        }
        Check(!Finish(a, b), "late validation abort"); db_clientdesc = database;
        Check(ItemSystem::GetItemOwner(item) == a && saves == 0, "late failure has no item effects");
        AssertClosed(a, b);
    }
    Reset(); auto a = ActorEntity(), b = ActorEntity(), x = Item(a), y = Item(b);
    Check(ExchangeSystem::Start(a, b), "quickslot reentry start"); Offer(a, x); Offer(b, y);
    auto& slots = g_registry.get<ecs::QuickSlots>(a);
    slots.slots[0] = slots.slots[1] = {QUICKSLOT_TYPE_ITEM, 0};
    onQuickslot = [&] {
        auto& current = g_registry.get<ecs::QuickSlots>(a);
        current.slots[1] = {QUICKSLOT_TYPE_ITEM, 0}; ++current.revision;
    };
    Check(Finish(a, b), "quickslot reentry transfer");
    Check(quickDeletes.size() == 1 && g_registry.get<ecs::QuickSlots>(a).slots[1].type == QUICKSLOT_TYPE_ITEM,
        "old quickslot publication cannot delete callback replacement");

    Reset(); a = ActorEntity(); b = ActorEntity(); x = Item(a);
    g_registry.get<ecs::GoldAmount>(a).amount = 10000000000LL;
    Check(ExchangeSystem::Start(a, b), "64-bit gold start");
    Check(ExchangeSystem::AddGold(a, 5000000000LL), "offer above 32-bit boundary");
    Check(Finish(a, b), "64-bit gold complete");
    Check(ecs::PointSystem::GetGold(a) == 5000000000LL && ecs::PointSystem::GetGold(b) == 5000010000LL,
        "64-bit balances preserved");
}
}
int main() {
    try {
        CLIENT_DESC db; db_clientdesc = &db;
        CHARACTER_MANAGER characters; DSManager dragons; quest::CQuestManager quests; LogManager logs;
        LifecycleTests(); StartGuards(); OffersAndValidation(); TransferTests(); GoldTests();
        InventoryKinds(); ReentrancyTests(); RetiredCardItemsAreOrdinary(); AdditionalFailureTests();
        Reset(); db_clientdesc = nullptr;
        std::cout << "Exchange tests passed: " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "Exchange test failed: " << error.what() << '\n'; return 1; }
}
