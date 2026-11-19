"""Offline equipment/content and complete ordinary equipment-state compiler.

Fixed damage = exact mean of the two source endpoints + refinement, in native
1/1000-point units. No endpoint/range or legacy dispatch map enters runtime.
"""
from collections import Counter,defaultdict
import copy,json,re
from pathlib import Path

from legacy_item_migration import MigrationError,PROTO_FIELDS,NAME_FIELDS,source_level,name_parts,value_hash,file_hash
from convert_quest_tokens import Catalog,verified_asset,translation_for,SOURCE_STACK_LIMIT
from legacy_equipment_rules import bonus,APPLY_NAMES

SLOTS={0:2,1:3,2:4,3:6,4:5,5:7,6:8}
WEAR={0:1,1:2,2:256,3:8,4:4,5:32,6:64}
SOURCE_SLOTS={4:0,0:1,1:2,10:3,2:4,3:5,5:6,6:7}
ZERO_STONES=[dict(state=1,grade=0,level=0,stone=None) for _ in range(3)]
SOURCE_ITEM_FIELDS={'id','owner_id','window','pos','count','vnum','lockedattr',*(f'socket{i}' for i in range(3)),*(f'attr{k}{i}' for k in ('type','value') for i in range(8))}


def fixed_attack(row):
    if not 0<=row['value3']<=row['value4'] or not 0<=row['value1']<=row['value2'] or row['value5']<0:
        raise MigrationError('invalid_weapon_damage')
    return ((row['value3']+row['value4'])*500+row['value5']*1000,
            (row['value1']+row['value2'])*500+row['value5']*1000)


def minimum_level(row):
    result=[]
    for i in range(2):
        kind,value=row[f'limittype{i}'],row[f'limitvalue{i}']
        if kind==1 and 0<=value<=65535:result.append(value)
        elif kind or value:raise MigrationError('unsupported_limit')
    if len(result)>1:raise MigrationError('duplicate_level_limit')
    return result[0] if result else 0


def effects(row):
    values=Counter()
    for i in range(5):
        source,value=row[f'applytype{i}'],row[f'applyvalue{i}']
        if source==0:continue # explicit APPLY_NONE is inert even with stale value
        try:entry=bonus(source,value)
        except ValueError as e:raise MigrationError(str(e)) from None
        if entry:values[int(entry['bonus_id'])]+=int(entry['value_scaled'])
    if row['immuneflag'] & ~7:raise MigrationError('unsupported_immunity_flag')
    for mask,identity in ((1,78),(2,79),(4,80)):
        if row['immuneflag'] & mask:values[identity]+=1
    if row['type']==2 and row['subtype'] in (0,1,2,4):
        values[116]+=(row['value1']+2*row['value5'])*1000
    return values


def policy(row):
    minimum_level(row)
    if set(row)!=PROTO_FIELDS:raise MigrationError('unexpected_proto_columns')
    if row['weight'] or row['vnum_range']:raise MigrationError('unsupported_weight_or_range')
    if not 1<=row['size']<=3:raise MigrationError('inventory_height')
    if row['flag'] & ~(1|2|4|8192):raise MigrationError('unsupported_item_flags')
    # The current server has four classes. The source's retired fifth-class
    # anti bit has no actor in this build; all active class/sex bits are mapped.
    if row['antiflag'] & ~(0x3f|128|256|8192|262144):raise MigrationError('unsupported_anti_flags')
    if row['antiflag']&3==3 or row['antiflag']&60==60:raise MigrationError('no_allowed_actor')
    if any(not isinstance(row[n],str) or '\ufffd' in row[n] or any(ord(c)<32 for c in row[n]) for n in NAME_FIELDS):raise MigrationError('invalid_source_name')
    if not row['locale_name_hu'] and not row['locale_name_en']:raise MigrationError('missing_name')
    if any(not 0<=row[n]<=2**63-1 for n in ('gold','shop_buy_price')):raise MigrationError('invalid_price')
    if row['type']==1:
        if row['subtype'] not in (0,1,2,3,4,5,7) or row['wearflag']!=16 or row['value0']:raise MigrationError('unsupported_weapon_policy')
        fixed_attack(row)
    elif row['type']==2:
        if row['subtype'] not in SLOTS or row['wearflag']!=WEAR[row['subtype']]:raise MigrationError('unsupported_armor_slot')
        if any(row['value'+str(i)] for i in (0,2,4)) or row['subtype']!=0 and row['value3']:raise MigrationError('unmapped_armor_value')
        if row['subtype'] in (3,5,6) and (row['value1'] or row['value5']):raise MigrationError('unmapped_accessory_value')
        if row['value1']<0 or row['value5']<0:raise MigrationError('negative_defense')
    elif row['type']==10:
        if row['vnum']!=28960 and (row['subtype'] not in (0,1) or not row['wearflag']&17 or row['wearflag']&~17):raise MigrationError('unsupported_stone_target')
        if any(row['value'+str(i)] for i in range(5)):raise MigrationError('unmapped_stone_value')
    else:raise MigrationError('not_standard_equipment')
    effects(row)


