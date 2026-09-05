
#define _cube_cpp_

#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
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
#include <limits>

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
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	LPCHARACTER	npc;
	npc = ch->GetQuestNPC();



	if (nullptr ==npc)
	{
		if (test_server)
			dev_log(LOG_DEB0, "cube_npc is NULL");
		return;
	}

	uint32_t npcVNUM = ecs::PlayerRuntime::GetRaceNum(((npc) ? (npc)->GetEntityHandle() : entt::null));

	if ( FN_check_valid_npc(npcVNUM) == false )
	{
		if ( test_server == true )
		{
			dev_log(LOG_DEB0, "cube not valid NPC");
		}
		return;
	}


	if (ecs::SocialSystem::GetExchange(chEntity) || ch->GetMyShop() || ch->GetShopOwner() || ch->IsOpenSafebox() || ch->IsCubeOpen()
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
		ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 815, "");
#endif
		return;
	}

	int32_t distance = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(chEntity) - ecs::PlayerRuntime::GetX(((npc) ? (npc)->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY(chEntity) - ecs::PlayerRuntime::GetY(((npc) ? (npc)->GetEntityHandle() : entt::null)));

	if (distance >= CUBE_MAX_DISTANCE)
	{
		LOG_INFO("CUBE: TOO_FAR: {} distance {}", ecs::PlayerRuntime::GetName(chEntity).data(), distance);
		return;
	}


	SendDateCubeRenewalPackets(ch,CUBE_RENEWAL_SUB_HEADER_CLEAR_DATES_RECEIVE);
	SendDateCubeRenewalPackets(ch,CUBE_RENEWAL_SUB_HEADER_DATES_RECEIVE,npcVNUM);
	SendDateCubeRenewalPackets(ch,CUBE_RENEWAL_SUB_HEADER_DATES_LOADING);
	SendDateCubeRenewalPackets(ch,CUBE_RENEWAL_SUB_HEADER_OPEN_RECEIVE);

	ch->SetCubeNpc((npc ? npc->GetEntityHandle() : entt::null));
}

void Cube_close(LPCHARACTER ch)
{
	ch->SetCubeNpc(entt::null);
}

void Cube_Make(LPCHARACTER ch, int index, int count_item, int index_item_improve)
{
	if (!ch || !ch->IsCubeOpen() || count_item <= 0 ||
		count_item > static_cast<int>(g_bItemCountLimit))
	{
		return;
	}

	LPCHARACTER npc = ch->GetQuestNPC();
	if (!npc)
		return;

	const entt::entity ownerEntity = ((ch) ? (ch)->GetEntityHandle() : entt::null);
	if (ownerEntity == entt::null || !g_registry.valid(ownerEntity))
		return;

	const auto resultIt = cube_info_map.find(ecs::PlayerRuntime::GetRaceNum(((npc) ? (npc)->GetEntityHandle() : entt::null)));
	if (resultIt == cube_info_map.end() || index < 0 ||
		static_cast<size_t>(index) >= resultIt->second.size())
	{
		return;
	}

	const SCubeMaterialInfo& materialInfo = resultIt->second[static_cast<size_t>(index)];
	if (materialInfo.reward.vnum == 0 || materialInfo.reward.count <= 0 ||
		materialInfo.percent < 0 || materialInfo.percent > 100)
	{
		return;
	}

	const uint64_t maximumRewardCount =
		static_cast<uint64_t>(materialInfo.reward.count) * static_cast<uint64_t>(count_item);
	if (maximumRewardCount == 0 ||
		maximumRewardCount > static_cast<uint64_t>(g_bItemCountLimit))
	{
		return;
	}

	bool materialCheck = true;
	bool itemFrozen = false;
	for (const auto& material : materialInfo.material)
	{
		if (material.vnum == 0 || material.count <= 0)
			return;

		const int64_t required64 =
			static_cast<int64_t>(material.count) * static_cast<int64_t>(count_item);
		if (required64 <= 0 || required64 > std::numeric_limits<int>::max())
			return;

		const int required = static_cast<int>(required64);
		if (ItemSystem::CountItemRenewal(ownerEntity, material.vnum) < required)
			itemFrozen = true;
		if (ItemSystem::CountItem(ownerEntity, material.vnum) < required)
			materialCheck = false;
	}

	if (materialInfo.gold < 0 ||
		(materialInfo.gold != 0 &&
		 materialInfo.gold > std::numeric_limits<int64_t>::max() / count_item))
	{
		return;
	}

	const int64_t requiredGold = materialInfo.gold * static_cast<int64_t>(count_item);
	if (ecs::PointSystem::GetGold(ownerEntity) < requiredGold)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 232, "");
