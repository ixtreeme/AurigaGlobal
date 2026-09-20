#include "../../SRC/Launcher/Base/StdAfx.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

namespace {
uint32_t platformClock = 0;
DWORD WINAPI FakeTimeGetTime() { return platformClock; }
}

// Include the real implementation after its platform declarations have been
// parsed. Only the platform clock is substituted; no production timer behavior
// is reimplemented in this test and no sleep or full client build is required.
#define timeGetTime FakeTimeGetTime
#include "../../SRC/Launcher/Base/Timer.cpp"
#undef timeGetTime

namespace {
int checks = 0;
int failures = 0;

void Equal(uint32_t actual, uint32_t expected, std::string_view message)
{
    ++checks;
    if (actual == expected)
        return;
    ++failures;
    std::cerr << message << ": expected=" << expected << " actual=" << actual << '\n';
}

void NetworkClock(uint32_t expected, std::string_view stage)
{
    ELTimer_SetFrameMSec();
    Equal(ELTimer_GetServerMSec(), expected, stage);
    Equal(ELTimer_GetServerFrameMSec(), expected, stage);
}

void RealTimeClock()
{
    platformClock = 1000;
    CTimer timer;
    platformClock += 500;
    ELTimer_SetServerMSec(100000);
    NetworkClock(100000, "real-time handshake");
    platformClock += 73;
    timer.Advance();
    NetworkClock(100073, "real-time elapsed interval");
}

void AgedCustomClockAndRelog()
{
    platformClock = 1000;
    CTimer timer;
    timer.UseCustomTime();
    timer.Adjust(306137);
    platformClock += 123203793;
    Equal(timer.GetCurrentMillisecond(), 306137, "custom animation clock fixture");

    // Reproduce the live mount failure: an old client process, a young custom
    // animation clock, and a freshly received game-server handshake timestamp.
    ELTimer_SetServerMSec(708957);
    NetworkClock(708957, "aged custom-clock handshake");

    // HandShakePhase resets this custom clock immediately after syncing. Such
    // animation resets must neither rewind network time nor change its epoch.
    timer.SetBaseTime();
    NetworkClock(708957, "handshake animation reset");
    timer.Advance();
    timer.Adjust(500000);
    NetworkClock(708957, "animation advance and adjustment");

    platformClock += 1234;
    NetworkClock(710191, "network elapsed time with custom animation clock");
    timer.Adjust(-400000);
    NetworkClock(710191, "backward animation adjustment");

    // Relog/channel transitions resynchronize the same running client.
    ELTimer_SetServerMSec(900000);
    NetworkClock(900000, "second handshake");
    timer.SetBaseTime();
    NetworkClock(900000, "second handshake animation reset");
    platformClock += 27;
    Equal(ELTimer_GetServerMSec(), 900027, "live clock progresses between frames");
    Equal(ELTimer_GetServerFrameMSec(), 900000, "frame clock remains sampled");
    NetworkClock(900027, "next sampled network frame");

    ELTimer_SetServerMSec(0);
    NetworkClock(900027, "zero handshake leaves previous synchronization intact");
}

void UnsignedClockWrap()
{
    platformClock = 0;
    CTimer timer;
    timer.UseCustomTime();
    timer.Adjust(306137);
    platformClock = (std::numeric_limits<uint32_t>::max)() - 16;
    ELTimer_SetServerMSec((std::numeric_limits<uint32_t>::max)() - 15);
    NetworkClock((std::numeric_limits<uint32_t>::max)() - 15, "before clock wrap");
    platformClock += 64;
    NetworkClock(48, "platform and server clock wrap");
    timer.SetBaseTime();
    timer.Advance();
    NetworkClock(48, "animation reset after clock wrap");
    platformClock += 100;
    NetworkClock(148, "elapsed interval after clock wrap");
    ELTimer_SetServerMSec(1000);
    NetworkClock(1000, "resynchronization after clock wrap");
}

void NetworkSchedulingArithmetic()
{
    Equal(ELTimer_AverageNetworkGap(0, 122898102), 36869430,
        "observed large network gap must not overflow its weighted product");
    Equal(ELTimer_AverageNetworkGap(122000000, 122000000), 122000000,
        "stable large network gap");
    Equal(static_cast<uint32_t>(ELTimer_AverageNetworkGap(-122000000, -122000000)),
        static_cast<uint32_t>(-122000000), "stable negative network gap");
    const auto largest = (std::numeric_limits<int32_t>::max)();
    const auto smallest = (std::numeric_limits<int32_t>::min)();
    Equal(ELTimer_AverageNetworkGap(largest, largest), largest, "maximum signed network gap");
    Equal(static_cast<uint32_t>(ELTimer_AverageNetworkGap(smallest, smallest)),
        static_cast<uint32_t>(smallest), "minimum signed network gap");
    Equal(ELTimer_AverageNetworkGap(largest, smallest), 858993458,
        "mixed-sign extreme network gaps");
    Equal(ELTimer_AverageNetworkGap(-1, 0), 0, "weighted gap truncates toward zero");

    Equal(ELTimer_IsTimeBefore(100, 110), true, "normal future command waits");
    Equal(ELTimer_IsTimeBefore(110, 110), false, "command is due at equality");
    Equal(ELTimer_IsTimeBefore(120, 110), false, "past command is due");
    const auto wrap = (std::numeric_limits<uint32_t>::max)();
    Equal(ELTimer_IsTimeBefore(wrap - 10, 5), true, "future command across clock wrap waits");
    Equal(ELTimer_IsTimeBefore(5, wrap - 10), false, "past command across clock wrap is due");
    Equal(ELTimer_IsTimeBefore(123733383, 4289583212u), false,
        "observed wrapped mount queue head does not block processing");
}
}

int main()
{
    RealTimeClock();
    AgedCustomClockAndRelog();
    UnsignedClockWrap();
    NetworkSchedulingArithmetic();
    std::cout << "Client timer checks: " << checks << ", failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
