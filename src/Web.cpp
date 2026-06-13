#include <Arduino.h>
#include "settings.h"

#include "Web.h"

#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "AudioPlayer.h"
#include "Battery.h"
#include "Bluetooth.h"
#include "Cmd.h"
#include "Common.h"
#include "ESPAsyncWebServer.h"
#include "EnumUtils.h"
#include "Ftp.h"
#include "HTMLbinary.h"
#include "HallEffectSensor.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "Mqtt.h"
#include "Rfid.h"
#include "RotaryEncoder.h"
#include "SdCard.h"
#include "System.h"
#include "Wlan.h"
#include "freertos/ringbuf.h"
#include "revision.h"
#include "soc/timer_group_reg.h"
#include "soc/timer_group_struct.h"

#include <Update.h>
#include <WiFi.h>
#include <atomic>
#include <esp_random.h>
#include <esp_task_wdt.h>
#include <mbedtls/sha256.h>
#include <nvs.h>

typedef struct {
	char nvsKey[cardIdStringSize];
	char nvsEntry[512];
} nvs_t;

AsyncWebServer wServer(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

static bool webserverStarted = false;

static const uint32_t start_chunk_size = 16384; // bigger chunks increase write-performance to SD-Card
static constexpr uint32_t nr_of_buffers = 3; // at least two buffers. No speed improvement yet with more than two.

static constexpr size_t retry_count = 3; // how often we retry is a malloc fails (also the times we halfe the chunk_size)

uint8_t *buffer[nr_of_buffers];
size_t chunk_size;
std::atomic<uint32_t> size_in_buffer[nr_of_buffers];
std::atomic<bool> buffer_full[nr_of_buffers];
uint32_t index_buffer_write = 0;
uint32_t index_buffer_read = 0;

static SemaphoreHandle_t explorerFileUploadFinished;
static TaskHandle_t fileStorageTaskHandle;
static std::atomic<bool> uploadAborted = false;

void Web_DumpSdToNvs(const char *_filename);
static void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
static void explorerHandleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
static void explorerHandleFileStorageTask(void *parameter);
static void explorerHandleListRequest(AsyncWebServerRequest *request);
static void explorerHandleDownloadRequest(AsyncWebServerRequest *request);
static void explorerHandleDeleteRequest(AsyncWebServerRequest *request);
static void handleCleanSdRequest(AsyncWebServerRequest *request);
static void explorerHandleCreateRequest(AsyncWebServerRequest *request);
static void explorerHandleRenameRequest(AsyncWebServerRequest *request);
static void explorerHandleAudioRequest(AsyncWebServerRequest *request);
static void handleTrackProgressRequest(AsyncWebServerRequest *request);
static void handleGetSavedSSIDs(AsyncWebServerRequest *request);
static void handlePostSavedSSIDs(AsyncWebServerRequest *request, JsonVariant &json);
static void handleDeleteSavedSSIDs(AsyncWebServerRequest *request);
static void handleGetActiveSSID(AsyncWebServerRequest *request);
static void handleGetWiFiConfig(AsyncWebServerRequest *request);
static void handlePostWiFiConfig(AsyncWebServerRequest *request, JsonVariant &json);
static void handleCoverImageRequest(AsyncWebServerRequest *request);
static void handleBluetoothScanRequest(AsyncWebServerRequest *request);
static void handleBluetoothResultsRequest(AsyncWebServerRequest *request);
static void handleBluetoothConnectRequest(AsyncWebServerRequest *request, JsonVariant &json);
static void handleWiFiScanRequest(AsyncWebServerRequest *request);
static void handleGetRFIDRequest(AsyncWebServerRequest *request);
static void handlePostRFIDRequest(AsyncWebServerRequest *request, JsonVariant &json);
static void handleDeleteRFIDRequest(AsyncWebServerRequest *request);
static void handleGetInfo(AsyncWebServerRequest *request);
static void handleGetSettings(AsyncWebServerRequest *request);
static void handlePostSettings(AsyncWebServerRequest *request, JsonVariant &json);
static void handleGetOperationMode(AsyncWebServerRequest *request);
static void handlePostOperationMode(AsyncWebServerRequest *request, JsonVariant &json);
static void handleDebugRequest(AsyncWebServerRequest *request);
static void handleGetEqRules(AsyncWebServerRequest *request);
static void handleSetEqRule(AsyncWebServerRequest *request);
static void handleDeleteEqRule(AsyncWebServerRequest *request);

static void onWebsocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
static void settingsToJSON(JsonObject obj, const String section);
static WebsocketCodeType JSONToSettings(JsonObject obj);
static void webserverStart(void);

// IPAddress converters, for a description see: https://arduinojson.org/news/2021/05/04/version-6-18-0/
void convertFromJson(JsonVariantConst src, IPAddress &dst) {
	dst = IPAddress();
	dst.fromString(src.as<const char *>());
}
bool canConvertFromJson(JsonVariantConst src, const IPAddress &) {
	if (!src.is<const char *>()) {
		return false; // this is not a string
	}
	IPAddress dst;
	return dst.fromString(src.as<const char *>());
}

// If PSRAM is available use it allocate memory for JSON-objects
struct SpiRamAllocator : ArduinoJson::Allocator {
	void *allocate(size_t size) override {
		return ps_malloc(size);
	}
	void deallocate(void *pointer) override {
		free(pointer);
	}
	void *reallocate(void *ptr, size_t new_size) override {
		return ps_realloc(ptr, new_size);
	}
};

static void destroyDoubleBuffer() {
	for (size_t i = 0; i < nr_of_buffers; i++) {
		free(buffer[i]);
		buffer[i] = nullptr;
	}
}

static bool allocateDoubleBuffer() {
	const auto checkAndAlloc = [](uint8_t *&ptr, const size_t memSize) -> bool {
		if (ptr) {
			// memory is there, so nothing to do
			return true;
		}
		// try to allocate buffer in faster internal RAM, not in PSRAM
		// ptr = (uint8_t *) malloc(memSize);
		ptr = (uint8_t *) heap_caps_aligned_alloc(32, memSize, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
		return (ptr != nullptr);
	};

	chunk_size = start_chunk_size;
	size_t retries = retry_count;
	while (retries) {
		if (chunk_size < 256) {
			// give up, since there is not even 256 bytes of memory left
			break;
		}
		bool success = true;
		for (size_t i = 0; i < nr_of_buffers; i++) {
			success &= checkAndAlloc(buffer[i], chunk_size);
		}
		if (success) {
			return true;
		} else {
			// one of our buffer went OOM --> free all buffer and retry with less chunk size
			destroyDoubleBuffer();
			chunk_size /= 2;
			retries--;
		}
	}
	destroyDoubleBuffer();
	return false;
}

void handleUploadError(AsyncWebServerRequest *request, int code) {
	if (request->_tempObject) {
		// we already have an error entered
		return;
	}
	// send the error to the client and record it in the request
	request->_tempObject = new int(code);
	request->send(code);
}

static void serveProgmemFiles(const String &uri, const String &contentType, const uint8_t *content, size_t len) {
	wServer.on(uri.c_str(), HTTP_GET, [contentType, content, len](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response;

		// const bool etag = request->hasHeader("if-None-Match") && request->getHeader("if-None-Match")->value().equals(gitRevShort);
		const bool etag = false;
		if (etag) {
			response = request->beginResponse(304);
		} else {
			response = request->beginResponse(200, contentType, content, len);
			response->addHeader("Content-Encoding", "gzip");
		}
		// response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
		// response->addHeader("ETag", gitRevShort);		// use git revision as digest
		request->send(response);
	});
}

class OneParamRewrite : public AsyncWebRewrite {
protected:
	String _urlPrefix;
	int _paramIndex;
	String _paramsBackup;

public:
	OneParamRewrite(const char *from, const char *to)
		: AsyncWebRewrite(from, to) {

		_paramIndex = _from.indexOf('{');

		if (_paramIndex >= 0 && _from.endsWith("}")) {
			_urlPrefix = _from.substring(0, _paramIndex);
			int index = _params.indexOf('{');
			if (index >= 0) {
				_params = _params.substring(0, index);
			}
		} else {
			_urlPrefix = _from;
		}
		_paramsBackup = _params;
	}

	bool match(AsyncWebServerRequest *request) override {
		if (request->url().startsWith(_urlPrefix)) {
			if (_paramIndex >= 0) {
				_params = _paramsBackup + request->url().substring(_paramIndex);
			} else {
				_params = _paramsBackup;
			}
			return true;

		} else {
			return false;
		}
	}
};

// List all key in NVS for a given namespace
// callback function is called for every key with userdefined data object
bool listNVSKeys(const char *_namespace, void *data, bool (*callback)(const char *key, void *data)) {
	constexpr const char *partname = "nvs";
#if (defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3))
	nvs_iterator_t it = nullptr;
	esp_err_t res = nvs_entry_find(partname, _namespace, NVS_TYPE_ANY, &it);
	while (res == ESP_OK) {
		nvs_entry_info_t info;
		nvs_entry_info(it, &info);
		if (isNumber(info.key)) {
			if (!callback(info.key, data)) {
				nvs_release_iterator(it);
				return false;
			}
		}
		// finished, NEXT
		res = nvs_entry_next(&it);
	}
	nvs_release_iterator(it);
#else
	nvs_iterator_t it = nvs_entry_find(partname, _namespace, NVS_TYPE_ANY);
	if (it == nullptr) {
		// no entries found
		return false;
	}
	while (it != nullptr) {
		nvs_entry_info_t info;
		nvs_entry_info(it, &info); // we got the key name here
		// some basic sanity checks
		if (isNumber(info.key)) {
			if (!callback(info.key, data)) {
				nvs_release_iterator(it);
				return false;
			}
		}
		// finished, NEXT!
		it = nvs_entry_next(it);
	}
#endif
	return true;
}

// callback for writing a NVS entry to file
bool DumpNvsToSdCallback(const char *key, void *data) {
	String s = gPrefsRfid.getString(key);
	File *file = (File *) data;
	file->printf("%s%s%s%s\n", stringOuterDelimiter, key, stringOuterDelimiter, s.c_str());
	return true;
}

// Dumps all RFID-entries from NVS into a file on SD-card
bool Web_DumpNvsToSd(const char *_namespace, const char *_destFile) {
	File file = gFSystem.open(_destFile, FILE_WRITE);
	if (!file) {
		return false;
	}
	// write UTF-8 BOM
	file.write(0xEF);
	file.write(0xBB);
	file.write(0xBF);
	// list all NVS keys
	bool success = listNVSKeys(_namespace, &file, DumpNvsToSdCallback);
	file.close();
	return success;
}

// First request will return 0 results unless you start scan from somewhere else (loop/setup)
// Do not request more often than 3-5 seconds
static void handleWiFiScanRequest(AsyncWebServerRequest *request) {
	String json = "[";
	int n = WiFi.scanComplete();
	if (n == -2) {
		// -2 if scan not triggered
		WiFi.scanNetworks(true, false, true, 120);
	} else if (n) {
		for (int i = 0; i < n; ++i) {
			if (i > 9) {
				break;
			}
			// calculate RSSI as quality in percent
			int quality;
			if (WiFi.RSSI(i) <= -100) {
				quality = 0;
			} else if (WiFi.RSSI(i) >= -50) {
				quality = 100;
			} else {
				quality = 2 * (WiFi.RSSI(i) + 100);
			}
			if (i) {
				json += ",";
			}
			json += "{";
			json += "\"ssid\":\"" + WiFi.SSID(i) + "\"";
			json += ",\"bssid\":\"" + WiFi.BSSIDstr(i) + "\"";
			json += ",\"rssi\":" + String(WiFi.RSSI(i));
			json += ",\"channel\":" + String(WiFi.channel(i));
			json += ",\"secure\":" + String(WiFi.encryptionType(i));
			json += ",\"quality\":" + String(quality); // WiFi strength in percent
			json += ",\"wico\":\"w" + String(int(round(map(quality, 0, 100, 1, 4)))) + "\""; // WiFi strength icon ("w1"-"w4")
			json += ",\"pico\":\"" + String((WIFI_AUTH_OPEN == WiFi.encryptionType(i)) ? "" : "pw") + "\""; // auth icon ("p1" for secured)
			json += "}";
		}
		WiFi.scanDelete();
		if (WiFi.scanComplete() == -2) {
			WiFi.scanNetworks(true, false, true, 120);
		}
	}
	json += "]";
	request->send(200, "application/json", json);
	json = String();
}

unsigned long lastCleanupClientsTimestamp;

void Web_Cyclic(void) {
	webserverStart();
	if ((millis() - lastCleanupClientsTimestamp) > 1000u) {
		// cleanup closed/deserted websocket clients once per second
		lastCleanupClientsTimestamp = millis();
		ws.cleanupClients();
	}
}

void Web_Exit(void) {
	esp_task_wdt_reset();
	if (webserverStarted) {
		// Gracefully abort active file storage task if running
		if (fileStorageTaskHandle != NULL) {
			uploadAborted = true;
			xTaskNotify(fileStorageTaskHandle, 2u, eSetValueWithOverwrite);
			// Wait for the task to exit and close open file handles
			uint32_t startWait = millis();
			while (fileStorageTaskHandle != NULL && (millis() - startWait < 2000)) {
				vTaskDelay(pdMS_TO_TICKS(50));
			}
		}
		Log_Println("shutdown webserver..", LOGLEVEL_NOTICE);
		wServer.end();
		webserverStarted = false;
	}
}
// ************************** Webinterface password protection ***************************
// A single shared password (no username). The session cookie carries a token derived from
// password + a stored salt, so all devices stay logged in for the cookie's lifetime (90
// days). Changing or clearing the password rotates the salt and invalidates all sessions.
static String wwwSessionToken = "";
static uint8_t wwwFailedLogins = 0;
static uint32_t wwwLoginLockedUntil = 0;

static const char wwwLoginPage[] PROGMEM = R"login(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESPuino</title><style>
:root{--c:#00f0ff;--m:#ff2a6d;--bg:#05070d;--p:#0a101e;--b:#1e2c4a;--t:#c5d4f2}
body{background:var(--bg);color:var(--t);font-family:"Lucida Console",Monaco,monospace;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0}
form{background:var(--p);border:1px solid var(--b);padding:40px 30px;max-width:280px;width:100%;text-align:center;clip-path:polygon(0 0,calc(100% - 18px) 0,100% 18px,100% 100%,0 100%)}
h1{font-size:16px;text-transform:uppercase;letter-spacing:.2em;color:var(--c);text-shadow:0 0 12px rgba(0,240,255,.6)}
h1::before{content:"// ";color:var(--m)}
input{width:100%;box-sizing:border-box;height:44px;background:#0e1626;border:1px solid var(--b);color:var(--t);padding:0 12px;font-family:inherit;margin:14px 0}
input:focus{outline:none;border-color:var(--c);box-shadow:0 0 12px rgba(0,240,255,.25)}
button{width:100%;height:44px;background:rgba(0,240,255,.08);border:1px solid rgba(0,240,255,.55);color:var(--c);cursor:pointer;font-family:inherit;text-transform:uppercase;letter-spacing:.08em;clip-path:polygon(6px 0,100% 0,100% calc(100% - 6px),calc(100% - 6px) 100%,0 100%,0 6px)}
button:hover{background:var(--c);color:#021019}
#e{color:var(--m);min-height:1.4em;margin-top:12px;font-size:13px}
</style></head><body>
<form id="f"><h1>Access</h1>
<input type="password" id="p" placeholder="Passwort / Password" autofocus>
<button type="submit">Login</button><div id="e"></div></form>
<script>
document.getElementById('f').onsubmit = async (ev) => {
	ev.preventDefault();
	const r = await fetch('/login', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ password: document.getElementById('p').value }) });
	if (r.ok) { location.href = '/'; } else { document.getElementById('e').textContent = (r.status == 429) ? 'Zu viele Versuche / too many attempts' : 'Falsches Passwort / wrong password'; }
};
</script></body></html>)login";

static String Web_BuildSessionToken(const String &password, const String &salt) {
	uint8_t hash[32];
	String input = password + ":" + salt;
	mbedtls_sha256((const unsigned char *) input.c_str(), input.length(), hash, 0);
	String hex;
	hex.reserve(64);
	for (uint8_t i = 0; i < sizeof(hash); i++) {
		char buf[3];
		snprintf(buf, sizeof(buf), "%02x", hash[i]);
		hex += buf;
	}
	return hex;
}

static void Web_RefreshSessionToken(void) {
	String password = gPrefsSettings.getString("wwwPassword", "");
	if (password.length() == 0) {
		wwwSessionToken = "";
		return;
	}
	String salt = gPrefsSettings.getString("wwwSalt", "");
	if (salt.length() == 0) {
		char saltBuf[17];
		snprintf(saltBuf, sizeof(saltBuf), "%08lx%08lx", (unsigned long) esp_random(), (unsigned long) esp_random());
		salt = saltBuf;
		gPrefsSettings.putString("wwwSalt", salt);
	}
	wwwSessionToken = Web_BuildSessionToken(password, salt);
}

static bool Web_IsAuthenticated(AsyncWebServerRequest *request) {
	if (wwwSessionToken.length() == 0) {
		return true;
	}
	if (!request->hasHeader("Cookie")) {
		return false;
	}
	const String &cookies = request->header("Cookie");
	int idx = cookies.indexOf("ESPUINO_SESSION=");
	if (idx < 0) {
		return false;
	}
	int start = idx + 16; // length of "ESPUINO_SESSION="
	int end = cookies.indexOf(';', start);
	String token = (end < 0) ? cookies.substring(start) : cookies.substring(start, end);
	token.trim();
	return token.equals(wwwSessionToken);
}

static void Web_HandlePostLogin(AsyncWebServerRequest *request, JsonVariant &json) {
	if (wwwLoginLockedUntil && (millis() < wwwLoginLockedUntil)) {
		request->send(429);
		return;
	}
	wwwLoginLockedUntil = 0;
	const char *password = json["password"];
	if ((wwwSessionToken.length() > 0) && password && gPrefsSettings.getString("wwwPassword", "").equals(password)) {
		wwwFailedLogins = 0;
		AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\"}");
		response->addHeader("Set-Cookie", "ESPUINO_SESSION=" + wwwSessionToken + "; Max-Age=7776000; Path=/; SameSite=Lax");
		request->send(response);
		Log_Println("webinterface login successful", LOGLEVEL_NOTICE);
	} else {
		if (++wwwFailedLogins >= 5) { // crude brute-force protection
			wwwFailedLogins = 0;
			wwwLoginLockedUntil = millis() + 60000;
		}
		Log_Println("webinterface login failed", LOGLEVEL_ERROR);
		request->send(401);
	}
}

