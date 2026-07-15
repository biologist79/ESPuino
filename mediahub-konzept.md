# MediaHub — Detailspezifikation (Entwurf 1)

*Stand: 15. Juli 2026 · Branch `feature-mediahub` · Status: Hub-Server (`mediahub/`) implementiert und getestet; ESPuino-Firmware-Seite noch offen (§15)*

## 1. Worum es geht

Manche Nutzer betreiben **mehrere ESPuinos** im Haushalt und möchten die **RFID-Zuweisungen zentral verwalten**, statt jede Karte an jedem Gerät einzeln anzulernen. Beispiel: Zwei Kinder tauschen Karten, und auf jedem Gerät passiert dasselbe.

Der **MediaHub** ist dafür ein **lokal betriebener Docker-Container** (kein Cloud-Dienst) mit einem ansprechenden Web-Interface. Dort verwaltet man pro ESPuino, welche Karte welchen Inhalt abspielt. Die ESPuinos holen sich diese Informationen und die zugehörigen Mediendateien selbstständig.

Es ist ein Feature für wenige Power-User; wer es nicht nutzt, bemerkt nichts davon.

## 2. Nicht-Ziele

- **Keine Cloud.** Der Hub läuft lokal beim Nutzer (Docker).
- **Nichts wird auf der RFID-Karte gespeichert.** Wir arbeiten ausschließlich mit der ausgelesenen 12-stelligen Nummer.
- **Keine Änderung des NVS-Speicherformats.**
- **Kein Streaming der eigenen Mediathek.** Datei-Inhalte werden auf die SD geladen und lokal abgespielt. (Webradio-Zuweisungen werden hingegen direkt gestreamt — siehe §7.2.)
- **Kein Blockieren der Wiedergabe durch Netzwerk-Timeouts** (Offline-/Unterwegs-Tauglichkeit ist Pflicht).
- **Keine Authentifizierung der ESPuino-Endpunkte** (§6). Manifest- und Medien-Abruf bleiben unauthentifiziert, da die Geräte sich nicht anmelden können. Für das Hub-Web-UI selbst gibt es optional ein Passwort (§5.4) — das ist eine reine Zugriffs-Absicherung der Verwaltungsoberfläche, keine Auth zwischen ESPuino und Hub.

## 3. Grundprinzipien

1. **Lokal zuerst.** Ist der Inhalt lokal vorhanden und plausibel (Größe), wird **sofort** abgespielt — ohne Netzwerkzugriff. Das muss auch unterwegs (Auto, kein Hub erreichbar) funktionieren.
2. **Nie blockierende Lookups.** Ein nicht erreichbarer Hub darf **niemals** zu langen Timeouts führen. Hintergrundaktionen sind erlaubt, dürfen aber nichts blockieren.
3. **Integrität lokal über Größe, beim Download über SHA-256.** Ein vollständiges Neu-Hashen lokaler (u. U. 100 MB großer) Dateien auf dem ESP32 ist keine Option und findet **nie** statt.
4. **Der Hub ist die Quelle der Wahrheit** für playMode und Dateiliste. Wo die Dateien auf der SD landen, bestimmt der ESPuino selbst (fester Medien-Root, §13.1).
5. **Robuster Download.** Immer erst in `.tmp`, erst nach erfolgreicher Verifikation an den Zielort verschieben. Ein abgebrochener Download hinterlässt nie eine kaputte „echte" Datei.

## 4. Architektur im Überblick

Es gibt genau **zwei Beteiligte** — den **MediaHub** und den **ESPuino** — mit **zwei Datenströmen** dazwischen. Der ESPuino **holt** (pull) beides bei Bedarf vom Hub und legt es auf seiner SD-Karte ab:

- **① Steuerungs-Ebene — Metadaten:** die kleinen **Manifeste** (welche Karte hat welchen Inhalt: Dateiliste, Größen, Hashes, `version`).
- **② Daten-Ebene — Inhalte:** die eigentlichen **Mediendateien**.

Die Zuweisung selbst (Kartennummer → Hub-Adresse) liegt unverändert im **NVS**; der heruntergeladene Inhalt und der Manifest-Cache liegen auf der **SD-Karte**.

```mermaid
flowchart LR
    subgraph HUB["MediaHub – lokaler Docker-Container"]
        WUI["Web-UI:<br/>Zuweisungen pro ESPuino verwalten"]
        API["Endpoint:<br/>manifest.json"]
        MED["Mediendateien"]
    end

    subgraph ESP["ESPuino"]
        NVS["NVS<br/>cardId ↦ mediahub://host:port<br/>(Format unverändert)"]
        SD["SD-Karte<br/>/.mediahub/manifests/ (Cache)<br/>/.mediahub/media/&lt;cardId&gt;/ (Medien)"]
    end

    API -->|"① Steuerungs-Ebene: Manifeste (Metadaten)"| SD
    MED -->|"② Daten-Ebene: Mediendateien"| SD
```

Der Abgleich passiert ausschließlich **pro Karte, beim Auflegen** — gekapselt in der Kernoperation `MediaHub_EnsureCard(cardId)` (siehe §10). Einen geräteweiten Bulk-Sync gibt es bewusst **nicht**.

## 5. NVS-Integration (ohne Formatänderung)

Der NVS-Wert bleibt unverändert `#`-getrennt mit exakt vier Feldern: `#<pfad>#<lastPlayPos>#<playMode>#<trackLastPlayed>` (Parser in `RfidCommon.cpp`, Schreiber `AudioPlayer_NvsRfidWriteWrapper()` in `AudioPlayer.cpp` — siehe §8.1 zu den beiden mittleren/letzten Feldern). Für eine MediaHub-Karte steht im **Pfad-Feld** ein eigenes Schema statt eines SD-Pfads:

```text
mediahub://<host:port>
```

- Ein **eigenes Schema** ist nötig, weil der AudioPlayer einen Pfad, der mit `http` beginnt, als **Webradio** interpretiert (`strncmp("http", …, 4)` in `AudioPlayer.cpp`). `mediahub://` kollidiert damit nicht.
- Die Basis-Adresse ist für **alle** Karten identisch → „Karte einschreiben" heißt einfach: Pfad-Feld auf die Hub-Basis setzen (vom Hub aus bulk-verteilbar).
- `<espId>` (Hostname/MAC) kennt das Gerät selbst, `<cardId>` ist der NVS-Schlüssel → ESPuino baut daraus die konkrete Manifest-URL.
- Der `playMode`-Token im NVS wird auf den **neuen Wert `MEDIAHUB`** gesetzt (siehe §8). Er ist damit zugleich der **eindeutige Marker**, dass es sich um eine MediaHub-Karte handelt; der *tatsächliche* playMode für die Wiedergabe kommt aus dem Manifest.
- **Aktivierung rein zur Laufzeit — kein Compile-Flag.** Der MediaHub-Code ist immer im Firmware-Image, wird aber nur aktiv, wenn eine Karte eine `mediahub://`-Adresse trägt. Wer das Feature nicht nutzt, zahlt keine Laufzeitkosten. (Die schweren Abhängigkeiten — HTTP-Client, SHA, JSON — sind ohnehin für OTA/Webradio/Web-UI im Image.)

