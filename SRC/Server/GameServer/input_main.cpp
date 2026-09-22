#include "stdafx.h"
#include "ecs/systems/InventorySystem.hpp"
#include "ecs/systems/SessionSystem.hpp"
#include "ecs/systems/MountSystem.hpp"
#include "ecs/systems/ViewSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/AcceSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/systems/SkillSystem.hpp"
#include "ecs/services/SpatialService.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/NetworkSyncSystem.hpp"
#include "ecs/CharacterAccessors.hpp"


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
#include "ecs/components/identity_components.hpp"
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
#include "ecs/systems/MountSystem.hpp"
#include "ecs/systems/ActivitySystem.hpp"
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
#include "ecs/systems/DragonSoulSystem.hpp"

#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
static inline std::string MakeNameWithPrefix(entt::entity chEntity)
{
	const bool valid = ecs::PlayerRuntime::IsValid(chEntity);
	const char* name = valid ? ecs::PlayerRuntime::GetName(chEntity).data() : "";

	std::string out;
	if (valid)
		out = NetworkSyncSystem::GetItemOnTitlePrefix(g_registry, chEntity); // std::string


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
	return (1000*10000); // 1õ��
}
#ifdef __SEND_TARGET_INFO__
void CInputMain::TargetInfoLoad(entt::entity character, const char* c_pData)
{
	if (!ecs::IsCharacter(character))
		return;

	const auto* request = reinterpret_cast<const TPacketCGTargetInfoLoad*>(c_pData);
	const entt::entity targetEntity = CHARACTER_MANAGER::instance().FindEntity(request->dwVID);

	if (!ecs::IsCharacter(targetEntity) || (ecs::PlayerRuntime::GetCharType(targetEntity) != CHAR_TYPE_MONSTER && !ecs::PlayerRuntime::IsStone(targetEntity)))
		return;

	std::vector<TargetInfoItem> items;
	if (!ITEM_MANAGER::instance().CreateDropItemVector(targetEntity, character, items))
		return;

	TPacketGCTargetInfo info{};
	info.header = HEADER_GC_TARGET_INFO;
	info.dwVID = ecs::PlayerRuntime::GetPacketVID(targetEntity);
	info.race = ecs::PlayerRuntime::GetRaceNum(targetEntity);

	for (const TargetInfoItem& item : items)
	{
		info.dwVnum = item.vnum;
		info.count = item.count;
		ecs::PlayerRuntime::GetDesc(character)->Packet(&info, sizeof(info));
	}
}
#endif
static void SendBlockChatInfo(entt::entity chEntity, int sec)
{
	if (sec <= 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 473, "");
#endif
		return;
	}

#ifdef TEXTS_IMPROVEMENT
	int32_t hour = sec / 3600;
	sec -= hour * 3600;
	int32_t min = (sec / 60);
	sec -= min * 60;
	if (hour > 0 && min > 0) {
		ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 475, "%d#%d#%d", hour, min, sec);
	}
	else if (hour > 0 && min == 0) {
		ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 476, "%d#%d", hour, sec);
	}
	else if (hour == 0 && min > 0) {
		ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 477, "%d#%d", min, sec);
	}
	else {
		ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 478, "%d", sec);
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

static bool SpamBlockCheck(entt::entity chEntity, const char* const buf, const size_t buflen)
{
	if (!ecs::PlayerRuntime::IsPC(chEntity) || !ecs::PlayerRuntime::GetDesc(chEntity))
		return true;
	if (ecs::PointSystem::GetLevel(chEntity) < g_iSpamBlockMaxLevel)
	{
		auto it = spam_score_of_ip.find(ecs::PlayerRuntime::GetDesc(chEntity)->GetHostName());

		if (it == spam_score_of_ip.end())
		{
			spam_score_of_ip.insert(std::make_pair(ecs::PlayerRuntime::GetDesc(chEntity)->GetHostName(), std::make_pair(0, (LPEVENT)nullptr)));
			it = spam_score_of_ip.find(ecs::PlayerRuntime::GetDesc(chEntity)->GetHostName());
		}

		if (it->second.second)
		{
			SendBlockChatInfo(chEntity, event_time(it->second.second) / passes_per_sec);
			return true;
		}

		unsigned int score;
		const char * word = SpamManager::instance().GetSpamScore(buf, buflen, score);

		it->second.first += score;

		if (word)
			LOG_INFO("SPAM_SCORE: {} text: {} score: {} total: {} word: {}", ecs::PlayerRuntime::GetName(chEntity).data(), buf, score, it->second.first, word);

		if (it->second.first >= g_uiSpamBlockScore)
		{
			spam_event_info* info = AllocEventInfo<spam_event_info>();
			strlcpy(info->host, ecs::PlayerRuntime::GetDesc(chEntity)->GetHostName(), sizeof(info->host));

			it->second.second = event_create(block_chat_by_ip_event, info, PASSES_PER_SEC(g_uiSpamBlockDuration));
			LOG_INFO("SPAM_IP: {} for {} seconds", info->host, g_uiSpamBlockDuration);

			LogManager::instance().CharLog(chEntity, 0, "SPAM", word);

			SendBlockChatInfo(chEntity, event_time(it->second.second) / passes_per_sec);

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

	if (*cur == '|') // ||�� |�� ǥ���Ѵ�.
	{
		tagLen = 2;
		return TEXT_TAG_TAG;
	}
	else if (*cur == 'c') // color |cffffffffblahblah|r
	{
		tagLen = 2;
		return TEXT_TAG_COLOR;
	}
	else if (*cur == 'H') // hyperlink |Hitem:10000:0:0:0:0|h[�̸�]|h
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

static int ProcessTextTag(entt::entity character, const char * c_pszText, uint64_t len)
{
	if (!ecs::PlayerRuntime::IsPC(character))
		return 4;
	//���λ����߿� �ݰ����� ����� ���
	//0 : ���������� ���
	//1 : �ݰ��� ����
	//2 : �ݰ����� ������, ���λ������� �����
	//3 : ��Ÿ
	//4 : ����
	int hyperlinks;
	bool colored;

	GetTextTagInfo(c_pszText, len, hyperlinks, colored);

	if (colored == true && hyperlinks == 0)
		return 4;

#ifdef ENABLE_NEWSTUFF
	if (g_bDisablePrismNeed)
		return 0;
#endif
	int nPrismCount = ItemSystem::CountItem(character, ITEM_PRISM);

	if (nPrismCount < hyperlinks)
		return 1;


	if (ecs::SocialSystem::GetMyShop(character) == entt::null)
	{
		if (hyperlinks > 0 && !ItemSystem::RemoveSpecifyItemEcs(character, ITEM_PRISM, hyperlinks))
			return 1;
		return 0;
	} else
	{
		int sellingNumber = ShopSystem::GetNumberByVnum(ecs::SocialSystem::GetMyShop(character), ITEM_PRISM);
		if(nPrismCount - sellingNumber < hyperlinks)
		{
			return 2;
		} else
		{
			if (hyperlinks > 0 && !ItemSystem::RemoveSpecifyItemEcs(character, ITEM_PRISM, hyperlinks))
				return 1;
			return 0;
		}
	}

	return 4;
}

int CInputMain::Whisper(entt::entity character, const char * data, uint64_t uiBytes)
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
		ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);
		return -1;
	}

	if (AffectSystem::FindAffect(character, AFFECT_BLOCK_CHAT))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 639, "");
#endif
		return (iExtraLen);
	}

	entt::entity chr = CHARACTER_MANAGER::instance().FindPCEntity(pinfo->szNameTo);
	if (!ecs::IsCharacter(chr))
		chr = entt::null;


	if (chr == (ecs::IsCharacter(character) ? character : entt::null))
		return (iExtraLen);

	LPDESC pkDesc = nullptr;

	uint8_t bOpponentEmpire = 0;

	if (test_server)
	{
		if (chr == entt::null)
			LOG_INFO("Whisper to {}({}) from {}", "Null", pinfo->szNameTo, ecs::PlayerRuntime::GetName(character).data());
		else
			LOG_INFO("Whisper to {}({}) from {}", ecs::PlayerRuntime::GetName(chr).data(), pinfo->szNameTo, ecs::PlayerRuntime::GetName(character).data());
	}

	if (ecs::PlayerRuntime::IsBlockMode(character, BLOCK_WHISPER))
	{
		if (ecs::PlayerRuntime::GetDesc(character))
		{
			TPacketGCWhisper pack;
			pack.bHeader = HEADER_GC_WHISPER;
			pack.bType = WHISPER_TYPE_SENDER_BLOCKED;
			pack.wSize = sizeof(TPacketGCWhisper);
			strlcpy(pack.szNameFrom, pinfo->szNameTo, sizeof(pack.szNameFrom));
			ecs::PlayerRuntime::GetDesc(character)->Packet(&pack, sizeof(pack));
		}

		return iExtraLen;
	}

	if (chr == entt::null)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find(pinfo->szNameTo);

		if (pkCCI)
		{
			pkDesc = pkCCI->pkDesc;
			pkDesc->SetRelay(pinfo->szNameTo);
			bOpponentEmpire = pkCCI->bEmpire;

			if (test_server)
				LOG_INFO("Whisper to {} from {} (Channel {} Mapindex {})", "Null", ecs::PlayerRuntime::GetName(character).data(), pkCCI->bChannel, pkCCI->lMapIndex);
		}
	}
	else
	{
		pkDesc = ecs::PlayerRuntime::GetDesc(chr);
		bOpponentEmpire = ecs::PlayerRuntime::GetEmpire(chr);
	}

	if (!pkDesc)
	{
		if (ecs::PlayerRuntime::GetDesc(character))
		{
#if defined(BL_OFFLINE_MESSAGE)
			const uint8_t bDelay = 10;
			char msg[64];
			if (get_dword_time() - ecs::ChatSystem::GetLastOfflineMessageTime(character) > bDelay * 1000)
			{
				char buf[CHAT_MAX_LEN + 1];
				strlcpy(buf, data + sizeof(TPacketCGWhisper), MIN(iExtraLen + 1, sizeof(buf)));
				const uint64_t buflen = strlen(buf);
				CBanwordManager::instance().ConvertString(buf, buflen);
				int processReturn = ProcessTextTag(character, buf, buflen);

				if (0 != processReturn)
				{
					TItemTable* pTable = ITEM_MANAGER::instance().GetTable(ITEM_PRISM);
					if (pTable) {
#ifdef ENABLE_MULTI_NAMES
						int Lang = ecs::IsCharacter(character) && ecs::PlayerRuntime::GetDesc(character) ? ecs::PlayerRuntime::GetDesc(character)->GetLanguage() : 0;
#endif
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 823, "%s",
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
					ecs::ChatSystem::SendOfflineMessage(character, pinfo->szNameTo, buf);
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
			ecs::PlayerRuntime::GetDesc(character)->Packet(buf.read_peek(), buf.size());

#else
			TPacketGCWhisper pack;
			pack.bHeader = HEADER_GC_WHISPER;
			pack.bType = WHISPER_TYPE_NOT_EXIST;
			pack.wSize = sizeof(TPacketGCWhisper);
			strlcpy(pack.szNameFrom, pinfo->szNameTo, sizeof(pack.szNameFrom));
			ecs::PlayerRuntime::GetDesc(character)->Packet(&pack, sizeof(TPacketGCWhisper));
			LOG_INFO("WHISPER: no player");
#endif
		}
	}
	else
	{
		if (ecs::PlayerRuntime::IsBlockMode(character, BLOCK_WHISPER))
		{
			if (ecs::PlayerRuntime::GetDesc(character))
			{
				TPacketGCWhisper pack;
				pack.bHeader = HEADER_GC_WHISPER;
				pack.bType = WHISPER_TYPE_SENDER_BLOCKED;
				pack.wSize = sizeof(TPacketGCWhisper);
				strlcpy(pack.szNameFrom, pinfo->szNameTo, sizeof(pack.szNameFrom));
				ecs::PlayerRuntime::GetDesc(character)->Packet(&pack, sizeof(pack));
			}
		}
		else if (chr != entt::null && ecs::PlayerRuntime::IsBlockMode(chr, BLOCK_WHISPER))
		{
			if (ecs::PlayerRuntime::GetDesc(character))
			{
				TPacketGCWhisper pack;
				pack.bHeader = HEADER_GC_WHISPER;
				pack.bType = WHISPER_TYPE_TARGET_BLOCKED;
				pack.wSize = sizeof(TPacketGCWhisper);
				strlcpy(pack.szNameFrom, pinfo->szNameTo, sizeof(pack.szNameFrom));
				ecs::PlayerRuntime::GetDesc(character)->Packet(&pack, sizeof(pack));
			}
		}
		else
		{
			uint8_t bType = WHISPER_TYPE_NORMAL;

			char buf[CHAT_MAX_LEN + 1];
			strlcpy(buf, data + sizeof(TPacketCGWhisper), MIN(iExtraLen + 1, sizeof(buf)));
			const uint64_t buflen = strlen(buf);

			if (true == SpamBlockCheck(character, buf, buflen))
			{
				if (chr == entt::null)
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
				if (!ItemSystem::IsEquipUniqueGroup(character, UNIQUE_GROUP_RING_OF_LANGUAGE))
					if (!ItemSystem::IsEquipUniqueGroup(chr, UNIQUE_GROUP_RING_OF_LANGUAGE))
						if (bOpponentEmpire != ecs::PlayerRuntime::GetEmpire(character) && ecs::PlayerRuntime::GetEmpire(character) && bOpponentEmpire // ���� ������ �ٸ��鼭
								&& ecs::PlayerRuntime::GetGMLevel(character) == GM_PLAYER && gm_get_level(pinfo->szNameTo) == GM_PLAYER) // �Ѵ� �Ϲ� �÷��̾��̸�
							// �̸� �ۿ� �𸣴� gm_get_level �Լ��� ���
						{
							if (chr == entt::null)
							{
								// �ٸ� ������ ������ ���� ǥ�ø� �Ѵ�. bType�� ���� 4��Ʈ�� Empire��ȣ�� ����Ѵ�.
								bType = ecs::PlayerRuntime::GetEmpire(character) << 4;
							}
							else
							{
								ConvertEmpireText(ecs::PlayerRuntime::GetEmpire(character), buf, buflen, 10 + 2 * SkillSystem::GetSkillPower(chr, SKILL_LANGUAGE1 + ecs::PlayerRuntime::GetEmpire(character) - 1)/*��ȯȮ��*/);
							}
						}

			int processReturn = ProcessTextTag(character, buf, buflen);
			if (0!=processReturn)
			{
				if (ecs::PlayerRuntime::GetDesc(character))
				{
					TItemTable * pTable = ITEM_MANAGER::instance().GetTable(ITEM_PRISM);

					if (pTable)
					{
#ifdef ENABLE_MULTI_NAMES
						int Lang = ecs::IsCharacter(character) && ecs::PlayerRuntime::GetDesc(character) ? ecs::PlayerRuntime::GetDesc(character)->GetLanguage() : 0;
#endif
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 823, "%s",
#ifdef ENABLE_MULTI_NAMES
						pTable->szLocaleName[Lang]
#else
						pTable->szLocaleName
#endif
						);
					}
				}

				// ������ ������ �� �����Ƿ� �����̸� Ǯ���ش�.
				pkDesc->SetRelay("");
				return (iExtraLen);
			}

			if (ecs::PlayerRuntime::IsGM(character))
				bType = (bType & 0xF0) | WHISPER_TYPE_GM;

			if (buflen > 0)
			{
				TPacketGCWhisper pack;

				pack.bHeader = HEADER_GC_WHISPER;
				pack.wSize = sizeof(TPacketGCWhisper) + buflen;
				pack.bType = bType;
				strlcpy(pack.szNameFrom, ecs::PlayerRuntime::GetName(character).data(), sizeof(pack.szNameFrom));
				// desc->BufferedPacket�� ���� �ʰ� ���ۿ� ����ϴ� ������
				// P2P relay�Ǿ� ��Ŷ�� ĸ��ȭ �� �� �ֱ� �����̴�.
				TEMP_BUFFER tmpbuf;

				tmpbuf.write(&pack, sizeof(pack));
				tmpbuf.write(buf, buflen);

				pkDesc->Packet(tmpbuf.read_peek(), tmpbuf.size());

				// @warme006
				// LOG_INFO(0, "WHISPER: %s -> %s : %s", ecs::PlayerRuntime::GetName(character).data(), pinfo->szNameTo, buf);
#ifdef ENABLE_CHAT_LOGGING
				if (ecs::PlayerRuntime::IsGM(character))
				{
					LogManager::instance().EscapeString(__escape_string, sizeof(__escape_string), buf, buflen);
					LogManager::instance().EscapeString(__escape_string2, sizeof(__escape_string2), pinfo->szNameTo, sizeof(pack.szNameFrom));
					LogManager::instance().ChatLog(ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetPlayerID(character), ecs::PlayerRuntime::GetName(character).data(), 0, __escape_string2, "WHISPER", __escape_string, ecs::PlayerRuntime::GetDesc(character) ? ecs::PlayerRuntime::GetDesc(character)->GetHostName() : "");
				}
#endif
			}
		}
	}
	if(pkDesc)
		pkDesc->SetRelay("");

	return (iExtraLen);
}

struct RawPacketToEntityFunc
{

	const void * m_buf;
	int	m_buf_len;

	RawPacketToEntityFunc(const void * buf, int buf_len) : m_buf(buf), m_buf_len(buf_len)
	{
	}

	void operator () (entt::entity recipient)
    {
        auto* desc = ecs::PlayerRuntime::GetDesc(recipient);
        if (ecs::PlayerRuntime::IsPC(recipient) && desc && desc->GetEntity() == recipient)
            desc->Packet(m_buf, m_buf_len);
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

    void operator () (LPDESC desc)
    {
        if (!desc)
            return;
        const auto recipient = desc->GetEntity();
        if (!ecs::PlayerRuntime::IsPC(recipient) ||
            ecs::PlayerRuntime::GetDesc(recipient) != desc ||
            ecs::PlayerRuntime::GetMapIndex(recipient) != iMapIndex ||
            orig_len < 0 || orig_len > sizeof(converted_msg))
            return;

        const char* message = orig_msg;
        if (desc->GetEmpire() != bEmpire && bEmpire != 0 &&
            ecs::PlayerRuntime::GetGMLevel(recipient) == GM_PLAYER &&
            !ItemSystem::IsEquipUniqueGroup(recipient, UNIQUE_GROUP_RING_OF_LANGUAGE))
        {
            const size_t len = std::min(size_t(strlcpy(converted_msg, orig_msg, sizeof(converted_msg))),
                sizeof(converted_msg) - 1);
            const size_t offset = std::min(size_t(std::max(namelen, 0)), len);
            ConvertEmpireText(bEmpire, converted_msg + offset, len - offset,
                10 + 2 * SkillSystem::GetSkillPower(recipient, SKILL_LANGUAGE1 + bEmpire - 1));
            message = converted_msg;
        }

        TEMP_BUFFER packet;
        packet.write(&p, sizeof(p));
        packet.write(message, orig_len);
        desc->Packet(packet.read_peek(), packet.size());
    }
};

#ifdef __NEWPET_SYSTEM__
void CInputMain::BraveRequestPetName(entt::entity character, const char* c_pData)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	const entt::entity ownerEntity = character;
	if (ownerEntity == entt::null || !g_registry.valid(ownerEntity) ||
		!ecs::PlayerRuntime::GetDesc(ownerEntity))
	{
		return;
	}

	const int eggVnum = ecs::PlayerRuntime::GetEggVID(character);
	if (eggVnum <= 0)
		return;

	const auto p = reinterpret_cast<const TPacketCGRequestPetName*>(c_pData);
	if (ecs::PointSystem::GetGold(ownerEntity) < 100000)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 768, "%d", 100000);
#endif
		return;
	}

	if (!ItemSystem::HasItem(ownerEntity, static_cast<uint32_t>(eggVnum)) ||
		check_name(p->petname) == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 770, "");
#endif
		return;
	}

