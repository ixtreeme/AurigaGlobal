# Nexoria UI source import

`ExportNexoriaSettings.py` imports the actual settings design from
`D:/Nexoria UI/Beallitasok`. It reads the PSD layer hierarchy and the `TABS`
data in its accompanying `scripts/nx_build_beallitasok.jsx`; it does not run the
authoring script or derive the layout from old client prefabs.

Install `psd-tools` and Pillow in the Python environment. With the prepared
Unity UI resource folder present, run:

```powershell
py -X utf8 tools/ui/ExportNexoriaSettings.py
```

Photoshop renders the isolated layers from a temporary document duplicate,
including their original effects. Source documents are never saved. Existing
Unity texture GUIDs are preserved, and unexpected edits to a previous export
stop the import. `settings-source.json` records the document/script hashes,
all 45 row positions, raster bounds and the hashes of the exported art.

If Photoshop's COM server is unavailable, use a fresh job directory:

```powershell
py -X utf8 tools/ui/ExportNexoriaSettings.py --script-job build/settings-export-job
```

When `ready.txt` appears, select that directory's `export.jsx` through
Photoshop's **File → Scripts → Browse**. The importer waits for the native
export, then crops and installs its outputs. `progress.txt` and `error.txt`
describe the native job. A blocking Photoshop dialog must be resolved before
starting the export.

`--raster-preview-only` is diagnostic: the Python compositor does not preserve
all Photoshop effects. The Unity authored-settings validator rejects that
fallback for delivery.

The other importers in this directory cover the prepared taskbar and catalogue
profiles. A catalogue profile alone does not establish that an old window has
been rebuilt to match its source PSD; each window still needs visual review and
real control bindings.

`ExportNexoriaItemWindows.py` prepares the authored NPC shop, player trade and
ESC menu from their individual PSDs and authoring scripts. It exports the clean
canvas, isolated action buttons and category states with Photoshop, removes
sample character/item/economy data, and records the original source hashes and
control rectangles. Use `--cache build/c40-native-cache` to retain completed
exports when Photoshop is interrupted. Installation happens only after all
three documents have rendered. This exporter performs no version-control
operation.

`ExportNexoriaInventoryStorage.py` exports the original inventory and storage
canvases, category controls, cells and footer actions. It removes demonstration
items, quantities, money, weight and the unimplemented password-protection
claim. `--cache build/c41-native-cache` keeps completed native exports. Both
item-window exporters preserve the other registered windows in the shared
manifest and retain existing texture GUIDs. Neither exporter publishes assets
or performs a version-control operation.

`ExportNexoriaDungeons.py` imports the original `Kazamata`, `Kazamata_Eredmeny`
and `Kazamata_Ranglista` PSDs. It exports 48 assets through Photoshop, removes
demonstration values and keeps the existing shared manifest entries and texture
GUIDs. Only the Kazamata leaderboard page is imported. Small PSD captions become
live text through `NexoriaLiveText.py`, so the compact windows remain readable.
The runtime bindings and local server/client verification are recorded in
`docs/reports/ui/nexoria_dungeon.evidence.json`.

`ExportNexoriaDeathRecap.py` imports `Halal_Osszesito` with Photoshop and exports
25 original assets. It excludes the preview backdrop, sample enemies/damage,
unconfigured item costs, countdown and sample advice. Runtime labels use the
server's bounded ten-second HP-loss journal and the existing native restart
actions. Evidence is in `docs/reports/ui/nexoria_death_recap.evidence.json`.