static void Web_HandlePostSecurity(AsyncWebServerRequest *request, JsonVariant &json) {
	String password = json["password"] | "";
	if (password.length() == 0) {
		gPrefsSettings.remove("wwwPassword");
		gPrefsSettings.remove("wwwSalt");
		wwwSessionToken = "";
		Log_Println("webinterface password protection disabled", LOGLEVEL_NOTICE);
		request->send(200, "application/json", "{\"enabled\":false}");
		return;
	}
	gPrefsSettings.putString("wwwPassword", password);
	// rotate the salt: all existing sessions become invalid
	char saltBuf[17];
	snprintf(saltBuf, sizeof(saltBuf), "%08lx%08lx", (unsigned long) esp_random(), (unsigned long) esp_random());
	gPrefsSettings.putString("wwwSalt", saltBuf);
	Web_RefreshSessionToken();
	Log_Println("webinterface password protection enabled", LOGLEVEL_NOTICE);
	// keep the client that changed the password logged in
	AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"enabled\":true}");
	response->addHeader("Set-Cookie", "ESPUINO_SESSION=" + wwwSessionToken + "; Max-Age=7776000; Path=/; SameSite=Lax");
	request->send(response);
}

// handle not found
void notFound(AsyncWebServerRequest *request) {
	Log_Printf(LOGLEVEL_ERROR, "%s not found, redirect to startpage", request->url().c_str());
	String html = "<!DOCTYPE html>Ooups - page \"" + request->url() + "\" not found (404)";
	html += "<script>async function tryRedirect() {try {var url = \"/\";const response = await fetch(url);window.location.href = url;} catch (error) {console.log(error);setTimeout(tryRedirect, 2000);}}tryRedirect();</script>";
	// for captive portal, send statuscode 200 & auto redirect to startpage
	request->send(200, "text/html", html);
}

void webserverStart(void) {
	if (!webserverStarted && (Wlan_IsConnected() || (WiFi.getMode() == WIFI_AP))) {
		// password protection (no-op when no password is set or in accesspoint-mode)
		Web_RefreshSessionToken();
		wServer.addMiddleware([](AsyncWebServerRequest *request, ArMiddlewareNext next) {
			if ((wwwSessionToken.length() == 0) || (WiFi.getMode() == WIFI_AP)) {
				return next();
			}
			const String &url = request->url();
			if (url.equals("/login") || url.equals("/logo") || url.equals("/logo.svg") || url.equals("/favicon.ico") || url.equals("/manifest.json") || url.equals("/appicon.svg")) {
				// manifest/icon: the browser fetches the web-app-manifest without cookies
				return next();
			}
			if (Web_IsAuthenticated(request)) {
				return next();
			}
			if ((request->method() == HTTP_GET) && (url.equals("/") || url.endsWith(".html"))) {
				request->redirect("/login");
			} else {
				request->send(401);
			}
		});
		wServer.on("/login", HTTP_GET, [](AsyncWebServerRequest *request) {
			if ((wwwSessionToken.length() == 0) || Web_IsAuthenticated(request)) {
				request->redirect("/");
				return;
			}
			request->send(200, "text/html", wwwLoginPage);
		});
		wServer.addHandler(new AsyncCallbackJsonWebHandler("/login", Web_HandlePostLogin));
		wServer.on("/logout", HTTP_POST, [](AsyncWebServerRequest *request) {
			AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\"}");
			response->addHeader("Set-Cookie", "ESPUINO_SESSION=; Max-Age=0; Path=/; SameSite=Lax");
			request->send(response);
		});
		wServer.on("/security", HTTP_GET, [](AsyncWebServerRequest *request) {
			request->send(200, "application/json", (wwwSessionToken.length() > 0) ? "{\"enabled\":true}" : "{\"enabled\":false}");
		});
		wServer.addHandler(new AsyncCallbackJsonWebHandler("/security", Web_HandlePostSecurity));

		// attach AsyncWebSocket for Mgmt-Interface
		ws.onEvent(onWebsocketEvent);
		wServer.addHandler(&ws);

		// attach AsyncEventSource
		wServer.addHandler(&events);

		// Default
		wServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
			AsyncWebServerResponse *response;

			// const bool etag = request->hasHeader("if-None-Match") && request->getHeader("if-None-Match")->value().equals(gitRevShort);
			const bool etag = false;
			if (etag) {
				response = request->beginResponse(304);
			} else {
				if (WiFi.getMode() == WIFI_STA) {
					// serve management.html in station-mode
#ifdef NO_SDCARD
					response = request->beginResponse(200, "text/html", (const uint8_t *) management_BIN, sizeof(management_BIN));
					response->addHeader("Content-Encoding", "gzip");
#else
					if (gFSystem.exists("/.html/index.htm")) {
						response = request->beginResponse(gFSystem, "/.html/index.htm", "text/html", false);
					} else {
						response = request->beginResponse(200, "text/html", (const uint8_t *) management_BIN, sizeof(management_BIN));
						response->addHeader("Content-Encoding", "gzip");
					}
#endif
				} else {
					// serve accesspoint.html in AP-mode
					response = request->beginResponse(200, "text/html", (const uint8_t *) accesspoint_BIN, sizeof(accesspoint_BIN));
					response->addHeader("Content-Encoding", "gzip");
				}
			}
			// response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
			// response->addHeader("ETag", gitRevShort);		// use git revision as digest
			request->send(response);
		});

		WWWData::registerRoutes(serveProgmemFiles);

		// Log
		wServer.on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
			request->send(200, "text/plain; charset=utf-8", Log_GetRingBuffer());
			System_UpdateActivityTimer();
		});

		// info
		wServer.on("/info", HTTP_GET, handleGetInfo);

		// NVS-backup-upload
		wServer.on(
			"/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
				request->send(200);
			},
			handleUpload);

		// OTA-upload
		wServer.on(
			"/update", HTTP_POST, [](AsyncWebServerRequest *request) {
#ifdef BOARD_HAS_16MB_FLASH_AND_OTA_SUPPORT
				if (Update.hasError()) {
					request->send(500, "text/plain", Update.errorString());
				} else {
					request->send(200, "text/html", restartWebsite);
				}
#else
				request->send(500, "text/html", otaNotSupportedWebsite);
#endif
			},
			[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
#ifndef BOARD_HAS_16MB_FLASH_AND_OTA_SUPPORT
				Log_Println(otaNotSupported, LOGLEVEL_NOTICE);
				return;
#endif

				if (!index) {
					// pause some tasks to get more free CPU time for the upload
					System_PauseTasksDuringUpload(true);
					System_UpdateActivityTimer();
					Update.begin();
					Log_Println(fwStart, LOGLEVEL_NOTICE);
				}

				Update.write(data, len);
				Log_Print(".", LOGLEVEL_NOTICE, false);

				if (final) {
					Update.end(true);
					// resume the paused tasks
					System_PauseTasksDuringUpload(false);
					Log_Println(fwEnd, LOGLEVEL_NOTICE);
					if (Update.hasError()) {
						Log_Println(Update.errorString(), LOGLEVEL_ERROR);
					}
					Serial.flush();
					// ESP.restart(); // restart is done via webpage javascript
				}
			});

		// ESP-restart
		wServer.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
			request->send(200, "text/html", restartWebsite);
			System_Restart();
		});

		// ESP-shutdown
		wServer.on("/shutdown", HTTP_POST, [](AsyncWebServerRequest *request) {
			request->send(200, "text/html", shutdownWebsite);
			System_RequestSleep();
		});

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
		// runtime task statistics
		wServer.on("/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
			AsyncResponseStream *response = request->beginResponseStream("text/html");
			response->println("<!DOCTYPE html><html><head> <meta charset='utf-8'><title>ESPuino runtime stats</title>");
			response->println("<meta http-equiv='refresh' content='2'>"); // refresh page every 2 seconds
			response->print("</head><body>");
			// show memory usage
			response->println("Memory:<div class='text'><pre>");
			response->println("Free heap:           " + String(ESP.getFreeHeap()));
			response->println("Largest free block:  " + String(ESP.getMaxAllocHeap()));
			response->println("DMA free:            " + String(heap_caps_get_free_size(MALLOC_CAP_DMA)));
			response->println("DMA largest:         " + String(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)));
			response->println("Free PSRAM heap:     " + String(ESP.getFreePsram()));
			response->println("Largest PSRAM block: " + String(ESP.getMaxAllocPsram()));
			response->println("</pre></div><br>");

			uint32_t taskCount = uxTaskGetNumberOfTasks();
			size_t bufferSize = (taskCount + 10) * 80; // Provide safer margin for vTaskList
			char *pbuffer = (char *) x_calloc(bufferSize, 1);
			if (pbuffer) {
				// show tasklist
				response->println("Tasklist:<div class='text'><pre>");
				response->println("Taskname\tState\tPrio\tStack\tNum\tCore");
				vTaskList(pbuffer);
				response->println(pbuffer);
				response->println("</pre></div><br><br>Runtime statistics:<div class='text'><pre>");
				response->println("Taskname\tRuntime\tPercentage");
				vTaskGetRunTimeStats(pbuffer);
				response->println(pbuffer);
				free(pbuffer);
			}
			response->println("</pre></div></body></html>");
			// send the response last
			request->send(response);
		});
#endif
		// debug info
		wServer.on("/debug", HTTP_GET, handleDebugRequest);

		// erase all RFID-assignments from NVS
		wServer.on("/rfidnvserase", HTTP_POST, [](AsyncWebServerRequest *request) {
			Log_Println(eraseRfidNvs, LOGLEVEL_NOTICE);
			// make a backup first
			Web_DumpNvsToSd("rfidTags", backupFile);
			if (gPrefsRfid.clear()) {
				request->send(200);
			} else {
				request->send(500);
			}
			System_UpdateActivityTimer();
		});

		// RFID
		wServer.on("/rfid", HTTP_GET, handleGetRFIDRequest);
		wServer.addRewrite(new OneParamRewrite("/rfid/ids-only", "/rfid?ids-only=true"));
		wServer.addHandler(new AsyncCallbackJsonWebHandler("/rfid", handlePostRFIDRequest));
		wServer.addRewrite(new OneParamRewrite("/rfid/{id}", "/rfid?id={id}"));
		wServer.on("/rfid", HTTP_DELETE, handleDeleteRFIDRequest);

		// WiFi scan
		wServer.on("/wifiscan", HTTP_GET, handleWiFiScanRequest);

		// Fileexplorer (realtime)
		wServer.on("/explorer", HTTP_GET, explorerHandleListRequest);

		wServer.on(
			"/explorer", HTTP_POST, [](AsyncWebServerRequest *request) {
				// clean up buffers
				request->onDisconnect([]() { destroyDoubleBuffer(); });
				// we are finished with the upload
				if (!request->_tempObject) {
					request->send(200);
				}
			},
			explorerHandleFileUpload);

		wServer.on("/explorerdownload", HTTP_GET, explorerHandleDownloadRequest);

		wServer.on("/explorer", HTTP_DELETE, explorerHandleDeleteRequest);

		wServer.on("/explorer", HTTP_PUT, explorerHandleCreateRequest);

		wServer.on("/explorer", HTTP_PATCH, explorerHandleRenameRequest);

		wServer.on("/sdclean", HTTP_POST, handleCleanSdRequest);

		wServer.on("/exploreraudio", HTTP_POST, explorerHandleAudioRequest);

		wServer.on("/trackprogress", HTTP_GET, handleTrackProgressRequest);

		wServer.on("/savedSSIDs", HTTP_GET, handleGetSavedSSIDs);
		wServer.addHandler(new AsyncCallbackJsonWebHandler("/savedSSIDs", handlePostSavedSSIDs));

		wServer.addRewrite(new OneParamRewrite("/savedSSIDs/{ssid}", "/savedSSIDs?ssid={ssid}"));
		wServer.on("/savedSSIDs", HTTP_DELETE, handleDeleteSavedSSIDs);
		wServer.on("/activeSSID", HTTP_GET, handleGetActiveSSID);

		wServer.on("/wificonfig", HTTP_GET, handleGetWiFiConfig);
		wServer.addHandler(new AsyncCallbackJsonWebHandler("/wificonfig", handlePostWiFiConfig));

		// current cover image
		wServer.on("/cover", HTTP_GET, handleCoverImageRequest);

		// Bluetooth-Scan and connect
		wServer.on("/bluetoothscan", HTTP_GET, handleBluetoothScanRequest);
		wServer.on("/bluetoothresults", HTTP_GET, handleBluetoothResultsRequest);
		wServer.addHandler(new AsyncCallbackJsonWebHandler("/bluetoothconnect", handleBluetoothConnectRequest));

		// ESPuino logo
		wServer.on("/logo", HTTP_GET, [](AsyncWebServerRequest *request) {
#ifndef NO_SDCARD
			Log_Println("logo request", LOGLEVEL_DEBUG);
			if (gFSystem.exists("/.html/logo.png")) {
				request->send(gFSystem, "/.html/logo.png", "image/png");
				return;
			};
			if (gFSystem.exists("/.html/logo.svg")) {
				request->send(gFSystem, "/.html/logo.svg", "image/svg+xml");
				return;
			};
#endif
			// embedded Leo Industries AT-1 logo
			request->redirect("/logo.svg");
		});
		// ESPuino favicon
		wServer.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
#ifndef NO_SDCARD
			if (gFSystem.exists("/.html/favicon.ico")) {
				request->send(gFSystem, "/.html/favicon.png", "image/x-icon");
				return;
			};
#endif
			request->redirect("https://espuino.de/espuino/favicon.ico");
		});
		// ESPuino settings
		wServer.on("/settings", HTTP_GET, handleGetSettings);
		wServer.addHandler(new AsyncCallbackJsonWebHandler("/settings", handlePostSettings));
		// per-path equalizer rules (directory/file -> EQ profile)
		wServer.on("/eqrules", HTTP_GET, handleGetEqRules);
		wServer.on("/eqrule", HTTP_POST, handleSetEqRule);
		wServer.on("/eqrule", HTTP_DELETE, handleDeleteEqRule);
		// currently active equalizer gains (may be overridden by a per-path rule for the running track)
		wServer.on("/activeequalizer", HTTP_GET, [](AsyncWebServerRequest *request) {
			char buf[96];
			snprintf(buf, sizeof(buf), "{\"gainLowPass\":%d,\"gainBandPass\":%d,\"gainHighPass\":%d}",
				gPlayProperties.gainLowPass, gPlayProperties.gainBandPass, gPlayProperties.gainHighPass);
			request->send(200, "application/json", buf);
		});
		// operation mode
		wServer.on("/mode", HTTP_GET, handleGetOperationMode);
		wServer.addHandler(new AsyncCallbackJsonWebHandler("/mode", handlePostOperationMode));
		// Init HallEffectSensor Value
#ifdef HALLEFFECT_SENSOR_ENABLE
		wServer.on("/inithalleffectsensor", HTTP_GET, [](AsyncWebServerRequest *request) {
			bool bres = gHallEffectSensor.saveActualFieldValue2NVS();
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "WebRequest>HallEffectSensor FieldValue: %d => NVS, Status: %s", gHallEffectSensor.NullFieldValue(), bres ? "OK" : "ERROR");
			Log_Println(buffer, LOGLEVEL_INFO);
			request->send(200, "text/html", buffer);
		});
#endif

		wServer.onNotFound(notFound);

		// allow cors for local debug (https://github.com/me-no-dev/ESPAsyncWebServer/issues/1080)
		DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Accept, Content-Type, Authorization");
		DefaultHeaders::Instance().addHeader("Access-Control-Allow-Credentials", "true");
		DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
		wServer.begin();
		webserverStarted = true;
		Log_Println(httpReady, LOGLEVEL_NOTICE);
		// start a first WiFi scan (to get a WiFi list more quickly in webview)
		WiFi.scanNetworks(true, false, true, 120);
	}
}

unsigned long lastPongTimestamp;

