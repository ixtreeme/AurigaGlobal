#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "war_map.h"
#include "sectree_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "affect.h"
#include "item.h"
#include "config.h"
#include "desc.h"
#include "desc_manager.h"
#include "guild_manager.h"
#include "buffer_manager.h"
#include "db.h"
#include "packet.h"
#include "locale_service.h"
#include "ecs/EventDispatcher.hpp"
#include "ecs/events.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"

EVENTINFO(war_map_info)
{
	int iStep;
	CWarMap * pWarMap;

	war_map_info()
	: iStep( 0 )
	, pWarMap( nullptr )
	{
	}
};

EVENTFUNC(war_begin_event)
{
	war_map_info* info = dynamic_cast<war_map_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("war_begin_event> <Factor> Null pointer");
		return 0;
	}

	CWarMap * pMap = info->pWarMap;
	pMap->CheckWarEnd();
	g_dispatcher.trigger(ecs::EvWarBegin { static_cast<uint32_t>(pMap->GetMapIndex()) });
	return PASSES_PER_SEC(10);
}

EVENTFUNC(war_end_event)
{
	war_map_info* info = dynamic_cast<war_map_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("war_end_event> <Factor> Null pointer");
		return 0;
	}

	CWarMap * pMap = info->pWarMap;

	if (info->iStep == 0)
	{
		++info->iStep;
		pMap->ExitAll();
		return PASSES_PER_SEC(5);
	}
	else
	{
		pMap->SetEndEvent(nullptr);
		g_dispatcher.trigger(ecs::EvWarEnd { static_cast<uint32_t>(pMap->GetMapIndex()) });
		CWarMapManager::instance().DestroyWarMap(pMap);
		return 0;
	}
}

EVENTFUNC(war_timeout_event)
{
	war_map_info* info = dynamic_cast<war_map_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("war_timeout_event> <Factor> Null pointer");
		return 0;
	}

	CWarMap * pMap = info->pWarMap;
	pMap->Timeout();
	g_dispatcher.trigger(ecs::EvWarTimeout { static_cast<uint32_t>(pMap->GetMapIndex()) });
	return 0;
}

void CWarMap::STeamData::Initialize()
{
	dwID = 0;
	pkGuild = nullptr;
	iMemberCount = 0;
	iUsePotionPrice = 0;
	iScore = 0;
	pkChrFlag = nullptr;
	pkChrFlagBase = nullptr;

	set_pidJoiner.clear();
}

CWarMap::CWarMap(int32_t lMapIndex, const TGuildWarInfo & r_info, TWarMapInfo * pkWarMapInfo, uint32_t dwGuildID1, uint32_t dwGuildID2)
{
	m_kMapInfo = *pkWarMapInfo;
	m_kMapInfo.lMapIndex = lMapIndex;

	memcpy(&m_WarInfo, &r_info, sizeof(TGuildWarInfo));

	m_TeamData[0].Initialize();
	m_TeamData[0].dwID = dwGuildID1;
	m_TeamData[0].pkGuild = CGuildManager::instance().TouchGuild(dwGuildID1);

	m_TeamData[1].Initialize();
	m_TeamData[1].dwID = dwGuildID2;
	m_TeamData[1].pkGuild = CGuildManager::instance().TouchGuild(dwGuildID2);
	m_iObserverCount = 0;

	war_map_info* info = AllocEventInfo<war_map_info>();
	info->pWarMap = this;

	SetBeginEvent(event_create(war_begin_event, info, PASSES_PER_SEC(60)));
	m_pkEndEvent = nullptr;
	m_pkTimeoutEvent = nullptr;
	m_pkResetFlagEvent = nullptr;
	m_bTimeout = false;
	m_dwStartTime = get_dword_time();
	m_bEnded = false;

	if (GetType() == WAR_MAP_TYPE_FLAG)
	{
		AddFlagBase(0);
		AddFlagBase(1);
		AddFlag(0);
		AddFlag(1);
	}
}

