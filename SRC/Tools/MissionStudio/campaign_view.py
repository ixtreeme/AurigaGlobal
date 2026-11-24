import itertools
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, simpledialog, ttk

from campaign import KINDS, gate, links, place_missing, set_link

BG, PANEL, BORDER = "#101619", "#182024", "#394044"
INK, MUTED, GOLD = "#eee7d9", "#adb3ae", "#e7bd70"
PALETTE = {"main": ("#30291f", "#c8a15a"), "side": ("#192b35", "#78adc7"), "secret": ("#2a2234", "#b797ce")}
ART = Path(__file__).with_name("art")
CARD_W, CARD_H = 170, 58


def label(parent, text, *, size=10, fg=INK, bg=PANEL, serif=False, **kw):
    return tk.Label(
        parent, text=text, font=("Georgia" if serif else "Segoe UI", size), fg=fg, bg=bg, anchor="w", **kw
    )


def button(parent, text, command, *, selected=False):
    return tk.Button(
        parent,
        text=text,
        command=command,
        font=("Segoe UI", 9),
        padx=9,
        pady=4,
        bg="#35332b" if selected else PANEL,
        fg=GOLD if selected else INK,
        relief="flat",
        activebackground="#394348",
        activeforeground=INK,
        borderwidth=0,
        cursor="hand2",
    )


