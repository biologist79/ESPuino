#include <Arduino.h>
#include "settings.h"

#include "MediaHub.h"

#include "AudioPlayer.h"
#include "Log.h"
#include "SdCard.h"
#include "System.h"
#include "Wlan.h"
#include "logmessages.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>

const char *const MediaHub_PathPrefix = "mediahub://";

// Short on purpose (concept §14): an unreachable hub must never turn into
// a long hang while a card is being tapped.
static constexpr int32_t MediaHub_ConnectTimeoutMs = 2000;
static constexpr uint16_t MediaHub_ReadTimeoutMs = 3000;

// Fixed, hidden storage layout (concept §13.1) — MediaHub is the only thing
// that ever touches this tree.
static String MediaHub_ManifestCachePath(const char *cardId) {
	return "/.mediahub/manifests/" + String(cardId) + ".json";
}

static String MediaHub_MediaDir(const char *cardId) {
	return "/.mediahub/media/" + String(cardId);
}

// SINGLE_TRACK/SINGLE_TRACK_LOOP name one specific file, not a folder to
// scan (mirrors SINGLE_FILE_PLAY_MODES on the hub, which caps assignment
// to exactly one file for these two modes).
static bool MediaHub_IsSingleFilePlayMode(uint32_t playMode) {
	return playMode == SINGLE_TRACK || playMode == SINGLE_TRACK_LOOP;
}

// Local integrity check is size-only, never a hash (concept §9): hashing a
// possibly 100 MB file on every tap just to confirm what the hub already
// verified at download time isn't an option on this hardware.
static bool MediaHub_FileFullySynced(const String &mediaDir, JsonVariantConst fileEntry) {
	const char *path = fileEntry["path"] | "";
	if (strlen(path) == 0) {
		return false;
	}
	const uint32_t expectedSize = fileEntry["size"] | 0;
	File f = gFSystem.open(mediaDir + "/" + path);
	if (!f || f.isDirectory()) {
		return false;
	}
	const bool sizeMatches = (uint32_t) f.size() == expectedSize;
	f.close();
	return sizeMatches;
}

static bool MediaHub_AllFilesSynced(const String &mediaDir, JsonArrayConst files) {
	if (files.size() == 0) {
		return false; // nothing to play
	}
	for (JsonVariantConst f : files) {
		if (!MediaHub_FileFullySynced(mediaDir, f)) {
			return false;
		}
	}
	return true;
}

// Local-first fast path (concept §3 principle 1): if a manifest is already
// cached and every one of its files is present with the right size, play
// immediately — no network access at all, so this works fully offline
// (e.g. in the car). Returns false if there's no cache yet, it doesn't
// parse, it's a webradio manifest (never offline-playable, §7.2/§14), or
// anything is missing/mismatched — the caller then falls back to asking
// the hub live.
static bool MediaHub_TryPlayFromLocalCache(const char *cardId, uint32_t lastPlayPos, uint16_t trackLastPlayed) {
	File manifestFile = gFSystem.open(MediaHub_ManifestCachePath(cardId));
	if (!manifestFile || manifestFile.isDirectory()) {
		return false;
	}

	JsonDocument doc;
	const DeserializationError jsonError = deserializeJson(doc, manifestFile);
	manifestFile.close();
	if (jsonError) {
		Log_Printf(LOGLEVEL_ERROR, jsonErrorMsg, jsonError.c_str());
		return false; // corrupt cache -> treat as if there was none
	}

	const uint32_t manifestPlayMode = doc["playMode"] | 0;
	if (manifestPlayMode == WEBSTREAM) {
		return false;
	}

	const String mediaDir = MediaHub_MediaDir(cardId);
	JsonArrayConst files = doc["files"].as<JsonArrayConst>();
	if (!MediaHub_AllFilesSynced(mediaDir, files)) {
		return false;
	}

	String itemToPlay;
	if (MediaHub_IsSingleFilePlayMode(manifestPlayMode)) {
		const char *singleFilePath = files[0]["path"] | "";
		itemToPlay = mediaDir + "/" + singleFilePath;
	} else {
		// Folder mode: point at the card's media directory and let the
		// existing SdCard_ReturnPlaylist() scan/sort/recurse it exactly like
		// any other SD folder — it already knows how to do that per
		// playMode, MediaHub doesn't need to reimplement any of it.
		itemToPlay = mediaDir;
	}

	Log_Println(mediaHubPlayingFromCache, LOGLEVEL_NOTICE);
	AudioPlayer_SetPlaylist(itemToPlay.c_str(), lastPlayPos, manifestPlayMode, trackLastPlayed);
	return true;
}

