#include "stdafx.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include "utils.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "motion.h"
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
	/*
	//{ "Ű��",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		MOTION_ACTION_FRENCH_KISS,	 1.0f },
	{ "�ǻ�",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		MOTION_ACTION_KISS,		 1.0f },
	{ "���ȱ�",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		MOTION_ACTION_SHORT_HUG,	 1.0f },
	{ "����",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		MOTION_ACTION_LONG_HUG,		 1.0f },
	{ "�������",	NEED_PC | SELF_DISARM,				MOTION_ACTION_PUT_ARMS_SHOULDER, 0.0f },
	{ "��¯",		NEED_PC	| WOMAN_ONLY | SELF_DISARM,		MOTION_ACTION_FOLD_ARM,		 0.0f },
	{ "����",		NEED_PC | SELF_DISARM,				MOTION_ACTION_SLAP,		 1.5f },

	{ "���Ķ�",		0,						MOTION_ACTION_CHEER_01,		 0.0f },
	{ "����",		0,						MOTION_ACTION_CHEER_02,		 0.0f },
	{ "�ڼ�",		0,						MOTION_ACTION_CHEER_03,		 0.0f },

	{ "ȣȣ",		0,						MOTION_ACTION_LAUGH_01,		 0.0f },
	{ "űű",		0,						MOTION_ACTION_LAUGH_02,		 0.0f },
	{ "������",		0,						MOTION_ACTION_LAUGH_03,		 0.0f },

	{ "����",		0,						MOTION_ACTION_CRY_01,		 0.0f },
	{ "����",		0,						MOTION_ACTION_CRY_02,		 0.0f },

	{ "�λ�",		0,						MOTION_ACTION_GREETING_01,	0.0f },
	{ "����",		0,						MOTION_ACTION_GREETING_02,	0.0f },
	{ "�����λ�",	0,						MOTION_ACTION_GREETING_03,	0.0f },

	{ "��",		0,						MOTION_ACTION_INSULT_01,	0.0f },
	{ "���",		SELF_DISARM,					MOTION_ACTION_INSULT_02,	0.0f },
	{ "����",		0,						MOTION_ACTION_INSULT_03,	0.0f },

	{ "�����",		0,						MOTION_ACTION_ETC_01,		0.0f },
	{ "������",	0,						MOTION_ACTION_ETC_02,		0.0f },
	{ "��������",	0,						MOTION_ACTION_ETC_03,		0.0f },
	{ "��������",	0,						MOTION_ACTION_ETC_04,		0.0f },
	{ "ơ",		0,						MOTION_ACTION_ETC_05,		0.0f },
	{ "��",		0,						MOTION_ACTION_ETC_06,		0.0f },
	 */
};


std::set<std::pair<uint32_t, uint32_t> > s_emotion_set;

ACMD(do_emotion_allow)
{
	if ( ch->GetArena() )
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t	val = 0; str_to_number(val, arg1);
	s_emotion_set.insert(std::make_pair(((ch)->GetLegacyVID()), val));
}

#ifdef ENABLE_NEWSTUFF
#include "config.h"
#endif

bool CHARACTER_CanEmotion(CHARACTER& rch)
{
#ifdef ENABLE_NEWSTUFF
	if (g_bDisableEmotionMask)
		return true;
#endif
	// ��ȥ�� �ʿ����� ����� �� �ִ�.
	if (marriage::WeddingManager::instance().IsWeddingMap(rch.GetMapIndex()))
		return true;

	// ������ ���� ����� ����� �� �ִ�.
	if (rch.IsEquipUniqueItem(UNIQUE_ITEM_EMOTION_MASK))
		return true;

	if (rch.IsEquipUniqueItem(UNIQUE_ITEM_EMOTION_MASK2))
		return true;

	return false;
}

ACMD(do_emotion)
{
	int i;
	{
		if (ch->IsRiding())
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 798, "");
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

	if (!CHARACTER_CanEmotion(*ch))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 409, "");
#endif
		return;
	}

	if (IS_SET(emotion_types[i].flag, WOMAN_ONLY) && SEX_MALE==GET_SEX(ch))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 383, "");
#endif
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	LPCHARACTER victim = nullptr;

	if (*arg1)
		victim = ch->FindCharacterInView(arg1, IS_SET(emotion_types[i].flag, NEED_PC));

	if (IS_SET(emotion_types[i].flag, NEED_TARGET | NEED_PC))
	{
		if (!victim)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 267, "");
#endif
			return;
		}
	}

	if (victim)
	{
		if (!(ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(victim))) || victim == ch)
			return;

		if (victim->IsRiding())
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 799, "");
#endif
			return;
		}

		int32_t distance = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(ch)) - ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(victim)), ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(ch)) - ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(victim)));

		if (distance < 10)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 288, "");
#endif
			return;
		}

		if (distance > 500)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 289, "");
#endif
			return;
		}

		if (IS_SET(emotion_types[i].flag, OTHER_SEX_ONLY))
		{
			if (GET_SEX(ch)==GET_SEX(victim))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 445, "");
#endif
				return;
			}
		}

		if (IS_SET(emotion_types[i].flag, NEED_PC))
		{
			if (s_emotion_set.find(std::make_pair(((victim)->GetLegacyVID()), ((ch)->GetLegacyVID()))) == s_emotion_set.end())
			{
				if (true == marriage::CManager::instance().IsMarried( (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))) ))
				{
					const marriage::TMarriage* marriageInfo = marriage::CManager::instance().Get( (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))) );

					const uint32_t other = marriageInfo->GetOther( (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))) );

					if (0 == other || other != (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(victim))))
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 432, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(victim)).data());
#endif
						return;
					}
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 432, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(victim)).data());
#endif
					return;
				}
			}

			s_emotion_set.insert(std::make_pair(((ch)->GetLegacyVID()), ((victim)->GetLegacyVID())));
		}
	}

	char chatbuf[256+1];
	int len = snprintf(chatbuf, sizeof(chatbuf), "%s %u %u",
			emotion_types[i].command_to_client,
			(uint32_t) ((ch)->GetLegacyVID()), victim ? (uint32_t) ((victim)->GetLegacyVID()) : 0);

	if (len < 0 || len >= (int) sizeof(chatbuf))
		len = sizeof(chatbuf) - 1;

	++len;  // \0 ���� ����

	TPacketGCChat pack_chat;
	pack_chat.header = HEADER_GC_CHAT;
	pack_chat.size = sizeof(TPacketGCChat) + len;
	pack_chat.type = CHAT_TYPE_COMMAND;
	pack_chat.id = 0;
	TEMP_BUFFER buf;
	buf.write(&pack_chat, sizeof(TPacketGCChat));
	buf.write(chatbuf, len);

	ch->PacketAround(buf.read_peek(), buf.size());

	if (victim)
		LOG_INFO("ACTION: {} TO {}", emotion_types[i].command, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(victim)).data());
	else
		LOG_INFO("ACTION: {}", emotion_types[i].command);
}



