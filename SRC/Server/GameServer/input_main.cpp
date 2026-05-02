#include "stdafx.h"
#include "ecs/systems/AffectSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/systems/SkillSystem.hpp"


#include "constants.h"
#include "config.h"
#include "utils.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "buffer_manager.h"
#include "packet.h"
#include "protocol.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "item.h"
#include "item_manager.h"
#include "cmd.h"
#include "shop.h"
#include "shop_manager.h"
#include "safebox.h"
#include "regen.h"
#include "battle.h"
#include "exchange.h"
#include "questmanager.h"
#include "profiler.h"
#include "messenger_manager.h"
#include "party.h"
#include "p2p.h"
#include "affect.h"
#include "guild.h"
#include "guild_manager.h"
#include "log.h"
#include "banword.h"
#include "empire_text_convert.h"
#include "unique_item.h"
#include "building.h"
#include "locale_service.h"
#include "gm.h"
#include "spam.h"
#include "ani.h"
#include "motion.h"
#include "OXEvent.h"
#include "locale_service.h"
#include "DragonSoul.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/EntityFactory.hpp"
#ifdef __NEWPET_SYSTEM__
#include "New_PetSystem.h"
#endif
#include "belt_inventory_helper.h" // @fixme119
#include "mount_inventory_helper.h"
#include "MountInventory.h"
#include "input.h"
#include "ecs/Registry.hpp"
#include "ecs/VIDRegistry.hpp"
#include "ecs/components/combat_components.hpp"
#include "ecs/components/dirty_components.hpp"
#include "ecs/components/movement_components.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/systems/PointSystem.hpp"

#ifdef ENABLE_SWITCHBOT
#include "new_switchbot.h"
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"
#endif
#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	#include "refine.h"
#endif

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
	#include "whisper_admin.h"
#endif
#ifdef __INGAME_WIKI__
#include "mob_manager.h"
#endif
#include <common/CommonDefines.h>

#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
static inline std::string MakeNameWithPrefix(LPCHARACTER ch)
{
	const char* name = ch ? ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data() : "";

	std::string out;
	if (ch)
		out = ch->GetItemOnTitlePrefix(); // std::string


	if (!out.empty() && out.back() != ' ')
		out.push_back(' ');

	out += name;
	return out;
}

#endif

#define ENABLE_CHECK_GHOSTMODE

#ifdef ENABLE_CHAT_LOGGING
static char	__escape_string[1024];
static char	__escape_string2[1024];
#endif
static int __deposit_limit()
{
	return (1000*10000); // 1Ãµ¸¸
}
#ifdef __SEND_TARGET_INFO__
void CInputMain::TargetInfoLoad(LPCHARACTER ch, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate TargetInfoLoad handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "CInputMain::PartyInvite.");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGTargetInfoLoad* p = (TPacketCGTargetInfoLoad*)c_pData;
	TPacketGCTargetInfo pInfo;
	pInfo.header = HEADER_GC_TARGET_INFO;
	static std::vector<LPITEM> s_vec_item;
	s_vec_item.clear();
	LPITEM pkInfoItem;
	LPCHARACTER m_pkChrTarget = CHARACTER_MANAGER::instance().Find(p->dwVID);

	if (!ch || !m_pkChrTarget)
		return;

	// if (m_pkChrTarget && (m_pkChrTarget->IsMonster() || ecs::PlayerRuntime::IsStone(AIHelpers::EcsOf(m_pkChrTarget))))
	// {
		// if (thecore_heart->pulse - (int) ch->GetLastTargetInfoPulse() < passes_per_sec * 3)
			// return;

		// ch->SetLastTargetInfoPulse(thecore_heart->pulse);

	if (ITEM_MANAGER::instance().CreateDropItemVector(m_pkChrTarget, ch, s_vec_item) && (m_pkChrTarget->IsMonster() || ecs::PlayerRuntime::IsStone(AIHelpers::EcsOf(m_pkChrTarget))))
	{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::  instance().CreateDropItemVector(");//INGAME_DEBUG_RAZOR93
#endif
		if (s_vec_item.size() == 0);
		else if (s_vec_item.size() == 1)
		{
			pkInfoItem = s_vec_item[0];
			pInfo.dwVID	= ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(m_pkChrTarget));
			pInfo.race = ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(m_pkChrTarget));
			pInfo.dwVnum = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, pkInfoItem));
			pInfo.count = ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, pkInfoItem));
			ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(&pInfo, sizeof(TPacketGCTargetInfo));
		}
		else
		{
			int iItemIdx = s_vec_item.size() - 1;
			while (iItemIdx >= 0)
			{
				pkInfoItem = s_vec_item[iItemIdx--];

				if (!pkInfoItem)
				{
					LOG_ERROR("pkInfoItem null in vector idx {}", iItemIdx + 1);
					continue;
				}

					pInfo.dwVID	= ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(m_pkChrTarget));
					pInfo.race = ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(m_pkChrTarget));
					pInfo.dwVnum = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, pkInfoItem));
					pInfo.count = ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, pkInfoItem));
					ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(&pInfo, sizeof(TPacketGCTargetInfo));
			}
		}
	}
	// }
}
#endif
void SendBlockChatInfo(LPCHARACTER ch, int sec)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::  SendBlockChatInfo(");//INGAME_DEBUG_RAZOR93
#endif
	if (sec <= 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 473, "");
#endif
		return;
	}

#ifdef TEXTS_IMPROVEMENT
	int32_t hour = sec / 3600;
	sec -= hour * 3600;
	int32_t min = (sec / 60);
	sec -= min * 60;
	if (hour > 0 && min > 0) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 475, "%d#%d#%d", hour, min, sec);
	}
	else if (hour > 0 && min == 0) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 476, "%d#%d", hour, sec);
	}
	else if (hour == 0 && min > 0) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 477, "%d#%d", min, sec);
	}
	else {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 478, "%d", sec);
	}
#endif
}

EVENTINFO(spam_event_info)
{
	char host[MAX_HOST_LENGTH+1];

	spam_event_info()
	{
		::memset( host, 0, MAX_HOST_LENGTH+1 );
	}
};

typedef std::unordered_map<std::string, std::pair<unsigned int, LPEVENT> > spam_score_of_ip_t;
spam_score_of_ip_t spam_score_of_ip;

EVENTFUNC(block_chat_by_ip_event)
{
	const auto info = dynamic_cast<spam_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("block_chat_by_ip_event> <Factor> Null pointer");
		return 0;
	}

	const char * host = info->host;

	auto it = spam_score_of_ip.find(host);

	if (it != spam_score_of_ip.end())
	{
		it->second.first = 0;
		it->second.second = nullptr;
	}

	return 0;
}

bool SpamBlockCheck(LPCHARACTER ch, const char* const buf, const size_t buflen)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::  bool SpamBlockCheck(LPCHARACTER ch, const char* const buf, const size_t buflen)(");//INGAME_DEBUG_RAZOR93
#endif
	if (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)) < g_iSpamBlockMaxLevel)
	{
		auto it = spam_score_of_ip.find(ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetHostName());

		if (it == spam_score_of_ip.end())
		{
			spam_score_of_ip.insert(std::make_pair(ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetHostName(), std::make_pair(0, (LPEVENT)nullptr)));
			it = spam_score_of_ip.find(ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetHostName());
		}

		if (it->second.second)
		{
			SendBlockChatInfo(ch, event_time(it->second.second) / passes_per_sec);
			return true;
		}

		unsigned int score;
		const char * word = SpamManager::instance().GetSpamScore(buf, buflen, score);

		it->second.first += score;

		if (word)
			LOG_INFO("SPAM_SCORE: {} text: {} score: {} total: {} word: {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), buf, score, it->second.first, word);

		if (it->second.first >= g_uiSpamBlockScore)
		{
			spam_event_info* info = AllocEventInfo<spam_event_info>();
			strlcpy(info->host, ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetHostName(), sizeof(info->host));

			it->second.second = event_create(block_chat_by_ip_event, info, PASSES_PER_SEC(g_uiSpamBlockDuration));
			LOG_INFO("SPAM_IP: {} for {} seconds", info->host, g_uiSpamBlockDuration);

			LogManager::instance().CharLog(ch, 0, "SPAM", word);

			SendBlockChatInfo(ch, event_time(it->second.second) / passes_per_sec);

			return true;
		}
	}

	return false;
}

enum
{
	TEXT_TAG_PLAIN,
	TEXT_TAG_TAG, // ||
	TEXT_TAG_COLOR, // |cffffffff
	TEXT_TAG_HYPERLINK_START, // |H
	TEXT_TAG_HYPERLINK_END, // |h ex) |Hitem:1234:1:1:1|h
	TEXT_TAG_RESTORE_COLOR,
};

int GetTextTag(const char * src, int maxLen, int & tagLen, std::string & extraInfo)
{
	tagLen = 1;

	if (maxLen < 2 || *src != '|')
		return TEXT_TAG_PLAIN;

	const char * cur = ++src;

	if (*cur == '|') // ||´Â |·Î Ç¥½ÃÇÑ´Ù.
	{
		tagLen = 2;
		return TEXT_TAG_TAG;
	}
	else if (*cur == 'c') // color |cffffffffblahblah|r
	{
		tagLen = 2;
		return TEXT_TAG_COLOR;
	}
	else if (*cur == 'H') // hyperlink |Hitem:10000:0:0:0:0|h[ÀÌ¸§]|h
	{
		tagLen = 2;
		return TEXT_TAG_HYPERLINK_START;
	}
	else if (*cur == 'h') // end of hyperlink
	{
		tagLen = 2;
		return TEXT_TAG_HYPERLINK_END;
	}

	return TEXT_TAG_PLAIN;
}

void GetTextTagInfo(const char * src, int src_len, int & hyperlinks, bool & colored)
{
	colored = false;
	hyperlinks = 0;

	int len;
	std::string extraInfo;

	for (int i = 0; i < src_len;)
	{
		int tag = GetTextTag(&src[i], src_len - i, len, extraInfo);

		if (tag == TEXT_TAG_HYPERLINK_START)
			++hyperlinks;

		if (tag == TEXT_TAG_COLOR)
			colored = true;

		i += len;
	}
}

int ProcessTextTag(LPCHARACTER ch, const char * c_pszText, uint64_t len)
{
	//°³ÀÎ»óÁ¡Áß¿¡ ±Ý°­°æÀ» »ç¿ëÇÒ °æ¿ì
	//0 : Á¤»óÀûÀ¸·Î »ç¿ë
	//1 : ±Ý°­°æ ºÎÁ·
	//2 : ±Ý°­°æÀÌ ÀÖÀ¸³ª, °³ÀÎ»óÁ¡¿¡¼­ »ç¿ëÁß
	//3 : ±âÅ¸
	//4 : ¿¡·¯
	int hyperlinks;
	bool colored;

	GetTextTagInfo(c_pszText, len, hyperlinks, colored);

	if (colored == true && hyperlinks == 0)
		return 4;

#ifdef ENABLE_NEWSTUFF
	if (g_bDisablePrismNeed)
		return 0;
#endif
	int nPrismCount = ch->CountSpecifyItem(ITEM_PRISM);

	if (nPrismCount < hyperlinks)
		return 1;


	if (!ch->GetMyShop())
	{
		ch->RemoveSpecifyItem(ITEM_PRISM, hyperlinks);
		return 0;
	} else
	{
		int sellingNumber = ch->GetMyShop()->GetNumberByVnum(ITEM_PRISM);
		if(nPrismCount - sellingNumber < hyperlinks)
		{
			return 2;
		} else
		{
			ch->RemoveSpecifyItem(ITEM_PRISM, hyperlinks);
			return 0;
		}
	}

	return 4;
}

int CInputMain::Whisper(LPCHARACTER ch, const char * data, uint64_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Whisper handler ECS
// DUAL-PATH: legacy only during migration window
	const auto pinfo = reinterpret_cast<const TPacketCGWhisper*>(data);

	if (uiBytes < pinfo->wSize)
		return -1;

	int iExtraLen = pinfo->wSize - sizeof(TPacketCGWhisper);

	if (iExtraLen < 0)
	{
		LOG_ERROR("invalid packet length (len {} size {} buffer {})", iExtraLen, pinfo->wSize, uiBytes);
		ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->SetPhase(PHASE_CLOSE);
		return -1;
	}

	if (AffectSystem::FindAffect(AIHelpers::EcsOf(ch), AFFECT_BLOCK_CHAT))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 639, "");
#endif
		return (iExtraLen);
	}

	LPCHARACTER pkChr = CHARACTER_MANAGER::instance().FindPC(pinfo->szNameTo);

	if (pkChr == ch)
		return (iExtraLen);

	LPDESC pkDesc = nullptr;

	uint8_t bOpponentEmpire = 0;

	if (test_server)
	{
		if (!pkChr)
			LOG_INFO("Whisper to {}({}) from {}", "Null", pinfo->szNameTo, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
		else
			LOG_INFO("Whisper to {}({}) from {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pkChr)).data(), pinfo->szNameTo, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
	}

	if (ch->IsBlockMode(BLOCK_WHISPER))
	{
		if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
		{
			TPacketGCWhisper pack;
			pack.bHeader = HEADER_GC_WHISPER;
			pack.bType = WHISPER_TYPE_SENDER_BLOCKED;
			pack.wSize = sizeof(TPacketGCWhisper);
			strlcpy(pack.szNameFrom, pinfo->szNameTo, sizeof(pack.szNameFrom));
			ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(&pack, sizeof(pack));
		}

		return iExtraLen;
	}

	if (!pkChr)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find(pinfo->szNameTo);

		if (pkCCI)
		{
			pkDesc = pkCCI->pkDesc;
			pkDesc->SetRelay(pinfo->szNameTo);
			bOpponentEmpire = pkCCI->bEmpire;

			if (test_server)
				LOG_INFO("Whisper to {} from {} (Channel {} Mapindex {})", "Null", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), pkCCI->bChannel, pkCCI->lMapIndex);
		}
	}
	else
	{
		pkDesc = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChr));
		bOpponentEmpire = ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(pkChr));
	}

	if (!pkDesc)
	{
		if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
		{
#if defined(BL_OFFLINE_MESSAGE)
			const uint8_t bDelay = 10;
			char msg[64];
			if (get_dword_time() - ch->GetLastOfflinePMTime() > bDelay * 1000)
			{
				char buf[CHAT_MAX_LEN + 1];
				strlcpy(buf, data + sizeof(TPacketCGWhisper), MIN(iExtraLen + 1, sizeof(buf)));
				const uint64_t buflen = strlen(buf);
				CBanwordManager::instance().ConvertString(buf, buflen);
				int processReturn = ProcessTextTag(ch, buf, buflen);

				if (0 != processReturn)
				{
					TItemTable* pTable = ITEM_MANAGER::instance().GetTable(ITEM_PRISM);
					if (pTable) {
#ifdef ENABLE_MULTI_NAMES
						int Lang = ch && ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetLanguage() : 0;
#endif
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 823, "%s",
#ifdef ENABLE_MULTI_NAMES
						pTable->szLocaleName[Lang]
#else
						pTable->szLocaleName
#endif
						);
#endif
					}

					return (iExtraLen);
				}

				if (buflen > 0)
				{
					ch->SendOfflineMessage(pinfo->szNameTo, buf);
					snprintf(msg, sizeof(msg), "An offline message has been sent.");
				}
				else
					return (iExtraLen);
			}
			else
			{
				snprintf(msg, sizeof(msg), "You have to wait %d seconds for send offline message.", bDelay);
			}

			TPacketGCWhisper pack;
			int len = MIN(CHAT_MAX_LEN, strlen(msg) + 1);
			pack.bHeader = HEADER_GC_WHISPER;
			pack.wSize = static_cast<uint16_t>(sizeof(TPacketGCWhisper) + len);
			pack.bType = WHISPER_TYPE_OFFLINE;
			strlcpy(pack.szNameFrom, pinfo->szNameTo, sizeof(pack.szNameFrom));

			TEMP_BUFFER buf;
			buf.write(&pack, sizeof(TPacketGCWhisper));
			buf.write(msg, len);
			ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(buf.read_peek(), buf.size());

#else
			TPacketGCWhisper pack;
			pack.bHeader = HEADER_GC_WHISPER;
			pack.bType = WHISPER_TYPE_NOT_EXIST;
			pack.wSize = sizeof(TPacketGCWhisper);
			strlcpy(pack.szNameFrom, pinfo->szNameTo, sizeof(pack.szNameFrom));
			ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(&pack, sizeof(TPacketGCWhisper));
			LOG_INFO("WHISPER: no player");
#endif
		}
	}
	else
	{
		if (ch->IsBlockMode(BLOCK_WHISPER))
		{
			if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
			{
				TPacketGCWhisper pack;
				pack.bHeader = HEADER_GC_WHISPER;
				pack.bType = WHISPER_TYPE_SENDER_BLOCKED;
				pack.wSize = sizeof(TPacketGCWhisper);
				strlcpy(pack.szNameFrom, pinfo->szNameTo, sizeof(pack.szNameFrom));
				ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(&pack, sizeof(pack));
			}
		}
		else if (pkChr && pkChr->IsBlockMode(BLOCK_WHISPER))
		{
			if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
			{
				TPacketGCWhisper pack;
				pack.bHeader = HEADER_GC_WHISPER;
				pack.bType = WHISPER_TYPE_TARGET_BLOCKED;
				pack.wSize = sizeof(TPacketGCWhisper);
				strlcpy(pack.szNameFrom, pinfo->szNameTo, sizeof(pack.szNameFrom));
				ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(&pack, sizeof(pack));
			}
		}
		else
		{
			uint8_t bType = WHISPER_TYPE_NORMAL;

			char buf[CHAT_MAX_LEN + 1];
			strlcpy(buf, data + sizeof(TPacketCGWhisper), MIN(iExtraLen + 1, sizeof(buf)));
			const uint64_t buflen = strlen(buf);

			if (true == SpamBlockCheck(ch, buf, buflen))
			{
				if (!pkChr)
				{
					CCI * pkCCI = P2P_MANAGER::instance().Find(pinfo->szNameTo);

					if (pkCCI)
					{
						pkDesc->SetRelay("");
					}
				}
				return iExtraLen;
			}

			CBanwordManager::instance().ConvertString(buf, buflen);

			if (g_bEmpireWhisper)
				if (!ch->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_LANGUAGE))
					if (!(pkChr && pkChr->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_LANGUAGE)))
						if (bOpponentEmpire != ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)) && ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)) && bOpponentEmpire // ¼­·Î Á¦±¹ÀÌ ´Ù¸£¸é¼­
								&& ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) == GM_PLAYER && gm_get_level(pinfo->szNameTo) == GM_PLAYER) // µÑ´Ù ÀÏ¹Ý ÇÃ·¹ÀÌ¾îÀÌ¸é
							// ÀÌ¸§ ¹Û¿¡ ¸ð¸£´Ï gm_get_level ÇÔ¼ö¸¦ »ç¿ë
						{
							if (!pkChr)
							{
								// ´Ù¸¥ ¼­¹ö¿¡ ÀÖÀ¸´Ï Á¦±¹ Ç¥½Ã¸¸ ÇÑ´Ù. bTypeÀÇ »óÀ§ 4ºñÆ®¸¦ Empire¹øÈ£·Î »ç¿ëÇÑ´Ù.
								bType = ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)) << 4;
							}
							else
							{
								ConvertEmpireText(ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)), buf, buflen, 10 + 2 * pkChr->GetSkillPower(SKILL_LANGUAGE1 + ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)) - 1)/*º¯È¯È®·ü*/);
							}
						}

			int processReturn = ProcessTextTag(ch, buf, buflen);
			if (0!=processReturn)
			{
				if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
				{
					TItemTable * pTable = ITEM_MANAGER::instance().GetTable(ITEM_PRISM);

					if (pTable)
					{
#ifdef ENABLE_MULTI_NAMES
						int Lang = ch && ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetLanguage() : 0;
#endif
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 823, "%s",
#ifdef ENABLE_MULTI_NAMES
						pTable->szLocaleName[Lang]
#else
						pTable->szLocaleName
#endif
						);
					}
				}

				// ¸±·¡ÀÌ »óÅÂÀÏ ¼ö ÀÖÀ¸¹Ç·Î ¸±·¡ÀÌ¸¦ Ç®¾îÁØ´Ù.
				pkDesc->SetRelay("");
				return (iExtraLen);
			}

			if (ch->IsGM())
				bType = (bType & 0xF0) | WHISPER_TYPE_GM;

			if (buflen > 0)
			{
				TPacketGCWhisper pack;

				pack.bHeader = HEADER_GC_WHISPER;
				pack.wSize = sizeof(TPacketGCWhisper) + buflen;
				pack.bType = bType;
				strlcpy(pack.szNameFrom, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), sizeof(pack.szNameFrom));
				// desc->BufferedPacketÀ» ÇÏÁö ¾Ê°í ¹öÆÛ¿¡ ½á¾ßÇÏ´Â ÀÌÀ¯´Â
				// P2P relayµÇ¾î ÆÐÅ¶ÀÌ Ä¸½¶È­ µÉ ¼ö ÀÖ±â ¶§¹®ÀÌ´Ù.
				TEMP_BUFFER tmpbuf;

				tmpbuf.write(&pack, sizeof(pack));
				tmpbuf.write(buf, buflen);

				pkDesc->Packet(tmpbuf.read_peek(), tmpbuf.size());

				// @warme006
				// LOG_INFO(0, "WHISPER: %s -> %s : %s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), pinfo->szNameTo, buf);
