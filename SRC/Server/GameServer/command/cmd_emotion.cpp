#include "stdafx.h"
#include "../ecs/systems/ViewSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include <Core/Logging.hpp>
#include "utils.h"
#include "cmd.h"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "packet.h"
#include "buffer_manager.h"
#include "unique_item.h"
#include "wedding.h"

#define NEED_TARGET	(1 << 0)
#define NEED_PC		(1 << 1)
#define WOMAN_ONLY	(1 << 2)
#define OTHER_SEX_ONLY	(1 << 3)
#define SELF_DISARM	(1 << 4)
#define TARGET_DISARM	(1 << 5)
#define BOTH_DISARM	(SELF_DISARM | TARGET_DISARM)

struct emotion_type_s
{
	const char *	command;
	const char *	command_to_client;
	int32_t	flag;
	float	extra_delay;
} emotion_types[] = {
	{ "Ű��",	"french_kiss",	NEED_PC | BOTH_DISARM,		2.0f },
	{ "�ǻ�",	"kiss",		NEED_PC | BOTH_DISARM,		1.5f },
	{ "����",	"slap",		NEED_PC | SELF_DISARM,				1.5f },
	{ "�ڼ�",	"clap",		0,						1.0f },
	{ "��",		"cheer1",	0,						1.0f },
	{ "����",	"cheer2",	0,						1.0f },

	// DANCE
	{ "���1",	"dance1",	0,						1.0f },
	{ "���2",	"dance2",	0,						1.0f },
	{ "���3",	"dance3",	0,						1.0f },
	{ "���4",	"dance4",	0,						1.0f },
	{ "���5",	"dance5",	0,						1.0f },
	{ "���6",	"dance6",	0,						1.0f },
	// END_OF_DANCE
	{ "����",	"congratulation",	0,				1.0f	},
	{ "�뼭",	"forgive",			0,				1.0f	},
	{ "ȭ��",	"angry",			0,				1.0f	},
	{ "��Ȥ",	"attractive",		0,				1.0f	},
	{ "����",	"sad",				0,				1.0f	},
	{ "���",	"shy",				0,				1.0f	},
	{ "����",	"cheerup",			0,				1.0f	},
	{ "����",	"banter",			0,				1.0f	},
	{ "���",	"joy",				0,				1.0f	},
	{ "����",	"selfie",				0,				1.0f	},
	{ "���",	"dance7",				0,				1.0f	},
	{ "��Ȥ",	"doze",				0,				1.0f	},
	{ "����",	"exercise",				0,				1.0f	},
	{ "����",	"pushup",				0,				1.0f	},
	{ "\n",	"\n",		0,						0.0f },
};


std::set<std::pair<uint32_t, uint32_t> > s_emotion_set;

ACMD(do_emotion_allow)
{
	if (!ecs::PlayerRuntime::IsPC(character)) return;
	if ( ecs::PlayerRuntime::GetArena(character) )
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t	val = 0; str_to_number(val, arg1);
	s_emotion_set.insert(std::make_pair(ecs::PlayerRuntime::GetPacketVID(character), val));
}

#ifdef ENABLE_NEWSTUFF
#include "config.h"
#endif

static bool CanEmotion(entt::entity character)
{
#ifdef ENABLE_NEWSTUFF
	if (g_bDisableEmotionMask)
		return true;
#endif
	if (marriage::WeddingManager::instance().IsWeddingMap(ecs::PlayerRuntime::GetMapIndex(character)))
		return true;

	if (ItemSystem::IsEquipUniqueItem(character, UNIQUE_ITEM_EMOTION_MASK))
		return true;

	if (ItemSystem::IsEquipUniqueItem(character, UNIQUE_ITEM_EMOTION_MASK2))
		return true;

	return false;
}