// process JSON to settings
WebsocketCodeType JSONToSettings(JsonObject doc) {
	if (!doc) {
		Log_Println("JSONToSettings: doc unassigned", LOGLEVEL_DEBUG);
		return WebsocketCodeType::Error;
	}
	if (doc["general"].is<JsonObject>()) {
		// general settings
		JsonObject generalObj = doc["general"];
		bool success = (gPrefsSettings.putUInt("initVolume", generalObj["initVolume"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUInt("maxVolumeSp", generalObj["maxVolumeSp"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUInt("maxVolumeHp", generalObj["maxVolumeHp"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUInt("mInactiviyT", generalObj["sleepInactivity"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putBool("playMono", generalObj["playMono"].as<bool>()) != 0);
		success = success && (gPrefsSettings.putBool("savePosShutdown", generalObj["savePosShutdown"].as<bool>()) != 0);
		success = success && (gPrefsSettings.putBool("savePosRfidChge", generalObj["savePosRfidChge"].as<bool>()) != 0);
		success = success && (gPrefsSettings.putBool("playLastOnBoot", generalObj["playLastRfidOnReboot"].as<bool>()) != 0);
		success = success && (gPrefsSettings.putBool("pauseRfidRem", generalObj["pauseIfRfidRemoved"].as<bool>()) != 0);
		success = success && (gPrefsSettings.putBool("dAccRfidTwice", generalObj["dontAcceptRfidTwice"].as<bool>()) != 0);
		success = success && (gPrefsSettings.putBool("pauseOnMinVol", generalObj["pauseOnMinVol"].as<bool>()) != 0);
		success = success && (gPrefsSettings.putBool("recoverVolBoot", generalObj["recoverVolBoot"].as<bool>()) != 0);
		success = success && (gPrefsSettings.putUChar("volumeCurve", generalObj["volumeCurve"].as<uint8_t>()) != 0);
		if (generalObj["readyPath"].is<const char *>()) {
			const char *path = generalObj["readyPath"].as<const char *>();
			size_t written = gPrefsSettings.putString("readyPath", path);
			if (strlen(path) > 0 && written == 0) {
				success = false;
			}
		}
		success = success && (gPrefsRfid.putUChar("rfidReaderType", generalObj["rfidReaderType"].as<uint8_t>()) != 0);
		success = success && (gPrefsRfid.putBool("pn5180Lpcd", generalObj["pn5180Lpcd"].as<bool>()) != 0);
		if (generalObj["slix2Password"].is<const char *>()) {
			const char *pwd = generalObj["slix2Password"].as<const char *>();
			size_t written = gPrefsRfid.putString("slix2Pwd", pwd);
			if (strlen(pwd) > 0 && written == 0) {
				success = false;
			}
		}
		success = success && (gPrefsRfid.putUChar("mfrc522Gain", generalObj["mfrc522Gain"].as<uint8_t>()) != 0);
		if (!success) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "general");
			return WebsocketCodeType::Error;
		}
		gPlayProperties.newPlayMono = generalObj["playMono"].as<bool>();
		gPlayProperties.SavePlayPosRfidChange = generalObj["savePosRfidChge"].as<bool>();
		gPlayProperties.pauseOnMinVolume = generalObj["pauseOnMinVol"].as<bool>();
		gPlayProperties.pauseIfRfidRemoved = generalObj["pauseIfRfidRemoved"].as<bool>();
		if (gPlayProperties.pauseIfRfidRemoved) {
			// ignore feature silently if PAUSE_WHEN_RFID_REMOVED is active
			Log_Println("pauseIfRfidRemoved is enabled -> deactivate dontAcceptRfidTwice", LOGLEVEL_NOTICE);
			gPlayProperties.dontAcceptRfidTwice = false;
		} else {
			gPlayProperties.dontAcceptRfidTwice = generalObj["dontAcceptRfidTwice"].as<bool>();
		}
	}
	if (doc["equalizer"].is<JsonObject>()) {
		int8_t _gainLowPass = doc["equalizer"]["gainLowPass"].as<int8_t>();
		int8_t _gainBandPass = doc["equalizer"]["gainBandPass"].as<int8_t>();
		int8_t _gainHighPass = doc["equalizer"]["gainHighPass"].as<int8_t>();
		// Active profile name (purely informational, drives the UI selection on reload)
		const char *_profile = doc["equalizer"]["profile"].as<const char *>();
		String _eqProfile = (_profile != nullptr) ? String(_profile) : String("custom");
		// equalizer settings
		if (
			gPrefsSettings.putChar("gainLowPass", _gainLowPass) == 0 || gPrefsSettings.putChar("gainBandPass", _gainBandPass) == 0 || gPrefsSettings.putChar("gainHighPass", _gainHighPass) == 0 || gPrefsSettings.putString("eqProfile", _eqProfile) == 0) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "equalizer");
			return WebsocketCodeType::Error;
		} else {
			AudioPlayer_SetEqualizer(_gainLowPass, _gainBandPass, _gainHighPass);
#ifdef MQTT_ENABLE
			// Keep the MQTT state in sync when the equalizer profile is changed via the web interface.
			publishMqtt(topicEqualizer, _eqProfile.c_str(), false);
#endif
		}
	}
	if (doc["wifi"].is<JsonObject>()) {
		// WiFi settings
		String hostName = doc["wifi"]["hostname"];
		if (!Wlan_ValidateHostname(hostName)) {
			Log_Println("Invalid hostname", LOGLEVEL_ERROR);
			return WebsocketCodeType::Error;
		}
		if (((!Wlan_SetHostname(hostName)) || gPrefsSettings.putBool("ScanWiFiOnStart", doc["wifi"]["scanOnStart"].as<bool>()) == 0)) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "wifi");
			return WebsocketCodeType::Error;
		}
	}
	if (doc["led"].is<JsonObject>()) {
		// Neopixel settings
		JsonObject ledObj = doc["led"];
		bool success = (gPrefsSettings.putUChar("iLedBrightness", ledObj["initBrightness"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("nLedBrightness", ledObj["nightBrightness"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("aLedBrightness", ledObj["atmoBrightness"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("numIndicator", ledObj["numIndicator"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("numControl", ledObj["numControl"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("numIdleDots", ledObj["numIdleDots"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUInt("idleColor", ledObj["idleColor"].as<uint32_t>()) != 0);
		success = success && (gPrefsSettings.putBool("offsetPause", ledObj["offsetPause"].as<bool>()) != 0);
		success = success && (gPrefsSettings.putUInt("progColorStart", ledObj["progColorStart"].as<uint32_t>()) != 0);
		success = success && (gPrefsSettings.putUInt("progColorEnd", ledObj["progColorEnd"].as<uint32_t>()) != 0);
		success = success && (gPrefsSettings.putShort("hueAtmo", ledObj["hueAtmo"].as<int16_t>()) != 0);
		success = success && (gPrefsSettings.putShort("satAtmo", ledObj["satAtmo"].as<int16_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("dimStates", ledObj["dimStates"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putBool("ledReverseRot", ledObj["reverseRot"].as<bool>()) != 0);
		success = success && (gPrefsSettings.putUChar("ledOffset", ledObj["offsetStart"].as<uint8_t>()) != 0);

		if (!success) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "led");
			return WebsocketCodeType::Error;
		}
		// write led control color array to NVS.
		JsonArray colorArr = ledObj["controlColors"].as<JsonArray>();
		if (colorArr.size() == 0) {
			if (gPrefsSettings.isKey("controlColors")) {
				gPrefsSettings.remove("controlColors");
			}
			gPrefsSettings.putUChar("numControl", 0);
		} else {
			std::vector<uint32_t> controlLedColors;
			for (uint8_t controlLed = 0; controlLed < colorArr.size(); controlLed++) {
				controlLedColors.push_back(colorArr[controlLed].as<uint32_t>());
			}
			gPrefsSettings.putBytes("controlColors", controlLedColors.data(), controlLedColors.size() * sizeof(uint32_t));
		}
		Led_Init();
	}
	if (doc["buttons"].is<JsonObject>()) {
		// buttons
		JsonObject buttonsObj = doc["buttons"];
		bool success = (gPrefsSettings.putUChar("btnShort0", buttonsObj["short0"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnShort1", buttonsObj["short1"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnShort2", buttonsObj["short2"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnShort3", buttonsObj["short3"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnShort4", buttonsObj["short4"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnShort5", buttonsObj["short5"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnShort6", buttonsObj["short6"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnLong0", buttonsObj["long0"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnLong1", buttonsObj["long1"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnLong2", buttonsObj["long2"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnLong3", buttonsObj["long3"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnLong4", buttonsObj["long4"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnLong5", buttonsObj["long5"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnLong6", buttonsObj["long6"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti01", buttonsObj["multi01"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti02", buttonsObj["multi02"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti03", buttonsObj["multi03"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti04", buttonsObj["multi04"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti05", buttonsObj["multi05"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti06", buttonsObj["multi06"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti12", buttonsObj["multi12"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti13", buttonsObj["multi13"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti14", buttonsObj["multi14"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti15", buttonsObj["multi15"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti16", buttonsObj["multi16"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti23", buttonsObj["multi23"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti24", buttonsObj["multi24"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti25", buttonsObj["multi25"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti26", buttonsObj["multi26"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti34", buttonsObj["multi34"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti35", buttonsObj["multi35"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti36", buttonsObj["multi36"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti45", buttonsObj["multi45"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti46", buttonsObj["multi46"].as<uint8_t>()) != 0);
		success = success && (gPrefsSettings.putUChar("btnMulti56", buttonsObj["multi56"].as<uint8_t>()) != 0);

		if (!success) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "buttons");
			return WebsocketCodeType::Error;
		}
	}
	if (doc["rotary"].is<JsonObject>()) {
		// Rotary encoder
		if (gPrefsSettings.putBool("rotaryReverse", doc["rotary"]["reverse"].as<bool>()) == 0) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "rotary");
			return WebsocketCodeType::Error;
		}
		RotaryEncoder_Init();
	}
	if (doc["battery"].is<JsonObject>()) {
		// Battery settings
		if (gPrefsSettings.putFloat("wLowVoltage", doc["battery"]["warnLowVoltage"].as<float>()) == 0 || gPrefsSettings.putFloat("vIndicatorLow", doc["battery"]["indicatorLow"].as<float>()) == 0 || gPrefsSettings.putFloat("vIndicatorHigh", doc["battery"]["indicatorHi"].as<float>()) == 0 || gPrefsSettings.putFloat("wCritVoltage", doc["battery"]["criticalVoltage"].as<float>()) == 0 || gPrefsSettings.putUInt("vCheckIntv", doc["battery"]["voltageCheckInterval"].as<uint8_t>()) == 0) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "battery");
			return WebsocketCodeType::Error;
		}
		Battery_Init();
	}
	if (doc["playlist"].is<JsonObject>()) {
		// playlist settings
		if (!AudioPlayer_SetPlaylistSortMode(doc["playlist"]["sortMode"].as<uint8_t>())) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "playlist");
			return WebsocketCodeType::Error;
		}
		if (!SdCard_SetMaxRecursionDepth(doc["playlist"]["recDepth"].as<uint8_t>())) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "playlist");
			return WebsocketCodeType::Error;
		}
	}
	if (doc["ftp"].is<JsonObject>()) {
		const char *_ftpUser = doc["ftp"]["username"];
		const char *_ftpPwd = doc["ftp"]["password"];

		gPrefsSettings.putString("ftpuser", (String) _ftpUser);
		gPrefsSettings.putString("ftppassword", (String) _ftpPwd);
		// Check if settings were written successfully
		if (!(String(_ftpUser).equals(gPrefsSettings.getString("ftpuser", "-1")) || String(_ftpPwd).equals(gPrefsSettings.getString("ftppassword", "-1")))) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "ftp");
			return WebsocketCodeType::Error;
		}
	} else if (doc["ftpStatus"].is<JsonObject>()) {
		uint8_t _ftpStart = doc["ftpStatus"]["start"].as<uint8_t>();
		if (_ftpStart == 1) { // ifdef FTP_ENABLE is checked in Ftp_EnableServer()
			Ftp_EnableServer();
		} else {
			Ftp_DisableServer();
		}
		Web_SendWebsocketData(0, WebsocketCodeType::FtpStatus); // broadcast new status to all clients
	}
	if (doc["mqtt"].is<JsonObject>()) {
		uint8_t _mqttEnable = doc["mqtt"]["enable"].as<uint8_t>();
		const char *_mqttClientId = doc["mqtt"]["clientID"];
		const char *_mqttDeviceId = doc["mqtt"]["deviceId"];
		const char *_mqttBaseTopic = doc["mqtt"]["baseTopic"];
		const char *_mqttServer = doc["mqtt"]["server"];
		const char *_mqttUser = doc["mqtt"]["username"];
		const char *_mqttPwd = doc["mqtt"]["password"];
		uint16_t _mqttPort = doc["mqtt"]["port"].as<uint16_t>();

		// Sanitize and validate inputs
		String mqttClientIdStr = String(_mqttClientId);
		String mqttDeviceIdStr = String(_mqttDeviceId);
		String mqttBaseTopicStr = String(_mqttBaseTopic);

		// sanitize base topic (trim slashes and whitespace)
		mqttBaseTopicStr.trim();
		while (mqttBaseTopicStr.startsWith("/")) {
			mqttBaseTopicStr = mqttBaseTopicStr.substring(1);
		}
		while (mqttBaseTopicStr.endsWith("/")) {
			mqttBaseTopicStr = mqttBaseTopicStr.substring(0, mqttBaseTopicStr.length() - 1);
		}
		if (mqttBaseTopicStr.length() >= mqttBaseTopicLength) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "mqtt.baseTopic too long");
			return WebsocketCodeType::Error;
		}

		// validate device id (must not be empty and must not contain '/')
		mqttDeviceIdStr.trim();
		if (mqttDeviceIdStr.length() == 0) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "mqtt.deviceId empty");
			return WebsocketCodeType::Error;
		}
		if (mqttDeviceIdStr.indexOf('/') >= 0) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "mqtt.deviceId contains invalid char '/'");
			return WebsocketCodeType::Error;
		}
		if (mqttDeviceIdStr.length() >= mqttDeviceIdLength) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "mqtt.deviceId too long");
			return WebsocketCodeType::Error;
		}

		// store sanitized values
		gPrefsSettings.putUChar("enableMQTT", _mqttEnable);
		gPrefsSettings.putString("mqttClientId", mqttClientIdStr);
		gPrefsSettings.putString("mqttDeviceId", mqttDeviceIdStr);
		gPrefsSettings.putString("mqttBaseTopic", mqttBaseTopicStr);
		gPrefsSettings.putString("mqttServer", (String) _mqttServer);
		gPrefsSettings.putString("mqttUser", (String) _mqttUser);
		gPrefsSettings.putString("mqttPassword", (String) _mqttPwd);
		gPrefsSettings.putUInt("mqttPort", _mqttPort);

		// verify writes (include deviceId and baseTopic)
		if ((gPrefsSettings.getUChar("enableMQTT", 99) != _mqttEnable) || (!String(_mqttServer).equals(gPrefsSettings.getString("mqttServer", "-1"))) || (!mqttBaseTopicStr.equals(gPrefsSettings.getString("mqttBaseTopic", "-1"))) || (!mqttDeviceIdStr.equals(gPrefsSettings.getString("mqttDeviceId", "-1")))) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "mqtt");
			return WebsocketCodeType::Error;
		}
	}
	if (doc["bluetooth"].is<JsonObject>()) {
		// bluetooth settings
		const char *_btDeviceName = doc["bluetooth"]["deviceName"];
		gPrefsSettings.putString("btDeviceName", (String) _btDeviceName);
		const char *btPinCode = doc["bluetooth"]["pinCode"];
		gPrefsSettings.putString("btPinCode", (String) btPinCode);
		// Check if settings were written successfully
		if (gPrefsSettings.getString("btDeviceName", "") != _btDeviceName || gPrefsSettings.getString("btPinCode", "") != btPinCode) {
			Log_Printf(LOGLEVEL_ERROR, webSaveSettingsError, "bluetooth");
			return WebsocketCodeType::Error;
		}
	} else if (doc["rfidMod"].is<JsonObject>()) {
		const char *_rfidIdModId = doc["rfidMod"]["rfidIdMod"];
		uint8_t _modId = doc["rfidMod"]["modId"];
		if (_modId <= 0) {
			gPrefsRfid.remove(_rfidIdModId);
		} else {
			char rfidString[12];
			snprintf(rfidString, sizeof(rfidString) / sizeof(rfidString[0]), "%s0%s0%s%u%s0", stringDelimiter, stringDelimiter, stringDelimiter, _modId, stringDelimiter);
			gPrefsRfid.putString(_rfidIdModId, rfidString);

			String s = gPrefsRfid.getString(_rfidIdModId, "-1");
			if (s.compareTo(rfidString)) {
				return WebsocketCodeType::Error;
			}
		}
		Web_DumpNvsToSd("rfidTags", backupFile); // Store backup-file every time when a new rfid-tag is programmed
	} else if (doc["rfidAssign"].is<JsonObject>()) {
		const char *_rfidIdAssinId = doc["rfidAssign"]["rfidIdMusic"];
		const char *_fileOrUrlAscii = doc["rfidAssign"]["fileOrUrl"];
		uint8_t _playMode = doc["rfidAssign"]["playMode"];
		if (_playMode <= 0) {
			Log_Println("rfidAssign: Invalid playmode", LOGLEVEL_ERROR);
			return WebsocketCodeType::Error;
		}
		char rfidString[275];
		snprintf(rfidString, sizeof(rfidString) / sizeof(rfidString[0]), "%s%s%s0%s%u%s0", stringDelimiter, _fileOrUrlAscii, stringDelimiter, stringDelimiter, _playMode, stringDelimiter);
		gPrefsRfid.putString(_rfidIdAssinId, rfidString);
		Rfid_ResetOldRfid(); // Reset reader task so it can immediately recognize the tag if it's already present

		String s = gPrefsRfid.getString(_rfidIdAssinId, "-1");
		if (s.compareTo(rfidString)) {
			return WebsocketCodeType::Error;
		}
		Web_DumpNvsToSd("rfidTags", backupFile); // Store backup-file every time when a new rfid-tag is programmed
		Web_SendWebsocketData(0, WebsocketCodeType::Ok);
	} else if (doc["ping"].is<JsonObject>()) {
		if ((millis() - lastPongTimestamp) > 1000u) {
			// send pong (keep-alive heartbeat), check for excessive calls
			lastPongTimestamp = millis();
			Web_SendWebsocketData(0, WebsocketCodeType::Pong);
		}
		return WebsocketCodeType::Error;
	} else if (doc["controls"].is<JsonObject>()) {
		// Prevent website control over volume and player actions in Bluetooth-Speaker mode
		// we could still allow sone special commands, but it's cleaner this way
		if (!System_IsWebControlAllowed()) {
			Log_Println(notAllowedInCurrentMode, LOGLEVEL_NOTICE);
			return WebsocketCodeType::NotAllowedInCurrentMode;
		}
		const JsonObject controlsObj = doc["controls"].as<JsonObject>();
		if (controlsObj["set_volume"].is<uint8_t>()) {
			uint8_t new_vol = controlsObj["set_volume"].as<uint8_t>();
			AudioPlayer_SetVolume(new_vol);
		}
		if (controlsObj["action"].is<uint8_t>()) {
			uint8_t cmd = controlsObj["action"].as<uint8_t>();
			Cmd_Action(cmd);
		}
	} else if (doc["trackinfo"].is<JsonObject>()) {
		Web_SendWebsocketData(0, WebsocketCodeType::TrackInfo);
	} else if (doc["coverimg"].is<JsonObject>()) {
		Web_SendWebsocketData(0, WebsocketCodeType::CoverImg);
	} else if (doc["volume"].is<JsonObject>()) {
		Web_SendWebsocketData(0, WebsocketCodeType::Volume);
	} else if (doc["settings"].is<JsonObject>()) {
		Web_SendWebsocketData(0, WebsocketCodeType::Settings);
	} else if (doc["ssids"].is<JsonObject>()) {
		Web_SendWebsocketData(0, WebsocketCodeType::Ssid);
	} else if (doc["trackProgress"].is<JsonObject>()) {
		// Prevent seeking in Bluetooth mode
		if (!System_IsWebControlAllowed()) {
			Log_Println(notAllowedInCurrentMode, LOGLEVEL_NOTICE);
			return WebsocketCodeType::NotAllowedInCurrentMode;
		}
		const JsonObject trackObj = doc["trackProgress"].as<JsonObject>();
		if (trackObj["posPercent"].is<uint8_t>()) {
			gPlayProperties.seekmode = SEEK_POS_PERCENT;
			gPlayProperties.currentRelPos = trackObj["posPercent"].as<uint8_t>();
		}
		Web_SendWebsocketData(0, WebsocketCodeType::TrackProgress);
	}

	return WebsocketCodeType::Ok;
}

// process settings to JSON object
static void settingsToJSON(JsonObject obj, const String section) {
	if ((section == "") || (section == "current")) {
		// current values
		JsonObject curObj = obj["current"].to<JsonObject>();
		curObj["volume"].set(AudioPlayer_GetCurrentVolume());
		curObj["rfidTagId"] = String(gCurrentRfidTagId);
	}
	if ((section == "") || (section == "general")) {
		// general settings
		JsonObject generalObj = obj["general"].to<JsonObject>();
		generalObj["initVolume"].set(gPrefsSettings.getUInt("initVolume", 3));
		generalObj["maxVolumeSp"].set(gPrefsSettings.getUInt("maxVolumeSp", 21));
		generalObj["maxVolumeHp"].set(gPrefsSettings.getUInt("maxVolumeHp", 21));
		generalObj["sleepInactivity"].set(gPrefsSettings.getUInt("mInactiviyT", 10));
		generalObj["playMono"].set(gPrefsSettings.getBool("playMono", false));
		generalObj["savePosShutdown"].set(gPrefsSettings.getBool("savePosShutdown", false)); // SAVE_PLAYPOS_BEFORE_SHUTDOWN
		generalObj["savePosRfidChge"].set(gPrefsSettings.getBool("savePosRfidChge", false)); // SAVE_PLAYPOS_WHEN_RFID_CHANGE
		generalObj["playLastRfidOnReboot"].set(gPrefsSettings.getBool("playLastOnBoot", false)); // PLAY_LAST_RFID_AFTER_REBOOT
		generalObj["pauseIfRfidRemoved"].set(gPrefsSettings.getBool("pauseRfidRem", false)); // PAUSE_WHEN_RFID_REMOVED
		generalObj["dontAcceptRfidTwice"].set(gPrefsSettings.getBool("dAccRfidTwice", false)); // DONT_ACCEPT_SAME_RFID_TWICE
		generalObj["rfidReaderType"].set(gPrefsRfid.getUChar("rfidReaderType", 0)); // RFID_READER_TYPE_RUNTIME
		generalObj["pn5180Lpcd"].set(gPrefsRfid.getBool("pn5180Lpcd", false)); // PN5180 LPCD
		generalObj["slix2Password"].set(gPrefsRfid.getString("slix2Pwd", "")); // SLIX2 Password
		generalObj["mfrc522Gain"].set(gPrefsRfid.getUChar("mfrc522Gain", 7)); // MFRC522_GAIN
		generalObj["pauseOnMinVol"].set(gPrefsSettings.getBool("pauseOnMinVol", false)); // PAUSE_ON_MIN_VOLUME
		generalObj["recoverVolBoot"].set(gPrefsSettings.getBool("recoverVolBoot", false)); // USE_LAST_VOLUME_AFTER_REBOOT
		generalObj["volumeCurve"].set(gPrefsSettings.getUChar("volumeCurve", 0)); // VOLUMECURVE
		generalObj["readyPath"].set(gPrefsSettings.getString("readyPath", "/ready.mp3")); // READY_PATH
	}
	if ((section == "") || (section == "equalizer")) {
		// equalizer settings
		JsonObject equalizerObj = obj["equalizer"].to<JsonObject>();
		equalizerObj["gainLowPass"].set(gPrefsSettings.getChar("gainLowPass", 0));
		equalizerObj["gainBandPass"].set(gPrefsSettings.getChar("gainBandPass", 0));
		equalizerObj["gainHighPass"].set(gPrefsSettings.getChar("gainHighPass", 0));
		equalizerObj["profile"].set(gPrefsSettings.getString("eqProfile", "custom"));
	}
	if ((section == "") || (section == "wifi")) {
		// WiFi settings
		JsonObject wifiObj = obj["wifi"].to<JsonObject>();
		wifiObj["hostname"] = Wlan_GetHostname();
		wifiObj["scanOnStart"].set(gPrefsSettings.getBool("ScanWiFiOnStart", false));
	}
	if (section == "ssids") {
		// saved SSID's
		JsonObject ssidsObj = obj["ssids"].to<JsonObject>();
		JsonArray ssidArr = ssidsObj["savedSSIDs"].to<JsonArray>();
		Wlan_GetSavedNetworks([ssidArr](const WiFiSettings &network) {
			ssidArr.add(network.ssid);
		});

		// active SSID
		if (Wlan_IsConnected()) {
			ssidsObj["active"] = Wlan_GetCurrentSSID();
		}
	}
#ifdef NEOPIXEL_ENABLE
	if ((section == "") || (section == "led")) {
		// LED settings
		JsonObject ledObj = obj["led"].to<JsonObject>();
		ledObj["initBrightness"].set(gPrefsSettings.getUChar("iLedBrightness", 0));
		ledObj["nightBrightness"].set(gPrefsSettings.getUChar("nLedBrightness", 0));
		ledObj["atmoBrightness"].set(gPrefsSettings.getUChar("aLedBrightness", 0));
		ledObj["numIndicator"].set(gPrefsSettings.getUChar("numIndicator", NUM_INDICATOR_LEDS));
		uint8_t numControlLeds = gPrefsSettings.getUChar("numControl", NUM_CONTROL_LEDS);
		ledObj["numControl"].set(numControlLeds);
		if (numControlLeds > 0) {
			// get control led colors from NVS
			std::vector<uint32_t> controlLedColors = CONTROL_LEDS_COLORS;
			size_t keySize = gPrefsSettings.getBytesLength("controlColors");
			if (keySize == (numControlLeds * sizeof(uint32_t))) {
				controlLedColors.resize(numControlLeds);
				gPrefsSettings.getBytes("controlColors", controlLedColors.data(), keySize);
			}
			if (controlLedColors.size() > 0) {
				JsonArray colorArr = ledObj["controlColors"].to<JsonArray>();
				for (uint8_t controlLed = 0; controlLed < controlLedColors.size(); controlLed++) {
					colorArr.add(controlLedColors[controlLed]);
				}
			}
		}
		ledObj["numIdleDots"].set(gPrefsSettings.getUChar("numIdleDots", NUM_LEDS_IDLE_DOTS));
		ledObj["idleColor"].set(gPrefsSettings.getUInt("idleColor", IDLE_COLOR));
		ledObj["offsetPause"].set(gPrefsSettings.getBool("offsetPause", OFFSET_PAUSE_LEDS));
		ledObj["progColorStart"].set(gPrefsSettings.getUInt("progColorStart", PROGRESS_COLOR_START));
		ledObj["progColorEnd"].set(gPrefsSettings.getUInt("progColorEnd", PROGRESS_COLOR_END));
		ledObj["hueAtmo"].set(gPrefsSettings.getShort("hueAtmo", ATMO_HUE));
		ledObj["satAtmo"].set(gPrefsSettings.getShort("satAtmo", ATMO_SATURATION));
		ledObj["dimStates"].set(gPrefsSettings.getUChar("dimStates", DIMMABLE_STATES));
		ledObj["reverseRot"].set(gPrefsSettings.getBool("ledReverseRot", false));
		ledObj["offsetStart"].set(gPrefsSettings.getUChar("ledOffset", 0));
	}
#endif
	if ((section == "") || (section == "buttons")) {
		// button settings
		JsonObject buttonsObj = obj["buttons"].to<JsonObject>();
		buttonsObj["short0"].set(gPrefsSettings.getUChar("btnShort0", BUTTON_0_SHORT));
		buttonsObj["short1"].set(gPrefsSettings.getUChar("btnShort1", BUTTON_1_SHORT));
		buttonsObj["short2"].set(gPrefsSettings.getUChar("btnShort2", BUTTON_2_SHORT));
		buttonsObj["short3"].set(gPrefsSettings.getUChar("btnShort3", BUTTON_3_SHORT));
		buttonsObj["short4"].set(gPrefsSettings.getUChar("btnShort4", BUTTON_4_SHORT));
		buttonsObj["short5"].set(gPrefsSettings.getUChar("btnShort5", BUTTON_5_SHORT));
		buttonsObj["short6"].set(gPrefsSettings.getUChar("btnShort6", BUTTON_6_SHORT));
		buttonsObj["long0"].set(gPrefsSettings.getUChar("btnLong0", BUTTON_0_LONG));
		buttonsObj["long1"].set(gPrefsSettings.getUChar("btnLong1", BUTTON_1_LONG));
		buttonsObj["long2"].set(gPrefsSettings.getUChar("btnLong2", BUTTON_2_LONG));
		buttonsObj["long3"].set(gPrefsSettings.getUChar("btnLong3", BUTTON_3_LONG));
		buttonsObj["long4"].set(gPrefsSettings.getUChar("btnLong4", BUTTON_4_LONG));
		buttonsObj["long5"].set(gPrefsSettings.getUChar("btnLong5", BUTTON_5_LONG));
		buttonsObj["long6"].set(gPrefsSettings.getUChar("btnLong6", BUTTON_6_LONG));
		buttonsObj["multi01"].set(gPrefsSettings.getUChar("btnMulti01", BUTTON_MULTI_01));
		buttonsObj["multi02"].set(gPrefsSettings.getUChar("btnMulti02", BUTTON_MULTI_02));
		buttonsObj["multi03"].set(gPrefsSettings.getUChar("btnMulti03", BUTTON_MULTI_03));
		buttonsObj["multi04"].set(gPrefsSettings.getUChar("btnMulti04", BUTTON_MULTI_04));
		buttonsObj["multi05"].set(gPrefsSettings.getUChar("btnMulti05", BUTTON_MULTI_05));
		buttonsObj["multi06"].set(gPrefsSettings.getUChar("btnMulti06", BUTTON_MULTI_06));
		buttonsObj["multi12"].set(gPrefsSettings.getUChar("btnMulti12", BUTTON_MULTI_12));
		buttonsObj["multi13"].set(gPrefsSettings.getUChar("btnMulti13", BUTTON_MULTI_13));
		buttonsObj["multi14"].set(gPrefsSettings.getUChar("btnMulti14", BUTTON_MULTI_14));
		buttonsObj["multi15"].set(gPrefsSettings.getUChar("btnMulti15", BUTTON_MULTI_15));
		buttonsObj["multi16"].set(gPrefsSettings.getUChar("btnMulti16", BUTTON_MULTI_16));
		buttonsObj["multi23"].set(gPrefsSettings.getUChar("btnMulti23", BUTTON_MULTI_23));
		buttonsObj["multi24"].set(gPrefsSettings.getUChar("btnMulti24", BUTTON_MULTI_24));
		buttonsObj["multi25"].set(gPrefsSettings.getUChar("btnMulti25", BUTTON_MULTI_25));
		buttonsObj["multi26"].set(gPrefsSettings.getUChar("btnMulti26", BUTTON_MULTI_26));
		buttonsObj["multi34"].set(gPrefsSettings.getUChar("btnMulti34", BUTTON_MULTI_34));
		buttonsObj["multi35"].set(gPrefsSettings.getUChar("btnMulti35", BUTTON_MULTI_35));
		buttonsObj["multi36"].set(gPrefsSettings.getUChar("btnMulti36", BUTTON_MULTI_36));
		buttonsObj["multi45"].set(gPrefsSettings.getUChar("btnMulti45", BUTTON_MULTI_45));
		buttonsObj["multi46"].set(gPrefsSettings.getUChar("btnMulti46", BUTTON_MULTI_46));
		buttonsObj["multi56"].set(gPrefsSettings.getUChar("btnMulti56", BUTTON_MULTI_56));
	}
	if ((section == "") || (section == "rotary")) {
		// Rotary encoder
		JsonObject rotaryObj = obj["rotary"].to<JsonObject>();
		rotaryObj["reverse"].set(gPrefsSettings.getBool("rotaryReverse", false));
	}
	// playlist
	if ((section == "") || (section == "playlist")) {
		JsonObject playlistObj = obj["playlist"].to<JsonObject>();
		playlistObj["sortMode"] = EnumUtils::underlying_value(AudioPlayer_GetPlaylistSortMode());
		playlistObj["recDepth"] = SdCard_GetMaxRecursionDepth();
	}
#ifdef BATTERY_MEASURE_ENABLE
	if ((section == "") || (section == "battery")) {
		// battery settings
		JsonObject batteryObj = obj["battery"].to<JsonObject>();
	#ifdef MEASURE_BATTERY_VOLTAGE
		batteryObj["warnLowVoltage"].set(gPrefsSettings.getFloat("wLowVoltage", s_warningLowVoltage));
		batteryObj["indicatorLow"].set(gPrefsSettings.getFloat("vIndicatorLow", s_voltageIndicatorLow));
		batteryObj["indicatorHi"].set(gPrefsSettings.getFloat("vIndicatorHigh", s_voltageIndicatorHigh));
		#ifdef SHUTDOWN_ON_BAT_CRITICAL
		batteryObj["criticalVoltage"].set(gPrefsSettings.getFloat("wCritVoltage", s_warningCriticalVoltage));
		#endif
	#endif

		batteryObj["voltageCheckInterval"].set(gPrefsSettings.getUInt("vCheckIntv", s_batteryCheckInterval));
	}
#endif
	if (section == "defaults") {
		// default factory settings NOTE: maintain the settings section structure as above to make it easier for clients to use
		JsonObject defaultsObj = obj["defaults"].to<JsonObject>();
		JsonObject genSettings = defaultsObj["general"].to<JsonObject>();
		genSettings["initVolume"].set(AUDIOPLAYER_VOLUME_INIT);
		genSettings["maxVolumeSp"].set(AUDIOPLAYER_VOLUME_MAX);
		genSettings["maxVolumeHp"].set(18u); // gPrefsSettings.getUInt("maxVolumeHp", 0));
		genSettings["sleepInactivity"].set(10u); // System_MaxInactivityTime
		genSettings["playMono"].set(false); // PLAY_MONO_SPEAKER
		genSettings["savePosShutdown"].set(false); // SAVE_PLAYPOS_BEFORE_SHUTDOWN
		genSettings["savePosRfidChge"].set(false); // SAVE_PLAYPOS_WHEN_RFID_CHANGE
		genSettings["playLastRfidOnReboot"].set(false); // PLAY_LAST_RFID_AFTER_REBOOT
		genSettings["pauseIfRfidRemoved"].set(false); // PAUSE_WHEN_RFID_REMOVED
		genSettings["dontAcceptRfidTwice"].set(false); // DONT_ACCEPT_SAME_RFID_TWICE
		genSettings["pauseOnMinVol"].set(false); // PAUSE_ON_MIN_VOLUME
		genSettings["recoverVolBoot"].set(false); // USE_LAST_VOLUME_AFTER_REBOOT
		genSettings["volumeCurve"].set(0u); // VOLUME_CURVE
		genSettings["rfidReaderType"].set(0u); // RFID_READER_TYPE_RUNTIME (auto-detect)
		genSettings["pn5180Lpcd"].set(false); // PN5180 LPCD disabled
		genSettings["mfrc522Gain"].set(7u); // MFRC522_GAIN default (max gain)
		JsonObject eqSettings = defaultsObj["equalizer"].to<JsonObject>();
		eqSettings["gainHighPass"].set(0);
		eqSettings["gainBandPass"].set(0);
		eqSettings["gainLowPass"].set(0);
		eqSettings["profile"].set("flat");
#ifdef NEOPIXEL_ENABLE
		JsonObject ledSettings = defaultsObj["led"].to<JsonObject>();
		ledSettings["initBrightness"].set(16u); // LED_INITIAL_BRIGHTNESS
		ledSettings["nightBrightness"].set(2u); // LED_INITIAL_NIGHT_BRIGHTNESS
		ledSettings["atmoBrightness"].set(30u); // LED_INITIAL_NIGHT_BRIGHTNESS
		ledSettings["numIndicator"].set(NUM_INDICATOR_LEDS); // NUM_INDICATOR_LEDS
		ledSettings["numControl"].set(NUM_CONTROL_LEDS); // NUM_CONTROL_LEDS
		ledSettings["numIdleDots"].set(NUM_LEDS_IDLE_DOTS); // NUM_LEDS_IDLE_DOTS
		ledSettings["idleColor"].set(IDLE_COLOR); // IDLE_COLOR
		ledSettings["offsetPause"].set(OFFSET_PAUSE_LEDS); // OFFSET_PAUSE_LEDS
		ledSettings["progColorStart"].set(PROGRESS_COLOR_START); // PROGRESS_COLOR_START
		ledSettings["progColorEnd"].set(PROGRESS_COLOR_END); // PROGRESS_COLOR_END
		ledSettings["hueAtmo"].set(ATMO_HUE);
		ledSettings["satAtmo"].set(ATMO_SATURATION);
		ledSettings["dimStates"].set(DIMMABLE_STATES); // DIMMABLE_STATES
	#ifdef NEOPIXEL_REVERSE_ROTATION
		ledSettings["reverseRot"].set(true);
	#else
		ledSettings["reverseRot"].set(false);
	#endif
	#ifdef LED_OFFSET
		ledSettings["offsetStart"].set(LED_OFFSET);
	#else
		ledSettings["offsetStart"].set(0);
	#endif
		JsonArray colorArr = ledSettings["controlColors"].to<JsonArray>();
		std::vector<uint32_t> controlLedColors = CONTROL_LEDS_COLORS;
		for (uint8_t controlLed = 0; controlLed < controlLedColors.size(); controlLed++) {
			colorArr.add(controlLedColors[controlLed]);
		}
#endif
		JsonObject buttonsSettings = defaultsObj["buttons"].to<JsonObject>();
		buttonsSettings["short0"].set(BUTTON_0_SHORT);
		buttonsSettings["short1"].set(BUTTON_1_SHORT);
		buttonsSettings["short2"].set(BUTTON_2_SHORT);
		buttonsSettings["short3"].set(BUTTON_3_SHORT);
		buttonsSettings["short4"].set(BUTTON_4_SHORT);
		buttonsSettings["short5"].set(BUTTON_5_SHORT);
		buttonsSettings["short6"].set(BUTTON_6_SHORT);
		buttonsSettings["long0"].set(BUTTON_0_LONG);
		buttonsSettings["long1"].set(BUTTON_1_LONG);
		buttonsSettings["long2"].set(BUTTON_2_LONG);
		buttonsSettings["long3"].set(BUTTON_3_LONG);
		buttonsSettings["long4"].set(BUTTON_4_LONG);
		buttonsSettings["long5"].set(BUTTON_5_LONG);
		buttonsSettings["long6"].set(BUTTON_6_LONG);
		buttonsSettings["multi01"].set(BUTTON_MULTI_01);
		buttonsSettings["multi02"].set(BUTTON_MULTI_02);
		buttonsSettings["multi03"].set(BUTTON_MULTI_03);
		buttonsSettings["multi04"].set(BUTTON_MULTI_04);
		buttonsSettings["multi05"].set(BUTTON_MULTI_05);
		buttonsSettings["multi06"].set(BUTTON_MULTI_06);
		buttonsSettings["multi12"].set(BUTTON_MULTI_12);
		buttonsSettings["multi13"].set(BUTTON_MULTI_13);
		buttonsSettings["multi14"].set(BUTTON_MULTI_14);
		buttonsSettings["multi15"].set(BUTTON_MULTI_15);
		buttonsSettings["multi16"].set(BUTTON_MULTI_16);
		buttonsSettings["multi23"].set(BUTTON_MULTI_23);
		buttonsSettings["multi24"].set(BUTTON_MULTI_24);
		buttonsSettings["multi25"].set(BUTTON_MULTI_25);
		buttonsSettings["multi26"].set(BUTTON_MULTI_26);
		buttonsSettings["multi34"].set(BUTTON_MULTI_34);
		buttonsSettings["multi35"].set(BUTTON_MULTI_35);
		buttonsSettings["multi36"].set(BUTTON_MULTI_36);
		buttonsSettings["multi45"].set(BUTTON_MULTI_45);
		buttonsSettings["multi46"].set(BUTTON_MULTI_46);
		buttonsSettings["multi56"].set(BUTTON_MULTI_56);
#ifdef USEROTARY_ENABLE
		JsonObject rotarySettings = defaultsObj["rotary"].to<JsonObject>();
		rotarySettings["reverse"].set(false); // REVERSE_ROTARY
#endif
		JsonObject playlistSettings = defaultsObj["playlist"].to<JsonObject>();
		playlistSettings["sortMode"].set(EnumUtils::underlying_value(AUDIOPLAYER_PLAYLIST_SORT_MODE_DEFAULT));
		playlistSettings["recDepth"].set(2u);
#ifdef BATTERY_MEASURE_ENABLE
		JsonObject batSettings = defaultsObj["battery"].to<JsonObject>();
	#ifdef MEASURE_BATTERY_VOLTAGE
		batSettings["warnLowVoltage"].set(s_warningLowVoltage);
		batSettings["indicatorLow"].set(s_voltageIndicatorLow);
		batSettings["indicatorHi"].set(s_voltageIndicatorHigh);
		#ifdef SHUTDOWN_ON_BAT_CRITICAL
		batSettings["criticalVoltage"].set(s_warningCriticalVoltage);
		#endif
	#endif
		batSettings["voltageCheckInterval"].set(s_batteryCheckInterval);
#endif
	}
// FTP
#ifdef FTP_ENABLE
	if ((section == "") || (section == "ftp")) {
		JsonObject ftpObj = obj["ftp"].to<JsonObject>();
		ftpObj["username"] = gPrefsSettings.getString("ftpuser", "-1");
		ftpObj["password"] = gPrefsSettings.getString("ftppassword", "-1");
		ftpObj["maxUserLength"].set(ftpUserLength - 1);
		ftpObj["maxPwdLength"].set(ftpUserLength - 1);
	}
#endif
// MQTT
#ifdef MQTT_ENABLE
	if ((section == "") || (section == "mqtt")) {
		JsonObject mqttObj = obj["mqtt"].to<JsonObject>();
		mqttObj["enable"].set(Mqtt_IsEnabled());
		String macPlain = Wlan_GetMacAddress(); // returns AA:BB:CC:DD:EE:FF or empty
		macPlain.replace(":", "");
		macPlain.toUpperCase();
		mqttObj["macAddressPlain"] = macPlain;
		mqttObj["clientID"] = gPrefsSettings.getString("mqttClientId", "-1");
		mqttObj["deviceId"] = gPrefsSettings.getString("mqttDeviceId", "-1");
		mqttObj["baseTopic"] = gPrefsSettings.getString("mqttBaseTopic", "-1");
		mqttObj["server"] = gPrefsSettings.getString("mqttServer", "-1");
		mqttObj["port"].set(gPrefsSettings.getUInt("mqttPort", 0));
		mqttObj["username"] = gPrefsSettings.getString("mqttUser", "-1");
		mqttObj["password"] = gPrefsSettings.getString("mqttPassword", "-1");
		mqttObj["maxUserLength"].set(mqttUserLength - 1);
		mqttObj["maxPwdLength"].set(mqttPasswordLength - 1);
		mqttObj["maxClientIdLength"].set(mqttClientIdLength - 1);
		mqttObj["maxServerLength"].set(mqttServerLength - 1);
		mqttObj["maxBaseTopicLength"].set(mqttBaseTopicLength - 1);
		mqttObj["maxDeviceIdLength"].set(mqttDeviceIdLength - 1);
	}
#endif
// Bluetooth
#ifdef BLUETOOTH_ENABLE
	if ((section == "") || (section == "bluetooth")) {
		JsonObject btObj = obj["bluetooth"].to<JsonObject>();
		if (gPrefsSettings.isKey("btDeviceName")) {
			btObj["deviceName"] = gPrefsSettings.getString("btDeviceName", "");
		} else {
			btObj["deviceName"] = "";
		}
		if (gPrefsSettings.isKey("btPinCode")) {
			btObj["pinCode"] = gPrefsSettings.getString("btPinCode", "");
		} else {
			btObj["pinCode"] = "";
		}
	}
#endif
}

// handle get info
void handleGetInfo(AsyncWebServerRequest *request) {

	// param to get a single info section
	String section = "";
	if (request->hasParam("section")) {
		section = request->getParam("section")->value();
	}
	AsyncJsonResponse *response = new AsyncJsonResponse(false);
	JsonObject infoObj = response->getRoot();
	// software
	if ((section == "") || (section == "software")) {
		JsonObject softwareObj = infoObj["software"].to<JsonObject>();
		softwareObj["version"] = (String) softwareRevision;
		softwareObj["git"] = (String) gitRevision;
		softwareObj["arduino"] = String(ESP_ARDUINO_VERSION_MAJOR) + "." + String(ESP_ARDUINO_VERSION_MINOR) + "." + String(ESP_ARDUINO_VERSION_PATCH);
		softwareObj["idf"] = String(ESP.getSdkVersion());
	}
	// hardware
	if ((section == "") || (section == "hardware")) {
		JsonObject hardwareObj = infoObj["hardware"].to<JsonObject>();
		hardwareObj["model"] = String(ESP.getChipModel());
		hardwareObj["revision"] = ESP.getChipRevision();
		hardwareObj["freq"] = ESP.getCpuFreqMHz();
	}
	// memory
	if ((section == "") || (section == "memory")) {
		JsonObject memoryObj = infoObj["memory"].to<JsonObject>();
		memoryObj["freeHeap"] = ESP.getFreeHeap();
		memoryObj["largestFreeBlock"] = (uint32_t) heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
		memoryObj["freePSRam"] = ESP.getFreePsram();
		memoryObj["largestFreePSRamBlock"] = String(ESP.getMaxAllocPsram());
	}
	// wifi
	if ((section == "") || (section == "wifi")) {
		JsonObject wifiObj = infoObj["wifi"].to<JsonObject>();
		wifiObj["ip"] = Wlan_GetIpAddress();
		wifiObj["macAddress"] = Wlan_GetMacAddress();
		wifiObj["rssi"] = (int8_t) Wlan_GetRssi();
	}
	// audio
	if ((section == "") || (section == "audio")) {
		JsonObject audioObj = infoObj["audio"].to<JsonObject>();
		audioObj["playtimeTotal"] = AudioPlayer_GetPlayTimeAllTime();
		audioObj["playtimeSinceStart"] = AudioPlayer_GetPlayTimeSinceStart();
		audioObj["firstStart"] = gPrefsSettings.getULong("firstStart", 0);
	}
	// sd card
	if ((section == "") || (section == "sdcard")) {
		JsonObject sdObj = infoObj["sdcard"].to<JsonObject>();
		sdObj["size"] = SdCard_GetSize() / (1024 * 1024); // MiB
		sdObj["free"] = SdCard_GetFreeSize() / (1024 * 1024); // MiB
	}
#ifdef BATTERY_MEASURE_ENABLE
	// battery
	if ((section == "") || (section == "battery")) {
		JsonObject batteryObj = infoObj["battery"].to<JsonObject>();
		batteryObj["currVoltage"] = Battery_GetVoltage();
		batteryObj["chargeLevel"] = Battery_EstimateLevel() * 100;
	}
#endif
#ifdef HALLEFFECT_SENSOR_ENABLE
	if ((section == "") || (section == "hallsensor")) {
		// hallsensor
		JsonObject hallObj = infoObj["hallsensor"].to<JsonObject>();
		uint16_t sva = gHallEffectSensor.readSensorValueAverage(true);
		int diff = sva - gHallEffectSensor.NullFieldValue();

		hallObj["nullFieldValue"] = gHallEffectSensor.NullFieldValue();
		hallObj["actual"] = sva;
		hallObj["diff"] = diff;
		hallObj["lastWaitState"] = gHallEffectSensor.LastWaitForState();
		hallObj["lastWaitMS"] = gHallEffectSensor.LastWaitForStateMS();
	}
#endif

	if (response->overflowed()) {
		// JSON buffer too small for data
		Log_Println(jsonbufferOverflow, LOGLEVEL_ERROR);
		request->send(500);
		return;
	}
	response->setLength();
	request->send(response);
	System_UpdateActivityTimer();
}

// handle get settings
void handleGetSettings(AsyncWebServerRequest *request) {

	// param to get a single settings section
	String section = "";
	if (request->hasParam("section")) {
		section = request->getParam("section")->value();
	}

	AsyncJsonResponse *response = new AsyncJsonResponse(false);
	JsonObject settingsObj = response->getRoot();
	settingsToJSON(settingsObj, section);
	if (response->overflowed()) {
		// JSON buffer too small for data
		Log_Println(jsonbufferOverflow, LOGLEVEL_ERROR);
		request->send(500);
		return;
	}
	response->setLength();
	request->send(response);
}

// Return all per-path equalizer rules as the stored JSON array.
void handleGetEqRules(AsyncWebServerRequest *request) {
	request->send(200, "application/json", gPrefsSettings.getString("eqRules", "[]"));
}

// Add or update a per-path equalizer rule (path + gains + profile name). Stored as a
// JSON array in NVS under "eqRules" and re-loaded into the audio player.
void handleSetEqRule(AsyncWebServerRequest *request) {
	if (!request->hasParam("path")) {
		request->send(400, "application/json", "{}");
		return;
	}
	const String path = request->getParam("path")->value();
	const int8_t low = request->hasParam("low") ? (int8_t) request->getParam("low")->value().toInt() : 0;
	const int8_t band = request->hasParam("band") ? (int8_t) request->getParam("band")->value().toInt() : 0;
	const int8_t high = request->hasParam("high") ? (int8_t) request->getParam("high")->value().toInt() : 0;
	const String profile = request->hasParam("profile") ? request->getParam("profile")->value() : String("");

	JsonDocument doc;
	deserializeJson(doc, gPrefsSettings.getString("eqRules", "[]"));
	if (!doc.is<JsonArray>()) {
		doc.to<JsonArray>();
	}
	JsonArray arr = doc.as<JsonArray>();
	for (size_t i = 0; i < arr.size();) {
		if (arr[i]["p"].as<String>() == path) {
			arr.remove(i);
		} else {
			i++;
		}
	}
	JsonObject o = arr.add<JsonObject>();
	o["p"] = path;
	o["l"] = low;
	o["b"] = band;
	o["h"] = high;
	o["pr"] = profile;

	String out;
	serializeJson(doc, out);
	gPrefsSettings.putString("eqRules", out);
	AudioPlayer_ReloadEqRules();
	request->send(200, "application/json", "{}");
}

// Delete the per-path equalizer rule for the given path.
void handleDeleteEqRule(AsyncWebServerRequest *request) {
	if (!request->hasParam("path")) {
		request->send(400, "application/json", "{}");
		return;
	}
	const String path = request->getParam("path")->value();

	JsonDocument doc;
	deserializeJson(doc, gPrefsSettings.getString("eqRules", "[]"));
	JsonArray arr = doc.as<JsonArray>();
	for (size_t i = 0; i < arr.size();) {
		if (arr[i]["p"].as<String>() == path) {
			arr.remove(i);
		} else {
			i++;
		}
	}
	String out;
	serializeJson(doc, out);
	gPrefsSettings.putString("eqRules", out);
	AudioPlayer_ReloadEqRules();
	request->send(200, "application/json", "{}");
}

// handle post settings
void handlePostSettings(AsyncWebServerRequest *request, JsonVariant &json) {
	const JsonObject &jsonObj = json.as<JsonObject>();
	WebsocketCodeType res = JSONToSettings(jsonObj);
	if (res != WebsocketCodeType::Error) {
		if (res != WebsocketCodeType::Ok) {
			Web_SendWebsocketData(0, res);
		}
		request->send(200);
	} else {
		request->send(500, "text/plain; charset=utf-8", "error saving settings");
	}
}

// handle get operation mode
void handleGetOperationMode(AsyncWebServerRequest *request) {
	AsyncJsonResponse *response = new AsyncJsonResponse(false);
	JsonObject object = response->getRoot();
	object["mode"] = System_GetOperationMode();
	response->setLength();
	request->send(response);
}

// handle post operation mode
void handlePostOperationMode(AsyncWebServerRequest *request, JsonVariant &json) {
	const JsonObject &jsonObj = json.as<JsonObject>();
	if (jsonObj["mode"].is<uint8_t>()) {
		uint8_t mode = jsonObj["mode"].as<uint8_t>();
		System_SetOperationMode(mode);
		request->send(200);
	} else {
		request->send(400, "text/plain; charset=utf-8", "missing 'mode' parameter");
	}
}

// handle debug request
// returns memory and task runtime information as JSON
void handleDebugRequest(AsyncWebServerRequest *request) {

	AsyncJsonResponse *response = new AsyncJsonResponse(false);
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
	JsonObject infoObj = response->getRoot();
	// task runtime info
	uint32_t pulTotalRunTime;
	uint32_t taskCount = uxTaskGetNumberOfTasks();
	std::vector<TaskStatus_t> task_status_arr(taskCount + 5);

	Log_Printf(LOGLEVEL_DEBUG, "number of tasks: %u", taskCount);

	uint32_t taskNum = uxTaskGetSystemState(task_status_arr.data(), task_status_arr.size(), &pulTotalRunTime);

	JsonObject tasksObj = infoObj["tasks"].to<JsonObject>();
	tasksObj["taskCount"] = taskNum;
	tasksObj["totalRunTime"] = pulTotalRunTime;
	JsonArray tasksList = tasksObj["tasksList"].to<JsonArray>();

	for (int i = 0; i < taskNum; i++) {
		JsonObject taskObj = tasksList.add<JsonObject>();

		float ulStatsAsPercentage = 100.f * ((float) task_status_arr[i].ulRunTimeCounter / (float) pulTotalRunTime);

		taskObj["name"] = task_status_arr[i].pcTaskName;
		taskObj["runtimeCounter"] = task_status_arr[i].ulRunTimeCounter;
		taskObj["core"] = task_status_arr[i].xCoreID;
		taskObj["runtimePercentage"] = ulStatsAsPercentage;
		taskObj["stackHighWaterMark"] = task_status_arr[i].usStackHighWaterMark;
	}
#endif
	if (response->overflowed()) {
		// JSON buffer too small for data
		Log_Println(jsonbufferOverflow, LOGLEVEL_ERROR);
		request->send(500);
		return;
	}
	response->setLength();
	request->send(response);
}

// Takes inputs from webgui, parses JSON and saves values in NVS
// If operation was successful (NVS-write is verified) true is returned
WebsocketCodeType processJsonRequest(char *_serialJson) {
	if (!_serialJson) {
		return WebsocketCodeType::Error;
	}
	SpiRamAllocator allocator;
	JsonDocument doc(&allocator);

	DeserializationError error = deserializeJson(doc, _serialJson);

	if (error) {
		Log_Printf(LOGLEVEL_ERROR, jsonErrorMsg, error.c_str());
		return WebsocketCodeType::Error;
	}

	JsonObject obj = doc.as<JsonObject>();
	return JSONToSettings(obj);
}

// Sends JSON-answers via websocket
void Web_SendWebsocketData(uint32_t client, WebsocketCodeType code) {
	if (!webserverStarted) {
		// webserver not yet started
		return;
	}
	if (ws.count() == 0) {
		// we do not have any webclient connected
		return;
	}

	SpiRamAllocator allocator;
	JsonDocument doc(&allocator);

	JsonObject object = doc.to<JsonObject>();

	if (code == WebsocketCodeType::Ok) {
		object["status"] = "ok";
	} else if (code == WebsocketCodeType::Error) {
		object["status"] = "error";
	} else if (code == WebsocketCodeType::Dropout) {
		object["status"] = "dropout";
	} else if (code == WebsocketCodeType::NotAllowedInCurrentMode) {
		object["status"] = "not_allowed_in_current_mode";
	} else if (code == WebsocketCodeType::CurrentRfid) {
		object["rfidId"] = gCurrentRfidTagId;
	} else if (code == WebsocketCodeType::Pong) {
		object["pong"] = "pong";
		object["rssi"] = Wlan_GetRssi();
#ifdef BATTERY_MEASURE_ENABLE
		JsonObject batteryObj = object["battery"].to<JsonObject>();
		batteryObj["voltage"] = Battery_GetVoltage();
		batteryObj["level"] = Battery_EstimateLevel() * 100;
		batteryObj["low"] = Battery_IsLow();
#endif
	} else if (code == WebsocketCodeType::OperationMode) {
		object["opmode"] = System_GetOperationMode();
	} else if (code == WebsocketCodeType::TrackInfo) {
		JsonObject entry = object["trackinfo"].to<JsonObject>();
		entry["pausePlay"] = gPlayProperties.pausePlay;
		entry["currentTrackNumber"] = gPlayProperties.currentTrackNumber + 1;
		entry["numberOfTracks"] = (gPlayProperties.playlist) ? gPlayProperties.playlist->size() : 0;
		entry["volume"] = AudioPlayer_GetCurrentVolume();
		entry["name"] = gPlayProperties.title;
		entry["posPercent"] = gPlayProperties.currentRelPos;
		entry["playMode"] = gPlayProperties.playMode;
	} else if (code == WebsocketCodeType::CoverImg) {
		object["coverimg"] = "coverimg";
	} else if (code == WebsocketCodeType::Volume) {
		object["volume"] = AudioPlayer_GetCurrentVolume();
	} else if (code == WebsocketCodeType::Settings) {
		JsonObject entry = object["settings"].to<JsonObject>();
		settingsToJSON(entry, "");
	} else if (code == WebsocketCodeType::Ssid) {
		JsonObject entry = object["settings"].to<JsonObject>();
		settingsToJSON(entry, "ssids");
	} else if (code == WebsocketCodeType::TrackProgress) {
		JsonObject entry = object["trackProgress"].to<JsonObject>();
		entry["posPercent"] = gPlayProperties.currentRelPos;
		entry["time"] = AudioPlayer_GetCurrentTime();
		entry["duration"] = AudioPlayer_GetFileDuration();
	} else if (code == WebsocketCodeType::BluetoothScanInProgress) {
		object["bt_scan"] = "in_progress";
	} else if (code == WebsocketCodeType::BluetoothScanComplete) {
		object["bt_scan"] = "complete";
	} else if (code == WebsocketCodeType::FtpStatus) {
		JsonObject entry = object["ftpStatus"].to<JsonObject>();
		entry["running"] = Ftp_IsServerRunning();
	};

	if (doc.overflowed()) {
		// JSON buffer too small for data
		Log_Println(jsonbufferOverflow, LOGLEVEL_ERROR);
	}

	// serialize JSON in a more optimized way using a shared buffer
	const size_t len = measureJson(doc);
	AsyncWebSocketMessageBuffer *jsonBuffer = ws.makeBuffer(len);
	if (!jsonBuffer) {
		// memory allocation of vector failed, we can not use the AsyncWebSocketMessageBuffer
		Log_Println(unableToAllocateMem, LOGLEVEL_ERROR);
		return;
	}
	serializeJson(doc, jsonBuffer->get(), len);
	if (client == 0) {
		ws.textAll(jsonBuffer);
	} else {
		ws.text(client, jsonBuffer);
	}
}

// Processes websocket-requests
void onWebsocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {

	// discard message on queue full, socket should not be closed
	client->setCloseClientOnQueueFull(false);

	if (type == WS_EVT_CONNECT) {
		// client connected
		Log_Printf(LOGLEVEL_DEBUG, "ws[%s][%u] connect", server->url(), client->id());
		// Send initial operation mode and RSSI to newly connected client
		Web_SendWebsocketData(client->id(), WebsocketCodeType::OperationMode);
		Web_SendWebsocketData(client->id(), WebsocketCodeType::FtpStatus);
		// client->printf("Hello Client %u :)", client->id());
		// client->ping();
	} else if (type == WS_EVT_DISCONNECT) {
		// client disconnected
		Log_Printf(LOGLEVEL_DEBUG, "ws[%s][%u] disconnect", server->url(), client->id());
	} else if (type == WS_EVT_ERROR) {
		// error was received from the other end
		Log_Printf(LOGLEVEL_DEBUG, "ws[%s][%u] error(%u): %s", server->url(), client->id(), *((uint16_t *) arg), (char *) data);
	} else if (type == WS_EVT_PONG) {
		// pong message was received (in response to a ping request maybe)
		Log_Printf(LOGLEVEL_DEBUG, "ws[%s][%u] pong[%u]: %s", server->url(), client->id(), len, (len) ? (char *) data : "");
	} else if (type == WS_EVT_DATA) {
		// data packet
		const AwsFrameInfo *info = (AwsFrameInfo *) arg;
		if (info && info->final && info->index == 0 && info->len == len && client && len > 0) {
			// the whole message is in a single frame and we got all of it's data
			// Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

			WebsocketCodeType result = processJsonRequest((char *) data);
			if (result != WebsocketCodeType::Error) {
				if (data && (strncmp((char *) data, "track", 5))) { // Don't send back ok-feedback if track's name is requested in background
					Web_SendWebsocketData(client->id(), result);
				}
			}

			if (info->opcode == WS_TEXT) {
				data[len] = 0;
				// Serial.printf("%s\n", (char *)data);
			} else {
				for (size_t i = 0; i < info->len; i++) {
					Serial.printf("%02x ", data[i]);
				}
				// Serial.printf("\n");
			}
		}
	}
}

// Handles file upload request from the explorer
// requires a GET parameter path, as directory path to the file
void explorerHandleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {

	System_UpdateActivityTimer();

	// New File
	if (!index) {
		// reset abort flag
		uploadAborted = false;

		String utf8Folder = "/";
		String utf8FilePath;
		if (request->hasParam("path")) {
			const AsyncWebParameter *param = request->getParam("path");
			utf8Folder = param->value() + "/";
		}
		utf8FilePath = utf8Folder + filename;

		const char *filePath = utf8FilePath.c_str();

		Log_Printf(LOGLEVEL_INFO, writingFile, filePath);

		if (!allocateDoubleBuffer()) {
			// we failed to allocate enough memory
			Log_Println(unableToAllocateMem, LOGLEVEL_ERROR);
			handleUploadError(request, 500);
			return;
		}

		// Create Queue for receiving a signal from the store task as synchronisation
		if (explorerFileUploadFinished == NULL) {
			explorerFileUploadFinished = xSemaphoreCreateBinary();
		} else {
			// make sure semaphore is empty
			xSemaphoreTake(explorerFileUploadFinished, 0);
		}

		// reset buffers
		index_buffer_write = 0;
		index_buffer_read = 0;
		for (uint32_t i = 0; i < nr_of_buffers; i++) {
			size_in_buffer[i] = 0;
			buffer_full[i] = false;
		}

		// Create Task for handling the storage of the data
		const char *filePathCopy = x_strdup(filePath);
		xTaskCreatePinnedToCore(
			explorerHandleFileStorageTask, /* Function to implement the task */
			"fileStorageTask", /* Name of the task */
			4000, /* Stack size in words */
			(void *) filePathCopy, /* Task input parameter */
			2 | portPRIVILEGE_BIT, /* Priority of the task */
			&fileStorageTaskHandle, /* Task handle. */
			1 /* Core where the task should run */
		);

		// register for early disconnect events
		request->onDisconnect([]() {
			// client went away before we were finished...
			// trigger task suicide, since we can not use Log_Println here
			xTaskNotify(fileStorageTaskHandle, 2u, eSetValueWithOverwrite);
		});
	}

	if (uploadAborted) {
		if (!request->_tempObject) {
			handleUploadError(request, 500);
		}
		return;
	}

	if (len) {
		// wait till buffer is ready
		while (buffer_full[index_buffer_write]) {
			if (uploadAborted) {
				return;
			}
			vTaskDelay(2u);
		}

		size_t len_to_write = len;
		size_t space_left = chunk_size - size_in_buffer[index_buffer_write];
		if (space_left < len_to_write) {
			len_to_write = space_left;
		}
		// write content to buffer
		memcpy(buffer[index_buffer_write] + size_in_buffer[index_buffer_write], data, len_to_write);
		size_in_buffer[index_buffer_write] = size_in_buffer[index_buffer_write] + len_to_write;

		// check if buffer is filled. If full, signal that ready and change buffers
		if (size_in_buffer[index_buffer_write] == chunk_size) {
			// signal, that buffer is ready. Increment index
			buffer_full[index_buffer_write] = true;
			index_buffer_write = (index_buffer_write + 1) % nr_of_buffers;

			// if still content left, put it into next buffer
			if (len_to_write < len) {
				// wait till new buffer is ready
				while (buffer_full[index_buffer_write]) {
					if (uploadAborted) {
						return;
					}
					vTaskDelay(2u);
				}
				size_t len_left_to_write = len - len_to_write;
				memcpy(buffer[index_buffer_write], data + len_to_write, len_left_to_write);
				size_in_buffer[index_buffer_write] = len_left_to_write;
			}
		}
	}

	if (final) {
		if (uploadAborted) {
			handleUploadError(request, 500);
			return;
		}
		// if file not completely done yet, signal that buffer is filled
		if (size_in_buffer[index_buffer_write] > 0) {
			buffer_full[index_buffer_write] = true;
		}
		// notify storage task that last data was stored on the ring buffer
		xTaskNotify(fileStorageTaskHandle, 1u, eSetValueWithOverwrite);
		// watit until the storage task is sending the signal to finish
		if (xSemaphoreTake(explorerFileUploadFinished, pdMS_TO_TICKS(30000)) != pdTRUE) {
			// timeout, something went wrong
			Log_Println(webTxCanceled, LOGLEVEL_ERROR);
			handleUploadError(request, 500);
			return;
		}
	}
}

// task for writing uploaded data from buffer to SD
// parameter contains the target file path and must be freed by the task.
static void explorerHandleFileStorageTask(void *parameter) {
	const char *filePath = (const char *) parameter;
	File uploadFile;
	size_t bytesOk = 0;
	uint32_t chunkCount = 0;
	uint32_t transferStartTimestamp = millis();
	uint32_t lastUpdateTimestamp = millis();
	uint32_t maxUploadDelay = 30; // After this delay (in seconds) task will be deleted as transfer is considered to be finally broken

	BaseType_t uploadFileNotification;
	uint32_t uploadFileNotificationValue;

	// pause some tasks to get more free CPU time for the upload
	System_PauseTasksDuringUpload(true);

	uploadFile = gFSystem.open(filePath, "w", true); // open file with create=true to make sure parent directories are created
	if (uploadFile) {
		uploadFile.setBufferSize(chunk_size);
	} else {
		Log_Printf(LOGLEVEL_ERROR, "Failed to open file %s for writing!", filePath);
		uploadAborted = true;
	}

	for (;;) {
		if (uploadAborted) {
			break;
		}
		// check buffer is full with enough data or all data already sent
		uploadFileNotification = xTaskNotifyWait(0, 0, &uploadFileNotificationValue, 0);
		if ((buffer_full[index_buffer_read]) || (uploadFileNotification == pdPASS && uploadFileNotificationValue == 1u)) {

			while (buffer_full[index_buffer_read]) {
				chunkCount++;
				size_t item_size = size_in_buffer[index_buffer_read];
				size_t written = 0;
				if (item_size > 0) {
					const uint8_t maxRetries = 3;
					for (uint8_t attempt = 0; attempt < maxRetries && written != item_size; attempt++) {
						if (attempt > 0) {
							Log_Printf(LOGLEVEL_DEBUG, "Write retry %u for chunk %zu on %s", attempt, chunkCount, filePath);
							vTaskDelay(pdMS_TO_TICKS(20 * attempt)); // backoff: 20ms, 40ms
						}
						written = uploadFile.write(buffer[index_buffer_read], item_size);
					}
				}

				if (item_size > 0 && written != item_size) {
					Log_Printf(LOGLEVEL_ERROR, "Write error during upload of %s! (expected %u, wrote %u after retries)",
						filePath, item_size, (uint32_t) written);
					uploadAborted = true;
					break;
				} else {
					bytesOk += written;
				}
				// update handling of buffers
				size_in_buffer[index_buffer_read] = 0;
				buffer_full[index_buffer_read] = 0;
				index_buffer_read = (index_buffer_read + 1) % nr_of_buffers;
				if (chunkCount % 64 == 0) {
					uploadFile.flush();
					System_UpdateActivityTimer();
				}
				// update timestamp
				lastUpdateTimestamp = millis();
			}

			if (uploadFileNotification == pdPASS) {
				if (uploadFile) {
					uploadFile.close();
				}
				Log_Printf(LOGLEVEL_INFO, fileWritten, filePath, bytesOk, (millis() - transferStartTimestamp), (bytesOk) / (millis() - transferStartTimestamp));
				Log_Printf(LOGLEVEL_DEBUG, "Bytes [ok] %zu, Chunks: %zu\n", bytesOk, chunkCount);
				// done exit loop to terminate
				break;
			}
		} else {
			if ((lastUpdateTimestamp + (maxUploadDelay * 1000)) < millis() || ((uploadFileNotification == pdPASS) && (uploadFileNotificationValue == 2u))) {
				Log_Println(webTxCanceled, LOGLEVEL_ERROR);
				if (uploadFile) {
					uploadFile.close();
				}
				uploadAborted = true;
				xSemaphoreGive(explorerFileUploadFinished);
				break;
			}
			vTaskDelay(portTICK_PERIOD_MS * 2);
			continue;
		}
	}
	free(parameter);
	// resume the paused tasks
	System_PauseTasksDuringUpload(false);
	System_UpdateActivityTimer();
	// send signal to upload function to terminate
	xSemaphoreGive(explorerFileUploadFinished);
	fileStorageTaskHandle = NULL;
	vTaskDelete(NULL);
}

// Sends a list of the content of a directory as JSON file
// requires a GET parameter path for the directory
void explorerHandleListRequest(AsyncWebServerRequest *request) {
#ifdef NO_SDCARD
	request->send(200, "application/json; charset=utf-8", "[]"); // maybe better to send 404 here?
	return;
#endif

	File root;
	bool isRoot = false;
	if (request->hasParam("path")) {
		const AsyncWebParameter *param;
		param = request->getParam("path");
		const char *filePath = param->value().c_str();
		if (strcmp(filePath, "/") == 0) {
			isRoot = true;
		}
		root = gFSystem.open(filePath);
	} else {
		root = gFSystem.open("/");
		isRoot = true;
	}

	if (!root) {
		Log_Println(failedToOpenDirectory, LOGLEVEL_DEBUG);
		return;
	}

	if (!root.isDirectory()) {
		Log_Println(notADirectory, LOGLEVEL_DEBUG);
		return;
	}

	AsyncJsonResponse *response = new AsyncJsonResponse(true);
	JsonArray obj = response->getRoot();

	// For root directory, add volume label as first element if available
	if (isRoot) {
		String volumeLabel = SdCard_GetVolumeLabel();
		if (volumeLabel.length() > 0) {
			JsonObject labelEntry = obj.add<JsonObject>();
			labelEntry["name"] = volumeLabel;
			labelEntry["root"] = "sd";
		}
	}

	bool isDir = false;
	String MyfileName = gFSystem.nextFileName(root, &isDir);
	while (MyfileName != "") {
		// ignore hidden folders, e.g. MacOS spotlight files
		if (!MyfileName.startsWith("/.")) {
			JsonObject entry = obj.add<JsonObject>();
			entry["name"] = MyfileName.substring(MyfileName.lastIndexOf('/') + 1);
			if (isDir) {
				entry["dir"].set(true);
			}
		}
		MyfileName = gFSystem.nextFileName(root, &isDir);
	}
	root.close();

	if (response->overflowed()) {
		// JSON buffer too small for data
		Log_Println(jsonbufferOverflow, LOGLEVEL_ERROR);
		request->send(500);
		return;
	}
	response->setLength();
	request->send(response);
}

bool explorerDeleteDirectory(File dir) {

	File file = dir.openNextFile();
	while (file) {

		if (file.isDirectory()) {
			explorerDeleteDirectory(file);
		} else {
			gFSystem.remove(file);
		}

		file = dir.openNextFile();

		esp_task_wdt_reset();
	}

	return gFSystem.rmdir(dir);
}

// macOS metadata junk: Finder/Spotlight droppings that are useless on a music box
static bool Web_IsMacJunkName(const char *name) {
	return (strcmp(name, ".DS_Store") == 0) || (strncmp(name, "._", 2) == 0) || (strcmp(name, ".Spotlight-V100") == 0) || (strcmp(name, ".fseventsd") == 0) || (strcmp(name, ".Trashes") == 0) || (strcmp(name, ".TemporaryItems") == 0);
}

static void Web_CleanDirectory(File dir, uint32_t &deletedCount) {
	File file = dir.openNextFile();
	while (file) {
		const String path = file.path();
		const bool isDir = file.isDirectory();
		if (Web_IsMacJunkName(file.name())) {
			file.close();
			if (isDir) {
				File junkDir = gFSystem.open(path);
				if (junkDir && explorerDeleteDirectory(junkDir)) {
					deletedCount++;
				}
			} else if (gFSystem.remove(path)) {
				deletedCount++;
			}
		} else if (isDir) {
			Web_CleanDirectory(file, deletedCount);
			file.close();
		} else {
			file.close();
		}
		file = dir.openNextFile();
		esp_task_wdt_reset();
	}
}

// Handles the request to remove macOS metadata junk from the SD card
static void handleCleanSdRequest(AsyncWebServerRequest *request) {
	uint32_t deletedCount = 0;
	File root = gFSystem.open("/");
	if (root && root.isDirectory()) {
		Web_CleanDirectory(root, deletedCount);
	}
	Log_Printf(LOGLEVEL_NOTICE, "SD cleanup: removed %lu entries", (unsigned long) deletedCount);
	AsyncJsonResponse *response = new AsyncJsonResponse(false);
	response->getRoot()["deleted"] = deletedCount;
	response->setLength();
	request->send(response);
}

// Handles download request of a file
// requires a GET parameter path to the file
void explorerHandleDownloadRequest(AsyncWebServerRequest *request) {
	File file;
	const AsyncWebParameter *param;
	// check has path param
	if (!request->hasParam("path")) {
		Log_Println("DOWNLOAD: No path variable set", LOGLEVEL_ERROR);
		request->send(404);
		return;
	}
	// check file exists on SD card
	param = request->getParam("path");
	const char *filePath = param->value().c_str();
	if (!gFSystem.exists(filePath)) {
		Log_Printf(LOGLEVEL_ERROR, "DOWNLOAD:  File not found on SD card: %s", filePath);
		request->send(404);
		return;
	}
	// check is file and not a directory
	file = gFSystem.open(filePath);
	if (file.isDirectory()) {
		Log_Printf(LOGLEVEL_ERROR, "DOWNLOAD:  Cannot download a directory %s", filePath);
		request->send(404);
		file.close();
		return;
	}

	// ready to serve the file for download.
	String dataType = "application/octet-stream";
	struct fileBlk {
		File dataFile;
	};
	fileBlk *fileObj = new fileBlk;
	fileObj->dataFile = file;
	request->_tempObject = (void *) fileObj;

	AsyncWebServerResponse *response = request->beginResponse(dataType, fileObj->dataFile.size(), [request](uint8_t *buffer, size_t maxlen, size_t index) -> size_t {
		fileBlk *fileObj = (fileBlk *) request->_tempObject;
		size_t thisSize = fileObj->dataFile.read(buffer, maxlen);
		if ((index + thisSize) >= fileObj->dataFile.size()) {
			fileObj->dataFile.close();
			request->_tempObject = NULL;
			delete fileObj;
		}
		return thisSize;
	});
	String filename = String(param->value().c_str());
	response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
	request->send(response);
}

// Handles delete request of a file or directory
// requires a GET parameter path to the file or directory
void explorerHandleDeleteRequest(AsyncWebServerRequest *request) {
	File file;
	if (request->hasParam("path")) {
		const AsyncWebParameter *param;
		param = request->getParam("path");
		System_UpdateActivityTimer();
		const char *filePath = param->value().c_str();
		if (gFSystem.exists(filePath)) {
			// stop playback, file to delete might be in use
			Cmd_Action(CMD_STOP);
			file = gFSystem.open(filePath);
			if (file.isDirectory()) {
				if (explorerDeleteDirectory(file)) {
					Log_Printf(LOGLEVEL_INFO, "DELETE:  %s deleted", filePath);
				} else {
					Log_Printf(LOGLEVEL_ERROR, "DELETE:  Cannot delete %s", filePath);
				}
			} else {
				if (gFSystem.remove(filePath)) {
					Log_Printf(LOGLEVEL_INFO, "DELETE:  %s deleted", filePath);
				} else {
					Log_Printf(LOGLEVEL_ERROR, "DELETE:  Cannot delete %s", filePath);
				}
			}
		} else {
			Log_Printf(LOGLEVEL_ERROR, "DELETE:  Path %s does not exist", filePath);
		}
	} else {
		Log_Println("DELETE:  No path variable set", LOGLEVEL_ERROR);
	}
	request->send(200);
	esp_task_wdt_reset();
}

// Handles create request of a directory
// requires a GET parameter path to the new directory
void explorerHandleCreateRequest(AsyncWebServerRequest *request) {
	if (request->hasParam("path")) {
		const AsyncWebParameter *param;
		System_UpdateActivityTimer();
		param = request->getParam("path");
		const char *filePath = param->value().c_str();
		if (gFSystem.mkdir(filePath)) {
			Log_Printf(LOGLEVEL_INFO, "CREATE:  %s created", filePath);
		} else {
			Log_Printf(LOGLEVEL_ERROR, "CREATE:  Cannot create %s", filePath);
		}
	} else {
		Log_Println("CREATE:  No path variable set", LOGLEVEL_ERROR);
	}
	request->send(200);
}

// Handles rename request of a file or directory
// requires a GET parameter srcpath to the old file or directory name
// requires a GET parameter dstpath to the new file or directory name
void explorerHandleRenameRequest(AsyncWebServerRequest *request) {
	if (request->hasParam("srcpath") && request->hasParam("dstpath")) {
		const AsyncWebParameter *srcPath;
		const AsyncWebParameter *dstPath;
		System_UpdateActivityTimer();
		srcPath = request->getParam("srcpath");
		dstPath = request->getParam("dstpath");
		const char *srcFullFilePath = srcPath->value().c_str();
		const char *dstFullFilePath = dstPath->value().c_str();
		if (gFSystem.exists(srcFullFilePath)) {
			if (gFSystem.rename(srcFullFilePath, dstFullFilePath)) {
				Log_Printf(LOGLEVEL_INFO, "RENAME:  %s renamed to %s", srcFullFilePath, dstFullFilePath);
			} else {
				Log_Printf(LOGLEVEL_ERROR, "RENAME:  Cannot rename %s", srcFullFilePath);
			}
		} else {
			Log_Printf(LOGLEVEL_ERROR, "RENAME: Path %s does not exist", srcFullFilePath);
		}
	} else {
		Log_Println("RENAME: No path variable set", LOGLEVEL_ERROR);
	}

	request->send(200);
}

// Handles audio play requests
// requires a GET parameter path to the audio file or directory
// requires a GET parameter playmode
void explorerHandleAudioRequest(AsyncWebServerRequest *request) {
	const AsyncWebParameter *param;
	String playModeString;
	uint32_t playMode;
	if (request->hasParam("path") && request->hasParam("playmode")) {
		if (!System_IsWebControlAllowed()) {
			Log_Println(notAllowedInCurrentMode, LOGLEVEL_NOTICE);
			Web_SendWebsocketData(0, WebsocketCodeType::NotAllowedInCurrentMode);
			request->send(200);
			return;
		}
		param = request->getParam("path");
		const char *filePath = param->value().c_str();
		param = request->getParam("playmode");
		playModeString = param->value();

		playMode = atoi(playModeString.c_str());
		Rfid_ResetOldRfid();
		AudioPlayer_SetPlaylist(filePath, 0, playMode, 0);
	} else {
		Log_Println("AUDIO: No path variable set", LOGLEVEL_ERROR);
	}

	request->send(200);
}

// Handles track progress requests
void handleTrackProgressRequest(AsyncWebServerRequest *request) {
	String json = "{\"trackProgress\":{";
	json += "\"posPercent\":" + String(gPlayProperties.currentRelPos);
	json += ",\"time\":" + String(AudioPlayer_GetCurrentTime());
	json += ",\"duration\":" + String(AudioPlayer_GetFileDuration());
	json += "}}";
	request->send(200, "application/json", json);
}

void handleGetSavedSSIDs(AsyncWebServerRequest *request) {
	// optional parameter to include credentials & static-ip config (used for backups)
	const bool withCredentials = request->hasParam("credentials");
	AsyncJsonResponse *response = new AsyncJsonResponse(true);
	JsonArray json_ssids = response->getRoot();
	Wlan_GetSavedNetworks([json_ssids, withCredentials](const WiFiSettings &network) {
		if (!withCredentials) {
			json_ssids.add(network.ssid);
			return;
		}
		JsonObject entry = json_ssids.add<JsonObject>();
		entry["ssid"] = network.ssid;
		entry["pwd"] = network.password;
		if (network.staticIp.isValid()) {
			entry["static"] = true;
			entry["static_addr"] = network.staticIp.addr.toString();
			entry["static_subnet"] = network.staticIp.subnet.toString();
			entry["static_gateway"] = network.staticIp.gateway.toString();
			entry["static_dns1"] = network.staticIp.dns1.toString();
			entry["static_dns2"] = network.staticIp.dns2.toString();
		}
	});

	response->setLength();
	request->send(response);
}

void handlePostSavedSSIDs(AsyncWebServerRequest *request, JsonVariant &json) {
	WiFiSettings networkSettings;

	networkSettings.ssid = json["ssid"].as<const char *>();
	networkSettings.password = json["pwd"].as<const char *>();

	if (json["static"].as<bool>()) {
		networkSettings.staticIp.addr = json["static_addr"].as<IPAddress>();
		networkSettings.staticIp.subnet = json["static_subnet"].as<IPAddress>();
		networkSettings.staticIp.gateway = json["static_gateway"].as<IPAddress>();
		networkSettings.staticIp.dns1 = json["static_dns1"].as<IPAddress>();
		networkSettings.staticIp.dns2 = json["static_dns2"].as<IPAddress>();
	}

	if (!networkSettings.isValid()) {
		// The data was corrupted, so user error
		request->send(400, "text/plain; charset=utf-8", "error adding network");
		return;
	}

	bool succ = Wlan_AddNetworkSettings(networkSettings);
	if (succ) {
		request->send(200, "text/plain; charset=utf-8", networkSettings.ssid);
	} else {
		request->send(500, "text/plain; charset=utf-8", "error adding network");
	}
}

void handleDeleteSavedSSIDs(AsyncWebServerRequest *request) {
	const AsyncWebParameter *p = request->getParam("ssid");
	const String ssid = p->value();

	bool succ = Wlan_DeleteNetwork(ssid);

	if (succ) {
		request->send(200, "text/plain; charset=utf-8", ssid);
	} else {
		request->send(500, "text/plain; charset=utf-8", "error deleting network");
	}
}

void handleGetActiveSSID(AsyncWebServerRequest *request) {
	AsyncJsonResponse *response = new AsyncJsonResponse();
	JsonObject obj = response->getRoot();

	if (Wlan_IsConnected()) {
		String active = Wlan_GetCurrentSSID();
		obj["active"] = active;
	}

	response->setLength();
	request->send(response);
}

void handleGetWiFiConfig(AsyncWebServerRequest *request) {
	AsyncJsonResponse *response = new AsyncJsonResponse();
	JsonObject obj = response->getRoot();
	bool scanWifiOnStart = gPrefsSettings.getBool("ScanWiFiOnStart", false);

	obj["hostname"] = Wlan_GetHostname();
	obj["scanOnStart"].set(scanWifiOnStart);

	response->setLength();
	request->send(response);
}

void handlePostWiFiConfig(AsyncWebServerRequest *request, JsonVariant &json) {
	const JsonObject &jsonObj = json.as<JsonObject>();

	// always perform perform a WiFi scan on startup?
	bool alwaysScan = jsonObj["scanOnStart"];
	gPrefsSettings.putBool("ScanWiFiOnStart", alwaysScan);

	// hostname
	String strHostname = jsonObj["hostname"];
	if (!Wlan_ValidateHostname(strHostname)) {
		Log_Println("hostname validation failed", LOGLEVEL_ERROR);
		request->send(400, "text/plain; charset=utf-8", "hostname validation failed");
		return;
	}

	bool succ = Wlan_SetHostname(strHostname);
	if (succ) {
		Log_Println("WiFi configuration saved.", LOGLEVEL_NOTICE);
		request->send(200, "text/plain; charset=utf-8", strHostname);
	} else {
		Log_Println("error setting hostname", LOGLEVEL_ERROR);
		request->send(500, "text/plain; charset=utf-8", "error setting hostname");
	}
}

static bool tagIdToJSON(const String tagId, JsonObject entry) {
	String s = gPrefsRfid.getString(tagId.c_str(), ""); // Try to lookup rfidId in NVS
	if (s.length() == 0 || s == "-1") {
		return false;
	}
	char _file[256] = {0};
	uint32_t _lastPlayPos = 0;
	uint16_t _trackLastPlayed = 0;
	uint32_t _mode = 1;

	char s_buf[512];
	strncpy(s_buf, s.c_str(), sizeof(s_buf) - 1);
	s_buf[sizeof(s_buf) - 1] = '\0';

	char *token = strtok(s_buf, stringDelimiter);
	uint8_t i = 1;
	while (token != NULL) { // Try to extract data from string after lookup
		if (i == 1) {
			strncpy(_file, token, sizeof(_file) - 1);
		} else if (i == 2) {
			_lastPlayPos = strtoul(token, NULL, 10);
		} else if (i == 3) {
			_mode = strtoul(token, NULL, 10);
		} else if (i == 4) {
			_trackLastPlayed = strtoul(token, NULL, 10);
		}
		i++;
		token = strtok(NULL, stringDelimiter);
	}
	entry["id"] = tagId;
	if (_mode >= 100) {
		entry["modId"] = _mode;
	} else {
		entry["fileOrUrl"] = _file;
		entry["playMode"] = _mode;
		entry["lastPlayPos"] = _lastPlayPos;
		entry["trackLastPlayed"] = _trackLastPlayed;
	}
	return true;
}

// callback for writing a NVS entry to list
bool DumpNvsToArrayCallback(const char *key, void *data) {
	std::vector<String> *keys = (std::vector<String> *) data;
	keys->push_back(key);
	return true;
}

static String tagIdToJsonStr(const char *key, const bool nameOnly) {
	if (nameOnly) {
		return "\"" + String(key) + "\"";
	} else {
		JsonDocument doc;
		JsonObject entry = doc[key].to<JsonObject>();
		if (!tagIdToJSON(key, entry)) {
			return "";
		}
		String serializedJsonString;
		serializeJson(entry, serializedJsonString);
		return serializedJsonString;
	}
}

// Handles rfid-assignments requests (GET)
// /rfid returns an array of tag-ids and details. Optional GET param "id" to list only a single assignment.
// /rfid/ids-only returns an array of tag-id keys
static void handleGetRFIDRequest(AsyncWebServerRequest *request) {

	String tagId = "";

	if (request->hasParam("id")) {
		tagId = request->getParam("id")->value();
	}

	if ((tagId != "") && gPrefsRfid.isKey(tagId.c_str())) {
		// return single RFID entry with details
		String json = tagIdToJsonStr(tagId.c_str(), false);
		request->send(200, "application/json", json);
		return;
	}
	// get tag details or just an array of id's
	bool idsOnly = request->hasParam("ids-only");

	std::vector<String> nvsKeys {};
	nvsKeys.clear();
	// Dumps all RFID-keys from NVS into key array
	listNVSKeys("rfidTags", &nvsKeys, DumpNvsToArrayCallback);
	if (nvsKeys.size() == 0) {
		// no entries
		request->send(200, "application/json", "[]");
		return;
	}
	// construct chunked repsonse
	AsyncWebServerResponse *response = request->beginChunkedResponse("application/json",
		[nvsKeys = std::move(nvsKeys), idsOnly, nvsIndex = size_t(0)](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
			maxLen = maxLen >> 1; // some sort of bug with actual size available, reduce the len
			size_t len = 0;
			String json;

			if (nvsIndex == 0) {
				// start, write first tag
				json = tagIdToJsonStr(nvsKeys[nvsIndex].c_str(), idsOnly);
				if (json.length() >= maxLen) {
					Log_Println("/rfid: Buffer too small", LOGLEVEL_ERROR);
					return len;
				}
				len += snprintf(((char *) buffer), maxLen - len, "[%s", json.c_str());
				nvsIndex++;
			}
			while (nvsIndex < nvsKeys.size()) {
				// write tags as long we have enough room
				json = tagIdToJsonStr(nvsKeys[nvsIndex].c_str(), idsOnly);
				if ((len + json.length()) >= maxLen) {
					break;
				}
				len += snprintf(((char *) buffer + len), maxLen - len, ",%s", json.c_str());
				nvsIndex++;
			}
			if (nvsIndex == nvsKeys.size()) {
				// finish
				len += snprintf(((char *) buffer + len), maxLen - len, "]");
				nvsIndex++;
			}
			return len;
		});
	request->send(response);
}

static void handlePostRFIDRequest(AsyncWebServerRequest *request, JsonVariant &json) {
	const JsonObject &jsonObj = json.as<JsonObject>();

	String tagId = jsonObj["id"];
	if (tagId.isEmpty()) {
		Log_Println("/rfid (POST): Missing tag id", LOGLEVEL_ERROR);
		request->send(500, "text/plain; charset=utf-8", "/rfid (POST): Missing tag id");
		return;
	}
	String fileOrUrl = jsonObj["fileOrUrl"];
	if (fileOrUrl.isEmpty()) {
		fileOrUrl = "0";
	}
	const char *_fileOrUrlAscii = fileOrUrl.c_str();
	uint8_t _playModeOrModId;
	if (jsonObj["modId"].is<u_int8_t>()) {
		_playModeOrModId = jsonObj["modId"];
	} else {
		_playModeOrModId = jsonObj["playMode"];
	}
	if (_playModeOrModId <= 0) {
		Log_Println("/rfid (POST): Invalid playMode or modId", LOGLEVEL_ERROR);
		request->send(500, "text/plain; charset=utf-8", "/rfid (POST): Invalid playMode or modId");
		return;
	}
	char rfidString[275];
	snprintf(rfidString, sizeof(rfidString) / sizeof(rfidString[0]), "%s%s%s0%s%u%s0", stringDelimiter, _fileOrUrlAscii, stringDelimiter, stringDelimiter, _playModeOrModId, stringDelimiter);
	gPrefsRfid.putString(tagId.c_str(), rfidString);

	String s = gPrefsRfid.getString(tagId.c_str(), "-1");
	if (s.compareTo(rfidString)) {
		request->send(500, "text/plain; charset=utf-8", "/rfid (POST): cannot save assignment to NVS");
		return;
	}
	Web_DumpNvsToSd("rfidTags", backupFile); // Store backup-file every time when a new rfid-tag is programmed
	// return the new/modified RFID assignment
	AsyncJsonResponse *response = new AsyncJsonResponse(false);
	JsonObject obj = response->getRoot();
	tagIdToJSON(tagId, obj);
	response->setLength();
	request->send(response);
}

static void handleDeleteRFIDRequest(AsyncWebServerRequest *request) {
	String tagId = "";
	if (request->hasParam("id")) {
		tagId = request->getParam("id")->value();
	}
	if (tagId.isEmpty()) {
		Log_Println("/rfid (DELETE): Missing tag id", LOGLEVEL_ERROR);
		request->send(500, "text/plain; charset=utf-8", "/rfid (DELETE): Missing tag id");
		return;
	}
	if (gPrefsRfid.isKey(tagId.c_str())) {
		if (tagId.equals(gCurrentRfidTagId)) {
			// stop playback, tag to delete is in use
			Cmd_Action(CMD_STOP);
		}
		if (gPrefsRfid.remove(tagId.c_str())) {
			Log_Printf(LOGLEVEL_INFO, "/rfid (DELETE): tag %s removed successfuly", tagId);
			request->send(200, "text/plain; charset=utf-8", tagId + " removed successfuly");
		} else {
			Log_Println("/rfid (DELETE):error removing tag from NVS", LOGLEVEL_ERROR);
			request->send(500, "text/plain; charset=utf-8", "error removing tag from NVS");
		}
	} else {
		Log_Printf(LOGLEVEL_DEBUG, "/rfid (DELETE): tag %s not exists", tagId);
		request->send(404, "text/plain; charset=utf-8", "error removing tag from NVS: Tag not exists");
	}
}

// Takes stream from file-upload and writes payload into a temporary sd-file.
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
	static File tmpFile;
	static size_t fileIndex = 0;
	static char tmpFileName[13];
	esp_task_wdt_reset();
	if (!index) {
		snprintf(tmpFileName, 13, "/_%lu", millis());
		tmpFile = gFSystem.open(tmpFileName, FILE_WRITE);
	} else {
		tmpFile.seek(fileIndex);
	}

	if (!tmpFile) {
		Log_Println(errorWritingTmpfile, LOGLEVEL_ERROR);
		return;
	}

	size_t wrote = tmpFile.write(data, len);
	if (wrote != len) {
		// we did not write all bytes --> fail
		Log_Printf(LOGLEVEL_ERROR, "Error writing %s. Expected %u, wrote %u (error: %u)!", tmpFile.path(), len, wrote, tmpFile.getWriteError());
		return;
	}
	fileIndex += wrote;

	if (final) {
		tmpFile.close();
		Web_DumpSdToNvs(tmpFileName);
		fileIndex = 0;
	}
}

// Parses content of temporary backup-file and writes payload into NVS
void Web_DumpSdToNvs(const char *_filename) {
	char ebuf[290];
	uint16_t j = 0;
	char *token;
	bool count = false;
	uint16_t importCount = 0;
	uint16_t invalidCount = 0;
	nvs_t nvsEntry[1];
	File tmpFile = gFSystem.open(_filename);

	if (!tmpFile || (tmpFile.available() < 3)) {
		Log_Println(errorReadingTmpfile, LOGLEVEL_ERROR);
		return;
	}

	Led_SetPause(true);
	// try to read UTF-8 BOM marker
	bool isUtf8 = (tmpFile.read() == 0xEF) && (tmpFile.read() == 0xBB) && (tmpFile.read() == 0xBF);
	if (!isUtf8) {
		// no BOM found, reset to start of file
		tmpFile.seek(0);
	}

	while (tmpFile.available() > 0) {
		if (j >= sizeof(ebuf)) {
			Log_Println(errorReadingTmpfile, LOGLEVEL_ERROR);
			return;
		}
		char buf = tmpFile.read();
		if (buf != '\n') {
			ebuf[j++] = buf;
		} else {
			ebuf[j] = '\0';
			j = 0;
			token = strtok(ebuf, stringOuterDelimiter);
			while (token != NULL) {
				if (!count) {
					count = true;
					size_t keyLen = std::min(strlen(token), sizeof(nvsEntry[0].nvsKey) - 1);
					memcpy(nvsEntry[0].nvsKey, token, keyLen);
					nvsEntry[0].nvsKey[keyLen] = '\0';
				} else {
					count = false;
					if (isUtf8) {
						size_t entryLen = std::min(strlen(token), sizeof(nvsEntry[0].nvsEntry) - 1);
						memcpy(nvsEntry[0].nvsEntry, token, entryLen);
						nvsEntry[0].nvsEntry[entryLen] = '\0';
					} else {
						convertAsciiToUtf8(String(token), nvsEntry[0].nvsEntry, sizeof(nvsEntry[0].nvsEntry));
					}
				}
				token = strtok(NULL, stringOuterDelimiter);
			}
			if (isNumber(nvsEntry[0].nvsKey) && nvsEntry[0].nvsEntry[0] == '#') {
				Log_Printf(LOGLEVEL_NOTICE, writeEntryToNvs, ++importCount, nvsEntry[0].nvsKey, nvsEntry[0].nvsEntry);
				gPrefsRfid.putString(nvsEntry[0].nvsKey, nvsEntry[0].nvsEntry);
			} else {
				invalidCount++;
			}
		}
	}

	Led_SetPause(false);
	Log_Printf(LOGLEVEL_NOTICE, importCountNokNvs, invalidCount);
	tmpFile.close();
	gFSystem.remove(_filename);
}

// handle album cover image request
static void handleCoverImageRequest(AsyncWebServerRequest *request) {

	if (!gPlayProperties.coverFilePos || !gPlayProperties.playlist) {
		String stationLogoUrl = AudioPlayer_GetStationLogoUrl();
		if (stationLogoUrl != "") {
			// serve station logo
			Log_Printf(LOGLEVEL_NOTICE, "serve station logo: '%s'", stationLogoUrl.c_str());
			request->redirect(stationLogoUrl);
			return;
		} else
			// empty image:
			// request->send(200, "image/svg+xml", "<?xml version=\"1.0\"?><svg xmlns=\"http://www.w3.org/2000/svg\"/>");
			if (gPlayProperties.playMode == WEBSTREAM || (gPlayProperties.playMode == LOCAL_M3U && gPlayProperties.isWebstream)) {
				// no cover -> send placeholder icon for webstream (fa-soundcloud)
				Log_Println("no cover image for webstream", LOGLEVEL_NOTICE);
				request->send(200, "image/svg+xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><svg width=\"2304\" height=\"1792\" viewBox=\"0 0 2304 1792\" transform=\"scale (0.6)\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M784 1372l16-241-16-523q-1-10-7.5-17t-16.5-7q-9 0-16 7t-7 17l-14 523 14 241q1 10 7.5 16.5t15.5 6.5q22 0 24-23zm296-29l11-211-12-586q0-16-13-24-8-5-16-5t-16 5q-13 8-13 24l-1 6-10 579q0 1 11 236v1q0 10 6 17 9 11 23 11 11 0 20-9 9-7 9-20zm-1045-340l20 128-20 126q-2 9-9 9t-9-9l-17-126 17-128q2-9 9-9t9 9zm86-79l26 207-26 203q-2 9-10 9-9 0-9-10l-23-202 23-207q0-9 9-9 8 0 10 9zm280 453zm-188-491l25 245-25 237q0 11-11 11-10 0-12-11l-21-237 21-245q2-12 12-12 11 0 11 12zm94-7l23 252-23 244q-2 13-14 13-13 0-13-13l-21-244 21-252q0-13 13-13 12 0 14 13zm94 18l21 234-21 246q-2 16-16 16-6 0-10.5-4.5t-4.5-11.5l-20-246 20-234q0-6 4.5-10.5t10.5-4.5q14 0 16 15zm383 475zm-289-621l21 380-21 246q0 7-5 12.5t-12 5.5q-16 0-18-18l-18-246 18-380q2-18 18-18 7 0 12 5.5t5 12.5zm94-86l19 468-19 244q0 8-5.5 13.5t-13.5 5.5q-18 0-20-19l-16-244 16-468q2-19 20-19 8 0 13.5 5.5t5.5 13.5zm98-40l18 506-18 242q-2 21-22 21-19 0-21-21l-16-242 16-506q0-9 6.5-15.5t14.5-6.5q9 0 15 6.5t7 15.5zm392 742zm-198-746l15 510-15 239q0 10-7.5 17.5t-17.5 7.5-17-7-8-18l-14-239 14-510q0-11 7.5-18t17.5-7 17.5 7 7.5 18zm99 19l14 492-14 236q0 11-8 19t-19 8-19-8-9-19l-12-236 12-492q1-12 9-20t19-8 18.5 8 8.5 20zm212 492l-14 231q0 13-9 22t-22 9-22-9-10-22l-6-114-6-117 12-636v-3q2-15 12-24 9-7 20-7 8 0 15 5 14 8 16 26zm1112-19q0 117-83 199.5t-200 82.5h-786q-13-2-22-11t-9-22v-899q0-23 28-33 85-34 181-34 195 0 338 131.5t160 323.5q53-22 110-22 117 0 200 83t83 201z\"/></svg>");
			} else {
				// no cover -> send placeholder icon for playing music from SD-card (fa-music)
				if (gPlayProperties.playMode != NO_PLAYLIST) {
					Log_Println("no cover image for SD-card audio", LOGLEVEL_DEBUG);
				}
				request->send(200, "image/svg+xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><svg width=\"1792\" height=\"1792\" viewBox=\"0 0 1792 1792\" transform=\"scale (0.6)\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M1664 224v1120q0 50-34 89t-86 60.5-103.5 32-96.5 10.5-96.5-10.5-103.5-32-86-60.5-34-89 34-89 86-60.5 103.5-32 96.5-10.5q105 0 192 39v-537l-768 237v709q0 50-34 89t-86 60.5-103.5 32-96.5 10.5-96.5-10.5-103.5-32-86-60.5-34-89 34-89 86-60.5 103.5-32 96.5-10.5q105 0 192 39v-967q0-31 19-56.5t49-35.5l832-256q12-4 28-4 40 0 68 28t28 68z\"/></svg>");
			}
		return;
	}
	if (gPlayProperties.currentTrackNumber >= gPlayProperties.playlist->size()) {
		return;
	}
	const char *coverFileName = gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber);
	String decodedCover = "/.cache";
	decodedCover.concat(coverFileName);

	File coverFile;
	if (gFSystem.exists(decodedCover)) {
		coverFile = gFSystem.open(decodedCover, FILE_READ);
	} else {
		coverFile = gFSystem.open(coverFileName, FILE_READ);
	}
	char mimeType[256] {0};
	char fileType[4];
	coverFile.readBytes(fileType, 4);
	if (strncmp(fileType, "ID3", 3) == 0) { // mp3 (ID3v2) Routine
		// seek to start position
		coverFile.seek(gPlayProperties.coverFilePos);
		uint8_t encoding = coverFile.read();
		if (fileType[3] == 0x02) {
			// image format (3 Bytes) for ID3v2.2
			coverFile.readBytes(mimeType, 3);
			if (strcmp(mimeType, "JPG") == 0) {
				strcpy(mimeType, "image/jpeg");
			} else if (strcmp(mimeType, "PNG") == 0) {
				strcpy(mimeType, "image/png");
			} else if (strcmp(mimeType, "-->") != 0) {
				strcpy(mimeType, "application/octet-stream");
			}
		} else {
			// mime-type (null terminated) for ID3v2.3 and ID3v2.4
			for (uint8_t i = 0u; i < 255; i++) {
				mimeType[i] = coverFile.read();
				if (uint8_t(mimeType[i]) == 0) {
					break;
				}
			}
		}
		// skip image type (1 Byte)
		coverFile.read();
		// skip description (null terminated)
		if (encoding == 0 || encoding == 3) { // ISO-8859-1 and UTF-8: 00 terminated
			while (coverFile.read() != 0) { }
		} else if (encoding == 1 || encoding == 2) { // UTF-16 and UTF-16BE: 00 00 terminated
			while ((coverFile.read() | (coverFile.read() << 8)) != 0) { }
		}
	} else if (strncmp(fileType, "fLaC", 4) == 0) { // flac Routine
		uint32_t length = 0; // length of strings: MIME type, description of the picture, binary picture data
		coverFile.seek(gPlayProperties.coverFilePos + 4); // pass only picture type (4 Bytes) (audioI2S points to METADATA_BLOCK_PICTURE since 6241daa)
		for (int i = 0; i < 4; ++i) { // length of mime type string
			length = (length << 8) | coverFile.read();
		}
		if (length > 255) {
			Log_Printf(LOGLEVEL_ERROR, "Unexpected MIME type string length (%u > 255). Possible corrupted cover image or wrong coverFilePos (%u). Aborting extraction.", length, gPlayProperties.coverFilePos);
			request->send(200, "image/svg+xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><svg width=\"1792\" height=\"1792\" viewBox=\"0 0 1792 1792\" transform=\"scale (0.6)\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M1664 224v1120q0 50-34 89t-86 60.5-103.5 32-96.5 10.5-96.5-10.5-103.5-32-86-60.5-34-89 34-89 86-60.5 103.5-32 96.5-10.5q105 0 192 39v-537l-768 237v709q0 50-34 89t-86 60.5-103.5 32-96.5 10.5-96.5-10.5-103.5-32-86-60.5-34-89 34-89 86-60.5 103.5-32 96.5-10.5q105 0 192 39v-967q0-31 19-56.5t49-35.5l832-256q12-4 28-4 40 0 68 28t28 68z\"/></svg>");
			coverFile.close();
			return;
		}
		for (uint8_t i = 0u; i < length; i++) {
			mimeType[i] = coverFile.read();
		}
		mimeType[length] = '\0';

		length = 0;
		for (int i = 0; i < 4; ++i) { // length of description string
			length = (length << 8) | coverFile.read();
		}
		coverFile.seek(length + 16, SeekCur); // pass description, width, height, color depth, number of colors

		length = 0;
		for (int i = 0; i < 4; ++i) { // length of picture data
			length = (length << 8) | coverFile.read();
		}
		gPlayProperties.coverFileSize = length;
	} else {
		// test for M4A header
		coverFile.seek(8);
		coverFile.readBytes(fileType, 3);
		if (strncmp(fileType, "M4A", 3) == 0) {
			strcpy(mimeType, "application/octet-stream");
			coverFile.seek(gPlayProperties.coverFilePos);
		}
	}
	if (strncmp(mimeType, "image", 5) != 0 && strncmp(mimeType, "application/octet-stream", 24) != 0) {
		Log_Printf(LOGLEVEL_ERROR, "Unexpected MIME type (%s). Possible corrupted cover image or wrong coverFilePos (%u). Aborting extraction.", mimeType, gPlayProperties.coverFilePos);
		request->send(200, "image/svg+xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><svg width=\"1792\" height=\"1792\" viewBox=\"0 0 1792 1792\" transform=\"scale (0.6)\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M1664 224v1120q0 50-34 89t-86 60.5-103.5 32-96.5 10.5-96.5-10.5-103.5-32-86-60.5-34-89 34-89 86-60.5 103.5-32 96.5-10.5q105 0 192 39v-537l-768 237v709q0 50-34 89t-86 60.5-103.5 32-96.5 10.5-96.5-10.5-103.5-32-86-60.5-34-89 34-89 86-60.5 103.5-32 96.5-10.5q105 0 192 39v-967q0-31 19-56.5t49-35.5l832-256q12-4 28-4 40 0 68 28t28 68z\"/></svg>");
		coverFile.close();
		return;
	}
	Log_Printf(LOGLEVEL_NOTICE, "serve cover image (%s): %s", mimeType, gFSystem.name(coverFile).c_str());

	int imageSize = gPlayProperties.coverFileSize;
	AsyncWebServerResponse *response = request->beginChunkedResponse(mimeType, [coverFile, imageSize](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
		// some kind of webserver bug with actual size available, reduce the len
		if (maxLen > 1024) {
			maxLen = 1024;
		}
		File file = coverFile; // local copy of file pointer
		size_t leftToWrite = imageSize - index;
		if (!leftToWrite) {
			file.close();
			return 0; // end of transfer
		}
		size_t willWrite = (leftToWrite > maxLen) ? maxLen : leftToWrite;
		file.read(buffer, willWrite);
		index += willWrite;
		return willWrite;
	});
	response->addHeader("Cache-Control", "no-cache, must-revalidate");
	request->send(response);
}

// Handles Bluetooth scan requests
static void handleBluetoothScanRequest(AsyncWebServerRequest *request) {
#ifdef BLUETOOTH_ENABLE
	if (System_GetOperationMode() != OPMODE_BLUETOOTH_SOURCE) {
		request->send(400, "text/plain", "Bluetooth source mode not active.");
		return;
	}

	Bluetooth_StartScan();
	Web_SendWebsocketData(0, WebsocketCodeType::BluetoothScanInProgress);
	request->send(200);
#else
	request->send(400, "text/plain", "Bluetooth is not enabled.");
#endif
}

// Handles Bluetooth results request
static void handleBluetoothResultsRequest(AsyncWebServerRequest *request) {
#ifdef BLUETOOTH_ENABLE
	std::vector<ScannedBluetoothDevice> devices = Bluetooth_GetScannedDevices();

	JsonDocument doc;
	JsonArray jsonArray = doc.to<JsonArray>();

	for (const auto &device : devices) {
		JsonObject obj = jsonArray.add<JsonObject>();
		obj["name"] = device.name;
		char addr_str[18];
		sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
			device.address[0], device.address[1], device.address[2],
			device.address[3], device.address[4], device.address[5]);
		obj["address"] = addr_str;
		obj["rssi"] = device.rssi;
	}

	String output;
	serializeJson(doc, output);
	request->send(200, "application/json", output);
#else
	request->send(400, "text/plain", "Bluetooth is not enabled.");
#endif
}

// Handles Bluetooth connect requests
static void handleBluetoothConnectRequest(AsyncWebServerRequest *request, JsonVariant &json) {
#ifdef BLUETOOTH_ENABLE
	if (System_GetOperationMode() != OPMODE_BLUETOOTH_SOURCE) {
		request->send(400, "text/plain", "Bluetooth source mode not active.");
		return;
	}

	String addressStr = json["address"].as<String>();
	esp_bd_addr_t address;
	sscanf(addressStr.c_str(), "%hhX:%hhX:%hhX:%hhX:%hhX:%hhX",
		&address[0], &address[1], &address[2], &address[3], &address[4], &address[5]);
	Bluetooth_ConnectToAddress(address);
	request->send(200, "text/plain", "Connecting to device.");
#else
	request->send(400, "text/plain", "Bluetooth is not enabled.");
#endif
}