#ifdef ENABLE_CHAT_LOGGING
				if (ch->IsGM())
				{
					LogManager::instance().EscapeString(__escape_string, sizeof(__escape_string), buf, buflen);
					LogManager::instance().EscapeString(__escape_string2, sizeof(__escape_string2), pinfo->szNameTo, sizeof(pack.szNameFrom));
					LogManager::instance().ChatLog(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), 0, __escape_string2, "WHISPER", __escape_string, ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetHostName() : "");
				}
#endif
			}
		}
	}
	if(pkDesc)
		pkDesc->SetRelay("");

	return (iExtraLen);
}

struct RawPacketToCharacterFunc
{

	const void * m_buf;
	int	m_buf_len;

	RawPacketToCharacterFunc(const void * buf, int buf_len) : m_buf(buf), m_buf_len(buf_len)
	{
	}

	void operator () (LPCHARACTER c)
	{
		if (!ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(c)))
			return;

		ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(c))->Packet(m_buf, m_buf_len);
	}
};

struct FEmpireChatPacket
{
	packet_chat& p;
	const char* orig_msg;
	int orig_len;
	char converted_msg[CHAT_MAX_LEN+1];

	uint8_t bEmpire;
	int iMapIndex;
	int namelen;

	FEmpireChatPacket(packet_chat& p, const char* chat_msg, int len, uint8_t bEmpire, int iMapIndex, int iNameLen)
		: p(p), orig_msg(chat_msg), orig_len(len), bEmpire(bEmpire), iMapIndex(iMapIndex), namelen(iNameLen)
	{
		memset( converted_msg, 0, sizeof(converted_msg) );
	}

	void operator () (LPDESC d)
	{
		if (!d->GetCharacter())
			return;

		if (ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(d->GetCharacter())) != iMapIndex)
			return;

		d->BufferedPacket(&p, sizeof(packet_chat));

		if (d->GetEmpire() == bEmpire ||
			bEmpire == 0 ||
			ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(d->GetCharacter())) > GM_PLAYER ||
			d->GetCharacter()->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_LANGUAGE))
		{
			d->Packet(orig_msg, orig_len);
		}
		else
		{
			// »ç¶÷¸¶´Ù ½ºÅ³·¹º§ÀÌ ´Ù¸£´Ï ¸Å¹ø ÇØ¾ßÇÕ´Ï´Ù
			uint64_t len = strlcpy(converted_msg, orig_msg, sizeof(converted_msg));

			if (len >= sizeof(converted_msg))
				len = sizeof(converted_msg) - 1;

			ConvertEmpireText(bEmpire, converted_msg + namelen, len - namelen, 10 + 2 * d->GetCharacter()->GetSkillPower(SKILL_LANGUAGE1 + bEmpire - 1));
			d->Packet(converted_msg, orig_len);
		}
	}
};

struct FYmirChatPacket
{
	packet_chat& packet;
	const char* m_szChat;
	uint64_t m_lenChat;
	const char* m_szName;

	int m_iMapIndex;
	uint8_t m_bEmpire;
	bool m_ring;

	char m_orig_msg[CHAT_MAX_LEN+1];
	int m_len_orig_msg;
	char m_conv_msg[CHAT_MAX_LEN+1];
	int m_len_conv_msg;

	FYmirChatPacket(packet_chat& p, const char* chat, uint64_t len_chat, const char* name, uint64_t len_name, int iMapIndex, uint8_t empire, bool ring)
		: packet(p),
		m_szChat(chat), m_lenChat(len_chat),
		m_szName(name),
		m_iMapIndex(iMapIndex), m_bEmpire(empire),
		m_ring(ring)
	{
		m_len_orig_msg = snprintf(m_orig_msg, sizeof(m_orig_msg), "%s : %s", m_szName, m_szChat) + 1; // ³Î ¹®ÀÚ Æ÷ÇÔ

		if (m_len_orig_msg < 0 || m_len_orig_msg >= (int) sizeof(m_orig_msg))
			m_len_orig_msg = sizeof(m_orig_msg) - 1;

		m_len_conv_msg = snprintf(m_conv_msg, sizeof(m_conv_msg), "??? : %s", m_szChat) + 1; // ³Î ¹®ÀÚ ¹ÌÆ÷ÇÔ

		if (m_len_conv_msg < 0 || m_len_conv_msg >= (int) sizeof(m_conv_msg))
			m_len_conv_msg = sizeof(m_conv_msg) - 1;

		ConvertEmpireText(m_bEmpire, m_conv_msg + 6, m_len_conv_msg - 6, 10); // 6Àº "??? : "ÀÇ ±æÀÌ
	}

	void operator() (LPDESC d)
	{
		if (!d->GetCharacter())
			return;

		if (ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(d->GetCharacter())) != m_iMapIndex)
			return;

		if (m_ring ||
			d->GetEmpire() == m_bEmpire ||
			ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(d->GetCharacter())) > GM_PLAYER ||
			d->GetCharacter()->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_LANGUAGE))
		{
			packet.size = m_len_orig_msg + sizeof(TPacketGCChat);

			d->BufferedPacket(&packet, sizeof(packet_chat));
			d->Packet(m_orig_msg, m_len_orig_msg);
		}
		else
		{
			packet.size = m_len_conv_msg + sizeof(TPacketGCChat);

			d->BufferedPacket(&packet, sizeof(packet_chat));
			d->Packet(m_conv_msg, m_len_conv_msg);
		}
	}
};

#ifdef __NEWPET_SYSTEM__
void CInputMain::BraveRequestPetName(LPCHARACTER ch, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate BraveRequestPetName handler ECS
// DUAL-PATH: legacy only during migration window
	if (!ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))) { return; }
	int vid = ch->GetEggVid();
	if (vid == 0) { return; }

	TPacketCGRequestPetName* p = (TPacketCGRequestPetName*)c_pData;

	if (ecs::PointSystem::GetGold(AIHelpers::EcsOf(ch)) < 100000)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 768, "%d", 100000);
#endif
	}

	if (ch->CountSpecifyItem(vid) > 0 && check_name(p->petname) != 0) {
#ifdef ENABLE_NEW_PET_EDITS
		{
			char szQuery[256] = {0};
			snprintf(szQuery, sizeof(szQuery), "SELECT id FROM player.new_petsystem%s WHERE name='%s';", get_table_postfix(), p->petname);
			std::unique_ptr<SQLMsg> pRes(DBManager::instance().DirectQuery(szQuery));
			int iRes = pRes->Get()->uiNumRows;
			if (iRes > 0) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 50, "");
#endif
				return;
			}
		}
#endif

		DBManager::instance().SendMoneyLog(MONEY_LOG_QUEST, ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), -100000);
		ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_GOLD, -100000, true);
		ch->RemoveSpecifyItem(vid, 1);
		LPITEM item = ch->AutoGiveItem(vid + 300, 1);
#ifdef ENABLE_NEW_PET_EDITS
		int tmpskill[4] = { -1, -1, -1, -1 };
#else
		int tmpskill[4] = { 0, 0, 0, 0 };
		int tmpslot = number(1, 3);
		for (int i = 0; i < 4; ++i)
		{
			if (i > tmpslot - 1)
				tmpskill[i] = -1;
		}
#endif
		int tmpdur = 3 * 24 * 60;
		char szQuery1[1024];
		int hp[] = {30, 35, 40};
		int mostri[] = {10, 15, 20};
		int medi[] = {10, 15, 20};
		snprintf(szQuery1, sizeof(szQuery1), "INSERT INTO new_petsystem VALUES(%u,'%s', 1, 0, 0, 0, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, 0"
#ifdef ENABLE_NEW_PET_EDITS
		", %lld"
#endif
		")", ItemSystem::GetItemID(EntityFactory::CreateItemEntity(g_registry, item)), p->petname, hp[number(0, 2)], mostri[number(0, 2)], medi[number(0, 2)], tmpskill[0], 0, tmpskill[1], 0, tmpskill[2], 0, tmpskill[3], 0, tmpdur, tmpdur, get_global_time());
		std::unique_ptr<SQLMsg> pmsg2(DBManager::instance().DirectQuery(szQuery1));
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 769, "");
#endif
	}
#ifdef TEXTS_IMPROVEMENT
	else  {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 770, "");
	}
#endif
}
#endif

int CInputMain::Chat(LPCHARACTER ch, const char * data, uint32_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Chat handler ECS
// DUAL-PATH: legacy only during migration window
	//if (ch->IsFakePlayer())
	//	return false;
	auto pinfo = reinterpret_cast<const TPacketCGChat*>(data);

	if (uiBytes < pinfo->size)
		return -1;

	const int iExtraLen = pinfo->size - sizeof(TPacketCGChat);

	if (iExtraLen < 0)
	{
		LOG_ERROR("invalid packet length (len {} size {} buffer {})", iExtraLen, pinfo->size, uiBytes);
		ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->SetPhase(PHASE_CLOSE);
		return -1;
	}

	char buf[CHAT_MAX_LEN - (CHARACTER_NAME_MAX_LEN + 3) + 1];
	strlcpy(buf, data + sizeof(TPacketCGChat), MIN(iExtraLen + 1, sizeof(buf)));
	const uint64_t buflen = strlen(buf);

	if (buflen > 1 && *buf == '/')
	{
#if defined(__ENABLE_NEW_OFFLINESHOP__) || defined(ENABLE_NEW_OFFLINESHOP)

		// /shplink [extra szoveg...]  (alias: /shoplink)
		{
			const char* pCmd = buf + 1;

			auto IsCmd = [&](const char* name) -> bool {
				const size_t n = strlen(name);
				if (strncasecmp(pCmd, name, n))
					return false;

				const char next = pCmd[n];
				return (next == '\0' || isspace((unsigned char)next));
				};

			if (IsCmd("shplink") || IsCmd("shoplink"))
			{
				offlineshop::CShop* pkShop = offlineshop::GetManager().GetShopByOwnerID(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
				if (!pkShop)
				{
					ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Nincs nyitott offline boltod./ You don't have open shop.");
					return iExtraLen;
				}

				// shout rules
				if (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)) < g_iShoutLimitLevel)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 411, "%d", g_iShoutLimitLevel);
#else
					ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Nem megfelelo szint a kiabalashoz.");
#endif
					return iExtraLen;
				}
				if (thecore_heart->pulse - (int)ch->GetLastShoutPulse() < passes_per_sec * 15)
					return iExtraLen;

				ch->SetLastShoutPulse(thecore_heart->pulse);


				const char* pExtra = pCmd;
				if (!strncasecmp(pCmd, "shplink", 6))
					pExtra = pCmd + 6;
				else
					pExtra = pCmd + 8; // "shoplink"

				while (*pExtra && isspace((unsigned char)*pExtra))
					++pExtra;

				char extra[CHAT_MAX_LEN];
				strlcpy(extra, pExtra, sizeof(extra));


				for (size_t i = 0; extra[i]; ++i)
				{
					if (extra[i] == '\r' || extra[i] == '\n' || extra[i] == '\t')
						extra[i] = ' ';
				}


				char shopName[256];
				strlcpy(shopName, pkShop->GetName(), sizeof(shopName));
				for (size_t i = 0; shopName[i]; ++i)
				{
					if (shopName[i] == '|' || shopName[i] == '\r' || shopName[i] == '\n' || shopName[i] == '\t')
						shopName[i] = ' ';
				}


				uint32_t linkVnum = 50300; // fallback
				if (pkShop->GetItems() && !pkShop->GetItems()->empty())
				{
					TItemTable* pTbl = nullptr;
					if ((*pkShop->GetItems())[0].GetTable(&pTbl) && pTbl)
						linkVnum = pTbl->dwVnum;
				}


				const char* MAGENTA = "|cFFFF00FF";
				const char* LIGHT_GREEN = "|cFF66FF66";

				char body[CHAT_MAX_LEN];


				if (extra[0])
				{
					snprintf(body, sizeof(body),
						"%sOfflineShop link: |r"
						"%s|Hitem:%x:%x:%x:%x:%x:%x|h[%s]|h|r %s",
						MAGENTA,
						LIGHT_GREEN,
						(unsigned)linkVnum,
						0u,
						(unsigned)ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)),   // socket0 = OWNER_ID
						(unsigned)0x0BADF00D,          // socket1 = SENTINEL
						0u,
						0u,
						shopName,
						extra);
				}
				else
				{
					snprintf(body, sizeof(body),
						"%sOfflineShop link: |r"
						"%s|Hitem:%x:%x:%x:%x:%x:%x|h[%s]|h|r",
						MAGENTA,
						LIGHT_GREEN,
						(unsigned)linkVnum,
						0u,
						(unsigned)ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)),
						(unsigned)0x0BADF00D,
						0u,
						0u,
						shopName);
				}

				char shoutbuf[CHAT_MAX_LEN + 1];
#ifdef ENABLE_MULTI_LANGUAGE
				std::string langName = ch->GetLang();
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
				const std::string nameWithPrefix = MakeNameWithPrefix(ch);

				snprintf(shoutbuf, sizeof(shoutbuf),
					"|L%s|l|E%d|e %s : %s",
					langName.c_str(), ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)), nameWithPrefix.c_str(), body);

#else
				snprintf(shoutbuf, sizeof(shoutbuf),
					"|L%s|l|E%d|e %s : %s",
					langName.c_str(), ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), body);
#endif
#else
				snprintf(shoutbuf, sizeof(shoutbuf), "%s : %s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), body);
#endif

				TPacketGGShout p;
				p.bHeader = HEADER_GG_SHOUT;
				p.bEmpire = ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch));
				strlcpy(p.szText, shoutbuf, sizeof(p.szText));

				P2P_MANAGER::instance().Send(&p, sizeof(p));
				SendShout(shoutbuf, ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)));
#ifdef ENABLE_BATTLE_PASS
				if (uint8_t bBattlePassId = ch->GetBattlePassId())
				{
					uint32_t dwCount, dwNotUsed;
					if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COUNTER_CHAT, &dwNotUsed, &dwCount))
					{
						if (!ch->IsCompletedMission(COUNTER_CHAT))
						{
							if (ch->GetMissionProgress(COUNTER_CHAT, bBattlePassId) < dwCount)
								ch->UpdateMissionProgress(COUNTER_CHAT, bBattlePassId, 1, dwCount);
						}
					}
				}
#endif
				return iExtraLen;
			}
		}

#endif

	interpret_command(ch, buf + 1, buflen - 1);
	return iExtraLen;
	}




/* 	if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) == GM_PLAYER && ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)) == 113)//OX mapon chat letiltva//
	{
		return iExtraLen;
	} */

	// Ã¤ÆÃ ±ÝÁö Affect Ã³¸®
	const CAffect* pAffect = AffectSystem::FindAffect(AIHelpers::EcsOf(ch), AFFECT_BLOCK_CHAT);

	if (pAffect != nullptr)
	{
		SendBlockChatInfo(ch, pAffect->lDuration);
		return iExtraLen;
	}

	if (true == SpamBlockCheck(ch, buf, buflen))
	{
		return iExtraLen;
	}

	// @fixme133 begin
	CBanwordManager::instance().ConvertString(buf, buflen);

	int processReturn = ProcessTextTag(ch, buf, buflen);
	if (0!=processReturn)
	{
#ifdef TEXTS_IMPROVEMENT
		const TItemTable* pTable = ITEM_MANAGER::instance().GetTable(ITEM_PRISM);
		if (nullptr != pTable)
		{
#ifdef ENABLE_MULTI_NAMES
			int lang = 0;
			if (ch) {
				LPDESC desc = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));
				lang = desc != nullptr ? desc->GetLanguage() : 0;
			}
#endif
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 642, "%s",
#ifdef ENABLE_MULTI_NAMES
			pTable->szLocaleName[lang]
#else
			pTable->szLocaleName
#endif
			);
		}
#endif
		return iExtraLen;
	}
	// @fixme133 end

	char chatbuf[CHAT_MAX_LEN + 1];
	//static const char* colorbuf[] = {"|cFFffa200|H|h[Staff]|h|r", "|cFFff0000|H|h[Shinsoo]|h|r", "|cFFffc700|H|h[Chunjo]|h|r", "|cFF000bff|H|h[Jinno]|h|r"};
#ifdef ENABLE_MULTI_LANGUAGE
	int len;
	std::string langName = ch->GetLang();
	if (pinfo->type == CHAT_TYPE_SHOUT) {

#ifdef ENABLE_EVENT_QUIZ_RAZOR93
		do
		{
			quest::CQuestManager& qm = quest::CQuestManager::instance();
			// csak glob?is (SHOUT) chat
			const uint8_t chatType = pinfo->type; // vagy: pinfo->bType
			if (chatType != CHAT_TYPE_SHOUT)
				break;

			// Event akt??
			if (qm.GetEventFlag("quiz_active") != 1)
				break;

			// Trim
			char* p = buf;
			while (*p == ' ' || *p == '\t') ++p;
			char* q = p + strlen(p);
			while (q > p && (q[-1] == ' ' || q[-1] == '\t' || q[-1] == '\r' || q[-1] == '\n')) --q;
			*q = '\0';
			if (*p == '\0') break;

			// tiszta integer (opcion?is +/-)
			const char* s = p;
			if (*s == '+' || *s == '-') ++s;
			if (*s == '\0') break;
			bool numeric = true;
			for (; *s; ++s)
				if (*s < '0' || *s > '9') { numeric = false; break; }
			if (!numeric) break;

			long long typed = strtoll(p, nullptr, 10);
			int  answer = qm.GetEventFlag("quiz_answer");
			if (typed != (long long)answer)
				break;

			// race guard + lez??
			if (qm.GetEventFlag("quiz_active") != 1)
				break;
			qm.SetEventFlag("quiz_active", 0);

			// Jutalom
			int vnum = qm.GetEventFlag("quiz_item");
			int count = qm.GetEventFlag("quiz_count");
			if (count <= 0) count = 1;

			ItemSystem::AutoGiveItemEcs(AIHelpers::EcsOf(ch), vnum, count);

			// T?gyn?
			const TItemTable* pTable = ITEM_MANAGER::instance().GetTable(vnum);
			const char* itemName = nullptr;
#ifdef ENABLE_MULTI_NAMES
			itemName = (pTable ? pTable->szLocaleName[0] : "item");
#else
			itemName = (pTable ? (pTable->szLocaleName ? pTable->szLocaleName : pTable->szName) : "item");
#endif

			// === Broadcast mindenkinek ===
#ifdef TEXTS_IMPROVEMENT

			//const DESC_MANAGER::DESC_SET& cset = DESC_MANAGER::instance().GetClientSet();
			//for (DESC_MANAGER::DESC_SET::const_iterator it = cset.begin(); it != cset.end(); ++it)
			//{
			//	LPDESC d = *it;
			//	LPCHARACTER rc;
			//	if (!d || !(rc = d->GetCharacter())) continue;
			//ecs::ChatSystem::SendNew(AIHelpers::EcsOf(rc), CHAT_TYPE_INFO, 2181,
			//	"%s#%d#",
			//	ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(),            // %s (winner)
			//	answer                   // %d (correct answer)
			//	//(itemName ? itemName : "item"), // %s
			//	//count                     // %d
			//);
			//	//
			//
			//}

			BroadcastNoticeNew(CHAT_TYPE_INFO, 0,0,2181,
				"%s#%d#",
				ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(),            // %s (winner)
				answer                   // %d (correct answer)
				//(itemName ? itemName : "item"), // %s
				//count                     // %d
			);
#else
			const char* C_BLUE = "|cFF0000FF";
			const char* C_RED = "|cFFFF0000";
			const char* C_GOLD = "|cFFFFD700";
			const char* C_GRN = "|cFF00FF00";
			const char* C_RST = "|r";

			char msg[256];
			snprintf(msg, sizeof(msg),
				"%s[Quiz]%s %s%s%s won! %sCorrect answer:%s %s%d%s. %sReward sent:%s %s%s%s x %s%d%s",
				C_BLUE, C_RST,
				C_RED, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), C_RST,
				C_GOLD, C_RST, C_GRN, answer, C_RST,
				C_GOLD, C_RST, C_BLUE, (itemName ? itemName : "item"), C_RST, C_GRN, count, C_RST);

			const DESC_MANAGER::DESC_SET& cset = DESC_MANAGER::instance().GetClientSet();
			for (DESC_MANAGER::DESC_SET::const_iterator it = cset.begin(); it != cset.end(); ++it)
			{
				LPDESC d = *it;
				if (!d || !d->GetCharacter()) continue;
				ecs::ChatSystem::Send(AIHelpers::EcsOf(d->GetCharacter()), CHAT_TYPE_INFO, "%s", msg);
			}
#endif

			qm.SetEventFlag("quiz_answer", 0);
			qm.SetEventFlag("quiz_item", 0);
			qm.SetEventFlag("quiz_count", 0);
			qm.SetEventFlag("quiz_active", 0);

			qm.RequestSetEventFlag("quiz_answer", 0);
			qm.RequestSetEventFlag("quiz_item", 0);
			qm.RequestSetEventFlag("quiz_count", 0);
			qm.RequestSetEventFlag("quiz_active", 0);

		} while (0);
