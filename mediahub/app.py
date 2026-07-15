"""ESPuino MediaHub — lightweight local hub for centrally managing the
RFID assignments of multiple ESPuinos.

See ../mediahub-konzept.md for the full specification.
"""

import json
import os

from flask import (
    Flask,
    abort,
    flash,
    jsonify,
    redirect,
    render_template,
    request,
    send_from_directory,
    session,
    url_for,
)
from flask_babel import Babel, get_locale, gettext as _, ngettext
from werkzeug.security import check_password_hash, generate_password_hash

import manifest as manifest_lib
import media_library
import store as store_lib
from espuino_client import delete_rfid_on_device

DATA_DIR = os.environ.get("MEDIAHUB_DATA", "/data")
MEDIA_DIR = os.environ.get("MEDIAHUB_MEDIA", "/media")
LANGUAGES = ["de", "en", "fr"]

app = Flask(__name__)
app.config["BABEL_DEFAULT_LOCALE"] = "de"
store = store_lib.Store(DATA_DIR)


def _secret_key():
    """A one-time, persisted secret is enough for flash messages and the language cookie."""
    path = os.path.join(DATA_DIR, "secret.key")
    if not os.path.exists(path):
        os.makedirs(DATA_DIR, exist_ok=True)
        with open(path, "wb") as f:
            f.write(os.urandom(32))
    with open(path, "rb") as f:
        return f.read()


app.secret_key = _secret_key()


def _select_locale():
    lang = session.get("lang")
    if lang in LANGUAGES:
        return lang
    return request.accept_languages.best_match(LANGUAGES, "de")


babel = Babel(app, locale_selector=_select_locale)

# Flask-Babel only registers gettext/ngettext as Jinja globals; `_` (used
# throughout the templates) and `get_locale` (for <html lang> and the
# language switcher) need to be added explicitly.
app.jinja_env.globals["_"] = app.jinja_env.globals["gettext"]
app.jinja_env.globals["get_locale"] = get_locale


def _short_datetime(value):
    """"2026-07-15T13:56:58+00:00" -> "2026-07-15 13:56" — timestamps are
    always UTC (now_iso()), so the seconds/offset just add width without
    adding information for an at-a-glance table."""
    return value[:16].replace("T", " ") if value else value


app.jinja_env.filters["shortdt"] = _short_datetime


@app.route("/lang/<lang_code>")
def set_language(lang_code):
    if lang_code in LANGUAGES:
        session["lang"] = lang_code
    return redirect(request.args.get("next") or url_for("index"))


# --------------------------------------------------------------------------
# Optional hub password (admin UI only — the ESPuino-facing API stays open,
# see PUBLIC_ENDPOINTS below; concept §2/§19 originally ruled out auth
# entirely, revised to make it optional since the hub may be reachable
# beyond a single trusted household).
# --------------------------------------------------------------------------
PUBLIC_ENDPOINTS = {"login", "set_language", "health", "card_manifest", "serve_media", "static"}


@app.before_request
def _require_login():
    if request.endpoint in PUBLIC_ENDPOINTS or request.endpoint is None:
        return
    if store.get_settings().get("password_hash") and not session.get("authenticated"):
        return redirect(url_for("login", next=request.path))


@app.context_processor
def _inject_auth_state():
    return {"hub_password_set": bool(store.get_settings().get("password_hash"))}


@app.route("/login", methods=["GET", "POST"])
def login():
    password_hash = store.get_settings().get("password_hash")
    if not password_hash:
        return redirect(url_for("index"))
    next_url = request.values.get("next") or url_for("index")
    if request.method == "POST":
        if check_password_hash(password_hash, request.form.get("password", "")):
            session["authenticated"] = True
            return redirect(next_url)
        flash(_("Incorrect password."), "error")
    return render_template("login.html", next=next_url)


@app.route("/logout", methods=["POST"])
def logout():
    session.pop("authenticated", None)
    return redirect(url_for("index"))


