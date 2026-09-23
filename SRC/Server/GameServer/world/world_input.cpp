#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "constants.h"
#include "db.h"
#include "desc.h"
#include "desc_manager.h"
#include "log.h"
#include "packet.h"
#include "protocol.h"
#include "utils.h"
#include "fishing.h"
#include "map_location.h"
#include "sectree_manager.h"
#include "start_position.h"
#include "regen.h"
#include "arena.h"
#include "BattleArena.h"
#include "building.h"
#include "../ecs/components/dirty_components.hpp"
#include "../ecs/systems/ActivitySystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "desc_client.h"
#include "gm.h"
#include "../ecs/systems/AcceSystem.hpp"
#include "item_manager.h"

void CInputMain::Fishing(entt::entity character, const char* c_pData)
{
	TPacketCGFishing* p = (TPacketCGFishing*)c_pData;
	ecs::MovementSystem::SetRotation(character, p->dir * 5);
	ActivitySystem::Fishing(character);
	return;
}

void CInputMain::Warp(entt::entity character, const char * pcData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Warp handler ECS
// DUAL-PATH: legacy only during migration window
	ecs::MovementSystem::WarpEnd(character);
}

#ifdef ENABLE_NEW_FISHING_SYSTEM
void CInputMain::FishingNew(entt::entity character, const char* c_pData)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	TPacketFishingNew* p = (TPacketFishingNew*)c_pData;
	switch (p->subheader) {
		case FISHING_SUBHEADER_NEW_START:
			{
				ecs::MovementSystem::SetRotation(character, p->dir * 5);
				ActivitySystem::StartFishing(character, get_dword_time());
			}
			break;
		case FISHING_SUBHEADER_NEW_STOP:
			{
				ecs::MovementSystem::SetRotation(character, p->dir * 5);
				ActivitySystem::StopFishing(character);
			}
			break;
		case FISHING_SUBHEADER_NEW_CATCH:
			{
				ActivitySystem::CatchFishing(character, get_dword_time());
			}
			break;
		case FISHING_SUBHEADER_NEW_CATCH_FAILED:
			{
				ActivitySystem::CatchFishingFailed(character);
			}
			break;
		default:
			return;
	}
}
#endif

#ifdef ENABLE_MAP_TELEPORTER
void CInputMain::MapTeleporter(entt::entity character, TPacketCGMapTeleporter* pPack)
{
// migrated from CHARACTER handler
	if (ecs::PlayerRuntime::IsHack(character) || ecs::SocialSystem::HasExchange(character) || ecs::SessionSystem::IsSafeboxOpen(character) || ecs::SessionSystem::IsCubeOpen(character) || ecs::SocialSystem::GetShop(character) != entt::null || ecs::SocialSystem::GetMyShop(character) != entt::null
#ifdef ENABLE_ACCE_SYSTEM
		|| ecs::AcceSystem::IsOpen(character)
#endif
		)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 647, "");
#endif
		return;
	}

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (AttrTransfer_is_open(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 647, "");
#endif
		return;
	}
#endif


	unsigned int iMapCode = pPack->iMapCode;
	if(iMapCode <0 || iMapCode >= g_vecMapConf.size())
		return;

	TMapConfig& rConf = g_vecMapConf[iMapCode];

	if(ecs::PointSystem::GetLevel(character) < rConf.iLevel)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 771, "%d", rConf.iLevel);
#endif
		return;
	}

	if(rConf.iLevelMax != 0 && ecs::PointSystem::GetLevel(character) > rConf.iLevelMax)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 772, "%d", rConf.iLevelMax);
#endif
		return;
	}

	if(ecs::PointSystem::GetGold(character) < rConf.price)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 773, "%d", rConf.price);
#endif
		return;
	}

	for (auto itemVnum : rConf.items)
		if (ItemSystem::CountItem(character, itemVnum) == 0)
			return;

	ecs::PointSystem::Change(character, POINT_GOLD, -rConf.price);

	for(auto itemVnum : rConf.items)
		ItemSystem::RemoveSpecifyItemEcs(character, itemVnum);

	// int iMapIndex = 0;

	// iMapIndex = rConf.iMapIndex;

	// PIXEL_POSITION pos;
	// SECTREE_MANAGER::instance().GetRecallPositionByEmpire(iMapIndex, ecs::PlayerRuntime::GetEmpire(character), pos);

	// ch->WarpSet(pos.x, pos.y);


	int32_t coord_x = 0;
	int32_t coord_y = 0;

	coord_x = rConf.coord_x;
	coord_y = rConf.coord_y;

	ecs::MovementSystem::WarpSet(character, coord_x, coord_y);

}
#endif

