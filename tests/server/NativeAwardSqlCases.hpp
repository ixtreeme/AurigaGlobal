// Included in the owned native item MariaDB fixture.
void NativeAwardsSql(CAsyncSQL& admin,CAsyncSQL& writer,const ItemCatalog& c)
{
    const ItemOwner account{ItemOwnerKind::Account,98101},other{ItemOwnerKind::Account,98102};
    Execute(admin,"CREATE DATABASE IF NOT EXISTS account");
    Execute(admin,"CREATE TABLE IF NOT EXISTS account.account(id INT UNSIGNED PRIMARY KEY,login VARCHAR(30) NOT NULL) ENGINE=InnoDB");
    Execute(admin,"GRANT SELECT ON account.* TO 'item_writer'@'localhost'");
    Execute(admin,"INSERT INTO account.account(id,login) VALUES(98101,'NativeAwardA'),(98102,'NativeAwardB')");
    Execute(admin,"INSERT INTO player.player(id,account_id,name) VALUES(98101,98101,'AwardA'),(98102,98101,'AwardB'),(98103,98102,'AwardC')");
    Execute(admin,"INSERT INTO player.guild VALUES(98100,98101)");
    Execute(admin,"INSERT INTO player.guild_member VALUES(98101,98100,1),(98102,98100,15),(98103,98100,15)");
    for(const auto owner:{account,other})Check(p::EnsureRuntimeStorageOwner(writer,c,owner).has_value(),"native award account bootstrap");
    std::array<std::uint8_t,16> operation{0x41,0x57,0x44,1};
    const auto key=f::Leather().key;
    auto queued=p::EnqueueGuildNativeAwards(writer,c,98100,operation,key,0,3);
    Check(queued && *queued==3,"one native award per original guild member, including two chars on one account");
    Check(p::EnqueueGuildNativeAwards(writer,c,98100,operation,key,0,3)==0,"same native guild request never duplicates awards");
    Check(!p::EnqueueGuildNativeAwards(writer,c,98100,operation,key,0,4),"replayed request cannot change reward count");
    Check(!p::EnqueueGuildNativeAwards(writer,c,98100,{0x41,0x57,0x44,9},key,UINT16_MAX,1),"invalid native refinement never enters the award queue");
    auto state=p::LoadCurrentItemOwner(writer,c,account).value();
    const auto beforeWallets=state.wallets;
    // Build and validate the actual claimed grant before delivery. Forging its
    // owner, amount or destination must fail before any SQL can mutate state.
    const auto id=std::stoull(Scalar(admin,"SELECT id FROM player.item_native_award WHERE account_id=98101 ORDER BY id LIMIT 1"));
    const auto block=p::ReserveItemInstanceIds(writer,c,1).value();
    const std::array ids{ItemInstanceId::Create(block.first).value()};
    auto preview=MakeGrantCommit(c,{0x41,0x57,0x44,2},state.fence,state.storages,state.items,ItemGrant{key,0,3},ids,ItemStorageKind::Mall).value();
    preview.nativeAward=NativeAwardClaim{id,account.id,key,0,3};
    Check(ValidateItemCommit(c,preview).has_value(),"native Mall award claim validated");
    for(unsigned bad=0;bad<4;++bad){auto forged=preview;if(bad==0)++forged.nativeAward->account;if(bad==1)++forged.nativeAward->count;if(bad==2)forged.items[0].after.placement->kind=ItemStorageKind::Safebox;if(bad==3)forged.nativeAward->id=0;Check(!ValidateItemCommit(c,forged),"forged award claim accepted");}
    // A forged persisted queue identity rolls the entire item creation back.
    auto mismatched=preview;mismatched.nativeAward->id+=1000000;
    const auto itemsBefore=state.items;
    Check(!p::CommitItems(writer,c,mismatched),"nonexistent queue claim committed");
    Check(p::LoadCurrentItemOwner(writer,c,account)->items==itemsBefore,"failed queue claim changed native inventory");
    Check(p::DeliverNativeItemAwards(writer,c,account)==2,"two member rewards delivered to the shared account Mall");
    Check(p::DeliverNativeItemAwards(writer,c,account)==0,"reopened Mall never duplicates claimed rewards");
    state=p::LoadCurrentItemOwner(writer,c,account).value();
    Check(state.wallets==beforeWallets,"native gift delivery never changes currency");
    std::uint64_t units=0;for(const auto& item:state.items)if(!item.destroyed && item.key==key){Check(item.placement && item.placement->kind==ItemStorageKind::Mall,"native gift outside Mall");units+=item.count;}
    Check(units==6,"both original guild members' units preserved");
    Check(Scalar(admin,"SELECT COUNT(*) FROM player.item_native_award WHERE account_id=98102 AND claimed_operation IS NULL")=="1","offline account award remains pending");
    // A full Mall must retain its pending award. After a real native withdrawal,
    // reopening it delivers once without losing the original packed stack.
    Execute(admin,"UPDATE player.item_storage SET grid_columns=1,grid_rows=1,grid_pages=1 WHERE owner_kind=2 AND owner_id=98102 AND storage_kind=4");
    auto full=p::LoadCurrentItemOwner(writer,c,other).value();
    const auto fullIds=p::ReserveItemInstanceIds(writer,c,1).value();const std::array fullReserved{ItemInstanceId::Create(fullIds.first).value()};
    const auto stack=c.Find(key)->base.maxStack;
    auto fill=MakeGrantCommit(c,{0x41,0x57,0x44,3},full.fence,full.storages,full.items,ItemGrant{key,0,stack},fullReserved,ItemStorageKind::Mall).value();
    Check(p::CommitItems(writer,c,fill).has_value(),"full Mall fixture persisted through native writer");
    Check(p::DeliverNativeItemAwards(writer,c,other)==0,"full Mall does not discard pending gift");
    Check(Scalar(admin,"SELECT COUNT(*) FROM player.item_native_award WHERE account_id=98102 AND claimed_operation IS NULL")=="1","full Mall leaves original queue row pending");
    const ItemOwner recipient{ItemOwnerKind::Character,98103};Check(p::EnsureRuntimeStorageOwner(writer,c,recipient).has_value(),"native award recipient inventory");
    const auto person=p::LoadCurrentItemOwner(writer,c,recipient).value();full=p::LoadCurrentItemOwner(writer,c,other).value();
    std::vector<ItemOwnerFence> fences{person.fence,full.fence};auto layouts=person.storages;layouts.insert(layouts.end(),full.storages.begin(),full.storages.end());
    auto items=person.items;items.insert(items.end(),full.items.begin(),full.items.end());auto wallets=person.wallets;wallets.insert(wallets.end(),full.wallets.begin(),full.wallets.end());
    const auto subject=full.items.front().id.Value();StorageActor facts{98103,98102,0,{120,0,0},true,true,false,false};
    auto withdraw=MakeStorageCommit(c,{0x41,0x57,0x44,4},fences,layouts,items,wallets,facts,{StorageRoute::Mall,StorageCommand::Withdraw,subject,0});
    Check(withdraw && p::CommitItems(writer,c,withdraw->commit).has_value(),"native Mall withdrawal frees gift space");
    Check(p::DeliverNativeItemAwards(writer,c,other)==1,"offline account receives its pending gift after freeing Mall space");
    Check(p::DeliverNativeItemAwards(writer,c,other)==0,"retry after full-Mall delivery is idempotent");
    Check(Scalar(admin,"SELECT COUNT(*) FROM player.item_native_award WHERE claimed_operation IS NOT NULL")=="3","every queue claim persisted once");
}