CWarMap::~CWarMap()
{
	event_cancel(&m_pkBeginEvent);
	event_cancel(&m_pkEndEvent);
	event_cancel(&m_pkTimeoutEvent);
	event_cancel(&m_pkResetFlagEvent);

	LOG_INFO("WarMap::~WarMap : map index {}", GetMapIndex());

	auto it = m_set_pkChr.begin();

	while (it != m_set_pkChr.end())
	{
		LPCHARACTER ch = *(it++);

		if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
		{
			LOG_INFO("WarMap::~WarMap : disconnecting {}", ch->GetName());
			DESC_MANAGER::instance().DestroyDesc(ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)));
		}
	}

	m_set_pkChr.clear();
}

void CWarMap::SetBeginEvent(LPEVENT pkEv)
{
	if (m_pkBeginEvent != nullptr) {
		event_cancel(&m_pkBeginEvent);
	}
	if (pkEv != nullptr) {
		m_pkBeginEvent = pkEv;
	}
}

void CWarMap::SetEndEvent(LPEVENT pkEv)
{
	if (m_pkEndEvent != nullptr) {
		event_cancel(&m_pkEndEvent);
	}
	if (pkEv != nullptr) {
		m_pkEndEvent = pkEv;
	}
}

void CWarMap::SetTimeoutEvent(LPEVENT pkEv)
{
	if (m_pkTimeoutEvent != nullptr) {
		event_cancel(&m_pkTimeoutEvent);
	}
	if (pkEv != nullptr) {
		m_pkTimeoutEvent = pkEv;
	}
}

void CWarMap::SetResetFlagEvent(LPEVENT pkEv)
{
	if (m_pkResetFlagEvent != nullptr) {
		event_cancel(&m_pkResetFlagEvent);
	}
	if (pkEv != nullptr) {
		m_pkResetFlagEvent = pkEv;
	}
}

bool CWarMap::GetTeamIndex(uint32_t dwGuildID, uint8_t & bIdx)
{
	if (m_TeamData[0].dwID == dwGuildID)
	{
		bIdx = 0;
		return true;
	}
	else if (m_TeamData[1].dwID == dwGuildID)
	{
		bIdx = 1;
		return true;
	}

	return false;
}

uint32_t CWarMap::GetGuildID(uint8_t bIdx)
{
	assert(bIdx < 2);
	return m_TeamData[bIdx].dwID;
}

CGuild * CWarMap::GetGuild(uint8_t bIdx)
{
	return m_TeamData[bIdx].pkGuild;
}

int32_t CWarMap::GetMapIndex()
{
	return m_kMapInfo.lMapIndex;
}

uint8_t CWarMap::GetType()
{
	return m_kMapInfo.bType;
}

uint32_t CWarMap::GetGuildOpponent(LPCHARACTER ch)
{
	if (ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch)))
	{
		uint32_t gid = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch))->GetID();
		uint8_t idx;

		if (GetTeamIndex(gid, idx))
			return m_TeamData[!idx].dwID;
	}
	return 0;
}

uint32_t CWarMap::GetWinnerGuild()
{
	uint32_t win_gid = 0;

	if (m_TeamData[1].iScore > m_TeamData[0].iScore)
	{
		win_gid = m_TeamData[1].dwID;
	}
	else if (m_TeamData[0].iScore > m_TeamData[1].iScore)
	{
		win_gid = m_TeamData[0].dwID;
	}

	return (win_gid);
}

void CWarMap::UsePotion(LPCHARACTER ch, LPITEM item)
{
	if (m_pkEndEvent)
		return;

	if (ch->IsObserverMode())
		return;

	if (!ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch)))
		return;

	const TItemTable* itemProto = ItemSystem::GetItemProto(EntityFactory::CreateItemEntity(g_registry, item));
	if (!itemProto)
		return;

	int iPrice = itemProto->dwGold;

	uint32_t gid = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch))->GetID();

	if (gid == m_TeamData[0].dwID)
		m_TeamData[0].iUsePotionPrice += iPrice;
	else if (gid == m_TeamData[1].dwID)
		m_TeamData[1].iUsePotionPrice += iPrice;
}

int CWarMap::STeamData::GetAccumulatedJoinerCount()
{
	return set_pidJoiner.size();
}

int CWarMap::STeamData::GetCurJointerCount()
{
	return iMemberCount;
}

void CWarMap::STeamData::AppendMember(LPCHARACTER ch)
{
	set_pidJoiner.insert(ch->GetPlayerID());
	++iMemberCount;
}

