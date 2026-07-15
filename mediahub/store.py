"""Persistent storage for devices, cards and settings.

A single JSON file (``db.json``) under ``DATA_DIR`` is plenty for the
expected scale (a handful of devices, a few dozen cards) — a database
server would be pure overhead here. Writes are serialized by a
process-local lock and applied atomically (tmp file + rename), mirroring
the download pattern used on the ESPuino itself (concept §13).

Cards are keyed by (esp_id, card_id), not card_id alone — the same
physical card can be enrolled on several ESPuinos (that's the point of
the feature: kids swap cards between devices), and each device gets its
own independent assignment. This also makes secure delete unambiguous:
an assignment already knows exactly which one ESPuino to call, no more
guessing from whichever device last happened to tap the card.
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


def _card_key(esp_id, card_id):
    return f"{esp_id}/{card_id}"


class Store:
    def __init__(self, data_dir):
        self.data_dir = data_dir
        self.db_path = os.path.join(data_dir, "db.json")
        os.makedirs(data_dir, exist_ok=True)
        if not os.path.exists(self.db_path):
            self._write(_DEFAULT_DB)
        else:
            self._migrate_legacy_cards()

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

    def _migrate_legacy_cards(self):
        """One-time upgrade from the pre-esp_id schema, where "cards" was
        keyed by card_id alone and carried a "last_seen_esp_id" field.
        Cards that never saw a device (no last_seen_esp_id) can't be
        migrated — there's nothing to attach them to — and are dropped.
        """

        def mutate(data):
            legacy = {
                key: card
                for key, card in data["cards"].items()
                if "esp_id" not in card
            }
            if not legacy:
                return 0
            for key, card in legacy.items():
                del data["cards"][key]
                esp_id = card.pop("last_seen_esp_id", None)
                if not esp_id:
                    continue
                card["esp_id"] = esp_id
                card["card_id"] = key
                data["cards"][_card_key(esp_id, key)] = card
            return len(legacy)

        self._mutate(mutate)

    # -- devices -------------------------------------------------------
    def list_devices(self):
        return self._read()["devices"]

    def touch_device(self, esp_id, ip, card_id, ts):
        def mutate(data):
            dev = data["devices"].setdefault(
                esp_id,
                {"first_seen": ts, "last_seen": ts, "ip": ip, "last_card_id": card_id, "alias": None},
            )
            dev["last_seen"] = ts
            dev["ip"] = ip
            dev["last_card_id"] = card_id
            return dev

        return self._mutate(mutate)

    def set_device_alias(self, esp_id, alias):
        def mutate(data):
            dev = data["devices"].get(esp_id)
            if dev is not None:
                dev["alias"] = alias or None
            return dev

        return self._mutate(mutate)

    def count_cards_for_device(self, esp_id):
        return sum(1 for c in self._read()["cards"].values() if c["esp_id"] == esp_id)

    def delete_device(self, esp_id):
        """Removes the device bookkeeping entry AND all of its card
        assignments (cascade) — the admin already confirmed the count
        client-side before this is called. Returns the number of card
        assignments removed."""

        def mutate(data):
            keys = [key for key, c in data["cards"].items() if c["esp_id"] == esp_id]
            for key in keys:
                del data["cards"][key]
            data["devices"].pop(esp_id, None)
            return len(keys)

        return self._mutate(mutate)

    # -- cards -----------------------------------------------------------
    def list_cards(self):
        return self._read()["cards"]

    def get_card(self, esp_id, card_id):
        return self._read()["cards"].get(_card_key(esp_id, card_id))

    def register_pending(self, esp_id, card_id, ts):
        """Registers a not-yet-known (esp_id, card_id) pair as 'pending'
        (concept §5.3)."""

        def mutate(data):
            key = _card_key(esp_id, card_id)
            if key in data["cards"]:
                card = data["cards"][key]
                card["last_seen"] = ts
                return card
            card = {
                "esp_id": esp_id,
                "card_id": card_id,
                "status": "pending",
                "name": "",
                "kind": "files",
                "play_mode": None,
                "stream_url": None,
                "files": [],
                "force_epoch": 0,
                "first_seen": ts,
                "last_seen": ts,
                "created_at": ts,
                "updated_at": ts,
            }
            data["cards"][key] = card
            return card

        return self._mutate(mutate)

    def touch_card_seen(self, esp_id, card_id, ts):
        def mutate(data):
            card = data["cards"].get(_card_key(esp_id, card_id))
            if card is not None:
                card["last_seen"] = ts
            return card

        return self._mutate(mutate)

    def save_assignment(self, esp_id, card_id, name, kind, play_mode, stream_url, files):
        def mutate(data):
            ts = now_iso()
            key = _card_key(esp_id, card_id)
            card = data["cards"].setdefault(
                key,
                {
                    "esp_id": esp_id,
                    "card_id": card_id,
                    "status": "pending",
                    "force_epoch": 0,
                    "first_seen": ts,
                    "last_seen": ts,
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

    def bump_force_epoch(self, esp_id, card_id):
        def mutate(data):
            card = data["cards"].get(_card_key(esp_id, card_id))
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

    def delete_card(self, esp_id, card_id):
        def mutate(data):
            return data["cards"].pop(_card_key(esp_id, card_id), None)

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
