#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace auriga::social::history {
using Key=std::array<unsigned char,32>;
struct Metadata {std::uint64_t id=0,created=0;std::uint32_t a=0,b=0,sender=0;std::uint8_t kind=0,empire=0,gm=0;std::string name;};
struct Sealed {std::string nonce,cipher,tag;};
std::string Hex(std::string_view bytes);
std::optional<std::string> Unhex(std::string_view text);
bool Valid(const Metadata& metadata);
std::optional<Sealed> Seal(const Key& key,const Metadata& metadata,std::string_view text);
std::optional<std::string> Open(const Key& key,const Metadata& metadata,const Sealed& sealed);
}
