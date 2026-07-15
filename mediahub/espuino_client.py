"""Calls back to the ESPuino for the REST cascade delete (concept §13.1).

`DELETE /rfid?id=<cardId>` already exists on the ESPuino
(`handleDeleteRFIDRequest`, Web.cpp) and, for a MEDIAHUB card, also cleans
up the media files and manifest cache on the SD card. For "secure delete"
the hub must get a successful response from this exact call before it
removes its own entry.
"""

import urllib.error
import urllib.parse
import urllib.request

REQUEST_TIMEOUT = 3  # seconds — short timeouts so an admin click never hangs (§13).


def delete_rfid_on_device(ip, card_id):
    """True only on HTTP 200 from the ESPuino, False otherwise (never raises)."""
    if not ip:
        return False
    url = f"http://{ip}/rfid?id={urllib.parse.quote(card_id)}"
    req = urllib.request.Request(url, method="DELETE")
    try:
        with urllib.request.urlopen(req, timeout=REQUEST_TIMEOUT) as resp:
            return resp.status == 200
    except (urllib.error.URLError, TimeoutError, OSError):
        return False
