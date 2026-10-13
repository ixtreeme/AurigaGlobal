#include "stdafx.h"

#include "heart.h"

#include <chrono>
#include <thread>

extern volatile int num_events_called;

LPHEART heart_new(int opt_usec, HEARTFUNC func)
{
	LPHEART ht;

	if (!func)
	{
		LOG_ERROR("no function defined");
		return nullptr;
	}

	CREATE(ht, HEART, 1);

	const auto now = std::chrono::steady_clock::now();

	ht->func = func;
	ht->opt_time = std::chrono::microseconds(opt_usec);
	ht->passes_per_sec = 1000000 / opt_usec;
	ht->last_time = now;
	ht->before_sleep = now;
	return (ht);
}

int heart_idle(LPHEART ht)
{
	const auto now = std::chrono::steady_clock::now();

	ht->before_sleep = now;

	const auto process_time = now - ht->last_time;

	int missed_pulse;

	if (process_time < ht->opt_time)
	{
		missed_pulse = 0;
	}
	else
	{
		missed_pulse = static_cast<int>(process_time / ht->opt_time);
	}

	if (missed_pulse > 0)
	{
		ht->last_time = now;
	}
	else
	{
		ht->last_time = ht->before_sleep + ht->opt_time;

		const auto timeout = ht->last_time - now;

		std::this_thread::sleep_for(timeout);
	}

	++missed_pulse;

	if (missed_pulse <= 0)
	{
		LOG_ERROR("missed_pulse is not positive! ({})", missed_pulse);
		missed_pulse = 1;
	}

	if (missed_pulse > (30 * ht->passes_per_sec))
	{
		LOG_ERROR("losing {} seconds. (lag occured)", missed_pulse / ht->passes_per_sec);
		missed_pulse = 30 * ht->passes_per_sec;
	}

	return missed_pulse;
}
