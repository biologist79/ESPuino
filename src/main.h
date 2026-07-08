#pragma once

// Set when a webstream-tag could not be started because WiFi was not connected yet (e.g. a webradio-tag
// applied right after boot). The remembered tag is re-injected into the RFID-queue as soon as WiFi is up
// (see handleWifiStateConnectionSuccess() in Wlan.cpp). gRetryRfidTagId is sized like cardIdStringSize.
extern bool gRetryRfidOnWifiConnect;
extern char gRetryRfidTagId[];

extern void recoverLastRfidPlayedFromNvs(bool force = false);
