#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"

#include "../../SRC/Server/GameServer/guild/guild_manager.h"
#include "../../SRC/Server/GameServer/world/war_map.h"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ViewSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/social/exchange.h"
#include "../../SRC/Server/GameServer/social/party.h"
#include "../../SRC/Server/GameServer/social/shop.h"
#include "../../SRC/Server/GameServer/social/shop_manager.h"
#include "../../SRC/Server/GameServer/dungeon/dungeon.h"
#include "../../SRC/Server/GameServer/social/wedding.h"
#include "../../SRC/Server/GameServer/social/marriage.h"
#include "../../SRC/Server/GameServer/social/banword.h"
#include "../../SRC/Server/GameServer/core/log.h"
#include "../../SRC/Server/GameServer/network/desc.h"
#include "../../SRC/Server/GameServer/network/desc_client.h"
#include "../../SRC/Server/GameServer/quest/questmanager.h"
#include "../../SRC/Server/GameServer/entity/char_manager.h"
#include "../../SRC/Server/GameServer/combat/affect.h"
#include <Core/Logging.hpp>
#include <Core/heart.h>

#include <iostream>
#include <map>
#include <stdexcept>

entt::registry g_registry;
entt::dispatcher g_dispatcher;
CLIENT_DESC* db_clientdesc = nullptr;
int passes_per_sec = 25;
int thecore_pulse() { return 0; }
void ContinueOnFatalError() {}
LPHEART thecore_heart = nullptr;
int number_ex(int from, int, const char*, int) { return from; }

namespace logging {
std::shared_ptr<spdlog::logger> GetLogger() { static auto log = std::make_shared<spdlog::logger>("social-test"); return log; }
std::shared_ptr<spdlog::logger> GetErrorLogger() { return GetLogger(); }
}

// The services the relation validation resolves against. The singleton base
// records them so SocialSystem::GetGuild/GetWarMap can call instance().
CGuildManager guildManager;
CWarMapManager warMapManager;

namespace {

std::map<uint32_t, CGuild*> guildRegistry;
std::map<int32_t, CWarMap*> warMapRegistry;
uint32_t lastGuildQuery = 0;
int32_t lastWarMapQuery = 0;

int checks = 0;
void Check(bool condition, const char* why) { ++checks; if (!condition) throw std::runtime_error(why); }
[[noreturn]] void Unexpected(const char* what) { throw std::runtime_error(what); }

} // namespace

// The guild and war map stay services, so the fixture answers their lookups
// from registries the test fills. Everything else in SocialSystem.cpp is a
// fail-fast seam: the tests below only select/release relations.
CGuildManager::CGuildManager() {}
CGuildManager::~CGuildManager() {}
CGuild* CGuildManager::FindGuild(uint32_t guild_id)
{
    lastGuildQuery = guild_id;
    const auto it = guildRegistry.find(guild_id);
    return it == guildRegistry.end() ? nullptr : it->second;
}

CWarMapManager::CWarMapManager() {}
CWarMapManager::~CWarMapManager() {}
CWarMap* CWarMapManager::Find(int32_t lMapIndex)
{
    lastWarMapQuery = lMapIndex;
    const auto it = warMapRegistry.find(lMapIndex);
    return it == warMapRegistry.end() ? nullptr : it->second;
}

int CWarMap::GetMapIndex() { Unexpected("war map index read from a fixture pointer"); }
void CWarMap::IncMember(entt::entity) { Unexpected("war map membership entered"); }
void CWarMap::DecMember(entt::entity) { Unexpected("war map membership left"); }

// Unrelated service leaves: compile-only doubles for paths these tests never
// select. The fixture is the relation validation, not those subsystems.
bool ecs::PlayerRuntime::IsValid(entt::entity e) { return g_registry.valid(e); }
bool ecs::PlayerRuntime::IsPC(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<ecs::TagPC>(e); }
bool ecs::PlayerRuntime::IsMonster(entt::entity) { Unexpected("IsMonster"); }
bool ecs::PlayerRuntime::IsReviver(entt::entity) { Unexpected("IsReviver"); }
bool ecs::PlayerRuntime::IsObserverMode(entt::entity) { Unexpected("IsObserverMode"); }
bool ecs::PlayerRuntime::IsBlockMode(entt::entity, uint8_t) { Unexpected("IsBlockMode"); }
DESC* ecs::PlayerRuntime::GetDesc(entt::entity) { return nullptr; }
std::string_view ecs::PlayerRuntime::GetName(entt::entity) { return {}; }
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity e)
{
    const auto* id = g_registry.valid(e) ? g_registry.try_get<ecs::PlayerID>(e) : nullptr;
    return id ? id->pid : 0;
}
uint32_t ecs::PlayerRuntime::GetPacketVID(entt::entity) { Unexpected("GetPacketVID"); }
int32_t ecs::PlayerRuntime::GetMapIndex(entt::entity) { Unexpected("GetMapIndex"); }
uint8_t ecs::PlayerRuntime::GetJob(entt::entity) { Unexpected("GetJob"); }
uint8_t ecs::PlayerRuntime::GetGMLevel(entt::entity) { Unexpected("GetGMLevel"); }
const TMobTable* ecs::PlayerRuntime::GetMobTable(entt::entity) { Unexpected("GetMobTable"); }
int ecs::PlayerRuntime::GetDuelOption(entt::entity, const char*) { Unexpected("GetDuelOption"); }

