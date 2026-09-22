#include "stdafx.h"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/ViewSystem.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include <Core/Logging.hpp>
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/Registry.hpp"
#include "constants.h"
#include "config.h"
#include "questmanager.h"
#include "start_position.h"
#include "packet.h"
#include "buffer_manager.h"
#include "log.h"
#include "char_interface.hpp"
#include "OXEvent.h"
#include "desc.h"
#include <charconv>
#include <limits>
#include <utility>

namespace {
bool IsOXPlayer(entt::entity e)
{
    return ecs::PlayerRuntime::IsPC(e) && ecs::PlayerRuntime::GetMapIndex(e) == OXEVENT_MAP_INDEX;
}
struct ScopedOXFlag {
    bool& flag;
    explicit ScopedOXFlag(bool& value) : flag(value) { flag = true; }
    ~ScopedOXFlag() { flag = false; }
};
}

COXEventManager::~COXEventManager()
{
    event_cancel(&m_timedEvent);
}

bool COXEventManager::Initialize()
{
	event_cancel(&m_timedEvent);
    ++m_roundRevision;
    m_missed.clear();
	m_participants.clear();
	m_attenders.clear();
	m_vec_quiz.clear();

	SetStatus(OXEVENT_FINISH);

	return true;
}

void COXEventManager::Destroy()
{
	CloseEvent();

	m_participants.clear();
	m_attenders.clear();
	m_vec_quiz.clear();

	SetStatus(OXEVENT_FINISH);
}

OXEventStatus COXEventManager::GetStatus()
{
	uint8_t ret = quest::CQuestManager::instance().GetEventFlag("oxevent_status");

	switch (ret)
	{
		case 0 :
			return OXEVENT_FINISH;

		case 1 :
			return OXEVENT_OPEN;

		case 2 :
			return OXEVENT_CLOSE;

		case 3 :
			return OXEVENT_QUIZ;

		default :
			return OXEVENT_ERR;
	}

	return OXEVENT_ERR;
}

void COXEventManager::SetStatus(OXEventStatus status)
{
	uint8_t val = 0;

	switch (status)
	{
		case OXEVENT_OPEN :
			val = 1;
			break;

		case OXEVENT_CLOSE :
			val = 2;
			break;

		case OXEVENT_QUIZ :
			val = 3;
			break;

		case OXEVENT_FINISH :
		case OXEVENT_ERR :
		default :
			val = 0;
			break;
	}
	quest::CQuestManager::instance().RequestSetEventFlag("oxevent_status", val);
}

bool COXEventManager::Enter(entt::entity pkChar)
{
    if (m_closing || !IsOXPlayer(pkChar)) return false;
	if (GetStatus() == OXEVENT_FINISH)
	{
		LOG_INFO("OXEVENT : map finished. but char enter. {}", ecs::PlayerRuntime::GetName(pkChar).data());
		return false;
	}

	PIXEL_POSITION pos;
	pos.x = ecs::PlayerRuntime::GetX(pkChar);
	pos.y = ecs::PlayerRuntime::GetY(pkChar);
	pos.z = ecs::PlayerRuntime::GetZ(pkChar);

	if (pos.x == 896500 && pos.y == 24600)
	{
		return EnterAttender(pkChar);
	}
	else if (pos.x == 896300 && pos.y == 28900)
	{
		return EnterAudience(pkChar);
	}
	else
	{
		LOG_INFO("OXEVENT : wrong pos enter {} {}", pos.x, pos.y);
		return false;
	}

	return false;
}

bool COXEventManager::EnterAttender(entt::entity character)
{
    if (m_closing || !IsOXPlayer(character)) return false;
    m_participants.insert(character);
    m_attenders.insert(character);
    m_missed.erase(character);
    return true;
}

bool COXEventManager::EnterAudience(entt::entity character)
{
    if (m_closing || !IsOXPlayer(character)) return false;
    m_participants.insert(character);
    return true;
}

bool COXEventManager::AddQuiz(unsigned char level, const char* pszQuestion, bool answer)
{
	if (m_vec_quiz.size() < (size_t) level + 1)
		m_vec_quiz.resize(level + 1);

	struct tag_Quiz tmpQuiz;

	tmpQuiz.level = level;
	strlcpy(tmpQuiz.Quiz, pszQuestion, sizeof(tmpQuiz.Quiz));
	tmpQuiz.answer = answer;

	m_vec_quiz[level].push_back(tmpQuiz);
	return true;
}

bool COXEventManager::ShowQuizList(entt::entity character)
{
    if (!ecs::PlayerRuntime::IsPC(character)) return false;
#ifdef TEXTS_IMPROVEMENT
    const auto quizzes = m_vec_quiz;
    int count = 0;
    for (const auto& level : quizzes) for (const auto& quiz : level) {
        if (!ecs::PlayerRuntime::IsPC(character)) return false;
        ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, quiz.answer ? 608 : 609, "%s", quiz.Quiz);
        ++count;
    }
    if (!ecs::PlayerRuntime::IsPC(character)) return false;
    ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 610, "%d", count);
