#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/guild/guild.h"
#include "../../SRC/Server/GameServer/guild/guild_manager.h"
#include "../../SRC/Server/GameServer/entity/char_manager.h"
#include "../../SRC/Server/GameServer/network/db.h"
#include "../../SRC/Server/GameServer/network/desc.h"
#include "../../SRC/Server/GameServer/network/desc_client.h"
#include "../../SRC/Server/GameServer/network/p2p.h"
#include "../../SRC/Server/GameServer/world/building.h"
#include "../../SRC/Server/GameServer/world/event.h"
#include "../../SRC/Server/GameServer/quest/questmanager.h"
#include "../../SRC/Server/GameServer/combat/skill.h"
#include "../../SRC/Server/GameServer/core/config.h"
#include "../../SRC/Server/GameServer/core/constants.h"
#include "../../SRC/Server/GameServer/core/packet.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/QuestSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SkillSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/SQL/AsyncSQL.h"
#include <Core/Logging.hpp>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// The production guild.cpp, driven through its member rows: the DB load and
// reload, adding and removing members, the member records the client reads,
// the per-member updates and the rows' lifetime with the guild. Characters
// are bare entities; the DB, descriptors and the other managers are
// recording doubles, and everything the member paths do not reach aborts.
entt::registry g_registry;
entt::dispatcher g_dispatcher;
int passes_per_sec = 25;
int test_server = 0;
bool g_bGuildInfiniteMembers = false;
bool g_bGuildInviteLimit = false;
uint8_t g_bChannel = 1;
const uint32_t guild_exp_table2[GUILD_MAX_LEVEL + 1] = {};
namespace { int dbMarker = 0; }
LPCLIENT_DESC db_clientdesc = reinterpret_cast<LPCLIENT_DESC>(&dbMarker);

namespace {
int checks = 0;
std::vector<CFuncQueryInfo*> pendingQueries;
std::vector<std::string> pendingQueryText;
std::vector<std::string> queries;
std::vector<uint8_t> dbHeaders;
std::map<LPDESC, std::vector<uint8_t>> sent;
std::map<uint32_t, CGuild*> links;
std::vector<uint32_t> unlinks;
std::map<entt::entity, CGuild*> guildOf;
std::unordered_map<entt::entity, uint32_t> playerIds;
std::unordered_map<entt::entity, std::string> names;
std::unordered_map<uint32_t, entt::entity> online;
std::vector<MYSQL_ROW> resultRows;
size_t resultCursor = 0;

void Check(bool value, const char* message)
{
    ++checks;
    if (!value) throw std::runtime_error(message);
}

[[noreturn]] void Unexpected(const char* service)
{
    std::cerr << "Unexpected service in a guild member test: " << service << "\n";
    std::abort();
}

LPDESC DescOf(entt::entity e) { return reinterpret_cast<LPDESC>(static_cast<uintptr_t>(0x1000 + entt::to_integral(e))); }
}

CHARACTER_MANAGER character_manager;
CGuildManager guild_manager;
P2P_MANAGER p2p_manager;
building::CManager building_manager;
DBManager db_manager;

// --- recording doubles --------------------------------------------------------
std::shared_ptr<spdlog::logger> logging::GetErrorLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("guild-error-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("guild-test");
    return logger;
}
int MIN(int a, int b) { return a < b ? a : b; }
int MAX(int a, int b) { return a > b ? a : b; }
int MINMAX(int min, int value, int max) { const int tv = min > value ? min : value; return max < tv ? max : tv; }
time_t get_global_time() { return 1'000'000; }
const char* get_table_postfix() { return ""; }

CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() = default;
CAsyncSQL::~CAsyncSQL() = default;
DBManager::DBManager() = default;
DBManager::~DBManager() = default;
// FuncQuery hands its callback here; the test answers it with rows of its own.
void CAsyncSQL::ReturnQuery(const char* query, void* userData)
{
    pendingQueries.push_back(static_cast<CFuncQueryInfo*>(userData));
    pendingQueryText.emplace_back(query);
}
void DBManager::Query(const char* format, ...)
{
    char text[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    queries.emplace_back(text);
}
MYSQL_ROW STDCALL mysql_fetch_row(MYSQL_RES*) { return resultCursor < resultRows.size() ? resultRows[resultCursor++] : nullptr; }
void STDCALL mysql_free_result(MYSQL_RES*) {}
// The rows come back through SQLResult::FetchRow, as in AsyncSQL.cpp.
MYSQL_ROW SQLResult::FetchRow() { return pSQLResult ? mysql_fetch_row(pSQLResult) : nullptr; }

void CLIENT_DESC::DBPacket(uint8_t header, uint32_t, const void*, uint32_t) { dbHeaders.push_back(header); }
void DESC::BufferedPacket(const void* data, int size)
{
    auto& stream = sent[this];
    stream.insert(stream.end(), static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
}
void DESC::Packet(const void* data, int size) { BufferedPacket(data, size); }

CHARACTER_MANAGER::CHARACTER_MANAGER() = default;
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
entt::entity CHARACTER_MANAGER::FindEntityByPID(uint32_t pid)
{
    const auto it = online.find(pid);
    return it == online.end() ? entt::null : it->second;
}
CGuildManager::CGuildManager() = default;
CGuildManager::~CGuildManager() = default;
void CGuildManager::Link(uint32_t pid, CGuild* guild) { links[pid] = guild; }
void CGuildManager::Unlink(uint32_t pid) { unlinks.push_back(pid); links.erase(pid); }
P2P_MANAGER::P2P_MANAGER() = default;
P2P_MANAGER::~P2P_MANAGER() = default;
void P2P_MANAGER::Send(const void*, int, LPDESC) {}
building::CManager::CManager() = default;
building::CManager::~CManager() = default;
building::CLand* building::CManager::FindLandByGuild(uint32_t) { return nullptr; }

uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity e) { return playerIds.at(e); }
std::string_view ecs::PlayerRuntime::GetName(entt::entity e) { return names.at(e); }
LPDESC ecs::PlayerRuntime::GetDesc(entt::entity e) { return playerIds.count(e) ? DescOf(e) : nullptr; }
bool ecs::PlayerRuntime::IsBlockMode(entt::entity, uint8_t) { return false; }
void ecs::SocialSystem::SetGuild(entt::entity e, CGuild* guild) { guildOf[e] = guild; }
CGuild* ecs::SocialSystem::GetGuild(entt::entity e) { const auto it = guildOf.find(e); return it == guildOf.end() ? nullptr : it->second; }
void CGuild::SendEnemyGuild(entt::entity) {}
uint32_t CGuild::UnderAnyWar(uint8_t) { return 0; }

// --- services only the war, skill, money, buff and notice paths use ------------
#define UNEXPECTED(signature, name) signature { Unexpected(name); }
UNEXPECTED(int64_t ecs::PointSystem::Get(entt::entity, uint8_t), "PointSystem::Get")
UNEXPECTED(int64_t ecs::PointSystem::GetGold(entt::entity), "PointSystem::GetGold")
UNEXPECTED(int ecs::PointSystem::GetLevel(entt::entity), "PointSystem::GetLevel")
UNEXPECTED(void ecs::PointSystem::Change(entt::entity, uint8_t, int64_t, bool, bool, bool), "PointSystem::Change")
UNEXPECTED(void ecs::PointSystem::Compute(entt::entity), "PointSystem::Compute")
UNEXPECTED(bool AffectSystem::AddAffect(entt::entity, uint32_t, uint8_t, int, uint32_t, int, int, bool, bool), "AffectSystem::AddAffect")
UNEXPECTED(bool AffectSystem::IsAffectFlag(entt::entity, uint32_t), "AffectSystem::IsAffectFlag")
UNEXPECTED(bool AffectSystem::RemoveAffect(entt::entity, CAffect*), "AffectSystem::RemoveAffect")
UNEXPECTED(bool AffectSystem::RemoveAffect(entt::entity, uint32_t), "AffectSystem::RemoveAffect")
UNEXPECTED(CAffect* AffectSystem::FindAffect(entt::entity, uint32_t, uint8_t), "AffectSystem::FindAffect")
UNEXPECTED(bool ecs::MovementSystem::CanMove(entt::entity), "MovementSystem::CanMove")
UNEXPECTED(bool ecs::MovementSystem::WarpSet(entt::entity, int32_t, int32_t, int32_t), "MovementSystem::WarpSet")
UNEXPECTED(bool ecs::SocialSystem::CanDeposit(entt::entity), "SocialSystem::CanDeposit")
UNEXPECTED(void ecs::SocialSystem::UpdateDepositPulse(entt::entity), "SocialSystem::UpdateDepositPulse")
UNEXPECTED(int32_t ecs::PlayerRuntime::GetX(entt::entity), "PlayerRuntime::GetX")
UNEXPECTED(int32_t ecs::PlayerRuntime::GetY(entt::entity), "PlayerRuntime::GetY")
UNEXPECTED(void ecs::PlayerRuntime::SetGold(entt::entity, int64_t), "PlayerRuntime::SetGold")
UNEXPECTED(void ecs::QuestSystem::SetFlag(entt::entity, std::string_view, int64_t), "QuestSystem::SetFlag")
UNEXPECTED(int SkillSystem::ComputeSkill(entt::entity, uint32_t, entt::entity, uint8_t), "SkillSystem::ComputeSkill")
UNEXPECTED(void NetworkSyncSystem::BroadcastSpecificEffect(entt::registry&, entt::entity, const char*), "NetworkSyncSystem::BroadcastSpecificEffect")
UNEXPECTED(void NetworkSyncSystem::PointsPacket(entt::entity), "NetworkSyncSystem::PointsPacket")
UNEXPECTED(void NetworkSyncSystem::UpdatePacket(entt::entity), "NetworkSyncSystem::UpdatePacket")
UNEXPECTED(std::span<const GuildAttr> GetGuildAttributesByLevel(uint8_t), "GetGuildAttributesByLevel")
UNEXPECTED(void ecs::ChatSystem::Send(entt::entity, uint8_t, const char*, ...), "ChatSystem::Send")
UNEXPECTED(void ecs::ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, const char*, ...), "ChatSystem::SendNew")
UNEXPECTED(CGuild* CGuildManager::FindGuild(uint32_t), "CGuildManager::FindGuild")
UNEXPECTED(void CGuildManager::RemoveGuildBuff(entt::entity), "CGuildManager::RemoveGuildBuff")
UNEXPECTED(void CGuild::ChangeLadderPoint(int), "CGuild::ChangeLadderPoint")
UNEXPECTED(CSkillProto* CSkillManager::Get(uint32_t), "CSkillManager::Get")
UNEXPECTED(void building::CManager::ClearLandByGuildID(uint32_t), "building::CManager::ClearLandByGuildID")
UNEXPECTED(quest::PC* quest::CQuestManager::GetPCForce(uint32_t), "CQuestManager::GetPCForce")
UNEXPECTED(int64_t quest::CQuestManager::GetEventFlag(const std::string&), "CQuestManager::GetEventFlag")
UNEXPECTED(CCI* P2P_MANAGER::FindByPID(uint32_t), "P2P_MANAGER::FindByPID")
UNEXPECTED(SQLMsg* DBManager::DirectQuery(const char*, ...), "DBManager::DirectQuery")
UNEXPECTED(uint32_t DBManager::EscapeString(char*, uint64_t, const char*, uint32_t), "DBManager::EscapeString")
UNEXPECTED(void DBManager::SendMoneyLog(uint8_t, uint32_t, int64_t), "DBManager::SendMoneyLog")
UNEXPECTED(SQLMsg* DBManager::DirectQueryPrepared(const char*, const SqlParams&), "DBManager::DirectQueryPrepared")
UNEXPECTED(void DBManager::QueryPrepared(const char*, const SqlParams&), "DBManager::QueryPrepared")
UNEXPECTED(void CAsyncSQL::ReturnQueryPrepared(const char*, const SqlParams&, void*), "CAsyncSQL::ReturnQueryPrepared")
UNEXPECTED(double CPoly::Eval(), "CPoly::Eval")
UNEXPECTED(int CPoly::SetVar(std::string_view, double), "CPoly::SetVar")
UNEXPECTED(LPEVENT event_create_ex(TEVENTFUNC, event_info_data*, int32_t), "event_create_ex")
UNEXPECTED(void event_cancel(LPEVENT*), "event_cancel")
void intrusive_ptr_add_ref(EVENT*) {}
void intrusive_ptr_release(EVENT*) {}
#undef UNEXPECTED

