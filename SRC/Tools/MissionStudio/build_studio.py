"""Package the standalone Windows editor with the same compiler as the server."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from target_names import make_index


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--catalog", required=True, type=Path, help="Published CatalogEditorTool read response")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve(strict=True)
    crypto = compiler.with_name("libcrypto-3-x64.dll").resolve(strict=True)
    output = args.output.resolve()
    if (output / "MissionStudio").exists():
        parser.error("Choose a new output directory to preserve the existing release.")
    with tempfile.TemporaryDirectory() as work:
        labels = Path(__file__).resolve().parents[3] / "config/server_manager/content_names_hu.json"
        index = Path(work) / "mission_targets.json"
        index.write_text(json.dumps(make_index(json.loads(labels.read_bytes()), json.loads(args.catalog.read_bytes())),
                                    ensure_ascii=False), encoding="utf-8")
        subprocess.run([sys.executable, "-m", "PyInstaller", "--windowed", "--onedir",
                        "--name", "MissionStudio", "--distpath", str(output),
                        "--workpath", str(Path(work)/"build"), "--specpath", work,
                        "--add-binary", f"{compiler}:.", "--add-binary", f"{crypto}:.", "--add-data", f"{index}:.",
                        "--add-data", f"{Path(__file__).with_name('art').resolve()}:art",
                        str(Path(__file__).with_name("studio.pyw").resolve())], check=True)


if __name__ == "__main__":
    main()
