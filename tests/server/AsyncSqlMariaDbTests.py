"""Run transaction integration tests in a fresh loopback-only MariaDB.

No existing service/config/datadir is used. The test datadir/logs are retained
under --work-dir for diagnosis. Also drop one actual COMMIT response at the
wire level, after MariaDB has acknowledged it to the proxy.
"""

from __future__ import annotations

import argparse
import contextlib
import os
from pathlib import Path
import socket
import subprocess
import threading
import time
import uuid


def free_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def receive(sock: socket.socket, count: int) -> bytes:
    data = bytearray()
    while len(data) < count:
        part = sock.recv(count - len(data))
        if not part:
            raise EOFError
        data.extend(part)
    return bytes(data)


def packet(sock: socket.socket) -> tuple[bytes, bytes]:
    header = receive(sock, 4)
    body = receive(sock, int.from_bytes(header[:3], "little"))
    return header, body


class CommitResponseProxy:
    def __init__(self, backend_port: int):
        self.backend_port = backend_port
        self.listener = socket.socket()
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen()
        self.listener.settimeout(0.2)
        self.port = self.listener.getsockname()[1]
        self.stop = threading.Event()
        self.dropped = threading.Event()
        self.lock = threading.Lock()
        self.claimed = False
        self.sockets: list[socket.socket] = []
        self.threads: list[threading.Thread] = []
        self.error: Exception | None = None
        self.acceptor = threading.Thread(target=self.accept, daemon=True)
        self.acceptor.start()

    def accept(self):
        while not self.stop.is_set():
            try:
                client, _ = self.listener.accept()
            except (TimeoutError, OSError):
                continue
            try:
                backend = socket.create_connection(("127.0.0.1", self.backend_port), timeout=3)
                client.settimeout(15)
                backend.settimeout(15)
                with self.lock:
                    self.sockets.extend((client, backend))
                pending_commit = threading.Event()
                for source, destination, client_direction in ((client, backend, True), (backend, client, False)):
                    thread = threading.Thread(target=self.relay,
                                              args=(source, destination, client_direction, pending_commit), daemon=True)
                    self.threads.append(thread)
                    thread.start()
            except OSError as error:
                client.close()
                self.error = error

    def relay(self, source, destination, client_direction, pending_commit):
        try:
            while not self.stop.is_set():
                header, body = packet(source)
                if client_direction and body == b"\x03COMMIT":
                    with self.lock:
                        if not self.claimed:
                            self.claimed = True
                            pending_commit.set()
                if not client_direction and pending_commit.is_set():
                    if not body or body[0] != 0:
                        raise RuntimeError("COMMIT did not produce the expected MariaDB OK packet")
                    self.dropped.set()
                    return  # Close both ends without forwarding the real commit acknowledgement.
                destination.sendall(header + body)
        except (EOFError, ConnectionError, OSError):
            pass
        except Exception as error:
            self.error = error
        finally:
            for stream in (source, destination):
                with contextlib.suppress(OSError):
                    stream.shutdown(socket.SHUT_RDWR)
                stream.close()

    def close(self):
        self.stop.set()
        self.listener.close()
        with self.lock:
            for stream in self.sockets:
                with contextlib.suppress(OSError):
                    stream.shutdown(socket.SHUT_RDWR)
                stream.close()
        self.acceptor.join(2)
        for thread in self.threads:
            thread.join(2)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server-bin-dir", type=Path, required=True)
    parser.add_argument("--test-executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--test-argument", action="append", default=[])
    parser.add_argument("--followup-argument", action="append", default=[])
    parser.add_argument("--crash-server-before-followup", action="store_true")
    args = parser.parse_args()
    if args.crash_server_before_followup and not args.followup_argument:
        parser.error("--crash-server-before-followup requires a followup process")
    binaries = args.server_bin_dir.resolve(strict=True)
    executable = args.test_executable.resolve(strict=True)
    suffix = ".exe" if (binaries / "mariadbd.exe").exists() else ""
    if not suffix:
        parser.error("this isolated installer currently supports the Windows MariaDB distribution")
    flags = subprocess.CREATE_NO_WINDOW
    root = args.work_dir.resolve() / uuid.uuid4().hex
    root.mkdir(parents=True, exist_ok=False)
    datadir = root / "data"
    port = free_port()
    log_path = root / "server.log"
    with (root / "initialize.log").open("wb") as log:
        subprocess.run([str(binaries / "mariadb-install-db.exe"), f"--datadir={datadir}", f"--port={port}",
                        "--silent"], stdout=log, stderr=subprocess.STDOUT, check=True, timeout=90,
                       creationflags=flags)
    with log_path.open("wb") as log:
        server = subprocess.Popen([str(binaries / "mariadbd.exe"), "--no-defaults", f"--basedir={binaries.parent}",
                                   f"--datadir={datadir}", "--bind-address=127.0.0.1", f"--port={port}",
                                   "--skip-log-bin", "--skip-ssl", "--innodb-buffer-pool-size=64M", "--innodb-log-file-size=16M",
                                   "--innodb-flush-log-at-trx-commit=1", "--max-allowed-packet=64M", "--console"],
                                  stdout=log, stderr=subprocess.STDOUT, creationflags=flags)
        proxy = None
        try:
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline:
                if server.poll() is not None:
                    raise RuntimeError(f"isolated MariaDB exited; inspect {log_path}")
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                        break
                except OSError:
                    time.sleep(0.1)
            else:
                raise TimeoutError(f"isolated MariaDB did not listen; inspect {log_path}")
            proxy = CommitResponseProxy(port)
            schema = "auriga_mission_sql_test_" + uuid.uuid4().hex
            # Connector/C 3.4 requires TLS verification by default. Only this
            # isolated child process talks plaintext to the packet fault proxy.
            # Production connector options and the parent environment stay intact.
            child_environment = dict(os.environ, MARIADB_TLS_DISABLE_PEER_VERIFICATION="1")
            result = subprocess.run([str(executable), str(port), schema, str(proxy.port), *args.test_argument],
                                    timeout=90, creationflags=flags, capture_output=True, env=child_environment)
            print(result.stdout.decode("utf-8", errors="replace"), end="", flush=True)
            print(result.stderr.decode("utf-8", errors="replace"), end="", flush=True)
            if result.returncode:
                print(f"Integration executable failed: {result.returncode} (0x{result.returncode & 0xffffffff:08x})", flush=True)
                if proxy.error:
                    print(f"Proxy error: {proxy.error}", flush=True)
                return result.returncode
            if proxy.error:
                raise proxy.error
            if not proxy.dropped.is_set():
                raise RuntimeError("test did not exercise a lost real COMMIT acknowledgement")
            if args.followup_argument:
                if args.crash_server_before_followup:
                    # Kill only the exact private process spawned above. Flush=1
                    # makes the subsequent cold process exercise InnoDB recovery
                    # of acknowledged commits, not just a reconnect to cached data.
                    command = server.args
                    server.kill()
                    server.wait(timeout=10)
                    log.flush()
                    log.seek(0, os.SEEK_END)
                    server = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, creationflags=flags)
                    restart_deadline = time.monotonic() + 45
                    while time.monotonic() < restart_deadline:
                        if server.poll() is not None:
                            raise RuntimeError(f"isolated MariaDB recovery failed; inspect {log_path}")
                        try:
                            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                                break
                        except OSError:
                            time.sleep(0.1)
                    else:
                        raise TimeoutError(f"isolated MariaDB recovery did not listen; inspect {log_path}")
                    print("Isolated MariaDB crash/restart completed.", flush=True)
                followup = subprocess.run([str(executable), str(port), schema, str(proxy.port),
                                           *args.test_argument, *args.followup_argument],
                                          timeout=90, creationflags=flags, capture_output=True, env=child_environment)
                print(followup.stdout.decode("utf-8", errors="replace"), end="", flush=True)
                print(followup.stderr.decode("utf-8", errors="replace"), end="", flush=True)
                if followup.returncode:
                    print(f"Cold-process executable failed: {followup.returncode}", flush=True)
                    return followup.returncode
            print(f"Isolated MariaDB and lost-COMMIT proxy checks passed. Logs: {root}", flush=True)
            return 0
        finally:
            if proxy:
                proxy.close()
            if server.poll() is None:
                with contextlib.suppress(subprocess.TimeoutExpired, OSError):
                    subprocess.run([str(binaries / "mariadb-admin.exe"), "--no-defaults", "--host=127.0.0.1",
                                    f"--port={port}", "--user=root", "shutdown"], stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL, timeout=10, creationflags=flags)
                try:
                    server.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    server.terminate()
                    server.wait(timeout=10)


if __name__ == "__main__":
    raise SystemExit(main())
