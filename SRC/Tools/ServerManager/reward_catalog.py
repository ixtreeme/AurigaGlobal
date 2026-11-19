"""Per-chest live contents and atomic multi-destination reward edits."""
import copy
from collections import Counter
from decimal import Decimal

import backend as api
import item_catalog as catalog
from content_names import item_name, item_sort_key, mob_name


def target_text(group):
    if str(group['target_kind']) == '2':
        return 'Közös drop · ' + catalog.MOB_RANKS.get(int(group['target_value']), 'Ismeretlen rang').split(' (')[0]
    return mob_name(group['target_value'])


def roll_text(group):
    roll = str(group['roll_kind'])
    if roll == '2': return '1 tárgy / %s ölés' % group['kills_per_drop']
    if roll == '3': return 'szinthez kötött, %s. szinttől' % group['minimum_killer_level']
    if roll == '4': return 'dupla drop'
    return 'minden bejegyzés'

GROUP_FIELDS = ('category_id', 'local_id', 'pick_mode')
ENTRY_FIELDS = ('category_id', 'local_id', *catalog.LOOT_FIELDS)


def key_of(row):
    return int(row['category_id']), int(row['local_id'])


def reward_item_key(row, kind):
    if kind == 'box':
        if row['reward_kind'] != '1':
            return None
        category, local = row['item_category_id'], row['item_local_id']
    else:
        category, local = row['category_id'], row['local_id']
    return category, local, int(row.get('refinement_level') or 0)


def normalize_loot(document):
    mode = str(document['pick_mode'])
    if mode not in ('1', '2'): raise api.EditorError('Válassz sorsolási módot.')
    entries = []
    for number, row in enumerate(document['entries'], 1):
        entry = {field: catalog.text(row.get(field, '0' if field == 'refinement_level' else None)) for field in catalog.LOOT_FIELDS}
        entry['entry_no'] = str(number)
        entries.append(entry)
    if not 1 <= len(entries) <= 256: raise api.EditorError('A ládának 1–256 jutalombejegyzése lehet.')
    return dict(pick_mode=mode, entries=entries)


