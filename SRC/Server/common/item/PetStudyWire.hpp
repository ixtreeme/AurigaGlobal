#pragma once
#include "PersistentItems.hpp"
namespace auriga::item {
inline constexpr std::uint8_t PetStudyRuntimeHeader=253;
struct PetStudyRuntimeView {
    std::uint8_t status=0;
    std::uint64_t item=0,quote=0,cost=0,gold=0;
    std::optional<ItemKey> target;
    const PersistentItem* seal=nullptr;
    std::uint64_t book=0,readyAtMs=0,elapsedMs=0;
    std::uint32_t skill=0,bypassTokens=0;
    std::uint8_t slot=0;
    std::uint16_t nextRank=0,chanceBp=0;
    std::vector<std::pair<std::uint64_t,std::uint8_t>> options;
    std::array<PetSkillSlot,4> slots{};
    bool usesBypass=false,withStudy=true;
};
// The ordinary GC253v1 details or v7 study response, shared by the running game and the
// SQL-backed integration worker. No UI/client decision enters this encoder.
std::expected<std::vector<std::uint8_t>,PetLifecycleError> EncodePetStudyRuntime(const ItemCatalog&,const PetStudyRuntimeView&);
}
