"""Original three-item costume attribute transfer; source IDs stay offline."""
from pathlib import Path
from full_catalog_behaviors import unused_values
from legacy_item_migration import MigrationError,file_hash
EXCLUDED=((73001,73012),(75001,75012),(75201,75212),(73251,73262),(73501,73512),(75401,75412),(73751,73762),(75601,75612))
def convert_costume_transfer(c,row):
    if row['type']!=36:return False
    unused_values(row)
    if row['subtype']!=0:raise MigrationError('unknown_source_costume_transfer_subtype')
    path=Path(__file__).resolve().parents[2]/'Server/GameServer/item/attr_transfer.cpp';text=path.read_text(encoding='utf-8')
    if not all(token in text for token in ('prepared.attrs[5] = {}','prepared.attrs[6] = {}','ItemCost{items[0], 1}','ItemCost{items[2], 1}','GetItemSubType(items[1]) != ItemSystem::GetItemSubType(items[2])')):raise MigrationError('original_costume_transfer_contract_changed')
    c.rule_inputs['original_costume_transfer']=file_hash(path)
    doc,icon=c.base(row,20)
    if doc['bonuses']:raise MigrationError('unexpected_costume_transfer_tool_bonus')
    doc['child'].update(operation_kind='8',units='1',parameter='0',bonus_id=None,value_scaled=None)
    c.finish(row,doc,icon,[],'original_three_item_costume_attribute_transfer')
    return True

def bind_costume_transfer_targets(c):
    if not any(v.get('semantics')=='original_three_item_costume_attribute_transfer' for v in c.accepted.values()):return dict(targets=0,excluded_sources=0)
    eligible={(int(r['category_id']),int(r['local_id'])) for r in c.model.tables['item_costume'] if r['costume_kind'] in ('1','2','3')}
    eligible|={(int(r['category_id']),int(r['local_id'])) for r in c.model.tables['item_regalia'] if r['regalia_kind']=='1'}
    permitted=set();excluded=set()
    for source,ref in c.accepted.items():
        key=ref['item']['category_id'],ref['item']['local_id']
        if key not in eligible:continue
        if any(low<=source<=high for low,high in EXCLUDED):excluded.add(key)
        else:permitted.add(key)
    if excluded & permitted:raise MigrationError('ambiguous_native_costume_transfer_eligibility')
    rows=c.model.tables['costume_transfer_target'];known={(int(r['category_id']),int(r['local_id'])) for r in rows}
    if excluded & known:raise MigrationError('issued_costume_transfer_exclusion_changed')
    for category,local in sorted(permitted-known):rows.append(dict(category_id=str(category),local_id=str(local)))
    return dict(targets=len(rows),excluded_sources=len(excluded))
