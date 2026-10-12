#include "stdafx.h"
#include <string_view>
#include <Core/Logging.hpp>
#include "locale_service.h"
#include "constants.h"
#include "banword.h"
#include "utils.h"
#include "mob_manager.h"
#include "empire_text_convert.h"
#include "config.h"
#include "skill_power.h"
#include <common/CommonDefines.h>
#include "Core/Logging.hpp"

using namespace std;

extern string		g_stQuestDir;
extern set<string> 	g_setQuestObjectDir;

string g_stServiceBasePath = ".";
string g_stServiceMapPath = "data/map";

string g_stLocale = "euckr";
string g_stLocaleFilename;

uint8_t PK_PROTECT_LEVEL = 30;

string 			g_stLocal = "";

int (*check_name) (const char * str) = nullptr;
int (*is_twobyte) (const char * str) = nullptr;

int is_twobyte_euckr(const char * str)
{
	return ishan(*str);
}

int is_twobyte_gb2312(const char * str)
{
	if (!str || !*str)
		return 0;

	uint8_t b1 = str[0];
	uint8_t b2 = str[1];

	if (!(b1 & 0x80))
		return 0;

	if (b1 < 0xb0 || b1 > 0xf7 || b2 < 0xa1 || b2 > 0xfe)
		return 0;

	return 1;
}

int is_twobyte_big5(const char * str)
{
	if (!str || !*str)
		return 0;

	uint8_t b1 = str[0];
	uint8_t b2 = str[1];

	uint8_t b[2];

	b[0] = b2;
	b[1] = b1;

	uint16_t b12 = 0;
	memcpy(&b12, b, 2);

	if (!(b1 & 0x80))
		return 0;

	if ((b12 < 0xa440 || b12 > 0xc67e) && (b12 < 0xc940 || b12 > 0xf9d5))
	{
		if (test_server)
		{
			LOG_INFO("twobyte_big5 {:x} {:x}", b1, b2);
		}
		return 0;
	}

	return 1;
}

int check_name_independent(const char * str)
{
	if (CBanwordManager::instance().CheckString(str, strlen(str)))
		return 0;

	char szTmp[256];
	str_lower(str, szTmp, sizeof(szTmp));

	if (CMobManager::instance().Get(szTmp, false))
		return 0;

	return 1;
}

int check_name_gb2312(const char * str)
{
	static const uint8_t exceptions[5][2] =
	{
		{ 0xd7, 0xfa },
		{ 0xd7, 0xfb },
		{ 0xd7, 0xfc },
		{ 0xd7, 0xfd },
		{ 0xd7, 0xfe }
	};

	int         i, j;
	uint8_t        b1, b2;

	if (!str || !*str)
		return 0;

	i = 0;

	size_t len = 0;

	while (str[i])
	{
		if (str[i] & 0x80)
		{
			if (!str[i + 1])
				return 0;

			b1 = str[i++];
			b2 = str[i++];

			if (b1 < 0xb0 || b1 > 0xf7 || b2 < 0xa1 || b2 > 0xfe)
				return 0;

			for (j = 0; j < 5; j++)
				if (b1 == exceptions[j][0] && b2 == exceptions[j][1])
					return 0;

			len++;
		}
		else
		{
			if (!isdigit(str[i]) && !isalpha(str[i]))
				return 0;

			i++;
			len++;
		}
	}

	if ( len > 6 ) return 0;

	return check_name_independent(str);
}

