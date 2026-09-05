#include "../../stdafx.h"

#include "GayaSystem.hpp"
#include "../../char.h"
#include "../../config.h"
#include "../../locale_service.h"
#include "../../questmanager.h"
#include "ItemSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "PointSystem.hpp"
#include "../Registry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include <Core/Logging.hpp>

#include <string>
#include <vector>

namespace
{

struct GayaShopEntry
{
	int value_1 { 0 };
	int value_2 { 0 };
	int value_3 { 0 };
	int value_4 { 0 };
	int value_5 { 0 };
	int value_6 { 0 };
};

struct GayaLoadEntry
{
	uint32_t items { 0 };
	uint32_t gaya { 0 };
	uint32_t count { 0 };
	uint32_t glimmerstone { 0 };
	uint32_t gaya_expansion { 0 };
	uint32_t gaya_refresh { 0 };
	uint32_t glimmerstone_count { 0 };
	uint32_t gaya_expansion_count { 0 };
	uint32_t gaya_refresh_count { 0 };
	uint32_t grade_stone { 0 };
	uint32_t give_gaya { 0 };
	uint32_t prob_gaya { 0 };
	uint32_t cost_gaya_yang { 0 };
};

struct GayaRuntimeState
{
	std::vector<GayaShopEntry> infoItems;
	std::vector<GayaShopEntry> infoSlots;
	std::vector<GayaLoadEntry> loadItems;
	GayaLoadEntry config;
	LPEVENT updateEvent;
};

GayaRuntimeState* GetGayaRuntime(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;

	return &g_registry.get_or_emplace<GayaRuntimeState>(e);
}

void MarkDirty(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e))
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

EVENTINFO(gaya_market_event_info)
{
	uint32_t playerID { 0 };
};

EVENTFUNC(check_time_market_event)
{
	auto* info = dynamic_cast<gaya_market_event_info*>(event->info);
	if (info == nullptr)
	{
		LOG_ERROR("check_time_market_event> <Factor> Null pointer");
		return 0;
	}

	const entt::entity e = ecs::PlayerRuntime::FindByPlayerID(info->playerID);
	if (e == entt::null || !g_registry.valid(e))
		return 0;

	if (ecs::PlayerRuntime::IsNPC(e))
		return 0;
	if (GayaSystem::GetState(e, "system_gaya.gaya_time_world_4") - init_gayaTime() <= 0)
	{
		GayaSystem::SetState(e, "system_gaya.gaya_time_world_4", init_gayaTime() + (60 * 60 * 5));
		ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "GayaMarketTime %d", GayaSystem::GetState(e, "system_gaya.gaya_time_world_4") - init_gayaTime());
		GayaSystem::RefreshItemsMarket(e);
		GayaSystem::InfoMarket(e);
	}
	else
	{
		ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "GayaMarketTime %d", GayaSystem::GetState(e, "system_gaya.gaya_time_world_4") - init_gayaTime());
	}

	return PASSES_PER_SEC(2);
}

} // namespace