# --------------------------------------------------------------------------
# Web UI: dashboard
# --------------------------------------------------------------------------
@app.route("/")
def index():
    cards = store.list_cards()
    devices = store.list_devices()
    pending_count = sum(1 for c in cards.values() if c["status"] == "pending")
    assigned_count = sum(1 for c in cards.values() if c["status"] == "assigned")
    total_bytes = sum(
        f["size"] for c in cards.values() if c["kind"] == "files" for f in c.get("files", [])
    )
    device_label = ngettext("%(num)s device", "%(num)s devices", len(devices)) % {"num": len(devices)}
    return render_template(
        "index.html",
        device_label=device_label,
        pending_count=pending_count,
        assigned_count=assigned_count,
        total_bytes=total_bytes,
    )


@app.route("/health")
def health():
    return jsonify(status="ok", service="espuino-mediahub")


# --------------------------------------------------------------------------
# Web UI: devices
# --------------------------------------------------------------------------
def _delete_device_confirm(esp_id, card_count):
    if not card_count:
        return _("Delete ESPuino %(esp_id)s?", esp_id=esp_id)
    return ngettext(
        "There is still %(num)s card assignment for this ESPuino, which must be "
        "deleted first. Delete it automatically? Note: cards are only removed "
        "locally in MediaHub.",
        "There are still %(num)s card assignments for this ESPuino, which must "
        "be deleted first. Delete them automatically? Note: cards are only "
        "removed locally in MediaHub.",
        card_count,
    ) % {"num": card_count}


@app.route("/devices")
def devices():
    devices_by_id = store.list_devices()
    rows = []
    for esp_id, dev in devices_by_id.items():
        card_count = store.count_cards_for_device(esp_id)
        rows.append((esp_id, dev, card_count, _delete_device_confirm(esp_id, card_count)))
    rows.sort(key=lambda row: row[1]["last_seen"], reverse=True)
    return render_template("devices.html", devices=rows)


@app.route("/devices/<esp_id>/alias", methods=["POST"])
def set_device_alias(esp_id):
    if esp_id not in store.list_devices():
        abort(404)
    store.set_device_alias(esp_id, request.form.get("alias", "").strip())
    flash(_("Alias saved."), "success")
    return redirect(url_for("devices"))


@app.route("/devices/<esp_id>/delete", methods=["POST"])
def delete_device(esp_id):
    if esp_id not in store.list_devices():
        abort(404)
    n = store.delete_device(esp_id)
    if n:
        flash(_("Device %(esp_id)s and its %(num)s card assignment(s) deleted (locally in MediaHub only).", esp_id=esp_id, num=n), "success")
    else:
        flash(_("Device %(esp_id)s deleted.", esp_id=esp_id), "success")
    return redirect(url_for("devices"))


# --------------------------------------------------------------------------
# Web UI: cards & assignments
# --------------------------------------------------------------------------
def _content_label(card):
    if card["kind"] == "webradio":
        return "📻 " + _("Webradio")
    n = len(card.get("files", []))
    return "📁 " + ngettext("%(num)s file", "%(num)s files", n) % {"num": n}


def _content_detail(card):
    """What's actually behind the label — one file's path, a shared folder
    if every file sits in the same one, or the full file list as a last
    resort. Shown truncated in the table with this as the hover tooltip."""
    if card["kind"] == "webradio":
        return card.get("stream_url") or ""
    files = card.get("files", [])
    if not files:
        return ""
    if len(files) == 1:
        return files[0]["path"]
    dirs = {os.path.dirname(f["path"]) for f in files}
    if len(dirs) == 1:
        return next(iter(dirs)) or "/"
    return ", ".join(f["path"] for f in files)


