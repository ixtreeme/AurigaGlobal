"""Shared catalog and internal pointer drags across the Studio work surfaces."""

import tkinter as tk
from dataclasses import replace
from functools import lru_cache
from tkinter import simpledialog, ttk

from quest_assets import Asset
from quest_flow import TYPES
from target_names import folded, target_names

GROUPS = {"Lépések": "step", "NPC-k": "npc", "Tárgyak": "item", "Szörnyek": "enemy", "Szövegek": "phrase"}
STEPS = ("text", "dialogue", "task", "condition", "reward", "timer")


@lru_cache(maxsize=3)
def catalog_rows(kind):
    index = target_names()
    table = index.get(kind)
    if table is None:
        table = index.get("mob", {}) if kind in ("npc", "enemy") else {}
    prefix = "mob" if kind == "enemy" else kind
    return sorted(
        (
            (folded(name) + " " + k, Asset(kind, name, prefix + "." + k + (".0" if kind == "item" else "")))
            for k, name in table.items()
        ),
        key=lambda pair: (pair[0], pair[1].subject),
    )


class TextDialog(simpledialog.Dialog):
    def body(self, frame):
        ttk.Label(frame, text="Név a könyvtárban").pack(anchor="w")
        self.name_field = ttk.Entry(frame, width=48)
        self.name_field.pack(fill="x")
        ttk.Label(frame, text="Beszélő").pack(anchor="w")
        self.speaker = ttk.Entry(frame)
        self.speaker.insert(0, "Mesélő")
        self.speaker.pack(fill="x")
        ttk.Label(frame, text="A játékosnak megjelenő szöveg").pack(anchor="w")
        self.line = tk.Text(frame, width=48, height=5, wrap="word")
        self.line.pack(fill="x")
        return self.name_field

    def validate(self):
        name, speaker, line = (
            self.name_field.get().strip(),
            self.speaker.get().strip(),
            self.line.get("1.0", "end-1c").strip(),
        )
        if not all((name, speaker, line)):
            self.bell()
            return False
        self.result = {"name": name, "speaker": speaker, "line": line}
        return True


class PointerDrag:
    def __init__(self, root, route, hint, finish):
        self.root, self.route, self.hint, self.finish = root, route, hint, finish
        self.pending = self.ghost = None
        self.root.bind("<Escape>", lambda _: self.cancel(), add=True)
        self.root.bind("<ButtonPress-3>", lambda _: self.cancel(), add=True)

    def bind(self, widget, resolve):
        def press(e):
            self.cancel()
            try:
                asset = resolve(e)
            except ValueError as error:
                self.hint(str(error))
                return "break"
            if asset is not None:
                self.pending = (asset, e.x_root, e.y_root)
                widget.focus_set()

        def move(e):
            if not self.pending:
                return None
            asset, x, y = self.pending
            if self.ghost is None:
                if max(abs(x - e.x_root), abs(y - e.y_root)) < 5:
                    return None
                self.ghost = tk.Label(
                    self.root,
                    text=asset.name,
                    padx=7,
                    pady=3,
                    fg="white",
                    bg="#355264",
                    borderwidth=1,
                    relief="solid",
                )
            self.ghost.place(
                x=e.x_root - self.root.winfo_rootx() + 15, y=e.y_root - self.root.winfo_rooty() + 16
            )
            self.ghost.lift()
            self.route(asset, e.x_root, e.y_root, False)
            return "break"

        def release(e):
            if not self.pending:
                return None
            asset = self.pending[0]
            dragged = self.ghost is not None
            self.cancel()
            if dragged:
                self.route(asset, e.x_root, e.y_root, True)
                return "break"

        widget.bind("<ButtonPress-1>", press, add=True)
        widget.bind("<B1-Motion>", move, add=True)
        widget.bind("<ButtonRelease-1>", release, add=True)

    def cancel(self):
        self.pending = None
        if self.ghost is not None:
            self.ghost.destroy()
            self.ghost = None
            self.finish()