#endif // ENABLE_EVENT_QUIZ_RAZOR93
#ifdef ENABLE_FAKE_SHOP_HEADER
		const char* mountColor = "";
		char mountTitleWithCount[64];
		int count = ch->GetMountCount();

		// 80 fölött arany
		if (count >= 80)
		{
			mountColor = "|cFFFFFF00";  // arany
			snprintf(mountTitleWithCount, sizeof(mountTitleWithCount), "[Mount Emperor (%d)]", count);
		}
		else
		{

			mountColor = "";

			if (count >= 70)
				snprintf(mountTitleWithCount, sizeof(mountTitleWithCount), "[Mount Conqueror (%d)]", count);
			else if (count >= 60)
				snprintf(mountTitleWithCount, sizeof(mountTitleWithCount), "[Mount Champion (%d)]", count);
			else if (count >= 50)
				snprintf(mountTitleWithCount, sizeof(mountTitleWithCount), "[Mount Master (%d)]", count);
			else if (count >= 40)
				snprintf(mountTitleWithCount, sizeof(mountTitleWithCount), "[Mount Overlord (%d)]", count);
			else if (count >= 30)
				snprintf(mountTitleWithCount, sizeof(mountTitleWithCount), "[Mount Lord (%d)]", count);
			else if (count >= 20)
				snprintf(mountTitleWithCount, sizeof(mountTitleWithCount), "[Mount Slayer (%d)]", count);
			else if (count >= 15)
				snprintf(mountTitleWithCount, sizeof(mountTitleWithCount), "[Mount Warrior (%d)]", count);
			else if (count >= 10)
				snprintf(mountTitleWithCount, sizeof(mountTitleWithCount), "[Mount Hunter (%d)]", count);
			else if (count >= 5)
				snprintf(mountTitleWithCount, sizeof(mountTitleWithCount), "[Mount Collector (%d)]", count);
			else
				snprintf(mountTitleWithCount, sizeof(mountTitleWithCount), "[New Rider (%d)]", count);
		}


		const std::string prefix = ch->GetItemOnTitlePrefix();
		const char* name = ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data();

		len = snprintf(chatbuf, sizeof(chatbuf),
			"|L%s|l|E%d|e |Hmsg:%s|h%s%s |h|r %s%s: %s",
			langName.c_str(),
			ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)),
			name,               // whisper target
			prefix.c_str(),
			name,
			mountColor ? mountColor : "",
			mountTitleWithCount ? mountTitleWithCount : "",
			buf ? buf : ""
		);


#else
		len = snprintf(chatbuf, sizeof(chatbuf),
			"|L%s|l|E%d|e |Hmsg:%s|h%s [PM]|h|r : %s",
			langName.c_str(),
			ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)),
			ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(),
			ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(),
			buf);
#endif


	} else {
//#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
		const std::string nameWithPrefix = MakeNameWithPrefix(ch);

		len = snprintf(chatbuf, sizeof(chatbuf), "|L%s|l|E%d|e %s : %s",
			langName.c_str(), ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)), nameWithPrefix.c_str(), buf);

// else
//		len = snprintf(chatbuf, sizeof(chatbuf), "|L%s|l %s %s : %s", langName.c_str(), (ch->IsGM()?colorbuf[0]:colorbuf[MINMAX(0, ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)), 3)]), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), buf);
		//len = snprintf(chatbuf, sizeof(chatbuf), "|L%s|l|E%d|e %s : %s", langName.c_str(), ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), buf);
//#endif
	}
#else
	int len = snprintf(chatbuf, sizeof(chatbuf), "%s %s : %s", (ch->IsGM()?colorbuf[0]:colorbuf[MINMAX(0, ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)), 3)]), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(),buf);
#endif

	if (CHAT_TYPE_SHOUT == pinfo->type)
	{
		LogManager::instance().ShoutLog(g_bChannel, ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)), chatbuf);
	}

	if (len < 0 || len >= (int) sizeof(chatbuf))
		len = sizeof(chatbuf) - 1;

	if (pinfo->type == CHAT_TYPE_SHOUT)
	{
		// const int SHOUT_LIMIT_LEVEL = 15;

		if (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)) < g_iShoutLimitLevel) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 411, "%d", g_iShoutLimitLevel);
#endif
			return (iExtraLen);
		}

		// if (thecore_heart->pulse - (int) ch->GetLastShoutPulse() < passes_per_sec * g_iShoutLimitTime)
		if (thecore_heart->pulse - (int) ch->GetLastShoutPulse() < passes_per_sec * 15)
			return (iExtraLen);

		ch->SetLastShoutPulse(thecore_heart->pulse);

		TPacketGGShout p;

		p.bHeader = HEADER_GG_SHOUT;
		p.bEmpire = ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch));
		strlcpy(p.szText, chatbuf, sizeof(p.szText));

		P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGShout));

		SendShout(chatbuf, ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)));

#ifdef ENABLE_BATTLE_PASS
		uint8_t bBattlePassId = ch->GetBattlePassId();
		if(bBattlePassId)
		{
			uint32_t dwCount, dwNotUsed;
			if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COUNTER_CHAT, &dwNotUsed, &dwCount))
			{
				if (!ch->IsCompletedMission(COUNTER_CHAT))
				{
					if(ch->GetMissionProgress(COUNTER_CHAT, bBattlePassId) < dwCount)
						ch->UpdateMissionProgress(COUNTER_CHAT, bBattlePassId, 1, dwCount);
				}
			}
		}

#endif

		return (iExtraLen);
	}

	TPacketGCChat pack_chat;

	pack_chat.header = HEADER_GC_CHAT;
	pack_chat.size = sizeof(TPacketGCChat) + len;
	pack_chat.type = pinfo->type;
	pack_chat.id = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(ch));

	switch (pinfo->type)
	{
		case CHAT_TYPE_TALKING:
			{
				const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();

				if (false)
				{
					std::for_each(c_ref_set.begin(), c_ref_set.end(),
							FYmirChatPacket(pack_chat,
								buf,
								strlen(buf),
								ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(),
								strlen(ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data()),
								ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)),
								ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)),
								ch->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_LANGUAGE)));
				}
				else
				{
					std::for_each(c_ref_set.begin(), c_ref_set.end(),
							FEmpireChatPacket(pack_chat,
								chatbuf,
								len,
								(ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER ||
								 ch->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_LANGUAGE)) ? 0 : ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)),
								ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), strlen(ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data())));
#ifdef ENABLE_CHAT_LOGGING
					if (ch->IsGM())
					{
						LogManager::instance().EscapeString(__escape_string, sizeof(__escape_string), chatbuf, len);
						LogManager::instance().ChatLog(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), 0, "", "NORMAL", __escape_string, ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetHostName() : "");
					}
#endif
				}
			}
			break;

		case CHAT_TYPE_PARTY:
			{
				if (!ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 485, "");
#endif
				if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
				{
					TEMP_BUFFER tbuf;

					tbuf.write(&pack_chat, sizeof(pack_chat));
					tbuf.write(chatbuf, len);

					RawPacketToCharacterFunc f(tbuf.read_peek(), tbuf.size());
					ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->ForEachOnlineMember(f);
#ifdef ENABLE_CHAT_LOGGING
					if (ch->IsGM())
					{
						LogManager::instance().EscapeString(__escape_string, sizeof(__escape_string), chatbuf, len);
						LogManager::instance().ChatLog(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->GetLeaderPID(), "", "PARTY", __escape_string, ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetHostName() : "");
					}
#endif
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 486, "");
				}
#endif
			}
			break;

		case CHAT_TYPE_GUILD:
			{
				if (ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch))) {
					ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch))->Chat(chatbuf);
#ifdef ENABLE_CHAT_LOGGING
					if (ch->IsGM())
					{
						LogManager::instance().EscapeString(__escape_string, sizeof(__escape_string), chatbuf, len);
						LogManager::instance().ChatLog(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch))->GetID(), ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch))->GetName(), "GUILD", __escape_string, ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetHostName() : "");
					}
#endif
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 271, "");
				}
#endif
			}
			break;

		default:
			LOG_ERROR("Unknown chat type {}", pinfo->type);
			break;
	}

	return (iExtraLen);
}

void CInputMain::ItemUse(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemUse handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::  CInputMain::ItemUse(");//INGAME_DEBUG_RAZOR93
#endif
	ch->UseItem(((struct command_item_use *) data)->Cell);
}

void CInputMain::ItemToItem(LPCHARACTER ch, const char * pcData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemToItem handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::ItemToItem(");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGItemUseToItem * p = (TPacketCGItemUseToItem *) pcData;
	if (ch)
		ch->UseItem(p->Cell, p->TargetCell);
}

void CInputMain::ItemDrop(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemDrop handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::ItemDrop");//INGAME_DEBUG_RAZOR93
#endif
	struct command_item_drop * pinfo = (struct command_item_drop *) data;
	if (!ch)
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR) {
		return;
	}
#endif

	// ¿¤Å©°¡ 0º¸´Ù Å©¸é ¿¤Å©¸¦ ¹ö¸®´Â °Í ÀÌ´Ù.
	if (pinfo->gold > 0)
		ch->DropGold(pinfo->gold);
	else
		ch->DropItem(pinfo->Cell);
}

void CInputMain::ItemDrop2(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemDrop2 handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::ItemDrop2");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGItemDrop2 * pinfo = (TPacketCGItemDrop2 *) data;
	if (!ch)
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR) {
		return;
	}
#endif

	if (pinfo->gold > 0)
		ch->DropGold(pinfo->gold);
	else
		ch->DropItem(pinfo->Cell, pinfo->count);
}

void CInputMain::ItemMove(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemMove handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::ItemMove");//INGAME_DEBUG_RAZOR93
#endif
	struct command_item_move * pinfo = (struct command_item_move *) data;

	if (ch)
		ch->MoveItem(pinfo->Cell, pinfo->CellTo, pinfo->count);
}

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
void CInputMain::InventoryExpansion(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate InventoryExpansion handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::InventoryExpansion");//INGAME_DEBUG_RAZOR93
#endif
	if (ch)
		ch->Update_Inven();
}
#endif

void CInputMain::ItemPickup(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemPickup handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::ItemPickup");//INGAME_DEBUG_RAZOR93
#endif
	struct command_item_pickup * pinfo = (struct command_item_pickup*) data;
	if (ch) {
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
		if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR) {
			return;
		}
#endif
	}

	ch->PickupItem(pinfo->vid);
}

void CInputMain::QuickslotAdd(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate QuickslotAdd handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::QuickslotAdd");//INGAME_DEBUG_RAZOR93
#endif
	struct command_quickslot_add * pinfo = (struct command_quickslot_add *) data;
#ifdef ENABLE_BUG_FIXES
	if (pinfo->slot.type == QUICKSLOT_TYPE_ITEM
#ifdef ENABLE_EXTRA_INVENTORY
 || pinfo->slot.type == 12
#endif
	) {
		LPITEM item = nullptr;

#ifdef ENABLE_EXTRA_INVENTORY
		uint8_t type;
		if (pinfo->slot.type == 12) {
			pinfo->slot.type = QUICKSLOT_TYPE_ITEM_EXTRA;
			type = EXTRA_INVENTORY;
		} else {
			type = INVENTORY;
		}
#endif

		TItemPos srcCell(
#ifdef ENABLE_EXTRA_INVENTORY
		type
#else
		INVENTORY
#endif
		, pinfo->slot.pos);

		if (!(item = ch->GetItem(srcCell)))
			return;

		if (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, item)) != ITEM_USE && ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, item)) != ITEM_QUEST)
			return;

#ifdef ENABLE_EXTRA_INVENTORY
		if (type == QUICKSLOT_TYPE_ITEM_EXTRA && ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, item)) == ITEM_USE && ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, item)) == USE_POTION) {
			return;
		}
#endif
	}
#endif

	ch->SetQuickslot(pinfo->pos, pinfo->slot);
}

void CInputMain::QuickslotDelete(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate QuickslotDelete handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::QuickslotDelete");//INGAME_DEBUG_RAZOR93
#endif
	struct command_quickslot_del * pinfo = (struct command_quickslot_del *) data;
	ch->DelQuickslot(pinfo->pos);
}

void CInputMain::QuickslotSwap(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate QuickslotSwap handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::QuickslotSwap");//INGAME_DEBUG_RAZOR93
#endif
	struct command_quickslot_swap * pinfo = (struct command_quickslot_swap *) data;
	ch->SwapQuickslot(pinfo->pos, pinfo->change_pos);
}

int CInputMain::Messenger(LPCHARACTER ch, const char* c_pData, uint64_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Messenger handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::Messenger");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGMessenger* p = (TPacketCGMessenger*) c_pData;

	if (uiBytes < sizeof(TPacketCGMessenger))
		return -1;

	c_pData += sizeof(TPacketCGMessenger);
	uiBytes -= sizeof(TPacketCGMessenger);

	switch (p->subheader)
	{
		case MESSENGER_SUBHEADER_CG_ADD_BY_VID:
			{
				if (uiBytes < sizeof(TPacketCGMessengerAddByVID))
					return -1;

				TPacketCGMessengerAddByVID * p2 = (TPacketCGMessengerAddByVID *) c_pData;
				LPCHARACTER ch_companion = CHARACTER_MANAGER::instance().Find(p2->vid);

				if (!ch_companion)
					return sizeof(TPacketCGMessengerAddByVID);

				if (ch->IsObserverMode())
					return sizeof(TPacketCGMessengerAddByVID);

				if (ch_companion->IsBlockMode(BLOCK_MESSENGER_INVITE))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 370, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch_companion)).data());
#endif
					return sizeof(TPacketCGMessengerAddByVID);
				}

				LPDESC d = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch_companion));

				if (!d)
					return sizeof(TPacketCGMessengerAddByVID);

				if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) == GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch_companion)) != GM_PLAYER)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 184, "");
#endif
					return sizeof(TPacketCGMessengerAddByVID);
				}

				if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) == d) // ÀÚ½ÅÀº Ãß°¡ÇÒ ¼ö ¾ø´Ù.
					return sizeof(TPacketCGMessengerAddByVID);

				MessengerManager::instance().RequestToAdd(ch, ch_companion);
				//MessengerManager::instance().AddToList(ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch_companion)).data());
			}
			return sizeof(TPacketCGMessengerAddByVID);

		case MESSENGER_SUBHEADER_CG_ADD_BY_NAME:
			{
				if (uiBytes < CHARACTER_NAME_MAX_LEN)
					return -1;

				char name[CHARACTER_NAME_MAX_LEN + 1];
				strlcpy(name, c_pData, sizeof(name));

				if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) == GM_PLAYER && gm_get_level(name) != GM_PLAYER)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 184, "");
#endif
					return CHARACTER_NAME_MAX_LEN;
				}

				LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(name);
				if (tch)
				{
					if (tch == ch) // ÀÚ½ÅÀº Ãß°¡ÇÒ ¼ö ¾ø´Ù.
						return CHARACTER_NAME_MAX_LEN;

					if (tch->IsBlockMode(BLOCK_MESSENGER_INVITE) == true)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 370, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(tch)).data());
#endif
					}
					else
					{
						// ¸Þ½ÅÀú°¡ Ä³¸¯ÅÍ´ÜÀ§°¡ µÇ¸é¼­ º¯°æ
						MessengerManager::instance().RequestToAdd(ch, tch);
						//MessengerManager::instance().AddToList(ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(tch)).data());
					}
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 108, "%s", name);
				}
#endif
			}
			return CHARACTER_NAME_MAX_LEN;

		case MESSENGER_SUBHEADER_CG_REMOVE:
			{
				if (uiBytes < CHARACTER_NAME_MAX_LEN)
					return -1;

				char char_name[CHARACTER_NAME_MAX_LEN + 1];
				strlcpy(char_name, c_pData, sizeof(char_name));
				MessengerManager::instance().RemoveFromList(ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), char_name);
#ifdef ENABLE_BUG_FIXES
				MessengerManager::instance().RemoveFromList(char_name, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
#endif
			}
			return CHARACTER_NAME_MAX_LEN;

		default:
			LOG_ERROR("CInputMain::Messenger : Unknown subheader {} : {}", p->subheader, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
			break;
	}

	return 0;
}

#ifdef ENABLE_BATTLE_PASS
int CInputMain::BattlePass(LPCHARACTER ch, const char* data, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate BattlePass handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: int CInputMain::BattlePas");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGBattlePassAction * p = (TPacketCGBattlePassAction *) data;

	if (uiBytes < sizeof(TPacketCGBattlePassAction))
		return -1;

	//const char * c_pData = data + sizeof(TPacketCGBattlePassAction);
	uiBytes -= sizeof(TPacketCGBattlePassAction);

	switch(p->bAction)
	{
		case 1:
			CBattlePass::instance().BattlePassRequestOpen(ch);
			break;

		case 2:
			CBattlePass::instance().BattlePassRequestReward(ch);
			break;

		case 3:
		{
			uint32_t dwPlayerId = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch));
			uint8_t bIsGlobal = 0;

			db_clientdesc->DBPacketHeader(HEADER_GD_BATTLE_PASS_RANKING, ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetHandle(), sizeof(uint32_t) + sizeof(uint8_t));
			db_clientdesc->Packet(&dwPlayerId, sizeof(uint32_t));
			db_clientdesc->Packet(&bIsGlobal, sizeof(uint8_t));
		}
		break;

		default:
			break;
	}

	return 0;
}
#endif

