#pragma once
#include "AbsorptionSashRuntime.hpp"
namespace auriga::item {
// GC220 v2 retains the v1 prefix. Selection 2/3/4 denotes combine/absorb/clear;
// CG214 remains 28 bytes. Source and target are real owned instance IDs.
std::vector<std::uint8_t> EncodeAbsorptionSashReply(const ItemCatalog&,std::uint8_t status,
    std::uint64_t nonce,std::uint64_t source,std::uint64_t target,std::uint8_t selection,
    const AbsorptionSashQuote* quote=nullptr,std::uint8_t outcome=0,std::uint64_t resultInstance=0);
}
