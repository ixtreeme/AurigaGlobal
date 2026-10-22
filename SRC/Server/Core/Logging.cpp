#define __LIBTHECORE__
#include "stdafx.h"
#include "Logging.hpp"

#include <spdlog/sinks/stdout_sinks.h>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <vector>

namespace logging
{
namespace
{
constexpr std::size_t LOG_ROTATE_SIZE = 50u * 1024u * 1024u;
constexpr std::size_t LOG_ROTATE_FILES = 10u;

std::mutex g_loggingMutex;
std::shared_ptr<spdlog::logger> g_logger;
std::shared_ptr<spdlog::logger> g_errorLogger;

// What the LOG_* macros write to outside Init()..Shutdown(): an object destroyed after
// Shutdown() (a manager's CAsyncSQL closing its connection, a static) still logs, and spdlog's
// registry holds no logger then, not even a default one. Synchronous stderr, and never
// destroyed, so it outlives every object that may log while the process ends.
std::shared_ptr<spdlog::logger> FallbackLogger()
{
    static auto* logger = new std::shared_ptr<spdlog::logger>([] {
        auto fallback = std::make_shared<spdlog::logger>("fallback", std::make_shared<spdlog::sinks::stderr_sink_mt>());
        fallback->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v");
        return fallback;
    }());
    return *logger;
}

std::string NormalizeLogDir(const std::string& logDir)
{
    if (logDir.empty())
        return ".";

    return logDir;
}
}

void Init(const std::string& logDir)
{
    std::lock_guard<std::mutex> lock(g_loggingMutex);

    if (g_logger && g_errorLogger)
        return;

    const std::string normalizedLogDir = NormalizeLogDir(logDir);
    std::error_code ec;
    std::filesystem::create_directories(normalizedLogDir, ec);

    if (!spdlog::thread_pool())
        spdlog::init_thread_pool(8192, 1);

    auto syslogSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        normalizedLogDir + "/syslog.txt", LOG_ROTATE_SIZE, LOG_ROTATE_FILES);
    auto syserrSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        normalizedLogDir + "/syserr.txt", LOG_ROTATE_SIZE, LOG_ROTATE_FILES);

    std::vector<spdlog::sink_ptr> syslogSinks{syslogSink};
    g_logger = std::make_shared<spdlog::async_logger>(
        "syslog",
        syslogSinks.begin(),
        syslogSinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest);

    std::vector<spdlog::sink_ptr> syserrSinks{syserrSink};
    g_errorLogger = std::make_shared<spdlog::async_logger>(
        "syserr",
        syserrSinks.begin(),
        syserrSinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest);

    g_logger->set_level(spdlog::level::info);
    g_errorLogger->set_level(spdlog::level::info);
    g_logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v");
    g_errorLogger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] [%@] %v");
    g_errorLogger->flush_on(spdlog::level::warn);

    spdlog::register_logger(g_logger);
    spdlog::register_logger(g_errorLogger);
    spdlog::set_default_logger(g_logger);
    spdlog::flush_every(std::chrono::seconds(1));
}

void Shutdown()
{
    std::lock_guard<std::mutex> lock(g_loggingMutex);

    if (g_logger)
        g_logger->flush();
    if (g_errorLogger)
        g_errorLogger->flush();

    spdlog::drop_all();
    g_errorLogger.reset();
    g_logger.reset();
    spdlog::shutdown();
    // spdlog::shutdown() dropped the default logger too; SPDLOG_* and LOG_* need one.
    spdlog::set_default_logger(FallbackLogger());
}

std::shared_ptr<spdlog::logger> GetLogger()
{
    if (g_logger)
        return g_logger;
    if (auto logger = spdlog::default_logger())
        return logger;
    return FallbackLogger();
}

std::shared_ptr<spdlog::logger> GetErrorLogger()
{
    if (g_errorLogger)
        return g_errorLogger;
    if (auto logger = spdlog::default_logger())
        return logger;
    return FallbackLogger();
}
}
