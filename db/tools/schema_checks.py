"""Data round trips and invariants for the source-derived schema."""
from source_sql import enum_values

# Historical SQL labels at unchanged C++ IDs. New types use their C++ spelling.
APPLY_ALIASES = {
    55:'MAGIC_ATT_GRADE_BONUS', 56:'MAGIC_DEF_GRADE_BONUS',
    59:'ATT_BONUS_TO_WARRIOR', 60:'ATT_BONUS_TO_ASSASSIN', 61:'ATT_BONUS_TO_SURA',
    62:'ATT_BONUS_TO_SHAMAN', 63:'ATT_BONUS_TO_MONSTER', 64:'ATT_BONUS',
    85:'MAGIC_ATT_BONUS_PER', 86:'MELEE_MAGIC_ATT_BONUS_PER',
    90:'RESIST_CRITICAL', 91:'RESIST_PENETRATE', 119:'Danni Medi[PVM]', 121:'ATTBONUS_RUNE',
}


def check_schema(client, names, remap):
    passed = []
    def check(ok, name):
        if not ok: raise RuntimeError('Schema invariant failed: ' + name)
        passed.append(name)
    def q(sql): return client.query(remap(sql))
    def scalar(sql): return q(sql)[0][0]
    slot_columns = client.query("SELECT COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='" + names['player'] + "' AND TABLE_NAME='player_index' AND COLUMN_NAME REGEXP '^pid[0-9]+$' ORDER BY ORDINAL_POSITION")
    check([row[0] for row in slot_columns] == ['pid1','pid2','pid3','pid4'], 'player_index has exactly four character slots')
    def expect_error(sql, name):
        try: q(sql)
        except RuntimeError: passed.append(name); return
        raise RuntimeError('Schema accepted invalid data: ' + name)
    def columns(database, table):
        return [row[0] for row in client.query("SELECT COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='" + names[database] + "' AND TABLE_NAME='" + table + "' ORDER BY ORDINAL_POSITION")]

    expected_extra = ['vnum','rarity','bonus_type0','bonus_value0','bonus_type1','bonus_value1']
    check(columns('player','item_extra_proto') == expected_extra, 'item_extra_proto six-column positional layout')
    check(columns('player','ishop_log') == ['accountID','playerName','buyDate','buyTime','ipAdress','itemVnum','itemCount','itemPrice'], 'ishop_log eight-column positional layout')
    check(len(columns('player','ishop_data')) == 30, 'ishop_data thirty-column positional layout')
    check(columns('player','guild_member') == ['pid','guild_id','grade','is_general','offer'], 'guild_member positional layout')
    check(len(columns('player','new_petsystem')) == 21, 'growth pet positional layout')
    for name in ('account_mount_inventory', 'sms_pool'):
        check(bool(columns('player',name)) and not columns('account',name), name + ' routes to player')
    check(columns('player','player') == columns('player','player_deleted'), 'deleted-player archive retains player column order')
    for table in ('item','account_mount_inventory','item_award'):
        check(all(name in columns('player',table) for i in range(8) for name in ('attrtype'+str(i),'attrvalue'+str(i))), table + ' stores all eight attributes')
    collations = client.query("SELECT TABLE_NAME,ENGINE,TABLE_COLLATION FROM information_schema.TABLES WHERE TABLE_SCHEMA IN (" + ','.join("'"+value+"'" for value in names.values()) + ')')
    check(len(collations) == 102 and all(row[1] == 'InnoDB' and row[2] == 'utf8mb4_unicode_ci' for row in collations), 'all 102 tables use transactional UTF8 storage')

    client.query('START TRANSACTION')
    try:
        q("INSERT INTO account.account (id,login,password) VALUES(7,'schema_fixture',PASSWORD('fixture_password'))")
        check(scalar('SELECT LENGTH(password) FROM account.account WHERE id=7') == '41', 'native PASSWORD hash is stored intact')
        q("INSERT INTO player.player (id,account_id,name) VALUES(7,7,'SchemaFixture')")
        check(q('SELECT skill_level,quickslot FROM player.player WHERE id=7')[0] == [None,None], 'missing player blobs retain the loader NULL fallback')
        expect_error('UPDATE player.player SET skill_level=0x00 WHERE id=7', 'short skill blob rejected before native memcpy')
        expect_error('UPDATE player.player SET quickslot=0x00 WHERE id=7', 'short quickslot blob rejected before native memcpy')
        q('UPDATE player.player SET skill_level=REPEAT(CHAR(0),2550),quickslot=REPEAT(CHAR(0),108) WHERE id=7')
        check(q('SELECT OCTET_LENGTH(skill_level),OCTET_LENGTH(quickslot) FROM player.player WHERE id=7')[0] == ['2550','108'], 'native packed player blob sizes')
        for value in (-(1 << 63), (1 << 63) - 1, 5000000001):
            for table, column in (('account.account','coins'),('player.player','gold')):
                q('UPDATE ' + table + ' SET ' + column + '=' + str(value) + ' WHERE id=7')
                check(scalar('SELECT '+column+' FROM '+table+' WHERE id=7') == str(value), table + '.' + column + ' signed64 ' + str(value))
        q('UPDATE player.player SET hp=18446744073709551615,mp=18446744073709551615,horse_hp_droptime=18446744073709551615 WHERE id=7')
        check(q('SELECT hp,mp,horse_hp_droptime FROM player.player WHERE id=7')[0] == ['18446744073709551615'] * 3, 'unsigned64 player HP/SP/horse deadlines')
        for value in (-(1 << 63), (1 << 63) - 1):
            q("REPLACE INTO player.quest (dwPID,szName,szState,lValue) VALUES(0,'schema','time',"+str(value)+')')
            check(scalar("SELECT lValue FROM player.quest WHERE dwPID=0 AND szName='schema' AND szState='time'") == str(value), 'signed64 quest/event flag ' + str(value))
        q("INSERT INTO player.guild (id,name,master) VALUES(7,'SchemaGuild',7)")
        check(scalar('SELECT HEX(skill) FROM player.guild WHERE id=7') == '00' * 12, 'guild skill default is a full zeroed binary snapshot')
        binary = '00ff80fe0102030405060708'
        q('UPDATE player.guild SET skill=0x' + binary + ' WHERE id=7')
        check(scalar('SELECT HEX(skill) FROM player.guild WHERE id=7').lower() == binary, 'guild skill binary bytes survive character sets')
        q("INSERT INTO player.mob_proto(vnum,name,locale_name_hu,st,size) VALUES(7,REPEAT('n',35),REPEAT('h',32),300,300)")
        check(q('SELECT CHAR_LENGTH(name),CHAR_LENGTH(locale_name_hu),st,size FROM player.mob_proto WHERE vnum=7')[0] == ['35','32','300','300'], 'legacy mob labels and numeric storage retain original values')
        q("INSERT INTO player.item_attr(apply,talisman) VALUES('MAX_HP',3000),('MAX_HP',3)")
        check(scalar("SELECT COUNT(*) FROM player.item_attr WHERE apply='MAX_HP'") == '2', 'legacy attribute definitions are retained without unique-key row loss')
        q('INSERT INTO log.money_log(time,type,vnum,gold) VALUES(NOW(),1,-1310082968,1)')
        check(scalar('SELECT vnum FROM log.money_log LIMIT 1') == '-1310082968', 'legacy signed log identifiers survive import')
        q('INSERT INTO player.affect VALUES(7,1,1,10,0,60,0),(7,1,1,20,0,60,0)')
        check(scalar('SELECT COUNT(*) FROM player.affect WHERE dwPID=7') == '2', 'affects with distinct values retain their four-column source identity')
        q("INSERT INTO log.levellog VALUES('SchemaFixture',1,NOW(),7,7,0),('SchemaFixture',2,NOW(),7,7,1)")
        check(scalar("SELECT COUNT(*) FROM log.levellog WHERE name='SchemaFixture'") == '2', 'multiple level history rows per character remain valid')
        q('INSERT INTO player.change_empire VALUES(7,1,NOW()),(7,2,NOW())')
        check(scalar('SELECT COUNT(*) FROM player.change_empire WHERE account_id=7') == '2', 'legacy empire-change rows remain archived without collapsing duplicates')
        q('INSERT INTO player.ishop_data (id,itemVnum) VALUES(7,1)')
        check(all(q('SELECT offerTime,addedTime FROM player.ishop_data WHERE id=7')[0]), 'item shop dates are non-NULL for native string readers')
        for table, key in (('offlineshop_shop_items','item_id'),('offlineshop_safebox_items','item_id'),('offlineshop_offers','offer_id')):
            q('INSERT INTO player.'+table+' ('+key+',owner_id) VALUES(0,77)')
            check(int(scalar('SELECT '+key+' FROM player.'+table+' WHERE owner_id=77')) > 0, table + ' allocates an ID for native zero-ID INSERT')
        for symbol, value in enum_values('SRC/Server/common/length.h','EWindows').items():
            if symbol == 'RESERVED_WINDOW': continue
            q('INSERT INTO player.item (id,window) VALUES('+str(1000+value)+','+str(value)+')')
            check(q('SELECT window,window+0 FROM player.item WHERE id='+str(1000+value))[0] == [symbol,str(value)], 'item window ordinal ' + symbol)
        expected_type = None
        for table in ('item_attr','item_attr_rare','item_attr_rare_8'):
            column_type = client.query("SELECT COLUMN_TYPE FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='"+names['player']+"' AND TABLE_NAME='"+table+"' AND COLUMN_NAME='apply'")[0][0]
            if expected_type is None: expected_type = column_type
            check(column_type == expected_type and column_type.startswith('enum('), table + ' shares the source ordinal contract')
        for symbol, value in enum_values('SRC/Server/common/length.h','EApplyTypes').items():
            if symbol in ('APPLY_NONE','MAX_APPLY_NUM'): continue
            q('INSERT INTO player.item_attr (apply) VALUES('+str(value)+')')
            name = APPLY_ALIASES.get(value, symbol.removeprefix('APPLY_'))
            check(q('SELECT apply,apply+0 FROM player.item_attr WHERE apply+0='+str(value))[0] == [name,str(value)], 'attribute ordinal ' + symbol)
    finally: client.query('ROLLBACK')
    return passed
