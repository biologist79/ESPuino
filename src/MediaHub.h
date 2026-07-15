#pragma once

// MediaHub (see mediahub-konzept.md at the repo root, §17 for the phased
// implementation plan): lets a locally-run companion server centrally
// manage RFID-card assignments across multiple ESPuinos. A card is
// MediaHub-managed if its NVS playMode is MEDIAHUB (values.h); the NVS
// path field then holds "mediahub://<host:port>" instead of an SD path,
// and the actual playMode/content comes from a per-card manifest fetched
// from that host at tap-time (never from NVS).
//
// Phase 2 (current): webradio manifests (playMode WEBSTREAM) always go
// live to the hub. File-based manifests play instantly from a locally
// cached manifest + already-synced files, offline, no network at all
// (concept §3/§9) — but if anything is missing or there's no cache yet,
// they're only recognized, not downloaded (that's a later phase).

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
// and trackLastPlayed are the values already parsed from NVS by the
// caller, to be passed through to AudioPlayer_SetPlaylist() once
// file-based manifest playback is implemented (§8.1, §11) — unused for now.
void MediaHub_HandleCardTapped(const char *cardId, const char *path, uint32_t lastPlayPos, uint16_t trackLastPlayed);
