import os
from copy import deepcopy
from pathlib import Path
import shutil
import subprocess
import tempfile
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk
from uuid import uuid4

from mission_source import EVENTS, add_objective, build_draft, check_source, compile_source, new_mission, new_package
from mission_source import completion_reward, objective_subject, read_json, save_draft, set_completion_reward
from mission_source import edit_translation
from target_names import display, folded, target_names
from campaign import check_board, crown_example, empty_board, remove_mission
from campaign_view import BG, PANEL, CampaignView, button


class TargetPicker(simpledialog.Dialog):
    def __init__(self, parent, event, current="", wildcard=True):
        self.event, self.current, self.wildcard = event, current, wildcard
        super().__init__(parent, "Célpont kiválasztása")

    def body(self, frame):
        self.prefix = EVENTS[self.event][1]
        is_item = self.prefix == "item."
        choices = target_names()["item" if is_item else "mob"]
        self.choices = sorted(((key, name, folded(name)) for key, name in choices.items()), key=lambda row: row[2])
        ttk.Label(frame, text="Keresés név szerint").pack(anchor="w")
        self.query = tk.StringVar()
        entry = ttk.Entry(frame, textvariable=self.query, width=44)
        entry.pack(fill="x", pady=4)
        box = ttk.Frame(frame)
        box.pack(fill="both", expand=True)
        self.list = tk.Listbox(box, width=48, height=13, exportselection=False)
        bar = ttk.Scrollbar(box, command=self.list.yview)
        self.list.configure(yscrollcommand=bar.set)
        self.list.pack(side="left", fill="both", expand=True)
        bar.pack(side="right", fill="y")
        self.level = tk.StringVar(value=self.current.rsplit(".", 1)[-1] if is_item and self.current.startswith("item.") else "0")
        if is_item:
            ttk.Label(frame, text="Fejlesztési szint").pack(anchor="w")
            ttk.Spinbox(frame, from_=0, to=255, textvariable=self.level).pack(anchor="w")
        self.query.trace_add("write", self.filter)
        self.list.bind("<Double-1>", lambda _: self.ok())
        self.filter()
        return entry

    def filter(self, *_):
        query = folded(self.query.get().strip())
        self.matches = [(key, name) for key, name, text in self.choices if query in text or key == query]
        if self.wildcard and (not query or query == "*" or query in "barmelyik"):
            self.matches.insert(0, ("*", "Bármelyik"))
        self.list.delete(0, "end")
        if self.matches:
            self.list.insert("end", *(name for _, name in self.matches))
            selected = self.current.removeprefix(self.prefix)
            if self.prefix == "item.":
                selected = selected.rpartition(".")[0]
            index = next((i for i, (key, _) in enumerate(self.matches) if key == selected), 0)
            self.list.selection_set(index)
            self.list.see(index)

    def validate(self):
        if not self.list.curselection():
            return False
        key, _ = self.matches[self.list.curselection()[0]]
        try:
            target = key if key == "*" or self.prefix != "item." else key + "." + self.level.get()
            self.result = objective_subject(self.event, target)
            return True
        except ValueError as error:
            messagebox.showerror("Célpont", str(error), parent=self)
            return False


