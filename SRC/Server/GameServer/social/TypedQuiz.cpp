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
        "SELECT id,category_id,local_id,refinement_level,stack_count FROM player.item_quiz_round WHERE active_slot=1 AND answer=? "
        "AND (expires_at IS NULL OR expires_at>CURRENT_TIMESTAMP(6))",
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
        "UPDATE player.item_quiz_round SET active_slot=NULL,winner_pid=?,claim_operation=? WHERE id=? AND active_slot=1 AND answer=? "
        "AND (expires_at IS NULL OR expires_at>CURRENT_TIMESTAMP(6))",
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

namespace
{
struct QuizPrompt
{
    unsigned preset;
    std::string question;
};

void AnnounceQuiz(const QuizPrompt& prompt, const std::string& nameKey, std::uint32_t count)
{
#ifdef TEXTS_IMPROVEMENT
    if (prompt.preset)
        BroadcastNoticeNew(CHAT_TYPE_NOTICE,0,0,prompt.preset == 100 ? 2182 : 2183,"");
    else
        BroadcastNoticeNew(CHAT_TYPE_NOTICE,0,0,2175,"%s",prompt.question.c_str());
    BroadcastNoticeNew(CHAT_TYPE_NOTICE,0,0,2176,"%s",nameKey.c_str());
    BroadcastNoticeNew(CHAT_TYPE_NOTICE,0,0,2177,"%u",count);
    BroadcastNoticeNew(CHAT_TYPE_NOTICE,0,0,2179,"");
#else
    const auto question = prompt.preset ? fmt::format("Gondoltam egy számra 1 és {} között.",prompt.preset) : prompt.question;
    BroadcastNotice(question.c_str());
    const auto reward = fmt::format("Kvízjutalom: {} × {}. A választ a globális chatbe írd.",nameKey,count);
    BroadcastNotice(reward.c_str());
#endif
}

void StartRound(entt::entity character, std::int64_t answer, std::string_view code,
    std::uint16_t level, std::uint32_t count, std::optional<QuizPrompt> prompt)
{
    const auto catalog = item::runtime::CurrentCatalog();
    if (!catalog)
    {
        Tell(character,"A típusos tárgykatalógus jelenleg nem érhető el.");
        return;
    }
    const auto found = std::ranges::find_if(catalog->Entries(),[&](const auto& row){return row.second.base.contentCode==code;});
    if (found == catalog->Entries().end() || found->second.base.retired || !sql::CanQueueNativeItemAward(*catalog,found->first,level,count))
    {
        Tell(character,"A megadott tárgy, fejlesztési szint vagy darabszám nem adható kvízjutalomként.");
        return;
    }
    const bool queued = item::runtime::ItemStore::Instance().Submit(
        [character,answer,key=found->first,level,count,prompt=std::move(prompt)](CAsyncSQL& db,const item::ItemCatalog& current) -> item::runtime::ItemStore::Completion {
        const auto* definition = current.Find(key);
        if (!definition || definition->base.retired || !sql::CanQueueNativeItemAward(current,key,level,count))
            return [character]{Tell(character,"A jutalom már nem érhető el a tárgykatalógusban.");};
        const std::unique_ptr<SQLMsg> write(db.DirectQueryPrepared(
            "INSERT INTO player.item_quiz_round(active_slot,answer,category_id,local_id,refinement_level,stack_count,expires_at) "
            "VALUES(1,?,?,?,?,?,IF(?=1,CURRENT_TIMESTAMP(6)+INTERVAL 60 SECOND,NULL))",
            {SqlParam::Int(answer),SqlParam::UInt(static_cast<unsigned>(key.Category())),SqlParam::UInt(key.LocalId()),
                SqlParam::UInt(level),SqlParam::UInt(count),SqlParam::UInt(prompt.has_value())}));
        const auto error = !write ? 1 : write->uiSQLErrno ? write->uiSQLErrno : !write->Get();
        return [character,error,prompt,name=definition->base.nameKey,count] {
            if (!error && prompt) AnnounceQuiz(*prompt,name,count);
            Tell(character,!error ? "A kvíz elindult." : error == 1062 ? "Már fut egy kvízforduló." : "A kvíz indítása nem sikerült.");
        };
    });
    if (!queued) Tell(character,"A kvíz indítása nem sikerült.");
}

struct ClosedQuiz
{
    std::int64_t answer;
    bool expired;
};

std::expected<std::optional<ClosedQuiz>,Error> CloseRound(CAsyncSQL& db, bool expiredOnly)
{
    const char* select = expiredOnly
        ? "SELECT id,answer,1 FROM player.item_quiz_round WHERE active_slot=1 AND expires_at<=CURRENT_TIMESTAMP(6)"
        : "SELECT id,answer,IFNULL(expires_at<=CURRENT_TIMESTAMP(6),0) FROM player.item_quiz_round WHERE active_slot=1";
    const std::unique_ptr<SQLMsg> round(db.DirectQueryPrepared(select,{}));
    if (!round || round->uiSQLErrno || !round->Get()) return std::unexpected(Error::SqlFailure);
    const auto row = round->Get()->FetchRow();
    if (!row) return std::optional<ClosedQuiz>{};
    std::uint64_t id = 0;
    std::int64_t answer = 0;
    if (!row[0] || !row[1] || !row[2] || !Parse(row[0],id) || !Parse(row[1],answer))
        return std::unexpected(Error::InvalidStoredData);
    const bool expired = *row[2] == '1';
    const std::unique_ptr<SQLMsg> result(db.DirectQueryPrepared(
        "UPDATE player.item_quiz_round SET active_slot=NULL WHERE id=? AND active_slot=1",{SqlParam::UInt(id)}));
    if (!result || result->uiSQLErrno || !result->Get()) return std::unexpected(Error::SqlFailure);
    if (!result->Get()->uiAffectedRows) return std::optional<ClosedQuiz>{};
    return ClosedQuiz{answer,expired};
}

void AnnounceClose(const ClosedQuiz& round)
{
#ifdef TEXTS_IMPROVEMENT
    BroadcastNoticeNew(CHAT_TYPE_NOTICE,0,0,round.expired ? 2180 : 2178,"%lld",static_cast<long long>(round.answer));
#else
    const auto notice = fmt::format("A kvíz véget ért. A helyes válasz: {}.",round.answer);
    BroadcastNotice(notice.c_str());
#endif
}

void CancelRound(entt::entity character, bool announce)
{
    const bool queued = item::runtime::ItemStore::Instance().Submit(
        [character,announce](CAsyncSQL& db,const item::ItemCatalog&) -> item::runtime::ItemStore::Completion {
        const auto closed = CloseRound(db,false);
        return [character,announce,closed] {
            if (closed && *closed && (announce || (**closed).expired)) AnnounceClose(**closed);
            Tell(character,!closed ? "A kvíz lezárása nem sikerült." : *closed ? "A kvízforduló lezárva." : "Nincs nyitott kvízforduló.");
        };
    });
    if (!queued) Tell(character,"A kvíz lezárása nem sikerült.");
}
}

