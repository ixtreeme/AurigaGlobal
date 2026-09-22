#pragma once

// Installs an unhandled-exception filter that writes a symbolised stack trace
// to stderr and to crash.txt in the working directory. Without it an access
// violation leaves nothing behind but a WER event: the async syslog queue is
// lost with the process, so the last logged line is not the last line run.
void InstallCrashReporter();
