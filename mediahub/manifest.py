"""Builds the per-card manifest (concept §7), including `version` hashing.

Per the concept, `version` is an "opaque change stamp = SHA-256 of the
canonical manifest, computed on the hub" — the ESPuino only ever compares
strings. We therefore hash on every request (manifests are tiny, this
costs nothing) rather than carrying a cached value that could go stale.
`force_epoch` feeds into the hash but never into the output — that's the
"force refresh" lever from §9: content unchanged, hash still new.
"""

import hashlib
import json
import re

from flask_babel import gettext as _

WEBSTREAM_PLAY_MODE = 8

# The ESPuino always builds a card ID from exactly 4 UID bytes formatted as
# "%03d" each (see cardIdSize/cardIdStringSize in src/Rfid.h) — always 12
# decimal digits, never hex.
CARD_ID_PATTERN = re.compile(r"^\d{12}$")


def is_valid_card_id(card_id):
    return bool(CARD_ID_PATTERN.match(card_id))

# Playmodes that make sense as an assignment target (values.h) — WEBSTREAM
# and the MediaHub marker itself are deliberately excluded: WEBSTREAM is set
# via the "webradio" content type, not the dropdown; NO_PLAYLIST/BUSY are not
# real targets; LOCAL_M3U is not supported per the concept. Labels follow the
# terminology already used in values.h and are translated at render time via
# gettext in the template.
FILE_PLAY_MODES = [
    (1, "Single track"),
    (2, "Single track (loop)"),
    (3, "Audiobook (remembers position)"),
    (4, "Audiobook (loop)"),
    (16, "Audiobook, recursive (remembers position)"),
    (5, "All tracks of a folder (sorted)"),
    (6, "All tracks of a folder (shuffled)"),
    (7, "All tracks of a folder (sorted, loop)"),
    (9, "All tracks of a folder (shuffled, loop)"),
    (17, "All tracks, recursive (shuffled)"),
    (15, "All tracks, recursive (sorted)"),
    (12, "Single random track of a folder"),
    (13, "Random subfolder (sorted)"),
    (14, "Random subfolder (shuffled)"),
]

_FILE_PLAY_MODE_VALUES = {value for value, _label in FILE_PLAY_MODES}


def is_valid_file_play_mode(play_mode):
    return play_mode in _FILE_PLAY_MODE_VALUES


def _translation_placeholders():  # pragma: no cover
    """Never called at runtime. `pybabel extract` only picks up string
    literals passed directly to `_()`; the template renders these labels
    via a variable (`{{ _(label) }}`), which a static extractor can't
    resolve. This mirrors the FILE_PLAY_MODES labels above so `pybabel
    extract` still finds them for translation.
    """
    return [
        _("Single track"),
        _("Single track (loop)"),
        _("Audiobook (remembers position)"),
        _("Audiobook (loop)"),
        _("Audiobook, recursive (remembers position)"),
        _("All tracks of a folder (sorted)"),
        _("All tracks of a folder (shuffled)"),
        _("All tracks of a folder (sorted, loop)"),
        _("All tracks of a folder (shuffled, loop)"),
        _("All tracks, recursive (shuffled)"),
        _("All tracks, recursive (sorted)"),
        _("Single random track of a folder"),
        _("Random subfolder (sorted)"),
        _("Random subfolder (shuffled)"),
    ]


def _canonical_json(obj):
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def build_manifest(card_id, card, files_base_url):
    """Builds the public manifest (with `version`) for an assigned card."""
    if card["kind"] == "webradio":
        body = {
            "schema": 1,
            "cardId": card_id,
            "playMode": WEBSTREAM_PLAY_MODE,
            "name": card.get("name") or "",
            "stream": card["stream_url"],
        }
    else:
        body = {
            "schema": 1,
            "cardId": card_id,
            "playMode": card["play_mode"],
            "name": card.get("name") or "",
            "filesBaseUrl": files_base_url,
            "files": [
                {"path": f["path"], "size": f["size"], "sha256": f["sha256"]}
                for f in card.get("files", [])
            ],
        }

    hashed = dict(body)
    hashed["_forceEpoch"] = card.get("force_epoch", 0)
    version = hashlib.sha256(_canonical_json(hashed)).hexdigest()

    return {"version": version, **body}
