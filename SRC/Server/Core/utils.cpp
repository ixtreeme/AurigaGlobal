#define __LIBTHECORE__
#include "stdafx.h"

#include "Time.h"

#include <chrono>
#include <thread>

static struct timeval null_time = { 0, 0 };

struct timeval* timediff(const struct timeval* a, const struct timeval* b)
{
	static struct timeval rslt;

	if (a->tv_sec < b->tv_sec)
		return &null_time;
	else if (a->tv_sec == b->tv_sec)
	{
		if (a->tv_usec < b->tv_usec)
			return &null_time;
		else
		{
			rslt.tv_sec = 0;
			rslt.tv_usec = a->tv_usec - b->tv_usec;
			return &rslt;
		}
	}
	else
	{
		rslt.tv_sec = a->tv_sec - b->tv_sec;

		if (a->tv_usec < b->tv_usec)
		{
			rslt.tv_usec = a->tv_usec + 1000000 - b->tv_usec;
			rslt.tv_sec--;
		}
		else
			rslt.tv_usec = a->tv_usec - b->tv_usec;

		return &rslt;
	}
}

struct timeval* timeadd(struct timeval* a, struct timeval* b)
{
	static struct timeval rslt;

	rslt.tv_sec = a->tv_sec + b->tv_sec;
	rslt.tv_usec = a->tv_usec + b->tv_usec;

	while (rslt.tv_usec >= 1000000)
	{
		rslt.tv_usec -= 1000000;
		rslt.tv_sec++;
	}

	return &rslt;
}


void trim_and_lower(const char* src, char* dest, uint64_t dest_size)
{
	const char* tmp = src;
	uint64_t len = 0;

	if (!dest || dest_size == 0)
		return;

	if (!src)
	{
		*dest = '\0';
		return;
	}

	while (*tmp)
	{
		if (!isnhspace(*tmp))
			break;

		tmp++;
	}

	--dest_size;

	while (*tmp && len < dest_size)
	{
		*(dest++) = LOWER(*tmp);
		++tmp;
		++len;
	}

	*dest = '\0';

	if (len > 0)
	{
		--dest;

		while (*dest && isnhspace(*dest) && len--)
			*(dest--) = '\0';
	}
}

void parse_token(char* src, char* token, char* value)
{
	char* tmp;

	for (tmp = src; *tmp && *tmp != ':'; tmp++)
	{
		if (isnhspace(*tmp))
			continue;

		*(token++) = LOWER(*tmp);
	}

	*token = '\0';

	for (tmp += 2; *tmp; tmp++)
	{
		if (*tmp == '\n' || *tmp == '\r')
			continue;

		*(value++) = *tmp;
	}

	*value = '\0';
}

struct tm* tm_calc(const struct tm* curr_tm, int days)
{
	char                yoon = false;
	static struct tm    new_tm;
	int                 monthdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if (!curr_tm)
	{
		time_t time_s = time(0);
		new_tm = auriga::time::LocalTime(time_s);
	}
	else
		memcpy(&new_tm, curr_tm, sizeof(struct tm));

	if (new_tm.tm_mon == 1)
	{
		if (!((new_tm.tm_year + 1900) % 4))
		{
			if (!((new_tm.tm_year + 1900) % 100))
			{
				if (!((new_tm.tm_year + 1900) % 400))
					yoon = true;
			}
			else
				yoon = true;
		}

		if (yoon)
			new_tm.tm_mday += 1;
	}

	if (yoon)
		monthdays[1] = 29;
	else
		monthdays[1] = 28;

	new_tm.tm_mday += days;

	if (new_tm.tm_mday <= 0)
	{
		new_tm.tm_mon--;

		if (new_tm.tm_mon < 0)
		{
			new_tm.tm_year--;
			new_tm.tm_mon = 11;
		}

		new_tm.tm_mday = monthdays[new_tm.tm_mon] + new_tm.tm_mday;
	}
	else if (new_tm.tm_mday > monthdays[new_tm.tm_mon])
	{
		new_tm.tm_mon++;

		if (new_tm.tm_mon > 11)
		{
			new_tm.tm_year++;
			new_tm.tm_mon = 0;
		}

		new_tm.tm_mday = monthdays[new_tm.tm_mon] - new_tm.tm_mday;
	}

	return (&new_tm);
}

int MIN(int a, int b)
{
	return a < b ? a : b;
}

int MAX(int a, int b)
{
	return a > b ? a : b;
}

int MINMAX(int min, int value, int max)
{
	int tv;

	tv = (min > value ? min : value);
	return (max < tv) ? max : tv;
}


uint32_t UMINMAX(uint32_t min, uint32_t value, uint32_t max)
{
	uint32_t tv;

	tv = (min > value ? min : value);
	return (max < tv) ? max : tv;
}

//int64_t MIN(int64_t a, int64_t b)
//{
//    return a < b ? a : b;
//}
//
//int64_t MAX(int64_t a, int64_t b)
//{
//    return a > b ? a : b;
//}
//
//int64_t MINMAX(int64_t min, int64_t value, int64_t max)
//{
//    int64_t tv;
//
//    tv = (min > value ? min : value);
//    return (max < tv) ? max : tv;
//}


uint32_t thecore_random()
{
#ifdef _WIN32
	return rand();
#else
	return random();
#endif
}

int number_ex(int from, int to, const char* file, int line)
{
	if (from > to)
	{
		int tmp = from;

		LOG_ERROR("number(): first argument is bigger than second argument {} -> {}, {} {}", from, to, file, line);

		from = to;
		to = tmp;
	}

	int returnValue = 0;

	if ((to - from + 1) != 0)
		returnValue = ((thecore_random() % (to - from + 1)) + from);
	else
		LOG_ERROR("number(): devided by 0");

	return returnValue;
}

float fnumber(float from, float to)
{
	return (((float)thecore_random() / (float)RAND_MAX) * (to - from)) + from;
}

#ifndef _WIN32
void core_dump_unix(const char* who, uint16_t line)
{
	LOG_ERROR("*** Dumping Core {}:{} ***", who, line);

	fflush(stdout);
	fflush(stderr);

	if (fork() == 0)
		abort();
}

#else

void gettimeofday(struct timeval* t, struct timezone* dummy)
{
	uint32_t millisec = GetTickCount();

	t->tv_sec = (millisec / 1000);
	t->tv_usec = (millisec % 1000) * 1000;
}

void core_dump_unix(const char* who, uint16_t line)
{
}

#endif

float get_float_time()
{
	// Monoton (steady_clock): NTP/DST/kezi oraallitas nem zavarja a
	// sync-ablakokat.
	return auriga::time::ProcessSeconds();
}

uint32_t get_dword_time()
{
	// Monoton ezredmasodperc a process indulasa ota; a reguli timer-ek
	// unsigned kulonbseget hasznaljak, igy a ~49 napos ujraindulas rendben.
	return static_cast<uint32_t>(auriga::time::ProcessMillis());
}
