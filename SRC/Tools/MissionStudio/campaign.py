"""Campaign layout and cross-mission gates in the existing native Mission schema."""

from math import isfinite

from mission_source import done_flag, new_mission, new_package

KINDS = {"main": "Főküldetés", "side": "Mellékküldetés", "secret": "Titkos küldetés"}
GATE = "studio.campaign.parents"
GATE_REF = {"op": "subgraph", "key": GATE}


def gate(graph):
    predicate = graph.get("predicates", {}).get(GATE)
    if predicate is None:
        return None
    condition = graph.get("prerequisite", {})
    children = condition.get("children", []) if condition.get("op") == "all_of" else []
    if (
        len(children) != 2
        or children[1] != GATE_REF
        or predicate.get("op") not in ("any_of", "all_of")
        or not predicate.get("children")
        or any(set(c) != {"op", "key"} or c["op"] != "has_flag" for c in predicate["children"])
    ):
        return None
    return predicate


def links(source):
    flags = {done_flag(g["mission"]): g["mission"] for g in source["missions"]}
    return [
        (flags[c["key"]], g["mission"])
        for g in source["missions"]
        if (p := gate(g))
        for c in p["children"]
        if c["key"] in flags
    ]


def set_link(source, parent, child, *, remove=False):
    graphs = {g["mission"]: g for g in source["missions"]}
    if parent not in graphs or child not in graphs or parent == child:
        raise ValueError("Két különböző küldetést válassz.")
    edges = links(source)
    if ((parent, child) in edges) != remove:
        raise ValueError("A kapcsolat már létezik." if not remove else "Nincs ilyen kapcsolat.")
    if not remove:
        pending, visited = [child], set()
        while pending:
            node = pending.pop()
            if node == parent:
                raise ValueError("Ez a kapcsolat körkörös előfeltételt hozna létre.")
            if node not in visited:
                visited.add(node)
                pending.extend(end for start, end in edges if start == node)
    graph = graphs[child]
    predicate = gate(graph)
    if predicate is None and GATE in graph.get("predicates", {}):
        raise ValueError("Egy egyedi feltétel már használja a kampánykapcsolat nevét.")
    flag = done_flag(parent)
    requirement = {"op": "has_flag", "key": flag}
    if remove:
        predicate["children"].remove(requirement)
        if not predicate["children"]:
            original = graph["prerequisite"]["children"][0]
            graph["prerequisite"] = original
            del graph["predicates"][GATE]
        return [graph]
    origin = graphs[parent]
    root = next(n for n in origin["nodes"] if n["id"] == origin["root"])
    if predicate is None:
        predicate = {"op": "all_of", "children": []}
        original = graph.get("prerequisite", {"op": "always"})
        graph["prerequisite"] = {"op": "all_of", "children": [original, dict(GATE_REF)]}
        graph.setdefault("predicates", {})[GATE] = predicate
    predicate["children"].append(requirement)
    flags = root.setdefault("completion_flags", [])
    if flag in flags:
        return [graph]
    flags.append(flag)
    return [origin, graph]


def empty_board(title="Új kampány"):
    return {"version": 1, "title": title, "acts": [{"name": "Első felvonás"}], "cards": {}}


def remove_mission(source, code):
    """Apply to a candidate copy so a custom story reference can veto the edit."""
    graph = next(g for g in source["missions"] if g["mission"] == code)
    affected = []
    for a, b in links(source):
        if a == code or b == code:
            set_link(source, a, b, remove=True)
            if b != code:
                affected.append(b)
    flags = {flag for n in graph["nodes"] for flag in n.get("completion_flags", [])}

    def needs_flag(value):
        if isinstance(value, list):
            return any(needs_flag(child) for child in value)
        if isinstance(value, dict):
            if value.get("op") in ("has_flag", "not_flag") and value.get("key") in flags:
                return True
            return any(needs_flag(child) for child in value.values())
        return False

    if any(needs_flag(other) for other in source["missions"] if other is not graph):
        raise ValueError(
            "Másik küldetés egyedi feltétele használja ennek a küldetésnek a történeti jelzőjét."
        )
    source["missions"].remove(graph)
    bindings = source["presentation"]["missions"]
    removed = bindings.pop(code)

    def text_keys(binding):
        return {binding["title"], binding["summary"], *(obj["title"] for obj in binding["objectives"])}

    used = {key for binding in bindings.values() for key in text_keys(binding)}
    for texts in source["presentation"]["locales"].values():
        for key in text_keys(removed) - used:
            texts.pop(key, None)
    return affected


