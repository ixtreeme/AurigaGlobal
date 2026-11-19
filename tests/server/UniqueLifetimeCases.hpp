void UniqueLifetimes()
{
    for(unsigned mode=2;mode<=4;++mode) {
        auto source=f::Catalog();source.usePolicies[0].useDuringCombat=false;const auto timed=ItemKey::Create(ItemCategory::Accessory,90).value(),permanent=ItemKey::Create(ItemCategory::Accessory,91).value();
        for(auto key:{timed,permanent}) {auto base=source.items[2];base.key=key;base.contentCode="unique."+std::to_string(key.LocalId());base.refinementProfile=1;source.items.push_back(base);source.accessories.push_back({key,EquipmentSlot::Unique,1,{}});}
        ItemDefinitionRules rule{timed};rule.clock=static_cast<CostumeClock>(mode);rule.durationMs=10000;source.definitionRules.push_back(rule);
        const auto c=BuildItemCatalog(source).value();const ItemOwner owner{ItemOwnerKind::Character,7};const ItemOwnerFence fence{owner,1,1};
        const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,2},ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},28,1,1}};
        std::vector<PersistentItem> items;unsigned serial=0;
        const auto grant=[&](ItemKey key){auto id=ItemInstanceId::Create(200+serial++).value();auto r=MakeGrantCommit(*c,{8,static_cast<std::uint8_t>(serial)},fence,layouts,items,ItemGrant{key,0,1,{},{},{},1000},std::array{id});Check(r.has_value(),"unique clock grant");items.push_back(r->items[0].after);return id;};
        const auto apply=[&](const ItemCommit& r){for(const auto& ch:r.items)*std::ranges::find(items,ch.after.id,&PersistentItem::id)=ch.after;};
        const auto first=grant(permanent),second=grant(timed),third=grant(permanent);EquipContext who{255,0,0,2000};who.inCombat=true;
        apply(MakeEquipCommit(*c,{9,1},fence,layouts,items,first,who).value());apply(MakeEquipCommit(*c,{9,2},fence,layouts,items,second,who).value());
        Check(items[0].placement->anchor==22 && items[1].placement->anchor==23,"timed unique uses free alternate slot beside permanent unique");
        const auto bytes=EncodeTypedInventory(*c,layouts,{},items).value();Check(std::to_integer<unsigned>(bytes[5])==25,"unique clock wire version 25");const auto view=DecodeTypedInventory(bytes,owner);Check(view && view->items[1].costume==items[1].costume,"unique clock native wire");
        for(std::size_t size=5;size<bytes.size();++size)Check(!DecodeTypedInventory(std::span{bytes}.first(size),owner),"truncated unique clock rejected");
        auto old=bytes;old[5]=std::byte{24};Check(!DecodeTypedInventory(old,owner),"v24 cannot disguise unique clock as stole");
        const std::array elapsed{CostumeElapsed{second.Value(),mode==4 ? 400u : 0u}};who.costumeElapsed=elapsed;who.nowUtcMs=2400;
        apply(MakeUnequipCommit(*c,{9,3},fence,layouts,items,second,{ItemStorageKind::Inventory,1,10},who).value());
        if(mode==4)Check(items[1].costume->remainingMs==9600,"unique unequip settles exact wear debt");
        who.costumeElapsed={};who.nowUtcMs=3000;apply(MakeEquipCommit(*c,{9,4},fence,layouts,items,third,who).value());
        apply(MakeEquipCommit(*c,{9,5},fence,layouts,items,second,who).value());Check(items[1].placement->anchor==22 && items[0].placement==ItemPlacement{ItemStorageKind::Inventory,1,10},"timed unique swaps permanent occupant with both slots full");
        auto expire=MakeCostumeClockCommit(*c,{9,6},fence,layouts,items,second,50000,mode==4 ? 9600 : 0).value();auto forged=expire;forged.items[0].after.costume->deadlineUtcMs++;Check(!ValidateItemCommit(*c,forged),"unique clock receipt resists mutation");apply(expire);
        Check(items[1].destroyed && !items[1].placement && !items[0].costume && !items[2].costume,"expiry preserves permanent uniques without clock state");
    }
}