namespace GayaSystem {

void Load(entt::entity pc)
{
	auto* state = GetGayaRuntime(pc);
	if (!state)
		return;

	FILE* fp;
	char one_line[256];
	int value1, value2, value3;
	const char* delim = " \t\r\n";
	char *v, *token_string;
	char file_name[256 + 1];

	state->loadItems.clear();
	state->config = {};

	GayaLoadEntry gayaValues {};

	snprintf(file_name, sizeof(file_name), "%s/gaya.txt", LocaleService_GetBasePath().c_str());
	fp = fopen(file_name, "r");
	if (fp == nullptr)
	{
		LOG_ERROR("Gaya: Error, it's not possible load {} !", file_name);
		return;
	}

	while (fgets(one_line, 256, fp))
	{
		value1 = value2 = value3 = 0;

		if (one_line[0] == '#')
			continue;

		token_string = strtok(one_line, delim);
		if (nullptr == token_string)
			continue;

		if ((v = strtok(nullptr, delim)))
			str_to_number(value1, v);

		if ((v = strtok(nullptr, delim)))
			str_to_number(value2, v);

		if ((v = strtok(nullptr, delim)))
			str_to_number(value3, v);

		TOKEN("ITEM")
		{
			gayaValues.items = value1;
			gayaValues.gaya = value2;
			gayaValues.count = value3;
			state->loadItems.push_back(gayaValues);
		}
		else TOKEN("GLIMMERSTONE") { state->config.glimmerstone = value1; }
		else TOKEN("GAYA_EXPANSION") { state->config.gaya_expansion = value1; }
		else TOKEN("GAYA_REFRESH") { state->config.gaya_refresh = value1; }
		else TOKEN("GLIMMERSTONE_COUNT") { state->config.glimmerstone_count = value1; }
		else TOKEN("GRADE_STONE") { state->config.grade_stone = value1; }
		else TOKEN("GIVE_GAYA") { state->config.give_gaya = value1; }
		else TOKEN("PROB_GAYA") { state->config.prob_gaya = value1; }
		else TOKEN("COST_GAYA_YANG") { state->config.cost_gaya_yang = value1; }
		else TOKEN("GAYA_EXPANSION_COUNT") { state->config.gaya_expansion_count = value1; }
		else TOKEN("GAYA_REFRESH_COUNT") { state->config.gaya_refresh_count = value1; }
	}

	fclose(fp);
	MarkDirty(pc);
}

bool CheckItemsFull(entt::entity pc)
{
	if (pc == entt::null || !g_registry.valid(pc))
		return false;

	FILE* fp;
	char file_name[256 + 1];
	snprintf(file_name, sizeof(file_name), "%s/gaya/%s_gaya_system.txt", LocaleService_GetBasePath().c_str(), ecs::PlayerRuntime::GetName(pc).data());
	return (fp = fopen(file_name, "r")) != nullptr ? (fclose(fp), true) : false;
}

void ClearMarket(entt::entity pc)
{
	auto* state = GetGayaRuntime(pc);
	if (!state)
		return;

	state->infoItems.clear();
	state->infoSlots.clear();
	MarkDirty(pc);
}

void InfoMarket(entt::entity pc)
{
	auto* state = GetGayaRuntime(pc);
	if (!state)
		return;

	ClearMarket(pc);
	UpdateItems0(pc);
	ecs::ChatSystem::Send(pc, CHAT_TYPE_COMMAND, "GayaMarketClear");

	for (int i = 0; i < static_cast<int>(state->infoItems.size()); ++i)
	{
		ecs::ChatSystem::Send(pc, CHAT_TYPE_COMMAND, "GayaMarketItems %d %d %d", state->infoItems[i].value_1, state->infoItems[i].value_2, state->infoItems[i].value_3);
	}

	if (state->infoSlots.empty())
		return;

	ecs::ChatSystem::Send(pc, CHAT_TYPE_COMMAND, "GayaMarketSlotsDesblock %d %d %d %d %d %d",
		state->infoSlots[0].value_1, state->infoSlots[0].value_2, state->infoSlots[0].value_3,
		state->infoSlots[0].value_4, state->infoSlots[0].value_5, state->infoSlots[0].value_6);
}

bool CheckSlot(entt::entity pc, int slot)
{
	auto* state = GetGayaRuntime(pc);
	if (!state || state->infoSlots.empty())
		return false;

	if (slot == 3) { return state->infoSlots[0].value_1 != 0; }
	else if (slot == 4) { return state->infoSlots[0].value_2 != 0; }
	else if (slot == 5) { return state->infoSlots[0].value_3 != 0; }
	else if (slot == 6) { return state->infoSlots[0].value_4 != 0; }
	else if (slot == 7) { return state->infoSlots[0].value_5 != 0; }
	else if (slot == 8) { return state->infoSlots[0].value_6 != 0; }
	return false;
}

void BuyItems(entt::entity pc, int slot)
{
	auto* state = GetGayaRuntime(pc);
	if (!state || slot < 0 || slot >= static_cast<int>(state->infoItems.size()))
		return;

	const auto& entry = state->infoItems[slot];
	if (ecs::PointSystem::Get(pc, POINT_GAYA) >= entry.value_2)
	{
		ecs::PointSystem::Change(pc, POINT_GAYA, -entry.value_2);
		ItemSystem::AutoGiveItemEcs(pc, entry.value_1, entry.value_3);
		MarkDirty(pc);
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(pc, CHAT_TYPE_INFO, 524, "");
#endif
		return;
	}
}

void RefreshItemsMarket(entt::entity pc)
{
	auto* state = GetGayaRuntime(pc);
	if (!state || state->loadItems.size() < 2)
		return;

	FILE* fileID;
	char file_name[256 + 1];

	snprintf(file_name, sizeof(file_name), "%s/gaya/%s_gaya_system.txt", LocaleService_GetBasePath().c_str(), ecs::PlayerRuntime::GetName(pc).data());
	fileID = fopen(file_name, "w");

	if (nullptr == fileID)
		return;

	for (int i = 0; i < 9; ++i)
	{
		const int rnd = number(1, static_cast<int>(state->loadItems.size()) - 1);
		fprintf(fileID, "Item\t%d\t%d\t%d\n", state->loadItems[rnd].items, state->loadItems[rnd].gaya, state->loadItems[rnd].count);
	}

	if (!state->infoSlots.empty())
		fprintf(fileID, "Slots_Desblock\t%d\t%d\t%d\t%d\t%d\t%d\n",
			state->infoSlots[0].value_1, state->infoSlots[0].value_2, state->infoSlots[0].value_3,
			state->infoSlots[0].value_4, state->infoSlots[0].value_5, state->infoSlots[0].value_6);

	fclose(fileID);
	MarkDirty(pc);
}

void UpdateSlot(entt::entity pc, int slot)
{
	auto* state = GetGayaRuntime(pc);
	if (!state || state->infoSlots.empty() || state->infoItems.size() < 9)
		return;

	FILE* fileID;
	char file_name[256 + 1];
	snprintf(file_name, sizeof(file_name), "%s/gaya/%s_gaya_system.txt", LocaleService_GetBasePath().c_str(), ecs::PlayerRuntime::GetName(pc).data());
	fileID = fopen(file_name, "w");

	for (int i = 0; i < 9; ++i)
		fprintf(fileID, "Item\t%d\t%d\t%d\n", state->infoItems[i].value_1, state->infoItems[i].value_2, state->infoItems[i].value_3);

	if (slot == 3) { state->infoSlots[0].value_1 = 1; }
	else if (slot == 4) { state->infoSlots[0].value_2 = 1; }
	else if (slot == 5) { state->infoSlots[0].value_3 = 1; }
	else if (slot == 6) { state->infoSlots[0].value_4 = 1; }
	else if (slot == 7) { state->infoSlots[0].value_5 = 1; }
	else if (slot == 8) { state->infoSlots[0].value_6 = 1; }

	fprintf(fileID, "Slots_Desblock\t%d\t%d\t%d\t%d\t%d\t%d\n",
		state->infoSlots[0].value_1, state->infoSlots[0].value_2, state->infoSlots[0].value_3,
		state->infoSlots[0].value_4, state->infoSlots[0].value_5, state->infoSlots[0].value_6);
	fclose(fileID);
	InfoMarket(pc);
}

void UpdateItems0(entt::entity pc)
{
	auto* state = GetGayaRuntime(pc);
	if (!state)
		return;

	FILE* fp;
	char one_line[256];
	int value1, value2, value3, value4, value5, value6;
	const char* delim = " \t\r\n";
	char *v, *token_string;
	char file_name[256 + 1];

	GayaShopEntry marketItem {};
	GayaShopEntry marketSlots {};

	snprintf(file_name, sizeof(file_name), "%s/gaya/%s_gaya_system.txt", LocaleService_GetBasePath().c_str(), ecs::PlayerRuntime::GetName(pc).data());
	fp = fopen(file_name, "r");
	if (!fp)
		return;

	while (fgets(one_line, 256, fp))
	{
		value1 = value2 = value3 = value4 = value5 = value6 = 0;

		if (one_line[0] == '#')
			continue;

		token_string = strtok(one_line, delim);
		if (nullptr == token_string)
			continue;

		if ((v = strtok(nullptr, delim))) str_to_number(value1, v);
		if ((v = strtok(nullptr, delim))) str_to_number(value2, v);
		if ((v = strtok(nullptr, delim))) str_to_number(value3, v);
		if ((v = strtok(nullptr, delim))) str_to_number(value4, v);
		if ((v = strtok(nullptr, delim))) str_to_number(value5, v);
		if ((v = strtok(nullptr, delim))) str_to_number(value6, v);

		TOKEN("Item")
		{
			marketItem.value_1 = value1;
			marketItem.value_2 = value2;
			marketItem.value_3 = value3;
			state->infoItems.push_back(marketItem);
		}
		else TOKEN("Slots_Desblock")
		{
			marketSlots.value_1 = value1;
			marketSlots.value_2 = value2;
			marketSlots.value_3 = value3;
			marketSlots.value_4 = value4;
			marketSlots.value_5 = value5;
			marketSlots.value_6 = value6;
			state->infoSlots.push_back(marketSlots);
		}
	}

	fclose(fp);
	MarkDirty(pc);
}

void UpdateItems(entt::entity pc)
{
	auto* state = GetGayaRuntime(pc);
	if (!state || state->loadItems.size() < 2)
		return;

	FILE* fileID;
	char file_name[256 + 1];
	snprintf(file_name, sizeof(file_name), "%s/gaya/%s_gaya_system.txt", LocaleService_GetBasePath().c_str(), ecs::PlayerRuntime::GetName(pc).data());
	fileID = fopen(file_name, "a");
	if (!fileID)
		return;

	for (int i = 0; i < 9; ++i)
	{
		const int rnd = number(1, static_cast<int>(state->loadItems.size()) - 1);
		fprintf(fileID, "Item\t%d\t%d\t%d\n", state->loadItems[rnd].items, state->loadItems[rnd].gaya, state->loadItems[rnd].count);
	}

	fprintf(fileID, "Slots_Desblock\t%d\t%d\t%d\t%d\t%d\t%d\n", 0, 0, 0, 0, 0, 0);
	fclose(fileID);
	MarkDirty(pc);
}

void CraftItems(entt::entity pc, int slot)
{
	auto* state = GetGayaRuntime(pc);
	if (!state)
		return;

#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(pc, CHAT_TYPE_INFO, "char_gaya.cpp::void CHARACTER::CraftGayaItemsm");
#endif

#ifdef ENABLE_EXTRA_INVENTORY
	const entt::entity item = ItemSystem::GetItem(pc, TItemPos(EXTRA_INVENTORY, slot));
#else
	const entt::entity item = ItemSystem::GetItem(pc, TItemPos(INVENTORY, slot));
#endif
	if (!ItemSystem::IsValidItem(item)) {
		return;
	}

	const uint32_t glimmerstoneVnum = state->config.glimmerstone;
	const uint32_t glimmerstoneCount = state->config.glimmerstone_count;
	const int gradeStone = state->config.grade_stone;
	const int pointGaya = state->config.give_gaya;
	const int randomPointGaya = number(1, 100);
	const int probabilityGaya = state->config.prob_gaya;
	const int costYang = state->config.cost_gaya_yang;

	if (ItemSystem::GetItemType(item) != ITEM_METIN ||
		ItemSystem::GetItemRefineLevel(item) > gradeStone) {
		return;
	}

	if (ItemSystem::CountItem(pc, glimmerstoneVnum) < static_cast<int>(glimmerstoneCount))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(pc, CHAT_TYPE_INFO, 525, "%d#%s", glimmerstoneCount,
			ItemSystem::GetItemNameByVnum(glimmerstoneVnum));
#endif
		return;
	}

