#include "../../stdafx.h"

#include "GayaSystem.hpp"

#include "../../char.h"
#include "../../config.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../locale_service.h"
#include "../../questmanager.h"
#include "../AIHelpers.hpp"
#include "../EntityFactory.hpp"
#include "ItemSystem.hpp"
#include "PointSystem.hpp"
#include "../Registry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include <Core/Logging.hpp>

namespace
{

using LegacyCharHandle = decltype(std::declval<ecs::LegacyCharPtr>().ptr);

static inline LegacyCharHandle LegacyCharOf(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;

	auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(e);
	if (!legacy)
		return nullptr;

	return legacy->ptr;
}

void MarkDirty(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e))
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

EVENTFUNC(check_time_market_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );
	if (info == nullptr)
	{
		LOG_ERROR("check_time_market_event> <Factor> Null pointer");
		return 0;
	}

	auto* ch = info->ch.Get();
	if (nullptr == ch || ch->IsNPC())
		return 0;

	const entt::entity e = AIHelpers::EcsOf(ch);
	if (GayaSystem::GetState(e, "system_gaya.gaya_time_world_4") - init_gayaTime() <= 0)
	{
		GayaSystem::SetState(e, "system_gaya.gaya_time_world_4", init_gayaTime() + (60 * 60 * 5));
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "GayaMarketTime %d", GayaSystem::GetState(e, "system_gaya.gaya_time_world_4") - init_gayaTime());
		GayaSystem::RefreshItemsMarket(e);
		GayaSystem::InfoMarket(e);
	}
	else
	{
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "GayaMarketTime %d", GayaSystem::GetState(e, "system_gaya.gaya_time_world_4") - init_gayaTime());
	}

	return PASSES_PER_SEC(2);
}

} // namespace

namespace GayaSystem {

void Load(entt::entity pc)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch)
		return;

	FILE* fp;
	char one_line[256];
	int value1, value2, value3;
	const char* delim = " \t\r\n";
	char *v, *token_string;
	char file_name[256 + 1];

	ch->load_gaya_items.clear();
	ch->load_gaya_values = { 0,0,0 };

	CHARACTER::Gaya_Load_Values gaya_values = { 0,0,0 };

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
			gaya_values.items = value1;
			gaya_values.gaya = value2;
			gaya_values.count = value3;
			ch->load_gaya_items.push_back(gaya_values);
		}
		else TOKEN("GLIMMERSTONE") { ch->load_gaya_values.glimmerstone = value1; }
		else TOKEN("GAYA_EXPANSION") { ch->load_gaya_values.gaya_expansion = value1; }
		else TOKEN("GAYA_REFRESH") { ch->load_gaya_values.gaya_refresh = value1; }
		else TOKEN("GLIMMERSTONE_COUNT") { ch->load_gaya_values.glimmerstone_count = value1; }
		else TOKEN("GRADE_STONE") { ch->load_gaya_values.grade_stone = value1; }
		else TOKEN("GIVE_GAYA") { ch->load_gaya_values.give_gaya = value1; }
		else TOKEN("PROB_GAYA") { ch->load_gaya_values.prob_gaya = value1; }
		else TOKEN("COST_GAYA_YANG") { ch->load_gaya_values.cost_gaya_yang = value1; }
		else TOKEN("GAYA_EXPANSION_COUNT") { ch->load_gaya_values.gaya_expansion_count = value1; }
		else TOKEN("GAYA_REFRESH_COUNT") { ch->load_gaya_values.gaya_refresh_count = value1; }
	}

	fclose(fp);
	MarkDirty(pc);
}

bool CheckItemsFull(entt::entity pc)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch)
		return false;

	FILE* fp;
	char file_name[256 + 1];
	snprintf(file_name, sizeof(file_name), "%s/gaya/%s_gaya_system.txt", LocaleService_GetBasePath().c_str(), ch->GetName());
	return (fp = fopen(file_name, "r")) != nullptr ? (fclose(fp), true) : false;
}