def asset(project,relative):
    path=Path(project)/relative;meta=path.with_name(path.name+'.meta')
    if not path.is_file() or not meta.is_file():raise MigrationError('missing_equipment_asset')
    ids=re.findall(r'^guid: ([a-f0-9]{32})$',meta.read_text(encoding='utf-8-sig'),re.M)
    if len(ids)!=1:raise MigrationError('equipment_asset_guid')
    return dict(path=relative,guid=ids[0],sha256=file_hash(path),meta_sha256=file_hash(meta))


def appearance(project,row):
    prefix='Assets/Metin2/AddressableContent/Equipment/'
    if row['type']==1:
        name='w'+str(row['vnum']);return 'Metin2Equipment/'+name,[asset(project,prefix+name+'.asset')]
    if row['type']==2 and row['subtype']==0:
        # Literal addresses resolve authored assets; no typed-key -> old-vnum
        # inference exists at runtime. {race} is the existing native binder API.
        files=[];race_by_class=((0,4),(5,1),(2,6),(7,3))
        classes=15 & ~(row['antiflag']>>2);sexes=3 & ~((row['antiflag']&1)<<1|(row['antiflag']&2)>>1)
        for c in range(4):
            for sex in range(2):
                if classes>>c&1 and sexes>>sex&1:files.append(asset(project,prefix+'a'+str(race_by_class[c][sex])+'_'+str(row['value3'])+'.asset'))
        return 'Metin2Equipment/a{race}_'+str(row['value3']),files
    return None,[]


def stone_families(tables,plan):
    mapping={r['source_id']:copy.deepcopy(r) for r in plan['mapping']};groups=defaultdict(list)
    for row in tables['player.item_proto']:
        if row['type']!=10 or row['vnum']==28960:continue
        level,error=source_level(row)
        # The old insertion implementation compares value5, not item numbers.
        # Combine only an identical effect group, target policy and named grade.
        names=name_parts(row.get('locale_name_en'))[0]
        if not names:continue
        fixed=tuple(row[n] for n in ('subtype','wearflag','flag','antiflag','immuneflag','size','weight','value5'))
        applies=tuple(row[f'applytype{i}'] for i in range(5))
        if error or level is None:continue
        groups[(fixed,applies,names)].append((level,row))
    result=[];used=set()
    for rows in groups.values():
        rows.sort(key=lambda p:p[0]);levels=[n for n,_ in rows]
        if levels!=list(range(len(rows))):continue
        root=rows[0][1];ref=mapping[root['vnum']]
        for level,row in rows:
            mapping[row['vnum']].update(target=ref['target'],content_code=ref['content_code'],refinement_level=level)
            used.add(row['vnum'])
        result.append(dict(key=ref['target'],content_code=ref['content_code'],source_ids=[r['vnum'] for _,r in rows],levels=levels,status='candidate',review='explicit_source_effect_group_and_contiguous_named_grades'))
    for row in tables['player.item_proto']:
        if row['type']==10 and row['vnum'] not in used:
            ref=mapping[row['vnum']];result.append(dict(key=ref['target'],content_code=ref['content_code'],source_ids=[row['vnum']],levels=[ref['refinement_level']],status=ref['status']))
    return mapping,result


