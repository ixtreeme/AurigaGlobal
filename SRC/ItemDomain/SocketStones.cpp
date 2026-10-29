#include <item/SocketStones.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"
#include <algorithm>
#include <limits>

namespace auriga::item
{
bool ValidSocketStone(const ItemSocketStoneRow& row) noexcept
{
    if(row.key.Category()!=ItemCategory::SocketStone)return false;
    if(row.kind==SocketStoneKind::BrokenShard)
        return !row.allowedTargets && !row.minimumSocketGrade && row.primaryGroup.empty() && row.secondaryGroup.empty();
    if(row.kind!=SocketStoneKind::Insertable || !row.allowedTargets || (row.allowedTargets&~3u) ||
       row.minimumSocketGrade<1 || row.minimumSocketGrade>2)return false;
    if(!row.secondaryGroup.empty() && row.secondaryGroup==row.primaryGroup)return false;
    return (row.primaryGroup.empty() || detail::ContentCode(row.primaryGroup)) &&
           (row.secondaryGroup.empty() || detail::ContentCode(row.secondaryGroup));
}
namespace
{
unsigned TargetMask(const ItemCatalogEntry& entry)
{
    if(std::holds_alternative<ItemWeaponRow>(entry.category))return 1;
    const auto* armor=std::get_if<ItemArmorRow>(&entry.category);
    return armor && armor->slot==EquipmentSlot::Body ? 2 : 0;
}
bool Conflicts(const ItemSocketStoneRow& a,const ItemSocketStoneRow& b)
{
    for(const auto* x:{&a.primaryGroup,&a.secondaryGroup})
        for(const auto* y:{&b.primaryGroup,&b.secondaryGroup})
            if(!x->empty() && *x==*y)return true;
    return false;
}
bool Snapshot(const StoneItemSnapshot& item,std::uint64_t owner)
{
    return item.instanceId && item.revision && item.revision!=std::numeric_limits<std::uint64_t>::max() &&
        item.ownerId==owner && !item.locked && !item.equipped && !item.exchanging;
}
}
std::expected<void,SocketStoneError> ValidateEquipmentStoneSockets(const ItemCatalog& catalog,ItemKey targetKey,
    const std::array<StoneSocketSlot,EquipmentStoneSockets>& sockets)
{
    using E=SocketStoneError;
    const auto* target=catalog.Find(targetKey);
    const auto mask=target ? TargetMask(*target) : 0;
    if(!mask)return std::unexpected(E::IncompatibleTarget);
    std::array<const ItemSocketStoneRow*,EquipmentStoneSockets> installed{};
    for(std::size_t i=0;i<sockets.size();++i)
    {
        const auto& socket=sockets[i];
        if(socket.state==StoneSocketState::Closed)
        {
            if(socket.grade || socket.stone || socket.refinementLevel)return std::unexpected(E::InvalidSocketState);
            continue;
        }
        if(socket.grade<1 || socket.grade>2)return std::unexpected(E::InvalidSocketState);
        if(socket.state==StoneSocketState::Occupied)
        {
            if(!socket.stone)return std::unexpected(E::InvalidSocketState);
            const auto* existing=catalog.Find(*socket.stone);
            const auto* row=existing ? std::get_if<ItemSocketStoneRow>(&existing->category) : nullptr;
            if(!row || row->kind!=SocketStoneKind::Insertable || !(row->allowedTargets&mask) ||
               row->minimumSocketGrade>socket.grade ||
               !catalog.FindRefinement(existing->base.refinementProfile)->FindLevel(socket.refinementLevel))
                return std::unexpected(E::InvalidSocketState);
            for(std::size_t j=0;j<i;++j)
                if(installed[j] && Conflicts(*row,*installed[j]))return std::unexpected(E::InvalidSocketState);
            installed[i]=row;
        }
        else
        {
            if(socket.stone || socket.refinementLevel ||
               (socket.state!=StoneSocketState::Empty && socket.state!=StoneSocketState::Broken))
                return std::unexpected(E::InvalidSocketState);
        }
    }
    return {};
}
std::expected<SocketStoneInsertPlan,SocketStoneError> PrepareSocketStoneInsert(
    const ItemCatalog& catalog,const SocketStoneInsertRequest& request)
{
    using E=SocketStoneError;
    if(!request.admittedByServer)return std::unexpected(E::Forbidden);
    if(!request.actorId || !request.rulesetRevision || std::ranges::all_of(request.operationId,[](auto b){return b==0;}))
        return std::unexpected(E::InvalidOperation);
    if(!Snapshot(request.stone,request.actorId) || !Snapshot(request.target,request.actorId) ||
       request.stone.instanceId==request.target.instanceId)return std::unexpected(E::InvalidSnapshot);
    const auto* stone=catalog.Find(request.stone.item);const auto* target=catalog.Find(request.target.item);
    if(!stone || !target)return std::unexpected(E::MissingItem);
    const auto* definition=std::get_if<ItemSocketStoneRow>(&stone->category);
    if(!definition || definition->kind!=SocketStoneKind::Insertable)return std::unexpected(E::NotStone);
    if(stone->base.retired || target->base.retired)return std::unexpected(E::RetiredItem);
    if(!request.stone.count || request.stone.count>stone->base.maxStack || request.target.count!=1)
        return std::unexpected(E::InvalidStack);
    if(!catalog.FindRefinement(stone->base.refinementProfile)->FindLevel(request.stone.refinementLevel) ||
       !catalog.FindRefinement(target->base.refinementProfile)->FindLevel(request.target.refinementLevel) ||
       request.characterLevel<stone->base.minimumLevel)return std::unexpected(E::InvalidLevel);
    if(request.inCombat && !catalog.FindUsePolicy(stone->base.usePolicy)->useDuringCombat)return std::unexpected(E::Forbidden);
    const auto mask=TargetMask(*target);
    if(!mask || !(definition->allowedTargets&mask))return std::unexpected(E::IncompatibleTarget);
    if(auto valid=ValidateEquipmentStoneSockets(catalog,request.target.item,request.sockets);!valid)
        return std::unexpected(valid.error());
    std::optional<std::uint8_t> selected;
    for(std::size_t i=0;i<request.sockets.size();++i)
    {
        const auto& socket=request.sockets[i];
        if(socket.state==StoneSocketState::Occupied)
        {
            const auto& row=std::get<ItemSocketStoneRow>(catalog.Find(*socket.stone)->category);
            if(Conflicts(*definition,row))return std::unexpected(E::ConflictingGroup);
        }
        else if(!selected && socket.state==StoneSocketState::Empty && socket.grade>=definition->minimumSocketGrade)
            selected=static_cast<std::uint8_t>(i);
    }
    if(!selected)return std::unexpected(E::NoEmptySocket);
    if(!request.chanceBp || *request.chanceBp>10000)return std::unexpected(E::InvalidDecision);
    const auto chance=*request.chanceBp;
    if((chance==0 || chance==10000) ? request.recordedRoll!=0 : (request.recordedRoll<1 || request.recordedRoll>10000))
        return std::unexpected(E::InvalidDecision);
    const bool success=chance==10000 || (chance && request.recordedRoll<=chance);
    StoneSocketSlot after{success ? StoneSocketState::Occupied : StoneSocketState::Broken,
                         request.sockets[*selected].grade,success ? std::optional{request.stone.item} : std::nullopt,
                         success ? request.stone.refinementLevel : std::uint16_t{0}};
    return SocketStoneInsertPlan{request,catalog.Revision(),catalog.ContentHash(),*selected,std::move(after),1,success};
}
} // namespace auriga::item