void StartQuestQuiz(entt::entity gm, std::int64_t answer, std::string_view code,
    std::uint32_t count, unsigned preset, std::string question)
{
    if (!Player::IsPC(gm) || Player::GetGMLevel(gm) < GM_HIGH_WIZARD) return;
    if ((preset != 0 && preset != 100 && preset != 1000) ||
        (preset && (answer < 1 || answer > preset)) || (!preset && (question.empty() || question.size() > 220 ||
            question.find_first_of("#|\r\n") != std::string::npos || question.find('\0') != std::string::npos)))
    {
        Tell(gm,"Érvénytelen kvízkérdés vagy válasz.");
        return;
    }
    StartRound(gm,answer,code,0,count,QuizPrompt{preset,std::move(question)});
}

void CloseQuestQuiz(entt::entity gm)
{
    if (Player::IsPC(gm) && Player::GetGMLevel(gm) >= GM_HIGH_WIZARD) CancelRound(gm,true);
}

void PollExpiredQuiz()
{
    using Clock = std::chrono::steady_clock;
    static Clock::time_point next{};
    static bool pending = false;
    if (pending || Clock::now() < next || !item::runtime::CurrentCatalog()) return;
    next = Clock::now() + std::chrono::seconds(2);
    pending = item::runtime::ItemStore::Instance().Submit([](CAsyncSQL& db,const item::ItemCatalog&) -> item::runtime::ItemStore::Completion {
        const auto closed = CloseRound(db,true);
        return [closed] {
            pending = false;
            if (!closed) next = Clock::now() + std::chrono::minutes(1);
            else if (*closed) AnnounceClose(**closed);
        };
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
    StartRound(character,answer,code,level,count,std::nullopt);
}

ACMD(do_quiz_cancel)
{
    CancelRound(character,false);
}
