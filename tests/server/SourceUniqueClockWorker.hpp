// All source unique lifetimes on the asynchronous game item worker and SQL receipts.
void UniqueClockCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& path,const std::vector<BulkSource>& sources,auriga::item::ItemKey permanent)
{
    using namespace auriga::item;CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"unique owned SQL connection");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-unique-clock-worker").has_value(),"unique actual item worker starts");
    const auto issued=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    std::array<unsigned,5> kinds{};
    for(unsigned n=0;n<sources.size();++n) {
        const auto key=sources[n].grant.key;const auto clock=FindWearableClock(*catalog,key).value();++kinds[static_cast<unsigned>(clock.clock)];
        Check(key.Category()==ItemCategory::Accessory && clock.clock!=CostumeClock::Permanent,"finite source unique accessory");
        const ItemOwner owner{ItemOwnerKind::Character,11000u+n};const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4},ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},28,1,1}};
        Check(persistence::ProvisionOwner(sql,*catalog,owner,layouts,{}).has_value(),"unique owner provisioned");auto state=persistence::LoadCurrentItemOwner(sql,*catalog,owner).value();unsigned serial=0;bool replay=false;
        const auto op=[&](){++serial;return std::array<std::uint8_t,16>{0xD1,static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(serial),static_cast<std::uint8_t>(serial>>8)};};
        const auto apply=[&](const ItemCommit& commit){std::optional<runtime::CommitOutcome> got;Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"unique queued");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"unique saved and reloaded");replay=got->write->replay;state=*got->state;};
        const auto find=[&](ItemInstanceId id)->const PersistentItem&{const auto it=std::ranges::find(state.items,id,&PersistentItem::id);Check(it!=state.items.end(),"unique expected instance");return *it;};
        const auto relog=[&](){const auto saved=state.items;const auto generation=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,generation).value();Check(state.items==saved,"unique exact state survives new SQL session");};
        const auto grant=[&](ItemKey k){const auto ids=runtime::PeekInstanceIds(1);runtime::SpendInstanceIds(1);ItemGrant g{k};g.createdAtUtcMs=issued;apply(MakeGrantCommit(*catalog,op(),state.fence,state.storages,state.items,g,ids).value());return ids[0];};
        const auto output=[&](const char* phase){for(unsigned kind=0;kind<2;++kind){auto bytes=kind ? EncodeTypedItemDetails(*catalog,state.items).value() : EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items).value();std::ofstream f(path+"."+std::to_string(n)+"."+phase+(kind ? ".details.hex" : ".inventory.hex"));for(auto b:bytes){auto v=std::to_integer<unsigned>(b);f<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}f<<'\n';Check(f.good(),"unique packet exported");}};
        const auto id=grant(key);Check(find(id).costume.has_value(),"unique owns its issued clock");output("born");relog();output("relogged");
        EquipContext actor{255,0,0,issued+1000};const auto* use=catalog->FindUsePolicy(catalog->Find(key)->base.usePolicy);while(!(use->allowedClasses&(1u<<actor.characterClass)))++actor.characterClass;while(!(use->allowedSexes&(1u<<actor.sex)))++actor.sex;
        const auto fixed=grant(permanent);apply(MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,fixed,actor).value());
        apply(MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,id,actor).value());Check(find(id).placement->anchor==23,"unique free alternate slot selected");const auto started=*find(id).costume;output("equipped");
        const auto baselineStats=BuildEquipmentStats(*catalog,owner,std::array{find(fixed)}).value().bonuses;
        const auto third=grant(permanent);const auto* rules=catalog->FindDefinitionRules(key);const bool locked=rules && !rules->allowUnequip;
        if(locked) {
            const auto denied=MakeUnequipCommit(*catalog,op(),state.fence,state.storages,state.items,id,{ItemStorageKind::Inventory,1,10},actor);
            Check(!denied && denied.error()==ItemPersistenceError::TransferDenied,"source no-unequip restriction is retained");output("locked");
            apply(MakeCostumeClockCommit(*catalog,op(),state.fence,state.storages,state.items,id,issued+1123,123).value());output("wear_debit");relog();
        } else {
            const std::array debt{CostumeElapsed{id.Value(),clock.clock==CostumeClock::EquippedTime ? 123u : 0u}};actor.costumeElapsed=debt;actor.nowUtcMs=issued+1123;
            apply(MakeUnequipCommit(*catalog,op(),state.fence,state.storages,state.items,id,{ItemStorageKind::Inventory,1,10},actor).value());
            const auto paused=*find(id).costume;Check(clock.clock==CostumeClock::EquippedTime ? paused.remainingMs+123==started.remainingMs : paused.deadlineUtcMs==started.deadlineUtcMs,"unique exact wear debit or immutable wall deadline");output("paused");relog();
            actor.costumeElapsed={};actor.nowUtcMs=issued+2000;apply(MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,third,actor).value());
            apply(MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,id,actor).value());Check(find(id).placement->anchor==22 && find(fixed).placement==ItemPlacement{ItemStorageKind::Inventory,1,10},"unique timed and permanent full-slot swap");Check(find(id).costume==paused,"re-equip never resets clock");relog();output("swapped");
        }
        const auto paused=*find(id).costume;
        const auto expiry=MakeCostumeClockCommit(*catalog,op(),state.fence,state.storages,state.items,id,clock.clock==CostumeClock::EquippedTime ? issued+3000 : paused.deadlineUtcMs,clock.clock==CostumeClock::EquippedTime ? paused.remainingMs : 0).value();
        auto forged=expiry;forged.items.front().after.costume->remainingMs++;Check(!ValidateItemCommit(*catalog,forged),"forged unique expiry rejected");apply(expiry);Check(std::ranges::find(state.items,id,&PersistentItem::id)==state.items.end(),"unique expiration removes source instance");
        Check(BuildEquipmentStats(*catalog,owner,state.items)->bonuses==baselineStats,"expiry removes only timed unique bonuses");
        Check(!find(fixed).costume && !find(third).costume,"permanent unique never gains clock");output("expired");const auto saved=state.items;apply(expiry);Check(replay && state.items==saved,"expiry retry cannot resurrect unique");relog();output("expired_relogged");
        if(clock.clock!=CostumeClock::EquippedTime) {
            const auto bag=grant(key);const auto back=*find(bag).placement;if(clock.clock==CostumeClock::SinceFirstUse){apply(MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,bag,actor).value());apply(MakeUnequipCommit(*catalog,op(),state.fence,state.storages,state.items,bag,back,actor).value());}
            const auto deadline=find(bag).costume->deadlineUtcMs;relog();apply(MakeCostumeClockCommit(*catalog,op(),state.fence,state.storages,state.items,bag,deadline,0).value());Check(std::ranges::find(state.items,bag,&PersistentItem::id)==state.items.end(),"unique wall deadline expires inside bag after relog");
        } else Check(!MakeCostumeClockCommit(*catalog,op(),state.fence,state.storages,state.items,fixed,issued+900000,0),"permanent unique cannot acquire a maintenance clock");
    }
    store.Stop();std::cout<<"SOURCE_UNIQUE_CLOCK_WORKER=SUCCESS sources="<<sources.size()<<" creation="<<kinds[2]<<" first_use="<<kinds[3]<<" wear="<<kinds[4]<<" checks="<<checks<<std::endl;
}
