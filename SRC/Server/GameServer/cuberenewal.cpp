
#define _cube_cpp_

#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "constants.h"
#include "utils.h"
#include "log.h"
#include "char_interface.hpp"
#include "dev_log.h"
#include <Core/Logging.hpp>
#include "locale_service.h"
#include "item.h"
#include "item_manager.h"
#include "questmanager.h"
#include <sstream>
#include "packet.h"
#include "desc_client.h"
#include "battle_pass.h"
#include "config.h"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include <string_view>

#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"
#endif
#ifdef ENABLE_STOLE_COSTUME
#include <common/stole_length.h>
#endif

static std::vector<CUBE_RENEWAL_DATA*>	s_cube_proto;

typedef std::vector<CUBE_RENEWAL_VALUE>	TCubeValueVector;

struct SCubeMaterialInfo
{
	SCubeMaterialInfo() : reward(), gold(0), percent(0), gaya(0), allowCopy(0)
	{
		bHaveComplicateMaterial = false;
	}

	CUBE_RENEWAL_VALUE			reward;							// º¸»ó?? ¹¹³?
	TCubeValueVector	material;						// ?ç·áµé?º ¹¹³?
	int64_t				gold;							// µ·?º ¾ó¸¶µå³?
	int 				percent;
	std::string		category;
	TCubeValueVector	complicateMaterial;				// º¹?â??-_- ?ç·áµé
#ifdef ENABLE_GAYA_SYSTEM
	uint32_t 				gaya;
#endif

#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD
	uint32_t 				allowCopy;
#endif

	std::string			infoText;
	bool				bHaveComplicateMaterial;		//
};

struct SItemNameAndLevel
{
	SItemNameAndLevel() { level = 0; }

	std::string		name;
	int				level;
};


typedef std::vector<SCubeMaterialInfo>								TCubeResultList;
typedef std::unordered_map<uint32_t, TCubeResultList>				TCubeMapByNPC;				// °¢°¢?? NPCº°·? ¾î¶² °? ¸¸µé ¼ö ??°í ?ç·á°¡ ¹º?ö...

TCubeMapByNPC cube_info_map;


static bool FN_check_valid_npc( uint16_t vnum )
{
	for ( std::vector<CUBE_RENEWAL_DATA*>::iterator iter = s_cube_proto.begin(); iter != s_cube_proto.end(); ++iter )
	{
		if ( std::find((*iter)->npc_vnum.begin(), (*iter)->npc_vnum.end(), vnum) != (*iter)->npc_vnum.end() )
			return true;
	}

	return false;
}


static bool FN_check_cube_data (CUBE_RENEWAL_DATA *cube_data)
{
	uint32_t	i = 0;
	uint32_t	end_index = 0;

	end_index = cube_data->npc_vnum.size();
	for (i=0; i<end_index; ++i)
	{
		if ( cube_data->npc_vnum[i] == 0 )	return false;
	}

	end_index = cube_data->item.size();
	for (i=0; i<end_index; ++i)
	{
		if ( cube_data->item[i].vnum == 0 )		return false;
		if ( cube_data->item[i].count == 0 )	return false;
	}

	end_index = cube_data->reward.size();
	for (i=0; i<end_index; ++i)
	{
		if ( cube_data->reward[i].vnum == 0 )	return false;
		if ( cube_data->reward[i].count == 0 )	return false;
	}
	return true;
}

static int FN_check_cube_item_vnum_material(const SCubeMaterialInfo& materialInfo, int index)
{
	if ((unsigned int)index <= materialInfo.material.size()){
		return materialInfo.material[index-1].vnum;
	}
	return 0;
}

static int FN_check_cube_item_count_material(const SCubeMaterialInfo& materialInfo,int index)
{
	if ((unsigned int)index <= materialInfo.material.size()){
		return materialInfo.material[index-1].count;
	}

	return 0;
}

CUBE_RENEWAL_DATA::CUBE_RENEWAL_DATA()
{
	this->gold = 0;
	this->category = "WORLDARD";
#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD
	this->allowCopy = 0;
#endif
#ifdef ENABLE_GAYA_SYSTEM
	this->gaya = 0;
#endif
}