def check_board(board, source):
    ids = {g["mission"] for g in source["missions"]}
    if (
        not isinstance(board, dict)
        or board.get("version") != 1
        or not isinstance(board.get("title"), str)
        or not isinstance(board.get("acts"), list)
        or not 1 <= len(board["acts"]) <= 128
        or not isinstance(board.get("cards"), dict)
        or board["cards"].keys() - ids
    ):
        raise ValueError("A kampányterv szerkezete érvénytelen.")
    snippets=board.get("texts",[])
    if not isinstance(snippets,list) or any(
            not isinstance(row,dict) or not all(isinstance(row.get(k),str) and row[k].strip() for k in ("name","speaker","line"))
            for row in snippets):
        raise ValueError("Hibás szöveg a kampány könyvtárában.")
    for act in board["acts"]:
        if not isinstance(act, dict) or not isinstance(act.get("name"), str):
            raise TypeError("A felvonáshoz szöveges név szükséges.")
        for section in ("cast", "scenes"):
            rows = act.get(section, [])
            if not isinstance(rows, list) or any(
                not isinstance(row, dict)
                or not all(isinstance(row.get(key, ""), str) for key in ("name", "role", "art"))
                for row in rows
            ):
                raise ValueError("Hibás szereplő vagy helyszín.")
    for card in board["cards"].values():
        if (
            not isinstance(card, dict)
            or card.get("kind") not in KINDS
            or type(card.get("act")) is not int
            or not 0 <= card["act"] < len(board["acts"])
            or not all(
                type(card.get(k)) in (int, float) and isfinite(card[k]) and 0 <= card[k] <= 100000
                for k in ("x", "y")
            )
            or not all(isinstance(card.get(k, ""), str) for k in ("speaker", "line", "scene"))
        ):
            raise ValueError("Érvénytelen kártya a kampánytervben.")
        positions = card.get("node_positions", {})
        if not isinstance(positions, dict) or not all(
            isinstance(key, str) and isinstance(pair, (list, tuple)) and len(pair) == 2 and
            all(type(v) in (int, float) and -100000 <= v <= 100000 for v in pair)
            for key, pair in positions.items()
        ):
            raise ValueError("Érvénytelen csomópontpozíció a kampánytervben.")
    return board


def place_missing(board, source, act=0):
    for graph in source["missions"]:
        if graph["mission"] not in board["cards"]:
            count = sum(c["act"] == act for c in board["cards"].values())
            board["cards"][graph["mission"]] = {
                "act": act,
                "kind": "main",
                "x": 34 + (count % 4) * 211,
                "y": 36 + (count // 4) * 88,
            }


def crown_example():
    source = new_package()
    board = empty_board("Az elveszett korona")
    board["acts"] = [
        {"name": name}
        for name in (
            "Az első nyomok",
            "A három szövetség",
            "A megtört pecsétek",
            "Az árulás ára",
            "A korona örököse",
        )
    ]
    act = board["acts"][0]
    act["cast"] = [
        {"name": n, "role": r, "art": a}
        for n, r, a in (
            ("Káldor", "Királyi tanácsadó", "kaldor"),
            ("Elara", "Határőr kapitány", "elara"),
            ("Brun", "Kovácsmester", "brun"),
            ("Morvan", "Ősi krónikák őre", "morvan"),
            ("Titokzatos idegen", "Ismeretlen szövetséges", "stranger"),
        )
    ]
    act["scenes"] = [
        {"name": n, "art": a}
        for n, a in (
            ("A királyi főváros", "citadel"),
            ("Határőr erőd", "outpost"),
            ("Ősi romok", "ruins"),
            ("Elhagyatott barlang", "cavern"),
        )
    ]
    quests = [
        (
            "A tanács hívása",
            "Keresd fel Káldort a királyi tanácsban.",
            "main",
            0,
            0,
            "Káldor",
            "A határvidéken furcsa jeleket jelentettek. Azt gyanítjuk, hogy kapcsolatban állnak az elveszett koronával.",
        ),
        (
            "A határvidék veszélyei",
            "Keresd meg Elarát a határőr erődben.",
            "main",
            1,
            0,
            "Elara",
            "A közelmúltban a farkasok szokatlanul agresszívek lettek. Valami felzaklatta őket.",
        ),
        (
            "Egy régi lelet",
            "Mutasd meg a töredéket Brunnak.",
            "main",
            2,
            0,
            "Brun",
            "Ezt a fémtöredéket egy halott farkas nyakában találtuk.",
        ),
        (
            "Az ősi írások",
            "Fejtsd meg a töredékeket Morvannal.",
            "main",
            3,
            0,
            "Morvan",
            "Ezek a szimbólumok a Királyok Korából származnak.",
        ),
        ("Eltűnt kereskedő", "Kövesd a kereskedő nyomát a határvidéken.", "side", 1, 1, "", ""),
        ("Régi kovácsnyomok", "Vizsgáld meg a régi kovács jelét.", "side", 2, 1, "", ""),
        ("A titkos napló", "Találd meg a nyomokat összekötő naplót.", "secret", 3, 2, "", ""),
    ]
    codes = []
    for title, description, kind, col, row, speaker, line in quests:
        graph = new_mission(source, title, description)
        codes.append(graph["mission"])
        board["cards"][graph["mission"]] = {
            "act": 0,
            "kind": kind,
            "x": 35 + col * 218,
            "y": 31 + row * 92,
            "speaker": speaker,
            "line": line,
        }
    for parent, child in ((0, 1), (1, 2), (2, 3), (0, 4), (4, 5), (4, 6), (5, 6)):
        set_link(source, codes[parent], codes[child])
    return source, board
