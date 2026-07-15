#include <Arduino.h>
#include "settings.h"

#include "MediaHub.h"

#include "AudioPlayer.h"
#include "Led.h"
#include "Log.h"
#include "SdCard.h"
#include "System.h"
#include "Wlan.h"
#include "logmessages.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <algorithm>
#include <esp_task_wdt.h>
#include <mbedtls/sha256.h>

const char *const MediaHub_PathPrefix = "mediahub://";

// Short on purpose (concept §14): an unreachable hub must never turn into
// a long hang while a card is being tapped.
static constexpr int32_t MediaHub_ConnectTimeoutMs = 2000;
static constexpr uint16_t MediaHub_ReadTimeoutMs = 3000;

// Per-chunk stall timeout while downloading a file body: media files can be
// large, so this is a "no bytes at all for this long" watchdog, not a total
// transfer-time budget.
static constexpr uint32_t MediaHub_DownloadStallTimeoutMs = 8000;
static constexpr size_t MediaHub_DownloadBufferSize = 4096;

// Set for the duration of an actual file download (concept §14/#16): a card
// tapped while this is true gets a "busy" error instead of being processed.
static volatile bool MediaHub_DownloadBusy = false;

struct MediaHub_BusyGuard {
	MediaHub_BusyGuard() {
		MediaHub_DownloadBusy = true;
	}
	~MediaHub_BusyGuard() {
		MediaHub_DownloadBusy = false;
		Led_SetDownloadProgress(false);
	}
};

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

// SINGLE_TRACK/SINGLE_TRACK_LOOP point at one specific file; every other mode
// points at the whole media folder and leaves scanning/sorting/recursion to
// SdCard_ReturnPlaylist() (same reasoning as in AudioPlayer_SetPlaylist()).
static String MediaHub_BuildItemToPlay(const String &mediaDir, uint32_t playMode, JsonArrayConst files) {
	if (MediaHub_IsSingleFilePlayMode(playMode)) {
		const char *singleFilePath = files[0]["path"] | "";
		return mediaDir + "/" + singleFilePath;
	}
	return mediaDir;
}

// Mirrors explorerDeleteDirectory() in Web.cpp: recurse via File objects only,
// never rebuild string paths for entries, so SanitizedFS's percent-encoding
// (FileSystem.h) can't be applied twice to an already-sanitized name.
static bool MediaHub_DeleteDirRecursive(File dir) {
	bool ok = true;
	File entry = dir.openNextFile();
	while (entry) {
		if (entry.isDirectory()) {
			ok &= MediaHub_DeleteDirRecursive(entry);
		} else {
			ok &= gFSystem.remove(entry);
		}
		entry = dir.openNextFile();
		esp_task_wdt_reset();
	}
	return gFSystem.rmdir(dir) && ok;
}

static String MediaHub_Sha256Hex(const uint8_t digest[32]) {
	static const char *hexDigits = "0123456789abcdef";
	String hex;
	hex.reserve(64);
	for (int i = 0; i < 32; i++) {
		hex += hexDigits[digest[i] >> 4];
		hex += hexDigits[digest[i] & 0x0F];
	}
	return hex;
}

