"""Offline semantic conversion of the reviewed remaining character inventory.

The review document pins entire source rows and dependent loot/quest files.
No source item identity is emitted into the playable catalog. Issued native
rows are immutable; incompatible provisional categories receive fresh keys.
"""
import copy,json
from pathlib import Path
from legacy_item_migration import MigrationError,value_hash,file_hash
from convert_quest_tokens import Catalog,translation_for,verified_asset,SOURCE_STACK_LIMIT
from convert_legacy_equipment import minimum_level,convert_instances as gear_instances,ZERO_STONES,SOURCE_ITEM_FIELDS
from legacy_equipment_rules import bonus


def compile_remaining(tables,plan,preserved,review,project,visuals,locales):
    source={r['vnum']:r for r in tables['player.item_proto']}
    mapping={r['source_id']:copy.deepcopy(r) for r in plan['mapping']}
    model=Catalog(preserved);accepted=[];texts={};assets={};remaps=[]
    counters={int(k):max(int(v),int(preserved['sequences'][k])) for k,v in plan['next_local_ids'].items()}
    for identity,spec in sorted(review['items'].items(),key=lambda p:int(p[0])):
        identity=int(identity);row=source[identity]
        if value_hash(row)!=spec['source_sha256']:raise MigrationError('reviewed_source_changed:'+str(identity))
        category=spec['category'];ref=mapping[identity];key=copy.deepcopy(ref['target'])
        if key['category_id']!=category:
            key=dict(category_id=category,local_id=counters[category]);counters[category]+=1
            remaps.append(dict(source_id=identity,provisional=ref['target'],native=key))
        if any(int(r['category_id'])==category and int(r['local_id'])==key['local_id'] for r in model.tables['item_base']):
            raise MigrationError('review_would_replace_issued_native_item')
        doc=model.new(category);code='imported.'+spec['code'];base=doc['base']
        icon=visuals.get(identity)
        if not icon:raise MigrationError('missing_visual:'+str(identity))
        icon_proof=verified_asset(project,'Assets/Metin2/Generated/ItemIcons/'+icon['iconPath'],icon.get('icon'))
        base.update(category_id=str(category),local_id=str(key['local_id']),content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',
                    icon_asset=icon['iconPath'],inventory_height=str(row['size']),min_level=str(minimum_level(row)),max_stack=str(SOURCE_STACK_LIMIT if row['flag']&4 else 1),
                    buy_amount=str(row['gold']),sell_amount=str(row['shop_buy_price']))
        doc['child'].update(category_id=base['category_id'],local_id=base['local_id'])
        anti=row['antiflag'];doc['use'].update(allowed_classes=str(15 & ~(anti>>2)),allowed_sexes=str(3 & ~((anti&1)<<1|(anti&2)>>1)))
        doc['trade'].update(allow_trade=str(int(not anti&8192)),allow_drop=str(int(not anti&128)),allow_sell=str(int(not anti&256)))
        kind=spec['kind']
        if kind=='automatic':
            base['max_stack']='1';doc['recovery'].update(delivery_kind='4',allowed_contexts='11',allow_during_pvp='0')
            doc['effect']=dict(delivery_kind='4',pool_kind=str(spec['pool']),supply_kind='1',capacity_raw=str(row['value0']*1000),tick_interval_ms='1000',max_pool_per_tick_bp='700',active_max_pool_bonus_bp='0')
        elif kind=='timed':
            doc.update(Catalog.timed_defaults(key['local_id']));doc['recovery']=doc['effect']=None
            doc['timed']['duration_ms']=str(row['value3']*1000)
            doc['timed_group'].update(code='imported.effect_apply_'+str(row['value1']),clock_kind='1',remove_on_death='1')
            # The old USE_AFFECT rejects any active effect on this point, even
            # when another item applied it. Reuse one native group per point.
            existing=next((r for r in model.tables['timed_effect_group'] if r['code']==doc['timed_group']['code']),None)
            if existing:doc['timed_group']['group_id']=existing['group_id'];doc['timed']['group_id']=existing['group_id']
            doc['timed_bonuses']=[bonus(row['value1'],row['value2'])]
        elif kind=='book_aid':
            doc['book_aid']=dict(aid_kind=str(spec['aid']),units_per_use='1',charges_per_use='1',maximum_charges='1')
        elif kind=='reroll':
            doc.update(Catalog.attribute_service_defaults());doc['attribute_service'].update(operation_kind='2',minimum_count='1',maximum_count='5',add_count='0')
            doc['attribute_tiers']=[dict(tier=str(i+1),chance_bp=str(n*100)) for i,n in enumerate(spec['tier_percent']) if n]
        elif kind=='currency':
            doc['child'].update(wallet_currency=str(spec['currency']),amount_per_unit=str(row['shop_buy_price']))
        elif kind=='forge':doc['child'].update(Catalog.forge_defaults('blessing')[0])
        elif kind=='quest_use':doc['child'].update(dispatch_kind='2',group_code=spec['handler'],allowed_contexts='1',allow_observer='0')
        elif kind in ('quest_token','material'):pass
        elif kind=='book':
            doc.update(Catalog.book_defaults());doc['book_targets'][0]['skill_id']=str(row['value0'])
        elif kind=='box':
            entries=[]
            for n,reward in enumerate(spec['rewards'],1):
                target=mapping[reward['source_id']]['target'] if reward.get('source_id') else None
                entries.append(dict(entry_no=str(n),reward_kind='1' if target else '9',item_category_id=str(target['category_id']) if target else None,
                    item_local_id=str(target['local_id']) if target else None,amount=str(reward['amount']),weight=str(reward['weight']),rare_attribute_chance_bp='0'))
            doc['loot']=dict(pick_mode='1',entries=entries)
        else:raise MigrationError('unknown_review_kind')
        model.apply(doc,encode_result=False)
        mapping[identity].update(target=key,content_code=code,refinement_level=0)
        accepted.append(dict(source_id=identity,item=key,refinement_level=0,source_row_hash=value_hash(row),semantics=kind))
        texts[code]=translation_for(row,code,locales);assets[code]=dict(icon=icon_proof)
    packet=copy.deepcopy(preserved);packet['tables']=model.encode();packet.pop('hash',None)
    for k,n in counters.items():packet['sequences'][str(k)]=str(n)
    # All item rewards must resolve after category corrections, not from the
    # provisional migration plan. Reject incomplete reference closure here.
    present={(int(r['category_id']),int(r['local_id'])) for r in model.tables['item_base']}
    for row in model.tables['loot_entry']:
        if row['reward_kind']=='1' and (int(row['item_category_id']),int(row['item_local_id'])) not in present:raise MigrationError('missing_container_reward:'+str((row['item_category_id'],row['item_local_id'])))
    return dict(packet=packet,accepted=accepted,mapping=list(mapping.values()),remaps=remaps,localization=texts,assets=assets,review_sha256=value_hash(review))


