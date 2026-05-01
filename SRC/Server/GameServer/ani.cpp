/*********************************************************************
 * date        : 2007.11.16
 * file        : ani.cpp
 * author      : mhh
 * description :
 */

#define _ani_cpp_

#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "item.h"
#include "ani.h"
#include "dev_log.h"
#include <Core/Logging.hpp>
#include <common/CommonDefines.h>
#include "Core/Logging.hpp"

const char* FN_race_name(int race)
{
#define FN_NAME(race)	case race: return #race
	switch (race)
	{
		FN_NAME(MAIN_RACE_WARRIOR_M);
		FN_NAME(MAIN_RACE_ASSASSIN_W);
		FN_NAME(MAIN_RACE_SURA_M);
		FN_NAME(MAIN_RACE_SHAMAN_W);
		FN_NAME(MAIN_RACE_WARRIOR_W);
		FN_NAME(MAIN_RACE_ASSASSIN_M);
		FN_NAME(MAIN_RACE_SURA_W);
		FN_NAME(MAIN_RACE_SHAMAN_M);
#ifdef ENABLE_WOLFMAN_CHARACTER
		FN_NAME(MAIN_RACE_WOLFMAN_M);
#endif
		FN_NAME(MAIN_RACE_MAX_NUM);
	}

	return "UNKNOWN";
#undef FN_NAME
}

const char* FN_weapon_type(int weapon)
{
#define FN_NAME(weapon)	case weapon: return #weapon
	switch (weapon)
	{
		FN_NAME(WEAPON_SWORD);
		FN_NAME(WEAPON_DAGGER);
		FN_NAME(WEAPON_BOW);
		FN_NAME(WEAPON_TWO_HANDED);
		FN_NAME(WEAPON_BELL);
		FN_NAME(WEAPON_FAN);
		FN_NAME(WEAPON_ARROW);
		FN_NAME(WEAPON_MOUNT_SPEAR);
#ifdef ENABLE_WOLFMAN_CHARACTER
		FN_NAME(WEAPON_CLAW);
#endif
		FN_NAME(WEAPON_NUM_TYPES);
	}

	return "UNKNOWN";
#undef FN_NAME
}

class ANI
{
	protected:
		// [����][�Ϲ�0Ż��1][����][�޺�]
		uint32_t m_speed[MAIN_RACE_MAX_NUM][2][WEAPON_NUM_TYPES][9];

	public:
		ANI();

	public:
		bool	load();
		bool	load_one_race(int race, const char *dir_name);
		uint32_t	load_one_weapon(const char *dir_name, int weapon, uint8_t combo, bool horse);
		uint32_t	attack_speed(int race, int weapon, uint8_t combo = 0, bool horse = false);

		void	print_attack_speed();
};

static class ANI s_ANI;

uint32_t FN_attack_speed_from_file(const char *file)
{
	FILE * fp = fopen(file, "r");

	if (nullptr == fp)
		return 0;

	int speed = 1000;

	char buf[1024];

	while (fgets(buf, 1024, fp))
	{
		const auto delim	= " \t\r\n";
		const char* field = strtok(buf, delim);
		const char* value = strtok(nullptr, delim);

		if (field && value)
		{
			if (const auto key	= "DirectInputTime"; 0 == strcasecmp(field, key))
			{
				float f_speed = strtof(value, nullptr);
				speed = (int) (f_speed * 1000.0);
				break;
			}
		}
	}

	fclose(fp);
	return speed;
}

ANI::ANI()
{
	// set default value
	for (auto& race : m_speed)
	{
		for (int weapon = 0; weapon < WEAPON_NUM_TYPES; ++weapon)
		{
			for (uint8_t combo = 0; combo <= 8; ++combo)
			{
				race[0][weapon][combo] = 1000;
				race[1][weapon][combo] = 1000;
			}
		}
	}
}

