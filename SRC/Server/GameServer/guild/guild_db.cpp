#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "guild.h"
#include "guild_manager.h"
#include "packet.h"
#include "protocol.h"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "char_manager.h"
#include "desc.h"
#include "desc_client.h"
#include "priv_manager.h"

void CInputDB::GuildLoad(const char * c_pData)
{
	CGuildManager::instance().LoadGuild(*(uint32_t *) c_pData);
}

void CInputDB::GuildSkillUpdate(const char* c_pData)
{
	TPacketGuildSkillUpdate * p = (TPacketGuildSkillUpdate *) c_pData;

	CGuild * g = CGuildManager::instance().TouchGuild(p->guild_id);

	if (g)
	{
		g->UpdateSkill(p->skill_point, p->skill_levels);
		g->GuildPointChange(POINT_SP, p->amount, p->save?true:false);
	}
}

void CInputDB::GuildWar(const char* c_pData)
{
	TPacketGuildWar * p = (TPacketGuildWar*) c_pData;

	LOG_INFO("InputDB::GuildWar {} {} state {}", p->dwGuildFrom, p->dwGuildTo, p->bWar);

	switch (p->bWar)
	{
		case GUILD_WAR_SEND_DECLARE:
		case GUILD_WAR_RECV_DECLARE:
			CGuildManager::instance().DeclareWar(p->dwGuildFrom, p->dwGuildTo, p->bType);
			break;

		case GUILD_WAR_REFUSE:
			CGuildManager::instance().RefuseWar(p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_WAIT_START:
			CGuildManager::instance().WaitStartWar(p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_CANCEL:
			CGuildManager::instance().CancelWar(p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_ON_WAR:
			CGuildManager::instance().StartWar(p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_END:
			CGuildManager::instance().EndWar(p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_OVER:
			CGuildManager::instance().WarOver(p->dwGuildFrom, p->dwGuildTo, p->bType);
			break;

		case GUILD_WAR_RESERVE:
			CGuildManager::instance().ReserveWar(p->dwGuildFrom, p->dwGuildTo, p->bType);
			break;

		default:
			LOG_ERROR("Unknown guild war state");
			break;
	}
}

#ifdef ADVANCED_GUILD_INFO
void CInputDB::GuildResetStats(const char* c_pData)
{
	CGuildManager::instance().ResetStatsToAll();
}
#endif

void CInputDB::GuildWarScore(const char* c_pData)
{
	TPacketGuildWarScore* p = (TPacketGuildWarScore*) c_pData;
	CGuild * g = CGuildManager::instance().TouchGuild(p->dwGuildGainPoint);

	if (!g)
		return;

	g->SetWarScoreAgainstTo(p->dwGuildOpponent, p->lScore);
}

void CInputDB::GuildSkillRecharge()
{
	CGuildManager::instance().SkillRecharge();
}

void CInputDB::GuildExpUpdate(const char* c_pData)
{
	TPacketGuildSkillUpdate * p = (TPacketGuildSkillUpdate *) c_pData;
	LOG_INFO("GuildExpUpdate {}", p->amount);

	CGuild * g = CGuildManager::instance().TouchGuild(p->guild_id);

	if (g)
		g->GuildPointChange(POINT_EXP, p->amount);
}

void CInputDB::GuildAddMember(const char* c_pData)
{
	TPacketDGGuildMember * p = (TPacketDGGuildMember *) c_pData;
	CGuild * g = CGuildManager::instance().TouchGuild(p->dwGuild);

	if (g)
		g->AddMember(p);
}

void CInputDB::GuildRemoveMember(const char* c_pData)
{
	TPacketGuild* p=(TPacketGuild*)c_pData;
	CGuild* g = CGuildManager::instance().TouchGuild(p->dwGuild);

	if (g)
		g->RemoveMember(p->dwInfo);
}

void CInputDB::GuildChangeGrade(const char* c_pData)
{
	TPacketGuild* p=(TPacketGuild*)c_pData;
	CGuild* g = CGuildManager::instance().TouchGuild(p->dwGuild);

	if (g)
		g->P2PChangeGrade((uint8_t)p->dwInfo);
}

void CInputDB::GuildChangeMemberData(const char* c_pData)
{
	LOG_INFO("Recv GuildChangeMemberData");
	TPacketGuildChangeMemberData * p = (TPacketGuildChangeMemberData *) c_pData;
	CGuild * g = CGuildManager::instance().TouchGuild(p->guild_id);

	if (g)
		g->ChangeMemberData(p->pid, p->offer, p->level, p->grade);
}

void CInputDB::GuildDisband(const char* c_pData)
{
	TPacketGuild * p = (TPacketGuild*) c_pData;
	CGuildManager::instance().DisbandGuild(p->dwGuild);
}

void CInputDB::GuildLadder(const char* c_pData)
{
	TPacketGuildLadder* p = (TPacketGuildLadder*) c_pData;
	LOG_INFO("Recv GuildLadder {} {} / w {} d {} l {}", p->dwGuild, p->lLadderPoint, p->lWin, p->lDraw, p->lLoss);
	CGuild * g = CGuildManager::instance().TouchGuild(p->dwGuild);

	if (!g)
		return;

	g->SetLadderPoint(p->lLadderPoint);
	g->SetWarData(p->lWin, p->lDraw, p->lLoss);
}

void CInputDB::GuildSkillUsableChange(const char* c_pData)
{
	TPacketGuildSkillUsableChange* p = (TPacketGuildSkillUsableChange*) c_pData;

	CGuild* g = CGuildManager::instance().TouchGuild(p->dwGuild);

	if (!g)
		return;

	g->SkillUsableChange(p->dwSkillVnum, p->bUsable?true:false);
}

void CInputDB::ChangeGuildPriv(const char* c_pData)
{
	TPacketDGChangeGuildPriv* p = (TPacketDGChangeGuildPriv*) c_pData;

	// ADD_GUILD_PRIV_TIME
	CPrivManager::instance().GiveGuildPriv(p->guild_id, p->type, p->value, p->bLog, p->end_time_sec);
	// END_OF_ADD_GUILD_PRIV_TIME
}

void CInputDB::GuildMoneyChange(const char* c_pData)
{
	TPacketDGGuildMoneyChange* p = (TPacketDGGuildMoneyChange*) c_pData;

	CGuild* g = CGuildManager::instance().TouchGuild(p->dwGuild);
	if (g)
	{
		g->RecvMoneyChange(p->iTotalGold);
	}
}

void CInputDB::GuildWithdrawMoney(const char* c_pData)
{
	TPacketDGGuildMoneyWithdraw* p = (TPacketDGGuildMoneyWithdraw*) c_pData;

	CGuild* g = CGuildManager::instance().TouchGuild(p->dwGuild);
	if (g)
	{
		g->RecvWithdrawMoneyGive(p->iChangeGold);
	}
}

void CInputDB::GuildWarReserveAdd(TGuildWarReserve * p)
{
	CGuildManager::instance().ReserveWarAdd(p);
}

void CInputDB::GuildWarReserveDelete(uint32_t dwID)
{
	CGuildManager::instance().ReserveWarDelete(dwID);
}

void CInputDB::GuildWarBet(TPacketGDGuildWarBet * p)
{
	CGuildManager::instance().ReserveWarBet(p);
}

void CInputDB::GuildChangeMaster(TPacketChangeGuildMaster* p)
{
	CGuildManager::instance().ChangeMaster(p->dwGuildID);
}
