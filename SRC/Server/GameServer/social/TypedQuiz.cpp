#include "stdafx.h"
#include "TypedQuiz.hpp"
#include "char_manager.h"
#include "cmd.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../item/ItemCatalogRuntime.hpp"
#include "../item/ItemStore.hpp"
#include "../item/TypedDrops.hpp"
#include "../item/TypedInventorySync.hpp"

#include <Core/Logging.hpp>
#include <NativeAwardsSql.hpp>
#include <SQL/AsyncSQL.h>
#include <charconv>
#include <chrono>
#include <openssl/rand.h>
#include <sstream>

namespace
{
namespace item = auriga::item;
namespace sql = item::persistence;
namespace Player = ecs::PlayerRuntime;
using Operation = std::array<std::uint8_t,16>;
using Error = item::ItemPersistenceError;

struct Round
{
    std::uint64_t id;
    item::ItemGrant reward;
};

struct Answer
{
    std::uint32_t pid;
    std::uint64_t token;
    std::string name;
    std::int64_t value;
};

enum class Claim { Won, Closed, Failed, Uncertain };

template <typename Number>
bool Parse(std::string_view text, Number& result)
{
    if (text.empty()) return false;
    const auto read = std::from_chars(text.data(), text.data()+text.size(), result);
    return read.ec == std::errc{} && read.ptr == text.data()+text.size();
}

bool ParseAnswer(std::string_view text, std::int64_t& value)
{
    const auto start = text.find_first_not_of(" \t\r\n");
    if (start == text.npos) return false;
    text = text.substr(start, text.find_last_not_of(" \t\r\n")-start+1);
    if (text.starts_with('+'))
    {
        text.remove_prefix(1);
        if (text.empty() || text.front() < '0' || text.front() > '9') return false;
    }
    return Parse(text, value);
}

entt::entity Current(const Answer& answer)
{
    const auto actor = CHARACTER_MANAGER::instance().FindEntityByPID(answer.pid);
    const auto* login = g_registry.valid(actor) ? g_registry.try_get<ecs::TypedItemLogin>(actor) : nullptr;
    return login && login->token == answer.token ? actor : entt::null;
}

void Tell(entt::entity actor, const char* message)
{
    if (Player::IsPC(actor))
        ecs::ChatSystem::Send(actor, CHAT_TYPE_INFO, "%s", message);
}

std::expected<std::optional<Round>,Error> FindRound(CAsyncSQL& db, std::int64_t answer)
{
    const std::unique_ptr<SQLMsg> query(db.DirectQueryPrepared(
        "SELECT id,category_id,local_id,refinement_level,stack_count FROM player.item_quiz_round WHERE active_slot=1 AND answer=?",
        {SqlParam::Int(answer)}));
    if (!query || query->uiSQLErrno || !query->Get())
        return std::unexpected(Error::SqlFailure);
    const auto row = query->Get()->FetchRow();
    if (!row) return std::optional<Round>{};
    std::uint64_t id = 0;
    std::uint16_t category = 0, level = 0;
    std::uint32_t local = 0, count = 0;
    if (!row[0] || !Parse(row[0],id) || !id || !row[1] || !Parse(row[1],category) ||
        !row[2] || !Parse(row[2],local) || !row[3] || !Parse(row[3],level) ||
        !row[4] || !Parse(row[4],count) || !count)
        return std::unexpected(Error::InvalidStoredData);
    const auto key = item::ItemKey::Create(category,local);
    if (!key) return std::unexpected(Error::InvalidStoredData);
    return Round{id,{*key,level,count}};
}

Claim WriteClaim(CAsyncSQL& db, const item::ItemCatalog& catalog, const Round& round, const Answer& answer,
    const Operation& operation, const std::optional<item::ItemCommit>& inventory)
{
    const std::string operationBytes(reinterpret_cast<const char*>(operation.data()), operation.size());
    SqlTransaction transaction{{
        "UPDATE player.item_quiz_round SET active_slot=NULL,winner_pid=?,claim_operation=? WHERE id=? AND active_slot=1 AND answer=?",
        {SqlParam::UInt(answer.pid), SqlParam::Blob(operationBytes), SqlParam::UInt(round.id), SqlParam::Int(answer.value)}, 1, {}
    }};
    if (inventory)
    {
        auto prepared = sql::PrepareItemWrite(catalog,*inventory);
        if (!prepared) return Claim::Failed;
        transaction.insert(transaction.end(),std::make_move_iterator(prepared->begin()),std::make_move_iterator(prepared->end()));
    }
    else
    {
        const auto& reward = round.reward;
        if (!sql::CanQueueNativeItemAward(catalog,reward.key,reward.refinementLevel,reward.count))
            return Claim::Failed;
        transaction.push_back({
            "INSERT INTO player.item_native_award(request_id,recipient_pid,account_id,category_id,local_id,refinement_level,stack_count) "
            "SELECT ?,p.id,p.account_id,?,?,?,? FROM player.player p JOIN account.account a ON a.id=p.account_id WHERE p.id=?",
            {SqlParam::Blob(operationBytes),SqlParam::UInt(static_cast<unsigned>(reward.key.Category())),
                SqlParam::UInt(reward.key.LocalId()),SqlParam::UInt(reward.refinementLevel),
                SqlParam::UInt(reward.count),SqlParam::UInt(answer.pid)},1,{}
        });
    }

    const std::unique_ptr<SQLMsg> result(db.DirectTransactionPrepared(std::move(transaction)));
    if (!result) return Claim::Failed;
    if (result->transactionOutcome == SqlTransactionOutcome::Committed) return Claim::Won;
    if (result->transactionOutcome == SqlTransactionOutcome::Conflict && result->transactionFailedStep == 0)
        return Claim::Closed;
    if (result->transactionOutcome != SqlTransactionOutcome::CommitUnknown)
        return Claim::Failed;

    // The round keeps the winning operation even after the next round starts.
    // A locking read waits for the original transaction to settle after reconnect.
    SqlTransaction check{{
        "SELECT id FROM player.item_quiz_round WHERE id=? AND winner_pid=? AND claim_operation=? LOCK IN SHARE MODE",
        {SqlParam::UInt(round.id),SqlParam::UInt(answer.pid),SqlParam::Blob(operationBytes)},{},{}
    }};
    const std::unique_ptr<SQLMsg> receipt(db.DirectTransactionPrepared(std::move(check)));
    if (!receipt || receipt->transactionOutcome != SqlTransactionOutcome::Committed || !receipt->Get())
        return Claim::Uncertain;
    return receipt->Get()->uiNumRows == 1 ? Claim::Won : Claim::Failed;
}

void AnnounceWinner(const Answer& answer)
{
#ifdef TEXTS_IMPROVEMENT
    BroadcastNoticeNew(CHAT_TYPE_INFO,0,0,2181,"%s#%lld#",answer.name.c_str(),static_cast<long long>(answer.value));
#else
    const auto notice = fmt::format("[Kvíz] {} nyert. A helyes válasz: {}.",answer.name,answer.value);
    BroadcastNotice(notice.c_str());
#endif
}

void ClaimRound(const Answer& answer, Round round)
{
    const auto actor = Current(answer);
    const auto catalog = item::runtime::CurrentCatalog();
    Operation operation{};
    if (!catalog || RAND_bytes(operation.data(),static_cast<int>(operation.size())) != 1)
    {
        Tell(actor,"A kvízjutalom mentése nem sikerült.");
        return;
    }

    std::optional<item::ItemCommit> inventory;
    const auto* owned = actor != entt::null ? g_registry.try_get<ecs::TypedItems>(actor) : nullptr;
    if (owned && !g_registry.all_of<ecs::TypedItemBusy>(actor))
    {
        const auto& state = owned->state;
        round.reward.createdAtUtcMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto ids = item::runtime::PeekInstanceIds(item::MaxItemCommitItems);
        auto plan = item::MakeGrantCommit(*catalog,operation,state.fence,state.storages,state.items,round.reward,ids);
        if (plan)
        {
            item::runtime::SpendInstanceIds(std::ranges::count_if(plan->items,[](const auto& change){return !change.before;}));
            inventory = std::move(*plan);
            g_registry.emplace<ecs::TypedItemBusy>(actor);
        }
    }

    const bool direct = inventory.has_value();
    const bool queued = item::runtime::ItemStore::Instance().Submit(
        [round, answer, operation, inventory=std::move(inventory), direct](CAsyncSQL& db,const item::ItemCatalog& current)
            -> item::runtime::ItemStore::Completion {
        const auto result = WriteClaim(db,current,round,answer,operation,inventory);
        std::optional<sql::LoadedItemOwner> refreshed;
        if (direct)
        {
            const auto& fence = inventory->owners.front();
            auto state = sql::LoadItemOwner(db,current,fence.owner,fence.sessionGeneration);
            if (state) refreshed = std::move(*state);
        }
        return [round, answer, result, direct, refreshed=std::move(refreshed)]() mutable {
            const auto player = Current(answer);
            if (player != entt::null)
            {
                if (direct)
                {
                    if (refreshed) g_registry.emplace_or_replace<ecs::TypedItems>(player,std::move(*refreshed));
                    g_registry.remove<ecs::TypedItemBusy>(player);
                    SendTypedInventory(player);
                    PumpTypedItemGrants(player);
                }
            }
            if (result == Claim::Won)
            {
                AnnounceWinner(answer);
                Tell(player,direct ? "A kvízjutalom bekerült a leltáradba." : "A kvízjutalmad az ajándéktárolóban vehető át.");
            }
            else if (result != Claim::Closed)
            {
                LOG_ERROR("Typed quiz claim failed: round={}, pid={}, result={}",round.id,answer.pid,static_cast<int>(result));
                Tell(player,result == Claim::Uncertain ? "A kvízjutalom állapota most nem ellenőrizhető." : "A kvízjutalom mentése nem sikerült.");
            }
        };
    });
    if (!queued && actor != entt::null)
    {
        if (direct) g_registry.remove<ecs::TypedItemBusy>(actor);
        Tell(actor,"A kvízjutalom mentése nem sikerült.");
    }
}
}