	if (ecs::PointSystem::GetGold(pc) < costYang) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(pc, CHAT_TYPE_INFO, 232, "");
#endif
		return;
	}

	if (randomPointGaya <= probabilityGaya) {
#ifdef ENABLE_RANKING
		ecs::PlayerRuntime::SetRankPoints(
			pc, 11, ecs::PlayerRuntime::GetRankPoints(pc, 11) + pointGaya);
#endif
		ecs::PointSystem::Change(pc, POINT_GAYA, pointGaya);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(pc, CHAT_TYPE_INFO, 526, "");
#endif
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(pc, CHAT_TYPE_INFO, 527, "");
	}
#endif

	ItemSystem::RemoveSpecifyItemEcs(pc, glimmerstoneVnum, glimmerstoneCount);
	ecs::PointSystem::Change(pc, POINT_GOLD, -costYang);
	ItemSystem::ConsumeItemEcs(item);
	ecs::ChatSystem::Send(pc, CHAT_TYPE_COMMAND, "GayaCheck");
	MarkDirty(pc);
}

void MarketItems(entt::entity pc, int slot)
{
	auto* state = GetGayaRuntime(pc);
	if (!state || !CheckItemsFull(pc))
		return;

	if (slot < 0 || slot > 8)
		return;

	const uint32_t expansionVnum = state->config.gaya_expansion;
	const uint32_t expansionCount = state->config.gaya_expansion_count;

	if (slot >= 3)
	{
		if (CheckSlot(pc, slot) == false)
		{
			if (ItemSystem::CountItem(pc, expansionVnum) >= static_cast<int>(expansionCount))
			{
				ItemSystem::RemoveSpecifyItemEcs(pc, expansionVnum, expansionCount);
				UpdateSlot(pc, slot);
				return;
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(pc, CHAT_TYPE_INFO, 525, "%d#%s", expansionCount,
					ItemSystem::GetItemNameByVnum(expansionVnum));
#endif
				return;
			}
		}
		else
		{
			BuyItems(pc, slot);
			return;
		}
	}
	else
	{
		BuyItems(pc, slot);
		return;
	}
}