void Cube_init()
{
	CUBE_RENEWAL_DATA * p_cube = nullptr;
	std::vector<CUBE_RENEWAL_DATA*>::iterator iter;

	char file_name[256+1];
	snprintf(file_name, sizeof(file_name), "%s/cube.txt", LocaleService_GetBasePath().c_str());

	LOG_INFO("Cube_Init {}", file_name);

	for (iter = s_cube_proto.begin(); iter!=s_cube_proto.end(); iter++)
	{
		p_cube = *iter;
		M2_DELETE(p_cube);
	}

	s_cube_proto.clear();

	if (false == Cube_load(file_name))
		LOG_ERROR("Cube_Init failed");
}

bool Cube_load (const char *file)
{
	FILE	*fp;


	const char	*value_string;

	char	one_line[256];
	int		value1, value2;
	const char	*delim = " \t\r\n";
	char	*v, *token_string;
	//char *v1;
	CUBE_RENEWAL_DATA	*cube_data = nullptr;
	CUBE_RENEWAL_VALUE	cube_value = {0,0};

	if (nullptr == file || 0 == file[0])
		return false;

	if ((fp = fopen(file, "r")) == nullptr)
		return false;

	while (fgets(one_line, 256, fp))
	{
		value1 = value2 = 0;

		if (one_line[0] == '#')
			continue;

		token_string = strtok(one_line, delim);

		if (nullptr == token_string)
			continue;

		// set value1, value2
		if ((v = strtok(nullptr, delim)))
			str_to_number(value1, v);
		    value_string = v;

		if ((v = strtok(nullptr, delim)))
			str_to_number(value2, v);

		TOKEN("section")
		{
			cube_data = M2_NEW CUBE_RENEWAL_DATA;
		}
		else TOKEN("npc")
		{
			cube_data->npc_vnum.push_back((uint16_t)value1);
		}
		else TOKEN("item")
		{
			cube_value.vnum		= value1;
			cube_value.count	= value2;

			cube_data->item.push_back(cube_value);
		}
		else TOKEN("reward")
		{
			cube_value.vnum		= value1;
			cube_value.count	= value2;

			cube_data->reward.push_back(cube_value);
		}
		else TOKEN("percent")
		{

			cube_data->percent = value1;
		}

		else TOKEN("category")
		{
			cube_data->category = value_string;
		}
#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD

		else TOKEN("allow_copy")
		{
			cube_data->allowCopy = value1;
		}

#endif

#ifdef ENABLE_GAYA_SYSTEM
		else TOKEN("gaya")
		{
			cube_data->gaya = value1;
		}
#endif
		else TOKEN("gold")
		{
			// ?¦?¶¿¡ ??¿ä?? ±?¾?
			cube_data->gold = value1;
		}
		else TOKEN("end")
		{

			// TODO : check cube data
			if (false == FN_check_cube_data(cube_data))
			{
				dev_log(LOG_DEB0, "something wrong");
				M2_DELETE(cube_data);
				continue;
			}
			s_cube_proto.push_back(cube_data);
		}
	}

	fclose(fp);
	return true;
}


SItemNameAndLevel SplitItemNameAndLevelFromName(std::string_view name)
{
	int level = 0;
	SItemNameAndLevel info;
	info.name.assign(name.data(), name.size());

	size_t pos = name.find("+");

	if (std::string_view::npos != pos)
	{
		const std::string levelStr(name.substr(pos + 1, name.size() - pos - 1));
		str_to_number(level, levelStr.c_str());

		info.name.assign(name.data(), pos);
	}

	info.level = level;

	return info;
};


bool Cube_InformationInitialize()
{
	for (unsigned int i = 0; i < s_cube_proto.size(); ++i)
	{
		CUBE_RENEWAL_DATA* cubeData = s_cube_proto[i];

		const std::vector<CUBE_RENEWAL_VALUE>& rewards = cubeData->reward;

		if (1 != rewards.size())
		{
			LOG_ERROR("[CubeInfo] WARNING! Does not support multiple rewards (count: {})", rewards.size());
			continue;
		}

		const CUBE_RENEWAL_VALUE& reward = rewards.at(0);
		if (cubeData->npc_vnum.empty())
		{
			LOG_ERROR("[CubeInfo] WARNING! Cube entry without npc_vnum (reward vnum {})", reward.vnum);
			continue;
		}
		const uint16_t& npcVNUM = cubeData->npc_vnum.front();
		// bool bComplicate = false;

		TCubeMapByNPC& cubeMap = cube_info_map;
		TCubeResultList& resultList = cubeMap[npcVNUM];
		SCubeMaterialInfo materialInfo;

		materialInfo.reward = reward;
		materialInfo.gold = cubeData->gold;
		materialInfo.percent = cubeData->percent;
#ifdef ENABLE_GAYA_SYSTEM
		materialInfo.gaya = cubeData->gaya;
#endif
#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD
		materialInfo.allowCopy = cubeData->allowCopy;
#endif
		materialInfo.material = cubeData->item;
		materialInfo.category = cubeData->category;

		resultList.push_back(materialInfo);
	}

	return true;
}