def attribute_document(tables,row):
    slot='weapon' if row['type']==1 else {0:'body',1:'head',2:'shield',3:'wrist',4:'foots',5:'neck',6:'ear',7:'talisman'}[row['subtype']]
    candidates=[];values=[];groups=[]
    for pool,table,capacity in ((1,'player.item_attr',5),(2,'player.item_attr_rare',2),(3,'player.item_attr_rare_8',1)):
        active=[]
        seen=set()
        for source in tables.get(table+'.effective',tables.get(table,[])):
            maximum=source.get(slot,0)
            if slot=='talisman':maximum=int(maximum)&255 # Database loader stores eligibility in unsigned char.
            if not source['prob'] or not maximum:continue
            if not 1<=maximum<=255:raise MigrationError('unsupported_attribute_tiers')
            # The old ordinary-item roll is capped at five; values 6+ in the
            # source eligibility byte do not select the costume-only columns.
            maximum=min(maximum,5)
            identity=source.get('apply_index',APPLY_NAMES.get(source['apply']))
            if identity is None:raise MigrationError('unported_attribute_name:'+source['apply'])
            if identity in seen:raise MigrationError('ambiguous_attribute_pool_requires_source_loader_capture')
            seen.add(identity)
            try:entries=[bonus(identity,source['lv'+str(i)]) for i in range(1,maximum+1)]
            except ValueError as e:raise MigrationError(str(e)) from None
            if any(e is None or int(e['value_scaled'])==0 for e in entries):raise MigrationError('zero_attribute_tier')
            key=entries[0]['bonus_id']
            active.append(dict(pool_kind=str(pool),bonus_id=key,weight=str(source['prob'])))
            values.extend(dict(pool_kind=str(pool),bonus_id=key,tier=str(i),value_scaled=e['value_scaled']) for i,e in enumerate(entries,1))
        if active:
            candidates.extend(active);groups.append(dict(pool_kind=str(pool),capacity=str(capacity),exclude_static=str(int(pool==1))))
    # The source addon independently supplies these two signed values; they
    # must be legal stored attributes even when absent from the random pool.
    if row['type']==1:
        for apply in (71,72):
            entry=bonus(apply,1)
            if not any(c['pool_kind']=='1' and c['bonus_id']==entry['bonus_id'] for c in candidates):
                candidates.append(dict(pool_kind='1',bonus_id=entry['bonus_id'],weight='0'))
                values.append(dict(pool_kind='1',bonus_id=entry['bonus_id'],tier='1',value_scaled=entry['value_scaled']))
        if not any(g['pool_kind']=='1' for g in groups):groups.append(dict(pool_kind='1',capacity='5',exclude_static='1'))
    # Historical instances may predate today's roll table. Admit only the
    # actual stored native bonuses, with zero probability for future rolls.
    protos={r['vnum']:r for r in tables['player.item_proto']}
    present={(c['pool_kind'],c['bonus_id']) for c in candidates}
    for store in ('player.item','player.guild_renewal_storage'):
        for item in tables.get(store,[]):
            proto=protos.get(item['vnum'])
            if not proto or proto['type']!=row['type'] or row['type']==2 and proto['subtype']!=row['subtype']:continue
            for i in range(8):
                source,value=item.get(f'attrtype{i}',0),item.get(f'attrvalue{i}',0)
                if not source or not value:continue
                try:entry=bonus(source,value)
                except ValueError:continue # unknown semantics still block that instance
                pool=str(1 if i<5 else 2 if i<7 else 3);key=(pool,entry['bonus_id'])
                if key in present:continue
                present.add(key);candidates.append(dict(pool_kind=pool,bonus_id=key[1],weight='0'))
                values.append(dict(pool_kind=pool,bonus_id=key[1],tier='1',value_scaled=entry['value_scaled']))
                if not any(g['pool_kind']==pool for g in groups):groups.append(dict(pool_kind=pool,capacity=str({ '1':5,'2':2,'3':1}[pool]),exclude_static=str(int(pool=='1'))))
    return dict(attribute_profile=dict(allow_lock='0' if slot=='talisman' else '1') if any(g['pool_kind']=='1' for g in groups) else None,
                attribute_groups=groups,attribute_candidates=candidates,attribute_values=values)