bool ANI::load()
{
	const char*	dir_name[MAIN_RACE_MAX_NUM] = {
		"data/pc/warrior",		// ����(��)
		"data/pc/assassin",		// �ڰ�(��)
		"data/pc/sura",			// ����(��)
		"data/pc/shaman",		// ����(��)
		"data/pc2/warrior",		// ����(��)
		"data/pc2/assassin",	// �ڰ�(��)
		"data/pc2/sura",		// ����(��)
		"data/pc2/shaman",		// ����(��)
#ifdef ENABLE_WOLFMAN_CHARACTER
		"data/pc3/wolfman",		// ������
#endif
	};

	for (int race = 0; race <MAIN_RACE_MAX_NUM; ++race)
	{
		if (false == load_one_race(race, dir_name[race]))
		{
			LOG_ERROR("ANI directory = {}", dir_name[race]);
			return false;
		}
	}

	return true;
}

uint32_t ANI::load_one_weapon(const char *dir_name, int weapon, uint8_t combo, bool horse)
{
	char format[128];
	char filename[256];

	switch (weapon)
	{
		case WEAPON_SWORD:
			strlcpy(format, "%s/%sonehand_sword/combo_%02d.msa", sizeof(format));
			break;

		case WEAPON_DAGGER:
			strlcpy(format, "%s/%sdualhand_sword/combo_%02d.msa", sizeof(format));
			break;

		case WEAPON_BOW:
			strlcpy(format, "%s/%sbow/attack.msa", sizeof(format));
			break;

		case WEAPON_TWO_HANDED:
			strlcpy(format, "%s/%stwohand_sword/combo_%02d.msa", sizeof(format));
			break;

		case WEAPON_BELL:
			strlcpy(format, "%s/%sbell/combo_%02d.msa", sizeof(format));
			break;

		case WEAPON_FAN:
			strlcpy(format, "%s/%sfan/combo_%02d.msa", sizeof(format));
			break;

#ifdef ENABLE_WOLFMAN_CHARACTER
		case WEAPON_CLAW:
			strlcpy(format, "%s/%sclaw/combo_%02d.msa", sizeof(format));
			break;
#endif

		default:
			return 1000;
	}

	snprintf(filename, sizeof(filename), format, dir_name, horse ? "horse_" : "", combo);
	uint32_t speed = FN_attack_speed_from_file(filename);

	if (speed == 0)
		return 1000;

	return speed;
}

bool ANI::load_one_race(int race, const char *dir_name)
{
	if (nullptr == dir_name || '\0' == dir_name[0])
		return false;

	for (int weapon = WEAPON_SWORD; weapon < WEAPON_NUM_TYPES; ++weapon)
	{
		dev_log(LOG_DEB0, "ANI (%s,%s)", FN_race_name(race), FN_weapon_type(weapon));

		for (uint8_t combo = 1; combo <= 8; ++combo)
		{
			// �� ������ ��
			m_speed[race][0][weapon][combo] = load_one_weapon(dir_name, weapon, combo, false);
			m_speed[race][0][weapon][0] = MIN(m_speed[race][0][weapon][0], m_speed[race][0][weapon][combo]); // �ּҰ�

			// �� ���� ��
			m_speed[race][1][weapon][combo] = load_one_weapon(dir_name, weapon, combo, true);
			m_speed[race][1][weapon][0] = MIN(m_speed[race][1][weapon][0], m_speed[race][1][weapon][combo]); // �ּҰ�

			dev_log(LOG_DEB0, "combo%02d speed=%d horse=%d",
					combo, m_speed[race][0][weapon][combo], m_speed[race][1][weapon][combo]);
		}

		dev_log(LOG_DEB0, "minspeed=%u", m_speed[race][0][weapon][0]);
	}

	return true;
}

uint32_t ANI::attack_speed(int race, int weapon, uint8_t combo, bool horse)
{
	switch (race)
	{
		case MAIN_RACE_WARRIOR_M:
		case MAIN_RACE_ASSASSIN_W:
		case MAIN_RACE_SURA_M:
		case MAIN_RACE_SHAMAN_W:
		case MAIN_RACE_WARRIOR_W:
		case MAIN_RACE_ASSASSIN_M:
		case MAIN_RACE_SURA_W:
		case MAIN_RACE_SHAMAN_M:
#ifdef ENABLE_WOLFMAN_CHARACTER
		case MAIN_RACE_WOLFMAN_M:
#endif
			break;
		default:
			return 1000;
	}

	switch (weapon)
	{
		case WEAPON_SWORD:
		case WEAPON_DAGGER:
		case WEAPON_BOW:
		case WEAPON_TWO_HANDED:
		case WEAPON_BELL:
		case WEAPON_FAN:
		case WEAPON_ARROW:
		case WEAPON_MOUNT_SPEAR:
#ifdef ENABLE_WOLFMAN_CHARACTER
		case WEAPON_CLAW:
#endif
			break;
		default:
			return 1000;
	}

	return m_speed[race][horse ? 1 : 0][weapon][combo];
}

