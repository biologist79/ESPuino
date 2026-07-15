# ESPuino MediaHub

Lightweight, **locally** run hub for centrally managing the RFID
assignments of multiple ESPuinos. Concept & details: [../mediahub-konzept.md](../mediahub-konzept.md).

## Quick start

The container runs as `www-data` (uid/gid `33:33`) rather than root, so
`./data` (the only thing it writes to) needs to be writable by that user
first. Point `./media` at wherever your existing audio library already
lives — it's mounted read-only and MediaHub never writes to it:

```bash
mkdir -p data
chown -R 33:33 data
docker compose up --build
```

By default `docker-compose.yml` maps a local `./media` folder; edit that
line to point at your actual library path instead, e.g. `/mnt/audiobooks:/media:ro`.

Then open [http://localhost:8080](http://localhost:8080).

For local development without Docker:

```bash
pip install -r requirements.txt
python app.py        # http://localhost:8080
```

## Stack

- **Python 3.12 + Flask** (served by Gunicorn in the container), image based on `python:3.12-slim`.
- **Framework-free frontend** — hand-written CSS in the ESPuino look (blue top bar, logo). Works offline, no CDN dependencies.
- Two volumes: `./data` (devices/cards/assignments in a single `db.json`, read-write) and `./media` (your own existing audio library, mounted read-only — MediaHub only browses and references it, it never copies or uploads files).
- Runs as non-root (`33:33` / `www-data`) by default; see [Quick start](#quick-start).
- **Multilingual** (DE/EN/FR) via Flask-Babel; language switcher top right, auto-detected via `Accept-Language`.
- **Optional password** for the web UI (Settings page) — the ESPuino-facing API stays open regardless, since devices can't log in.

## Feature overview

- **Devices** (`/devices`): ESPuinos that have contacted the hub (IP, last seen, last card).
- **Cards & Assignments** (`/cards`): overview, assign/edit — pick files/folders from the mounted library via an inline tree browser (`static/js/media-browser.js`, modeled after the ESPuino web UI's own SD explorer) or set a webradio stream URL — force refresh (per card/all), delete.
- **New Cards**: filter at `/cards?pending=1` — cards registered on tap but not yet assigned (see concept §5.3).
- **Media** (`/media`): storage usage per card; `/media/browse?path=` is the JSON API backing the tree browser.
- **Settings** (`/settings`): delete behavior lazy vs. secure (concept §13.1) — secure calls `DELETE /rfid` on the ESPuino and only removes the hub entry after a confirmed 200 response. Also: set/remove the optional hub password.
- **MediaHub API**: `GET /<espId>/card/<cardId>/manifest.json` (manifest, or `pending` registration), `GET /media/<path>` (media files, path relative to the library root).

## Maintaining translations

The source language for `_()`/`ngettext()` strings is English; German and French are catalog translations under `translations/<lang>/LC_MESSAGES/messages.po`. After text changes:

```bash
pybabel extract -F babel.cfg -o messages.pot .
pybabel update -i messages.pot -d translations   # update existing .po files
# fill in msgstr in translations/de/... and translations/fr/...
pybabel compile -d translations                  # only needed for local testing without Docker
```

The `.mo` files are compiled automatically at Docker build time (see Dockerfile) and are not committed.

## Status

Functional hub with device/card management, per-card manifests
(`version` = SHA-256, including the force-refresh lever), a library file/folder
browser for assignment (no uploads — files stay in place under `./media`),
`pending` registration, lazy/secure delete, and an optional web UI password.
Open (see `../mediahub-konzept.md` §15): the ESPuino-side implementation
(`MEDIAHUB` play mode, `MediaHub_EnsureCard`, LED download animation) is a
separate firmware topic not yet started.
