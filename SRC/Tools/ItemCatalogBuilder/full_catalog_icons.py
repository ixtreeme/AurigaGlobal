"""Explicit refinement-chain icon inheritance for a missing higher-state icon.

No numeric-neighbour fallback: a unique source edge, consecutive refinement,
same source family name and unchanged item restrictions are required.
"""
import copy,re
from legacy_item_migration import value_hash

def refinement_icon_aliases(source, visuals, level):
    parents={}
    for row in source.values():
        if row['refined_vnum']:parents.setdefault(row['refined_vnum'],[]).append(row)
    aliases={}
    def signature(row):
        return tuple(row[k] for k in ('type','subtype','size','flag','antiflag','wearflag','immuneflag'))+(re.sub(r'\+\s*[0-9]+\s*$','',row['name']).strip(),)
    for row in source.values():
        if row['type'] not in (1,2,16,33,34) or visuals.get(row['vnum'],{}).get('iconPath'):continue
        current=row;chain=[row];seen={row['vnum']}
        while len(parents.get(current['vnum'],[]))==1:
            parent=parents[current['vnum']][0]
            child_level,error=level(current);parent_level,parent_error=level(parent)
            if parent['vnum'] in seen or error or parent_error or child_level is None or parent_level is None or child_level!=parent_level+1 or signature(parent)!=signature(row):break
            chain.append(parent);seen.add(parent['vnum']);visual=visuals.get(parent['vnum'],{})
            if visual.get('iconPath') and visual.get('icon',{}).get('guid'):
                aliases[row['vnum']]=dict(visual=copy.deepcopy(visual),source_id=parent['vnum'],chain=[dict(source_id=r['vnum'],source_row_hash=value_hash(r)) for r in chain]);break
            current=parent
    return aliases
