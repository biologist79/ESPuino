# ESPuino MediaHub

Leichtgewichtiger, **lokal** betriebener Hub zur zentralen Verwaltung der
RFID-Zuweisungen mehrerer ESPuinos. Konzept & Details: [../mediahub-konzept.md](../mediahub-konzept.md).

## Schnellstart

```bash
docker compose up --build
```

Dann [http://localhost:8080](http://localhost:8080) öffnen.

Für die lokale Entwicklung ohne Docker:

```bash
pip install -r requirements.txt
python app.py        # http://localhost:8080
```

## Stack

- **Python 3.12 + Flask** (im Container über Gunicorn), Image auf `python:3.12-slim`.
- **Frontend ohne Framework** — handgeschriebenes CSS im ESPuino-Look (blaue Kopfleiste, Logo). Offline-fähig, keine CDN-Abhängigkeiten.
- Persistente Daten unter `./data` (per Volume gemountet).

## Status

**Gerüst.** Die Web-UI-Shell steht (Navbar, Logo, Dashboard). Die eigentliche
Logik — Geräte-/Karten-Verwaltung, Per-Karte-Manifeste (`version` = SHA-256),
Medien-Auslieferung, `pending`-Registrierung, lazy/secure delete — folgt als
nächster Schritt. Die API-Routen sind in `app.py` als Stub angelegt.