void ClearMarket(entt::entity pc)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch)
		return;

	ch->info_items.clear();
	ch->info_slots.clear();
	MarkDirty(pc);
}

void InfoMarket(entt::entity pc)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch)
		return;

	ClearMarket(pc);
	UpdateItems0(pc);
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "GayaMarketClear");

	for (int i = 0; i < (int)ch->info_items.size(); ++i)
	{
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "GayaMarketItems %d %d %d", ch->info_items[i].value_1, ch->info_items[i].value_2, ch->info_items[i].value_3);
	}

	if (ch->info_slots.empty())
		return;

	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "GayaMarketSlotsDesblock %d %d %d %d %d %d",
		ch->info_slots[0].value_1, ch->info_slots[0].value_2, ch->info_slots[0].value_3,
		ch->info_slots[0].value_4, ch->info_slots[0].value_5, ch->info_slots[0].value_6);
}

bool CheckSlot(entt::entity pc, int slot)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch || ch->info_slots.empty())
		return false;

	if (slot == 3) { return ch->info_slots[0].value_1 != 0; }
	else if (slot == 4) { return ch->info_slots[0].value_2 != 0; }
	else if (slot == 5) { return ch->info_slots[0].value_3 != 0; }
	else if (slot == 6) { return ch->info_slots[0].value_4 != 0; }
	else if (slot == 7) { return ch->info_slots[0].value_5 != 0; }
	else if (slot == 8) { return ch->info_slots[0].value_6 != 0; }
	return false;
}

void BuyItems(entt::entity pc, int slot)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch || slot < 0 || slot >= (int)ch->info_items.size())
		return;

	if (ch->GetGaya() >= ch->info_items[slot].value_2)
	{
		ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_GAYA, -ch->info_items[slot].value_2);
		ch->AutoGiveItem(ch->info_items[slot].value_1, ch->info_items[slot].value_3);
		MarkDirty(pc);
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 524, "");
#endif
		return;
	}
}

void RefreshItemsMarket(entt::entity pc)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch)
		return;

	FILE* fileID;
	char file_name[256 + 1];

	snprintf(file_name, sizeof(file_name), "%s/gaya/%s_gaya_system.txt", LocaleService_GetBasePath().c_str(), ch->GetName());
	fileID = fopen(file_name, "w");

	if (nullptr == fileID)
		return;

	for (int i = 0; i < 9; ++i)
	{
		int rnd = number(1, (int)ch->load_gaya_items.size() - 1);
		fprintf(fileID, "Item\t%d\t%d\t%d\n", ch->load_gaya_items[rnd].items, ch->load_gaya_items[rnd].gaya, ch->load_gaya_items[rnd].count);
	}

	if (!ch->info_slots.empty())
		fprintf(fileID, "Slots_Desblock\t%d\t%d\t%d\t%d\t%d\t%d\n",
			ch->info_slots[0].value_1, ch->info_slots[0].value_2, ch->info_slots[0].value_3,
			ch->info_slots[0].value_4, ch->info_slots[0].value_5, ch->info_slots[0].value_6);

	fclose(fileID);
	MarkDirty(pc);
}

