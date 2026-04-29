#pragma once

#include <memory>
#include <string>
#include <type_traits>

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

template <typename T, typename Char>
struct fmt::formatter<T, Char, std::enable_if_t<std::is_enum_v<T>>>
    : fmt::formatter<std::underlying_type_t<T>, Char>
{
    template <typename FormatContext>
    auto format(T value, FormatContext& ctx) const
    {
        using Underlying = std::underlying_type_t<T>;
        return fmt::formatter<Underlying, Char>::format(static_cast<Underlying>(value), ctx);
    }
};

template <typename T, typename Char>
struct fmt::formatter<
    T*,
    Char,
    std::enable_if_t<
        !std::is_void_v<T> &&
        !std::is_same_v<std::remove_cv_t<T>, char> &&
        !std::is_same_v<std::remove_cv_t<T>, wchar_t>>>
    : fmt::formatter<const void*, Char>
{
    template <typename FormatContext>
    auto format(T* value, FormatContext& ctx) const
    {
        return fmt::formatter<const void*, Char>::format(static_cast<const void*>(value), ctx);
    }
};

#ifdef LOG_TRACE
#undef LOG_TRACE
#endif
#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#ifdef LOG_INFO
#undef LOG_INFO
#endif
#ifdef LOG_WARN
#undef LOG_WARN
#endif
#ifdef LOG_ERROR
#undef LOG_ERROR
#endif

#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(logging::GetLogger(), __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(logging::GetLogger(), __VA_ARGS__)
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(logging::GetLogger(), __VA_ARGS__)
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(logging::GetLogger(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(logging::GetErrorLogger(), __VA_ARGS__)
