#include <item/PolymorphRuntime.hpp>
#include <item/EffectRuntime.hpp>

void PolymorphBookCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& path,const std::vector<BulkSource>& sources)
{
    using namespace auriga::item;
    Check(sources.size()==1,"one original practice-book prototype");
    CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"owned polymorph-book SQL connection");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-polymorph-book-worker").has_value(),"actual original book worker starts");
    unsigned cases=0;
    for(unsigned mob:{101u,103u,1901u})for(unsigned mastery=0;mastery<4;++mastery)for(unsigned skill:{0u,20u,40u}) {
        const unsigned n=cases++;
        const ItemOwner owner{ItemOwnerKind::Character,15000u+n};
        const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4}};
        const std::array wallets{ItemWallet{owner,WalletCurrency::Gold,1,0}};
        sql.DirectQuery(("INSERT INTO player.player(id) VALUES("+std::to_string(owner.id)+")").c_str());
        Check(persistence::ProvisionOwner(sql,*catalog,owner,layouts,wallets).has_value(),"original book owner provisioned");
        auto state=persistence::LoadCurrentItemOwner(sql,*catalog,owner).value();
        unsigned serial=0;bool replay=false;
        const auto op=[&](){++serial;return std::array<std::uint8_t,16>{0xF2,static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(serial),static_cast<std::uint8_t>(serial>>8)};};
        const auto submit=[&](const ItemCommit& commit){
            std::optional<runtime::CommitOutcome> got;
            Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"original book operation queued");
            for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            Check(got.has_value(),"original book operation callback completed");return std::move(*got);
        };
        const auto apply=[&](const ItemCommit& commit){auto out=submit(commit);if(!out.write || !out.state)std::cerr<<"BOOK_WRITE case="<<n<<" serial="<<serial<<" write="<<(out.write ? -1 : static_cast<int>(out.write.error()))<<" state="<<(out.state ? -1 : static_cast<int>(out.state.error()))<<std::endl;Check(out.write && out.state,"original book operation persisted and reloaded");replay=out.write->replay;state=*out.state;};
        const auto relog=[&](){const auto saved=state;const auto generation=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,generation).value();Check(state.items==saved.items && state.recovery==saved.recovery,"original book and full transformation survive fresh SQL session");};
        const auto grant=OriginalPolymorphBookGrant(*catalog,mob,2,1).value();
        Check(grant.key==sources[0].grant.key,"quest factory selects converted original practice book");
        const auto ids=runtime::PeekInstanceIds(1);runtime::SpendInstanceIds(1);
        apply(MakeGrantCommit(*catalog,op(),state.fence,state.storages,state.items,grant,ids).value());
        const auto id=ids.front();
        const auto source=[&]()->const PersistentItem&{const auto it=std::ranges::find(state.items,id,&PersistentItem::id);Check(it!=state.items.end(),"same book instance remains live");return *it;};
        auto actor=PolymorphRequest{grant.key};actor.actorId=owner.id;actor.rulesetRevision=actor.mobCatalogRevision=1;actor.operationId[0]=1;
        actor.characterLevel=35;actor.mobLevel=5;actor.mobTemplate=mob;actor.characterClass=actor.sex=1;actor.skillLevel=skill;actor.mastery=mastery;
        actor.action=PolymorphAction::Use;actor.admittedByServer=actor.formAllowedByServer=true;
        const auto* policy=catalog->FindUsePolicy(catalog->Find(grant.key)->base.usePolicy);
        while(!(policy->allowedClasses&(1u<<(actor.characterClass-1))))++actor.characterClass;
        while(!(policy->allowedSexes&(1u<<(actor.sex-1))))++actor.sex;
        const std::uint64_t now=1794697200000ULL;
        const auto output=[&](const std::string& phase,std::uint8_t purpose=1,std::uint8_t outcome=0,std::uint16_t roll=0){
            const auto write=[&](const char* kind,const auto& bytes){std::ofstream f(path+"."+std::to_string(n)+"."+phase+"."+kind+".hex");for(auto b:bytes){const auto v=static_cast<unsigned char>(b);f<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}f<<'\n';Check(f.good(),"actual original practice-book packet exported");};
            write("inventory",EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items).value());
            write("details",EncodeTypedItemDetails(*catalog,state.items).value());
            write("polymorph",EncodePolymorphRuntime(*catalog,{static_cast<std::uint8_t>(outcome ? 3 : 1),255,purpose,outcome,roll,123,456,now,&source(),&state.recovery,&actor,"Original monster "+std::to_string(mob),state.recovery.polymorph ? "Original monster "+std::to_string(mob) : ""}).value());
        };
        output("ready");
        const auto quote=QuotePolymorphRuntime(*catalog,source(),state.recovery,actor,now).value();
        Check(quote.chanceBp==std::min(100u,60+skill)*100,"original source chance on actual converted profile");
        if(quote.chanceBp<10000){
            const auto before=source();const auto failed=MakePolymorphRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,state.recovery,id,actor,0,now,quote.chanceBp+1).value();
            apply(failed.commit);Check(!failed.result.success && source().polymorph==before.polymorph && !state.recovery.polymorph,"failed original practice consumes neither book nor practice");
            const auto saved=state;apply(failed.commit);Check(replay && state.items==saved.items && state.recovery==saved.recovery,"failed recorded roll cannot redraw on retry");output("failed",1,1,quote.chanceBp+1);
        }
        const auto use=[&](){const auto q=QuotePolymorphRuntime(*catalog,source(),state.recovery,actor,now).value();return MakePolymorphRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,state.recovery,id,actor,0,now,q.chanceBp==10000 ? 0 : q.chanceBp).value();};
        const auto first=use();apply(first.commit);
        Check(source().polymorph->practiceRemaining==1 && source().count==1 && state.recovery.polymorph && state.recovery.polymorph->effect.expiresAtUtcMs==now+10000+mastery*5000,"successful book use persists practice and exact seconds-based original duration");
        Check(state.recovery.polymorph->effect.bonus==(mob==103 ? PolymorphBonus::None : PolymorphBonus::Movement) && state.recovery.polymorph->effect.bonusMagnitude==(mob==103 ? 0 : skill+1),"actual original form bonus map is not copied from authored fixtures");
        output("active",1,2);const auto saved=state;apply(first.commit);Check(replay && state.items==saved.items && state.recovery==saved.recovery,"success receipt cannot spend extra practice or refresh transformation");relog();output("relogged",1,2);
        const auto clear=[&](){auto commit=MakePolymorphCancel(*catalog,op(),state.fence,state.recovery,0,now,state.recovery.generation).value();apply(commit);Check(!state.recovery.polymorph,"cancel removes durable native transformation");};
        clear();output("cleared",1,4);const auto second=use();apply(second.commit);clear();Check(source().polymorph->practiceRemaining==0,"second success completes exact original practice count");output("practiced",2);
        actor.action=PolymorphAction::UpgradeBook;
        const auto upgrade=MakePolymorphRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,state.recovery,id,actor,0,now,0).value();apply(upgrade.commit);
        Check(source().polymorph->bookGrade==2 && source().polymorph->practiceRemaining==50,"upgrade stores old grade times50 and next grade together");const auto upgraded=state;apply(upgrade.commit);Check(replay && state.items==upgraded.items && state.recovery==upgraded.recovery,"upgrade receipt cannot increase grade twice");relog();output("upgraded",2,3);
        actor.action=PolymorphAction::Use;const auto final=use();apply(final.commit);
        auto after=state.recovery;NormalizeTimedItems(*catalog,after,now+10000+mastery*5000);Check(!after.polymorph,"exact original deadline expires shape and bonus");++after.revision;
        ItemCommit tick;tick.operationId=op();tick.catalogRevision=catalog->Revision();tick.catalogHash=catalog->ContentHash();tick.kind=ItemOperationKind::Service;tick.owners={state.fence};tick.outcomeCode=0x2003;tick.result={'R','T','M',1};tick.recovery=ItemRecoveryChange{owner,state.recovery,after,0};Check(ValidateItemCommit(*catalog,tick).has_value(),"same normalized lifetime tick as actual game is admitted");apply(tick);relog();output("expired",1,4);
        apply(first.commit);Check(replay && !state.recovery.polymorph && source().polymorph->bookGrade==2,"old transformation receipt cannot resurrect expired form or roll back upgraded book");
    }
    Check(!OriginalPolymorphBookGrant(*catalog,UINT32_MAX,1,1) && !OriginalPolymorphBookGrant(*catalog,101,1,256),"server grant rejects unknown original form and grade overflow");
    store.Stop();Check(!store.Running(),"original practice-book worker stops cleanly");
    std::cout<<"SOURCE_POLYMORPH_BOOK_WORKER=SUCCESS cases="<<cases<<" checks="<<checks<<std::endl;
}
