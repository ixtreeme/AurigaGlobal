#include "stdafx.h"
#include "../entity/char.h"
#include "TypedItemOps.hpp"
#include "TypedItemOperationSupport.hpp"
#include "TypedMoney.hpp"
#include "TypedRecovery.hpp"
#include "ItemCatalogRuntime.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "../dungeon/DungeonUiEvents.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../entity/char_manager.h"
#include <Core/Logging.hpp>
namespace items = auriga::item;
using auriga::item::runtime::detail::ErrorText;
using auriga::item::runtime::detail::NewOperationId;

std::expected<void, TypedGrantRefusal> RequestTypedItemGrant(entt::entity character, const items::ItemGrant& grant,
	std::function<void(bool written)> done)
{
	if (!g_registry.valid(character))
		return std::unexpected(TypedGrantRefusal::NotEnabled);
	auto* typed = g_registry.try_get<ecs::TypedItems>(character);
	const auto* login = g_registry.try_get<ecs::TypedItemLogin>(character);
	const auto catalog = items::runtime::CurrentCatalog();
	if (!typed || !login || !catalog)
		return std::unexpected(TypedGrantRefusal::NotEnabled);
	if (g_registry.all_of<ecs::TypedItemBusy>(character))
		return std::unexpected(TypedGrantRefusal::Busy);
	const auto* entry = catalog->Find(grant.key);
	if (!entry || !grant.count)
		return std::unexpected(TypedGrantRefusal::Invalid);
	if(catalog->CurrencyMode(grant.key)==items::CurrencyTokenMode::GroundGold){if(grant.refinementLevel || grant.count!=1 || !grant.currencyAmount)return std::unexpected(TypedGrantRefusal::Invalid);return RequestTypedGoldCredit(character,*grant.currencyAmount,{},std::move(done));}
	const std::uint32_t stack = entry->base.maxStack ? entry->base.maxStack : 1;
	const std::size_t most = (std::size_t { grant.count } + stack - 1) / stack;
	if (most > items::MaxItemCommitItems)
		return std::unexpected(TypedGrantRefusal::Invalid);
	const auto ids = items::runtime::PeekInstanceIds(most);
	const auto& state = typed->state;
	auto timedGrant=grant;if(!timedGrant.createdAtUtcMs)timedGrant.createdAtUtcMs=TypedItemEffectNow();
	auto commit = items::MakeGrantCommit(*catalog, NewOperationId(), state.fence, state.storages, state.items, timedGrant, ids);
	if (!commit)
	{
		switch (commit.error())
		{
		case items::ItemPersistenceError::InvalidPlacement: return std::unexpected(TypedGrantRefusal::NoRoom);
		case items::ItemPersistenceError::LimitExceeded:
			if (ids.size() < most) return std::unexpected(TypedGrantRefusal::NoIds);
			return std::unexpected(TypedGrantRefusal::Invalid);
		default: return std::unexpected(TypedGrantRefusal::Invalid);
		}
	}
	items::runtime::SpendInstanceIds(static_cast<std::size_t>(std::ranges::count_if(commit->items, [](const items::ItemChange& c) { return !c.before; })));
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(character);
	const uint64_t token = login->token;
	const auto dungeonRun=DungeonUi::Events::Context(character);
	g_registry.emplace_or_replace<ecs::TypedItemBusy>(character);
	const bool queued = items::runtime::RequestCommit(typed->state.fence.owner, typed->state.fence.sessionGeneration, std::move(*commit),
		[pid, token, grant, dungeonRun, code = entry->base.contentCode, done = std::move(done)](items::runtime::CommitOutcome outcome) {
			if(outcome.write)DungeonUi::Events::Loot(dungeonRun,pid,items::PackItemKey(grant.key),grant.refinementLevel,grant.count);
			const entt::entity entity = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
			const auto* current = entity != entt::null && g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItemLogin>(entity) : nullptr;
			if (!current || current->token != token)
			{
				if (done) done(outcome.write.has_value()); // persistence outlives the login
				return;
			}
			g_registry.remove<ecs::TypedItemBusy>(entity);
			if (outcome.state)
				g_registry.emplace_or_replace<ecs::TypedItems>(entity, std::move(*outcome.state));
			else
				LOG_ERROR("TypedItems: pid {} reload after a grant failed (error {})", pid, static_cast<int>(outcome.state.error()));
			if (outcome.write)
			{
				LOG_INFO("TypedItems: pid {} received {} x{} (+{})", pid, code, grant.count, grant.refinementLevel);
				ecs::ChatSystem::Send(entity, CHAT_TYPE_INFO, "Új tárgy: %s ×%u.", code.c_str(), static_cast<unsigned>(grant.count));
			}
			else
			{
				LOG_INFO("TypedItems: pid {} grant refused by the store (error {})", pid, static_cast<int>(outcome.write.error()));
				ecs::ChatSystem::Send(entity, CHAT_TYPE_INFO, "A tárgy átadása nem sikerült: %s.", ErrorText(outcome.write.error()));
			}
			SendTypedInventory(entity);
			if (done) done(outcome.write.has_value());
			PumpTypedItemGrants(entity); // drops that waited for this answer
		});
	if (!queued)
	{
		g_registry.remove<ecs::TypedItemBusy>(character);
		return std::unexpected(TypedGrantRefusal::NotEnabled);
	}
	return {};
}
