"""Original optional magic issuance, independent SHA/tier oracle and real SQL worker."""
import argparse, hashlib, json, math, os, re, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'SRC/Tools/ItemCatalogBuilder'),str(ROOT/'db/tools')]
from stage_exported_catalog import stage
from mariadb_client import statements
from legacy_item_migration import file_hash

class Reference:
    def __init__(self,candidate):
        p=candidate['packet']
        self.rows={t:[dict(zip(p['columns'][t],r)) for r in data] for t,data in p['tables'].items()}
        self.base={(int(r['category_id']),int(r['local_id'])):r for r in self.rows['item_base']}
        self.rules={(int(r['category_id']),int(r['local_id']),int(r['refinement_level'])):r for r in self.rows['item_definition_rules']}
        self.bindings={(int(r['category_id']),int(r['local_id'])):r['profile_id'] for r in self.rows['item_attribute_binding']}
        self.groups={r['profile_id']:r for r in self.rows['attribute_group'] if r['pool_kind']=='1'}
        self.profiles={r['profile_id']:r for r in self.rows['attribute_birth_profile']}
        self.static={}
        for r in self.rows['bonus_profile_entry']:self.static.setdefault(r['profile_id'],set()).add(int(r['bonus_id']))
        self.candidates={}
        for r in self.rows['attribute_candidate']:
            if r['pool_kind']=='1':self.candidates.setdefault(r['profile_id'],[]).append((int(r['bonus_id']),int(r['weight'])))
        for v in self.candidates.values():v.sort()
        self.values={}
        for r in self.rows['attribute_value']:
            if r['pool_kind']=='1':self.values.setdefault((r['profile_id'],int(r['bonus_id'])),[]).append((int(r['tier']),int(r['value_scaled'])))
        for k,v in self.values.items():self.values[k]=[value for tier,value in sorted(v)]
        self.refinements={(r['profile_id'],int(r['level'])):r for r in self.rows['refinement_level']}

    @staticmethod
    def random(domain,operation,instance):
        data=bytearray(64);data[:len(domain)]=domain;data[24:40]=operation;data[40:48]=instance.to_bytes(8,'little');counter=0
        def draw(maximum):
            nonlocal counter
            limit=(1<<32)//maximum*maximum
            while True:
                data[48:56]=counter.to_bytes(8,'little');counter+=1
                raw=int.from_bytes(hashlib.sha256(data).digest()[:4],'little')
                if raw<limit:return raw%maximum+1
        return draw

    def calculate(self,ref,operation,instance,policy,chance_override=None):
        key=ref['item']['category_id'],ref['item']['local_id'];level=ref['refinement_level'];rule=self.rules[*key,level]
        if key not in self.bindings:
            assert not rule['birth_attribute_profile_id'] and rule['birth_addon']=='0'
            return [] # Original non-equipment rewards ignore the rare override.
        binding=self.bindings[key];group=self.groups[binding];excluded=set()
        if group['exclude_static']=='1':
            base=self.base[key];excluded.update(self.static.get(base['base_bonus_profile_id'],set()))
            if base['refinement_profile_id']:excluded.update(self.static.get(self.refinements[base['refinement_profile_id'],level]['extra_bonus_profile_id'],set()))
        result=[]
        if rule['birth_addon']=='1':
            draw=self.random(b'auriga.item.addon.v1',operation,instance);a,b=draw(4294967295),draw(4294967295);x,y,z=draw(17)-1,draw(17)-1,draw(20)-1
            skill=max(-30,min(30,int(math.sqrt(-2*math.log((a-.5)/4294967295))*math.cos(2*math.pi*(b-.5)/4294967295)*5+.5)))
            hit=-2*skill+(abs(x+y-16)+z%4+1 if abs(skill)<=20 else z%5+1)
            result=[dict(bonus=bonus,value=value*100) for bonus,value in ((100,hit),(99,skill)) if value and bonus not in excluded]
        chance=chance_override if chance_override is not None else 10000 if policy==2 else int(rule['birth_magic_chance_bp']) if policy else 0
        if not chance:return result
        draw=self.random(b'auriga.item.magic.v1',operation,instance)
        if draw(10000)>chance:return result
        profile=self.profiles[rule['birth_attribute_profile_id']]
        for ordinal in range(3):
            if ordinal and draw(10000)>int(profile['second_chance_bp' if ordinal==1 else 'third_chance_bp']):continue
            if len(result)>=int(group['capacity']):continue
            tier_draw=draw(10000);tier=None
            # Source arrays, deliberately independent of the compiled birth tables.
            for i,weight in enumerate((3000,4000,2000,800,200) if ordinal==0 else (5000,4000,1000,0,0),1):
                if tier_draw<=weight:tier=i;break
                tier_draw-=weight
            choices=[(bonus,weight) for bonus,weight in self.candidates[binding] if bonus not in excluded and bonus not in {r['bonus'] for r in result}]
            if not sum(w for b,w in choices):continue
            type_draw=draw(sum(w for b,w in choices))
            for bonus,weight in choices:
                if type_draw<=weight:
                    values=self.values[binding,bonus];result.append(dict(bonus=bonus,value=values[min(tier,len(values))-1]));break
                type_draw-=weight
        return result