int CInputMain::Shop(LPCHARACTER ch, const char * data, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Shop handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::int CInputMain::Shop");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGShop * p = (TPacketCGShop *) data;

	if (uiBytes < sizeof(TPacketCGShop))
		return -1;

	if (test_server)
		LOG_INFO("CInputMain::Shop() ==> SubHeader {}", p->subheader);

	const char * c_pData = data + sizeof(TPacketCGShop);
	uiBytes -= sizeof(TPacketCGShop);

	switch (p->subheader)
	{
		case SHOP_SUBHEADER_CG_END:
			LOG_INFO("INPUT: {} SHOP: END", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
			CShopManager::instance().StopShopping(ch);
			return 0;

		case SHOP_SUBHEADER_CG_BUY:
			{
				if (uiBytes < sizeof(uint8_t) + sizeof(uint8_t))
					return -1;

				uint8_t bPos = *(c_pData + 1);
				LOG_INFO("INPUT: {} SHOP: BUY {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), bPos);
				CShopManager::instance().Buy(ch, bPos);
				return (sizeof(uint8_t) + sizeof(uint8_t));
			}
#ifndef ENABLE_EXTRA_INVENTORY
		case SHOP_SUBHEADER_CG_SELL:
			{
				if (uiBytes < sizeof(uint8_t))
					return -1;

				uint8_t pos = *c_pData;

				LOG_INFO("INPUT: {} SHOP: SELL", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
				CShopManager::instance().Sell(ch, pos);
				return sizeof(uint8_t);
			}
#endif
		case SHOP_SUBHEADER_CG_SELL2:
			{
				if (uiBytes < sizeof(uint8_t)
#ifdef ENABLE_EXTRA_INVENTORY
				+ sizeof(uint16_t)
#else
				+ sizeof(uint8_t)
#endif
#ifdef ENABLE_NEW_STACK_LIMIT
				+ sizeof(uint16_t)
#else
				+ sizeof(uint8_t)
#endif
				)
					return -1;

#ifdef ENABLE_EXTRA_INVENTORY
				uint8_t window = *(c_pData);
				uint16_t cell = *(uint16_t*)(c_pData + 1);
#else
				uint8_t pos = *(c_pData++);
#endif
#ifdef ENABLE_NEW_STACK_LIMIT
				uint16_t count = *(uint16_t*)(c_pData + sizeof(uint16_t));
#else
				uint8_t count = *(c_pData);
#endif

				LOG_INFO("INPUT: {} SHOP: SELL2", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
				CShopManager::instance().Sell(ch,
#ifdef ENABLE_EXTRA_INVENTORY
				TItemPos(window, cell),
#else
				pos,
#endif
				count);
				return sizeof(uint8_t)
#ifdef ENABLE_EXTRA_INVENTORY
				+ sizeof(uint16_t)
#else
				+ sizeof(uint8_t)
#endif
#ifdef ENABLE_NEW_STACK_LIMIT
				+ sizeof(uint16_t)
#else
				+ sizeof(uint8_t)
#endif
				;
			}
#ifdef ENABLE_BUY_STACK_FROM_SHOP
		case SHOP_SUBHEADER_CG_BUY2:
			{
				size_t size = sizeof(uint8_t) + sizeof(uint8_t);
				if (uiBytes < size) {
					return -1;
				}

				uint8_t p = *(c_pData++);
				uint8_t c = *(c_pData);
				LOG_INFO("INPUT: {} SHOP: MULTIPLE BUY {} COUNT {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), p, c);
				CShopManager::instance().MultipleBuy(ch, p, c);
				return size;
			}
#endif
		default:
			LOG_ERROR("CInputMain::Shop : Unknown subheader {} : {}", p->subheader, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
			break;
	}

	return 0;
}

void CInputMain::OnClick(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate OnClick handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::OnClick(LPCHARACTER ch, const char * data)");//INGAME_DEBUG_RAZOR93
#endif
	struct command_on_click *	pinfo = (struct command_on_click *) data;
	LPCHARACTER			victim;

	if ((victim = CHARACTER_MANAGER::instance().Find(pinfo->vid)))
		victim->OnClick(ch);
	else if (test_server)
	{
		LOG_ERROR("CInputMain::OnClick {}.Click.NOT_EXIST_VID[{}]", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), pinfo->vid);
	}
}

void CInputMain::Exchange(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Exchange handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::Exchange(LPCHARACTER ch, const char * data)");//INGAME_DEBUG_RAZOR93
#endif
	struct command_exchange * pinfo = (struct command_exchange *) data;
	LPCHARACTER	to_ch = nullptr;

	if (!ch->CanHandleItem())
		return;

	int iPulse = thecore_pulse();

	if ((to_ch = CHARACTER_MANAGER::instance().Find(pinfo->arg1)))
	{
		if (iPulse - to_ch->GetSafeboxLoadTime() < PASSES_PER_SEC(g_nPortalLimitTime))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(to_ch), CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
			return;
		}

		if( true == to_ch->IsDead() )
		{
			return;
		}
	}

	LOG_INFO("CInputMain()::Exchange()  SubHeader {} ", pinfo->sub_header);

	if (iPulse - ch->GetSafeboxLoadTime() < PASSES_PER_SEC(g_nPortalLimitTime))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
		return;
	}


	switch (pinfo->sub_header)
	{
		case EXCHANGE_SUBHEADER_CG_START:	// arg1 == vid of target character
			if (!ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)))
			{
				if ((to_ch = CHARACTER_MANAGER::instance().Find(pinfo->arg1)))
				{
					if (iPulse - ch->GetSafeboxLoadTime() < PASSES_PER_SEC(g_nPortalLimitTime))
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
						return;
					}

					if (iPulse - to_ch->GetSafeboxLoadTime() < PASSES_PER_SEC(g_nPortalLimitTime))
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(to_ch), CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
						return;
					}

					if (ecs::PointSystem::GetGold(AIHelpers::EcsOf(ch)) >= GOLD_MAX) {
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 406,

						"%lld"

						, GOLD_MAX);
#endif
						return;
					}

					if (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(to_ch)))
					{
						if (quest::CQuestManager::instance().GiveItemToPC(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), to_ch))
						{
							LOG_INFO("Exchange canceled by quest {} {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(to_ch)).data());
							return;
						}
					}


					if (ch->GetMyShop() || ch->IsOpenSafebox() || ch->GetShopOwner() || ch->IsCubeOpen()

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
						|| ch->GetWheelDestiny()
#endif
						)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 292, "");
#endif
						return;
					}

#ifdef __ATTR_TRANSFER_SYSTEM__
					if (ch->IsAttrTransferOpen())
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 292, "");
#endif
						return;
					}
#endif
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
					if ((ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR) || (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(to_ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(to_ch)) < GM_IMPLEMENTOR)) {
						return;
					}
#endif
					ch->ExchangeStart(to_ch);
				}
			}
			break;

		case EXCHANGE_SUBHEADER_CG_ITEM_ADD:	// arg1 == position of item, arg2 == position in exchange window
			if (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)))
			{
				if (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch))->GetCompany()->GetAcceptStatus() != true)
					ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch))->AddItem(pinfo->Pos, pinfo->arg2);
			}
			break;

		case EXCHANGE_SUBHEADER_CG_ITEM_DEL:	// arg1 == position of item
			if (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)))
			{
				if (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch))->GetCompany()->GetAcceptStatus() != true)
					ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch))->RemoveItem(pinfo->arg1);
			}
			break;

		case EXCHANGE_SUBHEADER_CG_ELK_ADD:	// arg1 == amount of gold
			if (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)))
			{

				const int64_t nTotalGold = ecs::PointSystem::GetGold(AIHelpers::EcsOf(ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch))->GetCompany()->GetOwner())) + pinfo->arg1;

				if (GOLD_MAX <= nTotalGold)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 226,

					"%lld"

					, nTotalGold);
#endif
					return;
				}

				if (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch))->GetCompany()->GetAcceptStatus() != true)
					ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch))->AddGold(pinfo->arg1);
			}
			break;
		case EXCHANGE_SUBHEADER_CG_ACCEPT:	// arg1 == not used
			if (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)))
			{
				LOG_INFO("CInputMain()::Exchange() ==> ACCEPT ");
				ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch))->Accept(true);
			}

			break;

		case EXCHANGE_SUBHEADER_CG_CANCEL:	// arg1 == not used
			if (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)))
				ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch))->Cancel();
			break;
	}
}

void CInputMain::Position(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Position handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::Position(LPCHARACTER ch, const char * data)");//INGAME_DEBUG_RAZOR93
#endif
	struct command_position * pinfo = (struct command_position *) data;

	switch (pinfo->position)
	{
		case POSITION_GENERAL:
			ch->Standup();
			break;

		case POSITION_SITTING_CHAIR:
			ch->Sitdown(0);
			break;

		case POSITION_SITTING_GROUND:
			ch->Sitdown(1);
			break;
	}
}

static const int ComboSequenceBySkillLevel[3][8] =
{
	// 0   1   2   3   4   5   6   7
	{ 14, 15, 16, 17,  0,  0,  0,  0 },
	{ 14, 15, 16, 18, 20,  0,  0,  0 },
	{ 14, 15, 16, 18, 19, 17,  0,  0 },
};

void CInputMain::Move(LPCHARACTER ch, const char * data)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::Move(LPCHARACTER ch, const char * data)");//INGAME_DEBUG_RAZOR93
#endif
	if (!ch)
		return;

	struct command_move * pinfo = (struct command_move *) data;
	if (!ch->CanMove())
		return;

	if (pinfo->bFunc >= FUNC_MAX_NUM && !(pinfo->bFunc & 0x80))
	{
		LOG_ERROR("invalid move type: {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
		return;
	}

	//enum EMoveFuncType
	//{
	//	FUNC_WAIT,
	//	FUNC_MOVE,
	//	FUNC_ATTACK,
	//	FUNC_COMBO,
	//	FUNC_MOB_SKILL,
	//	_FUNC_SKILL,
	//	FUNC_MAX_NUM,
	//	FUNC_SKILL = 0x80,
	//};

	// ÅÚ·¹Æ÷Æ® ÇÙ Ã¼Å©

//	if (!test_server)
	{
		const float fDistFromCurrent = DISTANCE_SQRT((ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(ch)) - pinfo->lX) / 100, (ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(ch)) - pinfo->lY) / 100);
		float fDist = fDistFromCurrent;

		// When movement is already in-flight, compare the next client target against the
		// pending server destination as well. Without this, legitimate follow-up move
		// packets get treated as teleports and the server rubberbands the player.
		if (pinfo->bFunc == FUNC_MOVE &&
			ch->GetCurrentMoveDuration() > 0 &&
			(ch->GetCurrentDestX() != ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(ch)) || ch->GetCurrentDestY() != ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(ch))))
		{
			const float fDistFromDest = DISTANCE_SQRT((ch->GetCurrentDestX() - pinfo->lX) / 100, (ch->GetCurrentDestY() - pinfo->lY) / 100);
			fDist = std::min(fDistFromCurrent, fDistFromDest);
		}
		if (((false == ch->IsRiding() && fDist > 30) || fDist > 60) && OXEVENT_MAP_INDEX != ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)))
		{
			LOG_INFO("MOVE: {} trying to move too far (dist: {:.1f}m current: {:.1f}m) Riding({})", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), fDist, fDistFromCurrent, ch->IsRiding());

			ch->Show(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(ch)), ch->GetZ());
			ch->Stop();
			return;
		}
#ifdef ENALBE_MOUNT_SECTREE_UPDATE_RAZOR93
		if (true == ch->IsRiding())
		{
			ch->UpdateSectree();
		}
#endif
#ifdef ENABLE_CHECK_GHOSTMODE
		if (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)) && ch->IsDead())
		{
			LOG_INFO("MOVE: {} trying to move as dead", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());

			ch->Show(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(ch)), ch->GetZ());
			ch->Stop();
			return;
		}
#endif

		uint32_t dwCurTime = get_dword_time();
		if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))) {
			bool CheckSpeedHack = (false == ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->IsHandshaking() && dwCurTime - ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetClientTime() > 7000);
			if (CheckSpeedHack)
			{
				int iDelta = (int)(dwCurTime - pinfo->dwTime);
				int iServerDelta = (int)(dwCurTime - ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetClientTime());
				if (iDelta >= 30000) {
					LOG_INFO("SPEEDHACK: slow timer name {} delta {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), iDelta);
					ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->DelayedDisconnect(3);
				} else if (iDelta < -(iServerDelta / 50)) {
					LOG_INFO("SPEEDHACK: DETECTED! {} (delta {} {})", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), iDelta, iServerDelta);
					ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->DelayedDisconnect(3);
				}
			}

			//if (pinfo->bFunc == FUNC_COMBO && g_bCheckMultiHack)
			//{
			//	CheckComboHack(ch, pinfo->bArg, pinfo->dwTime, CheckSpeedHack); // ÄÞº¸ Ã¼Å©
			//}
		}
	}

	// migrated from CHARACTER::Move
	entt::entity e = (ch && ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetEntity() : entt::null;
	if (e != entt::null && g_registry.valid(e))
	{
		g_registry.emplace_or_replace<ecs::MovementDestination>(e, static_cast<int32_t>(pinfo->lX), static_cast<int32_t>(pinfo->lY));
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	}
	// DUAL-PATH: ECS + legacy call
	if (pinfo->bFunc == FUNC_MOVE)
	{
		if (ch->GetLimitPoint(POINT_MOV_SPEED) == 0)
			return;

		ch->SetRotation(pinfo->bRot * 5.0f);
		ch->ResetStopTime();

		ch->Goto(pinfo->lX, pinfo->lY);
	}
	else
	{
		if (pinfo->bFunc == FUNC_ATTACK || pinfo->bFunc == FUNC_COMBO)
		{
			ch->OnMove(true);
		}
		else if (pinfo->bFunc & FUNC_SKILL)
		{
			const int MASK_SKILL_MOTION = 0x7F;
			unsigned int motion = pinfo->bFunc & MASK_SKILL_MOTION;

			if (!ch->IsUsableSkillMotion(motion))
			{
				ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->DelayedDisconnect(number(150, 500));
			}

			ch->OnMove();
		}

		ch->SetRotation(pinfo->bRot * 5.0f);
		ch->ResetStopTime();

		ch->Move(pinfo->lX, pinfo->lY);
		ch->Stop();
		ch->StopStaminaConsume();
	}

	TPacketGCMove pack;

	pack.bHeader      = HEADER_GC_MOVE;
	pack.bFunc        = pinfo->bFunc;
	pack.bArg         = pinfo->bArg;
	pack.bRot         = pinfo->bRot;
	pack.dwVID        = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(ch));
	pack.lX           = pinfo->lX;
	pack.lY           = pinfo->lY;
	pack.dwTime       = pinfo->dwTime;
	pack.dwDuration   = (pinfo->bFunc == FUNC_MOVE) ? ch->GetCurrentMoveDuration() : 0;

	ch->PacketAround(&pack, sizeof(TPacketGCMove), ch);
/*
	if (pinfo->dwTime == 10653691) // µð¹ö°Å ¹ß°ß
	{
		if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->DelayedDisconnect(number(15, 30)))
			LogManager::instance().HackLog("Debugger", ch);

	}
	else if (pinfo->dwTime == 10653971) // Softice ¹ß°ß
	{
		if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->DelayedDisconnect(number(15, 30)))
			LogManager::instance().HackLog("Softice", ch);
	}
*/
	/*
	LOG_INFO(
			"MOVE: {} Func:{} Arg:{} Pos:{}x{} Time:{} Dist:{:.1f}",
			ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(),
			pinfo->bFunc,
			pinfo->bArg,
			pinfo->lX / 100,
			pinfo->lY / 100,
			pinfo->dwTime,
			fDist);
	*/
}

#ifdef __SKILL_COLOR_SYSTEM__
void CInputMain::SetSkillColor(LPCHARACTER ch, const char* pcData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate SetSkillColor handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::SetSkillColor(LPCHARACTER ch, const char* pcData)");//INGAME_DEBUG_RAZOR93
#endif
	if (!ch)
		return;

	TPacketCGSkillColor * p = (TPacketCGSkillColor*)pcData;
	if (p->skill >= ESkillColorLength::MAX_SKILL_COUNT)
		return;

	if ((p->col1 != 0) || (p->col2 != 0) || (p->col3 != 0) || (p->col4 != 0) || (p->col5 != 0)) {
		if (ch->CountSpecifyItem(164406) < 1) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 16, "");
#endif
			return;
		} else {
			ch->RemoveSpecifyItem(164406, 1);
		}
	}

	uint32_t data[ESkillColorLength::MAX_SKILL_COUNT + ESkillColorLength::MAX_BUFF_COUNT][ESkillColorLength::MAX_EFFECT_COUNT];
	memcpy(data, ch->GetSkillColor(), sizeof(data));

	data[p->skill][0] = p->col1;
	data[p->skill][1] = p->col2;
	data[p->skill][2] = p->col3;
	data[p->skill][3] = p->col4;
	data[p->skill][4] = p->col5;

#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 15, "");
#endif

	ch->SetSkillColor(data[0]);

	TSkillColor db_pack;
	memcpy(db_pack.dwSkillColor, data, sizeof(data));
	db_pack.player_id = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch));
	db_clientdesc->DBPacketHeader(HEADER_GD_SKILL_COLOR_SAVE, 0, sizeof(TSkillColor));
	db_clientdesc->Packet(&db_pack, sizeof(TSkillColor));
}
#endif