#endif
		return;
	}

#ifdef ENABLE_GAYA_SYSTEM
	const uint64_t requiredGaya =
		static_cast<uint64_t>(materialInfo.gaya) * static_cast<uint64_t>(count_item);
	if (requiredGaya > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) ||
		static_cast<uint64_t>(ch->GetGaya()) < requiredGaya)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 524, "");
#endif
		return;
	}
#endif

	if (itemFrozen && materialCheck)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 816, "");
#endif
		return;
	}

	if (!materialCheck)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 819, "");
#endif
		return;
	}

	constexpr uint32_t kCubeImproveItemVnum = 79605;
	constexpr uint32_t kMaximumImproveItemCount = 40;
	entt::entity improveItem = entt::null;
	uint32_t improveAmount = 0;

	if (index_item_improve != -1)
	{
		if (materialInfo.percent >= 100 || index_item_improve < 0 ||
			index_item_improve >= INVENTORY_MAX_NUM)
		{
			return;
		}

		improveItem = ItemSystem::GetInventoryItem(
			ownerEntity, static_cast<uint16_t>(index_item_improve));
		if (!ItemSystem::IsValidItem(improveItem) ||
			ItemSystem::GetItemVnum(improveItem) != kCubeImproveItemVnum)
		{
			return;
		}

		const uint32_t availableImproveCount = ItemSystem::GetItemCount(improveItem);
		if (availableImproveCount == 0 || availableImproveCount > kMaximumImproveItemCount)
			return;

		improveAmount = std::min<uint32_t>(
			availableImproveCount, static_cast<uint32_t>(100 - materialInfo.percent));
	}

	int totalItemsGive = 0;
	const int effectivePercent = materialInfo.percent + static_cast<int>(improveAmount);
	for (int attempt = 0; attempt < count_item; ++attempt)
	{
		if (number(1, 100) <= effectivePercent)
			++totalItemsGive;
	}

	const uint32_t rewardCount = totalItemsGive > 0
		? static_cast<uint32_t>(
			static_cast<uint64_t>(materialInfo.reward.count) *
			static_cast<uint64_t>(totalItemsGive))
		: static_cast<uint32_t>(materialInfo.reward.count);

	entt::entity rewardItem =
		ITEM_MANAGER::instance().CreateItem(materialInfo.reward.vnum, rewardCount);
	if (!ItemSystem::IsValidItem(rewardItem))
		return;

	if (ItemSystem::GetEmptyInventoryPositionEcs(ownerEntity, rewardItem) < 0)
	{
		ItemSystem::DestroyItemEntityEcs(rewardItem, "CUBE_RENEWAL_NO_SPACE");
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 366, "");
#endif
		return;
	}

#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD
	uint32_t copyAttr[ITEM_ATTRIBUTE_MAX_NUM][2] {};
	uint32_t copySocket[ITEM_SOCKET_MAX_NUM] {};
	bool itemCopyBonus = false;

	if (materialInfo.allowCopy != 0)
	{
		const entt::entity copySource =
			ItemSystem::FindSpecifyItem(ownerEntity, materialInfo.allowCopy, false);
		if (ItemSystem::IsValidItem(copySource) &&
			(ItemSystem::GetItemType(copySource) == ITEM_WEAPON ||
			 ItemSystem::GetItemType(copySource) == ITEM_ARMOR) &&
			ItemSystem::GetItemSubType(copySource) == ItemSystem::GetItemSubType(rewardItem))
		{
			for (int attribute = 0; attribute < ITEM_ATTRIBUTE_MAX_NUM; ++attribute)
			{
				copyAttr[attribute][0] =
					static_cast<uint32_t>(ItemSystem::GetItemAttributeType(copySource, attribute));
				copyAttr[attribute][1] =
					static_cast<uint32_t>(ItemSystem::GetItemAttributeValue(copySource, attribute));
			}
			for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
			{
				copySocket[socket] =
					static_cast<uint32_t>(ItemSystem::GetItemSocket(copySource, socket));
			}
			itemCopyBonus = true;
		}
	}
#else
	constexpr bool itemCopyBonus = false;
