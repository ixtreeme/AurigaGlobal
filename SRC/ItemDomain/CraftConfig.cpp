#include <item/CraftConfig.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace auriga::item {
std::expected<std::vector<CraftWorkshop>,std::string> ParseCraftWorkshops(std::string_view text,const ItemCatalog& catalog) {
    try {
        if(text.size()>(1u<<20))return std::unexpected("crafting document exceeds 1 MiB");
        const auto json=nlohmann::json::parse(text);
        if(!json.is_object() || json.size()!=2 || (json.at("version")!=1 && json.at("version")!=2) || !json.at("workshops").is_array() || json.at("workshops").size()>MaxCraftWorkshops)return std::unexpected("invalid crafting document");
        const auto readNumber=[](const nlohmann::json& row,const char* key,std::uint64_t max)->std::uint64_t {
            const auto& v=row.at(key);if(!v.is_number_unsigned() || v.get<std::uint64_t>()>max)throw std::runtime_error("invalid crafting integer");return v.get<std::uint64_t>();};
        const auto key=[&](const nlohmann::json& row){auto value=ItemKey::Create(static_cast<std::uint16_t>(readNumber(row,"category",UINT16_MAX)),static_cast<std::uint32_t>(readNumber(row,"local",UINT32_MAX)));if(!value)throw std::runtime_error("invalid ItemKey");return *value;};
        std::vector<CraftWorkshop> definitions;
        for(const auto& w:json.at("workshops"))
        {
            if(!w.is_object() || w.size()!=3 || !w.at("recipes").is_array() || w.at("recipes").size()>MaxCraftRecipes)return std::unexpected("invalid workshop");
            CraftWorkshop shop{static_cast<std::uint32_t>(readNumber(w,"npc_race",UINT32_MAX)),w.at("name").get<std::string>(),{}};
            for(const auto& r:w.at("recipes"))
            {
                if(!r.is_object() || r.size()!=(json.at("version")==2 ? 8 : 6) || !r.at("ingredients").is_array() || r.at("ingredients").size()>MaxCraftIngredients || !r.at("result").is_object() || r.at("result").size()!=4)return std::unexpected("invalid crafting recipe");
                const auto& result=r.at("result");
                CraftRecipe recipe{static_cast<std::uint32_t>(readNumber(r,"id",UINT32_MAX)),r.at("name").get<std::string>(),
                    {key(result),static_cast<std::uint16_t>(readNumber(result,"level",255)),static_cast<std::uint32_t>(readNumber(result,"count",UINT32_MAX))},
                    readNumber(r,"gold",INT64_MAX),static_cast<std::uint16_t>(readNumber(r,"chance_bp",10000)),{}};
                for(const auto& i:r.at("ingredients"))
                {if(!i.is_object() || i.size()!=4)return std::unexpected("invalid crafting ingredient");recipe.ingredients.push_back({key(i),static_cast<std::uint16_t>(readNumber(i,"level",255)),static_cast<std::uint32_t>(readNumber(i,"count",UINT32_MAX))});}
                if(json.at("version")==2) {
                    recipe.gaya=static_cast<std::uint32_t>(readNumber(r,"gaya",MaxNativeGayaBalance));
                    if(!r.at("copy_source").is_null()) {
                        const auto& copy=r.at("copy_source");if(!copy.is_object() || copy.size()!=4)throw std::runtime_error("invalid copy source");
                        recipe.copySource=CraftIngredient{key(copy),static_cast<std::uint16_t>(readNumber(copy,"level",255)),static_cast<std::uint32_t>(readNumber(copy,"count",1))};
                    }
                }
                CraftWorkshop check{shop.npcRace,shop.name,{recipe}};if(!ValidateCraftWorkshops(catalog,std::span(&check,1)))return std::unexpected("Invalid native crafting recipe "+std::to_string(recipe.id)+" at NPC "+std::to_string(shop.npcRace));
                shop.recipes.push_back(std::move(recipe));
            }
            definitions.push_back(std::move(shop));
        }
        if(!ValidateCraftWorkshops(catalog,definitions))return std::unexpected("crafting recipes do not match the item catalog");
        return definitions;
    }catch(const std::exception& e){return std::unexpected(e.what());}
}
}
