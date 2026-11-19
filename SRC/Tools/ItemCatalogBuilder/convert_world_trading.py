"""Offline conversion of original local NPC stock and cube definitions."""
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re


def quest_shop_routes(root):
    result=defaultdict(set)
    for path in sorted(Path(root).rglob('*.lua')):
        # Only ASCII Lua syntax is inspected here. Preserve every byte of the
        # mixed original UTF-8/Windows-1250 files; never rewrite their text.
        text=re.sub(r'--\[\[.*?\]\]|--[^\n]*','',path.read_bytes().decode('latin1'),flags=re.S)
        events=list(re.finditer(r'\bwhen\s+(\d+)\.(?:chat|click)',text))
        for call in re.finditer(r'\bnpc\.open_shop\s*\(\s*(\d+)\s*\)',text):
            previous=[event for event in events if event.start()<call.start()]
            if previous:result[int(call[1])].add(int(previous[-1][1]))
    return result


def convert_shops(catalog, original, quest_root):
    accepted={r['source_id']:r for r in catalog['accepted']}
    deferred={r['source_id'] for r in catalog['blocked']}
    retired={r['source_id'] for r in catalog['retired']}
    packet=catalog['packet'];bases={(int(r[0]),int(r[1])):dict(zip(packet['columns']['item_base'],r)) for r in packet['tables']['item_base']}
    headers={int(r['vnum']):int(r['npc_vnum']) for r in original['shop']}
    routes=quest_shop_routes(quest_root)
    stock=defaultdict(list);audit=dict(excluded=[],unbound_shops=[],orphan_rows=[],missing_quest_shops=sorted(set(routes)-set(headers)))
    names={int(r['vnum']):r['locale_name'] for r in original.get('mob_names',[]) if r.get('locale_name')}

    def resolve(source):
        if source not in accepted:
            raise ValueError(('merge_deferred:' if source in deferred else 'retired:' if source in retired else 'missing_original_prototype:')+str(source))
        ref=accepted[source];return dict(category=ref['item']['category_id'],local=ref['item']['local_id'],level=ref['refinement_level'])

    ordered=sorted(original['shop_item'],key=lambda r:(int(r['shop_vnum']),int(r['item_vnum']),json.dumps(r,sort_keys=True)))
    for ordinal,row in enumerate(ordered,1):
        shop=int(row['shop_vnum'])
        if shop not in headers:
            audit['orphan_rows'].append(dict(shop=shop,source_id=int(row['item_vnum'])));continue
        if not headers[shop] and not routes.get(shop):
            continue
        try:
            reward=resolve(int(row['item_vnum']));count=int(row['count']);gold=int(row['gold'])
            base=bases[reward['category'],reward['local']]
            if not 0<count<=int(base['max_stack']) or not 0<=gold<=2**63-1:raise ValueError('invalid_original_bundle_count_or_price')
            costs=Counter()
            for i in range(1,6):
                source=int(row['itemvnum'+str(i)]);amount=int(row['itemcount'+str(i)])
                if not source:continue
                if amount<=0:raise ValueError('invalid_original_currency_count')
                key=resolve(source);costs[key['category'],key['local'],key['level']]+=amount
            if any(amount>2**32-1 for amount in costs.values()):raise ValueError('original_currency_overflow')
            stock[shop].append(dict(id=ordinal,**reward,count=count,gold=gold,try_magic=True,
                costs=[dict(category=key[0],local=key[1],level=key[2],count=amount) for key,amount in sorted(costs.items())]))
        except ValueError as error:
            audit['excluded'].append(dict(shop=shop,source_id=int(row['item_vnum']),reason=str(error)))
    defaults=defaultdict(list);routed=[]
    for shop,npc in sorted(headers.items()):
        if not npc and not routes.get(shop):audit['unbound_shops'].append(shop)
        offers=stock[shop]
        if not offers:continue
        if npc:defaults[npc].extend(offers)
        for race in sorted(({npc} if npc else set())|routes.get(shop,set())):
            if not shop:continue  # Route zero is the normal NPC stock lookup.
            routed.append(dict(source_id=shop,npc_race=race,name=names.get(race,'Kereskedés'),offers=offers))
    shops=[dict(npc_race=npc,name=names.get(npc,'Kereskedés'),offers=offers) for npc,offers in sorted(defaults.items())]
    if any(len(shop['offers'])>128 for shop in shops+routed):raise ValueError('original stock exceeds native offer limit')
    audit.update(npc_shops=len(shops),authored_routes=len(routed),active_source_offers=sum(len(v) for v in stock.values()),
        barter_offers=sum(bool(row['costs']) for values in stock.values() for row in values),free_offers=sum(row['gold']==0 and not row['costs'] for values in stock.values() for row in values))
    return dict(version=2,shops=shops,routes=routed),audit


