"""Original polymorph practice book and real client-bound forms, offline only."""
import re,struct
from pathlib import Path
from legacy_item_migration import MigrationError,file_hash

SERVER=Path(__file__).resolve().parents[2]/'Server'

def bind_original_polymorph_forms(c):
    source=SERVER/'GameServer/entity/polymorph.cpp';text=source.read_text(encoding='utf-8')
    # These maps are the actual original per-monster book bonus selection.
    speed={int(n) for n in re.findall(r'm_mapSPDType\.insert\(std::make_pair\((\d+), \d+\)\)',text)}
    if speed!={101,1901} or re.search(r'm_map(?:ATK|DEF)Type\.insert',text):raise MigrationError('polymorph_source_bonus_maps_changed')
    for token in ('dwDuration = 10;','dwDuration = 15;','dwDuration = 20;','dwDuration = 25;','(29 + bySkillLevel)','bookLevel * 50','bookLevel + 1'):
        if token not in text:raise MigrationError('polymorph_source_practice_rule_changed')
    c.rule_inputs['GameServer/entity/polymorph.cpp']=file_hash(source)
    index=c.project/'Assets/Metin2/Resources/Metin2RacePrefabIndex.asset';match=re.search(r'^  races: ([0-9a-f]+)$',index.read_text(encoding='utf-8'),re.M)
    if not match:raise MigrationError('polymorph_client_race_index_required')
    data=bytes.fromhex(match[1]);races=sorted(set(struct.unpack('<'+str(len(data)//4)+'I',data)))
    authored_path=c.project/'Assets/Metin2/GeneratedData/Metin2RaceCatalog.asset';authored=authored_path.read_text(encoding='utf-8')
    bound={int(m[1]) for m in re.finditer(r'^  - race: (\d+)\s*\n    folder: [^\n]*\n    prefab: \{fileID: -?[1-9][0-9]*, guid: [a-f0-9]{32}, type: 3\}',authored,re.M)}
    forms={r['code']:r for r in c.model.tables['polymorph_form']};next_id=int(c.model.allocate('polymorph_form','form_id'));proof=[];missing=[]
    for race in races:
        if not 8<=race<=65535:continue
        path=c.project/'Assets/Metin2/AddressableContent/Races/Prefabs'/f'{race}.asset';asset_hash=None
        if race not in bound:
            if not path.is_file():missing.append(race);continue
            body=path.read_text(encoding='utf-8')
            if not re.search(r'^  race: '+str(race)+r'$',body,re.M) or not re.search(r'^  prefab: \{fileID: -?[1-9][0-9]*, guid: [a-f0-9]{32}, type: 3\}$',body,re.M):missing.append(race);continue
            asset_hash=file_hash(path)
        code='polymorph.original.mob_'+str(race);bonus='4' if race in speed else '1'
        if code in forms:
            row=forms[code]
            if row['mob_template_id']!=str(race) or row['book_bonus']!=bonus:raise MigrationError('issued_original_polymorph_form_changed')
        else:
            row=dict(form_id=str(next_id),code=code,mob_template_id=str(race),book_bonus=bonus);next_id+=1;c.model.tables['polymorph_form'].append(row);forms[code]=row
        proof.append(dict(race=race,form_id=int(row['form_id']),book_bonus=int(bonus),asset_sha256=asset_hash))
    return dict(runtime_race_index_sha256=file_hash(index),authoring_catalog_sha256=file_hash(authored_path),forms=proof,missing_original_bindings=missing,server_checks_real_monster_type=True)

def convert_original_polymorph_book(c,row):
    if row['vnum']!=50322 or row['type']!=19:return False
    if any(row['value'+str(n)] for n in range(6)):raise MigrationError('unexpected_original_polymorph_book_value')
    doc,icon=c.base(row,13)
    if doc['bonuses']:raise MigrationError('unexpected_original_polymorph_book_bonus')
    code='polymorph.practice_book';doc['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',max_stack='1')
    doc['poly'].update(polymorph_kind='2',mob_level_grace='0',grace_loss_numerator='0',grace_loss_denominator='1',max_book_grade='255',practice_per_grade='50',book_chance_base_percent='29')
    for r in doc['poly_skills']:r.update(duration_ms='0',attack_bonus_bp='0')
    doc['poly_masteries']=[dict(mastery=str(n),duration_ms=str(seconds*1000),bonus_duration_ms=str((seconds-1)*1000)) for n,seconds in enumerate((10,15,20,25))]
    doc['child']['fixed_form_id']=None
    c.finish(row,doc,icon,[],'original_polymorph_practice_book');c.accepted[row['vnum']].update(requires_instance_form=True,practice_issued_by_server=True,source_unused_level_limit=True)
    return True