void UpdateSlot(entt::entity pc, int slot)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch || ch->info_slots.empty())
		return;

	FILE* fileID;
	char file_name[256 + 1];
	snprintf(file_name, sizeof(file_name), "%s/gaya/%s_gaya_system.txt", LocaleService_GetBasePath().c_str(), ch->GetName());
	fileID = fopen(file_name, "w");

	for (int i = 0; i < 9; ++i)
		fprintf(fileID, "Item\t%d\t%d\t%d\n", ch->info_items[i].value_1, ch->info_items[i].value_2, ch->info_items[i].value_3);

	if (slot == 3) { ch->info_slots[0].value_1 = 1; }
	else if (slot == 4) { ch->info_slots[0].value_2 = 1; }
	else if (slot == 5) { ch->info_slots[0].value_3 = 1; }
	else if (slot == 6) { ch->info_slots[0].value_4 = 1; }
	else if (slot == 7) { ch->info_slots[0].value_5 = 1; }
	else if (slot == 8) { ch->info_slots[0].value_6 = 1; }

	fprintf(fileID, "Slots_Desblock\t%d\t%d\t%d\t%d\t%d\t%d\n",
		ch->info_slots[0].value_1, ch->info_slots[0].value_2, ch->info_slots[0].value_3,
		ch->info_slots[0].value_4, ch->info_slots[0].value_5, ch->info_slots[0].value_6);
	fclose(fileID);
	InfoMarket(pc);
}

void UpdateItems0(entt::entity pc)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch)
		return;

	FILE* fp;
	char one_line[256];
	int value1, value2, value3, value4, value5, value6;
	const char* delim = " \t\r\n";
	char *v, *token_string;
	char file_name[256 + 1];

	CHARACTER::Gaya_Shop_Values market_gaya_values_0 = { 0,0,0 };
	CHARACTER::Gaya_Shop_Values market_gaya_values_1 = { 0,0,0,0,0,0 };

	snprintf(file_name, sizeof(file_name), "%s/gaya/%s_gaya_system.txt", LocaleService_GetBasePath().c_str(), ch->GetName());
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
			market_gaya_values_0.value_1 = value1;
			market_gaya_values_0.value_2 = value2;
			market_gaya_values_0.value_3 = value3;
			ch->info_items.push_back(market_gaya_values_0);
		}
		else TOKEN("Slots_Desblock")
		{
			market_gaya_values_1.value_1 = value1;
			market_gaya_values_1.value_2 = value2;
			market_gaya_values_1.value_3 = value3;
			market_gaya_values_1.value_4 = value4;
			market_gaya_values_1.value_5 = value5;
			market_gaya_values_1.value_6 = value6;
			ch->info_slots.push_back(market_gaya_values_1);
		}
	}

	fclose(fp);
	MarkDirty(pc);
}

void UpdateItems(entt::entity pc)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch)
		return;

	FILE* fileID;
	char file_name[256 + 1];
	snprintf(file_name, sizeof(file_name), "%s/gaya/%s_gaya_system.txt", LocaleService_GetBasePath().c_str(), ch->GetName());
	fileID = fopen(file_name, "a");
	if (!fileID)
		return;

	for (int i = 0; i < 9; ++i)
	{
		int rnd = number(1, (int)ch->load_gaya_items.size() - 1);
		fprintf(fileID, "Item\t%d\t%d\t%d\n", ch->load_gaya_items[rnd].items, ch->load_gaya_items[rnd].gaya, ch->load_gaya_items[rnd].count);
	}

	fprintf(fileID, "Slots_Desblock\t%d\t%d\t%d\t%d\t%d\t%d\n", 0, 0, 0, 0, 0, 0);
	fclose(fileID);
	MarkDirty(pc);
}

void CraftItems(entt::entity pc, int slot)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch)
		return;

#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "char_gaya.cpp::void CHARACTER::CraftGayaItemsm");
#endif

#ifdef ENABLE_EXTRA_INVENTORY
	LPITEM item = ch->GetItem(TItemPos(EXTRA_INVENTORY, slot));
#else
	LPITEM item = ch->GetItem(TItemPos(INVENTORY, slot));