#ifdef ENABLE_NEW_PET_EDITS
	char nameQuery[256] {};
	snprintf(
		nameQuery,
		sizeof(nameQuery),
		"SELECT id FROM player.new_petsystem%s WHERE name='%s';",
		get_table_postfix(),
		p->petname);
	std::unique_ptr<SQLMsg> nameResult(DBManager::instance().DirectQuery(nameQuery));
	if (nameResult->Get()->uiNumRows > 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 50, "");
#endif
		return;
	}
#endif

	const entt::entity petItem =
		ITEM_MANAGER::instance().CreateItem(static_cast<uint32_t>(eggVnum + 300), 1);
	if (!ItemSystem::IsValidItem(petItem))
		return;

	if (!ItemSystem::RemoveSpecifyItemEcs(
			ownerEntity, static_cast<uint32_t>(eggVnum), 1))
	{
		ItemSystem::DestroyItemEntityEcs(petItem, "PET_NAME_EGG_REMOVE_FAILED");
		return;
	}

	const uint32_t petItemId = ItemSystem::GetItemID(petItem);
	DBManager::instance().SendMoneyLog(
		MONEY_LOG_QUEST, ecs::PlayerRuntime::GetPlayerID(ownerEntity), -100000);
	ecs::PointSystem::Change(ownerEntity, POINT_GOLD, -100000, true);
	ItemSystem::AutoGiveItem(ownerEntity, petItem);

#ifdef ENABLE_NEW_PET_EDITS
	int tmpskill[4] = { -1, -1, -1, -1 };
#else
	int tmpskill[4] = { 0, 0, 0, 0 };
	const int tmpslot = number(1, 3);
	for (int i = 0; i < 4; ++i)
	{
		if (i > tmpslot - 1)
			tmpskill[i] = -1;
	}
#endif
	const int tmpdur = 3 * 24 * 60;
	char insertQuery[1024];
	int hp[] = {30, 35, 40};
	int mostri[] = {10, 15, 20};
	int medi[] = {10, 15, 20};
	snprintf(
		insertQuery,
		sizeof(insertQuery),
		"INSERT INTO new_petsystem VALUES(%u,'%s', 1, 0, 0, 0, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, 0"
#ifdef ENABLE_NEW_PET_EDITS
		", %lld"
#endif
		")",
		petItemId,
		p->petname,
		hp[number(0, 2)],
		mostri[number(0, 2)],
		medi[number(0, 2)],
		tmpskill[0],
		0,
		tmpskill[1],
		0,
		tmpskill[2],
		0,
		tmpskill[3],
		0,
		tmpdur,
		tmpdur,
		get_global_time());
	std::unique_ptr<SQLMsg> insertResult(DBManager::instance().DirectQuery(insertQuery));
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 769, "");
#endif
}
#endif
int CInputMain::Chat(entt::entity character, const char * data, uint32_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Chat handler ECS
// DUAL-PATH: legacy only during migration window
	auto pinfo = reinterpret_cast<const TPacketCGChat*>(data);

	if (uiBytes < pinfo->size)
		return -1;

	const int iExtraLen = pinfo->size - sizeof(TPacketCGChat);

	if (iExtraLen < 0)
	{
		LOG_ERROR("invalid packet length (len {} size {} buffer {})", iExtraLen, pinfo->size, uiBytes);
		ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);
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
				offlineshop::CShop* pkShop = offlineshop::GetManager().GetShopByOwnerID(ecs::PlayerRuntime::GetPlayerID(character));
				if (!pkShop)
				{
					ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Nincs nyitott offline boltod./ You don't have open shop.");
					return iExtraLen;
				}

				// shout rules
				if (ecs::PointSystem::GetLevel(character) < g_iShoutLimitLevel)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 411, "%d", g_iShoutLimitLevel);
#else
					ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Nem megfelelo szint a kiabalashoz.");
#endif
					return iExtraLen;
				}
				if (thecore_heart->pulse - (int)ecs::PlayerRuntime::GetLastShoutPulse(character) < passes_per_sec * 15)
					return iExtraLen;

				ecs::PlayerRuntime::SetLastShoutPulse(character, thecore_heart->pulse);


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
						(unsigned)ecs::PlayerRuntime::GetPlayerID(character),   // socket0 = OWNER_ID
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
						(unsigned)ecs::PlayerRuntime::GetPlayerID(character),
						(unsigned)0x0BADF00D,
						0u,
						0u,
						shopName);
				}

				char shoutbuf[CHAT_MAX_LEN + 1];
#ifdef ENABLE_MULTI_LANGUAGE
				std::string langName = ecs::PlayerRuntime::GetLang(character);
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
				const std::string nameWithPrefix = MakeNameWithPrefix(character);

				snprintf(shoutbuf, sizeof(shoutbuf),
					"|L%s|l|E%d|e %s : %s",
					langName.c_str(), ecs::PlayerRuntime::GetEmpire(character), nameWithPrefix.c_str(), body);

#else
				snprintf(shoutbuf, sizeof(shoutbuf),
					"|L%s|l|E%d|e %s : %s",
					langName.c_str(), ecs::PlayerRuntime::GetEmpire(character), ecs::PlayerRuntime::GetName(character).data(), body);
#endif
#else
				snprintf(shoutbuf, sizeof(shoutbuf), "%s : %s", ecs::PlayerRuntime::GetName(character).data(), body);
#endif

				TPacketGGShout p;
				p.bHeader = HEADER_GG_SHOUT;
				p.bEmpire = ecs::PlayerRuntime::GetEmpire(character);
				strlcpy(p.szText, shoutbuf, sizeof(p.szText));

				P2P_MANAGER::instance().Send(&p, sizeof(p));
				SendShout(shoutbuf, ecs::PlayerRuntime::GetEmpire(character));
#ifdef ENABLE_BATTLE_PASS
				if (uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(character))
				{
					uint32_t dwCount, dwNotUsed;
					if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COUNTER_CHAT, &dwNotUsed, &dwCount))
					{
						if (!ecs::PlayerRuntime::IsCompletedMission(character, COUNTER_CHAT))
						{
							if (ecs::PlayerRuntime::GetMissionProgress(character, COUNTER_CHAT, bBattlePassId) < dwCount)
								ecs::PlayerRuntime::UpdateMissionProgress(character, COUNTER_CHAT, bBattlePassId, 1, dwCount);
						}
					}
				}
#endif
				return iExtraLen;
			}
		}

#endif

	interpret_command(character, buf + 1, buflen - 1);
	return iExtraLen;
	}




/* 	if (ecs::PlayerRuntime::GetGMLevel(character) == GM_PLAYER && ecs::PlayerRuntime::GetMapIndex(character) == 113)//OX mapon chat letiltva//
	{
		return iExtraLen;
	} */

	// ä�� ���� Affect ó��
	const CAffect* pAffect = AffectSystem::FindAffect(character, AFFECT_BLOCK_CHAT);

	if (pAffect != nullptr)
	{
		SendBlockChatInfo(character, pAffect->lDuration);
		return iExtraLen;
	}

	if (true == SpamBlockCheck(character, buf, buflen))
	{
		return iExtraLen;
	}

	// @fixme133 begin
	CBanwordManager::instance().ConvertString(buf, buflen);

	int processReturn = ProcessTextTag(character, buf, buflen);
	if (0!=processReturn)
	{
#ifdef TEXTS_IMPROVEMENT
		const TItemTable* pTable = ITEM_MANAGER::instance().GetTable(ITEM_PRISM);
		if (nullptr != pTable)
		{
#ifdef ENABLE_MULTI_NAMES
			int lang = 0;
			if (ecs::IsCharacter(character)) {
				LPDESC desc = ecs::PlayerRuntime::GetDesc(character);
				lang = desc != nullptr ? desc->GetLanguage() : 0;
			}
#endif
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 642, "%s",
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
	std::string langName = ecs::PlayerRuntime::GetLang(character);
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

			ItemSystem::AutoGiveItemEcs(character, vnum, count);

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

			//	"%s#%d#",
			//	ecs::PlayerRuntime::GetName(character).data(),            // %s (winner)
			//	answer                   // %d (correct answer)
			//	//(itemName ? itemName : "item"), // %s
			//	//count                     // %d
			//);
			//	//
			//
			//}

			BroadcastNoticeNew(CHAT_TYPE_INFO, 0,0,2181,
				"%s#%d#",
				ecs::PlayerRuntime::GetName(character).data(),            // %s (winner)
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
				C_RED, ecs::PlayerRuntime::GetName(character).data(), C_RST,
				C_GOLD, C_RST, C_GRN, answer, C_RST,
				C_GOLD, C_RST, C_BLUE, (itemName ? itemName : "item"), C_RST, C_GRN, count, C_RST);

			const DESC_MANAGER::DESC_SET& cset = DESC_MANAGER::instance().GetClientSet();
			for (DESC_MANAGER::DESC_SET::const_iterator it = cset.begin(); it != cset.end(); ++it)
			{
				LPDESC d = *it;
				if (!d) continue;
                const auto recipient = d->GetEntity();
                if (!ecs::PlayerRuntime::IsPC(recipient) || ecs::PlayerRuntime::GetDesc(recipient) != d)
                    continue;
                ecs::ChatSystem::Send(recipient, CHAT_TYPE_INFO, "%s", msg);
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
		int count = MountSystem::GetMountCount(character);

		// 80 f�l�tt arany
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


		const std::string prefix = NetworkSyncSystem::GetItemOnTitlePrefix(g_registry, character);
		const char* name = ecs::PlayerRuntime::GetName(character).data();

		len = snprintf(chatbuf, sizeof(chatbuf),
			"|L%s|l|E%d|e |Hmsg:%s|h%s%s |h|r %s%s: %s",
			langName.c_str(),
			ecs::PlayerRuntime::GetEmpire(character),
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
			ecs::PlayerRuntime::GetEmpire(character),
			ecs::PlayerRuntime::GetName(character).data(),
			ecs::PlayerRuntime::GetName(character).data(),
			buf);
#endif


	} else {
//#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
		const std::string nameWithPrefix = MakeNameWithPrefix(character);

		len = snprintf(chatbuf, sizeof(chatbuf), "|L%s|l|E%d|e %s : %s",
			langName.c_str(), ecs::PlayerRuntime::GetEmpire(character), nameWithPrefix.c_str(), buf);

// else
//		len = snprintf(chatbuf, sizeof(chatbuf), "|L%s|l %s %s : %s", langName.c_str(), (ecs::PlayerRuntime::IsGM(character)?colorbuf[0]:colorbuf[MINMAX(0, ecs::PlayerRuntime::GetEmpire(character), 3)]), ecs::PlayerRuntime::GetName(character).data(), buf);
		//len = snprintf(chatbuf, sizeof(chatbuf), "|L%s|l|E%d|e %s : %s", langName.c_str(), ecs::PlayerRuntime::GetEmpire(character), ecs::PlayerRuntime::GetName(character).data(), buf);
//#endif
	}
#else
	int len = snprintf(chatbuf, sizeof(chatbuf), "%s %s : %s", (ecs::PlayerRuntime::IsGM(character)?colorbuf[0]:colorbuf[MINMAX(0, ecs::PlayerRuntime::GetEmpire(character), 3)]), ecs::PlayerRuntime::GetName(character).data(),buf);
#endif

	if (CHAT_TYPE_SHOUT == pinfo->type)
	{
		LogManager::instance().ShoutLog(g_bChannel, ecs::PlayerRuntime::GetEmpire(character), chatbuf);
	}

	if (len < 0 || len >= (int) sizeof(chatbuf))
		len = sizeof(chatbuf) - 1;

	if (pinfo->type == CHAT_TYPE_SHOUT)
	{
		// const int SHOUT_LIMIT_LEVEL = 15;

		if (ecs::PointSystem::GetLevel(character) < g_iShoutLimitLevel) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 411, "%d", g_iShoutLimitLevel);
#endif
			return (iExtraLen);
		}

		// if (thecore_heart->pulse - (int) ecs::PlayerRuntime::GetLastShoutPulse(character) < passes_per_sec * g_iShoutLimitTime)
		if (thecore_heart->pulse - (int) ecs::PlayerRuntime::GetLastShoutPulse(character) < passes_per_sec * 15)
			return (iExtraLen);

		ecs::PlayerRuntime::SetLastShoutPulse(character, thecore_heart->pulse);

		TPacketGGShout p;

		p.bHeader = HEADER_GG_SHOUT;
		p.bEmpire = ecs::PlayerRuntime::GetEmpire(character);
		strlcpy(p.szText, chatbuf, sizeof(p.szText));

		P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGShout));

		SendShout(chatbuf, ecs::PlayerRuntime::GetEmpire(character));