#endif
    return true;
}

void COXEventManager::ClearQuiz()
{
	for (unsigned int i = 0; i < m_vec_quiz.size(); ++i)
	{
		m_vec_quiz[i].clear();
	}

	m_vec_quiz.clear();
}

EVENTINFO(OXEventInfoData)
{
	bool answer;
    uint8_t stage { 0 };

	OXEventInfoData()
	: answer( false )
	{
	}
};

EVENTFUNC(oxevent_timer)
{
	OXEventInfoData* info = dynamic_cast<OXEventInfoData*>(event->info);

	if ( info == nullptr)
	{
		LOG_ERROR("oxevent_timer> <Factor> Null pointer");
		return 0;
	}

	switch (info->stage)
	{
		case 0:
#ifdef TEXTS_IMPROVEMENT
			SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, OXEVENT_MAP_INDEX, 579, "");
#endif
			++info->stage;
			return PASSES_PER_SEC(10);

		case 1:
			if (info->answer == true)
			{
				COXEventManager::instance().CheckAnswer(true);
                if (event->is_force_to_end) return 0;
#ifdef TEXTS_IMPROVEMENT
				SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, OXEVENT_MAP_INDEX, 580, "");
#endif
			}
			else
			{
				COXEventManager::instance().CheckAnswer(false);
                if (event->is_force_to_end) return 0;
#ifdef TEXTS_IMPROVEMENT
				SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, OXEVENT_MAP_INDEX, 581, "");
#endif
			}

if (event->is_force_to_end) return 0;
#ifdef TEXTS_IMPROVEMENT
			SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, OXEVENT_MAP_INDEX, 582, "");
#endif
			++info->stage;
			return PASSES_PER_SEC(5);
		case 2:
			COXEventManager::instance().WarpToAudience();
			if (event->is_force_to_end) return 0;
            COXEventManager::instance().SetStatus(OXEVENT_CLOSE);
            if (event->is_force_to_end) return 0;
#ifdef TEXTS_IMPROVEMENT
			SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, OXEVENT_MAP_INDEX, 583, "");
#endif
			info->stage = 0;
			break;
	}
	return 0;
}

bool COXEventManager::Quiz(unsigned char level, int timelimit)
{
    if (m_closing || m_vec_quiz.empty()) return false;
    if (level >= m_vec_quiz.size()) level = static_cast<unsigned char>(m_vec_quiz.size() - 1);
    if (m_vec_quiz[level].empty()) return false;
    const int index = number(0, static_cast<int>(m_vec_quiz[level].size() - 1));
    const auto quiz = m_vec_quiz[level][index];
#ifdef TEXTS_IMPROVEMENT
    uint32_t textID = 0;
    const auto* end = quiz.Quiz + std::strlen(quiz.Quiz);
    const auto parsed = std::from_chars(quiz.Quiz, end, textID);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        LOG_ERROR("OXEVENT: invalid quiz text ID {}", quiz.Quiz);
        return false;
    }
#endif
    if (timelimit < 0) timelimit = 30;
    const int seconds = std::clamp(timelimit, 15, std::numeric_limits<int>::max() / std::max(1, passes_per_sec)) - 15;
    event_cancel(&m_timedEvent);
    ++m_roundRevision;
    auto* info = AllocEventInfo<OXEventInfoData>();
    info->answer = quiz.answer;
    m_timedEvent = event_create(oxevent_timer, info, PASSES_PER_SEC(seconds));
    const LPEVENT current = m_timedEvent;
    m_vec_quiz[level].erase(m_vec_quiz[level].begin() + index);
    SetStatus(OXEVENT_QUIZ);
#ifdef TEXTS_IMPROVEMENT
    if (current != m_timedEvent || current->is_force_to_end) return true;
    SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, OXEVENT_MAP_INDEX, 584, "");
    if (current != m_timedEvent || current->is_force_to_end) return true;
    SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, OXEVENT_MAP_INDEX, textID, "");
    if (current != m_timedEvent || current->is_force_to_end) return true;
    SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, OXEVENT_MAP_INDEX, 585, "");
#endif
    return true;
}

