"""Source weapon visuals: decode the source values/variant rule offline only."""
import json,re
from legacy_item_migration import MigrationError,value_hash
from convert_legacy_equipment import asset

# Native Sword,Dagger,Bow,TwoHanded,Bell,Fan -> original value slot.
VALUE_ORDER=(0,2,3,1,4,5)
FIELDS=('weapon_sword_effect_id','weapon_dagger_effect_id','weapon_bow_effect_id','weapon_two_handed_effect_id','weapon_bell_effect_id','weapon_fan_effect_id','weapon_dagger_alternate_effect_id','weapon_fan_alternate_effect_id')

def alternate_weapon_effect(row):
    # InventorySystem::UpdateCharacter visual selection. There is no source ID
    # dispatch in the native game runtime; this becomes per-refinement policy.
    n=row['vnum']
    return row['type']==1 and (1090<=n<=1099 or 1180<=n<=1189 or n in (1199,1209,1219,1229,40099) or 7190<=n<=7199)

def convert_weapon_effect(c,row):
    if row['type']!=28 or row['subtype']!=9:return False
    values=[row['value'+str(n)] for n in VALUE_ORDER]
    values += [row['value2']+500,row['value5']+500]
    if any(not 0<n<=65535 for n in values):raise MigrationError('invalid_weapon_costume_effect')
    mapping_path='Assets/Metin2/Resources/Metin2CostumeEffectCatalog.json'
    if not (c.project/mapping_path).is_file():raise MigrationError('missing_costume_effect_catalog')
    mapping=json.loads((c.project/mapping_path).read_text(encoding='utf-8-sig'))
    palette={entry['id']:entry for entry in mapping['entries']}
    if len(palette)!=len(mapping['entries']):raise MigrationError('duplicate_costume_effect_palette_id')
    selected=set(values);pairs=[p for p in mapping.get('leftHandPairs',[]) if p['right'] in selected]
    selected.update(p['left'] for p in pairs);proofs=[]
    for identity in sorted(selected):
        entry=palette.get(identity)
        if not entry or not entry.get('catalogRegistered'):raise MigrationError('missing_weapon_costume_effect_mapping:'+str(identity))
        if entry['bone'] not in ('PART_WEAPON','PART_WEAPON_LEFT'):raise MigrationError('invalid_weapon_costume_effect_attachment:'+str(identity))
        path=entry['path']
        if not path.endswith('.mse') or '..' in path or path.startswith(('/', '\\')):raise MigrationError('invalid_weapon_costume_effect_path')
        definition_path='Assets/Metin2/Generated/Effects/Definitions/'+path[:-4]+'.asset';definition=asset(c.project,definition_path)
        if re.search(r'(?m)^\s*status: (?:missing|ambiguous)\s*$',(c.project/definition_path).read_text(encoding='utf-8-sig')):raise MigrationError('missing_weapon_costume_effect_resource:'+str(identity))
        proofs.extend([dict(path=mapping_path,entry_sha256=value_hash(entry)),definition])
    proofs.extend(dict(path=mapping_path,pair_sha256=value_hash(pair)) for pair in pairs)
    from convert_shop_content import clock
    _,clock_kind,duration=clock(row);doc,icon=c.base(row,4,allow_clock=True)
    doc['child'].update(costume_kind='7',weapon_subtype=None,clock_kind=str(clock_kind),duration_ms=str(duration),companion_form_id=None,body_effect_id='0',**{key:str(value) for key,value in zip(FIELDS,values)})
    doc['visuals']=[];c.finish(row,doc,icon,proofs,'costume_weapon_effect');return True