#ifdef ENABLE_BATTLE_PASS
		uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(character);
		if(bBattlePassId)
		{
			uint32_t dwCount, dwNotUsed;
			if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COUNTER_CHAT, &dwNotUsed, &dwCount))
			{
				if (!ecs::PlayerRuntime::IsCompletedMission(character, COUNTER_CHAT))
				{
					if(ecs::PlayerRuntime::GetMissionProgress(character, COUNTER_CHAT, bBattlePassId) < dwCount)
						ecs::PlayerRuntime::UpdateMissionProgress(character, COUNTER_CHAT, bBattlePassId, 1, dwCount);
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
	pack_chat.id = ecs::PlayerRuntime::GetPacketVID(character);

	switch (pinfo->type)
	{
		case CHAT_TYPE_TALKING:
			{
				const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();


				{
					std::for_each(c_ref_set.begin(), c_ref_set.end(),
							FEmpireChatPacket(pack_chat,
								chatbuf,
								len,
								(ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER ||
								 ItemSystem::IsEquipUniqueGroup(character, UNIQUE_GROUP_RING_OF_LANGUAGE)) ? 0 : ecs::PlayerRuntime::GetEmpire(character),
								ecs::PlayerRuntime::GetMapIndex(character), strlen(ecs::PlayerRuntime::GetName(character).data())));
#ifdef ENABLE_CHAT_LOGGING
					if (ecs::PlayerRuntime::IsGM(character))
					{
						LogManager::instance().EscapeString(__escape_string, sizeof(__escape_string), chatbuf, len);
						LogManager::instance().ChatLog(ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetPlayerID(character), ecs::PlayerRuntime::GetName(character).data(), 0, "", "NORMAL", __escape_string, ecs::PlayerRuntime::GetDesc(character) ? ecs::PlayerRuntime::GetDesc(character)->GetHostName() : "");
					}
#endif
				}
			}
			break;

		case CHAT_TYPE_PARTY:
			{
				const entt::entity chatParty = ecs::SocialSystem::GetParty(character);

				if (chatParty == entt::null)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 485, "");
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 486, "");
#endif
				}
				else
				{
					TEMP_BUFFER tbuf;

					tbuf.write(&pack_chat, sizeof(pack_chat));
					tbuf.write(chatbuf, len);

					RawPacketToEntityFunc f(tbuf.read_peek(), tbuf.size());
					ecs::SocialSystem::ForEachOnlinePartyMember(character, f);
#ifdef ENABLE_CHAT_LOGGING
					if (ecs::PlayerRuntime::IsGM(character))
					{
						LogManager::instance().EscapeString(__escape_string, sizeof(__escape_string), chatbuf, len);
						LogManager::instance().ChatLog(ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetPlayerID(character), ecs::PlayerRuntime::GetName(character).data(), PartySystem::GetLeaderPID(chatParty), "", "PARTY", __escape_string, ecs::PlayerRuntime::GetDesc(character) ? ecs::PlayerRuntime::GetDesc(character)->GetHostName() : "");
					}
#endif
				}
			}
			break;

		case CHAT_TYPE_GUILD:
			{
				if (ecs::SocialSystem::GetGuild(character)) {
					ecs::SocialSystem::GetGuild(character)->Chat(chatbuf);
#ifdef ENABLE_CHAT_LOGGING
					if (ecs::PlayerRuntime::IsGM(character))
					{
						LogManager::instance().EscapeString(__escape_string, sizeof(__escape_string), chatbuf, len);
						LogManager::instance().ChatLog(ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetPlayerID(character), ecs::PlayerRuntime::GetName(character).data(), ecs::SocialSystem::GetGuild(character)->GetID(), ecs::SocialSystem::GetGuild(character)->GetName(), "GUILD", __escape_string, ecs::PlayerRuntime::GetDesc(character) ? ecs::PlayerRuntime::GetDesc(character)->GetHostName() : "");
					}
#endif
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 271, "");
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

void CInputMain::ItemUse(entt::entity character, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemUse handler ECS
// DUAL-PATH: legacy only during migration window
	ItemSystem::UseItem(character, ((struct command_item_use *) data)->Cell);
}

void CInputMain::ItemToItem(entt::entity character, const char * pcData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemToItem handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGItemUseToItem * p = (TPacketCGItemUseToItem *) pcData;
	if (ecs::IsCharacter(character))
		ItemSystem::UseItem(character, p->Cell, p->TargetCell);
}

void CInputMain::ItemDrop(entt::entity character, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemDrop handler ECS
// DUAL-PATH: legacy only during migration window
	struct command_item_drop * pinfo = (struct command_item_drop *) data;
	if (!ecs::IsCharacter(character))
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR) {
		return;
	}
#endif

	// ��ũ�� 0���� ũ�� ��ũ�� ������ �� �̴�.
	if (pinfo->gold > 0)
		ItemSystem::DropGold(character, pinfo->gold);
	else
		ItemSystem::DropItem(character, pinfo->Cell);
}

void CInputMain::ItemDrop2(entt::entity character, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemDrop2 handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGItemDrop2 * pinfo = (TPacketCGItemDrop2 *) data;
	if (!ecs::IsCharacter(character))
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR) {
		return;
	}
#endif

	if (pinfo->gold > 0)
		ItemSystem::DropGold(character, pinfo->gold);
	else
		ItemSystem::DropItem(character, pinfo->Cell, pinfo->count);
}

void CInputMain::ItemMove(entt::entity character, const char * data)
{
	// Framing validates this fixed-size packet before dispatch. Copy packed data
	// so the transaction does not retain a pointer into the receive buffer.
	command_item_move packet {};
	memcpy(&packet, data, sizeof(packet));
	InventorySystem::MoveItem(character, packet.Cell, packet.CellTo, packet.count);
}

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
void CInputMain::InventoryExpansion(entt::entity character, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate InventoryExpansion handler ECS
// DUAL-PATH: legacy only during migration window
	InventorySystem::ExpandInventory(character);
}
#endif

void CInputMain::ItemPickup(entt::entity character, const char* data)
{
    if (!data || !ecs::PlayerRuntime::IsPC(character)) return;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    const auto gm = ecs::PlayerRuntime::GetGMLevel(character);
    if (gm > GM_PLAYER && gm < GM_IMPLEMENTOR) return;
#endif
    command_item_pickup packet {};
    memcpy(&packet, data, sizeof(packet));
    ItemSystem::PickupItem(character, packet.vid);
}

void CInputMain::QuickslotAdd(entt::entity character, const char* data)
{
    if (!data) return;
    command_quickslot_add packet {};
    memcpy(&packet, data, sizeof(packet));
    InventorySystem::SetQuickslotFromClient(character, packet.pos, packet.slot);
}

void CInputMain::QuickslotDelete(entt::entity character, const char* data)
{
    if (!data) return;
    command_quickslot_del packet {};
    memcpy(&packet, data, sizeof(packet));
    InventorySystem::DelQuickslot(character, packet.pos);
}

void CInputMain::QuickslotSwap(entt::entity character, const char* data)
{
    if (!data) return;
    command_quickslot_swap packet {};
    memcpy(&packet, data, sizeof(packet));
    InventorySystem::SwapQuickslot(character, packet.pos, packet.change_pos);
}

int CInputMain::Messenger(entt::entity character, const char* c_pData, uint64_t uiBytes)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character))
		return -1;
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
				const entt::entity ch_companionEntity = CHARACTER_MANAGER::instance().FindEntity(p2->vid);


				if (!ecs::PlayerRuntime::IsPC(ch_companionEntity))
					return sizeof(TPacketCGMessengerAddByVID);

				if (ecs::PlayerRuntime::IsObserverMode(character))
					return sizeof(TPacketCGMessengerAddByVID);

				if (ecs::PlayerRuntime::IsBlockMode(ch_companionEntity, BLOCK_MESSENGER_INVITE))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 370, "%s", ecs::PlayerRuntime::GetName(ch_companionEntity).data());
#endif
					return sizeof(TPacketCGMessengerAddByVID);
				}

				LPDESC d = ecs::PlayerRuntime::GetDesc(ch_companionEntity);

				if (!d)
					return sizeof(TPacketCGMessengerAddByVID);

				if (ecs::PlayerRuntime::GetGMLevel(character) == GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(ch_companionEntity) != GM_PLAYER)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 184, "");
#endif
					return sizeof(TPacketCGMessengerAddByVID);
				}

				if (ecs::PlayerRuntime::GetDesc(character) == d) // �ڽ��� �߰��� �� ����.
					return sizeof(TPacketCGMessengerAddByVID);

				MessengerManager::instance().RequestToAdd(character, ch_companionEntity);
			}
			return sizeof(TPacketCGMessengerAddByVID);

		case MESSENGER_SUBHEADER_CG_ADD_BY_NAME:
			{
				if (uiBytes < CHARACTER_NAME_MAX_LEN)
					return -1;

				char name[CHARACTER_NAME_MAX_LEN + 1];
				memcpy(name, c_pData, CHARACTER_NAME_MAX_LEN);
				name[CHARACTER_NAME_MAX_LEN] = '\0';

				if (ecs::PlayerRuntime::GetGMLevel(character) == GM_PLAYER && gm_get_level(name) != GM_PLAYER)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 184, "");
#endif
					return CHARACTER_NAME_MAX_LEN;
				}

				const entt::entity tch = CHARACTER_MANAGER::instance().FindPCEntity(name);
				if (ecs::PlayerRuntime::IsPC(tch))
				{
					if (tch == character) // �ڽ��� �߰��� �� ����.
						return CHARACTER_NAME_MAX_LEN;

					if (ecs::PlayerRuntime::IsBlockMode(tch, BLOCK_MESSENGER_INVITE) == true)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 370, "%s", ecs::PlayerRuntime::GetName(tch).data());
#endif
					}
					else
					{
						// �޽����� ĳ���ʹ����� �Ǹ鼭 ����
						MessengerManager::instance().RequestToAdd(character, tch);
					}
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 108, "%s", name);
				}
#endif
			}
			return CHARACTER_NAME_MAX_LEN;

		case MESSENGER_SUBHEADER_CG_REMOVE:
			{
				if (uiBytes < CHARACTER_NAME_MAX_LEN)
					return -1;

				char char_name[CHARACTER_NAME_MAX_LEN + 1];
				memcpy(char_name, c_pData, CHARACTER_NAME_MAX_LEN);
				char_name[CHARACTER_NAME_MAX_LEN] = '\0';
				MessengerManager::instance().RemoveFromList(ecs::PlayerRuntime::GetName(character).data(), char_name);
#ifdef ENABLE_BUG_FIXES
				MessengerManager::instance().RemoveFromList(char_name, ecs::PlayerRuntime::GetName(character).data());
#endif
			}
			return CHARACTER_NAME_MAX_LEN;

		default:
			LOG_ERROR("CInputMain::Messenger : Unknown subheader {} : {}", p->subheader, ecs::PlayerRuntime::GetName(character).data());
			break;
	}

	return 0;
}

