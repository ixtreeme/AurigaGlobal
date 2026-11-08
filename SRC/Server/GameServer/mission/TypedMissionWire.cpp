#include "TypedMissionWire.hpp"
namespace auriga::mission
{
std::expected<std::vector<std::byte>,item::TypedInventoryWireError> EncodeTypedMissions(const item::ItemCatalog& c,
    TypedMissionStatus status,uint64_t nonce,uint64_t request,uint32_t npc,std::span<const TypedMissionView> missions,
    const item::persistence::LoadedItemOwner* owner,std::string_view message)
{
    using E=item::TypedInventoryWireError;if(missions.size()>32 || message.size()>512)return std::unexpected(E::BadValue);
    std::vector<std::byte> out;const auto put=[&](uint64_t v,unsigned n){for(unsigned j=0;j<n;j++)out.push_back(std::byte(v>>(j*8)));};
    const auto text=[&](std::string_view s){put(s.size(),2);for(unsigned char b:s)out.push_back(std::byte(b));};
    put(202,1);put(0,4);put(1,1);put(uint8_t(status),1);put(nonce,8);put(request,8);put(npc,4);put(missions.size(),2);std::vector<item::ItemGrant> details;
    for(const auto& m:missions){text(m.code);put(m.version,4);text(m.title);text(m.summary);put(m.npcRace,4);put(m.minimumLevel,2);put(m.state,1);put(m.instance,8);put(m.repeatable,1);put(m.nodes.size(),2);
        for(const auto& n:m.nodes){text(n.id);text(n.title);text(n.subject);put(n.kind,1);put(n.state,1);put(n.event,1);put(n.counter,8);put(n.goal,8);put(n.deadline,8);put(n.choices.size(),1);for(const auto& choice:n.choices)text(choice);}
        for(unsigned which=0;which<3;which++){const std::vector<item::MissionItemAmount>* list=!m.reward?nullptr:which==0?&m.reward->requirements:which==1?&m.reward->costs:&m.reward->rewards;put(list?list->size():0,1);if(list)for(const auto& amount:*list){put(unsigned(amount.key.Category()),2);put(amount.key.LocalId(),4);put(amount.level,2);put(amount.count,4);put(owner?item::CountMissionItems(owner->items,owner->fence.owner,amount.key,amount.level):0,8);details.push_back({amount.key,amount.level,1});}}
        put(m.reward?m.reward->goldCost:0,8);put(m.reward?m.reward->goldReward:0,8);
    }
    const auto wire=item::EncodeTypedItemDetails(c,{},false,details);if(!wire)return std::unexpected(wire.error());put(wire->size(),4);out.insert(out.end(),wire->begin(),wire->end());text(message);
    if(out.size()>4*1024*1024)return std::unexpected(E::BadValue);for(unsigned j=0;j<4;j++)out[1+j]=std::byte(out.size()>>(8*j));return out;
}
}
