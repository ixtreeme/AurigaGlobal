"""Shared catalog documents and quest literals for the typed item reviews."""

import json


def definition(model, category, local, code, icon):
    doc = model.new(category)
    doc['base'].update(local_id=str(local), content_code=code, name_key=f'item.{code}.name',
                       description_key=f'item.{code}.description', max_stack='1', icon_asset=icon)
    doc['child']['local_id'] = str(local)
    doc['trade'] = dict(allow_drop='0', allow_sell='0', allow_trade='0')
    if category == 10:
        doc['child'].update(dispatch_kind='2', allowed_contexts='1', allow_observer='0')
    return doc


def text(value):
    return json.dumps(value, ensure_ascii=False)