#endif

	if (improveAmount != 0 &&
		!ItemSystem::ConsumeItemEcs(improveItem, improveAmount))
	{
		ItemSystem::DestroyItemEntityEcs(rewardItem, "CUBE_RENEWAL_IMPROVE_FAILED");
		return;
	}

	for (const auto& material : materialInfo.material)
	{
		const uint32_t required = static_cast<uint32_t>(
			static_cast<uint64_t>(material.count) * static_cast<uint64_t>(count_item));
		if (!ItemSystem::RemoveSpecifyItemEcs(ownerEntity, material.vnum, required, true))
		{
			ItemSystem::DestroyItemEntityEcs(rewardItem, "CUBE_RENEWAL_MATERIAL_FAILED");
			return;
		}
	}

	if (requiredGold != 0)
		ecs::PointSystem::Change(ownerEntity, POINT_GOLD, -requiredGold, false);

#ifdef ENABLE_GAYA_SYSTEM
	if (requiredGaya != 0)
	{
		ecs::PointSystem::Change(
			ownerEntity, POINT_GAYA, -static_cast<int64_t>(requiredGaya), false);
	}
#endif

	if (totalItemsGive <= 0)
	{
		ItemSystem::DestroyItemEntityEcs(rewardItem, "CUBE_RENEWAL_FAILED_ROLL");
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 817, "");
#endif
		return;
	}

#ifdef ENABLE_BATTLE_PASS
	const uint8_t battlePassId = ch->GetBattlePassId();
	if (battlePassId)
	{
		uint32_t missionItemVnum = 0;
		uint32_t missionCount = 0;
		if (CBattlePass::instance().BattlePassMissionGetInfo(
				battlePassId, CRAFT_ITEM, &missionItemVnum, &missionCount) &&
			missionItemVnum == materialInfo.reward.vnum &&
			ch->GetMissionProgress(CRAFT_ITEM, battlePassId) < missionCount)
		{
			ch->UpdateMissionProgress(
				CRAFT_ITEM, battlePassId, rewardCount, missionCount);
		}
	}
#endif

#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 818, "");
#endif

#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD
	if (itemCopyBonus)
	{
		ItemSystem::ClearItemAttributesEcs(rewardItem);
		for (int attribute = 0; attribute < ITEM_ATTRIBUTE_MAX_NUM; ++attribute)
		{
			ItemSystem::SetItemForceAttributeEcs(
				rewardItem, attribute, copyAttr[attribute][0], copyAttr[attribute][1]);
		}
		for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
			ItemSystem::SetItemSocket(rewardItem, socket, copySocket[socket]);
	}
#endif

#ifdef ENABLE_BUG_FIXES
	if (!itemCopyBonus && ItemSystem::GetItemType(rewardItem) == ITEM_COSTUME)
	{
		ItemSystem::ClearItemAttributesEcs(rewardItem);
#ifdef ENABLE_STOLE_COSTUME
		if (ItemSystem::GetItemSubType(rewardItem) == COSTUME_STOLE)
		{
			uint8_t grade = static_cast<uint8_t>(ItemSystem::GetItemValue(rewardItem, 0));
			if (grade > 0)
			{
				grade = grade > 4 ? 4 : grade;
				const uint8_t randomRange = grade * 4;
				for (int attribute = 0; attribute < MAX_ATTR; ++attribute)
				{
					ItemSystem::SetItemForceAttributeEcs(
						rewardItem,
						attribute,
						stoleInfoTable[attribute][0],
						stoleInfoTable[attribute][number(randomRange - 3, randomRange)]);
				}
			}
		}
		else
		{
			ItemSystem::AlterItemToMagicItem(rewardItem);
		}
#else
		ItemSystem::AlterItemToMagicItem(rewardItem);
#endif
	}
#endif

	ItemSystem::AutoGiveItem(ownerEntity, rewardItem);
}

void SendDateCubeRenewalPackets(LPCHARACTER ch, uint8_t subheader, uint32_t npcVNUM)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;

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

			pack.date_cube_renewal.item_reward_stackable =
				ItemSystem::IsItemVnumStackable(materialInfo.reward.vnum);

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

			LPDESC d = ecs::PlayerRuntime::GetDesc(chEntity);

			if (nullptr == d)
			{
				LOG_ERROR("User SendDateCubeRenewalPackets ({})'s DESC is NULL POINT.", ecs::PlayerRuntime::GetName(chEntity).data());
				return ;
			}

			d->Packet(&pack, sizeof(pack));
		}
	}
	else{

		LPDESC d = ecs::PlayerRuntime::GetDesc(chEntity);

		if (nullptr == d)
		{
			LOG_ERROR("User SendDateCubeRenewalPackets ({})'s DESC is NULL POINT.", ecs::PlayerRuntime::GetName(chEntity).data());
			return ;
		}

		d->Packet(&pack, sizeof(pack));
	}
}
