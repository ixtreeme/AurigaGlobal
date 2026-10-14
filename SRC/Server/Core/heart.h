#pragma once

#include <chrono>
#include <cstdint>

typedef struct heart	HEART;
typedef struct heart* LPHEART;

typedef void (*HEARTFUNC) (LPHEART heart, int pulse);

// A heartbeat monoton orat (steady_clock) hasznal: a falora ugralasa
// (NTP/DST/kezi allitas) nem torheti el a tick-utemezest
// (ld. docs/chrono_time_audit.md).
struct heart
{
    HEARTFUNC		func;

    std::chrono::steady_clock::time_point	before_sleep;
    std::chrono::microseconds	opt_time;
    std::chrono::steady_clock::time_point	last_time;

    int			passes_per_sec;
    int64_t		pulse;
};

extern LPHEART	heart_new(int opt_usec, HEARTFUNC func);
extern int	heart_idle(LPHEART ht);
