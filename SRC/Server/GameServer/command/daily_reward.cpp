#include "stdafx.h"
#include "daily_reward.h"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../item/ItemStore.hpp"
#include "desc.h"
#include <SQL/AsyncSQL.h>
#include <charconv>
#include <chrono>
#include <memory>
#include <openssl/rand.h>

namespace {
namespace item = auriga::item;
namespace store = item::runtime;
using Msg = std::unique_ptr<SQLMsg>;

struct Reward {
    item::ItemKey key;
    std::uint16_t level;
    std::uint32_t count;
    std::string name;
    std::string icon;
};

std::optional<std::uint32_t> Unsigned(const char* value)
{
    if (!value) return {};
    const std::string_view text(value);
    std::uint32_t number = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return {};
    return number;
}

bool Valid(const Msg& msg)
{
    return msg && !msg->uiSQLErrno && msg->Get();
}

std::optional<std::vector<Reward>> ReadRewards(CAsyncSQL& sql, const item::ItemCatalog& catalog,
                                               unsigned day, bool randomOrder)
{
    const Msg rows(sql.DirectQueryPrepared(randomOrder
        ? "SELECT category_id,local_id,refinement_level,stack_count FROM player.daily_reward_native_offer WHERE reward=? ORDER BY RAND()"
        : "SELECT category_id,local_id,refinement_level,stack_count FROM player.daily_reward_native_offer WHERE reward=? ORDER BY slot",
        {SqlParam::UInt(day)}));
    if (!Valid(rows)) return {};
    std::vector<Reward> rewards;
    for (auto row = rows->Get()->FetchRow(); row; row = rows->Get()->FetchRow())
    {
        const auto category=Unsigned(row[0]), local=Unsigned(row[1]), level=Unsigned(row[2]), count=Unsigned(row[3]);
        if (!category || *category>UINT16_MAX || !local || !level || *level>UINT16_MAX || !count || !*count) continue;
        const auto key=item::ItemKey::Create(static_cast<std::uint16_t>(*category),*local);
        const auto* entry=key ? catalog.Find(*key) : nullptr;
        if (!entry || entry->base.retired || !item::SupportsPersistentGrant(catalog,*entry)) continue;
        const auto slots=(std::uint64_t(*count)+entry->base.maxStack-1)/entry->base.maxStack;
        if (slots>item::MaxItemCommitItems) continue;
        std::vector<item::ItemInstanceId> ids;
        for(std::uint64_t id=1;id<=slots;++id) ids.push_back(item::ItemInstanceId::Create(id).value());
        const item::ItemOwner account{item::ItemOwnerKind::Account,1};
        const std::array layout{item::ItemStorageLayout{{account,item::ItemStorageKind::Mall,1},9,5,3}};
        item::ItemGrant grant{*key,static_cast<std::uint16_t>(*level),*count};
        grant.createdAtUtcMs=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (!item::MakeGrantCommit(catalog,{1},{account,1,1},layout,{},grant,ids,item::ItemStorageKind::Mall)) continue;
        rewards.push_back({*key,grant.refinementLevel,*count,entry->base.nameKey,entry->base.iconAsset});
    }
    return rewards;
}

bool CurrentPlayer(entt::entity character, std::uint32_t pid)
{
    return ecs::PlayerRuntime::IsValid(character) && ecs::PlayerRuntime::GetPlayerID(character)==pid;
}

enum class ClaimResult { Received, Unavailable, AlreadyClaimed };

ClaimResult ClaimReward(CAsyncSQL& sql, const item::ItemCatalog& catalog, std::uint32_t pid,
                        std::uint32_t account, const std::string& hardware)
{
    const Msg due(sql.DirectQueryPrepared(
        "SELECT reward FROM player.daily_reward_status WHERE pid=? AND time<NOW()", {SqlParam::UInt(pid)}));
    if (!Valid(due)) return ClaimResult::Unavailable;
    const auto row=due->Get()->FetchRow();
    if (!row) return ClaimResult::AlreadyClaimed;
    const auto day=Unsigned(row[0]);
    if (!day || *day>6) return ClaimResult::Unavailable;
    const auto offers=ReadRewards(sql,catalog,*day,true);
    if (!offers || offers->empty()) return ClaimResult::Unavailable;
    const auto& reward=offers->front();

    std::array<unsigned char,16> id{};
    if (RAND_bytes(id.data(),static_cast<int>(id.size()))!=1) return ClaimResult::Unavailable;
    const std::string request(reinterpret_cast<const char*>(id.data()),id.size());
    SqlTransaction transaction;
    transaction.push_back({
        "UPDATE player.daily_reward_status SET reward=?,total_rewards=total_rewards+1,time=NOW()+INTERVAL 1 DAY "
        "WHERE pid=? AND time<NOW() AND reward=?",
        {SqlParam::UInt((*day+1)%7),SqlParam::UInt(pid),SqlParam::Text(std::to_string(*day))},1});
#ifdef ENABLE_DAILY_REWARD_HWID_LIMIT_RAZOR93
    transaction.push_back({
        "INSERT IGNORE INTO player.daily_reward_claim_hwid(hwkey,claim_day,pid,account_id) VALUES(?,CURDATE(),?,?)",
        {SqlParam::Text(hardware),SqlParam::UInt(pid),SqlParam::UInt(account)},1});
#else
    (void)hardware;
#endif
    transaction.push_back({
        "INSERT INTO player.item_native_award(request_id,recipient_pid,account_id,category_id,local_id,refinement_level,stack_count) "
        "VALUES(?,?,?,?,?,?,?)",
        {SqlParam::Blob(request),SqlParam::UInt(pid),SqlParam::UInt(account),SqlParam::UInt(static_cast<unsigned>(reward.key.Category())),
         SqlParam::UInt(reward.key.LocalId()),SqlParam::UInt(reward.level),SqlParam::UInt(reward.count)},1});
    const Msg written(sql.DirectTransactionPrepared(std::move(transaction)));
    if (!written) return ClaimResult::Unavailable;
    if (written->transactionOutcome==SqlTransactionOutcome::Committed) return ClaimResult::Received;
    if (written->transactionOutcome==SqlTransactionOutcome::Conflict) return ClaimResult::AlreadyClaimed;
    if (written->transactionOutcome==SqlTransactionOutcome::CommitUnknown)
    {
        const Msg receipt(sql.DirectQueryPrepared("SELECT id FROM player.item_native_award WHERE request_id=? AND recipient_pid=?",
            {SqlParam::Blob(request),SqlParam::UInt(pid)}));
        if (Valid(receipt) && receipt->Get()->uiNumRows==1) return ClaimResult::Received;
    }
    return ClaimResult::Unavailable;
}
}