#ifdef ENABLE_BATTLE_PASS
int CInputMain::BattlePass(entt::entity character, const char* data, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate BattlePass handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGBattlePassAction * p = (TPacketCGBattlePassAction *) data;

	if (uiBytes < sizeof(TPacketCGBattlePassAction))
		return -1;

	//const char * c_pData = data + sizeof(TPacketCGBattlePassAction);
	uiBytes -= sizeof(TPacketCGBattlePassAction);

	switch(p->bAction)
	{
		case 1:
			CBattlePass::instance().BattlePassRequestOpen(character);
			break;

		case 2:
			CBattlePass::instance().BattlePassRequestReward(character);
			break;

		case 3:
		{
			uint32_t dwPlayerId = ecs::PlayerRuntime::GetPlayerID(character);
			uint8_t bIsGlobal = 0;

			db_clientdesc->DBPacketHeader(HEADER_GD_BATTLE_PASS_RANKING, ecs::PlayerRuntime::GetDesc(character)->GetHandle(), sizeof(uint32_t) + sizeof(uint8_t));
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

int CInputMain::Shop(entt::entity character, const char * data, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Shop handler ECS
// DUAL-PATH: legacy only during migration window
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
			LOG_INFO("INPUT: {} SHOP: END", ecs::PlayerRuntime::GetName(character).data());
			CShopManager::instance().StopShopping(character);
			return 0;

		case SHOP_SUBHEADER_CG_BUY:
			{
				if (uiBytes < sizeof(uint8_t) + sizeof(uint8_t))
					return -1;

				uint8_t bPos = *(c_pData + 1);
				LOG_INFO("INPUT: {} SHOP: BUY {}", ecs::PlayerRuntime::GetName(character).data(), bPos);
				CShopManager::instance().Buy(character, bPos);
				return (sizeof(uint8_t) + sizeof(uint8_t));
			}
#ifndef ENABLE_EXTRA_INVENTORY
		case SHOP_SUBHEADER_CG_SELL:
			{
				if (uiBytes < sizeof(uint8_t))
					return -1;

				uint8_t pos = *c_pData;

				LOG_INFO("INPUT: {} SHOP: SELL", ecs::PlayerRuntime::GetName(character).data());
				CShopManager::instance().Sell(character, pos);
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

				LOG_INFO("INPUT: {} SHOP: SELL2", ecs::PlayerRuntime::GetName(character).data());
				CShopManager::instance().Sell(character,
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
				LOG_INFO("INPUT: {} SHOP: MULTIPLE BUY {} COUNT {}", ecs::PlayerRuntime::GetName(character).data(), p, c);
				CShopManager::instance().MultipleBuy(character, p, c);
				return size;
			}
#endif
		default:
			LOG_ERROR("CInputMain::Shop : Unknown subheader {} : {}", p->subheader, ecs::PlayerRuntime::GetName(character).data());
			break;
	}

	return 0;
}

void CInputMain::OnClick(entt::entity character, const char* data)
{
    if (!data || !ecs::PlayerRuntime::IsPC(character)) return;
    command_on_click request {};
    memcpy(&request, data, sizeof(request));
    const entt::entity target = CHARACTER_MANAGER::instance().FindEntity(request.vid);
    if (ecs::PlayerRuntime::IsValid(target))
        ecs::PlayerRuntime::OnClick(target, character);
    else if (test_server)
        LOG_ERROR("OnClick {}: missing VID {}", ecs::PlayerRuntime::GetName(character), request.vid);
}

void CInputMain::Exchange(entt::entity character, const char* data)
{
    if (!ecs::PlayerRuntime::IsPC(character) || !data)
        return;
    // The packet decoder already checked the fixed packet size. Copying avoids
    // alignment assumptions and keeps the request stable across callbacks.
    command_exchange request {};
    std::memcpy(&request, data, sizeof(request));
    switch (request.sub_header)
    {
        case EXCHANGE_SUBHEADER_CG_START:
            if (request.arg1 > 0 && request.arg1 <= UINT32_MAX)
                ExchangeSystem::Start(character, CHARACTER_MANAGER::instance().FindEntity(static_cast<uint32_t>(request.arg1)));
            break;
        case EXCHANGE_SUBHEADER_CG_ITEM_ADD:
            ExchangeSystem::AddItem(character, request.Pos, request.arg2);
            break;
        case EXCHANGE_SUBHEADER_CG_ITEM_DEL:
            if (request.arg1 >= 0 && request.arg1 < EXCHANGE_ITEM_MAX_NUM)
                ExchangeSystem::RemoveItem(character, static_cast<uint32_t>(request.arg1));
            break;
        case EXCHANGE_SUBHEADER_CG_ELK_ADD:
            ExchangeSystem::AddGold(character, request.arg1);
            break;
        case EXCHANGE_SUBHEADER_CG_ACCEPT:
            ExchangeSystem::Accept(character);
            break;
        case EXCHANGE_SUBHEADER_CG_CANCEL:
            // Closing is always allowed, including after death/window changes.
            ExchangeSystem::Cancel(character);
            break;
    }
}

void CInputMain::Position(entt::entity character, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Position handler ECS
// DUAL-PATH: legacy only during migration window
	struct command_position * pinfo = (struct command_position *) data;

	switch (pinfo->position)
	{
		case POSITION_GENERAL:
			ecs::MovementSystem::Standup(character);
			break;

		case POSITION_SITTING_CHAIR:
			ecs::MovementSystem::Sitdown(character, 0);
			break;

		case POSITION_SITTING_GROUND:
			ecs::MovementSystem::Sitdown(character, 1);
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

void CInputMain::Move(entt::entity character, const char * data)
{
	if (!ecs::IsCharacter(character))
		return;

	struct command_move * pinfo = (struct command_move *) data;
	if (!ecs::MovementSystem::CanMove(character))
		return;

	if (pinfo->bFunc >= FUNC_MAX_NUM && !(pinfo->bFunc & 0x80))
	{
		LOG_ERROR("invalid move type: {}", ecs::PlayerRuntime::GetName(character).data());
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

	// �ڷ���Ʈ �� üũ

//	if (!test_server)
	{
		const float fDistFromCurrent = DISTANCE_SQRT((ecs::PlayerRuntime::GetX(character) - pinfo->lX) / 100, (ecs::PlayerRuntime::GetY(character) - pinfo->lY) / 100);
		float fDist = fDistFromCurrent;

		// When movement is already in-flight, compare the next client target against the
		// pending server destination as well. Without this, legitimate follow-up move
		// packets get treated as teleports and the server rubberbands the player.
		if (pinfo->bFunc == FUNC_MOVE &&
			ecs::MovementSystem::GetCurrentMoveDuration(character) > 0 &&
			(ecs::MovementSystem::GetCurrentDestX(character) != ecs::PlayerRuntime::GetX(character) || ecs::MovementSystem::GetCurrentDestY(character) != ecs::PlayerRuntime::GetY(character)))
		{
			const float fDistFromDest = DISTANCE_SQRT((ecs::MovementSystem::GetCurrentDestX(character) - pinfo->lX) / 100, (ecs::MovementSystem::GetCurrentDestY(character) - pinfo->lY) / 100);
			fDist = std::min(fDistFromCurrent, fDistFromDest);
		}
		if (((false == MountSystem::IsRiding(character) && fDist > 30) || fDist > 60) && OXEVENT_MAP_INDEX != ecs::PlayerRuntime::GetMapIndex(character))
		{
			LOG_INFO("MOVE: {} trying to move too far (dist: {:.1f}m current: {:.1f}m) Riding({})", ecs::PlayerRuntime::GetName(character).data(), fDist, fDistFromCurrent, MountSystem::IsRiding(character));

			ecs::MovementSystem::Show(character, ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetZ(character));
			ecs::MovementSystem::Stop(character);
// Phase 15E-final.LPENTITY.4-architect H fixup-4:
			// Anti-cheat backport early-returns BEFORE the line ~2385
			// PacketAround(GC_MOVE) broadcast, so peers never receive
			// a movement packet for the rejected client move.
			// Pre-Phase D the m_map_view polling at the receiving
			// peers next tick self-corrected via UpdateSectree.
			// After D.6 stubbed that polling, the peer is left
			// rendering whatever the last packet said - typically
			// frozen at the post-stop position.
			//
			// Fix: emit SendMovePacket(FUNC_WAIT). The FUNC_WAIT
			// branch pulls (x, y, duration) from GetCurrentDestX/Y
			// and MoveDuration; after Stop() above they evaluate
			// to the servers current position with duration 0 -
			// halt at this position.
			ecs::MovementSystem::SendMovePacket(character, FUNC_WAIT, 0, 0, 0, 0, 0, -1.0f);

						return;
		}
#ifdef ENALBE_MOUNT_SECTREE_UPDATE_RAZOR93
		if (true == MountSystem::IsRiding(character))
		{
			ecs::SpatialService::UpdateSectree(g_registry, character);
		}
#endif
#ifdef ENABLE_CHECK_GHOSTMODE
		if (ecs::PlayerRuntime::IsPC(character) && CombatSystem::IsDead(character))
		{
			LOG_INFO("MOVE: {} trying to move as dead", ecs::PlayerRuntime::GetName(character).data());

			ecs::MovementSystem::Show(character, ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetZ(character));
			ecs::MovementSystem::Stop(character);
			return;
		}
#endif

		uint32_t dwCurTime = get_dword_time();
		if (ecs::PlayerRuntime::GetDesc(character)) {
			bool CheckSpeedHack = (false == ecs::PlayerRuntime::GetDesc(character)->IsHandshaking() && dwCurTime - ecs::PlayerRuntime::GetDesc(character)->GetClientTime() > 7000);
			if (CheckSpeedHack)
			{
				int iDelta = (int)(dwCurTime - pinfo->dwTime);
				int iServerDelta = (int)(dwCurTime - ecs::PlayerRuntime::GetDesc(character)->GetClientTime());
				if (iDelta >= 30000) {
					LOG_INFO("SPEEDHACK: slow timer name {} delta {}", ecs::PlayerRuntime::GetName(character).data(), iDelta);
					ecs::PlayerRuntime::GetDesc(character)->DelayedDisconnect(3);
				} else if (iDelta < -(iServerDelta / 50)) {
					LOG_INFO("SPEEDHACK: DETECTED! {} (delta {} {})", ecs::PlayerRuntime::GetName(character).data(), iDelta, iServerDelta);
					ecs::PlayerRuntime::GetDesc(character)->DelayedDisconnect(3);
				}
			}

			//if (pinfo->bFunc == FUNC_COMBO && g_bCheckMultiHack)
			//{
			//	CheckComboHack(ch, pinfo->bArg, pinfo->dwTime, CheckSpeedHack); // �޺� üũ
			//}
		}
	}

	// migrated from CHARACTER::Move
	entt::entity e = (ecs::IsCharacter(character) && ecs::PlayerRuntime::GetDesc(character)) ? ecs::PlayerRuntime::GetDesc(character)->GetEntity() : entt::null;
	if (e != entt::null && g_registry.valid(e))
	{
		g_registry.emplace_or_replace<ecs::MovementDestination>(e, static_cast<int32_t>(pinfo->lX), static_cast<int32_t>(pinfo->lY));
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	}
	// DUAL-PATH: ECS + legacy call
	if (pinfo->bFunc == FUNC_MOVE)
	{
		if (ecs::PointSystem::GetLimitPoint(character, POINT_MOV_SPEED) == 0)
			return;

		ecs::MovementSystem::SetRotation(character, pinfo->bRot * 5.0f);
		ecs::MovementSystem::ResetStopTime(character);

		ecs::MovementSystem::Goto(character, pinfo->lX, pinfo->lY);
	}
	else
	{
		if (pinfo->bFunc == FUNC_ATTACK || pinfo->bFunc == FUNC_COMBO)
		{
			ecs::MovementSystem::OnMove(character, true);
		}
		else if (pinfo->bFunc & FUNC_SKILL)
		{
			const int MASK_SKILL_MOTION = 0x7F;
			unsigned int motion = pinfo->bFunc & MASK_SKILL_MOTION;

			if (!SkillSystem::IsUsableSkillMotion(character, motion))
			{
				ecs::PlayerRuntime::GetDesc(character)->DelayedDisconnect(number(150, 500));
			}

			ecs::MovementSystem::OnMove(character);
		}

		ecs::MovementSystem::SetRotation(character, pinfo->bRot * 5.0f);
		ecs::MovementSystem::ResetStopTime(character);

		ecs::MovementSystem::Move(character, pinfo->lX, pinfo->lY);
		ecs::MovementSystem::Stop(character);
	}

	TPacketGCMove pack;

	pack.bHeader      = HEADER_GC_MOVE;
	pack.bFunc        = pinfo->bFunc;
	pack.bArg         = pinfo->bArg;
	pack.bRot         = pinfo->bRot;
	pack.dwVID        = ecs::PlayerRuntime::GetPacketVID(character);
	pack.lX           = pinfo->lX;
	pack.lY           = pinfo->lY;
	pack.dwTime       = pinfo->dwTime;
	pack.dwDuration   = (pinfo->bFunc == FUNC_MOVE) ? ecs::MovementSystem::GetCurrentMoveDuration(character) : 0;

	ecs::ViewSystem::PacketView(character, &pack, sizeof(TPacketGCMove), character);
/*
	if (pinfo->dwTime == 10653691) // ����� �߰�
	{
		if (ecs::PlayerRuntime::GetDesc(character)->DelayedDisconnect(number(15, 30)))
			LogManager::instance().HackLog("Debugger", character);

	}
	else if (pinfo->dwTime == 10653971) // Softice �߰�
	{
		if (ecs::PlayerRuntime::GetDesc(character)->DelayedDisconnect(number(15, 30)))
			LogManager::instance().HackLog("Softice", character);
	}
*/
	/*
	LOG_INFO(
			"MOVE: {} Func:{} Arg:{} Pos:{}x{} Time:{} Dist:{:.1f}",
			ecs::PlayerRuntime::GetName(character).data(),
			pinfo->bFunc,
			pinfo->bArg,
			pinfo->lX / 100,
			pinfo->lY / 100,
			pinfo->dwTime,
			fDist);
	*/
}

#ifdef __SKILL_COLOR_SYSTEM__
void CInputMain::SetSkillColor(entt::entity character, const char* pcData)
{
    if (!pcData)
        return;
    TPacketCGSkillColor packet {};
    std::memcpy(&packet, pcData, sizeof(packet));
    SkillSystem::ChangeSkillColor(character, packet.skill,
        {packet.col1, packet.col2, packet.col3, packet.col4, packet.col5});
}
#endif

void CInputMain::Attack(entt::entity character, const uint8_t header, const char* data)
{
	if (!ecs::IsCharacter(character))
		return;


	struct type_identifier
	{
		uint8_t header;
		uint8_t type;
	};

	const struct type_identifier* const type = reinterpret_cast<const struct type_identifier*>(data);

	if (type->type > 0)
	{
		if (false == SkillSystem::CanUseSkill(character, type->type))
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
				if (nullptr == ecs::PlayerRuntime::GetDesc(character))
				{
					return;
				}

				const TPacketCGAttack* const packMelee = reinterpret_cast<const TPacketCGAttack*>(data);

				ecs::PlayerRuntime::GetDesc(character)->AssembleCRCMagicCube(packMelee->bCRCMagicCubeProcPiece, packMelee->bCRCMagicCubeFilePiece);

				const auto victim = CHARACTER_MANAGER::instance().FindEntity(packMelee->dwVID);

				if (!ecs::PlayerRuntime::IsValid(victim) || character == victim)
				{
					return;
				}

				const auto* victimType = g_registry.try_get<ecs::CharacterType>(victim);
				if (!victimType)
					return;
				switch (victimType->value)
				{
					case CHAR_TYPE_NPC:
					case CHAR_TYPE_WARP:
					case CHAR_TYPE_GOTO:
						return;
				}

				if (packMelee->bType > 0)
				{
					if (false == SkillSystem::CheckSkillHit(character, packMelee->bType, victim))
					{
						return;
					}
				}

				g_registry.emplace_or_replace<ecs::CombatTarget>(character, victim, get_dword_time());
				g_registry.emplace_or_replace<ecs::CombatActiveTag>(character);
				g_registry.emplace_or_replace<ecs::DirtyTag>(character);
				ecs::MovementSystem::OnMove(character, true);
				// Damage execution is still the legacy engine boundary.
				CombatSystem::Attack(character, victim, packMelee->bType);
			}
			break;

		case HEADER_CG_SHOOT:
			{
				const TPacketCGShoot* const packShoot = reinterpret_cast<const TPacketCGShoot*>(data);

				CombatSystem::Shoot(character, packShoot->bType);
			}
			break;
	}
}

int CInputMain::SyncPosition(entt::entity character, const char * c_pcData, uint64_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate SyncPosition handler ECS
// DUAL-PATH: legacy only during migration window
	const TPacketCGSyncPosition* pinfo = reinterpret_cast<const TPacketCGSyncPosition*>( c_pcData );

	if (uiBytes < pinfo->wSize)
		return -1;

	int iExtraLen = pinfo->wSize - sizeof(TPacketCGSyncPosition);

	if (iExtraLen < 0)
	{
		LOG_ERROR("invalid packet length (len {} size {} buffer {})", iExtraLen, pinfo->wSize, uiBytes);
		ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);
		return -1;
	}

	if (0 != (iExtraLen % sizeof(TPacketCGSyncPositionElement)))
	{
		LOG_ERROR("invalid packet length {} (name: {})", pinfo->wSize, ecs::PlayerRuntime::GetName(character).data());
		return iExtraLen;
	}

	int iCount = iExtraLen / sizeof(TPacketCGSyncPositionElement);

	if (iCount <= 0)
		return iExtraLen;

	static const int nCountLimit = 60;

	if( iCount > nCountLimit )
	{
		//LogManager::instance().HackLog( "SYNC_POSITION_HACK", ch );
		LOG_ERROR("Too many SyncPosition Count({}) from Name({})", iCount, ecs::PlayerRuntime::GetName(character).data());
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
		const entt::entity victimEntity = CHARACTER_MANAGER::instance().FindEntity(e->dwVID);


		if (!ecs::IsCharacter(victimEntity))
			continue;

		switch (ecs::PlayerRuntime::GetCharType(victimEntity))
		{
			case CHAR_TYPE_NPC:
			case CHAR_TYPE_WARP:
			case CHAR_TYPE_GOTO:
				continue;
		}

		if (!NetworkSyncSystem::SetSyncOwner(victimEntity, character))
			continue;

		const float fDistWithSyncOwner = DISTANCE_SQRT( (ecs::PlayerRuntime::GetX(victimEntity) - ecs::PlayerRuntime::GetX(character)) / 100, (ecs::PlayerRuntime::GetY(victimEntity) - ecs::PlayerRuntime::GetY(character)) / 100 );
		static constexpr float fLimitDistWithSyncOwner = 2500.f + 1000.f;

		if (fDistWithSyncOwner > fLimitDistWithSyncOwner)
		{
			if (ecs::PlayerRuntime::GetSyncHackCount(character) < 60){
				ecs::PlayerRuntime::SetSyncHackCount(character, ecs::PlayerRuntime::GetSyncHackCount(character) + 1);
				continue;
			} else{
				LogManager::instance().HackLog( "SYNC_POSITION_HACK", character );

				LOG_ERROR("Too far SyncPosition DistanceWithSyncOwner({})({}) from Name({}) CH({},{}) VICTIM({},{}) SYNC({},{})", fDistWithSyncOwner, ecs::PlayerRuntime::GetName(victimEntity).data(), ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), e->lX, e->lY);

				ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);

				return -1;
			}
		}

		const float fDist = DISTANCE_SQRT( (ecs::PlayerRuntime::GetX(victimEntity) - e->lX) / 100, (ecs::PlayerRuntime::GetY(victimEntity) - e->lY) / 100 );


		static constexpr int32_t g_lValidSyncInterval = 50 * 1000;
		const timeval& tvLastSyncTime = ecs::PlayerRuntime::GetLastSyncTime(victimEntity);
		timeval* tvDiff = timediff(&tvCurTime, &tvLastSyncTime);

		if (tvDiff->tv_sec == 0 && tvDiff->tv_usec < g_lValidSyncInterval)
		{
			if (ecs::PlayerRuntime::GetSyncHackCount(character) < 60)
			{
				ecs::PlayerRuntime::SetSyncHackCount(character, ecs::PlayerRuntime::GetSyncHackCount(character) + 1);
				continue;
			}
			else
			{
				LogManager::instance().HackLog("SYNC_POSITION_HACK", character);

				LOG_ERROR("Too often SyncPosition Interval({}ms)({}) from Name({}) VICTIM({},{}) SYNC({},{})", tvDiff->tv_sec * 1000 + tvDiff->tv_usec / 1000, ecs::PlayerRuntime::GetName(victimEntity).data(), ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), e->lX, e->lY);

				ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);

				return -1;
			}
		}
		else if( fDist > 40.0f ){

			LogManager::instance().HackLog( "SYNC_POSITION_HACK", character );

			LOG_ERROR("Too far SyncPosition Distance({})({}) from Name({}) CH({},{}) VICTIM({},{}) SYNC({},{})", fDist, ecs::PlayerRuntime::GetName(victimEntity).data(), ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), e->lX, e->lY);

			ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);

			return -1;
		} else{
			ecs::PlayerRuntime::SetLastSyncTime(victimEntity, tvCurTime);
			ecs::MovementSystem::Sync(victimEntity, e->lX, e->lY);
			buffer_write(lpBuf, e, sizeof(TPacketCGSyncPositionElement));
		}
	}

	if (buffer_size(lpBuf) != sizeof(TPacketGCSyncPosition))
	{
		pHeader->bHeader = HEADER_GC_SYNC_POSITION;
		pHeader->wSize = buffer_size(lpBuf);

		ecs::ViewSystem::PacketView(character, buffer_read_peek(lpBuf), buffer_size(lpBuf), character);
	}

	return iExtraLen;
}

