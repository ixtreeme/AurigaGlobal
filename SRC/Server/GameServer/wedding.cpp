#include "stdafx.h"
#include "ecs/systems/ItemSystem.hpp"
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
#include "ecs/Registry.hpp"
#include "ecs/components/social_components.hpp"
#include "ecs/components/dirty_components.hpp"
#include <cstdarg>
#include <utility>
#include <vector>

namespace marriage
{
	using namespace std;

	EVENTINFO(wedding_map_info)
	{
		entt::entity weddingMap { entt::null };
		int iStep;

		wedding_map_info()
		: weddingMap( entt::null )
		, iStep( 0 )
		{
		}
	};

	namespace
	{
		ecs::WeddingMapState* FindState(entt::entity map)
		{
			if (map == entt::null || !g_registry.valid(map))
				return nullptr;
			return g_registry.try_get<ecs::WeddingMapState>(map);
		}

		const char* BuildCommandPlayMusic(char* szCommand, size_t nCmdLen, uint8_t bSet, const char* c_szMusicFileName)
		{
			if (nCmdLen < 1)
			{
				szCommand[0] = '\0';
				return "PlayMusic 0 CommandLengthError";
			}

			snprintf(szCommand, nCmdLen, "PlayMusic %d %s", bSet, c_szMusicFileName);
			return szCommand;
		}
	}

	EVENTFUNC(wedding_end_event)
	{
		wedding_map_info* info = dynamic_cast<wedding_map_info*>( event->info );

		if ( info == nullptr)
		{
			LOG_ERROR("wedding_end_event> <Factor> Null pointer");
			return 0;
		}

		const entt::entity map = info->weddingMap;
		auto* state = FindState(map);
		// A cancelled or replaced callback cannot act on the map's successor.
		if (!state || state->endEvent != event)
			return 0;

		if (info->iStep == 0)
		{
			++info->iStep;
			WeddingSystem::WarpAll(map);
			return PASSES_PER_SEC(15);
		}
		WeddingManager::instance().DestroyWeddingMap(map);
		return 0;
	}

	namespace WeddingSystem
	{
	uint32_t GetMapIndex(entt::entity map)
	{
		const auto* state = FindState(map);
		return state ? state->mapIndex : 0;
	}

	void SetEnded(entt::entity map)
	{
		auto* state = FindState(map);
		if (!state)
			return;

		if (state->endEvent)
		{
			LOG_ERROR("WeddingMap::SetEnded - ALREADY EndEvent(m_pEndEvent={:x})", reinterpret_cast<uintptr_t>(get_pointer(state->endEvent)));
			return;
		}

		wedding_map_info* info = AllocEventInfo<wedding_map_info>();

		info->weddingMap = map;

		state->endEvent = event_create(wedding_end_event, info, PASSES_PER_SEC(5));

		const uint32_t pid1 = state->pid1;
		const uint32_t pid2 = state->pid2;

#ifdef TEXTS_IMPROVEMENT
		Notice(map, CHAT_TYPE_NOTICE, 704, "");
#endif

		const std::vector<entt::entity> members(state->members.begin(), state->members.end());
		for (const entt::entity chEntity : members)
		{
			auto* current = FindState(map);
			if (!current || !current->members.contains(chEntity))
				continue;

			if (ecs::PlayerRuntime::GetPlayerID(chEntity) == pid1 || ecs::PlayerRuntime::GetPlayerID(chEntity) == pid2)
				continue;

			if (ecs::PointSystem::GetLevel(chEntity) < 10) // 10 �������ϴ� �����ʴ´�.
				continue;

			//ch->AutoGiveItem(27003, 5);
			ItemSystem::AutoGiveItemEcs(chEntity, 27002, 5);
		}
	}

#ifdef TEXTS_IMPROVEMENT
	struct FNotice
	{
		uint8_t m_type;
		uint32_t m_idx;
		const char * m_format;
		FNotice(uint8_t type, uint32_t idx, const char * format) : m_type(type), m_idx(idx), m_format(format) {}

		void operator() (entt::entity character) {
			ecs::ChatSystem::SendNew(character, m_type, m_idx, m_format);
		}
	};

	void Notice(entt::entity map, uint8_t type, uint32_t idx, const char * format, ...)
	{
		const auto* state = FindState(map);
		if (!state)
			return;

		char chatbuf[256];
		va_list args;
		va_start(args, format);
		vsnprintf(chatbuf, sizeof(chatbuf), format, args);
		va_end(args);

		const std::vector<entt::entity> members(state->members.begin(), state->members.end());
		FNotice f(type, idx, chatbuf);
		std::for_each(members.begin(), members.end(), f);
	}
#endif