### 5.1 Registrierte Medienserver (Komfort im Web-UI)

Die Karten-ID lässt sich nur mit einem Kartenleser ermitteln — das Zuweisen passiert daher meist direkt am **ESPuino-Web-Interface** (Karte auflegen → ID wird automatisch ausgefüllt). Damit man dort nicht jedes Mal die `mediahub://`-URL von Hand eintippt, kann man im Web-Interface **Medienserver registrieren**:

- Eine kleine, gerätelokale Liste bekannter Hubs — je Eintrag ein **Anzeigename** + **Basis-Adresse** (`host:port`). Analog zur bestehenden Verwaltung „Netzwerke" (WLAN).
- Gespeichert als **eigener Settings-Eintrag** (NVS) — **nicht** im RFID-Zuweisungsformat. Die Vorgabe „NVS-Zuweisungsformat unverändert" bleibt gewahrt.
- Bei der RFID-Zuweisung wählt man den Server aus einem **Dropdown**; das Pfad-Feld wird automatisch mit `mediahub://<host:port>` befüllt.
- Ist nur ein Server registriert, wird er vorausgewählt. Optional kann das Web-UI die Erreichbarkeit des Hubs prüfen (kurzer Test-Request).

Das ist reine Enrollment-Ergonomie; der Laufzeit-Ablauf (§11) bleibt unberührt.

### 5.2 Wie Zuweisungen ins NVS kommen

Damit eine Karte überhaupt bekannt ist (Voraussetzung für den Offline-Fall), muss im NVS ein Eintrag `cardId → mediahub://host:port` liegen. Dieser Eintrag ist **uniform** — die inhaltlichen Details stehen im Manifest, nicht im NVS.

Der **einzige** Weg dorthin ist das **manuelle Anlernen (§5.1)**: Karte am ESPuino auflegen, Hub-Server wählen, speichern. Einen automatischen Bulk-Sync vom Hub gibt es bewusst **nicht** — die „diese Karte → dieser Hub"-Verdrahtung macht man pro Gerät von Hand. Die *Inhalte* (Dateiliste, playMode) bleiben davon unberührt zentral am Hub verwaltet und kommen on-demand beim Auflegen.

**Offline-Klarstellung:** Ein NVS-Eintrag macht eine Karte *bekannt*, aber noch nicht *abspielbar*. Dafür müssen zusätzlich das gecachte Manifest **und** die Mediendateien lokal liegen — was durch **einmaliges Auflegen zu Hause** passiert (Mechanismus a lädt Manifest + Dateien). Erst danach funktioniert die Karte offline (Auto).

### 5.3 Registrierung der Karte beim Hub (beim Auflegen)

Die Karten-ID entsteht zwangsläufig zuerst am ESPuino (er ist der Leser) und muss von dort zum Hub. Dieser Push passiert **beim Auflegen der Karte**, nicht beim Anlernen im Web-UI:

- Das **Anlernen (§5.1)** schreibt nur den NVS-Eintrag (`mediahub://…`, `MEDIAHUB`) — **kein** Hub-Kontakt.
- Beim **Auflegen** fragt der ESPuino das Manifest beim Hub an. Kennt der Hub das **(espId, cardId)-Paar** noch nicht, **registriert er es dabei** (legt die Zuweisung als noch-nicht-konfiguriert an, damit der Admin sie im Hub-UI sieht) und antwortet negativ. Der ESPuino zeigt einen **Fehler** — dem Bediener ist klar, dass diese Kombination neu ist.
- Danach weist der Admin am Hub-Web-UI Inhalt zu; beim nächsten Auflegen kommt das Manifest, und die Karte spielt (nach Download).

Wichtig: Die Zuweisung gehört zu **genau diesem** ESPuino (§5.5) — dasselbe Kartentapping auf einem zweiten, dem Hub noch unbekannten Gerät registriert dort **unabhängig davon** eine eigene, separate Zuweisung.

**Warum beim Auflegen statt beim Anlernen:** So ist der Zeitpunkt des ID-Pushs frei wählbar — einfach Karte auflegen. Käme die Registrierung beim Anlernen, müsste man zum erneuten Pushen (z. B. nach einem Hub-Reset) die Karte neu anlernen. Durch die Kopplung ans Auflegen genügt ein erneutes Auflegen.

Einrichtungs-Ablauf end-to-end:

```text
1. Web-UI: Karte auflegen, Hub-Server wählen, Speichern  → NVS: cardId → mediahub://…, MEDIAHUB (kein Hub-Kontakt)
2. Karte zum Abspielen auflegen                          → Hub-Anfrage; Hub kennt sie nicht → registriert + Fehler
3. Am Hub-Web-UI Inhalt zuweisen                         → Manifest existiert
4. Karte erneut auflegen                                 → Manifest → Download → spielen
```

### 5.4 Hub-Web-UI: Medienbibliothek statt Upload, optionales Passwort

**Kein Upload — die Bibliothek wird eingebunden.** Der Hub speichert selbst keine Mediendateien und bietet auch keinen Upload dafür an. Stattdessen mountet der Admin seine bereits vorhandene Musik-/Hörbuch-Sammlung read-only in den Container (`./media:/media:ro`, siehe Docker-Compose). Beim Zuweisen einer Karte wählt der Admin Dateien bzw. einen ganzen Ordner aus dieser Bibliothek über einen im Web-UI eingebetteten Datei-Browser aus — angelehnt an den bestehenden SD-Explorer im ESPuino-Web-Interface (Baumansicht, Ordner vor Dateien, Auswahl per Klick), aber schlicht in Vanilla-JS statt jsTree/jQuery, um ohne zusätzliche Frontend-Abhängigkeiten auszukommen.