def convert_mixed_instances(rows,compiled,owner):
    ready={r['source_id']:r for r in compiled['accepted']};result=[]
    for row in sorted(rows,key=lambda r:r['id']):
        ref=ready.get(row['vnum'])
        if not ref:raise MigrationError('uncompiled_instance_prototype:'+str(row['vnum']))
        if ref['item']['category_id'] in (1,2,3,7):result.extend(gear_instances([row],compiled,owner));continue
        if set(row)!=SOURCE_ITEM_FIELDS or row['owner_id']!=owner or row['window'] not in ('INVENTORY','EXTRA_INVENTORY'):raise MigrationError('incomplete_or_unowned_instance')
        if row['lockedattr']!=-1 or any(row[f'attr{k}{i}'] for k in ('type','value') for i in range(8)):raise MigrationError('unmapped_non_equipment_attributes')
        automatic=None
        if ref['semantics']=='automatic':
            active,spent,capacity=(row['socket'+str(i)] for i in range(3))
            if active not in (0,1) or not 0<=spent<capacity or capacity!=ref['capacity']:raise MigrationError('invalid_automatic_source')
            automatic=dict(active=bool(active),remaining_raw=(capacity-spent)*1000,next_tick_ms=1000 if active else 0)
        elif any(row['socket'+str(i)] for i in range(3)):raise MigrationError('unmapped_non_equipment_socket')
        result.append(dict(instance_id='0',**ref['item'],refinement_level=ref['refinement_level'],count=row['count'],placement=dict(kind=1,storage_id=1 if row['window']=='INVENTORY' else 2,anchor=row['pos']),
                           attributes=[None]*8,locked_slot=None,stones=copy.deepcopy(ZERO_STONES),accessory=None,automatic=automatic,source=row))
    return result


