"""Offline source semantics for already implemented native item operations.

No source discriminators or item numbers are emitted as runtime dispatch data.
The per-item source IDs used here select audited historical exceptions only.
"""
import re
from pathlib import Path
from legacy_item_migration import MigrationError,file_hash
from legacy_equipment_rules import APPLY_NAMES,bonus

SERVER=Path(__file__).resolve().parents[2]/'Server'

def source_reroll_group(identity):
    # ItemSystem_LegacyBridge.cpp ENABLE_CHANGE_ATTRIBUTE_RULES. This is an
    # offline classification; runtime admission uses native policy fields.
    defines='\n'.join((SERVER/p).read_text(encoding='utf-8') for p in ('common/service.h','common/CommonDefines.h'))
    active=set(re.findall(r'^\s*#define\s+(\w+)',defines,re.M))
    if 'ENABLE_CHANGE_ATTRIBUTE_RULES' not in active:return 1
    if 'DISABLE_ZODIAC_ATT' in active:return 2 if identity==12314141 else 1
    ranges=((19290,19312),(19490,19512),(19690,19712),(19890,19912),(300,319),(1180,1189),(2200,2209),(3220,3229),(5160,5169),(7300,7309),(8500,8569),(8640,8739))
    single={329,339,349,359,369,379,389,399,1199,1209,1219,1229,2219,2229,2239,2249,3239,3249,3259,3269,5179,5189,5199,5209,7319,7329,7339,7349}
    return 2 if identity in single or any(a<=identity<=b for a,b in ranges) else 1

def active_apply_array(header,symbol,width,count):
    defines=set()
    inputs={}
    for name in ('common/service.h','common/CommonDefines.h'):
        path=SERVER/name;inputs[name]=file_hash(path)
        defines.update(re.findall(r'^\s*#define\s+(\w+)',path.read_text(encoding='utf-8',errors='replace'),re.M))
    path=SERVER/header;inputs[header]=file_hash(path)
    text=path.read_text(encoding='utf-8',errors='strict');start=text.index(symbol)
    body=text[text.index('{',start):text.index('};',start)]
    active=True;stack=[];tokens=[]
    for line in body.splitlines():
        directive=re.match(r'\s*#\s*(ifdef|ifndef|else|endif)\b\s*(\w*)',line)
        if directive:
            kind,name=directive.groups()
            if kind in ('ifdef','ifndef'):stack.append((active,name in defines));active=active and ((name in defines)==(kind=='ifdef'))
            elif kind=='else':active=stack[-1][0] and not active
            else:active=stack.pop()[0]
            continue
        if line.lstrip().startswith('#'):raise MigrationError('unsupported_balance_preprocessor')
        if active:tokens.extend(re.findall(r'APPLY_\w+|-?\d+',line.split('//')[0]))
    if stack or len(tokens)!=width*count:raise MigrationError('source_balance_table_shape:'+symbol)
    result=[]
    for n in range(count):
        row=tokens[n*width:(n+1)*width];name=row[0].removeprefix('APPLY_')
        if name not in APPLY_NAMES:raise MigrationError('unported_balance_apply:'+name)
        result.append((APPLY_NAMES[name],[int(x) for x in row[1:]]))
    return result,inputs

def unused_values(row,allowed=()):
    if any(row['value'+str(i)] for i in range(6) if i not in allowed):raise MigrationError('unmapped_behavior_value')