int64_t ecs::PointSystem::GetGold(entt::entity) { Unexpected("GetGold"); }
void ecs::PointSystem::Change(entt::entity, uint8_t, int64_t, bool, bool, bool) { Unexpected("PointSystem::Change"); }
int ecs::PointSystem::GetLevel(entt::entity) { Unexpected("GetLevel"); }

bool ItemSystem::IsValidItem(entt::entity) { return false; }
bool ItemSystem::IsItemEquipped(entt::entity) { Unexpected("IsItemEquipped"); }
bool ItemSystem::IsItemLocked(entt::entity) { Unexpected("IsItemLocked"); }
bool ItemSystem::UnlockItem(entt::entity) { Unexpected("UnlockItem"); }
bool ItemSystem::SetItemSocket(entt::entity, int, uint32_t, bool) { Unexpected("SetItemSocket"); }
bool ItemSystem::RemoveSpecifyItemEcs(entt::entity, uint32_t, uint32_t, bool) { Unexpected("RemoveSpecifyItemEcs"); }
int ItemSystem::CountItem(entt::entity, uint32_t) { Unexpected("CountItem"); }
entt::entity ItemSystem::GetItem(entt::entity, TItemPos) { Unexpected("ItemSystem::GetItem"); }
entt::entity ItemSystem::FindItemByID(entt::entity, uint32_t) { Unexpected("FindItemByID"); }
const char* ItemSystem::GetItemName(entt::entity) { Unexpected("GetItemName"); }
const TItemTable* ItemSystem::GetItemProto(entt::entity) { Unexpected("GetItemProto"); }
uint32_t ItemSystem::GetItemID(entt::entity) { Unexpected("GetItemID"); }
uint32_t ItemSystem::GetItemVnum(entt::entity) { Unexpected("GetItemVnum"); }
uint32_t ItemSystem::GetItemCount(entt::entity) { Unexpected("GetItemCount"); }

bool InventorySystem::CanHandleItems(entt::entity, bool, bool) { Unexpected("CanHandleItems"); }
entt::entity InventorySystem::GetRefineNPC(entt::entity) { Unexpected("GetRefineNPC"); }

CAffect* AffectSystem::FindAffect(entt::entity, uint32_t, uint8_t) { Unexpected("FindAffect"); }
bool AffectSystem::RemoveAffect(entt::entity, uint32_t) { Unexpected("RemoveAffect"); }
bool AffectSystem::IsPolymorphed(entt::entity) { Unexpected("IsPolymorphed"); }
void AffectSystem::SetPolymorph(entt::entity, uint32_t, bool) { Unexpected("SetPolymorph"); }

bool CombatSystem::IsDead(entt::entity) { Unexpected("CombatSystem::IsDead"); }

entt::entity MountSystem::GetSummonedHorse(entt::entity) { Unexpected("GetSummonedHorse"); }
uint32_t MountSystem::GetMountVnum(entt::entity) { Unexpected("GetMountVnum"); }
void MountSystem::SummonHorse(entt::entity, bool, bool, uint32_t, const char*) { Unexpected("SummonHorse"); }

void ecs::ViewSystem::PacketView(entt::entity, const void*, int, entt::entity) { Unexpected("PacketView"); }
void NetworkSyncSystem::UpdatePacket(entt::entity) { Unexpected("UpdatePacket"); }
void ecs::ChatSystem::Send(entt::entity, uint8_t, const char*, ...) { Unexpected("ChatSystem::Send"); }
void ecs::ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, const char*, ...) { Unexpected("ChatSystem::SendNew"); }

entt::entity PartySystem::GetCharacterParty(entt::entity) { Unexpected("GetCharacterParty"); }
entt::entity PartySystem::GetLeader(entt::entity) { Unexpected("GetLeader"); }
uint32_t PartySystem::GetLeaderPID(entt::entity) { Unexpected("GetLeaderPID"); }
uint32_t PartySystem::GetMemberCount(entt::entity) { Unexpected("GetMemberCount"); }
bool PartySystem::IsValid(entt::entity) { Unexpected("PartySystem::IsValid"); }
void PartySystem::Join(entt::entity, uint32_t) { Unexpected("PartySystem::Join"); }
void PartySystem::Link(entt::entity, entt::entity) { Unexpected("PartySystem::Link"); }
void PartySystem::SendPartyInfoAllToOne(entt::entity, entt::entity) { Unexpected("SendPartyInfoAllToOne"); }
bool ShopSystem::IsValid(entt::entity) { Unexpected("ShopSystem::IsValid"); }

