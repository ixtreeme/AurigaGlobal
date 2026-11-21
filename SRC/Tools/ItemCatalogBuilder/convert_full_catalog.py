"""Compile the entire source inventory as native definitions, never as players.

Offline only. Already issued definitions are immutable. A source refinement
state can become its own native definition: incomplete chains, changed icons,
models and non-monotonic statistics do not make other source states disappear.
Every source row receives a ledger outcome; no unsupported type is replaced
with a fake potion, empty box or inert quest token.
"""
from collections import Counter
import copy,json,re,unicodedata
from pathlib import Path

from legacy_item_migration import MigrationError,NAME_FIELDS,PROTO_FIELDS,value_hash,name_parts
from convert_quest_tokens import Catalog,translation_for,verified_asset,SOURCE_STACK_LIMIT
from convert_legacy_equipment import asset,attribute_document,effects,install_accessory_profile
from legacy_equipment_rules import bonus
from convert_shop_content import clock,visuals_for,costume_attributes,stole_table,stole_rules,SASH_CATALOG,RACES
from full_catalog_behaviors import convert_behavior,source_reroll_group
from full_catalog_accessories import bind_source_accessories
from full_catalog_tools import convert_tools
from full_catalog_weapon_effects import convert_weapon_effect,alternate_weapon_effect
from full_catalog_services import convert_services
from full_catalog_icons import refinement_icon_aliases
from full_catalog_souls import convert_souls
from full_catalog_context_services import convert_context_services
from full_catalog_stoles import stole_appearance
from full_catalog_birth_attributes import bind_birth_attributes
from full_catalog_utilities import convert_utility
from full_catalog_world_rewards import bind_world_reward_availability
from full_catalog_absorption_sashes import convert_absorption_sash,bind_absorption_sashes
from full_catalog_effects import convert_original_food_bundle,bind_original_fish_conflicts
from full_catalog_polymorph import bind_original_polymorph_forms
from full_catalog_stateful_utilities import convert_stateful_utility
from full_catalog_pet_books import convert_pet_book,bind_original_pet_skills
from full_catalog_dungeon_keys import convert_dungeon_key
from full_catalog_final_policies import retired_original,PATH as RETIREMENT_POLICY_PATH
from full_catalog_money import convert_native_money
from full_catalog_private_shops import convert_private_shop_bundle
from full_catalog_energy import convert_energy
from full_catalog_costume_transfer import convert_costume_transfer,bind_costume_transfer_targets

ARMOR_SLOTS={0:2,1:3,2:4,3:6,4:5,5:7,6:8,7:21}
WEAPON_TYPES={0:1,1:2,2:3,3:4,4:5,5:6,7:7}

def source_level(row):
    """Authoritative base-name suffix, as ItemSystem::MakeProtoRef reads it.

    Translations are display text; a mistranslated +N must not change gameplay.
    Keep such discrepancies in the per-source ledger.
    """
    name=row.get('name','');suffix=name[name.rfind('+')+1:] if '+' in name else ''
    match=re.match(r'-?[0-9]+',suffix)
    level=max(0,int(match[0])) if match else 0
    if level>255:return None,'refinement_level_exceeds_255'
    return level,None

def disabled_source(row):
    decision=retired_original(row)
    if decision:return decision
    if not row['vnum']:return 'non_item_placeholder'
    if row['type'] in (29,30,31):return 'explicitly_retired_source_type'
    if row['type']==5 and row['subtype'] in (5,6,7):return 'explicitly_retired_material_type'
    if row['type']==3 and row['subtype'] in (27,28,42):return 'explicitly_retired_use_type'
    if row['type']==1 and row['subtype'] not in range(8):return 'undeclared_source_weapon_type'
    if row['type']==28 and row['subtype']>16:return 'undeclared_source_costume_type'
    if row['antiflag']&60==60:return 'no_supported_character_class'
    if row['antiflag']&3==3:return 'no_supported_character_sex'
    return None

def content_code(row,category):
    if row['vnum']==70610:return 'wheel.fortune_ticket'
    name=row.get('locale_name_en') or row.get('name') or row.get('locale_name_hu') or 'unnamed'
    name=unicodedata.normalize('NFKD',name).encode('ascii','ignore').decode().lower()
    name=re.sub('[^a-z0-9]+','_',name).strip('_') or 'item'
    return 'catalog.'+str(category)+'.'+name[:57]+'_'+value_hash(row)[:12]

