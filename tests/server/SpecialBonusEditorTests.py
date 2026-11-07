"""Round-trip the three regalia forms and six service forms through Qt and native validation."""
from pathlib import Path
import argparse
import os
import sys

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'SRC/Tools/ServerManager'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog-tool', type=Path, required=True)
    args = parser.parse_args()
    os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
    from PySide6.QtWidgets import QApplication
    import item_catalog
    from special_bonus_editor import SpecialBonusPanel
    app = QApplication.instance() or QApplication([])
    tool = args.catalog_tool.resolve()
    blank = item_catalog.run_tool(dict(action='blank'), executable=tool)
    checks = 0
    for kind, slot in [(1, 0), (2, 1), (2, 7)]:
        editor = item_catalog.Catalog(blank)
        doc = editor.new(19)
        panel = SpecialBonusPanel(lambda: None)
        panel.load(doc, blank['bonuses'])
        panel.kind.setCurrentIndex(kind - 1)
        panel.first.setValue(slot or 1)
        panel.seed_rows()
        panel.collect(doc)
        tables = editor.apply(doc)
        item_catalog.run_tool(dict(action='validate', tables=tables), blank['columns'], tool)
        got = editor.item((19, 1))
        assert got['child'] == doc['child'] and got['regalia_rules'] == doc['regalia_rules']
        panel.load(got, blank['bonuses'])
        panel.collect(got)
        assert got['child'] == doc['child'] and got['regalia_rules'] == doc['regalia_rules']
        checks += 1
        panel.deleteLater()
    for operation in range(1, 7):
        editor = item_catalog.Catalog(blank)
        doc = editor.new(20)
        panel = SpecialBonusPanel(lambda: None, tools=True)
        panel.load(doc, blank['bonuses'])
        panel.kind.setCurrentIndex(operation - 1)
        if operation == 2:
            panel.capacity.setText('75')
        if operation == 3:
            panel.seed_rows()
        panel.collect(doc)
        tables = editor.apply(doc)
        item_catalog.run_tool(dict(action='validate', tables=tables), blank['columns'], tool)
        got = editor.item((20, 1))
        assert got['child'] == doc['child'] and got['special_tiers'] == doc['special_tiers']
        panel.load(got, blank['bonuses'])
        panel.collect(got)
        assert got['child'] == doc['child'] and got['special_tiers'] == doc['special_tiers']
        checks += 1
        panel.deleteLater()
    print('SPECIAL_BONUS_EDITOR=SUCCESS cases=' + str(checks))


if __name__ == '__main__':
    main()
