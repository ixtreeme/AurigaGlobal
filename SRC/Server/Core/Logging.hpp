#pragma once

#include <memory>
#include <string>

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

namespace logging
{
void Init(const std::string& logDir);
void Shutdown();

std::shared_ptr<spdlog::logger> GetLogger();
std::shared_ptr<spdlog::logger> GetErrorLogger();
}

#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(logging::GetLogger(), __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(logging::GetLogger(), __VA_ARGS__)
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(logging::GetLogger(), __VA_ARGS__)
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(logging::GetLogger(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(logging::GetErrorLogger(), __VA_ARGS__)