class AssetShelf(tk.Frame):
    def __init__(self, parent, view):
        super().__init__(parent, bg="#17222c")
        self.view = view
        self.group = tk.StringVar(value="Lépések")
        self.query = tk.StringVar()
        self.count = tk.StringVar(value="1")
        self.level = tk.StringVar(value="0")
        self.matches = []
        title = tk.Label(
            self, text="Tartalomkönyvtár", anchor="w", bg=self["bg"], fg="#e5bf7d", font=("Segoe UI", 12)
        )
        title.pack(fill="x", pady=(6, 2))
        groups = ttk.Combobox(self, textvariable=self.group, values=list(GROUPS), state="readonly")
        groups.pack(fill="x")
        groups.bind("<<ComboboxSelected>>", lambda _: self.refresh())
        self.search = ttk.Entry(self, textvariable=self.query)
        self.search.pack(fill="x", pady=(4, 2))
        self.query.trace_add("write", lambda *_: self.refresh())
        area = tk.Frame(self)
        area.pack(fill="both", expand=True)
        self.list = tk.Listbox(
            area,
            bg="#101820",
            fg="#e0ebf0",
            selectbackground="#335466",
            activestyle="none",
            highlightthickness=0,
            exportselection=False,
            height=8,
            borderwidth=0,
            font=("Segoe UI", 10),
        )
        scroll = ttk.Scrollbar(area, command=self.list.yview)
        self.list.configure(yscrollcommand=scroll.set)
        scroll.pack(side="right", fill="y")
        self.list.pack(fill="both", expand=True)
        self.list.bind("<<ListboxSelect>>", self.identify_unnamed)
        self.options = tk.Frame(self, bg=self["bg"])
        for caption, var, high in (("Db", self.count, 50000), ("+Szint", self.level, 255)):
            tk.Label(self.options, text=caption, bg=self["bg"], fg="#b5c6d0").pack(side="left")
            ttk.Spinbox(
                self.options, from_=1 if var is self.count else 0, to=high, textvariable=var, width=5
            ).pack(side="left", padx=2)
        self.new = ttk.Button(self, text="+ Szöveg a könyvtárba…", command=self.new_text)
        self.bin = tk.Label(
            self, text="Eltávolítás · húzd ide az elemet", wraplength=185, pady=5, fg="#d9b49c", bg="#2c2829"
        )
        self.bin.pack(fill="x", side="bottom", pady=(4, 0))
        view.dragger.bind(self.list, self.grab_row)
        self.list.bind("<Return>", self.use_selected)
        self.refresh()

    def identify_unnamed(self, _):
        selected = self.list.curselection()
        if selected and selected[0] < len(self.matches):
            asset = self.matches[selected[0]]
            if asset.kind == "item" and asset.name == "Névtelen tárgy":
                self.view.studio.status.set(
                    "Fordítás nélküli tárgy · " + asset.subject.removeprefix("item.").rsplit(".", 1)[0]
                )

    def new_text(self):
        row = TextDialog(self, title="Új szövegdoboz").result
        if row:
            self.view.studio.board.setdefault("texts", []).append(row)
            self.query.set("")
            self.refresh()
            self.view.studio.save_layout()

    def refresh(self):
        self.options.pack_forget()
        self.new.pack_forget()
        kind = GROUPS[self.group.get()]
        if kind == "step":
            rows = [(folded(TYPES[k]), Asset("step", TYPES[k], k)) for k in STEPS]
        elif kind == "phrase":
            self.new.pack(fill="x")
            entries = {}
            board = getattr(self.view.studio, "board", {})

            def include(name, speaker, line):
                if line:
                    entries[(name, speaker, line)] = Asset("phrase", name, line=line, speaker=speaker)

            for t in board.get("texts", []):
                include(t["name"], t["speaker"], t["line"])
            for card in board.get("cards", {}).values():
                include(card.get("speaker") or "Történeti jegyzet", card.get("speaker"), card.get("line"))
                for n in card.get("flow", {}).get("nodes", {}).values():
                    if n["kind"] in ("text", "dialogue"):
                        include(n.get("name") or n["speaker"], n["speaker"], n["line"])
            rows = [(folded(a.name + " " + (a.speaker or "") + " " + a.line), a) for a in entries.values()]
        else:
            rows = catalog_rows(kind)
            if kind == "item":
                self.options.pack(fill="x", pady=3)
        query = folded(self.query.get().strip())
        self.matches = [asset for match, asset in rows if query in match]
        self.list.delete(0, "end")
        if self.matches:
            self.list.insert("end", *(a.name for a in self.matches))

    def grab_row(self, e):
        row = self.list.nearest(e.y)
        bbox = self.list.bbox(row) if row >= 0 else None
        if not bbox or not bbox[1] <= e.y < bbox[1] + bbox[3] + 1:
            return None
        asset = self.matches[row]
        if asset.kind == "item":
            try:
                amount, level = int(self.count.get()), int(self.level.get())
                if not (1 <= amount <= 50000 and 0 <= level <= 255):
                    raise ValueError
            except ValueError as error:
                raise ValueError("A darabszám 1–50 000, a fejlesztési szint 0–255 lehet.") from error
            index = target_names()
            base = asset.subject.removeprefix("item.").rsplit(".", 1)[0]
            profile = index.get("item_profiles", {}).get(base)
            if profile is not None and level not in index["levels"].get(profile, []):
                raise ValueError("Ehhez a tárgyhoz nem tartozik ilyen fejlesztési szint.")
            asset = replace(asset, count=amount, subject=asset.subject.rsplit(".", 1)[0] + "." + str(level))
        return asset

    def use_selected(self, e):
        try:
            selected = self.list.curselection()
            if not selected:
                return
            bbox = self.list.bbox(selected[0])
            if not bbox:
                return
            e.y = bbox[1] + bbox[3] // 2
            asset = self.grab_row(e)
            if asset and self.view.code:
                self.view.drop_on(self.view.code, asset, self.view.selected if asset.kind != "step" else None)
        except ValueError as error:
            self.view.studio.status.set(str(error))