def compile_refinement_services(tables,compiled,owner=3):
    """Close the original gear's existing native families over smith inputs.

    No recipe is guessed for an absent target/material. These explicit blockers
    are separate from valid carried item state and retained in the audit.
    """
    model=Catalog(compiled['packet']);ready={r['source_id']:r for r in compiled['accepted']};sources={r['vnum']:r for r in tables['player.item_proto']}
    recipes={r['id']:r for r in tables['player.refine_proto']};accepted=[];blocked=[]
    owned={r['vnum'] for r in tables['player.item'] if r['owner_id']==owner and r['window'] in ('INVENTORY','EQUIPMENT','EXTRA_INVENTORY')}
    pair=lambda key:(key['category_id'],key['local_id'])
    families={pair(ready[n]['item']) for n in owned if n in ready and ready[n]['item']['category_id'] in (1,2,3,7)}
    identity=model.allocate('refinement_recipe','recipe_id')
    for n,ref in sorted(ready.items()):
        row=sources[n]
        if pair(ref['item']) not in families or not row['refined_vnum']:continue
        target=ready.get(row['refined_vnum']);recipe=recipes.get(row['refine_set'])
        if not target or not recipe:blocked.append(dict(source_id=n,reason='missing_recipe_or_target'));continue
        ingredients=[];missing=[]
        for i in range(5):
            vnum,count=recipe['vnum'+str(i)],recipe['count'+str(i)]
            if not vnum and not count:continue
            material=ready.get(vnum)
            if not material or not count:missing.append(vnum);continue
            ingredients.append(dict(category_id=str(material['item']['category_id']),local_id=str(material['item']['local_id']),refinement_level=str(material['refinement_level']),amount=str(count)))
        if missing:blocked.append(dict(source_id=n,reason='missing_recipe_material',source_materials=missing));continue
        source=ref['item'];destination=target['item']
        model.replace('refinement_recipe',{'recipe_id':identity},[dict(recipe_id=identity,code='imported.refine_'+str(identity),source_category_id=str(source['category_id']),source_local_id=str(source['local_id']),source_level=str(ref['refinement_level']),target_category_id=str(destination['category_id']),target_local_id=str(destination['local_id']),target_level=str(target['refinement_level']),gold_cost=str(recipe['cost']),success_chance_bp=str(recipe['prob']*100),failure_kind='2',failure_level=None)])
        model.replace('refinement_ingredient',{'recipe_id':identity},[dict(r,recipe_id=identity) for r in ingredients]);accepted.append(dict(source_id=n,source_recipe_sha256=value_hash(recipe),native_recipe=identity));identity=str(int(identity)+1)
    compiled['packet']['tables']=model.encode();compiled['refinement_services']=dict(accepted=accepted,blocked=blocked)


def main():
    import argparse
    from legacy_item_migration import read_checkpoint,strict_json,analyze
    from convert_quest_tokens import read_visuals,read_locales
    from upgrade_exported_catalog import native
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','mapping','equipment','review','project','helper','output'):parser.add_argument('--'+name,required=True,type=Path)
    parser.add_argument('--owner',type=int,required=True)
    args=parser.parse_args();tables,manifest=read_checkpoint(args.source);plan=strict_json(args.mapping.read_text(encoding='utf-8'))
    if plan!=analyze(tables,value_hash(dict(source_manifest=value_hash(manifest))),manifest.get('owner_keys')):raise MigrationError('Stale source mapping')
    previous=strict_json(args.equipment.read_text(encoding='utf-8'));review=strict_json(args.review.read_text(encoding='utf-8'))
    # Content references must match immutable source evidence, including the
    # reviewed quest and loot group. They were captured with the full source.
    for path,digest in review['inputs'].items():
        if manifest['files'].get(path)!=digest:raise MigrationError('Reviewed reference changed or absent')
    verified=native(args.helper,dict(action='validate',tables=previous['packet']['tables']))
    if verified['hash']!=previous['packet']['hash']:raise MigrationError('Preserved catalog hash mismatch')
    source={r['vnum']:r for r in tables['player.item_proto']}
    if any(r['source_row_hash']!=value_hash(source[r['source_id']]) for r in previous['accepted']):raise MigrationError('Equipment source differs from checkpoint')
    visuals=read_visuals(args.project/'Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset');locales,_=read_locales(args.project)
    result=compile_remaining(tables,plan,previous['packet'],review,args.project,visuals,locales)
    keys={(int(r[0]),int(r[1])) for r in previous['packet']['tables']['item_base']}
    passive=[dict(source_id=r['source_id'],item=r['target'],refinement_level=r['refinement_level'],semantics='passive') for r in plan['mapping'] if r['target'] and r['target']['category_id'] in (6,10) and (r['target']['category_id'],r['target']['local_id']) in keys]
    result['accepted']=previous['accepted']+passive+result['accepted'];result['stone_grades']=previous['stone_grades'];source={r['vnum']:r for r in tables['player.item_proto']}
    for row in result['accepted']:
        if row['semantics']=='automatic':row['capacity']=source[row['source_id']]['value0']
    result['instances']=convert_mixed_instances([r for r in tables['player.item'] if r['owner_id']==args.owner and r['window'] in ('INVENTORY','EQUIPMENT','EXTRA_INVENTORY')],result,args.owner)
    result['localization']={**previous['localization'],**result['localization']};compile_refinement_services(tables,result,args.owner)
    result['packet']['hash']=native(args.helper,dict(action='validate',tables=result['packet']['tables']))['hash']
    result['input_hashes']=dict(source_manifest=value_hash(manifest),mapping=file_hash(args.mapping),equipment=file_hash(args.equipment),review=file_hash(args.review))
    with args.output.open('x',encoding='utf-8') as f:json.dump(result,f,sort_keys=True,ensure_ascii=True);f.write('\n')
    print(json.dumps(dict(items=len(result['instances']),units=sum(r['count'] for r in result['instances']),catalog_hash=result['packet']['hash'])))

if __name__=='__main__':main()
