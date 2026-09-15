#!/usr/bin/env python3
"""Watch html/ for changes and upload each changed file to the ESPuino.

Usage:
  python3 watch-and-upload.py          # watch mode
  python3 watch-and-upload.py <file>   # upload a single file and exit

Device host resolution order:
  1. .vscode/espuino-device  (one line, gitignored)
  2. ESPUINO_HOST env var
  3. espuino.local (mDNS default)

Requires the watchdog library (watch mode only):
  pip install watchdog
"""

import os
import sys
import time
from pathlib import Path
from urllib import request as urllib_request
from urllib.error import URLError

WORKSPACE = Path(__file__).resolve().parent.parent
HTML_DIR = WORKSPACE / "html"


def get_device() -> str:
    device_file = Path(__file__).parent / "espuino-device"
    if device_file.exists():
        return device_file.read_text().strip()
    return os.environ.get("ESPUINO_HOST", "espuino.local")


def upload_file(path: Path) -> bool:
    try:
        rel = path.relative_to(HTML_DIR)
    except ValueError:
        return False

    dest = "/.html/" + rel.as_posix()
    device = get_device()
    url = f"http://{device}/explorer?path={dest}"

    print(f"ESPuino upload: {rel.as_posix()} → http://{device}{dest}")
    try:
        data = path.read_bytes()
        req = urllib_request.Request(url, data=data, method="POST")
        with urllib_request.urlopen(req, timeout=10) as resp:
            if resp.status == 200:
                print("  ✓ done")
                return True
            print(f"  ✗ HTTP {resp.status}")
    except URLError as e:
        print(f"  ✗ failed — is the device reachable at {device}? ({e.reason})")
    except Exception as e:  # noqa: BLE001
        print(f"  ✗ failed: {e}")
    return False


def watch():
    try:
        from watchdog.events import FileSystemEventHandler
        from watchdog.observers import Observer
    except ImportError:
        print("Error: watchdog not installed. Run the VS Code task 'Install watchdog (ESPuino upload dependency)' first.", file=sys.stderr)
        sys.exit(1)

    # Debounce: skip duplicate events within 200 ms of a successful upload.
    last_upload: dict[Path, float] = {}
    DEBOUNCE = 0.2

    class Handler(FileSystemEventHandler):
        def _handle(self, path: Path):
            now = time.monotonic()
            if now - last_upload.get(path, 0) < DEBOUNCE:
                return
            last_upload[path] = now
            upload_file(path)

        def on_modified(self, event):
            if not event.is_directory:
                self._handle(Path(event.src_path))

        def on_created(self, event):
            if not event.is_directory:
                self._handle(Path(event.src_path))

        def on_moved(self, event):
            # Atomic saves (write to temp file → rename) show up as a move.
            if not event.is_directory:
                self._handle(Path(event.dest_path))

    print(f"Watching {HTML_DIR}/ — Ctrl+C to stop")
    observer = Observer()
    observer.schedule(Handler(), str(HTML_DIR), recursive=True)
    observer.start()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        observer.stop()
        observer.join()


if __name__ == "__main__":
    if len(sys.argv) == 2:
        # Single-file mode: upload the given file and exit.
        upload_file(Path(sys.argv[1]).resolve())
    else:
        if not HTML_DIR.exists():
            print(f"Error: {HTML_DIR} not found", file=sys.stderr)
            sys.exit(1)
        watch()
