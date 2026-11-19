"""Compile explicit red/blue Soul transitions, preserving every grade's artwork."""
from legacy_item_migration import MigrationError

def convert_souls(c,row):
    if row['type']!=35 or row['subtype'] not in (0,1):return False
    graph={};current=row
    while current and current['vnum'] not in c.accepted:
        if current['vnum'] in graph:raise MigrationError('source_soul_transition_cycle')
        if current['type']!=35 or current['subtype']!=row['subtype']:raise MigrationError('source_soul_transition_channel')
        graph[current['vnum']]=current
        target=current['refined_vnum']
        if not target:break
        following=c.source.get(target)
        if following and (following['type']!=35 or following['subtype']!=row['subtype']):raise MigrationError('source_soul_transition_channel')
        if not following or following['value1']!=current['value1']+1:raise MigrationError('unresolved_soul_grade_transition')
        current=following
    for identity,source in sorted(graph.items()):
        capacity=source['limitvalue1'];strikes=source['value2']
        if source['value0'] or not 1<=source['value1']<=255 or capacity not in (60,120,180) or not 1<=strikes<10000 or source['limittype1']:
            raise MigrationError('unsupported_source_soul_charge_policy')
        if any(source['value'+str(i)]<10 or source['value'+str(i)]>1000 for i in (3,4,5)):raise MigrationError('invalid_source_soul_multiplier')
        if source['limittype0'] not in (0,1):raise MigrationError('unsupported_source_soul_limit')
        doc,icon=c.base(source,14)
        if doc['bonuses']:raise MigrationError('unexpected_source_soul_static_bonus')
        doc['child'].update(soul_kind='1',damage_channel=str(source['subtype']+1),next_category_id=None,next_local_id=None)
        doc['soul_levels']=[dict(level='0',charge_capacity=str(capacity),cycle_charge_cost='60',strikes_per_cycle=str(strikes),
            initial_charge='0',initial_strikes=str(strikes),recharge_interval_ms='60000',
            **{name:str(source['value'+str(i)]*1000) for name,i in [('first_multiplier_bp',3),('second_multiplier_bp',4),('third_multiplier_bp',5)]})]
        c.finish(source,doc,icon,[],'soul_core')
        c.accepted[identity]['source_soul_grade']=source['value1']
    children={(int(r['category_id']),int(r['local_id'])):r for r in c.model.tables['item_soul']}
    for identity,source in graph.items():
        ref=c.accepted[identity];child=children[ref['item']['category_id'],ref['item']['local_id']]
        if source['refined_vnum']:
            target=c.accepted.get(source['refined_vnum'])
            if not target or target['item']['category_id']!=14:raise MigrationError('unconverted_soul_refinement_target')
            child.update(next_category_id='14',next_local_id=str(target['item']['local_id']))
        c.blocked.pop(identity,None)
    return True
