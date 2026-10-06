#include <Arduino.h>
#include "settings.h"

#include "MediaHub.h"

#include "AudioPlayer.h"
#include "Common.h"
#include "Led.h"
#include "Log.h"
#include "Rfid.h"
#include "SdCard.h"
#include "System.h"
#include "Wlan.h"
#include "logmessages.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <algorithm>
#include <atomic>
#include <esp_heap_caps.h>
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

// The NVS path field's "hostPort" part may itself already carry a scheme
// (e.g. "https://myhub:8443", written there via the Web-UI's http/https
// dropdown, concept §5.1) - use it as-is if so, otherwise default to plain
// http:// for the common case. HTTPClient::begin() already handles either
// scheme transparently (falling back to an unverified/insecure TLS
// connection for https - fine for a local, trusted hub, concept #19).
static String MediaHub_BuildBaseUrl(const String &hostPort) {
	if (hostPort.startsWith("http://") || hostPort.startsWith("https://")) {
		return hostPort;
	}
	return "http://" + hostPort;
}

// "stale"/"needs resync" (concept §9/§13) share one marker and one recovery
// path: both mean "the next tap should wipe and fully re-download". A
// re-sync that fails partway simply never clears the marker, which is
// exactly the "needs resync" behavior the concept describes.
static String MediaHub_StaleMarkerPath(const char *cardId) {
	return "/.mediahub/manifests/" + String(cardId) + ".stale";
}

// True if the stale marker exists.
static bool MediaHub_IsStale(const char *cardId) {
	return gFSystem.exists(MediaHub_StaleMarkerPath(cardId));
}

// Creates the stale marker.
static void MediaHub_MarkStale(const char *cardId) {
	File f = gFSystem.open(MediaHub_StaleMarkerPath(cardId), FILE_WRITE, true);
	if (f) {
		f.close();
	}
}

// Removes the stale marker.
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

// True only if every file in `files` passes MediaHub_FileFullySynced().
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

// Directory portion of a manifest path, or "" if it sits at the library root.
static String MediaHub_DirOf(const String &path) {
	const int slash = path.lastIndexOf('/');
	if (slash < 0) {
		return "";
	}
	return path.substring(0, slash);
}

// Splits a path into its '/'-separated segments.
static std::vector<String> MediaHub_SplitPath(const String &path) {
	std::vector<String> segments;
	int start = 0;
	while (start <= (int) path.length()) {
		int slash = path.indexOf('/', start);
		if (slash < 0) {
			segments.push_back(path.substring(start));
			break;
		}
		segments.push_back(path.substring(start, slash));
		start = slash + 1;
	}
	return segments;
}

// Longest common leading path-segments across every file's directory.
// Non-single-file manifests are always built from one selected folder on the
// hub, optionally scanned recursively (concept §5.4) - this reliably yields
// exactly that folder back, regardless of how deep individual files end up
// nested under it (which AudioPlayer_SetPlaylist()'s own recursion, keyed
// off the real playMode, then handles from there). Returns "" if the files
// sit directly at the library root, i.e. mediaDir is already the right item.
static String MediaHub_CommonDirectory(JsonArrayConst files) {
	std::vector<String> common;
	bool first = true;
	for (JsonVariantConst f : files) {
		const char *path = f["path"] | "";
		const String dir = MediaHub_DirOf(String(path));
		std::vector<String> segments = dir.length() > 0 ? MediaHub_SplitPath(dir) : std::vector<String>();
		if (first) {
			common = segments;
			first = false;
			continue;
		}
		size_t matched = 0;
		while (matched < common.size() && matched < segments.size() && common[matched] == segments[matched]) {
			matched++;
		}
		common.resize(matched);
	}
	String result;
	for (size_t i = 0; i < common.size(); i++) {
		if (i > 0) {
			result += "/";
		}
		result += common[i];
	}
	return result;
}