// Downloads one file to <finalPath>.tmp, hashing incrementally while writing
// (concept §9: SHA-256 only ever checked during download, never recomputed
// over local files afterwards). Renames into place only once size and hash
// both match; leaves no partial/renamed file behind on any failure.
static bool MediaHub_DownloadAndVerifyFile(const String &fileUrl, const String &finalPath, uint32_t expectedSize, const char *expectedSha256Hex, uint64_t &completedBytes, uint64_t totalBytes) {
	HTTPClient http;
	http.setConnectTimeout(MediaHub_ConnectTimeoutMs);
	http.setTimeout(MediaHub_DownloadStallTimeoutMs);
	if (!http.begin(fileUrl)) {
		Log_Printf(LOGLEVEL_ERROR, mediaHubDownloadFailed, fileUrl.c_str());
		return false;
	}

	const int httpCode = http.GET();
	if (httpCode != HTTP_CODE_OK) {
		Log_Printf(LOGLEVEL_ERROR, mediaHubDownloadFailed, fileUrl.c_str());
		http.end();
		return false;
	}

	const String tmpPath = finalPath + ".tmp";
	File tmpFile = gFSystem.open(tmpPath, FILE_WRITE, true); // create=true: also creates missing parent dirs
	if (!tmpFile) {
		Log_Printf(LOGLEVEL_ERROR, mediaHubDownloadFailed, fileUrl.c_str());
		http.end();
		return false;
	}

	mbedtls_sha256_context shaCtx;
	mbedtls_sha256_init(&shaCtx);
	mbedtls_sha256_starts(&shaCtx, 0); // 0 = SHA-256 (not the SHA-224 variant)

	auto *stream = http.getStreamPtr();
	uint8_t buf[MediaHub_DownloadBufferSize];
	size_t totalRead = 0;
	bool transferError = false;
	uint32_t lastDataMs = millis();

	while (totalRead < expectedSize) {
		const int avail = stream->available();
		if (avail <= 0) {
			if (millis() - lastDataMs > MediaHub_DownloadStallTimeoutMs) {
				transferError = true;
				break;
			}
			delay(1);
			continue;
		}
		const size_t toRead = std::min((size_t) avail, sizeof(buf));
		const size_t got = stream->readBytes(buf, toRead);
		if (got == 0) {
			transferError = true;
			break;
		}
		lastDataMs = millis();
		mbedtls_sha256_update(&shaCtx, buf, got);
		if (tmpFile.write(buf, got) != got) {
			transferError = true;
			break;
		}
		totalRead += got;
		completedBytes += got;
		if (totalBytes > 0) {
			Led_SetDownloadProgress(true, (uint8_t) std::min<uint64_t>(100, (completedBytes * 100) / totalBytes));
		}
		esp_task_wdt_reset();
	}
	http.end();
	tmpFile.close();

	uint8_t digest[32];
	mbedtls_sha256_finish(&shaCtx, digest);
	mbedtls_sha256_free(&shaCtx);

	if (transferError || totalRead != expectedSize) {
		gFSystem.remove(tmpPath);
		Log_Printf(LOGLEVEL_ERROR, mediaHubDownloadFailed, fileUrl.c_str());
		return false;
	}

	if (!MediaHub_Sha256Hex(digest).equalsIgnoreCase(expectedSha256Hex)) {
		gFSystem.remove(tmpPath);
		Log_Printf(LOGLEVEL_ERROR, mediaHubVerifyFailed, fileUrl.c_str());
		return false;
	}

	gFSystem.remove(finalPath); // clears a stale leftover from an earlier aborted sync, if any
	if (!gFSystem.rename(tmpPath, finalPath)) {
		gFSystem.remove(tmpPath);
		Log_Printf(LOGLEVEL_ERROR, mediaHubDownloadFailed, fileUrl.c_str());
		return false;
	}
	return true;
}

// Downloads every file in `files` that isn't already fully synced in mediaDir.
// Checks free SD space against the total size of missing files up front
// (concept §13) so a card is never left half-downloaded due to running out
// of space mid-way through.
static bool MediaHub_SyncMissingFiles(const String &filesBaseUrl, const String &mediaDir, JsonArrayConst files) {
	// Total-vs-missing split doubles as the progress-bar baseline (concept
	// §7.1): a card that already has some files from an earlier partial sync
	// starts the bar accordingly, instead of jumping back to 0.
	uint64_t totalBytes = 0;
	uint64_t missingBytes = 0;
	for (JsonVariantConst f : files) {
		const uint32_t size = f["size"] | 0;
		totalBytes += size;
		if (!MediaHub_FileFullySynced(mediaDir, f)) {
			missingBytes += size;
		}
	}
	if (missingBytes == 0) {
		return true; // already fully synced
	}
	if (SdCard_GetFreeSize() < missingBytes) {
		Log_Println(mediaHubSdFull, LOGLEVEL_ERROR);
		return false;
	}

	MediaHub_BusyGuard busyGuard;
	uint64_t completedBytes = totalBytes - missingBytes;
	if (totalBytes > 0) {
		Led_SetDownloadProgress(true, (uint8_t) ((completedBytes * 100) / totalBytes));
	}
	for (JsonVariantConst f : files) {
		if (MediaHub_FileFullySynced(mediaDir, f)) {
			continue;
		}
		const char *path = f["path"] | "";
		const uint32_t size = f["size"] | 0;
		const char *sha256 = f["sha256"] | "";
		if (strlen(path) == 0 || strlen(sha256) == 0) {
			Log_Println(mediaHubInvalidManifest, LOGLEVEL_ERROR);
			return false;
		}
		Log_Printf(LOGLEVEL_NOTICE, mediaHubDownloadingFile, path);
		if (!MediaHub_DownloadAndVerifyFile(filesBaseUrl + path, mediaDir + "/" + path, size, sha256, completedBytes, totalBytes)) {
			return false;
		}
	}
	return true;
}

