"""Persistent storage for devices, cards and settings.

A single JSON file (``db.json``) under ``DATA_DIR`` is plenty for the
expected scale (a handful of devices, a few dozen cards) — a database
server would be pure overhead here. Writes are serialized by a
process-local lock and applied atomically (tmp file + rename), mirroring
the download pattern used on the ESPuino itself (concept §13).
"""

import json
import os
import threading
from datetime import datetime, timezone

_lock = threading.Lock()

_DEFAULT_DB = {
    "settings": {
        "delete_mode": "lazy",  # "lazy" | "secure" — see concept §13.1
        "password_hash": None,  # None = hub web UI has no login requirement
    },
    "devices": {},
    "cards": {},
}


def now_iso():
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


class Store:
    def __init__(self, data_dir):
        self.data_dir = data_dir
        self.db_path = os.path.join(data_dir, "db.json")
        os.makedirs(data_dir, exist_ok=True)
        if not os.path.exists(self.db_path):
            self._write(_DEFAULT_DB)

    # -- low-level ---------------------------------------------------
    def _read(self):
        with open(self.db_path, "r", encoding="utf-8") as f:
            return json.load(f)

    def _write(self, data):
        tmp_path = self.db_path + ".tmp"
        with open(tmp_path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, sort_keys=True, ensure_ascii=False)
        os.replace(tmp_path, self.db_path)

    def _mutate(self, fn):
        """Read, let fn mutate the data in place, write back atomically."""
        with _lock:
            data = self._read()
            result = fn(data)
            self._write(data)
            return result

    # -- devices -------------------------------------------------------
    def list_devices(self):
        return self._read()["devices"]

    def touch_device(self, esp_id, ip, card_id, ts):
        def mutate(data):
            dev = data["devices"].setdefault(
                esp_id, {"first_seen": ts, "last_seen": ts, "ip": ip, "last_card_id": card_id}
            )
            dev["last_seen"] = ts
            dev["ip"] = ip
            dev["last_card_id"] = card_id
            return dev

        return self._mutate(mutate)

    # -- cards -----------------------------------------------------------
    def list_cards(self):
        return self._read()["cards"]

    def get_card(self, card_id):
        return self._read()["cards"].get(card_id)

    def register_pending(self, card_id, esp_id, ts):
        """Registers a not-yet-known card as 'pending' (concept §5.3)."""

        def mutate(data):
            if card_id in data["cards"]:
                card = data["cards"][card_id]
                card["last_seen"] = ts
                card["last_seen_esp_id"] = esp_id
                return card
            card = {
                "status": "pending",
                "name": "",
                "kind": "files",
                "play_mode": None,
                "stream_url": None,
                "files": [],
                "force_epoch": 0,
                "first_seen": ts,
                "last_seen": ts,
                "last_seen_esp_id": esp_id,
                "created_at": ts,
                "updated_at": ts,
            }
            data["cards"][card_id] = card
            return card

        return self._mutate(mutate)

    def touch_card_seen(self, card_id, esp_id, ts):
        def mutate(data):
            card = data["cards"].get(card_id)
            if card is not None:
                card["last_seen"] = ts
                card["last_seen_esp_id"] = esp_id
            return card

        return self._mutate(mutate)

    def save_assignment(self, card_id, name, kind, play_mode, stream_url, files):
        def mutate(data):
            ts = now_iso()
            card = data["cards"].setdefault(
                card_id,
                {
                    "status": "pending",
                    "force_epoch": 0,
                    "first_seen": ts,
                    "last_seen": ts,
                    "last_seen_esp_id": None,
                },
            )
            card["status"] = "assigned"
            card["name"] = name
            card["kind"] = kind
            card["play_mode"] = play_mode
            card["stream_url"] = stream_url
            card["files"] = files
            card["updated_at"] = ts
            return card

        return self._mutate(mutate)

    def bump_force_epoch(self, card_id):
        def mutate(data):
            card = data["cards"].get(card_id)
            if card is not None:
                card["force_epoch"] = card.get("force_epoch", 0) + 1
                card["updated_at"] = now_iso()
            return card

        return self._mutate(mutate)

    def bump_force_epoch_all(self):
        def mutate(data):
            ts = now_iso()
            for card in data["cards"].values():
                card["force_epoch"] = card.get("force_epoch", 0) + 1
                card["updated_at"] = ts
            return len(data["cards"])

        return self._mutate(mutate)

    def delete_card(self, card_id):
        def mutate(data):
            return data["cards"].pop(card_id, None)

        return self._mutate(mutate)

    # -- settings ----------------------------------------------------
    def get_settings(self):
        return self._read()["settings"]

    def set_delete_mode(self, mode):
        def mutate(data):
            data["settings"]["delete_mode"] = mode
            return mode

        return self._mutate(mutate)

    def set_password_hash(self, password_hash):
        """password_hash is None to disable the login requirement entirely."""

        def mutate(data):
            data["settings"]["password_hash"] = password_hash
            return password_hash

        return self._mutate(mutate)