void CInputMain::FlyTarget(entt::entity character, const char * pcData, uint8_t bHeader)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate FlyTarget handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGFlyTargeting * p = (TPacketCGFlyTargeting *) pcData;
	CombatSystem::FlyTarget(character, p->dwTargetVID, p->x, p->y, bHeader);
}

void CInputMain::UseSkill(entt::entity character, const char * pcData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate UseSkill handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGUseSkill * p = (TPacketCGUseSkill *) pcData;
	SkillSystem::UseSkill(character, p->dwVnum, CHARACTER_MANAGER::instance().FindEntity(p->dwVID));
}

void CInputMain::ScriptButton(entt::entity character, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ScriptButton handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGScriptButton * p = (TPacketCGScriptButton *) c_pData;
	LOG_INFO("QUEST ScriptButton pid {} idx {}", ecs::PlayerRuntime::GetPlayerID(character), p->idx);

	quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(character));
	if (pc && pc->IsConfirmWait())
	{
		quest::CQuestManager::instance().Confirm(ecs::PlayerRuntime::GetPlayerID(character), quest::CONFIRM_TIMEOUT);
	}
	else if (p->idx & 0x80000000)
	{
		//����Ʈ â���� Ŭ����(__SelectQuest) �����
		quest::CQuestManager::Instance().QuestInfo(ecs::PlayerRuntime::GetPlayerID(character), p->idx & 0x7fffffff);
	}
	else
	{
		quest::CQuestManager::Instance().QuestButton(ecs::PlayerRuntime::GetPlayerID(character), p->idx);
	}
}

void CInputMain::ScriptAnswer(entt::entity character, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ScriptAnswer handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGScriptAnswer * p = (TPacketCGScriptAnswer *) c_pData;
	LOG_INFO("QUEST ScriptAnswer pid {} answer {}", ecs::PlayerRuntime::GetPlayerID(character), p->answer);

	if (p->answer > 250) // ���� ��ư�� ���� �������� �� ��Ŷ�� ���
	{
		quest::CQuestManager::Instance().Resume(ecs::PlayerRuntime::GetPlayerID(character));
	}
	else // ���� ��ư�� ��� �� ��Ŷ�� ���
	{
		quest::CQuestManager::Instance().Select(ecs::PlayerRuntime::GetPlayerID(character),  p->answer);
	}
}


// SCRIPT_SELECT_ITEM
void CInputMain::ScriptSelectItem(entt::entity character, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ScriptSelectItem handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGScriptSelectItem* p = (TPacketCGScriptSelectItem*) c_pData;
	LOG_INFO("QUEST ScriptSelectItem pid {} answer {}", ecs::PlayerRuntime::GetPlayerID(character), p->selection);
	quest::CQuestManager::Instance().SelectItem(ecs::PlayerRuntime::GetPlayerID(character), p->selection);
}
// END_OF_SCRIPT_SELECT_ITEM

void CInputMain::QuestInputString(entt::entity character, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate QuestInputString handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGQuestInputString * p = (TPacketCGQuestInputString*) c_pData;

	char msg[65];
	strlcpy(msg, p->msg, sizeof(msg));
	LOG_INFO("QUEST InputString pid {} msg {}", ecs::PlayerRuntime::GetPlayerID(character), msg);

	quest::CQuestManager::Instance().Input(ecs::PlayerRuntime::GetPlayerID(character), msg);
}

void CInputMain::QuestConfirm(entt::entity character, const void* c_pData)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character))
		return;
	const auto* p = static_cast<const TPacketCGQuestConfirm*>(c_pData);
	const auto waiting = CHARACTER_MANAGER::instance().FindEntityByPID(p->requestPID);
	if (!ecs::PlayerRuntime::IsPC(waiting))
		return;
	const auto answer = p->answer ? quest::CONFIRM_YES : static_cast<quest::EQuestConfirmType>(p->answer);
	LOG_INFO("QuestConfirm from {} pid {} name {} answer {}", ecs::PlayerRuntime::GetName(character).data(),
		p->requestPID, ecs::PlayerRuntime::GetName(waiting).data(), static_cast<int>(answer));
	quest::CQuestManager::Instance().Confirm(ecs::PlayerRuntime::GetPlayerID(waiting), answer,
		ecs::PlayerRuntime::GetPlayerID(character));
}

void CInputMain::Target(entt::entity character, const char * pcData)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;
	TPacketCGTarget * p = (TPacketCGTarget *) pcData;

	const entt::entity buildingEntity = building::CManager::instance().FindObjectByVID(p->dwVID);

	if (buildingEntity != entt::null)
	{
		TPacketGCTarget pckTarget;
		pckTarget.header = HEADER_GC_TARGET;
		pckTarget.dwVID = p->dwVID;
		ecs::PlayerRuntime::GetDesc(character)->Packet(&pckTarget, sizeof(TPacketGCTarget));
	}
	else
		CombatSystem::SetTarget(character, CHARACTER_MANAGER::instance().FindEntity(p->dwVID));
}

void CInputMain::Warp(entt::entity character, const char * pcData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Warp handler ECS
// DUAL-PATH: legacy only during migration window
	ecs::MovementSystem::WarpEnd(character);
}


namespace
{
bool IsInputInventoryPosition(TItemPos position)
{
    return position.window_type == INVENTORY || position.window_type == DRAGON_SOUL_INVENTORY
#ifdef ENABLE_EXTRA_INVENTORY
        || position.window_type == EXTRA_INVENTORY
#endif
        ;
}

bool IsInputItemAt(entt::entity owner, entt::entity item, TItemPos position)
{
    if (!ecs::PlayerRuntime::IsPC(owner) || !ItemSystem::IsValidItem(item))
        return false;
    const auto* ownership = g_registry.try_get<ecs::ItemOwner>(item);
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    return ownership && ownership->owner == owner && location &&
        location->window == position.window_type && location->cell == position.cell;
}

bool IsDetachedInputItem(entt::entity item)
{
    if (!ItemSystem::IsValidItem(item))
        return false;
    const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    return owner && owner->owner == entt::null && location && location->window == RESERVED_WINDOW;
}

bool RestoreInputItem(entt::entity owner, entt::entity item, TItemPos position)
{
    return ecs::PlayerRuntime::IsPC(owner) && IsDetachedInputItem(item) &&
        InventorySystem::IsEmptyItemGrid(owner, position, ItemSystem::GetItemSize(item)) &&
        ItemSystem::PlaceItemEcs(owner, item, position.window_type, position.cell);
}
}

void CInputMain::SafeboxCheckin(entt::entity character, const char * c_pData)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character) || !InventorySystem::CanHandleItems(character))
		return;

	const entt::entity ownerEntity = character;
	if (ownerEntity == entt::null || !g_registry.valid(ownerEntity))
		return;

	auto* questPC = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(ownerEntity));
	if (!questPC || questPC->IsRunning())
	{
		return;
	}

	const auto request = *reinterpret_cast<const TPacketCGSafeboxCheckin*>(c_pData);
	const auto* p = &request;
	if (!IsInputInventoryPosition(p->ItemPos))
		return;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(ownerEntity) > GM_PLAYER &&
		ecs::PlayerRuntime::GetGMLevel(ownerEntity) < GM_IMPLEMENTOR)
	{
		return;
	}
#endif

	if (p->ItemPos.IsBeltInventoryPosition())
	{
		ecs::ChatSystem::Send(
			ownerEntity,
			CHAT_TYPE_INFO,
			"You cannot place items from the Belt inventory into the safebox.");
		return;
	}

	const auto safebox = SafeboxSystem::Get(character, SAFEBOX);
	const entt::entity itemEntity = ItemSystem::GetItem(ownerEntity, p->ItemPos);
	if (safebox == entt::null || !IsInputItemAt(character, itemEntity, p->ItemPos) || ItemSystem::IsItemExchanging(itemEntity))
		return;

#ifdef ENABLE_BUG_FIXES
	if (ItemSystem::IsItemEquipped(itemEntity))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 1244, "");
#endif
		return;
	}
#endif

#if defined(ENABLE_EXTRA_INVENTORY) && !defined(ENABLE_SPECIAL_INV_TO_SAFEBOX)
	if (ItemSystem::IsExtraItem(itemEntity))
		return;
#endif

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	if (ItemSystem::GetItemCell(itemEntity) >= InventorySystem::GetInventorySize(character) &&
		IS_SET(ItemSystem::GetItemFlags(itemEntity), ITEM_FLAG_IRREMOVABLE))
#else
	if (ItemSystem::GetItemCell(itemEntity) >= INVENTORY_MAX_NUM &&
		IS_SET(ItemSystem::GetItemFlags(itemEntity), ITEM_FLAG_IRREMOVABLE))
#endif
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 640, "");
#endif
		return;
	}

	if (!SafeboxSystem::IsEmpty(safebox, p->bSafePos, ItemSystem::GetItemSize(itemEntity)))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 641, "");
#endif
		return;
	}

	if (ItemSystem::GetItemVnum(itemEntity) == UNIQUE_ITEM_SAFEBOX_EXPAND ||
		IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_SAFEBOX) ||
		ItemSystem::IsItemLocked(itemEntity))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 187, "");
#endif
		return;
	}

	if (ItemSystem::GetItemType(itemEntity) == ITEM_BELT &&
		InventorySystem::HasBeltItems(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 385, "");
#endif
		return;
	}

	const TItemPos originalPos = p->ItemPos;
	const bool clearQuickslot = !ItemSystem::IsDragonSoulItem(itemEntity) && !ItemSystem::IsExtraItem(itemEntity);
	if (!ItemSystem::RemoveItemEcs(itemEntity))
		return;


	if (SafeboxSystem::Get(character, SAFEBOX) != safebox || !IsDetachedInputItem(itemEntity) ||
		!SafeboxSystem::Add(safebox, p->bSafePos, itemEntity))
	{
		RestoreInputItem(character, itemEntity, originalPos);
		return;
	}
	if (!IsInputItemAt(character, itemEntity, TItemPos(SAFEBOX, p->bSafePos)))
		return;
	if (clearQuickslot && ItemSystem::GetItem(character, originalPos) == entt::null)
		InventorySystem::SyncQuickslot(character, QUICKSLOT_TYPE_ITEM, originalPos.cell, 255);
	if (!IsInputItemAt(character, itemEntity, TItemPos(SAFEBOX, p->bSafePos)))
		return;

	char hint[128];
	snprintf(
		hint,
		sizeof(hint),
		"%s %u",
		ItemSystem::GetItemName(itemEntity),
		ItemSystem::GetItemCount(itemEntity));
	LogManager::instance().ItemLogEntity(character, itemEntity, "SAFEBOX PUT", hint);
}
void CInputMain::SafeboxCheckout(entt::entity character, const char * c_pData, bool bMall)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character) || !InventorySystem::CanHandleItems(character))
		return;

	const entt::entity ownerEntity = character;
	if (ownerEntity == entt::null || !g_registry.valid(ownerEntity))
		return;

	const auto request = *reinterpret_cast<const TPacketCGSafeboxCheckout*>(c_pData);
	const auto* p = &request;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(ownerEntity) > GM_PLAYER &&
		ecs::PlayerRuntime::GetGMLevel(ownerEntity) < GM_IMPLEMENTOR)
	{
		return;
	}
#endif

	const uint8_t window = bMall ? MALL : SAFEBOX;
	const auto safebox = SafeboxSystem::Get(character, window);
	if (safebox == entt::null)
		return;

	const entt::entity itemEntity = SafeboxSystem::GetItem(safebox, p->bSafePos);
	if (!IsInputItemAt(character, itemEntity, TItemPos(window, p->bSafePos)) ||
		ItemSystem::IsItemLocked(itemEntity) || ItemSystem::IsItemExchanging(itemEntity))
		return;

	TItemPos destination = p->ItemPos;
	if (!InventorySystem::IsEmptyItemGrid(character, destination, ItemSystem::GetItemSize(itemEntity)))
		return;

	if (ItemSystem::IsDragonSoulItem(itemEntity))
	{
		if (bMall)
		{
			DSManager::instance().DragonSoulItemInitialize(itemEntity);
			if (SafeboxSystem::Get(character, window) != safebox ||
				!IsInputItemAt(character, itemEntity, TItemPos(window, p->bSafePos)))
				return;
		}

		if (destination.window_type != DRAGON_SOUL_INVENTORY)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 643, "");
