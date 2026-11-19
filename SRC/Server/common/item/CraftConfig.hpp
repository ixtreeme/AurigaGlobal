#pragma once
#include "Crafting.hpp"
#include <string_view>
namespace auriga::item {
std::expected<std::vector<CraftWorkshop>,std::string> ParseCraftWorkshops(std::string_view,const ItemCatalog&);
}