void CWarMap::STeamData::RemoveMember(LPCHARACTER ch)
{
	// set_pidJoiner 는 누적 인원을 계산하기 때문에 제거하지 않는다
	--iMemberCount;
}


struct FSendUserCount
{
	char buf1[30];
	char buf2[128];

	FSendUserCount(uint32_t g1, int g1_count, uint32_t g2, int g2_count, int observer)
	{
		snprintf(buf1, sizeof(buf1), "ObserverCount %d", observer);
		snprintf(buf2, sizeof(buf2), "WarUC %u %d %u %d %d", g1, g1_count, g2, g2_count, observer);
	}

	void operator() (LPCHARACTER ch)
	{
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, buf1);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, buf2);
	}
};

void CWarMap::UpdateUserCount()
{
	FSendUserCount f(
			m_TeamData[0].dwID,
			m_TeamData[0].GetAccumulatedJoinerCount(),
			m_TeamData[1].dwID,
			m_TeamData[1].GetAccumulatedJoinerCount(),
			m_iObserverCount);

	std::for_each(m_set_pkChr.begin(), m_set_pkChr.end(), f);
}

void CWarMap::IncMember(LPCHARACTER ch)
{
	if (!ch->IsPC())
		return;

	LOG_TRACE("WarMap::IncMember");
	uint32_t gid = 0;

	if (ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch)))
		gid = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch))->GetID();

	bool isWarMember = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "war.is_war_member") > 0 ? true : false;

	if (isWarMember && gid != m_TeamData[0].dwID && gid != m_TeamData[1].dwID)
	{
		ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "war.is_war_member", 0);
		isWarMember = false;
	}

	if (isWarMember)
	{
		if (gid == m_TeamData[0].dwID)
		{
			m_TeamData[0].AppendMember(ch);

		}
		else if (gid == m_TeamData[1].dwID)
		{
			m_TeamData[1].AppendMember(ch);

		}

		event_cancel(&m_pkTimeoutEvent);

		LOG_TRACE("WarMap +m {}(cur:{}, acc:{}) vs {}(cur:{}, acc:{})", m_TeamData[0].dwID, m_TeamData[0].GetCurJointerCount(), m_TeamData[0].GetAccumulatedJoinerCount(), m_TeamData[1].dwID, m_TeamData[1].GetCurJointerCount(), m_TeamData[1].GetAccumulatedJoinerCount());
	}
	else
	{
		++m_iObserverCount;
		LOG_TRACE("WarMap +o {}", m_iObserverCount);
		ch->SetObserverMode(true);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 255, "");
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 448, "");
#endif
	}

	UpdateUserCount();

	m_set_pkChr.insert(ch);

	LPDESC d = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));

	SendWarPacket(d);
	SendScorePacket(0, d);
	SendScorePacket(1, d);
}

void CWarMap::DecMember(LPCHARACTER ch)
{
	if (!ch->IsPC())
		return;

	LOG_TRACE("WarMap::DecMember");
	uint32_t gid = 0;

	if (ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch)))
		gid = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch))->GetID();

	if (!ch->IsObserverMode())
	{
		if (gid == m_TeamData[0].dwID)
			m_TeamData[0].RemoveMember(ch);
		else if (gid == m_TeamData[1].dwID)
			m_TeamData[1].RemoveMember(ch);

		if (m_kMapInfo.bType == WAR_MAP_TYPE_FLAG)
		{
			CAffect * pkAff = ch->FindAffect(AFFECT_WAR_FLAG);

			if (pkAff)
			{
				uint8_t idx;

				if (GetTeamIndex(pkAff->lApplyValue, idx))
					AddFlag(idx, ch->GetX(), ch->GetY());

				AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), AFFECT_WAR_FLAG);
			}
		}

		LOG_TRACE("WarMap -m {}(cur:{}, acc:{}) vs {}(cur:{}, acc:{})", m_TeamData[0].dwID, m_TeamData[0].GetCurJointerCount(), m_TeamData[0].GetAccumulatedJoinerCount(), m_TeamData[1].dwID, m_TeamData[1].GetCurJointerCount(), m_TeamData[1].GetAccumulatedJoinerCount());

		CheckWarEnd();
		ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "war.is_war_member", 0);
	}
	else
	{
		--m_iObserverCount;

		LOG_TRACE("WarMap -o {}", m_iObserverCount);
		ch->SetObserverMode(false);
	}

	UpdateUserCount();

	m_set_pkChr.erase(ch);
}

