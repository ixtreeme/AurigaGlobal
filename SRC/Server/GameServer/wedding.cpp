#include "stdafx.h"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "desc_client.h"
#include "desc_manager.h"
#include "char_manager.h"
#include "sectree_manager.h"
#include "config.h"
#include "char_interface.hpp"
#include "wedding.h"
#include "regen.h"
#include "locale_service.h"

namespace marriage
{
	using namespace std;

	EVENTINFO(wedding_map_info)
	{
		WeddingMap * pWeddingMap;
		int iStep;

		wedding_map_info()
		: pWeddingMap( nullptr )
		, iStep( 0 )
		{
		}
	};

	EVENTFUNC(wedding_end_event)
	{
		wedding_map_info* info = dynamic_cast<wedding_map_info*>( event->info );

		if ( info == nullptr)
		{
			LOG_ERROR("wedding_end_event> <Factor> Null pointer");
			return 0;
		}

		WeddingMap* pMap = info->pWeddingMap;

		if (info->iStep == 0)
		{
			++info->iStep;
			pMap->WarpAll();
			return PASSES_PER_SEC(15);
		}
		WeddingManager::instance().DestroyWeddingMap(pMap);
		return 0;
	}

	// Map instance
	WeddingMap::WeddingMap(uint32_t dwMapIndex, uint32_t dwPID1, uint32_t dwPID2) :
		m_dwMapIndex(dwMapIndex),
		m_pEndEvent(nullptr),
		m_isDark(false),
		m_isSnow(false),
		m_isMusic(false),
		dwPID1(dwPID1),
		dwPID2(dwPID2)
	{
	}

	WeddingMap::~WeddingMap()
	{
		event_cancel(&m_pEndEvent);
	}

	void WeddingMap::SetEnded()
	{
		if (m_pEndEvent)
		{
			LOG_ERROR("WeddingMap::SetEnded - ALREADY EndEvent(m_pEndEvent={:x})", reinterpret_cast<uintptr_t>(get_pointer(m_pEndEvent)));
			return;
		}

		wedding_map_info* info = AllocEventInfo<wedding_map_info>();

		info->pWeddingMap = this;

		m_pEndEvent = event_create(wedding_end_event, info, PASSES_PER_SEC(5));

#ifdef TEXTS_IMPROVEMENT
		Notice(CHAT_TYPE_NOTICE, 704, "");
#endif

		for (auto it = m_set_pkChr.begin(); it != m_set_pkChr.end(); ++it)
		{
			LPCHARACTER ch = *it;
			if (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)) == dwPID1 || ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)) == dwPID2)
				continue;

			if (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)) < 10) // 10 레벨이하는 주지않는다.
				continue;

			//ch->AutoGiveItem(27003, 5);
			ch->AutoGiveItem(27002, 5);
		}
	}

#ifdef TEXTS_IMPROVEMENT
	struct FNotice
	{
		uint8_t m_type;
		uint32_t m_idx;
		const char * m_format;
		FNotice(uint8_t type, uint32_t idx, const char * format) : m_type(type), m_idx(idx), m_format(format) {}

		void operator() (LPCHARACTER ch) {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), m_type, m_idx, m_format);
		}
	};
#endif

#ifdef TEXTS_IMPROVEMENT
	void WeddingMap::Notice(uint8_t type, uint32_t idx, const char * format, ...)
	{
		char chatbuf[256];
		va_list args;
		va_start(args, format);
		vsnprintf(chatbuf, sizeof(chatbuf), format, args);
		va_end(args);

		FNotice f(type, idx, chatbuf);
		std::for_each(m_set_pkChr.begin(), m_set_pkChr.end(), f);
	}