// Writes the just-fetched manifest body to the local cache, best-effort: a
// failure here doesn't block playback since the manifest is already parsed
// in memory, it just means this device won't have an offline copy for the
// next tap.
static void MediaHub_WriteManifestCache(const char *cardId, const String &body) {
	File f = gFSystem.open(MediaHub_ManifestCachePath(cardId), FILE_WRITE, true);
	if (!f) {
		return;
	}
	f.print(body);
	f.close();
}

// Fetches missing files and then plays (concept §10 "EnsureCard" core, sans
// the "stale"/re-sync wipe which is Phase 6). Called after a live manifest
// fetch for a file-based (non-webradio) manifest.
static bool MediaHub_SyncAndPlay(const char *cardId, JsonDocument &doc, uint32_t manifestPlayMode, uint32_t lastPlayPos, uint16_t trackLastPlayed) {
	const char *filesBaseUrl = doc["filesBaseUrl"] | "";
	JsonArrayConst files = doc["files"].as<JsonArrayConst>();
	if (strlen(filesBaseUrl) == 0 || files.size() == 0) {
		Log_Println(mediaHubInvalidManifest, LOGLEVEL_ERROR);
		return false;
	}

	const String mediaDir = MediaHub_MediaDir(cardId);
	if (!MediaHub_SyncMissingFiles(filesBaseUrl, mediaDir, files)) {
		return false;
	}

	const String itemToPlay = MediaHub_BuildItemToPlay(mediaDir, manifestPlayMode, files);
	Log_Println(mediaHubPlayingAfterSync, LOGLEVEL_NOTICE);
	AudioPlayer_SetPlaylist(itemToPlay.c_str(), lastPlayPos, manifestPlayMode, trackLastPlayed);
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

	const String itemToPlay = MediaHub_BuildItemToPlay(mediaDir, manifestPlayMode, files);

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
	if (MediaHub_DownloadBusy) {
		Log_Println(mediaHubBusy, LOGLEVEL_NOTICE);
		System_IndicateError();
		return;
	}

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

	// Cross-check that the manifest actually belongs to the tapped card
	// (concept §7.1) before trusting/caching any of it.
	const char *manifestCardId = doc["cardId"] | "";
	if (strcmp(manifestCardId, cardId) != 0) {
		Log_Println(mediaHubInvalidManifest, LOGLEVEL_ERROR);
		System_IndicateError();
		return;
	}
	MediaHub_WriteManifestCache(cardId, body);

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

	if (!MediaHub_SyncAndPlay(cardId, doc, manifestPlayMode, lastPlayPos, trackLastPlayed)) {
		System_IndicateError();
	}
}

// Escape-hatch (concept §9/#7): discards the local cache for one card so the
// next tap re-fetches the manifest and re-downloads everything from scratch.
// Not wired to a trigger yet (Admin-Karte / Hub-Button lands in a later
// phase alongside the "stale" mechanism it shares its machinery with).
bool MediaHub_ForceRefresh(const char *cardId) {
	gFSystem.remove(MediaHub_ManifestCachePath(cardId));
	File mediaDir = gFSystem.open(MediaHub_MediaDir(cardId));
	if (!mediaDir || !mediaDir.isDirectory()) {
		return true; // nothing to wipe
	}
	return MediaHub_DeleteDirRecursive(mediaDir);
}

// Same escape-hatch for every MediaHub-managed card at once ("für alle",
// concept §9).
bool MediaHub_ForceRefreshAll() {
	bool ok = true;
	File manifestsDir = gFSystem.open("/.mediahub/manifests");
	if (manifestsDir && manifestsDir.isDirectory()) {
		ok &= MediaHub_DeleteDirRecursive(manifestsDir);
	}
	File mediaRootDir = gFSystem.open("/.mediahub/media");
	if (mediaRootDir && mediaRootDir.isDirectory()) {
		ok &= MediaHub_DeleteDirRecursive(mediaRootDir);
	}
	return ok;
}