int check_name_big5(const char * str )
{
	int i;
	uint8_t b1, b2;

	if (!str || !*str)
		return 0;

	i = 0;

	while (str[i])
	{
		if (str[i] & 0x80)
		{
			if (!str[i + 1])
				return 0;

			b1 = str[i++];
			b2 = str[i++];

			uint8_t b[2];

			b[0] = b2;
			b[1] = b1;

			//  big5: 0xA140--0xF9D5
			//  extended big5: 0x40--0x7E and 0xA1--0xFE

			// 0xa440-0xC67E
			// 0xC940-0xF9D5
			//
			/*
				Plan Code Range Description
				1 A140H - A3E0H Symbol and Chinese Control Code
				1 A440H - C67EH Commonly Used Characters
				2 C940H - F9D5H Less Commonly Used Characters
				UDF FA40H - FEFE User-Defined Characters

				8E40H - A0FEH User-Defined Characters

				8140H - 8DFEH User-Defined Characters

				8181H - 8C82H User-Defined Characters

				F9D6H - F9F1H User-Defined Characters
			*/

			uint16_t b12 = 0;
			memcpy(&b12, b, 2);

			if ((b12 < 0xa440 || b12 > 0xc67e) && (b12 < 0xc940 || b12 > 0xf9d5))
			{
				if (test_server)
					LOG_INFO("check_name_big5[{}][{}] {:x} {:x} {:x}", i - 2, str, b1, b2, b12);

				return 0;
			}
		}
		else
		{
			if (!isdigit(str[i]) && !isalpha(str[i]))
				return 0;

			i++;
		}
	}

	return check_name_independent(str);
}

int check_name_euckr(const char * str)
{
	int		code;
	const char*	tmp;

	if (!str || !*str)
		return 0;

	if ( strlen(str) < 2 || strlen(str) > 12 )
		return 0;

	for (tmp = str; *tmp; ++tmp)
	{
		if (isnhspace(*tmp))
			return 0;

		if (isnhdigit(*tmp))
			continue;

		if (!ishan(*tmp) && isalpha(*tmp))
			continue;

		code = *tmp;
		code += 256;

		if (code < 176 || code > 200)
			return 0;

		++tmp;

		if (!*tmp)
			break;
	}

	return check_name_independent(str);
}

int check_name_latin1(const char * str)
{
	int		code;
	const char*	tmp;

	if (!str || !*str)
		return 0;

	if (strlen(str) < 2)
		return 0;

	for (tmp = str; *tmp; ++tmp)
	{
		if (isnhspace(*tmp))
			return 0;

		if (isnhdigit(*tmp))
			continue;

		if (!ishan(*tmp) && isalpha(*tmp))
			continue;

		unsigned char uc_tmp = *tmp;

		if (uc_tmp == 145 || uc_tmp == 146 || uc_tmp == 196
				|| uc_tmp == 214 || uc_tmp == 220 || uc_tmp == 223
				|| uc_tmp == 228 || uc_tmp == 246 || uc_tmp == 252 )
			continue;
		code = *tmp;
		code += 256;

		if (code < 176 || code > 200)
			return 0;

		++tmp;

		if (!*tmp)
			break;
	}

	return check_name_independent(str);
}

int check_name_alphabet(const char * str)
{
	const char*	tmp;

	if (!str || !*str)
		return 0;

	if (strlen(str) < 2)
		return 0;

	for (tmp = str; *tmp; ++tmp)
	{
		if (isdigit(*tmp) || isalpha(*tmp))
			continue;
		else
			return 0;
	}

	return check_name_independent(str);
}
// DISABLE_SPECIAL_CHAR_NAMING
bool sjis_is_disable_name_char(const char* src)
{
	static const char* sjis_symbols = "?";

	if (strncmp(src, sjis_symbols, 2) == 0)
		return true;

	return false;
}
// END_OF_DISABLE_SPECIAL_CHAR_NAMING



//-----------------------------------------------
// CHECK SJIS STRING
//-----------------------------------------------
#define issjishead(c) ((0x81<=(c) && (c)<=0x9f) || \
		                               ((0xe0<=(c)) && (c)<=0xfc))
#define issjistail(c) ((0x40<=(c) && (c)<=0x7e) || \
		                               (0x80<=(c) && (c)<=0xfc))

static int is_char_sjis(const char *p, const char *e)
{
	return (issjishead((uint8_t) *p) && (e-p)>1 && issjistail((uint8_t)p[1]) ? true : false);
}

int is_twobyte_sjis(const char *str)
{
	if (str && str[0] && str[1])
		return issjishead((uint8_t)str[0]) && issjistail((uint8_t)str[1]);
	else
		return 0;
}