#endif
			return;
		}

		if (!DSManager::instance().IsValidCellForThisItem(itemEntity, destination))
		{
			const int emptyCell =
				ItemSystem::GetEmptyDragonSoulInventory(ownerEntity, itemEntity);
			if (emptyCell < 0)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 644, "");
#endif
				return;
			}
			destination = TItemPos(DRAGON_SOUL_INVENTORY, emptyCell);
		}
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (ItemSystem::IsExtraItem(itemEntity))
	{
		if (destination.window_type != EXTRA_INVENTORY)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 1292, "");
#endif
			return;
		}

		const uint32_t category = ItemSystem::GetItemExtraCategory(itemEntity);
		const uint32_t categoryBegin = category * EXTRA_INVENTORY_CATEGORY_MAX_NUM;
		const uint32_t categoryEnd = categoryBegin + EXTRA_INVENTORY_CATEGORY_MAX_NUM;
		if (destination.cell < categoryBegin || destination.cell >= categoryEnd)
			return;
	}
#endif
	else
	{
		if (destination.window_type != INVENTORY ||
			destination.IsBeltInventoryPosition())
		{
			ecs::ChatSystem::Send(
				ownerEntity,
				CHAT_TYPE_INFO,
				"You cannot place this item directly into that inventory.");
			return;
		}
	}

	if (SafeboxSystem::Get(character, window) != safebox ||
		!InventorySystem::IsEmptyItemGrid(character, destination, ItemSystem::GetItemSize(itemEntity)))
		return;
	const entt::entity removedItem = SafeboxSystem::Remove(safebox, p->bSafePos);
	if (removedItem != itemEntity || !IsDetachedInputItem(itemEntity))
		return;

	if (!RestoreInputItem(ownerEntity, itemEntity, destination))
	{
		if (SafeboxSystem::Get(character, window) == safebox && IsDetachedInputItem(itemEntity))
			SafeboxSystem::Add(safebox, p->bSafePos, itemEntity);
		return;
	}

	if (!IsInputItemAt(character, itemEntity, destination))
		return;
	ItemSystem::FlushDelayedSaveEcs(itemEntity);
	if (!IsInputItemAt(character, itemEntity, destination))
		return;

	const uint32_t itemId = ItemSystem::GetItemID(itemEntity);
	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_FLUSH, 0, sizeof(itemId));
	db_clientdesc->Packet(&itemId, sizeof(itemId));

	if (!IsInputItemAt(character, itemEntity, destination))
		return;
	char hint[128];
	snprintf(
		hint,
		sizeof(hint),
		"%s %u",
		ItemSystem::GetItemName(itemEntity),
		ItemSystem::GetItemCount(itemEntity));
	LogManager::instance().ItemLogEntity(
		character, itemEntity, bMall ? "MALL GET" : "SAFEBOX GET", hint);
}
void CInputMain::SafeboxItemMove(entt::entity character, const char * data)
{

	const auto* pinfo = reinterpret_cast<const command_item_move*>(data);

	if (!data || !ecs::PlayerRuntime::IsPC(character) || !InventorySystem::CanHandleItems(character))
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR) {
		return;
	}
#endif

	const auto safebox = SafeboxSystem::Get(character, SAFEBOX);
	if (safebox == entt::null)
		return;

	SafeboxSystem::MoveItem(safebox, pinfo->Cell.cell, pinfo->CellTo.cell, pinfo->count);
}

void CInputMain::MountInventoryCheckin(entt::entity character, const char* c_pData)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character) || !InventorySystem::CanHandleItems(character))
		return;

	const entt::entity ownerEntity = character;
	if (ownerEntity == entt::null || !g_registry.valid(ownerEntity))
		return;

	const auto request = *reinterpret_cast<const TPacketCGMountInventoryCheckin*>(c_pData);
	const auto* p = &request;
	if (!IsInputInventoryPosition(p->ItemPos))
		return;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(ownerEntity) > GM_PLAYER &&
		ecs::PlayerRuntime::GetGMLevel(ownerEntity) < GM_IMPLEMENTOR)
	{
		return;
	}
#endif

	const entt::entity mountInventory = MountSystem::GetMountInventory(character);
	if (mountInventory == entt::null)
		return;

	const entt::entity itemEntity = ItemSystem::GetItem(ownerEntity, p->ItemPos);
	if (!IsInputItemAt(character, itemEntity, p->ItemPos))
		return;

	if (!MountSystem::IsMountInventoryPositionEmpty(
		character, p->wMountPos, ItemSystem::GetItemSize(itemEntity)))
	{
		return;
	}

	if (ItemSystem::IsItemEquipped(itemEntity) ||
		ItemSystem::IsItemExchanging(itemEntity) ||
		ItemSystem::IsItemLocked(itemEntity) ||
		ItemSystem::IsExtraItem(itemEntity))
	{
		return;
	}

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	if (ItemSystem::GetItemCell(itemEntity) >= InventorySystem::GetInventorySize(character) &&
		IS_SET(ItemSystem::GetItemFlags(itemEntity), ITEM_FLAG_IRREMOVABLE))
#else
	if (ItemSystem::GetItemCell(itemEntity) >= INVENTORY_MAX_NUM &&
		IS_SET(ItemSystem::GetItemFlags(itemEntity), ITEM_FLAG_IRREMOVABLE))
#endif
	{
		return;
	}

	if (!CMountInventoryHelper::CanMoveIntoMountInventory(itemEntity))
		return;

	const uint32_t vnum = ItemSystem::GetItemVnum(itemEntity);
	const int totalSlots =
		MountSystem::GetMountInventoryWidth(character) *
		MountSystem::GetMountInventorySize(character);
	for (int slot = 0; slot < totalSlots; ++slot)
	{
		const entt::entity storedItem = MountSystem::GetMountInventoryItem(character, slot);
		if (!ItemSystem::IsValidItem(storedItem))
			continue;

		if (ItemSystem::GetItemVnum(storedItem) == vnum)
		{
			ecs::ChatSystem::Send(
				ownerEntity,
				CHAT_TYPE_INFO,
				"This mount is already in your account inventory.");
			return;
		}
	}

	if (vnum >= 18000 && vnum <= 18149)
	{
		const uint32_t group = vnum / 10;
		for (int slot = 0; slot < totalSlots; ++slot)
		{
			const entt::entity storedItem = MountSystem::GetMountInventoryItem(character, slot);
			if (!ItemSystem::IsValidItem(storedItem))
				continue;

			const uint32_t storedVnum = ItemSystem::GetItemVnum(storedItem);
			if (storedVnum >= 18000 && storedVnum <= 18149 &&
				storedVnum / 10 == group)
			{
				ecs::ChatSystem::Send(
					ownerEntity,
					CHAT_TYPE_INFO,
					"You already have a belt of this type in your inventory.");
				return;
			}
		}
	}

	const TItemPos originalPos = p->ItemPos;
	const bool clearQuickslot = !ItemSystem::IsDragonSoulItem(itemEntity) && !ItemSystem::IsExtraItem(itemEntity);
	if (!ItemSystem::RemoveItemEcs(itemEntity))
		return;


	if (IsDetachedInputItem(itemEntity))
		ItemSystem::FlushDelayedSaveEcs(itemEntity);
	if (MountSystem::GetMountInventory(character) != mountInventory || !IsDetachedInputItem(itemEntity) ||
		!MountSystem::AddMountInventoryItem(character, p->wMountPos, itemEntity))
	{
		if (IsDetachedInputItem(itemEntity))
			ItemSystem::SetItemSkipSave(itemEntity, false);
		if (RestoreInputItem(character, itemEntity, originalPos) &&
			IsInputItemAt(character, itemEntity, originalPos))
			ItemSystem::FlushDelayedSaveEcs(itemEntity);
		return;
	}
	if (!IsInputItemAt(character, itemEntity, TItemPos(MOUNT_INVENTORY, p->wMountPos)))
		return;
	if (clearQuickslot && ItemSystem::GetItem(character, originalPos) == entt::null)
		InventorySystem::SyncQuickslot(character, QUICKSLOT_TYPE_ITEM, originalPos.cell, 255);

	if (!ecs::PlayerRuntime::IsPC(character))
		return;
	MountSystem::SendMountInventory(character);
	if (!ecs::PlayerRuntime::IsPC(character))
		return;
	ecs::PointSystem::Compute(character);
	NetworkSyncSystem::PointsPacket(ownerEntity);
#ifdef ENABLE_FAKE_SHOP_HEADER
	MountSystem::UpdateMountCountOverheadToViewers(character);
#endif
}



void CInputMain::MountInventoryCheckout(entt::entity character, const char* c_pData)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character) || !InventorySystem::CanHandleItems(character))
		return;

	const entt::entity ownerEntity = character;
	if (ownerEntity == entt::null || !g_registry.valid(ownerEntity))
		return;

	const auto request = *reinterpret_cast<const TPacketCGMountInventoryCheckout*>(c_pData);
	const auto* p = &request;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(ownerEntity) > GM_PLAYER &&
		ecs::PlayerRuntime::GetGMLevel(ownerEntity) < GM_IMPLEMENTOR)
	{
		return;
	}
#endif

	const entt::entity mountInventory = MountSystem::GetMountInventory(character);
	if (mountInventory == entt::null ||
		!MountSystem::IsMountInventoryPositionValid(character, p->wMountPos))
		return;

	if (p->ItemPos.window_type != INVENTORY ||
		p->ItemPos.IsBeltInventoryPosition())
	{
		return;
	}

	const entt::entity itemEntity = MountSystem::GetMountInventoryItem(character, p->wMountPos);
	if (!IsInputItemAt(character, itemEntity, TItemPos(MOUNT_INVENTORY, p->wMountPos)) ||
		ItemSystem::IsItemExchanging(itemEntity) ||
		ItemSystem::IsItemLocked(itemEntity))
	{
		return;
	}

	if (!InventorySystem::IsEmptyItemGrid(character, p->ItemPos, ItemSystem::GetItemSize(itemEntity)))
		return;

	if (MountSystem::RemoveMountInventoryItem(character, p->wMountPos) != itemEntity ||
		!IsDetachedInputItem(itemEntity))
		return;

	ItemSystem::SetItemSkipSave(itemEntity, false);
	if (!RestoreInputItem(ownerEntity, itemEntity, p->ItemPos))
	{
		if (MountSystem::GetMountInventory(character) == mountInventory && IsDetachedInputItem(itemEntity))
			MountSystem::AddMountInventoryItem(character, p->wMountPos, itemEntity);
		return;
	}

	if (!IsInputItemAt(character, itemEntity, p->ItemPos))
		return;
	ItemSystem::FlushDelayedSaveEcs(itemEntity);
	if (!IsInputItemAt(character, itemEntity, p->ItemPos))
		return;

	const uint32_t itemId = ItemSystem::GetItemID(itemEntity);
	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_FLUSH, 0, sizeof(itemId));
	db_clientdesc->Packet(&itemId, sizeof(itemId));

	if (!ecs::PlayerRuntime::IsPC(character))
		return;
	MountSystem::SendMountInventory(character);
	if (!ecs::PlayerRuntime::IsPC(character))
		return;
	ecs::PointSystem::Compute(character);
	NetworkSyncSystem::PointsPacket(ownerEntity);
#ifdef ENABLE_FAKE_SHOP_HEADER
	MountSystem::UpdateMountCountOverheadToViewers(character);
#endif
}



void CInputMain::MountInventoryItemMove(entt::entity character, const char* data)
{

	const auto p = reinterpret_cast<const TPacketCGMountInventoryItemMove*>(data);

	if (!data || !ecs::PlayerRuntime::IsPC(character) || !InventorySystem::CanHandleItems(character))
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR)
		return;
#endif

	if (MountSystem::GetMountInventory(character) == entt::null)
		return;

	MountSystem::MoveMountInventoryItem(character, p->wMountPos, p->wDestPos);
	if (!ecs::PlayerRuntime::IsPC(character))
		return;
	MountSystem::SendMountInventory(character);

	// (count nem v�ltozik, de egys�ges)
	if (!ecs::PlayerRuntime::IsPC(character))
		return;
	ecs::PointSystem::Compute(character);

#ifdef ENABLE_FAKE_SHOP_HEADER
	MountSystem::UpdateMountCountOverheadToViewers(character);
#endif
}



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

// PARTY_JOIN_BUG_FIX
void CInputMain::PartyInvite(entt::entity character, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyInvite handler ECS
// DUAL-PATH: legacy only during migration window
	if (ecs::PlayerRuntime::GetArena(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	TPacketCGPartyInvite * p = (TPacketCGPartyInvite*) c_pData;

	const entt::entity pInvitee = CHARACTER_MANAGER::instance().FindEntity(p->vid);

	if (pInvitee == entt::null || !ecs::PlayerRuntime::GetDesc(character) || !ecs::PlayerRuntime::GetDesc(pInvitee))
	{
		LOG_ERROR("PARTY Cannot find invited character");
		return;
	}

	ecs::SocialSystem::PartyInvite(character, pInvitee);
}

void CInputMain::PartyInviteAnswer(entt::entity character, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyInviteAnswer handler ECS
// DUAL-PATH: legacy only during migration window
	if (ecs::PlayerRuntime::GetArena(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	TPacketCGPartyInviteAnswer * p = (TPacketCGPartyInviteAnswer*) c_pData;

	const entt::entity inviter = CHARACTER_MANAGER::instance().FindEntity(p->leader_vid);
	if (!ecs::IsCharacter(inviter) || !ecs::PlayerRuntime::GetDesc(inviter)) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 217, "");
#endif
	}
	else if (!p->accept) {
		ecs::SocialSystem::PartyInviteDeny(inviter, ecs::PlayerRuntime::GetPlayerID(character));
	} else {
		ecs::SocialSystem::PartyInviteAccept(inviter, character);
	}
}
// END_OF_PARTY_JOIN_BUG_FIX

void CInputMain::PartySetState(entt::entity character, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartySetState handler ECS
// DUAL-PATH: legacy only during migration window
	if (!CPartyManager::instance().IsEnablePCParty())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 208, "");
#endif
		return;
	}

	TPacketCGPartySetState* p = (TPacketCGPartySetState*) c_pData;

	const entt::entity party = ecs::SocialSystem::GetParty(character);

	if (party == entt::null)
		return;

	if (PartySystem::GetLeaderPID(party) != ecs::PlayerRuntime::GetPlayerID(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 206, "");
#endif
		return;
	}

	if (!PartySystem::IsMember(party, p->pid))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 207, "");
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
			if (PartySystem::SetRole(party, pid, p->byRole, p->flag))
			{
				TPacketPartyStateChange pack;
				pack.dwLeaderPID = ecs::PlayerRuntime::GetPlayerID(character);
				pack.dwPID = p->pid;
				pack.bRole = p->byRole;
				pack.bFlag = p->flag;
				db_clientdesc->DBPacket(HEADER_GD_PARTY_STATE_CHANGE, 0, &pack, sizeof(pack));
			}
			break;
		default:
			LOG_ERROR("wrong byRole in PartySetState Packet name {} state {}", ecs::PlayerRuntime::GetName(character).data(), p->byRole);
			break;
	}
}

void CInputMain::PartyRemove(entt::entity character, const char* c_pData)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character))
		return;
	if (ecs::PlayerRuntime::GetArena(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	if (!CPartyManager::instance().IsEnablePCParty())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 208, "");
#endif
		return;
	}

	if (ecs::SocialSystem::GetDungeon(character) != entt::null)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 203, "");