class FullCatalogCompiler:
    def __init__(self,tables,preserved,project,visuals,locales,known=(),groups=(),fishing=(),skills=(),blends=()):
        self.tables=tables;self.original=copy.deepcopy(preserved);self.model=Catalog(preserved)
        self.project=Path(project);self.visuals=visuals;self.locales=locales
        self.source={r['vnum']:r for r in tables['player.item_proto']}
        if len(self.source)!=len(tables['player.item_proto']):raise MigrationError('duplicate_source_prototype')
        self.icon_aliases=refinement_icon_aliases(self.source,self.visuals,source_level)
        if tables.get('player.quest_item_proto'):raise MigrationError('additional_quest_prototypes_require_explicit_merge')
        self.extra={r['vnum']:r for r in tables.get('player.item_extra_proto',[])}
        if len(self.extra)!=len(tables.get('player.item_extra_proto',[])):raise MigrationError('duplicate_extra_prototype')
        self.accepted={};self.blocked={};self.retired={};self.texts={};self.assets={}
        self.attribute_profiles={};self.accessory_profile=None
        server=Path(__file__).resolve().parents[2]/'Server'
        from legacy_item_migration import file_hash
        self.rule_inputs={name:file_hash(server/name) for name in (
            'common/service.h','common/CommonDefines.h','common/rune_length.h','common/stole_length.h',
            'GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp','GameServer/ecs/systems/ItemSystem.cpp',
            'GameServer/ecs/systems/ItemAttributeSystem.cpp','GameServer/ecs/systems/SkillSystem.cpp','GameServer/ecs/systems/InventorySystem.cpp',
            'GameServer/core/constants.cpp','GameServer/world/fishing.cpp','GameServer/world/mining.cpp',
            'GameServer/item/item_manager.cpp','GameServer/ecs/EntityFactory.cpp',
            'GameServer/ecs/components/item_components.hpp','GameServer/item/unique_item.h',
            'GameServer/combat/skill.h','common/length.h','GameServer/combat/affect.h',
            'GameServer/ecs/systems/PlayerRuntimeSystem.cpp','GameServer/ecs/components/pet_mount_components.hpp')}
        self.bonus_bounds={int(r['id']):(int(r['minimum']),int(r['maximum'])) for r in preserved['bonuses']}
        self.groups={g['vnum']:g for g in groups}
        self.blends={r["item_vnum"]:r for r in blends}
        if len(self.blends)!=len(blends):raise MigrationError("duplicate_blend_definition")
        self.fishing=list(fishing)[:37] # fishing.cpp::MAX_FISH
        self.secondary_skills={r['skill_id']:r for r in skills}
        if len(self.secondary_skills)!=len(skills):raise MigrationError('duplicate_secondary_skill_definition')
        if len(self.groups)!=len(groups):raise MigrationError('duplicate_special_item_group')
        keys={(int(r['category_id']),int(r['local_id'])) for r in self.model.tables['item_base']}
        for ref in known:
            if ref['source_id'] not in self.source:continue
            key=ref['item'];row=self.source[ref['source_id']]
            if (int(key['category_id']),int(key['local_id'])) not in keys:raise MigrationError('issued_mapping_missing_native_key')
            if ref.get('source_row_hash') and ref['source_row_hash']!=value_hash(row):raise MigrationError('issued_source_definition_changed:'+str(row['vnum']))
            old=self.accepted.get(ref['source_id'])
            if old and (old['item']!=key or old['refinement_level']!=ref['refinement_level']):raise MigrationError('contradictory_issued_source_mapping')
            self.accepted[ref['source_id']]=copy.deepcopy(ref)
            self.accepted[ref['source_id']].setdefault('source_row_hash',value_hash(row))
        self.fish_conflicts=bind_original_fish_conflicts(self)
        self.polymorph_forms=bind_original_polymorph_forms(self) if 50322 in self.source else None
        self.families={}
        # Long talisman curves are real linked families. Keeping 201 separate
        # copies of their 0..200 structural curve wastes the capture budget.
        for root in self.source.values():
            if root['type']!=2 or root['subtype']!=7 or root['vnum'] in self.accepted:continue
            level,error=source_level(root)
            if error or level!=0:continue
            chain=[];row=root;seen=set()
            while row and row['vnum'] not in seen and row['vnum'] not in self.accepted:
                n,error=source_level(row)
                static=('type','subtype','size','weight','flag','antiflag','wearflag','immuneflag')
                if error or n!=len(chain) or any(row[k]!=root[k] for k in static):break
                if self.visuals.get(row['vnum'],{}).get('iconPath')!=self.visuals.get(root['vnum'],{}).get('iconPath'):break
                chain.append(row);seen.add(row['vnum']);row=self.source.get(row['refined_vnum'])
            if len(chain)>1:self.families[root['vnum']]=chain

    def base(self,row,category,allow_clock=False,ignore_bonuses=False):
        if set(row)!=PROTO_FIELDS:raise MigrationError('unexpected_source_columns')
        if not 1<=row['size']<=255:raise MigrationError('invalid_inventory_height')
        if row['vnum_range']:raise MigrationError('source_range_requires_expansion')
        minimum=0
        for i in range(2):
            kind,value=row['limittype'+str(i)],row['limitvalue'+str(i)]
            if kind==1:minimum=max(minimum,value)
            elif kind not in (0,6,7,8,9,10):raise MigrationError('unknown_source_limit:'+str(kind))
        doc=self.model.new(category);local=doc['base']['local_id'];code=content_code(row,category)
        doc['base'].update(content_code=code,name_key='item.'+code+'.name',description_key='item.'+code+'.description',
            inventory_height=str(row['size']),weight_grams=str(row['weight']),min_level=str(minimum),
            max_stack=str(SOURCE_STACK_LIMIT if row['flag']&4 and not row['antiflag']&32768 else 1),
            buy_amount=str(row['gold']),sell_amount=str(row['shop_buy_price']))
        if category in (1,2,3,4,11,14,15,19):doc['base']['max_stack']='1'
        doc['use'].update(allowed_classes=str(15 & ~(row['antiflag']>>2)),allowed_sexes=str(3 & ~((row['antiflag']&1)<<1|(row['antiflag']&2)>>1)))
        doc['trade'].update(allow_trade=str(int(not row['antiflag']&8192)),allow_drop=str(int(not row['antiflag']&128)),allow_sell=str(int(not row['antiflag']&256)))
        # ITEM_ANTIFLAG_SAVE is an unused prototype flag in this source build.
        # EntityFactory defaults skipSave=false; CreateItem clears its temporary
        # initialization guard before normal persistence. Do not mistake the
        # named enum for an effective non-persistence rule. Keep it in the ledger.
        if row['flag'] & 512:raise MigrationError('source_confirmation_required')
        visual=self.visuals.get(row['vnum'])
        alias=self.icon_aliases.get(row['vnum'])
        if (not visual or not visual.get('iconPath')) and alias:visual=alias['visual']
        if not visual or not visual.get('iconPath'):raise MigrationError('missing_source_icon')
        icon=visual['iconPath'];icon_file='Assets/Metin2/Generated/ItemIcons/'+icon
        # Quest icons already imported by the Unity project retain their source
        # URI in the item catalog, but live as PNG Sprite assets in Resources.
        if not (self.project/icon_file).is_file() and re.fullmatch(r'ui/game/quest/questicon/[A-Za-z0-9_]+\.tga',icon):
            icon_file='Assets/Metin2/Resources/Metin2/QuestIcons/'+Path(icon).stem+'.png'
        proof=verified_asset(self.project,icon_file,visual.get('icon'))
        if alias:proof=dict(proof,refinement_icon_source=alias['source_id'],refinement_icon_chain=alias['chain'])
        doc['base']['icon_asset']=icon
        values=Counter() if ignore_bonuses else effects(row)
        extra=self.extra.get(row['vnum'])
        if extra and not ignore_bonuses:
            for n in range(2):
                if extra['bonus_type'+str(n)]:
                    b=bonus(extra['bonus_type'+str(n)],extra['bonus_value'+str(n)])
                    if b:values[int(b['bonus_id'])]+=int(b['value_scaled'])
        doc['bonuses']=[dict(bonus_id=str(k),value_scaled=str(v)) for k,v in sorted(values.items()) if v]
        return doc,proof

    def passive(self,row):
        kind,sub=row['type'],row['subtype']
        if kind not in (0,5,6,14,18):return False
        category=6 if kind in (5,14) else 10
        doc,icon=self.base(row,category,ignore_bonuses=True)
        if category==6:doc['child']['material_kind']=str(sub+1 if kind==5 and sub<=4 else 6)
        quest_use=category==10 and kind==18 and row['vnum']!=70610
        if quest_use:
            # Quest-use intent is separate from its external quest implementation.
            doc['child'].update(dispatch_kind='3' if row['flag']&2048 else '2',group_code='',allowed_contexts='1',allow_observer='0')
        self.finish(row,doc,icon,[],'material' if category==6 else 'quest_script' if quest_use else 'passive_token')
        self.accepted[row['vnum']]['inactive_source_fields']=dict(applies=[(row['applytype'+str(i)],row['applyvalue'+str(i)]) for i in range(5)],immuneflag=row['immuneflag'],extra=copy.deepcopy(self.extra.get(row['vnum'])))
        return True

    def consumable(self,row):
        unique=row['type']==16 and row['subtype']==7
        if row['type']!=3 and not unique:return False
        sub=row['subtype']
        if sub not in (0,7,8,11,16,47,14,17,6):return False
        if sub==6:
            doc,icon=self.base(row,12)
            if row['value0']<=0:raise MigrationError('invalid_bait_power')
            doc['child'].update(fishing_kind='3',bait_power=str(row['value0']))
            self.finish(row,doc,icon,[],'fishing_bait');return True
        doc,icon=self.base(row,5);values=[row['value'+str(i)] for i in range(6)]
        if sub in (0,11,16):
            if any(n<0 for n in values):raise MigrationError('negative_recovery_value')
            delivery={0:2,11:1,16:3}[sub]
            doc['recovery'].update(delivery_kind=str(delivery),allowed_contexts='15' if sub==0 or sub==11 and row['vnum'] in (70020,71018,71019,71020) else '11',
                arena_quota_cost='1' if sub==0 or sub==11 and row['vnum'] in (70020,71018,71019,71020) else '0')
            if sub==11:
                if values[2] or values[5]:raise MigrationError('unmapped_instant_recovery_value')
                doc['effect']=dict(delivery_kind='1',hp_raw=str(values[0]*1000),sp_raw=str(values[1]*1000),max_hp_bp=str(values[3]*100),max_sp_bp=str(values[4]*100),flat_bonus_cap_bp='100000')
                # SeedBottleTime is written, but never read by this source
                # build. A nonfunctional timestamp is not a cooldown policy.
            elif sub==0:
                if any(values[2:]):raise MigrationError('unmapped_reserve_recovery_value')
                doc['effect']=dict(delivery_kind='2',hp_raw=str(values[0]*1000),sp_raw=str(values[1]*1000),flat_bonus_cap_bp='10000',tick_interval_ms='1000',max_pool_per_tick_bp='700')
            else:
                if any(values[3:]) or values[2]<=0:raise MigrationError('invalid_periodic_recovery')
                doc['effect']=dict(delivery_kind='3',hp_per_tick_raw=str(values[0]*1000),sp_per_tick_raw=str((0 if values[0] else values[1])*1000),tick_interval_ms='1000',duration_ms=str(values[2]*1000))
        elif sub in (7,8):
            apply=values[0] if sub==7 else values[1];duration=values[1] if sub==7 else values[3]
            if duration<=0:raise MigrationError('invalid_timed_effect_duration')
            if sub==7 and apply not in ((3,4,5,6,7,8,9,37,53,54) if unique else (3,4,5,6,7,8,9,53,54)):raise MigrationError('unsupported_ability_effect')
            effect=bonus(apply,values[2])
            if not effect:raise MigrationError('non_bonus_affect_requires_dedicated_behavior')
            doc.update(Catalog.timed_defaults(doc['base']['local_id']));doc['recovery']=doc['effect']=None
            doc['timed']['duration_ms']=str(duration*1000)
            doc['timed_group'].update(code='catalog.effect_'+('ability_' if sub==7 else 'affect_')+effect['bonus_id'],reapply_kind='2' if sub==7 else '1',remove_on_death='1')
            if unique:doc['timed_group']['code']='catalog.effect_unique_ability'
            doc['timed_bonuses']=[effect]
            group=next((r for r in self.model.tables['timed_effect_group'] if r['code']==doc['timed_group']['code']),None)
            if group:doc['timed_group']['group_id']=group['group_id'];doc['timed']['group_id']=group['group_id']
        elif sub==47:
            effect=bonus(row['applytype0'],row['applyvalue0'])
            if not effect or row['limitvalue0']<0 or row['limitvalue0']==0 and row['limittype0']!=0:raise MigrationError('invalid_toggle_effect_definition')
            # The source initializes remaining active time from limitvalue0;
            # its LIMIT_REAL_TIME label does not start a wall-clock expiry.
            doc['base']['max_stack']='1'
            doc.update(Catalog.toggle_defaults(doc['base']['local_id']));doc['recovery']=doc['effect']=None
            affect=values[0];group=min(affect,585) if affect in (585,586) else min(affect,590) if affect in (590,591) else affect
            doc['toggle_group']['code']='catalog.toggle_group_'+str(group)
            doc['toggle'].update(variant_id='2' if affect in (586,591) else '1',duration_ms=str(row['limitvalue0']*1000),requirements='1' if 593<=affect<=598 else '2' if affect==600 else '0',allowed_contexts='15',allow_during_pvp='1')
            doc['toggle_bonuses']=[effect];doc['bonuses']=[]
            existing=next((r for r in self.model.tables['toggle_effect_group'] if r['code']==doc['toggle_group']['code']),None)
            if existing:doc['toggle_group']['group_id']=existing['group_id'];doc['toggle']['group_id']=existing['group_id']
        else:
            doc['recovery']=doc['effect']=None
            doc['socket_service']=dict(service_kind='2' if sub==14 else '1',units_per_use='1')
        self.finish(row,doc,icon,[],'consumable')
        if row['vnum'] in (50085,50086):self.accepted[row['vnum']]['source_unused_seed_bottle_timestamp']=True
        return True

    def skill_book(self,row):
        forget=row['type']==22 and row['subtype']==0 and row['vnum']==70037
        if not forget and (row['type']!=17 or row['subtype']!=0):return False
        if any(row['value'+str(i)] for i in range(1,6)):raise MigrationError('unmapped_skill_book_value')
        if (row['vnum']==50300 or forget) and row['value0']==0:
            wanted=[n for start in (1,16,31,46,61,76,91,106) for n in range(start,start+6)]
            manager=(Path(__file__).resolve().parents[3]/'config/item_combat/original_item_manager.source.cpp').read_text(encoding='utf-8')
            pool=manager.split('static constexpr uint32_t skills[JOB_MAX_NUM][SKILL_GROUP_MAX_NUM][6] = {',1)[1].split('};',1)[0]
            if [int(n) for n in re.findall(r'\b[0-9]+\b',pool)]!=wanted:raise MigrationError('source_random_book_pool_contract_changed')
            available=[n for n in wanted if n in self.secondary_skills]
            if not available:raise MigrationError('missing_main_skill_definitions')
            doc,icon=self.base(row,9)
            if forget:doc.update(Catalog.book_defaults('forget'))
            doc['book']['target_mode']='2'
            doc['book_targets']=[dict(skill_id=str(n),refund_pool='1' if forget else '0',forbidden_empire='0') for n in available]
            self.finish(row,doc,icon,[],'instance_bound_forget_book' if forget else 'instance_bound_main_skill_book')
            self.accepted[row['vnum']]['source_factory_skill_pool']=available
            self.accepted[row['vnum']]['source_missing_factory_skills']=sorted(set(wanted)-set(available))
            return True
        if row['value0']<=0:raise MigrationError('skill_selected_on_instance_not_in_prototype')
        doc,icon=self.base(row,9)
        doc['book_targets'][0]['skill_id']=str(row['value0'])
        self.finish(row,doc,icon,[],'skill_book');return True

    def container(self,row):
        kind=row['type']
        if kind not in (20,21,23):return False
        doc,icon=self.base(row,18 if kind==21 else 8)
        if kind==21:
            if not 0<row['value0']<=65535:raise MigrationError('invalid_chest_lock')
            doc['child']['lock_id']=str(row['value0'])
        else:
            group=self.groups.get(row['vnum'])
            if not group:raise MigrationError('missing_source_container_reward_group')
            if group.get('type') not in (None,'','pct'):raise MigrationError('scripted_or_special_container_group')
            entries=[];symbols={'gold':2,'exp':3,'mob':4,'group':5,'slow':6,'drain_hp':7,'poison':8}
            for reward in group['items']:
                weight=reward['prob']
                # CSpecialItemGroup::AddItem ignores zero-probability entries.
                if not weight:continue
                # Reject malformed source probabilities (e.g. 30101), without
                # guessing another meaning or silently truncating/clamping them.
                if not 0<=reward.get('rare',0)<=100:raise MigrationError('source_reward_rare_field_out_of_range')
                name=reward['item'];target=None;reward_kind=symbols.get(name)
                # Source sentinel domain from CSpecialItemGroup::EGiveType.
                # These are decoded offline; no sentinel IDs enter native loot.
                if name.isdecimal() and 1<=int(name)<=7:reward_kind={1:2,2:3,3:4,4:6,5:7,6:8,7:5}[int(name)]
                if reward_kind is None:
                    if not name.isdecimal():raise MigrationError('source_loot_name_lookup_required:'+name)
                    target=self.accepted.get(int(name))
                    if not target:raise MigrationError('unresolved_container_reward:'+name)
                    reward_kind=1
                amount=reward['count']*(100 if reward_kind==7 else 1)
                entries.append(dict(entry_no=str(len(entries)+1),reward_kind=str(reward_kind),item_category_id=str(target['item']['category_id']) if target else None,
                    item_local_id=str(target['item']['local_id']) if target else None,amount=str(amount),weight=str(weight*(100 if group.get('type')=='pct' else 1)),rare_attribute_chance_bp=str(reward.get('rare',0)*100),refinement_level=str(target['refinement_level'] if target else 0)))
            if not entries:raise MigrationError('empty_source_reward_group')
            doc['loot']=dict(pick_mode='2' if group.get('type')=='pct' else '1',entries=entries)
            doc['child'].update(container_kind='2' if kind==20 else '1',lock_id=str(row['value0']) if kind==20 else '0')
        self.finish(row,doc,icon,[],'container_key' if kind==21 else 'source_reward_container');return True

    def cosmetic(self,row):
        if row['type']!=28 or row['subtype'] not in (0,1,2,4,5,6,7,8):return False
        sub=row['subtype'];category=15 if sub==2 else 19 if sub==5 else 4
        family={0:'costume_body',1:'costume_hair',2:'mount',4:'costume_weapon',5:'stole',6:'pet_skin',7:'mount_skin',8:'costume_body_effect'}[sub]
        doc,icon=self.base(row,category,allow_clock=True)
        minimum,clock_kind,duration=clock(row)
        if sub==8:
            identity=row['value0']
            if not 0<identity<=65535 or any(row['value'+str(n)] for n in range(1,6)):raise MigrationError('invalid_body_costume_effect')
            mapping_path='Assets/Metin2/Resources/Metin2CostumeEffectCatalog.json'
            if not (self.project/mapping_path).is_file():raise MigrationError('missing_costume_effect_catalog')
            mapping=json.loads((self.project/mapping_path).read_text(encoding='utf-8-sig'))
            matches=[e for e in mapping['entries'] if e['id']==identity]
            if len(matches)!=1 or not matches[0].get('catalogRegistered') or matches[0]['bone']!='Bip01':raise MigrationError('missing_body_costume_effect_mapping:'+str(identity))
            entry=matches[0];path=entry['path']
            if not path.endswith('.mse') or '..' in path or path.startswith(('/', '\\')):raise MigrationError('invalid_body_costume_effect_path')
            definition_path='Assets/Metin2/Generated/Effects/Definitions/'+path[:-4]+'.asset'
            definition=asset(self.project,definition_path)
            if re.search(r'(?m)^\s*status: (?:missing|ambiguous)\s*$',(self.project/definition_path).read_text(encoding='utf-8-sig')):raise MigrationError('missing_body_costume_effect_resource:'+str(identity))
            proofs=[dict(path=mapping_path,entry_sha256=value_hash(entry)),definition]
            doc['child'].update(costume_kind='6',weapon_subtype=None,clock_kind=str(clock_kind),duration_ms=str(duration),companion_form_id=None,body_effect_id=str(identity))
            doc['visuals']=[]
        elif sub in (6,7):
            mob=row['value0']
            if mob<=0:raise MigrationError('invalid_companion_skin_form')
            proofs=[asset(self.project,RACES+str(mob)+'.asset')]
            form=next((r for r in self.model.tables['companion_form'] if r['mob_template_id']==str(mob)),None)
            doc['child'].update(costume_kind='4' if sub==6 else '5',weapon_subtype=None,clock_kind=str(clock_kind),duration_ms=str(duration),companion_form_id=form['form_id'] if form else None)
            doc['new_companion_form']=None if form else dict(code='companion.form_race_'+str(mob),mob_template_id=str(mob),model_asset='Metin2RacePrefabs/'+str(mob))
            doc['visuals']=[]
        elif category==4:
            doc['child'].update(costume_kind={0:'1',1:'2',4:'3'}[sub],weapon_subtype=str(WEAPON_TYPES[row['value3']]) if sub==4 else None,
                                clock_kind=str(clock_kind),duration_ms=str(duration))
            doc['visuals'],proofs=visuals_for(self.project,row,family)
            pool={'costume_body':'costume_body','costume_hair':'costume_hair','costume_weapon':'costume_weapon'}[family]
            if family not in self.attribute_profiles:
                effective=dict(self.tables);effective['player.item_attr']=effective['player.item_attr.effective']
                doc.update(costume_attributes(effective,pool))
        elif category==15:
            mob=row['value1']
            if mob<=0 or row['value5'] not in (0,1):raise MigrationError('invalid_mount_definition')
            proofs=[asset(self.project,RACES+str(mob)+'.asset')]
            form=next((r for r in self.model.tables['companion_form'] if r['mob_template_id']==str(mob)),None)
            doc['child'].update(companion_kind='2',clock_kind=str(clock_kind),duration_ms=str(duration),dungeon_only_bonus='0',flying_mount=str(row['value5']))
            doc['new_companion_form']=None if form else dict(code='companion.form_race_'+str(mob),mob_template_id=str(mob),model_asset='Metin2RacePrefabs/'+str(mob))
            if form:doc['child']['form_id']=form['form_id']
            doc['riding_same_form']=True;doc['new_riding_form']=None
        else:
            grade=row['value0']
            if not 1<=grade<=4:raise MigrationError('invalid_stole_grade')
            sash=__import__('json').loads((self.project/SASH_CATALOG).read_text(encoding='utf-8-sig'))
            visual=next((r for r in sash['entries'] if r['vnum']==row['vnum']),None)
            if not visual:raise MigrationError('missing_stole_model')
            appearance,extra_proofs=stole_appearance(self.project,visual)
            source,_=stole_table(Path(__file__).resolve().parents[2]/'Server')
            doc['child'].update(regalia_kind='1',rune_slot='0',stole_grade=str(grade),capacity_ms='0',initial_ms='0')
            doc['appearance']=dict(model_asset=appearance);doc['regalia_rules']=stole_rules(source,grade)
            proofs=[dict(path=SASH_CATALOG,entry_sha256=value_hash(visual)),*extra_proofs]
        self.finish(row,doc,icon,proofs,family)
        if category==4 and sub not in (6,7,8):
            key={'category_id':'4','local_id':doc['base']['local_id']}
            if family in self.attribute_profiles:self.model.replace('item_attribute_binding',key,[dict(**key,profile_id=self.attribute_profiles[family])])
            else:
                binding=next((r for r in self.model.tables['item_attribute_binding'] if all(r[k]==v for k,v in key.items())),None)
                if binding:self.attribute_profiles[family]=binding['profile_id']
        return True

    def equipment(self,row):
        kind,sub=row['type'],row['subtype']
        if kind==16 and sub in (7,10):return False
        if kind==1:category=21 if sub==6 else 1
        elif kind==2:category=3 if sub in (3,5,6) else 2
        elif kind in (16,33,34):category=3
        elif kind==10:category=7
        else:return False
        doc,icon=self.base(row,category);proofs=[]
        if category==1:
            if sub not in WEAPON_TYPES:raise MigrationError('undeclared_weapon_subtype')
            # The source state's fixed values are independent from its neighbour.
            physical=(row['value3']+row['value4'])*500+row['value5']*1000
            magical=(row['value1']+row['value2'])*500+row['value5']*1000
            if physical<0 or magical<0:raise MigrationError('negative_source_attack')
            doc['child'].update(weapon_subtype=str(WEAPON_TYPES[sub]),physical_attack_base_raw=str(physical),magical_attack_base_raw=str(magical))
            doc['ammo']=dict(ammo_kind='2' if sub==2 else '1',units_per_hit='1' if sub==2 else '0')
            name='w'+str(row['vnum']);proofs=[asset(self.project,'Assets/Metin2/AddressableContent/Equipment/'+name+'.asset')]
            doc['appearance']=dict(model_asset='Metin2Equipment/'+name)
        elif category in (2,3):
            slot=22 if kind==34 else 23 if kind in (16,33) else ARMOR_SLOTS.get(sub)
            if not slot:raise MigrationError('undeclared_armor_slot')
            doc['child']['equip_slot']=str(slot)
            if category==3 and kind==2:
                if self.accessory_profile is None:self.accessory_profile=install_accessory_profile(self.model)
                doc['child']['socket_profile_id']=self.accessory_profile
            if kind==2 and sub==0:
                races=((0,4),(5,1),(2,6),(7,3))
                for c in range(4):
                    for sex in range(2):
                        if int(doc['use']['allowed_classes'])>>c&1 and int(doc['use']['allowed_sexes'])>>sex&1:
                            proofs.append(asset(self.project,'Assets/Metin2/AddressableContent/Equipment/a'+str(races[c][sex])+'_'+str(row['value3'])+'.asset'))
                doc['appearance']=dict(model_asset='Metin2Equipment/a{race}_'+str(row['value3']))
        elif category==7:
            broken=row['vnum']==28960
            if not broken and not row['wearflag']&17:raise MigrationError('source_stone_has_no_equipment_target')
            doc['child'].update(stone_kind='2' if broken else '1',allowed_targets='0' if broken else str((1 if row['wearflag']&16 else 0)|(2 if row['wearflag']&1 else 0)),minimum_socket_grade='0' if broken else '1',primary_group='' if broken else 'stone.group_'+str(row['value5']))
        if category in (1,2,3) and kind in (1,2):
            attr='weapon' if kind==1 else sub
            if attr not in self.attribute_profiles:doc.update(attribute_document(self.tables,row))
        self.finish(row,doc,icon,proofs,'equipment_state')
        if category in (1,2,3) and kind in (1,2):
            binding=next((r for r in self.model.tables['item_attribute_binding'] if r['category_id']==doc['base']['category_id'] and r['local_id']==doc['base']['local_id']),None)
            if attr in self.attribute_profiles:
                self.model.replace('item_attribute_binding',{'category_id':doc['base']['category_id'],'local_id':doc['base']['local_id']},[dict(category_id=doc['base']['category_id'],local_id=doc['base']['local_id'],profile_id=self.attribute_profiles[attr])])
            elif binding:self.attribute_profiles[attr]=binding['profile_id']
        return True

    def finish(self,row,doc,icon,models,semantics):
        family=self.families.get(row['vnum'])
        level=0
        if row['type'] in (1,2,10,16,33,34):
            level,error=source_level(row)
            if error:raise MigrationError(error)
            level=level or 0
            doc['refinement']['max_level']=str(level)
            doc['levels']=[dict(level=str(n),physical_attack_gain_raw='0',magical_attack_gain_raw='0',physical_defense_gain_raw='0',magical_defense_gain_raw='0',extra_bonus_profile_id=None,bonuses=[]) for n in range(level+1)]
            if level and not family:
                doc['refinement']['profile_kind']='10';doc['levels']=[doc['levels'][-1]]
        if family:
            doc['refinement']['max_level']=str(len(family)-1);doc['levels']=[]
            baseline=Counter({int(b['bonus_id']):int(b['value_scaled']) for b in doc['bonuses']})
            for n,state in enumerate(family):
                values=effects(state)
                for k in range(2):
                    extra=self.extra.get(state['vnum'],{})
                    if extra.get('bonus_type'+str(k)):
                        b=bonus(extra['bonus_type'+str(k)],extra['bonus_value'+str(k)])
                        if b:values[int(b['bonus_id'])]+=int(b['value_scaled'])
                minimum=max((state['limitvalue'+str(i)] for i in range(2) if state['limittype'+str(i)]==1),default=0)
                doc['levels'].append(dict(level=str(n),physical_attack_gain_raw='0',magical_attack_gain_raw='0',physical_defense_gain_raw='0',magical_defense_gain_raw='0',extra_bonus_profile_id=None,
                    minimum_level=str(minimum),buy_amount=str(state['gold']),sell_amount=str(state['shop_buy_price']),bonuses=[dict(bonus_id=str(k),value_scaled=str(values[k]-baseline[k])) for k in sorted(set(values)|set(baseline)) if values[k]!=baseline[k]]))
        for entries in [doc['bonuses'],doc.get('timed_bonuses',[]),doc.get('toggle_bonuses',[])]+[r.get('bonuses',[]) for r in doc['levels']]:
            for entry in entries:
                low,high=self.bonus_bounds[int(entry['bonus_id'])]
                if not low<=int(entry['value_scaled'])<=high:raise MigrationError('source_bonus_out_of_native_bounds:'+entry['bonus_id'])
        self.model.apply(doc,encode_result=False)
        category=int(doc['base']['category_id']);local=int(doc['base']['local_id'])
        self.model.packet['sequences'][str(category)]=str(local+1)
        code=doc['base']['content_code'];self.texts[code]=translation_for(row,code,self.locales)
        self.assets[code]=dict(icon=icon,equipment=models)
        if row['type'] in (1,2,10,16,33,34):
            for text in self.texts[code]['texts'].values():text['name']=re.sub(r'\s*\+\s*[0-9]+\s*$','',text['name']).strip()
        for state in family or [row]:
            state_level=source_level(state)[0] if family else level
            self.accepted[state['vnum']]=dict(source_id=state['vnum'],item=dict(category_id=category,local_id=local),refinement_level=state_level,
                source_row_hash=value_hash(state),semantics=semantics,source_extra=copy.deepcopy(self.extra.get(state['vnum'])),
                source_level_issues=[name for name in NAME_FIELDS if name_parts(state.get(name))[1] not in (None,state_level)],
                native_stack_policy='unique_instance_per_unit' if category in (1,2,3,4,11,14,15,19) and state['flag']&4 else 'source_stack_policy')
            if state['antiflag']&4096:self.accepted[state['vnum']]['inactive_source_antiflags']=['save_bit_unused_by_source_persistence']

    def convert_row(self,row):
        identity=row['vnum']
        if identity in self.accepted:return
        retired=disabled_source(row)
        if retired:self.retired[identity]=dict(source_id=identity,reason=retired,source_row_hash=value_hash(row));return
        checkpoint={k:list(v) for k,v in self.model.tables.items()};sequences=dict(self.model.packet['sequences']);accessory=self.accessory_profile
        attributes=dict(self.attribute_profiles);texts=set(self.texts);assets=set(self.assets);accepted=set(self.accepted)
        try:
            if convert_private_shop_bundle(self,row):
                self.blocked.pop(identity,None)
                return
            if convert_native_money(self,row):
                self.blocked.pop(identity,None)
                return
            if convert_energy(self,row):
                self.blocked.pop(identity,None)
                return
            if convert_costume_transfer(self,row):
                self.blocked.pop(identity,None)
                return
            if not (self.equipment(row) or self.passive(row) or convert_absorption_sash(self,row) or self.cosmetic(row) or convert_original_food_bundle(self,row) or convert_context_services(self,row) or convert_stateful_utility(self,row) or convert_pet_book(self,row) or convert_dungeon_key(self,row) or convert_utility(self,row) or self.consumable(row) or self.skill_book(row) or self.container(row) or convert_souls(self,row) or convert_tools(self,row) or convert_weapon_effect(self,row) or convert_services(self,row) or convert_behavior(self,row)):raise MigrationError('semantic_converter_required:'+str(row['type'])+':'+str(row['subtype']))
            self.blocked.pop(identity,None)
        except (ValueError,KeyError) as error:
            self.model.tables=checkpoint;self.model.packet['sequences']=sequences;self.accessory_profile=accessory
            self.attribute_profiles=attributes
            for key in set(self.accepted)-accepted:self.accepted.pop(key)
            for code in set(self.texts)-texts:self.texts.pop(code)
            for code in set(self.assets)-assets:self.assets.pop(code)
            self.blocked[identity]=dict(source_id=identity,reasons=[str(error)],source_row_hash=value_hash(row))

    def compile(self,progress=None):
        for row in sorted(self.source.values(),key=lambda r:(1 if r['type']==12 and r['subtype']==1 else 2 if r['type']==12 else 3 if r['type'] in (20,23) else 0,r['vnum'])):
            self.convert_row(row)
            if progress and (len(self.accepted)+len(self.blocked)+len(self.retired))%500==0:progress(len(self.accepted),len(self.blocked),len(self.retired))
        original_pet_skills=bind_original_pet_skills(self)
        costume_transfer_targets=bind_costume_transfer_targets(self)
        while True:
            ready=[]
            for identity,record in self.blocked.items():
                reason=record['reasons'][0]
                if reason.startswith(('unresolved_container_reward:','unresolved_stone_draw_reward:')) and int(reason.split(':')[1]) in self.accepted:ready.append(identity)
            if not ready:break
            for identity in sorted(ready):self.convert_row(self.source[identity])
        fish_conflicts=bind_original_fish_conflicts(self)
        accessory_services=bind_source_accessories(self)
        self.definition_rules()
        birth_attributes=bind_birth_attributes(self)
        absorption_sashes=bind_absorption_sashes(self)
        refinement=self.refinement_services()
        packet=copy.deepcopy(self.original);packet['tables']=self.model.encode();packet['sequences']=self.model.packet['sequences'];packet.pop('hash',None)
        outcomes=[set(self.accepted),set(self.retired),set(self.blocked)]
        if set(self.source)!=set.union(*outcomes) or sum(map(len,outcomes))!=len(self.source):raise MigrationError('source_ledger_not_a_partition')
        return dict(packet=packet,accepted=list(self.accepted.values()),retired=list(self.retired.values()),blocked=list(self.blocked.values()),
            localization=self.texts,assets=self.assets,preserved_hash=self.original['hash'],conversion_kind='full_prototype_catalog',release_ready=False,refinement_services=refinement,accessory_services=accessory_services,
            original_pet_skills=original_pet_skills,birth_attributes=birth_attributes,absorption_sashes=absorption_sashes,fish_conflicts=fish_conflicts,polymorph_forms=self.polymorph_forms,orphan_extra_prototypes=[self.extra[n] for n in sorted(set(self.extra)-set(self.source))],
            costume_transfer_targets=costume_transfer_targets,
            rule_inputs=self.rule_inputs,
            summary=dict(source_rows=len(self.source),converted_source_rows=len(self.accepted),retired_source_rows=len(self.retired),blocked_source_rows=len(self.blocked),
                         new_native_definitions=len(packet['tables']['item_base'])-len(self.original['tables']['item_base']),
                         blocked_reasons=dict(Counter(r['reasons'][0] for r in self.blocked.values()))))

    def definition_rules(self):
        if 'item_definition_rules' not in self.model.tables:raise MigrationError('native_definition_rules_schema_required')
        levels={}
        for ref in self.accepted.values():
            row=self.source[ref['source_id']];key=ref['item'];level=ref['refinement_level'];category=key['category_id']
            clock_kind,duration=1,0
            clocks=[(row['limittype'+str(i)],row['limitvalue'+str(i)]) for i in range(2) if row['limittype'+str(i)] in (7,8,9)]
            if len(clocks)>1:raise MigrationError('conflicting_source_clocks:'+str(row['vnum']))
            if clocks and category not in (4,15) and not (row['type']==3 and row['subtype']==47) and not (row['type']==28 and 10<=row['subtype']<=16):
                t,v=clocks[0];clock_kind={7:2,8:3,9:4}[t];duration=(v if v>0 else 36000 if t==9 else 604800)*1000
            elif row['type']==16 and (category==3 or ref.get('private_shop_role')==2) and not clocks and all(row['limittype'+str(i)]!=10 for i in range(2)) and row['value0']>0:
                clock_kind=2 if row['value2'] else 4;duration=row['value0']*(1000 if row['value2'] else 60000)
            extra=self.extra.get(row['vnum'],{})
            rule=dict(category_id=str(category),local_id=str(key['local_id']),refinement_level=str(level),display_refinement_offset=str(ref.get('source_soul_grade',0)),rarity=str(extra.get('rarity',0)),
                clock_kind=str(clock_kind),duration_ms=str(duration),allow_account_storage=str(int(not row['antiflag']&131072)),allow_acquire=str(int(not row['antiflag']&64)),
                allow_unequip=str(int(not row['flag']&256)),allowed_empires=str(7 & ~(row['antiflag']>>9)),minimum_grant_count=str(max(1,row['value1']) if row['flag']&128 else 1),
                birth_magic_chance_bp=str(min(100,row['magic_pct'])*100),birth_socket_chance_bp=str(min(100,row['socket_pct'])*100),birth_addon=str(int(row['addon_type']!=0)),unique_group='',available='1',attribute_reroll_group=str(source_reroll_group(row['vnum'])),weapon_effect_alternate=str(int(alternate_weapon_effect(row))),birth_open_stone_sockets=str(min(3,row['socket_pct']) if category==1 or category==2 and row['type']==2 and row['subtype']==0 else 0),birth_attribute_profile_id=None)
            levels.setdefault((category,key['local_id']),{})[level]=rule
        replaced={(str(c),str(n)) for c,n in levels}
        rows=[r for r in self.model.tables['item_definition_rules'] if (r['category_id'],r['local_id']) not in replaced]
        base_profiles={(r['category_id'],r['local_id']):r['refinement_profile_id'] for r in self.model.tables['item_base']}
        exact_profiles={r['profile_id'] for r in self.model.tables['refinement_profile'] if r['profile_kind']=='10'}
        for (category,local),rules in levels.items():
            prototype=rules[min(rules)]
            for n in sorted(rules) if base_profiles[str(category),str(local)] in exact_profiles else range(max(rules)+1):
                rule=rules.get(n) or dict(prototype,refinement_level=str(n),available='0')
                rows.append(rule)
        self.model.tables['item_definition_rules']=rows

    def refinement_services(self):
        recipes={r['id']:r for r in self.tables.get('player.refine_proto',[])}
        added=[];blocked=[];identity=int(self.model.allocate('refinement_recipe','recipe_id'))
        source_tuple=lambda r:(str(r['item']['category_id']),str(r['item']['local_id']),str(r['refinement_level']))
        issued={(r['source_category_id'],r['source_local_id'],r['source_level']) for r in self.model.tables['refinement_recipe']}
        for source in sorted(self.accepted):
            row=self.source[source]
            if not row['refined_vnum'] or not row['refine_set'] or row['type'] not in (1,2,10,16,33,34):continue
            ref=self.accepted[source];recipe=recipes.get(row['refine_set']);target=self.accepted.get(row['refined_vnum'])
            if source_tuple(ref) in issued:continue
            try:
                if not recipe:raise MigrationError('missing_source_recipe')
                if not target:raise MigrationError('unconverted_refinement_target:'+str(row['refined_vnum']))
                if source_tuple(ref)==source_tuple(target):raise MigrationError('source_refinement_self_loop')
                inputs=Counter()
                for i in range(5):
                    material,count=recipe['vnum'+str(i)],recipe['count'+str(i)]
                    if not material and not count:continue
                    if material not in self.accepted or count<=0:raise MigrationError('unconverted_refinement_material:'+str(material))
                    inputs[source_tuple(self.accepted[material])]+=count
                s=source_tuple(ref);t=source_tuple(target)
                head=dict(recipe_id=str(identity),code='catalog.refinement_'+str(identity),source_category_id=s[0],source_local_id=s[1],source_level=s[2],
                    target_category_id=t[0],target_local_id=t[1],target_level=t[2],gold_cost=str(recipe['cost']),success_chance_bp=str(recipe['prob']*100),failure_kind='2',failure_level=None)
                self.model.tables['refinement_recipe'].append(head)
                self.model.tables['refinement_ingredient'].extend(dict(recipe_id=str(identity),category_id=k[0],local_id=k[1],refinement_level=k[2],amount=str(v)) for k,v in sorted(inputs.items()))
                added.append(dict(source_id=source,recipe_id=identity,source_recipe_hash=value_hash(recipe)));identity+=1
            except MigrationError as error:blocked.append(dict(source_id=source,reason=str(error)))
        return dict(added=added,blocked=blocked)