struct FExitGuildWar
{
	void operator() (LPCHARACTER ch)
	{
		if (ch->IsPC())
		{
			ch->ExitToSavedLocation();
		}
	}
};

void CWarMap::ExitAll()
{
	FExitGuildWar f;
	std::for_each(m_set_pkChr.begin(), m_set_pkChr.end(), f);
}

void CWarMap::CheckWarEnd()
{
	if (m_bEnded)
		return;

	if (m_TeamData[0].iMemberCount == 0 || m_TeamData[1].iMemberCount == 0)
	{
		if (m_bTimeout)
			return;

		if (m_pkTimeoutEvent)
			return;

#ifdef TEXTS_IMPROVEMENT
		Notice(CHAT_TYPE_NOTICE, 701, "");
		Notice(CHAT_TYPE_NOTICE, 702, "");
#endif
		war_map_info* info = AllocEventInfo<war_map_info>();
		info->pWarMap = this;

		SetTimeoutEvent(event_create(war_timeout_event, info, PASSES_PER_SEC(60)));
	}
	else
		CheckScore();
}

int CWarMap::GetRewardGold(uint8_t bWinnerIdx)
{
	int iRewardGold = m_WarInfo.iWarPrice;
	iRewardGold += (m_TeamData[bWinnerIdx].iUsePotionPrice * m_WarInfo.iWinnerPotionRewardPctToWinner) / 100;
	iRewardGold += (m_TeamData[bWinnerIdx ? 0 : 1].iUsePotionPrice * m_WarInfo.iLoserPotionRewardPctToWinner) / 100;
	return iRewardGold;
}

void CWarMap::Draw()
{
	CGuildManager::instance().RequestWarOver(m_TeamData[0].dwID, m_TeamData[1].dwID, 0, 0);
}

void CWarMap::Timeout()
{
	SetTimeoutEvent(nullptr);

	if (m_bTimeout)
		return;

	if (m_bEnded)
		return;

	uint32_t dwWinner = 0;
	uint32_t dwLoser = 0;
	int iRewardGold = 0;

	if (get_dword_time() - m_dwStartTime < 60000 * 5)
	{
#ifdef TEXTS_IMPROVEMENT
		Notice(CHAT_TYPE_NOTICE, 703, "");
#endif
		dwWinner = 0;
		dwLoser = 0;
	}
	else
	{
		int iWinnerIdx = -1;

		if (m_TeamData[0].iMemberCount == 0)
			iWinnerIdx = 1;
		else if (m_TeamData[1].iMemberCount == 0)
			iWinnerIdx = 0;

		if (iWinnerIdx == -1)
		{
			dwWinner = GetWinnerGuild();

			if (dwWinner == m_TeamData[0].dwID)
			{
				iRewardGold = GetRewardGold(0);
				dwLoser = m_TeamData[1].dwID;
			}
			else if (dwWinner == m_TeamData[1].dwID)
			{
				iRewardGold = GetRewardGold(1);
				dwLoser = m_TeamData[0].dwID;
			}

			LOG_ERROR("WarMap: member count is not zero, guild1 {} {} guild2 {} {}, winner {}", m_TeamData[0].dwID, m_TeamData[0].iMemberCount, m_TeamData[1].dwID, m_TeamData[1].iMemberCount, dwWinner);
		}
		else
		{
			dwWinner = m_TeamData[iWinnerIdx].dwID;
			dwLoser = m_TeamData[iWinnerIdx == 0 ? 1 : 0].dwID;

			iRewardGold = GetRewardGold(iWinnerIdx);
		}
	}

	LOG_INFO("WarMap: Timeout {} {} winner {} loser {} reward {} map {}", m_TeamData[0].dwID, m_TeamData[1].dwID, dwWinner, dwLoser, iRewardGold, m_kMapInfo.lMapIndex);

	if (dwWinner)
		CGuildManager::instance().RequestWarOver(dwWinner, dwLoser, dwWinner, iRewardGold);
	else
		CGuildManager::instance().RequestWarOver(m_TeamData[0].dwID, m_TeamData[1].dwID, dwWinner, iRewardGold);

	m_bTimeout = true;
}

