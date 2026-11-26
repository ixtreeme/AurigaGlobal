#include "stdafx.h"
#include "../item/TypedUniqueAccessories.hpp"
#include "SocialActions.hpp"
#include "SocialHistory.hpp"
#include "TypedChatLinks.hpp"
#include "TypedQuiz.hpp"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/components/dirty_components.hpp"
#include "../ecs/components/social_components.hpp"
#include "../ecs/systems/ActivitySystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/PlayerItemActions.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "banword.h"
#include "buffer_manager.h"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "constants.h"
#include "db.h"
#include "desc.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "empire_text_convert.h"
#include "event.h"
#include "log.h"
#include "packet.h"
#include "protocol.h"
#include "skill.h"
#include "spam.h"
#include "utils.h"
#include "../ecs/systems/MountSystem.hpp"
#include "guild.h"
#include "p2p.h"
#include "shop.h"
#include "unique_item.h"
#include "gm.h"

#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
static inline std::string MakeNameWithPrefix(entt::entity chEntity)
{
	const bool valid = ecs::PlayerRuntime::IsValid(chEntity);
	const char* name = valid ? ecs::PlayerRuntime::GetName(chEntity).data() : "";

	std::string out;



	if (!out.empty() && out.back() != ' ')
		out.push_back(' ');

	out += name;
	return out;
}

#endif

#ifdef ENABLE_CHAT_LOGGING
static char	__escape_string[1024];
static char	__escape_string2[1024];
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

namespace
{
template <std::size_t Size>
void CopyChatBody(char (&buffer)[Size], const char* body, std::size_t bodySize)
{
    const auto length = std::min(bodySize, Size - 1);
    std::memcpy(buffer, body, length);
    buffer[length] = '\0';
}

template <typename Packet>
std::vector<char> ChatPacketWithBody(Packet header, const std::string& text)
{
    std::vector<char> packet(sizeof(header) + text.size() + 1);
    if constexpr (std::is_same_v<Packet, TPacketCGChat>)
        header.size = static_cast<std::uint16_t>(packet.size());
    else
        header.wSize = static_cast<std::uint16_t>(packet.size());
    std::memcpy(packet.data(), &header, sizeof(header));
    std::memcpy(packet.data() + sizeof(header), text.c_str(), text.size() + 1);
    return packet;
}

int HandleWhisper(entt::entity character, const char* data, uint64_t uiBytes, bool prepared)

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