int check_name_sjis(const char *str)
{
	const char	*p = str;
	const char	*e = str + strlen(str);	// NULL position

	if ( strlen(str) < 2 || strlen(str) > 16 )
		return 0;

	while (*p)
	{
		if (is_char_sjis(p, e))
		{
			// DISABLE_SPECIAL_CHAR_NAMING
			if (sjis_is_disable_name_char(p))
				return false;

			// END_OF_DISABLE_SPECIAL_CHAR_NAMING
			if ((uint8_t)p[0]==0x81 && (uint8_t)p[1]==0x40) return false;

			p += 2;
			continue;
		}
		else
		{
			if (isalpha(*p) || isdigit(*p))
			{
				p += 1;
				continue;
			}
			else
			{
				return 0;
			}
		}
	}


	return check_name_independent(str);
}
//-----------------------------------------------
// END OF CHECK SJIS STRING
//-----------------------------------------------
void LocaleService_LoadEmpireTextConvertTables()
{
	char szFileName[256];

	for (int iEmpire = 1; iEmpire <= 3; ++iEmpire)
	{
		snprintf(szFileName, sizeof(szFileName), "%s/lang%d.cvt", LocaleService_GetBasePath().c_str(), iEmpire);
		LOG_INFO("Load {}", szFileName);

		LoadEmpireTextConvertTable(iEmpire, szFileName);
	}
}

bool LocaleService_InitFlat(const std::string& dataPath, const std::string& locale)
{
	if (g_stServiceBasePath != ".")
	{
		LOG_ERROR("ALREADY exist service");
		return false;
	}

	if (!dataPath.empty())
		g_stServiceBasePath = dataPath;

	g_stServiceMapPath = g_stServiceBasePath + "/map";
	g_stQuestDir = g_stServiceBasePath + "/quest";

	g_setQuestObjectDir.clear();
	g_setQuestObjectDir.insert(g_stQuestDir + "/object");

	if (!locale.empty())
		g_stLocale = locale;

	if (g_stLocale == "sjis")
	{
		check_name = check_name_sjis;
		is_twobyte = is_twobyte_sjis;
	}
	else if (g_stLocale == "big5")
	{
		check_name = check_name_big5;
		is_twobyte = is_twobyte_big5;
	}
	else if (g_stLocale == "gb2312")
	{
		check_name = check_name_gb2312;
		is_twobyte = is_twobyte_gb2312;
	}
	else if (g_stLocale == "euckr")
	{
		check_name = check_name_euckr;
		is_twobyte = is_twobyte_euckr;
	}
	else
	{
		check_name = check_name_alphabet;
		is_twobyte = is_twobyte_euckr;
	}

	LOG_INFO("Locale {} (data path {})", g_stLocale, g_stServiceBasePath);
	return true;
}

void LocaleService_TransferDefaultSetting()
{
	if (!check_name)
		check_name = check_name_euckr;

	if (!is_twobyte)
		is_twobyte = is_twobyte_euckr;

	if (!exp_table)
		exp_table = exp_table_common;
	
#ifdef __NEWPET_SYSTEM__
	if (!exppet_table)
		exppet_table = exppet_table_common;
#endif

	if (!CTableBySkill::instance().Check())
		exit(1);

	if (!aiPercentByDeltaLevForBoss)
		aiPercentByDeltaLevForBoss = aiPercentByDeltaLevForBoss_euckr;

	if (!aiPercentByDeltaLev)
		aiPercentByDeltaLev = aiPercentByDeltaLev_euckr;

	if (!aiChainLightningCountBySkillLevel)
		aiChainLightningCountBySkillLevel = aiChainLightningCountBySkillLevel_euckr;
}

const std::string& LocaleService_GetBasePath()
{
	return g_stServiceBasePath;
}

const std::string& LocaleService_GetMapPath()
{
	return g_stServiceMapPath;
}

const std::string& LocaleService_GetQuestPath()
{
	return g_stQuestDir;
}

