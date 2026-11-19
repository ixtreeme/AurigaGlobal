void ReusableRareCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& path,const std::vector<BulkSource>& sources)
{
    using namespace auriga::item;Check(sources.size()==5,"reusable rare source, real adder and three original target families");
    CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);Check(sql.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"reusable rare owned SQL connects");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-reusable-rare-worker").has_value(),"reusable rare actual item worker starts");
    for(unsigned n=2;n<sources.size();++n){
        const ItemOwner owner{ItemOwnerKind::Character,66000u+n};const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4}};
        Check(persistence::ProvisionOwner(sql,*catalog,owner,layouts,{}).has_value(),"reusable rare owner provisioned");auto state=persistence::LoadCurrentItemOwner(sql,*catalog,owner).value();unsigned serial=0;bool replay=false;
        const auto op=[&](){return std::array<std::uint8_t,16>{0xB9,static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(++serial)};};
        const auto submit=[&](const ItemCommit& commit){std::optional<runtime::CommitOutcome> got;Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"reusable rare commit queues");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got.has_value(),"reusable rare SQL callback completes");return std::move(*got);};
        const auto apply=[&](const ItemCommit& commit){auto got=submit(commit);Check(got.write && got.state,"reusable rare write saves and reloads");replay=got.write->replay;state=*got.state;};
        const auto relog=[&](){const auto before=state;const auto generation=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,generation).value();Check(state.items==before.items,"reusable tool and target bonuses survive a new SQL session");};
        const auto grant=[&](ItemGrant spec){const auto ids=runtime::PeekInstanceIds(1);runtime::SpendInstanceIds(1);apply(MakeGrantCommit(*catalog,op(),state.fence,state.storages,state.items,spec,ids).value());return ids[0];};
        const auto tool=grant(sources[0].grant);auto supply=sources[1].grant;supply.count=2;const auto adder=grant(supply);const auto target=grant(sources[n].grant);
        const auto item=[&](ItemInstanceId id)->const PersistentItem&{return *std::ranges::find(state.items,id,&PersistentItem::id);};
        SocketMaintenanceActor actor{255,0,0,false};
        for(unsigned i=0;i<2;++i){const auto q=PrepareAttributeRuntime(*catalog,state.fence,state.items,adder,target,actor).value();auto added=MakeAttributeRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,adder,target,actor,q,[](auto){return 1;}).value();apply(added.commit);}
        Check(std::ranges::none_of(state.items,[&](const auto& i){return i.id==adder;}),"real original rare adder debits exactly two units before reusable test");
        const auto original=item(tool);const auto unchangedNormal=item(target).attributes;
        const auto quote=[&](){return PrepareAttributeRuntime(*catalog,state.fence,state.items,tool,target,actor).value();};
        const auto write=[&](const std::string& phase,const char* kind,const auto& bytes){std::ofstream f(path+"."+std::to_string(n-2)+"."+phase+"."+kind+".hex");for(auto b:bytes){auto v=static_cast<unsigned char>(b);f<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}f<<'\n';Check(f.good(),"reusable rare actual SQL state packet exported");};
        const auto output=[&](const std::string& phase,AttributeRuntimeStatus status){const auto q=quote();const std::array targets{target.Value()};write(phase,"inventory",EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items).value());write(phase,"details",EncodeTypedItemDetails(*catalog,state.items).value());write(phase,"attributes",EncodeAttributeRuntime(*catalog,123,&item(tool),&item(target),state.fence.revision,status,q,status==AttributeRuntimeStatus::Applied ? &item(target).attributes : nullptr,targets).value());};
        Check(quote().units==0 && quote().available==1 && quote().draws==2,"original reusable quote owns last tool and two existing rare bonuses");output("ready",AttributeRuntimeStatus::Ready);
        for(unsigned round=0;round<2;++round){
            if(round)output("ready2",AttributeRuntimeStatus::Ready);
            const auto q=quote();auto changed=MakeAttributeRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,tool,target,actor,q,[&](auto max){return round ? max : 1;}).value();Check(changed.commit.items.size()==1 && changed.commit.items[0].after.id==target,"durable rare operation mutates target alone");apply(changed.commit);
            Check(item(tool)==original,"SQL retains tool identity count placement binding revision and exact state");for(unsigned i=0;i<5;++i)Check(item(target).attributes.slots[i]==unchangedNormal.slots[i],"normal bonus slot preserved through reusable rare reroll");Check(item(target).attributes.lockedSlot==unchangedNormal.lockedSlot,"normal lock preserved");
            const auto saved=state.items;apply(changed.commit);Check(replay && state.items==saved,"same SQL receipt cannot reroll twice");output("used"+std::to_string(round+1),AttributeRuntimeStatus::Applied);relog();output("relogged"+std::to_string(round+1),AttributeRuntimeStatus::Applied);Check(item(tool)==original,"relogged last tool is still reusable");
        }
        const auto q=quote();auto stale=MakeAttributeRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,tool,target,actor,q,[](auto){return 1;}).value();const auto saved=state.items;
        sql.DirectQuery(("UPDATE player.item_owner_fence SET state_revision=state_revision+1 WHERE owner_kind=1 AND owner_id="+std::to_string(owner.id)).c_str());auto refused=submit(stale.commit);Check(!refused.write && refused.state && refused.state->items==saved,"stale SQL fence cannot change target even when source debit is zero");state=*refused.state;output("stale",AttributeRuntimeStatus::Stale);relog();
    }
    store.Stop();std::cout<<"SOURCE_REUSABLE_RARE_WORKER=SUCCESS targets=3 checks="<<checks<<std::endl;
}
