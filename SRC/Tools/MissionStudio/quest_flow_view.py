"""Tk canvas editor for the steps and branches of a single Mission Studio quest."""

import tkinter as tk
from copy import deepcopy
from tkinter import messagebox, simpledialog, ttk

from asset_shelf import STEPS, AssetShelf, PointerDrag
from campaign_view import button
from mission_source import EVENTS
from quest_assets import (
    Asset,
    apply_drop,
    change_stack,
    drop_action,
    reorder_answer,
    reward_subject,
)
from quest_flow import (
    TYPES,
    connect,
    drop_node,
    import_flow,
    materialize,
    ports,
    title,
    validate,
)
from target_names import display, folded

BG, PANEL, CARD, GRID = "#101820", "#17222c", "#20303e", "#1d2b36"
TEXT, MUTED, GOLD = "#eaf1f3", "#a4b6c3", "#e5bf7d"
COLORS = {
    "start": "#6ece95",
    "dialogue": "#7cc3ea",
    "text": "#a5ceda",
    "task": "#c2a0e6",
    "condition": "#edbb72",
    "reward": "#84cfac",
    "timer": "#dfaa85",
    "end": "#79d1c2",
    "native": "#aebbc5",
}
SYMBOL = {
    "start": "▶",
    "dialogue": "◇",
    "text": "▤",
    "task": "◆",
    "condition": "⑂",
    "reward": "✦",
    "timer": "◷",
    "end": "⚑",
    "native": "▣",
}
EDITABLE = STEPS
W = 174


def label(parent, value, size=10, color=TEXT, **opts):
    return tk.Label(
        parent,
        text=value,
        fg=color,
        bg=parent["bg"],
        font=("Segoe UI", size),
        anchor="w",
        justify="left",
        **opts,
    )


def clear(parent):
    for child in parent.winfo_children():
        child.destroy()


def outlet(p):
    return {"next": "Tovább", "yes": "Igen", "no": "Nem"}.get(p, p)


def details(n, flow):
    kind = n["kind"]
    if kind == "task":
        return f"{EVENTS[n['event']][0]}\n{display(n['subject'])} × {n['goal']}"
    if kind == "dialogue":
        return f"{n['speaker']} · {len(n['answers'])} válasz"
    if kind == "text":
        return n["speaker"] + " · " + n["line"]
    if kind == "reward":
        b = n["bundle"]
        lines = [f"{b.get('experience', 0):,} TP  ·  {b.get('gold', 0):,} Yang".replace(",", " ")]
        lines.extend(f"{display(reward_subject(r))} × {r['count']}" for r in b.get("typed_items", []))
        return "\n".join(lines)
    if kind == "condition":
        task = flow["nodes"].get(n.get("test"))
        return "Teljesült? " + (title(task) if task else "Válassz feladatot")
    if kind == "timer":
        return f"{n['duration'] / 1000:g} másodperc"
    if kind == "native":
        return n["original"]["kind"]
    return "Küldetés felvétele" if kind == "start" else "Küldetés teljesítve"


class NodeDialog(simpledialog.Dialog):
    def __init__(self, view, key):
        self.view, self.key = view, key
        self.n = deepcopy(view.flow["nodes"][key])
        self.fields = {}
        super().__init__(view, TYPES[self.n["kind"]] + " szerkesztése")

    def field(self, frame, name, caption, value, lines=0):
        ttk.Label(frame, text=caption).pack(anchor="w", pady=(6, 2))
        if lines:
            widget = tk.Text(frame, width=55, height=lines, wrap="word", font=("Segoe UI", 10))
            widget.insert("1.0", value)
            self.fields[name] = lambda: widget.get("1.0", "end-1c").strip()
        else:
            widget = ttk.Entry(frame, width=55)
            widget.insert(0, str(value))
            self.fields[name] = lambda: widget.get().strip()
        widget.pack(fill="x")
        return widget

    def body(self, frame):
        n = self.n
        first = self.field(frame, "name", "Csomópont neve", title(n))
        if n["kind"] in ("text", "dialogue"):
            self.field(frame, "speaker", "Beszélő", n["speaker"])
            self.field(frame, "line", "Szöveg", n["line"], 3)
        if n["kind"] == "dialogue":
            self.field(
                frame, "answers", "Válaszok sorrendben (soronként egy, A–H)", "\n".join(n["answers"]), 5
            )
        elif n["kind"] == "condition":
            self.tasks = [k for k, v in self.view.flow["nodes"].items() if v["kind"] == "task"]
            ttk.Label(frame, text="Melyik feladat teljesült?").pack(anchor="w", pady=(6, 2))
            self.test = ttk.Combobox(
                frame, values=[title(self.view.flow["nodes"][k]) for k in self.tasks], state="readonly"
            )
            self.test.pack(fill="x")
            if self.tasks:
                self.test.current(self.tasks.index(n["test"]) if n.get("test") in self.tasks else 0)
        elif n["kind"] == "timer":
            self.field(frame, "duration", "Várakozás (ezredmásodperc)", n["duration"])
        elif n["kind"] == "reward":
            for key, caption in (("experience", "Tapasztalati pont"), ("gold", "Yang")):
                self.field(frame, key, caption, n["bundle"].get(key, 0))
        return first

    def validate(self):
        try:
            data = {k: get() for k, get in self.fields.items()}
            if not data["name"]:
                raise ValueError("Adj nevet a csomópontnak.")
            n = deepcopy(self.n)
            n["name"] = data["name"]
            if n["kind"] in ("text", "dialogue"):
                if n["speaker"] != data["speaker"]:
                    n.pop("speaker_target", None)
                    n.pop("portrait", None)
                n.update(speaker=data["speaker"], line=data["line"])
            if n["kind"] == "dialogue":
                n["answers"] = data["answers"].splitlines()
                if any(p not in ports(n) for p in self.view.flow["wires"].get(self.key, {})):
                    raise ValueError("A törlendő válasz vezetékét előbb bontsd a gráfon.")
            elif n["kind"] == "timer":
                n["duration"] = int(data["duration"])
            elif n["kind"] == "condition":
                if self.test.current() < 0:
                    raise ValueError("Előbb hozz létre egy feladatot a gráfon.")
                n["test"] = self.tasks[self.test.current()]
            elif n["kind"] == "reward":
                for key in ("experience", "gold"):
                    n["bundle"][key] = int(data[key])
                    if not 0 <= n["bundle"][key] <= 2**63 - 1:
                        raise ValueError("A jutalom összege kívül esik a megengedett tartományon.")
            candidate = {**self.view.flow, "nodes": {**self.view.flow["nodes"], self.key: n}}
            validate(candidate)
            self.result = n
            return True
        except (ValueError, KeyError) as error:
            messagebox.showerror("Csomópont", str(error), parent=self)
            return False


