"""Small PSD type becomes live client text (C48), shared by the native Nexoria exporters.

The windows are drawn at roughly half of their PSD size, so 14-22 px PSD type ends up 7-12 px
on screen when it is baked into the artwork. The exporters therefore hide every small type
layer in their native renders and record it on the asset it belonged to; the client draws it
as real text, sized for the actual screen. Two kinds of companion art move with the grown
text and are exported as separate "followers": a caption's icon ("X — ikon" next to
"X — felirat") and a section title's diamond and rule ("Szakasz gyémánt/díszvonal — X").
The free space each text may grow into (to the next element on its line, inside its
containing shape, half way to a neighbouring text) is measured here from the source layout.
"""
import re

VERSION = 1
LIVE_MAX = 30.0      # PSD px; larger type (window titles) stays baked
PAD = 8.0            # inner padding kept to a containing shape
AIR = 2.0            # extra air kept between two texts
GAP = 10.0           # distance kept to a non-text neighbour
MIN_RULE = 48.0      # a section rule never shrinks below this


def _box(layer):
    x0, y0, x1, y1 = layer.bbox
    return (float(x0), float(y0), float(x1), float(y1)) if x1 > x0 and y1 > y0 else None


def _gradient(effect, color):
    try:
        stops = [s[b'Clr '] for s in effect.gradient[b'Clrs']]
        mean = [sum(float(s[k]) for s in stops) / len(stops) / 255 for k in (b'Rd  ', b'Grn ', b'Bl  ')]
        k = float(effect.opacity) / 100
        return [color[i] * (1 - k) + mean[i] * k for i in range(3)] + [color[3]]
    except Exception:
        return color


def style(layer):
    """Size (PSD px), RGBA, Unity alignment (0 left, 1 centre, 2 right), face and shadow of a type layer."""
    data = layer.engine_dict
    sheet = data['StyleRun']['RunArray'][0]['StyleSheet']['StyleSheetData']
    fonts = [str(f['Name']).strip("'") for f in layer.resource_dict['FontSet']]
    font = fonts[int(sheet.get('Font', 0))] if fonts else ''
    t = layer.transform
    size = float(sheet.get('FontSize', 12)) * (abs(float(t[3])) if t else 1.0)
    fill = sheet.get('FillColor', {}).get('Values') or [1, 1, 1, 1]
    color = [float(fill[1]), float(fill[2]), float(fill[3]), float(fill[0]) * layer.opacity / 255]
    shadow = False
    if layer.has_effects():
        for effect in layer.effects:
            if not effect.enabled: continue
            kind = type(effect).__name__
            if kind == 'DropShadow': shadow = True
            elif kind == 'GradientOverlay': color = _gradient(effect, color)
    justification = int(data['ParagraphRun']['RunArray'][0]['ParagraphSheet']['Properties'].get('Justification', 0))
    face = 'display' if 'nexoria' in font.lower() else 'semibold' if re.search('semibold|bold', font, re.I) else 'regular'
    return size, [round(c, 4) for c in color], {0: 0, 1: 2, 2: 1}.get(justification, 0), face, shadow