namespace {
struct Member { uint32_t pid; uint8_t grade; bool general; uint32_t offer; uint8_t level; uint8_t job; std::string name; };

entt::entity Character(uint32_t pid, const char* name)
{
    const auto e = g_registry.create();
    playerIds[e] = pid;
    names[e] = name;
    online[pid] = e;
    return e;
}

size_t RowCount() { return g_registry.storage<ecs::GuildMemberRow>().size(); }

// Answers the guild's member query with the given rows, as the DB would.
void AnswerMemberQuery(const std::vector<Member>& members)
{
    size_t index = pendingQueries.size();
    for (size_t i = 0; i < pendingQueryText.size(); ++i)
        if (pendingQueryText[i].find("guild_member") != std::string::npos)
            index = i;
    Check(index < pendingQueries.size(), "the guild asked for no member rows");

    static std::vector<std::vector<std::string>> storage;
    static std::vector<std::vector<char*>> pointers;
    storage.clear();
    pointers.clear();
    for (const auto& member : members)
    {
        storage.push_back({ std::to_string(member.pid), std::to_string(member.grade), member.general ? "1" : "0",
            std::to_string(member.offer), std::to_string(member.level), std::to_string(member.job), member.name });
    }
    for (auto& row : storage)
    {
        pointers.emplace_back();
        for (auto& field : row)
            pointers.back().push_back(field.data());
    }
    resultRows.clear();
    for (auto& row : pointers)
        resultRows.push_back(row.data());
    resultCursor = 0;

    SQLMsg msg;
    auto* result = new SQLResult;
    result->uiNumRows = members.size();
    result->pSQLResult = reinterpret_cast<MYSQL_RES*>(&resultCursor);
    msg.vec_pkResult.push_back(result);

    CFuncQueryInfo* info = pendingQueries[index];
    info->f(&msg);
}

void DropQueries()
{
    for (auto* info : pendingQueries)
        delete info;
    pendingQueries.clear();
    pendingQueryText.clear();
}

// Splits a descriptor's stream into guild packets by their size field and
// returns the ones with the given subheader.
std::vector<std::vector<uint8_t>> GuildPackets(entt::entity viewer, uint8_t subheader)
{
    std::vector<std::vector<uint8_t>> out;
    const auto& stream = sent[DescOf(viewer)];
    size_t at = 0;
    while (at + sizeof(TPacketGCGuild) <= stream.size())
    {
        TPacketGCGuild pack;
        std::memcpy(&pack, stream.data() + at, sizeof(pack));
        Check(pack.header == HEADER_GC_GUILD && pack.size >= sizeof(pack) && at + pack.size <= stream.size(),
            "a guild packet is malformed");
        if (pack.subheader == subheader)
            out.emplace_back(stream.begin() + at, stream.begin() + at + pack.size);
        at += pack.size;
    }
    Check(at == stream.size(), "a guild packet overran its size");
    return out;
}

TGuildMemberPacketData Record(const std::vector<uint8_t>& packet, size_t index, size_t size)
{
    TGuildMemberPacketData data;
    std::memset(&data, 0, sizeof(data));
    std::memcpy(&data, packet.data() + sizeof(TPacketGCGuild) + index * size, size);
    return data;
}

void Reset()
{
    DropQueries();
    g_registry.clear();
    queries.clear(); dbHeaders.clear(); sent.clear(); links.clear(); unlinks.clear();
    guildOf.clear(); playerIds.clear(); names.clear(); online.clear();
    g_bGuildInviteLimit = false;
}

const std::vector<Member> kMembers = {
    { 10, GUILD_LEADER_GRADE, true, 100, 50, 2, "Leader" },
    { 20, 15, false, 0, 30, 1, "Second" },
    { 30, 15, false, 7, 20, 3, "Third" },
};

// The DB load makes one row entity per member, indexed by player id; a
// reload replaces them, and a pid the result repeats keeps its first row.
void LoadBuildsRowsAndReloadReplacesThem()
{
    Reset();
    CGuild guild(1234);
    AnswerMemberQuery(kMembers);
    Check(guild.GetMemberCount() == 3 && RowCount() == 3, "the load did not make one row per member");
    const auto* leader = guild.GetMember(10);
    Check(leader && leader->grade == GUILD_LEADER_GRADE && leader->general && leader->offerExp == 100 &&
        leader->level == 50 && leader->job == 2 && leader->name == "Leader" && leader->guildId == 1234,
        "the leader's row lost a field");
    Check(guild.GetGeneralCount() == 1 && guild.GetTotalLevel() == 100, "the counts do not follow the rows");
    Check(guild.GetMemberPID("Third") == 30 && guild.GetMemberPID("Nobody") == 0, "the name lookup failed");
    Check(guild.GetMember(99) == nullptr, "a stranger has a row");
    Check(links.size() == 3 && links[20] == &guild, "the members were not linked to the guild");

    AnswerMemberQuery({ { 40, 15, false, 1, 10, 0, "Fourth" }, { 40, 3, true, 2, 11, 0, "Again" } });
    Check(guild.GetMemberCount() == 1 && RowCount() == 1, "the reload kept the old rows");
    Check(guild.GetMember(10) == nullptr && guild.GetMember(40)->name == "Fourth" && guild.GetMember(40)->grade == 15,
        "a repeated pid did not keep its first row");

    AnswerMemberQuery({});
    Check(RowCount() == 1, "an empty result dropped the rows");
    DropQueries();
}

// Adding makes a row once and updates it after; removing destroys it, and
// the leader cannot be removed.
void AddAndRemoveMembers()
{
    Reset();
    CGuild guild(1234);
    AnswerMemberQuery(kMembers);

    TPacketDGGuildMember packet {};
    packet.dwPID = 50; packet.dwGuild = 1234; packet.bGrade = 15; packet.bJob = 1; packet.bLevel = 40; packet.dwOffer = 3;
    std::strcpy(packet.szName, "Fifth");
    guild.AddMember(&packet);
    Check(RowCount() == 4 && guild.GetMember(50) && guild.GetMember(50)->level == 40, "AddMember made no row");
    Check(links[50] == &guild, "AddMember did not link the member");

    packet.bGrade = 4; packet.isGeneral = 1; packet.dwOffer = 9;
    guild.AddMember(&packet);
    Check(RowCount() == 4 && guild.GetMember(50)->grade == 4 && guild.GetMember(50)->general && guild.GetMember(50)->offerExp == 9,
        "a second AddMember did not update the row");

    Check(!guild.RequestRemoveMember(10) && !guild.RemoveMember(10), "the leader was removable");
    Check(!guild.RequestRemoveMember(99) && !guild.RemoveMember(99), "a stranger was removable");
    Check(guild.RequestRemoveMember(50) && dbHeaders.back() == HEADER_GD_GUILD_REMOVE_MEMBER, "the removal was not requested");
    Check(guild.RemoveMember(50), "a member could not be removed");
    Check(RowCount() == 3 && guild.GetMember(50) == nullptr && guild.GetMemberCount() == 3, "the removed row survived");
    Check(unlinks.size() == 1 && unlinks[0] == 50, "the removed member was not unlinked");
    DropQueries();
}

// The list the client reads: one 38-byte record per member, fields in wire
// order, the name flag set and the name zero-padded.
void ListRecordsKeepTheWireLayout()
{
    Reset();
    CGuild guild(1234);
    AnswerMemberQuery(kMembers);
    const auto viewer = Character(10, "Leader");
    sent.clear();

    guild.SendListPacket(viewer);
    const auto lists = GuildPackets(viewer, GUILD_SUBHEADER_GC_LIST);
    Check(lists.size() == 1, "no member list was sent");
    const auto& list = lists[0];
    Check(list.size() == sizeof(TPacketGCGuild) + 3 * sizeof(TGuildMemberPacketData), "the list has the wrong size");
    Check(sizeof(TGuildMemberPacketData) == 13 + CHARACTER_NAME_MAX_LEN + 1, "the member record is not 38 bytes");

    for (size_t i = 0; i < kMembers.size(); ++i)
    {
        const auto record = Record(list, i, sizeof(TGuildMemberPacketData));
        const auto& expected = kMembers[i];
        Check(record.pid == expected.pid && record.grade == expected.grade && record.is_general == (expected.general ? 1 : 0) &&
            record.job == expected.job && record.level == expected.level && record.offer == expected.offer,
            "a list record lost a field");
        Check(record.name_flag == 1 && std::string(record.name) == expected.name, "a list record lost its name");
        for (size_t c = expected.name.size(); c < sizeof(record.name); ++c)
            Check(record.name[c] == 0, "a list record's name is not zero-padded");
    }
    DropQueries();
}

// Level, data and grade changes update the row and send the 13-byte head,
// without a name, to every online member.
void MemberUpdatesSendTheRecordHead()
{
    Reset();
    CGuild guild(1234);
    AnswerMemberQuery(kMembers);
    const auto viewer = Character(10, "Leader");
    guild.LoginMember(viewer);
    Check(guildOf[viewer] == &guild, "the login did not bind the guild");

    sent.clear();
    guild.LevelChange(20, 55);
    Check(guild.GetMember(20)->level == 55 && dbHeaders.back() == HEADER_GD_GUILD_CHANGE_MEMBER_DATA, "the level was not changed");
    auto heads = GuildPackets(viewer, GUILD_SUBHEADER_GC_LIST);
    Check(heads.size() == 1 && heads[0].size() == sizeof(TPacketGCGuild) + 13, "the level change sent no record head");
    auto head = Record(heads[0], 0, 13);
    Check(head.pid == 20 && head.level == 55 && head.grade == 15 && head.name_flag == 0, "the record head is wrong");

    sent.clear();
    guild.ChangeMemberData(20, 500, 60, 3);
    const auto* row = guild.GetMember(20);
    Check(row->offerExp == 500 && row->level == 60 && row->grade == 3, "the member data was not applied");
    heads = GuildPackets(viewer, GUILD_SUBHEADER_GC_LIST);
    head = Record(heads.at(0), 0, 13);
    Check(head.offer == 500 && head.level == 60 && head.grade == 3 && head.name_flag == 0, "the data head is wrong");

    queries.clear();
    guild.ChangeMemberGrade(20, 4);
    Check(guild.GetMember(20)->grade == 4 && queries.size() == 1 && queries[0].find("grade = 4") != std::string::npos,
        "the grade was not changed and saved");
    guild.ChangeMemberGrade(20, 1);
    Check(guild.GetMember(20)->grade == 4, "a member was made leader through a grade change");
    guild.LevelChange(99, 10);
    guild.ChangeMemberData(99, 1, 1, 1);
    Check(guild.GetMember(99) == nullptr, "an update made a stranger's row");
    DropQueries();
}

// The general count is read from the rows, so it follows every change.
void GeneralsAreCountedFromTheRows()
{
    Reset();
    CGuild guild(1234);
    AnswerMemberQuery(kMembers);
    Check(guild.GetGeneralCount() == 1, "the loaded general was not counted");
    Check(!guild.ChangeMemberGeneral(20, 1) && !guild.GetMember(20)->general, "a second general was allowed");
    Check(guild.ChangeMemberGeneral(10, 0) && guild.GetGeneralCount() == 0, "the general was not released");
    Check(guild.ChangeMemberGeneral(20, 1) && guild.GetGeneralCount() == 1 && guild.GetMember(20)->general, "the new general was refused");
    Check(guild.ChangeMemberGeneral(99, 0), "an unknown member was refused");
    TPacketDGGuildMember packet {};
    packet.dwPID = 30; packet.dwGuild = 1234; packet.bGrade = 15; packet.isGeneral = 1;
    std::strcpy(packet.szName, "Third");
    guild.AddMember(&packet);
    Check(guild.GetGeneralCount() == 2, "a general made by AddMember was not counted");
    DropQueries();
}

// The rows belong to their guild: destroying it destroys them.
void RowsGoWithTheGuild()
{
    Reset();
    {
        CGuild guild(1234);
        AnswerMemberQuery(kMembers);
        Check(RowCount() == 3, "the guild has no rows");
        DropQueries();
    }
    Check(RowCount() == 0, "the rows outlived their guild");
}
}

int main()
{
    try {
        LoadBuildsRowsAndReloadReplacesThem();
        AddAndRemoveMembers();
        ListRecordsKeepTheWireLayout();
        MemberUpdatesSendTheRecordHead();
        GeneralsAreCountedFromTheRows();
        RowsGoWithTheGuild();
        Reset();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << "\n";
        return 1;
    }
    std::cout << "guild member lifecycle: " << checks << " checks passed\n";
    return 0;
}