#endif
	if (!item) {
		return;
	}

	int ID_Glimmerstone = ch->load_gaya_values.glimmerstone;
	int Count_Glimmerstone = ch->load_gaya_values.glimmerstone_count;
	int Grade_Stone = ch->load_gaya_values.grade_stone;
	int Point_Gaya = ch->load_gaya_values.give_gaya;
	int Random_Point_Gaya = number(1, 100);
	int Prob_Gaya = ch->load_gaya_values.prob_gaya;
	int Cost_Gaya_Yang = ch->load_gaya_values.cost_gaya_yang;

	LPITEM item_glimmerstone = ITEM_MANAGER::instance().CreateItem(ID_Glimmerstone, Count_Glimmerstone, 0, true);
	if (!item_glimmerstone)
		return;

	if (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, item)) != ITEM_METIN || item->GetRefineLevel() > Grade_Stone) {
		ItemSystem::DestroyItemEntityEcs(
			EntityFactory::CreateItemEntity(g_registry, item_glimmerstone),
			"GAYA_INVALID_GLIMMERSTONE");
		return;
	}

	if (ch->CountSpecifyItem(ID_Glimmerstone) < Count_Glimmerstone)
	{
#ifdef TEXTS_IMPROVEMENT
		const std::string glimmerstoneName = item_glimmerstone->GetName();
#endif
		ItemSystem::DestroyItemEntityEcs(
			EntityFactory::CreateItemEntity(g_registry, item_glimmerstone),
			"GAYA_MISSING_GLIMMERSTONE");
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 525, "%d#%s", Count_Glimmerstone, glimmerstoneName.c_str());
#endif
		return;
	}

	if (ch->GetGold() < Cost_Gaya_Yang) {
		ItemSystem::DestroyItemEntityEcs(
			EntityFactory::CreateItemEntity(g_registry, item_glimmerstone),
			"GAYA_MISSING_GOLD");
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 232, "");
#endif
		return;
	}

	if (Random_Point_Gaya <= Prob_Gaya) {
#ifdef ENABLE_RANKING
		ch->SetRankPoints(11, ch->GetRankPoints(11) + Point_Gaya);
#endif
		ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_GAYA, Point_Gaya);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 526, "");
#endif
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 527, "");
	}
#endif

	ch->RemoveSpecifyItem(ID_Glimmerstone, Count_Glimmerstone);
	ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_GOLD, -Cost_Gaya_Yang);
	ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item));
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "GayaCheck");
	MarkDirty(pc);
}

void MarketItems(entt::entity pc, int slot)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch || !CheckItemsFull(pc))
		return;

	if (slot > 8)
		return;

	int ID_GayaMarketExpansion = ch->load_gaya_values.gaya_expansion;

	LPITEM item_gayarexpansion = ITEM_MANAGER::instance().CreateItem(ID_GayaMarketExpansion, ch->load_gaya_values.gaya_expansion_count, 0, true);
	if (!item_gayarexpansion)
		return;

	if (slot >= 3)
	{
		if (CheckSlot(pc, slot) == false)
		{
			if (ch->CountSpecifyItem(ID_GayaMarketExpansion) >= (int)ch->load_gaya_values.gaya_expansion_count)
			{
				ch->RemoveSpecifyItem(ID_GayaMarketExpansion, ch->load_gaya_values.gaya_expansion_count);
				UpdateSlot(pc, slot);
				return;
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 525, "%d#%s", ch->load_gaya_values.gaya_expansion_count, item_gayarexpansion->GetName());
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
	auto* ch = LegacyCharOf(pc);
	if (!ch || !CheckItemsFull(pc))
		return;

	int ID_GayaMarketRefresh = ch->load_gaya_values.gaya_refresh;

	LPITEM item_gayarefresh = ITEM_MANAGER::instance().CreateItem(ID_GayaMarketRefresh, ch->load_gaya_values.gaya_refresh_count, 0, true);
	if (!item_gayarefresh)
		return;

	if (ch->CountSpecifyItem(ID_GayaMarketRefresh) < (int)ch->load_gaya_values.gaya_refresh_count)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 525, "%d#%s", ch->load_gaya_values.gaya_refresh_count, item_gayarefresh->GetName());
#endif
		return;
	}

	ch->RemoveSpecifyItem(ID_GayaMarketRefresh, ch->load_gaya_values.gaya_refresh_count);
	SetState(pc, "system_gaya.gaya_time_world_4", init_gayaTime() - (GetState(pc, "system_gaya.gaya_time_world_4") - init_gayaTime()));
	MarkDirty(pc);
}

