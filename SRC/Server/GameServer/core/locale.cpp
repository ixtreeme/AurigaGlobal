#include "stdafx.h"
#include "locale.hpp"
#include "locale_service.h"
#include <common/length.h>

#include <fstream>

typedef std::map< std::string, std::string > LocaleStringMapType;
typedef std::map< std::string, LocaleStringMapType> LocaleMapType;

LocaleMapType localeMap; 

int g_iUseLocale = 0;

const char * locale_convert_language(uint8_t bLanguage)
{
	switch (bLanguage)
	{
		case LANGUAGE_EN:
			return "en";
		
		case LANGUAGE_RO:
			return "ro";		

		case LANGUAGE_IT:
			return "it";
			
		case LANGUAGE_TR:
			return "tr";
			
		case LANGUAGE_DE:
			return "de";
			
		case LANGUAGE_PL:
			return "pl";

		case LANGUAGE_PT:
			return "pt";
			
		case LANGUAGE_ES:
			return "es";
			
		case LANGUAGE_CZ:
			return "cz";
			
		case LANGUAGE_HU:
			return "hu";

		default:
			return "en";
	}
}

void global_locale_add(std::string lang, LocaleStringMapType map)
{
	LocaleMapType::const_iterator iter = localeMap.find(lang);
	
	if (iter == localeMap.end())
	{
		localeMap.insert(std::make_pair(lang, map));
	}
}

void locale_add(std::string lang, const char **strings)
{
	LocaleMapType::const_iterator mapIter = localeMap.find(lang);

	if (mapIter == localeMap.end())
		return;

	LocaleStringMapType localeStrings = mapIter->second;
	LocaleStringMapType::const_iterator iter = localeStrings.find(strings[0]);

	if (iter == localeStrings.end())
	{
		localeStrings.insert(std::make_pair(strings[0], strings[1]));
	}
}

const char *quote_find_end(const char *string)
{
	const char  *tmp = string;
	int32_t         quote = 0;

	while (*tmp)
	{
		if (quote && *tmp == '\\' && *(tmp + 1))
		{
			switch (*(tmp + 1))
			{
			case '"':
				tmp += 2;
				continue;
			}
		}
		else if (*tmp == '"')
		{
			quote = !quote;
		}
		else if (!quote && *tmp == ';')
			return (tmp);

		tmp++;
	}

	return nullptr;
}

char *locale_convert(const char *src, int32_t len)
{
	const char	*tmp;
	int32_t		i, j;
	char	*buf, *dest;
	int32_t		start = 0;
	char	last_char = 0;

	if (!len)
		return nullptr;

	buf = M2_NEW char[len + 1];

	for (j = i = 0, tmp = src, dest = buf; i < len; i++, tmp++)
	{
		if (*tmp == '"')
		{
			if (last_char != '\\')
				start = !start;
			else
				goto ENCODE;
		}
		else if (*tmp == ';')
		{
			if (last_char != '\\' && !start)
				break;
			else
				goto ENCODE;
		}
		else if (start)
		{
		ENCODE:
			if (*tmp == '\\' && *(tmp + 1) == 'n')
			{
				*(dest++) = '\n';
				tmp++;
				last_char = '\n';
			}
			else
			{
				*(dest++) = *tmp;
				last_char = *tmp;
			}

			j++;
		}
	}

	if (!j)
	{
		M2_DELETE_ARRAY(buf);
		return nullptr;
	}

	*dest = '\0';
	return (buf);
}
