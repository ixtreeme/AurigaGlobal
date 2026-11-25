"""Produce the four approved voucher pools as selected-instance quest redemptions."""

import json
from pathlib import Path

from quest_item_review import definition, text

RULES = Path(__file__).resolve().parents[3] / "config/item_combat/quest_costume_vouchers.json"


def vouchers(model, first):
    rows = json.loads(RULES.read_bytes())
    active = {row['content_code'] for row in model.tables['item_base']
              if row['category_id'] == '4' and row['retired'] == '0'}
    documents, labels, handlers = [], {}, []
    seen = set()
    for offset, (row, slots) in enumerate(zip(rows, (12, 12, 10, 10), strict=True)):
        rewards = row['rewards']
        if len(set(rewards)) != slots or len(rewards) != slots or seen.intersection(rewards) or not active.issuperset(rewards):
            raise ValueError('Invalid or unavailable voucher rewards: ' + row['code'])
        seen.update(rewards)
        documents.append(definition(model, 10, first + offset, row['code'], 'icon/item/89003.tga'))
        labels[row['code']] = [row['hu'], row['en']]
        handlers.extend([f'        when {text(row["code"])}.typed_use begin',
                         '            costume_system.redeem({',
                         *['                ' + text(reward) + ',' for reward in rewards],
                         '            })', '        end'])
    code = '''quest costume_system begin
    state start begin
        function redeem(pool)
            local language = pc.get_language()
            if not pc.can_warp() then
                syschat(gameforge[language].reset_scroll._35)
                return
            end
            local slot = game.typed_item_draw(#pool)
            if not slot then return end
            local category, reward = game.typed_item_key(pool[slot])
            if category ~= 4 then
                syschat(language == "hu" and "Ez a jutalom most nem elérhető." or "This reward is unavailable.")
                return
            end
            local ok, reason = pc.typed_item_redeem({{category, reward, 0, 1}}, 0)
            if not ok then
                if reason == "inventory_full" then
                    syschat(language == "hu" and "Nincs elég hely a leltárban." or "Make room in your inventory.")
                else
                    syschat(language == "hu" and "Az utalvány beváltása sikertelen." or "Could not redeem the voucher.")
                end
            end
        end
'''
    return documents, labels, code + '\n'.join(handlers) + '\n    end\nend\n'