def measure(layers):
    """Live type layers by path (text, style, free space) and each one's followers."""
    by_path = {p: l for l, p in layers}
    flags = {p: bool(l.visible) for l, p in layers}

    def shown(p, top):
        """Visible with the text's page: its own top-level group counts as shown, other tab pages
        ("Oldal — …", only one of them is ever on screen) never do."""
        if p[0] != top and p[0].startswith('Oldal') and top.startswith('Oldal'): return False
        return all(flags[p[:i]] or (i == 1 and p[0] == top) for i in range(1, len(p) + 1))

    live = {}
    for layer, p in layers:
        if layer.kind != 'type' or _box(layer) is None: continue
        size, color, align, face, shadow = style(layer)
        if size > LIVE_MAX: continue
        text = layer.text.replace('\r\n', '\n').replace('\r', '\n').replace('\x03', '\n').strip('\n')
        if not text.strip(): continue
        live[p] = dict(Text=text, Rect=list(_box(layer)), Size=round(size, 2), Color=color, Align=align, Face=face,
                       Shadow=shadow, Lines=text.count('\n') + 1, Followers=[])

    # Followers: a caption's icon, a section title's diamond and rule.
    follow = {}
    for p, info in live.items():
        name, parent = p[-1], p[:-1]
        chain = []
        if name.endswith(' — felirat'):
            icon = parent + (name[:-len('felirat')] + 'ikon',)
            if icon in by_path and _box(by_path[icon]): chain.append((icon, 'before' if _box(by_path[icon])[2] <= info['Rect'][0] + 2 else 'after', False))
        if name.startswith('Szakasz — '):
            title = name[len('Szakasz — '):]
            for part, stretch in (('Szakasz gyémánt — ', False), ('Szakasz díszvonal — ', True)):
                q = parent + (part + title,)
                if q in by_path and _box(by_path[q]): chain.append((q, 'after', stretch))
        chain.sort(key=lambda c: _box(by_path[c[0]])[0])
        follow[p] = chain

    # Free space, measured on the source layout.
    solid = [(p, _box(l)) for l, p in layers if not l.is_group() and _box(l) is not None and 'rejtett' not in l.name.lower()]
    for p, info in live.items():
        x0, y0, x1, y1 = info['Rect']; top = p[0]
        own = {q for q, _, _ in follow[p]} | {p}
        others = [(q, b) for q, b in solid if q not in own and shown(q, top)]
        is_text = lambda q: q in live or by_path[q].kind == 'type'
        left, right = 0.0, 1e9
        cy0, cy1 = 0.0, 1e9
        for q, (a0, b0, a1, b1) in others:
            if a0 <= x0 + 1 and b0 <= y0 + 1 and a1 >= x1 - 1 and b1 >= y1 - 1:   # container
                left, right = max(left, a0 + PAD), min(right, a1 - PAD)
                cy0, cy1 = max(cy0, b0 + 2), min(cy1, b1 - 2)
                continue
            if b1 <= y0 or b0 >= y1: continue                                     # not on this line
            if a0 >= x1 - 2: right = min(right, (x1 + a0) / 2 - AIR if is_text(q) else a0 - GAP)
            elif a1 <= x0 + 2: left = max(left, (a1 + x0) / 2 + AIR if is_text(q) else a1 + GAP)
        chain = follow[p]
        before = [(_box(by_path[q]), s) for q, side, s in chain if side == 'before']
        after = [(_box(by_path[q]), s) for q, side, s in chain if side == 'after']
        span = lambda boxes: sum(b[2] - b[0] for b, _ in boxes) + sum(max(0.0, boxes[i + 1][0][0] - boxes[i][0][2]) for i in range(len(boxes) - 1))
        if after and after[-1][1]:                                                 # section rule
            rule = after[-1][0]
            fixed = after[:-1]
            used = (fixed[0][0][0] - x1 if fixed else rule[0] - x1) + span(fixed) + (rule[0] - fixed[-1][0][2] if fixed else 0)
            right = min(right, rule[2] - MIN_RULE - used)
        elif after:
            right = min(right, right - (after[-1][0][2] - x1))
        extra = (x0 - before[0][0][0]) if before else 0.0
        if info['Align'] == 0: width = right - x0
        elif info['Align'] == 2: width = x1 - left - extra
        else:
            unit0, unit1 = x0 - extra, x1
            centre = (unit0 + unit1) / 2
            width = 2 * min(centre - left, right - centre) - extra
        width = max(width, x1 - x0)
        # Vertical room over the horizontal span the text may take.
        if info['Align'] == 0: h0, h1 = x0, x0 + width
        elif info['Align'] == 2: h0, h1 = x1 - width, x1
        else: h0, h1 = (x0 + x1) / 2 - width / 2, (x0 + x1) / 2 + width / 2
        for q, (a0, b0, a1, b1) in others:
            if a0 <= x0 + 1 and b0 <= y0 + 1 and a1 >= x1 - 1 and b1 >= y1 - 1: continue
            if a1 <= h0 or a0 >= h1: continue
            if b1 <= y0 + 1: cy0 = max(cy0, (b1 + y0) / 2 + AIR if is_text(q) else b1 + 2)
            elif b0 >= y1 - 1: cy1 = min(cy1, (y1 + b0) / 2 - AIR if is_text(q) else b0 - 2)
        centre_y = (y0 + y1) / 2
        ink = 2 * min(centre_y - cy0, cy1 - centre_y)
        info['MaxWidth'] = round(width, 1)
        info['MaxSize'] = round(max(info['Size'], info['Size'] * ink / max(1.0, y1 - y0)), 2)
    return live, follow


def prepare(layers, specs, assets, add_asset):
    """Moves small type out of every render in ``specs`` and onto its asset record in ``assets``.

    ``layers`` is the exporter's [(layer, path)] walk, ``add_asset(name, predicate)`` its own
    asset factory (used for the followers). Call after every asset is declared, before rendering.
    """
    live, follow = measure(layers)
    # Hide live text (and its followers) in every render it appeared in; record it on that asset.
    follower_assets = {}
    records = {a['Asset']: a for a in assets}
    for spec in list(specs):
        desired = {tuple(s['Path']): s['Visible'] for s in spec['States']}
        visible = lambda p: all(desired.get(p[:i]) for i in range(1, len(p) + 1))
        texts = []
        for p, info in live.items():
            if not visible(p): continue
            chain = [f for f in follow[p] if visible(f[0])]
            hidden = {p} | {f for f, _, _ in chain}
            for s in spec['States']:
                if tuple(s['Path']) in hidden: s['Visible'] = False
            texts.append(dict(info, Followers=[dict(Asset=_follower(follower_assets, q, add_asset), Side=side, Stretch=stretch) for q, side, stretch in chain]))
        if texts: records[spec['Asset']]['Texts'] = texts
    return len(live)


def refresh(saved, assets):
    """A cached render keeps its pixels; its live-text records are re-measured from ``assets``."""
    fresh = {a['Asset']: a for a in assets}
    for record in saved:
        texts = fresh.get(record['Asset'], {}).get('Texts')
        if texts: record['Texts'] = texts
        else: record.pop('Texts', None)


def _follower(cache, path, add_asset):
    if path not in cache:
        slug = re.sub(r'[^a-z0-9]+', '_', path[-1].lower().translate(str.maketrans('áéíóöőúüű', 'aeiooouuu'))).strip('_')
        cache[path] = add_asset('f_' + slug + '_' + str(len(cache)), lambda layer, p, q=tuple(path): p == q[:len(p)])
    return cache[path]


def crop(record, raster, target):
    """Crops a native render; an asset that only held live text keeps a transparent box under it."""
    bounds = raster.getbbox()
    if bounds is None:
        if not record.get('Texts'): raise RuntimeError('Empty native export: ' + record['Asset'])
        rects = [t['Rect'] for t in record['Texts']]
        bounds = (int(min(r[0] for r in rects)), int(min(r[1] for r in rects)), int(max(r[2] for r in rects)) + 1, int(max(r[3] for r in rects)) + 1)
    raster.crop(bounds).save(target)
    return list(bounds)