class ObjectiveDialog(simpledialog.Dialog):
    def __init__(self, parent, values):
        self.values = values
        super().__init__(parent, "Küldetéscél")

    def body(self, frame):
        title, event, subject, goal = self.values
        self.name = tk.StringVar(value=title)
        self.event = tk.StringVar(value=EVENTS[event][0])
        self.target = tk.StringVar(value=subject)
        self.count = tk.StringVar(value=str(goal))
        for row, label in enumerate(("Cél neve", "Esemény", "Célpont", "Darabszám")):
            ttk.Label(frame, text=label).grid(row=row, column=0, padx=8, pady=5, sticky="w")
        first = ttk.Entry(frame, textvariable=self.name, width=42)
        first.grid(row=0, column=1)
        combo = ttk.Combobox(frame, textvariable=self.event, values=[e[0] for e in EVENTS.values()],
                             state="readonly", width=39)
        combo.grid(row=1, column=1)
        self.pick = ttk.Button(frame, text=display(subject) or "Kiválasztás…", command=self.choose)
        self.pick.grid(row=2, column=1, sticky="ew")
        combo.bind("<<ComboboxSelected>>", lambda _: (self.target.set(""), self.pick.config(text="Kiválasztás…")))
        ttk.Entry(frame, textvariable=self.count, width=42).grid(row=3, column=1)
        return first

    def choose(self):
        event = next(key for key, (label, _) in EVENTS.items() if label == self.event.get())
        result = TargetPicker(self, event, self.target.get()).result
        if result:
            self.target.set(result)
            self.pick.config(text=display(result))

    def validate(self):
        try:
            event = next(key for key, (label, _) in EVENTS.items() if label == self.event.get())
            subject = objective_subject(event, self.target.get().strip())
            count = int(self.count.get())
            if not self.name.get().strip() or not 1 <= count <= 1_000_000_000:
                raise ValueError("Adj meg célnevet és pozitív, legfeljebb egymilliárdos darabszámot.")
            self.result = (self.name.get().strip(), event, subject, count)
            return True
        except ValueError as error:
            messagebox.showerror(self.title(), str(error), parent=self)
            return False