ACMD(do_emotion)
{
	if (!ecs::PlayerRuntime::IsPC(character)) return;
	int i;
	{
		if (MountSystem::IsRiding(character))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 798, "");
#endif
			return;
		}
	}

	for (i = 0; *emotion_types[i].command != '\n'; ++i)
	{
		if (!strcmp(cmd_info[cmd].command, emotion_types[i].command))
			break;

		if (!strcmp(cmd_info[cmd].command, emotion_types[i].command_to_client))
			break;
	}

	if (*emotion_types[i].command == '\n')
	{
		LOG_ERROR("cannot find emotion");
		return;
	}

	if (!CanEmotion(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 409, "");
#endif
		return;
	}

	if (IS_SET(emotion_types[i].flag, WOMAN_ONLY) && SEX_MALE==ecs::PlayerRuntime::GetSex(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 383, "");
#endif
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	entt::entity victim = entt::null;

	if (*arg1) {
		victim = NetworkSyncSystem::FindCharacterInView(g_registry, character, arg1, IS_SET(emotion_types[i].flag, NEED_PC));
	}

	if (IS_SET(emotion_types[i].flag, NEED_TARGET | NEED_PC))
	{
		if (!ecs::PlayerRuntime::IsPC(victim))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 267, "");
#endif
			return;
		}
	}

	if (victim != entt::null)
	{
		if (!ecs::PlayerRuntime::IsPC(victim) || victim == character)
			return;

		if (MountSystem::IsRiding(victim))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 799, "");
#endif
			return;
		}

		int32_t distance = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(character) - ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(character) - ecs::PlayerRuntime::GetY(victim));

		if (distance < 10)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 288, "");
#endif
			return;
		}

		if (distance > 500)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 289, "");
#endif
			return;
		}

		if (IS_SET(emotion_types[i].flag, OTHER_SEX_ONLY))
		{
			if (ecs::PlayerRuntime::GetSex(character)==ecs::PlayerRuntime::GetSex(victim))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 445, "");
#endif
				return;
			}
		}

		if (IS_SET(emotion_types[i].flag, NEED_PC))
		{
			if (s_emotion_set.find(std::make_pair(ecs::PlayerRuntime::GetPacketVID(victim), ecs::PlayerRuntime::GetPacketVID(character))) == s_emotion_set.end())
			{
				if (true == marriage::CManager::instance().IsMarried( (ecs::PlayerRuntime::GetPlayerID(character)) ))
				{
					const marriage::TMarriage* marriageInfo = marriage::CManager::instance().Get( (ecs::PlayerRuntime::GetPlayerID(character)) );

					const uint32_t other = marriageInfo->GetOther( (ecs::PlayerRuntime::GetPlayerID(character)) );

					if (0 == other || other != (ecs::PlayerRuntime::GetPlayerID(victim)))
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 432, "%s", ecs::PlayerRuntime::GetName(victim).data());
#endif
						return;
					}
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 432, "%s", ecs::PlayerRuntime::GetName(victim).data());
#endif
					return;
				}
			}

			s_emotion_set.insert(std::make_pair(ecs::PlayerRuntime::GetPacketVID(character), ecs::PlayerRuntime::GetPacketVID(victim)));
		}
	}

	char chatbuf[256+1];
	int len = snprintf(chatbuf, sizeof(chatbuf), "%s %u %u",
			emotion_types[i].command_to_client,
			(uint32_t) ecs::PlayerRuntime::GetPacketVID(character), victim != entt::null ? (uint32_t) ecs::PlayerRuntime::GetPacketVID(victim) : 0);

	if (len < 0 || len >= (int) sizeof(chatbuf))
		len = sizeof(chatbuf) - 1;

	++len;

	TPacketGCChat pack_chat;
	pack_chat.header = HEADER_GC_CHAT;
	pack_chat.size = sizeof(TPacketGCChat) + len;
	pack_chat.type = CHAT_TYPE_COMMAND;
	pack_chat.id = 0;
	TEMP_BUFFER buf;
	buf.write(&pack_chat, sizeof(TPacketGCChat));
	buf.write(chatbuf, len);

	ecs::ViewSystem::PacketView(character, buf.read_peek(), buf.size());

	if (ecs::PlayerRuntime::IsPC(victim))
		LOG_INFO("ACTION: {} TO {}", emotion_types[i].command, ecs::PlayerRuntime::GetName(victim).data());
	else
		LOG_INFO("ACTION: {}", emotion_types[i].command);
}



