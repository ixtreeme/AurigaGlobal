"""Compile each reachable source tool graph atomically into native identities."""
import re
from pathlib import Path
from legacy_item_migration import MigrationError,NAME_FIELDS,name_parts
from convert_legacy_equipment import asset

def grade(row):
    suffix=row['name'].rsplit('+',1)[-1] if '+' in row['name'] else ''
    match=re.match(r'-?[0-9]+',suffix)
    value=max(0,int(match[0])) if match else 0
    if value>255:raise MigrationError('tool_display_grade_exceeds_255')
    return value

def convert_tools(c,row):
    if row['type'] not in (13,24):return False
    server=Path(__file__).resolve().parents[2]/'Server'
    defines=set(re.findall(r'^\s*#define\s+(\w+)', '\n'.join((server/p).read_text(encoding='utf-8') for p in ('common/service.h','common/CommonDefines.h')),re.M))
    renewal='ENABLE_FISHINGROD_RENEWAL' if row['type']==13 else 'ENABLE_PICKAXE_RENEWAL'
    graph={};pending=[row['vnum']]
    while pending:
        identity=pending.pop()
        if identity in graph or identity in c.accepted:continue
        source=c.source.get(identity)
        if not source or source['type']!=row['type']:raise MigrationError('unresolved_tool_transition:'+str(identity))
        graph[identity]=source
        if source['refined_vnum']:
            pending.append(source['refined_vnum'])
            if source['value3']<100 and renewal not in defines:pending.append(source['value4'])
    created=[]
    for identity,source in sorted(graph.items()):
        if source['value0']<0 or source['value2']<0 or not 0<=source['value3']<=100:raise MigrationError('invalid_source_tool_values')
        training=bool(source['refined_vnum'])
        if training and (source['value1']<=0 or source['value2']<=0):raise MigrationError('invalid_source_tool_training')
        visual=c.visuals.get(identity,{})
        uri=visual.get('modelPath','')
        if uri not in ('item/etc/fishing_rod.gr2','item/etc/digger_1.gr2','item/etc/canna_2.gr2'):raise MigrationError('missing_tool_model:'+uri)
        name=Path(uri).stem
        model_path=('Assets/Metin2/Generated/Recovered/Tools/canna_2.prefab' if name=='canna_2' else
            'Assets/Metin2/Converted/'+str(Path(uri).with_suffix('.fbx')).replace('\\','/'))
        model=asset(c.project,model_path)
        if visual.get('model',{}).get('guid')!=model['guid']:raise MigrationError('tool_model_reference_mismatch')
        wrapper_path='Assets/Metin2/Resources/Metin2Equipment/typed_tool_'+name+'.asset'
        wrapper=asset(c.project,wrapper_path)
        # The checked Unity producer embeds the exact FBX in this wrapper.
        if model['guid'] not in (c.project/wrapper_path).read_text(encoding='utf-8-sig'):raise MigrationError('tool_wrapper_model_mismatch')
        doc,icon=c.base(source,11)
        doc['base']['max_stack']='1'
        doc['child'].update(tool_kind='2' if source['type']==13 else '1',fishing_pool='2' if source['type']==13 else '1',all_water_pools='1' if source['type']==13 else '0')
        doc['appearance']=dict(model_asset='Metin2Equipment/typed_tool_'+name)
        display=grade(source)
        level=c.model.tool_level(0,terminal=not training)
        level.update(power=str(source['value0']),display_grade=str(display),gather_bonus_bp=str((3,5,8,11,15,20,26,32,40,50)[min(display,9)]*100 if source['type']==24 else 0))
        if training:level.update(required_points=str(source['value2']),practice_numerator='1',practice_denominator=str(source['value1']),refine_chance_bp=str(source['value3']*100))
        doc['tool_levels']=[level]
        c.finish(source,doc,icon,[model,wrapper],'gathering_tool')
        c.accepted[identity]['tool_display_grade']=display
        c.accepted[identity]['source_level_issues']=[name for name in NAME_FIELDS if name_parts(source.get(name))[1] not in (None,display)]
        c.accepted[identity]['source_inactive_tool_values']={'value5':source['value5']}
        if not training:c.accepted[identity]['source_terminal_refine_disabled']='no_success_target; do_not_degrade_a_terminal_tool'
        created.append(identity)
    children={(r['category_id'],r['local_id']):r for r in c.model.tables['item_tool']}
    for identity in created:
        source=graph[identity];ref=c.accepted[identity];key=ref['item']
        child=children[str(key['category_id']),str(key['local_id'])]
        level=next(r for r in c.model.tables['tool_level'] if r['profile_id']==child['profile_id'] and r['level']=='0')
        if not source['refined_vnum']:continue
        target=c.accepted[source['refined_vnum']]
        if target['item']['category_id']!=11:raise MigrationError('tool_target_not_native_tool')
        level.update(success_category_id='11',success_local_id=str(target['item']['local_id']),success_level=str(target['refinement_level']))
        if source['value3']<100:
            if renewal in defines:level.update(failure_kind='1',failure_loss_bp='2000' if source['type']==13 else '1000')
            else:
                failure=c.accepted[source['value4']]
                if failure['item']['category_id']!=11:raise MigrationError('tool_failure_not_native_tool')
                level.update(failure_kind='2',failure_level=str(failure['refinement_level']),failure_category_id='11',failure_local_id=str(failure['item']['local_id']))
    for identity in created:c.blocked.pop(identity,None)
    return True
