#include "stdafx.h"
#include "ItemCatalogRuntime.hpp"
#include "../core/GameConfig.h"

#include "CatalogSqlReader.hpp"
#include "LiveMobDropsSql.hpp"
#include "ItemStore.hpp"
#include <SQL/AsyncSQL.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <ctime>

#include <atomic>
#include <fmt/format.h>

namespace auriga::item::runtime
{
namespace
{
std::atomic<std::shared_ptr<const ItemCatalog>> g_catalog;
std::atomic<std::shared_ptr<const sql::LiveMobDrops>> g_drops;
bool dropReadPending=false;
std::chrono::steady_clock::time_point nextDropPoll{};
std::string dropInstance,dropTokenHash,dropError;

void WriteDropStatus()
{
    const auto state=g_drops.load();if(!state || dropTokenHash.empty())return;
#ifdef _WIN32
    const int pid=_getpid();
#else
    const int pid=getpid();
#endif
    using Json=nlohmann::json;namespace fs=std::filesystem;
    const auto file=fs::current_path()/".auriga-drops-status.json";
    auto temporary=file;temporary+=".new-"+std::to_string(pid);
    if(fs::is_symlink(file) || fs::is_symlink(temporary))throw std::runtime_error("linked drop status file");
    Json value{{"protocol",1},{"pid",pid},{"instance",dropInstance},{"token_hash",dropTokenHash},
        {"updated_at",std::time(nullptr)},{"supported",state->supported},{"containers_supported",state->containersSupported},{"base_hash",state->baseHash},
        {"hash",state->hash},{"release",state->release},{"error",dropError},{"pending",dropReadPending}};
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out<<value.dump();out.flush();if(!out)throw std::runtime_error("drop status write failed");}
    fs::permissions(temporary,fs::perms::owner_read|fs::perms::owner_write,fs::perm_options::replace);
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("drop status publication failed");
#else
    fs::rename(temporary,file);
#endif
}

constexpr const char* ReadErrorNames[] = {
	"InvalidPin", "RowLimit", "ByteBudget", "ColumnCount", "NullValue", "InvalidNumber",
	"InvalidText", "InvalidKey", "RegistryMismatch", "FormatMismatch", "OrphanRow",
	"DuplicateRow", "InvalidChild", "InvalidCatalog", "HashMismatch", "SqlFailure",
	"IsolationRequired", "SnapshotUnconfirmed", "ResultShape",
};

std::string Describe(const sql::ReadError& error)
{
	const auto code = static_cast<std::size_t>(error.code);
	std::string text = code < std::size(ReadErrorNames) ? ReadErrorNames[code] : "Unknown";
	if (error.table)
	{
		const auto table = static_cast<std::size_t>(*error.table);
		text += fmt::format(" table={} row={} column={}", sql::Tables()[table].name, error.row, error.column);
	}
	if (error.sqlError) text += fmt::format(" sql_errno={}", error.sqlError);
	if (error.catalogError) text += fmt::format(" catalog_error={}", static_cast<int>(error.catalogError->code));
	return text;
}

std::string Hex(const ItemCatalogDigest& digest)
{
	std::string out;
	for (const auto byte : digest) out += fmt::format("{:02x}", byte);
	return out;
}

std::optional<ItemCatalogDigest> Unhex(const std::string& text)
{
	if (text.size() != 64) return std::nullopt;
	ItemCatalogDigest digest {};
	for (std::size_t i = 0; i < digest.size(); ++i)
	{
		const auto nibble = [](char c) -> int { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
		const int high = nibble(text[i * 2]), low = nibble(text[i * 2 + 1]);
		if (high < 0 || low < 0) return std::nullopt;
		digest[i] = static_cast<std::uint8_t>(high * 16 + low);
	}
	return digest;
}
} // namespace

std::expected<std::optional<CatalogSummary>, std::string> LoadCatalogAtBoot(const CatalogBootSettings& settings, const GameSqlConfig& player)
{
	if (settings.mode != CatalogMode::Required)
        return std::unexpected(std::string("the legacy item backend has been retired"));
	sql::ReadPin pin { settings.revision, std::nullopt };
	if (!settings.sha256.empty())
	{
		pin.expectedHash = Unhex(settings.sha256);
		if (!pin.expectedHash) return std::unexpected(std::string("items.catalog_sha256 is not a valid digest"));
	}
	// A dedicated direct connection: the reader needs its own REPEATABLE READ
	// snapshot and must never share the player queue.
	CAsyncSQL connection;
	connection.SetTimeouts(10);
	connection.SetRetryConnectionErrors(false);
	if (!connection.Setup(player.host.c_str(), player.user.c_str(), player.password.c_str(), "protos", "utf8mb4", true, player.port))
		return std::unexpected(fmt::format("cannot connect to the protos catalog at {}:{}", player.host, player.port));
	auto catalog = sql::ReadCatalog(connection, pin);
	if (!catalog) return std::unexpected("the protos catalog is not usable: " + Describe(catalog.error()));
	CatalogSummary summary { Hex((*catalog)->ContentHash()), (*catalog)->Revision(), (*catalog)->Entries().size() };
	g_catalog.store(std::move(*catalog));
    g_drops.store(nullptr);dropReadPending=false;dropError.clear();nextDropPoll={};
	return std::optional<CatalogSummary> { std::move(summary) };
}

std::shared_ptr<const ItemCatalog> CurrentCatalog() noexcept
{
	return g_catalog.load();
}
std::shared_ptr<const ItemCatalog> CurrentDropCatalog() noexcept
{
    const auto state=g_drops.load();return state?state->catalog:CurrentCatalog();
}
std::expected<void,std::string> LoadLiveDropCatalog(CAsyncSQL& db,std::shared_ptr<const ItemCatalog> base)
{
    auto loaded=sql::ReadLiveMobDrops(db,std::move(base));
    if(!loaded)return std::unexpected("Cannot load live drops: "+loaded.error());
    if(*loaded)g_drops.store(std::make_shared<const sql::LiveMobDrops>(std::move(**loaded)));
    return {};
}
void ConfigureDropReloadControl(const std::string& instance)
{
    dropInstance=instance;dropTokenHash.clear();const char* token=std::getenv("AURIGA_SHUTDOWN_TOKEN");
    if(!token || !std::regex_match(token,std::regex("[a-f0-9]{48}")))return;
    ItemCatalogDigest hash{};unsigned length=0;
    if(EVP_Digest(token,std::strlen(token),hash.data(),&length,EVP_sha256(),nullptr)!=1 || length!=hash.size())throw std::runtime_error("drop control token digest failed");
    dropTokenHash=sql::CatalogHashHex(hash);
}
void TickLiveDropReload()
{
    const auto now=std::chrono::steady_clock::now();if(now<nextDropPoll || !ItemStore::Instance().Running())return;
    nextDropPoll=now+std::chrono::seconds(1);
    try{WriteDropStatus();}catch(const std::exception& e){LOG_ERROR("Drop reload status: {}",e.what());}
    if(dropReadPending)return;
    const auto state=g_drops.load();const auto base=CurrentCatalog();if(!base)return;
    dropReadPending=ItemStore::Instance().Submit([base,known=state && state->supported?std::optional(state->release):std::nullopt](CAsyncSQL& db,const ItemCatalog&)->ItemStore::Completion{
        auto loaded=sql::ReadLiveMobDrops(db,base,known);
        return [loaded=std::move(loaded)]()mutable{
            dropReadPending=false;
            if(!loaded){if(dropError!=loaded.error())LOG_ERROR("Drop reload retained previous release: {}",loaded.error());dropError=loaded.error();}
            else{
                dropError.clear();
                if(*loaded){auto next=std::make_shared<const sql::LiveMobDrops>(std::move(**loaded));const auto old=g_drops.load();
                    if(!old || old->release!=next->release || old->hash!=next->hash || old->supported!=next->supported){g_drops.store(next);LOG_INFO("Live drops activated release={} hash={}",next->release,next->hash);}}
            }
            try{WriteDropStatus();}catch(const std::exception& e){LOG_ERROR("Drop reload status: {}",e.what());}
        };
    });
}

bool TypedDropsEnabled() noexcept
{
	return g_catalog.load() != nullptr;
}
} // namespace auriga::item::runtime