def convert_behavior(c,row):
    kind,sub=row['type'],row['subtype']
    if kind==3 and sub in (39,40):
        unused_values(row)
        doc,icon=c.base(row,20)
        if doc['bonuses']:raise MigrationError('unexpected_costume_grant_static_bonus')
        from legacy_equipment_rules import APPLY_NAMES
        value=5 if sub==39 else 10
        doc['base']['max_stack']='1';doc['child'].update(operation_kind='4',units='1',parameter='0',bonus_id=None,value_scaled=None)
        names=('ATTBONUS_MONSTER','ATTBONUS_BOSS','ATTBONUS_METIN','ATTBONUS_HUMAN','RESIST_MEZZIUOMINI')
        doc['bonus_choices']=[dict(choice_no=str(n),**bonus(APPLY_NAMES[name],value)) for n,name in enumerate(names,1)]
        c.finish(row,doc,icon,[],'random_costume_bonus');return True
    if kind==3 and sub in (20,21):
        unused_values(row,(0,))
        if row['value0'] not in (0,1) or sub==20 and row['value0']:raise MigrationError('invalid_accessory_material_mode')
        doc,icon=c.base(row,6)
        if doc['bonuses']:raise MigrationError('unexpected_accessory_material_bonus')
        doc['child']['material_kind']='5' if sub==21 else '6'
        c.finish(row,doc,icon,[],'accessory_socket_material');return True
    if kind==37 and row['vnum']==55002:
        unused_values(row)
        doc,icon=c.base(row,17)
        if doc['bonuses']:raise MigrationError('unexpected_pet_carrier_bonus')
        doc['supply']['supply_kind']='7';doc['base']['max_stack']='1'
        c.finish(row,doc,icon,[],'growth_pet_carrier');return True
    if kind==3 and sub==10 and row['vnum'] in (39069,80003,80004,80005,80006,80007,80008,39065,89003,89004,89005,89006,89007):
        unused_values(row)
        if row['shop_buy_price']<=0:raise MigrationError('currency_token_has_no_redemption_value')
        doc,icon=c.base(row,16)
        if doc['bonuses']:raise MigrationError('unexpected_currency_token_bonus')
        account=row['vnum'] in (39065,89003,89004,89005,89006,89007)
        doc['child'].update(wallet_currency='2' if account else '1',amount_per_unit=str(row['shop_buy_price']))
        c.finish(row,doc,icon,[],'account_currency_voucher' if account else 'gold_currency_token');return True
    if kind==3 and sub in (43,44):
        unused_values(row,(0,) if sub==43 else ())
        doc,icon=c.base(row,17)
        if doc['bonuses']:raise MigrationError('unexpected_pet_supply_bonus')
        if sub==43:
            if row['value0']<0:raise MigrationError('invalid_pet_extension_days')
            permanent=row['vnum']==86074
            if (row['value0']==0)!=permanent:raise MigrationError('unrecognized_permanent_pet_extension')
            doc['supply']['supply_kind']='5' if permanent else '4'
            if not permanent:
                doc['pet_extension'].update(added_ms=str(row['value0']*86400000),maximum_capacity_ms=str(365*86400000-60000))
        else:
            doc['supply']['supply_kind']='6'
            # Source number(1,100)>70, +5, capped traits 150/100/100.
            doc['pet_enchant'].update(success_bp='3000',step='5',hp_limit='150',monster_limit='100',pve_limit='100')
        c.finish(row,doc,icon,[],'growth_pet_supply');return True
    if kind==3 and sub==49:
        unused_values(row,(0,1,2) if row['vnum']==61400 else (0,))
        applies=[bonus(row['applytype'+str(n)],row['applyvalue'+str(n)]) for n in range(5) if row['applytype'+str(n)]]
        if not applies:raise MigrationError('source_fish_has_no_effect')
        if row['value0']<=0:raise MigrationError('invalid_cooked_fish_duration')
        doc,icon=c.base(row,5);doc['recovery']=doc['effect']=None;doc['bonuses']=[]
        doc.update(c.model.timed_defaults(doc['base']['local_id']))
        doc['timed']['duration_ms']=str(row['value0']*1000)
        code='catalog.fish_effect_'+applies[-1]['bonus_id'];doc['timed_group'].update(code=code,family_kind='1');doc['timed']['conflict_bonus_id']=applies[-1]['bonus_id'];doc['timed_bonuses']=applies
        group=next((r for r in c.model.tables['timed_effect_group'] if r['code']==code),None)
        if group:doc['timed_group']['group_id']=group['group_id'];doc['timed']['group_id']=group['group_id']
        c.finish(row,doc,icon,[],'cooked_fish_effect')
        if row['vnum']==61400:c.accepted[61400]['source_ignored_fish_values']=[row['value1'],row['value2']]
        return True

    if kind==12 and sub in (0,1) and c.fishing:
        unused_values(row,(0,))
        entries=[(index,f) for index,f in enumerate(c.fishing) if f.get('vnum' if sub==0 else 'dead_vnum')==row['vnum']]
        if len(entries)!=1:raise MigrationError('missing_or_ambiguous_fish_processing_definition')
        index,entry=entries[0]
        if row['value0'] not in (0,index):raise MigrationError('source_fish_index_disagrees_with_explicit_table')
        doc,icon=c.base(row,12);doc['child'].update(fishing_kind='1' if sub==0 else '2',bait_power='0')
        if doc['bonuses']:raise MigrationError('unexpected_raw_fish_bonus')
        if sub==0:
            rewards=[]
            for source,weight in ((entry.get('dead_vnum',0),31),(27799,69)):
                target=c.accepted.get(source)
                if not target:raise MigrationError('unresolved_fishing_result:'+str(source))
                if target['refinement_level']:raise MigrationError('refined_fishing_result')
                key=target['item'];rewards.append(dict(entry_no=str(len(rewards)+1),reward_kind='1',item_category_id=str(key['category_id']),item_local_id=str(key['local_id']),amount='1',weight=str(weight),rare_attribute_chance_bp='0'))
            doc['loot']=dict(pick_mode='1',entries=rewards)
        elif entry.get('grill_vnum'):
            target=c.accepted.get(entry['grill_vnum'])
            if not target:raise MigrationError('unresolved_fishing_result:'+str(entry['grill_vnum']))
            if target['refinement_level']:raise MigrationError('refined_fishing_result')
            doc['child'].update(grill_category_id=str(target['item']['category_id']),grill_local_id=str(target['item']['local_id']))
        c.finish(row,doc,icon,[],'fish_processing')
        c.accepted[row['vnum']]['source_fishing_index']=row['value0']
        if sub==1:c.accepted[row['vnum']]['source_fishing_dispatch_repaired']='grill_by_explicit_table_reference'
        return True
    if kind==3 and sub==2:
        unused_values(row,(0,1))
        selector=row['value0']
        if selector==9:
            doc,icon=c.base(row,14)
            if not 0<=row['value1']<=100:raise MigrationError('invalid_soul_scroll_chance')
            doc['base']['max_stack']=str(50000 if row['flag']&4 and not row['antiflag']&32768 else 1)
            doc['child'].update(soul_kind='2',damage_channel=None,profile_id=None,refine_chance_bp=str(row['value1']*100))
            doc['refinement']['profile_kind']='1'
            c.finish(row,doc,icon,[],'soul_refinement_scroll');return True
        presets={0:'blessing',1:'metal',2:'dragon_god',3:'musin',4:'smith_book',5:'memo',6:'black_dragon'}
        if selector not in presets:raise MigrationError('unknown_forge_scroll')
        doc,icon=c.base(row,22);doc['child'].update(c.model.forge_defaults(presets[selector])[0]);doc['forge_levels']=c.model.forge_defaults(presets[selector])[1]
        if selector==5:
            if not 0<=row['value1']<=255:raise MigrationError('invalid_memo_scroll_level')
            doc['child'].update(minimum_level=str(row['value1']),maximum_level=str(row['value1']))
        elif row['value1']:raise MigrationError('unexpected_scroll_parameter')
        c.finish(row,doc,icon,[],'forge_scroll');return True

    if kind==28 and 10<=sub<=16:
        unused_values(row,(0,));capacity=row['value0']
        if capacity<100 or capacity%100:raise MigrationError('rune_capacity_requires_exact_source_percent')
        if not hasattr(c,'rune_source'):
            c.rune_source,proof=active_apply_array('common/rune_length.h','aApplyRuneInfo',8,14);c.rule_inputs.update(proof)
        doc,icon=c.base(row,19);slot=sub-9
        limits=[row['limitvalue'+str(i)] for i in range(2) if row['limittype'+str(i)]==9]
        if any(row['limittype'+str(i)] not in (0,1,9,10) for i in range(2)):raise MigrationError('unsupported_rune_expiry')
        initial=(limits[0] or 36000) if limits else 0
        if initial>capacity:raise MigrationError('rune_initial_charge_exceeds_capacity')
        doc['child'].update(regalia_kind='2',rune_slot=str(slot),stole_grade='0',capacity_ms=str(capacity*1000 if slot<7 else 0),initial_ms=str(initial*1000 if slot<7 else 0))
        doc['regalia_rules']=[]
        for index,(apply,values) in enumerate(c.rune_source[(slot-1)*2:slot*2]):
            for threshold,value in zip((0,600,1100,2100,4100,6100,8100),values):
                b=bonus(apply,value)
                if b is None:raise MigrationError('empty_rune_bonus')
                doc['regalia_rules'].append(dict(bonus_index=str(index),variant_no='0',minimum_charge_bp=str(threshold),**b))
        if doc['bonuses']:raise MigrationError('unexpected_static_rune_bonus')
        c.finish(row,doc,icon,[],'rune');return True

    if kind==3 and sub in (46,48,41):
        unused_values(row,(0,) if sub==46 else ())
        doc,icon=c.base(row,20)
        doc['child'].update(operation_kind=str({46:2,48:1,41:5}[sub]),units='1',parameter=str(row['value0']*100 if sub==46 else 0),bonus_id=None,value_scaled=None)
        if sub==46:
            if not 1<=row['value0']<=100:raise MigrationError('invalid_rune_charge_percent')
            doc['base']['max_stack']='1'
        c.finish(row,doc,icon,[],'bonus_tool');return True

    if kind==19 and row['vnum']==50322:
        from full_catalog_polymorph import convert_original_polymorph_book
        return convert_original_polymorph_book(c,row)
    if kind==19:
        unused_values(row)
        doc,icon=c.base(row,13)
        c.finish(row,doc,icon,[],'instance_bound_polymorph_orb')
        c.accepted[row['vnum']]['requires_instance_form']=True
        return True
    if kind==3 and (sub in (19,22,35,36,37,31,38) or sub==18):
        unused_values(row,(0,))
        if sub in (31,38):operation,mask=2,8
        elif sub==18:operation,mask=2,16 if row['vnum'] in (86051,88965) else 7
        elif sub in (19,22):operation,mask=1,16 if row['vnum'] in (86050,88966,86052,88964) else 7
        else:operation,mask={35:3,36:4,37:5}[sub],7
        doc,icon=c.base(row,5);doc['recovery']=doc['effect']=None
        doc.update(c.model.attribute_service_defaults())
        doc['attribute_service'].update(operation_kind=str(operation),target_mask=str(mask),minimum_count='1' if operation==2 else '0',maximum_count='5' if operation!=1 else '3' if mask==16 else '4',add_count='1' if operation==1 else '0')
        if operation==1:
            # Source AddItemAttributeEcs, distinct from the reroll distribution.
            chances=(40,50,10,0,0)
            if sub==22:doc['attribute_service'].update(minimum_count='4',maximum_count='4',success_bp='7500' if mask==16 else '3000',consume_on_failure='1')
            if row['vnum'] in (71152,76024):doc['attribute_service']['maximum_target_level']='30'
        else:chances=(0,10,40,35,15)
        if sub==18:
            doc['attribute_service']['target_reroll_group']=str(2 if row['vnum']==86060 else 1)
            if row['vnum']==76014:chances=(0,10,50,39,1)
            if row['vnum'] in (71151,76023):doc['attribute_service']['maximum_target_level']='30'
        doc['attribute_tiers']=[dict(tier=str(i+1),chance_bp=str(n*100)) for i,n in enumerate(chances) if n] if operation in (1,2) else []
        c.finish(row,doc,icon,[],'attribute_service');return True
    if kind==17 and sub==10:
        vnum=row['vnum'];doc,icon=c.base(row,9)
        if vnum in (50301,50302,50303):
            unused_values(row,(0,1));doc.update(c.model.book_defaults('leadership'))
            low,high=row['value0'],row['value1']
            doc['book_steps']=[s for s in doc['book_steps'] if low<=int(s['current_level'])<high]
        elif vnum in (50314,50315,50316):
            unused_values(row,(0,1,2,3));low,high=row['value0'],row['value1'];chance=row['value2']
            if not 0<=low<high<=40 or not 0<chance<=100:raise MigrationError('invalid_polymorph_learning_book')
            doc['book'].update(allowed_masteries='7')
            doc['book_targets']=[dict(skill_id='129',refund_pool='0',forbidden_empire='0')]
            doc['book_steps']=[dict(current_level=str(n),next_level=str(n+1),min_character_level=str(row['value3']),required_successes='1',success_chance_bp=str(chance*100),boosted_chance_bp=str(min(100,chance+chance//2)*100),experience_cost='0',delay_minimum_ms='64800000',delay_maximum_ms='108000000',delay_on_failure='1') for n in range(low,high)]
        elif vnum in (50333,50334,50335,50336):
            unused_values(row,(0,1));doc.update(c.model.book_defaults('support'))
            if not 143<=row['value0']<=146:raise MigrationError('invalid_support_skill_book')
            doc['book_targets'][0]['skill_id']=str(row['value0'])
            # These prototypes are ITEM_SKILLBOOK: its common dispatch assigns
            # the master-book delay, rather than the USE_SPECIAL 3h branch.
            for s in doc['book_steps']:s.update(delay_minimum_ms='64800000',delay_maximum_ms='108000000')
        else:return False
        if not doc['book_steps']:raise MigrationError('empty_skill_book_level_range')
        c.finish(row,doc,icon,[],'secondary_skill_book')
        if vnum in (50314,50315,50316):c.accepted[vnum]['source_book_dispatch_repaired']='polymorph_learning_from_explicit_legacy_handler'
        return True
    return False