class QuestFlowView(tk.Frame):
    def __init__(self, parent, studio):
        super().__init__(parent, bg=BG)
        self.studio = studio
        self.flow, self.code, self.selected = None, None, None
        self.zoom, self.dx, self.dy = 1, 20, 40
        self.gesture = self.edge = self.playing = None
        self.completed = set()
        self.columnconfigure(1, weight=1)
        self.rowconfigure(0, weight=1)
        sidebar = studio.dock
        self.dragger = PointerDrag(
            studio.root, self.route_drop, lambda s: studio.status.set(s), self.clear_drop_hint
        )
        label(sidebar, "Küldetések", 12, GOLD).pack(fill="x", padx=10, pady=(13, 6))
        self.query = tk.StringVar()
        label(sidebar, "Keresés név szerint", 9, MUTED).pack(anchor="w", padx=10)
        tk.Entry(
            sidebar,
            textvariable=self.query,
            bg=BG,
            fg=TEXT,
            insertbackground=TEXT,
            relief="flat",
            highlightthickness=1,
            highlightbackground="#3a505f",
        ).pack(fill="x", padx=8, pady=(1, 0))
        self.query.trace_add("write", lambda *_: self.tree_refresh())
        self.tree = ttk.Treeview(sidebar, show="tree", height=7, selectmode="browse", style="Quest.Treeview")
        self.tree.column("#0", width=166, minwidth=50)
        self.tree.pack(fill="both", padx=5, pady=6)

        def mission_asset(e):
            code = self.tree.identify_row(e.y)
            if code and not code.startswith("act:"):
                return Asset("mission", self.tree.item(code, "text"), subject=code)
            return None

        self.dragger.bind(self.tree, mission_asset)
        self.tree.bind("<ButtonRelease-1>", self.pick_mission, add=True)
        self.tree.bind("<KeyRelease>", self.pick_mission, add=True)
        button(sidebar, "+ Küldetés", studio.add_mission).pack(anchor="w", padx=8)
        self.shelf = AssetShelf(sidebar, self)
        self.shelf.pack(fill="both", expand=True, padx=8, pady=6)
        middle = tk.Frame(self, bg=BG)
        middle.grid(row=0, column=1, sticky="nsew")
        bar = tk.Frame(middle, bg=PANEL, pady=3)
        bar.pack(fill="x")
        self.head = label(bar, "Küldetésgráf", 12, GOLD)
        self.head.pack(side="left", padx=10)
        for text, cmd in (
            ("Illesztés", self.fit),
            ("+", lambda: self.scale(1.2)),
            ("−", lambda: self.scale(1 / 1.2)),
        ):
            button(bar, text, cmd).pack(side="right")
        self.pct = label(bar, "100%", 9, MUTED)
        self.pct.pack(side="right", padx=3)
        self.canvas = tk.Canvas(middle, bg=BG, highlightthickness=0, takefocus=True)
        self.canvas.pack(fill="both", expand=True)
        for ev, handler in (
            ("<Configure>", self.resize),
            ("<ButtonPress-1>", self.press),
            ("<B1-Motion>", self.motion),
            ("<ButtonRelease-1>", self.release),
            ("<Double-Button-1>", self.double_click),
            ("<Button-3>", self.context),
            ("<MouseWheel>", lambda e: self.scale(1.15 if e.delta > 0 else 1 / 1.15, e.x, e.y)),
            ("<Delete>", lambda _: self.remove()),
            ("<Escape>", lambda _: self.cancel()),
        ):
            self.canvas.bind(ev, handler)
        self.inspector = tk.Frame(self, bg=PANEL, width=204)
        self.inspector.grid(row=0, column=2, sticky="nsew", padx=(2, 0))
        self.inspector.pack_propagate(False)
        bottom = tk.Frame(self, bg=GRID, height=222)
        bottom.grid(row=1, column=1, columnspan=2, sticky="ew", pady=(2, 0))
        bottom.pack_propagate(False)
        self.script = tk.Frame(bottom, bg=PANEL, padx=13, pady=8)
        self.script.pack(side="left", fill="both", expand=True)
        self.preview = self.scroll_panel(bottom, "#1c272b")
        self.tree_busy = False
        self.auto_fit = True

    def scroll_panel(self, parent, color):
        shell = tk.Frame(parent, bg=color)
        shell.pack(side="left", fill="both", expand=True, padx=(2, 0))
        canvas = tk.Canvas(shell, bg=color, width=310, highlightthickness=0)
        canvas.pack(side="left", fill="both", expand=True)
        scroll = ttk.Scrollbar(shell, command=canvas.yview)
        scroll.pack(side="right", fill="y")
        canvas.configure(yscrollcommand=scroll.set)
        body = tk.Frame(canvas, bg=color, padx=13, pady=8)
        window = canvas.create_window(0, 0, anchor="nw", window=body)
        canvas.bind("<Configure>", lambda e: canvas.itemconfigure(window, width=e.width))
        body.bind("<Configure>", lambda _: canvas.configure(scrollregion=canvas.bbox("all")))
        return body

    def tree_refresh(self):
        if not hasattr(self.studio, "board"):
            return
        self.tree_busy = True
        self.tree.delete(*self.tree.get_children())
        query = folded(self.query.get().strip())
        if not query:
            for i, act in enumerate(self.studio.board["acts"]):
                self.tree.insert("", "end", iid=f"act:{i}", text=act["name"], open=True)
        for g in self.studio.source["missions"]:
            name = self.mission_name(g)
            if query not in folded(name):
                continue
            code = g["mission"]
            act = self.studio.board["cards"].get(code, {}).get("act", 0)
            parent = "act:" + str(act)
            if not self.tree.exists(parent):
                self.tree.insert(
                    "", "end", iid=parent, text=self.studio.board["acts"][act]["name"], open=True
                )
            self.tree.insert(parent, "end", iid=code, text=name)
        if self.code and self.tree.exists(self.code):
            self.tree.selection_set(self.code)
            self.tree.see(self.code)
        self.tree_busy = False

    def mission_name(self, g):
        b, t = self.studio.presentation(g)
        return t[b["title"]]

    def pick_mission(self, _):
        sel = self.tree.selection()
        if not self.tree_busy and sel and not sel[0].startswith("act:") and sel[0] != self.code:
            self.studio.open_details(sel[0])

    def refresh(self, reset=False):
        g = self.studio.current()
        code = g["mission"] if g else None
        switched = reset or self.code != code
        self.code = code
        self.flow = None
        if g:
            card = self.studio.board["cards"][code]
            self.flow = card.get("flow")
            if self.flow is None:
                self.flow = import_flow(self.studio.source, g)
                for k, (x, y) in card.get("node_positions", {}).items():
                    if k in self.flow["nodes"]:
                        self.flow["nodes"][k].update(x=x, y=y)
        if switched:
            self.gesture = self.edge = self.playing = None
            self.selected = (
                next(
                    (k for k, v in (self.flow or {}).get("nodes", {}).items() if v["kind"] == "dialogue"),
                    "start",
                )
                if g
                else None
            )
            self.auto_fit = True
        if self.flow and self.selected not in self.flow["nodes"]:
            self.selected = "start"
        self.head.config(text=self.mission_name(g) if g else "Küldetésgráf")
        self.tree_refresh()
        if self.shelf.group.get() == "Szövegek":
            self.shelf.refresh()
        self.fit() if self.auto_fit else self.draw()
        self.panels()

    def commit(self, layout=False):
        card = self.studio.board["cards"][self.code]
        if layout and "flow" not in card:
            card["node_positions"] = {k: [n["x"], n["y"]] for k, n in self.flow["nodes"].items()}
        else:
            card["flow"] = self.flow
            card.pop("node_positions", None)
        self.playing = None
        if layout:
            self.studio.save_layout()
        else:
            self.studio.changed(self.studio.current())

    def xy(self, x, y):
        return self.dx + x * self.zoom, self.dy + y * self.zoom

    def world(self, x, y):
        return (x - self.dx) / self.zoom, (y - self.dy) / self.zoom

    def height(self, n):
        return max(75, 59 + len(ports(n)) * 16) if n["kind"] not in ("start", "end") else 56

    def width(self, n):
        return 121 if n["kind"] in ("start", "end") else W

    def socket(self, k, port=None):
        n = self.flow["nodes"][k]
        if port is None:
            return self.xy(n["x"], n["y"] + 26)
        outputs = ports(n)
        oy = 26 if len(outputs) == 1 else 59 + outputs.index(port) * 16
        return self.xy(n["x"] + self.width(n), n["y"] + oy)

    def resize(self, _=None):
        self.fit() if self.auto_fit else self.draw()

    def fit(self):
        c = self.canvas
        self.auto_fit = True
        if self.flow and c.winfo_width() > 10:
            ns = list(self.flow["nodes"].values())
            x, y = min(n["x"] for n in ns), min(n["y"] for n in ns)
            w = max(n["x"] + self.width(n) for n in ns) - x
            h = max(n["y"] + self.height(n) for n in ns) - y
            self.zoom = max(0.18, min(1.25, (c.winfo_width() - 44) / w, (c.winfo_height() - 50) / h))
            self.dx = (c.winfo_width() - w * self.zoom) / 2 - x * self.zoom
            self.dy = (c.winfo_height() - h * self.zoom) / 2 - y * self.zoom
        self.draw()

    def scale(self, factor, x=None, y=None):
        if x is None:
            x, y = self.canvas.winfo_width() / 2, self.canvas.winfo_height() / 2
        a, b = self.world(x, y)
        self.zoom = max(0.18, min(2.6, self.zoom * factor))
        self.dx, self.dy = x - a * self.zoom, y - b * self.zoom
        self.auto_fit = False
        self.draw()

    def wire(self, x, y, ex, ey, color, width, tags=()):
        bend = max(abs(ex - x) * 0.48, 28 * self.zoom)
        return self.canvas.create_line(
            x,
            y,
            x + bend,
            y,
            ex - bend,
            ey,
            ex,
            ey,
            smooth="raw",
            splinesteps=36,
            fill=color,
            width=width,
            tags=tags,
        )

    def card_text(self, x, y, value, color, size, bold=False):
        # Pixel fonts let text and geometry scale together at any display DPI.
        return self.canvas.create_text(
            x,
            y,
            text=value,
            fill=color,
            anchor="nw",
            font=("Segoe UI", -max(5, round(size * self.zoom)), "bold" if bold else "normal"),
        )

    def draw(self):
        c = self.canvas
        c.delete("all")
        self.hits = {}
        self.pct.config(text=f"{self.zoom:.0%}")
        step = max(12, int(26 * self.zoom))
        width, height = c.winfo_width(), c.winfo_height()
        for x in range(int(self.dx) % step, width, step):
            c.create_line(x, 0, x, height, fill=GRID)
        for y in range(int(self.dy) % step, height, step):
            c.create_line(0, y, width, y, fill=GRID)
        if not self.flow:
            c.create_text(width / 2, height / 2, text="Hozz létre egy küldetést a bal oldalon.", fill=MUTED)
            return
        ns = self.flow["nodes"]
        for k, edges in self.flow["wires"].items():
            for p, d in edges.items():
                selected = self.edge == (k, p)
                i = self.wire(
                    *self.socket(k, p),
                    *self.socket(d),
                    TEXT if selected else COLORS[ns[k]["kind"]],
                    4 if selected else 2,
                )
                self.hits[i] = ("edge", k, p)
        for k, n in ns.items():
            x, y = self.xy(n["x"], n["y"])
            z = self.zoom
            w, h = self.width(n) * z, self.height(n) * z
            color = COLORS[n["kind"]]
            chosen = k == self.selected
            ids = [
                c.create_rectangle(
                    x,
                    y,
                    x + w,
                    y + h,
                    fill=CARD if chosen else PANEL,
                    outline=color if chosen else "#48606e",
                    width=2 if chosen else 1,
                ),
                c.create_rectangle(x, y, x + 3 * z, y + h, fill=color, outline=""),
            ]

            ids.append(
                self.card_text(
                    x + 9 * z, y + 5 * z, SYMBOL[n["kind"]] + "  " + TYPES[n["kind"]], color, 13, True
                )
            )
            name = title(n) if n["kind"] not in ("start", "end") else details(n, self.flow)
            ids.append(
                self.card_text(
                    x + 9 * z, y + 25 * z, name[:24] + ("…" if len(name) > 24 else ""), TEXT, 13, True
                )
            )
            if n["kind"] not in ("start", "end"):
                info = details(n, self.flow).split("\n")[-1]
                ids.append(
                    self.card_text(
                        x + 9 * z, y + 44 * z, info[:26] + ("…" if len(info) > 26 else ""), MUTED, 11
                    )
                )
            for i in ids:
                self.hits[i] = ("node", k)
            for p in ([None] if k != "start" else []) + ports(n):
                sx, sy = self.socket(k, p)
                if p is not None and len(ports(n)) > 1:
                    ab = c.create_text(
                        sx - 10 * z,
                        sy,
                        text=outlet(p),
                        fill=color,
                        anchor="e",
                        font=("Segoe UI", -max(6, int(11 * z)), "bold"),
                    )
                    self.hits[ab] = ("out", k, p)
                r = max(3, 4.3 * z)
                dot = c.create_oval(
                    sx - r, sy - r, sx + r, sy + r, fill=color if p else BG, outline=color, width=2
                )
                self.hits[dot] = ("out", k, p) if p else ("in", k)

    def hit(self, x, y):
        for i in reversed(self.canvas.find_overlapping(x - 4, y - 4, x + 4, y + 4)):
            if i in self.hits:
                return self.hits[i]
        return ()

    def press(self, e):
        self.canvas.focus_set()
        hit = self.hit(e.x, e.y)
        self.edge = None
        if not hit:
            self.gesture = ("pan", e.x, e.y, self.dx, self.dy)
        elif hit[0] == "edge":
            self.edge = hit[1:]
            if not self.flow.get("readonly"):
                self.gesture = ("edge", *self.edge)
            self.draw()
        else:
            _, k, *p = hit
            self.selected = k
            n = self.flow["nodes"][k]
            if not self.flow.get("readonly"):
                self.gesture = ("wire", k, p[0]) if hit[0] == "out" else ("move", k, e.x, e.y, n["x"], n["y"])
            self.draw()
            self.panels()

    def motion(self, e):
        if not self.gesture:
            return
        mode, *g = self.gesture
        if mode in ("move", "wire", "edge") and self.local_point(self.shelf.bin, e.x_root, e.y_root):
            self.studio.status.set(
                "A kezdés és a befejezés rögzített."
                if mode == "move" and g[0] in ("start", "end")
                else "Az egér elengedésével eltávolítod a kijelölt elemet."
            )
            return
        if mode == "edge":
            return
        if mode == "wire":
            self.canvas.delete("loose")
            self.wire(*self.socket(*g), e.x, e.y, GOLD, 3, "loose")
        elif mode == "pan":
            x, y, dx, dy = g
            self.dx, self.dy = dx + e.x - x, dy + e.y - y
            self.auto_fit = False
            self.draw()
        else:
            k, x, y, ox, oy = g
            n = self.flow["nodes"][k]
            n["x"] = max(-100000, min(100000, ox + (e.x - x) / self.zoom))
            n["y"] = max(-100000, min(100000, oy + (e.y - y) / self.zoom))
            self.auto_fit = False
            self.draw()

    def release(self, e):
        if not self.gesture:
            return
        mode, *g = self.gesture
        self.gesture = None
        if mode in ("move", "wire", "edge") and not self.local_point(self.canvas, e.x_root, e.y_root):
            if mode == "move":
                self.flow["nodes"][g[0]].update(x=g[3], y=g[4])
            if self.local_point(self.shelf.bin, e.x_root, e.y_root):
                if mode == "move" and g[0] not in ("start", "end"):
                    self.remove()
                elif mode in ("wire", "edge") and g[1] in self.flow["wires"].get(g[0], {}):
                    self.edge = tuple(g)
                    self.remove()
            self.draw()
            return
        if mode == "wire":
            hit = self.hit(e.x, e.y)
            if hit and hit[0] == "in":
                try:
                    connect(self.flow, *g, hit[1])
                    self.commit()
                except ValueError as error:
                    self.studio.status.set(str(error))
            self.draw()
        elif mode == "move" and (self.flow["nodes"][g[0]]["x"], self.flow["nodes"][g[0]]["y"]) != (
            g[3],
            g[4],
        ):
            self.commit(layout=True)

    def cancel(self):
        if self.gesture and self.gesture[0] == "move":
            _, k, _, _, x, y = self.gesture
            self.flow["nodes"][k].update(x=x, y=y)
        self.gesture = None
        self.draw()

    def double_click(self, e):
        hit = self.hit(e.x, e.y)
        if hit and hit[0] == "node":
            self.gesture = None
            self.edit(hit[1])

    def context(self, e):
        if not self.flow or self.flow.get("readonly"):
            return
        menu = tk.Menu(self, tearoff=False)
        hit = self.hit(e.x, e.y)
        if hit and hit[0] == "edge":
            self.edge = hit[1:]
            menu.add_command(label="Vezeték bontása", command=self.remove)
        else:
            pos = self.world(e.x, e.y)
            for kind in EDITABLE:
                menu.add_command(label=TYPES[kind], command=lambda k=kind: self.add(k, pos))
        menu.tk_popup(e.x_root, e.y_root)

    @staticmethod
    def local_point(widget, x, y):
        if not widget or not widget.winfo_exists() or not widget.winfo_ismapped():
            return None
        px, py = x - widget.winfo_rootx(), y - widget.winfo_rooty()
        return (px, py) if 0 <= px < widget.winfo_width() and 0 <= py < widget.winfo_height() else None

    def clear_drop_hint(self):
        self.canvas.delete("drop_hint")
        self.studio.status.set("")

    def route_drop(self, asset, x, y, apply):
        self.canvas.delete("drop_hint")
        try:
            description = self.resolve_drop(asset, x, y, apply)
            if not apply:
                self.studio.status.set(description)
        except (ValueError, KeyError) as error:
            self.studio.status.set(str(error))

    def resolve_drop(self, asset, x, y, apply):
        app = self.studio
        if self.local_point(self.shelf.bin, x, y) and asset.kind == "stack" and asset.mission == self.code:
            if apply:
                candidate = deepcopy(self.flow)
                change_stack(candidate["nodes"][asset.origin]["bundle"], asset.subject, -asset.count)
                self.flow = candidate
                self.commit()
            return "Kivétel a jutalomból · " + asset.name
        code = self.code
        target = edge = pos = None
        tab = app.tabs.select()
        tree_point = self.local_point(self.tree, x, y)
        graph_point = self.local_point(self.canvas, x, y)
        if asset.kind == "mission":
            act = None
            if tree_point:
                row = self.tree.identify_row(tree_point[1])
                if row.startswith("act:"):
                    act = int(row[4:])
            elif pt := self.local_point(app.campaign.canvas, x, y):
                act = app.campaign.act
                c = app.campaign.canvas
                px, py = c.canvasx(pt[0]), c.canvasy(pt[1])
                hit, other = app.campaign.hit(px, py)
                if hit in ("node", "out", "in") and other != asset.subject:
                    if apply:
                        app.campaign.change_link(asset.subject, other)
                    return "Küldetések összekötése"
                pos = (max(0, min(100000, round(px))), max(0, min(100000, round(py))))
            if act is None:
                raise ValueError("Húzd a küldetést egy felvonásra vagy a kampánygráfra.")
            if apply:
                card = app.board["cards"][asset.subject]
                card["act"] = act
                if pos:
                    card.update(x=pos[0], y=pos[1])
                app.campaign.act = act
                app.campaign.selected = asset.subject
                app.select_mission(asset.subject)
                app.campaign.refresh()
                self.refresh()
                app.save_layout()
            return "Küldetés elhelyezése · " + app.board["acts"][act]["name"]
        if tree_point:
            code = self.tree.identify_row(tree_point[1])
            if not code or code.startswith("act:"):
                raise ValueError("Egy küldetés nevére húzd az elemet.")
            target = "start" if asset.kind == "npc" else "end"
        elif tab == str(self) and graph_point:
            px, py = graph_point
            pos = self.world(px, py)
            hit = self.hit(px, py)
            if hit:
                if hit[0] in ("edge", "out") and asset.kind != "answer":
                    edge = tuple(hit[1:])
                else:
                    target = hit[1] if hit[0] != "edge" else None
        elif tab == str(self) and (
            self.local_point(self.inspector, x, y)
            or self.local_point(self.script, x, y)
            or self.local_point(self.preview, x, y)
        ):
            target = "start" if self.local_point(getattr(self, "giver_button", None), x, y) else self.selected
            table = getattr(self, "answers_table", None)
            row_point = self.local_point(table, x, y)
            if asset.kind == "answer" and row_point and asset.mission == code and asset.origin == target:
                p = table.identify_row(row_point[1])
                if not p:
                    raise ValueError("A célhelyhez húzd egy válasz sorára.")
                if apply and p != asset.subject:
                    reorder_answer(self.flow, target, asset.subject, p)
                    self.commit()
                return "Válasz és hozzá tartozó ág átrendezése"
        elif tab == str(app.campaign) and (pt := self.local_point(app.campaign.canvas, x, y)):
            c = app.campaign.canvas
            kind, code = app.campaign.hit(c.canvasx(pt[0]), c.canvasy(pt[1]))
            if kind not in ("in", "out", "node"):
                raise ValueError("A kampányban egy küldetéskártyára húzd az elemet.")
            target = "start" if asset.kind == "npc" else "end"
        elif tab == str(app.editor) and (pt := self.local_point(app.steps, x, y)):
            target = app.steps.identify_row(pt[1])
            if not target:
                raise ValueError("Válassz egy lépést a listában.")
        elif tab == str(app.editor) and (pt := self.local_point(app.missions, x, y)):
            i = app.missions.nearest(pt[1])
            bbox = app.missions.bbox(i) if i >= 0 else None
            if not bbox or pt[1] >= bbox[1] + bbox[3] + 1:
                raise ValueError("Egy küldetés sorára húzd az elemet.")
            code = app.source["missions"][i]["mission"]
            target = "start" if asset.kind == "npc" else "end"
        else:
            raise ValueError("Húzd az elemet a gráfra vagy egy küldetésre.")
        if not code:
            raise ValueError("Előbb válassz küldetést.")
        if asset.kind in ("scene", "cast") or (
            asset.kind == "phrase" and tab == str(app.campaign) and not tree_point
        ):
            if tab != str(app.campaign):
                raise ValueError("Húzd a kampányban a célküldetésre.")
            if apply:
                card = app.board["cards"][code]
                if asset.kind == "scene":
                    card["scene"] = asset.name
                elif asset.kind == "cast":
                    card["speaker"] = asset.name
                else:
                    card.update(line=asset.line, speaker=asset.speaker or "")
                app.campaign.selected = code
                app.campaign.refresh()
                app.save_layout()
            return "Hozzárendelés a kampánykártyához · " + asset.name
        hint = self.drop_on(code, asset, target, edge, pos, apply)
        if not apply and graph_point and self.flow:
            if target in self.flow["nodes"]:
                n = self.flow["nodes"][target]
                self.canvas.create_rectangle(
                    *self.xy(n["x"] - 3, n["y"] - 3),
                    *self.xy(n["x"] + self.width(n) + 3, n["y"] + self.height(n) + 3),
                    outline=GOLD,
                    width=3,
                    tags="drop_hint",
                )
            elif edge:
                a, p = edge
                other = self.flow["wires"].get(a, {}).get(p)
                if other:
                    self.wire(*self.socket(a, p), *self.socket(other), GOLD, 4, "drop_hint")
        return hint

    def drop_on(self, code, asset, target=None, edge=None, pos=None, apply=True):
        app = self.studio
        graph = next(g for g in app.source["missions"] if g["mission"] == code)
        card = app.board["cards"][code]
        flow = self.flow if self.code == code else card.get("flow")
        if flow is None:
            flow = import_flow(app.source, graph)
            for key, (x, y) in card.get("node_positions", {}).items():
                if key in flow["nodes"]:
                    flow["nodes"][key].update(x=x, y=y)
        if flow.get("readonly"):
            raise ValueError("Ez az importált gráf csak olvasható.")
        if target == graph["root"] and target not in flow["nodes"]:
            target = "start" if asset.kind == "npc" else "end"
        if target is not None and target not in flow["nodes"]:
            raise ValueError("Ez a lépés nem szerkeszthető a gráfon.")
        if asset.mission and asset.mission != code:
            raise ValueError("Ezt az elemet ugyanazon küldetésen belül húzhatod át.")
        kind = flow["nodes"].get(target, {}).get("kind")
        hint = drop_action(asset, kind, edge)
        if not hint:
            raise ValueError("Ez az elem itt nem helyezhető el.")
        if (
            asset.kind == "item"
            and kind == "reward"
            and flow["nodes"][target]["bundle"].get("schema", 1) == 1
        ):
            raise ValueError("Ez a jutalom régi formátumú.")
        if not apply:
            return hint + " · " + asset.name
        if asset.kind == "npc" and target == "start" and edge is None:
            old = app.rules.get(code, [0, 1])
            app.rules[code] = [int(asset.subject.split(".")[1]), old[1]]
            selected = "start"
        else:
            if pos is None:
                anchor = flow["nodes"][target or "end"]
                pos = anchor["x"] - 92, anchor["y"] + 107
            if target or edge:
                predecessor = (
                    edge[0]
                    if edge
                    else next((k for k, outputs in flow["wires"].items() if target in outputs.values()), None)
                )
                following = flow["wires"].get(edge[0], {}).get(edge[1]) if edge else target
                if predecessor and following:
                    left, right = flow["nodes"][predecessor], flow["nodes"][following]
                    pos = ((left["x"] + self.width(left) + right["x"] - W) / 2, (left["y"] + right["y"]) / 2)
                pos = self.free_position(flow, pos)
            pos = tuple(max(-100000, min(100000, p)) for p in pos)
            candidate = deepcopy(flow)
            selected = apply_drop(candidate, asset, pos, target, edge)
            validate(candidate)
            card["flow"] = candidate
            card.pop("node_positions", None)
        self.playing = self.gesture = self.edge = None
        app.campaign.selected = code
        app.campaign.act = card["act"]
        app.changed(graph)
        self.selected = selected
        self.draw()
        self.panels()
        return hint

    def free_position(self, flow, pos):
        x, y = pos
        obstacles = [
            (n["x"] - W - 14, n["x"] + self.width(n) + 14, n["y"] - 100, n["y"] + self.height(n) + 14)
            for n in flow["nodes"].values()
        ]
        for i in range(len(obstacles) + 2):
            for py in (y + i * 112, y - i * 112):
                if -100000 <= py <= 100000 and not any(
                    l < x < r and top < py < bottom for l, r, top, bottom in obstacles
                ):
                    return x, py
        return pos

    def add(self, kind, pos=None):
        if not self.flow or self.flow.get("readonly"):
            return
        if len(self.flow["nodes"]) >= 512:
            self.studio.status.set("Egy gráf legfeljebb 512 csomópontot tartalmazhat.")
            return
        if pos is None:
            pos = self.world(self.canvas.winfo_width() * 0.42, self.canvas.winfo_height() * 0.45)
        self.drop_on(self.code, Asset("step", TYPES[kind], kind), pos=pos)

    def remove(self):
        if not self.flow or self.flow.get("readonly"):
            return
        if self.edge:
            k, p = self.edge
            self.flow["wires"][k].pop(p, None)
            self.edge = None
        elif self.selected not in (None, "start", "end"):
            drop_node(self.flow, self.selected)
            self.selected = "start"
        else:
            return
        self.commit()

    def edit(self, k=None):
        if not self.flow or self.flow.get("readonly"):
            return
        k = k or self.selected
        n = self.flow["nodes"][k]
        if n["kind"] == "task":
            r = self.studio.edit_flow_task((title(n), n["event"], n["subject"], n["goal"]))
            if r:
                n.update(name=r[0], event=r[1], subject=r[2], goal=r[3])
                self.commit()
        elif n["kind"] in EDITABLE:
            if n["kind"] == "reward" and n["bundle"].get("schema", 1) != 3:
                self.studio.status.set("A régi sémájú jutalom csak olvasható.")
                return
            result = NodeDialog(self, k).result
            if result:
                self.flow["nodes"][k] = result
                self.commit()

    def panels(self):
        for panel in (self.inspector, self.script, self.preview):
            clear(panel)
        if not self.flow or self.selected not in self.flow["nodes"]:
            return
        n = self.flow["nodes"][self.selected]
        box = self.inspector

        def line(parent, s, sz=10, col=TEXT):
            label(parent, s, sz, col, wraplength=177).pack(anchor="w", padx=10, pady=(4, 0))

        line(box, "Tulajdonságok", 12, GOLD)
        line(box, TYPES[n["kind"]], 10, COLORS[n["kind"]])
        line(box, title(n), 13)
        line(box, details(n, self.flow), 10, MUTED)
        if n["kind"] in EDITABLE and not self.flow.get("readonly"):
            button(box, "Szerkesztés…", self.edit).pack(anchor="w", padx=9, pady=6)
        edges = self.flow["wires"].get(self.selected, {})
        if ports(n):
            line(box, "Kimenetek", 11, GOLD)
            for p in ports(n):
                dest = edges.get(p)
                line(
                    box,
                    outlet(p) + "  →  " + (title(self.flow["nodes"][dest]) if dest else "Nincs bekötve"),
                    9,
                )
        if self.flow.get("readonly"):
            line(box, "Összetett importált natív gráf · olvasási nézet", 10, GOLD)
        else:
            btn = tk.Frame(box, bg=PANEL)
            btn.pack(side="bottom", fill="x", pady=8, padx=5)
            self.giver_button = button(btn, "Küldetésadó…", self.studio.admission)
            self.giver_button.pack(anchor="w")
            button(btn, "Küldetés neve és leírása…", self.studio.edit_text).pack(anchor="w")
        label(
            self.script, "Párbeszéd szerkesztő" if n["kind"] == "dialogue" else "Kijelölt lépés", 11, GOLD
        ).pack(anchor="w")
        if n["kind"] == "dialogue":
            label(self.script, n["speaker"], 12).pack(anchor="w", pady=(3, 1))
            answer_list = ttk.Treeview(
                self.script, columns=("answer", "next"), show="headings", height=3, style="Quest.Treeview"
            )
            answer_list.heading("answer", text="Válasz")
            answer_list.heading("next", text="Következő lépés")
            answer_list.column("answer", width=205, minwidth=80)
            answer_list.column("next", width=160, minwidth=70)
            answer_list.pack(fill="both", expand=True, pady=(2, 1))
            for p, answer in zip(ports(n), n["answers"]):
                dest = edges.get(p)
                answer_list.insert(
                    "",
                    "end",
                    iid=p,
                    values=(p + "   " + answer, title(self.flow["nodes"][dest]) if dest else "Nincs bekötve"),
                )

            def follow(_):
                choice = answer_list.selection()
                if choice and choice[0] in edges:
                    self.selected = edges[choice[0]]
                    self.draw()
                    self.panels()

            answer_list.bind("<Double-1>", follow)
            self.answers_table = answer_list

            def answer_asset(e):
                port = answer_list.identify_row(e.y)
                if port:
                    return Asset(
                        "answer",
                        n["answers"][ports(n).index(port)],
                        subject=port,
                        origin=self.selected,
                        mission=self.code,
                    )
                return None

            self.dragger.bind(answer_list, answer_asset)
            button(self.script, "Szöveg és válaszok szerkesztése…", self.edit).pack(anchor="w", pady=3)
        elif n["kind"] == "reward":
            label(self.script, "Jutalomtárgyak", 12).pack(anchor="w")
            table = ttk.Treeview(
                self.script, columns=("name", "count"), show="headings", height=4, style="Quest.Treeview"
            )
            table.heading("name", text="Tárgy")
            table.heading("count", text="Darab")
            table.column("count", width=50, stretch=False)
            table.pack(fill="both", expand=True, pady=3)
            for r in n["bundle"].get("typed_items", []):
                subject = reward_subject(r)
                table.insert("", "end", iid=subject, values=(display(subject), r["count"]))

            def stack(e):
                subject = table.identify_row(e.y)
                if subject:
                    count = int(table.set(subject, "count"))
                    return Asset(
                        "stack",
                        display(subject),
                        subject,
                        count=count,
                        origin=self.selected,
                        mission=self.code,
                    )
                return None

            self.dragger.bind(table, stack)

            def quantity(e):
                a = stack(e)
                if not a or self.flow.get("readonly"):
                    return
                value = simpledialog.askinteger(
                    a.name, "Darabszám", initialvalue=a.count, minvalue=1, maxvalue=50000, parent=self
                )
                if value and value != a.count:
                    change_stack(n["bundle"], a.subject, value - a.count)
                    self.commit()

            table.bind("<Double-1>", quantity)
        elif n["kind"] == "text":
            label(self.script, n["speaker"], 12).pack(anchor="w")
            label(self.script, n["line"], 10, wraplength=350).pack(anchor="w")
            button(self.script, "Szöveg szerkesztése…", self.edit).pack(anchor="w", pady=4)
        else:
            label(self.script, title(n), 13).pack(anchor="w", pady=(7, 2))
            label(self.script, details(n, self.flow), 10, MUTED, wraplength=330).pack(anchor="w")
            if n["kind"] == "task":
                ref = label(self.script, "↗  Húzd egy feltételre", 10, COLORS["condition"], cursor="hand2")
                ref.pack(anchor="w")
                self.dragger.bind(
                    ref, lambda _: Asset("taskref", title(n), origin=self.selected, mission=self.code)
                )
        self.preview_draw()

    def start_preview(self):
        if not self.flow or self.flow.get("readonly"):
            return
        try:
            source = deepcopy(self.studio.source)
            graph = next(g for g in source["missions"] if g["mission"] == self.code)
            materialize(source, graph, self.flow)
        except ValueError as error:
            self.studio.status.set(str(error))
            return
        self.completed = set()
        self.playing = "start"
        self.advance("next")

    def advance(self, port):
        if self.playing is None:
            return
        n = self.flow["nodes"][self.playing]
        if n["kind"] == "task":
            self.completed.add(self.playing)
        self.playing = self.flow["wires"][self.playing][port]
        while self.flow["nodes"][self.playing]["kind"] == "condition":
            n = self.flow["nodes"][self.playing]
            self.playing = self.flow["wires"][self.playing][
                "yes" if n.get("test") in self.completed else "no"
            ]
        self.selected = self.playing
        self.draw()
        self.panels()

    def preview_draw(self):
        box = self.preview
        clear(box)
        header = tk.Frame(box, bg=box["bg"])
        header.pack(fill="x")
        label(header, "Próbajáték", 11, GOLD).pack(side="left")
        if not self.flow.get("readonly"):
            button(header, "Újrakezdés" if self.playing else "▶  Indítás", self.start_preview).pack(
                side="right"
            )
        n = self.flow["nodes"][self.playing or self.selected]
        body = tk.Frame(box, bg=box["bg"])
        body.pack(fill="both", expand=True, pady=(4, 0))
        if n["kind"] in ("text", "dialogue"):
            portrait_key = n.get("portrait")
            if portrait_key in self.studio.campaign.images:
                tk.Label(body, image=self.studio.campaign.images[portrait_key], bg=box["bg"]).pack(
                    side="left", anchor="n", padx=(0, 10)
                )
            panel = tk.Frame(body, bg=box["bg"])
            panel.pack(side="left", fill="both", expand=True)
            label(panel, n["speaker"], 12, GOLD).pack(anchor="w")
            line = label(panel, n["line"], 10, wraplength=330)
            line.pack(anchor="w", fill="x")
            self.dragger.bind(line, lambda _: Asset("phrase", title(n), line=n["line"], speaker=n["speaker"]))
            panel.bind("<Configure>", lambda e: line.configure(wraplength=max(80, e.width - 4)))
            for p, ans in zip(ports(n), n.get("answers", ["Tovább"])):
                if self.playing:
                    button(panel, p + "  " + ans, lambda out=p: self.advance(out)).pack(anchor="w")
                else:
                    label(panel, p + "   " + ans, 9, "#b1d8c6").pack(anchor="w")
        else:
            label(body, title(n), 14).pack(anchor="w")
            label(body, details(n, self.flow), 10, MUTED).pack(anchor="w", pady=3)
            if self.playing and n["kind"] != "end":
                caption = {
                    "task": "Feladat teljesítése",
                    "timer": "Várakozás átlépése",
                    "reward": "Jutalom átvétele",
                }.get(n["kind"], "Tovább")
                button(body, caption, lambda: self.advance("next")).pack(anchor="w")
