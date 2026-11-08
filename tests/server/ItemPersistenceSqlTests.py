"""Exercise the native item writer in a new owned MariaDB, never a live DB."""
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
        if [int(path.name[:3]) for path in paths] != list(range(37)):
            raise RuntimeError("expected complete native catalog drafts 000..036")
        paths.append(ROOT / "db/migrations/item_instance/001_item_persistence.sql")
        paths.append(ROOT / "db/migrations/item_instance/002_instance_id_allocator.sql")
        paths.append(ROOT / "db/migrations/item_instance/003_owned_ground.sql")
        paths.append(ROOT / "db/migrations/item_instance/004_recovery_schedule.sql")
        paths.append(ROOT / "db/migrations/item_instance/005_automatic_recovery.sql")
        paths.append(ROOT / "db/migrations/item_instance/006_item_effects.sql")
        paths.append(ROOT / "db/migrations/item_instance/007_book_learning.sql")
        paths.append(ROOT / "db/migrations/item_instance/008_polymorph.sql")
        paths.append(ROOT / "db/migrations/item_instance/009_gathering_tools.sql")
        paths.append(ROOT / "db/migrations/item_instance/010_companion_seals.sql")
        paths.append(ROOT / "db/migrations/item_instance/011_growth_pet_birth.sql")
        paths.append(ROOT / "db/migrations/item_instance/012_pet_study_cooldown.sql")
        paths.append(ROOT / "db/migrations/item_instance/013_pet_supplies.sql")
        paths.append(ROOT / "db/migrations/item_instance/014_mount_riding.sql")
        paths.append(ROOT / "db/migrations/item_instance/015_companion_lifetime.sql")
        paths.append(ROOT / "db/migrations/item_instance/016_costume_lifetime.sql")
        paths.append(ROOT / "db/migrations/item_instance/017_regalia_special_bonuses.sql")
        paths.append(ROOT / "db/migrations/item_instance/018_soul_runtime.sql")
        paths.append(ROOT / "db/migrations/item_instance/019_ammo_runtime.sql")
        paths.append(ROOT / "db/migrations/item_instance/020_shared_storage.sql")
        paths.append(ROOT / "db/migrations/item_instance/021_mission_events.sql")
        sandbox.client.query("CREATE TABLE player.player(id INT UNSIGNED PRIMARY KEY, name VARCHAR(24) NOT NULL DEFAULT '', exp INT UNSIGNED NOT NULL DEFAULT 0, skill_point SMALLINT NOT NULL DEFAULT 0, sub_skill_point SMALLINT NOT NULL DEFAULT 0, horse_skill_point SMALLINT NOT NULL DEFAULT 0, skill_level BLOB NULL) ENGINE=InnoDB")
        sandbox.client.query("CREATE TABLE player.player_deleted LIKE player.player")
        sandbox.client.query("ALTER TABLE player.player ADD account_id INT UNSIGNED NOT NULL DEFAULT 0")
        sandbox.client.query("CREATE TABLE player.guild(id INT UNSIGNED PRIMARY KEY, master INT UNSIGNED NOT NULL) ENGINE=InnoDB")
        sandbox.client.query("CREATE TABLE player.guild_member(pid INT UNSIGNED PRIMARY KEY,guild_id INT UNSIGNED NOT NULL,grade TINYINT UNSIGNED NOT NULL) ENGINE=InnoDB")
        for path in paths:
            for statement in statements(path.read_text(encoding="utf-8")):
                try:
                    sandbox.client.query(statement)
                except Exception:
                    if path.name=='013_pet_supplies.sql':
                        print('PET_STORAGE_MIGRATION_DDL',sandbox.client.query('SHOW CREATE TABLE player.item_storage'),flush=True)
                        print('PET_STORAGE_MIGRATION_PLAN',sandbox.client.query('SELECT @pet_storage_ddl'),flush=True)
                    raise
        sandbox.client.query("CREATE DATABASE mission CHARACTER SET utf8mb4 COLLATE utf8mb4_bin")
        sandbox.client.query("USE mission")
        for statement in statements((ROOT/'db/mission_schema.sql').read_text(encoding='utf-8')):
            sandbox.client.query(statement)
        sandbox.client.query("USE player")
        sandbox.client.query("CREATE TABLE protos.ic_persistence_owner(token CHAR(32) NOT NULL) ENGINE=InnoDB")
        sandbox.client.query("INSERT INTO protos.ic_persistence_owner VALUES('" + token + "')")
        sandbox.client.query("CREATE USER 'item_writer'@'localhost' IDENTIFIED BY '" + sandbox.password + "'")
        sandbox.client.query("GRANT SELECT ON protos.* TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT SELECT ON player.* TO 'item_writer'@'localhost'")
        for table in ("item_wallet", "item_operation_receipt", "item_instance",
                      "item_instance_attribute", "item_instance_socket", "item_occupied_cell", "item_ground_core", "item_ground_entry", "item_recovery_schedule"):
            sandbox.client.query("GRANT INSERT,UPDATE,DELETE ON player." + table + " TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT INSERT,UPDATE,DELETE ON player.item_book_learning TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT INSERT ON player.item_ammo_shot_event TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT INSERT ON player.item_soul_strike_event TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT INSERT,UPDATE ON player.item_pet_study_cooldown TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT INSERT,DELETE ON player.item_storage TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT INSERT,UPDATE ON player.item_owner_fence TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT UPDATE ON player.player TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT INSERT,UPDATE ON player.item_shared_storage_gate TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT UPDATE ON player.item_instance_id_allocator TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT ALL ON mission.* TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT INSERT,UPDATE,DELETE ON player.item_mission_event TO 'item_writer'@'localhost'")
        sandbox.client.query("GRANT INSERT,DELETE ON player.item_mission_watch TO 'item_writer'@'localhost'")
        metadata = {"version": sandbox.client.query("SELECT VERSION()")[0][0],
                    "owned_datadir_verified": True, "catalog_select_only": True,
                    "catalog_drafts": sum(path.parent.name=='item_combat' for path in paths), "instance_schema": 1}
        (sandbox.work / "environment.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        result = subprocess.run([str(args.test_executable.resolve()), "--owned-sandbox"],
                                input=f"{sandbox.port}\n{sandbox.password}\n{token}\n",
                                text=True, capture_output=True, timeout=120)
        output = (result.stdout + result.stderr).replace(sandbox.password, "[redacted]")
        (sandbox.work / "native-test.log").write_text(output, encoding="utf-8")
        print(output, end="")
        if result.returncode:
            raise RuntimeError(f"native item persistence tests failed (exit {result.returncode}, 0x{result.returncode & 0xffffffff:08x})")
        sandbox.verify()
    finally:
        sandbox.stop()
        print("Owned item persistence SQL logs: " + str(sandbox.work))


if __name__ == "__main__":
    main()
