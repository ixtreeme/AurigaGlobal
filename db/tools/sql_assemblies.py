"""Reviewed assembly contracts for query strings built over several C++ calls.

File hashes in sql_assemblies.json force review when a builder changes outside
its initial SQL literal. Literal SQL statements are validated directly instead.
"""
import re
from source_sql import ROOT, literals, formatted_sql, feature_macros, active_text, without_comments


def attributes(prefix='attrtype', value='attrvalue'):
    return [part for i in range(8) for part in (prefix + str(i), value + str(i))]


def insert(table, columns, verb='INSERT'):
    return verb + ' INTO ' + table + ' (' + ','.join('`' + name + '`' for name in columns) + ') VALUES (' + ','.join('?' for _ in columns) + ')'


def assembled(row, kind):
    sql = row['sql']
    sockets = ['socket' + str(i) for i in range(3)]
    if kind in ('item_cache', 'item_award'):
        columns = ['id', 'owner_id', 'window', 'pos', 'count', 'vnum']
        if kind == 'item_cache': columns += ['lockedattr']
        columns += sockets + attributes()
        return insert('item', columns, 'REPLACE' if kind == 'item_cache' else 'INSERT')
    if kind in ('mob_proto', 'item_proto'):
        body = (ROOT / row['file']).read_text(encoding='utf-8')
        languages = re.findall(r'"(\w+)"', re.search(r'languages\[\]\s*=\s*\{(.*?)\}', body, re.S)[1])
        start = 'type, rank, battle_type' if kind == 'mob_proto' else 'gold, shop_buy_price'
        suffix = [value for _, _, _, value in literals(row['file']) if value.startswith(start) and ' FROM ' + kind + '%s' in value]
        if len(suffix) != 1: raise ValueError('Cannot uniquely resolve proto builder suffix')
        return formatted_sql(sql + ''.join('locale_name_' + language + ', ' for language in languages) + suffix[0])
    if kind == 'player_create':
        text = active_text(without_comments((ROOT / row['file']).read_text()), feature_macros())
        return insert('player', re.findall(r'\badd\("([^"]+)"', text))
    if kind == 'player_save':
        return formatted_sql(sql) + 'skill_level=?, quickslot=? WHERE id=?'
    if kind == 'mount_save':
        return insert('account_mount_inventory', ['id','account_id','slot','vnum','count'] + sockets + attributes(), 'REPLACE')
    shop_columns = ['item_id','owner_id','price_yang','vnum','count'] + sockets + attributes('attr','attrval') + ['is_sold','locked_attr','expiration']
    safebox_columns = ['item_id','owner_id','vnum','count'] + sockets + attributes('attr','attrval') + ['locked_attr','expiration']
    auction_columns = ['owner_id','duration','name','vnum','count','init_yang'] + sockets + attributes('attr','attrval') + ['locked_attr','expiration']
    if kind == 'shop_insert': return insert('player.offlineshop_shop_items', shop_columns)
    if kind == 'safebox_insert': return insert('player.offlineshop_safebox_items', safebox_columns)
    if kind == 'auction_insert': return insert('player.offlineshop_auctions', auction_columns)
    if kind == 'shop_select':
        return 'SELECT ' + ','.join(shop_columns) + ' FROM player.offlineshop_shop_items'
    if kind == 'auction_select':
        columns = ['owner_id','duration','name','init_yang','vnum','count'] + sockets + attributes('attr','attrval') + ['locked_attr','expiration']
        return 'SELECT ' + ','.join(columns) + ' FROM player.offlineshop_auctions'
    if kind == 'safebox_select':
        return 'SELECT ' + ','.join(c for c in safebox_columns if c != 'owner_id') + ' FROM player.offlineshop_safebox_items WHERE owner_id=?'
    raise ValueError('Unknown assembly kind: ' + kind)