- Größe und SHA-256 der ausgewählten Dateien berechnet der Hub beim Speichern der Zuweisung direkt von der eingebundenen Bibliothek — nicht beim Hochladen, da es keinen Upload gibt.
- `filesBaseUrl` im Manifest (§7.1) ist für alle Karten identisch (Wurzel der Bibliothek); `files[].path` ist relativ zu dieser Wurzel, nicht mehr pro Karte isoliert.
- **Löschen einer Zuweisung löscht nie Dateien in der Bibliothek** — der Hub besitzt sie nicht, er referenziert sie nur. Das gilt für lazy- wie secure-delete gleichermaßen (§13.1); beide räumen ausschließlich den Hub-eigenen Eintrag (und ggf. den ESPuino-Cache) auf, nie die Quelldateien.
- Mehrere Karten dürfen denselben Ordner/dieselben Dateien referenzieren (z. B. dieselbe Geschichte für zwei Kinder-Karten).
- Der „Ordner verwenden"-Button im Datei-Browser sammelt standardmäßig nur die Dateien der gewählten Ebene. Bei den **rekursiven** Abspielmodi (`AUDIOBOOK_RECURSIVE`, `ALL_TRACKS_OF_DIR_SORTED_RECURSIVE`, `ALL_TRACKS_OF_DIR_RANDOM_RECURSIVE`) steigt er stattdessen in Unterordner ab — begrenzt durch eine in den Einstellungen konfigurierbare **Rekursionstiefe** (Default 3, Bereich 0–20), damit eine riesige Bibliothek nicht versehentlich ein unbegrenzt großes Manifest erzeugt. Nicht-rekursive Modi ignorieren diese Einstellung und nehmen immer nur die direkte Ordnerebene.
- Ein **„Manifest"-Button** je zugewiesener Karte zeigt im Hub-Web-UI das exakte JSON, das ein ESPuino für diese Karte bekäme (derselbe Manifest-Builder wie §7.1, aber ohne die Seiteneffekte des echten Endpunkts wie Geräte-Tracking oder Pending-Registrierung) — reine Debugging-/Kontroll-Ergonomie.

**Optionales Passwort fürs Hub-Web-UI.** Über die Einstellungen-Seite kann der Admin ein Passwort setzen, ändern oder wieder entfernen; gespeichert wird nur ein gesalzener Hash (nie das Klartext-Passwort) in der JSON-DB. Ist ein Passwort gesetzt, verlangt jede Web-UI-Seite eine Anmeldung (Session-Cookie). Die ESPuino-Endpunkte (§6) bleiben davon **unberührt** — sie sind technisch nicht absicherbar, da die Geräte sich nicht anmelden können, und das war ohnehin nie das Bedrohungsmodell (§2).

### 5.5 Zuweisungs-Identität: ESPuino + Karten-ID, nicht Karte allein

**Problem.** Eine Karte kann legitim auf mehreren ESPuinos bekannt sein (genau der Zweck des Features: Kinder tauschen Karten zwischen Geräten). Würde der Hub Zuweisungen nur nach `cardId` führen, gäbe es zu einer Karte nur *einen* Datensatz — welchen der ggf. mehreren Geräte-IPs sollte dann z. B. Secure Delete (§13.1) anrufen? Ein früherer Entwurf löste das über „das zuletzt gesehene Gerät", was bei mehreren Geräten pro Karte falsch bzw. unvollständig ist (siehe Entscheidung #27 unten).

**Lösung.** Der Schlüssel einer Zuweisung im Hub ist das Paar **(espId, cardId)**, nicht die Karten-ID allein:

- Man kann **dieselbe** 12-stellige Karten-ID mehrfach anlegen — einmal pro ESPuino. Jede (espId, cardId)-Kombination ist ein eigener, unabhängiger Datensatz mit eigenem Namen, Abspielmodus/Dateien und eigenem Lifecycle.
- Beim manuellen Anlegen im Hub-Web-UI (§5.3) wählt der Admin daher **zusätzlich zur Karten-ID auch das Ziel-ESPuino** aus einem Dropdown — befüllt mit allen Geräten, die sich beim Hub schon mindestens einmal gemeldet haben (jede Manifest-Anfrage registriert das anfragende Gerät, auch bei unbekannten Karten, siehe §5.3).
- Damit ist **Secure Delete eindeutig**: Eine Zuweisung kennt ihr ESPuino direkt, keine Heuristik über „zuletzt gesehen" mehr nötig.
- Ein **„Duplicate"-Button** je zugewiesener Karte ist die Ergonomie-Abkürzung für genau diesen Fall: Er übernimmt Name, Abspielmodus/Dateien bzw. Stream-URL unverändert in eine neue, unabhängige Zuweisung für ein anderes (bereits bekanntes) ESPuino — spart das manuelle Nachbauen der Konfiguration, wenn dieselbe Karte auf einem zweiten Gerät genauso funktionieren soll.

