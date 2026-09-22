#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "char.h"
#include "char_manager.h"
#include "desc.h"
#include "config.h"
#include "constants.h"
#include "db.h"
#include "guild.h"
#include "guild_manager.h"
#include "log.h"
#include "packet.h"
#include "protocol.h"
#include "questmanager.h"
#include "utils.h"

static int __deposit_limit()
{
	return (1000*10000);
}

void CInputMain::AnswerMakeGuild(entt::entity character, const char* c_pData)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character))
		return;
	TPacketCGAnswerMakeGuild* p = (TPacketCGAnswerMakeGuild*) c_pData;

	if (ecs::PointSystem::GetGold(character) < 200000) {
		return;
	}
#ifdef ENABLE_BUG_FIXES
	else if (ecs::PointSystem::GetLevel(character) < 40) {
		return;
	}
#endif

	if (get_global_time() - ecs::QuestSystem::GetFlag(character, "guild_manage.new_disband_time") < CGuildManager::instance().GetDisbandDelay()) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 181, "%d", quest::CQuestManager::instance().GetEventFlag("guild_disband_delay"));
#endif
		return;
	}

	if (get_global_time() - ecs::QuestSystem::GetFlag(character, "guild_manage.new_withdraw_time") < CGuildManager::instance().GetWithdrawDelay()) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 179, "%d", quest::CQuestManager::instance().GetEventFlag("guild_withdraw_delay"));
#endif
		return;
	}

	if (ecs::SocialSystem::GetGuild(character))
		return;

	CGuildManager& gm = CGuildManager::instance();

	TGuildCreateParameter cp;
	memset(&cp, 0, sizeof(cp));

	cp.master = character;
	static_assert(sizeof(cp.name) == sizeof(p->guild_name));
	memcpy(cp.name, p->guild_name, sizeof(cp.name));
	cp.name[sizeof(cp.name) - 1] = '\0';

	if (cp.name[0] == 0 || !check_name(cp.name))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 455, "");
#endif
		return;
	}

	uint32_t dwGuildID = gm.CreateGuild(cp);

	if (dwGuildID)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 125, "%s", cp.name);
#endif
		int GuildCreateFee = 200000;

		ecs::PointSystem::Change(character, POINT_GOLD, -GuildCreateFee);
		DBManager::instance().SendMoneyLog(MONEY_LOG_GUILD, ecs::PlayerRuntime::GetPlayerID(character), -GuildCreateFee);

		char Log[128];
		snprintf(Log, sizeof(Log), "GUILD_NAME %s MASTER %s", cp.name, ecs::PlayerRuntime::GetName(character).data());
		LogManager::instance().CharLog(character, 0, "MAKE_GUILD", Log);

		ItemSystem::RemoveSpecifyItemEcs(character, GUILD_CREATE_ITEM_VNUM, 1);
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 132, "");
	}
#endif
}

size_t GetSubPacketSize(const GUILD_SUBHEADER_CG& header)
{
	switch (header)
	{
		case GUILD_SUBHEADER_CG_DEPOSIT_MONEY:				return sizeof(int);
		case GUILD_SUBHEADER_CG_WITHDRAW_MONEY:				return sizeof(int);
		case GUILD_SUBHEADER_CG_ADD_MEMBER:					return sizeof(uint32_t);
		case GUILD_SUBHEADER_CG_REMOVE_MEMBER:				return sizeof(uint32_t);
		case GUILD_SUBHEADER_CG_CHANGE_GRADE_NAME:			return 10;
		case GUILD_SUBHEADER_CG_CHANGE_GRADE_AUTHORITY:		return sizeof(uint8_t) + sizeof(uint8_t);
		case GUILD_SUBHEADER_CG_OFFER:						return sizeof(uint32_t);
		case GUILD_SUBHEADER_CG_CHARGE_GSP:					return sizeof(int);
		case GUILD_SUBHEADER_CG_POST_COMMENT:				return 1;
		case GUILD_SUBHEADER_CG_DELETE_COMMENT:				return sizeof(uint32_t);
		case GUILD_SUBHEADER_CG_REFRESH_COMMENT:			return 0;
		case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GRADE:		return sizeof(uint32_t) + sizeof(uint8_t);
		case GUILD_SUBHEADER_CG_USE_SKILL:					return sizeof(TPacketCGGuildUseSkill);
		case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GENERAL:		return sizeof(uint32_t) + sizeof(uint8_t);
		case GUILD_SUBHEADER_CG_GUILD_INVITE_ANSWER:		return sizeof(uint32_t) + sizeof(uint8_t);
	}

	return 0;
}

