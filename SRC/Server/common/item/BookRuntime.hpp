#pragma once
#include "PersistentItems.hpp"
#include "TypedInventoryWire.hpp"
namespace auriga::item
{
// Main/horse/passive skills use the runtime's absolute 40-level mastery
// ladder; their proto bMaxLevel is also a power-formula parameter, not its
// learning cap. Secondary (type 0) skills additionally use their declared cap.
constexpr std::uint8_t BookRuntimeSkillCap(std::uint32_t type,std::uint8_t declared) noexcept
{return type==0 && declared<MaxBookSkillLevel ? declared : MaxBookSkillLevel;}
const SkillBookProfile* BookItemProfile(const ItemCatalog&,ItemKey);
std::expected<SkillBookAdmission,SkillBookError> BookRuntimeAdmission(const ItemCatalog&,const PersistentItem&,const BookLearning&,const BookCharacter&,SkillBookAdmission facts,std::uint64_t now);
struct BookRuntimePlan{ItemCommit commit;SkillBookPlan result;};
std::expected<BookRuntimePlan,SkillBookError> MakeBookRuntimeCommit(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,const BookLearning&,const BookCharacter&,ItemInstanceId,SkillBookAdmission,std::uint64_t now,SkillBookDecisions);
struct BookRuntimeView
{
    std::uint8_t status=0,error=255;std::uint64_t control=0,preview=0,now=0;
    const PersistentItem* source=nullptr;const BookLearning* learning=nullptr;
    BookCharacter character;SkillBookAdmission actor;std::string skillName;
    const SkillBookPlan* result=nullptr;SkillBookDecisions decisions;
};
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeBookRuntime(const ItemCatalog&,const BookRuntimeView&);
}
