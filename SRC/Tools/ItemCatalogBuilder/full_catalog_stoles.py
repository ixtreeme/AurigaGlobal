"""Effect-only stoles intentionally use empty.gr2; their MSE is the real visual."""
import re
from convert_legacy_equipment import asset
from legacy_item_migration import MigrationError

PREFIX='Metin2SashEffect/'

def stole_appearance(project,entry):
    if entry.get('address'):
        return entry['address'],[]
    if entry.get('model')!='item/wing/empty.gr2':
        raise MigrationError('missing_stole_effect_asset')
    path=entry.get('effectPath','')
    if not path.endswith('.mse') or '..' in path or '\\' in path or path.startswith('/') or entry.get('effectBone') not in ('Bip01','Bip01 Spine2'):
        raise MigrationError('invalid_stole_effect_attachment')
    catalog='Assets/Metin2/Resources/Metin2EffectCatalog.asset'
    if not (project/catalog).is_file() or not re.search(r'(?m)^  - key: '+re.escape(path)+r'\s*$',(project/catalog).read_text(encoding='utf-8-sig')):
        raise MigrationError('missing_stole_effect_registration')
    definition='Assets/Metin2/Generated/Effects/Definitions/'+path[:-4]+'.asset'
    proof=asset(project,definition)
    if re.search(r'(?m)^\s*status: (?:missing|ambiguous)\s*$',(project/definition).read_text(encoding='utf-8-sig')):
        raise MigrationError('missing_stole_effect_resource')
    # The client resolves the exact authored effect/bone via the shared catalog.
    return PREFIX+path[:-4],[proof,asset(project,catalog)]
