#pragma once
// Typed item store of the GameServer (item system F4, IC-034 decision B1).
//
// One background thread with its own direct SQL connection (sql.player
// account) runs every typed-item transaction. The game thread only submits
// work and applies completions in Drain(); it never waits on SQL. Runs only
// when the typed catalog is loaded (items.catalog = "required").
#include <ItemPersistenceSql.hpp>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <thread>
#include <vector>

struct GameSqlConfig;
class CAsyncSQL;

namespace auriga::item::runtime
{
class ItemStore final
{
public:
	using Completion = std::function<void()>;
	// Runs on the store thread; the returned completion runs on the game thread.
	using Work = std::function<Completion(CAsyncSQL&, const ItemCatalog&)>;

	static ItemStore& Instance();

	// Connects on the store thread and returns once connected or failed.
	std::expected<void, std::string> Start(const GameSqlConfig& player, std::shared_ptr<const ItemCatalog> catalog);
	bool Running() const noexcept;
	// False when the store is not running; the work is then not queued.
	bool Submit(Work work);
	// Game thread: applies at most `limit` completions, in submission order.
	std::size_t Drain(std::size_t limit = 64);
	// Finishes every queued job (writes are never abandoned), joins the
	// thread, then drops completions nobody will apply any more.
	void Stop();

	// Instance IDs for items the game creates (drops, purchases, rewards; F4
	// C7a). The store reserves them in blocks from the shared allocator: the
	// first at Start, the next in the background once a quarter is left.
	// Game thread only. Peek returns up to `count` IDs without spending them;
	// Spend then removes the first `used` of them.
	static constexpr std::uint32_t IdBlockSize = 4096;
	std::vector<ItemInstanceId> PeekIds(std::size_t count) const;
	void SpendIds(std::size_t used);
	std::size_t AvailableIds() const;

	ItemStore() = default;
	~ItemStore();
	ItemStore(const ItemStore&) = delete;
	ItemStore& operator=(const ItemStore&) = delete;

private:
	void Run(GameSqlConfig player, std::shared_ptr<std::promise<std::string>> ready);
	void RefillIds();

	mutable std::mutex mutex_;
	std::condition_variable wake_;
	std::deque<Work> jobs_;
	std::deque<Completion> done_;
	std::shared_ptr<const ItemCatalog> catalog_;
	std::thread thread_;
	bool running_ = false, stopping_ = false;
	// Handed from the store thread to Start.
	std::optional<persistence::ItemInstanceIdBlock> firstIds_;
	// Game thread only.
	std::deque<persistence::ItemInstanceIdBlock> ids_;
	bool refillingIds_ = false;
};

// Game-thread shorthands for the store's ID pool.
std::vector<ItemInstanceId> PeekInstanceIds(std::size_t count);
void SpendInstanceIds(std::size_t used);

// Login of a character: claim a new session generation, then load the owner
// in one job. MissingOwner means the character has no typed items yet.
struct LoginLoad
{
	ItemOwner owner;
	std::uint64_t session = 0;
	std::expected<persistence::LoadedItemOwner, ItemPersistenceError> loaded;
};
bool RequestLoginLoad(std::uint32_t pid, std::function<void(LoginLoad)> done);

// One typed-item operation of the current login: the prepared commit, then a
// fresh reload of the owner in the same job, so the game thread always gets
// back the authoritative state (also after a refused or conflicting commit).
struct CommitOutcome
{
	std::expected<persistence::ItemWriteResult, ItemPersistenceError> write;
	std::expected<persistence::LoadedItemOwner, ItemPersistenceError> state;
};
bool RequestCommit(ItemOwner owner, std::uint64_t session, ItemCommit commit, std::function<void(CommitOutcome)> done);
} // namespace auriga::item::runtime

namespace ecs
{
// The login the pending typed load belongs to; a completion for an older
// login (relog, logout) finds a different token and is dropped.
struct TypedItemLogin
{
	std::uint64_t token = 0;
};
// The character's typed items as loaded at login (read-only shadow while the
// legacy item runtime is still authoritative).
struct TypedItems
{
	auriga::item::persistence::LoadedItemOwner state;
};
// One typed operation of this character is in the item store; a second one
// waits for its answer instead of building on a state about to change.
struct TypedItemBusy {};
// Grants (monster drops, C7b-3) that wait for the character's operation in
// the store; the next one is sent when that operation answers.
struct TypedGrantQueue
{
	std::deque<auriga::item::ItemGrant> pending;
};
// When each consumable cooldown of this character ends, by cooldown profile,
// in steady-clock milliseconds. Runtime only: not saved across logins yet.
struct TypedItemCooldowns
{
	std::unordered_map<std::uint32_t, std::int64_t> readyAtMs;
};
} // namespace ecs