def convert_cubes(catalog, original, prototypes):
    accepted={r['source_id']:r for r in catalog['accepted']}
    deferred={r['source_id'] for r in catalog['blocked']};retired={r['source_id'] for r in catalog['retired']}
    packet=catalog['packet'];bases={(int(r[0]),int(r[1])):dict(zip(packet['columns']['item_base'],r)) for r in packet['tables']['item_base']}
    prototypes={int(r['vnum']):r for r in prototypes}
    workshops=defaultdict(list);audit=dict(excluded=[],ignored_original_copy=[],converted_source_recipes=0,gaya_recipes=0,copy_recipes=0)
    def resolve(source):
        if source not in accepted:raise ValueError(('merge_deferred:' if source in deferred else 'retired:' if source in retired else 'missing_original_prototype:')+str(source))
        row=accepted[source];return dict(category=row['item']['category_id'],local=row['item']['local_id'],level=row['refinement_level'])
    for ordinal,row in enumerate(original['cubes'],1):
        try:
            if len(row['rewards'])!=1:raise ValueError('unsupported_original_multiple_rewards')
            reward=row['rewards'][0];result=resolve(int(reward['vnum']));result['count']=int(reward['count'])
            base=bases[result['category'],result['local']]
            if not 0<result['count']<=int(base['max_stack']):raise ValueError('invalid_original_reward_count')
            costs=Counter()
            for i in row['items']:
                key=resolve(int(i['vnum']));count=int(i['count'])
                if not 0<count<=(2**32-1)//100:raise ValueError('invalid_original_ingredient_count')
                costs[key['category'],key['local'],key['level']]+=count
            if not 0<len(costs)<=16 or any(v>(2**32-1)//100 for v in costs.values()):raise ValueError('invalid_original_ingredients')
            gold=int(row['gold']);gaya=int(row.get('gaya',0));chance=int(row['percent'])*100
            if not 0<=gold<=(2**63-1)//100 or not 0<=gaya<=999998 or not 0<=chance<=10000:raise ValueError('invalid_original_cost_or_chance')
            copy=None;copy_id=int(row.get('allow_copy',0))
            if copy_id:
                source=prototypes.get(copy_id);target=prototypes.get(int(reward['vnum']))
                # Match the actual original copy branch. Pet/costume/material
                # and belt flags never copied state in that implementation.
                if source and target and int(source['type']) in (1,2) and int(source['subtype'])==int(target['subtype']):
                    copy=resolve(copy_id);copy['count']=1
                    if costs.get((copy['category'],copy['local'],copy['level']))!=1:raise ValueError('original_copy_not_one_ingredient')
                else:audit['ignored_original_copy'].append(dict(recipe=ordinal,source_id=copy_id))
            prototype=prototypes.get(int(reward['vnum']),{})
            title=prototype.get('locale_name_hu') or prototype.get('locale_name') or base['content_code']
            title=' '.join(title.split())
            if result['level'] and not re.search(r'\+\d+\s*$',title):title+=' +'+str(result['level'])
            recipe=dict(id=ordinal,name=title,result=result,gold=gold,gaya=gaya,chance_bp=chance,copy_source=copy,
                ingredients=[dict(category=k[0],local=k[1],level=k[2],count=n) for k,n in sorted(costs.items())])
            for npc in sorted(set(map(int,row['npcs']))):
                if npc<=0:raise ValueError('invalid_original_npc')
                workshops[npc].append(recipe)
            audit['converted_source_recipes']+=1;audit['gaya_recipes']+=bool(gaya);audit['copy_recipes']+=copy is not None
        except (ValueError,KeyError) as error:audit['excluded'].append(dict(recipe=ordinal,reason=str(error)))
    if any(len(rows)>128 for rows in workshops.values()):raise ValueError('original workshop exceeds native recipe limit')
    document=dict(version=2,workshops=[dict(npc_race=npc,name='Tárgykészítés',recipes=rows) for npc,rows in sorted(workshops.items())])
    audit['workshops']=len(workshops);audit['routed_recipes']=sum(map(len,workshops.values()))
    return document,audit


def main():
    import argparse
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('catalog','shops','quests','output'):p.add_argument('--'+name,required=True,type=Path)
    p.add_argument('--cubes',type=Path);p.add_argument('--prototypes',type=Path)
    args=p.parse_args()
    if args.output.exists():raise ValueError('output must be a new directory')
    catalog=json.loads(args.catalog.read_text(encoding='utf-8'))
    shops=json.loads(args.shops.read_text(encoding='utf-8'))
    document,audit=convert_shops(catalog,shops,args.quests)
    args.output.mkdir(parents=True)
    if args.cubes:
        if not args.prototypes:raise ValueError('--prototypes is required for original copy semantics')
        cubes=json.loads(args.cubes.read_text(encoding='utf-8'));prototypes=json.loads(args.prototypes.read_text(encoding='utf-8'))['tables']['player.item_proto']
        crafting,cube_audit=convert_cubes(catalog,cubes,prototypes)
        (args.output/'typed_crafting.json').write_text(json.dumps(crafting,ensure_ascii=False,sort_keys=True)+'\n',encoding='utf-8')
        audit['crafting']=cube_audit
        services=dict(Workshops=sorted(w['npc_race'] for w in crafting['workshops']),Shops=sorted(w['npc_race'] for w in document['shops']))
        (args.output/'typed_world_services.json').write_text(json.dumps(services,sort_keys=True)+'\n',encoding='utf-8')
    (args.output/'typed_shops.json').write_text(json.dumps(document,ensure_ascii=False,sort_keys=True)+'\n',encoding='utf-8')
    audit['inputs']={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in (args.catalog,args.shops,*((args.cubes,args.prototypes) if args.cubes else ())) }
    (args.output/'audit.json').write_text(json.dumps(audit,ensure_ascii=False,sort_keys=True)+'\n',encoding='utf-8')
    print(json.dumps({key:value for key,value in audit.items() if isinstance(value,int)}))


if __name__=='__main__':main()
