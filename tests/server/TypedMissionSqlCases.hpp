#include "MissionProgressionSqlCases.hpp"
void TypedMissionGolden(const std::filesystem::path& path){
    namespace m=auriga::mission;const auto catalog=BuildItemCatalog(f::Catalog()).value();m::TypedMissionView view;view.code="typed.story";view.title="Mission proba";view.summary="Gyujtes es egyszeri jutalom";view.version=1;view.npcRace=9003;view.minimumLevel=1;view.state=2;view.instance=1;view.nodes={{"collect","Anyaggyujtes","item.6.1.0",0,2,1,3,3,0,{}},{"reward","Jutalom","",6,1,0,0,1,0,{}}};
    ItemMission contract;contract.id=1;contract.version=1;contract.code="typed.story";contract.name="Mission";contract.requirements={{item_catalog_fixture::Leather(),0,10}};contract.costs={{item_catalog_fixture::Leather(),0,4}};contract.rewards={{item_catalog_fixture::Leather(),0,2}};contract.goldCost=25;contract.goldReward=75;view.reward=contract;view.experience=123456;view.levels=2;view.accountCoins=75;
    p::LoadedItemOwner owner;owner.fence={f::Character(),1,1};auto material=f::Leather(2);material.count=10;owner.items={material};BookCharacter character;character.experience=900;character.progression=CharacterProgression{60};auto packet=m::EncodeTypedMissions(*catalog,m::TypedMissionStatus::Ready,99,4,123,std::span(&view,1),&owner,"",&character,500);Check(packet.has_value(),"native typed Mission projection");
    std::ofstream out(path);for(auto b:*packet)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';Check(out.good(),"typed Mission golden written");
}
void TypedMissionSql(CAsyncSQL& admin,CAsyncSQL& writer,const ItemCatalog& catalog,unsigned port,const std::string& password)
{
    namespace m=auriga::mission;namespace mp=m::persistence;
    const ItemOwner owner{ItemOwnerKind::Character,880234};
    Check(p::EnsureRuntimeStorageOwner(writer,catalog,owner).has_value(),"typed Mission item owner");
    auto state=p::LoadCurrentItemOwner(writer,catalog,owner).value();
    auto grant=MakeGrantCommit(catalog,{34,1},state.fence,state.storages,state.items,{item_catalog_fixture::Leather(),0,10},std::array{ItemInstanceId::Create(34000001).value()});
    Check(grant && p::CommitItems(writer,catalog,*grant).has_value(),"Mission existing costs provisioned");
    m::Definition definition;definition.id="typed.sql.story";definition.version=1;definition.root="root";
    m::Node root;root.id="root";root.kind=m::NodeKind::Sequence;root.children={"collect","reward","done"};
    m::Node collect;collect.id="collect";collect.kind=m::NodeKind::Objective;collect.objective={m::EventKind::ItemCollected,"item.6.1.0",2,false};
    m::Node reward;reward.id="reward";reward.kind=m::NodeKind::Reward;reward.reward="bundle";
    m::Node done;done.id="done";done.kind=m::NodeKind::Success;definition.nodes={root,collect,reward,done};
    m::RewardBundle bundle;bundle.schema=2;const auto material=item_catalog_fixture::Leather();bundle.typedConsumeItems[{uint16_t(material.Category()),material.LocalId(),0}]=4;bundle.typedItems[{uint16_t(material.Category()),material.LocalId(),0}]=2;bundle.typedRequirements[{uint16_t(material.Category()),material.LocalId(),0}]=10;bundle.gold=75;definition.rewards["bundle"]=bundle;
    m::PackageSource source;source.manifest={"typed.sql.package",1,"hu",m::RequiredNativeTypes(definition)};source.missions={definition};source.presentation.locales["hu"]={{"title","Mission"},{"summary","Typed SQL"}};source.presentation.missions[definition.id]={"title","summary",{}};
    auto package=m::CompilePackage(source);Check(bool(package),"typed Mission package compiles");auto decoded=m::LoadPackage(package.bytes);Check(bool(decoded) && decoded.package->Graphs()[0]->Source()==definition,"typed Mission package exact roundtrip");
    CAsyncSQL db;db.SetRetryConnectionErrors(false);Check(db.Setup("127.0.0.1","item_writer",password.c_str(),"mission","utf8mb4",true,port),"Mission connection");
    const auto run=[&](const mp::Plan& plan){Check(bool(plan),"Mission SQL plan valid");std::unique_ptr<SQLMsg> msg(db.DirectTransactionPrepared(plan.statements));return mp::Confirmed(*msg);};
    Check(run(mp::ArchivePackage(package.bytes)),"typed package archived");auto claim=mp::Claim(owner.id,0,mp::NewToken(),mp::NewToken());m::FenceTypedMissionPlan(claim,state.fence);Check(run(claim),"Mission owner claim fenced by actual native login");
    // The claim token is carried in its bound UPDATE, never inferred from SQL.
    mp::Owner holder{uint32_t(owner.id),1,{}};const auto& token=claim.statements[4].params[0].bytes; // Preconditions + inserted native guard + INSERT + UPDATE.
    Check(token.size()==16,"Mission claim owner token");std::copy(token.begin(),token.end(),holder.token.begin());
    m::DefinitionRegistry registry;Check(registry.Activate({definition}).status==m::Publication::Status::Published,"typed registry");m::MissionRuntime runtime(registry,{uint32_t(owner.id),1},100);
    Check(runtime.Start(definition.id,1,1,100).code==m::ResultCode::Applied,"Mission starts");auto save=mp::Save(runtime.Save(),registry,holder,0,mp::NewToken());m::FenceTypedMissionPlan(save,state.fence);Check(run(save),"start checkpoint durable");
    Check(runtime.Dispatch({{uint32_t(owner.id),1},{1,1},101,m::EventKind::ItemCollected,"item.6.1.0",1,{}},101).code==m::ResultCode::Applied,"first accepted collection");save=mp::Save(runtime.Save(),registry,holder,1,mp::NewToken());m::FenceTypedMissionPlan(save,state.fence);Check(run(save),"partial objective checkpoint durable");
    auto before=p::LoadCurrentItemOwner(writer,catalog,owner).value();Check(p::ClaimOwnerSession(writer,catalog,owner).has_value(),"typed Mission relog");Check(!run(save),"old login cannot publish Mission progress");state=p::LoadCurrentItemOwner(writer,catalog,owner).value();
    auto nextToken=mp::NewToken();claim=mp::Claim(owner.id,1,nextToken,mp::NewToken());m::FenceTypedMissionPlan(claim,state.fence);Check(run(claim),"relog claims Mission owner");holder={uint32_t(owner.id),2,nextToken};
    const auto read=mp::ReadSnapshot(holder);std::unique_ptr<SQLMsg> msg(db.DirectQueryPrepared(read.query.c_str(),read.params));auto snapshot=mp::DecodeSnapshot(*msg,holder);auto checkpoint=m::DecodeCheckpoint(snapshot.snapshot.bytes,registry);Check(snapshot.status==mp::ReadStatus::Found && bool(checkpoint),"exact persisted Mission restored");m::MissionRuntime restored(registry,{uint32_t(owner.id),2},101);Check(restored.Restore(*checkpoint.checkpoint,101).code==m::ResultCode::Applied && restored.Inspect(1)->nodes[1].counter==1,"one collection survives relog");
    Check(restored.Dispatch({{uint32_t(owner.id),2},{1,1},101,m::EventKind::ItemCollected,"item.6.1.0",1,{}},101).code==m::ResultCode::Duplicate,"event dedup survives relog");
    Check(restored.Dispatch({{uint32_t(owner.id),2},{2,1},102,m::EventKind::ItemCollected,"item.6.1.0",1,{}},102).code==m::ResultCode::Applied,"second collection");save=mp::Save(restored.Save(),registry,holder,2,mp::NewToken());m::FenceTypedMissionPlan(save,state.fence);Check(run(save),"pending action and outbox persisted");
    const auto action=restored.PendingActions().front();auto write=m::PrepareTypedMissionWrite(catalog,registry,restored.Save(),holder,3,action,state,120,std::array{ItemInstanceId::Create(34000002).value()},103);Check(write.has_value(),"joint entitlement item and acknowledgement plan");
    auto corrupt=write->plan;corrupt.statements.push_back({"SELECT 1 WHERE 0",{},{},1});Check(!run(corrupt),"late failure rolls back entire reward transaction");auto unchanged=p::LoadCurrentItemOwner(writer,catalog,owner).value();Check(unchanged.items==state.items && unchanged.wallets==state.wallets,"no partial consumption or reward");
    Check(run(write->plan),"item effect and Mission completion commit together");state=p::LoadCurrentItemOwner(writer,catalog,owner).value();Check(Find(state,34000001).count==8 && state.wallets[0].balance==75,"exact combined consume reward and Yang");
    auto resolve=mp::Resolve(write->plan);std::unique_ptr<SQLMsg> settled(db.DirectTransactionPrepared(resolve));Check(mp::DecodeResolution(*settled,write->plan).status==mp::Resolution::Committed,"durable reward replay resolves without another item write");
    Check(!run(write->plan),"repeated joint write rejected atomically");Check(p::LoadCurrentItemOwner(writer,catalog,owner)->items==state.items,"repeat grants nothing");
    Check(Scalar(admin,"SELECT COUNT(*) FROM player.item_mission_event WHERE character_id=880234")=="0","unwatched grants create no Mission events");
    Execute(writer,"INSERT INTO player.item_mission_watch(character_id) VALUES(880234)");
    auto collection=MakeGrantCommit(catalog,{34,2},state.fence,state.storages,state.items,{material,0,1},std::array{ItemInstanceId::Create(34000003).value()});
    Check(collection && p::CommitItems(writer,catalog,*collection).has_value(),"accepted grant and Mission outbox commit together");
    Check(Scalar(admin,"SELECT COUNT(*) FROM player.item_mission_event WHERE character_id=880234 AND units=1 AND delivered=0")=="1","one durable collection event");
    Check(p::CommitItems(writer,catalog,*collection)->replay && Scalar(admin,"SELECT COUNT(*) FROM player.item_mission_event WHERE character_id=880234")=="1","item replay cannot duplicate its event");
    state=p::LoadCurrentItemOwner(writer,catalog,owner).value();auto rejected=MakeGrantCommit(catalog,{34,3},state.fence,state.storages,state.items,{material,0,1},std::array{ItemInstanceId::Create(34000004).value()});
    auto transaction=p::PrepareItemWrite(catalog,*rejected).value();transaction.push_back({"SELECT 1 WHERE 0",{},{},1});std::unique_ptr<SQLMsg> rollback(writer.DirectTransactionPrepared(transaction));
    Check(rollback->transactionOutcome==SqlTransactionOutcome::RolledBack || rollback->transactionOutcome==SqlTransactionOutcome::Conflict,"late item failure rolls back outbox too");
    Check(p::LoadCurrentItemOwner(writer,catalog,owner)->items==state.items && Scalar(admin,"SELECT COUNT(*) FROM player.item_mission_event WHERE character_id=880234")=="1","failed grant emits no collection event");

    MissionProgressionSql(admin,db,writer,catalog);

}
