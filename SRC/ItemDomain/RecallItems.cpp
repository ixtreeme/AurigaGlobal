#include <item/RecallItems.hpp>
#include <item/ItemCatalog.hpp>

namespace auriga::item {
namespace {
BuiltinQuestItem Handler(const ItemCatalog& c,ItemKey key){const auto* entry=c.Find(key);const auto* row=entry ? std::get_if<ItemQuestRow>(&entry->category) : nullptr;return row ? BuiltinQuestHandler(row->groupCode) : BuiltinQuestItem::None;}
}
RecallMode RecallItemMode(const ItemCatalog& c,ItemKey key){
    const auto kind=Handler(c,key);
    if(kind==BuiltinQuestItem::RecallTown || kind==BuiltinQuestItem::RecallTownRestricted)return RecallMode::Town;
    if(kind==BuiltinQuestItem::RecallMemory || kind==BuiltinQuestItem::RecallMemoryRestricted)return RecallMode::Memory;
    return RecallMode::None;
}
bool RecallItemSummonGates(const ItemCatalog& c,ItemKey key){const auto kind=Handler(c,key);return kind==BuiltinQuestItem::RecallTownRestricted || kind==BuiltinQuestItem::RecallMemoryRestricted;}
std::vector<std::uint8_t> EncodeRecallItemState(const RecallItemState& state){
    std::vector<std::uint8_t> out{'R','C','L',1,static_cast<std::uint8_t>(state.destination.has_value())};
    if(state.destination){const auto& p=*state.destination;if(p.map<=0 || p.map>=10000 || p.map==66 || p.map==216)return {};
        for(const auto value:{p.map,p.x,p.y})for(unsigned n=0;n<4;++n)out.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value)>>(8*n)));
    }
    return out;
}
std::optional<RecallItemState> DecodeRecallItemState(std::span<const std::uint8_t> bytes){
    if(bytes.size()<5 || bytes[0]!='R' || bytes[1]!='C' || bytes[2]!='L' || bytes[3]!=1 || bytes[4]>1 || bytes.size()!=(bytes[4] ? 17u : 5u))return {};
    RecallItemState state;
    if(bytes[4]){const auto read=[&](unsigned at){std::uint32_t v=0;for(unsigned n=0;n<4;++n)v|=std::uint32_t{bytes[at+n]}<<(8*n);return static_cast<std::int32_t>(v);};const RecallPosition p{read(5),read(9),read(13)};if(p.map<=0 || p.map>=10000 || p.map==66 || p.map==216)return {};state.destination=p;}
    return state;
}
}
