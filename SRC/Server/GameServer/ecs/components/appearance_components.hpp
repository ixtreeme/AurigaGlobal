#pragma once

#include <cstdint>
#include <string>

#include <common/tables.h>

namespace ecs {

struct AppearancePartsComponent {
    uint16_t parts[PART_MAX_NUM] {};
    uint8_t basePart { 0 };
};

struct ItemTitlePrefixCache {
    std::string prefix;
};

struct HideCostumeFlags {
	bool body { false };
	bool hair { false };
	bool accessory { false };
	bool weapon { false };
};

} // namespace ecs