class Studio:
    def __init__(self, root, drafts):
        self.root, self.drafts = root, drafts
        self.drafts.mkdir(parents=True, exist_ok=True)
        adjacent = Path(__file__).resolve().with_name("MissionCompiler.exe")
        self.compiler = os.environ.get("AURIGA_MISSION_COMPILER") or (
            str(adjacent) if adjacent.is_file() else shutil.which("MissionCompiler"))
        self.root.title("Auriga · Mission Studio")
        width = min(1330, root.winfo_screenwidth()-30)
        height = min(860, root.winfo_screenheight()-90)
        self.root.geometry(f"{width}x{height}")
        self.root.minsize(min(width, 900), min(height, 540))
        self.root.configure(bg=BG)
        toolbar = tk.Frame(root, bg=PANEL, padx=8, pady=4)
        toolbar.pack(fill="x")
        for label, command in (("Új csomag", self.blank), ("Forrás megnyitása…", self.open_source),
                               ("Korona mintaterv", self.example), ("Ellenőrzés és csomagolás…", self.build)):
            button(toolbar, label, command).pack(side="left", padx=3)
        self.tabs = ttk.Notebook(root)
        self.tabs.pack(fill="both", expand=True, padx=8, pady=(5, 0))
        self.campaign = CampaignView(self.tabs, self)
        self.tabs.add(self.campaign, text="  Kampánygráf  ")
        editor = ttk.Frame(self.tabs)
        self.tabs.add(editor, text="  Küldetés részletei  ")
        panels = ttk.Panedwindow(editor, orient="horizontal")
        panels.pack(fill="both", expand=True, padx=10)
        left = ttk.Labelframe(panels, text="Küldetések", padding=6)
        right = ttk.Frame(panels, padding=(8, 0))
        panels.add(left, weight=1)
        panels.add(right, weight=3)
        self.missions = tk.Listbox(left, exportselection=False, width=28)
        self.missions.pack(fill="both", expand=True)
        self.missions.bind("<<ListboxSelect>>", self.list_selected)
        self.heading = ttk.Label(right, font=("Segoe UI", 15, "bold"))
        self.heading.pack(anchor="w", pady=4)
        self.description = ttk.Label(right, wraplength=540)
        self.description.pack(anchor="w", pady=4)
        self.steps = ttk.Treeview(right, columns=("event", "target", "count"), height=10)
        for key, label, width in (("#0", "Lépés", 185), ("event", "Esemény", 140),
                                  ("target", "Célpont", 100), ("count", "Darab", 55)):
            self.steps.heading(key, text=label)
            self.steps.column(key, width=width)
        self.steps.pack(fill="both", expand=True, pady=8)
        self.steps.bind("<Double-1>", lambda _: self.edit_step())
        actions = ttk.Frame(right)
        actions.pack(fill="x")
        ttk.Button(left, text="Új küldetés…", command=self.add_mission).pack(fill="x",pady=(5,0))
        for label, command in (("Név és leírás…", self.edit_text), ("Küldetésadó…", self.admission), ("Új cél…", self.add_step),
                               ("Cél szerkesztése…", self.edit_step), ("Zárójutalom…", self.reward)):
            ttk.Button(actions, text=label, command=command).pack(side="left", padx=3)
        self.status = tk.StringVar()
        ttk.Label(root, textvariable=self.status, padding=10, wraplength=920).pack(fill="x")
        self.example()

    def load(self, source, rules, imported=False, board=None):
        # A damaged local draft must leave the currently open work available.
        locales = source["presentation"]["locales"]
        texts = locales[source["default_locale"]]
        for graph in source["missions"]:
            binding = source["presentation"]["missions"][graph["mission"]]
            nodes = {n["id"]: n for n in graph["nodes"]}
            root = nodes[graph["root"]]
            for child in root.get("children", []):
                if child not in nodes:
                    raise ValueError("A tervezet hiányzó küldetéslépésre hivatkozik.")
            for key in (binding["title"], binding["summary"], *(o["title"] for o in binding["objectives"])):
                if key not in texts:
                    raise ValueError("A tervezetből hiányzik egy megjelenítendő szöveg.")
            if graph["mission"] in rules and len(rules[graph["mission"]]) != 2:
                raise ValueError("Érvénytelen küldetésadó a tervezetben.")
        versions = {g["mission"]: g["version"] for g in source["missions"]} if imported else {}
        version = source["version"] if imported else None
        verified_board = check_board(board if board is not None else empty_board(), source)
        self.source, self.rules, self.board = source, rules, verified_board
        self.path = self.drafts / uuid4().hex / "tervezet.json"
        self.original_versions, self.original_package = versions, version
        self.campaign.reset()
        if source["missions"]:
            self.campaign.selected = source["missions"][0]["mission"]
            self.campaign.act = self.board["cards"].get(self.campaign.selected, {}).get("act", 0)
        self.refresh()
        self.tabs.select(self.campaign)

    def example(self):
        source, board = crown_example()
        self.load(source, {}, board=board)
        self.status.set("Mintaterv · A játékbeli csomagoláshoz add meg a küldetésadókat és a teljesítési célokat.")

    def select_mission(self, code):
        index = next(i for i,g in enumerate(self.source["missions"]) if g["mission"] == code)
        self.missions.selection_clear(0, "end")
        self.missions.selection_set(index)
        self.missions.see(index)
        self.show_mission()

    def open_details(self, code):
        self.select_mission(code)
        self.tabs.select(1)

    def remove_card(self, code):
        candidate = deepcopy(self.source)
        affected = remove_mission(candidate, code)
        self.source = candidate
        self.rules.pop(code, None)
        self.board["cards"].pop(code)
        if self.original_package is not None:
            self.source["version"] = self.original_package + 1
        for graph in self.source["missions"]:
            key = graph["mission"]
            if key in affected and key in self.original_versions:
                graph["version"] = self.original_versions[key] + 1
        self.campaign.selected = next(iter(self.campaign.shown()), None)
        self.campaign.edge = None
        self.refresh()
        self.save_layout()

    def list_selected(self, _):
        self.show_mission()
        graph = self.current()
        if graph and self.tabs.index(self.tabs.select()) == 1:
            code = graph["mission"]
            self.campaign.act = self.board["cards"][code]["act"]
            self.campaign.selected = code
            self.campaign.edge = None
            self.campaign.refresh()

    def blank(self):
        self.load(new_package(), {})
        self.status.set("Új tervezet. Adj hozzá egy küldetést.")

    def get_compiler(self):
        if not self.compiler:
            self.compiler = filedialog.askopenfilename(title="A C++ MissionCompiler kiválasztása",
                                                      filetypes=[("MissionCompiler", "MissionCompiler*")])
        return self.compiler

    def open_source(self):
        name = filedialog.askopenfilename(initialdir=self.drafts, title="Küldetéscsomag forrása",
                                          filetypes=[("Küldetésforrás", "*.json")])
        if not name:
            return
        try:
            path = Path(name).resolve()
            document = read_json(path)
            own = isinstance(document, dict) and document.get("mission_studio") == 1
            source = check_source(document["source"] if own else document)
            rules = dict(document["admissions"]) if own else {}
            # Drafts can be incomplete. Imported packages must pass the same
            # structural validation as the server before becoming editable.
            if not own:
                compiler = self.get_compiler()
                if not compiler:
                    return
                with tempfile.TemporaryDirectory() as temp:
                    compile_source(compiler, source, Path(temp))
                config = path.with_name("typed_mission_runtime.json")
                if config.is_file():
                    for row in read_json(config)["missions"]:
                        if any(g["mission"] == row["code"] and g["version"] == row["version"] for g in source["missions"]):
                            rules[row["code"]] = [row["npc_race"], row["min_level"]]
            board = document.get("campaign") if own else None
            sidecar = path.with_name("kampanyterv.json")
            if not own and sidecar.is_file():
                layout = read_json(sidecar)
                if layout["package"] == source["package"] and layout["version"] == source["version"]:
                    board = layout["campaign"]
            self.load(source, rules, True, board)
            self.status.set("Megnyitva · " + str(path))
        except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
            messagebox.showerror("A forrás megnyitása sikertelen", str(error), parent=self.root)

    def refresh(self, index=0):
        self.missions.delete(0, "end")
        for graph in self.source["missions"]:
            binding, texts = self.presentation(graph)
            self.missions.insert("end", texts.get(binding["title"], graph["mission"]))
        if self.source["missions"]:
            self.missions.selection_set(index)
            self.missions.see(index)
        self.show_mission()
        self.campaign.refresh()

    def current(self):
        selected = self.missions.curselection()
        return self.source["missions"][selected[0]] if selected else None

    def presentation(self, graph):
        presentation = self.source["presentation"]
        return presentation["missions"][graph["mission"]], presentation["locales"][self.source["default_locale"]]

    def show_mission(self):
        self.steps.delete(*self.steps.get_children())
        graph = self.current()
        if not graph:
            self.heading.config(text="")
            self.description.config(text="")
            return
        binding, texts = self.presentation(graph)
        self.heading.config(text=texts[binding["title"]])
        npc, level = self.rules.get(graph["mission"], (None, 1))
        giver = display(f"npc.{npc}") if npc else "nincs megadva"
        self.description.config(text=texts[binding["summary"]] + f"\nKüldetésadó: {giver} · Szint: {level}")
        labels = {b["node"]: texts[b["title"]] for b in binding["objectives"]}
        nodes = {n["id"]: n for n in graph["nodes"]}
        root = nodes[graph["root"]]
        order = root.get("children", []) if root["kind"] == "sequence" else []
        for key in dict.fromkeys([*order, *nodes]):
            n = nodes[key]
            if key == graph["root"] and order:
                continue
            obj = n.get("objective", {})
            event = obj.get("event", "")
            self.steps.insert("", "end", iid=key, text=labels.get(key, n["kind"]),
                              values=(EVENTS.get(event, (event, ""))[0], display(obj.get("subject", "")), obj.get("goal", "")))

    def changed(self, graph, affected=None):
        if self.original_package is not None:
            self.source["version"] = self.original_package + 1
        for changed_graph in affected or [graph]:
            if changed_graph["mission"] in self.original_versions:
                changed_graph["version"] = self.original_versions[changed_graph["mission"]] + 1
        index = self.source["missions"].index(graph)
        self.refresh(index)
        self.save_layout()

    def save_layout(self):
        try:
            self.save()
        except OSError as error:
            self.status.set("A helyi mentés sikertelen.")
            messagebox.showerror("Tervezet mentése",str(error),parent=self.root)
            return
        self.status.set(f"Tervezet mentve · {self.path}")

    def save(self):
        save_draft(self.path, {"mission_studio": 1, "source": self.source, "admissions": self.rules, "campaign": self.board})

    def admission(self):
        graph = self.current()
        if not graph:
            return
        npc, level = self.rules.get(graph["mission"], (None, 1))
        target = TargetPicker(self.root, "npc_interacted", f"npc.{npc}" if npc else "", wildcard=False).result
        if not target:
            return
        race = int(target.removeprefix("npc."))
        minimum = simpledialog.askinteger("Küldetésadó", "Szükséges karakterszint", initialvalue=level,
                                          minvalue=0, maxvalue=255, parent=self.root)
        if minimum is not None:
            self.rules[graph["mission"]] = [race, minimum]
            self.changed(graph)

    def add_mission(self):
        name = simpledialog.askstring("Új küldetés", "A játékosnak megjelenő név", parent=self.root)
        if not name or not name.strip():
            return
        summary = simpledialog.askstring("Új küldetés", "Leírás a missziónaplóhoz", parent=self.root)
        if summary and summary.strip():
            graph = new_mission(self.source, name.strip(), summary.strip())
            self.campaign.selected = graph["mission"]
            self.changed(graph)

    def edit_text(self):
        graph = self.current()
        if not graph:
            return
        b, texts = self.presentation(graph)
        title = simpledialog.askstring("Név és leírás", "Név", initialvalue=texts[b["title"]], parent=self.root)
        if not title or not title.strip():
            return
        info = simpledialog.askstring("Név és leírás", "Leírás", initialvalue=texts[b["summary"]], parent=self.root)
        if info and info.strip() and (title.strip(), info.strip()) != (texts[b["title"]], texts[b["summary"]]):
            edit_translation(self.source, b, "title", title.strip())
            edit_translation(self.source, b, "summary", info.strip())
            self.changed(graph)

    def add_step(self):
        graph = self.current()
        if not graph:
            return
        result = ObjectiveDialog(self.root, ("", "mob_killed", "", 1)).result
        if result:
            try:
                add_objective(self.source, graph, *result)
                self.changed(graph)
            except ValueError as error:
                messagebox.showerror("Új cél", str(error), parent=self.root)

    def edit_step(self):
        graph = self.current()
        selected = self.steps.selection()
        if not graph or not selected:
            return
        node = next(n for n in graph["nodes"] if n["id"] == selected[0])
        obj = node.get("objective", {})
        binding, texts = self.presentation(graph)
        label = next((b for b in binding["objectives"] if b["node"] == node["id"]), None)
        if not label or obj.get("event") not in EVENTS:
            messagebox.showinfo("Küldetéscél", "Ez a lépéstípus a szerkesztőben csak megtekinthető.", parent=self.root)
            return
        current = (texts[label["title"]], obj["event"], obj["subject"], obj["goal"])
        result = ObjectiveDialog(self.root, current).result
        if result and result != current:
            edit_translation(self.source, label, "title", result[0])
            obj["event"], obj["subject"], obj["goal"] = result[1:]
            self.changed(graph)

    def reward(self):
        graph = self.current()
        if not graph:
            return
        try:
            _, bundle = completion_reward(graph)
            values = []
            for key, label in (("experience", "Tapasztalati pont"), ("gold", "Yang")):
                value = simpledialog.askstring("Zárójutalom", label, parent=self.root,
                                               initialvalue=str((bundle or {}).get(key, 0)))
                if value is None:
                    return
                if not value.isascii() or not value.isdecimal():
                    raise ValueError("Egész darabszámot adj meg.")
                values.append(int(value))
            set_completion_reward(self.source, graph, *values)
            self.changed(graph)
        except ValueError as error:
            messagebox.showerror("Zárójutalom", str(error), parent=self.root)

    def build(self):
        if not self.source["missions"]:
            return
        compiler = self.get_compiler()
        if not compiler:
            return
        try:
            self.save()
            output = build_draft(compiler, self.source, self.rules, self.path.parent / "csomagok", self.board)
            self.status.set(f"Ellenőrzött helyi csomag · {output}")
            if messagebox.askyesno("A csomag elkészült", "Megnyitod az ellenőrzött helyi csomag mappáját?",
                                   parent=self.root):
                os.startfile(output)
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            messagebox.showerror("A csomagolás sikertelen", str(error), parent=self.root)


if __name__ == "__main__":
    window = tk.Tk()
    Studio(window, Path(os.environ.get("LOCALAPPDATA", Path.home())) / "Auriga" / "MissionStudio" / "Tervezetek")
    window.mainloop()
