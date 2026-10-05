#define __LIBTHECORE__
#include "stdafx.h"

int is_hangul(const uint8_t * str)
{
    if (str[0] >= 0xb0 && str[0] <= 0xc8 && str[1] >= 0xa1 && str[1] <= 0xfe)
	return 1;

    return 0;
}

int under_han(const void * orig)
{
    const uint8_t * str = (const uint8_t *) orig;
    uint8_t high, low;
    int len;

    len = strlen((const char*) str);

    if (len < 2) 
	return 0;

    if (str[len - 1] == ')')
    {
	while (str[len] != '(')
	    len--;
    }

    high = str[len - 2];
    low  = str[len - 1];

    if (!is_hangul(&str[len - 2])) 
	return 0; 

    high = KStbl[(high - 0xb0) * 94 + low - 0xa1] & 0x1f;

    if (high < 2)
	return 0;

    if (high > 28) 
	return 0;

    return 1;
}
