#include <item/SpecialBonusRuntime.hpp>

// Real converted definitions, asynchronous game item worker, receipts and SQL reloads.
void StoleCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& path,const std::vector<BulkSource>& sources,
    auriga::item::ItemKey permanentKey,auriga::item::ItemKey rerollKey)
{
    using namespace auriga::item;CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"stole owned SQL connection");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-stole-worker").has_value(),"real stole item worker starts");
    unsigned creation=0,firstUse=0,permanents=0;constexpr std::uint64_t issued=1794697200000ULL;
    for(unsigned n=0;n<sources.size();++n){
        const auto key=sources[n].grant.key;const auto clock=FindWearableClock(*catalog,key).value();
        Check(clock.clock>=CostumeClock::Permanent && clock.clock<=CostumeClock::SinceFirstUse,"captured source clock kind");
        if(clock.clock==CostumeClock::SinceCreation)++creation;else if(clock.clock==CostumeClock::SinceFirstUse)++firstUse;else ++permanents;
        const ItemOwner owner{ItemOwnerKind::Character,8800u+n};
        const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4},ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},28,1,1}};
        Check(persistence::ProvisionOwner(sql,*catalog,owner,layouts,{}).has_value(),"stole owner provisioned");
        auto state=persistence::LoadCurrentItemOwner(sql,*catalog,owner).value();unsigned serial=0;bool replay=false;
        const auto op=[&](){++serial;return std::array<std::uint8_t,16>{0xFC,static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(serial),static_cast<std::uint8_t>(serial>>8)};};
        const auto apply=[&](const ItemCommit& commit){std::optional<runtime::CommitOutcome> got;Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"stole operation queued");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"stole receipt persisted and reloaded");replay=got->write->replay;state=*got->state;};
        const auto grant=[&](ItemKey item,unsigned count=1){const auto ids=runtime::PeekInstanceIds(count);runtime::SpendInstanceIds(count);ItemGrant spec{item,0,count};spec.createdAtUtcMs=issued;auto plan=MakeGrantCommit(*catalog,op(),state.fence,state.storages,state.items,spec,ids);Check(plan.has_value(),"real source stole grant");apply(*plan);return ids.front();};
        const auto find=[&](ItemInstanceId id)->const PersistentItem&{const auto it=std::ranges::find(state.items,id,&PersistentItem::id);Check(it!=state.items.end(),"expected stole instance");return *it;};
        const auto absent=[&](ItemInstanceId id){return std::ranges::find(state.items,id,&PersistentItem::id)==state.items.end();};
        const auto relog=[&](){const auto before=state;const auto generation=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,generation).value();Check(state.items==before.items,"stole clock and rolled bonuses survive a new SQL session exactly");};
        const auto output=[&](const std::string& phase){const auto write=[&](const char* suffix,const std::vector<std::byte>& bytes){std::ofstream f(path+"."+std::to_string(n)+"."+phase+suffix);for(auto b:bytes){const auto v=std::to_integer<unsigned>(b);f<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}f<<'\n';Check(f.good(),"stole actual packet exported");};const auto wire=EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items);Check(wire && DecodeTypedInventory(*wire,owner).has_value(),"stole native inventory roundtrip");write(".inventory.hex",*wire);write(".details.hex",EncodeTypedItemDetails(*catalog,state.items).value());CostumeAppearance look{313,state.fence.revision};for(const auto& i:state.items)if(i.regalia && i.placement && i.placement->kind==ItemStorageKind::Equipment){const auto* row=catalog->Find(i.key);if(row->base.appearanceProfile)look.models[5]=catalog->FindAppearance(*row->base.appearanceProfile)->modelAsset;}std::vector<std::byte> appearance;for(auto b:EncodeCostumeAppearance(look).value())appearance.push_back(std::byte(b));write(".appearance.hex",appearance);};
        const auto id=grant(key),permanent=grant(permanentKey),reroll=grant(rerollKey,2);
        Check(find(id).costume.has_value()==(clock.clock!=CostumeClock::Permanent) && find(id).regalia && !find(permanent).costume,"only timed C19 stoles own a clock");
        Check(!find(id).costume || find(id).costume->started==(clock.clock==CostumeClock::SinceCreation),"source issuance clock semantics");output("bag");
        const auto* use=catalog->FindUsePolicy(catalog->Find(key)->base.usePolicy);unsigned job=0,sex=0;while(!(use->allowedClasses&(1u<<job)))++job;while(!(use->allowedSexes&(1u<<sex)))++sex;
        EquipContext actor{255,static_cast<std::uint8_t>(job),static_cast<std::uint8_t>(sex),issued+5000};auto badActor=actor;badActor.characterClass=4;
        Check(!MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,id,badActor),"unadmitted actor cannot start a stole clock");
        auto forged=find(id);if(forged.costume)forged.costume.reset();else forged.costume=CostumeLifetimeState{};Check(!ValidatePersistentItem(*catalog,forged),"stole cannot forge its lifetime state");
        forged=find(permanent);forged.costume=CostumeLifetimeState{};Check(!ValidatePersistentItem(*catalog,forged),"permanent stole cannot smuggle a lifetime");
        const auto equipped=MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,id,actor).value();apply(equipped);
        const auto lifetime=find(id).costume;const auto expectedDeadline=(clock.clock==CostumeClock::SinceCreation ? issued : actor.nowUtcMs)+clock.durationMs;
        Check((!lifetime || lifetime->started && lifetime->deadlineUtcMs==expectedDeadline) && find(id).placement->anchor==11,"source start time and independent stole slot");
        auto expected=BuildEquipmentStats(*catalog,owner,state.items).value();for(const auto& b:find(id).regalia->bonuses)Check(b && expected.bonuses[static_cast<unsigned>(b->bonus)]==b->valueScaled,"six actual stole bonuses contribute");output("equipped");
        const auto worn=state.items;apply(equipped);Check(replay && state.items==worn,"equip replay preserves deadline and bonuses");relog();output("relogged");
        apply(MakeUnequipCommit(*catalog,op(),state.fence,state.storages,state.items,id,{ItemStorageKind::Inventory,1,0},actor).value());
        Check(find(id).costume==lifetime,"unequip never pauses or refills wall time");output("unequipped");
        const auto rolled=MakeSpecialBonusCommit(*catalog,op(),state.fence,state.storages,state.items,reroll,id,SpecialBonusAction::Service,0,0,actor,[](auto maximum){return std::optional<std::uint32_t>{maximum};}).value();apply(rolled);
        Check(find(id).costume==lifetime && find(reroll).count==1,"stole reroll consumes one material and preserves clock");const auto afterRoll=state.items;apply(rolled);Check(replay && state.items==afterRoll,"reroll retry cannot redraw or spend twice");relog();output("rerolled");
        apply(MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,id,actor).value());
        apply(MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,permanent,actor).value());
        Check(find(id).placement->kind==ItemStorageKind::Inventory && find(id).costume==lifetime && !find(permanent).costume,"permanent stole safely displaces a timed one");output("permanent_swap");
        apply(MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,id,actor).value());Check(find(id).costume==lifetime && find(permanent).placement->kind==ItemStorageKind::Inventory,"timed stole safely displaces permanent one");output("timed_swap");
        if(clock.clock==CostumeClock::Permanent)continue;
        Check(!MakeCostumeClockCommit(*catalog,op(),state.fence,state.storages,state.items,id,expectedDeadline-1,0),"deadline minus one millisecond remains live");
        actor.nowUtcMs=expectedDeadline;Check(!MakeSpecialBonusCommit(*catalog,op(),state.fence,state.storages,state.items,reroll,id,SpecialBonusAction::Service,0,0,actor,[](auto){return std::optional<std::uint32_t>{0};}),"expired stole cannot reroll or consume material");
        const auto expiry=MakeCostumeClockCommit(*catalog,op(),state.fence,state.storages,state.items,id,expectedDeadline,0).value();
        auto corrupt=expiry;corrupt.items.front().after.costume->deadlineUtcMs++;Check(!ValidateItemCommit(*catalog,corrupt),"forged expiry receipt is rejected");apply(expiry);Check(absent(id),"deadline destroys actual stole instance");
        Check(std::ranges::all_of(BuildEquipmentStats(*catalog,owner,state.items)->bonuses,[](auto v){return v==0;}),"expiry removes every equipped bonus");output("expired");
        const auto dead=state.items;apply(expiry);Check(replay && state.items==dead,"expiry receipt replays without resurrection");relog();output("expired_relogged");
        const auto bagId=grant(key);actor.nowUtcMs=issued+5000;
        if(clock.clock==CostumeClock::SinceFirstUse){apply(MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,bagId,actor).value());apply(MakeUnequipCommit(*catalog,op(),state.fence,state.storages,state.items,bagId,{ItemStorageKind::Inventory,1,0},actor).value());}
        output("bag_expiry_ready");const auto bagDeadline=find(bagId).costume->deadlineUtcMs;relog();
        apply(MakeCostumeClockCommit(*catalog,op(),state.fence,state.storages,state.items,bagId,bagDeadline,0).value());Check(absent(bagId),"wall clock expires in the bag after offline session");relog();output("bag_expired");
    }
    store.Stop();std::cout<<"SOURCE_STOLE_WORKER=SUCCESS sources="<<sources.size()<<" creation="<<creation<<" first_use="<<firstUse<<" permanent="<<permanents<<" checks="<<checks<<std::endl;
}