void CInputMain::Attack(LPCHARACTER ch, const uint8_t header, const char* data)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::Attack(LPCHARACTER");//INGAME_DEBUG_RAZOR93
#endif
	if (nullptr == ch)
		return;


	struct type_identifier
	{
		uint8_t header;
		uint8_t type;
	};

	const struct type_identifier* const type = reinterpret_cast<const struct type_identifier*>(data);

	if (type->type > 0)
	{
		if (false == SkillSystem::CanUseSkill(AIHelpers::EcsOf(ch), type->type))
		{
			return;
		}

		switch (type->type)
		{
			case SKILL_GEOMPUNG:
			case SKILL_SANGONG:
			case SKILL_YEONSA:
			case SKILL_KWANKYEOK:
			case SKILL_HWAJO:
			case SKILL_GIGUNG:
			case SKILL_PABEOB:
			case SKILL_MARYUNG:
			case SKILL_TUSOK:
			case SKILL_MAHWAN:
			case SKILL_BIPABU:
			case SKILL_NOEJEON:
			case SKILL_CHAIN:
			case SKILL_HORSE_WILDATTACK_RANGE:
				if (HEADER_CG_SHOOT != type->header)
				{
					return;
				}
				break;
		}
	}

	switch (header)
	{
		case HEADER_CG_ATTACK:
			{
				if (nullptr == ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
				{
					return;
				}

				const TPacketCGAttack* const packMelee = reinterpret_cast<const TPacketCGAttack*>(data);

				ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->AssembleCRCMagicCube(packMelee->bCRCMagicCubeProcPiece, packMelee->bCRCMagicCubeFilePiece);

				LPCHARACTER	victim = CHARACTER_MANAGER::instance().Find(packMelee->dwVID);

				if (nullptr == victim || ch == victim)
				{
					return;
				}

				switch (victim->GetCharType())
				{
					case CHAR_TYPE_NPC:
					case CHAR_TYPE_WARP:
					case CHAR_TYPE_GOTO:
						return;
				}

				if (packMelee->bType > 0)
				{
					if (false == ch->CheckSkillHitCount(packMelee->bType, victim->GetEntityHandle()))
					{
						return;
					}
				}

				// migrated from CHARACTER::Attack
				entt::entity attacker = (ch && ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetEntity() : entt::null;
				entt::entity target = CVIDRegistry::Instance().Find(packMelee->dwVID);
				if (attacker != entt::null && target != entt::null && g_registry.valid(attacker) && g_registry.valid(target))
				{
					g_registry.emplace_or_replace<ecs::CombatTarget>(attacker, target, get_dword_time());
					g_registry.emplace_or_replace<ecs::CombatActiveTag>(attacker);
					g_registry.emplace_or_replace<ecs::DirtyTag>(attacker);
				}
				// DUAL-PATH: ECS + legacy call
				ch->OnMove(true);
				ch->Attack(victim, packMelee->bType);
			}
			break;

		case HEADER_CG_SHOOT:
			{
				const TPacketCGShoot* const packShoot = reinterpret_cast<const TPacketCGShoot*>(data);

				ch->Shoot(packShoot->bType);
			}
			break;
	}
}

int CInputMain::SyncPosition(LPCHARACTER ch, const char * c_pcData, uint64_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate SyncPosition handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::int CInputMain::SyncPosition");//INGAME_DEBUG_RAZOR93
#endif
	const TPacketCGSyncPosition* pinfo = reinterpret_cast<const TPacketCGSyncPosition*>( c_pcData );

	if (uiBytes < pinfo->wSize)
		return -1;

	int iExtraLen = pinfo->wSize - sizeof(TPacketCGSyncPosition);

	if (iExtraLen < 0)
	{
		LOG_ERROR("invalid packet length (len {} size {} buffer {})", iExtraLen, pinfo->wSize, uiBytes);
		ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->SetPhase(PHASE_CLOSE);
		return -1;
	}

	if (0 != (iExtraLen % sizeof(TPacketCGSyncPositionElement)))
	{
		LOG_ERROR("invalid packet length {} (name: {})", pinfo->wSize, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
		return iExtraLen;
	}

	int iCount = iExtraLen / sizeof(TPacketCGSyncPositionElement);

	if (iCount <= 0)
		return iExtraLen;

	static const int nCountLimit = 60;

	if( iCount > nCountLimit )
	{
		//LogManager::instance().HackLog( "SYNC_POSITION_HACK", ch );
		LOG_ERROR("Too many SyncPosition Count({}) from Name({})", iCount, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
		//ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->SetPhase(PHASE_CLOSE);
		//return -1;
		iCount = nCountLimit;
	}

	TEMP_BUFFER tbuf;
	LPBUFFER lpBuf = tbuf.getptr();

	TPacketGCSyncPosition * pHeader = (TPacketGCSyncPosition *) buffer_write_peek(lpBuf);
	buffer_write_proceed(lpBuf, sizeof(TPacketGCSyncPosition));

	const TPacketCGSyncPositionElement* e =
		reinterpret_cast<const TPacketCGSyncPositionElement*>(c_pcData + sizeof(TPacketCGSyncPosition));

	timeval tvCurTime;
	gettimeofday(&tvCurTime, nullptr);

	for (int i = 0; i < iCount; ++i, ++e)
	{
		LPCHARACTER victim = CHARACTER_MANAGER::instance().Find(e->dwVID);

		if (!victim)
			continue;

		switch (victim->GetCharType())
		{
			case CHAR_TYPE_NPC:
			case CHAR_TYPE_WARP:
			case CHAR_TYPE_GOTO:
				continue;
		}

		if (!victim->SetSyncOwner(ch))
			continue;

		const float fDistWithSyncOwner = DISTANCE_SQRT( (ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(victim)) - ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(ch))) / 100, (ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(victim)) - ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(ch))) / 100 );
		static constexpr float fLimitDistWithSyncOwner = 2500.f + 1000.f;

		if (fDistWithSyncOwner > fLimitDistWithSyncOwner)
		{
			if (ch->GetSyncHackCount() < 60){
				ch->SetSyncHackCount(ch->GetSyncHackCount() + 1);
				continue;
			} else{
				LogManager::instance().HackLog( "SYNC_POSITION_HACK", ch );

				LOG_ERROR("Too far SyncPosition DistanceWithSyncOwner({})({}) from Name({}) CH({},{}) VICTIM({},{}) SYNC({},{})", fDistWithSyncOwner, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(victim)).data(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(victim)), ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(victim)), e->lX, e->lY);

				ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->SetPhase(PHASE_CLOSE);

				return -1;
			}
		}

		const float fDist = DISTANCE_SQRT( (ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(victim)) - e->lX) / 100, (ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(victim)) - e->lY) / 100 );


		static constexpr int32_t g_lValidSyncInterval = 50 * 1000;
		const timeval& tvLastSyncTime = victim->GetLastSyncTime();
		timeval* tvDiff = timediff(&tvCurTime, &tvLastSyncTime);

		if (tvDiff->tv_sec == 0 && tvDiff->tv_usec < g_lValidSyncInterval)
		{
			if (ch->GetSyncHackCount() < 60)
			{
				ch->SetSyncHackCount(ch->GetSyncHackCount() + 1);
				continue;
			}
			else
			{
				LogManager::instance().HackLog("SYNC_POSITION_HACK", ch);

				LOG_ERROR("Too often SyncPosition Interval({}ms)({}) from Name({}) VICTIM({},{}) SYNC({},{})", tvDiff->tv_sec * 1000 + tvDiff->tv_usec / 1000, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(victim)).data(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(victim)), ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(victim)), e->lX, e->lY);

				ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->SetPhase(PHASE_CLOSE);

				return -1;
			}
		}
		else if( fDist > 40.0f ){

			LogManager::instance().HackLog( "SYNC_POSITION_HACK", ch );

			LOG_ERROR("Too far SyncPosition Distance({})({}) from Name({}) CH({},{}) VICTIM({},{}) SYNC({},{})", fDist, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(victim)).data(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(victim)), ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(victim)), e->lX, e->lY);

			ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->SetPhase(PHASE_CLOSE);

			return -1;
		} else{
			victim->SetLastSyncTime(tvCurTime);
			victim->Sync(e->lX, e->lY);
			buffer_write(lpBuf, e, sizeof(TPacketCGSyncPositionElement));
		}
	}

	if (buffer_size(lpBuf) != sizeof(TPacketGCSyncPosition))
	{
		pHeader->bHeader = HEADER_GC_SYNC_POSITION;
		pHeader->wSize = buffer_size(lpBuf);

		ch->PacketAround(buffer_read_peek(lpBuf), buffer_size(lpBuf), ch);
	}

	return iExtraLen;
}

void CInputMain::FlyTarget(LPCHARACTER ch, const char * pcData, uint8_t bHeader)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate FlyTarget handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::FlyTarget");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGFlyTargeting * p = (TPacketCGFlyTargeting *) pcData;
	ch->FlyTarget(p->dwTargetVID, p->x, p->y, bHeader);
}

void CInputMain::UseSkill(LPCHARACTER ch, const char * pcData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate UseSkill handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::UseSkill");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGUseSkill * p = (TPacketCGUseSkill *) pcData;
	ch->UseSkill(p->dwVnum, CHARACTER_MANAGER::instance().Find(p->dwVID));
}

void CInputMain::ScriptButton(LPCHARACTER ch, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ScriptButton handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::ScriptButton");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGScriptButton * p = (TPacketCGScriptButton *) c_pData;
	LOG_INFO("QUEST ScriptButton pid {} idx {}", ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), p->idx);

	quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
	if (pc && pc->IsConfirmWait())
	{
		quest::CQuestManager::instance().Confirm(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), quest::CONFIRM_TIMEOUT);
	}
	else if (p->idx & 0x80000000)
	{
		//Äù½ºÆ® Ã¢¿¡¼­ Å¬¸¯½Ã(__SelectQuest) ¿©±â·Î
		quest::CQuestManager::Instance().QuestInfo(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), p->idx & 0x7fffffff);
	}
	else
	{
		quest::CQuestManager::Instance().QuestButton(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), p->idx);
	}
}

void CInputMain::ScriptAnswer(LPCHARACTER ch, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ScriptAnswer handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::ScriptAnswer");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGScriptAnswer * p = (TPacketCGScriptAnswer *) c_pData;
	LOG_INFO("QUEST ScriptAnswer pid {} answer {}", ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), p->answer);

	if (p->answer > 250) // ´ÙÀ½ ¹öÆ°¿¡ ´ëÇÑ ÀÀ´äÀ¸·Î ¿Â ÆÐÅ¶ÀÎ °æ¿ì
	{
		quest::CQuestManager::Instance().Resume(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
	}
	else // ¼±ÅÃ ¹öÆ°À» °ñ¶ó¼­ ¿Â ÆÐÅ¶ÀÎ °æ¿ì
	{
		quest::CQuestManager::Instance().Select(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)),  p->answer);
	}
}


// SCRIPT_SELECT_ITEM
void CInputMain::ScriptSelectItem(LPCHARACTER ch, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ScriptSelectItem handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::ScriptSelectItem");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGScriptSelectItem* p = (TPacketCGScriptSelectItem*) c_pData;
	LOG_INFO("QUEST ScriptSelectItem pid {} answer {}", ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), p->selection);
	quest::CQuestManager::Instance().SelectItem(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), p->selection);
}
// END_OF_SCRIPT_SELECT_ITEM

void CInputMain::QuestInputString(LPCHARACTER ch, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate QuestInputString handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::QuestInputString");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGQuestInputString * p = (TPacketCGQuestInputString*) c_pData;

	char msg[65];
	strlcpy(msg, p->msg, sizeof(msg));
	LOG_INFO("QUEST InputString pid {} msg {}", ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), msg);

	quest::CQuestManager::Instance().Input(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), msg);
}

void CInputMain::QuestConfirm(LPCHARACTER ch, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate QuestConfirm handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::QuestConfirm");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGQuestConfirm* p = (TPacketCGQuestConfirm*) c_pData;
	LPCHARACTER ch_wait = CHARACTER_MANAGER::instance().FindByPID(p->requestPID);
	if (p->answer)
		p->answer = quest::CONFIRM_YES;
	LOG_INFO("QuestConfirm from {} pid {} name {} answer {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), p->requestPID, (ch_wait)?ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch_wait)).data():"", p->answer);
	if (ch_wait)
	{
		quest::CQuestManager::Instance().Confirm(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch_wait)), (quest::EQuestConfirmType) p->answer, ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
	}
}

void CInputMain::Target(LPCHARACTER ch, const char * pcData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Target handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::Target");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGTarget * p = (TPacketCGTarget *) pcData;

	building::LPOBJECT pkObj = building::CManager::instance().FindObjectByVID(p->dwVID);

	if (pkObj)
	{
		TPacketGCTarget pckTarget;
		pckTarget.header = HEADER_GC_TARGET;
		pckTarget.dwVID = p->dwVID;
		ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(&pckTarget, sizeof(TPacketGCTarget));
	}
	else
		ch->SetTarget(CHARACTER_MANAGER::instance().Find(p->dwVID));
}

void CInputMain::Warp(LPCHARACTER ch, const char * pcData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Warp handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::Warp");//INGAME_DEBUG_RAZOR93
#endif
	ch->WarpEnd();
}

void CInputMain::SafeboxCheckin(LPCHARACTER ch, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate SafeboxCheckin handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::SafeboxCheckin");//INGAME_DEBUG_RAZOR93
#endif

	if (quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))->IsRunning() == true)
		return;

	TPacketCGSafeboxCheckin * p = (TPacketCGSafeboxCheckin *) c_pData;

	if (!ch->CanHandleItem())
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR) {
		return;
	}
#endif
	if (p->ItemPos.IsBeltInventoryPosition())
	{

		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You cannot place items from the Belt inventory into the safebox.");
		//LOG_INFO(0, "BELT_TO_SAFEBOX_BLOCKED: Player %s tried to move item from belt to safebox.", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
		return;
	}

	CSafebox * pkSafebox = ch->GetSafebox();
	LPITEM pkItem = ch->GetItem(p->ItemPos);

	if (!pkSafebox || !pkItem)
		return;

#ifdef ENABLE_BUG_FIXES
	if (ItemSystem::IsItemEquipped(EntityFactory::CreateItemEntity(g_registry, pkItem)))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 1244, "");
#endif
		return;
	}
#endif

#if defined(ENABLE_EXTRA_INVENTORY) && !defined(ENABLE_SPECIAL_INV_TO_SAFEBOX)
	if (pkItem->IsExtraItem())
		return;
#endif

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	if (ItemSystem::GetItemCell(EntityFactory::CreateItemEntity(g_registry, pkItem)) >= ch->Inventory_Size() && IS_SET(pkItem->GetFlag(),ITEM_FLAG_IRREMOVABLE))
#else
	if (ItemSystem::GetItemCell(EntityFactory::CreateItemEntity(g_registry, pkItem)) >= INVENTORY_MAX_NUM && IS_SET(pkItem->GetFlag(), ITEM_FLAG_IRREMOVABLE))
#endif
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 640, "");
#endif
		return;
	}

	if (!pkSafebox->IsEmpty(p->bSafePos, pkItem->GetSize()))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 641, "");
#endif
		return;
	}

	if (ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, pkItem)) == UNIQUE_ITEM_SAFEBOX_EXPAND)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 187, "");
#endif
		return;
	}

	if( IS_SET(ItemSystem::GetItemAntiFlag(EntityFactory::CreateItemEntity(g_registry, pkItem)), ITEM_ANTIFLAG_SAFEBOX) )
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 187, "");
#endif
		return;
	}

	if (true == pkItem->isLocked())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 187, "");
#endif
		return;
	}

	// @fixme140 BEGIN
	if (ITEM_BELT == ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)) && CBeltInventoryHelper::IsExistItemInBeltInventory(ch))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 385, "");
#endif
		return;
	}
	// @fixme140 END

	pkItem->RemoveFromCharacter();
#ifdef ENABLE_EXTRA_INVENTORY
	if (!pkItem->IsDragonSoul() && !pkItem->IsExtraItem())
#else
	if (!pkItem->IsDragonSoul())
#endif
	{
		ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM, p->ItemPos.cell, 255);
	}

	pkSafebox->Add(p->bSafePos, pkItem);

	char szHint[128];
	snprintf(szHint, sizeof(szHint), "%s %u", pkItem->GetName(), ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, pkItem)));
	LogManager::instance().ItemLog(ch, pkItem, "SAFEBOX PUT", szHint);
}

void CInputMain::SafeboxCheckout(LPCHARACTER ch, const char * c_pData, bool bMall)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate SafeboxCheckout handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::SafeboxCheckout");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGSafeboxCheckout * p = (TPacketCGSafeboxCheckout *) c_pData;

	if (!ch->CanHandleItem())
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR) {
		return;
	}
#endif

	CSafebox * pkSafebox;

	if (bMall)
		pkSafebox = ch->GetMall();
	else
		pkSafebox = ch->GetSafebox();

	if (!pkSafebox)
		return;

	LPITEM pkItem = pkSafebox->Get(p->bSafePos);

	if (!pkItem)
		return;

	if (!ch->IsEmptyItemGrid(p->ItemPos, pkItem->GetSize()))
		return;

	// ¾ÆÀÌÅÛ ¸ô¿¡¼­ ÀÎº¥À¸·Î ¿Å±â´Â ºÎºÐ¿¡¼­ ¿ëÈ¥¼® Æ¯¼ö Ã³¸®
	// (¸ô¿¡¼­ ¸¸µå´Â ¾ÆÀÌÅÛÀº item_proto¿¡ Á¤ÀÇµÈ´ë·Î ¼Ó¼ºÀÌ ºÙ±â ¶§¹®¿¡,
	//  ¿ëÈ¥¼®ÀÇ °æ¿ì, ÀÌ Ã³¸®¸¦ ÇÏÁö ¾ÊÀ¸¸é ¼Ó¼ºÀÌ ÇÏ³ªµµ ºÙÁö ¾Ê°Ô µÈ´Ù.)
	if (pkItem->IsDragonSoul())
	{
		if (bMall)
		{
			DSManager::instance().DragonSoulItemInitialize(EntityFactory::CreateItemEntity(g_registry, pkItem));
		}

		if (DRAGON_SOUL_INVENTORY != p->ItemPos.window_type)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 643, "");
#endif
			return;
		}

		TItemPos DestPos = p->ItemPos;
		if (!DSManager::instance().IsValidCellForThisItem(EntityFactory::CreateItemEntity(g_registry, pkItem), DestPos))
		{
			int iCell = ch->GetEmptyDragonSoulInventory(pkItem);
			if (iCell < 0)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 644, "");
#endif
				return ;
			}
			DestPos = TItemPos (DRAGON_SOUL_INVENTORY, iCell);
		}

		pkSafebox->Remove(p->bSafePos);
		pkItem->AddToCharacter(ch, DestPos);
		ITEM_MANAGER::instance().FlushDelayedSave(pkItem);
	}
#ifdef ENABLE_EXTRA_INVENTORY//RAzor93
	else if(pkItem->IsExtraItem())
	{
//		if (p->ItemPos.window_type != EXTRA_INVENTORY)
//		{
//#ifdef TEXTS_IMPROVEMENT
//			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 1292, "");
//#endif
//			return;
//		}

		uint8_t category = pkItem->GetExtraCategory();
		if (p->ItemPos.cell < category * EXTRA_INVENTORY_CATEGORY_MAX_NUM || p->ItemPos.cell >= (category + 1) * EXTRA_INVENTORY_CATEGORY_MAX_NUM) {
			return;
		}

		pkSafebox->Remove(p->bSafePos);
		pkItem->AddToCharacter(ch, p->ItemPos);
		ITEM_MANAGER::instance().FlushDelayedSave(pkItem);
	}
#endif
	else
	{
		// safebox to beltinventory tiltas
		if (p->ItemPos.IsBeltInventoryPosition())
		{
			ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Nem helyezhetsz tárgyat közvetlenül a belt inventoryba.");
			LOG_INFO("BELT_BLOCKED_SFBCHECK: Tiltott belt slot próbálva: {}", p->ItemPos.cell);
			return;
		}

#ifdef ENABLE_EXTRA_INVENTORY
		if (EXTRA_INVENTORY == p->ItemPos.window_type)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 645, "");
#endif
			return;
		}
#endif
		if (DRAGON_SOUL_INVENTORY == p->ItemPos.window_type)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 646, "");
#endif
			return;
		}
		// @fixme119
		if (p->ItemPos.IsBeltInventoryPosition() && false == CBeltInventoryHelper::CanMoveIntoBeltInventory(pkItem))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 509, "");
#endif
			return;
		}

		pkSafebox->Remove(p->bSafePos);
		pkItem->AddToCharacter(ch, p->ItemPos);
		ITEM_MANAGER::instance().FlushDelayedSave(pkItem);
	}

	uint32_t dwID = ItemSystem::GetItemID(EntityFactory::CreateItemEntity(g_registry, pkItem));
	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_FLUSH, 0, sizeof(uint32_t));
	db_clientdesc->Packet(&dwID, sizeof(uint32_t));

	char szHint[128];
	snprintf(szHint, sizeof(szHint), "%s %u", pkItem->GetName(), ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, pkItem)));
	if (bMall)
		LogManager::instance().ItemLog(ch, pkItem, "MALL GET", szHint);
	else
		LogManager::instance().ItemLog(ch, pkItem, "SAFEBOX GET", szHint);
}

void CInputMain::SafeboxItemMove(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate SafeboxItemMove handler ECS
// DUAL-PATH: legacy only during migration window

#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::SafeboxItemMove");//INGAME_DEBUG_RAZOR93
#endif
	struct command_item_move * pinfo = (struct command_item_move *) data;

	if (!ch->CanHandleItem())
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR) {
		return;
	}
#endif

	if (!ch->GetSafebox())
		return;

	ch->GetSafebox()->MoveItem(pinfo->Cell.cell, pinfo->CellTo.cell, pinfo->count);
}

void CInputMain::MountInventoryCheckin(LPCHARACTER ch, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate MountInventoryCheckin handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::MountInventoryCheckin");
#endif

	const auto p = reinterpret_cast<const TPacketCGMountInventoryCheckin*>(c_pData);

	if (!ch || !ch->CanHandleItem())
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR)
		return;
#endif

	CMountInventory* mi = ch->GetMountInventory();
	if (!mi)
		return;

	LPITEM item = ch->GetItem(p->ItemPos);
	if (!item)
		return;

	if (!mi->IsValidPosition(p->wMountPos) || !mi->IsEmpty(p->wMountPos, item->GetSize()))
		return;

	if (ItemSystem::IsItemEquipped(EntityFactory::CreateItemEntity(g_registry, item)) || item->IsExchanging() || item->isLocked())
		return;

#ifdef ENABLE_EXTRA_INVENTORY
	if (item->IsExtraItem())
		return;
#endif

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	if (ItemSystem::GetItemCell(EntityFactory::CreateItemEntity(g_registry, item)) >= ch->Inventory_Size() && IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
#else
	if (ItemSystem::GetItemCell(EntityFactory::CreateItemEntity(g_registry, item)) >= INVENTORY_MAX_NUM && IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
#endif
		return;

	// whitelist (mount_inventory_helper.h)
	if (!CMountInventoryHelper::CanMoveIntoMountInventory(item))
		return;

	// duplikáció tiltás + 18000 group szabály
	{
		const uint32_t vnum = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item));
		const int total = mi->GetWidth() * mi->GetSize();

		for (int i = 0; i < total; ++i)
		{
			LPITEM it = mi->Get(i);
			if (!it)
				continue;

			if (ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, it)) == vnum)
			{
				ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "This mount already added,you can't add two time!");
				return;
			}
		}

		if (vnum >= 18000 && vnum <= 18149)
		{
			const uint32_t group = vnum / 10;
			for (int i = 0; i < total; ++i)
			{
				LPITEM it = mi->Get(i);
				if (!it)
					continue;

				const uint32_t ov = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, it));
				if (ov < 18000 || ov > 18149)
					continue;

				if ((ov / 10) == group)
				{
					ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You already have a belt of this type in your inventory.");
					return;
				}
			}
		}
	}

	// mozgatás
	item->RemoveFromCharacter();