// SINGLE_TRACK/SINGLE_TRACK_LOOP point at one specific file. Every other mode
// points at the folder the files actually share (see MediaHub_CommonDirectory())
// and leaves scanning/sorting/recursion from there to SdCard_ReturnPlaylist()
// (same reasoning as in AudioPlayer_SetPlaylist()) - pointing at mediaDir
// itself would be wrong whenever files[].path carries subdirectories
// (recursive selections, or any folder that isn't the media library root).
static String MediaHub_BuildItemToPlay(const String &mediaDir, uint32_t playMode, JsonArrayConst files) {
	if (MediaHub_IsSingleFilePlayMode(playMode)) {
		const char *singleFilePath = files[0]["path"] | "";
		return mediaDir + "/" + singleFilePath;
	}
	const String commonDir = MediaHub_CommonDirectory(files);
	if (commonDir.length() == 0) {
		return mediaDir;
	}
	return mediaDir + "/" + commonDir;
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

// Total size of an existing directory's contents, 0 if it doesn't exist.
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
// task.
//
// Allocated fresh per file, *after* that file's TLS handshake already
// succeeded, and freed again once the transfer is done - not as a permanent
// compile-time internal-DRAM array. A TLS handshake needs ~32-40 KB of
// *internal* heap for its fixed in/out buffers (CONFIG_MBEDTLS_SSL_VARIABLE_
// BUFFER_LENGTH is off); parking these 32 KB permanently in internal DRAM
// left too little of it free/contiguous for that handshake to succeed on
// https downloads (seen as HTTPC_ERROR_CONNECTION_REFUSED on real hardware).
// Internal RAM stays the preferred allocator (PSRAM is SPI-attached and
// measurably slower - moving these buffers there permanently cost the
// throughput this double-buffering exists for in the first place); PSRAM is
// only a last-resort fallback if internal allocation fails even at this
// later, safer point.
static constexpr size_t MediaHub_DownloadNumBuffers = 2;
static uint8_t *MediaHub_DownloadBuffers[MediaHub_DownloadNumBuffers] = {nullptr, nullptr};
static std::atomic<uint32_t> MediaHub_DownloadBufferBytes[MediaHub_DownloadNumBuffers];
static std::atomic<bool> MediaHub_DownloadBufferFull[MediaHub_DownloadNumBuffers];
static TaskHandle_t MediaHub_DownloadWriterTaskHandle = NULL;
static SemaphoreHandle_t MediaHub_DownloadWriterDone = NULL;
static volatile bool MediaHub_DownloadWriterError = false;

static bool MediaHub_EnsureDownloadBuffersAllocated() {
	for (size_t i = 0; i < MediaHub_DownloadNumBuffers; i++) {
		if (MediaHub_DownloadBuffers[i] != nullptr) {
			continue;
		}
		MediaHub_DownloadBuffers[i] = static_cast<uint8_t *>(heap_caps_malloc(MediaHub_DownloadBufferSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
		if (MediaHub_DownloadBuffers[i] == nullptr) {
			MediaHub_DownloadBuffers[i] = static_cast<uint8_t *>(heap_caps_malloc(MediaHub_DownloadBufferSize, MALLOC_CAP_SPIRAM));
		}
		if (MediaHub_DownloadBuffers[i] == nullptr) {
			return false;
		}
	}
	return true;
}

// Releases the two download buffers again once a file's transfer is done, so
// they aren't parked in internal RAM across the next file's TLS handshake.
static void MediaHub_FreeDownloadBuffers() {
	for (size_t i = 0; i < MediaHub_DownloadNumBuffers; i++) {
		heap_caps_free(MediaHub_DownloadBuffers[i]);
		MediaHub_DownloadBuffers[i] = nullptr;
	}
}

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

	// The handshake (connect + TLS) happens here, before the download buffers
	// exist - so it never has to compete with them for internal heap.
	const int httpCode = http.GET();
	if (httpCode != HTTP_CODE_OK) {
		Log_Printf(LOGLEVEL_ERROR, mediaHubDownloadHttpError, httpCode, fileUrl.c_str());
		http.end();
		return false;
	}

	if (!MediaHub_EnsureDownloadBuffersAllocated()) {
		Log_Printf(LOGLEVEL_ERROR, mediaHubDownloadFailed, fileUrl.c_str());
		http.end();
		return false;
	}

	const String tmpPath = finalPath + ".tmp";
	File tmpFile = gFSystem.open(tmpPath, FILE_WRITE, true); // create=true: also creates missing parent dirs
	if (!tmpFile) {
		Log_Printf(LOGLEVEL_ERROR, mediaHubDownloadFileError, fileUrl.c_str());
		http.end();
		MediaHub_FreeDownloadBuffers();
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

	// The writer task has now stopped touching the buffers either way (done
	// or aborted) - safe to release them before this file's next steps run,
	// so a following file's TLS handshake doesn't have to compete with them.
	MediaHub_FreeDownloadBuffers();

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

	// Temporary diagnostic (kept for easy re-enabling, not deleted): confirms
	// the per-file alloc/free of the download buffers isn't fragmenting the
	// internal heap over a multi-file sync. Re-enable if fragmentation is
	// ever suspected again - was confirmed stable across a multi-file test.
	// Log_Printf(LOGLEVEL_NOTICE, "MediaHub: internal heap largestFreeBlock=%u after %s", (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), fileUrl.c_str());

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

// Loads from the cached manifest exactly what the incremental re-sync below
// decides on: the force epoch, plus path + sha256 per file. The filter is what
// makes this affordable - a manifest is mostly SHA-256 sums, and nothing else
// ever reaches memory (same trick as MediaHub_GetCachedCardInfo()).
static bool MediaHub_LoadCachedSyncState(const char *cardId, JsonDocument &out) {
	File file = gFSystem.open(MediaHub_ManifestCachePath(cardId));
	if (!file || file.isDirectory()) {
		return false;
	}
	JsonDocument filter;
	filter["forceEpoch"] = true;
	filter["files"][0]["path"] = true;
	filter["files"][0]["sha256"] = true;
	const DeserializationError err = deserializeJson(out, file, DeserializationOption::Filter(filter));
	file.close();
	return !err;
}

// The entry for `path` in a manifest's files[], or a null variant.
static JsonVariantConst MediaHub_EntryForPath(JsonArrayConst files, const char *path) {
	for (JsonVariantConst f : files) {
		if (strcmp(f["path"] | "", path) == 0) {
			return f;
		}
	}
	return JsonVariantConst();
}

// True if the copy already on the SD card may stay where it is: the hub
// reports the same SHA-256 for this path as it did at the last sync, so the
// bytes down there are the ones the new manifest asks for. Both sums come out
// of manifests - the one just fetched and the one cached at the last sync -
// so nothing is read back off the card and nothing is re-hashed here (that is
// the whole point: hashing a 100 MB file would cost more than fetching it).
// The size check is only the cheap net against a copy truncated in between.
static bool MediaHub_MayKeepFile(const String &mediaDir, JsonVariantConst newEntry, JsonArrayConst cachedFiles) {
	const char *path = newEntry["path"] | "";
	const char *newSha = newEntry["sha256"] | "";
	if (strlen(path) == 0 || strlen(newSha) == 0) {
		return false;
	}
	const char *oldSha = MediaHub_EntryForPath(cachedFiles, path)["sha256"] | "";
	if (strlen(oldSha) == 0 || !String(oldSha).equalsIgnoreCase(newSha)) {
		return false;
	}
	return MediaHub_FileFullySynced(mediaDir, newEntry);
}

// Drops folders left empty after a file was pruned, walking up towards (but
// never including) the card's media dir. rmdir only succeeds on an empty
// folder, so a parent still holding other tracks simply stays.
static void MediaHub_RemoveEmptyParents(const String &mediaDir, const String &relPath) {
	int cut = relPath.lastIndexOf('/');
	while (cut > 0) {
		if (!gFSystem.rmdir(mediaDir + "/" + relPath.substring(0, cut))) {
			return;
		}
		cut = relPath.lastIndexOf('/', cut - 1);
	}
}

// Re-sync flow for a card already marked "stale" (concept §11/§13): fetches
// the fresh manifest live, stops playback, prunes what the hub changed or
// dropped, downloads only the difference (forum #4607) and then plays the
// fresh version - the concept's original "RS -> PLAY" flowchart (§11). That
// path was abandoned once because a re-sync meant minutes of silence and
// unprompted audio afterwards startled people; fetching only the difference
// takes that argument away, and since this function now stops playback
// itself, not playing would leave the listener in silence they never asked
// for. Returns false for anything that leaves the OLD, still-complete local
// copy as the better fallback (hub unreachable, bad manifest, SD too full for
// the new version) — nothing is stopped or deleted before those checks, so
// the caller simply plays that old copy and the card stays marked "stale" for
// the next attempt. Only clears "stale" on full success.
static bool MediaHub_TryReSync(const char *cardId, const String &hostPort, uint32_t lastPlayPos, uint16_t trackLastPlayed) {
	String espId = MediaHub_GetEspId();
	String url = MediaHub_BuildBaseUrl(hostPort) + "/" + espId + "/card/" + String(cardId) + "/manifest.json";

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

	// Stop whatever an earlier tap started before the download begins. Nothing
	// else does: the RFID handler dispatches here without touching playback,
	// and this function sets its own playlist only at the very end - so the
	// old card kept playing right through the transfer, competing with it for
	// SD and CPU, while the web UI went on showing its cover and track count
	// as if nothing had changed. Deliberately only here, past the manifest
	// fetch and its checks: an unreachable hub must not cost the listener
	// their playback, since that stale-but-complete copy is exactly what the
	// caller falls back to. STOP also clears the cover and sets NO_PLAYLIST,
	// so the web UI stops claiming a playlist that is being replaced
	// underneath it.
	//
	// AudioPlayer_SetTrackControl() only parks the command; AudioPlayer_Loop()
	// acts on it, and that runs in THIS task - main.cpp calls
	// AudioPlayer_Cyclic() a few lines above the RFID handler that lands here.
	// So it cannot run again until the download below returns, and the stop
	// would arrive *after* the playlist set at the end of this function and
	// kill the very playback it was meant to precede. Driving the loop once by
	// hand applies it now instead; AudioPlayer_Exit() uses the same trick to
	// make its pause take effect. Waiting for it would deadlock - the loop
	// that would clear it is the one being blocked.
	//
	// Only while something is playing: a track command on a finished playlist
	// is answered with System_IndicateError(), and a red blink for a card that
	// synced perfectly well would be a bug of its own.
	if (!gPlayProperties.playlistFinished) {
		AudioPlayer_SetTrackControl(STOP);
		AudioPlayer_Loop();
	}

	const String mediaDir = MediaHub_MediaDir(cardId);

	// Keep what the hub didn't change, instead of wiping the folder and
	// fetching all of it again (forum #4607): adding one track to a 150 MB
	// audiobook used to pull all 150 MB over the wire a second time. Telling
	// apart "same file" from "changed file" needs the manifest cached at the
	// last sync; without it there is no record of what is down there, and
	// wiping everything stays the only safe thing to do.
	JsonDocument cachedDoc;
	JsonArrayConst cachedFiles;
	uint32_t cachedForceEpoch = 0;
	if (MediaHub_LoadCachedSyncState(cardId, cachedDoc)) {
		cachedFiles = cachedDoc["files"].as<JsonArrayConst>();
		cachedForceEpoch = cachedDoc["forceEpoch"] | 0;
	}

	// "Force refresh" on the hub bumps an epoch and changes nothing else, so
	// the comparison below would find every file unchanged and keep all of
	// them - the button would do nothing at all. It has to overrule the
	// comparison rather than pass through it, because it means the one thing
	// a manifest can never tell us: that the LOCAL copy is not to be trusted.
	// (A manifest cached before the hub published this field reads 0, so a
	// card that was force-refreshed in the past gets one full re-sync once.)
	const bool forced = (uint32_t) (doc["forceEpoch"] | 0) != cachedForceEpoch;
	const bool incremental = cachedFiles.size() > 0 && !forced;

	// Both sums are settled before anything is deleted: the space check below
	// is the last moment at which the old copy is still complete, so it must
	// not depend on a prune that has already happened.
	uint64_t bytesToDownload = 0;
	for (JsonVariantConst f : files) {
		if (!incremental || !MediaHub_MayKeepFile(mediaDir, f, cachedFiles)) {
			bytesToDownload += (uint32_t) (f["size"] | 0);
		}
	}

	std::vector<String> toDelete;
	uint64_t bytesFreed = 0;
	if (incremental) {
		for (JsonVariantConst old : cachedFiles) {
			const char *path = old["path"] | "";
			if (strlen(path) == 0) {
				continue;
			}
			// A path the new manifest no longer lists resolves to a null
			// entry here, which MediaHub_MayKeepFile rejects - so dropped
			// tracks are pruned just like changed ones. That matters beyond
			// tidiness: the folder play modes play what lies in the
			// directory, not what the manifest lists.
			if (MediaHub_MayKeepFile(mediaDir, MediaHub_EntryForPath(files, path), cachedFiles)) {
				continue;
			}
			toDelete.push_back(String(path));
			File f = gFSystem.open(mediaDir + "/" + path);
			if (f) {
				if (!f.isDirectory()) {
					bytesFreed += f.size();
				}
				f.close();
			}
		}
	} else {
		bytesFreed = MediaHub_DirSize(mediaDir);
	}

	if (SdCard_GetFreeSize() + bytesFreed < bytesToDownload) {
		// Nothing deleted yet, so the old, working version stays untouched;
		// card remains "stale" so this is retried on the next tap.
		Log_Println(mediaHubSdFull, LOGLEVEL_ERROR);
		return false;
	}

	if (incremental) {
		for (const String &path : toDelete) {
			gFSystem.remove(mediaDir + "/" + path);
			gFSystem.remove(mediaDir + "/" + path + ".tmp"); // leftover of an interrupted download
			MediaHub_RemoveEmptyParents(mediaDir, path);
		}
		// Everything not explicitly kept has to be fetched, so clear the way
		// for it: MediaHub_SyncMissingFiles() decides on size alone, and would
		// skip an outdated leftover that happens to have the right length.
		// Deleting first makes "not kept" and "downloaded fresh" the same
		// thing, which is what the prune above relies on.
		for (JsonVariantConst f : files) {
			const char *path = f["path"] | "";
			if (strlen(path) > 0 && !MediaHub_MayKeepFile(mediaDir, f, cachedFiles)) {
				gFSystem.remove(mediaDir + "/" + path);
			}
		}
	} else {
		File oldDir = gFSystem.open(mediaDir);
		if (oldDir && oldDir.isDirectory()) {
			MediaHub_DeleteDirRecursive(oldDir);
		}
	}
	// Past this point the local copy is no longer the complete old version:
	// any further failure leaves the card marked "stale" ("needs resync") for
	// a retry on the next tap, exactly like a fresh sync failure would. What
	// survived the prune is reused by that retry, so an interrupted re-sync
	// resumes rather than starting over.

	MediaHub_WriteManifestCache(cardId, body);
	if (!MediaHub_SyncMissingFiles(filesBaseUrl, mediaDir, files)) {
		return false;
	}

	MediaHub_ClearStale(cardId);
	System_IndicateOk();

	// Play straight away rather than asking for another tap. The old rule
	// assumed a re-sync meant minutes of silence, after which unprompted audio
	// startled more than it helped - but a re-sync now fetches only what
	// actually changed, and the stop above means the listener is standing in
	// silence they did not ask for. Leaving them to tap again is the worse of
	// the two surprises.
	const String itemToPlay = MediaHub_BuildItemToPlay(mediaDir, manifestPlayMode, files);
	Log_Println(mediaHubPlayingAfterSync, LOGLEVEL_NOTICE);
	AudioPlayer_SetPlaylist(itemToPlay.c_str(), lastPlayPos, manifestPlayMode, trackLastPlayed);
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
		String url = MediaHub_BuildBaseUrl(args->hostPort) + "/" + espId + "/card/" + args->cardId + "/manifest.json";

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

// Spawns MediaHub_VersionCheckTask() and returns immediately.
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

// Dispatch entry point - see MediaHub.h for the contract. Order: local-cache
// fast path, then stale re-sync, then a live manifest fetch.
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
		if (MediaHub_TryReSync(cardId, hostPort, lastPlayPos, trackLastPlayed)) {
			return; // re-synced and playing the fresh version (see MediaHub_TryReSync())
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
	String url = MediaHub_BuildBaseUrl(hostPort) + "/" + espId + "/card/" + String(cardId) + "/manifest.json";

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
		s.https = entry["https"] | false; // absent (servers registered before https support) -> http
		if (s.name.length() > 0 && s.hostPort.length() > 0) {
			servers.push_back(s);
		}
	}
	return servers;
}

// Writes the full list back as one JSON blob (see MediaHub_ServersNvsKey).
static bool MediaHub_SaveServers(const std::vector<MediaHubServer> &servers) {
	JsonDocument doc;
	JsonArray arr = doc.to<JsonArray>();
	for (const auto &s : servers) {
		JsonObject o = arr.add<JsonObject>();
		o["name"] = s.name;
		o["hostPort"] = s.hostPort;
		o["https"] = s.https;
	}
	String json;
	serializeJson(doc, json);
	return gPrefsSettings.putString(MediaHub_ServersNvsKey, json) == json.length();
}

bool MediaHub_SaveServer(const String &name, const String &hostPort, bool https) {
	if (name.length() == 0 || hostPort.length() == 0) {
		return false;
	}
	std::vector<MediaHubServer> servers = MediaHub_GetServers();
	for (auto &s : servers) {
		if (s.name == name) {
			s.hostPort = hostPort;
			s.https = https;
			return MediaHub_SaveServers(servers);
		}
	}
	if (servers.size() >= MediaHub_MaxServers) {
		return false;
	}
	servers.push_back({name, hostPort, https});
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

bool MediaHub_GetCachedCardInfo(const char *cardId, MediaHubCardInfo &info) {
	File file = gFSystem.open(MediaHub_ManifestCachePath(cardId));
	if (!file || file.isDirectory()) {
		return false;
	}
	// Filtered on purpose: the files[] array with its SHA-256 sums is the bulk of
	// a manifest and none of it is shown, so it never even reaches memory. That
	// keeps this a few hundred bytes per card regardless of how big the audiobook
	// behind it is - which is what makes it affordable for every entry of the
	// assignment list.
	JsonDocument filter;
	filter["name"] = true;
	filter["playMode"] = true;
	JsonDocument doc;
	const DeserializationError err = deserializeJson(doc, file, DeserializationOption::Filter(filter));
	file.close();
	if (err) {
		return false;
	}

	info.name = doc["name"] | "";
	info.playMode = doc["playMode"] | 0;
	info.isWebstream = (info.playMode == WEBSTREAM);
	info.stale = MediaHub_IsStale(cardId);
	return true;
}

// ── Adopting unknown cards (forum #4779) ──────────────────────────────────
// A card that isn't in NVS is normally just an error. With this enabled, the
// registered hubs get asked first: if one of them already has the card
// assigned, it's adopted here instead of having to be assigned by hand in the
// web interface beforehand. The very same request also announces the card to
// every hub that doesn't know it -- the hub registers it as "pending" on a
// miss (concept §5.3) -- so one round trip does both jobs.
static constexpr const char *MediaHub_AskUnknownNvsKey = "mhAskUnknown";

// Hubs that failed at transport level, remembered for this session only. No
// timer, no expiry: an idle ESPuino goes to deep sleep, and waking from deep
// sleep *is* a restart (System.cpp, esp_deep_sleep_start()), so in practice
// the list clears itself several times a day -- and "until the next restart"
// is a rule a user can actually hold in their head. Saving the MediaHub
// settings clears it too, for the box that plays all afternoon without ever
// going to sleep.
static std::vector<String> MediaHub_SkippedHubs;

void MediaHub_ClearSkippedHubs() {
	MediaHub_SkippedHubs.clear();
}

bool MediaHub_IsAskUnknownEnabled() {
	return gPrefsSettings.getBool(MediaHub_AskUnknownNvsKey, false);
}

bool MediaHub_SetAskUnknownEnabled(bool enabled) {
	return gPrefsSettings.putBool(MediaHub_AskUnknownNvsKey, enabled);
}

// Everything MediaHub keeps locally lives under one hidden folder, which the
// file browser filters out along with the macOS leftovers. Showing it is
// opt-in, for looking at what actually got synced for a card.
const char *const MediaHub_RootDir = "/.mediahub";
static constexpr const char *MediaHub_ShowDirNvsKey = "mhShowDir";

bool MediaHub_IsShowDirEnabled() {
	return gPrefsSettings.getBool(MediaHub_ShowDirNvsKey, false);
}

bool MediaHub_SetShowDirEnabled(bool enabled) {
	return gPrefsSettings.putBool(MediaHub_ShowDirNvsKey, enabled);
}

enum class MediaHubProbeResult {
	Found, // hub has an assigned card and returned its manifest
	NotAssigned, // hub answered, but doesn't have this card (it's pending there now)
	Unreachable, // no answer at all -- transport level
};

static MediaHubProbeResult MediaHub_ProbeCard(const String &base, const char *cardId) {
	const String url = MediaHub_BuildBaseUrl(base) + "/" + MediaHub_GetEspId() + "/card/" + String(cardId) + "/manifest.json";

	HTTPClient http;
	http.setConnectTimeout(MediaHub_ConnectTimeoutMs);
	http.setTimeout(MediaHub_ReadTimeoutMs);
	if (!http.begin(url)) {
		return MediaHubProbeResult::Unreachable;
	}
	const int httpCode = http.GET();
	http.end();

	if (httpCode <= 0) {
		return MediaHubProbeResult::Unreachable;
	}
	// Only a transport failure counts as "unreachable". A 404 is the hub
	// answering properly -- it just doesn't have this card (yet) -- and must
	// never get the hub skipped for the rest of the session.
	return (httpCode == HTTP_CODE_OK) ? MediaHubProbeResult::Found : MediaHubProbeResult::NotAssigned;
}

bool MediaHub_TryAdoptUnknownCard(const char *cardId) {
	if (!MediaHub_IsAskUnknownEnabled() || !Wlan_IsConnected() || MediaHub_DownloadBusy) {
		return false;
	}
	const std::vector<MediaHubServer> servers = MediaHub_GetServers();
	if (servers.empty()) {
		return false;
	}

	for (const MediaHubServer &server : servers) {
		const String base = String(server.https ? "https://" : "http://") + server.hostPort;
		if (std::find(MediaHub_SkippedHubs.begin(), MediaHub_SkippedHubs.end(), base) != MediaHub_SkippedHubs.end()) {
			continue;
		}

		const MediaHubProbeResult probe = MediaHub_ProbeCard(base, cardId);
		if (probe == MediaHubProbeResult::Unreachable) {
			Log_Printf(LOGLEVEL_NOTICE, mediaHubHubSkipped, base.c_str());
			MediaHub_SkippedHubs.push_back(base);
			continue;
		}
		if (probe == MediaHubProbeResult::NotAssigned) {
			// The hub answered but doesn't have this card - and in doing so it has
			// just put it on its own pending list, which is the whole point of
			// asking even when we expect a miss.
			Log_Printf(LOGLEVEL_NOTICE, mediaHubReportedPending, base.c_str(), cardId);
			continue;
		}

		// First hub that answers wins, and the remaining ones are deliberately
		// left unasked: two hubs holding the same card is a configuration
		// error, and paying one round trip per hub on every tap to detect it
		// would slow down the case that actually happens. Which hub it was goes
		// into the log, and into the assignment itself -- visible in the web
		// interface from here on.
		const String nvsPath = String(MediaHub_PathPrefix) + base;
		if (!Rfid_SaveAssignment(cardId, nvsPath.c_str(), MEDIAHUB)) {
			Log_Println(mediaHubAdoptWriteFailed, LOGLEVEL_ERROR);
			System_IndicateError();
			return true; // handled: reporting it as "unknown card" on top would only confuse
		}
		Log_Printf(LOGLEVEL_NOTICE, mediaHubCardAdopted, cardId, base.c_str());

		// Hand over to the regular path, which fetches the manifest again and
		// does the caching, syncing and playback. That costs one extra (small)
		// manifest request compared to reusing the probe's response, and buys
		// not having a second copy of all that logic here.
		MediaHub_HandleCardTapped(cardId, nvsPath.c_str(), 0, 0);
		return true;
	}

	return false;
}
