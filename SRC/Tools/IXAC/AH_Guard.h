#pragma once

namespace AntiHook
{
    // intervalMs: milyen gyakran fusson a watchdog / guardian ciklus (pl. 500–1000 ms).
    void Start(unsigned intervalMs = 1000);
    void Stop();
}