def install_accessory_profile(model):
    rows=model.tables['accessory_socket_profile'];identity=str(max((int(r['profile_id']) for r in rows),default=0)+1)
    model.replace('accessory_socket_profile',{'profile_id':identity},[dict(profile_id=identity,code='imported.accessory_lifetime_'+identity,max_grade='3',unlock_units_per_attempt='1',charge_units_per_attempt='1')])
    times=(0,86400,43200,21600);pct=(0,1000,2000,4000)
    grades=[dict(profile_id=identity,grade=str(i),duration_ms=str(times[i]*1000),bonus_increase_bp=str(pct[i]),minimum_raw=str(i*1000),minimum_bp=str(i*100),minimum_integer=str(i),unlock_chance_bp='0',charge_chance_bp='0',truncate_bonus='1') for i in range(4)]
    model.replace('accessory_socket_grade',{'profile_id':identity},grades)
    return identity


def compile_equipment(tables,plan,preserved,project,visuals,locales):
    by_id={r['vnum']:r for r in tables['player.item_proto']};mapping,stones=stone_families(tables,plan)
    families=[f for f in plan['families'] if f['key'] and f['key']['category_id'] in (1,2,3)]+stones
    model=Catalog(copy.deepcopy(preserved));accepted=[];blocked=[];localization={};assets={};socket_profile=None;attribute_profiles={}
    extra={r['vnum'] for r in tables.get('player.item_extra_proto',[])}
    source_groups={n:'stone.imported_group_'+str(i+1) for i,n in enumerate(sorted({r['value5'] for r in by_id.values() if r['type']==10 and r['vnum']!=28960}))}
    for family in families:
        ids=family['source_ids'];root=by_id[ids[0]]
        checkpoint=None;previous_socket=socket_profile
        try:
            if family['status']!='candidate' or family['levels']!=list(range(len(ids))):raise MigrationError('incomplete_or_unavailable_family')
            for identity in ids:
                row=by_id[identity];policy(row)
                if identity in extra:raise MigrationError('extra_proto_conversion_required')
            if root['type'] not in (1,2,10):raise MigrationError('not_standard_equipment')
            if root['type']==2 and any(by_id[i]['value3']!=root['value3'] for i in ids):raise MigrationError('changing_body_appearance')
            icon=visuals.get(root['vnum'])
            if not icon or not icon.get('iconPath','').startswith('icon/item/') or not icon['iconPath'].endswith('.tga'):raise MigrationError('missing_icon')
            for identity in ids:
                if visuals.get(identity,{}).get('iconPath')!=icon['iconPath']:raise MigrationError('changing_family_icon')
            icon_asset=verified_asset(project,'Assets/Metin2/Generated/ItemIcons/'+icon['iconPath'],icon.get('icon'))
            address,models=appearance(project,root)
            # New identities only: the editor replaces table lists/rows instead
            # of modifying issued rows. Keep a cheap list savepoint per family.
            checkpoint={name:list(rows) for name,rows in model.tables.items()}
            category=family['key']['category_id'];document=model.new(category);base=document['base'];code=family['content_code']
            base.update(local_id=str(family['key']['local_id']),content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',
                        icon_asset=icon['iconPath'],inventory_height=str(root['size']),min_level=str(minimum_level(root)),buy_amount=str(root['gold']),sell_amount=str(root['shop_buy_price']),
                        max_stack=str(SOURCE_STACK_LIMIT if category==7 and root['flag']&4 else 1))
            document['child']['local_id']=base['local_id']
            document['use'].update(allowed_classes=str(15 & ~(root['antiflag']>>2)),allowed_sexes=str(3 & ~((root['antiflag']&1)<<1|(root['antiflag']&2)>>1)))
            document['trade'].update(allow_trade=str(int(not root['antiflag']&8192)),allow_drop=str(int(not root['antiflag']&128)),allow_sell=str(int(not root['antiflag']&256)))
            document['appearance']=dict(model_asset=address) if address else None
            if category==1:
                subtype={0:1,1:2,2:3,3:4,4:5,5:6,7:7}[root['subtype']];document['child']['weapon_subtype']=str(subtype)
                if subtype==3:raise MigrationError('bow_ammo_policy_conversion_required')
                physical,magical=fixed_attack(root);document['child'].update(physical_attack_base_raw=str(physical),magical_attack_base_raw=str(magical))
            if category in (2,3):document['child']['equip_slot']=str(SLOTS[root['subtype']])
            if category==3:
                if socket_profile is None:socket_profile=install_accessory_profile(model)
                document['child']['socket_profile_id']=socket_profile
            if category==7:
                broken=root['vnum']==28960
                document['child'].update(stone_kind='2' if broken else '1',allowed_targets='0' if broken else str((1 if root['wearflag']&16 else 0)|(2 if root['wearflag']&1 else 0)),minimum_socket_grade='0' if broken else '1',primary_group='' if broken else source_groups[root['value5']],secondary_group='')
            attribute_kind=('weapon' if category==1 else root['subtype']) if category in (1,2,3) else None
            if attribute_kind is not None and attribute_kind not in attribute_profiles:document.update(attribute_document(tables,root))
            original=effects(root);document['bonuses']=[dict(bonus_id=str(k),value_scaled=str(v)) for k,v in sorted(original.items()) if v]
            document['refinement']['max_level']=str(len(ids)-1);document['levels']=[]
            previous_attack=(physical,magical) if category==1 else None
            for level,identity in enumerate(ids):
                row=by_id[identity];curve=dict(level=str(level),physical_attack_gain_raw='0',magical_attack_gain_raw='0',physical_defense_gain_raw='0',magical_defense_gain_raw='0',extra_bonus_profile_id=None,
                                             minimum_level=str(minimum_level(row)),buy_amount=str(row['gold']),sell_amount=str(row['shop_buy_price']),bonuses=[])
                if category==1:
                    p,m=fixed_attack(row)
                    if p<previous_attack[0] or m<previous_attack[1]:raise MigrationError('decreasing_weapon_curve')
                    previous_attack=(p,m)
                    curve.update(physical_attack_gain_raw=str(p-physical),magical_attack_gain_raw=str(m-magical))
                now=effects(row)
                curve['bonuses']=[dict(bonus_id=str(k),value_scaled=str(now[k]-original[k])) for k in sorted(set(now)|set(original)) if now[k]!=original[k]]
                document['levels'].append(curve)
            model.apply(document,encode_result=False)
            if attribute_kind in attribute_profiles:
                model.replace('item_attribute_binding',{'category_id':str(category),'local_id':base['local_id']},[dict(category_id=str(category),local_id=base['local_id'],profile_id=attribute_profiles[attribute_kind])])
            text=translation_for(root,code,locales)
            for language in text['texts'].values():language['name']=re.sub(r'\s*\+\s*[0-9]+\s*$','',language['name']).strip()
            localization[code]=text;assets[code]=dict(icon=icon_asset,equipment=models)
            if attribute_kind is not None and attribute_kind not in attribute_profiles:
                binding=next(r for r in model.tables['item_attribute_binding'] if r['category_id']==str(category) and r['local_id']==base['local_id'])
                attribute_profiles[attribute_kind]=binding['profile_id']
            for identity in ids:accepted.append(dict(source_id=identity,item=family['key'],refinement_level=mapping[identity]['refinement_level'],source_row_hash=value_hash(by_id[identity]),semantics='ordinary_equipment_existing_state',reviewed=False,
                                                     source_birth_policy={n:by_id[identity][n] for n in ('magic_pct','socket_pct','addon_type')},material_services_pending=category==3))
        except (MigrationError,ValueError) as e:
            if checkpoint is not None:model.tables=checkpoint
            socket_profile=previous_socket
            blocked.extend(dict(source_id=i,source_row_hash=value_hash(by_id[i]),reasons=[str(e)]) for i in ids)
    packet=copy.deepcopy(preserved);packet['tables']=model.encode();packet.pop('hash',None)
    for category,counter in plan['next_local_ids'].items():packet['sequences'][category]=str(max(int(counter),int(packet['sequences'][category])))
    return dict(packet=packet,accepted=accepted,blocked=blocked,mapping=[mapping[i] for i in sorted(mapping)],localization=localization,assets=assets,
                stone_grades={str(r['vnum']):1 for r in by_id.values() if r['type']==10},
                installable_release=False,summary=dict(converted_source_rows=len(accepted),blocked_source_rows=len(blocked),fixed_damage='exact_mean_plus_refinement',blocked_reasons=dict(Counter(reason for r in blocked for reason in r['reasons']))))


