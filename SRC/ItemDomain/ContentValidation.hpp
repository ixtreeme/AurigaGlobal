#pragma once
#include <cstddef>
#include <string_view>

namespace auriga::item::detail
{
inline bool ContentCode(std::string_view value, std::size_t maximum = 96) noexcept
{
    if (value.empty() || value.size() > maximum || value.front() < 'a' || value.front() > 'z') return false;
    bool dot = false;
    for (unsigned char c : value)
    {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.')) return false;
        if (c == '.' && dot) return false;
        dot = c == '.';
    }
    return !dot;
}
inline bool AssetReference(std::string_view value, bool allowEmpty = false) noexcept
{
    if (value.empty()) return allowEmpty;
    if (value.size() > 192 || value.front() == '/' || value.back() == '/') return false;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= value.size(); ++i)
    {
        if (i == value.size() || value[i] == '/')
        {
            const auto part = value.substr(start, i - start);
            if (part.empty() || part == "." || part == "..") return false;
            start = i + 1;
        }
        else
        {
            const auto c = static_cast<unsigned char>(value[i]);
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
        }
    }
    return true;
}
} // namespace auriga::item::detail