void Cube_open (LPCHARACTER ch)
{
	LPCHARACTER	npc;
	npc = ch->GetQuestNPC();



	if (nullptr ==npc)
	{
		if (test_server)
			dev_log(LOG_DEB0, "cube_npc is NULL");
		return;
	}

	uint32_t npcVNUM = ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(npc));

	if ( FN_check_valid_npc(npcVNUM) == false )
	{
		if ( test_server == true )
		{
			dev_log(LOG_DEB0, "cube not valid NPC");
		}
		return;
	}


	if (ch->GetExchange() || ch->GetMyShop() || ch->GetShopOwner() || ch->IsOpenSafebox() || ch->IsCubeOpen()
#ifdef ENABLE_ACCE_SYSTEM
		 || ch->IsAcceOpen()
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
		 || ch->GetOfflineShopGuest() || ch->GetAuctionGuest()
#endif
#ifdef __ATTR_TRANSFER_SYSTEM__
		 || ch->IsAttrTransferOpen()
#endif
	)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 815, "");
#endif
		return;
	}

	int32_t distance = DISTANCE_APPROX(ch->GetX() - npc->GetX(), ch->GetY() - npc->GetY());

	if (distance >= CUBE_MAX_DISTANCE)
	{
		LOG_INFO("CUBE: TOO_FAR: {} distance {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), distance);
		return;
	}


	SendDateCubeRenewalPackets(ch,CUBE_RENEWAL_SUB_HEADER_CLEAR_DATES_RECEIVE);
	SendDateCubeRenewalPackets(ch,CUBE_RENEWAL_SUB_HEADER_DATES_RECEIVE,npcVNUM);
	SendDateCubeRenewalPackets(ch,CUBE_RENEWAL_SUB_HEADER_DATES_LOADING);
	SendDateCubeRenewalPackets(ch,CUBE_RENEWAL_SUB_HEADER_OPEN_RECEIVE);

	ch->SetCubeNpc(npc);
}

void Cube_close(LPCHARACTER ch)
{
	ch->SetCubeNpc(nullptr);
}

void Cube_Make(LPCHARACTER ch, int index, int count_item, int index_item_improve)
{
	LPCHARACTER	npc;

	npc = ch->GetQuestNPC();

	if (!ch->IsCubeOpen()) {
		return;
	}

	if (nullptr == npc)
	{
		return;
	}

	int index_value = 0;
	bool material_check = true;
	bool item_frozen = false;
	LPITEM pItem;
	int iEmptyPos;
#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD
	uint32_t copyAttr[ITEM_ATTRIBUTE_MAX_NUM][2];
	uint32_t copySocket[ITEM_SOCKET_MAX_NUM];
	bool item_copy_bonus = false;
#endif
	const TCubeResultList& resultList = cube_info_map[ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(npc))];
	for (TCubeResultList::const_iterator iter = resultList.begin(); resultList.end() != iter; ++iter)
	{
		if (index_value == index)
		{
			const SCubeMaterialInfo& materialInfo = *iter;

			for (unsigned int i = 0; i < materialInfo.material.size(); ++i)
			{
				if (ch->CountSpecifyItemRenewal(materialInfo.material[i].vnum) < (materialInfo.material[i].count*count_item))
				{
					item_frozen = true;
				}

				if (ch->CountSpecifyItem(materialInfo.material[i].vnum) < (materialInfo.material[i].count*count_item))
				{
					material_check = false;
				}

			}

			if (materialInfo.gold != 0){
				if (ch->GetGold() < (materialInfo.gold*count_item)) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 232, "");
#endif
					return;
				}
			}
#ifdef ENABLE_GAYA_SYSTEM
			if (materialInfo.gaya != 0){
				if ((int32_t)ch->GetGaya() < (int32_t)(materialInfo.gaya*count_item))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 524, "");
#endif
					return;
				}
			}
#endif

			if (item_frozen && material_check)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 816, "");
