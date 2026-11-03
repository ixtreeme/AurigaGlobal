import json,csv,io,shutil,hashlib,re
from pathlib import Path
r=Path('C:/UnityProjects/AurigaUnity');repo=r/'build/SRC';release=r/'Migration/TeraCompanions'
entries=json.loads((release/'release.json').read_text(encoding='utf-8'))['entries']
probe=json.loads(Path('work/tera-db-probe.json').read_text(encoding='utf-8'))
out=repo/'integration/tera_companions';out.mkdir(parents=True,exist_ok=True)
def tabrow(text):
 lines=text.splitlines();return dict(zip(lines[0].split('\t'),lines[1].split('\t')))
templates={'mount':tabrow(probe['mount_item']),'pet':tabrow(probe['pet_template'])}
mobtemplates={'mount':tabrow(probe['mount_mob']),'pet':tabrow(probe['pet_mob'])}
def literal(value):
 if value=='NULL':return 'NULL'
 return "CONVERT(0x"+str(value).encode('utf-8').hex()+" USING utf8mb4)" if str(value) else "''"
sql=['-- TERA companions: new IDs only; existing records remain unchanged.','SET NAMES utf8mb4;','START TRANSACTION;']
items=[];mobs=[];motions=[]
models={m['key']:m for m in json.loads((r/'Assets/TeraCompanionLab/catalog.json').read_text(encoding='utf-8'))['models']}
for e in entries:
 item=dict(templates[e['kind']]);item.update(vnum=str(e['item']),name='TERA_'+e['key'],locale_name=e['itemNames']['en'],value0='0',value1=str(e['mob']),value2='0',value3='0',value4='0',value5=str(int(e['kind']=='mount' and any(c.lower()=='flyforward' for c in e['clips']))),gold='0',shop_buy_price='0',refined_vnum='0',refine_set='0',vnum_range='0')
 for prefix in ('limittype','limitvalue','applytype','applyvalue'):
  for k in list(item):
   if k.startswith(prefix):item[k]='0'
 if e['kind']=='mount':item.update(limittype0='10',limitvalue0='1')
 mob=dict(mobtemplates[e['kind']]);mob.update(vnum=str(e['mob']),name='TERA_'+e['key'],locale_name=e['names']['en'],folder='tera_'+e['key'],gold_min='0',gold_max='0',exp='0',drop_item='0',resurrection_vnum='0',damage_min='0',damage_max='0',summon='0',polymorph_item='0')
 for lang in e['names']:
  item['locale_name_'+lang]=e['itemNames'][lang];mob['locale_name_'+lang]=e['names'][lang]
 for table,row in (('item_proto',item),('mob_proto',mob)):
  sql.append('INSERT INTO `'+table+'` ('+','.join('`'+k+'`' for k in row)+') VALUES ('+','.join(literal(v) for v in row.values())+');')
 items.append(item);mobs.append(mob)
 attack=next((a for a in models[e['key']]['animations'] if re.search(r'attack|atk',a['name'],re.I)),None)
 motion=dict(folder='tera_'+e['key'],walk_speed='150',run_speed='300',attack_duration=str(round(attack['seconds']*1000)) if attack else '0',**{'skill'+str(i)+'_duration':'0' for i in range(1,6)})
 motions.append(motion)
 sql.append('INSERT INTO `mob_motion` ('+','.join('`'+k+'`' for k in motion)+') VALUES ('+','.join(literal(v) for v in motion.values())+');')
sql+=['COMMIT;']
(out/'prototypes.sql').write_text('\n'.join(sql)+'\n',encoding='utf-8')
for table,rows in (('item_proto',items),('mob_proto',mobs),('mob_motion',motions)):
 with (release/(table+'.csv')).open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 shutil.copy2(release/(table+'.csv'),out/(table+'.csv'))
