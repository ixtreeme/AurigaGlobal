"""Explicit, exact-source retirement decisions; changed merge data needs review."""
import json
from pathlib import Path
from legacy_item_migration import value_hash
PATH=Path(__file__).resolve().parents[3]/'config/item_combat/full_proto_source_retirements.json'
POLICY=json.loads(PATH.read_text(encoding='utf-8'))
RETIREMENTS={r['source_id']:r for r in POLICY['items']}
if POLICY['version']!=1 or len(RETIREMENTS)!=len(POLICY['items']):raise ValueError('invalid_full_proto_retirement_policy')

def retired_original(row):
    decision=RETIREMENTS.get(row['vnum'])
    return decision['reason'] if decision and decision['source_row_hash']==value_hash(row) else None