@app.route("/cards")
def cards():
    only_pending = request.args.get("pending") == "1"
    esp_id_filter = request.args.get("esp_id") or ""
    cards_by_key = store.list_cards()
    devices_by_id = store.list_devices()
    rows = [
        (
            c,
            _content_label(c),
            _content_detail(c),
            devices_by_id.get(c["esp_id"], {}).get("alias") or c["esp_id"],
        )
        for c in cards_by_key.values()
        if (not only_pending or c["status"] == "pending")
        and (not esp_id_filter or c["esp_id"] == esp_id_filter)
    ]
    rows.sort(key=lambda row: row[0]["last_seen"], reverse=True)
    return render_template(
        "cards.html",
        cards=rows,
        only_pending=only_pending,
        devices=devices_by_id,
        esp_id_filter=esp_id_filter,
        esp_id_filter_label=(devices_by_id.get(esp_id_filter, {}).get("alias") or esp_id_filter),
    )


@app.route("/cards/add", methods=["POST"])
def add_card():
    esp_id = request.form.get("esp_id", "").strip()
    card_id = request.form.get("card_id", "").strip()

    if not manifest_lib.is_valid_card_id(card_id):
        flash(_("Card ID must be exactly 12 digits."), "error")
        return redirect(url_for("cards"))

    if esp_id not in store.list_devices():
        flash(_("Please choose a known ESPuino."), "error")
        return redirect(url_for("cards"))

    existing = store.get_card(esp_id, card_id)
    if existing and existing["status"] == "assigned":
        # Not an error — "Add" doubles as "jump to this assignment's edit
        # form" — but flag it so a card ID typo doesn't silently overwrite
        # an already-configured assignment without the admin noticing.
        flash(
            _("Card %(id)s is already assigned on this ESPuino — opening it for editing.", id=card_id),
            "info",
        )
    return redirect(url_for("assign_card", esp_id=esp_id, card_id=card_id))


@app.route("/cards/<esp_id>/<card_id>/duplicate", methods=["POST"])
def duplicate_card(esp_id, card_id):
    source = store.get_card(esp_id, card_id)
    if source is None or source["status"] != "assigned":
        abort(404)

    target_esp_id = request.form.get("target_esp_id", "").strip()
    if target_esp_id == esp_id or target_esp_id not in store.list_devices():
        flash(_("Please choose a different, known ESPuino."), "error")
        return redirect(url_for("cards"))

    existing = store.get_card(target_esp_id, card_id)
    if existing and existing["status"] == "assigned":
        flash(
            _("Card %(id)s is already assigned on this ESPuino — opening it for editing.", id=card_id),
            "info",
        )
        return redirect(url_for("assign_card", esp_id=target_esp_id, card_id=card_id))

    store.save_assignment(
        target_esp_id,
        card_id,
        source["name"],
        source["kind"],
        source["play_mode"],
        source["stream_url"],
        source["files"],
    )
    flash(_("Card %(id)s duplicated to %(esp_id)s.", id=card_id, esp_id=target_esp_id), "success")
    return redirect(url_for("cards"))


