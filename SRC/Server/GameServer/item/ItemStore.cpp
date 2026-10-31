#include "stdafx.h"
#include "ItemStore.hpp"
#include "../core/GameConfig.h"

#include <SQL/AsyncSQL.h>
#include <Core/Logging.hpp>

#include <exception>
#include <fmt/format.h>

namespace auriga::item::runtime
{
ItemStore& ItemStore::Instance()
{
	static ItemStore store;
	return store;
}

ItemStore::~ItemStore()
{
	Stop();
}

std::expected<void, std::string> ItemStore::Start(const GameSqlConfig& player, std::shared_ptr<const ItemCatalog> catalog)
{
	{
		std::lock_guard lock(mutex_);
		if (running_ || thread_.joinable()) return std::unexpected(std::string("the item store is already running"));
		if (!catalog) return std::unexpected(std::string("the item store needs the typed catalog"));
		catalog_ = std::move(catalog);
		stopping_ = false;
	}
	auto ready = std::make_shared<std::promise<std::string>>();
	auto connected = ready->get_future();
	thread_ = std::thread(&ItemStore::Run, this, player, ready);
	auto error = connected.get();
	if (!error.empty())
	{
		thread_.join();
		std::lock_guard lock(mutex_);
		catalog_.reset();
		return std::unexpected(std::move(error));
	}
	std::lock_guard lock(mutex_);
	ids_.clear();
	if (firstIds_)
		ids_.push_back(*firstIds_);
	firstIds_.reset();
	refillingIds_ = false;
	return {};
}

bool ItemStore::Running() const noexcept
{
	std::lock_guard lock(mutex_);
	return running_ && !stopping_;
}

bool ItemStore::Submit(Work work)
{
	{
		std::lock_guard lock(mutex_);
		if (!running_ || stopping_ || !work) return false;
		jobs_.push_back(std::move(work));
	}
	wake_.notify_one();
	return true;
}

std::size_t ItemStore::Drain(std::size_t limit)
{
	std::size_t applied = 0;
	while (applied < limit)
	{
		Completion next;
		{
			std::lock_guard lock(mutex_);
			if (done_.empty()) break;
			next = std::move(done_.front());
			done_.pop_front();
		}
		++applied;
		if (!next) continue;
		try { next(); }
		catch (const std::exception& error) { LOG_ERROR("ItemStore: completion failed: {}", error.what()); }
	}
	return applied;
}

void ItemStore::Stop()
{
	{
		std::lock_guard lock(mutex_);
		if (!thread_.joinable()) return;
		stopping_ = true;
	}
	wake_.notify_all();
	thread_.join();
	std::lock_guard lock(mutex_);
	if (!done_.empty()) LOG_INFO("ItemStore: {} completions dropped at shutdown", done_.size());
	done_.clear();
	catalog_.reset();
	running_ = false;
	ids_.clear();
	refillingIds_ = false;
}

std::vector<ItemInstanceId> ItemStore::PeekIds(std::size_t count) const
{
	std::vector<ItemInstanceId> out;
	for (const auto& block : ids_)
		for (auto id = block.first; id < block.end && out.size() < count; ++id)
			out.push_back(ItemInstanceId::Create(id).value());
	return out;
}

void ItemStore::SpendIds(std::size_t used)
{
	while (used && !ids_.empty())
	{
		auto& block = ids_.front();
		const auto take = std::min<std::uint64_t>(used, block.end - block.first);
		block.first += take;
		used -= static_cast<std::size_t>(take);
		if (block.first == block.end)
			ids_.pop_front();
	}
	if (AvailableIds() < IdBlockSize / 4)
		RefillIds();
}

std::size_t ItemStore::AvailableIds() const
{
	std::uint64_t total = 0;
	for (const auto& block : ids_)
		total += block.end - block.first;
	return static_cast<std::size_t>(total);
}

void ItemStore::RefillIds()
{
	if (refillingIds_)
		return;
	refillingIds_ = Submit([this](CAsyncSQL& sql, const ItemCatalog& catalog) -> Completion {
		auto block = persistence::ReserveItemInstanceIds(sql, catalog, IdBlockSize);
		if (!block)
			LOG_ERROR("ItemStore: no new instance ID block (error {})", static_cast<int>(block.error()));
		return [this, block] {
			refillingIds_ = false;
			if (block)
				ids_.push_back(*block);
		};
	});
}

void ItemStore::Run(GameSqlConfig player, std::shared_ptr<std::promise<std::string>> ready)
{
	CAsyncSQL connection;
	connection.SetTimeouts(10);
	connection.SetRetryConnectionErrors(false);
	if (!connection.Setup(player.host.c_str(), player.user.c_str(), player.password.c_str(), player.database.c_str(), "utf8mb4", true, player.port))
	{
		ready->set_value(fmt::format("the item store cannot connect to {}:{}", player.host, player.port));
		return;
	}
	std::shared_ptr<const ItemCatalog> catalog;
	{
		std::lock_guard lock(mutex_);
		catalog = catalog_;
	}
	// Without IDs the store still loads and changes existing items; it only
	// cannot create new ones (db/migrations/item_instance/002 missing?).
	auto ids = persistence::ReserveItemInstanceIds(connection, *catalog, IdBlockSize);
	if (!ids)
		LOG_ERROR("ItemStore: no instance ID block (error {}); new typed items cannot be created", static_cast<int>(ids.error()));
	{
		std::lock_guard lock(mutex_);
		running_ = true;
		firstIds_ = ids ? std::optional(*ids) : std::nullopt;
	}
	ready->set_value({});
	for (;;)
	{
		Work job;
		{
			std::unique_lock lock(mutex_);
			wake_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
			// Stop only once the queue is empty: queued writes are finished.
			if (jobs_.empty()) break;
			job = std::move(jobs_.front());
			jobs_.pop_front();
		}
		Completion completion;
		try { completion = job(connection, *catalog); }
		catch (const std::exception& error) { LOG_ERROR("ItemStore: job failed: {}", error.what()); }
		std::lock_guard lock(mutex_);
		done_.push_back(std::move(completion));
	}
}

bool RequestLoginLoad(std::uint32_t pid, std::function<void(LoginLoad)> done)
{
	const ItemOwner owner { ItemOwnerKind::Character, pid };
	return ItemStore::Instance().Submit([owner, done = std::move(done)](CAsyncSQL& sql, const ItemCatalog& catalog) mutable -> ItemStore::Completion {
		LoginLoad result { owner, 0, std::unexpected(ItemPersistenceError::InvalidOwner) };
		const auto session = persistence::ClaimOwnerSession(sql, catalog, owner);
		if (!session)
			result.loaded = std::unexpected(session.error());
		else
		{
			result.session = *session;
			result.loaded = persistence::LoadItemOwner(sql, catalog, owner, *session);
		}
		return [done = std::move(done), result = std::move(result)]() mutable { done(std::move(result)); };
	});
}
std::vector<ItemInstanceId> PeekInstanceIds(std::size_t count)
{
	return ItemStore::Instance().PeekIds(count);
}
void SpendInstanceIds(std::size_t used)
{
	ItemStore::Instance().SpendIds(used);
}
bool RequestCommit(ItemOwner owner, std::uint64_t session, ItemCommit commit, std::function<void(CommitOutcome)> done)
{
	return ItemStore::Instance().Submit([owner, session, commit = std::move(commit), done = std::move(done)](CAsyncSQL& sql, const ItemCatalog& catalog) mutable -> ItemStore::Completion {
		CommitOutcome outcome { persistence::CommitItems(sql, catalog, commit), std::unexpected(ItemPersistenceError::SqlFailure) };
		outcome.state = persistence::LoadItemOwner(sql, catalog, owner, session);
		return [done = std::move(done), outcome = std::move(outcome)]() mutable { done(std::move(outcome)); };
	});
}
} // namespace auriga::item::runtime
