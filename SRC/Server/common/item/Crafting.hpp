#pragma once
#include "PersistentItems.hpp"
#include "TypedItemDetails.hpp"

namespace auriga::item
{
struct CraftIngredient { ItemKey key; std::uint16_t level=0; std::uint32_t count=0; };
struct CraftRecipe
{
    std::uint32_t id=0;std::string name;ItemGrant result;
    std::uint64_t gold=0;std::uint16_t chance=10000;
    std::vector<CraftIngredient> ingredients;
};
struct CraftWorkshop { std::uint32_t npcRace=0;std::string name;std::vector<CraftRecipe> recipes; };
inline constexpr std::size_t MaxCraftWorkshops=256,MaxCraftRecipes=128,MaxCraftIngredients=16;
inline constexpr std::uint32_t MaxCraftBatch=100;
enum class CraftError { Invalid, NoMaterials, NoMoney, NoRoom, Unavailable, Limit };
enum class CraftStatus : std::uint8_t { Closed, Ready, Crafted, Busy, Invalid, Denied, NoMaterials, NoMoney, NoRoom, Failed, Stale, Limit, Pending };
struct CraftPlan { ItemCommit commit;std::uint32_t successes=0; };
bool CraftIngredientUsable(const PersistentItem&,ItemOwner);
std::expected<void,CraftError> ValidateCraftWorkshops(const ItemCatalog&,std::span<const CraftWorkshop>);
// One roll in [0,9999] per attempt, supplied by the server and recorded in the
// receipt. All attempts, ingredients, gold and produced units commit together.
std::expected<CraftPlan,CraftError> MakeCraftCommit(const ItemCatalog&,std::array<std::uint8_t,16>,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,std::span<const ItemWallet>,
    const CraftRecipe&,std::uint32_t attempts,std::span<const std::uint16_t> rolls,std::span<const ItemInstanceId> ids,std::uint64_t nowUtcMs=0);
// GC230 v1: header,size:u32,version,status,npc:u32,session:u64,request:u64,
// recipe:u32,attempts:u32,successes:u32,name:text16,recipes:u16
// {id:u32,name:text16,result:key,level:u16,units:u32,gold:u64,chance:u16,
// ingredients:u8 {key,level:u16,count:u32}},detailsSize:u32,GC227.
// Closed response is the 47-byte envelope with empty name/recipes/details.
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeCraftWorkshop(const ItemCatalog&,const CraftWorkshop*,
    std::uint32_t npc,std::uint64_t session,std::uint64_t request,CraftStatus,
    std::uint32_t recipe=0,std::uint32_t attempts=0,std::uint32_t successes=0);
}