int CInputMain::Guild(entt::entity character, const char * data, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Guild handler ECS
// DUAL-PATH: legacy only during migration window
	if (uiBytes < sizeof(TPacketCGGuild))
		return -1;

	const TPacketCGGuild* p = reinterpret_cast<const TPacketCGGuild*>(data);
	const char* c_pData = data + sizeof(TPacketCGGuild);

	uiBytes -= sizeof(TPacketCGGuild);

	const GUILD_SUBHEADER_CG SubHeader = static_cast<GUILD_SUBHEADER_CG>(p->subheader);
	const size_t SubPacketLen = GetSubPacketSize(SubHeader);

	if (uiBytes < SubPacketLen)
	{
		return -1;
	}

	CGuild* pGuild = ecs::SocialSystem::GetGuild(character);

	if (nullptr == pGuild)
	{
		if (SubHeader != GUILD_SUBHEADER_CG_GUILD_INVITE_ANSWER)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 138, "");
#endif
			return SubPacketLen;
		}
	}

	switch (SubHeader)
	{
		case GUILD_SUBHEADER_CG_DEPOSIT_MONEY:
			{
				return SubPacketLen;

				const int gold = MIN(*reinterpret_cast<const int*>(c_pData), __deposit_limit());

				if (gold < 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 170, "");
#endif
					return SubPacketLen;
				}

				if (ecs::PointSystem::GetGold(character) < gold)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 126, "");
#endif
					return SubPacketLen;
				}

				ecs::SocialSystem::DepositGuildMoney(character, *pGuild, gold);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_WITHDRAW_MONEY:
			{
				return SubPacketLen;

				const int gold = MIN(*reinterpret_cast<const int*>(c_pData), 500000);

				if (gold < 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 170, "");
#endif
					return SubPacketLen;
				}

				pGuild->RequestWithdrawMoney(character, gold);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_ADD_MEMBER:
			{
				const uint32_t vid = *reinterpret_cast<const uint32_t*>(c_pData);
				const entt::entity newmember = CHARACTER_MANAGER::instance().FindEntity(vid);

				if (newmember == entt::null)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 128, "");
#endif
					return SubPacketLen;
				}

				// @fixme145 BEGIN (+newmember ispc check)
				if (!ecs::PlayerRuntime::IsPC(character) || !ecs::PlayerRuntime::IsPC(newmember))
					return SubPacketLen;
				// @fixme145 END

				pGuild->Invite(character, newmember);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_REMOVE_MEMBER:
			{
				if (pGuild->UnderAnyWar() != 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 649, "");
#endif
					return SubPacketLen;
				}

				const uint32_t pid = *reinterpret_cast<const uint32_t*>(c_pData);
				const TGuildMember* m = pGuild->GetMember(ecs::PlayerRuntime::GetPlayerID(character));

				if (nullptr == m)
					return -1;

				const entt::entity member = CHARACTER_MANAGER::instance().FindEntityByPID(pid);

				if (member != entt::null)
				{
					if (ecs::SocialSystem::GetGuild(member) != pGuild)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 161, "");
#endif
						return SubPacketLen;
					}

					if (!pGuild->HasGradeAuth(m->grade, GUILD_AUTH_REMOVE_MEMBER))
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 139, "");
#endif
						return SubPacketLen;
					}

					ecs::QuestSystem::SetFlag(member, "guild_manage.new_withdraw_time", get_global_time());
					pGuild->RequestRemoveMember(ecs::PlayerRuntime::GetPlayerID(member));

					if (g_bGuildInviteLimit)
					{
						DBManager::instance().Query("REPLACE INTO guild_invite_limit VALUES(%d, %d)", pGuild->GetID(), get_global_time());
					}
				}
				else
				{
					if (!pGuild->HasGradeAuth(m->grade, GUILD_AUTH_REMOVE_MEMBER))
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 139, "");
#endif
						return SubPacketLen;
					}

#ifdef TEXTS_IMPROVEMENT
					if (pGuild->RequestRemoveMember(pid)) {
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 129, "");
					} else {
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 128, "");
					}
#endif
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHANGE_GRADE_NAME:
			{
				char gradename[GUILD_GRADE_NAME_MAX_LEN + 1];
				strlcpy(gradename, c_pData + 1, sizeof(gradename));

				const TGuildMember * m = pGuild->GetMember(ecs::PlayerRuntime::GetPlayerID(character));

				if (nullptr == m)
					return -1;

				if (m->grade != GUILD_LEADER_GRADE) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 175, "");
#endif
				} else if (*c_pData == GUILD_LEADER_GRADE) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 143, "");
#endif
				}
				else if (!check_name(gradename)) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 171, "");