def main(argv=None):
    """Reproducible offline conversion. Never accepts player-instance rows."""
    import argparse
    from legacy_item_migration import strict_json,file_hash
    from convert_quest_tokens import read_visuals,read_locales
    from upgrade_exported_catalog import native,upgrade
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','preserved','project','helper','output'):parser.add_argument('--'+name,required=True,type=Path)
    parser.add_argument('--known',action='append',default=[],type=Path,help='Previously issued source-to-native mappings, definitions only')
    parser.add_argument('--groups',type=Path,help='Captured special_item_group.json; never read from a remote server')
    parser.add_argument('--fishing',type=Path,help='Captured fishing.json for explicit fish processing references')
    parser.add_argument('--blends',type=Path,help='Captured blend.json for original energy crystal birth values')
    parser.add_argument('--skills',type=Path,help='Captured secondary skill definitions for new secondary books')
    args=parser.parse_args(argv)
    capture=strict_json(args.source.read_text(encoding='utf-8'));tables=capture['tables']
    allowed={'player.item_proto','player.quest_item_proto','player.item_extra_proto','player.item_attr','player.item_attr_rare','player.item_attr_rare_8','player.refine_proto',
             'player.item_attr.effective','player.item_attr_rare.effective','player.item_attr_rare_8.effective'}
    if set(tables)-allowed:raise MigrationError('source_must_contain_definitions_only')
    preserved=strict_json(args.preserved.read_text(encoding='utf-8'))
    blank=native(args.helper,dict(action='blank'))
    if preserved['columns']!=blank['columns'] or preserved['tables']['catalog_format']!=blank['tables']['catalog_format']:
        old={'protos.'+t:[dict(zip(preserved['columns'][t],r)) for r in rows] for t,rows in preserved['tables'].items()}
        old['protos.catalog_format'][0]['schema_version']=int(old['protos.catalog_format'][0]['schema_version'])
        old['protos.item_local_id_sequence']=[dict(category_id=k,next_local_id=v) for k,v in preserved['sequences'].items()]
        preserved,_=upgrade(old,blank)
    preserved['hash']=native(args.helper,dict(action='validate',tables=preserved['tables']))['hash']
    known=[]
    for path in args.known:
        data=strict_json(path.read_text(encoding='utf-8'));known.extend(data if isinstance(data,list) else data['accepted'])
    project=args.project.resolve(strict=True)
    visuals=read_visuals(project/'Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset');locales,_=read_locales(project)
    groups=strict_json(args.groups.read_text(encoding='utf-8'))['groups'] if args.groups else []
    fishing=strict_json(args.fishing.read_text(encoding='utf-8-sig'))['fish'] if args.fishing else []
    skills=strict_json(args.skills.read_text(encoding='utf-8'))['skills'] if args.skills else []
    result=FullCatalogCompiler(tables,preserved,project,visuals,locales,known,groups,fishing,skills,strict_json(args.blends.read_text(encoding="utf-8"))["blends"] if args.blends else []).compile()
    result['inputs']=dict(source=file_hash(args.source),preserved=file_hash(args.preserved),known=[file_hash(p) for p in args.known],groups=file_hash(args.groups) if args.groups else None,fishing=file_hash(args.fishing) if args.fishing else None,helper=file_hash(args.helper))
    result['inputs']['unity_item_catalog']=file_hash(project/'Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset')
    result['inputs']['secondary_skills']=file_hash(args.skills) if args.skills else None
    result['inputs']['birth_attributes_compiler']=file_hash(Path(__file__).with_name('full_catalog_birth_attributes.py'))
    result['inputs']['absorption_sashes_compiler']=file_hash(Path(__file__).with_name('full_catalog_absorption_sashes.py'))
    result['inputs']['world_utilities_compiler']=file_hash(Path(__file__).with_name('full_catalog_utilities.py'))
    result['inputs']['polymorph_compiler']=file_hash(Path(__file__).with_name('full_catalog_polymorph.py'))
    result['inputs']['stateful_world_utilities_compiler']=file_hash(Path(__file__).with_name('full_catalog_stateful_utilities.py'))
    result['inputs']['original_pet_books_compiler']=file_hash(Path(__file__).with_name('full_catalog_pet_books.py'))
    result['inputs']['dungeon_keys_compiler']=file_hash(Path(__file__).with_name('full_catalog_dungeon_keys.py'))
    result['inputs']['energy_blends']=file_hash(args.blends) if args.blends else None
    result['inputs']['private_shops_compiler']=file_hash(Path(__file__).with_name('full_catalog_private_shops.py'))
    result['inputs']['native_money_compiler']=file_hash(Path(__file__).with_name('full_catalog_money.py'))
    result['inputs']['energy_compiler']=file_hash(Path(__file__).with_name('full_catalog_energy.py'))
    result['inputs']['costume_transfer_compiler']=file_hash(Path(__file__).with_name('full_catalog_costume_transfer.py'))
    result['inputs']['source_retirement_policy']=file_hash(RETIREMENT_POLICY_PATH)
    result['inputs']['final_policies_compiler']=file_hash(Path(__file__).with_name('full_catalog_final_policies.py'))
    result['world_rewards_runtime']=bind_world_reward_availability(result,project,args.groups.with_name('group.json') if args.groups else Path('__missing_world_groups__'))
    result['inputs']['world_rewards_compiler']=file_hash(Path(__file__).with_name('full_catalog_world_rewards.py'))
    try:
        result['packet']['hash']=native(args.helper,dict(action='validate',tables=result['packet']['tables']))['hash']
        result['native_validation']=dict(success=True)
    except MigrationError as error:result['native_validation']=dict(success=False,error=str(error))
    result['release_blockers']=['Runtime admission, generic item lifetime, birth policies and local Unity game flow require verification']
    if result['blocked']:result['release_blockers'].append('Unconverted source rows: '+str(len(result['blocked'])))
    if not result['native_validation']['success']:result['release_blockers'].append('Native catalog validation failed')
    with args.output.open('x',encoding='utf-8') as f:json.dump(result,f,ensure_ascii=True,sort_keys=True);f.write('\n')
    print(json.dumps(dict(result['summary'],native_validation=result['native_validation'],release_ready=result['release_ready']),sort_keys=True))


if __name__=='__main__':main()
