// GameServer boot-time typed catalog (F4, IC-034) against an owned MariaDB.
// Driven by ItemCatalogRuntimeTests.py: stdin carries port, reader password,
// expected item count and expected hash. Never a live database.
#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/core/GameConfig.h"
#include "../../SRC/Server/GameServer/item/ItemCatalogRuntime.hpp"
#include <item/ItemCatalog.hpp>

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

		std::cout << "ItemCatalogRuntimeTests: " << checks << " checks passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cout << "FAILED: " << error.what() << '\n';
		return 1;
	}
}