#endif
		return;
	}

	TPacketCGPartyRemove* p = (TPacketCGPartyRemove*) c_pData;

	const entt::entity pParty = ecs::SocialSystem::GetParty(character);

	if (pParty == entt::null)
		return;

	if (PartySystem::GetLeaderPID(pParty) == ecs::PlayerRuntime::GetPlayerID(character))
	{
		if (ecs::SocialSystem::GetDungeon(character) == entt::null) {
			// ���漺���� ��Ƽ���� ���� �ۿ��� ��Ƽ �ػ� ���ϰ� ����
			if(PartySystem::IsPartyInDungeon(pParty, 351))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 648, "");
#endif
				return;
			}

			// leader can remove any member
			if (p->pid == ecs::PlayerRuntime::GetPlayerID(character) || PartySystem::GetMemberCount(pParty) == 2)
			{
				// party disband
				CPartyManager::instance().DeleteParty(pParty);
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				const entt::entity B = CHARACTER_MANAGER::instance().FindEntityByPID(p->pid);
				if (B != entt::null) {
					//pParty->SendPartyRemoveOneToAll(B);
					ecs::ChatSystem::SendNew(B, CHAT_TYPE_INFO, 216, "");
					//pParty->Unlink(B);
					//CPartyManager::instance().SetPartyMember(ecs::PlayerRuntime::GetPlayerID(B), NULL);
				}
#endif
				PartySystem::Quit(pParty, p->pid);
			}
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 205, "");
		}
#endif
	}
	else
	{
		if (p->pid == ecs::PlayerRuntime::GetPlayerID(character))
		{
			if (ecs::SocialSystem::GetDungeon(character) == entt::null) {
				if (PartySystem::GetMemberCount(pParty) == 2) {
					CPartyManager::instance().DeleteParty(pParty);
				} else {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 215, "");
#endif
					//pParty->SendPartyRemoveOneToAll(ch);
					PartySystem::Quit(pParty, ecs::PlayerRuntime::GetPlayerID(character));
					//pParty->SendPartyRemoveAllToOne(ch);
					//CPartyManager::instance().SetPartyMember(ecs::PlayerRuntime::GetPlayerID(character), NULL);
				}
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 204, "");
			}
#endif
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 197, "");
		}
#endif
	}
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

void CInputMain::PartyUseSkill(entt::entity character, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyUseSkill handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGPartyUseSkill* p = (TPacketCGPartyUseSkill*) c_pData;
	const entt::entity pParty = ecs::SocialSystem::GetParty(character);
	if (pParty == entt::null)
		return;

	if (ecs::PlayerRuntime::GetPlayerID(character) != PartySystem::GetLeaderPID(pParty))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 211, "");
#endif
		return;
	}

	switch (p->bySkillIndex)
	{
		case PARTY_SKILL_HEAL:
			PartySystem::HealParty(pParty);
			break;
		case PARTY_SKILL_WARP:
			{
				const entt::entity pch = CHARACTER_MANAGER::instance().FindEntity(p->vid);
				if (pch != entt::null) {
					PartySystem::SummonToLeader(pParty, ecs::PlayerRuntime::GetPlayerID(pch));
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 209, "");
				}
#endif
			}
			break;
	}
}

void CInputMain::PartyParameter(entt::entity character, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyParameter handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGPartyParameter * p = (TPacketCGPartyParameter *) c_pData;

	const entt::entity pParty = ecs::SocialSystem::GetParty(character);
	if (pParty != entt::null)
		PartySystem::SetParameter(pParty, p->bDistributeMode);
}

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
				// by mhh : ����ڱ��� ��а� ���� �� ����.
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
				// by mhh : ����ڱ��� ��а� �� �� ����.
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
					// �߸��� ����.. ��������.
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

void CInputMain::Fishing(entt::entity character, const char* c_pData)
{
	TPacketCGFishing* p = (TPacketCGFishing*)c_pData;
	ecs::MovementSystem::SetRotation(character, p->dir * 5);
	ActivitySystem::Fishing(character);
	return;
}

void CInputMain::ItemGive(entt::entity character, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemGive handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGGiveItem* p = (TPacketCGGiveItem*) c_pData;
	const entt::entity to_chEntity = CHARACTER_MANAGER::instance().FindEntity(p->dwTargetVID);


	if (to_chEntity != entt::null) {
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
		if ((ecs::PlayerRuntime::GetGMLevel(to_chEntity) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(to_chEntity) < GM_IMPLEMENTOR) || (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR)) {
			return;
		}
#endif

		ItemSystem::GiveItem(character, to_chEntity, p->ItemPos);
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 403, "");
	}
#endif
}

void CInputMain::Hack(entt::entity character, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Hack handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGHack * p = (TPacketCGHack *) c_pData;

	char buf[sizeof(p->szBuf)];
	strlcpy(buf, p->szBuf, sizeof(buf));

	LOG_ERROR("HACK_DETECT: {} {}", ecs::PlayerRuntime::GetName(character).data(), buf);

	// ���� Ŭ���̾�Ʈ���� �� ��Ŷ�� ������ ��찡 �����Ƿ� ������ ������ �Ѵ�
	ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);
}

int CInputMain::MyShop(entt::entity character, const char * c_pData, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate MyShop handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGMyShop * p = (TPacketCGMyShop *) c_pData;
	int iExtraLen = p->bCount * sizeof(TShopItemTable);

	if (uiBytes < sizeof(TPacketCGMyShop) + iExtraLen)
		return -1;

	if (ecs::PointSystem::GetGold(character) >= GOLD_MAX)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 226,
		"%lld"

		, GOLD_MAX);
#endif
		return (iExtraLen);
	}

	if (CombatSystem::IsStun(character) || CombatSystem::IsDead(character))
		return (iExtraLen);

	if (ecs::SocialSystem::HasExchange(character) || ecs::SessionSystem::IsSafeboxOpen(character) || ecs::SocialSystem::GetShopOwner(character) != entt::null || ecs::SessionSystem::IsCubeOpen(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 292, "");
#endif
		return (iExtraLen);
	}

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (AttrTransfer_is_open(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 292, "");
#endif
		return (iExtraLen);
	}
#endif

	LOG_INFO("MyShop count {}", p->bCount);
	ecs::SocialSystem::OpenMyShop(character, p->szSign, (TShopItemTable *) (c_pData + sizeof(TPacketCGMyShop)), p->bCount
#ifdef KASMIR_PAKET_SYSTEM
	, p->dwKasmirNpc, p->bKasmirBaslik
#endif
	);
	return (iExtraLen);
}

void CInputMain::Refine(entt::entity character, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Refine handler ECS
// DUAL-PATH: legacy only during migration window
	const TPacketCGRefine* p = reinterpret_cast<const TPacketCGRefine*>(c_pData);
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR) {
		InventorySystem::ClearRefineMode(character);
		return;
	}
#endif

	if (ecs::SocialSystem::HasExchange(character) || ecs::SessionSystem::IsSafeboxOpen(character) || ecs::SocialSystem::GetShopOwner(character) != entt::null || ecs::SocialSystem::GetMyShop(character) != entt::null || ecs::SessionSystem::IsCubeOpen(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 502, "");
#endif
		InventorySystem::ClearRefineMode(character);
		return;
	}

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (AttrTransfer_is_open(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 292, "");
#endif
		InventorySystem::ClearRefineMode(character);
		return;
	}
#endif

	if (p->type == 255)
	{
		// DoRefine Cancel
		InventorySystem::ClearRefineMode(character);
		return;
	}

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	if (p->pos >= InventorySystem::GetInventorySize(character))
#else
	if (p->pos >= INVENTORY_MAX_NUM)
#endif
	{
		InventorySystem::ClearRefineMode(character);
		return;
	}

	const entt::entity owner = character;
	const entt::entity itemEntity = ItemSystem::GetInventoryItem(owner, p->pos);

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	if (!CRefineManager::instance().GetPercentage(owner, p->lLow, p->lMedium, p->lExtra, p->lTotal, itemEntity))
	{
		InventorySystem::ClearRefineMode(character);
		return;
	}

	CRefineManager::instance().Increase(owner, p->lLow, p->lMedium, p->lExtra);
#endif

	if (!ItemSystem::IsValidItem(itemEntity))
	{
		InventorySystem::ClearRefineMode(character);
		return;
	}

	ecs::SocialSystem::SetRefineTime(owner);

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
		if (ItemSystem::IsValidItem(itemEntity)) {
			if (ecs::QuestSystem::GetFlag(character, "deviltower_zone.can_refine"))
			{
#ifdef ENABLE_BUG_FIXES
				if (ItemSystem::DoRefine(owner, itemEntity, true)) {
					ecs::QuestSystem::SetFlag(character, "deviltower_zone.can_refine", 0);
				}
#else
				ItemSystem::DoRefine(owner, itemEntity, true);
				ecs::QuestSystem::SetFlag(character, "deviltower_zone.can_refine", 0);
#endif
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 361, "");
			}
#endif
		}
	}

	InventorySystem::ClearRefineMode(character);
}



#ifdef ENABLE_ACCE_SYSTEM
void CInputMain::Acce(entt::entity character, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Acce handler ECS
// DUAL-PATH: legacy only during migration window

	quest::PC * pPC = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(character));
	if (pPC->IsRunning())
		return;

	TPacketAcce * sPacket = (TPacketAcce*) c_pData;
	switch (sPacket->subheader)
	{
	case ACCE_SUBHEADER_CG_CLOSE:
	{
		ecs::AcceSystem::Close(character);
	}
	break;
	case ACCE_SUBHEADER_CG_ADD:
	{
		ecs::AcceSystem::AddMaterial(character, sPacket->tPos, sPacket->bPos);
	}
	break;
	case ACCE_SUBHEADER_CG_REMOVE:
	{
		ecs::AcceSystem::RemoveMaterial(character, sPacket->bPos);
	}
	break;
	case ACCE_SUBHEADER_CG_REFINE:
	{
		ecs::AcceSystem::Refine(character);
	}
	break;
	default:
		break;
	}
}
#endif

#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
void CInputMain::CubeRenewalSend(entt::entity character, const char* data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate CubeRenewalSend handler ECS
// DUAL-PATH: legacy only during migration window
	struct packet_send_cube_renewal * pinfo = (struct packet_send_cube_renewal *) data;
	switch (pinfo->subheader)
	{
		case CUBE_RENEWAL_SUB_HEADER_MAKE_ITEM:
		{

			if (pinfo->index_item > static_cast<uint32_t>(INT_MAX) ||
				pinfo->count_item == 0 ||
				pinfo->count_item > static_cast<uint32_t>(g_bItemCountLimit))
			{
				return;
			}

			int index_item_improve = -1;
			if (pinfo->index_item_improve != UINT32_MAX)
			{
				if (pinfo->index_item_improve >= INVENTORY_MAX_NUM)
					return;
				index_item_improve = static_cast<int>(pinfo->index_item_improve);
			}

			Cube_Make(
				character,
				static_cast<int>(pinfo->index_item),
				static_cast<int>(pinfo->count_item),
				index_item_improve);
		}
		break;

		case CUBE_RENEWAL_SUB_HEADER_CLOSE:
		{
			Cube_close(character);
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

int OfflineshopPacketCreateNewShop(entt::entity ch, const char* data, int iBufferLeft)
{
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
		if (ecs::PlayerRuntime::IsValid(ch)) {
			offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_CREATE_SHOP);
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "RefreshOfflineShop");
		}
	}

	return iExtra;
}


int OfflineshopPacketChangeShopName(entt::entity ch, const char* data, int iBufferLeft)
{
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


int OfflineshopPacketForceCloseShop(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopForceCloseClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_FORCE_CLOSE);

	return 0;
}


int OfflineshopPacketRequestShopList(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopRequestListClientPacket(ch);
	return 0;
}


int OfflineshopPacketOpenShop(entt::entity ch, const char* data, int iBufferLeft)
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


int OfflineshopPacketOpenShowOwner(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopOpenMyShopClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_OPEN_SHOP_OWNER);

	return 0;
}


int OfflineshopPacketBuyItem(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopBuyItem* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopBuyItemClientPacket(ch, pack->dwOwnerID, pack->dwItemID, pack->bIsSearch, pack->TotalPriceSeen)) //patch seen price check
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_BUY_ITEM);

	return iExtra;
}


int OfflineshopPacketAddItem(entt::entity ch, const char* data, int iBufferLeft)
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


int OfflineshopPacketRemoveItem(entt::entity ch, const char* data, int iBufferLeft)
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


int OfflineshopPacketEditItem(entt::entity ch, const char* data, int iBufferLeft)
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


int OfflineshopPacketFilterRequest(entt::entity ch, const char* data, int iBufferLeft)
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


int OfflineshopPacketCreateOffer(entt::entity ch, const char* data, int iBufferLeft)
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


int OfflineshopPacketAcceptOffer(entt::entity ch, const char* data, int iBufferLeft)
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



int OfflineshopPacketOfferCancel(entt::entity ch, const char* data, int iBufferLeft)
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


int OfflineshopPacketOfferListRequest(entt::entity ch)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvOfferListRequestPacket(ch);
	return 0;
}


int OfflineshopPacketOpenSafebox(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopSafeboxOpenClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_OPEN_SAFEBOX);

	return 0;
}


int OfflineshopPacketCloseBoard(entt::entity ch)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvCloseBoardClientPacket(ch);
	return 0;
}

int OfflineshopPacketCloseMyAuction(entt::entity ch)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvCloseMyAuction(ch);
	return 0;
}

int OfflineshopPacketGetItemSafebox(entt::entity ch, const char* data, int iBufferLeft)
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


int OfflineshopPacketGetValutesSafebox(entt::entity ch, const char* data, int iBufferLeft)
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


int OfflineshopPacketCloseSafebox(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopSafeboxCloseClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_SAFEBOX_CLOSE);

	return 0;
}


int OfflineshopPacketListRequest(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvAuctionListRequestClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_AUCTION_CANNOT_SEND_LIST);

	return 0;
}



int OfflineshopPacketOpenAuctionRequest(entt::entity ch, const char* data, int iBufferLeft)
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



int OfflineshopPacketOpenMyAuctionRequest(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvMyAuctionOpenRequestClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_AUCTION_CANNOT_SEND_LIST);

	return 0;
}



int OfflineshopPacketCreateAuction(entt::entity ch, const char* data, int iBufferLeft)
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



int OfflineshopPacketAddOffer(entt::entity ch, const char* data, int iBufferLeft)
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



int OfflineshopPacketExitFromAuction(entt::entity ch, const char* data, int iBufferLeft)
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
int OfflineshopPacketClickEntity(entt::entity ch, const char* data, int iBufferLeft)
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



int OfflineshopPacket(const char* data , entt::entity ch, int32_t iBufferLeft)
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

void CInputMain::ItemDestroy(entt::entity character, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemDestroy handler ECS
// DUAL-PATH: legacy only during migration window
	struct command_item_destroy * pinfo = (struct command_item_destroy *) data;
	if (ecs::IsCharacter(character)) {
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
		if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR) {
			return;
		}
#endif
		ItemSystem::DestroyItem(character, pinfo->Cell);
	}
}