#ifdef ENABLE_EXTRA_INVENTORY
	if (!item->IsDragonSoul() && !item->IsExtraItem())
#else
	if (!item->IsDragonSoul())
#endif
		ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM, p->ItemPos.cell, 255);

	// player item táblából biztosan kimenjen mentésben
	ITEM_MANAGER::instance().FlushDelayedSave(item);

	mi->Add(p->wMountPos, EntityFactory::CreateItemEntity(g_registry, item));
	ch->SendMountInventory();

	// mount bonus frissítés
	ch->ComputePoints();
	ch->PointsPacket();
	// overhead frissítés
#ifdef ENABLE_FAKE_SHOP_HEADER
	ch->UpdateMountCountOverheadToViewers();
#endif
}




void CInputMain::MountInventoryCheckout(LPCHARACTER ch, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate MountInventoryCheckout handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::MountInventoryCheckout");
#endif

	const auto p = reinterpret_cast<const TPacketCGMountInventoryCheckout*>(c_pData);

	if (!ch || !ch->CanHandleItem())
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR)
		return;
#endif

	CMountInventory* mi = ch->GetMountInventory();
	if (!mi)
		return;

	if (!mi->IsValidPosition(p->wMountPos))
		return;

	// mount -> safebox/mall tiltás (belt-szabály)
	if (p->ItemPos.window_type == SAFEBOX || p->ItemPos.window_type == MALL)
		return;

	LPITEM item = mi->Get(p->wMountPos);
	if (!item)
		return;

	if (item->IsExchanging() || item->isLocked())
		return;

	if (!ch->IsEmptyItemGrid(p->ItemPos, item->GetSize()))
		return;

	mi->Remove(p->wMountPos);

	// FONTOS: vissza kell kapcsolni a mentést
	ItemSystem::SetItemSkipSave(EntityFactory::CreateItemEntity(g_registry, item), false);

	item->AddToCharacter(ch, p->ItemPos);
	ITEM_MANAGER::instance().FlushDelayedSave(item);

	// extra biztos flush
	uint32_t dwID = ItemSystem::GetItemID(EntityFactory::CreateItemEntity(g_registry, item));
	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_FLUSH, 0, sizeof(uint32_t));
	db_clientdesc->Packet(&dwID, sizeof(uint32_t));

	ch->SendMountInventory();

	// mount bonus frissítés
	ch->ComputePoints();
	ch->PointsPacket();
	// overhead frissítés
#ifdef ENABLE_FAKE_SHOP_HEADER
	ch->UpdateMountCountOverheadToViewers();
#endif
}



void CInputMain::MountInventoryItemMove(LPCHARACTER ch, const char* data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate MountInventoryItemMove handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::MountInventoryItemMove");
#endif

	const auto p = reinterpret_cast<const TPacketCGMountInventoryItemMove*>(data);

	if (!ch || !ch->CanHandleItem())
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR)
		return;
#endif

	CMountInventory* mi = ch->GetMountInventory();
	if (!mi)
		return;

	mi->MoveItem(p->wMountPos, p->wDestPos);
	ch->SendMountInventory();

	// (count nem változik, de egységes)
	ch->ComputePoints();

#ifdef ENABLE_FAKE_SHOP_HEADER
	ch->UpdateMountCountOverheadToViewers();
#endif
}



#ifdef ENABLE_MAP_TELEPORTER
void CInputMain::MapTeleporter(LPCHARACTER ch, TPacketCGMapTeleporter* pPack)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate MapTeleporter handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::MapTeleporter");//INGAME_DEBUG_RAZOR93
#endif
	if (ch->IsHack() || ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)) || ch->IsOpenSafebox() || ch->IsCubeOpen() || ch->GetShop() || ch->GetMyShop()
#ifdef ENABLE_ACCE_SYSTEM
		|| ch->IsAcceOpen()
#endif
		)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 647, "");
#endif
		return;
	}

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (ch->IsAttrTransferOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 647, "");
#endif
		return;
	}
#endif

	//Check DungeonMap Genezis
	//Check if current map is a dungeon!
//	if (ch->GetDungeon())
//	{
//#ifdef TEXTS_IMPROVEMENT
//		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 48, "");
//#endif
//		return;
//	}

	unsigned int iMapCode = pPack->iMapCode;
	if(iMapCode <0 || iMapCode >= g_vecMapConf.size())
		return;

	TMapConfig& rConf = g_vecMapConf[iMapCode];

	if(ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)) < rConf.iLevel)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 771, "%d", rConf.iLevel);
#endif
		return;
	}

	if(rConf.iLevelMax != 0 && ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)) > rConf.iLevelMax)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 772, "%d", rConf.iLevelMax);
#endif
		return;
	}

	if(ecs::PointSystem::GetGold(AIHelpers::EcsOf(ch)) < rConf.price)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 773, "%d", rConf.price);
#endif
		return;
	}

	ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_GOLD, -rConf.price);

	for (auto itemVnum : rConf.items)
		if (ch->CountSpecifyItem(itemVnum) == 0)
			return;

	for(auto itemVnum : rConf.items)
		ch->RemoveSpecifyItem(itemVnum);

	// int iMapIndex = 0;

	// iMapIndex = rConf.iMapIndex;

	// PIXEL_POSITION pos;
	// SECTREE_MANAGER::instance().GetRecallPositionByEmpire(iMapIndex, ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)), pos);

	// ch->WarpSet(pos.x, pos.y);


	int32_t coord_x = 0;
	int32_t coord_y = 0;

	coord_x = rConf.coord_x;
	coord_y = rConf.coord_y;

	ch->WarpSet(coord_x, coord_y);

}
#endif

// PARTY_JOIN_BUG_FIX
void CInputMain::PartyInvite(LPCHARACTER ch, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyInvite handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::PartyInvite");//INGAME_DEBUG_RAZOR93
#endif
	if (ch->GetArena())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	TPacketCGPartyInvite * p = (TPacketCGPartyInvite*) c_pData;

	LPCHARACTER pInvitee = CHARACTER_MANAGER::instance().Find(p->vid);

	if (!pInvitee || !ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) || !ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pInvitee)))
	{
		LOG_ERROR("PARTY Cannot find invited character");
		return;
	}

	ch->PartyInvite(pInvitee);
}

void CInputMain::PartyInviteAnswer(LPCHARACTER ch, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyInviteAnswer handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::PartyInviteAnswer");//INGAME_DEBUG_RAZOR93
#endif
	if (ch->GetArena())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	TPacketCGPartyInviteAnswer * p = (TPacketCGPartyInviteAnswer*) c_pData;

	LPCHARACTER pInviter = CHARACTER_MANAGER::instance().Find(p->leader_vid);
	if (!pInviter || !ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pInviter))) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 217, "");
#endif
	}
	else if (!p->accept) {
		pInviter->PartyInviteDeny(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
	} else {
		pInviter->PartyInviteAccept(ch);
	}
}
// END_OF_PARTY_JOIN_BUG_FIX

void CInputMain::PartySetState(LPCHARACTER ch, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartySetState handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::PartySetState");//INGAME_DEBUG_RAZOR93
#endif
	if (!CPartyManager::instance().IsEnablePCParty())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 208, "");
#endif
		return;
	}

	TPacketCGPartySetState* p = (TPacketCGPartySetState*) c_pData;

	if (!ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
		return;

	if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->GetLeaderPID() != ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 206, "");
#endif
		return;
	}

	if (!ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->IsMember(p->pid))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 207, "");
#endif
		return;
	}

	uint32_t pid = p->pid;
	LOG_INFO("PARTY SetRole pid {} to role {} state {}", pid, p->byRole, p->flag ? "on" : "off");

	switch (p->byRole)
	{
		case PARTY_ROLE_NORMAL:
			break;

		case PARTY_ROLE_ATTACKER:
		case PARTY_ROLE_TANKER:
		case PARTY_ROLE_BUFFER:
		case PARTY_ROLE_SKILL_MASTER:
		case PARTY_ROLE_HASTE:
		case PARTY_ROLE_DEFENDER:
			if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->SetRole(pid, p->byRole, p->flag))
			{
				TPacketPartyStateChange pack;
				pack.dwLeaderPID = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch));
				pack.dwPID = p->pid;
				pack.bRole = p->byRole;
				pack.bFlag = p->flag;
				db_clientdesc->DBPacket(HEADER_GD_PARTY_STATE_CHANGE, 0, &pack, sizeof(pack));
			}
			break;
		default:
			LOG_ERROR("wrong byRole in PartySetState Packet name {} state {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), p->byRole);
			break;
	}
}

void CInputMain::PartyRemove(LPCHARACTER ch, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyRemove handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::PartyRemove");//INGAME_DEBUG_RAZOR93
#endif
	if (ch->GetArena())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	if (!CPartyManager::instance().IsEnablePCParty())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 208, "");
#endif
		return;
	}

	if (ch->GetDungeon())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 203, "");
#endif
		return;
	}

	TPacketCGPartyRemove* p = (TPacketCGPartyRemove*) c_pData;

	if (!ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
		return;

	LPPARTY pParty = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
	if (pParty->GetLeaderPID() == ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))
	{
		if (!ch->GetDungeon()) {
			// Àû·æ¼º¿¡¼­ ÆÄÆ¼ÀåÀÌ ´øÁ¯ ¹Û¿¡¼­ ÆÄÆ¼ ÇØ»ê ¸øÇÏ°Ô ¸·ÀÚ
			if(pParty->IsPartyInDungeon(351))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 648, "");
#endif
				return;
			}

			// leader can remove any member
			if (p->pid == ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)) || pParty->GetMemberCount() == 2)
			{
				// party disband
				CPartyManager::instance().DeleteParty(pParty);
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				LPCHARACTER B = CHARACTER_MANAGER::instance().FindByPID(p->pid);
				if (B) {
					//pParty->SendPartyRemoveOneToAll(B);
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(B), CHAT_TYPE_INFO, 216, "");
					//pParty->Unlink(B);
					//CPartyManager::instance().SetPartyMember(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(B)), NULL);
				}
#endif
				pParty->Quit(p->pid);
			}
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 205, "");
		}
#endif
	}
	else
	{
		if (p->pid == ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))
		{
			if (!ch->GetDungeon()) {
				if (pParty->GetMemberCount() == 2) {
					CPartyManager::instance().DeleteParty(pParty);
				} else {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 215, "");
#endif
					//pParty->SendPartyRemoveOneToAll(ch);
					pParty->Quit(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
					//pParty->SendPartyRemoveAllToOne(ch);
					//CPartyManager::instance().SetPartyMember(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), NULL);
				}
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 204, "");
			}
#endif
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 197, "");
		}
#endif
	}
}

void CInputMain::AnswerMakeGuild(LPCHARACTER ch, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate AnswerMakeGuild handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::AnswerMakeGuild");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGAnswerMakeGuild* p = (TPacketCGAnswerMakeGuild*) c_pData;

	if (ecs::PointSystem::GetGold(AIHelpers::EcsOf(ch)) < 200000) {
		return;
	}
#ifdef ENABLE_BUG_FIXES
	else if (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)) < 40) {
		return;
	}
#endif

	if (get_global_time() - ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "guild_manage.new_disband_time") < CGuildManager::instance().GetDisbandDelay()) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 181, "%d", quest::CQuestManager::instance().GetEventFlag("guild_disband_delay"));
#endif
		return;
	}

	if (get_global_time() - ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "guild_manage.new_withdraw_time") < CGuildManager::instance().GetWithdrawDelay()) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 179, "%d", quest::CQuestManager::instance().GetEventFlag("guild_withdraw_delay"));
#endif
		return;
	}

	if (ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch)))
		return;

	CGuildManager& gm = CGuildManager::instance();

	TGuildCreateParameter cp;
	memset(&cp, 0, sizeof(cp));

	cp.master = ch;
	strlcpy(cp.name, p->guild_name, sizeof(cp.name));

	if (cp.name[0] == 0 || !check_name(cp.name))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 455, "");
#endif
		return;
	}

	uint32_t dwGuildID = gm.CreateGuild(cp);

	if (dwGuildID)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 125, "%s", cp.name);
#endif
		int GuildCreateFee = 200000;

		ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_GOLD, -GuildCreateFee);
		DBManager::instance().SendMoneyLog(MONEY_LOG_GUILD, ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), -GuildCreateFee);

		char Log[128];
		snprintf(Log, sizeof(Log), "GUILD_NAME %s MASTER %s", cp.name, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
		LogManager::instance().CharLog(ch, 0, "MAKE_GUILD", Log);

		ch->RemoveSpecifyItem(GUILD_CREATE_ITEM_VNUM, 1);
		//ch->SendGuildName(dwGuildID);
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 132, "");
	}
#endif
}

void CInputMain::PartyUseSkill(LPCHARACTER ch, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyUseSkill handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::PartyUseSkill");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGPartyUseSkill* p = (TPacketCGPartyUseSkill*) c_pData;
	if (!ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
		return;

	if (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)) != ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->GetLeaderPID())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 211, "");
#endif
		return;
	}

	switch (p->bySkillIndex)
	{
		case PARTY_SKILL_HEAL:
			ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->HealParty();
			break;
		case PARTY_SKILL_WARP:
			{
				LPCHARACTER pch = CHARACTER_MANAGER::instance().Find(p->vid);
				if (pch) {
					ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->SummonToLeader(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pch)));
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 209, "");
				}
#endif
			}
			break;
	}
}

void CInputMain::PartyParameter(LPCHARACTER ch, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyParameter handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGPartyParameter * p = (TPacketCGPartyParameter *) c_pData;

	if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
		ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->SetParameter(p->bDistributeMode);
}

#ifdef __INGAME_WIKI__
void CInputMain::RecvWikiPacket(LPCHARACTER ch, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate RecvWikiPacket handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::RecvWikiPacket");//INGAME_DEBUG_RAZOR93
#endif
	if (!ch || (ch && !ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))))
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

		ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(buf.read_peek(), buf.size());
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

		ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(buf.read_peek(), buf.size());
	}
}
#endif

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

int CInputMain::Guild(LPCHARACTER ch, const char * data, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Guild handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::int CInputMain::Guild");//INGAME_DEBUG_RAZOR93
#endif
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

	CGuild* pGuild = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch));

	if (nullptr == pGuild)
	{
		if (SubHeader != GUILD_SUBHEADER_CG_GUILD_INVITE_ANSWER)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 138, "");
#endif
			return SubPacketLen;
		}
	}

	switch (SubHeader)
	{
		case GUILD_SUBHEADER_CG_DEPOSIT_MONEY:
			{
				// by mhh : ±æµåÀÚ±ÝÀº ´çºÐ°£ ³ÖÀ» ¼ö ¾ø´Ù.
				return SubPacketLen;

				const int gold = MIN(*reinterpret_cast<const int*>(c_pData), __deposit_limit());

				if (gold < 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 170, "");
#endif
					return SubPacketLen;
				}

				if (ecs::PointSystem::GetGold(AIHelpers::EcsOf(ch)) < gold)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 126, "");
#endif
					return SubPacketLen;
				}

				pGuild->RequestDepositMoney(ch, gold);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_WITHDRAW_MONEY:
			{
				// by mhh : ±æµåÀÚ±ÝÀº ´çºÐ°£ »¬ ¼ö ¾ø´Ù.
				return SubPacketLen;

				const int gold = MIN(*reinterpret_cast<const int*>(c_pData), 500000);

				if (gold < 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 170, "");
#endif
					return SubPacketLen;
				}

				pGuild->RequestWithdrawMoney(ch, gold);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_ADD_MEMBER:
			{
				const uint32_t vid = *reinterpret_cast<const uint32_t*>(c_pData);
				LPCHARACTER newmember = CHARACTER_MANAGER::instance().Find(vid);

				if (!newmember)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 128, "");
#endif
					return SubPacketLen;
				}

				// @fixme145 BEGIN (+newmember ispc check)
				if (!ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)) || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(newmember)))
					return SubPacketLen;
				// @fixme145 END

				pGuild->Invite(ch, newmember);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_REMOVE_MEMBER:
			{
				if (pGuild->UnderAnyWar() != 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 649, "");
#endif
					return SubPacketLen;
				}

				const uint32_t pid = *reinterpret_cast<const uint32_t*>(c_pData);
				const TGuildMember* m = pGuild->GetMember(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));

				if (nullptr == m)
					return -1;

				LPCHARACTER member = CHARACTER_MANAGER::instance().FindByPID(pid);

				if (member)
				{
					if (ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(member)) != pGuild)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 161, "");
#endif
						return SubPacketLen;
					}

					if (!pGuild->HasGradeAuth(m->grade, GUILD_AUTH_REMOVE_MEMBER))
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 139, "");
#endif
						return SubPacketLen;
					}

					ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(member), "guild_manage.new_withdraw_time", get_global_time());
					pGuild->RequestRemoveMember(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(member)));

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
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 139, "");
#endif
						return SubPacketLen;
					}

#ifdef TEXTS_IMPROVEMENT
					if (pGuild->RequestRemoveMember(pid)) {
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 129, "");
					} else {
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 128, "");
					}
#endif
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHANGE_GRADE_NAME:
			{
				char gradename[GUILD_GRADE_NAME_MAX_LEN + 1];
				strlcpy(gradename, c_pData + 1, sizeof(gradename));

				const TGuildMember * m = pGuild->GetMember(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));

				if (nullptr == m)
					return -1;

				if (m->grade != GUILD_LEADER_GRADE) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 175, "");
#endif
				} else if (*c_pData == GUILD_LEADER_GRADE) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 143, "");
#endif
				}
				else if (!check_name(gradename)) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 171, "");
#endif
				}
				else {
					pGuild->ChangeGradeName(*c_pData, gradename);
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHANGE_GRADE_AUTHORITY:
			{
				const TGuildMember* m = pGuild->GetMember(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
				if (nullptr == m)
					return -1;

				if (m->grade != GUILD_LEADER_GRADE) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 174, "");
#endif
				} else if (*c_pData == GUILD_LEADER_GRADE) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 142, "");
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
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 650, "%d", GUILD_MAX_LEVEL);
#endif
				}
				else
				{
					offer /= 100;
					offer *= 100;
#ifdef TEXTS_IMPROVEMENT
					if (pGuild->OfferExp(ch, offer)) {
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 121, "%u", offer);
					} else {
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 122, "");
					}
#endif
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHARGE_GSP:
			{
				const int offer = *reinterpret_cast<const int*>(c_pData);
				const int gold = offer * 100;

				if (offer < 0 || gold < offer || gold < 0 || ecs::PointSystem::GetGold(AIHelpers::EcsOf(ch)) < gold)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 151, "");
#endif
					return SubPacketLen;
				}

#ifdef TEXTS_IMPROVEMENT
				if (!pGuild->ChargeSP(ch, offer)) {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 164, "");
				}
#endif
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_POST_COMMENT:
			{
				const size_t length = *c_pData;

				if (length > GUILD_COMMENT_MAX_LEN)
				{
					// Àß¸øµÈ ±æÀÌ.. ²÷¾îÁÖÀÚ.
					LOG_ERROR("POST_COMMENT: {} comment too long (length: {})", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), length);
					ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->SetPhase(PHASE_CLOSE);
					return -1;
				}

				if (uiBytes < 1 + length)
					return -1;

				const TGuildMember* m = pGuild->GetMember(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));

				if (nullptr == m)
					return -1;

				if (length && !pGuild->HasGradeAuth(m->grade, GUILD_AUTH_NOTICE) && *(c_pData + 1) == '!')
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 127, "");
#endif
				}
				else
				{
					std::string str(c_pData + 1, length);
					pGuild->AddComment(ch, str);
				}

				return (1 + length);
			}

		case GUILD_SUBHEADER_CG_DELETE_COMMENT:
			{
				const uint32_t comment_id = *reinterpret_cast<const uint32_t*>(c_pData);

				pGuild->DeleteComment(ch, comment_id);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_REFRESH_COMMENT:
			pGuild->RefreshComment(ch);
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GRADE:
			{
				const uint32_t pid = *reinterpret_cast<const uint32_t*>(c_pData);
				const uint8_t grade = *(c_pData + sizeof(uint32_t));
				const TGuildMember* m = pGuild->GetMember(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));

				if (nullptr == m)
					return -1;

				if (m->grade != GUILD_LEADER_GRADE) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 176, "");
