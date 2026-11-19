#pragma once
// Dragon Coin (Sárkányérme) lives in the native account wallet:
// player.item_wallet owner_kind=2 (account), currency=2. account.account.coins is
// only an inbox for legacy and external top-ups; Sweep moves it into the wallet
// in one transaction and writes a ledger row, so no value is lost or doubled.
//
// Every write raises both the wallet and the account owner-fence revision. A
// typed item operation that still holds an older account snapshot then fails its
// CAS and reloads instead of overwriting the new balance (IC-068).
// The Database process and the GameServer both use these statements.
#include <SQL/AsyncSQL.h>
#include <SQL/PreparedTransaction.h>
#include <cstdint>
#include <cstdlib>
#include <memory>

namespace auriga::dragon_coin
{
inline constexpr std::uint64_t MaxBalance = 9223372036854775807ULL;

// Fence (session 1, revision 1) and an empty wallet, unless they already exist.
inline void EnsureWallet(SqlTransaction& tx, std::uint32_t account)
{
	tx.push_back({"INSERT IGNORE INTO player.item_owner_fence(owner_kind,owner_id,session_generation,state_revision) VALUES(2,?,1,1)",
		{SqlParam::UInt(account)}, {}, {}});
	tx.push_back({"INSERT IGNORE INTO player.item_wallet(owner_kind,owner_id,currency,state_revision,balance) VALUES(2,?,2,1,0)",
		{SqlParam::UInt(account)}, {}, {}});
}

inline void BumpFence(SqlTransaction& tx, std::uint32_t account)
{
	tx.push_back({"UPDATE player.item_owner_fence SET state_revision=state_revision+1 WHERE owner_kind=2 AND owner_id=?",
		{SqlParam::UInt(account)}, 1, {}});
}

// Adds `amount` (>0). Refused (rolled back) above MaxBalance.
inline SqlTransaction Credit(std::uint32_t account, std::uint64_t amount)
{
	SqlTransaction tx;
	EnsureWallet(tx, account);
	BumpFence(tx, account);
	tx.push_back({"UPDATE player.item_wallet SET balance=balance+?,state_revision=state_revision+1 WHERE owner_kind=2 AND owner_id=? AND currency=2 AND balance<=?",
		{SqlParam::UInt(amount), SqlParam::UInt(account), SqlParam::UInt(MaxBalance - amount)}, 1, {}});
	return tx;
}

// Takes `amount` (>0). Refused (rolled back) when the balance is lower.
inline SqlTransaction Debit(std::uint32_t account, std::uint64_t amount)
{
	SqlTransaction tx;
	EnsureWallet(tx, account);
	BumpFence(tx, account);
	tx.push_back({"UPDATE player.item_wallet SET balance=balance-?,state_revision=state_revision+1 WHERE owner_kind=2 AND owner_id=? AND currency=2 AND balance>=?",
		{SqlParam::UInt(amount), SqlParam::UInt(account), SqlParam::UInt(amount)}, 1, {}});
	return tx;
}

// Moves exactly `legacy` coins (the value just read from account.coins) into the
// wallet. If account.coins changed meanwhile, the guard read finds no row and
// the whole transaction rolls back; the next sweep reads the new value.
inline SqlTransaction Sweep(std::uint32_t account, std::uint64_t legacy)
{
	SqlTransaction tx;
	tx.push_back({"SELECT 1 FROM account.account WHERE id=? AND coins=? FOR UPDATE",
		{SqlParam::UInt(account), SqlParam::UInt(legacy)}, {}, 1});
	EnsureWallet(tx, account);
	BumpFence(tx, account);
	tx.push_back({"UPDATE player.item_wallet SET balance=balance+?,state_revision=state_revision+1 WHERE owner_kind=2 AND owner_id=? AND currency=2 AND balance<=?",
		{SqlParam::UInt(legacy), SqlParam::UInt(account), SqlParam::UInt(MaxBalance - legacy)}, 1, {}});
	tx.push_back({"UPDATE account.account SET coins=0 WHERE id=? AND coins=?",
		{SqlParam::UInt(account), SqlParam::UInt(legacy)}, 1, {}});
	tx.push_back({"INSERT INTO player.item_wallet_legacy_sweep(account_id,amount,swept_at) VALUES(?,?,NOW())",
		{SqlParam::UInt(account), SqlParam::UInt(legacy)}, 1, {}});
	return tx;
}

inline constexpr const char* LegacyCoinsQuery = "SELECT coins FROM account.account WHERE id=?";
inline constexpr const char* BalanceQuery = "SELECT balance FROM player.item_wallet WHERE owner_kind=2 AND owner_id=? AND currency=2";

// Runs the steps above on one direct connection. `Sql` is CAsyncSQL (or the
// Database process's CDBManager slot wrapper); it must expose
// DirectQueryPrepared and DirectTransactionPrepared returning SQLMsg*.
template<class Sql> bool Committed(Sql& sql, SqlTransaction tx)
{
	std::unique_ptr<SQLMsg> m(sql.DirectTransactionPrepared(std::move(tx)));
	return m && m->bTransaction && !m->uiSQLErrno && m->transactionOutcome == SqlTransactionOutcome::Committed;
}

template<class Sql> std::uint64_t ReadUnsigned(Sql& sql, const char* query, std::uint32_t account, bool& ok)
{
	ok = false;
	std::unique_ptr<SQLMsg> m(sql.DirectQueryPrepared(query, {SqlParam::UInt(account)}));
	if (!m || m->uiSQLErrno || !m->Get())
		return 0;
	ok = true;
	if (m->Get()->uiNumRows == 0)
		return 0;
	MYSQL_ROW row = m->Get()->FetchRow();
	if (!row || !row[0] || row[0][0] == '-')
		return 0;
	return std::strtoull(row[0], nullptr, 10);
}

// Sweeps the inbox (retrying once if it moved), then returns the wallet balance.
// `ok` is false when SQL failed; the balance is then unknown.
template<class Sql> std::uint64_t SweepAndRead(Sql& sql, std::uint32_t account, bool& ok)
{
	for (int attempt = 0; attempt < 2; ++attempt)
	{
		const auto legacy = ReadUnsigned(sql, LegacyCoinsQuery, account, ok);
		if (!ok)
			return 0;
		if (!legacy || Committed(sql, Sweep(account, legacy)))
			break;
	}
	return ReadUnsigned(sql, BalanceQuery, account, ok);
}
} // namespace auriga::dragon_coin
