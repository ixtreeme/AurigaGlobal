#include "TypedCostumes.hpp"
#include "stdafx.h"
#include "ItemStore.hpp"
#include "TypedBooks.hpp"
#include "TypedAccessoryClock.hpp"
#include "TypedCompanions.hpp"
#include "TypedRecovery.hpp"
#include "TypedDungeonEntry.hpp"
#include <PrivateShopsSql.hpp>
#include <DungeonEntrySql.hpp>
#include "TypedSoulClock.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../core/GameConfig.h"

#include <SQL/AsyncSQL.h>
#include <StoragePersistenceSql.hpp>
#include <Core/Logging.hpp>

#include <exception>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <set>
#include <openssl/evp.h>

namespace auriga::item::runtime
{
namespace { std::vector<persistence::CharacterBirthProfile> characterBirth; }
std::expected<void,std::string> LoadNativeCharacterBirth(const std::string& file)
{
    characterBirth.clear();if(!std::filesystem::exists(file))return {};
    try {
        if(std::filesystem::file_size(file)>256*1024)return std::unexpected("native starter pack exceeds 256 KiB");
        std::ifstream stream(file);const auto document=nlohmann::json::parse(stream);
        if(document.at("version")!=1 || document.at("profiles").size()!=16)return std::unexpected("invalid native starter profiles");
        std::set<std::pair<unsigned,unsigned>> seen;
        for(const auto& row:document.at("profiles")) {
            const auto race=row.at("race").get<unsigned>(),group=row.at("skill_group").get<unsigned>();
            persistence::CharacterBirthProfile profile;profile.race=static_cast<std::uint8_t>(race);profile.skillGroup=static_cast<std::uint8_t>(group);profile.contract=row.at("contract");
            if(race>7 || group<1 || group>2 || !seen.emplace(race,group).second ||
               profile.contract.size()!=64 || profile.contract.find_first_not_of("0123456789abcdef")!=std::string::npos)return std::unexpected("invalid native starter contract");
            auto content=row;content.erase("contract");const auto bytes=content.dump();ItemCatalogDigest digest{};unsigned length=0;
            if(EVP_Digest(bytes.data(),bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)!=1 || length!=digest.size())return std::unexpected("native starter contract hashing failed");
            std::string hex;for(auto value:digest)hex+=fmt::format("{:02x}",value);if(hex!=profile.contract)return std::unexpected("native starter contract hash mismatch");
            for(const auto& value:row.at("grants")) {
                const auto key=ItemKey::Create(value.at("category").get<std::uint16_t>(),value.at("local").get<std::uint32_t>());
                if(!key)return std::unexpected("invalid native starter key");
                persistence::CharacterBirthGrant grant{{*key}};grant.item.refinementLevel=value.at("level");grant.item.count=value.at("count");grant.item.tryMagic=value.at("try_magic");
                if(value.contains("equipped"))grant.equipped=static_cast<EquipmentSlot>(value.at("equipped").get<unsigned>());
                if(value.contains("attributes")) {grant.attributes.emplace();std::size_t slot=0;for(const auto& a:value.at("attributes")) {if(slot>=5)return std::unexpected("too many native starter bonuses");grant.attributes->slots[slot++]=BonusEntry{static_cast<BonusId>(a.at("bonus").get<unsigned>()),a.at("value").get<std::int64_t>()};}}
                const auto catalog=CurrentCatalog();const auto* entry=catalog ? catalog->Find(*key) : nullptr;
                if(!entry || !SupportsPersistentGrant(*catalog,*entry) || !catalog->RefinedBaseStats(*key,grant.item.refinementLevel) ||
                   !grant.item.count || grant.item.count>entry->base.maxStack || (grant.attributes && !ValidateInstanceAttributes(*catalog,*key,*grant.attributes)))return std::unexpected("native starter definition or bonuses are unavailable");
                profile.grants.push_back(std::move(grant));
            }
            if(profile.grants.empty() || profile.grants.size()>32)return std::unexpected("invalid native starter grant count");
            characterBirth.push_back(std::move(profile));
        }
        return {};
    }catch(const std::exception& error){return std::unexpected(std::string("cannot load native starter pack: ")+error.what());}
}
ItemStore& ItemStore::Instance()
{
	static ItemStore store;
	return store;
}

ItemStore::~ItemStore()
{
	Stop();
}

std::expected<void, std::string> ItemStore::Start(const GameSqlConfig& player, std::shared_ptr<const ItemCatalog> catalog, std::string groundCore)
{
	{
		std::lock_guard lock(mutex_);
		if (running_ || thread_.joinable()) return std::unexpected(std::string("the item store is already running"));
		if (!catalog) return std::unexpected(std::string("the item store needs the typed catalog"));
		catalog_ = std::move(catalog);
        groundRun_.reset();
		stopping_ = false;
	}
	auto ready = std::make_shared<std::promise<std::string>>();
	auto connected = ready->get_future();
	thread_ = std::thread(&ItemStore::Run, this, player, ready, std::move(groundCore));
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

std::optional<ItemGroundChange> ItemStore::GroundRun() const
{
    std::lock_guard lock(mutex_); return groundRun_;
}

bool ItemStore::Running() const noexcept
{
	std::lock_guard lock(mutex_);
	return running_ && !stopping_;
}

bool ItemStore::Submit(Work work, std::chrono::milliseconds delay)
{
	{
		std::lock_guard lock(mutex_);
		if (!running_ || stopping_ || !work) return false;
		jobs_.emplace(std::chrono::steady_clock::now() + delay, std::move(work));
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

void ItemStore::Run(GameSqlConfig player, std::shared_ptr<std::promise<std::string>> ready, std::string groundCore)
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

    std::unique_ptr<SQLMsg> format(connection.DirectQuery("SELECT native_runtime_version,(SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='player' AND TABLE_NAME='item_native_award' AND ENGINE='InnoDB') FROM player.item_persistence_format WHERE singleton=1"));
    if (!format || format->uiSQLErrno || !format->Get() || format->Get()->uiNumRows != 1)
    {
        ready->set_value("native item runtime requires migrations 041_native_runtime.sql and 042_native_item_awards.sql");
        return;
    }
    const auto row = format->Get()->FetchRow();
    if (!row || !row[0] || !row[1] || std::string_view(row[0]) != "1" || std::string_view(row[1]) != "1")
    {
        ready->set_value("unsupported native item runtime format");
        return;
    }

    if(const auto drops=LoadLiveDropCatalog(connection,catalog);!drops){ready->set_value(drops.error());return;}
    if(!groundCore.empty())
    {
        const auto run=persistence::BeginOwnedGroundRun(connection,*catalog,groundCore);
        if(!run) { ready->set_value(fmt::format("owned ground startup failed (error {}); apply migration 003_owned_ground.sql",static_cast<int>(run.error()))); return; }
        std::lock_guard lock(mutex_); groundRun_=ItemGroundChange{std::move(groundCore),*run,*run,0,false};
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
			while (!stopping_)
			{
				if (jobs_.empty())
					wake_.wait(lock);
				else if (jobs_.begin()->first > std::chrono::steady_clock::now())
					wake_.wait_until(lock, jobs_.begin()->first);
				else
					break;
			}
			// Stop only once the queue is empty: queued writes are finished.
			if (jobs_.empty()) break;
			job = std::move(jobs_.begin()->second);
			jobs_.erase(jobs_.begin());
		}
		Completion completion;
		try { completion = job(connection, *catalog); }
		catch (const std::exception& error) { LOG_ERROR("ItemStore: job failed: {}", error.what()); }
		std::lock_guard lock(mutex_);
		done_.push_back(std::move(completion));
	}
}

bool RequestPlayerLoginLoad(std::uint32_t pid,std::function<void(LoginLoad)> done)
{
    return ItemStore::Instance().Submit([pid,done=std::move(done)](CAsyncSQL& sql,const ItemCatalog& catalog)->ItemStore::Completion{
        auto ready=persistence::EnsureFreshCharacterOwner(sql,catalog,pid);
        if(ready && *ready && !characterBirth.empty()) {
            const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            if(auto birth=persistence::IssueFreshCharacterItems(sql,catalog,pid,characterBirth,now);!birth)ready=std::unexpected(birth.error());
        }
        return [pid,done,ready]{
            if(!ready){done(LoginLoad{{ItemOwnerKind::Character,pid},0,std::unexpected(ready.error())});return;}
            if(!RequestLoginLoad(pid,done))done(LoginLoad{{ItemOwnerKind::Character,pid},0,std::unexpected(ItemPersistenceError::SqlFailure)});
        };
    });
}
bool RequestLoginLoad(std::uint32_t pid, std::function<void(LoginLoad)> done)
{
    if(DeferDungeonEntryLogin(pid,[pid,done]{RequestLoginLoad(pid,done);}))return true;
    if(DeferTypedBookLogin(pid,[pid,done]{RequestLoginLoad(pid,done);})){return true;}
    if(DeferRecoveryLogin(pid,[pid,done]{RequestLoginLoad(pid,done);})){TickTypedRecovery();return true;}
    if(DeferAccessoryLogin(pid,[pid,done]{RequestLoginLoad(pid,done);})){TickTypedAccessoryClocks();return true;}
    if(DeferGrowthPetLogin(pid,[pid,done]{RequestLoginLoad(pid,done);}))return true;
    if(DeferPetSkinLogin(pid,[pid,done]{RequestLoginLoad(pid,done);}))return true;
    if(DeferSoulLogin(pid,[pid,done]{RequestLoginLoad(pid,done);}))return true;
    if(DeferCostumeLogin(pid,[pid,done]{RequestLoginLoad(pid,done);}))return true;
    if(DeferFixedCompanionLogin(pid,[pid,done]{RequestLoginLoad(pid,done);}))return true;
	const ItemOwner owner { ItemOwnerKind::Character, pid };
	return ItemStore::Instance().Submit([owner, done = std::move(done)](CAsyncSQL& sql, const ItemCatalog& catalog) mutable -> ItemStore::Completion {
		LoginLoad result { owner, 0, std::unexpected(ItemPersistenceError::InvalidOwner) };
		const auto session = persistence::ClaimOwnerSession(sql, catalog, owner);
		if (!session)
			result.loaded = std::unexpected(session.error());
		else
		{
			result.session = *session;
			// An already native owner receives storages added later (C47 special
			// inventory); a missing owner stays missing (ClaimOwnerSession above).
			if (const auto ensured = persistence::EnsureRuntimeStorageOwner(sql, catalog, owner); !ensured)
				LOG_ERROR("TypedItems: pid {} storages could not be completed (error {})", owner.id, static_cast<int>(ensured.error()));
            const auto released=persistence::RecoverDungeonEntries(sql,catalog,owner,*session);
            if(!released && released.error()!=ItemPersistenceError::MissingOwner)result.loaded=std::unexpected(released.error());
            else {
                const auto recovered=persistence::RecoverPrivateShop(sql,catalog,owner,*session);
                if(!recovered && recovered.error()!=ItemPersistenceError::MissingOwner)result.loaded=std::unexpected(recovered.error());
                else result.loaded = persistence::LoadItemOwner(sql, catalog, owner, *session);
            }
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
namespace
{
struct PendingCommit
{
	std::vector<OwnerRead> owners;
	ItemCommit commit;
	std::function<void(MultiCommitOutcome)> done;
    std::optional<std::expected<persistence::ItemWriteResult,ItemPersistenceError>> settled;
	bool reconciling = false;
};

bool QueueCommit(std::shared_ptr<PendingCommit> pending, std::chrono::milliseconds delay = {})
{
	return ItemStore::Instance().Submit([pending](CAsyncSQL& sql, const ItemCatalog& catalog) -> ItemStore::Completion {
		std::expected<persistence::ItemWriteResult, ItemPersistenceError> write = std::unexpected(ItemPersistenceError::CommitUnknown);
		if (!pending->settled && !pending->reconciling)
		{
			write = persistence::CommitItems(sql, catalog, pending->commit);
			pending->reconciling = !write && write.error() == ItemPersistenceError::CommitUnknown;
			if (pending->reconciling)
				LOG_ERROR("ItemStore: owner {} commit acknowledgement lost; reconciling its original receipt", pending->owners.front().owner.id);
		}
		if (!pending->settled && pending->reconciling)
		{
			const auto receipt = persistence::FindItemReceipt(sql, catalog, pending->commit);
			if (!receipt)
				return [pending] { QueueCommit(pending, std::chrono::seconds(1)); };
			// A confirmed absence after the locking read means the original write
			// rolled back. Until that read succeeds, keep the caller (and ground
			// loot) pending, even if the character has logged out meanwhile.
			if (*receipt)
				write = persistence::ItemWriteResult { **receipt, true };
			else
				write = std::unexpected(ItemPersistenceError::SqlFailure);
		}
        if(!pending->settled) pending->settled=std::move(write);
        MultiCommitOutcome outcome{*pending->settled,{}};
        for(const auto& owner:pending->owners)
        {
            auto state=owner.session ? persistence::LoadItemOwner(sql,catalog,owner.owner,owner.session) : persistence::LoadCurrentItemOwner(sql,catalog,owner.owner);
            if(!state && state.error()==ItemPersistenceError::SqlFailure)
                return [pending] { QueueCommit(pending,std::chrono::seconds(1)); };
            outcome.states.push_back(std::move(state));
        }
		return [pending, outcome = std::move(outcome)]() mutable { pending->done(std::move(outcome)); };
	}, delay);
}
} // namespace

bool RequestCommit(ItemOwner owner, std::uint64_t session, ItemCommit commit, std::function<void(CommitOutcome)> done)
{
    return RequestCommitMany({{owner,session}},std::move(commit),[done=std::move(done)](MultiCommitOutcome result) mutable {
        done(CommitOutcome{std::move(result.write),std::move(result.states.front())});
    });
}
bool RequestCommitMany(std::vector<OwnerRead> owners,ItemCommit commit,std::function<void(MultiCommitOutcome)> done)
{
    if(owners.empty()) return false;
    auto clock=BeginAccessoryClockCommit(owners,commit);if(!clock)return false;
    auto recovery=BeginRecoveryClockCommit(owners,commit);if(!recovery){(*clock)(nullptr);return false;}
    auto pet=BeginGrowthPetClockCommit(owners,commit);
    auto skins=BeginPetSkinCommit(owners,commit);
    auto fixed=BeginFixedCompanionCommit(owners,commit);
    auto costumes=BeginCostumeCommit(owners,commit);
    auto souls=BeginSoulCommit(owners,commit);
    auto completed=[settle=*clock,recover=*recovery,pet,skins,fixed,costumes,souls,done=std::move(done)](MultiCommitOutcome result) mutable {settle(&result);recover(&result);pet(&result);skins(&result);fixed(&result);costumes(&result);souls(&result);done(std::move(result));};
    const bool queued=QueueCommit(std::make_shared<PendingCommit>(PendingCommit{std::move(owners),std::move(commit),std::move(completed)}));
    if(!queued){(*clock)(nullptr);(*recovery)(nullptr);pet(nullptr);skins(nullptr);fixed(nullptr);costumes(nullptr);souls(nullptr);}return queued;
}
} // namespace auriga::item::runtime
