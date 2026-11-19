// Included after the owned native runtime fixture's Check helper.
void LiveDropWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& base,const std::string& password)
{
    using namespace auriga::item;namespace db=auriga::item::sql;namespace p=auriga::item::persistence;
    CAsyncSQL admin;admin.SetTimeouts(5);admin.SetRetryConnectionErrors(false);
    Check(admin.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"live-drop owned admin connects");
    const auto execute=[&](const std::string& q,SqlParams args=SqlParams{}){std::unique_ptr<SQLMsg> result(admin.DirectQueryPrepared(q.c_str(),args));Check(result && !result->uiSQLErrno && result->Get(),"live-drop fixture query");};
    auto initial=db::ReadLiveMobDrops(admin,base);Check(initial && *initial && (**initial).supported && (**initial).release.empty(),"live-drop schema installed with static fallback");
    const db::Rows groups{{"880001","drop.live_worker","1","999601","1","0","0"}};
    db::Rows entries{{"880001","1","6","1","0","3","1000000000","0","0","1","65535"}};
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,base).has_value(),"live-drop item worker starts");
    const ItemOwner owner{ItemOwnerKind::Character,789901};const ItemStorageLayout bag{{owner,ItemStorageKind::Inventory,1},5,9,4};
    Check(p::ProvisionOwner(admin,*base,owner,std::span(&bag,1),{}).has_value(),"live-drop native test owner provisioned");
    const auto state=p::LoadCurrentItemOwner(admin,*base,owner).value();const auto ids=runtime::PeekInstanceIds(1);
    auto grant=MakeGrantCommit(*base,{0xD7,1},state.fence,state.storages,state.items,{ItemKey::Create(6,1).value(),0,7},ids).value();runtime::SpendInstanceIds(1);
    std::promise<void> entered,release;auto gate=release.get_future().share();
    Check(store.Submit([&entered,gate](CAsyncSQL&,const ItemCatalog&){entered.set_value();gate.wait();return runtime::ItemStore::Completion{};}),"worker barrier queued");
    Check(entered.get_future().wait_for(std::chrono::seconds(5))==std::future_status::ready,"worker barrier entered");
    std::optional<runtime::CommitOutcome> written;
    Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,grant,[&](auto result){written=std::move(result);}),"native grant queued before drop publication");
    auto published=db::PublishLiveMobDrops(admin,base,groups,entries,"",(**initial).hash,std::string(32,'a'));
    release.set_value();Check(published.has_value(),"live-drop release committed while native work was queued");
    const auto oldView=runtime::CurrentDropCatalog();
    const auto wait=[&](const auto& ready,int milliseconds=5000){const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);while(!ready() && std::chrono::steady_clock::now()<end){store.Drain();runtime::TickLiveDropReload();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(ready(),"live-drop worker completion timeout");};
    wait([&]{return written && runtime::CurrentDropCatalog()->ContentHash()==published->catalog->ContentHash();});
    Check(written->write && written->state && written->state->items.size()==1 && written->state->items[0].count==7,"queued grant preserved its original catalog pin");
    Check(runtime::CurrentCatalog()==base && base->ContentHash()==grant.catalogHash,"drop swap changed persistence catalog identity");
    store.Stop();Check(store.Start(player,base).has_value(),"item worker restarts with a published live release");
    Check(runtime::CurrentDropCatalog()->ContentHash()==published->catalog->ContentHash(),"worker startup did not load the durable active drop release");
    Check(!oldView->FindMobDropGroup(880001),"held old drop view changed under a reader");
    MobDropContext kill;kill.mobVnum=999601;kill.mobRank=5;kill.killerLevel=30;
    const auto rolled=PlanMobDrops(*runtime::CurrentDropCatalog(),kill,[](auto){return 1u;});
    Check(rolled && std::ranges::any_of(*rolled,[](const auto& d){return d.group==880001 && d.amount==3;}),"next kill uses newly published drop group");
    auto bad=entries;bad[0][4]="65535";
    Check(!db::PublishLiveMobDrops(admin,base,groups,bad,published->release,published->hash,std::string(32,'b')),"invalid live release entered SQL");
    entries[0][5]="4";
    Check(!db::PublishLiveMobDrops(admin,base,groups,entries,"",(**initial).hash,std::string(32,'b')),"stale editor replaced another drop release");
    auto next=db::PublishLiveMobDrops(admin,base,groups,entries,published->release,published->hash,std::string(32,'b'));Check(next.has_value(),"second immutable release published");
    execute("UPDATE protos.mob_drop_live_entry SET amount=5 WHERE release_id=?",{SqlParam::Text(next->release)});
    Check(!db::ReadLiveMobDrops(admin,base),"corrupt release bypassed checksum validation");
    const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(2200);
    while(std::chrono::steady_clock::now()<until){store.Drain();runtime::TickLiveDropReload();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    Check(runtime::CurrentDropCatalog()->ContentHash()==published->catalog->ContentHash(),"failed reload replaced the last good drop list");
    execute("UPDATE protos.mob_drop_live_head SET release_id=? WHERE singleton=1",{SqlParam::Text(published->release)});
    auto empty=db::PublishLiveMobDrops(admin,base,{},{},published->release,published->hash,std::string(32,'c'));Check(empty.has_value(),"empty drop release published");
    wait([&]{return runtime::CurrentDropCatalog()->ContentHash()==empty->catalog->ContentHash();});
    Check(!runtime::CurrentDropCatalog()->FindMobDropGroup(880001),"deleting drops left the old group active");
    if((**initial).containersSupported){
        const auto box=ItemKey::Create(8,1).value();Check(base->Find(box)!=nullptr,"live chest fixture exists");
        db::Rows boxGroups{{"8","1","2"}},boxEntries{{"8","1","1","1","6","1","2","10000","0","0"}};
        auto firstBox=db::PublishLiveMobDrops(admin,base,{},{},empty->release,empty->hash,std::string(32,'d'),std::pair{boxGroups,boxEntries});
        Check(firstBox.has_value(),"worker live chest release published");
        wait([&]{return runtime::CurrentDropCatalog()->ContentHash()==firstBox->catalog->ContentHash();});
        const auto held=runtime::CurrentDropCatalog();const auto firstProfile=std::get<ItemContainerRow>(held->Find(box)->category).lootProfile;
        Check(held->FindLoot(*firstProfile)->Data().entries[0].amount==2,"worker swap activates live chest contents");
        boxEntries[0][6]="3";
        auto secondBox=db::PublishLiveMobDrops(admin,base,{},{},firstBox->release,firstBox->hash,std::string(32,'e'),std::pair{boxGroups,boxEntries});
        Check(secondBox.has_value(),"second live chest release published");
        wait([&]{return runtime::CurrentDropCatalog()->ContentHash()==secondBox->catalog->ContentHash();});
        Check(held->FindLoot(*firstProfile)->Data().entries[0].amount==2 && runtime::CurrentCatalog()==base,"held chest reader and persistence catalog remain immutable");
        store.Stop();Check(store.Start(player,base).has_value(),"worker restarts with live chest contents");
        Check(runtime::CurrentDropCatalog()->ContentHash()==secondBox->catalog->ContentHash(),"durable chest release loads at worker startup");
    }
    execute("UPDATE protos.mob_drop_live_head SET release_id=NULL WHERE singleton=1");
    // Read once without a known version to prove reboot uses the durable head.
    auto restored=db::ReadLiveMobDrops(admin,base);Check(restored && *restored && (**restored).catalog==base,"durable static fallback restored");
    store.Stop();std::cout<<"LIVE_DROP_WORKER=SUCCESS swap queued_item_write old_reader invalid_release stale_editor delete\n";
}
