#pragma once
#include <cstddef>
#include <cstdint>

// Fixed string fields only. The packet processor still owns framing and the
// individual handlers validate variable payload lengths and field semantics.
bool ValidateClientPacketStrings(uint8_t header, const void* data, std::size_t length);