namespace
{
	struct FPacket
	{
		FPacket(const void * p, int size) : m_pvData(p), m_iSize(size)
		{
		}

		void operator () (LPCHARACTER ch)
		{
			ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(m_pvData, m_iSize);
		}

		const void * m_pvData;
		int m_iSize;
	};

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
};

#ifdef TEXTS_IMPROVEMENT
void CWarMap::Notice(uint8_t type, uint32_t idx, const char * format, ...)
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

void CWarMap::Packet(const void * p, int size)
{
	FPacket f(p, size);
	std::for_each(m_set_pkChr.begin(), m_set_pkChr.end(), f);
}

void CWarMap::SendWarPacket(LPDESC d)
{
	TPacketGCGuild pack;
	TPacketGCGuildWar pack2;

	pack.header		= HEADER_GC_GUILD;
	pack.subheader	= GUILD_SUBHEADER_GC_WAR;
	pack.size		= sizeof(pack) + sizeof(pack2);

	pack2.dwGuildSelf	= m_TeamData[0].dwID;
	pack2.dwGuildOpp	= m_TeamData[1].dwID;
	pack2.bType		= CGuildManager::instance().TouchGuild(m_TeamData[0].dwID)->GetGuildWarType(m_TeamData[1].dwID);
	pack2.bWarState	= CGuildManager::instance().TouchGuild(m_TeamData[0].dwID)->GetGuildWarState(m_TeamData[1].dwID);

	d->BufferedPacket(&pack, sizeof(pack));
	d->Packet(&pack2, sizeof(pack2));
}

void CWarMap::SendScorePacket(uint8_t bIdx, LPDESC d)
{
	TPacketGCGuild p;

	p.header = HEADER_GC_GUILD;
	p.subheader = GUILD_SUBHEADER_GC_WAR_SCORE;
	p.size = sizeof(p) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int32_t);

	TEMP_BUFFER buf;
	buf.write(&p, sizeof(p));
	buf.write(&m_TeamData[bIdx].dwID, sizeof(uint32_t));
	buf.write(&m_TeamData[bIdx ? 0 : 1].dwID, sizeof(uint32_t));
	buf.write(&m_TeamData[bIdx].iScore, sizeof(int32_t));

	if (d)
		d->Packet(buf.read_peek(), buf.size());
	else
		Packet(buf.read_peek(), buf.size());
}

void CWarMap::UpdateScore(uint32_t g1, int score1, uint32_t g2, int score2)
{
	uint8_t idx;

	if (GetTeamIndex(g1, idx))
	{
		if (m_TeamData[idx].iScore != score1)
		{
			m_TeamData[idx].iScore = score1;
			SendScorePacket(idx);
		}
	}

	if (GetTeamIndex(g2, idx))
	{
		if (m_TeamData[idx].iScore != score2)
		{
			m_TeamData[idx].iScore = score2;
			SendScorePacket(idx);
		}
	}

	CheckScore();
}

bool CWarMap::CheckScore()
{
	if (m_bEnded)
		return true;

	// 30초 이후 부터 확인한다.
	if (get_dword_time() - m_dwStartTime < 30000)
		return false;

	// 점수가 같으면 체크하지 않는다.
	if (m_TeamData[0].iScore == m_TeamData[1].iScore)
		return false;

	int iEndScore = m_WarInfo.iEndScore;

	if (test_server) iEndScore /= 10;

	uint32_t dwWinner;
	uint32_t dwLoser;

	if (m_TeamData[0].iScore >= iEndScore)
	{
		dwWinner = m_TeamData[0].dwID;
		dwLoser = m_TeamData[1].dwID;
	}
	else if (m_TeamData[1].iScore >= iEndScore)
	{
		dwWinner = m_TeamData[1].dwID;
		dwLoser = m_TeamData[0].dwID;
	}
	else
		return false;

	int iRewardGold = 0;

	if (dwWinner == m_TeamData[0].dwID)
		iRewardGold = GetRewardGold(0);
	else if (dwWinner == m_TeamData[1].dwID)
		iRewardGold = GetRewardGold(1);

	LOG_INFO("WarMap::CheckScore end score {} guild1 {} score guild2 {} {} score {} winner {} reward {}", iEndScore, m_TeamData[0].dwID, m_TeamData[0].iScore, m_TeamData[1].dwID, m_TeamData[1].iScore, dwWinner, iRewardGold);

	CGuildManager::instance().RequestWarOver(dwWinner, dwLoser, dwWinner, iRewardGold);
	return true;
}

