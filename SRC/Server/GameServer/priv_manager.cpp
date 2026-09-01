#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/Registry.hpp"
#include "constants.h"
#include "priv_manager.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "desc_client.h"
#include "guild.h"
#include "guild_manager.h"
#include "unique_item.h"
#include "utils.h"
#include "log.h"

static const char * GetEmpireName(int priv)
{
	return c_apszEmpireNames[priv];
}

static const char * GetPrivName(int priv)
{
	return c_apszPrivNames[priv];
}

CPrivManager::CPrivManager()
{
	memset(m_aakPrivEmpireData, 0, sizeof(m_aakPrivEmpireData));
}

void CPrivManager::RequestGiveGuildPriv(uint32_t guild_id, uint8_t type, int value, time_t duration_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		LOG_ERROR("PRIV_MANAGER: RequestGiveGuildPriv: wrong guild priv type({})", static_cast<int>(type));
		return;
	}

	value = MINMAX(0, value, 50);
	duration_sec = MINMAX(0, duration_sec, 60*60*24*7);

	TPacketGiveGuildPriv p;
	p.type = type;
	p.value = value;
	p.guild_id = guild_id;
	p.duration_sec = duration_sec;

	db_clientdesc->DBPacket(HEADER_GD_REQUEST_GUILD_PRIV, 0, &p, sizeof(p));
}

void CPrivManager::RequestGiveEmpirePriv(uint8_t empire, uint8_t type, int value, time_t duration_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		LOG_ERROR("PRIV_MANAGER: RequestGiveEmpirePriv: wrong empire priv type({})", static_cast<int>(type));
		return;
	}

	value = MINMAX(0, value, 200);
	duration_sec = MINMAX(0, duration_sec, 60*60*24*7);

	TPacketGiveEmpirePriv p;
	p.type = type;
	p.value = value;
	p.empire = empire;
	p.duration_sec = duration_sec;

	db_clientdesc->DBPacket(HEADER_GD_REQUEST_EMPIRE_PRIV, 0, &p, sizeof(p));
}

void CPrivManager::RequestGiveCharacterPriv(uint32_t pid, uint8_t type, int value)
{
	if (MAX_PRIV_NUM <= type)
	{
		LOG_ERROR("PRIV_MANAGER: RequestGiveCharacterPriv: wrong char priv type({})", static_cast<int>(type));
		return;
	}

	value = MINMAX(0, value, 100);

	TPacketGiveCharacterPriv p;
	p.type = type;
	p.value = value;
	p.pid = pid;

	db_clientdesc->DBPacket(HEADER_GD_REQUEST_CHARACTER_PRIV, 0, &p, sizeof(p));
}

void CPrivManager::GiveGuildPriv(uint32_t guild_id, uint8_t type, int value, uint8_t bLog, time_t end_time_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		LOG_ERROR("PRIV_MANAGER: GiveGuildPriv: wrong guild priv type({})", static_cast<int>(type));
		return;
	}

	LOG_INFO("Set Guild Priv: guild_id({}) type({}) value({}) duration_sec({})", guild_id, static_cast<int>(type), value, end_time_sec - get_global_time());

	value = MINMAX(0, value, 50);
	end_time_sec = MINMAX(0, end_time_sec, get_global_time()+60*60*24*7);

	m_aPrivGuild[type][guild_id].value = value;
	m_aPrivGuild[type][guild_id].end_time_sec = end_time_sec;

	CGuild* g = CGuildManager::instance().FindGuild(guild_id);

	if (g)
	{
#ifdef TEXTS_IMPROVEMENT
		if (value) {
			SendNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 573, "%s#%s#%d", GetPrivName(type), g->GetName(), value);
		} else {
			SendNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 574, "%s#%s", GetPrivName(type), g->GetName());
		}
#endif
		if (bLog) {
			LogManager::instance().CharLog(0, guild_id, type, value, "GUILD_PRIV", "", "");
		}
	}
}

void CPrivManager::GiveCharacterPriv(uint32_t pid, uint8_t type, int value, uint8_t bLog)
{
	if (MAX_PRIV_NUM <= type)
	{
		LOG_ERROR("PRIV_MANAGER: GiveCharacterPriv: wrong char priv type({})", static_cast<int>(type));
		return;
	}

	LOG_INFO("Set Character Priv {} {} {}", pid, static_cast<int>(type), value);

	value = MINMAX(0, value, 100);

	m_aPrivChar[type][pid] = value;

	if (bLog)
		LogManager::instance().CharLog(pid, 0, type, value, "CHARACTER_PRIV", "", "");
}

void CPrivManager::GiveEmpirePriv(uint8_t empire, uint8_t type, int value, uint8_t bLog, time_t end_time_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		LOG_ERROR("PRIV_MANAGER: GiveEmpirePriv: wrong empire priv type({})", static_cast<int>(type));
		return;
	}

	LOG_INFO("Set Empire Priv: empire({}) type({}) value({}) duration_sec({})", static_cast<int>(empire), static_cast<int>(type), value, end_time_sec-get_global_time());

	value = MINMAX(0, value, 200);
	end_time_sec = MINMAX(0, end_time_sec, get_global_time()+60*60*24*7);

	SPrivEmpireData& rkPrivEmpireData=m_aakPrivEmpireData[type][empire];
	rkPrivEmpireData.m_value = value;
	rkPrivEmpireData.m_end_time_sec = end_time_sec;