class Rewards:
    def __init__(self, packet, model=None):
        self.packet = packet
        self.model = model if model is not None else catalog.Catalog(packet, live_drops=True)
        self.bases = {key_of(row):row for row in self.model.items()}
        self.boxes = {key_of(row):row for row in self.model.tables['item_container']}
        self.profiles = {row['profile_id']:row for row in self.model.tables['loot_profile']}
        self.profile_entries = {}
        for row in self.model.tables['loot_entry']:
            self.profile_entries.setdefault(row['profile_id'],[]).append({field:row[field] for field in catalog.LOOT_FIELDS})
        self._targets = None
        live = packet.get('live_drops', {})
        self.overrides = {}
        for values in live.get('container_groups', []):
            row = dict(zip(GROUP_FIELDS, values))
            self.overrides[key_of(row)] = dict(pick_mode=row['pick_mode'], entries=[])
        for values in live.get('container_entries', []):
            row = dict(zip(ENTRY_FIELDS, values))
            self.overrides[key_of(row)]['entries'].append({field:row[field] for field in catalog.LOOT_FIELDS})

    def containers(self):
        return sorted((self.bases[key] for key,row in self.boxes.items()
                       if row['container_kind'] in ('1','2') and self.bases[key]['retired'] != '1'), key=item_sort_key)

    def loot(self, key):
        key = tuple(key)
        if key in self.overrides: return copy.deepcopy(self.overrides[key])
        return self.base_loot(key)

    def base_loot(self,key):
        profile=self.boxes.get(tuple(key),{}).get('loot_profile_id')
        if profile not in self.profiles:raise api.EditorError('Ennek a tárgynak nincs szerkeszthető ládatartalma.')
        return normalize_loot(dict(pick_mode=self.profiles[profile]['pick_mode'],entries=self.profile_entries.get(profile,[])))

    def set_loot(self, key, document):
        key = tuple(key)
        if key not in self.boxes or self.boxes[key]['container_kind'] not in ('1','2') or self.bases[key]['retired'] != '0':
            raise api.EditorError('Csak használható láda vagy kulccsal nyitható láda tartalma szerkeszthető.')
        wanted = normalize_loot(document)
        original = self.base_loot(key)
        if wanted == original: self.overrides.pop(key, None)
        else: self.overrides[key] = wanted
        self._targets=None

    def container_rows(self):
        groups, entries = [], []
        for key, loot in sorted(self.overrides.items()):
            groups.append([str(key[0]),str(key[1]),loot['pick_mode']])
            for row in loot['entries']:
                entries.append([str(key[0]),str(key[1]),*[row[field] for field in catalog.LOOT_FIELDS]])
        return dict(container_groups=groups, container_entries=entries)

    def targets(self):
        if self._targets is not None:return self._targets
        result = []
        for base in self.containers():
            key = key_of(base); loot = self.loot(key)
            result.append(dict(key=('box', key), name=item_name(base), kind='Láda', weighted=loot['pick_mode']=='1',
                               mode='Egy jutalom, súly szerint' if loot['pick_mode']=='1' else 'Minden jutalom külön eséllyel'))
        for group in self.model.drop_groups():
            result.append(dict(key=('mob', group['group_id']), name=target_text(group), kind='Mobdrop',
                               weighted=group['roll_kind']=='2', mode=roll_text(group)))
        from content_names import search_text
        self._targets=sorted(result, key=lambda row:(search_text(row['name']),row['kind'],str(row['key'])))
        return self._targets

    def plan_add(self, item, selected, *, amount=1, level=0, rare='0', box_chance='1', drop_chance='1', weight=1,
                 existing='skip', overrides=None):
        item = tuple(item); base = self.bases.get(item)
        if not base or base['retired'] != '0': raise api.EditorError('Válassz használható tárgyat.')
        level = int(level); rare_bp = catalog.scaled(rare, 100)
        if not 0 <= level <= (self.model.target_levels(item) or 0): raise api.EditorError('A tárgy fejlesztési szintje nem használható.')
        if not 0 <= rare_bp <= 10000: raise api.EditorError('A bónusszal létrejövés esélye 0–100% lehet.')
        if existing not in ('skip','update'): raise api.EditorError('Ismeretlen meglévő-bejegyzés szabály.')
        edits = copy.copy(self);edits.overrides=copy.deepcopy(self.overrides)
        mob_groups = {row['group_id']:copy.deepcopy(row) for row in self.model.tables['mob_drop_group']}
        mob_entries = {key:[] for key in mob_groups}
        for row in self.model.tables['mob_drop_entry']: mob_entries[row['group_id']].append(copy.deepcopy(row))
        target_map = {row['key']:row for row in self.targets()}
        preview = []; changed = 0
        for destination in dict.fromkeys(selected):
            if destination not in target_map: raise api.EditorError('A kiválasztott cél már nem található. Frissítsd a katalógust.')
            target = target_map[destination]; kind, identity = destination
            custom = (overrides or {}).get(destination, {})
            try: count = int(str(custom.get('amount',amount)))
            except ValueError: raise api.EditorError(target['name'] + ': a darabszám egész szám legyen.') from None
            if not 1 <= count <= int(base['max_stack']): raise api.EditorError(target['name'] + ': a darabszám 1–%s lehet ennél a tárgynál.' % base['max_stack'])
            rate = custom.get('rate', weight if target['weighted'] else box_chance if kind=='box' else drop_chance)
            scale = 1 if target['weighted'] else 100 if kind=='box' else catalog.DROP_CHANCE_SCALE
            raw = catalog.scaled(rate, scale)
            if raw <= 0 or raw > (2**32-1 if target['weighted'] else 100*scale):
                raise api.EditorError(target['name'] + ': az esély 0–100% közé essen, a súly pozitív egész legyen.')
            loot = edits.loot(identity) if kind=='box' else None
            rows = loot['entries'] if loot else mob_entries[identity]
            previous_rows=copy.deepcopy(rows)
            category_field, local_field = ('item_category_id','item_local_id') if kind=='box' else ('category_id','local_id')
            matches = [row for row in rows if (kind!='box' or row['reward_kind']=='1') and
                       (row.get(category_field),row.get(local_field),str(row.get('refinement_level','0'))) == (str(item[0]),str(item[1]),str(level))]
            action = 'Már szerepel · kihagyás' if matches and existing=='skip' else 'Meglévő sor módosítása' if matches else 'Hozzáadás'
            if not matches or existing=='update':
                if not matches:
                    if len(rows) >= 256: raise api.EditorError(target['name'] + ': a 256 bejegyzéses lista megtelt.')
                    row = dict(entry_no=str(len(rows)+1), refinement_level=str(level), amount=str(count), rare_attribute_chance_bp=str(rare_bp))
                    if kind=='box': row.update(reward_kind='1',item_category_id=str(item[0]),item_local_id=str(item[1]))
                    else: row.update(group_id=identity,category_id=str(item[0]),local_id=str(item[1]),killer_level_min='1',killer_level_max='65535')
                    rows.append(row); matches=[row]
                for row in matches:
                    row.update(amount=str(count),rare_attribute_chance_bp=str(rare_bp))
                    if kind=='box': row['weight']=str(raw)
                    else: row.update(chance_ppb='0' if target['weighted'] else str(raw),weight=str(raw) if target['weighted'] else '0')
                if rows!=previous_rows:
                    if kind=='box': edits.set_loot(identity,loot)
                    changed += 1
                else:action='Megegyezik · kihagyás'
            if rows==previous_rows and matches:
                raw=int(matches[0]['weight'] if kind=='box' or target['weighted'] else matches[0]['chance_ppb'])
                count=int(matches[0]['amount'])
            total = sum(int(row.get('weight') or 0) for row in rows)
            if target['weighted'] and total>2**32-1: raise api.EditorError(target['name'] + ': a súlyok összege túl nagy.')
            chance = Decimal(raw)/scale if not target['weighted'] else Decimal(100)*raw/total
            if target['weighted'] and kind=='mob': chance /= int(mob_groups[identity]['kills_per_drop'])
            preview.append(dict(target=destination,name=target['name'],kind=target['kind'],action=action,amount=str(count),rate=str(rate),
                                weighted=target['weighted'],chance=format(chance,'.5g')+'%'))
        tables = catalog.copy_tables(self.model.packet['tables'])
        for table, rows in (('mob_drop_group',list(mob_groups.values())),('mob_drop_entry',[row for key in sorted(mob_entries,key=int) for row in mob_entries[key]])):
            tables[table] = [[row[field] for field in self.packet['columns'][table]] for row in rows]
        return dict(tables=tables,containers=edits.container_rows(),preview=preview,changed=changed)

    def plan_add_group(self, entries, selected):
        """Append independent copies to every destination in one bulk plan."""
        from reward_groups import check_capacity, destination_entries, validate_items

        destinations = list(dict.fromkeys(selected))
        if not destinations:
            raise api.EditorError('Jelölj ki legalább egy ládát vagy mobdropot.')
        rows = validate_items(entries, self.model)
        targets = {target['key']: target for target in self.targets()}
        edited = copy.copy(self)
        edited.overrides = copy.deepcopy(self.overrides)
        mob_entries = {group['group_id']: [] for group in self.model.tables['mob_drop_group']}
        for entry in self.model.tables['mob_drop_entry']:
            mob_entries[entry['group_id']].append(entry)
        preview = []
        converted = {}
        for destination in destinations:
            target = targets.get(destination)
            if target is None:
                raise api.EditorError('A kiválasztott cél már nem található. Frissítsd a dropokat.')
            kind, identity = destination
            weighted = target['weighted']
            loot = self.loot(identity) if kind == 'box' else None
            current = loot['entries'] if loot else mob_entries[identity]
            try:
                check_capacity(rows, weighted=weighted, existing_rows=len(current),
                               existing_weight=sum(int(entry['weight']) for entry in current) if weighted else 0)
                mode = (kind, weighted)
                if mode not in converted:
                    converted[mode] = destination_entries(rows, target=kind, weighted=weighted)
                additions = converted[mode]
                if loot:
                    loot['entries'].extend(additions)
                    edited.set_loot(identity, loot)
                else:
                    next_number = max((int(entry['entry_no']) for entry in current), default=0) + 1
                    current.extend(dict(entry, group_id=identity, entry_no=str(number))
                                   for number, entry in enumerate(additions, next_number))
            except api.EditorError as error:
                raise api.EditorError(f"{target['name']}: {error}") from None
            preview.append(dict(name=target['name'], kind=target['kind'], action=f'{len(rows)} tárgysor hozzáadása'))

        return dict(tables=self._reward_tables(mob_entries), containers=edited.container_rows(),
                    preview=preview, changed=len(preview))

    def _reward_tables(self, mob_entries, groups=None):
        tables = catalog.copy_tables(self.model.packet['tables'])
        if groups is None:
            groups = self.model.tables['mob_drop_group']
        entries = (row for key in sorted(mob_entries, key=int) for row in mob_entries[key])
        for table, rows in (('mob_drop_group', groups), ('mob_drop_entry', entries)):
            tables[table] = [[row[field] for field in self.packet['columns'][table]] for row in rows]
        return tables

    def plan_remove(self, item, selected, *, level=None):
        """Remove every matching row, optionally restricted to one refinement."""
        item=tuple(item)
        if item not in self.bases:raise api.EditorError('Válassz tárgyat.')
        if level is not None and not 0<=int(level)<=255:raise api.EditorError('Érvénytelen fejlesztési szint.')
        key = tuple(map(str, item))
        refinement = int(level) if level is not None else None

        def matches(entry):
            return entry is not None and entry[:2] == key and (refinement is None or entry[2] == refinement)

        plan = self._plan_remove(selected, matches)
        for row in plan['preview']:
            row['level'] = 'Minden fejlesztési szint' if level is None else '+'+str(level)
        return plan

    def _group_member_rows(self, entries):
        from reward_groups import normalize_entries

        members = {}
        for index, row in enumerate(normalize_entries(entries)):
            base = self.bases.get(key_of(row))
            if base and base['content_code'] == row['content_code']:
                members.setdefault(reward_item_key(row, 'mob'), []).append(index)
        return members

    def group_match_counts(self, entries):
        keys = self._group_member_rows(entries)
        counts = Counter(('mob', row['group_id']) for row in self.model.tables['mob_drop_entry']
                         if reward_item_key(row, 'mob') in keys)
        for base in self.containers():
            key = key_of(base)
            counts['box', key] = sum(reward_item_key(row, 'box') in keys for row in self.loot(key)['entries'])
        return counts

    def plan_remove_group(self, entries, selected):
        """Remove matching item and refinement rows, including edited copies."""
        keys = self._group_member_rows(entries)
        return self._plan_remove(selected, lambda entry: entry in keys)

    def plan_update_group(self, entries, selected, *, amounts=None, chances=None, weights=None):
        """Apply per-member values to existing item and refinement rows."""
        from reward_groups import MAX_WEIGHT

        if amounts is None and chances is None and weights is None:
            raise api.EditorError('Jelöld be a darabszám vagy az esély módosítását.')
        members = self._group_member_rows(entries)
        edited = copy.copy(self)
        edited.overrides = copy.deepcopy(self.overrides)
        mob_entries = {group['group_id']: [] for group in self.model.tables['mob_drop_group']}
        for row in self.model.tables['mob_drop_entry']:
            mob_entries[row['group_id']].append(row)
        targets = {target['key']: target for target in self.targets()}
        preview = []
        changes_by_mode = {}

        def value_for(values, indices, scale, maximum, label):
            parsed = {catalog.scaled(values[index], scale) for index in indices}
            if len(parsed) != 1:
                raise api.EditorError(f'A csoportban ugyanahhoz a tárgyhoz és fejlesztési szinthez eltérő {label} tartozik.')
            value = parsed.pop()
            if not 1 <= value <= maximum:
                raise api.EditorError(f'A megadott {label} kívül esik a megengedett tartományon.')
            return str(value)

        for destination in dict.fromkeys(selected):
            target = targets.get(destination)
            if target is None:
                raise api.EditorError('A kiválasztott cél már nem található. Frissítsd a dropokat.')
            kind, identity = destination
            weighted = target['weighted']
            loot = self.loot(identity) if kind == 'box' else None
            rows = loot['entries'] if loot else mob_entries[identity]
            matched = altered = 0
            for index, row in enumerate(rows):
                key = reward_item_key(row, kind)
                if key not in members:
                    continue
                matched += 1
                mode = (key, kind, weighted)
                if mode not in changes_by_mode:
                    indices = members[key]
                    base = self.bases[int(key[0]), int(key[1])]
                    changes = {}
                    try:
                        if amounts is not None:
                            changes['amount'] = value_for(amounts, indices, 1, int(base['max_stack']), 'darabszám')
                        if weighted and weights is not None:
                            changes['weight'] = value_for(weights, indices, 1, MAX_WEIGHT, 'súly')
                        if not weighted and chances is not None:
                            scale = 100 if kind == 'box' else catalog.DROP_CHANCE_SCALE
                            field = 'weight' if kind == 'box' else 'chance_ppb'
                            changes[field] = value_for(chances, indices, scale, 100 * scale, 'esély')
                    except api.EditorError as error:
                        raise api.EditorError(f"{target['name']} · {item_name(base)} +{key[2]}: {error}") from None
                    changes_by_mode[mode] = changes
                updated = dict(row, **changes_by_mode[mode])
                if updated != row:
                    rows[index] = updated
                    altered += 1
            if weighted and altered and sum(int(row['weight']) for row in rows) > MAX_WEIGHT:
                raise api.EditorError(f"{target['name']}: a súlyok összege túl nagy.")
            if altered and loot:
                edited.set_loot(identity, loot)
            action = f'{altered} sor módosítása' if altered else 'Nincs eltérés'
            preview.append(dict(target=destination, name=target['name'], kind=target['kind'],
                                matched=matched, altered=altered, action=action))

        return dict(tables=self._reward_tables(mob_entries), containers=edited.container_rows(),
                    preview=preview, changed=sum(row['altered'] > 0 for row in preview), operation='update')

    def _plan_remove(self, selected, matches_item):
        edits=copy.copy(self);edits.overrides=copy.deepcopy(self.overrides)
        groups={row['group_id']:dict(row) for row in self.model.tables['mob_drop_group']}
        entries={key:[] for key in groups}
        for row in self.model.tables['mob_drop_entry']:entries[row['group_id']].append(dict(row))
        targets={row['key']:row for row in self.targets()};preview=[];changed=0
        for destination in dict.fromkeys(selected):
            if destination not in targets:raise api.EditorError('A kiválasztott cél már nem található. Frissítsd a katalógust.')
            kind,identity=destination;target=targets[destination]
            loot=edits.loot(identity) if kind=='box' else None
            rows=loot['entries'] if loot else entries[identity]
            kept=[row for row in rows if not matches_item(reward_item_key(row, kind))]
            removed=len(rows)-len(kept)
            action='Nem szerepel · kihagyás'
            if removed:
                changed+=1;action='Eltávolítás'
                if kind=='box':
                    if not kept:
                        # The domain already supports an explicit no-reward draw.
                        # Keep the chest usable and show this in the review.
                        kept=[dict(entry_no='1',reward_kind='9',item_category_id=None,item_local_id=None,
                                   amount='0',weight='1' if loot['pick_mode']=='1' else '10000',rare_attribute_chance_bp='0',refinement_level='0')]
                        action='Eltávolítás · üres láda marad'
                    edits.set_loot(identity,dict(pick_mode=loot['pick_mode'],entries=kept))
                elif kept:
                    entries[identity]=[dict(row,entry_no=str(number)) for number,row in enumerate(kept,1)]
                else:
                    groups.pop(identity);entries.pop(identity);action='Eltávolítás · üres droptábla törlése'
            preview.append(dict(target=destination,name=target['name'],kind=target['kind'],action=action,removed=removed,
                                remaining=len(rows)-removed))
        return dict(tables=self._reward_tables(entries, groups.values()), containers=edits.container_rows(),
                    preview=preview, changed=changed, operation='remove')

    def plan_update(self,item,selected,*,amount=None,box_chance=None,drop_chance=None,weight=None,level=None,overrides=None):
        """Change only existing matching rows; preserve every other setting."""
        item=tuple(item);base=self.bases.get(item)
        if not base:raise api.EditorError('Válassz tárgyat.')
        if all(value is None for value in (amount,box_chance,drop_chance,weight)):
            raise api.EditorError('Jelöld be a darabszám vagy az esély módosítását.')
        if level is not None and not 0<=int(level)<=255:raise api.EditorError('Érvénytelen fejlesztési szint.')
        edits=copy.copy(self);edits.overrides=copy.deepcopy(self.overrides)
        groups={row['group_id']:dict(row) for row in self.model.tables['mob_drop_group']}
        entries={key:[] for key in groups}
        for row in self.model.tables['mob_drop_entry']:entries[row['group_id']].append(dict(row))
        targets={row['key']:row for row in self.targets()};preview=[];changed=0
        for destination in dict.fromkeys(selected):
            if destination not in targets:raise api.EditorError('A kiválasztott cél már nem található. Frissítsd a katalógust.')
            target=targets[destination];kind,identity=destination;custom=(overrides or {}).get(destination,{})
            loot=edits.loot(identity) if kind=='box' else None;rows=loot['entries'] if loot else entries[identity]
            category,local=('item_category_id','item_local_id') if kind=='box' else ('category_id','local_id')
            matches=[row for row in rows if (kind!='box' or row['reward_kind']=='1') and
                (row.get(category),row.get(local))==(str(item[0]),str(item[1])) and
                (level is None or int(row.get('refinement_level') or 0)==int(level))]
            previous=copy.deepcopy(matches);count=custom.get('amount',amount) if amount is not None else None
            rate=custom.get('rate',weight if target['weighted'] else box_chance if kind=='box' else drop_chance)
            change_rate=(weight if target['weighted'] else box_chance if kind=='box' else drop_chance) is not None
            if matches and count is not None:
                try:count=int(str(count))
                except ValueError:raise api.EditorError(target['name']+': a darabszám egész szám legyen.') from None
                if not 1<=count<=int(base['max_stack']):raise api.EditorError(target['name']+': a darabszám 1–%s lehet ennél a tárgynál.'%base['max_stack'])
                for row in matches:row['amount']=str(count)
            scale=1 if target['weighted'] else 100 if kind=='box' else catalog.DROP_CHANCE_SCALE
            rate_field='weight' if kind=='box' or target['weighted'] else 'chance_ppb'
            if matches and change_rate:
                raw=catalog.scaled(rate,scale)
                if not 0<raw<=(2**32-1 if target['weighted'] else 100*scale):
                    raise api.EditorError(target['name']+': az esély 0-nál nagyobb, legfeljebb 100% lehet; a súly pozitív egész legyen.')
                for row in matches:row[rate_field]=str(raw)
            total=sum(int(row.get('weight') or 0) for row in rows)
            if target['weighted'] and total>2**32-1:raise api.EditorError(target['name']+': a súlyok összege túl nagy.')
            altered=sum(old!=new for old,new in zip(previous,matches))
            if altered:
                changed+=1
                if kind=='box':edits.set_loot(identity,loot)
            def chances(values,denominator):
                result=[]
                for row in values:
                    value=Decimal(row[rate_field])/scale if not target['weighted'] else Decimal(100)*int(row[rate_field])/denominator
                    if target['weighted'] and kind=='mob':value/=int(groups[identity]['kills_per_drop'])
                    result.append(format(value,'.5g')+'%')
                return ', '.join(dict.fromkeys(result)) or '—'
            def distinct(field,values):return ', '.join(dict.fromkeys(str(row[field]) for row in values)) or '—'
            before_total=total-sum(int(row.get('weight') or 0) for row in matches)+sum(int(row.get('weight') or 0) for row in previous)
            preview.append(dict(target=destination,name=target['name'],kind=target['kind'],weighted=target['weighted'],matched=len(matches),altered=altered,
                action='Módosítás · %d sor'%altered if altered else 'Megegyezik · kihagyás' if matches else 'Nem szerepel · kihagyás',
                amount=str(count) if amount is not None else distinct('amount',matches),
                rate=str(rate) if change_rate else ', '.join(dict.fromkeys(catalog.display(row[rate_field],scale) for row in matches)) or '—',
                chance=chances(matches,total),before_amount=distinct('amount',previous),before_rate=distinct(rate_field,previous),before_chance=chances(previous,before_total)))
        tables=catalog.copy_tables(self.model.packet['tables'])
        for table,values in (('mob_drop_group',list(groups.values())),('mob_drop_entry',[row for key in sorted(entries,key=int) for row in entries[key]])):
            tables[table]=[[row[field] for field in self.packet['columns'][table]] for row in values]
        return dict(tables=tables,containers=edits.container_rows(),preview=preview,changed=changed,operation='update')

    def occurrences(self,item,level=None):
        """Counts by destination, including duplicate rows and all levels."""
        item=tuple(map(str,item));result={}
        for base in self.containers():
            key=key_of(base)
            result[('box',key)]=sum(row['reward_kind']=='1' and (row['item_category_id'],row['item_local_id'])==item and
                (level is None or int(row.get('refinement_level') or 0)==int(level)) for row in self.loot(key)['entries'])
        for row in self.model.tables['mob_drop_entry']:
            if (row['category_id'],row['local_id'])==item and (level is None or int(row['refinement_level'])==int(level)):
                key=('mob',row['group_id']);result[key]=result.get(key,0)+1
        return result


def save_contents(root, before, key, document):
    model = Rewards(before); model.set_loot(key, document)
    return catalog.commit_drop_tables(root,before,model.model.encode(),dict(container=list(key)),containers=model.container_rows())


def save_bulk(root, before, plan):
    return catalog.commit_drop_tables(root,before,plan['tables'],dict(bulk_targets=[dict(name=row['name'],kind=row['kind'],action=row['action'])
                                     for row in plan['preview']]),containers=plan['containers'])