int GetState(entt::entity pc, const std::string& state)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch)
		return 0;

	quest::CQuestManager& q = quest::CQuestManager::instance();
	quest::PC* pPC = q.GetPC(ch->GetPlayerID());

	if (!pPC)
	{
		LOG_ERROR("Nullpointer in CHARACTER::GetQuestFlag {}", ch->GetPlayerID());
		return 0;
	}

	return pPC->GetFlag(state);
}

void SetState(entt::entity pc, const std::string& state, int value)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch)
		return;

	quest::CQuestManager& q = quest::CQuestManager::instance();
	quest::PC* pPC = q.GetPC(ch->GetPlayerID());

	if (!pPC)
	{
		LOG_ERROR("Nullpointer in CHARACTER::GetQuestFlag {}", ch->GetPlayerID());
		return;
	}

	pPC->SetFlag(state, value);
	MarkDirty(pc);
}

void StartCheckTimeMarket(entt::entity pc)
{
	auto* ch = LegacyCharOf(pc);
	if (!ch || ch->GayaUpdateTime)
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();
	info->ch = ch;
	ch->GayaUpdateTime = event_create(check_time_market_event, info, 1);
	MarkDirty(pc);
}

} // namespace GayaSystem

void CHARACTER::LOAD_GAYA()
{
	GayaSystem::Load(AIHelpers::EcsOf(this));
}

bool CHARACTER::CheckItemsFull()
{
	return GayaSystem::CheckItemsFull(AIHelpers::EcsOf(this));
}

void CHARACTER::ClearGayaMarket()
{
	GayaSystem::ClearMarket(AIHelpers::EcsOf(this));
}

void CHARACTER::InfoGayaMarker()
{
	GayaSystem::InfoMarket(AIHelpers::EcsOf(this));
}

bool CHARACTER::CheckSlotGayaMarket(int slot)
{
	return GayaSystem::CheckSlot(AIHelpers::EcsOf(this), slot);
}

void CHARACTER::BuyItemsGayaMarket(int slot)
{
	GayaSystem::BuyItems(AIHelpers::EcsOf(this), slot);
}

void CHARACTER::RefreshItemsGayaMarket()
{
	GayaSystem::RefreshItemsMarket(AIHelpers::EcsOf(this));
}

void CHARACTER::UpdateSlotGayaMarket(int slot)
{
	GayaSystem::UpdateSlot(AIHelpers::EcsOf(this), slot);
}

void CHARACTER::UpdateItemsGayaMarker0()
{
	GayaSystem::UpdateItems0(AIHelpers::EcsOf(this));
}

void CHARACTER::UpdateItemsGayaMarker()
{
	GayaSystem::UpdateItems(AIHelpers::EcsOf(this));
}

void CHARACTER::CraftGayaItems(int slot)
{
	GayaSystem::CraftItems(AIHelpers::EcsOf(this), slot);
}

void CHARACTER::MarketGayaItems(int slot)
{
	GayaSystem::MarketItems(AIHelpers::EcsOf(this), slot);
}

void CHARACTER::RefreshGayaItems()
{
	GayaSystem::RefreshItems(AIHelpers::EcsOf(this));
}

int CHARACTER::GetGayaState(const std::string& state) const
{
	return GayaSystem::GetState(AIHelpers::EcsOf(this), state);
}

void CHARACTER::SetGayaState(const std::string& state, int szValue)
{
	GayaSystem::SetState(AIHelpers::EcsOf(this), state, szValue);
}

void CHARACTER::StartCheckTimeMarket()
{
	GayaSystem::StartCheckTimeMarket(AIHelpers::EcsOf(this));
}