#ifdef TEXTS_IMPROVEMENT
	if (value) {
		SendNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 575, "%s#%s#%d", GetPrivName(type), GetEmpireName(empire), value);
	} else {
		SendNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 576, "%s#%s", GetPrivName(type), GetEmpireName(empire));
	}
#endif
	if (bLog) {
		LogManager::instance().CharLog(0, empire, type, value, "EMPIRE_PRIV", "", "");
	}
}

void CPrivManager::RemoveGuildPriv(uint32_t guild_id, uint8_t type)
{
	if (MAX_PRIV_NUM <= type)
	{
		LOG_ERROR("PRIV_MANAGER: RemoveGuildPriv: wrong guild priv type({})", static_cast<int>(type));
		return;
	}

	m_aPrivGuild[type][guild_id].value = 0;
	m_aPrivGuild[type][guild_id].end_time_sec = 0;
}

void CPrivManager::RemoveEmpirePriv(uint8_t empire, uint8_t type)
{
	if (MAX_PRIV_NUM <= type)
	{
		LOG_ERROR("PRIV_MANAGER: RemoveEmpirePriv: wrong empire priv type({})", static_cast<int>(type));
		return;
	}

	SPrivEmpireData& rkPrivEmpireData=m_aakPrivEmpireData[type][empire];
	rkPrivEmpireData.m_value = 0;
	rkPrivEmpireData.m_end_time_sec = 0;
}

void CPrivManager::RemoveCharacterPriv(uint32_t pid, uint8_t type)
{
	if (MAX_PRIV_NUM <= type)
	{
		LOG_ERROR("PRIV_MANAGER: RemoveCharacterPriv: wrong char priv type({})", static_cast<int>(type));
		return;
	}

	auto it = m_aPrivChar[type].find(pid);

	if (it != m_aPrivChar[type].end())
		m_aPrivChar[type].erase(it);
}

int CPrivManager::GetPriv(LPCHARACTER ch, uint8_t type)
{
	return GetPriv(ch ? ch->GetEntityHandle() : entt::null, type);
}

int CPrivManager::GetPriv(entt::entity character, uint8_t type)
{
	if (character == entt::null || !g_registry.valid(character))
		return 0;
	// ĳ������ ���� ��ġ�� -��� ������ -�� ����ǰ�
	int val_ch = GetPrivByCharacter(ecs::PlayerRuntime::GetPlayerID(character), type);

	if (val_ch < 0 && !ItemSystem::IsEquipUniqueItem(character, UNIQUE_ITEM_NO_BAD_LUCK_EFFECT))
		return val_ch;
	else
	{
		int val;

		// ����, ����, ���, ��ü �� ū ���� ���Ѵ�.
		val = MAX(val_ch, GetPrivByEmpire(0, type));
		val = MAX(val, GetPrivByEmpire(ecs::PlayerRuntime::GetEmpire(character), type));

		if (CGuild* guild = ecs::SocialSystem::GetGuild(character))
			val = MAX(val, GetPrivByGuild(guild->GetID(), type));

		return val;
	}
}

int CPrivManager::GetPrivByEmpire(uint8_t bEmpire, uint8_t type)
{
	SPrivEmpireData* pkPrivEmpireData = GetPrivByEmpireEx(bEmpire, type);

	if (pkPrivEmpireData)
		return pkPrivEmpireData->m_value;

	return 0;
}

CPrivManager::SPrivEmpireData* CPrivManager::GetPrivByEmpireEx(uint8_t bEmpire, uint8_t type)
{
	if (type >= MAX_PRIV_NUM)
		return nullptr;

	if (bEmpire >= EMPIRE_MAX_NUM)
		return nullptr;

	return &m_aakPrivEmpireData[type][bEmpire];
}

int CPrivManager::GetPrivByGuild(uint32_t guild_id, uint8_t type)
{
	if (type >= MAX_PRIV_NUM)
		return 0;

	auto itFind = m_aPrivGuild[ type ].find( guild_id );

	if ( itFind == m_aPrivGuild[ type ].end() )
		return 0;

	return itFind->second.value;
}

const CPrivManager::SPrivGuildData* CPrivManager::GetPrivByGuildEx( uint32_t dwGuildID, uint8_t byType ) const
{
	if ( byType >= MAX_PRIV_NUM )
		return nullptr;

	auto itFind = m_aPrivGuild[ byType ].find( dwGuildID );

	if ( itFind == m_aPrivGuild[ byType ].end() )
		return nullptr;

	return &itFind->second;
}

int CPrivManager::GetPrivByCharacter(uint32_t pid, uint8_t type)
{
	if (type >= MAX_PRIV_NUM)
		return 0;

	auto it = m_aPrivChar[type].find(pid);

	if (it != m_aPrivChar[type].end())
		return it->second;

	return 0;
}