#endif
				return;
			}

			if (material_check){

				int percent_number;
				int total_items_give = 0;


				int porcent_item_improve = 0;

				if (index_item_improve != -1)
				{

					LPITEM item = ch->GetInventoryItem(index_item_improve);
					if(item != nullptr)
					{

						if(ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item)) <= 40){
							if (materialInfo.percent+ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item)) <= 100){
								porcent_item_improve = ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item));
							}

							if(materialInfo.percent < 100)
							{
								if (materialInfo.percent+ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item)) > 100){
									porcent_item_improve = 100 - materialInfo.percent;
								}
							}
						}
					}

					if(porcent_item_improve != 0)
					{
						ItemSystem::ConsumeItemEcs(
							EntityFactory::CreateItemEntity(g_registry, item),
							porcent_item_improve);
					}
				}

				for (int i = 0; i < count_item; ++i)
				{
					percent_number = number(1,100);
					if ( percent_number<=materialInfo.percent+porcent_item_improve)
					{
						total_items_give++;
					}
				}
#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD

				LPITEM item = ITEM_MANAGER::instance().CreateItem(materialInfo.reward.vnum);

				pItem = ch->FindSpecifyItem(materialInfo.allowCopy);

				if(pItem != nullptr){
					if ((ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pItem)) == ITEM_WEAPON  || ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pItem)) == ITEM_ARMOR) && ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, item)) == ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pItem)) && item_copy_bonus == false)
					{

						for (int a = 0; a < ITEM_ATTRIBUTE_MAX_NUM; a++)
						{
							copyAttr[a][0] = pItem->GetAttributeType(a);
							copyAttr[a][1] = pItem->GetAttributeValue(a);
						}

						for (int a = 0; a < ITEM_SOCKET_MAX_NUM; a++)
						{
							copySocket[a] = ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pItem), a);
						}

						item_copy_bonus = true;
					}
				}
#endif
				pItem = ITEM_MANAGER::instance().CreateItem(materialInfo.reward.vnum,(materialInfo.reward.count*count_item));
				if (pItem->IsDragonSoul())
				{
					iEmptyPos = ch->GetEmptyDragonSoulInventory(pItem);
				}

#ifdef ENABLE_EXTRA_INVENTORY
				else if (pItem->IsExtraItem())
				{
					iEmptyPos = ch->GetEmptyExtraInventory(pItem);
				}
#endif
				else{
					iEmptyPos = ch->GetEmptyInventory(pItem->GetSize());
				}

				if (iEmptyPos < 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 366, "");
#endif
					return;
				}

				for (unsigned int i = 0; i < materialInfo.material.size(); ++i)
				{
					ch->RemoveSpecifyItem(materialInfo.material[i].vnum, (materialInfo.material[i].count*count_item), true);
				}

				if (materialInfo.gold != 0){
					ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_GOLD, -materialInfo.gold*count_item, false);

				}
#ifdef ENABLE_GAYA_SYSTEM
				if (materialInfo.gaya != 0)
				{
					ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_GAYA, -static_cast<int64_t>(materialInfo.gaya*count_item), false);
				}
#endif
				if(total_items_give <= 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 817, "");
#endif
					return;
				}

				int totalCount = materialInfo.reward.count * total_items_give;
				pItem = ITEM_MANAGER::instance().CreateItem(materialInfo.reward.vnum, totalCount);
#ifdef ENABLE_BATTLE_PASS
				uint8_t bBattlePassId = ch->GetBattlePassId();
				if(bBattlePassId)
				{
					uint32_t dwItemVnum, dwCount;
					if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, CRAFT_ITEM, &dwItemVnum, &dwCount))
					{
						if(dwItemVnum == materialInfo.reward.vnum && ch->GetMissionProgress(CRAFT_ITEM, bBattlePassId) < dwCount)
							ch->UpdateMissionProgress(CRAFT_ITEM, bBattlePassId, totalCount, dwCount);
					}
				}
#endif
				//toshow ChatPacket success item
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 818, "");
#endif

#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD
				if (materialInfo.allowCopy != 0 && item_copy_bonus == true) {
					ItemSystem::ClearItemAttributesEcs(EntityFactory::CreateItemEntity(g_registry, pItem));
					for (int a = 0; a < ITEM_ATTRIBUTE_MAX_NUM; a++) {
						ItemSystem::SetItemForceAttributeEcs(EntityFactory::CreateItemEntity(g_registry, pItem), a, copyAttr[a][0], copyAttr[a][1]);
					}

					for (int b = 0; b < ITEM_SOCKET_MAX_NUM; b++) {
						ItemSystem::SetItemSocket(EntityFactory::CreateItemEntity(g_registry, pItem), b, copySocket[b]);
					}
				}