	void WarpAll(entt::entity map)
	{
		const auto* state = FindState(map);
		if (!state)
			return;

		const std::vector<entt::entity> members(state->members.begin(), state->members.end());
		for (const entt::entity chEntity : members)
		{
			if (ecs::PlayerRuntime::IsPC(chEntity))
			{
				// ExitToSavedLocation�� WarpSet�� �θ��µ� �� �Լ�����
				// Sectree�� NULL�� �ȴ�. �� �� SectreeManager�� ���ʹ�
				// �� ĳ���͸� ã�� �� �����Ƿ� �Ʒ� DestroyAll���� ���� ó����
				ecs::MovementSystem::ExitToSavedLocation(chEntity);
			}
		}
	}

	void DestroyAll(entt::entity map)
	{
		auto* state = FindState(map);
		if (!state)
			return;

		LOG_INFO("WeddingMap::DestroyAll: m_setMember size {}", state->members.size());

		// Destroy each member out of the set first: teardown calls back into
		// SetWeddingMap, and the loop must always make progress and never
		// visit a member twice.
		for (;;)
		{
			auto* current = FindState(map);
			if (!current || current->members.empty())
				break;

			const entt::entity chEntity = *current->members.begin();
			current->members.erase(current->members.begin());

			LOG_INFO("WeddingMap::DestroyAll: {}", ecs::PlayerRuntime::GetName(chEntity).data());

			if (ecs::PlayerRuntime::GetDesc(chEntity))
				DESC_MANAGER::instance().DestroyDesc(ecs::PlayerRuntime::GetDesc(chEntity));
			else
				ecs::PlayerRuntime::DestroyCharacter(chEntity);
		}
	}

	// The members of the ceremony map. They were held as pointers, which is
	// why SocialSystem::SetWeddingMap resolved a character just to count it.
	void IncMember(entt::entity map, entt::entity character)
	{
		auto* state = FindState(map);
		if (!state || character == entt::null)
			return;

		if (!state->members.insert(character).second)
			return;

		SendLocalEvent(map, character);

		if (ecs::PointSystem::GetLevel(character) < 10)
		{
			ecs::PlayerRuntime::SetObserverMode(character, true);
		}
	}

	void DecMember(entt::entity map, entt::entity character)
	{
		auto* state = FindState(map);
		if (!state || state->members.erase(character) == 0)
			return;

		if (ecs::PointSystem::GetLevel(character) < 10)
		{
			ecs::PlayerRuntime::SetObserverMode(character, false);
		}
	}

	bool IsMember(entt::entity map, entt::entity character)
	{
		const auto* state = FindState(map);
		if (!state || state->members.empty())
			return false;

		return state->members.contains(character);
	}

	void ShoutInMap(entt::entity map, uint8_t type, const char* msg)
	{
		const auto* state = FindState(map);
		if (!state)
			return;

		const std::vector<entt::entity> members(state->members.begin(), state->members.end());
		for (const entt::entity member : members)
		{
			ecs::ChatSystem::Send(member, type, msg);
		}
	}

	void SetMusic(entt::entity map, bool bSet, const char* musicFileName)
	{
		auto* state = FindState(map);
		if (!state)
			return;
		if (state->isMusic == bSet)
			return;

		state->isMusic = bSet;
		state->musicFileName = musicFileName ? musicFileName : "";

		char szCommand[256];
		if (state->isMusic)
		{
			ShoutInMap(map, CHAT_TYPE_COMMAND, BuildCommandPlayMusic(szCommand, sizeof(szCommand), 1, state->musicFileName.c_str()));
		}
		else
		{
			ShoutInMap(map, CHAT_TYPE_COMMAND, BuildCommandPlayMusic(szCommand, sizeof(szCommand), 0, "default"));
		}
	}

	void SetDark(entt::entity map, bool bSet)
	{
		auto* state = FindState(map);
		if (!state || state->isDark == bSet)
			return;

		state->isDark = bSet;

		if (state->isDark)
			ShoutInMap(map, CHAT_TYPE_COMMAND, "DayMode dark");
		else
			ShoutInMap(map, CHAT_TYPE_COMMAND, "DayMode light");
	}