	if(pinfo->szNameTo[CHARACTER_NAME_MAX_LEN]!='\0')return iExtraLen;
	if(SocialBlocked(std::string(ecs::PlayerRuntime::GetName(character)),pinfo->szNameTo)){
		TPacketGCWhisper blocked{};blocked.bHeader=HEADER_GC_WHISPER;blocked.bType=WHISPER_TYPE_TARGET_BLOCKED;blocked.wSize=sizeof(blocked);strlcpy(blocked.szNameFrom,pinfo->szNameTo,sizeof(blocked.szNameFrom));
		if(auto* desc=ecs::PlayerRuntime::GetDesc(character))desc->Packet(&blocked,sizeof(blocked));return iExtraLen;
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
				CopyChatBody(buf, data + sizeof(TPacketCGWhisper), iExtraLen);
				const uint64_t buflen = strlen(buf);
                if (!prepared)
                {
                    CBanwordManager::instance().ConvertString(buf, buflen);
                    if (PrepareTypedChatLinks(character, std::string_view(buf, buflen), CHAT_MAX_LEN - 1,
                        [character, packet = *pinfo](std::string text) {
                            const auto wire = ChatPacketWithBody(packet, text);
                            HandleWhisper(character, wire.data(), wire.size(), true);
                        }))
                        return iExtraLen;
                }
				if (buflen > 0)
				{
					if(!SavePrivateHistory(character,pinfo->szNameTo,std::string(buf,buflen)))return iExtraLen;
					if(SocialHistoryEnabled())g_registry.get_or_emplace<ecs::OfflineMessageState>(character).lastSentTime=get_dword_time();
					else ecs::ChatSystem::SendOfflineMessage(character, pinfo->szNameTo, buf);
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
			CopyChatBody(buf, data + sizeof(TPacketCGWhisper), iExtraLen);
			const uint64_t buflen = strlen(buf);

			if (!prepared && SpamBlockCheck(character, buf, buflen))
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

            if (!prepared)
            {
                CBanwordManager::instance().ConvertString(buf, buflen);
                if (PrepareTypedChatLinks(character, std::string_view(buf, buflen), CHAT_MAX_LEN - 1,
                    [character, packet = *pinfo](std::string text) {
                        const auto wire = ChatPacketWithBody(packet, text);
                        HandleWhisper(character, wire.data(), wire.size(), true);
                    }))
                    return iExtraLen;
            }

			const std::string historyBody(buf,buflen);
			if (g_bEmpireWhisper)
				if (!auriga::item::runtime::HasTypedLanguageRing(character))
					if (!auriga::item::runtime::HasTypedLanguageRing(chr))
						if (bOpponentEmpire != ecs::PlayerRuntime::GetEmpire(character) && ecs::PlayerRuntime::GetEmpire(character) && bOpponentEmpire
								&& ecs::PlayerRuntime::GetGMLevel(character) == GM_PLAYER && gm_get_level(pinfo->szNameTo) == GM_PLAYER)
						{
							if (chr == entt::null)
							{
								bType = ecs::PlayerRuntime::GetEmpire(character) << 4;
							}
							else
							{
								ConvertEmpireText(ecs::PlayerRuntime::GetEmpire(character), buf, buflen, 10 + 2 * SkillSystem::GetSkillPower(chr, SKILL_LANGUAGE1 + ecs::PlayerRuntime::GetEmpire(character) - 1));
							}
						}

			if (ecs::PlayerRuntime::IsGM(character))
				bType = (bType & 0xF0) | WHISPER_TYPE_GM;

			if (buflen > 0)
			{
				if(!SavePrivateHistory(character,pinfo->szNameTo,historyBody)){pkDesc->SetRelay("");return iExtraLen;}
				TPacketGCWhisper pack;

				pack.bHeader = HEADER_GC_WHISPER;
				pack.wSize = sizeof(TPacketGCWhisper) + buflen;
				pack.bType = bType;
				strlcpy(pack.szNameFrom, ecs::PlayerRuntime::GetName(character).data(), sizeof(pack.szNameFrom));
				TEMP_BUFFER tmpbuf;

				tmpbuf.write(&pack, sizeof(pack));
				tmpbuf.write(buf, buflen);

                if (chr == entt::null)
                    pkDesc->SetRelay(pinfo->szNameTo);
				pkDesc->Packet(tmpbuf.read_peek(), tmpbuf.size());

				// @warme006
				// LOG_INFO(0, "WHISPER: %s -> %s : %s", ecs::PlayerRuntime::GetName(character).data(), pinfo->szNameTo, buf);
#ifdef ENABLE_CHAT_LOGGING
				if (!SocialHistoryEnabled()&&ecs::PlayerRuntime::IsGM(character))
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
            !auriga::item::runtime::HasTypedLanguageRing(recipient))
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

int HandleChat(entt::entity character, const char* data, uint32_t uiBytes, bool prepared)
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
	CopyChatBody(buf, data + sizeof(TPacketCGChat), iExtraLen);
	const uint64_t buflen = strlen(buf);

	if (buflen > 1 && *buf == '/')
	{

	interpret_command(character, buf + 1, buflen - 1);
	return iExtraLen;
	}


/* 	if (ecs::PlayerRuntime::GetGMLevel(character) == GM_PLAYER && ecs::PlayerRuntime::GetMapIndex(character) == 113)//OX mapon chat letiltva//
	{
		return iExtraLen;
	} */

	const CAffect* pAffect = AffectSystem::FindAffect(character, AFFECT_BLOCK_CHAT);

	if (pAffect != nullptr)
	{
		SendBlockChatInfo(character, pAffect->lDuration);
		return iExtraLen;
	}

    if (!prepared)
    {
        if (SpamBlockCheck(character, buf, buflen))
            return iExtraLen;
        CBanwordManager::instance().ConvertString(buf, buflen);
        // Space for the channel, speaker name and rank labels added below.
        if (PrepareTypedChatLinks(character, std::string_view(buf, buflen), CHAT_MAX_LEN - 128,
            [character, packet = *pinfo](std::string text) {
                const auto wire = ChatPacketWithBody(packet, text);
                HandleChat(character, wire.data(), static_cast<std::uint32_t>(wire.size()), true);
            }))
            return iExtraLen;
    }

	char chatbuf[CHAT_MAX_LEN + 1];
	//static const char* colorbuf[] = {"|cFFffa200|H|h[Staff]|h|r", "|cFFff0000|H|h[Shinsoo]|h|r", "|cFFffc700|H|h[Chunjo]|h|r", "|cFF000bff|H|h[Jinno]|h|r"};
#ifdef ENABLE_MULTI_LANGUAGE
	int len;
	std::string langName = ecs::PlayerRuntime::GetLang(character);
	if (pinfo->type == CHAT_TYPE_SHOUT) {

#ifdef ENABLE_EVENT_QUIZ_RAZOR93
        CheckTypedQuizAnswer(character,std::string_view(buf,buflen));
#endif
#ifdef ENABLE_FAKE_SHOP_HEADER
		const char* mountColor = "";
		char mountTitleWithCount[64];
		int count = MountSystem::GetMountCount(character);

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


		const std::string prefix;
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
								 auriga::item::runtime::HasTypedLanguageRing(character)) ? 0 : ecs::PlayerRuntime::GetEmpire(character),
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
					if(!SaveGuildHistory(character,std::string(ecs::PlayerRuntime::GetName(character))+" : "+buf))return iExtraLen;
					ecs::SocialSystem::GetGuild(character)->Chat(chatbuf);
#ifdef ENABLE_CHAT_LOGGING
					if (!SocialHistoryEnabled()&&ecs::PlayerRuntime::IsGM(character))
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

} // namespace

int CInputMain::Whisper(entt::entity character, const char* data, uint64_t size)
{
    return HandleWhisper(character, data, size, false);
}

int CInputMain::Chat(entt::entity character, const char* data, uint32_t size)
{
    return HandleChat(character, data, size, false);
}