#ifdef __INGAME_WIKI__
void CInputMain::RecvWikiPacket(entt::entity character, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate RecvWikiPacket handler ECS
// DUAL-PATH: legacy only during migration window
	if (!ecs::PlayerRuntime::GetDesc(character))
		return;

	if (!c_pData)
		return;

	InGameWiki::TCGWikiPacket * p = nullptr;
	if (!(p = (InGameWiki::TCGWikiPacket *) c_pData))
		return;

	InGameWiki::TGCWikiPacket pack;
	pack.set_data_type(!p->is_mob ? InGameWiki::LOAD_WIKI_ITEM : InGameWiki::LOAD_WIKI_MOB);
	pack.increment_data_size(uint16_t(sizeof(InGameWiki::TGCWikiPacket)));

	if (pack.is_data_type(InGameWiki::LOAD_WIKI_ITEM))
	{
		const std::vector<CommonWikiData::TWikiItemOriginInfo>& originVec = ITEM_MANAGER::Instance().GetItemOrigin(p->vnum);
		const std::vector<CSpecialItemGroup::CSpecialItemInfo> _gV = ITEM_MANAGER::instance().GetWikiChestInfo(p->vnum);
		const std::vector<CommonWikiData::TWikiRefineInfo> _rV = ITEM_MANAGER::instance().GetWikiRefineInfo(p->vnum);
		const CommonWikiData::TWikiInfoTable* _wif = ITEM_MANAGER::instance().GetItemWikiInfo(p->vnum);

		if (!_wif)
			return;

		const size_t origin_size = originVec.size();
		const size_t chest_info_count = _wif->chest_info_count;
		const size_t refine_infos_count = _wif->refine_infos_count;
		const size_t buf_data_dize = sizeof(InGameWiki::TGCItemWikiPacket) +
								(origin_size * sizeof(CommonWikiData::TWikiItemOriginInfo)) +
								(chest_info_count * sizeof(CommonWikiData::TWikiChestInfo)) +
								(refine_infos_count * sizeof(CommonWikiData::TWikiRefineInfo));

		if (chest_info_count != _gV.size()) {
			LOG_ERROR("Item Vnum : {} || ERROR TYPE -> 1", p->vnum);
			return;
		}

		if (refine_infos_count != _rV.size()) {
			LOG_ERROR("Item Vnum : {} || ERROR TYPE -> 2", p->vnum);
			return;
		}

		pack.increment_data_size(uint16_t(buf_data_dize));

		TEMP_BUFFER buf;
		buf.write(&pack, sizeof(InGameWiki::TGCWikiPacket));

		InGameWiki::TGCItemWikiPacket data_packet;
		data_packet.mutable_wiki_info(*_wif);
		data_packet.set_origin_infos_count(origin_size);
		data_packet.set_vnum(p->vnum);
		data_packet.set_ret_id(p->ret_id);
		buf.write(&data_packet, sizeof(data_packet));

		{
			if (origin_size)
				for (int idx = 0; idx < (int)origin_size; ++idx)
					buf.write(&(originVec[idx]), sizeof(CommonWikiData::TWikiItemOriginInfo));

			if (chest_info_count > 0) {
				for (int idx = 0; idx < (int)chest_info_count; ++idx) {
					CommonWikiData::TWikiChestInfo write_struct(_gV[idx].vnum, _gV[idx].count);
					buf.write(&write_struct, sizeof(CommonWikiData::TWikiChestInfo));
				}
			}

			if (refine_infos_count > 0)
				for (int idx = 0; idx < (int)refine_infos_count; ++idx)
					buf.write(&(_rV[idx]), sizeof(CommonWikiData::TWikiRefineInfo));
		}

		ecs::PlayerRuntime::GetDesc(character)->Packet(buf.read_peek(), buf.size());
	}
	else
	{
		CMobManager::TMobWikiInfoVector& mobVec = CMobManager::instance().GetMobWikiInfo(p->vnum);
		const size_t _mobVec_size = mobVec.size();

		if (!_mobVec_size) {
			if (test_server)
				LOG_INFO("Mob Vnum: {} : || LOG TYPE -> 1", p->vnum);
			return;
		}

		const size_t buf_data_dize = (sizeof(InGameWiki::TGCMobWikiPacket) + (_mobVec_size * sizeof(CommonWikiData::TWikiMobDropInfo)));
		pack.increment_data_size(uint16_t(buf_data_dize));

		TEMP_BUFFER buf;
		buf.write(&pack, sizeof(InGameWiki::TGCWikiPacket));

		InGameWiki::TGCMobWikiPacket data_packet;
		data_packet.set_drop_info_count(_mobVec_size);
		data_packet.set_vnum(p->vnum);
		data_packet.set_ret_id(p->ret_id);
		buf.write(&data_packet, sizeof(InGameWiki::TGCMobWikiPacket));

		{
			if (_mobVec_size) {
				for (int idx = 0; idx < (int)_mobVec_size; ++idx) {
					CommonWikiData::TWikiMobDropInfo write_struct(mobVec[idx].vnum, mobVec[idx].count);
					buf.write(&write_struct, sizeof(CommonWikiData::TWikiMobDropInfo));
				}
			}
		}

		ecs::PlayerRuntime::GetDesc(character)->Packet(buf.read_peek(), buf.size());
	}
}
#endif
