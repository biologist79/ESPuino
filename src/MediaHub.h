#pragma once

// MediaHub (see mediahub-konzept.md at the repo root, §17 for the phased
// implementation plan): lets a locally-run companion server centrally
// manage RFID-card assignments across multiple ESPuinos. A card is
// MediaHub-managed if its NVS playMode is MEDIAHUB (values.h); the NVS
// path field then holds "mediahub://<host:port>" instead of an SD path,
// and the actual playMode/content comes from a per-card manifest fetched
// from that host at tap-time (never from NVS).
//
// Phase 4 (current): webradio manifests (playMode WEBSTREAM) always go
// live to the hub. File-based manifests play instantly from a locally
// cached manifest + already-synced files, offline, no network at all
// (concept §3/§9); if anything is missing and the hub is reachable, the
// missing files are downloaded in the foreground (.tmp -> SHA-256 verify
// -> move, SD-space checked up front, concept §10/§13) before playback.
// Change detection ("stale") and the re-sync wipe are a later phase —
// MediaHub_ForceRefresh(All)() already exist as the escape-hatch those will
// share, but nothing calls them yet.

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
