# Client clock regression

TimerTests executes the production Base/Timer.cpp with a deterministic replacement
for timeGetTime. Custom animation time may reset or advance independently, but
network synchronization and network frame time must share the application clock.
No renderer, socket or running game process is used by this test.

The live failure had application frame time around 123 million milliseconds and
custom animation time around 300 thousand. Subtracting the custom handshake base
from application frame time overflowed the 32-bit weighted network-gap average.
A mount's command queue then waited on timestamp 4289583212 while network actor
positions updated normally. The visible model never executed its queued moves.

Tests cover that clock-domain mismatch, custom-time resets/adjustments, relog
resynchronization, sampled frame time, timer wrap, widened weighted averages and
wrap-aware command deadlines. Before the clock fix, 26 of 35 clock assertions
failed. Scheduling checks call the same helpers used by InstanceBase, but do not
claim to run its complete rendering or command-processing loop.

Build TimerTests, then run:

```powershell
ctest --test-dir build -C RelWithDebInfo -R client_timer --output-on-failure
```

Use build-asan for the AddressSanitizer configuration.
The full UserInterface target must also compile. In-game verification requires
restarting the client with the rebuilt executable, then testing mount following,
monster pursuit, relog and channel changes. Replacing the server alone does not
update this client-side clock fix.
