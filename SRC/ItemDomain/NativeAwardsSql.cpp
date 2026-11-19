#include "NativeAwardsSql.hpp"
#include <SQL/AsyncSQL.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <memory>
#include <openssl/rand.h>

namespace auriga::item::persistence
{
namespace
{
using E=ItemPersistenceError;
std::string Bytes(const std::array<std::uint8_t,16>& id)
{
    return {reinterpret_cast<const char*>(id.data()),id.size()};
}
std::optional<std::uint64_t> Number(const char* text)
{
    if (!text) return {};
    const std::string_view value(text);std::uint64_t result=0;
    const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
    if (parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size()) return {};
    return result;
}
}

std::expected<unsigned,E> EnqueueGuildNativeAwards(CAsyncSQL& db,const ItemCatalog& catalog,
    std::uint32_t guild,std::array<std::uint8_t,16> request,ItemKey key,std::uint16_t level,std::uint32_t count)
{
    const auto* entry=catalog.Find(key);
    if (!guild || !entry || entry->base.retired || !count || !SupportsPersistentGrant(catalog,*entry) ||
        std::ranges::all_of(request,[](auto b){return b==0;}) ||
        (std::uint64_t(count)+(std::max)(1u,entry->base.maxStack)-1)/(std::max)(1u,entry->base.maxStack)>MaxItemCommitItems)
        return std::unexpected(E::InvalidItem);
    const auto slots=(std::uint64_t(count)+(std::max)(1u,entry->base.maxStack)-1)/(std::max)(1u,entry->base.maxStack);
    std::vector<ItemInstanceId> probeIds;
    for(std::uint64_t n=1;n<=slots;++n)probeIds.push_back(ItemInstanceId::Create(n).value());
    const ItemOwner probeOwner{ItemOwnerKind::Account,1};
    const std::array layouts{ItemStorageLayout{{probeOwner,ItemStorageKind::Mall,1},9,5,3}};
    ItemGrant probe{key,level,count};probe.createdAtUtcMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if(!MakeGrantCommit(catalog,request,{probeOwner,1,1},layouts,{},probe,probeIds,ItemStorageKind::Mall))
        return std::unexpected(E::InvalidItem);
    const auto kind=static_cast<unsigned>(key.Category());
    // A request cannot be reused for a different economic contract.
    std::unique_ptr<SQLMsg> existing(db.DirectQueryPrepared(
        "SELECT category_id,local_id,refinement_level,stack_count FROM player.item_native_award WHERE request_id=? LIMIT 1",
        {SqlParam::Blob(Bytes(request))}));
    if (!existing || existing->uiSQLErrno || !existing->Get()) return std::unexpected(E::SqlFailure);
    if (auto row=existing->Get()->FetchRow())
        if (Number(row[0])!=kind || Number(row[1])!=key.LocalId() || Number(row[2])!=level || Number(row[3])!=count)
            return std::unexpected(E::ReceiptMismatch);
    std::unique_ptr<SQLMsg> write(db.DirectQueryPrepared(
        "INSERT IGNORE INTO player.item_native_award(request_id,recipient_pid,account_id,category_id,local_id,refinement_level,stack_count) "
        "SELECT ?,p.id,p.account_id,?,?,?,? FROM player.guild_member gm "
        "JOIN player.guild g ON g.id=gm.guild_id JOIN player.player p ON p.id=gm.pid "
        "JOIN account.account a ON a.id=p.account_id WHERE gm.guild_id=?",
        {SqlParam::Blob(Bytes(request)),SqlParam::UInt(kind),SqlParam::UInt(key.LocalId()),
         SqlParam::UInt(level),SqlParam::UInt(count),SqlParam::UInt(guild)}));
    if (!write || write->uiSQLErrno || !write->Get()) return std::unexpected(E::SqlFailure);
    return static_cast<unsigned>(write->Get()->uiAffectedRows);
}

std::expected<unsigned,E> DeliverNativeItemAwards(CAsyncSQL& db,const ItemCatalog& catalog,ItemOwner account)
{
    if (account.kind!=ItemOwnerKind::Account || !ValidItemOwner(account)) return std::unexpected(E::InvalidOwner);
    std::unique_ptr<SQLMsg> rows(db.DirectQueryPrepared(
        "SELECT id,category_id,local_id,refinement_level,stack_count FROM player.item_native_award "
        "WHERE account_id=? AND claimed_operation IS NULL ORDER BY id LIMIT 32",{SqlParam::UInt(account.id)}));
    if (!rows || rows->uiSQLErrno || !rows->Get()) return std::unexpected(E::SqlFailure);
    std::vector<NativeAwardClaim> pending;
    for (auto row=rows->Get()->FetchRow();row;row=rows->Get()->FetchRow())
    {
        const auto id=Number(row[0]),category=Number(row[1]),local=Number(row[2]),level=Number(row[3]),count=Number(row[4]);
        if (!id || !*id || *id>INT64_MAX || !category || *category>UINT16_MAX || !local || *local>UINT32_MAX ||
            !level || *level>UINT16_MAX || !count || !*count || *count>UINT32_MAX) return std::unexpected(E::InvalidStoredData);
        const auto key=ItemKey::Create(static_cast<std::uint16_t>(*category),static_cast<std::uint32_t>(*local));
        if (!key) return std::unexpected(E::InvalidStoredData);
        pending.push_back({*id,account.id,*key,static_cast<std::uint16_t>(*level),static_cast<std::uint32_t>(*count)});
    }
    unsigned delivered=0;
    for (const auto& award:pending)
    {
        const auto* entry=catalog.Find(award.key);
        if (!entry || entry->base.retired) return delivered; // Preserve pending content across catalog changes.
        const auto slots=(std::uint64_t(award.count)+(std::max)(1u,entry->base.maxStack)-1)/(std::max)(1u,entry->base.maxStack);
        if (slots>MaxItemCommitItems) return std::unexpected(E::InvalidStoredData);
        auto state=LoadCurrentItemOwner(db,catalog,account);if (!state) return std::unexpected(state.error());
        auto block=ReserveItemInstanceIds(db,catalog,static_cast<std::size_t>(slots));if (!block) return std::unexpected(block.error());
        std::vector<ItemInstanceId> ids;for (auto n=block->first;n<block->end;++n) ids.push_back(ItemInstanceId::Create(n).value());
        std::array<std::uint8_t,16> operation{};
        if (RAND_bytes(operation.data(),static_cast<int>(operation.size()))!=1 || std::ranges::all_of(operation,[](auto b){return b==0;}))
            return std::unexpected(E::InvalidOperation);
        ItemGrant grant{award.key,award.level,award.count};
        grant.createdAtUtcMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        auto commit=MakeGrantCommit(catalog,operation,state->fence,state->storages,state->items,grant,ids,ItemStorageKind::Mall);
        if (!commit)
        {
            if (commit.error()==E::InvalidPlacement || commit.error()==E::LimitExceeded) return delivered;
            return std::unexpected(commit.error());
        }
        commit->nativeAward=award;
        auto written=CommitItems(db,catalog,*commit);
        if (!written && written.error()==E::CommitUnknown)
        {
            const auto receipt=FindItemReceipt(db,catalog,*commit);
            if (!receipt) return std::unexpected(receipt.error());
            if (*receipt) written=ItemWriteResult{**receipt,true};
            else written=CommitItems(db,catalog,*commit);
        }
        if (!written) return std::unexpected(written.error());
        ++delivered;
    }
    return delivered;
}
}
