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
#include <optional>
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
		std::string port, password, items, hash, proxyPort;
		std::getline(std::cin, port); std::getline(std::cin, password); std::getline(std::cin, items); std::getline(std::cin, hash);
		std::getline(std::cin, proxyPort);
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
            Check(persistence::ProvisionGroundStorage(importer,*catalog,provisioned).has_value(), "importer pre-provisions the ground escrow");
			Check(!store.Start(GameSqlConfig { "127.0.0.1", std::stoi(port), "game_reader", "wrong", "player" }, catalog), "store started with a wrong password");
			Check(!store.Running(), "failed start left the store running");
			Check(store.Start(player, catalog, "runtime-test-core").has_value() && store.Running(), "store did not start");
            Check(store.GroundRun() && store.GroundRun()->run==1, "store starts a fenced owned-ground run");
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
			// C3: a move through the store is committed and answered with the reloaded state;
			// a move built from an outdated state is refused and the reload stays authoritative.
			{
				auto owned = persistence::LoadItemOwner(importer, *catalog, provisioned, 3);
				Check(owned.has_value(), "relogged owner loads at session 3");
				PersistentItem sword { ItemInstanceId::Create(0x00C3000000000001ULL).value(), ItemKey::Create(1, 1).value(), provisioned };
				sword.placement = ItemPlacement { ItemStorageKind::Inventory, 1, 0 };
				ItemCommit grant; grant.operationId.fill(0x31); grant.catalogRevision = catalog->Revision(); grant.catalogHash = catalog->ContentHash();
				grant.kind = ItemOperationKind::Grant; grant.owners = { owned->fence }; grant.storages = { bag };
				grant.items.push_back({ std::nullopt, sword }); grant.outcomeCode = 1; grant.result = { 1 };
				Check(persistence::CommitItems(importer, *catalog, grant).has_value(), "grant for the move test");
				auto before = persistence::LoadItemOwner(importer, *catalog, provisioned, 3);
				Check(before && before->items.size() == 1, "granted sword loads");
				std::array<std::uint8_t, 16> op {}; op.fill(0x32);
				auto move = MakeMoveCommit(*catalog, op, before->fence, before->storages, before->items, sword.id, ItemPlacement { ItemStorageKind::Inventory, 1, 7 });
				Check(move.has_value(), "move commit built");
				std::optional<runtime::CommitOutcome> got;
				const auto wait = [&] { for (int spins = 0; !got && spins < 1000; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
				Check(runtime::RequestCommit(provisioned, 3, *move, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "move not queued");
				wait();
				Check(got && got->write && !got->write->replay && got->state && got->state->items.at(0).placement->anchor == 7 &&
				      got->state->items.at(0).revision == before->items.at(0).revision + 1, "move committed and reloaded");
				op.fill(0x33);
				auto stale = MakeMoveCommit(*catalog, op, before->fence, before->storages, before->items, sword.id, ItemPlacement { ItemStorageKind::Inventory, 1, 9 });
				got.reset();
				Check(stale && runtime::RequestCommit(provisioned, 3, *stale, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "stale move not queued");
				wait();
				Check(got && !got->write && got->state && got->state->items.at(0).placement->anchor == 7, "stale move refused, reload authoritative");
				got.reset();
				auto previousLogin = *move; previousLogin.operationId.fill(0x34); previousLogin.owners[0].sessionGeneration = 2;
				Check(runtime::RequestCommit(provisioned, 2, previousLogin, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "old-session move not queued");
				wait();
				Check(got && !got->write && !got->state, "a previous login's session can neither write nor reload");
			}
			// C4: equip and unequip through the store, each answered with the reloaded state.
			{
				const ItemOwner wearer { ItemOwnerKind::Character, 7003 };
				const ItemStorageLayout layouts[] { { { wearer, ItemStorageKind::Inventory, 1 }, 5, 9, 4 }, { { wearer, ItemStorageKind::Equipment, 1 }, 8, 1, 1 } };
				const ItemWallet purse { wearer, WalletCurrency::Gold, 1, 0 };
				auto made = persistence::ProvisionOwner(importer, *catalog, wearer, layouts, std::span(&purse, 1));
				Check(made && *made, "wearer provisioned with inventory and equipment");
				const auto session = persistence::ClaimOwnerSession(importer, *catalog, wearer);
				Check(session && *session == 2, "wearer session claimed");
				auto owned = persistence::LoadItemOwner(importer, *catalog, wearer, *session);
				PersistentItem blade { ItemInstanceId::Create(0x00C4000000000001ULL).value(), ItemKey::Create(1, 1).value(), wearer };
				blade.placement = ItemPlacement { ItemStorageKind::Inventory, 1, 4 };
				ItemCommit grant; grant.operationId.fill(0x41); grant.catalogRevision = catalog->Revision(); grant.catalogHash = catalog->ContentHash();
				grant.kind = ItemOperationKind::Grant; grant.owners = { owned->fence }; grant.storages = { layouts[0] };
				grant.items.push_back({ std::nullopt, blade }); grant.outcomeCode = 1; grant.result = { 1 };
				Check(persistence::CommitItems(importer, *catalog, grant).has_value(), "grant for the equip test");
				auto before = persistence::LoadItemOwner(importer, *catalog, wearer, *session);
				std::array<std::uint8_t, 16> op {}; op.fill(0x42);
				auto equip = MakeEquipCommit(*catalog, op, before->fence, before->storages, before->items, blade.id, EquipContext { 1, 0, 0 });
				Check(equip.has_value(), "equip commit built");
				std::optional<runtime::CommitOutcome> got;
				const auto wait = [&] { for (int spins = 0; !got && spins < 1000; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
				Check(runtime::RequestCommit(wearer, *session, *equip, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "equip not queued");
				wait();
				Check(got && got->write && got->state && got->state->items.at(0).placement == ItemPlacement { ItemStorageKind::Equipment, 1, 0 }, "equipped and reloaded");
				op.fill(0x43);
				auto off = MakeUnequipCommit(*catalog, op, got->state->fence, got->state->storages, got->state->items, blade.id, ItemPlacement { ItemStorageKind::Inventory, 1, 11 });
				Check(off.has_value(), "unequip commit built");
				got.reset();
				Check(runtime::RequestCommit(wearer, *session, *off, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "unequip not queued");
				wait();
				Check(got && got->write && got->state && got->state->items.at(0).placement == ItemPlacement { ItemStorageKind::Inventory, 1, 11 }, "taken off and reloaded");
			}
			// C5: a flask's units are spent through the store; the last one leaves no live item.
			{
				const ItemOwner drinker { ItemOwnerKind::Character, 7005 };
				const ItemStorageLayout pack { { drinker, ItemStorageKind::Inventory, 1 }, 5, 9, 4 };
				const ItemWallet purse { drinker, WalletCurrency::Gold, 1, 0 };
				auto made = persistence::ProvisionOwner(importer, *catalog, drinker, std::span(&pack, 1), std::span(&purse, 1));
				Check(made && *made, "drinker provisioned");
				const auto session = persistence::ClaimOwnerSession(importer, *catalog, drinker);
				Check(session.has_value(), "drinker session claimed");
				auto owned = persistence::LoadItemOwner(importer, *catalog, drinker, *session);
				PersistentItem flask { ItemInstanceId::Create(0x00C5000000000001ULL).value(), ItemKey::Create(5, 1).value(), drinker };
				flask.count = 2; flask.placement = ItemPlacement { ItemStorageKind::Inventory, 1, 6 };
				ItemCommit grant; grant.operationId.fill(0x51); grant.catalogRevision = catalog->Revision(); grant.catalogHash = catalog->ContentHash();
				grant.kind = ItemOperationKind::Grant; grant.owners = { owned->fence }; grant.storages = { pack };
				grant.items.push_back({ std::nullopt, flask }); grant.outcomeCode = 1; grant.result = { 1 };
				Check(persistence::CommitItems(importer, *catalog, grant).has_value(), "grant for the consume test");
				auto before = persistence::LoadItemOwner(importer, *catalog, drinker, *session);
				Check(before && before->items.size() == 1 && before->items[0].count == 2, "granted flask loads");
				std::array<std::uint8_t, 16> op {}; op.fill(0x52);
				auto sip = MakeConsumeCommit(*catalog, op, before->fence, before->storages, before->items, flask.id, 1);
				Check(sip.has_value(), "consume commit built");
				std::optional<runtime::CommitOutcome> got;
				const auto wait = [&] { for (int spins = 0; !got && spins < 1000; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
				Check(runtime::RequestCommit(drinker, *session, *sip, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "consume not queued");
				wait();
				Check(got && got->write && got->state && got->state->items.at(0).count == 1, "one unit spent and reloaded");
				op.fill(0x53);
				auto last = MakeConsumeCommit(*catalog, op, got->state->fence, got->state->storages, got->state->items, flask.id, 1);
				Check(last.has_value(), "last consume commit built");
				got.reset();
				Check(runtime::RequestCommit(drinker, *session, *last, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "last consume not queued");
				wait();
				Check(got && got->write && got->state && std::ranges::none_of(got->state->items, [](const PersistentItem& i) { return !i.destroyed; }),
				      "the last unit left no live flask");
			}
			// C6: refinement through the store. +0 -> +1 spends the gold and the
			// materials; the certain failure +1 -> +2 destroys the sword and still
			// costs its gold. Each answer is the reloaded state.
			{
				const ItemOwner customer { ItemOwnerKind::Character, 7006 };
				const ItemStorageLayout pack { { customer, ItemStorageKind::Inventory, 1 }, 5, 9, 4 };
				const ItemWallet purse { customer, WalletCurrency::Gold, 1, 1000 };
				auto made = persistence::ProvisionOwner(importer, *catalog, customer, std::span(&pack, 1), std::span(&purse, 1));
				Check(made && *made, "smith customer provisioned");
				const auto session = persistence::ClaimOwnerSession(importer, *catalog, customer);
				Check(session.has_value(), "smith customer session claimed");
				auto owned = persistence::LoadItemOwner(importer, *catalog, customer, *session);
				PersistentItem blade { ItemInstanceId::Create(0x00C6000000000001ULL).value(), ItemKey::Create(1, 1).value(), customer };
				blade.placement = ItemPlacement { ItemStorageKind::Inventory, 1, 0 };
				PersistentItem ore { ItemInstanceId::Create(0x00C6000000000002ULL).value(), ItemKey::Create(6, 1).value(), customer };
				ore.count = 3; ore.placement = ItemPlacement { ItemStorageKind::Inventory, 1, 20 };
				ItemCommit grant; grant.operationId.fill(0x61); grant.catalogRevision = catalog->Revision(); grant.catalogHash = catalog->ContentHash();
				grant.kind = ItemOperationKind::Grant; grant.owners = { owned->fence }; grant.storages = { pack };
				grant.items.push_back({ std::nullopt, blade }); grant.items.push_back({ std::nullopt, ore }); grant.outcomeCode = 1; grant.result = { 1 };
				Check(persistence::CommitItems(importer, *catalog, grant).has_value(), "grant for the refine test");
				auto before = persistence::LoadItemOwner(importer, *catalog, customer, *session);
				Check(before && before->items.size() == 2, "granted sword and ore load");
				const auto first = catalog->RefinementRecipesFrom(blade.key, 0);
				Check(first.size() == 1, "one recipe from +0");
				std::array<std::uint8_t, 16> op {}; op.fill(0x62);
				auto up = MakeOwnerRefinement(*catalog, before->fence, before->storages, before->items, before->wallets, { op, blade.id, first[0]->id, 0, true });
				Check(up && up->plan.success, "refinement built");
				std::optional<runtime::CommitOutcome> got;
				const auto wait = [&] { for (int spins = 0; !got && spins < 1000; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
				Check(runtime::RequestCommit(customer, *session, up->commit, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "refine not queued");
				wait();
				const auto live = [&](ItemInstanceId id) -> const PersistentItem* {
					const auto it = std::ranges::find_if(got->state->items, [&](const PersistentItem& i) { return i.id == id && !i.destroyed; });
					return it == got->state->items.end() ? nullptr : &*it;
				};
				Check(got && got->write && got->state && live(blade.id) && live(blade.id)->refinementLevel == 1 && live(ore.id) && live(ore.id)->count == 1 &&
				      got->state->wallets.at(0).balance == 900, "+1 committed: gold and materials spent, reloaded");
				const auto second = catalog->RefinementRecipesFrom(blade.key, 1);
				Check(second.size() == 1, "one recipe from +1");
				op.fill(0x63);
				auto lost = MakeOwnerRefinement(*catalog, got->state->fence, got->state->storages, got->state->items, got->state->wallets, { op, blade.id, second[0]->id, 0, true });
				Check(lost && !lost->plan.success, "certain failure built");
				got.reset();
				Check(runtime::RequestCommit(customer, *session, lost->commit, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "failed refine not queued");
				wait();
				Check(got && got->write && got->state && !live(blade.id) && live(ore.id) && got->state->wallets.at(0).balance == 850,
				      "the failure destroyed the sword and still cost its gold");
			}
			// C7a: the store reserved a block of instance IDs at start; a grant spends
			// them, and a low pool is refilled in the background with a disjoint block.
			{
				Check(store.AvailableIds() == runtime::ItemStore::IdBlockSize, "a full ID block reserved at start");
				const auto peeked = runtime::PeekInstanceIds(2);
				Check(peeked.size() == 2 && peeked[0].Value() >= (std::uint64_t { 1 } << 32) && peeked[1].Value() == peeked[0].Value() + 1,
				      "the pool hands out reserved IDs above the legacy ID space");
				const ItemOwner receiver { ItemOwnerKind::Character, 7007 };
				const ItemStorageLayout pack { { receiver, ItemStorageKind::Inventory, 1 }, 5, 9, 4 };
				const ItemWallet purse { receiver, WalletCurrency::Gold, 1, 0 };
				auto made = persistence::ProvisionOwner(importer, *catalog, receiver, std::span(&pack, 1), std::span(&purse, 1));
				Check(made && *made, "receiver provisioned");
				const auto session = persistence::ClaimOwnerSession(importer, *catalog, receiver);
				Check(session.has_value(), "receiver session claimed");
				auto before = persistence::LoadItemOwner(importer, *catalog, receiver, *session);
				std::array<std::uint8_t, 16> op {}; op.fill(0x71);
				auto grant = MakeGrantCommit(*catalog, op, before->fence, before->storages, before->items, { ItemKey::Create(6, 1).value(), 0, 250 }, peeked);
				Check(grant && grant->items.size() == 2 && grant->items[0].after.count == 200 && grant->items[1].after.count == 50, "250 ore in two stacks");
				runtime::SpendInstanceIds(2);
				Check(store.AvailableIds() == runtime::ItemStore::IdBlockSize - 2, "the grant's IDs are spent");
				std::optional<runtime::CommitOutcome> got;
				const auto wait = [&] { for (int spins = 0; !got && spins < 1000; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
				Check(runtime::RequestCommit(receiver, *session, *grant, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "grant not queued");
				wait();
				Check(got && got->write && got->state && got->state->items.size() == 2 &&
				      std::ranges::all_of(got->state->items, [&](const PersistentItem& i) { return i.id == peeked[0] || i.id == peeked[1]; }),
				      "granted with the reserved IDs and reloaded");
				const auto last = peeked[1].Value();
				runtime::SpendInstanceIds(store.AvailableIds() - 10);
				for (int spins = 0; store.AvailableIds() <= 10 && spins < 1000; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
				Check(store.AvailableIds() == 10 + runtime::ItemStore::IdBlockSize, "a low pool is refilled with a whole block");
				const auto next = runtime::PeekInstanceIds(11);
				Check(next.size() == 11 && next[0].Value() > last && next[10].Value() >= next[9].Value() + 1, "the refill is a new block after the first");
			}
			// Queued work is finished at stop; its completion is dropped, not run late.
			bool ran = false, applied = false;
			Check(store.Submit([&ran, &applied](CAsyncSQL&, const ItemCatalog&) { ran = true; return runtime::ItemStore::Completion { [&applied] { applied = true; } }; }), "work refused");
			store.Stop();
			Check(ran && !store.Running() && store.Drain() == 0 && !applied, "stop did not finish queued work or ran a late completion");
			Check(!store.Submit([](CAsyncSQL&, const ItemCatalog&) { return runtime::ItemStore::Completion {}; }), "store accepts work after stop");
			// Lose a real write acknowledgement and the first reconciliation
			// response. No refusal may escape while the result is unknown, and
			// the retry must read the same receipt without creating another item.
			const ItemOwner picker { ItemOwnerKind::Character, 7008 };
			const ItemStorageLayout pack { { picker, ItemStorageKind::Inventory, 1 }, 5, 9, 4 };
			Check(persistence::ProvisionOwner(importer, *catalog, picker, std::span(&pack, 1), {}).has_value(), "fault-test owner provisioned");
			auto owned = persistence::LoadItemOwner(importer, *catalog, picker, 1);
			Check(owned.has_value(), "fault-test owner loads");
			GameSqlConfig proxied = player; proxied.port = std::stoi(proxyPort);
			Check(store.Start(proxied, catalog).has_value(), "store starts through the fault proxy");
			const auto ids = runtime::PeekInstanceIds(1);
			std::array<std::uint8_t, 16> op {}; op.fill(0x81);
			auto grant = MakeGrantCommit(*catalog, op, owned->fence, owned->storages, owned->items, { ItemKey::Create(6, 1).value(), 0, 5 }, ids);
			Check(grant.has_value(), "fault-test grant built");
			runtime::SpendInstanceIds(1);
			std::optional<runtime::CommitOutcome> got;
			unsigned completions = 0;
			const auto started = std::chrono::steady_clock::now();
			Check(runtime::RequestCommit(picker, 1, *grant, [&](runtime::CommitOutcome outcome) { ++completions; got = std::move(outcome); }), "fault-test grant queued");
			for (int spins = 0; !got && spins < 1500; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
			Check(got && got->write && got->write->replay && got->state && got->state->items.size() == 1 &&
			      got->state->items[0].count == 5 && got->state->fence.revision == owned->fence.revision + 1 && completions == 1,
			      "unknown write is reconciled once, with one item and one fence increment");
			Check(std::chrono::steady_clock::now() - started >= std::chrono::seconds(1), "failed receipt read must wait before retrying");
			store.Stop();
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
