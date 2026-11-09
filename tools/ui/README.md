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