bool CWarMap::SetEnded()
{
	LOG_INFO("WarMap::SetEnded {}", m_kMapInfo.lMapIndex);

	if (m_pkEndEvent)
		return false;

	if (m_TeamData[0].pkChrFlag)
	{
		M2_DESTROY_CHARACTER(m_TeamData[0].pkChrFlag);
		m_TeamData[0].pkChrFlag = nullptr;
	}

	if (m_TeamData[0].pkChrFlagBase)
	{
		M2_DESTROY_CHARACTER(m_TeamData[0].pkChrFlagBase);
		m_TeamData[0].pkChrFlagBase = nullptr;
	}

	if (m_TeamData[1].pkChrFlag)
	{
		M2_DESTROY_CHARACTER(m_TeamData[1].pkChrFlag);
		m_TeamData[1].pkChrFlag = nullptr;
	}

	if (m_TeamData[1].pkChrFlagBase)
	{
		M2_DESTROY_CHARACTER(m_TeamData[1].pkChrFlagBase);
		m_TeamData[1].pkChrFlagBase = nullptr;
	}

	event_cancel(&m_pkResetFlagEvent);
	m_bEnded = true;

	war_map_info* info = AllocEventInfo<war_map_info>();

	info->pWarMap = this;
	info->iStep = 0;
	SetEndEvent(event_create(war_end_event, info, PASSES_PER_SEC(10)));
	return true;
}

void CWarMap::OnKill(LPCHARACTER killer, LPCHARACTER ch)
{
	if (m_bEnded)
		return;

	uint32_t dwKillerGuild = 0;
	uint32_t dwDeadGuild = 0;

	if (ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(killer)))
		dwKillerGuild = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(killer))->GetID();

	if (ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch)))
		dwDeadGuild = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch))->GetID();

	uint8_t idx;

	LOG_INFO("WarMap::OnKill {} {}", dwKillerGuild, dwDeadGuild);

	if (!GetTeamIndex(dwKillerGuild, idx))
		return;

	if (!GetTeamIndex(dwDeadGuild, idx))
		return;

	switch (m_kMapInfo.bType)
	{
		case WAR_MAP_TYPE_NORMAL:
			SendGuildWarScore(dwKillerGuild, dwDeadGuild, 1, ch->GetLevel());
			break;

		case WAR_MAP_TYPE_FLAG:
			{
				CAffect * pkAff = ch->FindAffect(AFFECT_WAR_FLAG);

				if (pkAff)
				{
					if (GetTeamIndex(pkAff->lApplyValue, idx))
						AddFlag(idx, ch->GetX(), ch->GetY());

					AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), AFFECT_WAR_FLAG);
				}
			}
			break;

		default:
			LOG_ERROR("unknown war map type {} index {}", static_cast<int>(m_kMapInfo.bType), m_kMapInfo.lMapIndex);
			break;
	}
}

void CWarMap::AddFlagBase(uint8_t bIdx, uint32_t x, uint32_t y)
{
	if (m_bEnded)
		return;

	assert(bIdx < 2);

	TeamData & r = m_TeamData[bIdx];

	if (r.pkChrFlagBase)
		return;

	if (x == 0)
	{
		x = m_kMapInfo.posStart[bIdx].x;
		y = m_kMapInfo.posStart[bIdx].y;
	}

	r.pkChrFlagBase = CHARACTER_MANAGER::instance().SpawnMob(warmap::WAR_FLAG_BASE_VNUM, m_kMapInfo.lMapIndex, x, y, 0);
	LOG_INFO("WarMap::AddFlagBase {} {} id {}", static_cast<int>(bIdx), static_cast<const void*>(get_pointer(r.pkChrFlagBase)), r.dwID);

	r.pkChrFlagBase->SetPoint(POINT_STAT, r.dwID);
	r.pkChrFlagBase->SetWarMap(this);
}