def convert_instances(rows,compiled,owner):
    ready={r['source_id']:r for r in compiled['accepted']};mapped={r['source_id']:r for r in compiled['mapping']};items=[]
    for row in sorted(rows,key=lambda r:r['id']):
        if set(row)!=SOURCE_ITEM_FIELDS or row['owner_id']!=owner:raise MigrationError('incomplete_instance_source')
        ref=ready.get(row['vnum'])
        if ref is None:raise MigrationError('uncompiled_instance_prototype:'+str(row['vnum']))
        key=ref['item'];attrs=[]
        for i in range(8):
            try:attrs.append(bonus(row[f'attrtype{i}'],row[f'attrvalue{i}']))
            except ValueError as e:raise MigrationError(str(e)) from None
        lock=row['lockedattr']
        if lock!=-1 and (not 0<=lock<5 or attrs[lock] is None):raise MigrationError('invalid_locked_attribute')
        stones=copy.deepcopy(ZERO_STONES);accessory=None
        if key['category_id']==3:
            grade,capacity,time=(row[f'socket{i}'] for i in range(3))
            if not 0<=grade<=capacity<=3:raise MigrationError('invalid_accessory_grade')
            permanent=time==86410
            if not grade and time or grade and not permanent and not 0<time<=(0,86400,43200,21600)[grade]:raise MigrationError('invalid_accessory_lifetime')
            accessory=dict(grade=grade,capacity=capacity,remaining_ms=0 if permanent else time*1000,permanent=permanent)
        elif key['category_id'] in (1,2):
            for i in range(3):
                value=row[f'socket{i}']
                if value==0:continue
                if value in (1,2):stones[i]=dict(state=2,grade=value,level=0,stone=None)
                elif value==28960:stones[i]=dict(state=4,grade=1,level=0,stone=None) # legacy cleaning restores a normal slot
                else:
                    stone=ready.get(value)
                    if stone is None or stone['item']['category_id']!=7:raise MigrationError('uncompiled_embedded_stone:'+str(value))
                    stones[i]=dict(state=3,grade=compiled.get('stone_grades',{}).get(str(value),1),level=stone['refinement_level'],stone=stone['item'])
        elif any(row[f'socket{i}'] for i in range(3)):raise MigrationError('unsupported_stone_item_state')
        if row['window']=='EQUIPMENT':
            if row['pos'] not in SOURCE_SLOTS:raise MigrationError('unsupported_equipment_slot')
            placement=dict(kind=2,storage_id=1,anchor=SOURCE_SLOTS[row['pos']])
        elif row['window'] in ('INVENTORY','EXTRA_INVENTORY'):placement=dict(kind=1,storage_id=1 if row['window']=='INVENTORY' else 2,anchor=row['pos'])
        else:raise MigrationError('unsupported_source_window')
        items.append(dict(instance_id='0',category_id=key['category_id'],local_id=key['local_id'],refinement_level=ref['refinement_level'],count=row['count'],
                          placement=placement,attributes=attrs,locked_slot=None if lock==-1 else lock,stones=stones,accessory=accessory,source=row))
    return items


