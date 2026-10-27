"""Run the native catalog loader only inside a newly spawned private MariaDB."""
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
        migrations = sorted((ROOT / "db/migrations/item_combat").glob("[0-9][0-9][0-9]_*.sql"))
        if [int(path.name[:3]) for path in migrations] != list(range(13)):
            raise RuntimeError("the native fixture requires schema drafts 000 through 012")
        fixtures = [ROOT / "tests/fixtures/item_combat/v5/recovery_consumables.sql", ROOT / "tests/fixtures/item_combat/v6/containers.sql"]
        for path in [*migrations, *fixtures]:
            for statement in statements(path.read_text(encoding="utf-8")):
                sandbox.client.query(statement)
        sandbox.client.query("CREATE TABLE protos.ic_loader_owner(token CHAR(32) NOT NULL) ENGINE=InnoDB")
        sandbox.client.query("INSERT INTO protos.ic_loader_owner VALUES('" + token + "')")
        sandbox.client.query("CREATE USER 'catalog_reader'@'localhost' IDENTIFIED BY '" + sandbox.password + "'")
        sandbox.client.query("GRANT SELECT ON protos.* TO 'catalog_reader'@'localhost'")
        metadata = {"version": sandbox.client.query("SELECT VERSION()")[0][0],
                    "isolation": sandbox.client.query("SELECT @@tx_isolation")[0][0],
                    "owned_datadir_verified": True, "read_only_loader_account": True,
                    "fixture": "v5/recovery_consumables.sql + v6/containers.sql", "schema_drafts": len(migrations)}
        (sandbox.work / "environment.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        result = subprocess.run([str(args.test_executable.resolve()), "--owned-sandbox"],
                                input=f"{sandbox.port}\n{sandbox.password}\n{token}\n",
                                text=True, capture_output=True, timeout=90)
        output = result.stdout + result.stderr
        output = output.replace(sandbox.password, "[redacted]")
        (sandbox.work / "native-test.log").write_text(output, encoding="utf-8")
        print(output, end="")
        if result.returncode:
            raise RuntimeError("native catalogue loader test failed")
        sandbox.verify()
    finally:
        sandbox.stop()
        print("Owned catalogue SQL logs: " + str(sandbox.work))


if __name__ == "__main__":
    main()
