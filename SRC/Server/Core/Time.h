#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>

// Idokezelesi alapok (ld. docs/chrono_time_audit.md).
//
//  - WallClock  : unix idobelyeg (DB, naptar, megjelenites) - ugralhat
//                 (NTP/DST/kezi allitas), ezert csak tartositasra/kiirasra.
//  - SteadyClock: monoton (process uptime) - minden duration/timer/expiry.
namespace auriga::time
{
	using WallClock = std::chrono::system_clock;
	using SteadyClock = std::chrono::steady_clock;

	using Milliseconds = std::chrono::milliseconds;
	using Seconds = std::chrono::seconds;

	// Wall clock unix masodperc (64-bit).
	inline int64_t UnixSeconds()
	{
		return std::chrono::duration_cast<Seconds>(WallClock::now().time_since_epoch()).count();
	}

	inline WallClock::time_point FromUnixSeconds(int64_t seconds)
	{
		return WallClock::time_point(std::chrono::seconds(seconds));
	}

	// Monoton ezredmasodperc a process indulasa ota (elso hivas a 0 pont).
	inline uint64_t ProcessMillis()
	{
		static const SteadyClock::time_point start = SteadyClock::now();
		return static_cast<uint64_t>(std::chrono::duration_cast<Milliseconds>(SteadyClock::now() - start).count());
	}

	// Monoton masodperc float-kent (sync ablakok).
	inline float ProcessSeconds()
	{
		static const SteadyClock::time_point start = SteadyClock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(SteadyClock::now() - start).count();
		return static_cast<float>(elapsed) / 1000000.0f;
	}

	// Thread-safe helyi ido ertek szerint (localtime_s/localtime_r).
	inline std::tm LocalTime(std::time_t t)
	{
		std::tm out {};
#ifdef _WIN32
		localtime_s(&out, &t);
#else
		localtime_r(&t, &out);
#endif
		return out;
	}
}