void CWarMap::AddFlag(uint8_t bIdx, uint32_t x, uint32_t y)
{
	if (m_bEnded)
		return;

	assert(bIdx < 2);

	TeamData & r = m_TeamData[bIdx];

	if (r.pkChrFlag)
		return;

	if (x == 0)
	{
		x = m_kMapInfo.posStart[bIdx].x;
		y = m_kMapInfo.posStart[bIdx].y;
	}

	r.pkChrFlag = CHARACTER_MANAGER::instance().SpawnMob(bIdx == 0 ? warmap::WAR_FLAG_VNUM0 : warmap::WAR_FLAG_VNUM1, m_kMapInfo.lMapIndex, x, y, 0);
	LOG_INFO("WarMap::AddFlag {} {} id {}", static_cast<int>(bIdx), static_cast<const void*>(get_pointer(r.pkChrFlag)), r.dwID);

	r.pkChrFlag->SetPoint(POINT_STAT, r.dwID);
	r.pkChrFlag->SetWarMap(this);
}

void CWarMap::RemoveFlag(uint8_t bIdx)
{
	assert(bIdx < 2);

	TeamData & r = m_TeamData[bIdx];

	if (!r.pkChrFlag)
		return;

	LOG_INFO("WarMap::RemoveFlag {} {}", static_cast<int>(bIdx), static_cast<const void*>(get_pointer(r.pkChrFlag)));

	r.pkChrFlag->Dead(nullptr, true);
	r.pkChrFlag = nullptr;
}

bool CWarMap::IsFlagOnBase(uint8_t bIdx)
{
	assert(bIdx < 2);

	TeamData & r = m_TeamData[bIdx];

	if (!r.pkChrFlag)
		return false;

	const PIXEL_POSITION & pos = r.pkChrFlag->GetXYZ();

	if (pos.x == m_kMapInfo.posStart[bIdx].x && pos.y == m_kMapInfo.posStart[bIdx].y)
		return true;

	return false;
}

EVENTFUNC(war_reset_flag_event)
{
	war_map_info* info = dynamic_cast<war_map_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("war_reset_flag_event> <Factor> Null pointer");
		return 0;
	}

	CWarMap * pMap = info->pWarMap;

	pMap->AddFlag(0);
	pMap->AddFlag(1);

	pMap->SetResetFlagEvent(nullptr);
	return 0;
}

struct FRemoveFlagAffect
{
	void operator() (LPCHARACTER ch)
	{
		if (ch->FindAffect(AFFECT_WAR_FLAG))
			AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), AFFECT_WAR_FLAG);
	}
};

void CWarMap::ResetFlag()
{
	if (m_kMapInfo.bType != WAR_MAP_TYPE_FLAG)
		return;

	if (m_pkResetFlagEvent)
		return;

	if (m_bEnded)
		return;

	FRemoveFlagAffect f;
	std::for_each(m_set_pkChr.begin(), m_set_pkChr.end(), f);

	RemoveFlag(0);
	RemoveFlag(1);

	war_map_info* info = AllocEventInfo<war_map_info>();

	info->pWarMap = this;
	info->iStep = 0;
	SetResetFlagEvent(event_create(war_reset_flag_event, info, PASSES_PER_SEC(10)));
}

/////////////////////////////////////////////////////////////////////////////////
// WarMapManager
/////////////////////////////////////////////////////////////////////////////////
CWarMapManager::CWarMapManager()
{
}

CWarMapManager::~CWarMapManager()
{
	for( std::map<int32_t, TWarMapInfo *>::const_iterator iter = m_map_kWarMapInfo.begin() ; iter != m_map_kWarMapInfo.end() ; ++iter )
	{
		M2_DELETE(iter->second);
	}

	m_map_kWarMapInfo.clear();
}