bool COXEventManager::CheckAnswer(bool answer)
{
    if (m_checkingAnswer || m_closing || m_attenders.empty()) return true;
    ScopedOXFlag checking(m_checkingAnswer);
    const auto revision = m_roundRevision;
    const auto attenders = m_attenders;
    m_missed.clear();
    const int left = answer ? 896600 : 892600;
    const int right = answer ? 900300 : 896300;
    for (const auto character : attenders) {
        if (revision != m_roundRevision) break;
        if (!m_attenders.contains(character)) continue;
        if (!IsOXPlayer(character)) {
            m_attenders.erase(character);
            m_participants.erase(character);
            m_missed.erase(character);
            continue;
        }
        const auto x = ecs::PlayerRuntime::GetX(character), y = ecs::PlayerRuntime::GetY(character);
        if (x < left || x > right || y < 22900 || y > 26400) {
            // Commit elimination before publishing a callback-capable effect.
            m_attenders.erase(character);
            m_missed.insert(character);
            NetworkSyncSystem::BroadcastEffect(g_registry, character, SE_FAIL);
            continue;
        }

        char chat[256];
        int length = snprintf(chat, sizeof(chat), "%s %u %u",
            number(0, 1) == 1 ? "cheer1" : "cheer2", ecs::PlayerRuntime::GetPacketVID(character), 0u);
        if (length < 0 || length >= static_cast<int>(sizeof(chat))) length = sizeof(chat) - 1;
        ++length;
        TPacketGCChat packet {};
        packet.header = HEADER_GC_CHAT;
        packet.size = sizeof(packet) + length;
        packet.type = CHAT_TYPE_COMMAND;
        TEMP_BUFFER buffer;
        buffer.write(&packet, sizeof(packet));
        buffer.write(chat, length);
        ecs::ViewSystem::PacketView(character, buffer.read_peek(), buffer.size());
        if (revision == m_roundRevision && m_attenders.contains(character) && IsOXPlayer(character))
            NetworkSyncSystem::BroadcastEffect(g_registry, character, SE_SUCCESS);
    }
    return true;
}

void COXEventManager::WarpToAudience()
{
    const auto revision = m_roundRevision;
    const auto missed = std::exchange(m_missed, {});
    constexpr int32_t positions[4][2] = {{896300,28900},{890900,28100},{896600,20500},{902500,28100}};
    for (const auto character : missed) {
        if (revision != m_roundRevision) break;
        if (!m_participants.contains(character) || !IsOXPlayer(character)) continue;
        const auto& position = positions[number(0, 3)];
        ecs::MovementSystem::Show(character, OXEVENT_MAP_INDEX, position[0], position[1]);
    }
}

bool COXEventManager::CloseEvent()
{
    if (m_closing) return true;
    ScopedOXFlag closing(m_closing);
    event_cancel(&m_timedEvent);
    ++m_roundRevision;
    const auto participants = std::exchange(m_participants, {});
    m_attenders.clear();
    m_missed.clear();
    for (const auto character : participants) {
        if (!IsOXPlayer(character)) continue;
        const auto empire = ecs::PlayerRuntime::GetEmpire(character);
        ecs::MovementSystem::WarpSet(character, EMPIRE_START_X(empire), EMPIRE_START_Y(empire));
    }
    return true;
}

bool COXEventManager::LogWinner()
{
    const auto revision = m_roundRevision;
    const auto attenders = m_attenders;
    for (const auto character : attenders) {
        if (revision != m_roundRevision) break;
        if (m_attenders.contains(character) && IsOXPlayer(character))
            LogManager::instance().CharLog(character, 0, "OXEVENT", "LastManStanding");
    }
    return true;
}

bool COXEventManager::GiveItemToAttender(uint32_t itemVnum,
#ifdef ENABLE_NEW_STACK_LIMIT
    int
#else
    uint8_t
#endif
    count)
{
    if (!itemVnum || count <= 0 || m_rewarding || m_closing) return false;
    ScopedOXFlag rewarding(m_rewarding);
    const auto revision = m_roundRevision;
    const auto attenders = m_attenders;
    for (const auto character : attenders) {
        if (revision != m_roundRevision) break;
        if (!m_attenders.contains(character) || !IsOXPlayer(character)) continue;
#ifdef ENABLE_BLOCK_MULTIFARM
        if (!AffectSystem::FindAffect(character, AFFECT_DROP_UNBLOCK, APPLY_NONE)) continue;
#endif
        // Audit data is a value snapshot; item delivery may retire the recipient.
        const auto pid = ecs::PlayerRuntime::GetPlayerID(character);
        auto* desc = ecs::PlayerRuntime::GetDesc(character);
        const std::string host = desc ? desc->GetHostName() : "";
        ItemSystem::AutoGiveItemEcs(character, itemVnum, static_cast<uint32_t>(count));
        LogManager::instance().ItemLog(pid, 0, count, itemVnum, "OXEVENT_REWARD", "", host.c_str(), itemVnum);
    }
    return true;
}

uint32_t COXEventManager::GetAttenderCount()
{
    std::erase_if(m_attenders, [](entt::entity character) { return !IsOXPlayer(character); });
    return static_cast<uint32_t>(m_attenders.size());
}
