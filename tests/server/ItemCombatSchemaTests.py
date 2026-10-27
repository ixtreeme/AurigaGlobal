"""Exercise the new category schema only in a newly spawned owned MariaDB.

No endpoint, password or existing datadir can be supplied. Reuses the repository
sandbox/client; this does not implement another runtime SQL connection layer.
The ic_audit_* tables below are test fixtures, not the final item schema.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import sys
import threading
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "db/tools"))
from backup_sandbox import Sandbox
from mariadb_client import Client, statements


class CategorySchemaTests(unittest.TestCase):
    binary_dir = None
    work_dir = None

    @classmethod
    def setUpClass(cls):
        cls.sandbox = Sandbox(cls.work_dir / uuid.uuid4().hex, cls.binary_dir)
        try:
            cls.sandbox.start()
            cls.sandbox.verify()
            cls.connection = cls.sandbox.client
            for name in ("000_protos_database.sql", "001_category_registry.sql", "002_category_seed.sql",
                         "003_equipment_profiles.sql", "004_refinement_profiles.sql", "005_weapon_armor_catalog.sql",
                         "006_bonus_profiles.sql", "007_bonus_registry_seed.sql", "008_accessory_material_catalog.sql",
                         "009_accessory_socket_profiles.sql", "010_recovery_consumables.sql", "011_catalog_format.sql", "012_containers.sql",
                         "013_currency_tokens.sql"):
                text = (ROOT / "db/migrations/item_combat" / name).read_text(encoding="utf-8")
                for statement in statements(text):
                    cls.connection.query(statement)
            for fixture_path in ("v5/recovery_consumables.sql", "v6/containers.sql", "v7/currency_tokens.sql"):
                fixture = (ROOT / "tests/fixtures/item_combat" / fixture_path).read_text(encoding="utf-8")
                for statement in statements(fixture):
                    cls.connection.query(statement)
            # New fixture schemas/tables in this exact private DB process.
            cls.connection.query("CREATE DATABASE player CHARACTER SET utf8mb4 COLLATE utf8mb4_bin")
            cls.connection.query("""CREATE TABLE protos.ic_audit_key (
                category_id SMALLINT UNSIGNED NOT NULL,
                local_id INT UNSIGNED NOT NULL,
                PRIMARY KEY(category_id, local_id),
                FOREIGN KEY(category_id) REFERENCES protos.item_category(category_id),
                CHECK(local_id > 0)) ENGINE=InnoDB""")
            cls.connection.query("""CREATE TABLE player.ic_audit_instance (
                instance_id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
                category_id SMALLINT UNSIGNED NOT NULL,
                local_id INT UNSIGNED NOT NULL,
                FOREIGN KEY(category_id, local_id) REFERENCES protos.ic_audit_key(category_id, local_id),
                CHECK(instance_id > 0)) ENGINE=InnoDB""")
            initial = cls.connection.query("SELECT category_id,next_local_id FROM protos.item_local_id_sequence ORDER BY category_id")
            if initial != [[str(i), "1"] for i in range(1, 19)]:
                raise RuntimeError("fresh category sequences do not start at one independently")
            cls.metadata = {
                "version": cls.connection.query("SELECT VERSION()")[0][0],
                "session_sql_mode": cls.connection.query("SELECT @@SESSION.sql_mode")[0][0],
                "global_sql_mode": cls.connection.query("SELECT @@GLOBAL.sql_mode")[0][0],
                "database_path_verified": True,
                "fresh_sequences": initial,
                "scope": "staged seven-category catalog with recovery consumables, accessory socket profiles, containers and keys; not full P1/P2",
            }
            (cls.sandbox.work / "item-combat-environment.json").write_text(
                json.dumps(cls.metadata, indent=2) + "\n", encoding="utf-8")
        except BaseException:
            cls.sandbox.stop()
            raise

    @classmethod
    def tearDownClass(cls):
        cls.sandbox.stop()
        print("Owned item/combat SQL logs: " + str(cls.sandbox.work))

    def setUp(self):
        self.sandbox.verify()

    def q(self, sql):
        return self.connection.query(sql)

    def new_client(self):
        return Client(self.sandbox.port, library=str(self.sandbox.binary_dir.parent / "lib/libmariadb.dll"),
                      password=self.sandbox.password)

    def test_catalog_registry_constraints_and_dialect(self):
        self.assertIn("MariaDB", self.metadata["version"])
        self.assertIn("STRICT_ALL_TABLES", self.metadata["session_sql_mode"])
        actual = self.q("SELECT category_id,code,prototype_table FROM protos.item_category ORDER BY category_id")
        registry = json.loads((ROOT / "config/item_combat/category_registry.json").read_text(encoding="utf-8"))
        self.assertEqual(actual, [[str(row["id"]), row["code"], row["table"]] for row in registry["categories"]])
        engines = self.q("""SELECT ENGINE FROM information_schema.TABLES
            WHERE TABLE_SCHEMA='protos' AND TABLE_NAME IN ('item_category','item_local_id_sequence')""")
        self.assertEqual(engines, [["InnoDB"], ["InnoDB"]])
        for sql, code in [
            ("INSERT INTO protos.item_category VALUES(0,'invalid','item_invalid')", "4025"),
            ("INSERT INTO protos.item_category VALUES(1,'duplicate','item_duplicate')", "1062"),
            ("INSERT INTO protos.item_local_id_sequence VALUES(65000,1)", "1452"),
            ("UPDATE protos.item_local_id_sequence SET next_local_id=0 WHERE category_id=10", "4025"),
            ("UPDATE protos.item_local_id_sequence SET next_local_id=4294967297 WHERE category_id=10", "4025"),
        ]:
            with self.subTest(code=code, sql=sql), self.assertRaisesRegex(RuntimeError, "^" + code + ":"):
                self.q(sql)

    def test_concurrent_reservations_use_category_row_lock(self):
        barrier = threading.Barrier(8, timeout=10)

        def publish_one(_):
            client = self.new_client()
            try:
                client.query("SET SESSION innodb_lock_wait_timeout=5")
                barrier.wait()
                client.query("START TRANSACTION")
                allocated = int(client.query("SELECT next_local_id FROM protos.item_local_id_sequence WHERE category_id=1 FOR UPDATE")[0][0])
                self.assertLessEqual(allocated, 4294967295)
                # Fixture publishing transaction: reserve and publish together.
                client.query("UPDATE protos.item_local_id_sequence SET next_local_id=" + str(allocated + 1) + " WHERE category_id=1")
                client.query("INSERT INTO protos.ic_audit_key VALUES(1," + str(allocated) + ")")
                client.query("COMMIT")
                return allocated
            except BaseException:
                client.query("ROLLBACK")
                raise
            finally:
                client.close()

        with ThreadPoolExecutor(max_workers=8) as pool:
            allocated = list(pool.map(publish_one, range(8)))
        self.assertEqual(sorted(allocated), list(range(1, 9)))
        self.assertEqual(self.q("SELECT next_local_id FROM protos.item_local_id_sequence WHERE category_id=1"), [["9"]])
        self.assertEqual(self.q("SELECT next_local_id FROM protos.item_local_id_sequence WHERE category_id=2"), [["1"]])
        self.q("DELETE FROM protos.ic_audit_key WHERE category_id=1 AND local_id=1")
        self.assertEqual(self.q("SELECT next_local_id FROM protos.item_local_id_sequence WHERE category_id=1"), [["9"]])
        # A rolled-back, unpublished reservation does not consume a published ID.
        self.q("START TRANSACTION")
        self.q("SELECT next_local_id FROM protos.item_local_id_sequence WHERE category_id=2 FOR UPDATE")
        self.q("UPDATE protos.item_local_id_sequence SET next_local_id=2 WHERE category_id=2")
        self.q("ROLLBACK")
        self.assertEqual(self.q("SELECT next_local_id FROM protos.item_local_id_sequence WHERE category_id=2"), [["1"]])

    def test_cross_database_fk_prepared_commit_and_rollback(self):
        self.assertEqual(self.connection.prepare("player", "SELECT next_local_id FROM protos.item_local_id_sequence WHERE category_id=? FOR UPDATE"), (1, 1))
        self.assertEqual(self.connection.prepare("player", "INSERT INTO protos.ic_audit_key VALUES(?,?)"), (0, 2))
        self.assertEqual(self.connection.prepare("protos", "INSERT INTO player.ic_audit_instance VALUES(?,?,?)"), (0, 3))
        self.q("PREPARE ic_parent FROM 'INSERT INTO protos.ic_audit_key VALUES(?,?)'")
        self.q("PREPARE ic_child FROM 'INSERT INTO player.ic_audit_instance VALUES(?,?,?)'")
        try:
            self.q("SET @category=2,@local=7001,@instance=18446744073709551615")
            self.q("START TRANSACTION")
            self.q("EXECUTE ic_parent USING @category,@local")
            self.q("EXECUTE ic_child USING @instance,@category,@local")
            self.q("ROLLBACK")
            self.assertEqual(self.q("SELECT COUNT(*) FROM protos.ic_audit_key WHERE local_id=7001"), [["0"]])
            self.assertEqual(self.q("SELECT COUNT(*) FROM player.ic_audit_instance"), [["0"]])
            self.q("START TRANSACTION")
            self.q("EXECUTE ic_parent USING @category,@local")
            self.q("EXECUTE ic_child USING @instance,@category,@local")
            self.q("COMMIT")
            observer = self.new_client()
            try:
                self.assertEqual(observer.query("SELECT category_id,local_id FROM player.ic_audit_instance"), [["2", "7001"]])
                self.assertEqual(observer.query("SELECT COUNT(*) FROM protos.ic_audit_key WHERE category_id=2 AND local_id=7001"), [["1"]])
            finally:
                observer.close()
            with self.assertRaisesRegex(RuntimeError, "^1452:"):
                self.q("INSERT INTO player.ic_audit_instance VALUES(9002,2,7002)")
            with self.assertRaisesRegex(RuntimeError, "^1451:"):
                self.q("DELETE FROM protos.ic_audit_key WHERE category_id=2 AND local_id=7001")
            self.q("START TRANSACTION")
            self.q("INSERT INTO protos.ic_audit_key VALUES(2,7002)")
            with self.assertRaisesRegex(RuntimeError, "^1452:"):
                self.q("INSERT INTO player.ic_audit_instance VALUES(9002,1,7002)")
            self.q("ROLLBACK")
            self.assertEqual(self.q("SELECT COUNT(*) FROM protos.ic_audit_key WHERE local_id=7002"), [["0"]])
        finally:
            self.q("ROLLBACK")
            self.q("DEALLOCATE PREPARE ic_parent")
            self.q("DEALLOCATE PREPARE ic_child")

    def test_runtime_catalog_is_read_only(self):
        self.q("CREATE USER 'catalog_runtime'@'127.0.0.1' IDENTIFIED BY 'isolated_fixture_only'")
        self.q("GRANT SELECT ON protos.* TO 'catalog_runtime'@'127.0.0.1'")
        reader = Client(self.sandbox.port, library=str(self.sandbox.binary_dir.parent / "lib/libmariadb.dll"),
                        user="catalog_runtime", password="isolated_fixture_only")
        try:
            self.assertEqual(reader.query("SELECT COUNT(*) FROM protos.item_category"), [["18"]])
            self.assertEqual(reader.query("SELECT COUNT(*) FROM protos.item_base"), [["15"]])
            for sql in ["UPDATE protos.item_category SET code='edited' WHERE category_id=1",
                        "DELETE FROM protos.item_category WHERE category_id=1",
                        "UPDATE protos.item_local_id_sequence SET next_local_id=30 WHERE category_id=1",
                        "UPDATE protos.item_weapon SET physical_attack_base_raw=0 WHERE local_id=1",
                        "UPDATE protos.bonus_profile_entry SET value_scaled=0 WHERE profile_id=1",
                        "UPDATE protos.bonus_definition SET unit_kind=2 WHERE bonus_id=1",
                        "UPDATE protos.item_accessory SET equip_slot=7 WHERE local_id=1",
                        "UPDATE protos.item_material SET material_kind=2 WHERE local_id=1",
                        "UPDATE protos.accessory_socket_grade SET duration_ms=1000 WHERE profile_id=1 AND grade=1",
                        "DELETE FROM protos.accessory_socket_material WHERE profile_id=1",
                        "UPDATE protos.instant_recovery_profile SET hp_raw=1 WHERE profile_id=1",
                        "DELETE FROM protos.consumable_cooldown_profile WHERE profile_id=1"]:
                with self.subTest(sql=sql), self.assertRaisesRegex(RuntimeError, "^1142:"):
                    reader.query(sql)
        finally:
            reader.close()

    def test_sequence_exhaustion_and_seed_reapply(self):
        self.q("START TRANSACTION")
        self.q("UPDATE protos.item_local_id_sequence SET next_local_id=4294967295 WHERE category_id=18")
        last = int(self.q("SELECT next_local_id FROM protos.item_local_id_sequence WHERE category_id=18 FOR UPDATE")[0][0])
        self.assertEqual(last, 4294967295)
        self.q("INSERT INTO protos.ic_audit_key VALUES(18,4294967295)")
        self.q("UPDATE protos.item_local_id_sequence SET next_local_id=4294967296 WHERE category_id=18")
        self.q("COMMIT")
        self.assertEqual(self.q("SELECT next_local_id FROM protos.item_local_id_sequence WHERE category_id=18"), [["4294967296"]])
        # This sentinel must be rejected by the future publisher, not cast to u32.
        with self.assertRaisesRegex(RuntimeError, "^4025:"):
            self.q("UPDATE protos.item_local_id_sequence SET next_local_id=4294967297 WHERE category_id=18")
        seed = (ROOT / "db/migrations/item_combat/002_category_seed.sql").read_text(encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "^1062:"):
            try:
                for statement in statements(seed):
                    self.q(statement)
            finally:
                self.q("ROLLBACK")
        self.assertEqual(self.q("SELECT next_local_id FROM protos.item_local_id_sequence WHERE category_id=18"), [["4294967296"]])

    def test_typed_equipment_and_refinement_golden(self):
        self.assertEqual(self.q("SELECT category_id,local_id FROM protos.item_base ORDER BY category_id,local_id"),
                         [["1","1"],["2","1"],["3","1"],["5","1"],["5","2"],["5","3"],["6","1"],["6","2"],["6","3"],
                          ["8","1"],["8","2"],["8","3"],["16","1"],["16","2"],["18","1"]])
        self.assertEqual(self.q("""SELECT w.physical_attack_base_raw+r.physical_attack_gain_raw,
            w.magical_attack_base_raw+r.magical_attack_gain_raw FROM protos.item_weapon w
            JOIN protos.item_base b ON b.category_id=w.category_id AND b.local_id=w.local_id
            JOIN protos.refinement_level r ON r.profile_id=b.refinement_profile_id
            WHERE w.category_id=1 AND w.local_id=1 AND r.level=3"""), [["150000", "42000"]])
        self.assertEqual(self.q("""SELECT a.physical_defense_base_raw+r.physical_defense_gain_raw,
            a.magical_defense_base_raw+r.magical_defense_gain_raw FROM protos.item_armor a
            JOIN protos.item_base b ON b.category_id=a.category_id AND b.local_id=a.local_id
            JOIN protos.refinement_level r ON r.profile_id=b.refinement_profile_id
            WHERE a.category_id=2 AND a.local_id=1 AND r.level=2"""), [["50000", "24000"]])

    def test_typed_constraints_and_profile_references(self):
        for sql, code in [
            ("UPDATE protos.item_base SET max_stack=2 WHERE category_id=1", "4025"),
            ("UPDATE protos.item_base SET base_bonus_profile_id=999 WHERE category_id=1", "1452"),
            ("UPDATE protos.item_base SET refinement_profile_id=999 WHERE category_id=1", "1452"),
            ("UPDATE protos.item_base SET appearance_profile_id=999 WHERE category_id=1", "1452"),
            ("UPDATE protos.item_weapon SET attack_profile_id=999", "1452"),
            ("UPDATE protos.item_weapon SET physical_attack_base_raw=-1", "4025"),
            ("UPDATE protos.item_weapon SET physical_attack_base_raw=1000000000000001", "4025"),
            ("UPDATE protos.item_armor SET equip_slot=1", "4025"),
            ("UPDATE protos.refinement_level SET physical_attack_gain_raw=1 WHERE profile_id=2 AND level=0", "4025"),
            ("UPDATE protos.refinement_profile SET max_level=256 WHERE profile_id=2", "4025"),
            ("UPDATE protos.item_use_policy SET allowed_classes=0", "4025"),
            ("UPDATE protos.item_resistance_profile SET fire_bp=10001", "4025"),
            ("UPDATE protos.item_ammo_policy SET units_per_hit=1 WHERE ammo_kind=1", "4025"),
            ("DELETE FROM protos.refinement_profile WHERE profile_id=2", "1451"),
        ]:
            with self.subTest(sql=sql), self.assertRaisesRegex(RuntimeError, "^" + code + ":"):
                self.q(sql)

    def test_bonus_registry_and_current_level_profiles(self):
        self.assertEqual(self.q("SELECT COUNT(*) FROM protos.bonus_definition"), [["25"]])
        self.assertEqual(self.q("""SELECT bonus_id,unit_kind,stage_kind,channel_filter,action_filter,mode_filter,
            minimum_value,maximum_value FROM protos.bonus_definition WHERE bonus_id IN (1,7,9,17,24) ORDER BY bonus_id"""), [
                ["1","1","1","2","1","1","-1000000000000000","1000000000000000"],
                ["7","2","3","1","2","1","-100000","100000"],
                ["9","2","4","1","1","2","-100000","100000"],
                ["17","2","5","2","1","1","0","10000"],
                ["24","2","10","2","1","1","0","10000"]])
        self.assertEqual(self.q("SELECT profile_id,source_kind FROM protos.bonus_profile ORDER BY profile_id"),
                         [["1","1"],["2","2"],["3","2"],["4","2"],["5","2"],["6","1"]])
        self.assertEqual(self.q("""SELECT r.level,e.bonus_id,e.value_scaled FROM protos.refinement_level r
            JOIN protos.bonus_profile_entry e ON e.profile_id=r.extra_bonus_profile_id
            WHERE r.profile_id=2 AND r.level IN (3,9) ORDER BY r.level,e.bonus_id"""),
            [["3","1","5000"],["3","21","100"],["9","21","200"]])
        self.assertEqual(self.q("""SELECT COUNT(*) FROM protos.refinement_level
            WHERE profile_id=2 AND level=4 AND extra_bonus_profile_id IS NULL"""), [["1"]])
        # Independent raw SQL shape; no aggregate policy is applied by the DB.
        self.assertEqual(self.q("SELECT bonus_id,value_scaled FROM protos.bonus_profile_entry WHERE profile_id=1 ORDER BY bonus_id"),
                         [["1","2500"],["7","500"]])

    def test_bonus_constraints_and_seed_reapply(self):
        for sql, code in [
            ("INSERT INTO protos.bonus_profile_entry VALUES(1,999,1)", "1452"),
            ("INSERT INTO protos.bonus_profile_entry VALUES(999,1,1)", "1452"),
            ("INSERT INTO protos.bonus_profile_entry VALUES(1,1,1)", "1062"),
            ("UPDATE protos.bonus_profile_entry SET value_scaled=9223372036854775807 WHERE profile_id=1", "4025"),
            ("UPDATE protos.bonus_profile SET source_kind=0 WHERE profile_id=1", "4025"),
            ("UPDATE protos.bonus_definition SET unit_kind=4 WHERE bonus_id=1", "4025"),
            ("UPDATE protos.bonus_definition SET family_filter=1 WHERE bonus_id=1", "4025"),
            ("UPDATE protos.bonus_definition SET allowed_sources=4 WHERE bonus_id=1", "4025"),
            ("UPDATE protos.bonus_definition SET minimum_value=1,maximum_value=0 WHERE bonus_id=1", "4025"),
            ("DELETE FROM protos.bonus_definition WHERE bonus_id=1", "1451"),
            ("UPDATE protos.refinement_level SET extra_bonus_profile_id=0 WHERE profile_id=2 AND level=3", "1452"),
        ]:
            with self.subTest(sql=sql), self.assertRaisesRegex(RuntimeError, "^" + code + ":"):
                self.q(sql)
        seed = (ROOT / "db/migrations/item_combat/007_bonus_registry_seed.sql").read_text(encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "^1062:"):
            try:
                for statement in statements(seed):
                    self.q(statement)
            finally:
                self.q("ROLLBACK")
        self.assertEqual(self.q("SELECT value_scaled FROM protos.bonus_profile_entry WHERE profile_id=1 AND bonus_id=1"), [["2500"]])

    def test_refinement_above_nine_keeps_single_prototype(self):
        self.q("START TRANSACTION")
        try:
            self.q("INSERT INTO protos.refinement_profile VALUES(40,'refine.extended_weapon',2,30)")
            for level in range(31):
                self.q(f"""INSERT INTO protos.refinement_level
                    (profile_id,level,physical_attack_gain_raw,magical_attack_gain_raw)
                    VALUES(40,{level},{10000*level},{4000*level})""")
            self.q("UPDATE protos.item_base SET refinement_profile_id=40 WHERE category_id=1 AND local_id=1")
            self.assertEqual(self.q("SELECT COUNT(*) FROM protos.item_base WHERE category_id=1"), [["1"]])
            self.assertEqual(self.q("""SELECT r.level,w.physical_attack_base_raw+r.physical_attack_gain_raw,
                w.magical_attack_base_raw+r.magical_attack_gain_raw FROM protos.item_weapon w
                JOIN protos.item_base b ON b.category_id=w.category_id AND b.local_id=w.local_id
                JOIN protos.refinement_profile p ON p.profile_id=b.refinement_profile_id
                JOIN protos.refinement_level r ON r.profile_id=p.profile_id
                WHERE w.category_id=1 AND w.local_id=1 AND r.level IN (9,10,15,30)
                  AND r.level<=p.max_level ORDER BY r.level"""),
                [["9","210000","66000"],["10","220000","70000"],["15","270000","90000"],["30","420000","150000"]])
            self.assertEqual(self.q("SELECT COUNT(*) FROM protos.refinement_level WHERE profile_id=40"), [["31"]])
        finally:
            self.q("ROLLBACK")

    def test_typed_accessory_and_material_catalog(self):
        self.assertEqual(self.q("SELECT category_id,local_id,equip_slot,resistance_profile_id FROM protos.item_accessory"), [["3","1","6","1"]])
        self.assertEqual(self.q("SELECT category_id,local_id,material_kind FROM protos.item_material WHERE local_id=1"), [["6","1","1"]])
        self.assertEqual(self.q("SELECT category_id,max_stack FROM protos.item_base WHERE category_id IN (3,6) AND local_id=1 ORDER BY category_id"),
                         [["3","1"],["6","200"]])
        self.assertEqual(self.q("""SELECT p.profile_kind,p.max_level,r.level,e.bonus_id,e.value_scaled
            FROM protos.item_base b JOIN protos.refinement_profile p ON p.profile_id=b.refinement_profile_id
            JOIN protos.refinement_level r ON r.profile_id=p.profile_id
            LEFT JOIN protos.bonus_profile_entry e ON e.profile_id=r.extra_bonus_profile_id
            WHERE b.category_id=3 AND b.local_id=1 AND r.level IN (0,9,10,15) ORDER BY r.level"""),
            [["4","15","0",None,None],["4","15","9","3","2000"],
             ["4","15","10","3","2000"],["4","15","15","3","6000"]])
        self.assertEqual(self.q("SELECT COUNT(*) FROM protos.item_base WHERE category_id=3"), [["1"]])
        self.assertEqual(self.q("""SELECT p.profile_kind,p.max_level FROM protos.item_base b
            JOIN protos.refinement_profile p ON p.profile_id=b.refinement_profile_id WHERE b.category_id=6 AND b.local_id=1"""), [["1","0"]])

    def test_accessory_and_material_constraints(self):
        for sql, code in [
            ("INSERT INTO protos.item_accessory VALUES(3,1,6,1,NULL)", "1062"),
            ("INSERT INTO protos.item_material VALUES(6,1,1)", "1062"),
            ("INSERT INTO protos.item_accessory VALUES(3,999,6,1,NULL)", "1452"),
            ("INSERT INTO protos.item_material VALUES(6,999,1)", "1452"),
            ("UPDATE protos.item_accessory SET equip_slot=1", "4025"),
            ("UPDATE protos.item_accessory SET equip_slot=9", "4025"),
            ("UPDATE protos.item_accessory SET resistance_profile_id=999", "1452"),
            ("UPDATE protos.item_material SET material_kind=0", "4025"),
            ("UPDATE protos.item_material SET material_kind=6", "4025"),
            ("UPDATE protos.item_base SET max_stack=2 WHERE category_id=3", "4025"),
            ("UPDATE protos.item_base SET max_stack=0 WHERE category_id=6", "4025"),
            ("UPDATE protos.item_base SET binding_policy=3 WHERE category_id=6", "4025"),
            ("UPDATE protos.item_base SET base_bonus_profile_id=1 WHERE category_id=6", "4025"),
            ("UPDATE protos.item_base SET appearance_profile_id=1 WHERE category_id=6", "4025"),
            ("DELETE FROM protos.item_base WHERE category_id=3", "1451"),
            ("DELETE FROM protos.item_base WHERE category_id=6", "1451"),
        ]:
            with self.subTest(sql=sql), self.assertRaisesRegex(RuntimeError, "^" + code + ":"):
                self.q(sql)

    def test_accessory_socket_profiles_and_compatibility(self):
        self.assertEqual(self.q("SELECT socket_profile_id FROM protos.item_accessory WHERE category_id=3 AND local_id=1"), [["1"]])
        self.assertEqual(self.q("SELECT max_grade,unlock_units_per_attempt,charge_units_per_attempt FROM protos.accessory_socket_profile WHERE profile_id=1"), [["3","1","1"]])
        self.assertEqual(self.q("""SELECT grade,duration_ms,bonus_increase_bp,minimum_raw,minimum_bp,
            minimum_integer,unlock_chance_bp,charge_chance_bp FROM protos.accessory_socket_grade WHERE profile_id=1 ORDER BY grade"""),
            [["0","0","0","0","0","0","5000","10000"],
             ["1","86400000","1000","1000","100","1","5000","10000"],
             ["2","43200000","2000","2000","200","2","5000","10000"],
             ["3","21600000","4000","3000","300","3","0","0"]])
        self.assertEqual(self.q("""SELECT c.operation_kind,c.category_id,c.local_id,m.material_kind
            FROM protos.accessory_socket_material c JOIN protos.item_material m
            ON m.category_id=c.category_id AND m.local_id=c.local_id WHERE c.profile_id=1 ORDER BY c.operation_kind"""),
            [["1","6","3","4"],["2","6","2","5"]])
        self.assertEqual(self.q("SELECT COUNT(*) FROM protos.item_base WHERE category_id=3"), [["1"]])

    def test_accessory_socket_permanent_compatibility(self):
        # Fresh private sandbox only; do not change the shared fixture after this
        # test. Cross-row Jewel/disjoint-list validation also belongs to C++.
        self.q("START TRANSACTION")
        try:
            self.q("UPDATE protos.item_material SET material_kind=5 WHERE category_id=6 AND local_id=1")
            self.q("INSERT INTO protos.accessory_socket_material VALUES(1,3,6,1)")
            self.assertEqual(self.q("""SELECT c.operation_kind,c.category_id,c.local_id,m.material_kind
                FROM protos.accessory_socket_material c JOIN protos.item_material m
                ON m.category_id=c.category_id AND m.local_id=c.local_id
                WHERE c.profile_id=1 AND c.operation_kind=3"""), [["3","6","1","5"]])
            self.assertEqual(self.q("SELECT COUNT(*) FROM protos.item_base WHERE category_id=3"), [["1"]])
            with self.assertRaisesRegex(RuntimeError, "^1062:"):
                self.q("INSERT INTO protos.accessory_socket_material VALUES(1,3,6,1)")
            with self.assertRaisesRegex(RuntimeError, "^1452:"):
                self.q("INSERT INTO protos.accessory_socket_material VALUES(1,3,6,999)")
        finally:
            self.q("ROLLBACK")
        self.assertEqual(self.q("SELECT COUNT(*) FROM protos.accessory_socket_material WHERE operation_kind=3"), [["0"]])
        self.assertEqual(self.q("SELECT material_kind FROM protos.item_material WHERE category_id=6 AND local_id=1"), [["1"]])

    def test_recovery_profiles_and_typed_delivery(self):
        self.assertEqual(self.q("SELECT category_id,local_id,recovery_profile_id FROM protos.item_consumable ORDER BY local_id"),
                         [["5","1","1"],["5","2","2"],["5","3","3"]])
        self.assertEqual(self.q("SELECT profile_id,delivery_kind,units_per_use,allowed_contexts,allow_during_pvp,cooldown_profile_id FROM protos.recovery_profile ORDER BY profile_id"),
                         [["1","1","1","15","1","1"],["2","2","1","9","0","1"],["3","3","1","15","0",None]])
        self.assertEqual(self.q("SELECT hp_raw,sp_raw,max_hp_bp,max_sp_bp,flat_bonus_cap_bp FROM protos.instant_recovery_profile"),
                         [["300000","100000","500","1000","10000"]])
        self.assertEqual(self.q("SELECT hp_raw,sp_raw,tick_interval_ms,max_pool_per_tick_bp FROM protos.reserve_recovery_profile"),
                         [["500000","200000","1000","700"]])
        self.assertEqual(self.q("SELECT hp_per_tick_raw,sp_per_tick_raw,tick_interval_ms,duration_ms FROM protos.periodic_recovery_profile"),
                         [["25000","0","1000","60000"]])
        self.assertEqual(self.q("SELECT duration_ms FROM protos.consumable_cooldown_profile"), [["1000"]])
        self.assertEqual(self.q("""SELECT COUNT(*) FROM protos.item_base b JOIN protos.refinement_profile r
            ON b.refinement_profile_id=r.profile_id WHERE b.category_id=5 AND r.profile_kind=1 AND r.max_level=0"""), [["3"]])

    def test_recovery_sql_constraints(self):
        for sql,code in [
            ("UPDATE protos.recovery_profile SET units_per_use=0 WHERE profile_id=1","4025"),
            ("UPDATE protos.recovery_profile SET allowed_contexts=16 WHERE profile_id=1","4025"),
            ("UPDATE protos.recovery_profile SET allow_during_pvp=2 WHERE profile_id=1","4025"),
            ("UPDATE protos.recovery_profile SET cooldown_profile_id=999 WHERE profile_id=1","1452"),
            ("UPDATE protos.consumable_cooldown_profile SET duration_ms=0 WHERE profile_id=1","4025"),
            ("UPDATE protos.consumable_cooldown_profile SET duration_ms=9223372036854775808 WHERE profile_id=1","4025"),
            ("INSERT INTO protos.instant_recovery_profile VALUES(2,1,1,0,0,0,0)","1452"),
            ("UPDATE protos.instant_recovery_profile SET hp_raw=-1 WHERE profile_id=1","4025"),
            ("UPDATE protos.instant_recovery_profile SET max_hp_bp=10001 WHERE profile_id=1","4025"),
            ("UPDATE protos.instant_recovery_profile SET flat_bonus_cap_bp=100001 WHERE profile_id=1","4025"),
            ("UPDATE protos.instant_recovery_profile SET hp_raw=0,sp_raw=0,max_hp_bp=0,max_sp_bp=0 WHERE profile_id=1","4025"),
            ("UPDATE protos.reserve_recovery_profile SET tick_interval_ms=0 WHERE profile_id=2","4025"),
            ("UPDATE protos.reserve_recovery_profile SET max_pool_per_tick_bp=0 WHERE profile_id=2","4025"),
            ("UPDATE protos.periodic_recovery_profile SET sp_per_tick_raw=1 WHERE profile_id=3","4025"),
            ("UPDATE protos.periodic_recovery_profile SET duration_ms=999 WHERE profile_id=3","4025"),
            ("UPDATE protos.periodic_recovery_profile SET duration_ms=60001 WHERE profile_id=3","4025"),
            ("UPDATE protos.item_consumable SET recovery_profile_id=999 WHERE local_id=1","1452"),
            ("INSERT INTO protos.item_consumable VALUES(5,999,1)","1452"),
            ("INSERT INTO protos.item_consumable VALUES(5,1,1)","1062"),
            ("UPDATE protos.item_base SET binding_policy=3 WHERE category_id=5","4025"),
            ("UPDATE protos.item_base SET base_bonus_profile_id=1 WHERE category_id=5","4025"),
            ("UPDATE protos.item_base SET appearance_profile_id=1 WHERE category_id=5","4025"),
            ("DELETE FROM protos.recovery_profile WHERE profile_id=1","1451"),
            ("DELETE FROM protos.consumable_cooldown_profile WHERE profile_id=1","1451"),
        ]:
            with self.subTest(sql=sql), self.assertRaisesRegex(RuntimeError,"^"+code+":"):
                self.q(sql)

    def test_recovery_stack_and_transaction_rollback(self):
        self.q("START TRANSACTION")
        try:
            self.q("UPDATE protos.item_base SET max_stack=4294967295 WHERE category_id=5 AND local_id=1")
            self.assertEqual(self.q("SELECT max_stack FROM protos.item_base WHERE category_id=5 AND local_id=1"), [["4294967295"]])
            self.q("UPDATE protos.instant_recovery_profile SET hp_raw=1000000000000000,max_hp_bp=10000 WHERE profile_id=1")
            self.assertEqual(self.q("SELECT hp_raw,max_hp_bp FROM protos.instant_recovery_profile WHERE profile_id=1"), [["1000000000000000","10000"]])
        finally:
            self.q("ROLLBACK")
        self.assertEqual(self.q("SELECT hp_raw FROM protos.instant_recovery_profile WHERE profile_id=1"), [["300000"]])
        self.assertEqual(self.q("SELECT COUNT(*) FROM protos.item_base WHERE category_id=5"), [["3"]])

    def test_accessory_socket_sql_constraints(self):
        for sql, code in [
            ("UPDATE protos.item_accessory SET socket_profile_id=999", "1452"),
            ("INSERT INTO protos.accessory_socket_material VALUES(1,2,6,999)", "1452"),
            ("INSERT INTO protos.accessory_socket_material VALUES(1,2,6,2)", "1062"),
            ("INSERT INTO protos.accessory_socket_material VALUES(999,2,6,2)", "1452"),
            ("INSERT INTO protos.accessory_socket_material VALUES(1,0,6,2)", "4025"),
            ("INSERT INTO protos.accessory_socket_material VALUES(1,4,6,2)", "4025"),
            ("UPDATE protos.accessory_socket_profile SET max_grade=0 WHERE profile_id=1", "4025"),
            ("UPDATE protos.accessory_socket_profile SET unlock_units_per_attempt=0 WHERE profile_id=1", "4025"),
            ("UPDATE protos.accessory_socket_grade SET duration_ms=0 WHERE profile_id=1 AND grade=1", "4025"),
            ("UPDATE protos.accessory_socket_grade SET duration_ms=9223372036854775808 WHERE profile_id=1 AND grade=1", "4025"),
            ("UPDATE protos.accessory_socket_grade SET minimum_raw=1 WHERE profile_id=1 AND grade=0", "4025"),
            ("UPDATE protos.accessory_socket_grade SET charge_chance_bp=10001 WHERE profile_id=1 AND grade=1", "4025"),
            ("UPDATE protos.accessory_socket_grade SET bonus_increase_bp=100001 WHERE profile_id=1 AND grade=1", "4025"),
            ("DELETE FROM protos.item_material WHERE category_id=6 AND local_id=2", "1451"),
            ("DELETE FROM protos.accessory_socket_profile WHERE profile_id=1", "1451"),
        ]:
            with self.subTest(sql=sql), self.assertRaisesRegex(RuntimeError, "^" + code + ":"):
                self.q(sql)

    def test_containers_keys_and_loot(self):
        self.assertEqual(self.q("SELECT schema_version,canonical_domain FROM protos.catalog_format"), [["7", "auriga.item_catalog.v7"]])
        self.assertEqual(self.q("SELECT category_id,local_id,container_kind,loot_profile_id,lock_id FROM protos.item_container ORDER BY local_id"),
                         [["8","1","1","1","0"],["8","2","2","2","7"],["8","3","3",None,"0"]])
        self.assertEqual(self.q("SELECT category_id,local_id,lock_id FROM protos.item_container_key"), [["18","1","7"]])
        self.assertEqual(self.q("SELECT profile_id,COUNT(*),SUM(weight) FROM protos.loot_entry GROUP BY profile_id ORDER BY profile_id"),
                         [["1","3","100"],["2","4","13100"]])
        self.assertEqual(self.q("SELECT CONSTRAINT_NAME FROM information_schema.CHECK_CONSTRAINTS WHERE CONSTRAINT_SCHEMA='protos' "
                                "AND TABLE_NAME='item_base' AND CONSTRAINT_NAME LIKE 'ck_item_base_%' ORDER BY 1"),
                         [["ck_item_base_category"], ["ck_item_base_non_equipment"], ["ck_item_base_stack"]])
        self.assertEqual(self.q("SELECT COUNT(*) FROM information_schema.CHECK_CONSTRAINTS WHERE CONSTRAINT_SCHEMA='protos' "
                                "AND TABLE_NAME='item_base' AND CHECK_CLAUSE LIKE '%in (1,2,3,5,6)%'"), [["0"]])
        self.assertEqual(self.q("SELECT CHECK_CLAUSE FROM information_schema.CHECK_CONSTRAINTS WHERE CONSTRAINT_SCHEMA='protos' "
                                "AND TABLE_NAME='item_base' AND CONSTRAINT_NAME='ck_item_base_category'"), [["`category_id` in (1,2,3,5,6,8,16,18)"]])

    def test_container_sql_constraints(self):
        for sql, code in [
            ("UPDATE protos.item_container SET container_kind=4 WHERE local_id=1", "4025"),
            ("UPDATE protos.item_container SET loot_profile_id=NULL WHERE local_id=1", "4025"),
            ("UPDATE protos.item_container SET loot_profile_id=1 WHERE local_id=3", "4025"),
            ("UPDATE protos.item_container SET lock_id=0 WHERE local_id=2", "4025"),
            ("UPDATE protos.item_container SET lock_id=3 WHERE local_id=1", "4025"),
            ("UPDATE protos.item_container SET loot_profile_id=999 WHERE local_id=1", "1452"),
            ("INSERT INTO protos.item_container VALUES(8,999,3,NULL,0)", "1452"),
            ("INSERT INTO protos.item_container VALUES(6,1,3,NULL,0)", "4025"),
            ("UPDATE protos.item_container_key SET lock_id=0", "4025"),
            ("UPDATE protos.loot_profile SET pick_mode=3 WHERE profile_id=1", "4025"),
            ("UPDATE protos.loot_entry SET weight=0 WHERE profile_id=1 AND entry_no=1", "4025"),
            ("UPDATE protos.loot_entry SET item_category_id=NULL WHERE profile_id=1 AND entry_no=1", "4025"),
            ("UPDATE protos.loot_entry SET item_local_id=999 WHERE profile_id=1 AND entry_no=1", "1452"),
            ("UPDATE protos.loot_entry SET amount=0 WHERE profile_id=1 AND entry_no=1", "4025"),
            ("UPDATE protos.loot_entry SET rare_attribute_chance_bp=10001 WHERE profile_id=1 AND entry_no=3", "4025"),
            ("UPDATE protos.loot_entry SET rare_attribute_chance_bp=1 WHERE profile_id=1 AND entry_no=2", "4025"),
            ("UPDATE protos.loot_entry SET amount=1 WHERE profile_id=2 AND entry_no=4", "4025"),
            ("INSERT INTO protos.loot_entry VALUES(1,4,7,NULL,NULL,10001,1,0)", "4025"),
            ("INSERT INTO protos.loot_entry VALUES(1,4,9,NULL,NULL,1,1,0)", "4025"),
            ("INSERT INTO protos.loot_entry VALUES(1,4,2,6,1,1,1,0)", "4025"),
            ("UPDATE protos.item_base SET binding_policy=3 WHERE category_id=8 AND local_id=1", "4025"),
            ("UPDATE protos.item_base SET appearance_profile_id=1 WHERE category_id=18", "4025"),
            ("INSERT INTO protos.item_base VALUES(4,1,'costume.unported','item.c.name','item.c.description','icons/c.png',"
             "1,1,1,0,0,1,1,1,1,NULL,NULL,1,0,0,0)", "4025"),
            ("DELETE FROM protos.loot_profile WHERE profile_id=1", "1451"),
            ("DELETE FROM protos.item_base WHERE category_id=6 AND local_id=1", "1451"),
        ]:
            with self.subTest(sql=sql), self.assertRaisesRegex(RuntimeError, "^" + code + ":"):
                self.q(sql)
        # Stackable boxes and keys are allowed above one unit.
        self.q("START TRANSACTION")
        try:
            self.q("UPDATE protos.item_base SET max_stack=500 WHERE category_id IN (8,18)")
        finally:
            self.q("ROLLBACK")

    def test_currency_token_rows_and_constraints(self):
        self.assertEqual(self.q("SELECT category_id,local_id,wallet_currency,amount_per_unit FROM protos.item_currency_token ORDER BY local_id"),
                         [["16","1","1","5000000"],["16","2","2","100"]])
        for sql, code in [
            ("UPDATE protos.item_currency_token SET wallet_currency=3 WHERE local_id=1", "4025"),
            ("UPDATE protos.item_currency_token SET wallet_currency=0 WHERE local_id=1", "4025"),
            ("UPDATE protos.item_currency_token SET amount_per_unit=0 WHERE local_id=1", "4025"),
            ("UPDATE protos.item_currency_token SET amount_per_unit=9223372036854775808 WHERE local_id=1", "4025"),
            ("INSERT INTO protos.item_currency_token VALUES(16,999,1,1)", "1452"),
            ("INSERT INTO protos.item_currency_token VALUES(6,1,1,1)", "4025"),
            ("INSERT INTO protos.item_currency_token VALUES(16,1,1,1)", "1062"),
            ("UPDATE protos.item_base SET binding_policy=3 WHERE category_id=16", "4025"),
            ("UPDATE protos.item_base SET appearance_profile_id=1 WHERE category_id=16", "4025"),
            ("DELETE FROM protos.item_base WHERE category_id=16 AND local_id=1", "1451"),
        ]:
            with self.subTest(sql=sql), self.assertRaisesRegex(RuntimeError, "^" + code + ":"):
                self.q(sql)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mariadb-bin", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args, remaining = parser.parse_known_args()
    CategorySchemaTests.binary_dir = args.mariadb_bin.resolve(strict=True)
    CategorySchemaTests.work_dir = args.work_dir.resolve()
    program = unittest.main(argv=[sys.argv[0], *remaining], exit=False)
    raise SystemExit(0 if program.result.wasSuccessful() else 1)