void DailyReward::Reload(entt::entity character)
{
    if (!ecs::PlayerRuntime::IsValid(character)) return;
    const auto pid=ecs::PlayerRuntime::GetPlayerID(character);
    store::ItemStore::Instance().Submit([=](CAsyncSQL& sql,const item::ItemCatalog& catalog) -> store::ItemStore::Completion {
        const Msg initialized(sql.DirectQueryPrepared(
            "INSERT INTO player.daily_reward_status(pid,time,reward,total_rewards) VALUES(?,NOW(),'0',0) "
            "ON DUPLICATE KEY UPDATE reward=IF(time<NOW()-INTERVAL 1 DAY,'0',reward),"
            "total_rewards=IF(time<NOW()-INTERVAL 1 DAY,0,total_rewards),time=IF(time<NOW()-INTERVAL 1 DAY,NOW(),time)",
            {SqlParam::UInt(pid)}));
        if (!Valid(initialized)) return {};
        const Msg status(sql.DirectQueryPrepared(
            "SELECT UNIX_TIMESTAMP(time),reward FROM player.daily_reward_status WHERE pid=?", {SqlParam::UInt(pid)}));
        if (!Valid(status)) return {};
        const auto row=status->Get()->FetchRow();
        if (!row || !row[0]) return {};
        const auto day=Unsigned(row[1]);
        if (!day || *day>6) return {};
        auto offers=ReadRewards(sql,catalog,*day,false);
        if (!offers) return {};
        return [=, time=std::string(row[0]), offers=std::move(*offers)] {
            if (!CurrentPlayer(character,pid)) return;
            ecs::ChatSystem::Send(character,CHAT_TYPE_COMMAND,"ManagerGiftSystem DeleteRewards|");
            for (const auto& reward:offers)
                ecs::ChatSystem::Send(character,CHAT_TYPE_COMMAND,"ManagerGiftSystem SetNativeReward|%u|%u|%u|%u|%s|%s",
                    static_cast<unsigned>(reward.key.Category()),reward.key.LocalId(),static_cast<unsigned>(reward.level),
                    reward.count,reward.name.c_str(),reward.icon.c_str());
            ecs::ChatSystem::Send(character,CHAT_TYPE_COMMAND,"ManagerGiftSystem SetTime|%s",time.c_str());
            ecs::ChatSystem::Send(character,CHAT_TYPE_COMMAND,"ManagerGiftSystem SetDailyReward|%d",offers.empty() ? -1 : static_cast<int>(*day));
            ecs::ChatSystem::Send(character,CHAT_TYPE_COMMAND,"ManagerGiftSystem SetRewardDone|");
        };
    });
}

void DailyReward::Claim(entt::entity character,std::string hardware,std::uint32_t account)
{
    if (!ecs::PlayerRuntime::IsValid(character)) return;
    auto* desc = ecs::PlayerRuntime::GetDesc(character);
    if (!desc) return;
    if (!account) account=desc->GetAccountTable().id;
    const auto pid=ecs::PlayerRuntime::GetPlayerID(character);
    if (!pid || !account) return;
#ifdef ENABLE_DAILY_REWARD_HWID_LIMIT_RAZOR93
    if (hardware.empty() || hardware.size()>128) return;
#endif
    if (!store::ItemStore::Instance().Submit([=,hardware=std::move(hardware)](CAsyncSQL& sql,const item::ItemCatalog& catalog) -> store::ItemStore::Completion {
        const auto result=ClaimReward(sql,catalog,pid,account,hardware);
        return [=] {
            if (!CurrentPlayer(character,pid)) return;
            switch(result)
            {
            case ClaimResult::Received:
                ecs::ChatSystem::Send(character,CHAT_TYPE_INFO,"A napi jutalmat az ajándéktárolóban veheted át.");
                break;
            case ClaimResult::AlreadyClaimed:
                ecs::ChatSystem::Send(character,CHAT_TYPE_INFO,"A napi jutalom jelenleg nem vehető át.");
                break;
            case ClaimResult::Unavailable:
                ecs::ChatSystem::Send(character,CHAT_TYPE_INFO,"A napi jutalom átmenetileg nem érhető el.");
                break;
            }
        };
    })) ecs::ChatSystem::Send(character,CHAT_TYPE_INFO,"A napi jutalom átmenetileg nem érhető el.");
}