	void SetSnow(entt::entity map, bool bSet)
	{
		auto* state = FindState(map);
		if (!state || state->isSnow == bSet)
			return;

		state->isSnow = bSet;

		if (state->isSnow)
			ShoutInMap(map, CHAT_TYPE_COMMAND, "xmas_snow 1");
		else
			ShoutInMap(map, CHAT_TYPE_COMMAND, "xmas_snow 0");
	}

	bool IsPlayingMusic(entt::entity map)
	{
		const auto* state = FindState(map);
		return state && state->isMusic;
	}

	void SendLocalEvent(entt::entity map, entt::entity ch)
	{
		const auto* state = FindState(map);
		if (!state)
			return;

		char szCommand[256];

		if (state->isDark)
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "DayMode dark");
		if (state->isSnow)
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "xmas_snow 1");
		if (state->isMusic)
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, BuildCommandPlayMusic(szCommand, sizeof(szCommand), 1, state->musicFileName.c_str()));
	}

	void SetMemberMap(entt::entity character, entt::entity map)
	{
		if (character == entt::null || !g_registry.valid(character))
			return;

		if (map != entt::null && !FindState(map))
			map = entt::null;

		auto& state = g_registry.get_or_emplace<ecs::MarriageState>(character);

		if (state.weddingMap != entt::null)
			DecMember(state.weddingMap, character);

		state.weddingMap = map;

		if (state.weddingMap != entt::null)
			IncMember(state.weddingMap, character);

		g_registry.emplace_or_replace<ecs::DirtyTag>(character);
	}

	entt::entity GetMemberMap(entt::entity character)
	{
		if (character == entt::null || !g_registry.valid(character))
			return entt::null;

		const auto* state = g_registry.try_get<ecs::MarriageState>(character);
		return state ? state->weddingMap : entt::null;
	}
	} // namespace WeddingSystem

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

	entt::entity WeddingManager::Find(uint32_t dwMapIndex)
	{
		auto it = m_mapWedding.find(dwMapIndex);

		if (it == m_mapWedding.end())
			return entt::null;

		const auto* state = g_registry.valid(it->second) ? g_registry.try_get<ecs::WeddingMapState>(it->second) : nullptr;
		return state && state->mapIndex == dwMapIndex ? it->second : entt::null;
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

		const entt::entity mapEntity = g_registry.create();
		auto& state = g_registry.emplace<ecs::WeddingMapState>(mapEntity);
		state.mapIndex = dwMapIndex;
		state.pid1 = dwPID1;
		state.pid2 = dwPID2;

		// A stale entry for a retired map on this private index is replaced.
		m_mapWedding[dwMapIndex] = mapEntity;


		// LOCALE_SERVICE
		LPSECTREE_MAP pkSectreeMap = rkSecTreeMgr.GetMap(dwMapIndex);
		if (pkSectreeMap == nullptr) {
			return 0;
		}
		string st_weddingMapRegenFileName;
		st_weddingMapRegenFileName.reserve(64);
		st_weddingMapRegenFileName  = LocaleService_GetMapPath();
		st_weddingMapRegenFileName += "/metin2_map_wedding_01/npc.txt";

		if (!regen_do(st_weddingMapRegenFileName.c_str(), dwMapIndex, pkSectreeMap->m_setting.iBaseX, pkSectreeMap->m_setting.iBaseY, entt::null, true))
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

	void WeddingManager::DestroyWeddingMap(entt::entity mapEntity)
	{
		const auto* state = g_registry.valid(mapEntity) ? g_registry.try_get<ecs::WeddingMapState>(mapEntity) : nullptr;
		if (!state)
			return;

		const uint32_t dwMapIndex = state->mapIndex;
		LOG_INFO("DestroyWeddingMap(index={})", dwMapIndex);

		WeddingSystem::DestroyAll(mapEntity);
		auto it = m_mapWedding.find(dwMapIndex);
		if (it != m_mapWedding.end() && it->second == mapEntity)
			m_mapWedding.erase(it);
		SECTREE_MANAGER::instance().DestroyPrivateMap(dwMapIndex);
		if (g_registry.valid(mapEntity))
			g_registry.destroy(mapEntity);
	}

	bool WeddingManager::End(uint32_t dwMapIndex)
	{
		auto it = m_mapWedding.find(dwMapIndex);

		if (it == m_mapWedding.end())
			return false;

		WeddingSystem::SetEnded(it->second);
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

			db_clientdesc->DBPacket(HEADER_GD_WEDDING_READY, 0, &p, sizeof(TPacketWeddingReady));
		}
	}

}