#endif
				} else if (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)) == pid) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 143, "");
#endif
				} else if (grade == 1) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 141, "");
#endif
				} else {
					pGuild->ChangeMemberGrade(pid, grade);
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_USE_SKILL:
			{
				const TPacketCGGuildUseSkill* p = reinterpret_cast<const TPacketCGGuildUseSkill*>(c_pData);

				pGuild->UseSkill(p->dwVnum, ch, p->dwPID);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GENERAL:
			{
				const uint32_t pid = *reinterpret_cast<const uint32_t*>(c_pData);
				const uint8_t is_general = *(c_pData + sizeof(uint32_t));
				const TGuildMember* m = pGuild->GetMember(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));

				if (nullptr == m)
					return -1;

#ifdef TEXTS_IMPROVEMENT
				if (m->grade != GUILD_LEADER_GRADE) {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 150, "");
				} else if (!pGuild->ChangeMemberGeneral(pid, is_general)) {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 149, "");
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
						g->InviteAccept(ch);
					else
						g->InviteDeny(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
				}
			}
			return SubPacketLen;

	}

	return 0;
}

void CInputMain::Fishing(LPCHARACTER ch, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Fishing handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::Fishin");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGFishing* p = (TPacketCGFishing*)c_pData;
	ch->SetRotation(p->dir * 5);
	ch->fishing();
	return;
}

void CInputMain::ItemGive(LPCHARACTER ch, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemGive handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::ItemGive");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGGiveItem* p = (TPacketCGGiveItem*) c_pData;
	LPCHARACTER to_ch = CHARACTER_MANAGER::instance().Find(p->dwTargetVID);

	if (to_ch) {
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
		if ((ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(to_ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(to_ch)) < GM_IMPLEMENTOR) || (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR)) {
			return;
		}
#endif

		ch->GiveItem(to_ch, p->ItemPos);
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 403, "");
	}
#endif
}

void CInputMain::Hack(LPCHARACTER ch, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Hack handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::Hack");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGHack * p = (TPacketCGHack *) c_pData;

	char buf[sizeof(p->szBuf)];
	strlcpy(buf, p->szBuf, sizeof(buf));

	LOG_ERROR("HACK_DETECT: {} {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), buf);

	// ÇöÀç Å¬¶óÀÌ¾ðÆ®¿¡¼­ ÀÌ ÆÐÅ¶À» º¸³»´Â °æ¿ì°¡ ¾øÀ¸¹Ç·Î ¹«Á¶°Ç ²÷µµ·Ï ÇÑ´Ù
	ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->SetPhase(PHASE_CLOSE);
}

int CInputMain::MyShop(LPCHARACTER ch, const char * c_pData, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate MyShop handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::int CInputMain::MyShop");//INGAME_DEBUG_RAZOR93
#endif
	TPacketCGMyShop * p = (TPacketCGMyShop *) c_pData;
	int iExtraLen = p->bCount * sizeof(TShopItemTable);

	if (uiBytes < sizeof(TPacketCGMyShop) + iExtraLen)
		return -1;

	if (ecs::PointSystem::GetGold(AIHelpers::EcsOf(ch)) >= GOLD_MAX)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 226,
		"%lld"

		, GOLD_MAX);
#endif
		return (iExtraLen);
	}

	if (CombatSystem::IsStun(AIHelpers::EcsOf(ch)) || ch->IsDead())
		return (iExtraLen);

	if (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)) || ch->IsOpenSafebox() || ch->GetShopOwner() || ch->IsCubeOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 292, "");
#endif
		return (iExtraLen);
	}

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (ch->IsAttrTransferOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 292, "");
#endif
		return (iExtraLen);
	}
#endif

	LOG_INFO("MyShop count {}", p->bCount);
	ch->OpenMyShop(p->szSign, (TShopItemTable *) (c_pData + sizeof(TPacketCGMyShop)), p->bCount
#ifdef KASMIR_PAKET_SYSTEM
	, p->dwKasmirNpc, p->bKasmirBaslik
#endif
	);
	return (iExtraLen);
}

void CInputMain::Refine(LPCHARACTER ch, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Refine handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::Refine");//INGAME_DEBUG_RAZOR93
#endif
	const TPacketCGRefine* p = reinterpret_cast<const TPacketCGRefine*>(c_pData);
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR) {
		ch->ClearRefineMode();
		return;
	}
#endif

	if (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)) || ch->IsOpenSafebox() || ch->GetShopOwner() || ch->GetMyShop() || ch->IsCubeOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 502, "");
#endif
		ch->ClearRefineMode();
		return;
	}

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (ch->IsAttrTransferOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 292, "");
#endif
		ch->ClearRefineMode();
		return;
	}
#endif

	if (p->type == 255)
	{
		// DoRefine Cancel
		ch->ClearRefineMode();
		return;
	}

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	if (p->pos >= ch->Inventory_Size())
#else
	if (p->pos >= INVENTORY_MAX_NUM)
#endif
	{
		ch->ClearRefineMode();
		return;
	}

	const entt::entity owner = AIHelpers::EcsOf(ch);
	const entt::entity itemEntity = ItemSystem::GetInventoryItem(owner, p->pos);
	LPITEM item = ItemSystem::GetInventoryItemPtr(AIHelpers::EcsOf(ch), p->pos);


#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	if (!CRefineManager::instance().GetPercentage(ch, p->lLow, p->lMedium, p->lExtra, p->lTotal, item))
	{
		ch->ClearRefineMode();
		return;
	}

	CRefineManager::instance().Increase(ch, p->lLow, p->lMedium, p->lExtra);
#endif

	if (!item)
	{
		ch->ClearRefineMode();
		return;
	}

	ch->SetRefineTime();

	if (p->type == REFINE_TYPE_NORMAL)
	{
		LOG_INFO("refine_type_noraml");
		ItemSystem::DoRefine(owner, itemEntity);
	}
	else if (p->type == REFINE_TYPE_SCROLL || p->type == REFINE_TYPE_HYUNIRON || p->type == REFINE_TYPE_MUSIN || p->type == REFINE_TYPE_BDRAGON)
	{
		LOG_INFO("refine_type_scroll, ...");
		ItemSystem::DoRefineWithScroll(owner, itemEntity);
	}

#ifdef ENABLE_SOUL_SYSTEM
	else if (p->type == REFINE_TYPE_SOUL)
	{
		LOG_INFO("refine_type_soul, ...");
		ItemSystem::DoRefineItemSoul(owner, itemEntity);
	}
#endif
	else if (p->type == REFINE_TYPE_MONEY_ONLY) {
		if (item) {
			if (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "deviltower_zone.can_refine"))
			{
#ifdef ENABLE_BUG_FIXES
				if (ItemSystem::DoRefine(owner, itemEntity, true)) {
					ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "deviltower_zone.can_refine", 0);
				}
#else
				ItemSystem::DoRefine(owner, itemEntity, true);
				ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "deviltower_zone.can_refine", 0);
#endif
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 361, "");
			}
#endif
		}
	}

	ch->ClearRefineMode();
}



#ifdef ENABLE_ACCE_SYSTEM
void CInputMain::Acce(LPCHARACTER pkChar, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Acce handler ECS
// DUAL-PATH: legacy only during migration window

	quest::PC * pPC = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChar)));
	if (pPC->IsRunning())
		return;

	TPacketAcce * sPacket = (TPacketAcce*) c_pData;
	switch (sPacket->subheader)
	{
	case ACCE_SUBHEADER_CG_CLOSE:
	{
		pkChar->CloseAcce();
	}
	break;
	case ACCE_SUBHEADER_CG_ADD:
	{
		pkChar->AddAcceMaterial(sPacket->tPos, sPacket->bPos);
	}
	break;
	case ACCE_SUBHEADER_CG_REMOVE:
	{
		pkChar->RemoveAcceMaterial(sPacket->bPos);
	}
	break;
	case ACCE_SUBHEADER_CG_REFINE:
	{
		pkChar->RefineAcceMaterials();
	}
	break;
	default:
		break;
	}
}
#endif

#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
void CInputMain::CubeRenewalSend(LPCHARACTER ch, const char* data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate CubeRenewalSend handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp::void CInputMain::CubeRenewalSend");//INGAME_DEBUG_RAZOR93
#endif
	struct packet_send_cube_renewal * pinfo = (struct packet_send_cube_renewal *) data;
	switch (pinfo->subheader)
	{
		case CUBE_RENEWAL_SUB_HEADER_MAKE_ITEM:
		{

			int index_item = pinfo->index_item;
			int count_item = pinfo->count_item;
			int index_item_improve = pinfo->index_item_improve;

			Cube_Make(ch,index_item,count_item,index_item_improve);
		}
		break;

		case CUBE_RENEWAL_SUB_HEADER_CLOSE:
		{
			Cube_close(ch);
		}
		break;
	}
}
#endif


#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"
template <class T>
bool CanDecode(T* p, int buffleft) {
	return buffleft >= (int)sizeof(T);
}

template <class T>
const char* Decode(T*& pObj, const char* data, int* pbufferLeng = nullptr, int* piBufferLeft=nullptr)
{
	pObj = (T*) data;

	if(pbufferLeng)
		*pbufferLeng += sizeof(T);

	if(piBufferLeft)
		*piBufferLeft -= sizeof(T);

	return data + sizeof(T);
}

int OfflineshopPacketCreateNewShop(LPCHARACTER ch, const char* data, int iBufferLeft)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: int OfflineshopPacketCreateNewShop");//INGAME_DEBUG_RAZOR93
#endif
	TSubPacketCGShopCreate* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack, data, &iExtra, &iBufferLeft);

	offlineshop::TShopInfo& rShopInfo = pack->shop;

	//fix flooding
	if (rShopInfo.dwCount > 500 || rShopInfo.dwCount == 0) {
		LOG_ERROR("tried to open a shop with 500+ items.");
		return -1;
	}

	std::vector<offlineshop::TShopItemInfo> vec;
	vec.reserve(rShopInfo.dwCount);

	offlineshop::TShopItemInfo* pItem=nullptr;


	for (uint32_t i = 0; i < rShopInfo.dwCount; ++i)
	{
		if(!CanDecode(pItem, iBufferLeft))
			return -1;

		data = Decode(pItem, data, &iExtra, &iBufferLeft);
		vec.push_back(*pItem);
	}

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopCreateNewClientPacket(ch, rShopInfo, vec)) {
		if (ch) {
			offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_CREATE_SHOP);
			ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "RefreshOfflineShop");
		}
	}

	return iExtra;
}


int OfflineshopPacketChangeShopName(LPCHARACTER ch, const char* data, int iBufferLeft)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: int OfflineshopPacketChangeShopName");//INGAME_DEBUG_RAZOR93
#endif
	TSubPacketCGShopChangeName* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopChangeNameClientPacket(ch, pack->szName))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_CHANGE_NAME);

	return iExtra;
}


int OfflineshopPacketForceCloseShop(LPCHARACTER ch, const char* data, int iBufferLeft)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: int OfflineshopPacketForceCloseShop");//INGAME_DEBUG_RAZOR93
#endif
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopForceCloseClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_FORCE_CLOSE);

	return 0;
}


int OfflineshopPacketRequestShopList(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopRequestListClientPacket(ch);
	return 0;
}


int OfflineshopPacketOpenShop(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopOpen* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopOpenClientPacket(ch,pack->dwOwnerID))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_OPEN_SHOP);

	return iExtra;
}


int OfflineshopPacketOpenShowOwner(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopOpenMyShopClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_OPEN_SHOP_OWNER);

	return 0;
}


int OfflineshopPacketBuyItem(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopBuyItem* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopBuyItemClientPacket(ch , pack->dwOwnerID, pack->dwItemID, pack->bIsSearch, pack->TotalPriceSeen)) //patch seen price check
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_BUY_ITEM);

	return iExtra;
}


int OfflineshopPacketAddItem(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGAddItem* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopAddItemClientPacket(ch, pack->pos, pack->price))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_ADD_ITEM);

	return iExtra;
}


int OfflineshopPacketRemoveItem(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGRemoveItem* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopRemoveItemClientPacket(ch, pack->dwItemID))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_REMOVE_ITEM);

	return iExtra;
}


int OfflineshopPacketEditItem(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGEditItem* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopEditItemClientPacket(ch, pack->dwItemID, pack->price))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_EDIT_ITEM);

	return iExtra;
}


int OfflineshopPacketFilterRequest(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGFilterRequest* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopFilterRequestClientPacket(ch, pack->filter))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_FILTER);

	return iExtra;
}


int OfflineshopPacketCreateOffer(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGOfferCreate* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopCreateOfferClientPacket(ch, pack->offer))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_CREATE_OFFER);

	return iExtra;
}


int OfflineshopPacketAcceptOffer(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGOfferAccept* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopAcceptOfferClientPacket(ch, pack->dwOfferID))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_ACCEPT_OFFER);

	return iExtra;
}



int OfflineshopPacketOfferCancel(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGOfferCancel* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopCancelOfferClientPacket(ch, pack->dwOfferID, pack->dwOwnerID))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_ACCEPT_OFFER);

	return iExtra;
}


int OfflineshopPacketOfferListRequest(LPCHARACTER ch)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvOfferListRequestPacket(ch);
	return 0;
}


int OfflineshopPacketOpenSafebox(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopSafeboxOpenClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_OPEN_SAFEBOX);

	return 0;
}


int OfflineshopPacketCloseBoard(LPCHARACTER ch)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvCloseBoardClientPacket(ch);
	return 0;
}

int OfflineshopPacketCloseMyAuction(LPCHARACTER ch)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvCloseMyAuction(ch);
	return 0;
}

int OfflineshopPacketGetItemSafebox(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopSafeboxGetItem* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopSafeboxGetItemClientPacket(ch, pack->dwItemID))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_SAFEBOX_GET_ITEM);

	return iExtra;

}


int OfflineshopPacketGetValutesSafebox(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopSafeboxGetValutes* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopSafeboxGetValutesClientPacket(ch, pack->valutes))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_SAFEBOX_GET_VALUTES);

	return iExtra;
}


int OfflineshopPacketCloseSafebox(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopSafeboxCloseClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_SAFEBOX_CLOSE);

	return 0;
}


int OfflineshopPacketListRequest(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvAuctionListRequestClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_AUCTION_CANNOT_SEND_LIST);

	return 0;
}



int OfflineshopPacketOpenAuctionRequest(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGAuctionOpenRequest* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvAuctionOpenRequestClientPacket(ch, pack->dwOwnerID))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_AUCTION_CANNOT_OPEN_AUCTION);

	return iExtra;
}



int OfflineshopPacketOpenMyAuctionRequest(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvMyAuctionOpenRequestClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_AUCTION_CANNOT_SEND_LIST);

	return 0;
}



int OfflineshopPacketCreateAuction(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGAuctionCreate* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvAuctionCreateClientPacket(ch, pack->dwDuration, pack->init_price, pack->pos))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_AUCTION_CANNOT_CREATE_AUCTION);

	return iExtra;
}



int OfflineshopPacketAddOffer(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGAuctionAddOffer* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvAuctionAddOfferClientPacket(ch, pack->dwOwnerID, pack->price))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_AUCTION_CANNOT_ADD_OFFER);

	return iExtra;
}



int OfflineshopPacketExitFromAuction(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGAuctionExitFrom* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvAuctionExitFromAuction(ch, pack->dwOwnerID);
	return iExtra;
}


#ifdef ENABLE_NEW_SHOP_IN_CITIES
int OfflineshopPacketClickEntity(LPCHARACTER ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopClickEntity* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack, data, &iExtra, &iBufferLeft);


	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopClickEntity(ch, pack->dwShopVID);
	return iExtra;
}

#endif