#endif

	struct FWarpEveryone
	{
		void operator() (LPCHARACTER ch)
		{
			if (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
			{
				// ExitToSavedLocation은 WarpSet을 부르는데 이 함수에서
				// Sectree가 NULL이 된다. 추 후 SectreeManager로 부터는
				// 이 캐릭터를 찾을 수 없으므로 아래 DestroyAll에서 별도 처리함
				ecs::MovementSystem::ExitToSavedLocation(AIHelpers::EcsOf(ch));
			}
		}
	};

	void WeddingMap::WarpAll()
	{
		FWarpEveryone f;
		for_each(m_set_pkChr.begin(), m_set_pkChr.end(), f);
	}

	struct FDestroyEveryone
	{
		void operator() (LPCHARACTER ch)
		{
			LOG_INFO("WeddingMap::DestroyAll: {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());

			if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
				DESC_MANAGER::instance().DestroyDesc(ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)));
			else
				M2_DESTROY_CHARACTER(ch);
		}
	};

	void WeddingMap::DestroyAll()
	{
		LOG_INFO("WeddingMap::DestroyAll: m_set_pkChr size {}", m_set_pkChr.size());

		FDestroyEveryone f;

		for (charset_t::iterator it = m_set_pkChr.begin(); it != m_set_pkChr.end(); it = m_set_pkChr.begin())
			f(*it);
	}

	void WeddingMap::IncMember(LPCHARACTER ch)
	{
		if (IsMember(ch) == true)
			return;

		//0, "WeddingMap: IncMember %s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
		m_set_pkChr.insert(ch);

		SendLocalEvent(ch);

		if (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)) < 10)
		{
			ch->SetObserverMode(true);
		}
	}

	void WeddingMap::DecMember(LPCHARACTER ch)
	{
		if (IsMember(ch) == false)
			return;

		//0, "WeddingMap: DecMember %s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
		m_set_pkChr.erase(ch);

		if (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)) < 10)
		{
			ch->SetObserverMode(false);
		}
	}

	bool WeddingMap::IsMember(LPCHARACTER ch)
	{
		if (m_set_pkChr.size() <= 0)
			return false;

		return m_set_pkChr.find(ch) != m_set_pkChr.end();
	}

	void WeddingMap::ShoutInMap(uint8_t type, const char* msg)
	{
		for (auto it = m_set_pkChr.begin(); it != m_set_pkChr.end(); ++it)
		{
			LPCHARACTER ch = *it;
			ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, msg);
		}
	}

	void WeddingMap::SetMusic(bool bSet, const char* musicFileName)
	{
		if (m_isMusic != bSet)
		{
			m_isMusic = bSet;
			m_stMusicFileName = musicFileName;

			char szCommand[256];
			if (m_isMusic)
			{
				ShoutInMap(CHAT_TYPE_COMMAND, __BuildCommandPlayMusic(szCommand, sizeof(szCommand), 1, m_stMusicFileName.c_str()));
			}
			else
			{
				ShoutInMap(CHAT_TYPE_COMMAND, __BuildCommandPlayMusic(szCommand, sizeof(szCommand), 0, "default"));
			}
		}
	}

	void WeddingMap::SetDark(bool bSet)
	{
		if (m_isDark != bSet)
		{
			m_isDark = bSet;

			if (m_isDark)
				ShoutInMap(CHAT_TYPE_COMMAND, "DayMode dark");
			else
				ShoutInMap(CHAT_TYPE_COMMAND, "DayMode light");
		}
	}

	void WeddingMap::SetSnow(bool bSet)
	{
		if (m_isSnow != bSet)
		{
			m_isSnow = bSet;

			if (m_isSnow)
				ShoutInMap(CHAT_TYPE_COMMAND, "xmas_snow 1");
			else
				ShoutInMap(CHAT_TYPE_COMMAND, "xmas_snow 0");
		}
	}

	bool WeddingMap::IsPlayingMusic()
	{
		return m_isMusic;
	}

	void WeddingMap::SendLocalEvent(LPCHARACTER ch)
	{
		char szCommand[256];

		if (m_isDark)
			ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "DayMode dark");
		if (m_isSnow)
			ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "xmas_snow 1");
		if (m_isMusic)
			ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, __BuildCommandPlayMusic(szCommand, sizeof(szCommand), 1, m_stMusicFileName.c_str()));
	}

	const char* WeddingMap::__BuildCommandPlayMusic(char* szCommand, size_t nCmdLen, uint8_t bSet, const char* c_szMusicFileName)
	{
		if (nCmdLen < 1)
		{
			szCommand[0] = '\0';
			return "PlayMusic 0 CommandLengthError";
		}

		snprintf(szCommand, nCmdLen, "PlayMusic %d %s", bSet, c_szMusicFileName);
		return szCommand;
	}
	// Manager

	WeddingManager::WeddingManager()
	{
	}

	WeddingManager::~WeddingManager()
	{
	}

	bool WeddingManager::IsWeddingMap(uint32_t dwMapIndex)
	{
		return (dwMapIndex == WEDDING_MAP_INDEX || dwMapIndex / 10000 == WEDDING_MAP_INDEX);
	}

	WeddingMap* WeddingManager::Find(uint32_t dwMapIndex)
	{
		auto it = m_mapWedding.find(dwMapIndex);

		if (it == m_mapWedding.end())
			return nullptr;

		return it->second;
	}

	uint32_t WeddingManager::__CreateWeddingMap(uint32_t dwPID1, uint32_t dwPID2)
	{
		SECTREE_MANAGER& rkSecTreeMgr = SECTREE_MANAGER::instance();

		uint32_t dwMapIndex = rkSecTreeMgr.CreatePrivateMap(WEDDING_MAP_INDEX);

		if (!dwMapIndex)
		{
			LOG_ERROR("CreateWeddingMap(pid1={}, pid2={}) / CreatePrivateMap({}) FAILED", dwPID1, dwPID2, WEDDING_MAP_INDEX);
			return 0;
		}

		m_mapWedding.insert(make_pair(dwMapIndex, M2_NEW WeddingMap(dwMapIndex, dwPID1, dwPID2)));


		// LOCALE_SERVICE
		LPSECTREE_MAP pkSectreeMap = rkSecTreeMgr.GetMap(dwMapIndex);
		if (pkSectreeMap == nullptr) {
			return 0;
		}
		string st_weddingMapRegenFileName;
		st_weddingMapRegenFileName.reserve(64);
		st_weddingMapRegenFileName  = LocaleService_GetMapPath();
		st_weddingMapRegenFileName += "/metin2_map_wedding_01/npc.txt";

		if (!regen_do(st_weddingMapRegenFileName.c_str(), dwMapIndex, pkSectreeMap->m_setting.iBaseX, pkSectreeMap->m_setting.iBaseY, nullptr, true))
		{
			LOG_ERROR("CreateWeddingMap(pid1={}, pid2={}) / regen_do(fileName={}, mapIndex={}, basePos=({}, {})) FAILED", dwPID1, dwPID2, st_weddingMapRegenFileName.c_str(), dwMapIndex, pkSectreeMap->m_setting.iBaseX, pkSectreeMap->m_setting.iBaseY);
		}
		else
		{
			LOG_INFO("CreateWeddingMap(pid1={}, pid2={}) / regen_do(fileName={}, mapIndex={}, basePos=({}, {})) ok", dwPID1, dwPID2, st_weddingMapRegenFileName.c_str(), dwMapIndex, pkSectreeMap->m_setting.iBaseX, pkSectreeMap->m_setting.iBaseY);
		}
		// END_OF_LOCALE_SERVICE

		return dwMapIndex;
	}

	void WeddingManager::DestroyWeddingMap(WeddingMap* pMap)
	{
		LOG_INFO("DestroyWeddingMap(index={})", pMap->GetMapIndex());
		pMap->DestroyAll();
		m_mapWedding.erase(pMap->GetMapIndex());
		SECTREE_MANAGER::instance().DestroyPrivateMap(pMap->GetMapIndex());
		M2_DELETE(pMap);
	}

	bool WeddingManager::End(uint32_t dwMapIndex)
	{
		auto it = m_mapWedding.find(dwMapIndex);

		if (it == m_mapWedding.end())
			return false;

		it->second->SetEnded();
		return true;
	}

	void WeddingManager::Request(uint32_t dwPID1, uint32_t dwPID2)
	{
		if (map_allow_find(WEDDING_MAP_INDEX))
		{
			uint32_t dwMapIndex = __CreateWeddingMap(dwPID1, dwPID2);

			if (!dwMapIndex)
			{
				LOG_ERROR("cannot create wedding map for {}, {}", dwPID1, dwPID2);
				return;
			}

			TPacketWeddingReady p;
			p.dwPID1 = dwPID1;
			p.dwPID2 = dwPID2;
			p.dwMapIndex = dwMapIndex;

			db_clientdesc->DBPacket(HEADER_GD_WEDDING_READY, 0, &p, sizeof(p));
		}
	}

}

