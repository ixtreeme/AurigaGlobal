#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/social/priv_manager.h"
#include "../../SRC/Server/GameServer/guild/guild_manager.h"
#include "../../SRC/Server/GameServer/network/desc_client.h"
#include "../../SRC/Server/GameServer/core/log.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include <Core/Logging.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
namespace {
int checks = 0, dbToken = 0;
int64_t now = 0;
uint8_t lastHeader = 0;
std::vector<char> lastPacket;
void Check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected privilege service"); }
template<class T> T Packet(uint8_t header) {
    Check(lastHeader == header && lastPacket.size() == sizeof(T), "wrong privilege request frame");
    T result {}; std::memcpy(&result, lastPacket.data(), sizeof(result)); return result;
}
}
CLIENT_DESC* db_clientdesc = reinterpret_cast<CLIENT_DESC*>(&dbToken);
time_t get_global_time() { return now; }
int MINMAX(int min, int value, int max) { return std::clamp(value, min, max); }
int MAX(int a, int b) { return std::max(a, b); }
void CLIENT_DESC::DBPacket(uint8_t header, uint32_t, const void* data, uint32_t size) {
    Check(this == db_clientdesc, "privilege request sent to wrong link");
    lastHeader = header;
    const auto* bytes = static_cast<const char*>(data); lastPacket.assign(bytes, bytes + size);
}
CGuildManager::CGuildManager() = default;
CGuildManager::~CGuildManager() = default;
CGuild* CGuildManager::FindGuild(uint32_t) { return nullptr; }
void SendNoticeNew(uint8_t, uint8_t, int32_t, uint32_t, const char*, ...) {}
void LogManager::CharLog(uint32_t, uint32_t, uint32_t, uint32_t, const char*, const char*, const char*) { Unexpected(); }
const char* c_apszEmpireNames[EMPIRE_MAX_NUM] {"all", "one", "two", "three"};
const char* c_apszPrivNames[MAX_PRIV_NUM] {};
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity) { Unexpected(); }
uint8_t ecs::PlayerRuntime::GetEmpire(entt::entity) { Unexpected(); }
bool ItemSystem::IsEquipUniqueItem(entt::entity, uint32_t) { Unexpected(); }
CGuild* ecs::SocialSystem::GetGuild(entt::entity) { Unexpected(); }
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("privilege-time-test"); return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }
int main() {
    try {
        CGuildManager guilds; CPrivManager privileges;
        constexpr int64_t week = 7 * 24 * 60 * 60;
        for (const int64_t timestamp : {int64_t{2147483648}, int64_t{4294967296}, int64_t{1} << 40, INT64_MAX - week}) {
            now = timestamp;
            privileges.GiveGuildPriv(42, 1, 25, 0, now + 3600);
            privileges.GiveEmpirePriv(1, 1, 50, 0, now + 3600);
            Check(privileges.GetPrivByGuildEx(42, 1)->end_time_sec == now + 3600 &&
                privileges.GetPrivByEmpireEx(1, 1)->m_end_time_sec == now + 3600,
                "GameServer privilege deadline narrowed through int clamp");
            privileges.GiveGuildPriv(42, 1, 25, 0, INT64_MAX);
            privileges.GiveEmpirePriv(1, 1, 50, 0, INT64_MAX);
            Check(privileges.GetPrivByGuildEx(42, 1)->end_time_sec == now + week &&
                privileges.GetPrivByEmpireEx(1, 1)->m_end_time_sec == now + week,
                "privilege maximum duration policy lost");
        }
        for (const int64_t duration : {INT64_MIN, int64_t{-1}, int64_t{3600}, INT64_MAX}) {
            privileges.RequestGiveGuildPriv(42, 1, 25, duration);
            Check(Packet<TPacketGiveGuildPriv>(HEADER_GD_REQUEST_GUILD_PRIV).duration_sec ==
                std::clamp<int64_t>(duration, 0, week), "guild duration narrowed before clamp");
            privileges.RequestGiveEmpirePriv(1, 1, 50, duration);
            Check(Packet<TPacketGiveEmpirePriv>(HEADER_GD_REQUEST_EMPIRE_PRIV).duration_sec ==
                std::clamp<int64_t>(duration, 0, week), "empire duration narrowed before clamp");
        }
        now = INT64_MAX - 10;
        privileges.GiveGuildPriv(42, 1, 25, 0, INT64_MAX);
        privileges.GiveEmpirePriv(1, 1, 50, 0, INT64_MAX);
        Check(privileges.GetPrivByGuildEx(42, 1)->end_time_sec == INT64_MAX &&
            privileges.GetPrivByEmpireEx(1, 1)->m_end_time_sec == INT64_MAX, "privilege time addition overflowed");
        privileges.GiveGuildPriv(42, 1, 25, 0, INT64_MIN);
        Check(privileges.GetPrivByGuildEx(42, 1)->end_time_sec == 0, "negative privilege timestamp wrapped");
        std::cout << "Privilege time checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
