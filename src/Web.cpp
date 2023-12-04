#include <Arduino.h>
#include "settings.h"

#include "Web.h"

#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "AudioPlayer.h"
#include "Battery.h"
#include "Cmd.h"
#include "Common.h"
#include "ESPAsyncWebServer.h"
#include "Ftp.h"
#include "HTMLbinary.h"
#include "HallEffectSensor.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "Mqtt.h"
#include "Rfid.h"
#include "SdCard.h"
#include "System.h"
#include "Wlan.h"
#include "freertos/ringbuf.h"
#include "revision.h"
#include "soc/timer_group_reg.h"
#include "soc/timer_group_struct.h"

#include <Update.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <nvsDump.h>

typedef struct {
	char nvsKey[13];
	char nvsEntry[275];
} nvs_t;

AsyncWebServer wServer(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

static bool webserverStarted = false;
static const uint32_t chunk_size = 16384; // bigger chunks increase write-performance to SD-Card
static const uint32_t nr_of_buffers = 2; // at least two buffers. No speed improvement yet with more than two.

uint8_t buffer[nr_of_buffers][chunk_size];
volatile uint32_t size_in_buffer[nr_of_buffers];
volatile bool buffer_full[nr_of_buffers];
uint32_t index_buffer_write = 0;
uint32_t index_buffer_read = 0;

static QueueHandle_t explorerFileUploadStatusQueue;
static TaskHandle_t fileStorageTaskHandle;

void Web_DumpSdToNvs(const char *_filename);
static void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
static void explorerHandleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
static void explorerHandleFileStorageTask(void *parameter);
static void explorerHandleListRequest(AsyncWebServerRequest *request);
static void explorerHandleDownloadRequest(AsyncWebServerRequest *request);
static void explorerHandleDeleteRequest(AsyncWebServerRequest *request);
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
static void handleWiFiScanRequest(AsyncWebServerRequest *request);
static void handleGetRFIDRequest(AsyncWebServerRequest *request);
static void handlePostRFIDRequest(AsyncWebServerRequest *request, JsonVariant &json);
static void handleDeleteRFIDRequest(AsyncWebServerRequest *request);
static void handleGetInfo(AsyncWebServerRequest *request);
static void handleGetSettings(AsyncWebServerRequest *request);
static void handlePostSettings(AsyncWebServerRequest *request, JsonVariant &json);

static void onWebsocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
static void settingsToJSON(JsonObject obj, const String section);
static bool JSONToSettings(JsonObject obj);
static void webserverStart(void);

// If PSRAM is available use it allocate memory for JSON-objects
struct SpiRamAllocator {
	void *allocate(size_t size) {
		return ps_malloc(size);
	}
	void deallocate(void *pointer) {
		free(pointer);
	}
};
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

static void serveProgmemFiles(const String &uri, const String &contentType, const uint8_t *content, size_t len) {
	wServer.on(uri.c_str(), HTTP_GET, [contentType, content, len](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response;

		// const bool etag = request->hasHeader("if-None-Match") && request->getHeader("if-None-Match")->value().equals(gitRevShort);
		const bool etag = false;
		if (etag) {
			response = request->beginResponse(304);
		} else {
			response = request->beginResponse_P(200, contentType, content, len);
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
	Led_SetPause(true); // Workaround to prevent exceptions due to Neopixel-signalisation while NVS-write
	esp_partition_iterator_t pi; // Iterator for find
	const esp_partition_t *nvs; // Pointer to partition struct
	esp_err_t result = ESP_OK;
	const char *partname = "nvs";
	uint8_t pagenr = 0; // Page number in NVS
	uint8_t i; // Index in Entry 0..125
	uint8_t bm; // Bitmap for an entry
	uint32_t offset = 0; // Offset in nvs partition
	uint8_t namespace_ID; // Namespace ID found

	pi = esp_partition_find(ESP_PARTITION_TYPE_DATA, // Get partition iterator for
		ESP_PARTITION_SUBTYPE_ANY, // this partition
		partname);
	if (pi) {
		nvs = esp_partition_get(pi); // Get partition struct
		esp_partition_iterator_release(pi); // Release the iterator
		Log_Printf(LOGLEVEL_DEBUG, "Partition %s found, %d bytes", partname, nvs->size);
	} else {
		Log_Printf(LOGLEVEL_ERROR, "Partition %s not found!", partname);
		return false;
	}
	namespace_ID = FindNsID(nvs, _namespace); // Find ID of our namespace in NVS
	while (offset < nvs->size) {
		result = esp_partition_read(nvs, offset, // Read 1 page in nvs partition
			&buf,
			sizeof(nvs_page));
		if (result != ESP_OK) {
			Log_Println("Error reading NVS!", LOGLEVEL_ERROR);
			return false;
		}

		i = 0;

		while (i < 126) {
			bm = (buf.Bitmap[i / 4] >> ((i % 4) * 2)) & 0x03; // Get bitmap for this entry
			if (bm == 2) {
				if ((namespace_ID == 0xFF) || // Show all if ID = 0xFF
					(buf.Entry[i].Ns == namespace_ID)) { // otherwise just my namespace
					if (isNumber(buf.Entry[i].Key)) {
						if (!callback(buf.Entry[i].Key, data)) {
							return false;
						}
					}
				}
				i += buf.Entry[i].Span; // Next entry
			} else {
				i++;
			}
		}
		offset += sizeof(nvs_page); // Prepare to read next page in nvs
		pagenr++;
	}
	Led_SetPause(false);

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
					response = request->beginResponse_P(200, "text/html", (const uint8_t *) management_BIN, sizeof(management_BIN));
					response->addHeader("Content-Encoding", "gzip");
#else
					if (gFSystem.exists("/.html/index.htm")) {
						response = request->beginResponse(gFSystem, "/.html/index.htm", "text/html", false);
					} else {
						response = request->beginResponse_P(200, "text/html", (const uint8_t *) management_BIN, sizeof(management_BIN));
						response->addHeader("Content-Encoding", "gzip");
					}
#endif
				} else {
					// serve accesspoint.html in AP-mode
					response = request->beginResponse_P(200, "text/html", (const uint8_t *) accesspoint_BIN, sizeof(accesspoint_BIN));
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
					vTaskSuspend(AudioTaskHandle);
					Led_TaskPause();
					Rfid_TaskPause();
					Update.begin();
					Log_Println(fwStart, LOGLEVEL_NOTICE);
				}

				Update.write(data, len);
				Log_Print(".", LOGLEVEL_NOTICE, false);

				if (final) {
					Update.end(true);
					// resume the paused tasks
					Led_TaskResume();
					vTaskResume(AudioTaskHandle);
					Rfid_TaskResume();
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
			request->send_P(200, "text/html", restartWebsite);
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
			response->println("</head><body>Tasklist:<div class='text'><pre>");
			// show tasklist
			char *pbuffer = (char *) calloc(2048, 1);
			vTaskList(pbuffer);
			response->println(pbuffer);
			response->println("</pre></div><br><br>Runtime statistics:<div class='text'><pre>");
			// show vTaskGetRunTimeStats()
			vTaskGetRunTimeStats(pbuffer);
			response->println(pbuffer);
			response->println("</pre></div></body></html>");
			free(pbuffer);
			// send the response last
			request->send(response);
		});
#endif

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
		wServer.addHandler(new AsyncCallbackJsonWebHandler("/rfid", handlePostRFIDRequest));
		wServer.addRewrite(new OneParamRewrite("/rfid/{id}", "/rfid?id={id}"));
		wServer.on("/rfid", HTTP_DELETE, handleDeleteRFIDRequest);

		// WiFi scan
		wServer.on("/wifiscan", HTTP_GET, handleWiFiScanRequest);

		// Fileexplorer (realtime)
		wServer.on("/explorer", HTTP_GET, explorerHandleListRequest);

		wServer.on(
			"/explorer", HTTP_POST, [](AsyncWebServerRequest *request) {
				request->send(200);
			},
			explorerHandleFileUpload);

		wServer.on("/explorerdownload", HTTP_GET, explorerHandleDownloadRequest);

		wServer.on("/explorer", HTTP_DELETE, explorerHandleDeleteRequest);

		wServer.on("/explorer", HTTP_PUT, explorerHandleCreateRequest);

		wServer.on("/explorer", HTTP_PATCH, explorerHandleRenameRequest);

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
			request->redirect("https://www.espuino.de/Espuino.webp");
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
bool JSONToSettings(JsonObject doc) {
	if (!doc) {
		Log_Println("JSONToSettings: doc unassigned", LOGLEVEL_DEBUG);
		return false;
	}
	if (doc.containsKey("general")) {
		// general settings
		if (gPrefsSettings.putUInt("initVolume", doc["general"]["initVolume"].as<uint8_t>()) == 0 || gPrefsSettings.putUInt("maxVolumeSp", doc["general"]["maxVolumeSp"].as<uint8_t>()) == 0 || gPrefsSettings.putUInt("maxVolumeHp", doc["general"]["maxVolumeHp"].as<uint8_t>()) == 0 || gPrefsSettings.putUInt("mInactiviyT", doc["general"]["sleepInactivity"].as<uint8_t>()) == 0) {
			Log_Println("Failed to save general settings", LOGLEVEL_ERROR);
			return false;
		}
	}
	if (doc.containsKey("wifi")) {
		// WiFi settings
		String hostName = doc["wifi"]["hostname"];
		if (!Wlan_ValidateHostname(hostName)) {
			Log_Println("Invalid hostname", LOGLEVEL_ERROR);
			return false;
		}
		if (((!Wlan_SetHostname(hostName)) || gPrefsSettings.putBool("ScanWiFiOnStart", doc["wifi"]["scanOnStart"].as<bool>()) == 0)) {
			Log_Println("Failed to save wifi settings", LOGLEVEL_ERROR);
			return false;
		}
	}
	if (doc.containsKey("led")) {
		// Neopixel settings
		if (gPrefsSettings.putUChar("iLedBrightness", doc["led"]["initBrightness"].as<uint8_t>()) == 0 || gPrefsSettings.putUChar("nLedBrightness", doc["led"]["nightBrightness"].as<uint8_t>()) == 0) {
			Log_Println("Failed to save LED settings", LOGLEVEL_ERROR);
			return false;
		}
	}
	if (doc.containsKey("battery")) {
		// Battery settings
		if (gPrefsSettings.putFloat("wLowVoltage", doc["battery"]["warnLowVoltage"].as<float>()) == 0 || gPrefsSettings.putFloat("vIndicatorLow", doc["battery"]["indicatorLow"].as<float>()) == 0 || gPrefsSettings.putFloat("vIndicatorHigh", doc["battery"]["indicatorHi"].as<float>()) == 0 || gPrefsSettings.putUInt("vCheckIntv", doc["battery"]["voltageCheckInterval"].as<uint8_t>()) == 0) {
			Log_Println("Failed to save battery settings", LOGLEVEL_ERROR);
			return false;
		}
		Battery_Init();
	}
	if (doc.containsKey("ftp")) {
		const char *_ftpUser = doc["ftp"]["username"];
		const char *_ftpPwd = doc["ftp"]["password"];

		gPrefsSettings.putString("ftpuser", (String) _ftpUser);
		gPrefsSettings.putString("ftppassword", (String) _ftpPwd);
		// Check if settings were written successfully
		if (!(String(_ftpUser).equals(gPrefsSettings.getString("ftpuser", "-1")) || String(_ftpPwd).equals(gPrefsSettings.getString("ftppassword", "-1")))) {
			Log_Println("Failed to save ftp settings", LOGLEVEL_ERROR);
			return false;
		}
	} else if (doc.containsKey("ftpStatus")) {
		uint8_t _ftpStart = doc["ftpStatus"]["start"].as<uint8_t>();
		if (_ftpStart == 1) { // ifdef FTP_ENABLE is checked in Ftp_EnableServer()
			Ftp_EnableServer();
		}
	}
	if (doc.containsKey("mqtt")) {
		uint8_t _mqttEnable = doc["mqtt"]["enable"].as<uint8_t>();
		const char *_mqttClientId = doc["mqtt"]["clientID"];
		const char *_mqttServer = doc["mqtt"]["server"];
		const char *_mqttUser = doc["mqtt"]["username"];
		const char *_mqttPwd = doc["mqtt"]["password"];
		uint16_t _mqttPort = doc["mqtt"]["port"].as<uint16_t>();

		gPrefsSettings.putUChar("enableMQTT", _mqttEnable);
		gPrefsSettings.putString("mqttClientId", (String) _mqttClientId);
		gPrefsSettings.putString("mqttServer", (String) _mqttServer);
		gPrefsSettings.putString("mqttUser", (String) _mqttUser);
		gPrefsSettings.putString("mqttPassword", (String) _mqttPwd);
		gPrefsSettings.putUInt("mqttPort", _mqttPort);

		if ((gPrefsSettings.getUChar("enableMQTT", 99) != _mqttEnable) || (!String(_mqttServer).equals(gPrefsSettings.getString("mqttServer", "-1")))) {
			Log_Println("Failed to save mqtt settings", LOGLEVEL_ERROR);
			return false;
		}
	}
	if (doc.containsKey("bluetooth")) {
		// bluetooth settings
		const char *_btDeviceName = doc["bluetooth"]["deviceName"];
		gPrefsSettings.putString("btDeviceName", (String) _btDeviceName);
		const char *btPinCode = doc["bluetooth"]["pinCode"];
		gPrefsSettings.putString("btPinCode", (String) btPinCode);
		// Check if settings were written successfully
		if (gPrefsSettings.getString("btDeviceName", "") != _btDeviceName || gPrefsSettings.getString("btPinCode", "") != btPinCode) {
			Log_Println("Failed to save bluetooth settings", LOGLEVEL_ERROR);
			return false;
		}
	} else if (doc.containsKey("rfidMod")) {
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
				return false;
			}
		}
		Web_DumpNvsToSd("rfidTags", backupFile); // Store backup-file every time when a new rfid-tag is programmed
	} else if (doc.containsKey("rfidAssign")) {
		const char *_rfidIdAssinId = doc["rfidAssign"]["rfidIdMusic"];
		char _fileOrUrlAscii[MAX_FILEPATH_LENTGH];
		convertFilenameToAscii(doc["rfidAssign"]["fileOrUrl"], _fileOrUrlAscii);
		uint8_t _playMode = doc["rfidAssign"]["playMode"];
		if (_playMode <= 0) {
			Log_Println("rfidAssign: Invalid playmode", LOGLEVEL_ERROR);
			return false;
		}
		char rfidString[275];
		snprintf(rfidString, sizeof(rfidString) / sizeof(rfidString[0]), "%s%s%s0%s%u%s0", stringDelimiter, _fileOrUrlAscii, stringDelimiter, stringDelimiter, _playMode, stringDelimiter);
		gPrefsRfid.putString(_rfidIdAssinId, rfidString);
#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
		Rfid_ResetOldRfid(); // Set old rfid-id to crap in order to allow to re-apply a new assigned rfid-tag exactly once
#endif

		String s = gPrefsRfid.getString(_rfidIdAssinId, "-1");
		if (s.compareTo(rfidString)) {
			return false;
		}
		Web_DumpNvsToSd("rfidTags", backupFile); // Store backup-file every time when a new rfid-tag is programmed
	} else if (doc.containsKey("ping")) {
		if ((millis() - lastPongTimestamp) > 1000u) {
			// send pong (keep-alive heartbeat), check for excessive calls
			lastPongTimestamp = millis();
			Web_SendWebsocketData(0, 20);
		}
		return false;
	} else if (doc.containsKey("controls")) {
		if (doc["controls"].containsKey("set_volume")) {
			uint8_t new_vol = doc["controls"]["set_volume"].as<uint8_t>();
			AudioPlayer_VolumeToQueueSender(new_vol, true);
		}
		if (doc["controls"].containsKey("action")) {
			uint8_t cmd = doc["controls"]["action"].as<uint8_t>();
			Cmd_Action(cmd);
		}
	} else if (doc.containsKey("trackinfo")) {
		Web_SendWebsocketData(0, 30);
	} else if (doc.containsKey("coverimg")) {
		Web_SendWebsocketData(0, 40);
	} else if (doc.containsKey("volume")) {
		Web_SendWebsocketData(0, 50);
	} else if (doc.containsKey("settings")) {
		Web_SendWebsocketData(0, 60);
	} else if (doc.containsKey("ssids")) {
		Web_SendWebsocketData(0, 70);
	} else if (doc.containsKey("trackProgress")) {
		if (doc["trackProgress"].containsKey("posPercent")) {
			gPlayProperties.seekmode = SEEK_POS_PERCENT;
			gPlayProperties.currentRelPos = doc["trackProgress"]["posPercent"].as<uint8_t>();
		}
		Web_SendWebsocketData(0, 80);
	}

	return true;
}

// process settings to JSON object
static void settingsToJSON(JsonObject obj, const String section) {
	if ((section == "") || (section == "current")) {
		// current values
		JsonObject curObj = obj.createNestedObject("current");
		curObj["volume"].set(AudioPlayer_GetCurrentVolume());
		curObj["rfidTagId"] = String(gCurrentRfidTagId);
	}
	if ((section == "") || (section == "general")) {
		// general settings
		JsonObject generalObj = obj.createNestedObject("general");
		generalObj["initVolume"].set(gPrefsSettings.getUInt("initVolume", 0));
		generalObj["maxVolumeSp"].set(gPrefsSettings.getUInt("maxVolumeSp", 0));
		generalObj["maxVolumeHp"].set(gPrefsSettings.getUInt("maxVolumeHp", 0));
		generalObj["sleepInactivity"].set(gPrefsSettings.getUInt("mInactiviyT", 0));
	}
	if ((section == "") || (section == "wifi")) {
		// WiFi settings
		JsonObject wifiObj = obj.createNestedObject("wifi");
		wifiObj["hostname"] = Wlan_GetHostname();
		wifiObj["scanOnStart"].set(gPrefsSettings.getBool("ScanWiFiOnStart", false));
	}
	if (section == "ssids") {
		// saved SSID's
		JsonObject ssidsObj = obj.createNestedObject("ssids");
		JsonArray ssidArr = ssidsObj.createNestedArray("savedSSIDs");
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
		JsonObject ledObj = obj.createNestedObject("led");
		ledObj["initBrightness"].set(gPrefsSettings.getUChar("iLedBrightness", 0));
		ledObj["nightBrightness"].set(gPrefsSettings.getUChar("nLedBrightness", 0));
	}
#endif
#ifdef MEASURE_BATTERY_VOLTAGE
	if ((section == "") || (section == "battery")) {
		// battery settings
		JsonObject batteryObj = obj.createNestedObject("battery");
		batteryObj["warnLowVoltage"].set(gPrefsSettings.getFloat("wLowVoltage", s_warningLowVoltage));
		batteryObj["indicatorLow"].set(gPrefsSettings.getFloat("vIndicatorLow", s_voltageIndicatorLow));
		batteryObj["indicatorHi"].set(gPrefsSettings.getFloat("vIndicatorHigh", s_voltageIndicatorHigh));
		batteryObj["voltageCheckInterval"].set(gPrefsSettings.getUInt("vCheckIntv", s_batteryCheckInterval));
	}
#endif
	if (section == "defaults") {
		// default factory settings
		JsonObject defaultsObj = obj.createNestedObject("defaults");
		defaultsObj["initVolume"].set(3u); // AUDIOPLAYER_VOLUME_INIT
		defaultsObj["maxVolumeSp"].set(21u); // AUDIOPLAYER_VOLUME_MAX
		defaultsObj["maxVolumeHp"].set(18u); // gPrefsSettings.getUInt("maxVolumeHp", 0));
		defaultsObj["sleepInactivity"].set(10u); // System_MaxInactivityTime
#ifdef NEOPIXEL_ENABLE
		defaultsObj["initBrightness"].set(16u); // LED_INITIAL_BRIGHTNESS
		defaultsObj["nightBrightness"].set(2u); // LED_INITIAL_NIGHT_BRIGHTNESS
#endif
#ifdef MEASURE_BATTERY_VOLTAGE
		defaultsObj["warnLowVoltage"].set(s_warningLowVoltage);
		defaultsObj["indicatorLow"].set(s_voltageIndicatorLow);
		defaultsObj["indicatorHi"].set(s_voltageIndicatorHigh);
		defaultsObj["voltageCheckInterval"].set(s_batteryCheckInterval);
#endif
	}
// FTP
#ifdef FTP_ENABLE
	if ((section == "") || (section == "ftp")) {
		JsonObject ftpObj = obj.createNestedObject("ftp");
		ftpObj["username"] = gPrefsSettings.getString("ftpuser", "-1");
		ftpObj["password"] = gPrefsSettings.getString("ftppassword", "-1");
		ftpObj["maxUserLength"].set(ftpUserLength - 1);
		ftpObj["maxPwdLength"].set(ftpUserLength - 1);
	}
#endif
// MQTT
#ifdef MQTT_ENABLE
	if ((section == "") || (section == "mqtt")) {
		JsonObject mqttObj = obj.createNestedObject("mqtt");
		mqttObj["enable"].set(Mqtt_IsEnabled());
		mqttObj["clientID"] = gPrefsSettings.getString("mqttClientId", "-1");
		mqttObj["server"] = gPrefsSettings.getString("mqttServer", "-1");
		mqttObj["port"].set(gPrefsSettings.getUInt("mqttPort", 0));
		mqttObj["username"] = gPrefsSettings.getString("mqttUser", "-1");
		mqttObj["password"] = gPrefsSettings.getString("mqttPassword", "-1");
		mqttObj["maxUserLength"].set(mqttUserLength - 1);
		mqttObj["maxPwdLength"].set(mqttPasswordLength - 1);
		mqttObj["maxClientIdLength"].set(mqttClientIdLength - 1);
		mqttObj["maxServerLength"].set(mqttServerLength - 1);
	}
#endif
// Bluetooth
#ifdef BLUETOOTH_ENABLE
	if ((section == "") || (section == "bluetooth")) {
		JsonObject btObj = obj.createNestedObject("bluetooth");
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
#ifdef BOARD_HAS_PSRAM
	SpiRamJsonDocument doc(512);
#else
	StaticJsonDocument<512> doc;
#endif
	JsonObject infoObj = doc.createNestedObject("info");
	// software
	if ((section == "") || (section == "software")) {
		JsonObject softwareObj = infoObj.createNestedObject("software");
		softwareObj["version"] = (String) softwareRevision;
		softwareObj["git"] = (String) gitRevision;
		softwareObj["arduino"] = String(ESP_ARDUINO_VERSION_MAJOR) + "." + String(ESP_ARDUINO_VERSION_MINOR) + "." + String(ESP_ARDUINO_VERSION_PATCH);
		softwareObj["idf"] = String(ESP.getSdkVersion());
	}
	// hardware
	if ((section == "") || (section == "hardware")) {
		JsonObject hardwareObj = infoObj.createNestedObject("hardware");
		hardwareObj["model"] = String(ESP.getChipModel());
		hardwareObj["revision"] = ESP.getChipRevision();
		hardwareObj["freq"] = ESP.getCpuFreqMHz();
	}
	// memory
	if ((section == "") || (section == "memory")) {
		JsonObject memoryObj = infoObj.createNestedObject("memory");
		memoryObj["freeHeap"] = ESP.getFreeHeap();
		memoryObj["largestFreeBlock"] = (uint32_t) heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
		if (psramFound()) {
			memoryObj["freePSRam"] = ESP.getFreePsram();
		}
	}
	// wifi
	if ((section == "") || (section == "wifi")) {
		JsonObject wifiObj = infoObj.createNestedObject("wifi");
		wifiObj["ip"] = Wlan_GetIpAddress();
		wifiObj["rssi"] = (int8_t) Wlan_GetRssi();
	}
	// audio
	if ((section == "") || (section == "audio")) {
		JsonObject audioObj = infoObj.createNestedObject("audio");
		audioObj["playtimeTotal"] = AudioPlayer_GetPlayTimeAllTime();
		audioObj["playtimeSinceStart"] = AudioPlayer_GetPlayTimeSinceStart();
		audioObj["firstStart"] = gPrefsSettings.getULong("firstStart", 0);
	}
#ifdef BATTERY_MEASURE_ENABLE
	// battery
	if ((section == "") || (section == "battery")) {
		JsonObject batteryObj = infoObj.createNestedObject("battery");
		batteryObj["currVoltage"] = Battery_GetVoltage();
		batteryObj["chargeLevel"] = Battery_EstimateLevel() * 100;
	}
#endif
#ifdef HALLEFFECT_SENSOR_ENABLE
	if ((section == "") || (section == "hallsensor")) {
		// hallsensor
		JsonObject hallObj = infoObj.createNestedObject("hallsensor");
		uint16_t sva = gHallEffectSensor.readSensorValueAverage(true);
		int diff = sva - gHallEffectSensor.NullFieldValue();

		hallObj["nullFieldValue"] = gHallEffectSensor.NullFieldValue();
		hallObj["actual"] = sva;
		hallObj["diff"] = diff;
		hallObj["lastWaitState"] = gHallEffectSensor.LastWaitForState();
		hallObj["lastWaitMS"] = gHallEffectSensor.LastWaitForStateMS();
	}
#endif

	String serializedJsonString;
	serializeJson(infoObj, serializedJsonString);
	request->send(200, "application/json; charset=utf-8", serializedJsonString);
	System_UpdateActivityTimer();
}

// handle get settings
void handleGetSettings(AsyncWebServerRequest *request) {

	// param to get a single settings section
	String section = "";
	if (request->hasParam("section")) {
		section = request->getParam("section")->value();
	}
#ifdef BOARD_HAS_PSRAM
	SpiRamJsonDocument doc(8192);
#else
	StaticJsonDocument<8192> doc;
#endif
	JsonObject settingsObj = doc.createNestedObject("settings");
	settingsToJSON(settingsObj, section);
	String serializedJsonString;
	serializeJson(settingsObj, serializedJsonString);
	request->send(200, "application/json; charset=utf-8", serializedJsonString);
}

// handle post settings
void handlePostSettings(AsyncWebServerRequest *request, JsonVariant &json) {
	const JsonObject &jsonObj = json.as<JsonObject>();
	bool succ = JSONToSettings(jsonObj);
	if (succ) {
		request->send(200);
	} else {
		request->send(500, "text/plain; charset=utf-8", "error saving settings");
	}
}

// Takes inputs from webgui, parses JSON and saves values in NVS
// If operation was successful (NVS-write is verified) true is returned
bool processJsonRequest(char *_serialJson) {
	if (!_serialJson) {
		return false;
	}
#ifdef BOARD_HAS_PSRAM
	SpiRamJsonDocument doc(1000);
#else
	StaticJsonDocument<1000> doc;
#endif

	DeserializationError error = deserializeJson(doc, _serialJson);

	if (error) {
		Log_Printf(LOGLEVEL_ERROR, jsonErrorMsg, error.c_str());
		return false;
	}

	JsonObject obj = doc.as<JsonObject>();
	return JSONToSettings(obj);
}

// Sends JSON-answers via websocket
void Web_SendWebsocketData(uint32_t client, uint8_t code) {
	if (!webserverStarted) {
		// webserver not yet started
		return;
	}
	if (ws.count() == 0) {
		// we do not have any webclient connected
		return;
	}
	// check if we can send message to the client(s)
	if (client == 0) {
		if (!ws.availableForWriteAll()) {
			Log_Println("Websocket: Cannot send data (Too many messages queued)!", LOGLEVEL_ERROR);
			return;
		}
	} else {
		if (!ws.availableForWrite(client)) {
			Log_Printf(LOGLEVEL_ERROR, "Websocket: Cannot send data to client %d (Too many messages queued)!", client);
			return;
		}
	}
	char *jBuf = (char *) x_calloc(1024, sizeof(char));
	StaticJsonDocument<1024> doc;
	JsonObject object = doc.to<JsonObject>();

	if (code == 1) {
		object["status"] = "ok";
	} else if (code == 2) {
		object["status"] = "error";
	} else if (code == 10) {
		object["rfidId"] = gCurrentRfidTagId;
	} else if (code == 20) {
		object["pong"] = "pong";
		object["rssi"] = Wlan_GetRssi();
		// todo: battery percent + loading status +++
		// object["battery"] = Battery_GetVoltage();
	} else if (code == 30) {
		JsonObject entry = object.createNestedObject("trackinfo");
		entry["pausePlay"] = gPlayProperties.pausePlay;
		entry["currentTrackNumber"] = gPlayProperties.currentTrackNumber + 1;
		entry["numberOfTracks"] = gPlayProperties.numberOfTracks;
		entry["volume"] = AudioPlayer_GetCurrentVolume();
		entry["name"] = gPlayProperties.title;
		entry["posPercent"] = gPlayProperties.currentRelPos;
		entry["playMode"] = gPlayProperties.playMode;
	} else if (code == 40) {
		object["coverimg"] = "coverimg";
	} else if (code == 50) {
		object["volume"] = AudioPlayer_GetCurrentVolume();
	} else if (code == 60) {
		JsonObject entry = object.createNestedObject("settings");
		settingsToJSON(entry, "");
	} else if (code == 70) {
		JsonObject entry = object.createNestedObject("settings");
		settingsToJSON(entry, "ssids");
	} else if (code == 80) {
		JsonObject entry = object.createNestedObject("trackProgress");
		entry["posPercent"] = gPlayProperties.currentRelPos;
		entry["time"] = AudioPlayer_GetCurrentTime();
		entry["duration"] = AudioPlayer_GetFileDuration();
	};

	serializeJson(doc, jBuf, 1024);

	if (client == 0) {
		ws.printfAll(jBuf);
	} else {
		ws.printf(client, jBuf);
	}
	free(jBuf);
}

// Processes websocket-requests
void onWebsocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {

	if (type == WS_EVT_CONNECT) {
		// client connected
		Log_Printf(LOGLEVEL_DEBUG, "ws[%s][%u] connect", server->url(), client->id());
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

			if (processJsonRequest((char *) data)) {
				if (data && (strncmp((char *) data, "track", 5))) { // Don't send back ok-feedback if track's name is requested in background
					Web_SendWebsocketData(client->id(), 1);
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

void explorerCreateParentDirectories(const char *filePath) {
	char tmpPath[MAX_FILEPATH_LENTGH];
	char *rest;

	rest = strchr(filePath, '/');
	while (rest) {
		if (rest - filePath != 0) {
			memcpy(tmpPath, filePath, rest - filePath);
			tmpPath[rest - filePath] = '\0';
			if (!gFSystem.exists(tmpPath)) {
				Log_Printf(LOGLEVEL_DEBUG, "creating dir \"%s\"\n", tmpPath);
				gFSystem.mkdir(tmpPath);
			}
		}
		rest = strchr(rest + 1, '/');
	}
}

// Handles file upload request from the explorer
// requires a GET parameter path, as directory path to the file
void explorerHandleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {

	System_UpdateActivityTimer();

	// New File
	if (!index) {
		String utf8Folder = "/";
		String utf8FilePath;
		static char filePath[MAX_FILEPATH_LENTGH];
		if (request->hasParam("path")) {
			const AsyncWebParameter *param = request->getParam("path");
			utf8Folder = param->value() + "/";
		}
		utf8FilePath = utf8Folder + filename;

		convertFilenameToAscii(utf8FilePath, filePath);

		Log_Printf(LOGLEVEL_INFO, writingFile, utf8FilePath.c_str());

		// Create Parent directories
		explorerCreateParentDirectories(filePath);

		// Create Queue for receiving a signal from the store task as synchronisation
		if (explorerFileUploadStatusQueue == NULL) {
			explorerFileUploadStatusQueue = xQueueCreate(1, sizeof(uint8_t));
		}

		// reset buffers
		index_buffer_write = 0;
		index_buffer_read = 0;
		for (uint32_t i = 0; i < nr_of_buffers; i++) {
			size_in_buffer[i] = 0;
			buffer_full[i] = false;
		}

		// Create Task for handling the storage of the data
		xTaskCreatePinnedToCore(
			explorerHandleFileStorageTask, /* Function to implement the task */
			"fileStorageTask", /* Name of the task */
			4000, /* Stack size in words */
			filePath, /* Task input parameter */
			2 | portPRIVILEGE_BIT, /* Priority of the task */
			&fileStorageTaskHandle, /* Task handle. */
			1 /* Core where the task should run */
		);
	}

	if (len) {
		// wait till buffer is ready
		while (buffer_full[index_buffer_write]) {
			vTaskDelay(2u);
		}

		size_t len_to_write = len;
		size_t space_left = chunk_size - size_in_buffer[index_buffer_write];
		if (space_left < len_to_write) {
			len_to_write = space_left;
		}
		// write content to buffer
		memcpy(buffer[index_buffer_write] + size_in_buffer[index_buffer_write], data, len_to_write);
		size_in_buffer[index_buffer_write] += len_to_write;

		// check if buffer is filled. If full, signal that ready and change buffers
		if (size_in_buffer[index_buffer_write] == chunk_size) {
			// signal, that buffer is ready. Increment index
			buffer_full[index_buffer_write] = true;
			index_buffer_write = (index_buffer_write + 1) % nr_of_buffers;

			// if still content left, put it into next buffer
			if (len_to_write < len) {
				// wait till new buffer is ready
				while (buffer_full[index_buffer_write]) {
					vTaskDelay(2u);
				}
				size_t len_left_to_write = len - len_to_write;
				memcpy(buffer[index_buffer_write], data + len_to_write, len_left_to_write);
				size_in_buffer[index_buffer_write] = len_left_to_write;
			}
		}
	}

	if (final) {
		// if file not completely done yet, signal that buffer is filled
		if (size_in_buffer[index_buffer_write] > 0) {
			buffer_full[index_buffer_write] = true;
		}
		// notify storage task that last data was stored on the ring buffer
		xTaskNotify(fileStorageTaskHandle, 1u, eNoAction);
		// watit until the storage task is sending the signal to finish
		uint8_t signal;
		xQueueReceive(explorerFileUploadStatusQueue, &signal, portMAX_DELAY);
	}
}

// feed the watchdog timer without delay
void feedTheDog(void) {
#if defined(SD_MMC_1BIT_MODE) && defined(CONFIG_IDF_TARGET_ESP32)
	// feed dog 0
	TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE; // write enable
	TIMERG0.wdt_feed = 1; // feed dog
	TIMERG0.wdt_wprotect = 0; // write protect
	// feed dog 1
	TIMERG1.wdt_wprotect = TIMG_WDT_WKEY_VALUE; // write enable
	TIMERG1.wdt_feed = 1; // feed dog
	TIMERG1.wdt_wprotect = 0; // write protect
#else
	// Without delay upload-feature is broken for SD via SPI (for whatever reason...)
	vTaskDelay(portTICK_PERIOD_MS * 11);
#endif
}

void explorerHandleFileStorageTask(void *parameter) {
	File uploadFile;
	size_t bytesOk = 0;
	size_t bytesNok = 0;
	uint32_t chunkCount = 0;
	uint32_t transferStartTimestamp = millis();
	uint8_t value = 0;
	uint32_t lastUpdateTimestamp = millis();
	uint32_t maxUploadDelay = 20; // After this delay (in seconds) task will be deleted as transfer is considered to be finally broken

	BaseType_t uploadFileNotification;
	uint32_t uploadFileNotificationValue;
	uploadFile = gFSystem.open((char *) parameter, "w");
	uploadFile.setBufferSize(chunk_size);

	// pause some tasks to get more free CPU time for the upload
	vTaskSuspend(AudioTaskHandle);
	Led_TaskPause();
	Rfid_TaskPause();

	for (;;) {
		// check buffer is full with enough data or all data already sent
		uploadFileNotification = xTaskNotifyWait(0, 0, &uploadFileNotificationValue, 0);
		if ((buffer_full[index_buffer_read]) || (uploadFileNotification == pdPASS)) {

			while (buffer_full[index_buffer_read]) {
				chunkCount++;
				size_t item_size = size_in_buffer[index_buffer_read];
				if (!uploadFile.write(buffer[index_buffer_read], item_size)) {
					bytesNok += item_size;
					feedTheDog();
				} else {
					bytesOk += item_size;
				}
				// update handling of buffers
				size_in_buffer[index_buffer_read] = 0;
				buffer_full[index_buffer_read] = 0;
				index_buffer_read = (index_buffer_read + 1) % nr_of_buffers;
				// update timestamp
				lastUpdateTimestamp = millis();
			}

			if (uploadFileNotification == pdPASS) {
				uploadFile.close();
				Log_Printf(LOGLEVEL_INFO, fileWritten, (char *) parameter, bytesNok + bytesOk, (millis() - transferStartTimestamp), (bytesNok + bytesOk) / (millis() - transferStartTimestamp));
				Log_Printf(LOGLEVEL_DEBUG, "Bytes [ok] %zu / [not ok] %zu, Chunks: %zu\n", bytesOk, bytesNok, chunkCount);
				// done exit loop to terminate
				break;
			}
		} else {
			if (lastUpdateTimestamp + maxUploadDelay * 1000 < millis()) {
				Log_Println(webTxCanceled, LOGLEVEL_ERROR);
				// resume the paused tasks
				Led_TaskResume();
				vTaskResume(AudioTaskHandle);
				Rfid_TaskResume();
				// just delete task without signaling (abort)
				vTaskDelete(NULL);
				return;
			}
			vTaskDelay(portTICK_PERIOD_MS * 2);
			continue;
		}
	}
	// resume the paused tasks
	Led_TaskResume();
	vTaskResume(AudioTaskHandle);
	Rfid_TaskResume();
	// send signal to upload function to terminate
	xQueueSend(explorerFileUploadStatusQueue, &value, 0);
	vTaskDelete(NULL);
}

// Sends a list of the content of a directory as JSON file
// requires a GET parameter path for the directory
void explorerHandleListRequest(AsyncWebServerRequest *request) {
#ifdef NO_SDCARD
	request->send(200, "application/json; charset=utf-8", "[]"); // maybe better to send 404 here?
	return;
#endif
#ifdef BOARD_HAS_PSRAM
	SpiRamJsonDocument jsonBuffer(65636);
#else
	StaticJsonDocument<8192> jsonBuffer;
#endif

	String serializedJsonString;
	char filePath[MAX_FILEPATH_LENTGH];
	JsonArray obj = jsonBuffer.createNestedArray();
	File root;
	if (request->hasParam("path")) {
		AsyncWebParameter *param;
		param = request->getParam("path");
		convertFilenameToAscii(param->value(), filePath);
		root = gFSystem.open(filePath);
	} else {
		root = gFSystem.open("/");
	}

	if (!root) {
		Log_Println(failedToOpenDirectory, LOGLEVEL_DEBUG);
		return;
	}

	if (!root.isDirectory()) {
		Log_Println(notADirectory, LOGLEVEL_DEBUG);
		return;
	}

	bool isDir = false;
	String MyfileName = root.getNextFileName(&isDir);
	while (MyfileName != "") {
		// ignore hidden folders, e.g. MacOS spotlight files
		if (!MyfileName.startsWith("/.")) {
			JsonObject entry = obj.createNestedObject();
			entry["name"] = MyfileName.substring(MyfileName.lastIndexOf('/') + 1);
			if (isDir) {
				entry["dir"].set(true);
			}
		}
		MyfileName = root.getNextFileName(&isDir);
	}
	root.close();

	serializeJson(obj, serializedJsonString);
	request->send(200, "application/json; charset=utf-8", serializedJsonString);
}

bool explorerDeleteDirectory(File dir) {

	File file = dir.openNextFile();
	while (file) {

		if (file.isDirectory()) {
			explorerDeleteDirectory(file);
		} else {
			gFSystem.remove(file.path());
		}

		file = dir.openNextFile();

		esp_task_wdt_reset();
	}

	return gFSystem.rmdir(dir.path());
}

// Handles download request of a file
// requires a GET parameter path to the file
void explorerHandleDownloadRequest(AsyncWebServerRequest *request) {
	File file;
	AsyncWebParameter *param;
	char filePath[MAX_FILEPATH_LENTGH];
	// check has path param
	if (!request->hasParam("path")) {
		Log_Println("DOWNLOAD: No path variable set", LOGLEVEL_ERROR);
		request->send(404);
		return;
	}
	// check file exists on SD card
	param = request->getParam("path");
	convertFilenameToAscii(param->value(), filePath);
	if (!gFSystem.exists(filePath)) {
		Log_Printf(LOGLEVEL_ERROR, "DOWNLOAD:  File not found on SD card: %s", param->value().c_str());
		request->send(404);
		return;
	}
	// check is file and not a directory
	file = gFSystem.open(filePath);
	if (file.isDirectory()) {
		Log_Printf(LOGLEVEL_ERROR, "DOWNLOAD:  Cannot download a directory %s", param->value().c_str());
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
	char filePath[MAX_FILEPATH_LENTGH];
	if (request->hasParam("path")) {
		AsyncWebParameter *param;
		param = request->getParam("path");
		convertFilenameToAscii(param->value(), filePath);
		if (gFSystem.exists(filePath)) {
			// stop playback, file to delete might be in use
			Cmd_Action(CMD_STOP);
			file = gFSystem.open(filePath);
			if (file.isDirectory()) {
				if (explorerDeleteDirectory(file)) {
					Log_Printf(LOGLEVEL_INFO, "DELETE:  %s deleted", param->value().c_str());
				} else {
					Log_Printf(LOGLEVEL_ERROR, "DELETE:  Cannot delete %s", param->value().c_str());
				}
			} else {
				const String cPath = filePath;
				if (gFSystem.remove(filePath)) {
					Log_Printf(LOGLEVEL_INFO, "DELETE:  %s deleted", param->value().c_str());
				} else {
					Log_Printf(LOGLEVEL_ERROR, "DELETE:  Cannot delete %s", param->value().c_str());
				}
			}
		} else {
			Log_Printf(LOGLEVEL_ERROR, "DELETE:  Path %s does not exist", param->value().c_str());
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
		AsyncWebParameter *param;
		char filePath[MAX_FILEPATH_LENTGH];
		param = request->getParam("path");
		convertFilenameToAscii(param->value(), filePath);
		if (gFSystem.mkdir(filePath)) {
			Log_Printf(LOGLEVEL_INFO, "CREATE:  %s created", param->value().c_str());
		} else {
			Log_Printf(LOGLEVEL_ERROR, "CREATE:  Cannot create %s", param->value().c_str());
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
		AsyncWebParameter *srcPath;
		AsyncWebParameter *dstPath;
		char srcFullFilePath[MAX_FILEPATH_LENTGH];
		char dstFullFilePath[MAX_FILEPATH_LENTGH];
		srcPath = request->getParam("srcpath");
		dstPath = request->getParam("dstpath");
		convertFilenameToAscii(srcPath->value(), srcFullFilePath);
		convertFilenameToAscii(dstPath->value(), dstFullFilePath);
		if (gFSystem.exists(srcFullFilePath)) {
			if (gFSystem.rename(srcFullFilePath, dstFullFilePath)) {
				Log_Printf(LOGLEVEL_INFO, "RENAME:  %s renamed to %s", srcPath->value().c_str(), dstPath->value().c_str());
			} else {
				Log_Printf(LOGLEVEL_ERROR, "RENAME:  Cannot rename %s", srcPath->value().c_str());
			}
		} else {
			Log_Printf(LOGLEVEL_ERROR, "RENAME: Path %s does not exist", srcPath->value().c_str());
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
	AsyncWebParameter *param;
	String playModeString;
	uint32_t playMode;
	if (request->hasParam("path") && request->hasParam("playmode")) {
		param = request->getParam("path");
		char filePath[MAX_FILEPATH_LENTGH];
		convertFilenameToAscii(param->value(), filePath);
		param = request->getParam("playmode");
		playModeString = param->value();

		playMode = atoi(playModeString.c_str());
		AudioPlayer_TrackQueueDispatcher(filePath, 0, playMode, 0);
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
	AsyncJsonResponse *response = new AsyncJsonResponse(true);
	JsonArray json_ssids = response->getRoot();
	Wlan_GetSavedNetworks([json_ssids](const WiFiSettings &network) {
		json_ssids.add(network.ssid);
	});

	response->setLength();
	request->send(response);
}

void handlePostSavedSSIDs(AsyncWebServerRequest *request, JsonVariant &json) {
	const JsonObject &jsonObj = json.as<JsonObject>();

	struct WiFiSettings networkSettings;

	// TODO: we truncate ssid and password, which is better than not checking at all, but still silently failing
	strncpy(networkSettings.ssid, (const char *) jsonObj["ssid"], 32);
	networkSettings.ssid[32] = '\0';
	strncpy(networkSettings.password, (const char *) jsonObj["pwd"], 64);
	networkSettings.password[64] = '\0';

	networkSettings.use_static_ip = (bool) jsonObj["static"];

	if (jsonObj.containsKey("static_addr")) {
		networkSettings.static_addr = (uint32_t) IPAddress().fromString((const char *) jsonObj["static_addr"]);
	}
	if (jsonObj.containsKey("static_gateway")) {
		networkSettings.static_gateway = (uint32_t) IPAddress().fromString((const char *) jsonObj["static_gateway"]);
	}
	if (jsonObj.containsKey("static_subnet")) {
		networkSettings.static_subnet = (uint32_t) IPAddress().fromString((const char *) jsonObj["static_subnet"]);
	}
	if (jsonObj.containsKey("static_dns1")) {
		networkSettings.static_dns1 = (uint32_t) IPAddress().fromString((const char *) jsonObj["static_dns1"]);
	}
	if (jsonObj.containsKey("static_dns2")) {
		networkSettings.static_dns2 = (uint32_t) IPAddress().fromString((const char *) jsonObj["static_dns2"]);
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
	String s = gPrefsRfid.getString(tagId.c_str(), "-1"); // Try to lookup rfidId in NVS
	if (!s.compareTo("-1")) {
		return false;
	}
	char _file[255];
	uint32_t _lastPlayPos = 0;
	uint16_t _trackLastPlayed = 0;
	uint32_t _mode = 1;
	char *token;
	uint8_t i = 1;
	token = strtok((char *) s.c_str(), stringDelimiter);
	while (token != NULL) { // Try to extract data from string after lookup
		if (i == 1) {
			strncpy(_file, token, sizeof(_file) / sizeof(_file[0]));
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

// callback for writing a NVS entry to JSON
bool DumpNvsToJSONCallback(const char *key, void *data) {
	JsonArray *myArr = (JsonArray *) data;
	JsonObject obj = myArr->createNestedObject();
	return tagIdToJSON(key, obj);
}

static void handleGetRFIDRequest(AsyncWebServerRequest *request) {
	String tagId = "";
	if (request->hasParam("id")) {
		tagId = request->getParam("id")->value();
	}
	if (tagId == "") {
		// return all RFID assignments
		AsyncJsonResponse *response = new AsyncJsonResponse(true, 8192);
		JsonArray Arr = response->getRoot();
		if (listNVSKeys("rfidTags", &Arr, DumpNvsToJSONCallback)) {
			response->setLength();
			request->send(response);
		} else {
			request->send(500, "error reading entries from NVS");
		}
	} else {
		if (gPrefsRfid.isKey(tagId.c_str())) {
			// return single RFID assignment
			AsyncJsonResponse *response = new AsyncJsonResponse(false);
			JsonObject obj = response->getRoot();
			tagIdToJSON(tagId, obj);
			response->setLength();
			request->send(response);
		} else {
			// RFID assignment not found
			request->send(404);
		}
	}
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
	char _fileOrUrlAscii[MAX_FILEPATH_LENTGH];
	convertFilenameToAscii(fileOrUrl, _fileOrUrlAscii);
	uint8_t _playModeOrModId;
	if (jsonObj.containsKey("modId")) {
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
					memcpy(nvsEntry[0].nvsKey, token, strlen(token));
					nvsEntry[0].nvsKey[strlen(token)] = '\0';
				} else {
					count = false;
					if (isUtf8) {
						memcpy(nvsEntry[0].nvsEntry, token, strlen(token));
						nvsEntry[0].nvsEntry[strlen(token)] = '\0';
					} else {
						convertAsciiToUtf8(String(token), nvsEntry[0].nvsEntry);
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

	if (!gPlayProperties.coverFilePos) {
		// empty image:
		// request->send(200, "image/svg+xml", "<?xml version=\"1.0\"?><svg xmlns=\"http://www.w3.org/2000/svg\"/>");
		if (gPlayProperties.playMode == WEBSTREAM) {
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
	char *coverFileName = *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber);
	Log_Println(coverFileName, LOGLEVEL_DEBUG);

	File coverFile = gFSystem.open(coverFileName, FILE_READ);
	// seek to start position
	coverFile.seek(gPlayProperties.coverFilePos);
	uint8_t encoding = coverFile.read();
	// mime-type (null terminated)
	char mimeType[255];
	for (uint8_t i = 0u; i < 255; i++) {
		mimeType[i] = coverFile.read();
		if (uint8_t(mimeType[i]) == 0) {
			break;
		}
	}
	Log_Printf(LOGLEVEL_NOTICE, "serve cover image (%s): %s", mimeType, coverFileName);

	// skip image type (1 Byte)
	coverFile.read();
	// skip description (null terminated)
	for (uint8_t i = 0u; i < 255; i++) {
		if (uint8_t(coverFile.read()) == 0) {
			break;
		}
	}
	// UTF-16 and UTF-16BE are terminated with an extra 0
	if (encoding == 1 || encoding == 2) {
		coverFile.read();
	}

	int imageSize = gPlayProperties.coverFileSize;
	AsyncWebServerResponse *response = request->beginChunkedResponse(mimeType, [coverFile, imageSize](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
		// some kind of webserver bug with actual size available, reduce the len
		maxLen = maxLen >> 1;

		File file = coverFile; // local copy of file pointer
		size_t leftToWrite = imageSize - index;
		if (!leftToWrite) {
			return 0; // end of transfer
		}
		size_t willWrite = (leftToWrite > maxLen) ? maxLen : leftToWrite;
		file.read(buffer, willWrite);
		index += willWrite;
		return willWrite;
	});
	response->addHeader("Cache Control", "no-cache, must-revalidate");
	request->send(response);
}