bool CWarMapManager::LoadWarMapInfo(const char * c_pszFileName)
{
	TWarMapInfo * k;

	k = M2_NEW TWarMapInfo;
	k->bType = WAR_MAP_TYPE_NORMAL;

	k->lMapIndex = 110;//Razor93
	k->posStart[0].x = 48 * 733 + 0;//368 00---------------35193 22380
	k->posStart[0].y = 52 * 430 + 0;//5200-----------36800x5200 1.pozicio 
	k->posStart[1].x = 183 * 288 + 0;//503 00------52818 5071
	k->posStart[1].y = 206 * 25 + 0;//206 00----------50300x20600 2. pozicio
	k->posStart[2].x = 141 * 100 + 32000;//461 00-----
	k->posStart[2].y = 117 * 100 + 0;//117 00---------nez?k pozicioja

	m_map_kWarMapInfo.insert(std::make_pair(k->lMapIndex, k));

	k = M2_NEW TWarMapInfo;
	k->bType = WAR_MAP_TYPE_FLAG;

	k->lMapIndex = 111;
	k->posStart[0].x = 68 * 100 + 57600;
	k->posStart[0].y = 69 * 100 + 0;
	k->posStart[1].x = 171 * 100 + 57600;
	k->posStart[1].y = 182 * 100 + 0;
	k->posStart[2].x = 122 * 100 + 57600;
	k->posStart[2].y = 131 * 100 + 0;

	m_map_kWarMapInfo.insert(std::make_pair(k->lMapIndex, k));
	return true;
}

bool CWarMapManager::IsWarMap(int32_t lMapIndex)
{
	return GetWarMapInfo(lMapIndex) ? true : false;
}

bool CWarMapManager::GetStartPosition(int32_t lMapIndex, uint8_t bIdx, PIXEL_POSITION & pos)
{
	assert(bIdx < 3);

	TWarMapInfo * pi = GetWarMapInfo(lMapIndex);

	if (!pi)
	{
		LOG_INFO("GetStartPosition FAILED [{}] WarMapInfoSize({})", lMapIndex, m_map_kWarMapInfo.size());

		for (auto it = m_map_kWarMapInfo.begin(); it != m_map_kWarMapInfo.end(); ++it)
		{
			PIXEL_POSITION& cur=it->second->posStart[bIdx];
			LOG_INFO("WarMap[{}]=Pos({}, {})", it->first, cur.x, cur.y);
		}
		return false;
	}

	pos = pi->posStart[bIdx];
	return true;
}

int32_t CWarMapManager::CreateWarMap(const TGuildWarInfo& guildWarInfo, uint32_t dwGuildID1, uint32_t dwGuildID2)
{
	TWarMapInfo * pkInfo = GetWarMapInfo(guildWarInfo.lMapIndex);
	if (!pkInfo)
	{
		LOG_ERROR("GuildWar.CreateWarMap.NOT_FOUND_MAPINFO[{}]", guildWarInfo.lMapIndex);
		return 0;
	}

	uint32_t lMapIndex = SECTREE_MANAGER::instance().CreatePrivateMap(guildWarInfo.lMapIndex);

	if (lMapIndex)
	{
		m_mapWarMap.insert(std::make_pair(lMapIndex, M2_NEW CWarMap(lMapIndex, guildWarInfo, pkInfo, dwGuildID1, dwGuildID2)));
	}

	return lMapIndex;
}

TWarMapInfo * CWarMapManager::GetWarMapInfo(int32_t lMapIndex)
{
	if (lMapIndex >= 10000)
		lMapIndex /= 10000;

	auto it = m_map_kWarMapInfo.find(lMapIndex);

	if (m_map_kWarMapInfo.end() == it)
		return nullptr;

	return it->second;
}

void CWarMapManager::DestroyWarMap(CWarMap* pMap)
{
	int32_t mapIdx = pMap->GetMapIndex();

	LOG_INFO("WarMap::DestroyWarMap : {}", mapIdx);

	m_mapWarMap.erase(pMap->GetMapIndex());
	M2_DELETE(pMap);

	SECTREE_MANAGER::instance().DestroyPrivateMap(mapIdx);
}

CWarMap * CWarMapManager::Find(int32_t lMapIndex)
{
	auto it = m_mapWarMap.find(lMapIndex);

	if (it == m_mapWarMap.end())
		return nullptr;

	return it->second;
}

void CWarMapManager::OnShutdown()
{
	auto it = m_mapWarMap.begin();

	while (it != m_mapWarMap.end())
		(it++)->second->Draw();
}