#endif

#ifdef ENABLE_BUG_FIXES
				if (!item_copy_bonus && ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pItem)) == ITEM_COSTUME)
				{
					ItemSystem::ClearItemAttributesEcs(EntityFactory::CreateItemEntity(g_registry, pItem));
#ifdef ENABLE_STOLE_COSTUME
					if (ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pItem)) == COSTUME_STOLE)
					{
						uint8_t grade = ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pItem), 0);
						if (grade > 0)
						{
							grade = grade > 4 ? 4 : grade;
							uint8_t random = (grade * 4);

							for (int i = 0; i < MAX_ATTR; i++)
							{
								ItemSystem::SetItemForceAttributeEcs(EntityFactory::CreateItemEntity(g_registry, pItem), i, stoleInfoTable[i][0], stoleInfoTable[i][number(random - 3, random)]);
							}
						}
					}
					else
					{
						ItemSystem::AlterItemToMagicItem(EntityFactory::CreateItemEntity(g_registry, pItem));
					}
#else
					ItemSystem::AlterItemToMagicItem(EntityFactory::CreateItemEntity(g_registry, pItem));
#endif
				}
#endif

				if (pItem->IsDragonSoul())
				{
					iEmptyPos = ch->GetEmptyDragonSoulInventory(pItem);
					pItem->AddToCharacter(ch, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyPos));
				}

#ifdef ENABLE_EXTRA_INVENTORY
				else if (pItem->IsExtraItem())
				{
					iEmptyPos = ch->GetEmptyExtraInventory(pItem);
					if (pItem->IsStackable() && !IS_SET(ItemSystem::GetItemAntiFlag(EntityFactory::CreateItemEntity(g_registry, pItem)), ITEM_ANTIFLAG_STACK)) {
#ifdef ENABLE_NEW_STACK_LIMIT
						int
#else
						uint8_t
#endif
						bCount = ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, pItem));
						for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i) {
							LPITEM item2 = ch->GetExtraInventoryItem(i);
							if (!item2)
								continue;

							if (ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item2)) == ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, pItem))) {
								int j = 0;
								for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
									if (ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, item2), j) != ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pItem), j))
										break;

								if (j != ITEM_SOCKET_MAX_NUM)
									continue;

#ifdef ENABLE_NEW_STACK_LIMIT
								int
#else
								uint8_t
#endif
								bCount2 = MIN(g_bItemCountLimit - ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item2)), bCount);
								bCount -= bCount2;

								ItemSystem::AddItemCountEcs(
									EntityFactory::CreateItemEntity(g_registry, item2),
									bCount2);
								if (bCount == 0) {
									ItemSystem::DestroyItemEntityEcs(
									EntityFactory::CreateItemEntity(g_registry, pItem),
									"CUBE_REFINING_CONSUME");
									pItem = nullptr;
									break;
								}
							}
						}

						if (pItem != nullptr) {
							ItemSystem::SetItemCountEcs(
							EntityFactory::CreateItemEntity(g_registry, pItem),
							bCount);
							pItem->AddToCharacter(ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
						}
					} else {
						pItem->AddToCharacter(ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
					}
				}
#endif
				else {
					iEmptyPos = ch->GetEmptyInventory(pItem->GetSize());
					if (pItem->IsStackable() && !IS_SET(ItemSystem::GetItemAntiFlag(EntityFactory::CreateItemEntity(g_registry, pItem)), ITEM_ANTIFLAG_STACK)) {
#ifdef ENABLE_NEW_STACK_LIMIT
						int
#else
						uint8_t
#endif
						bCount = ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, pItem));
						for (int i = 0; i < INVENTORY_MAX_NUM; ++i) {
							LPITEM item2 = ch->GetInventoryItem(i);
							if (!item2)
								continue;

							if (ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item2)) == ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, pItem))) {
								int j = 0;
								for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
									if (ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, item2), j) != ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pItem), j))
										break;

								if (j != ITEM_SOCKET_MAX_NUM)
									continue;

#ifdef ENABLE_NEW_STACK_LIMIT
								int
#else
								uint8_t