def main():
    """Write an offline, natively validated candidate; never connect to SQL."""
    import argparse
    from legacy_item_migration import read_checkpoint,strict_json,analyze
    from upgrade_exported_catalog import native
    from convert_quest_tokens import read_visuals,read_locales
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','mapping','preserved','attributes','project','helper','output'):
        parser.add_argument('--'+name,required=True,type=Path)
    parser.add_argument('--owner',type=int,help='Only emit instances if this entire legacy owner is equipment-closed')
    args=parser.parse_args();tables,manifest=read_checkpoint(args.source)
    plan=strict_json(args.mapping.read_text(encoding='utf-8'))
    if plan!=analyze(tables,value_hash(dict(source_manifest=value_hash(manifest))),manifest.get('owner_keys')):
        raise MigrationError('Full source mapping is stale or edited')
    supplement=strict_json(args.attributes.read_text(encoding='utf-8'))
    if supplement.get('checkpoint')!=manifest['files']['checkpoint.sql']:raise MigrationError('Attribute loader capture belongs to another checkpoint')
    expected={name+'.effective' for name in ('player.item_attr','player.item_attr_rare','player.item_attr_rare_8')}
    if set(supplement['tables'])!=expected:raise MigrationError('Incomplete effective attribute pools')
    for table,rows in supplement['tables'].items():
        raw=table.removesuffix('.effective');source={value_hash(r) for r in tables[raw]};seen=set()
        metadata=strict_json((args.source/manifest['tables'][raw]['file']).read_text(encoding='utf-8'))
        enum=next(r['sql_type'] for r in metadata['columns'] if r['name']=='apply')
        labels=re.findall(r"'([^']*)'",enum)
        for row in rows:
            plain=dict(row);index=plain.pop('apply_index')
            if type(index) is not int or not 1<=index<=len(labels) or labels[index-1]!=plain['apply'] or index in seen or value_hash(plain) not in source:
                raise MigrationError('Effective attribute pool differs from captured SQL enum/rows')
            seen.add(index)
        if {r['apply'] for r in rows}!={r['apply'] for r in tables[raw]}:raise MigrationError('Effective attribute pool lost a source identity')
        tables[table]=rows
    packet=strict_json(args.preserved.read_text(encoding='utf-8'));packet=packet.get('packet',packet)
    blank=native(args.helper,dict(action='blank'))
    if packet['columns']!=blank['columns']:raise MigrationError('Upgrade the preserved native catalog before equipment conversion')
    native(args.helper,dict(action='validate',tables=packet['tables']))
    visuals=read_visuals(args.project/'Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset');locales,locale_hashes=read_locales(args.project)
    result=compile_equipment(tables,plan,packet,args.project,visuals,locales)
    result['packet']['hash']=native(args.helper,dict(action='validate',tables=result['packet']['tables']))['hash']
    result['input_hashes']=dict(source_manifest=value_hash(manifest),mapping=file_hash(args.mapping),preserved=file_hash(args.preserved),attribute_capture=file_hash(args.attributes),locales=locale_hashes)
    result['scope']='ordinary_equipment_and_complete_equipment_owner_states'
    accepted={r['source_id']:r for r in result['accepted']};blocked={r['source_id']:r for r in result['blocked']}
    result['prototype_ledger']=[dict(source_id=r['source_id'],target=r['target'],level=r['refinement_level'],compiled=r['source_id'] in accepted,
        blockers=blocked[r['source_id']]['reasons'] if r['source_id'] in blocked else [] if r['source_id'] in accepted else ['outside_equipment_scope']) for r in result['mapping']]
    result['instances']=[]
    if args.owner is not None:
        rows=[r for r in tables['player.item'] if r['owner_id']==args.owner]
        if not rows:raise MigrationError('Owner has no captured source inventory')
        result['instances']=convert_instances(rows,result,args.owner)
    with args.output.open('x',encoding='utf-8',newline='\n') as output:
        json.dump(result,output,ensure_ascii=True,sort_keys=True,indent=2);output.write('\n')
    print(json.dumps(dict(result['summary'],catalog_hash=result['packet']['hash'],instances=len(result['instances']),installable_release=False)))


if __name__=='__main__':main()