void RefreshItems(entt::entity pc)
{
	auto* state = GetGayaRuntime(pc);
	if (!state || !CheckItemsFull(pc))
		return;

	const uint32_t refreshVnum = state->config.gaya_refresh;
	const uint32_t refreshCount = state->config.gaya_refresh_count;

	if (ItemSystem::CountItem(pc, refreshVnum) < static_cast<int>(refreshCount))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(pc, CHAT_TYPE_INFO, 525, "%d#%s", refreshCount,
			ItemSystem::GetItemNameByVnum(refreshVnum));
#endif
		return;
	}

	ItemSystem::RemoveSpecifyItemEcs(pc, refreshVnum, refreshCount);
	SetState(pc, "system_gaya.gaya_time_world_4", init_gayaTime() - (GetState(pc, "system_gaya.gaya_time_world_4") - init_gayaTime()));
	MarkDirty(pc);
}

int GetState(entt::entity pc, std::string_view state)
{
	if (pc == entt::null || !g_registry.valid(pc))
		return 0;

	quest::CQuestManager& q = quest::CQuestManager::instance();
	quest::PC* pPC = q.GetPC(ecs::PlayerRuntime::GetPlayerID(pc));

	if (!pPC)
	{
		LOG_ERROR("Nullpointer in CHARACTER::GetQuestFlag {}", ecs::PlayerRuntime::GetPlayerID(pc));
		return 0;
	}

	return pPC->GetFlag(std::string(state));
}