@app.route("/cards/<esp_id>/<card_id>/assign", methods=["GET", "POST"])
def assign_card(esp_id, card_id):
    card = store.get_card(esp_id, card_id) or {
        "esp_id": esp_id,
        "card_id": card_id,
        "status": "pending",
        "name": "",
        "kind": "files",
        "play_mode": None,
        "stream_url": "",
        "files": [],
    }

    if request.method == "POST":
        name = request.form.get("name", "").strip()
        kind = request.form.get("kind", "files")

        if kind == "webradio":
            stream_url = request.form.get("stream_url", "").strip()
            if not stream_url:
                flash(_("A stream URL is required for webradio."), "error")
                return redirect(url_for("assign_card", esp_id=esp_id, card_id=card_id))
            store.save_assignment(esp_id, card_id, name, "webradio", None, stream_url, [])
        else:
            try:
                play_mode = int(request.form.get("play_mode", ""))
            except ValueError:
                play_mode = -1
            if not manifest_lib.is_valid_file_play_mode(play_mode):
                flash(_("Please choose a valid play mode."), "error")
                return redirect(url_for("assign_card", esp_id=esp_id, card_id=card_id))

            try:
                selected_paths = json.loads(request.form.get("selected_paths_json", "[]"))
            except ValueError:
                selected_paths = []

            files, missing = [], []
            for relpath in selected_paths:
                info = media_library.stat_and_hash(MEDIA_DIR, relpath)
                if info is None:
                    missing.append(relpath)
                else:
                    files.append(info)

            if missing:
                flash(
                    _(
                        "Some selected files could not be used (missing, or not "
                        "readable by the container's user) and were skipped: "
                        "%(paths)s",
                        paths=", ".join(missing),
                    ),
                    "error",
                )

            if not files:
                flash(_("At least one audio file is required."), "error")
                return redirect(url_for("assign_card", esp_id=esp_id, card_id=card_id))

            if play_mode in manifest_lib.SINGLE_FILE_PLAY_MODES and len(files) > 1:
                flash(_("This play mode only supports a single file — please select just one."), "error")
                return redirect(url_for("assign_card", esp_id=esp_id, card_id=card_id))

            files.sort(key=lambda f: f["path"])
            store.save_assignment(esp_id, card_id, name, "files", play_mode, None, files)

        flash(_("Card %(id)s assigned.", id=card_id), "success")
        return redirect(url_for("cards"))

    return render_template(
        "card_form.html",
        esp_id=esp_id,
        card_id=card_id,
        card=card,
        device=store.list_devices().get(esp_id),
        play_modes=manifest_lib.FILE_PLAY_MODES,
        single_file_play_modes=list(manifest_lib.SINGLE_FILE_PLAY_MODES),
        recursive_play_modes=list(manifest_lib.RECURSIVE_PLAY_MODES),
    )


@app.route("/cards/<esp_id>/<card_id>/force-refresh", methods=["POST"])
def force_refresh_card(esp_id, card_id):
    if store.bump_force_epoch(esp_id, card_id) is None:
        abort(404)
    flash(_("Force refresh triggered for card %(id)s.", id=card_id), "success")
    return redirect(url_for("cards"))


@app.route("/cards/force-refresh-all", methods=["POST"])
def force_refresh_all():
    n = store.bump_force_epoch_all()
    flash(_("Force refresh triggered for all %(num)s cards.", num=n), "success")
    return redirect(url_for("cards"))


@app.route("/cards/<esp_id>/<card_id>/delete", methods=["POST"])
def delete_card(esp_id, card_id):
    card = store.get_card(esp_id, card_id)
    if card is None:
        abort(404)

    delete_mode = store.get_settings()["delete_mode"]
    if delete_mode == "secure" and card["status"] == "assigned":
        # The assignment already names its one ESPuino — no more guessing
        # from whichever device last happened to tap the card.
        device = store.list_devices().get(esp_id)
        ip = device.get("ip") if device else None
        if not delete_rfid_on_device(ip, card_id):
            flash(
                _(
                    "Secure delete: ESPuino (%(esp_id)s) did not confirm the deletion. "
                    "Card was kept, please retry.",
                    esp_id=esp_id,
                ),
                "error",
            )
            return redirect(url_for("cards"))

    # Only the assignment (NVS-equivalent entry) is removed — the underlying
    # files belong to the admin's own media library, not to MediaHub, and
    # are never touched here.
    store.delete_card(esp_id, card_id)

    flash(_("Card %(id)s deleted (%(mode)s).", id=card_id, mode=delete_mode), "success")
    return redirect(url_for("cards"))


@app.route("/cards/<esp_id>/<card_id>/manifest-preview")
def manifest_preview(esp_id, card_id):
    """Admin-only preview of the manifest an ESPuino would receive for this
    card — same builder as card_manifest(), but without its side effects
    (no device/card "last seen" tracking, no pending-registration)."""
    card = store.get_card(esp_id, card_id)
    if card is None or card["status"] != "assigned":
        abort(404)
    files_base_url = url_for("serve_media", filename="", _external=True)
    return jsonify(manifest_lib.build_manifest(card_id, card, files_base_url))


