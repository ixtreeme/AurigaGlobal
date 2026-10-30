"""F4.1: GameServer startup catalog against a new owned MariaDB, never a live DB."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "db/tools"))
from backup_sandbox import Sandbox
from mariadb_client import statements


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mariadb-bin", type=Path, required=True)
    parser.add_argument("--test-executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    token = uuid.uuid4().hex
    sandbox = Sandbox(args.work_dir / token, args.mariadb_bin)
    try:
        sandbox.start()
        sandbox.verify()
        sandbox.client.query("CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin")
        paths = sorted((ROOT / "db/migrations/item_combat").glob("[0-9][0-9][0-9]_*.sql"))
        if [int(path.name[:3]) for path in paths] != list(range(31)):
            raise RuntimeError("expected complete native catalog drafts 000..030")
        paths.append(ROOT / "db/migrations/item_instance/001_item_persistence.sql")
        for path in paths:
            for statement in statements(path.read_text(encoding="utf-8")):
                sandbox.client.query(statement)
        sandbox.client.query("CREATE TABLE protos.ic_runtime_owner(token CHAR(32) NOT NULL) ENGINE=InnoDB")
        sandbox.client.query("INSERT INTO protos.ic_runtime_owner VALUES('" + token + "')")
        # The catalog account only reads protos; the pin is read on the player connection.
        sandbox.client.query("CREATE USER 'catalog_reader'@'localhost' IDENTIFIED BY '" + sandbox.password + "'")
        sandbox.client.query("GRANT SELECT ON protos.* TO 'catalog_reader'@'localhost'")
        sandbox.client.query("CREATE USER 'item_runtime'@'localhost' IDENTIFIED BY '" + sandbox.password + "'")
        sandbox.client.query("GRANT SELECT ON player.* TO 'item_runtime'@'localhost'")
        metadata = {"version": sandbox.client.query("SELECT VERSION()")[0][0],
                    "owned_datadir_verified": True, "catalog_select_only": True,
                    "catalog_drafts": 31, "instance_schema": 1}
        (sandbox.work / "environment.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        result = subprocess.run([str(args.test_executable.resolve()), "--owned-sandbox"],
                                input=f"{sandbox.port}\n{sandbox.password}\n{token}\n",
                                text=True, capture_output=True, timeout=120)
        output = (result.stdout + result.stderr).replace(sandbox.password, "[redacted]")
        (sandbox.work / "native-test.log").write_text(output, encoding="utf-8")
        print(output, end="")
        if result.returncode:
            raise RuntimeError("native item catalog runtime tests failed")
        sandbox.verify()
    finally:
        sandbox.stop()
        print("Owned item catalog runtime SQL logs: " + str(sandbox.work))


if __name__ == "__main__":
    main()
