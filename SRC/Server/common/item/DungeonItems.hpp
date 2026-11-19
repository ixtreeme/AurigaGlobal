#pragma once
#include "ItemMissions.hpp"
namespace auriga::item {
inline constexpr std::string_view RuneFragmentCode="dungeon.rune.fragment",RuneFloorKeyCode="dungeon.rune.floor_key",OrcKeyCode="dungeon.orcs.key";
enum class RuneKeyAction : std::uint8_t { Exchange=1,Advance,Offer };
struct RuneKeyFacts {
    bool admitted=false,dungeon=false,leader=false,pedestal=false;
    std::uint8_t floor=0,type=0;
    std::uint16_t level=0;
    std::uint8_t characterClass=0,sex=0;
    bool inCombat=false;
};
inline constexpr std::uint16_t RuneKeyOutcome=0x2B01;
enum class DungeonUseRole : std::uint8_t { None,CastleKey1,CastleKey2,CastleKey3,CastleKey4,CastleKey5,CastleTile,CathedralCross,CathedralSeal,CathedralBlood,VikingFire,VikingRune,VikingReset };
struct DungeonUseBinding {DungeonUseRole role;std::string_view code;};
inline constexpr std::array<DungeonUseBinding,12> DungeonUseBindings{{
    DungeonUseBinding{DungeonUseRole::CastleKey1,"material.level"},{DungeonUseRole::CastleKey2,"material.sotet_pafrany"},
    {DungeonUseRole::CastleKey3,"material.chaegirab_gyuruje"},{DungeonUseRole::CastleKey4,"material.metin_kristaly"},
    {DungeonUseRole::CastleKey5,"material.tisztitott_mag"},{DungeonUseRole::CastleTile,"material.ork_amulett"},
    {DungeonUseRole::CathedralCross,"catalog.3.cathedral_cross_e53168e5fbdd"},{DungeonUseRole::CathedralSeal,"catalog.3.vampire_amulet_1832270ffa5c"},
    {DungeonUseRole::CathedralBlood,"catalog.3.bottle_with_blood_b1073b7d0c34"},{DungeonUseRole::VikingFire,"catalog.3.frost_fire_ee5f9aff91f0"},
    {DungeonUseRole::VikingRune,"catalog.3.frostbite_runestone_b47aac06e11a"},{DungeonUseRole::VikingReset,"catalog.3.scroll_of_rebirth_bcfb30ea408a"}}};
DungeonUseRole DungeonItemUseRole(const ItemCatalog&,ItemKey);
struct DungeonUseFacts {EquipContext character;bool admitted=false,stageReady=false;};
std::expected<ItemCommit,ItemMissionError> MakeDungeonUseCommit(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId,const DungeonUseFacts&);
std::optional<ItemKey> DungeonItemKey(const ItemCatalog&,std::string_view);
bool RuneKeyItem(const ItemCatalog&,ItemKey);
std::expected<RuneKeyAction,ItemMissionError> QuoteRuneKey(const ItemCatalog&,const PersistentItem&,ItemOwner,const RuneKeyFacts&);
std::expected<ItemCommit,ItemMissionError> MakeRuneKeyCommit(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId,const RuneKeyFacts&,std::span<const ItemInstanceId>,std::uint64_t now=0);
std::expected<ItemCommit,ItemMissionError> MakeDungeonCleanupCommit(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemKey,std::uint32_t maximum=UINT32_MAX);
}