# --------------------------------------------------------------------------
# Web UI: media (per-card storage usage)
# --------------------------------------------------------------------------
@app.route("/media")
def media_overview():
    cards_by_key = store.list_cards()
    rows = [
        (c, sum(f["size"] for f in c.get("files", [])))
        for c in cards_by_key.values()
        if c["kind"] == "files" and c.get("files")
    ]
    rows.sort(key=lambda row: row[1], reverse=True)
    return render_template("media.html", rows=rows)


@app.route("/media/browse")
def media_browse():
    """JSON directory listing under MEDIA_DIR, consumed by the card
    assignment form's inline file tree (static/js/media-browser.js)."""
    relpath = request.args.get("path", "")
    listing = media_library.list_directory(MEDIA_DIR, relpath)
    if listing is None:
        abort(404)
    return jsonify(path=relpath.strip("/"), dirs=listing["dirs"], files=listing["files"])


@app.route("/media/browse-recursive")
def media_browse_recursive():
    """Like /media/browse, but gathers audio files from subfolders too (up
    to the configured recursion depth) — used by "use folder" when the
    card's play mode is one of the recursive ones."""
    relpath = request.args.get("path", "")
    if media_library.list_directory(MEDIA_DIR, relpath) is None:
        abort(404)
    depth = store.get_settings().get("recursion_depth", store_lib.DEFAULT_RECURSION_DEPTH)
    files = media_library.list_audio_files_recursive(MEDIA_DIR, relpath, depth)
    return jsonify(path=relpath.strip("/"), files=files)


# --------------------------------------------------------------------------
# Web UI: settings
# --------------------------------------------------------------------------
@app.route("/settings", methods=["GET", "POST"])
def settings():
    if request.method == "POST":
        mode = request.form.get("delete_mode")
        if mode in ("lazy", "secure"):
            store.set_delete_mode(mode)

        try:
            depth = int(request.form.get("recursion_depth", ""))
        except ValueError:
            depth = -1
        if 0 <= depth <= 20:
            store.set_recursion_depth(depth)
        else:
            flash(_("Recursion depth must be between 0 and 20."), "error")
            return redirect(url_for("settings"))

        flash(_("Settings saved."), "success")
        return redirect(url_for("settings"))
    return render_template("settings.html", settings=store.get_settings())


@app.route("/settings/password", methods=["POST"])
def set_password():
    new_password = request.form.get("new_password", "")
    confirm_password = request.form.get("confirm_password", "")
    if not new_password:
        flash(_("Please enter a password."), "error")
    elif new_password != confirm_password:
        flash(_("Passwords do not match."), "error")
    else:
        store.set_password_hash(generate_password_hash(new_password))
        flash(_("Hub password set."), "success")
    return redirect(url_for("settings"))


@app.route("/settings/password/remove", methods=["POST"])
def remove_password():
    store.set_password_hash(None)
    session.pop("authenticated", None)
    flash(_("Hub password removed."), "success")
    return redirect(url_for("settings"))


# --------------------------------------------------------------------------
# MediaHub API (see mediahub-konzept.md §6/§7)
# --------------------------------------------------------------------------
@app.route("/<esp_id>/card/<card_id>/manifest.json")
def card_manifest(esp_id, card_id):
    ts = store_lib.now_iso()
    store.touch_device(esp_id, request.remote_addr, card_id, ts)

    card = store.get_card(esp_id, card_id)
    if card is None:
        store.register_pending(esp_id, card_id, ts)
        return jsonify(error="unknown_card", status="pending"), 404

    store.touch_card_seen(esp_id, card_id, ts)
    if card["status"] != "assigned":
        return jsonify(error="not_assigned", status="pending"), 404

    files_base_url = url_for("serve_media", filename="", _external=True)
    return jsonify(manifest_lib.build_manifest(card_id, card, files_base_url))


@app.route("/media/<path:filename>")
def serve_media(filename):
    return send_from_directory(MEDIA_DIR, filename)


if __name__ == "__main__":
    # Development only; the container runs Gunicorn instead (see Dockerfile).
    app.run(host="0.0.0.0", port=8080, debug=True)