int OfflineshopPacket(const char* data , LPCHARACTER ch, int32_t iBufferLeft)
{
	unsigned int iBufferLeftCompare = iBufferLeft;
	if(iBufferLeftCompare < sizeof(TPacketCGNewOfflineShop))
		return -1;

	TPacketCGNewOfflineShop* pPack=nullptr;
	iBufferLeft -= sizeof(TPacketCGNewOfflineShop);
	data = Decode(pPack, data);



	switch (pPack->bSubHeader)
	{

	case offlineshop::SUBHEADER_CG_SHOP_CREATE_NEW:				return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketCreateNewShop(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_CHANGE_NAME:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketChangeShopName(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_FORCE_CLOSE:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketForceCloseShop(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_REQUEST_SHOPLIST:		return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketRequestShopList(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_OPEN:					return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOpenShop(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_OPEN_OWNER:				return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOpenShowOwner(ch,data,iBufferLeft);

	case offlineshop::SUBHEADER_CG_SHOP_BUY_ITEM:				return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketBuyItem(ch, data , iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_ADD_ITEM:				return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketAddItem(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_REMOVE_ITEM:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketRemoveItem(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_EDIT_ITEM:				return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketEditItem(ch,data,iBufferLeft);

	case offlineshop::SUBHEADER_CG_SHOP_FILTER_REQUEST:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketFilterRequest(ch,data,iBufferLeft);

	case offlineshop::SUBHEADER_CG_SHOP_OFFER_CREATE:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketCreateOffer(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_OFFER_ACCEPT:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketAcceptOffer(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_REQUEST_OFFER_LIST:		return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOfferListRequest(ch);
	case offlineshop::SUBHEADER_CG_SHOP_OFFER_CANCEL:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOfferCancel(ch, data,iBufferLeft);

	case offlineshop::SUBHEADER_CG_SHOP_SAFEBOX_OPEN:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOpenSafebox(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_SAFEBOX_GET_ITEM:		return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketGetItemSafebox(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_SAFEBOX_GET_VALUTES:	return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketGetValutesSafebox(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_SAFEBOX_CLOSE:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketCloseSafebox(ch,data,iBufferLeft);

	case offlineshop::SUBHEADER_CG_AUCTION_LIST_REQUEST:		return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketListRequest(ch, data, iBufferLeft);
	case offlineshop::SUBHEADER_CG_AUCTION_OPEN_REQUEST:		return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOpenAuctionRequest(ch, data, iBufferLeft);
	case offlineshop::SUBHEADER_CG_MY_AUCTION_OPEN_REQUEST:		return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOpenMyAuctionRequest(ch, data, iBufferLeft);
	case offlineshop::SUBHEADER_CG_CREATE_AUCTION:				return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketCreateAuction(ch, data, iBufferLeft);
	case offlineshop::SUBHEADER_CG_AUCTION_ADD_OFFER:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketAddOffer(ch, data, iBufferLeft);
	case offlineshop::SUBHEADER_CG_EXIT_FROM_AUCTION:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketExitFromAuction(ch, data, iBufferLeft);

	case offlineshop::SUBHEADER_CG_CLOSE_BOARD:					return /*sizeof(TPacketCGNewOfflineshop) +*/ OfflineshopPacketCloseBoard(ch);
#ifdef ENABLE_NEW_SHOP_IN_CITIES
	case offlineshop::SUBHEADER_CG_CLICK_ENTITY:				return /*sizeof(TPacketCGNewOfflineshop) +*/ OfflineshopPacketClickEntity(ch, data, iBufferLeft);
#endif
	case offlineshop::SUBHEADER_CG_AUCTION_CLOSE:
		return /*sizeof(TPacketCGNewOfflineshop) +*/ OfflineshopPacketCloseMyAuction(ch);

	default:
		LOG_ERROR("UNKNOWN SUBHEADER {} ", pPack->bSubHeader);
		return -1;
	}

}
#endif

void CInputMain::ItemDestroy(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemDestroy handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::ItemDestroy ");//INGAME_DEBUG_RAZOR93
#endif
	struct command_item_destroy * pinfo = (struct command_item_destroy *) data;
	if (ch) {
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
		if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR) {
			return;
		}
#endif
		ch->DestroyItem(pinfo->Cell);
	}
}

void CInputMain::ItemDivision(LPCHARACTER ch, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemDivision handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::ItemDivision ");//INGAME_DEBUG_RAZOR93
#endif
	struct command_item_division * pinfo = (struct command_item_division *) data;
	if (ch)
		ch->ItemDivision(pinfo->pos);
}




#ifdef ENABLE_NEW_FISHING_SYSTEM
void CInputMain::FishingNew(LPCHARACTER ch, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate FishingNew handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::FishingNew ");//INGAME_DEBUG_RAZOR93
#endif
	if (!ch) {
		return;
	}

	TPacketFishingNew* p = (TPacketFishingNew*)c_pData;
	switch (p->subheader) {
		case FISHING_SUBHEADER_NEW_START:
			{
				ch->SetRotation(p->dir * 5);
				ch->fishing_new_start();
			}
			break;
		case FISHING_SUBHEADER_NEW_STOP:
			{
				ch->SetRotation(p->dir * 5);
				ch->fishing_new_stop();
			}
			break;
		case FISHING_SUBHEADER_NEW_CATCH:
			{
				ch->fishing_new_catch();
			}
			break;
		case FISHING_SUBHEADER_NEW_CATCH_FAILED:
			{
				ch->fishing_new_catch_failed();
			}
			break;
		default:
			return;
	}
}
#endif

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
void CInputMain::WheelDestiny(LPCHARACTER ch, const char* data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate WheelDestiny handler ECS
// DUAL-PATH: legacy only during migration window
	if (!ch)
	{
		return;
	}

	if (ch->IsObserverMode() || ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)))
	{
		return;
	}

	const auto pinfo = reinterpret_cast<const TPacketCGWheelDestiny*>(data);
	enum { OPEN, CLOSE, TURN, GIVE };

	switch (pinfo->option)
	{
	case OPEN:
	{

		if (!ch->GetWheelDestiny())
		{
			ch->SetWheelDestiny(std::make_shared<CWheelDestiny>(ch));
		}
	}
	break;
	case CLOSE:

	{
		if (ch->GetWheelDestiny())
		{
			//if (ch->GetWheelDestiny()->IsTurning())
			//{
			//	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Do not close now!!");
			//	return;
			//}


			if (ch->GetWheelDestiny()->GetGiftVnum())
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 1307, "");
#endif
			}
			else
			{
				ch->SetWheelDestiny(nullptr);
				ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "BINARY_WHEEL_CLOSE");
			}
		}
	}
	break;
	case TURN:
	{
		if (ch->GetDungeon() != nullptr || ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)) >= 10000)
		{
			ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Dungeonban nem tudsz pörgetni./You cannot in dungeon");
			return;
		}
		if (ch->GetWheelDestiny())
		{
			static const uint32_t WHEEL_TICKET_VNUM = 70610;

			if (ch->CountSpecifyItem(WHEEL_TICKET_VNUM) < 1)
			{

				ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You Dont have Battle Pass Ticket");
				return;
			}

			ch->RemoveSpecifyItem(WHEEL_TICKET_VNUM, 1);

			ch->GetWheelDestiny()->TurnWheel();
		}
	}
	break;

	case GIVE:
	{
		if (ch->GetWheelDestiny())
		{
			ch->GetWheelDestiny()->GiveMyFuckingGift();
		}
	}
	break;
	default:
	{
		LOG_ERROR("CInputMain::WheelDestiny : Unknown option {} : {}", pinfo->option, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
	}
	break;
	}
}
#endif

int CInputMain::Analyze(LPDESC d, uint8_t bHeader, const char * c_pData)
{

	LPCHARACTER ch;

	if (!(ch = d->GetCharacter()))
	{
		LOG_ERROR("no character on desc");
		d->SetPhase(PHASE_CLOSE);
		return (0);
	}

	int iExtraLen = 0;

	if (test_server && bHeader != HEADER_CG_MOVE)
		LOG_INFO("CInputMain::Analyze() ==> Header [{}] ", bHeader);

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_TIME_SYNC:
			Handshake(d, c_pData);
			break;

		case HEADER_CG_CHAT:
			if (test_server)
			{
				const auto pBuf = const_cast<char*>(c_pData);
				LOG_INFO("{}", pBuf + sizeof(TPacketCGChat));
			}

			if ((iExtraLen = Chat(ch, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_WHISPER:
			if ((iExtraLen = Whisper(ch, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_MOVE:
			Move(ch, c_pData);
			// @fixme103 (removed CheckClientVersion since useless in here)
			break;

		case HEADER_CG_CHARACTER_POSITION:
			Position(ch, c_pData);
			break;

		case HEADER_CG_ITEM_USE:
			if (!ch->IsObserverMode())
				ItemUse(ch, c_pData);
			break;

		case HEADER_CG_ITEM_DROP:
			if (!ch->IsObserverMode())
			{
				ItemDrop(ch, c_pData);
			}
			break;

#ifdef ENABLE_ACCE_SYSTEM
		case HEADER_CG_ACCE:
			Acce(ch, c_pData);
			break;
#endif

		case HEADER_CG_ITEM_DROP2:
			if (!ch->IsObserverMode())
				ItemDrop2(ch, c_pData);
			break;
		case HEADER_CG_ITEM_DESTROY:
			if (!ch->IsObserverMode())
				ItemDestroy(ch, c_pData);
			break;
		case HEADER_CG_ITEM_DIVISION:
			{
				if (!ch->IsObserverMode())
					ItemDivision(ch, c_pData);
			}
			break;
		case HEADER_CG_ITEM_MOVE:
			if (!ch->IsObserverMode())
				ItemMove(ch, c_pData);
			break;


#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
		case ENVANTER_BLACK:
			if (!ch->IsObserverMode())
				InventoryExpansion(ch, c_pData);
		break;
#endif

		case HEADER_CG_ITEM_PICKUP:
			if (!ch->IsObserverMode())
				ItemPickup(ch, c_pData);
			break;

		case HEADER_CG_ITEM_USE_TO_ITEM:
			if (!ch->IsObserverMode())
				ItemToItem(ch, c_pData);
			break;

		case HEADER_CG_ITEM_GIVE:
			if (!ch->IsObserverMode())
				ItemGive(ch, c_pData);
			break;

		case HEADER_CG_EXCHANGE:
			if (!ch->IsObserverMode())
				Exchange(ch, c_pData);
			break;

		case HEADER_CG_ATTACK:
		case HEADER_CG_SHOOT:
			if (!ch->IsObserverMode())
			{
				Attack(ch, bHeader, c_pData);
			}
			break;

		case HEADER_CG_USE_SKILL:
			if (!ch->IsObserverMode())
				UseSkill(ch, c_pData);
			break;

#ifdef __SKILL_COLOR_SYSTEM__
		case HEADER_CG_SKILL_COLOR:
			SetSkillColor(ch, c_pData);
			break;
#endif
#ifdef ENABLE_OPENSHOP_PACKET
		case HEADER_CG_OPENSHOP: {
				TPacketOpenShop* p = reinterpret_cast<TPacketOpenShop*>((void*)c_pData);
				if (p->shopid > 0) {
					if (!(ch->IsObserverMode() || ch->IsOpenSafebox() || ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)) || ch->IsCubeOpen() || CombatSystem::IsStun(AIHelpers::EcsOf(ch)) || ch->IsDead()
#ifdef __ATTR_TRANSFER_SYSTEM__
						 || ch->IsAttrTransferOpen()
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
						 || ch->GetOfflineShopGuest() || ch->GetAuctionGuest()
#endif
					)) {
						LPSHOP shop = CShopManager::instance().Get(p->shopid);
						if (shop) {
							shop->AddGuest(ch, 0, false);
							ch->SetShopOwner(nullptr);
						}
					}
				}
			} break;
#endif
		case HEADER_CG_QUICKSLOT_ADD:
			QuickslotAdd(ch, c_pData);
			break;

		case HEADER_CG_QUICKSLOT_DEL:
			QuickslotDelete(ch, c_pData);
			break;

		case HEADER_CG_QUICKSLOT_SWAP:
			QuickslotSwap(ch, c_pData);
			break;

		case HEADER_CG_SHOP:
			if ((iExtraLen = Shop(ch, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_MESSENGER:
			if ((iExtraLen = Messenger(ch, c_pData, m_iBufferLeft))<0)
				return -1;
			break;

#ifdef ENABLE_BATTLE_PASS
		case HEADER_CG_BATTLE_PASS:
			if ((iExtraLen = BattlePass(ch, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;
#endif

		case HEADER_CG_ON_CLICK:
			OnClick(ch, c_pData);
			break;

		case HEADER_CG_SYNC_POSITION:
			if ((iExtraLen = SyncPosition(ch, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_ADD_FLY_TARGETING:
		case HEADER_CG_FLY_TARGETING:
			FlyTarget(ch, c_pData, bHeader);
			break;

		case HEADER_CG_SCRIPT_BUTTON:
			ScriptButton(ch, c_pData);
			break;

			// SCRIPT_SELECT_ITEM
		case HEADER_CG_SCRIPT_SELECT_ITEM:
			ScriptSelectItem(ch, c_pData);
			break;
			// END_OF_SCRIPT_SELECT_ITEM

		case HEADER_CG_SCRIPT_ANSWER:
			ScriptAnswer(ch, c_pData);
			break;

		case HEADER_CG_QUEST_INPUT_STRING:
			QuestInputString(ch, c_pData);
			break;

		case HEADER_CG_QUEST_CONFIRM:
			QuestConfirm(ch, c_pData);
			break;

		case HEADER_CG_TARGET:
			Target(ch, c_pData);
			break;

		case HEADER_CG_WARP:
			Warp(ch, c_pData);
			break;

		case HEADER_CG_SAFEBOX_CHECKIN:
			SafeboxCheckin(ch, c_pData);
			break;

		case HEADER_CG_SAFEBOX_CHECKOUT:
			SafeboxCheckout(ch, c_pData, false);
			break;

		case HEADER_CG_SAFEBOX_ITEM_MOVE:
			SafeboxItemMove(ch, c_pData);
			break;

		case HEADER_CG_MALL_CHECKOUT:
			SafeboxCheckout(ch, c_pData, true);
			break;


		case HEADER_CG_MOUNT_INVENTORY_CHECKIN:
			MountInventoryCheckin(ch, c_pData);
			break;

		case HEADER_CG_MOUNT_INVENTORY_CHECKOUT:
			MountInventoryCheckout(ch, c_pData);
			break;

		case HEADER_CG_MOUNT_INVENTORY_ITEM_MOVE:
			MountInventoryItemMove(ch, c_pData);
			break;

		case HEADER_CG_PARTY_INVITE:
			PartyInvite(ch, c_pData);
			break;

		case HEADER_CG_PARTY_REMOVE:
			PartyRemove(ch, c_pData);
			break;

		case HEADER_CG_PARTY_INVITE_ANSWER:
			PartyInviteAnswer(ch, c_pData);
			break;

		case HEADER_CG_PARTY_SET_STATE:
			PartySetState(ch, c_pData);
			break;

		case HEADER_CG_PARTY_USE_SKILL:
			PartyUseSkill(ch, c_pData);
			break;

		case HEADER_CG_PARTY_PARAMETER:
			PartyParameter(ch, c_pData);
			break;
#ifdef __INGAME_WIKI__
		case InGameWiki::HEADER_CG_WIKI:
			RecvWikiPacket(ch, c_pData);
			break;
#endif
		case HEADER_CG_ANSWER_MAKE_GUILD:
#ifdef ENABLE_NEWGUILDMAKE
			ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "<%s> AnswerMakeGuild disabled", __FUNCTION__);
#else
			AnswerMakeGuild(ch, c_pData);
#endif
			break;

		case HEADER_CG_GUILD:
			if ((iExtraLen = Guild(ch, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_FISHING:
			Fishing(ch, c_pData);
			break;
		case HEADER_CG_HACK:
			Hack(ch, c_pData);
			break;

#ifdef __NEWPET_SYSTEM__
		case HEADER_CG_PetSetName:
			BraveRequestPetName(ch, c_pData);
			break;
#endif
		case HEADER_CG_MYSHOP:
			if ((iExtraLen = MyShop(ch, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_REFINE:
			Refine(ch, c_pData);
			break;

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
		case HEADER_CG_WHISPER_ADMIN:
			CWhisperAdmin::instance().Manager(ch, c_pData);
			break;
#endif

		case HEADER_CG_CLIENT_VERSION:
			Version(ch, c_pData);
			break;


#ifdef ENABLE_MULTI_LANGUAGE
		case HEADER_CG_CHANGE_LANGUAGE:
			{
				TPacketChangeLanguage* p = reinterpret_cast <TPacketChangeLanguage*>((void*)c_pData);
				ChangeLanguage(ch, p->bLanguage);
			}
			break;
		case HEADER_CG_REQUEST_LANGUAGE:
			{
				TPacketRequestLang* p = reinterpret_cast <TPacketRequestLang*>((void*)c_pData);
				RequestLanguage(ch, p->targetName);
			}
			break;
#endif
#ifdef __SEND_TARGET_INFO__
		case HEADER_CG_TARGET_INFO_LOAD:
			{
				TargetInfoLoad(ch, c_pData);
			}
			break;
#endif
#ifdef ENABLE_SWITCHBOT
		case HEADER_CG_SWITCHBOT:
			if ((iExtraLen = Switchbot(ch, c_pData, m_iBufferLeft)) < 0)
			{
				return -1;
			}
			break;
#endif
#ifdef ENABLE_MAP_TELEPORTER
		case HEADER_CG_MAP_TELEPORTER:
			MapTeleporter(ch, (TPacketCGMapTeleporter*) c_pData);
			break;
#endif
		case HEADER_CG_DRAGON_SOUL_REFINE:
			{
				TPacketCGDragonSoulRefine* p = reinterpret_cast <TPacketCGDragonSoulRefine*>((void*)c_pData);
				switch(p->bSubType)
				{
				case DS_SUB_HEADER_CLOSE:
					ch->DragonSoul_RefineWindow_Close();
					break;
				case DS_SUB_HEADER_DO_REFINE_GRADE:
					{
						DSManager::instance().DoRefineGradeEcs(AIHelpers::EcsOf(ch), p->ItemGrid);
					}
					break;
				case DS_SUB_HEADER_DO_REFINE_STEP:
					{
						DSManager::instance().DoRefineStepEcs(AIHelpers::EcsOf(ch), p->ItemGrid);
					}
					break;
				case DS_SUB_HEADER_DO_REFINE_STRENGTH:
					{
						DSManager::instance().DoRefineStrengthEcs(AIHelpers::EcsOf(ch), p->ItemGrid);
					}
					break;
				}
			}
			break;
#ifdef ENABLE_DS_REFINE_ALL
		case HEADER_CG_DRAGON_SOUL_REFINE_ALL: {
			TPacketDragonSoulRefineAll* p = reinterpret_cast <TPacketDragonSoulRefineAll*>((void*)c_pData);
			DSManager::instance().DoRefineAllEcs(AIHelpers::EcsOf(ch), p->subheader, p->type, p->grade);
		} break;
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
		case HEADER_CG_NEW_OFFLINESHOP:
			if((iExtraLen = OfflineshopPacket(c_pData, ch, m_iBufferLeft))< 0)
				return -1;
			break;
#endif
#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
		case HEADER_CG_CUBE_RENEWAL:
			CubeRenewalSend(ch, c_pData);
			break;
#endif
#ifdef ENABLE_NEW_FISHING_SYSTEM
		case HEADER_CG_FISHING_NEW:
			{
				FishingNew(ch, c_pData);
			}
			break;
#endif
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
		case HEADER_CG_WHEEL_DESTINY:
		{
			WheelDestiny(ch, c_pData);
		}
		break;
#endif
	}
	return (iExtraLen);
}

int CInputDead::Analyze(LPDESC d, uint8_t bHeader, const char * c_pData)
{
	LPCHARACTER ch;

	if (!(ch = d->GetCharacter()))
	{
		LOG_ERROR("no character on desc");
		return 0;
	}

	int iExtraLen = 0;

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_TIME_SYNC:
			Handshake(d, c_pData);
			break;

		case HEADER_CG_CHAT:
			if ((iExtraLen = Chat(ch, c_pData, m_iBufferLeft)) < 0)
				return -1;

			break;

		case HEADER_CG_WHISPER:
			if ((iExtraLen = Whisper(ch, c_pData, m_iBufferLeft)) < 0)
				return -1;

			break;

		case HEADER_CG_HACK:
			Hack(ch, c_pData);
			break;

		default:
			return (0);
	}

	return (iExtraLen);
}
#ifdef ENABLE_SWITCHBOT
int CInputMain::Switchbot(LPCHARACTER ch, const char* data, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Switchbot handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: int CInputMain::Switchbot ");//INGAME_DEBUG_RAZOR93
#endif
	const TPacketCGSwitchbot* p = reinterpret_cast<const TPacketCGSwitchbot*>(data);

	if (uiBytes < sizeof(TPacketCGSwitchbot))
	{
		return -1;
	}

	const char* c_pData = data + sizeof(TPacketCGSwitchbot);
	uiBytes -= sizeof(TPacketCGSwitchbot);

	switch (p->subheader)
	{
	case SUBHEADER_CG_SWITCHBOT_START:
	{
		size_t extraLen = sizeof(TSwitchbotAttributeAlternativeTable) * SWITCHBOT_ALTERNATIVE_COUNT;
		if (uiBytes < extraLen)
		{
			return -1;
		}

		std::vector<TSwitchbotAttributeAlternativeTable> vec_alternatives;

		for (uint8_t alternative = 0; alternative < SWITCHBOT_ALTERNATIVE_COUNT; ++alternative)
		{
			const TSwitchbotAttributeAlternativeTable* pAttr = reinterpret_cast<const TSwitchbotAttributeAlternativeTable*>(c_pData);
			c_pData += sizeof(TSwitchbotAttributeAlternativeTable);

			vec_alternatives.emplace_back(*pAttr);
		}

		CSwitchbotManager::Instance().Start(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), p->slot, vec_alternatives);
		return extraLen;
	}

	case SUBHEADER_CG_SWITCHBOT_STOP:
	{
		CSwitchbotManager::Instance().Stop(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), p->slot);
		return 0;
	}
	}

	return 0;
}
#endif



#ifdef ENABLE_MULTI_LANGUAGE
void CInputMain::ChangeLanguage(LPCHARACTER ch, uint8_t bLanguage)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ChangeLanguage handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::ChangeLanguage ");//INGAME_DEBUG_RAZOR93
#endif
	if (!ch)
		return;

	if (!ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
		return;

	uint8_t bCurrentLanguage = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetLanguage();

	if(bCurrentLanguage == bLanguage)
		return;

	if(bLanguage > LANGUAGE_DEFAULT && bLanguage < LANGUAGE_MAX_NUM)
	{
		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE account.account SET language = %d WHERE id = %d;", bLanguage, ch->GetAID()));
		ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->SetLanguage(bLanguage);
	}
}

void CInputMain::RequestLanguage(LPCHARACTER ch, const char* targetName)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate RequestLanguage handler ECS
// DUAL-PATH: legacy only during migration window
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::RequestLanguage ");//INGAME_DEBUG_RAZOR93
#endif
	if (!ch)
		return;

	LPDESC d = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));
	if (!d)
		return;

	int id = 0;
	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT account_id FROM player.player WHERE name='%s'", targetName));
	if (pMsg->Get()->uiNumRows != 0) {
		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
		id = atoi(row[0]);
	}

	if (id == 0)
		return;

	std::unique_ptr<SQLMsg> pMsg2(DBManager::instance().DirectQuery("SELECT language FROM account.account WHERE id=%d", id));
	if (pMsg2->Get()->uiNumRows != 0) {
		MYSQL_ROW row = mysql_fetch_row(pMsg2->Get()->pSQLResult);

		TPacketRecvLang p;
		p.bHeader = HEADER_GC_RECV_LANGUAGE;
		strncpy(p.targetName, targetName, sizeof(p.targetName));
		strncpy(p.targetLanguage, row[0], sizeof(p.targetLanguage));
		d->Packet(&p, sizeof(p));
	}
}
#endif




