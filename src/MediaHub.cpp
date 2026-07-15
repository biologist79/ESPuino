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
#include <atomic>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
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
// Matches start_chunk_size in Web.cpp's upload path: bigger chunks mean fewer
// read()/write() round-trips per file, which is where most of the throughput
// difference against the (~450 kB/s) upload path came from besides power-save.
static constexpr size_t MediaHub_DownloadBufferSize = 16384;

// Set for the duration of an actual file download (concept §14/#16): a card
// tapped while this is true gets a "busy" error instead of being processed.
static volatile bool MediaHub_DownloadBusy = false;

struct MediaHub_BusyGuard {
	MediaHub_BusyGuard() {
		MediaHub_DownloadBusy = true;
		// Full WiFi power for the duration of the download - same win the
		// upload path gets from System_PauseTasksDuringUpload(). Deliberately
		// not reusing that function: it also suspends Led_Task and the RFID
		// task, which would kill the non-suspending download animation from
		// Phase 5 and pause the very task this download is running on.
		Wlan_SetPowerSave(false);
	}
	~MediaHub_BusyGuard() {
		MediaHub_DownloadBusy = false;
		Wlan_SetPowerSave(true);
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

// "stale"/"needs resync" (concept §9/§13) share one marker and one recovery
// path: both mean "the next tap should wipe and fully re-download". A
// re-sync that fails partway simply never clears the marker, which is
// exactly the "needs resync" behavior the concept describes.
static String MediaHub_StaleMarkerPath(const char *cardId) {
	return "/.mediahub/manifests/" + String(cardId) + ".stale";
}

static bool MediaHub_IsStale(const char *cardId) {
	return gFSystem.exists(MediaHub_StaleMarkerPath(cardId));
}

static void MediaHub_MarkStale(const char *cardId) {
	File f = gFSystem.open(MediaHub_StaleMarkerPath(cardId), FILE_WRITE, true);
	if (f) {
		f.close();
	}
}

static void MediaHub_ClearStale(const char *cardId) {
	gFSystem.remove(MediaHub_StaleMarkerPath(cardId));
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

// Same File-object-based recursion as MediaHub_DeleteDirRecursive(), for the
// same reason: avoids re-sanitizing an already-sanitized on-disk path.
static uint64_t MediaHub_DirSizeRecursive(File dir) {
	uint64_t total = 0;
	File entry = dir.openNextFile();
	while (entry) {
		if (entry.isDirectory()) {
			total += MediaHub_DirSizeRecursive(entry);
		} else {
			total += entry.size();
		}
		entry = dir.openNextFile();
		esp_task_wdt_reset();
	}
	return total;
}

static uint64_t MediaHub_DirSize(const String &path) {
	File dir = gFSystem.open(path);
	if (!dir || !dir.isDirectory()) {
		return 0;
	}
	return MediaHub_DirSizeRecursive(dir);
}

// Percent-encodes a manifest file path for use in the download URL. Library
// paths come straight from the hub's filesystem and routinely contain
// spaces or other characters that aren't valid unencoded in a URL (distinct
// from SanitizedFS's FAT-illegal-char encoding used for local storage,
// which is unrelated and already handled elsewhere). '/' is preserved since
// it's the path separator, not data to encode.
static String MediaHub_UrlEncodePath(const String &path) {
	String encoded;
	encoded.reserve(path.length());
	for (size_t i = 0; i < path.length(); i++) {
		const uint8_t c = (uint8_t) path[i];
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
			encoded += (char) c;
		} else {
			char hex[4];
			snprintf(hex, sizeof(hex), "%%%02X", c);
			encoded += hex;
		}
	}
	return encoded;
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

// Double-buffered download/write, mirroring explorerHandleFileUpload() /
// explorerHandleFileStorageTask() in Web.cpp: while one buffer is being
// written to SD by a dedicated task, the network read loop already fills the
// other one, instead of serializing "read network" and "write SD" on the same
// task. File-scope statics (not per-call locals) are fine here: only one
// download ever runs at a time (MediaHub_BusyGuard), and re-initialized at
// the top of every MediaHub_DownloadAndVerifyFile() call.
static constexpr size_t MediaHub_DownloadNumBuffers = 2;
static uint8_t MediaHub_DownloadBuffers[MediaHub_DownloadNumBuffers][MediaHub_DownloadBufferSize];
static std::atomic<uint32_t> MediaHub_DownloadBufferBytes[MediaHub_DownloadNumBuffers];
static std::atomic<bool> MediaHub_DownloadBufferFull[MediaHub_DownloadNumBuffers];
static TaskHandle_t MediaHub_DownloadWriterTaskHandle = NULL;
static SemaphoreHandle_t MediaHub_DownloadWriterDone = NULL;
static volatile bool MediaHub_DownloadWriterError = false;

struct MediaHub_WriterTaskArgs {
	File file; // already open at index 0, created by the caller
};

// Drains full buffers to `file` as they become available. Notification value
// 1 = producer done normally (drain what's left, then exit); 2 = producer
// aborted (exit immediately, whatever's unwritten doesn't matter since the
// caller discards the .tmp file on any error anyway).
static void MediaHub_DownloadWriterTask(void *parameter) {
	auto *args = static_cast<MediaHub_WriterTaskArgs *>(parameter);
	File file = args->file;
	delete args;

	uint32_t readIndex = 0;
	for (;;) {
		uint32_t notifyValue = 0;
		const BaseType_t notified = xTaskNotifyWait(0, 0, &notifyValue, 0);
		if (notified == pdPASS && notifyValue == 2u) {
			break; // producer aborted, don't bother writing anything more
		}
		if (MediaHub_DownloadBufferFull[readIndex]) {
			while (MediaHub_DownloadBufferFull[readIndex]) {
				const uint32_t len = MediaHub_DownloadBufferBytes[readIndex];
				if (len > 0 && file.write(MediaHub_DownloadBuffers[readIndex], len) != len) {
					MediaHub_DownloadWriterError = true;
				}
				MediaHub_DownloadBufferBytes[readIndex] = 0;
				MediaHub_DownloadBufferFull[readIndex] = false;
				readIndex = (readIndex + 1) % MediaHub_DownloadNumBuffers;
				if (MediaHub_DownloadWriterError) {
					break; // don't keep writing to a file that just failed
				}
				esp_task_wdt_reset();
			}
			if (MediaHub_DownloadWriterError || (notified == pdPASS && notifyValue == 1u)) {
				break; // write failed, or that was the last (partial) buffer
			}
		} else if (notified == pdPASS && notifyValue == 1u) {
			break; // done, and nothing was left to drain
		} else {
			vTaskDelay(pdMS_TO_TICKS(1));
		}
	}
	file.close();
	MediaHub_DownloadWriterTaskHandle = NULL;
	xSemaphoreGive(MediaHub_DownloadWriterDone);
	vTaskDelete(NULL);
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
		Log_Printf(LOGLEVEL_ERROR, mediaHubDownloadHttpError, httpCode, fileUrl.c_str());
		http.end();
		return false;
	}

	const String tmpPath = finalPath + ".tmp";
	File tmpFile = gFSystem.open(tmpPath, FILE_WRITE, true); // create=true: also creates missing parent dirs
	if (!tmpFile) {
		Log_Printf(LOGLEVEL_ERROR, mediaHubDownloadFileError, fileUrl.c_str());
		http.end();
		return false;
	}

	mbedtls_sha256_context shaCtx;
	mbedtls_sha256_init(&shaCtx);
	mbedtls_sha256_starts(&shaCtx, 0); // 0 = SHA-256 (not the SHA-224 variant)

	// Hand the open file off to the writer task; from here on the producer
	// (this function/task) only ever touches the buffers, never the file.
	for (size_t i = 0; i < MediaHub_DownloadNumBuffers; i++) {
		MediaHub_DownloadBufferBytes[i] = 0;
		MediaHub_DownloadBufferFull[i] = false;
	}
	MediaHub_DownloadWriterError = false;
	if (MediaHub_DownloadWriterDone == NULL) {
		MediaHub_DownloadWriterDone = xSemaphoreCreateBinary();
	} else {
		xSemaphoreTake(MediaHub_DownloadWriterDone, 0); // make sure it's empty before this run
	}
	auto *writerArgs = new MediaHub_WriterTaskArgs {tmpFile};
	xTaskCreatePinnedToCore(MediaHub_DownloadWriterTask, "MediaHubWriter", 3072, writerArgs, 2, &MediaHub_DownloadWriterTaskHandle, 1);

	auto *stream = http.getStreamPtr();
	size_t totalRead = 0;
	uint32_t writeIndex = 0;
	bool transferError = false;
	bool restartRequested = false;
	const uint32_t transferStartMs = millis();
	uint32_t lastDataMs = transferStartMs;

	while (totalRead < expectedSize) {
		// A pending restart/shutdown must never wait out a multi-minute download
		// first (concept-adjacent hardening): bail out now, System_Cyclic() picks
		// it up as soon as this call unwinds back to loop(). Whatever's on disk
		// stays a discarded .tmp file either way, so there's nothing to protect.
		if (System_IsRestartOrSleepPending()) {
			restartRequested = true;
			break;
		}
		if (MediaHub_DownloadWriterError) {
			transferError = true;
			break;
		}
		const int avail = stream->available();
		if (avail <= 0) {
			if (millis() - lastDataMs > MediaHub_DownloadStallTimeoutMs) {
				transferError = true;
				break;
			}
			delay(1);
			continue;
		}
		// wait for the writer task to finish draining this buffer before reusing it
		while (MediaHub_DownloadBufferFull[writeIndex]) {
			if (MediaHub_DownloadWriterError) {
				transferError = true;
				break;
			}
			if (System_IsRestartOrSleepPending()) {
				restartRequested = true;
				break;
			}
			vTaskDelay(pdMS_TO_TICKS(1));
		}
		if (transferError || restartRequested) {
			break;
		}

		const size_t bufferedSoFar = MediaHub_DownloadBufferBytes[writeIndex];
		const size_t spaceLeft = MediaHub_DownloadBufferSize - bufferedSoFar;
		const size_t toRead = std::min((size_t) avail, spaceLeft);
		const size_t got = stream->readBytes(MediaHub_DownloadBuffers[writeIndex] + bufferedSoFar, toRead);
		if (got == 0) {
			transferError = true;
			break;
		}
		lastDataMs = millis();
		mbedtls_sha256_update(&shaCtx, MediaHub_DownloadBuffers[writeIndex] + bufferedSoFar, got);
		MediaHub_DownloadBufferBytes[writeIndex] = bufferedSoFar + got;
		totalRead += got;
		completedBytes += got;
		if (totalBytes > 0) {
			Led_SetDownloadProgress(true, (uint8_t) std::min<uint64_t>(100, (completedBytes * 100) / totalBytes));
		}
		if (MediaHub_DownloadBufferBytes[writeIndex] == MediaHub_DownloadBufferSize) {
			MediaHub_DownloadBufferFull[writeIndex] = true;
			writeIndex = (writeIndex + 1) % MediaHub_DownloadNumBuffers;
		}
		esp_task_wdt_reset();
	}
	http.end();

	// hand off whatever's left in the current buffer, then tell the writer
	// task we're done (2 = abort, 1 = normal finish - drain and exit either way)
	if (MediaHub_DownloadBufferBytes[writeIndex] > 0 && !MediaHub_DownloadBufferFull[writeIndex]) {
		MediaHub_DownloadBufferFull[writeIndex] = true;
	}
	xTaskNotify(MediaHub_DownloadWriterTaskHandle, (transferError || restartRequested) ? 2u : 1u, eSetValueWithOverwrite);
	if (xSemaphoreTake(MediaHub_DownloadWriterDone, pdMS_TO_TICKS(30000)) != pdTRUE) {
		// writer task got stuck somehow - don't hang forever, just report failure
		transferError = true;
	}
	if (MediaHub_DownloadWriterError) {
		transferError = true;
	}

	uint8_t digest[32];
	mbedtls_sha256_finish(&shaCtx, digest);
	mbedtls_sha256_free(&shaCtx);

	if (restartRequested) {
		gFSystem.remove(tmpPath);
		Log_Println(mediaHubDownloadAbortedForRestart, LOGLEVEL_NOTICE);
		return false;
	}

	if (transferError || totalRead != expectedSize) {
		gFSystem.remove(tmpPath);
		Log_Printf(LOGLEVEL_ERROR, mediaHubDownloadTransferError, fileUrl.c_str(), (unsigned) totalRead, (unsigned) expectedSize);
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
		Log_Printf(LOGLEVEL_ERROR, mediaHubDownloadRenameError, fileUrl.c_str());
		return false;
	}

	const uint32_t elapsedMs = millis() - transferStartMs;
	const float rateKBs = elapsedMs > 0 ? (totalRead / 1024.0f) / (elapsedMs / 1000.0f) : 0.0f;
	Log_Printf(LOGLEVEL_NOTICE, mediaHubDownloadRate, fileUrl.c_str(), (unsigned) totalRead, rateKBs);
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
		// URL-encode only for the HTTP request; the local path (mediaDir + path)
		// stays as-is for SanitizedFS, which does its own FAT-safe encoding.
		if (!MediaHub_DownloadAndVerifyFile(filesBaseUrl + MediaHub_UrlEncodePath(path), mediaDir + "/" + path, size, sha256, completedBytes, totalBytes)) {
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

// Re-sync flow for a card already marked "stale" (concept §11/§13): fetches
// the fresh manifest live, then wipes the media folder and re-downloads
// everything. Deliberately never starts playback on success (unlike the
// concept's original "RS -> PLAY" flowchart, §11): a re-sync can take
// minutes, and starting playback of whatever just finished downloading,
// unprompted, after that long a silent wait surprised more than it helped in
// practice - the next tap plays it instead, by then straight from the now-
// fully-synced local cache. Returns false for anything that leaves the OLD,
// still-complete local copy as the better fallback (hub unreachable, bad
// manifest, SD too full for the new version) — the caller then plays that
// old copy instead, and the card stays marked "stale" for the next attempt.
// Only clears "stale" on full success.
static bool MediaHub_TryReSync(const char *cardId, const String &hostPort) {
	String espId = MediaHub_GetEspId();
	String url = "http://" + hostPort + "/" + espId + "/card/" + String(cardId) + "/manifest.json";

	HTTPClient http;
	http.setConnectTimeout(MediaHub_ConnectTimeoutMs);
	http.setTimeout(MediaHub_ReadTimeoutMs);
	if (!http.begin(url)) {
		return false;
	}
	const int httpCode = http.GET();
	if (httpCode != HTTP_CODE_OK) {
		http.end();
		return false;
	}
	const String body = http.getString();
	http.end();

	JsonDocument doc;
	if (deserializeJson(doc, body)) {
		return false;
	}
	const char *manifestCardId = doc["cardId"] | "";
	if (strcmp(manifestCardId, cardId) != 0) {
		return false;
	}

	Log_Println(mediaHubResyncing, LOGLEVEL_NOTICE);

	const uint32_t manifestPlayMode = doc["playMode"] | 0;
	if (manifestPlayMode == WEBSTREAM) {
		const char *stream = doc["stream"] | "";
		if (strlen(stream) == 0) {
			return false;
		}
		MediaHub_WriteManifestCache(cardId, body);
		MediaHub_ClearStale(cardId);
		Log_Println(mediaHubResyncComplete, LOGLEVEL_NOTICE);
		System_IndicateOk();
		return true;
	}

	const char *filesBaseUrl = doc["filesBaseUrl"] | "";
	JsonArrayConst files = doc["files"].as<JsonArrayConst>();
	if (strlen(filesBaseUrl) == 0 || files.size() == 0) {
		return false;
	}

	uint64_t totalNeeded = 0;
	for (JsonVariantConst f : files) {
		totalNeeded += (uint32_t) (f["size"] | 0);
	}

	const String mediaDir = MediaHub_MediaDir(cardId);
	const uint64_t oldFolderSize = MediaHub_DirSize(mediaDir);
	if (SdCard_GetFreeSize() + oldFolderSize < totalNeeded) {
		// Old, working version stays untouched (nothing wiped yet); card
		// remains "stale" so this is retried on the next tap.
		Log_Println(mediaHubSdFull, LOGLEVEL_ERROR);
		return false;
	}

	File oldDir = gFSystem.open(mediaDir);
	if (oldDir && oldDir.isDirectory()) {
		MediaHub_DeleteDirRecursive(oldDir);
	}
	// Past this point there's no complete old copy left to fall back to: any
	// further failure leaves the card marked "stale" ("needs resync") for a
	// retry on the next tap, exactly like a fresh sync failure would.

	MediaHub_WriteManifestCache(cardId, body);
	if (!MediaHub_SyncMissingFiles(filesBaseUrl, mediaDir, files)) {
		return false;
	}

	MediaHub_ClearStale(cardId);
	Log_Println(mediaHubResyncComplete, LOGLEVEL_NOTICE);
	System_IndicateOk();
	return true;
}

struct MediaHub_VersionCheckArgs {
	String cardId;
	String hostPort;
};

// Runs on its own short-lived task so it never delays returning from
// MediaHub_HandleCardTapped (concept §9/§11: "Hintergrund"-check must not
// block the tap that's already playing). Only ever compares/marks — it
// never downloads or touches the manifest cache; the actual update happens
// via MediaHub_TryReSync() on a later tap.
static void MediaHub_VersionCheckTask(void *pvParameters) {
	auto *args = static_cast<MediaHub_VersionCheckArgs *>(pvParameters);

	String cachedVersion;
	File cacheFile = gFSystem.open(MediaHub_ManifestCachePath(args->cardId.c_str()));
	if (cacheFile && !cacheFile.isDirectory()) {
		JsonDocument cachedDoc;
		if (!deserializeJson(cachedDoc, cacheFile)) {
			cachedVersion = String((const char *) (cachedDoc["version"] | ""));
		}
		cacheFile.close();
	}

	if (cachedVersion.length() > 0) {
		String espId = MediaHub_GetEspId();
		String url = "http://" + args->hostPort + "/" + espId + "/card/" + args->cardId + "/manifest.json";

		HTTPClient http;
		http.setConnectTimeout(MediaHub_ConnectTimeoutMs);
		http.setTimeout(MediaHub_ReadTimeoutMs);
		if (http.begin(url)) {
			const int httpCode = http.GET();
			if (httpCode == HTTP_CODE_OK) {
				const String body = http.getString();
				JsonDocument doc;
				if (!deserializeJson(doc, body)) {
					const char *manifestCardId = doc["cardId"] | "";
					const char *freshVersion = doc["version"] | "";
					if (strcmp(manifestCardId, args->cardId.c_str()) == 0 && strlen(freshVersion) > 0 && cachedVersion != freshVersion) {
						MediaHub_MarkStale(args->cardId.c_str());
						Log_Println(mediaHubMarkedStale, LOGLEVEL_NOTICE);
					}
				}
			}
			http.end();
		}
	}

	delete args;
	vTaskDelete(NULL);
}

static void MediaHub_StartBackgroundVersionCheck(const char *cardId, const String &hostPort) {
	auto *args = new MediaHub_VersionCheckArgs {String(cardId), hostPort};
	xTaskCreatePinnedToCore(MediaHub_VersionCheckTask, "MediaHubVerChk", 4096, args, 1, NULL, 1);
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

	String hostPort = String(path).substring(strlen(MediaHub_PathPrefix));
	if (hostPort.length() == 0) {
		Log_Println(mediaHubInvalidPath, LOGLEVEL_ERROR);
		System_IndicateError();
		return;
	}

	const bool online = Wlan_IsConnected();

	if (MediaHub_IsStale(cardId) && online) {
		if (MediaHub_TryReSync(cardId, hostPort)) {
			return; // re-synced; this tap doesn't play, the next one does (see MediaHub_TryReSync())
		}
		// Re-sync wasn't possible right now (hub unreachable, bad manifest, SD
		// full for the new version, ...) — fall through and play the old,
		// still-complete local copy instead (concept §14: never block on a
		// transient hub failure while a working copy exists).
	}

	if (MediaHub_TryPlayFromLocalCache(cardId, lastPlayPos, trackLastPlayed)) {
		if (online) {
			MediaHub_StartBackgroundVersionCheck(cardId, hostPort);
		}
		return;
	}

	if (!online) {
		Log_Println(mediaHubNotReachable, LOGLEVEL_ERROR);
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
		MediaHub_StartBackgroundVersionCheck(cardId, hostPort);
		return;
	}

	if (MediaHub_SyncAndPlay(cardId, doc, manifestPlayMode, lastPlayPos, trackLastPlayed)) {
		MediaHub_StartBackgroundVersionCheck(cardId, hostPort);
	} else {
		System_IndicateError();
	}
}

// Removes everything MediaHub keeps locally for one card: manifest cache,
// stale marker, and downloaded media. Shared by MediaHub_ForceRefresh() (the
// card comes back on the next tap) and MediaHub_DeleteCard() (it doesn't).
static bool MediaHub_WipeCard(const char *cardId) {
	gFSystem.remove(MediaHub_ManifestCachePath(cardId));
	MediaHub_ClearStale(cardId);
	File mediaDir = gFSystem.open(MediaHub_MediaDir(cardId));
	if (!mediaDir || !mediaDir.isDirectory()) {
		return true; // nothing to wipe
	}
	return MediaHub_DeleteDirRecursive(mediaDir);
}

// Escape-hatch (concept §9/#7): discards the local cache for one card so the
// next tap re-fetches the manifest and re-downloads everything from scratch.
// Not wired to a trigger yet (Admin-Karte / Hub-Button lands in a later
// phase alongside the "stale" mechanism it shares its machinery with).
bool MediaHub_ForceRefresh(const char *cardId) {
	return MediaHub_WipeCard(cardId);
}

// REST-cascade target for DELETE /rfid?id=<cardId> (concept §13.1): called by
// Web.cpp's handleDeleteRFIDRequest() for MediaHub-managed cards, in addition
// to (not instead of) removing the NVS entry itself.
bool MediaHub_DeleteCard(const char *cardId) {
	return MediaHub_WipeCard(cardId);
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

// Registered media servers (concept §5.1): stored as one JSON array under a
// single settings key rather than mirroring Wlan.cpp's per-entry NVS-key
// scheme, since this is always a short list of two-string records - a
// dedicated per-entry NVS layout would be complexity this doesn't need.
static constexpr const char *MediaHub_ServersNvsKey = "mediaHubSrvs";
static constexpr uint8_t MediaHub_MaxServers = 10;

std::vector<MediaHubServer> MediaHub_GetServers() {
	std::vector<MediaHubServer> servers;
	const String json = gPrefsSettings.getString(MediaHub_ServersNvsKey, "[]");
	JsonDocument doc;
	if (deserializeJson(doc, json)) {
		return servers; // corrupt/missing -> treat as empty
	}
	for (JsonVariantConst entry : doc.as<JsonArrayConst>()) {
		MediaHubServer s;
		s.name = entry["name"] | "";
		s.hostPort = entry["hostPort"] | "";
		if (s.name.length() > 0 && s.hostPort.length() > 0) {
			servers.push_back(s);
		}
	}
	return servers;
}

static bool MediaHub_SaveServers(const std::vector<MediaHubServer> &servers) {
	JsonDocument doc;
	JsonArray arr = doc.to<JsonArray>();
	for (const auto &s : servers) {
		JsonObject o = arr.add<JsonObject>();
		o["name"] = s.name;
		o["hostPort"] = s.hostPort;
	}
	String json;
	serializeJson(doc, json);
	return gPrefsSettings.putString(MediaHub_ServersNvsKey, json) == json.length();
}

bool MediaHub_SaveServer(const String &name, const String &hostPort) {
	if (name.length() == 0 || hostPort.length() == 0) {
		return false;
	}
	std::vector<MediaHubServer> servers = MediaHub_GetServers();
	for (auto &s : servers) {
		if (s.name == name) {
			s.hostPort = hostPort;
			return MediaHub_SaveServers(servers);
		}
	}
	if (servers.size() >= MediaHub_MaxServers) {
		return false;
	}
	servers.push_back({name, hostPort});
	return MediaHub_SaveServers(servers);
}

bool MediaHub_DeleteServer(const String &name) {
	std::vector<MediaHubServer> servers = MediaHub_GetServers();
	const size_t before = servers.size();
	servers.erase(std::remove_if(servers.begin(), servers.end(), [&name](const MediaHubServer &s) { return s.name == name; }), servers.end());
	if (servers.size() == before) {
		return false; // nothing to delete
	}
	return MediaHub_SaveServers(servers);
}
