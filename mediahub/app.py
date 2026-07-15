"""ESPuino MediaHub — leichtgewichtiger lokaler Hub zur zentralen Verwaltung
der RFID-Zuweisungen mehrerer ESPuinos.

Gerüst: Diese Datei serviert das Web-UI im ESPuino-Look. Die eigentliche
Karten-/Manifest-/Medien-Logik (siehe ../mediahub-konzept.md) ist als Stub
angelegt und wird als nächster Schritt implementiert.
"""

import os

from flask import Flask, jsonify, render_template

DATA_DIR = os.environ.get("MEDIAHUB_DATA", "/data")

app = Flask(__name__)


# --------------------------------------------------------------------------
# Web-UI
# --------------------------------------------------------------------------
@app.route("/")
def index():
    return render_template("index.html")


@app.route("/health")
def health():
    return jsonify(status="ok", service="espuino-mediahub")


# --------------------------------------------------------------------------
# MediaHub-API (siehe mediahub-konzept.md §6/§7) — TODO: implementieren
# --------------------------------------------------------------------------
@app.route("/<esp_id>/card/<card_id>/manifest.json")
def card_manifest(esp_id, card_id):
    # TODO §5.3/§7: bekannte, zugewiesene Karte -> Per-Karte-Manifest
    #   (inkl. version = SHA-256 des kanonischen Manifests).
    # Unbekannte Karte -> als "pending" registrieren und 404 zurückgeben.
    return jsonify(error="not implemented", esp_id=esp_id, card_id=card_id), 501


if __name__ == "__main__":
    # Nur für die lokale Entwicklung; im Container läuft Gunicorn (siehe Dockerfile).
    app.run(host="0.0.0.0", port=8080, debug=True)
