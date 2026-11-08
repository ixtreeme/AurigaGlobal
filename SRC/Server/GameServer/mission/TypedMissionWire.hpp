#pragma once
#include "TypedMissionItems.hpp"
namespace auriga::mission
{
enum class TypedMissionStatus:uint8_t {Ready,Applied,Pending,Denied,Invalid,Stale,Failed,Closed,Loading};
struct TypedMissionNodeView {std::string id,title,subject;uint8_t kind=0,state=0,event=0;uint64_t counter=0,goal=0;Timestamp deadline=0;std::vector<std::string> choices;};
struct TypedMissionView
{
    std::string code,title,summary;uint32_t version=0,npcRace=0;uint16_t minimumLevel=0;uint8_t state=0;uint64_t instance=0;bool repeatable=false;
    std::vector<TypedMissionNodeView> nodes;std::optional<item::ItemMission> reward;
};
std::expected<std::vector<std::byte>,item::TypedInventoryWireError> EncodeTypedMissions(const item::ItemCatalog&,
    TypedMissionStatus,uint64_t nonce,uint64_t request,uint32_t npc,std::span<const TypedMissionView>,
    const item::persistence::LoadedItemOwner*,std::string_view message={});
}
