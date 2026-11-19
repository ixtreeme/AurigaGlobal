"""Localized editor labels. Database identities stay in widget user data."""
from functools import lru_cache
import json
from pathlib import Path
import sys
import unicodedata


def search_text(value):
    return ''.join(c for c in unicodedata.normalize('NFKD', str(value).casefold())
                   if not unicodedata.combining(c))


@lru_cache(maxsize=1)
def names():
    bundled = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parent)) / 'content_names_hu.json'
    source = Path(__file__).resolve().parents[3] / 'config/server_manager/content_names_hu.json'
    for path in (bundled, source):
        if path.is_file():
            value = json.loads(path.read_text(encoding='utf-8-sig'))
            if value.get('version') != 1 or value.get('language') != 'hu':
                raise ValueError('Nem támogatott névjegyzék: ' + str(path))
            return value
    return dict(items={}, mobs={})


def item_name(base):
    value = names()['items'].get(base.get('name_key', ''))
    if value:
        return value
    # A prototype with no name must not masquerade as a translated code.
    return 'Névtelen tárgy'


def mob_name(identity):
    return mob_record(identity).get('name') or 'Névtelen szörny'


def mob_record(identity):
    return names()['mobs'].get(str(identity), {})


def mob_choices():
    return sorted(names()['mobs'].items(), key=lambda pair: (search_text(pair[1]['name']),
                                                           pair[1].get('level', 0), int(pair[0])))


def item_sort_key(base):
    return search_text(item_name(base)), int(base['category_id']), int(base['local_id'])