class CampaignView(tk.Frame):
    def __init__(self, parent, studio):
        super().__init__(parent, bg=BG)
        self.studio = studio
        self.act, self.selected, self.edge = 0, None, None
        self.drag = self.wiring = None
        self.images = {p.stem: tk.PhotoImage(file=p) for p in ART.glob("*.png")}
        self.header = tk.Frame(self, bg=PANEL)
        self.header.pack(fill="x")
        self.cover = tk.Label(self.header, bg=PANEL, borderwidth=0)
        self.cover.pack(side="left", padx=(0, 14))
        titles = tk.Frame(self.header, bg=PANEL)
        titles.pack(side="left", fill="both", expand=True, pady=8)
        label(titles, "AURIGA GLOBAL   /   MISSION STUDIO", size=9, fg=GOLD).pack(anchor="w")
        self.title = label(titles, "", size=23, serif=True)
        self.title.pack(anchor="w")
        self.act_title = label(titles, "", size=11, fg=MUTED, serif=True)
        self.act_title.pack(anchor="w", pady=(3, 0))
        button(self.header, "Kampány neve…", self.rename_campaign).pack(side="right", padx=12)
        self.acts = tk.Frame(self, bg=BG)
        self.acts.pack(fill="x", pady=(5, 6))
        self.body = tk.Frame(self, bg=BG)
        self.body.pack(fill="both", expand=True)
        self.rail = tk.Frame(self.body, bg=PANEL, width=205)
        self.rail.pack(side="left", fill="y", padx=(0, 7))
        self.rail.pack_propagate(False)
        self.right = tk.Frame(self.body, bg=BG)
        self.right.pack(side="left", fill="both", expand=True)
        self.scenes = tk.Frame(self.right, bg=PANEL)
        self.scenes.pack(fill="x")
        self.graphbar = tk.Frame(self.right, bg=BG)
        self.graphbar.pack(fill="x", pady=(6, 4))
        label(self.graphbar, "Küldetésfa", size=15, fg=GOLD, bg=BG, serif=True).pack(side="left", padx=8)
        button(self.graphbar, "Új küldetés…", studio.add_mission).pack(side="right")
        self.unlink_button = button(self.graphbar, "Kapcsolat bontása", self.unlink)
        self.unlink_button.pack(side="right", padx=3)
        self.detail = tk.Frame(self.right, bg=PANEL)
        self.detail.pack(side="bottom", fill="x", pady=(5, 0))
        frame = tk.Frame(self.right, bg=BORDER)
        frame.pack(fill="both", expand=True)
        self.canvas = tk.Canvas(
            frame,
            bg="#10191e",
            highlightthickness=1,
            highlightbackground=BORDER,
            takefocus=True,
            width=480,
            height=240,
        )
        hx = ttk.Scrollbar(frame, orient="horizontal", command=self.canvas.xview)
        vy = ttk.Scrollbar(frame, command=self.canvas.yview)
        self.canvas.configure(xscrollcommand=hx.set, yscrollcommand=vy.set)
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        vy.grid(row=0, column=1, sticky="ns")
        hx.grid(row=1, column=0, sticky="ew")
        for event, action in (
            ("<ButtonPress-1>", self.press),
            ("<B1-Motion>", self.motion),
            ("<ButtonRelease-1>", self.release),
            ("<Double-Button-1>", self.open_card),
            ("<Delete>", lambda _: self.unlink()),
            ("<Escape>", self.cancel_gesture),
        ):
            self.canvas.bind(event, action)
        self.canvas.bind("<MouseWheel>", lambda e: self.canvas.yview_scroll(-int(e.delta / 120), "units"))
        self.canvas.bind(
            "<Shift-MouseWheel>", lambda e: self.canvas.xview_scroll(-int(e.delta / 120), "units")
        )

    @property
    def board(self):
        return self.studio.board

    def graph(self, code):
        return next(g for g in self.studio.source["missions"] if g["mission"] == code)

    def name(self, code):
        binding, texts = self.studio.presentation(self.graph(code))
        return texts[binding["title"]]

    def reset(self):
        self.act, self.selected, self.edge = 0, None, None
        self.drag = self.wiring = None
        self.canvas.xview_moveto(0)
        self.canvas.yview_moveto(0)

    @staticmethod
    def clear(frame):
        for widget in frame.winfo_children():
            widget.destroy()

    def refresh(self):
        place_missing(self.board, self.studio.source, self.act)
        self.title.config(text=self.board["title"])
        act = self.board["acts"][self.act]
        self.act_title.config(text=f"{self.act + 1}. felvonás   ·   {act['name']}")
        with_art = any(r.get("art") for r in act.get("scenes", []))
        self.cover.config(image=self.images.get("panorama", "") if with_art else "")
        self.clear(self.acts)
        if len(self.board["acts"]) <= 5:
            for i, item in enumerate(self.board["acts"]):
                button(
                    self.acts,
                    f"{i + 1}. {item['name']}",
                    lambda n=i: self.choose_act(n),
                    selected=i == self.act,
                ).pack(side="left", padx=(0, 3))
        else:
            chooser = ttk.Combobox(self.acts, values=self.act_names(), state="readonly", width=34)
            chooser.current(self.act)
            chooser.pack(side="left", padx=4)
            chooser.bind("<<ComboboxSelected>>", lambda _: self.choose_act(chooser.current()))
        button(self.acts, "+ Felvonás", self.add_act).pack(side="left")
        button(self.acts, "Felvonás neve…", self.rename_act).pack(side="right")
        self.clear(self.rail)
        label(self.rail, "Főszereplők", size=14, fg=GOLD, serif=True).pack(anchor="w", padx=12, pady=(10, 6))
        for row in act.get("cast", []):
            tile = tk.Frame(self.rail, bg=PANEL)
            tile.pack(fill="x", padx=9, pady=3)
            if row.get("art") in self.images:
                tk.Label(tile, image=self.images[row["art"]], bg=BORDER, padx=1, pady=1).pack(side="left")
            text = tk.Frame(tile, bg=PANEL)
            text.pack(side="left", padx=7)
            label(text, row["name"], fg=GOLD, serif=True, wraplength=115).pack(anchor="w")
            label(text, row.get("role", ""), size=9, fg=MUTED, wraplength=116).pack(anchor="w")
        button(self.rail, "+ Szereplő", lambda: self.add_reference("cast")).pack(anchor="w", padx=8, pady=5)
        self.clear(self.scenes)
        head = tk.Frame(self.scenes, bg=PANEL)
        head.pack(fill="x", padx=8, pady=(3, 0))
        label(head, "Helyszínek", size=13, fg=GOLD, serif=True).pack(side="left")
        button(head, "+ Helyszín", lambda: self.add_reference("scenes")).pack(side="right")
        strip = tk.Frame(self.scenes, bg=PANEL)
        strip.pack(fill="x", padx=6)
        for i, row in enumerate(act.get("scenes", [])):
            strip.columnconfigure(i, weight=1, uniform="scene")
            tile = tk.Frame(strip, bg=PANEL)
            tile.grid(row=0, column=i, sticky="nsew", padx=3, pady=(0, 3))
            if row.get("art") in self.images:
                area = tk.Canvas(tile, height=65, width=1, bg=PANEL, highlightthickness=0)
                area.pack(fill="x")
                area.create_image(0, -12, anchor="nw", image=self.images[row["art"]])
            label(tile, row["name"], size=9).pack(anchor="w")
        self.draw()
        self.details()

    def choose_act(self, index):
        self.reset()
        self.act = index
        self.refresh()

    def act_names(self):
        return [f"{i + 1}. {a['name']}" for i, a in enumerate(self.board["acts"])]

    def add_act(self):
        if len(self.board["acts"]) >= 128:
            return
        name = simpledialog.askstring("Új felvonás", "Felvonás neve", parent=self)
        if name and name.strip():
            self.board["acts"].append({"name": name.strip()})
            self.choose_act(len(self.board["acts"]) - 1)
            self.studio.save_layout()

    def rename_campaign(self):
        name = simpledialog.askstring(
            "Kampány", "Kampány neve", initialvalue=self.board["title"], parent=self
        )
        if name and name.strip():
            self.board["title"] = name.strip()
            self.refresh()
            self.studio.save_layout()

    def rename_act(self):
        act = self.board["acts"][self.act]
        name = simpledialog.askstring("Felvonás", "Felvonás neve", initialvalue=act["name"], parent=self)
        if name and name.strip():
            act["name"] = name.strip()
            self.refresh()
            self.studio.save_layout()

    def add_reference(self, section):
        name = simpledialog.askstring(
            "Felvonás", "Szereplő neve" if section == "cast" else "Helyszín neve", parent=self
        )
        if not name or not name.strip():
            return
        role = (
            simpledialog.askstring("Szereplő", "Szerepe a történetben", parent=self)
            if section == "cast"
            else ""
        )
        if role is not None:
            self.board["acts"][self.act].setdefault(section, []).append(
                {"name": name.strip(), "role": role.strip()}
            )
            self.refresh()
            self.studio.save_layout()

    def shown(self):
        return {k: v for k, v in self.board["cards"].items() if v["act"] == self.act}

    @staticmethod
    def arrow_path(cards, a, b):
        left, right = cards[a], cards[b]
        x1, y1 = left["x"] + CARD_W, left["y"] + CARD_H / 2
        x2, y2 = right["x"], right["y"] + CARD_H / 2
        middle = (x1 + x2) / 2
        direct = [(x1, y1), (middle, y1), (middle, y2), (x2, y2)]

        def clear(path):
            for (x, y), (xx, yy) in itertools.pairwise(path):
                for key, card in cards.items():
                    if key in (a, b):
                        continue
                    if (
                        max(x, xx) >= card["x"] - 4
                        and min(x, xx) <= card["x"] + CARD_W + 4
                        and max(y, yy) >= card["y"] - 4
                        and min(y, yy) <= card["y"] + CARD_H + 4
                    ):
                        return False
            return True

        if x2 > x1 and clear(direct):
            return direct
        lanes = sorted(
            {max(5, c["y"] - 16) for c in cards.values()} | {c["y"] + CARD_H + 16 for c in cards.values()},
            key=lambda y: abs(y - y1) + abs(y - y2),
        )
        for y in lanes:
            path = [(x1, y1), (x1 + 14, y1), (x1 + 14, y), (x2 - 14, y), (x2 - 14, y2), (x2, y2)]
            if clear(path):
                return path
        return direct

    def draw(self):
        c = self.canvas
        c.delete("all")
        cards = self.shown()
        for i, (a, b) in enumerate(links(self.studio.source)):
            if a not in cards or b not in cards:
                continue
            path = self.arrow_path(cards, a, b)
            c.create_line(
                *[v for p in path for v in p],
                arrow="last",
                width=3 if self.edge == (a, b) else 2,
                fill=INK if self.edge == (a, b) else PALETTE[cards[b]["kind"]][1],
                tags=(f"wire:{i}",),
            )
        for key, card in cards.items():
            x, y = card["x"], card["y"]
            fill, color = PALETTE[card["kind"]]
            tag = f"node:{key}"
            c.create_rectangle(
                x,
                y,
                x + CARD_W,
                y + CARD_H,
                fill=fill,
                outline=INK if key == self.selected else color,
                width=2 if key == self.selected else 1,
                tags=(tag,),
            )
            c.create_rectangle(x + 1, y + 1, x + 4, y + CARD_H - 1, fill=color, outline="", tags=(tag,))
            c.create_text(
                x + 12,
                y + 10,
                text=KINDS[card["kind"]].upper(),
                font=("Segoe UI", 8),
                anchor="nw",
                fill=color,
                tags=(tag,),
            )
            c.create_text(
                x + 12,
                y + 27,
                text=self.name(key),
                font=("Georgia", 10),
                width=CARD_W - 21,
                anchor="nw",
                fill=INK,
                tags=(tag,),
            )
            for px, side in ((x, "in"), (x + CARD_W, "out")):
                cy = y + CARD_H / 2
                c.create_oval(
                    px - 5, cy - 5, px + 5, cy + 5, fill=color, outline=BG, tags=(f"{side}:{key}", tag)
                )
        xmax = max([p["x"] + CARD_W + 35 for p in cards.values()] or [1])
        ymax = max([p["y"] + CARD_H + 25 for p in cards.values()] or [1])
        c.configure(scrollregion=(0, 0, max(xmax, c.winfo_width() - 2), max(ymax, c.winfo_height() - 2)))
        self.unlink_button.config(state="normal" if self.edge else "disabled")

    def hit(self, x, y):
        for item in reversed(self.canvas.find_overlapping(x - 3, y - 3, x + 3, y + 3)):
            for tag in self.canvas.gettags(item):
                if ":" in tag:
                    return tag.split(":", 1)
        return "", ""

    def point(self, e):
        return self.canvas.canvasx(e.x), self.canvas.canvasy(e.y)

    def press(self, e):
        self.canvas.focus_set()
        x, y = self.point(e)
        kind, key = self.hit(x, y)
        self.drag = self.wiring = self.edge = None
        if kind == "out":
            self.wiring = key
        elif kind in ("node", "in"):
            card = self.board["cards"][key]
            self.drag = (key, x - card["x"], y - card["y"], card["x"], card["y"])
        elif kind == "wire":
            self.edge = links(self.studio.source)[int(key)]
        else:
            self.canvas.scan_mark(e.x, e.y)
        if kind in ("node", "in", "out"):
            self.selected = key
            self.studio.select_mission(key)
        self.draw()
        self.details()

    def motion(self, e):
        x, y = self.point(e)
        if self.wiring:
            card = self.board["cards"][self.wiring]
            self.canvas.delete("preview")
            self.canvas.create_line(
                card["x"] + CARD_W,
                card["y"] + CARD_H / 2,
                x,
                y,
                fill=GOLD,
                dash=(4, 3),
                arrow="last",
                tags=("preview",),
            )
        elif self.drag:
            key, dx, dy, _, _ = self.drag
            self.board["cards"][key].update(
                x=min(100000, max(9, round(x - dx))), y=min(100000, max(6, round(y - dy)))
            )
            self.draw()
        elif not self.edge:
            self.canvas.scan_dragto(e.x, e.y, gain=1)

    def release(self, e):
        if self.wiring:
            origin = self.wiring
            self.wiring = None
            self.canvas.delete("preview")
            kind, target = self.hit(*self.point(e))
            if kind in ("in", "node"):
                self.change_link(origin, target)
        if self.drag:
            key, _, _, old_x, old_y = self.drag
            self.drag = None
            card = self.board["cards"][key]
            if (old_x, old_y) != (card["x"], card["y"]):
                self.studio.save_layout()

    def cancel_gesture(self, _=None):
        if self.drag:
            key, _, _, x, y = self.drag
            self.board["cards"][key].update(x=x, y=y)
        self.drag = self.wiring = self.edge = None
        self.draw()
        self.details()

    def change_link(self, a, b, remove=False):
        try:
            affected = set_link(self.studio.source, a, b, remove=remove)
        except ValueError as error:
            messagebox.showerror("Kapcsolat", str(error), parent=self)
            return
        self.edge = None
        self.selected = b
        self.studio.changed(self.graph(b), affected=affected)

    def unlink(self):
        if self.edge:
            self.change_link(*self.edge, remove=True)

    def open_card(self, e):
        kind, key = self.hit(*self.point(e))
        if kind == "node":
            self.drag = self.wiring = None
            self.studio.open_details(key)

    def details(self):
        self.clear(self.detail)
        if self.edge:
            a, b = self.edge
            label(self.detail, self.name(a) + "   →   " + self.name(b), fg=GOLD).pack(
                anchor="w", padx=9, pady=9
            )
            return
        if self.selected not in self.shown():
            label(
                self.detail,
                "A nyíl létrehozásához húzd az egyik kártya jobb oldali pontját a következő kártyára.",
                fg=MUTED,
                wraplength=660,
            ).pack(anchor="w", padx=9, pady=9)
            return
        code = self.selected
        graph = self.graph(code)
        card = self.board["cards"][code]
        row = tk.Frame(self.detail, bg=PANEL)
        row.pack(fill="x", padx=8, pady=(6, 2))
        label(row, self.name(code), size=13, fg=GOLD, serif=True).pack(side="left")
        button(row, "Küldetés szerkesztése", lambda: self.studio.open_details(code)).pack(side="right")
        button(row, "Kapcsolatok…", lambda: self.connections(code)).pack(side="right", padx=3)
        button(row, "Történeti jegyzet…", lambda: self.note(card)).pack(side="right", padx=3)
        kind = ttk.Combobox(row, state="readonly", values=list(KINDS.values()), width=14)
        kind.set(KINDS[card["kind"]])
        kind.pack(side="right", padx=5)

        def category(_):
            card["kind"] = next(k for k, v in KINDS.items() if v == kind.get())
            self.draw()
            self.studio.save_layout()

        kind.bind("<<ComboboxSelected>>", category)
        if card.get("line"):
            quote_bar = tk.Frame(self.detail, bg="#e1d3b4", padx=11, pady=5)
            quote_bar.pack(fill="x", padx=8, pady=(2, 6))
            if card.get("speaker"):
                label(quote_bar, card["speaker"], size=10, fg="#594322", bg="#e1d3b4", serif=True).pack(
                    anchor="w"
                )
            label(
                quote_bar,
                card["line"],
                fg="#322c25",
                bg="#e1d3b4",
                serif=True,
                wraplength=max(620, self.right.winfo_width() - 54),
                justify="left",
            ).pack(anchor="w")
        parents = [self.name(a) for a, b in links(self.studio.source) if b == code]
        if parents:
            rule = gate(graph)
            lower = tk.Frame(self.detail, bg=PANEL)
            lower.pack(anchor="w", padx=9, pady=(0, 4))
            label(lower, "Előfeltétel", size=9, fg=MUTED).pack(side="left")
            mode = ttk.Combobox(lower, state="readonly", width=10, values=["Mindegyik", "Bármelyik"])
            mode.set("Mindegyik" if rule["op"] == "all_of" else "Bármelyik")
            mode.pack(side="left", padx=6)
            label(lower, ", ".join(parents), size=9, fg=MUTED, wraplength=430).pack(side="left")

            def change_mode(_):
                rule["op"] = "all_of" if mode.get() == "Mindegyik" else "any_of"
                self.studio.changed(graph)

            mode.bind("<<ComboboxSelected>>", change_mode)

    def note(self, card):
        speaker = simpledialog.askstring(
            "Történeti jegyzet", "Beszélő neve", initialvalue=card.get("speaker", ""), parent=self
        )
        if speaker is None:
            return
        line = simpledialog.askstring(
            "Történeti jegyzet", "Párbeszédvázlat", initialvalue=card.get("line", ""), parent=self
        )
        if line is not None:
            card.update(speaker=speaker.strip(), line=line.strip())
            self.details()
            self.studio.save_layout()

    def connections(self, code):
        # This list also exposes incoming connections from other acts and
        # allows authoring the graph using only the keyboard.
        win = tk.Toplevel(self)
        win.title(self.name(code) + " · Kapcsolatok")
        win.transient(self.winfo_toplevel())
        body = ttk.Frame(win, padding=12)
        body.pack(fill="both", expand=True)
        ttk.Label(body, text="A kártya felvonása").pack(anchor="w")
        acts = ttk.Combobox(body, state="readonly", values=self.act_names(), width=50)
        acts.current(self.board["cards"][code]["act"])
        acts.pack(fill="x", pady=(2, 10))
        ttk.Label(body, text="Lehetséges előfeltétel").pack(anchor="w")
        box = ttk.Treeview(body, columns=("state",), height=9, selectmode="browse")
        box.heading("#0", text="Felvonás és küldetés")
        box.heading("state", text="Kapcsolat")
        box.column("state", width=110, stretch=False)
        box.pack(fill="both", expand=True)

        def populate():
            parents = {a for a, b in links(self.studio.source) if b == code}
            active = box.selection()
            box.delete(*box.get_children())
            for other, card in self.board["cards"].items():
                if other != code:
                    act = self.board["acts"][card["act"]]["name"]
                    box.insert(
                        "",
                        "end",
                        iid=other,
                        text=act + "  /  " + self.name(other),
                        values=("Előfeltétel" if other in parents else "—",),
                    )
            if active:
                box.selection_set(active[0])

        populate()

        def move(_):
            self.board["cards"][code]["act"] = acts.current()
            self.choose_act(acts.current())
            self.selected = code
            self.refresh()
            self.studio.save_layout()

        acts.bind("<<ComboboxSelected>>", move)

        def toggle():
            selected = box.selection()
            if selected:
                parent = selected[0]
                self.change_link(parent, code, remove=(parent, code) in links(self.studio.source))
                populate()

        ttk.Button(body, text="Kapcsolat hozzáadása / bontása", command=toggle).pack(anchor="w", pady=(8, 0))

        def remove():
            if messagebox.askyesno(
                "Küldetés kivétele",
                "Kiveszed a küldetést és a hozzá tartozó nyilakat a tervezetből?",
                parent=win,
            ):
                try:
                    self.studio.remove_card(code)
                except ValueError as error:
                    messagebox.showerror("Küldetés kivétele", str(error), parent=win)
                else:
                    win.destroy()

        ttk.Button(body, text="Küldetés kivétele a tervezetből…", command=remove).pack(
            anchor="w", pady=(6, 0)
        )
        box.bind("<Return>", lambda _: toggle())
        win.bind("<Escape>", lambda _: win.destroy())
        win.grab_set()
        box.focus_set()