void CInputMain::ItemDivision(entt::entity character, const char * data)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;
	struct command_item_division * pinfo = (struct command_item_division *) data;
	ItemSystem::ItemDivision(character, pinfo->pos);
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

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
void CInputMain::WheelDestiny(entt::entity character, const char* data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate WheelDestiny handler ECS
// DUAL-PATH: legacy only during migration window
	if (!ecs::IsCharacter(character))
	{
		return;
	}

	if (ecs::PlayerRuntime::IsObserverMode(character) || ecs::SocialSystem::HasExchange(character))
	{
		return;
	}

	const auto pinfo = reinterpret_cast<const TPacketCGWheelDestiny*>(data);
	enum { OPEN, CLOSE, TURN, GIVE };

	switch (pinfo->option)
	{
	case OPEN:
	{

		if (!ecs::PlayerRuntime::GetWheelDestiny(character))
		{
			ecs::PlayerRuntime::SetWheelDestiny(character, std::make_shared<CWheelDestiny>(character));
		}
	}
	break;
	case CLOSE:

	{
		if (ecs::PlayerRuntime::GetWheelDestiny(character))
		{


			if (ecs::PlayerRuntime::GetWheelDestiny(character)->GetGiftVnum())
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1307, "");
#endif
			}
			else
			{
				ecs::PlayerRuntime::SetWheelDestiny(character, nullptr);
				ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "BINARY_WHEEL_CLOSE");
			}
		}
	}
	break;
	case TURN:
	{
		if (ecs::SocialSystem::GetDungeon(character) != entt::null || ecs::PlayerRuntime::GetMapIndex(character) >= 10000)
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Dungeonban nem tudsz p�rgetni./You cannot in dungeon");
			return;
		}
		if (ecs::PlayerRuntime::GetWheelDestiny(character))
		{
			static const uint32_t WHEEL_TICKET_VNUM = 70610;

			if (ItemSystem::CountItem(character, WHEEL_TICKET_VNUM) < 1)
			{

				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You Dont have Battle Pass Ticket");
				return;
			}

			ItemSystem::RemoveSpecifyItemEcs(character, WHEEL_TICKET_VNUM, 1);

			ecs::PlayerRuntime::GetWheelDestiny(character)->TurnWheel();
		}
	}
	break;

	case GIVE:
	{
		if (ecs::PlayerRuntime::GetWheelDestiny(character))
		{
			ecs::PlayerRuntime::GetWheelDestiny(character)->GiveMyFuckingGift();
		}
	}
	break;
	default:
	{
		LOG_ERROR("CInputMain::WheelDestiny : Unknown option {} : {}", pinfo->option, ecs::PlayerRuntime::GetName(character).data());
	}
	break;
	}
}
#endif

int CInputMain::Analyze(LPDESC d, uint8_t bHeader, const char * c_pData)
{

	if (!d || !c_pData)
		return 0;
	const entt::entity character = d->GetEntity();
	if (!ecs::PlayerRuntime::IsPC(character) || ecs::PlayerRuntime::GetDesc(character) != d)
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
			if ((iExtraLen = Chat(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_WHISPER:
			if ((iExtraLen = Whisper(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_MOVE:
			Move(character, c_pData);
			// @fixme103 (removed CheckClientVersion since useless in here)
			break;

		case HEADER_CG_CHARACTER_POSITION:
			Position(character, c_pData);
			break;

		case HEADER_CG_ITEM_USE:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemUse(character, c_pData);
			break;

		case HEADER_CG_ITEM_DROP:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
			{
				ItemDrop(character, c_pData);
			}
			break;

#ifdef ENABLE_ACCE_SYSTEM
		case HEADER_CG_ACCE:
			Acce(character, c_pData);
			break;
#endif

		case HEADER_CG_ITEM_DROP2:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemDrop2(character, c_pData);
			break;
		case HEADER_CG_ITEM_DESTROY:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemDestroy(character, c_pData);
			break;
		case HEADER_CG_ITEM_DIVISION:
			{
				if (!ecs::PlayerRuntime::IsObserverMode(character))
					ItemDivision(character, c_pData);
			}
			break;
		case HEADER_CG_ITEM_MOVE:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemMove(character, c_pData);
			break;


#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
		case ENVANTER_BLACK:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				InventoryExpansion(character, c_pData);
		break;
#endif

		case HEADER_CG_ITEM_PICKUP:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemPickup(character, c_pData);
			break;

		case HEADER_CG_ITEM_USE_TO_ITEM:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemToItem(character, c_pData);
			break;

		case HEADER_CG_ITEM_GIVE:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemGive(character, c_pData);
			break;

		case HEADER_CG_EXCHANGE:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				Exchange(character, c_pData);
			break;

		case HEADER_CG_ATTACK:
		case HEADER_CG_SHOOT:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
			{
				Attack(character, bHeader, c_pData);
			}
			break;

		case HEADER_CG_USE_SKILL:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				UseSkill(character, c_pData);
			break;

#ifdef __SKILL_COLOR_SYSTEM__
		case HEADER_CG_SKILL_COLOR:
			SetSkillColor(character, c_pData);
			break;
#endif
#ifdef ENABLE_OPENSHOP_PACKET
		case HEADER_CG_OPENSHOP: {
				TPacketOpenShop* p = reinterpret_cast<TPacketOpenShop*>((void*)c_pData);
				if (p->shopid > 0) {
					if (!(ecs::PlayerRuntime::IsObserverMode(character) || ecs::SessionSystem::IsSafeboxOpen(character) || ecs::SocialSystem::HasExchange(character) || ecs::SessionSystem::IsCubeOpen(character) || CombatSystem::IsStun(character) || CombatSystem::IsDead(character)
#ifdef __ATTR_TRANSFER_SYSTEM__
						 || AttrTransfer_is_open(character)
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
						 || (g_registry.try_get<ecs::ShopState>(character) &&
						     (g_registry.get<ecs::ShopState>(character).offlineShopGuest || g_registry.get<ecs::ShopState>(character).auctionGuest))
#endif
					)) {
						const entt::entity shop = CShopManager::instance().Get(p->shopid);
						if (shop != entt::null) {
							ShopSystem::AddGuest(shop, character, 0, false);
							ecs::SocialSystem::SetShopOwner(character, entt::null);
						}
					}
				}
			} break;
#endif
		case HEADER_CG_QUICKSLOT_ADD:
			QuickslotAdd(character, c_pData);
			break;

		case HEADER_CG_QUICKSLOT_DEL:
			QuickslotDelete(character, c_pData);
			break;

		case HEADER_CG_QUICKSLOT_SWAP:
			QuickslotSwap(character, c_pData);
			break;

		case HEADER_CG_SHOP:
			if ((iExtraLen = Shop(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_MESSENGER:
			if ((iExtraLen = Messenger(character, c_pData, m_iBufferLeft))<0)
				return -1;
			break;

#ifdef ENABLE_BATTLE_PASS
		case HEADER_CG_BATTLE_PASS:
			if ((iExtraLen = BattlePass(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;
#endif

		case HEADER_CG_ON_CLICK:
			OnClick(character, c_pData);
			break;

		case HEADER_CG_SYNC_POSITION:
			if ((iExtraLen = SyncPosition(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_ADD_FLY_TARGETING:
		case HEADER_CG_FLY_TARGETING:
			FlyTarget(character, c_pData, bHeader);
			break;

		case HEADER_CG_SCRIPT_BUTTON:
			ScriptButton(character, c_pData);
			break;

			// SCRIPT_SELECT_ITEM
		case HEADER_CG_SCRIPT_SELECT_ITEM:
			ScriptSelectItem(character, c_pData);
			break;
			// END_OF_SCRIPT_SELECT_ITEM

		case HEADER_CG_SCRIPT_ANSWER:
			ScriptAnswer(character, c_pData);
			break;

		case HEADER_CG_QUEST_INPUT_STRING:
			QuestInputString(character, c_pData);
			break;

		case HEADER_CG_QUEST_CONFIRM:
			QuestConfirm(character, c_pData);
			break;

		case HEADER_CG_TARGET:
			Target(character, c_pData);
			break;

		case HEADER_CG_WARP:
			Warp(character, c_pData);
			break;

		case HEADER_CG_SAFEBOX_CHECKIN:
			SafeboxCheckin(character, c_pData);
			break;

		case HEADER_CG_SAFEBOX_CHECKOUT:
			SafeboxCheckout(character, c_pData, false);
			break;

		case HEADER_CG_SAFEBOX_ITEM_MOVE:
			SafeboxItemMove(character, c_pData);
			break;

		case HEADER_CG_MALL_CHECKOUT:
			SafeboxCheckout(character, c_pData, true);
			break;


		case HEADER_CG_MOUNT_INVENTORY_CHECKIN:
			MountInventoryCheckin(character, c_pData);
			break;

		case HEADER_CG_MOUNT_INVENTORY_CHECKOUT:
			MountInventoryCheckout(character, c_pData);
			break;

		case HEADER_CG_MOUNT_INVENTORY_ITEM_MOVE:
			MountInventoryItemMove(character, c_pData);
			break;

		case HEADER_CG_PARTY_INVITE:
			PartyInvite(character, c_pData);
			break;

		case HEADER_CG_PARTY_REMOVE:
			PartyRemove(character, c_pData);
			break;

		case HEADER_CG_PARTY_INVITE_ANSWER:
			PartyInviteAnswer(character, c_pData);
			break;

		case HEADER_CG_PARTY_SET_STATE:
			PartySetState(character, c_pData);
			break;

		case HEADER_CG_PARTY_USE_SKILL:
			PartyUseSkill(character, c_pData);
			break;

		case HEADER_CG_PARTY_PARAMETER:
			PartyParameter(character, c_pData);
			break;
#ifdef __INGAME_WIKI__
		case InGameWiki::HEADER_CG_WIKI:
			RecvWikiPacket(character, c_pData);
			break;
#endif
		case HEADER_CG_ANSWER_MAKE_GUILD:
#ifdef ENABLE_NEWGUILDMAKE
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "<%s> AnswerMakeGuild disabled", __FUNCTION__);
#else
			AnswerMakeGuild(character, c_pData);
#endif
			break;

		case HEADER_CG_GUILD:
			if ((iExtraLen = Guild(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_FISHING:
			Fishing(character, c_pData);
			break;
		case HEADER_CG_HACK:
			Hack(character, c_pData);
			break;

#ifdef __NEWPET_SYSTEM__
		case HEADER_CG_PetSetName:
			BraveRequestPetName(character, c_pData);
			break;
#endif
		case HEADER_CG_MYSHOP:
			if ((iExtraLen = MyShop(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_REFINE:
			Refine(character, c_pData);
			break;

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
		case HEADER_CG_WHISPER_ADMIN:
			CWhisperAdmin::instance().Manager(character, c_pData);
			break;
#endif

		case HEADER_CG_CLIENT_VERSION:
			Version(character, c_pData);
			break;


#ifdef ENABLE_MULTI_LANGUAGE
		case HEADER_CG_CHANGE_LANGUAGE:
			{
				TPacketChangeLanguage* p = reinterpret_cast <TPacketChangeLanguage*>((void*)c_pData);
				ChangeLanguage(character, p->bLanguage);
			}
			break;
		case HEADER_CG_REQUEST_LANGUAGE:
			{
				TPacketRequestLang* p = reinterpret_cast <TPacketRequestLang*>((void*)c_pData);
				RequestLanguage(character, p->targetName);
			}
			break;
#endif
#ifdef __SEND_TARGET_INFO__
		case HEADER_CG_TARGET_INFO_LOAD:
			{
				TargetInfoLoad(character, c_pData);
			}
			break;
#endif
#ifdef ENABLE_SWITCHBOT
		case HEADER_CG_SWITCHBOT:
			if ((iExtraLen = Switchbot(character, c_pData, m_iBufferLeft)) < 0)
			{
				return -1;
			}
			break;
#endif
#ifdef ENABLE_MAP_TELEPORTER
		case HEADER_CG_MAP_TELEPORTER:
			MapTeleporter(character, (TPacketCGMapTeleporter*) c_pData);
			break;
#endif
		case HEADER_CG_DRAGON_SOUL_REFINE:
			{
				TPacketCGDragonSoulRefine* p = reinterpret_cast <TPacketCGDragonSoulRefine*>((void*)c_pData);
				switch(p->bSubType)
				{
				case DS_SUB_HEADER_CLOSE:
					DragonSoulSystem::CloseRefineWindow(character);
					break;
				case DS_SUB_HEADER_DO_REFINE_GRADE:
					{
						DSManager::instance().DoRefineGradeEcs(character, p->ItemGrid);
					}
					break;
				case DS_SUB_HEADER_DO_REFINE_STEP:
					{
						DSManager::instance().DoRefineStepEcs(character, p->ItemGrid);
					}
					break;
				case DS_SUB_HEADER_DO_REFINE_STRENGTH:
					{
						DSManager::instance().DoRefineStrengthEcs(character, p->ItemGrid);
					}
					break;
				}
			}
			break;
#ifdef ENABLE_DS_REFINE_ALL
		case HEADER_CG_DRAGON_SOUL_REFINE_ALL: {
			TPacketDragonSoulRefineAll* p = reinterpret_cast <TPacketDragonSoulRefineAll*>((void*)c_pData);
			DSManager::instance().DoRefineAllEcs(character, p->subheader, p->type, p->grade);
		} break;
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
		case HEADER_CG_NEW_OFFLINESHOP:
			if((iExtraLen = OfflineshopPacket(c_pData, character, m_iBufferLeft))< 0)
				return -1;
			break;
#endif
#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
		case HEADER_CG_CUBE_RENEWAL:
			CubeRenewalSend(character, c_pData);
			break;
#endif
#ifdef ENABLE_NEW_FISHING_SYSTEM
		case HEADER_CG_FISHING_NEW:
			{
				FishingNew(character, c_pData);
			}
			break;
#endif
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
		case HEADER_CG_WHEEL_DESTINY:
		{
			WheelDestiny(character, c_pData);
		}
		break;
#endif
	}
	return (iExtraLen);
}

int CInputDead::Analyze(LPDESC d, uint8_t bHeader, const char * c_pData)
{
	if (!d || !c_pData)
		return 0;
	const entt::entity character = d->GetEntity();
	if (!ecs::PlayerRuntime::IsPC(character) || ecs::PlayerRuntime::GetDesc(character) != d)
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
			if ((iExtraLen = Chat(character, c_pData, m_iBufferLeft)) < 0)
				return -1;

			break;

		case HEADER_CG_WHISPER:
			if ((iExtraLen = Whisper(character, c_pData, m_iBufferLeft)) < 0)
				return -1;

			break;

		case HEADER_CG_HACK:
			Hack(character, c_pData);
			break;

		default:
			return (0);
	}

	return (iExtraLen);
}
#ifdef ENABLE_SWITCHBOT
int CInputMain::Switchbot(entt::entity character, const char* data, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Switchbot handler ECS
// DUAL-PATH: legacy only during migration window
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

		CSwitchbotManager::Instance().Start(ecs::PlayerRuntime::GetPlayerID(character), p->slot, vec_alternatives);
		return extraLen;
	}

	case SUBHEADER_CG_SWITCHBOT_STOP:
	{
		CSwitchbotManager::Instance().Stop(ecs::PlayerRuntime::GetPlayerID(character), p->slot);
		return 0;
	}
	}

	return 0;
}
#endif



#ifdef ENABLE_MULTI_LANGUAGE
void CInputMain::ChangeLanguage(entt::entity character, uint8_t bLanguage)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	if (!ecs::PlayerRuntime::GetDesc(character))
		return;

	uint8_t bCurrentLanguage = ecs::PlayerRuntime::GetDesc(character)->GetLanguage();

	if(bCurrentLanguage == bLanguage)
		return;

	if(bLanguage > LANGUAGE_DEFAULT && bLanguage < LANGUAGE_MAX_NUM)
	{
		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE account.account SET language = %d WHERE id = %d;", bLanguage, ecs::PlayerRuntime::GetAccountID(character)));
		ecs::PlayerRuntime::GetDesc(character)->SetLanguage(bLanguage);
	}
}

void CInputMain::RequestLanguage(entt::entity character, const char* targetName)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate RequestLanguage handler ECS
// DUAL-PATH: legacy only during migration window
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	LPDESC d = ecs::PlayerRuntime::GetDesc(character);
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