shutil.copy2(release/'release.json',out/'release.json')
shutil.copy2(release/'approved-settings.json',out/'approved-settings.json')
pet=[e for e in entries if e['kind']=='pet']
quest='quest tera_companions begin\n    state start begin\n        when '+ ' or '.join(str(e['item'])+'.use' for e in pet)+' begin\n            local races = {\n'+''.join('                ['+str(e['item'])+'] = '+str(e['mob'])+',\n' for e in pet)+'''            }
            local race = races[item.get_vnum()]
            if race == nil then return end
            if pet.is_summon(race) then
                pet.unsummon(race)
            elseif pet.count_summoned() < 1 then
                pet.summon(race, false)
            else
                syschat(gameforge[pc.get_language()].pet_system._1)
            end
        end
    end
end
'''
(out/'tera_companions.lua').write_text(quest,encoding='utf-8')
header=repo/'SRC/Server/GameServer/pet/mount_inventory_helper.h'
text=header.read_text(encoding='utf-8')
start='\t\t\t// TERA companion release: permanent summon seals.\n';end='\t\t\t// End TERA companion release.\n'
if start in text:text=text[:text.index(start)]+text[text.index(end)+len(end):]
ids=[e['item'] for e in entries if e['kind']=='mount']
block=start+''.join('\t\t\t'+', '.join(map(str,ids[i:i+12]))+',\n' for i in range(0,len(ids),12))+end
text=text.replace('\t\t\t// Mountok\n',block+'\n\t\t\t// Mountok\n');header.write_text(text,encoding='utf-8',newline='\n')
# Update only the new IDs in all client language tables, preserving all existing entries.
locale=r/'Assets/Metin2/Resources/Metin2/Locales'
for lang in entries[0]['names']:
 for suffix,isitem in (('items',True),('mobs',False)):
  p=locale/(lang+'_'+suffix+'.json');data=json.loads(p.read_text(encoding='utf-8-sig'))
  ids={e['item' if isitem else 'mob'] for e in entries};data['entries']=[x for x in data['entries'] if x['vnum'] not in ids]
  for e in entries:
   data['entries'].append(dict(vnum=e['item'],range=0,name=e['itemNames'][lang],description=e['descriptions'][lang],summary='') if isitem else dict(vnum=e['mob'],level=1,rank=5,name=e['names'][lang]))
  data['entries'].sort(key=lambda x:x['vnum']);p.write_text(json.dumps(data,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
# Numeric client proto exports for every rebuild path.
for filename in ('item_proto.tsv','item_proto_client.tsv'):
 p=r/'Migration/Converted'/filename;lines=p.read_text(encoding='utf-8-sig').splitlines();head=lines[0];columns=head.lstrip('# ').split('\t');ids={e['item'] for e in entries}
 lines=[head]+[l for l in lines[1:] if l and int(l.split('\t')[0]) not in ids]
 alias={'flags':'flag','buy_item_price':'gold','sell_item_price':'shop_buy_price'}
 lines+=['\t'.join(x.get(alias.get(k,k),'0') for k in columns) for x in items]
 p.write_text('\n'.join(lines)+'\n',encoding='utf-8')
for filename in ('item_proto_names.tsv','item_proto_names_client.tsv'):
 p=r/'Migration/Converted'/filename;lines=p.read_text(encoding='utf-8-sig').splitlines();ids={e['item'] for e in entries}
 lines=[l for l in lines if not re.match(r'^\d+\t',l) or int(l.split('\t')[0]) not in ids]
 if filename=='item_proto_names.tsv':lines+=['\t'.join([str(e['item'])]+[e['itemNames'][l] for l in ('cz','de','en','es','hu','it','pl','pt','ro','tr')]+['TERA_'+e['key']]) for e in entries]
 else:lines+=['\t'.join([str(e['item']),e['itemNames']['hu'],'TERA_'+e['key']]) for e in entries]
 p.write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('Prepared SQL/quest/source/client locales:',len(items),'items,',len(mobs),'mobs')
