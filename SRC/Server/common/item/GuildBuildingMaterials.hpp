#pragma once
#include <common/building.h>
#include <nlohmann/json.hpp>
#include <array>
#include <optional>
#include <set>
#include <string_view>

namespace auriga::item {
inline std::optional<std::array<building::TObjectMaterial, building::OBJECT_MATERIAL_MAX_NUM>>
ReadGuildBuildingMaterials(std::string_view text)
{
    try {
        const auto rows=nlohmann::json::parse(text);
        if(!rows.is_array() || rows.size()>building::OBJECT_MATERIAL_MAX_NUM) return {};
        std::array<building::TObjectMaterial,building::OBJECT_MATERIAL_MAX_NUM> result{};
        std::set<std::pair<uint16_t,uint32_t>> keys;
        size_t i=0;
        for(const auto& row:rows) {
            if(!row.is_object() || row.size()!=3 || !row.contains("category") || !row.contains("local") || !row.contains("count")) return {};
            const auto number=[&](const char* name,uint64_t limit)->uint64_t {
                const auto& v=row.at(name);
                if(!v.is_number_integer() || (v.is_number_integer() && v.get<int64_t>()<=0) || v.get<uint64_t>()>limit)
                    throw std::invalid_argument("Invalid native construction material");
                return v.get<uint64_t>();
            };
            const auto category=static_cast<uint16_t>(number("category",UINT16_MAX));
            const auto local=static_cast<uint32_t>(number("local",UINT32_MAX));
            const auto count=static_cast<uint32_t>(number("count",UINT32_MAX));
            if(!keys.emplace(category,local).second) return {};
            result[i++]={category,local,count};
        }
        return result;
    } catch(const std::exception&) {return {};}
}
}