void SetState(entt::entity pc, std::string_view state, int value)
{
	if (pc == entt::null || !g_registry.valid(pc))
		return;

	quest::CQuestManager& q = quest::CQuestManager::instance();
	quest::PC* pPC = q.GetPC(ecs::PlayerRuntime::GetPlayerID(pc));

	if (!pPC)
	{
		LOG_ERROR("Nullpointer in CHARACTER::GetQuestFlag {}", ecs::PlayerRuntime::GetPlayerID(pc));
		return;
	}

	pPC->SetFlag(std::string(state), value);
	MarkDirty(pc);
}

void StartCheckTimeMarket(entt::entity pc)
{
	auto* state = GetGayaRuntime(pc);
	if (!state || state->updateEvent)
		return;

	auto* info = AllocEventInfo<gaya_market_event_info>();
	info->playerID = ecs::PlayerRuntime::GetPlayerID(pc);
	state->updateEvent = event_create(check_time_market_event, info, 1);
	MarkDirty(pc);
}

} // namespace GayaSystem

void CHARACTER::CraftGayaItems(int slot)
{
	GayaSystem::CraftItems(GetEntityHandle(), slot);
}

void CHARACTER::MarketGayaItems(int slot)
{
	GayaSystem::MarketItems(GetEntityHandle(), slot);
}

void CHARACTER::RefreshGayaItems()
{
	GayaSystem::RefreshItems(GetEntityHandle());
}
