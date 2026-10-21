"""Start one server instance, or validate every instance, from a central server.json."""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


def command(binary_dir, config, instance, check=False):
    name = {'database': 'Database', 'login': 'LoginServer'}.get(instance, 'GameServer')
    filename = name + ('.exe' if os.name == 'nt' else '')
    binary = (binary_dir / filename).resolve()
    cmake_binary = binary_dir / 'SRC/Server' / name / 'RelWithDebInfo' / filename
    if not binary.is_file() and cmake_binary.is_file(): binary = cmake_binary.resolve()
    result = [str(binary), '--config', str(config.resolve()), '--instance', instance]
    if check: result.append('--check-config')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=Path('server.json'))
    parser.add_argument('--bin-dir', required=True, type=Path, help='Directory containing the three server executables')
    parser.add_argument('--instance', help='database, login, or a key in game_servers')
    parser.add_argument('--check-config', action='store_true', help='Check all instances without SQL, TLS or listeners')
    args = parser.parse_args()
    try:
        doc = json.loads(args.config.read_text(encoding='utf-8-sig'))
        if doc.get('schema_version') != 1 or not isinstance(doc.get('game_servers'), dict):
            raise ValueError('Expected a version 1 central configuration')
        instances = ['database', 'login'] + list(doc['game_servers'])
        if args.instance is not None and args.instance not in instances: raise ValueError('Unknown instance name')
        selected = [args.instance] if args.instance else instances
        for instance in selected:
            result = subprocess.run(command(args.bin_dir, args.config, instance, check=True), check=False,
                                    capture_output=True, text=True,
                                    creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            sys.stdout.write(result.stdout)
            sys.stderr.write(result.stderr)
            if result.returncode: return result.returncode
        if args.check_config: return 0
        if not args.instance: raise ValueError('Select --instance to start a process, or --check-config to validate all')
        argv = command(args.bin_dir, args.config, args.instance)
        # Replacement preserves normal terminal/signal ownership; no background services.
        os.execv(argv[0], argv)
    except (OSError, ValueError, TypeError, AttributeError):
        parser.error('Cannot run server: verify central config, instance and executable paths. Config values are not printed.')


if __name__ == '__main__':
    raise SystemExit(main())