void DungeonSystem::SetMemberDungeon(entt::entity, entt::entity) { Unexpected("SetMemberDungeon"); }
entt::entity DungeonSystem::GetMemberDungeon(entt::entity) { Unexpected("GetMemberDungeon"); }
void DungeonSystem::ClearMemberDungeonIfOtherMap(entt::entity, int32_t) { Unexpected("ClearMemberDungeonIfOtherMap"); }

void marriage::WeddingSystem::SetMemberMap(entt::entity, entt::entity) { Unexpected("SetMemberMap"); }
entt::entity marriage::WeddingSystem::GetMemberMap(entt::entity) { Unexpected("GetMemberMap"); }
entt::entity marriage::CManager::Get(uint32_t) { return entt::null; }
int marriage::MarriageSystem::GetBonus(entt::entity, uint32_t, bool, entt::entity) { Unexpected("MarriageSystem::GetBonus"); }

void ExchangeSystem::Cancel(entt::entity) { Unexpected("ExchangeSystem::Cancel"); }
bool CBanwordManager::CheckString(const char*, size_t) { return false; }
void LogManager::CharLog(entt::entity, uint32_t, const char*, const char*) {}
void DESC::Packet(const void*, int) {}
void CLIENT_DESC::DBPacket(uint8_t, uint32_t, const void*, uint32_t) {}
entt::entity CPartyManager::CreateParty(entt::entity) { Unexpected("CreateParty"); }
entt::entity CShopManager::CreatePCShop(entt::entity, TShopItemTable*, uint8_t) { Unexpected("CreatePCShop"); }
void CShopManager::DestroyPCShop(entt::entity) { Unexpected("DestroyPCShop"); }
quest::PC* quest::CQuestManager::GetPCForce(unsigned int) { Unexpected("GetPCForce"); }
entt::entity CHARACTER_MANAGER::FindEntityByPID(uint32_t) { Unexpected("FindEntityByPID"); }

namespace {

void GuildRelation() {
    g_registry.clear();
    const auto e = g_registry.create();
    Check(!ecs::SocialSystem::GetGuild(entt::null) && !ecs::SocialSystem::GetGuild(e),
        "empty character reported a guild");

    // F1: the stored pointer must never be dereferenced to find its key. A
    // freed-looking sentinel with no durable id answers null.
    auto* stored = reinterpret_cast<CGuild*>(static_cast<uintptr_t>(0x10));
    auto& refs = g_registry.emplace<ecs::SocialRefs>(e);
    refs.guild = stored;
    refs.guildId = 0;
    lastGuildQuery = 0;
    Check(!ecs::SocialSystem::GetGuild(e), "pointer without a durable id was trusted");
    Check(lastGuildQuery == 0, "zero guild id reached the lookup");

    // The lookup key must come from the id, and a disbanded guild (a lookup
    // miss) must not resurrect the stored pointer.
    refs.guildId = 77;
    guildRegistry.clear();
    Check(!ecs::SocialSystem::GetGuild(e) && lastGuildQuery == 77,
        "disbanded guild pointer was trusted after a lookup miss");

    guildRegistry[77] = stored;
    Check(ecs::SocialSystem::GetGuild(e) == stored, "live guild relation was rejected");

    // A replacement at the same id must not match the old pointer.
    guildRegistry[77] = reinterpret_cast<CGuild*>(static_cast<uintptr_t>(0x20));
    Check(!ecs::SocialSystem::GetGuild(e), "stale guild pointer was trusted after a replacement");

    guildRegistry.clear();
    g_registry.clear();
}

void WarMapRelation() {
    g_registry.clear();
    const auto e = g_registry.create();
    auto* stored = reinterpret_cast<CWarMap*>(static_cast<uintptr_t>(0x30));
    auto& membership = g_registry.emplace<ecs::DungeonMembership>(e);
    membership.warMap = stored;
    membership.warMapIndex = 0;
    Check(!ecs::SocialSystem::GetWarMap(e), "war map without a durable index was trusted");

    membership.warMapIndex = 111;
    warMapRegistry.clear();
    Check(!ecs::SocialSystem::GetWarMap(e) && lastWarMapQuery == 111,
        "destroyed war map pointer was trusted after a lookup miss");

    warMapRegistry[111] = stored;
    Check(ecs::SocialSystem::GetWarMap(e) == stored, "live war map relation was rejected");

    warMapRegistry[111] = reinterpret_cast<CWarMap*>(static_cast<uintptr_t>(0x40));
    Check(!ecs::SocialSystem::GetWarMap(e), "stale war map pointer was trusted after a replacement");

    // Releasing the relation must not call into the stale map and must reset
    // the durable index with the pointer.
    warMapRegistry.clear();
    membership.warMap = stored;
    membership.warMapIndex = 111;
    ecs::SocialSystem::SetWarMap(e, nullptr);
    const auto* after = g_registry.try_get<ecs::DungeonMembership>(e);
    Check(after && after->warMap == nullptr && after->warMapIndex == 0,
        "clearing the war map relation left stale state");

    warMapRegistry.clear();
    g_registry.clear();
}

} // namespace

int main() {
    try {
        GuildRelation();
        WarMapRelation();
        std::cout << "Social relation checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
