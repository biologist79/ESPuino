#pragma once

// MediaHub (see mediahub-konzept.md at the repo root, §17 for the phased
// implementation plan): lets a locally-run companion server centrally
// manage RFID-card assignments across multiple ESPuinos. A card is
// MediaHub-managed if its NVS playMode is MEDIAHUB (values.h); the NVS
// path field then holds "mediahub://<host:port>" instead of an SD path,
// and the actual playMode/content comes from a per-card manifest fetched
// from that host at tap-time (never from NVS).
//
// Phase 6 (current): webradio manifests (playMode WEBSTREAM) always go
// live to the hub. File-based manifests play instantly from a locally
// cached manifest + already-synced files, offline, no network at all
// (concept §3/§9); if anything is missing and the hub is reachable, the
// missing files are downloaded in the foreground (.tmp -> SHA-256 verify
// -> move, SD-space checked up front, concept §10/§13) before playback.
// After every successful online play, a short-lived background task
// compares the hub's current manifest version against the cached one and
// marks the card "stale" on a mismatch (concept §9); a "stale" card gets a
// foreground re-sync (wipe + full re-download) on its next tap, falling
// back to the old, still-complete copy if that isn't possible right now.
// MediaHub_ForceRefresh(All)() are the same escape-hatch, callable directly
// but still not wired to a trigger (Admin-Karte / Hub-Button).

// Prefix identifying a MediaHub base-address in the NVS path field.
extern const char *const MediaHub_PathPrefix; // "mediahub://"

// True if path starts with MediaHub_PathPrefix.
bool MediaHub_IsMediaHubPath(const char *path);

// Stable per-device identifier sent to the hub as <espId> (concept §6):
// the WiFi MAC address without colons, uppercase (e.g. "AABBCCDDEEFF").
// Unlike the configurable hostname (defaults to "ESPuino" for every
// device), this needs no setup and can't collide between devices.
String MediaHub_GetEspId();

// Dispatch entry point for a card whose NVS playMode is MEDIAHUB (called
// from RfidCommon.cpp before the normal AudioPlayer dispatch). lastPlayPos
// and trackLastPlayed are the values already parsed from NVS by the caller
// (§8.1), passed through to AudioPlayer_SetPlaylist() once the real
// manifest playMode is known.
void MediaHub_HandleCardTapped(const char *cardId, const char *path, uint32_t lastPlayPos, uint16_t trackLastPlayed);

// Discards the local manifest cache + downloaded media for one card, or for
// every MediaHub-managed card, forcing a full re-fetch/re-download on the
// next tap (concept §9). Returns false if any part of the wipe failed.
bool MediaHub_ForceRefresh(const char *cardId);
bool MediaHub_ForceRefreshAll();

// Same wipe as MediaHub_ForceRefresh(), but the card isn't coming back: for
// the DELETE /rfid cascade (concept §13.1), called in addition to removing
// the NVS entry. Returns false if any part of the wipe failed.
bool MediaHub_DeleteCard(const char *cardId);
