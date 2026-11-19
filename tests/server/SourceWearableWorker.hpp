void WearableCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& path,const std::vector<BulkSource>& sources,
    auriga::item::ItemGrant targetGrant,const std::array<auriga::item::ItemKey,5>& tools)
{
    using namespace auriga::item;
    CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"wearable importer connects to owned SQL");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-wearable-worker").has_value(),"actual source wearable item worker starts");
    unsigned wearables=0,services=0,boxes=0;constexpr std::uint64_t issuedAt=1794660000000ULL;
    for(unsigned n=0;n<sources.size();++n) {
        const auto& source=sources[n];const auto key=source.grant.key;std::cout<<"WEARABLE_SOURCE_CASE="<<n<<" key="<<static_cast<unsigned>(key.Category())<<":"<<key.LocalId()<<std::endl;
        const ItemOwner owner{ItemOwnerKind::Character,7600u+n};
        const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4},ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},28,1,1}};
        Check(persistence::ProvisionOwner(sql,*catalog,owner,layouts,{}).has_value(),"source wearable owner provisioned");auto state=persistence::LoadCurrentItemOwner(sql,*catalog,owner).value();
        unsigned serial=0;const auto op=[&](){++serial;return std::array<std::uint8_t,16>{0xF9,static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(n>>8),static_cast<std::uint8_t>(serial)};};
        bool replay=false;
        const auto apply=[&](const ItemCommit& commit){std::optional<runtime::CommitOutcome> got;Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"source wearable operation queued");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"source wearable operation saved and reloaded");replay=got->write->replay;state=*got->state;};
        const auto grant=[&](ItemGrant spec){const auto ids=runtime::PeekInstanceIds(1);runtime::SpendInstanceIds(1);spec.createdAtUtcMs=issuedAt;auto commit=MakeGrantCommit(*catalog,op(),state.fence,state.storages,state.items,spec,ids);Check(commit.has_value(),"actual source prototype grant accepted");apply(*commit);return ids.front();};
        const auto find=[&](ItemInstanceId id)->const PersistentItem&{auto it=std::ranges::find(state.items,id,&PersistentItem::id);Check(it!=state.items.end(),"expected source instance is loaded");return *it;};
        const auto output=[&](const std::string& phase,const std::vector<std::byte>& service={}) {
            const auto write=[&](const char* suffix,const std::vector<std::byte>& bytes){std::ofstream file(path+"."+std::to_string(n)+"."+phase+suffix);for(auto b:bytes){const auto v=std::to_integer<unsigned>(b);file<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}file<<'\n';Check(file.good(),"source wearable packet exported");};
            auto inventory=EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items);Check(inventory && DecodeTypedInventory(*inventory,owner).has_value(),"source wearable inventory decodes natively");write(".inventory.hex",*inventory);write(".details.hex",EncodeTypedItemDetails(*catalog,state.items).value());if(!service.empty())write(".service.hex",service);
            if(key.Category()==ItemCategory::Costume) {
                CostumeAppearance appearance{313,state.fence.revision};const auto* use=catalog->FindUsePolicy(catalog->Find(key)->base.usePolicy);unsigned job=0,sex=0;while(!(use->allowedClasses&(1u<<job)))++job;while(!(use->allowedSexes&(1u<<sex)))++sex;
                for(const auto& i:state.items)if(i.key==key && i.placement && i.placement->kind==ItemStorageKind::Equipment) {
                    const auto& row=std::get<ItemCostumeRow>(catalog->Find(key)->category);const auto* visual=catalog->FindCostumeVisual(key,job+1,sex+1);Check(visual!=nullptr,"worn source has an explicit class and sex visual");appearance.models[static_cast<unsigned>(row.kind)+1]=visual->modelAsset;
                }
                std::vector<std::byte> bytes;for(auto b:EncodeCostumeAppearance(appearance).value())bytes.push_back(std::byte(b));write(".appearance.hex",bytes);
            }
        };
        if(key.Category()==ItemCategory::Costume || key.Category()==ItemCategory::Armor) {
            ++wearables;const auto id=grant(source.grant);const auto* use=catalog->FindUsePolicy(catalog->Find(key)->base.usePolicy);unsigned job=0,sex=0;while(!(use->allowedClasses&(1u<<job)))++job;while(!(use->allowedSexes&(1u<<sex)))++sex;
            const EquipContext actor{255,static_cast<std::uint8_t>(job),static_cast<std::uint8_t>(sex),issuedAt+100};output("bag");
            auto equipped=MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,id,actor);Check(equipped.has_value(),"authored source wearable can equip");apply(*equipped);
            auto stats=BuildEquipmentStats(*catalog,owner,state.items).value();std::array<std::int64_t,MaximumBonusId+1> expected{};for(auto b:source.bonuses)expected[static_cast<unsigned>(b.bonus)]+=b.valueScaled;
            Check(stats.bonuses==expected && find(id).placement->kind==ItemStorageKind::Equipment,"wearable grants exact independently decoded source bonuses");output("equipped");
            const auto worn=state.items;apply(*equipped);Check(replay && state.items==worn,"wearable equip retry never duplicates or refills the item");
            const auto generation=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,generation).value();Check(state.items==worn,"wearable exact state survives a new owner session");output("relogged");
            apply(MakeUnequipCommit(*catalog,op(),state.fence,state.storages,state.items,id,{ItemStorageKind::Inventory,1,0},actor).value());
            Check(std::ranges::all_of(BuildEquipmentStats(*catalog,owner,state.items)->bonuses,[](auto value){return value==0;}),"wearable unequip removes its contributions");output("unequipped");
            const auto* costume=std::get_if<ItemCostumeRow>(&catalog->Find(key)->category);
            if(costume && costume->clock!=CostumeClock::Permanent) {
                apply(MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,id,actor).value());const auto saved=find(id).costume.value();
                const auto now=costume->clock==CostumeClock::EquippedTime ? issuedAt+100+saved.remainingMs : saved.deadlineUtcMs;
                apply(MakeCostumeClockCommit(*catalog,op(),state.fence,state.storages,state.items,id,now,costume->clock==CostumeClock::EquippedTime ? saved.remainingMs : 0).value());
                Check(state.items.empty() && BuildEquipmentStats(*catalog,owner,state.items)->equipped.empty(),"source costume clock expires and removes the saved item and bonuses");output("expired");
            }
        } else if(key.Category()==ItemCategory::Consumable) {
            ++services;const auto target=grant(targetGrant);const auto* child=std::get_if<ItemConsumableRow>(&catalog->Find(key)->category);Check(child && child->attributeServiceProfile,"rare source selects an explicit attribute service");const auto* profile=catalog->FindAttributeService(*child->attributeServiceProfile);
            const SocketMaintenanceActor actor{255,0,0,false};
            const auto seed=[&](ItemKey tool){const auto material=grant({tool,0,1});const auto quote=PrepareAttributeRuntime(*catalog,state.fence,state.items,material,target,actor);if(!quote)std::cout<<"SEED_REFUSAL source="<<tool.LocalId()<<" target="<<targetGrant.key.LocalId()<<" status="<<static_cast<unsigned>(quote.error())<<std::endl;Check(quote.has_value(),"actual source seed material prepares");auto plan=MakeAttributeRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,material,target,actor,*quote,[](auto){return 1u;});Check(plan.has_value(),"actual source seed material rolls");apply(plan->commit);};
            seed(tools[4]); // Retain an independently rolled ordinary slot.
            if(profile->pool==AttributePool::Extra || profile->operation==AttributeOperation::Reroll)seed(tools[0]);
            if(profile->pool==AttributePool::Extra && profile->operation==AttributeOperation::Reroll)seed(tools[2]);
            auto spec=source.grant;spec.count=2;const auto material=grant(spec);
            const auto quote=PrepareAttributeRuntime(*catalog,state.fence,state.items,material,target,actor);Check(quote && quote->pool==static_cast<unsigned>(profile->pool) && quote->tiers==std::vector<AttributeRuntimeTier>{{5,10000}},"rare quote uses the correct pool and highest eligible source tier");
            output("ready",EncodeAttributeRuntime(*catalog,313,&find(material),&find(target),state.fence.revision,AttributeRuntimeStatus::Ready,*quote,nullptr,std::array{target.Value()}).value());
            const auto before=find(target).attributes;auto plan=MakeAttributeRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,material,target,actor,*quote,[](auto){return 1u;});Check(plan.has_value() && plan->succeeded,"rare source service applies");apply(plan->commit);
            const auto after=find(target).attributes;const unsigned begin=profile->pool==AttributePool::Rare ? 5 : 7,capacity=profile->pool==AttributePool::Rare ? 2 : 1;
            const auto* binding=catalog->FindItemAttributes(targetGrant.key);
            for(unsigned slot=0;slot<8;++slot) {
                if(slot<begin || slot>=begin+capacity)Check(before.slots[slot]==after.slots[slot],"rare operation leaves every other pool unchanged");
                else if(after.slots[slot]) {const auto candidate=std::ranges::find_if(binding->candidates,[&](const auto& x){return x.pool==profile->pool && x.bonus==after.slots[slot]->bonus;});Check(candidate!=binding->candidates.end() && after.slots[slot]->valueScaled==candidate->values.back().value,"rare result uses the highest eligible value");}
            }
            Check(find(material).count==1,"accepted rare operation consumes exactly one unit");
            output("applied",EncodeAttributeRuntime(*catalog,313,&find(material),&find(target),state.fence.revision,AttributeRuntimeStatus::Applied,*quote,&after,std::array{target.Value()}).value());
            const auto applied=state.items;apply(plan->commit);Check(replay && state.items==applied,"rare service retry reuses its saved decision and cost");
            unsigned rng=0;Check(!MakeAttributeRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,material,target,actor,*quote,[&](auto){++rng;return 1u;}) && rng==0,"stale rare preview rejected before randomness or debit");
            const auto generation=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,generation).value();Check(state.items==applied,"rare slots and source debit persist across sessions");output("relogged");
        } else {
            ++boxes;Check(key.Category()==ItemCategory::Container,"remaining source is a newly resolved chest");const auto id=grant(source.grant);output("bag");
            // Definition conversion may resolve a reward before its random
            // birth policy is playable. Verify the refusal explicitly.
            Check(ValidateContainerContents(*catalog,key).error()==ContainerOpeningError::Unsupported,"rare-birth chest stays unavailable until its birth policy is implemented");
            unsigned draws=0;const auto ids=runtime::PeekInstanceIds(64);const auto plan=MakeContainerOpeningCommit(*catalog,op(),state.fence,state.storages,state.items,state.wallets,{id,{}, {255,0,0,false},issuedAt},ids,[&](auto){++draws;return 0;});
            Check(!plan && plan.error()==ContainerOpeningError::Unsupported && draws==0 && find(id).count==1,"unavailable chest cannot consume a box or sample a reward");
            output("unsupported",EncodeContainerOpening(*catalog,313,&find(id),nullptr,state.fence.revision,ContainerOpeningStatus::Unsupported,{},{}).value());
        }
    }
    store.Stop();std::cout<<"SOURCE_WEARABLE_WORKER=SUCCESS wearables="<<wearables<<" rare_services="<<services<<" unavailable_boxes="<<boxes<<" checks="<<checks<<std::endl;
}
