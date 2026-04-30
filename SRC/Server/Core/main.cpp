/*
 *    Filename: main.c
 * Description: 라이브러리 초기화/삭제 등
 *
 *      Author: 비엽 aka. Cronan
 */
#define __LIBTHECORE__
#include "stdafx.h"
#include "memory.h"
#include "Core/Logging.hpp"
//#ifdef DISABLE_CORE_PULSE_RAZOR93
#define THECORE_SECS_TO_PASSES(secs) ((secs) * thecore_heart->passes_per_sec)
//#endif
extern void GOST_Init();

LPHEART		thecore_heart = nullptr;

volatile int	shutdowned = false;
volatile int	tics = 0;
unsigned int	thecore_profiler[NUM_PF];

// newstuff
int	bCheckpointCheck = 1;

static int pid_init(void)
{
#ifdef _WIN32
	return true;
#else
	FILE*	fp;
	if ((fp = fopen("pid", "w")))
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
		LOG_INFO("\nStart of pid: {}\n", getpid()); // @warme012
	}
	else
	{
		LOG_INFO("pid_init(): could not open file for writing. (filename: ./pid)");
		LOG_ERROR("\nError writing pid file\n");
		return false;
	}
	return true;
#endif
}

static void pid_deinit(void)
{
#ifdef _WIN32
    return;
#else
    remove("./pid");
	LOG_INFO("\nEnd of pid\n"); // @warme012
#endif
}

int thecore_init(int fps, HEARTFUNC heartbeat_func)
{
#ifdef _WIN32
    srand(time(nullptr));
#else
    srandom(time(0) + getpid() + getuid());
    srandomdev();
#endif
    signal_setup();

	if (!log_init() || !pid_init())
		return false;

	GOST_Init();

	thecore_heart = heart_new(1000000 / fps, heartbeat_func);
	return true;
}

void thecore_shutdown()
{
    shutdowned = true;
}

int thecore_idle(void)
{
    thecore_tick();

    if (shutdowned)
		return 0;

	int pulses;
	uint32_t t = get_dword_time();

	if (!(pulses = heart_idle(thecore_heart)))
	{
		thecore_profiler[PF_IDLE] += (get_dword_time() - t);
		return 0;
	}

    thecore_profiler[PF_IDLE] += (get_dword_time() - t);
    return pulses;
}

void thecore_destroy(void)
{
	pid_deinit();
	log_destroy();
}

int thecore_pulse(void)
{
	return (thecore_heart->pulse);
}

float thecore_pulse_per_second(void)
{
	return ((float) thecore_heart->passes_per_sec);
}

float thecore_time(void)
{
	return ((float) thecore_heart->pulse / (float) thecore_heart->passes_per_sec);
}

int thecore_is_shutdowned(void)
{
	return shutdowned;
}

void thecore_tick(void)
{
	++tics;
}
