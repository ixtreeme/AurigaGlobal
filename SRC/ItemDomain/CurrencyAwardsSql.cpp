#include "CurrencyAwardsSql.hpp"
#include <SQL/AsyncSQL.h>
#include <openssl/rand.h>
#include <algorithm>
#include <charconv>
#include <memory>
namespace auriga::item::persistence {
std::expected<unsigned,ItemPersistenceError> DeliverCurrencyAwards(CAsyncSQL& db,const ItemCatalog& c,ItemOwner account,std::string_view authenticatedLogin){
    using E=ItemPersistenceError;
    const auto key=c.CurrencyKey(CurrencyTokenMode::DynamicGold);if(!key)return 0u;
    const auto login=NormalizeCurrencyAwardLogin(authenticatedLogin);if(account.kind!=ItemOwnerKind::Account || !ValidItemOwner(account) || !login)return std::unexpected(E::InvalidOwner);
    std::unique_ptr<SQLMsg> rows(db.DirectQueryPrepared("SELECT id,amount FROM player.item_currency_award WHERE BINARY login=? AND claimed_operation IS NULL ORDER BY id LIMIT 32",{SqlParam::Blob(*login)}));
    if(!rows || rows->uiSQLErrno || !rows->Get())return std::unexpected(E::SqlFailure);
    std::vector<std::pair<std::uint64_t,std::uint64_t>> pending;
    for(auto row=rows->Get()->FetchRow();row;row=rows->Get()->FetchRow()){
        const auto read=[&](unsigned n)->std::optional<std::uint64_t>{if(!row[n])return {};std::string_view s(row[n]);std::uint64_t v=0;const auto parsed=std::from_chars(s.data(),s.data()+s.size(),v);if(parsed.ec!=std::errc{} || parsed.ptr!=s.data()+s.size() || !v || v>INT64_MAX)return {};return v;};
        const auto id=read(0),amount=read(1);if(!id || !amount)return std::unexpected(E::InvalidStoredData);pending.emplace_back(*id,*amount);
    }
    unsigned delivered=0;
    for(auto [id,amount]:pending){
        auto state=LoadCurrentItemOwner(db,c,account);if(!state)return std::unexpected(state.error());
        const auto block=ReserveItemInstanceIds(db,c,1);if(!block)return std::unexpected(block.error());const auto instance=ItemInstanceId::Create(block->first);if(!instance)return std::unexpected(E::InvalidItem);
        std::array<std::uint8_t,16> operation{};if(RAND_bytes(operation.data(),operation.size())!=1 || std::ranges::all_of(operation,[](auto ch){return ch==0;}))return std::unexpected(E::InvalidOperation);
        ItemGrant grant{*key,0,1};grant.currencyAmount=amount;const std::array ids{*instance};
        auto commit=MakeGrantCommit(c,operation,state->fence,state->storages,state->items,grant,ids,ItemStorageKind::Safebox);
        if(!commit){if(commit.error()==E::InvalidPlacement)return delivered;return std::unexpected(commit.error());}
        commit->currencyAward=CurrencyAwardClaim{id,amount,*login};if(!ValidateItemCommit(c,*commit))return std::unexpected(E::InvalidChange);
        auto written=CommitItems(db,c,*commit);
        if(!written && written.error()==E::CommitUnknown){
            const auto found=FindItemReceipt(db,c,*commit);if(!found)return std::unexpected(found.error());
            if(*found)written=ItemWriteResult{**found,true};else written=CommitItems(db,c,*commit);
        }
        if(!written)return std::unexpected(written.error());++delivered;
    }
    return delivered;
}
}