bool MediaHub_IsMediaHubPath(const char *path) {
	return path != NULL && strncmp(path, MediaHub_PathPrefix, strlen(MediaHub_PathPrefix)) == 0;
}

String MediaHub_GetEspId() {
	String mac = Wlan_GetMacAddress(); // "AA:BB:CC:DD:EE:FF" or empty if not available yet
	mac.replace(":", "");
	mac.toUpperCase();
	return mac;
}

void MediaHub_HandleCardTapped(const char *cardId, const char *path, uint32_t lastPlayPos, uint16_t trackLastPlayed) {
	if (!MediaHub_IsMediaHubPath(path)) {
		Log_Println(mediaHubInvalidPath, LOGLEVEL_ERROR);
		System_IndicateError();
		return;
	}

	if (MediaHub_TryPlayFromLocalCache(cardId, lastPlayPos, trackLastPlayed)) {
		return;
	}

	if (!Wlan_IsConnected()) {
		Log_Println(mediaHubNotReachable, LOGLEVEL_ERROR);
		System_IndicateError();
		return;
	}

	String hostPort = String(path).substring(strlen(MediaHub_PathPrefix));
	if (hostPort.length() == 0) {
		Log_Println(mediaHubInvalidPath, LOGLEVEL_ERROR);
		System_IndicateError();
		return;
	}

	String espId = MediaHub_GetEspId();
	String url = "http://" + hostPort + "/" + espId + "/card/" + String(cardId) + "/manifest.json";

	HTTPClient http;
	http.setConnectTimeout(MediaHub_ConnectTimeoutMs);
	http.setTimeout(MediaHub_ReadTimeoutMs);
	if (!http.begin(url)) {
		Log_Println(mediaHubNotReachable, LOGLEVEL_ERROR);
		System_IndicateError();
		return;
	}

	const int httpCode = http.GET();
	if (httpCode <= 0) {
		// Transport-level failure (no route, connection refused, timeout, ...):
		// never block, just report and bail (concept §14).
		Log_Println(mediaHubNotReachable, LOGLEVEL_ERROR);
		System_IndicateError();
		http.end();
		return;
	}

	const String body = http.getString();
	http.end();

	if (httpCode == 404) {
		// Unknown / not-yet-assigned card: the hub has (re-)registered it as
		// pending for the admin to assign (concept §5.3). Not an error the
		// user can fix here, just make it visible.
		Log_Println(mediaHubCardPending, LOGLEVEL_NOTICE);
		System_IndicateError();
		return;
	}

	if (httpCode != 200) {
		Log_Printf(LOGLEVEL_ERROR, mediaHubUnexpectedStatus, httpCode);
		System_IndicateError();
		return;
	}

	JsonDocument doc;
	const DeserializationError jsonError = deserializeJson(doc, body);
	if (jsonError) {
		Log_Printf(LOGLEVEL_ERROR, jsonErrorMsg, jsonError.c_str());
		System_IndicateError();
		return;
	}

	const uint32_t manifestPlayMode = doc["playMode"] | 0;
	if (manifestPlayMode == WEBSTREAM) {
		const char *stream = doc["stream"] | "";
		if (strlen(stream) == 0) {
			Log_Println(mediaHubInvalidManifest, LOGLEVEL_ERROR);
			System_IndicateError();
			return;
		}
		Log_Println(mediaHubWebstreamFromManifest, LOGLEVEL_NOTICE);
		// Webradio manifests have no files to sync and no play-position to
		// track (concept §7.2) — hand the stream straight to the existing
		// webradio playback path, same as a native WEBSTREAM card.
		AudioPlayer_SetPlaylist(stream, 0, WEBSTREAM, 0);
		return;
	}

	// File-based manifests (download/sync pipeline, §10/§13) land in a later
	// phase — recognized here, but not played yet.
	Log_Println(mediaHubFileModeNotYetSupported, LOGLEVEL_NOTICE);
	System_IndicateError();
}
