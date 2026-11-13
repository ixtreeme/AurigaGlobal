#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace auriga {
// Exact thousandths. No implicit numeric conversion: an integer-only call site
// must explicitly choose whether it is a bonus, an ID, or a final HP/damage value.
// This type does not change the existing wire protocol until explicitly integrated.
class BonusValue {
public:
    static constexpr int64_t Scale = 1000;
    constexpr BonusValue() = default;
    static constexpr BonusValue FromRaw(int64_t raw) { return BonusValue(raw); }
    constexpr int64_t Raw() const { return raw_; }
    constexpr bool operator==(const BonusValue&) const = default;

    static constexpr bool TryFromWhole(int64_t whole, BonusValue& out) {
        if (whole > Max / Scale || whole < Min / Scale) return false;
        out = FromRaw(whole * Scale);
        return true;
    }

    // Parse invariant decimal text. A nonzero fourth decimal is rejected, never
    // rounded; extra trailing zeros are accepted because they lose no precision.
    static bool TryParse(std::string_view text, BonusValue& out) {
        if (text.empty()) return false;
        size_t i = 0;
        const bool negative = text[0] == '-';
        if (text[0] == '-' || text[0] == '+') ++i;
        if (i == text.size()) return false;
        const uint64_t limit = negative ? uint64_t(Max) + 1 : uint64_t(Max);
        uint64_t whole = 0, fraction = 0;
        unsigned decimals = 0;
        bool dot = false, digit = false;
        for (; i < text.size(); ++i) {
            const char c = text[i];
            if (c == '.' && !dot) { dot = true; continue; }
            if (c < '0' || c > '9') return false;
            digit = true;
            const auto n = static_cast<unsigned>(c - '0');
            if (!dot) {
                if (whole > (limit / Scale - n) / 10) return false;
                whole = whole * 10 + n;
                if (whole > limit / Scale) return false;
            } else if (decimals < 3) {
                fraction = fraction * 10 + n;
                ++decimals;
            } else if (n != 0) {
                return false;
            }
        }
        if (!digit) return false;
        for (; decimals < 3; ++decimals) fraction *= 10;
        if (whole > (limit - fraction) / Scale) return false;
        const uint64_t magnitude = whole * Scale + fraction;
        int64_t raw;
        if (negative && magnitude == uint64_t(Max) + 1) raw = Min;
        else raw = negative ? -static_cast<int64_t>(magnitude) : static_cast<int64_t>(magnitude);
        out = FromRaw(raw);
        return true;
    }

    std::string ToString() const {
        const bool negative = raw_ < 0;
        const uint64_t magnitude = negative ? uint64_t(-(raw_ + 1)) + 1 : uint64_t(raw_);
        std::string result = negative ? "-" : "";
        result += std::to_string(magnitude / Scale);
        const auto fraction = magnitude % Scale;
        if (fraction) {
            std::string digits = std::to_string(Scale + fraction).substr(1);
            while (digits.back() == '0') digits.pop_back();
            result += "." + digits;
        }
        return result;
    }

    static constexpr bool TryAdd(BonusValue a, BonusValue b, BonusValue& out) {
        if ((b.raw_ > 0 && a.raw_ > Max - b.raw_) ||
            (b.raw_ < 0 && a.raw_ < Min - b.raw_)) return false;
        out = FromRaw(a.raw_ + b.raw_);
        return true;
    }
    static constexpr bool TrySubtract(BonusValue a, BonusValue b, BonusValue& out) {
        if ((b.raw_ > 0 && a.raw_ < Min + b.raw_) ||
            (b.raw_ < 0 && a.raw_ > Max + b.raw_)) return false;
        out = FromRaw(a.raw_ - b.raw_);
        return true;
    }

    // Explicit little-endian encoding; never memcpy a changed C++ struct into
    // an existing packet. Version/length negotiation belongs to the caller.
    constexpr std::array<uint8_t, 8> EncodeRaw() const {
        std::array<uint8_t, 8> bytes {};
        const uint64_t bits = static_cast<uint64_t>(raw_);
        for (unsigned i = 0; i < 8; ++i) bytes[i] = static_cast<uint8_t>(bits >> (i * 8));
        return bytes;
    }
    static constexpr BonusValue DecodeRaw(const std::array<uint8_t, 8>& bytes) {
        uint64_t bits = 0;
        for (unsigned i = 0; i < 8; ++i) bits |= uint64_t(bytes[i]) << (i * 8);
        if (bits <= uint64_t(Max)) return FromRaw(static_cast<int64_t>(bits));
        return FromRaw(Min + static_cast<int64_t>(bits - (uint64_t(Max) + 1)));
    }

private:
    static constexpr int64_t Max = std::numeric_limits<int64_t>::max();
    static constexpr int64_t Min = std::numeric_limits<int64_t>::min();
    explicit constexpr BonusValue(int64_t raw) : raw_(raw) {}
    int64_t raw_ {};
};
static_assert(sizeof(BonusValue) == sizeof(int64_t));
} // namespace auriga
