#pragma once

namespace AntiHook::HandleScanner
{
    // Más folyamatok által felénk nyitott erõs handle-ok (VM_WRITE/ALL_ACCESS).
    // Ha egy PID túl sok ilyet tart, log + TerminateProcess.
    void LogSuspiciousProcesses(const char* logFile);
}