#endif
				}
				else {
					pGuild->ChangeGradeName(*c_pData, gradename);
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHANGE_GRADE_AUTHORITY:
			{
				const TGuildMember* m = pGuild->GetMember(ecs::PlayerRuntime::GetPlayerID(character));
				if (nullptr == m)
					return -1;

				if (m->grade != GUILD_LEADER_GRADE) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 174, "");
#endif
				} else if (*c_pData == GUILD_LEADER_GRADE) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 142, "");
#endif
				}
				else {
					pGuild->ChangeGradeAuth(*c_pData, *(c_pData + 1));
				}
			}
			return SubPacketLen;
		case GUILD_SUBHEADER_CG_OFFER:
			{
				uint32_t offer = *reinterpret_cast<const uint32_t*>(c_pData);

				if (pGuild->GetLevel() >= GUILD_MAX_LEVEL)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 650, "%d", GUILD_MAX_LEVEL);
#endif
				}
				else
				{
					offer /= 100;
					offer *= 100;
#ifdef TEXTS_IMPROVEMENT
					if (pGuild->OfferExp(character, offer)) {
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 121, "%u", offer);
					} else {
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 122, "");
					}
#endif
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHARGE_GSP:
			{
				const int offer = *reinterpret_cast<const int*>(c_pData);
				const int gold = offer * 100;

				if (offer < 0 || gold < offer || gold < 0 || ecs::PointSystem::GetGold(character) < gold)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 151, "");
#endif
					return SubPacketLen;
				}

#ifdef TEXTS_IMPROVEMENT
				if (!pGuild->ChargeSP(character, offer)) {
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 164, "");
				}
#endif
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_POST_COMMENT:
			{
				const size_t length = *c_pData;

				if (length > GUILD_COMMENT_MAX_LEN)
				{
					LOG_ERROR("POST_COMMENT: {} comment too long (length: {})", ecs::PlayerRuntime::GetName(character).data(), length);
					ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);
					return -1;
				}

				if (uiBytes < 1 + length)
					return -1;

				const TGuildMember* m = pGuild->GetMember(ecs::PlayerRuntime::GetPlayerID(character));

				if (nullptr == m)
					return -1;

				if (length && !pGuild->HasGradeAuth(m->grade, GUILD_AUTH_NOTICE) && *(c_pData + 1) == '!')
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 127, "");
#endif
				}
				else
				{
					std::string str(c_pData + 1, length);
					pGuild->AddComment(character, str);
				}

				return (1 + length);
			}

		case GUILD_SUBHEADER_CG_DELETE_COMMENT:
			{
				const uint32_t comment_id = *reinterpret_cast<const uint32_t*>(c_pData);

				pGuild->DeleteComment(character, comment_id);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_REFRESH_COMMENT:
			pGuild->RefreshComment(character);
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GRADE:
			{
				const uint32_t pid = *reinterpret_cast<const uint32_t*>(c_pData);
				const uint8_t grade = *(c_pData + sizeof(uint32_t));
				const TGuildMember* m = pGuild->GetMember(ecs::PlayerRuntime::GetPlayerID(character));

				if (nullptr == m)
					return -1;

				if (m->grade != GUILD_LEADER_GRADE) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 176, "");
#endif
				} else if (ecs::PlayerRuntime::GetPlayerID(character) == pid) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 143, "");
#endif
				} else if (grade == 1) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 141, "");
#endif
				} else {
					pGuild->ChangeMemberGrade(pid, grade);
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_USE_SKILL:
			{
				const TPacketCGGuildUseSkill* p = reinterpret_cast<const TPacketCGGuildUseSkill*>(c_pData);

				pGuild->UseSkill(p->dwVnum, character, p->dwPID);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GENERAL:
			{
				const uint32_t pid = *reinterpret_cast<const uint32_t*>(c_pData);
				const uint8_t is_general = *(c_pData + sizeof(uint32_t));
				const TGuildMember* m = pGuild->GetMember(ecs::PlayerRuntime::GetPlayerID(character));

				if (nullptr == m)
					return -1;

#ifdef TEXTS_IMPROVEMENT
				if (m->grade != GUILD_LEADER_GRADE) {
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 150, "");
				} else if (!pGuild->ChangeMemberGeneral(pid, is_general)) {
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 149, "");
				}
#endif
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_GUILD_INVITE_ANSWER:
			{
				const uint32_t guild_id = *reinterpret_cast<const uint32_t*>(c_pData);
				const uint8_t accept = *(c_pData + sizeof(uint32_t));

				CGuild * g = CGuildManager::instance().FindGuild(guild_id);

				if (g)
				{
					if (accept)
						g->InviteAccept(character);
					else
						g->InviteDeny(ecs::PlayerRuntime::GetPlayerID(character));
				}
			}
			return SubPacketLen;

	}

	return 0;
}