**Geräte-Alias-Liste.** Die vom ESPuino gesendete `espId` ist vermutlich MAC-Adresse oder Hostname — für Menschen nicht schön lesbar. Der Hub führt daher pro Gerät ein optionales, frei vergebbares **Alias** (z. B. „Kinderzimmer"), gepflegt auf der Geräte-Seite. Das Alias ist reine Anzeige-Ergonomie im Hub-UI (Dropdown, Kartenliste) und hat keine Wirkung auf Protokoll oder NVS.

**Gerät löschen.** Der Admin kann einen Geräte-Eintrag (Alias, IP, „zuletzt gesehen") im Hub-UI wieder entfernen. Da Zuweisungen an die `espId` gebunden sind, fragt der Hub dabei ab, wie viele Zuweisungen betroffen sind, und lässt sie **auf Bestätigung mit löschen** (kaskadierend) — das betrifft ausdrücklich nur den Hub-eigenen Datensatz, nie den ESPuino selbst oder dessen NVS/SD-Cache. Meldet sich das Gerät später erneut (Tap), wird es beim nächsten Manifest-Request automatisch neu registriert.

## 6. Endpunkte

```text
Manifest (pro Karte)   GET  http://<host:port>/<espId>/card/<cardId>/manifest.json
Mediendateien          GET  <filesBaseUrl>/<files[].path>
```

Ein Manifest-Request beim Auflegen dient zugleich der **Registrierung** (§5.3): Kennt der Hub die Karte noch nicht, legt er sie als noch-nicht-zugewiesen an und antwortet negativ. Ein separater Register-Endpoint ist damit nicht nötig.

## 7. Manifest-Format

### 7.1 Per-Karte-Manifest

```json
{
  "schema": 1,
  "cardId": "012345678901",
  "version": "9f86d081884c7d65...",
  "name": "Benjamin Blümchen – Folge 12",
  "playMode": 3,
  "filesBaseUrl": "http://192.168.1.50:8080/media/",
  "files": [
    { "path": "Benjamin Blümchen/Folge 12/01.mp3", "size": 5432101, "sha256": "9f86d0..." },
    { "path": "Benjamin Blümchen/Folge 12/02.mp3", "size": 4998210, "sha256": "ef01ab..." }
  ]
}
```

(`cardId` ist immer 12 Dezimalstellen — 4 UID-Bytes, je als `%03d` formatiert, siehe `cardIdSize`/`cardIdStringSize` in `src/Rfid.h`.)

| Feld | Bedeutung / Verwendung auf dem ESPuino |
| --- | --- |
| `schema` | Format-Version für Vorwärtskompatibilität. |
| `cardId` | Gegenprüfung, dass das Manifest zur aufgelegten Karte gehört. |
| `version` | **Opaker Änderungsstempel = SHA-256 des kanonischen Manifests, berechnet auf dem Hub.** Der ESPuino rechnet hier nichts, er **vergleicht nur Strings**. Grundlage der Änderungserkennung. Da es ein Content-Hash ist, kann der Hub das „Hochzählen" nicht vergessen. |
| `name` | Optional, nur für Logs/Web-UI. |
| `playMode` | ESPuino-`playMode`-Enum (`values.h`), z. B. 3 = Hörbuch. **Quelle der Wahrheit ist der Hub.** |
| `filesBaseUrl` | Download-Basis. Download-URL je Datei = `filesBaseUrl + path`. Entkoppelt Medien-Ablage vom Manifest — auf dem Hub identisch für alle Karten, da `path` relativ zur Wurzel der (eingebundenen) Medienbibliothek ist, nicht mehr pro Karte isoliert (§5.4). |
| `files[].path` | Relativ zur Bibliotheks-Wurzel des Hubs. Lokal auf dem ESPuino landet die Datei trotzdem unter `/.mediahub/media/<cardId>/` + `path` — kein `target` nötig, die Ablage dort bestimmt weiterhin der ESPuino selbst (§13.1). |
| `files[].size` | **Schneller lokaler Integritätscheck.** |
| `files[].sha256` | **Nur inkrementell während des Downloads** geprüft (HW-beschleunigt, WiFi ist der Flaschenhals). **Nie** über lokale Dateien neu berechnet. |

Den Gesamt-Fortschritt (Progress-Balken) berechnet der ESPuino aus der Summe der `files[].size` selbst.

### 7.2 Webradio-Variante

Trägt eine Karte einen Webradio-Sender, gibt es **keine Dateien** herunterzuladen — das Manifest beschreibt dann nur den Stream. Unterschieden wird am `playMode` (WEBSTREAM = 8):

```json
{
  "schema": 1,
  "cardId": "012345678901",
  "version": "...",
  "playMode": 8,
  "name": "Radio Beispiel",
  "stream": "http://radio.example/stream.mp3"
}
```

- Kein `target`, kein `filesBaseUrl`, kein `files`.
- ESPuino liest das Manifest, erkennt `playMode = WEBSTREAM` und übergibt dem AudioPlayer direkt die `stream`-URL — kein Download, keine Integritätsprüfung.
- Die `version`-/Änderungslogik gilt weiterhin (die Stream-URL kann sich zentral ändern).
- **Offline nicht abspielbar** — Webradio braucht prinzipbedingt Netz.

Gemischte `LOCAL_M3U`-Listen (SD-Dateien und Webstreams gemischt) unterstützt MediaHub **nicht**.

## 8. playMode: `MEDIAHUB`-Marker im NVS, echter Modus aus dem Manifest

MediaHub bekommt einen **neuen playMode-Wert `MEDIAHUB`** (zusätzlicher Eintrag in `values.h`, nächster freier Wert, z. B. 18). Im NVS steht für eine MediaHub-Karte immer `playMode = MEDIAHUB`. Das ist zugleich der **eindeutige Marker** für die Dispatch-Logik in `RfidCommon.cpp`: Erkennt sie `MEDIAHUB`, übergibt sie an MediaHub (Manifest auflösen), **bevor** die normale AudioPlayer-Logik greift (insbesondere die `http`→Webradio-Erkennung).

Der **tatsächliche** playMode für die Wiedergabe (Hörbuch, Einzeltitel, Webstream …) kommt aus dem **Manifest**. Vorteil: Eine zentrale Änderung (z. B. „Einzeltitel" → „Hörbuch") propagiert über den normalen Manifest-Refresh, ohne das NVS anzufassen.

Ein „Fallback-playMode" im NVS entfällt damit: `MEDIAHUB` allein ist nicht abspielbar — ohne Manifest weiß der ESPuino nicht, *was* zu spielen ist. Das entspricht genau dem gewünschten Verhalten (das erste Auflegen braucht das Manifest).

### 8.1 Play-Position-Persistenz für Hörbuch-Manifeste

Die Hörbuch-Modi (`AUDIOBOOK`, `AUDIOBOOK_LOOP`, `AUDIOBOOK_RECURSIVE`) merken sich die Abspielposition (`lastPlayPos`, Sekunden) und den zuletzt gespielten Track-Index (`trackLastPlayed`) — beides Felder desselben NVS-Eintrags (§5), nicht separat abgelegt. Das muss für MediaHub-Karten mit einem Hörbuch-artigen Manifest-`playMode` genauso funktionieren, hat aber einen Fallstrick:

**Das Problem.** `AudioPlayer_NvsRfidWriteWrapper()` (`AudioPlayer.cpp`) — die einzige Stelle, die `lastPlayPos`/`playMode`/`trackLastPlayed` ins NVS zurückschreibt (ausgelöst bei Pause, Track-/Ordnerwechsel, Playlist-Ende, Shutdown) — bekommt den zu schreibenden `playMode` an jeder Aufrufstelle als `gPlayProperties.playMode` übergeben. Das ist der **tatsächliche, gerade abgespielte** Modus (z. B. `AUDIOBOOK`), nicht der NVS-Marker `MEDIAHUB`. Ohne Gegenmaßnahme würde also schon die erste Pause/der erste Trackwechsel einer MediaHub-Hörbuch-Karte das NVS-`playMode`-Feld stillschweigend von `MEDIAHUB` auf `AUDIOBOOK` überschreiben — beim nächsten Auflegen erkennt `RfidCommon.cpp` die Karte dann nicht mehr als MediaHub-Karte (der Marker ist weg) und versucht, `mediahub://host:port` als echten SD-Pfad zu öffnen. Bricht komplett.

**Der Fix — an einer einzigen Stelle.** `AudioPlayer_NvsRfidWriteWrapper()` liest ohnehin schon den aktuellen NVS-Wert, um das Pfad-Feld unverändert zu erhalten (`firstPart`). Sie muss dabei zusätzlich prüfen, ob dieses Pfad-Feld mit `mediahub://` beginnt — falls ja, wird der übergebene `playMode`-Parameter vor dem Schreiben auf `MEDIAHUB` überschrieben, unabhängig davon, welcher echte Modus gerade lief. `lastPlayPos` und `trackLastPlayed` werden ganz normal mit den echten Werten geschrieben — nur das `playMode`-Feld bleibt für MediaHub-Karten dauerhaft der Marker.

**Der Rest fügt sich ohne weitere Änderungen an AudioPlayer.cpp/System.cpp:**

- Beim Auflegen liest `Rfid_PreferenceLookupHandler()` die vier Felder ohnehin generisch, unabhängig vom `playMode`-Wert — `lastPlayPos`/`trackLastPlayed` stehen also bereits zur Verfügung, bevor die MediaHub-Dispatch-Logik (§11) überhaupt greift.
- Erkennt `RfidCommon.cpp` `MEDIAHUB`, reicht sie diese beiden Werte an `MediaHub_EnsureCard` (§10) durch. Nach erfolgreichem Sync ruft der ESPuino `AudioPlayer_SetPlaylist(<lokaler /.mediahub/media/cardId-Pfad>, lastPlayPos, <echter playMode aus Manifest>, trackLastPlayed)` auf — mit dem **echten** Modus aus dem Manifest, nicht dem Marker, und dem lokalen SD-Pfad, nicht der `mediahub://`-Zeichenkette.
- Ab hier ist es für `AudioPlayer_SetPlaylist` ein ganz gewöhnliches Hörbuch: Der bestehende Switch setzt `saveLastPlayPosition = true` für die drei Hörbuch-Modi wie eh und je, und der literale Playmode-Vergleich im Shutdown-Flush (`AudioPlayer_Exit()`) greift ebenso — beide kennen MediaHub nicht und müssen es auch nicht kennen.
- Nicht-Hörbuch-Manifest-Modi (z. B. `SINGLE_TRACK`) setzen `saveLastPlayPosition` schon heute nicht — für sie ändert sich nichts, `lastPlayPos`/`trackLastPlayed` bleiben unbenutzt bei `0`.

## 9. Integrität, Änderungserkennung, Force Refresh

- **Lokaler Integritätscheck:** ausschließlich über **Dateigröße**. Schnell, kein Hashing großer Dateien.
- **Download-Verifikation:** SHA-256 **inkrementell** während des Downloads gegen `files[].sha256`. Fängt abgeschnittene/korrupte Downloads ab, die zufällig die richtige Größe hätten.
- **Änderungserkennung:** über `version`. Der Hintergrund-Check nach dem Start vergleicht die lokal gecachte `version` mit dem frisch geholten Manifest. Weicht sie ab, wird die Karte als **„stale" markiert** — das Update passiert **beim nächsten Auflegen** (§11), nicht mitten in der Wiedergabe.
- **Force Refresh (Escape-Hatch):** ignoriert den lokalen Zustand und lädt neu — deckt die bewusste Schwäche des Größen-Checks ab (gleiche Größe, anderer Inhalt). Technisch: gecachte `version` (und optional die lokalen Dateien) verwerfen → normaler Download-Pfad greift.
  - **pro Karte** (Button im Hub-UI / Admin-Karte)
  - **für alle Karten** (alle gecachten `version`-Werte verwerfen → jede Karte lädt beim nächsten Auflegen neu)

## 10. Kernoperation `MediaHub_EnsureCard`

Die Primitive kapselt Manifest-Beschaffung, lokalen Integritätscheck und Download. Sie läuft **im Vordergrund beim Auflegen** (mit Play + Fortschritt). Der Hintergrund-`version`-Check danach lädt **nichts** — er markiert bei einer Änderung nur „stale" (§9, §11).

```text
MediaHub_EnsureCard(cardId, {showProgress}):
    manifest ← Cache /.mediahub/manifests/<cardId>.json
    wenn kein Cache und Hub erreichbar:
        manifest ← GET .../manifest.json;  Cache schreiben
    wenn immer noch kein Manifest:
        return NICHT_VERFUEGBAR            // offline & nie geladen

    wenn manifest ist Webradio (playMode WEBSTREAM):
        return BEREIT                       // nichts herunterzuladen; Wiedergabe streamt direkt

    mediaDir ← /.mediahub/media/<cardId>/
    wenn Karte als "stale" markiert:
        mediaDir komplett leeren            // Wipe → sauberer Neuaufbau

    fehlend ← alle files, deren lokale Datei fehlt oder deren Größe ≠ manifest.size
    wenn fehlend leer:
        return BEREIT                       // sofort spielbar

    wenn Hub nicht erreichbar:
        return UNVOLLSTAENDIG               // z. B. offline, Teil-Ordner

    für jede Datei in fehlend:
        download → mediaDir/<path>.tmp
        SHA-256 inkrementell prüfen (gegen files[].sha256)
        bei Erfolg: move .tmp → mediaDir/<path>;  Fortschritt anzeigen (wenn showProgress)
        bei Fehler/Abbruch: .tmp entfernen; Karte "needs resync" markieren; return FEHLER
    "stale"/"needs resync" löschen
    return BEREIT
```

- **Vordergrund (beim Auflegen):** `EnsureCard(card, showProgress=true)`; bei `BEREIT` folgt die Wiedergabe.
- **Danach:** ein **leichter Hintergrund-`version`-Check** (nur Manifest holen + vergleichen, kein Download). Bei Änderung → Karte „stale" markieren; das eigentliche Update macht der **nächste** Auflege-Vorgang (Wipe + Neuladen).

## 11. Ablauf beim Kartenauflegen (Mechanismus a)

```mermaid
flowchart TD
    A([Karte aufgelegt]) --> B[12-stellige cardId auslesen]
    B --> C{NVS-Eintrag<br/>vorhanden?}
    C -- nein --> Z[Unbekannte Karte:<br/>Verhalten wie heute]
    C -- ja --> D{playMode = MEDIAHUB?}
    D -- nein --> P[Normale Wiedergabe wie heute<br/>SD-Pfad / Webradio / Modifikation]
    D -- ja --> ST{Karte "stale"<br/>und Hub erreichbar?}
    ST -- ja --> RS[Re-Sync im Vordergrund:<br/>neues Manifest, Ordner wipen,<br/>neu laden, Ring-Fortschritt]
    RS --> PLAY
    ST -- nein --> E[Manifest bestimmen<br/>Cache: /.mediahub/manifests/cardId.json]
    E --> F{Cache<br/>vorhanden?}
    F -- nein --> H{Hub<br/>erreichbar?}
    H -- nein --> ERR[Kurze Fehler-Anzeige<br/>NICHT blockieren]
    H -- ja --> I[Manifest beim Hub anfragen]
    I --> MM{Antwort?}
    MM -->|"unbekannt / nicht zugewiesen"| ERR3[Fehler-Anzeige<br/>Hub registriert Karte<br/>für spätere Zuweisung]
    MM -->|OK| IC[cachen]
    IC --> G
    F -- ja --> G[Manifest ausgewertet]
    G --> S{Webradio?<br/>playMode = WEBSTREAM}
    S -- ja --> STR[[Stream-URL direkt abspielen<br/>braucht Netz]]
    S -- nein --> J{Alle Dateien lokal<br/>und Größe passt?}
    J -- ja --> PLAY[[Sofort abspielen]]
    J -- nein --> K{Hub<br/>erreichbar?}
    K -- nein --> ERR2[Kurze Fehler-Anzeige<br/>NICHT blockieren]
    K -- ja --> DL[Vordergrund-Download der fehlenden Dateien<br/>.tmp → SHA-Verify → Move nach /.mediahub/media/cardId/<br/>Ring zeigt Fortschrittsbalken]
    DL --> PLAY
    PLAY --> BG[/Hintergrund: nur version prüfen;<br/>bei Änderung Karte "stale" markieren<br/>Update beim nächsten Auflegen/]
```

### Textuelle Zusammenfassung des kritischen Pfads

1. Karte → `cardId`. NVS-Lookup (sofort, lokal).
2. Kein `MEDIAHUB`-playMode → alles wie bisher (SD, Webradio, Modifikation).
3. `MEDIAHUB` + Karte „stale" & Hub erreichbar → **Vordergrund-Re-Sync** (neues Manifest, Ordner wipen, neu laden), dann neue Version spielen.
4. Sonst: Manifest aus Cache (oder, falls fehlend und Hub erreichbar, einmalig laden).
5. Alle Dateien lokal & Größen passen → **sofort spielen** (offline-tauglich).
6. Sonst & Hub erreichbar → **Vordergrund-Download** mit Ring-Fortschritt, danach spielen.
7. Sonst (Hub weg) → **kurze Fehleranzeige, kein Blockieren**.
8. Nach dem Start: **Hintergrund-`version`-Check** (nur prüfen); bei Änderung Karte „stale" markieren → Update beim nächsten Auflegen.

Der „PLAY"-Schritt ruft `AudioPlayer_SetPlaylist()` mit dem **echten** `playMode` aus dem Manifest sowie den beim NVS-Lookup (Schritt 1) bereits gelesenen `lastPlayPos`/`trackLastPlayed` auf — Details und der dafür nötige Fix in `AudioPlayer_NvsRfidWriteWrapper()` siehe §8.1.

## 12. LED / Fortschritt

Der Download-Fortschritt wird auf dem Neopixel-Ring angezeigt. **Wichtig:** Nicht über `Led_ShowOtaProgress()` — das suspendiert den `Led_Task` und blockiert andere Animationen. Stattdessen eine **eigene, nicht-suspendierende Download-Animation** (neuer `LedAnimationType::Download` oder Wiederverwendung von `Animation_Progress`), die sich in die Prioritätenlogik des `Led_Task` einreiht (`Led.cpp`).

## 13. Download-Robustheit

- Der Karten-Ordner `/.mediahub/media/<cardId>/` wird bei Bedarf angelegt.
- Jede Datei wird nach `…/<path>.tmp` geladen und erst nach erfolgreicher SHA-Prüfung per Rename an den endgültigen Ort verschoben.
- Abbruch/Fehler (Netzwerk weg, Hub-Fehler, SD-Fehler) → `.tmp` entfernen, klare Fehlermeldung, kein Zombie-File.
- **SD-Platz vorab prüfen** (frei = `totalBytes − usedBytes`, gegen die Summe der `files[].size`): frischer Download nur, wenn `frei ≥ benötigt`. Beim **Re-Sync erst prüfen, dann wipen** — `frei + alte_Ordnergröße ≥ benötigt`; passt es nicht, wird **nicht gewiped** (die alte, funktionierende Version bleibt) und ein „SD voll"-Fehler angezeigt.
- **Re-Sync (stale-Karte):** der Ordner wird geleert und neu befüllt. Bricht das ab, bleibt die Karte **„needs resync"** markiert → der nächste Tap versucht es erneut. (Ehrlicher Preis der Wipe-Strategie: bis dahin ist die Karte ggf. unvollständig.)
- Kurze Connect-Timeouts (schnelles Scheitern), damit ein unerreichbarer Hub nie hängt.

### 13.1 Löschen & Aufräumen (Option B)

**Speicher-Layout.** Alle MediaHub-Daten liegen unter einem festen, im Datei-Browser versteckten Root:

```text
/.mediahub/media/<cardId>/<files[].path>      ← Mediendateien der Karte
/.mediahub/manifests/<cardId>.json            ← Manifest-Cache
```

Der ESPuino bestimmt die Ablage selbst (daher kein `target` im Manifest). Vorteil: Der gesamte `/.mediahub/`-Baum ist die einzige „verwaltete Zone" — außerhalb davon fasst MediaHub **nie** etwas an, und das Aufräumen einer Karte ist ein simpler Subdir-Löschvorgang.

**Löschen einer Karte — REST-Cascade.** Das ESPuino-REST-API hat bereits `DELETE /rfid?id=<cardId>` (`handleDeleteRFIDRequest`, `Web.cpp`). Dort hängen wir einen Schritt an:

> Ist der `playMode` des zu löschenden Eintrags **`MEDIAHUB`**, werden zusätzlich `/.mediahub/media/<cardId>/` und der Manifest-Cache gelöscht. Der Endpoint gibt **200 nur zurück, wenn alles** (NVS + Ordner) erfolgreich entfernt wurde — sonst **500** (der Aufrufer kann es erneut versuchen).

Ein Code-Pfad für beide Auslöser: Der **Nutzer** löscht die Karte im ESPuino-Web-UI (Tools-Tab ruft ohnehin dieses `DELETE /rfid`), oder der **Hub** ruft dasselbe `DELETE /rfid` auf, um zentral zu löschen.

**Hub-seitige Konfiguration (lazy vs. secure).** Der Hub-Nutzer stellt ein, was beim Löschen passiert:

- **lazy delete:** Der Hub löscht nur seinen eigenen Eintrag. Der ESPuino bleibt unangetastet — die Karte lebt lokal weiter (spielt aus dem Cache, auch offline). Konsequenz: Bei einem späteren Online-Tap kennt der Hub sie nicht mehr → sie registriert sich als neue/pending Karte (für einen Soft-Delete akzeptiert).
- **secure delete:** Der Hub ruft `DELETE /rfid?id=<cardId>` auf **dem in der Zuweisung hinterlegten ESPuino** (§5.5) an und löscht seinen eigenen Eintrag **erst nach einer 200**. Zwei-Phasen → konsistent; scheitert der Call, behält der Hub seinen Eintrag und versucht es erneut. Da die Zuweisung ihr Gerät direkt kennt (statt es aus „zuletzt gesehen" zu erraten), ist auch bei mehreren ESPuinos pro Karte immer klar, welches genau angerufen wird.

**Aufräumen bei Inhaltsänderung.** Ein Content-Update räumt implizit auf: Beim Re-Sync (nächstes Auflegen einer „stale"-Karte, §11) wird `/.mediahub/media/<cardId>/` **geleert und neu befüllt** — verwaiste Dateien verschwinden dabei automatisch.

## 14. Offline- & Fehlerverhalten

**Offline / Hub nicht erreichbar (transient):**

- `mediahub://`-Karte, Inhalt lokal vollständig → **spielt normal**, kein Netzwerkzugriff nötig.
- Inhalt fehlt/unvollständig und Hub nicht erreichbar → **kurze Fehleranzeige**, kein Timeout, kein Hängen.
- Der Manifest-Cache auf der SD (`/.mediahub/manifests/<cardId>.json`) macht `playMode` und die erwarteten Größen offline verfügbar.
- **Webradio-Zuweisungen** brauchen prinzipbedingt Netz und sind offline nicht abspielbar (kurze Fehleranzeige).

**Hub erreichbar, aber Karte (noch) nicht zugewiesen:**

- Der Hub registriert die Karte (§5.3) und antwortet negativ → **Fehleranzeige**. Für den Bediener bedeutet das „neue / noch nicht eingerichtete Karte".
- **Kein Löschen bei Einzel-Fehler:** Ein einzelner Negativ-/404-Response löscht **nie** lokale Dateien oder den NVS-Eintrag. Entfernt wird nur durch eine **bewusste Aktion** (`DELETE /rfid` — manuell im Web-UI oder vom Hub, §13.1). (Gilt auch für den Hintergrund-`version`-Check.)

**Während eines laufenden Downloads:**

- Der Vordergrund-Download ist blockierend. Wird währenddessen eine **weitere Karte** aufgelegt, wird das mit einem **Fehler quittiert** und ignoriert — der ESPuino ist „busy".

## 15. Offene Punkte / später

- Keine offenen **Konzept**-Fragen mehr. Verbleibende Details (LED-Fehlermuster, Web-UI-Status-Texte, genaue „needs resync"-Retry-Politik) klären sich bei der Umsetzung.

## 16. Entscheidungslog

| # | Entscheidung |
| --- | --- |
| 1 | Lokaler Hub (Docker), keine Cloud. |
| 2 | Nichts auf der Karte; nur die 12-stellige Nummer. |
| 3 | NVS-Format unverändert; Hub-Adresse via `mediahub://` im Pfad-Feld. |
| 4 | Per-Karte-Manifest, kein großes Sammel-Manifest. |
| 5 | `version` = Hub-seitiger Content-Hash (ESP32 vergleicht nur). |
| 6 | Lokale Integrität über Größe; SHA-256 nur inkrementell beim Download. |
| 7 | Force Refresh als Escape-Hatch (pro Karte / alle). |
| 8 | Neuer playMode-Wert `MEDIAHUB` als NVS-Marker (zugleich Diskriminator in `RfidCommon.cpp`); echter Modus kommt aus dem Manifest, kein Fallback-playMode. |
| 9 | Erst-Download blockiert die Wiedergabe (mit Fortschrittsbalken). Änderungen werden im Hintergrund nur erkannt (Karte „stale"); das Update passiert beim nächsten Auflegen. |
| 10 | Eigene, nicht-suspendierende LED-Download-Animation statt `Led_ShowOtaProgress()`. |
| 11 | Kein `#ifdef` — MediaHub ist rein laufzeit-gesteuert, aktiv nur bei `mediahub://`-Karten. |
| 12 | Webradio unterstützt: Stream-Manifest (`playMode` WEBSTREAM) ohne Download. `LOCAL_M3U` wird nicht unterstützt. |
| 13 | Medienserver im ESPuino-Web-UI registrierbar (Name + Basis-URL, eigener Settings-Eintrag); Dropdown füllt das `mediahub://`-Feld beim Zuweisen. |
| 14 | Registrierung der Karten-ID erfolgt **beim Auflegen** (nicht beim Anlernen); der Manifest-Request ist zugleich die Registrierung. Zeitpunkt frei wählbar. |
| 15 | Unbekannte / nicht zugewiesene Karte → **Fehleranzeige** (akzeptiert; Bediener erkennt neue Karte). Kein Löschen lokaler Daten bei Einzel-Fehler. |
| 16 | Während eines laufenden (blockierenden) Downloads wird das Auflegen weiterer Karten mit Fehler quittiert („busy"). |
| 17 | **Bulk-Sync/Index (früher „Mechanismus b") verworfen** — Fehlerbehandlung zu komplex, Downloadrate ~450 kB/s. Zuweisungen nur via manuelles Anlernen, Inhalte on-demand beim Auflegen. |
| 18 | `playMode` lebt ausschließlich im Manifest (keine Rückpropagierung ins NVS). |
| 19 | Keine Authentifizierung des Hub-Zugriffs (lokaler, vertrauenswürdiger Hub im Heimnetz) — für die ESPuino-Endpunkte weiterhin so; für das Hub-Web-UI durch #25 um ein optionales Passwort ergänzt. |
| 20 | Speicher-Layout Option B: Medien unter `/.mediahub/media/<cardId>/`, Cache unter `/.mediahub/manifests/`. `target` entfällt — die Ablage bestimmt der ESPuino. |
| 21 | Löschen via `DELETE /rfid?id=X`-Cascade (MEDIAHUB-gated: NVS + Medien + Cache; 200 nur bei vollem Erfolg). Kein `action`-Feld im Manifest. |
| 22 | Hub-Config: **lazy** (nur Hub-Eintrag) vs. **secure** (zwei-Phasen: erst `DELETE /rfid`, Hub-Eintrag erst nach 200). |
| 23 | Change-Handling = Lazy Update beim nächsten Auflegen (stale-Markierung; Wipe + Neuladen erst beim nächsten Tap → Resync braucht 2× Auflegen). |
| 24 | SD-Platz wird vor dem Download geprüft (frisch: `frei ≥ benötigt`; Re-Sync: erst prüfen `frei + alt ≥ benötigt`, dann wipen — sonst alte Version behalten). |
| 25 | Hub-Web-UI optional per Passwort absicherbar (Settings, gehashter Wert in der JSON-DB). Betrifft nur die Verwaltungsoberfläche — die ESPuino-Endpunkte (§6) bleiben immer offen (§19). |
| 26 | Karten-Zuweisung über eine read-only in den Container gemountete Medienbibliothek (`/media`) statt Datei-Upload; Auswahl per eingebettetem Datei-Browser (§5.4). Löschen einer Zuweisung fasst nie die Bibliotheksdateien an — der Hub referenziert sie nur. |
| 27 | **Zuweisungs-Schlüssel ist (espId, cardId), nicht cardId allein** (§5.5) — dieselbe Karte kann pro ESPuino unabhängig konfiguriert werden. Löst die Mehrdeutigkeit bei Secure Delete: Vorher rief der Hub das „zuletzt gesehene" Gerät zur Karte an (falsch/unvollständig bei mehreren ESPuinos pro Karte); jetzt kennt jede Zuweisung ihr Gerät direkt. |
| 28 | Geräte-Alias-Liste im Hub (frei vergebbarer Name je `espId`, z. B. „Kinderzimmer") — reine Anzeige-Ergonomie, da die vom ESPuino gesendete `espId` (MAC/Hostname) für Menschen unhandlich ist. Gerät im Hub-UI löschbar; bestehende Zuweisungen dieses Geräts werden dabei nach Bestätigung mit gelöscht (nur der Hub-Datensatz, nie ESPuino/NVS). |
| 29 | **Play-Position-Persistenz (§8.1):** NVS-Format korrigiert auf die tatsächlichen vier Felder `path#lastPlayPos#playMode#trackLastPlayed`. `AudioPlayer_NvsRfidWriteWrapper()` muss bei einem `mediahub://`-Pfad das `playMode`-Feld beim Zurückschreiben immer auf `MEDIAHUB` erzwingen (statt des echten, gerade abgespielten Modus) — sonst überschreibt der erste Pause-/Trackwechsel-Save den Marker und die Karte wird beim nächsten Auflegen nicht mehr als MediaHub-Karte erkannt. `AudioPlayer_SetPlaylist()` wird beim Abspielen mit dem echten Manifest-`playMode` plus den aus dem NVS gelesenen `lastPlayPos`/`trackLastPlayed` aufgerufen — dadurch greifen `saveLastPlayPosition` und der Shutdown-Flush unverändert, ohne dass AudioPlayer.cpp/System.cpp MediaHub kennen müssen. |

## 17. Implementierungsplan ESPuino-Seite (Phasen)

Der Hub (`mediahub/`) ist implementiert und getestet (§15). Für die Firmware-Seite folgt die Umsetzung in Phasen, aufsteigend nach Abhängigkeiten sortiert — jede Phase soll für sich testbar/demofähig sein, bevor die nächste draufkommt.

### Phase 0 — Fundament & Dispatch-Weiche

- Neuer `MEDIAHUB`-playMode-Wert in `values.h` (< 100, siehe Constraint mit Modifikationskarten, §8)
- `mediahub://`-Erkennung im Pfad-Feld, Dispatch-Weiche in `RfidCommon.cpp` **vor** der bestehenden Webradio-Erkennung — vorerst nur geloggt/gestubbt, keine echte Netzwerkaktivität
- Wichtigster Test: **alle bestehenden, normalen Karten verhalten sich exakt unverändert.** Das ist der riskanteste Berührungspunkt (Code-Pfad, den jede Karte durchläuft), deshalb isoliert und zuerst.

### Phase 1 — Vertikale Scheibe: nur Webradio

- Manifest per HTTP GET holen + parsen, Registrierung-beim-Auflegen (§5.3, unbekannte Karte → pending + Fehleranzeige)
- Bei `playMode = WEBSTREAM`: Stream-URL direkt an den bestehenden AudioPlayer-Webradio-Pfad übergeben
- Kurze Connect-Timeouts, Offline-Fehleranzeige
- Bewusst zuerst, weil komplett ohne Download-/SD-Logik — testet Netzwerk- und Dispatch-Plumbing isoliert, erstes demofähiges Ergebnis (Karte auflegen → Stream läuft).

### Phase 2 — Datei-Wiedergabe, nur der lokale Pfad

- Manifest-Cache lesen, lokaler Integritätscheck (nur Größe, §9)
- Ist alles bereits lokal vorhanden → sofort abspielen, kein Netzwerk nötig
- Testbar mit einer von Hand vorbereiteten SD-Struktur + Manifest-Cache, ganz ohne Downloadpfad.

### Phase 3 — Play-Position-Fix

- Der in §8.1 beschriebene Fix in `AudioPlayer_NvsRfidWriteWrapper()` (Marker beim Zurückschreiben erzwingen) + Durchreichen von `lastPlayPos`/`trackLastPlayed` an `AudioPlayer_SetPlaylist()`
- Klein, aber jetzt einbauen, weil Phase 2 gerade den ersten echten Hörbuch-Wiedergabepfad liefert, an dem sich das überhaupt testen lässt (Pause → erneut auflegen → Marker noch da?).

### Phase 4 — Download-Pipeline (der aufwändigste Teil)

- `MediaHub_EnsureCard` (§10): fehlende Dateien erkennen, `.tmp` → SHA-256-Verify → Move, SD-Platz-Check vorab (§13)
- Vordergrund-Download mit Fortschritt, „busy"-Antwort bei Kartenauflegen während laufendem Download (§14)
- Force Refresh, pro Karte / alle (§9)

### Phase 5 — LED-Integration

- Eigene, nicht-suspendierende Download-Animation in die `Led_Task`-Prioritätslogik einhängen (§12)

### Phase 6 — Änderungserkennung & Re-Sync

- Hintergrund-`version`-Check nach Wiedergabestart, „stale"-Markierung (§9)
- Re-Sync-Ablauf beim nächsten Auflegen (SD-Platz-Check → wipen → neu laden, §13/§13.1)

### Phase 7 — Löschen-Kaskade

- `DELETE /rfid`-Handler um den MEDIAHUB-Fall erweitern (Medien + Manifest-Cache lokal löschen, §13.1)
- Lazy/Secure werden hub-seitig entschieden (bereits implementiert, §13.1/§25); hier nur der Ausführungs-Endpunkt

### Phase 8 — ESPuino-Web-UI-Ergonomie

- Registrierte Medienserver-Liste + Dropdown beim Kartenzuweisen (§5.1)

### Phase 9 — Härtung & vollständiger Testdurchlauf

- Kompletter Abgleich gegen die Fehlerfälle aus §14 (Hub weg, SD voll, Abbruch mitten im Download, Re-Sync scheitert, …) auf echter Hardware
