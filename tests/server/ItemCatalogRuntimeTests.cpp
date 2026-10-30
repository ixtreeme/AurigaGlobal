// GameServer boot-time typed catalog (F4, IC-034) against an owned MariaDB.
// Driven by ItemCatalogRuntimeTests.py: stdin carries port, reader password,
// expected item count and expected hash. Never a live database.
#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/core/GameConfig.h"
#include "../../SRC/Server/GameServer/item/ItemCatalogRuntime.hpp"
#include <item/ItemCatalog.hpp>
#include "../../SRC/Server/GameServer/item/ItemStore.hpp"
#include <SQL/AsyncSQL.h>

#include <chrono>
#include <thread>
#include <vector>

#include <iostream>
#include <stdexcept>
#include <string>

namespace runtime = auriga::item::runtime;

namespace
{
int checks = 0;
void Check(bool result, const std::string& message)
{
	++checks;
	if (!result) throw std::runtime_error(message);
}
}

int main(int argc, char** argv)
{
	try
	{
		if (argc != 2 || std::string(argv[1]) != "--owned-sandbox") throw std::runtime_error("run through ItemCatalogRuntimeTests.py");
		std::string port, password, items, hash;
		std::getline(std::cin, port); std::getline(std::cin, password); std::getline(std::cin, items); std::getline(std::cin, hash);
		GameSqlConfig player { "127.0.0.1", std::stoi(port), "game_reader", password, "player" };

		auto off = runtime::LoadCatalogAtBoot({ runtime::CatalogMode::Off, 1, {} }, player);
		Check(off.has_value() && !off->has_value() && !runtime::CurrentCatalog(), "off mode loaded something");

		auto loaded = runtime::LoadCatalogAtBoot({ runtime::CatalogMode::Required, 1, hash }, player);
		Check(loaded.has_value() && loaded->has_value(), "required catalog did not load: " + (loaded ? std::string() : loaded.error()));
		Check((*loaded)->items == std::stoull(items) && (*loaded)->sha256 == hash && (*loaded)->revision == 1, "summary differs from the catalog");
		const auto catalog = runtime::CurrentCatalog();
		Check(catalog && catalog->Entries().size() == std::stoull(items), "loaded catalog is not published");

		auto pinned = runtime::LoadCatalogAtBoot({ runtime::CatalogMode::Required, 1, std::string(64, '0') }, player);
		Check(!pinned && pinned.error().find("HashMismatch") != std::string::npos, "a different release pin was accepted");
		Check(runtime::CurrentCatalog() == catalog, "a failed load replaced the published catalog");

		auto denied = runtime::LoadCatalogAtBoot({ runtime::CatalogMode::Required, 1, {} }, GameSqlConfig { "127.0.0.1", std::stoi(port), "game_reader", "wrong", "player" });
		Check(!denied && denied.error().find("cannot connect") != std::string::npos && denied.error().find("wrong") == std::string::npos,
			"a wrong password connected or leaked into the error");

		auto bad = runtime::LoadCatalogAtBoot({ runtime::CatalogMode::Required, 1, "xyz" }, player);
		Check(!bad, "an invalid pin was accepted");

		// Item store: own thread and connection; login claim + load; ordered
		// completions applied on the calling (game) thread only; clean stop.
		{
			using namespace auriga::item;
			auto& store = runtime::ItemStore::Instance();
			Check(!store.Running() && !store.Submit([](CAsyncSQL&, const ItemCatalog&) { return runtime::ItemStore::Completion {}; }), "store accepts work before start");
			CAsyncSQL importer; importer.SetTimeouts(5); importer.SetRetryConnectionErrors(false);
			Check(importer.Setup("127.0.0.1", "root", password.c_str(), "player", "utf8mb4", true, std::stoi(port)), "importer connection");
			const ItemOwner provisioned { ItemOwnerKind::Character, 7001 };
			const ItemStorageLayout bag { { provisioned, ItemStorageKind::Inventory, 1 }, 5, 9, 4 };
			const ItemWallet gold { provisioned, WalletCurrency::Gold, 1, 777 };
			auto made = persistence::ProvisionOwner(importer, *catalog, provisioned, std::span(&bag, 1), std::span(&gold, 1));
			Check(made && *made, "owner provisioned for the store test");
			Check(!store.Start(GameSqlConfig { "127.0.0.1", std::stoi(port), "game_reader", "wrong", "player" }, catalog), "store started with a wrong password");
			Check(!store.Running(), "failed start left the store running");
			Check(store.Start(player, catalog).has_value() && store.Running(), "store did not start");
			Check(!store.Start(player, catalog), "store started twice");
			const auto gameThread = std::this_thread::get_id();
			std::vector<std::string> order;
			auto record = [&](std::string tag) { return [&order, tag, gameThread](runtime::LoginLoad result) {
				if (std::this_thread::get_id() != gameThread) throw std::runtime_error("completion ran off the game thread");
				order.push_back(tag + ":" + (result.loaded ? "loaded" + std::to_string(result.session) + "/" + std::to_string(result.loaded->wallets.at(0).balance)
				                                           : "error" + std::to_string(static_cast<int>(result.loaded.error()))));
			}; };
			Check(runtime::RequestLoginLoad(7001, record("first")) && runtime::RequestLoginLoad(7002, record("missing")) &&
			      runtime::RequestLoginLoad(7001, record("relog")), "login loads not queued");
			for (int spins = 0; order.size() < 3 && spins < 1000; ++spins)
			{
				store.Drain();
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			const auto missing = "missing:error" + std::to_string(static_cast<int>(ItemPersistenceError::MissingOwner));
			Check(order == std::vector<std::string> { "first:loaded2/777", missing, "relog:loaded3/777" }, "login loads out of order or wrong");
			// Queued work is finished at stop; its completion is dropped, not run late.
			bool ran = false, applied = false;
			Check(store.Submit([&ran, &applied](CAsyncSQL&, const ItemCatalog&) { ran = true; return runtime::ItemStore::Completion { [&applied] { applied = true; } }; }), "work refused");
			store.Stop();
			Check(ran && !store.Running() && store.Drain() == 0 && !applied, "stop did not finish queued work or ran a late completion");
			Check(!store.Submit([](CAsyncSQL&, const ItemCatalog&) { return runtime::ItemStore::Completion {}; }), "store accepts work after stop");
		}

		std::cout << "ItemCatalogRuntimeTests: " << checks << " checks passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cout << "FAILED: " << error.what() << '\n';
		return 1;
	}
}