void CheckTypedQuizAnswer(entt::entity character, std::string_view text)
{
    std::int64_t value = 0;
    if (!ParseAnswer(text,value) || !Player::IsPC(character)) return;
    const auto* login = g_registry.try_get<ecs::TypedItemLogin>(character);
    if (!login || login->mode != ecs::TypedItemLogin::Mode::Native) return;
    const Answer answer{Player::GetPlayerID(character),login->token,std::string(Player::GetName(character)),value};
    if (!answer.pid) return;
    item::runtime::ItemStore::Instance().Submit(
        [answer](CAsyncSQL& db,const item::ItemCatalog&) -> item::runtime::ItemStore::Completion {
        const auto round = FindRound(db,answer.value);
        if (!round || !*round) return {};
        return [answer,round=**round] { ClaimRound(answer,round); };
    });
}

ACMD(do_quiz_start)
{
    std::istringstream args{argument};
    std::string answerText, code, levelText, countText, extra;
    std::int64_t answer = 0;
    std::uint16_t level = 0;
    std::uint32_t count = 0;
    if (!(args>>answerText>>code>>levelText>>countText) || (args>>extra) ||
        !ParseAnswer(answerText,answer) || !Parse(levelText,level) || !Parse(countText,count) || !count)
    {
        Tell(character,"Használat: /quiz_start válasz tárgykód fejlesztési_szint darabszám");
        return;
    }
    const auto catalog = item::runtime::CurrentCatalog();
    if (!catalog)
    {
        Tell(character,"A típusos tárgykatalógus jelenleg nem érhető el.");
        return;
    }
    const auto found = std::ranges::find_if(catalog->Entries(),[&](const auto& row){return row.second.base.contentCode==code;});
    if (found == catalog->Entries().end() || !sql::CanQueueNativeItemAward(*catalog,found->first,level,count))
    {
        Tell(character,"A megadott tárgy, fejlesztési szint vagy darabszám nem adható kvízjutalomként.");
        return;
    }
    const auto key = found->first;
    const bool queued = item::runtime::ItemStore::Instance().Submit(
        [character,answer,key,level,count](CAsyncSQL& db,const item::ItemCatalog& current) -> item::runtime::ItemStore::Completion {
        if (!sql::CanQueueNativeItemAward(current,key,level,count))
            return [character]{Tell(character,"A jutalom már nem érhető el a tárgykatalógusban.");};
        const std::unique_ptr<SQLMsg> write(db.DirectQueryPrepared(
            "INSERT INTO player.item_quiz_round(active_slot,answer,category_id,local_id,refinement_level,stack_count) VALUES(1,?,?,?,?,?)",
            {SqlParam::Int(answer),SqlParam::UInt(static_cast<unsigned>(key.Category())),
                SqlParam::UInt(key.LocalId()),SqlParam::UInt(level),SqlParam::UInt(count)}));
        const auto code = write ? write->uiSQLErrno : 1;
        return [character,code] {
            Tell(character,code == 0 ? "A kvíz elindult." : code == 1062 ? "Már fut egy kvízforduló." : "A kvíz indítása nem sikerült.");
        };
    });
    if (!queued) Tell(character,"A kvíz indítása nem sikerült.");
}

ACMD(do_quiz_cancel)
{
    const bool queued = item::runtime::ItemStore::Instance().Submit(
        [character](CAsyncSQL& db,const item::ItemCatalog&) -> item::runtime::ItemStore::Completion {
        const std::unique_ptr<SQLMsg> write(db.DirectQueryPrepared(
            "UPDATE player.item_quiz_round SET active_slot=NULL WHERE active_slot=1",{}));
        const bool ok = write && !write->uiSQLErrno && write->Get();
        const bool cancelled = ok && write->Get()->uiAffectedRows != 0;
        return [character,ok,cancelled] {
            Tell(character,!ok ? "A kvíz lezárása nem sikerült." : cancelled ? "A kvízforduló lezárva." : "Nincs nyitott kvízforduló.");
        };
    });
    if (!queued) Tell(character,"A kvíz lezárása nem sikerült.");
}