const char* FN_race_string(int race)
{
	switch (race)
	{
		case MAIN_RACE_WARRIOR_M:	return "WARRIOR_M";
		case MAIN_RACE_ASSASSIN_W:	return "ASSASSIN_W";
		case MAIN_RACE_SURA_M:		return "SURA_M";
		case MAIN_RACE_SHAMAN_W:	return "SHAMAN_W";

		case MAIN_RACE_WARRIOR_W:	return "WARRIOR_W";
		case MAIN_RACE_ASSASSIN_M:	return "ASSASSIN_M";
		case MAIN_RACE_SURA_W:		return "SURA_W";
		case MAIN_RACE_SHAMAN_M:	return "SHAMAN_M";
#ifdef ENABLE_WOLFMAN_CHARACTER
		case MAIN_RACE_WOLFMAN_M:	return "WOLFMAN_M";
#endif
	}

	return "UNKNOWN_RACE";
}

const char* FN_weapon_string(int weapon)
{
	switch (weapon)
	{
		case WEAPON_SWORD:		return "SWORD";
		case WEAPON_DAGGER:		return "DAGGER";
		case WEAPON_BOW:		return "BOW";
		case WEAPON_TWO_HANDED:	return "TWO_HANDED";
		case WEAPON_BELL:		return "BELL";
		case WEAPON_FAN:		return "FAN";
		case WEAPON_ARROW:		return "ARROW";
		case WEAPON_MOUNT_SPEAR:return "WEAPON_MOUNT_SPEAR";
#ifdef ENABLE_WOLFMAN_CHARACTER
		case WEAPON_CLAW:		return "CLAW";
#endif
	}

	return "UNKNOWN";
}

void ANI::print_attack_speed()
{
	for (int race = 0; race < MAIN_RACE_MAX_NUM; ++race)
	{
		for (int weapon = 0; weapon < WEAPON_NUM_TYPES; ++weapon)
		{
			LOG_INFO("[{}][{}] = {}", FN_race_string(race), FN_weapon_string(weapon), attack_speed(race, weapon));
		}
		LOG_INFO("");
	}
}

void ani_init()
{
	s_ANI.load();
}

uint32_t ani_attack_speed(LPCHARACTER ch)
{
	uint32_t speed = 1000;

	if (nullptr == ch)
		return speed;

	LPITEM item = ch->GetWear(WEAR_WEAPON);

	if (nullptr == item)
		return speed;

	if (ITEM_WEAPON != ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, item)))
		return speed;

	int race = (ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(ch)));
	int weapon = ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, item));

	/*
	dev_log(LOG_DEB0, "%s : (race,weapon) = (%s,%s) POINT_ATT_SPEED = %d",
			((ch)->GetName()),
			FN_race_name(race),
			FN_weapon_type(weapon),
			ch->GetPoint(POINT_ATT_SPEED));
	*/

	/* ���ڵ�� �ҵ��� ��� �￬�����ݰ� �¸��� */
	/* ������ ���� �Ѽհ� �ӵ��� ��������       */
	if (weapon == WEAPON_TWO_HANDED)
		weapon = WEAPON_SWORD;

	return s_ANI.attack_speed(race, weapon);
}

uint32_t ani_combo_speed(LPCHARACTER ch, uint8_t combo)
{
	LPITEM item = ch->GetWear(WEAR_WEAPON);

	if (nullptr == item || combo > 8)
		return 1000;

	return s_ANI.attack_speed((ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(ch))), ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, item)), combo, ch->IsRiding());
}

void ani_print_attack_speed()
{
	s_ANI.print_attack_speed();
}

#if 0
int main(int argc, char **argv)
{
	ani_init();
	ani_print_attack_speed();
	exit(0);
}
#endif