def main():
    parser=argparse.ArgumentParser()
    for name in ('candidate','baseline','helper','test-executable','work','mariadb-bin'):parser.add_argument('--'+name,required=True,type=Path)
    args=parser.parse_args();candidate=json.loads(args.candidate.read_text(encoding='utf8'));old=json.loads(args.baseline.read_text(encoding='utf8'));assert candidate['accepted']==old['accepted']
    reference=Reference(candidate);source={r['vnum']:r for r in json.loads((ROOT/'build/full-proto-inputs.json').read_text(encoding='utf8'))['tables']['player.item_proto']}
    cases=sorted((r for r in candidate['accepted'] if reference.rules.get((r['item']['category_id'],r['item']['local_id'],r['refinement_level']),{}).get('birth_attribute_profile_id')),key=lambda r:r['source_id']);assert len(cases)==6612
    expected=[];samples={};weapon={(int(r['category_id']),int(r['local_id'])):r for r in reference.rows['item_weapon']}
    for n,ref in enumerate(cases):
        key=ref['item']['category_id'],ref['item']['local_id'];rule=reference.rules[*key,ref['refinement_level']];original=source[ref['source_id']]
        family=1 if original['type']==1 else 2 if original['type']==2 and original['subtype']==0 else 3 if original['type']==2 else 4
        profile=reference.profiles[rule['birth_attribute_profile_id']];assert (int(profile['second_chance_bp']),int(profile['third_chance_bp']))=={1:(2000,500),2:(1000,200),3:(1000,100),4:(10000,10000)}[family]
        assert int(rule['birth_magic_chance_bp'])==original['magic_pct']*100
        operation=bytes([0xA7,n&255,n>>8])+bytes(13);expected.append([reference.calculate(ref,operation,14000000+n,policy) for policy in range(3)])
        if rule['allow_unequip']=='1':samples.setdefault((family,weapon.get(key,{}).get('weapon_subtype'),original['addon_type'],original['magic_pct']),n)
    indices=sorted(set(samples.values()));assert len(indices)>=10
    packets=[];box_packets=[]
    def verify(sandbox,catalog):
        db=sandbox.client;db.query('CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin');db.query('CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY,exp INT UNSIGNED NOT NULL DEFAULT 0,skill_point SMALLINT NOT NULL DEFAULT 0,sub_skill_point SMALLINT NOT NULL DEFAULT 0,horse_skill_point SMALLINT NOT NULL DEFAULT 0,skill_level BLOB NULL) ENGINE=InnoDB');db.query('CREATE TABLE player.player_deleted LIKE player.player')
        paths=sorted((ROOT/'db/migrations/item_instance').glob('[0-9][0-9][0-9]_*.sql'));assert [int(p.name[:3]) for p in paths]==list(range(1,45))
        for path in paths:
            for statement in statements(path.read_text(encoding='utf-8-sig')):db.query(statement)
        db.query("INSERT INTO player.item_persistence_format(singleton,schema_version,catalog_revision,catalog_sha256) VALUES(1,1,1,UNHEX('"+catalog['hash']+"'))");db.query("CREATE USER 'game_reader'@'localhost' IDENTIFIED BY '"+sandbox.password+"'")
        for name in ('protos','player'):db.query('GRANT SELECT ON '+name+".* TO 'game_reader'@'localhost'")
        for table in ('item_owner_fence','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_ground_core','item_ground_entry','item_book_learning','item_recovery_schedule','item_soul_strike_event'):db.query('GRANT INSERT,UPDATE,DELETE ON player.'+table+" TO 'game_reader'@'localhost'")
        db.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'game_reader'@'localhost'");db.query("GRANT UPDATE ON player.player TO 'game_reader'@'localhost'");db.query("GRANT INSERT ON player.item_mission_event TO 'game_reader'@'localhost'")
        output=(args.work/'source-magic-birth-runtime').resolve();stdin=[str(sandbox.port),sandbox.password,str(len(catalog['tables']['item_base'])),catalog['hash'],'0',str(output),str(len(cases))]
        for ref in cases:stdin.extend(map(str,(ref['item']['category_id'],ref['item']['local_id'],ref['refinement_level'],0)))
        for policies in expected:
            for bonuses in policies:
                stdin.append(str(len(bonuses)))
                for bonus in bonuses:stdin.extend(map(str,(bonus['bonus'],bonus['value'])))
        stdin.extend(map(str,(len(indices),*indices)))
        result=subprocess.run([str(args.test_executable.resolve()),'--magic-birth-catalog'],input='\n'.join(stdin)+'\n',text=True,capture_output=True,timeout=900,env=dict(os.environ,MARIADB_TLS_DISABLE_PEER_VERIFICATION='1'))
        log=(result.stdout+result.stderr).replace(sandbox.password,'[redacted]');(args.work/'source-magic-birth-worker.log').write_text(log,encoding='utf8')
        if result.returncode or 'SOURCE_MAGIC_BIRTH_WORKER=SUCCESS' not in log:raise RuntimeError('Magic birth worker failed; see owned work log')
        actual={}
        for n,policy,identity,bonuses in re.findall(r'MAGIC_SQL_BIRTH=(\d+) (\d+) (\d+)((?: -?\d+:-?\d+)*)',log):
            n,policy,identity=int(n),int(policy),int(identity);operation=bytes([0xA8,n&255,n>>8,policy,1])+bytes(11);values=reference.calculate(cases[n],operation,identity,policy);assert values==[dict(bonus=int(b),value=int(v)) for b,v in re.findall(r'(-?\d+):(-?\d+)',bonuses)];actual[n,policy]=values
        assert len(actual)==len(indices)*3
        for n in indices:
            ref=cases[n];key=ref['item']['category_id'],ref['item']['local_id']
            for policy in range(3):
                for phase in ('born','relogged','equipped','moved'):
                    files={kind:Path(str(output)+f'.{n}.{policy}.{phase}.{kind}.hex') for kind in ('inventory','details')}
                    for kind,path in files.items():
                        wire=bytes.fromhex(path.read_text());assert wire[0]=={'inventory':225,'details':227}[kind] and int.from_bytes(wire[1:5],'little')==len(wire)
                    packets.append(dict(source=ref['source_id'],category=key[0],local=key[1],level=ref['refinement_level'],policy=policy,expected=actual[n,policy],phase=phase,icon=reference.base[key]['icon_asset'],**{k:str(v) for k,v in files.items()}))
        all_refs={(r['item']['category_id'],r['item']['local_id'],r['refinement_level']):r for r in candidate['accepted']}
        for n,category,local,level,chance,identity,bonuses in re.findall(r'MAGIC_BOX_BIRTH=(\d+) (\d+) (\d+) (\d+) (\d+) (\d+)((?: -?\d+:-?\d+)*)',log):
            n,category,local,level,chance,identity=map(int,(n,category,local,level,chance,identity));ref=all_refs[category,local,level];values=reference.calculate(ref,bytes([0xA9,n,2])+bytes(13),identity,1,chance);assert values==[dict(bonus=int(b),value=int(v)) for b,v in re.findall(r'(-?\d+):(-?\d+)',bonuses)]
            for phase in ('ready','opened','relogged'):
                files={kind:Path(str(output)+f'.box.{n}.{phase}.{kind}.hex') for kind in ('inventory','details','container')};assert all(path.exists() for path in files.values())
                box_packets.append(dict(source=ref['source_id'],category=category,local=local,level=level,chance=chance,expected=values,phase=phase,**{k:str(v) for k,v in files.items()}))
        assert len(box_packets)==6
        sandbox.verify()
    result=stage(args.candidate,args.helper,args.work,args.mariadb_bin,verify);evidence=dict(success=True,candidate_sha256=file_hash(args.candidate),catalog_hash=result['catalog_hash'],source_states=len(cases),source_policies=len(cases)*3,sql_samples=len(indices),packets=packets,box_packets=box_packets,actual_game_item_worker=True,full_actor_world_test=False,installed_to_game=False)
    (args.work/'source-magic-birth-worker-result.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf8');print(json.dumps(dict(success=True,sources=len(cases),sql_samples=len(indices),packets=len(packets),installed_to_game=False)))
if __name__=='__main__':main()
