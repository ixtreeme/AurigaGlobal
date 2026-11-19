"""OFFLINE additive catalog installer and atomic equipment importer helpers.

Caller must own the stopped local package's supervisor lock and have a verified
SQL checkpoint. These helpers are never imported by the running game server.
"""
from pathlib import Path
import json,subprocess
from legacy_item_migration import MigrationError
from stage_exported_catalog import INSERT_ORDER,literal,identifier,validate_candidate
from upgrade_exported_catalog import native

ROOT=Path(__file__).resolve().parents[3]

def apply_schema(db):
    from mariadb_client import statements
    version=db.query('SELECT schema_version FROM protos.catalog_format')
    if version in ([['30']],[['31']]):
        for sql in statements((ROOT/'db/migrations/item_combat/037_equipment_conversion.sql').read_text(encoding='utf-8')):db.query(sql)
    else:raise MigrationError('Unsupported cutover schema')
    for sql in statements((ROOT/'db/migrations/item_instance/022_legacy_equipment_import.sql').read_text(encoding='utf-8')):db.query(sql)

def install_additive(db,helper,connection,candidate):
    blank=native(helper,dict(action='blank'));validate_candidate(candidate,blank)
    expected=native(helper,dict(action='validate',tables=candidate['tables']))['hash']
    if expected!=candidate['hash']:raise MigrationError('Unvalidated candidate')
    before=native(helper,dict(action='read',connection=connection))
    if before['columns']!=candidate['columns']:raise MigrationError('Schema column drift')
    for table in before['tables']:
        if not {tuple(r) for r in before['tables'][table]}<={tuple(r) for r in candidate['tables'][table]}:
            raise MigrationError('Issued catalog rows would change: '+table)
    counts={}
    db.query('START TRANSACTION')
    try:
        for table in INSERT_ORDER:
            old={tuple(r) for r in before['tables'][table]}
            rows=[r for r in candidate['tables'][table] if tuple(r) not in old]
            counts[table]=len(rows)
            columns=','.join(map(identifier,candidate['columns'][table]))
            for offset in range(0,len(rows),64):
                db.query('INSERT INTO protos.'+identifier(table)+' ('+columns+') VALUES '+','.join('('+','.join(map(literal,row))+')' for row in rows[offset:offset+64]))
        for category,next_id in candidate['sequences'].items():
            if int(next_id)<int(before['sequences'][category]):raise MigrationError('Category allocator would rewind')
            db.query('UPDATE protos.item_local_id_sequence SET next_local_id='+str(int(next_id))+' WHERE category_id='+str(int(category)))
        db.query('COMMIT')
    except BaseException:
        db.query('ROLLBACK');raise
    after=native(helper,dict(action='read',connection=connection))
    if after['hash']!=expected or after['sequences']!=candidate['sequences']:raise MigrationError('Catalog readback differs')
    return dict(hash=expected,added=counts,preserved_items=len(before['tables']['item_base']))

def import_tool(helper,connection,request,expected_ok=True):
    process=subprocess.run([str(Path(helper).resolve(strict=True))],input=json.dumps(dict(request,connection=connection)),capture_output=True,text=True,encoding='utf-8',timeout=120,
                           creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    prefix='AURIGA_IMPORT_RESULT '
    rows=[json.loads(line[len(prefix):]) for line in process.stdout.splitlines() if line.startswith(prefix)]
    if len(rows)!=1:raise MigrationError('Missing native import result')
    result=rows[0]
    if result.get('ok')!=expected_ok or bool(process.returncode)==expected_ok:raise MigrationError('Native import: '+str(result.get('error','unexpected exit')))
    return result.get('result') if expected_ok else result