#endif
								bCount2 = MIN(g_bItemCountLimit - ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item2)), bCount);
								bCount -= bCount2;

								ItemSystem::AddItemCountEcs(
									EntityFactory::CreateItemEntity(g_registry, item2),
									bCount2);
								if (bCount == 0) {
									ItemSystem::DestroyItemEntityEcs(
									EntityFactory::CreateItemEntity(g_registry, pItem),
									"CUBE_REFINING_CONSUME");
									pItem = nullptr;
									break;
								}
							}
						}

						if (pItem != nullptr) {
							ItemSystem::SetItemCountEcs(
							EntityFactory::CreateItemEntity(g_registry, pItem),
							bCount);
							pItem->AddToCharacter(ch, TItemPos(INVENTORY, iEmptyPos));
						}
					} else {
						pItem->AddToCharacter(ch, TItemPos(INVENTORY, iEmptyPos));
					}
				}
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 819, "");
			}
#endif
		}

		index_value++;
	}
}


void SendDateCubeRenewalPackets(LPCHARACTER ch, uint8_t subheader, uint32_t npcVNUM)
{

	TPacketGCCubeRenewalReceive pack;
	pack.subheader = subheader;

	if(subheader == CUBE_RENEWAL_SUB_HEADER_DATES_RECEIVE)
	{
		const TCubeResultList& resultList = cube_info_map[npcVNUM];
		for (TCubeResultList::const_iterator iter = resultList.begin(); resultList.end() != iter; ++iter)
		{

			const SCubeMaterialInfo& materialInfo = *iter;

			pack.date_cube_renewal.vnum_reward = materialInfo.reward.vnum;
			pack.date_cube_renewal.count_reward = materialInfo.reward.count;

			LPITEM item = ITEM_MANAGER::instance().CreateItem(materialInfo.reward.vnum, materialInfo.reward.count);
			//tofix item not stackable
			if (item->IsStackable() && !IS_SET(ItemSystem::GetItemAntiFlag(EntityFactory::CreateItemEntity(g_registry, item)), ITEM_ANTIFLAG_STACK)){
				pack.date_cube_renewal.item_reward_stackable = true;
			}else{
				pack.date_cube_renewal.item_reward_stackable = false;
			}

			pack.date_cube_renewal.vnum_material_1 = FN_check_cube_item_vnum_material(materialInfo,1);
			pack.date_cube_renewal.count_material_1 = FN_check_cube_item_count_material(materialInfo,1);

			pack.date_cube_renewal.vnum_material_2 = FN_check_cube_item_vnum_material(materialInfo,2);
			pack.date_cube_renewal.count_material_2 = FN_check_cube_item_count_material(materialInfo,2);

			pack.date_cube_renewal.vnum_material_3 = FN_check_cube_item_vnum_material(materialInfo,3);
			pack.date_cube_renewal.count_material_3 = FN_check_cube_item_count_material(materialInfo,3);

			pack.date_cube_renewal.vnum_material_4 = FN_check_cube_item_vnum_material(materialInfo,4);
			pack.date_cube_renewal.count_material_4 = FN_check_cube_item_count_material(materialInfo,4);

			pack.date_cube_renewal.vnum_material_5 = FN_check_cube_item_vnum_material(materialInfo,5);
			pack.date_cube_renewal.count_material_5 = FN_check_cube_item_count_material(materialInfo,5);

			pack.date_cube_renewal.gold = materialInfo.gold;
			pack.date_cube_renewal.percent = materialInfo.percent;

#ifdef ENABLE_GAYA_SYSTEM
			pack.date_cube_renewal.gaya = materialInfo.gaya;
#endif

#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD
			pack.date_cube_renewal.allowCopy = materialInfo.allowCopy;
#endif

			memcpy (pack.date_cube_renewal.category, 	materialInfo.category.c_str(), 		sizeof(pack.date_cube_renewal.category));

			LPDESC d = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));

			if (nullptr == d)
			{
				LOG_ERROR("User SendDateCubeRenewalPackets ({})'s DESC is NULL POINT.", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
				return ;
			}

			d->Packet(&pack, sizeof(pack));
		}
	}
	else{

		LPDESC d = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));

		if (nullptr == d)
		{
			LOG_ERROR("User SendDateCubeRenewalPackets ({})'s DESC is NULL POINT.", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
			return ;
		}

		d->Packet(&pack, sizeof(pack));
	}
}
