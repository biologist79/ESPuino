#include <Arduino.h>
#include <WiFi.h>
#include <Update.h>
#include <nvsDump.h>
#include <esp_task_wdt.h>
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "freertos/ringbuf.h"
#include "ESPAsyncWebServer.h"
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include "settings.h"
#include "AudioPlayer.h"
#include "Battery.h"
#include "Cmd.h"
#include "Common.h"
#include "Ftp.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "Mqtt.h"
#include "Rfid.h"
#include "SdCard.h"
#include "System.h"
#include "Web.h"
#include "Wlan.h"
#include "revision.h"
#include "Rfid.h"
#include "HallEffectSensor.h"

#include "HTMLaccesspoint.h"
#include "HTMLmanagement.h"
#include "HTMLbinary.h"

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
static void handleGetSavedSSIDs(AsyncWebServerRequest *request);
static void handlePostSavedSSIDs(AsyncWebServerRequest *request, JsonVariant &json);
static void handleDeleteSavedSSIDs(AsyncWebServerRequest *request);
static void handleGetActiveSSID(AsyncWebServerRequest *request);
static void handleGetWiFiConfig(AsyncWebServerRequest *request);
static void handlePostWiFiConfig(AsyncWebServerRequest *request, JsonVariant &json);
static void handleCoverImageRequest(AsyncWebServerRequest *request);
static void handleWiFiScanRequest(AsyncWebServerRequest *request);

static bool Web_DumpNvsToSd(const char *_namespace, const char *_destFile);

static void onWebsocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
static String templateProcessor(const String &templ);
static void webserverStart(void);

// If PSRAM is available use it allocate memory for JSON-objects
struct SpiRamAllocator {
	void* allocate(size_t size) {
		return ps_malloc(size);

	}
	void deallocate(void* pointer) {
		free(pointer);
	}
};
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

static void serveProgmemFiles(const String& uri, const String& contentType, const uint8_t *content, size_t len) {
	wServer.on(uri.c_str(), HTTP_GET, [contentType, content, len](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response;

		// const bool etag = request->hasHeader("if-None-Match") && request->getHeader("if-None-Match")->value().equals(gitRevShort);
		const bool etag = false;
		if (etag)
			response = request->beginResponse(304);
		else {
			response = request->beginResponse_P(200, contentType, content, len);
			response->addHeader("Content-Encoding", "gzip");
		}
		// response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
		// response->addHeader("ETag", gitRevShort);		// use git revision as digest
		request->send(response);
	});
}


class OneParamRewrite : public AsyncWebRewrite
{
  protected:
    String _urlPrefix;
    int _paramIndex;
    String _paramsBackup;

  public:
  OneParamRewrite(const char* from, const char* to)
    : AsyncWebRewrite(from, to) {

      _paramIndex = _from.indexOf('{');

      if( _paramIndex >=0 && _from.endsWith("}")) {
        _urlPrefix = _from.substring(0, _paramIndex);
        int index = _params.indexOf('{');
        if(index >= 0) {
          _params = _params.substring(0, index);
        }
      } else {
        _urlPrefix = _from;
      }
      _paramsBackup = _params;
  }

  bool match(AsyncWebServerRequest *request) override {
    if(request->url().startsWith(_urlPrefix)) {
      if(_paramIndex >= 0) {
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



// First request will return 0 results unless you start scan from somewhere else (loop/setup)
// Do not request more often than 3-5 seconds
static void handleWiFiScanRequest(AsyncWebServerRequest *request) {
	String json = "[";
	int n = WiFi.scanComplete();
	if (n == -2) {
		// -2 if scan not triggered
		WiFi.scanNetworks(true, false, true, 120);
	} else if(n) {
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
			if (i) json += ",";
			json += "{";
			json += "\"ssid\":\"" + WiFi.SSID(i) + "\"";
			json += ",\"bssid\":\"" + WiFi.BSSIDstr(i) + "\"";
			json += ",\"rssi\":" + String(WiFi.RSSI(i));
			json += ",\"channel\":" + String(WiFi.channel(i));
			json += ",\"secure\":" + String(WiFi.encryptionType(i));
			json += ",\"quality\":"+ String(quality); // WiFi strength in percent
			json += ",\"wico\":\"w" + String(int(round(map(quality, 0, 100, 1, 4)))) + "\""; // WiFi strength icon ("w1"-"w4")
			json += ",\"pico\":\"" + String((WIFI_AUTH_OPEN == WiFi.encryptionType(i)) ? "" : "pw") + "\""; // auth icon ("p1" for secured)
			json += "}";
		}
		WiFi.scanDelete();
		if(WiFi.scanComplete() == -2) {
			WiFi.scanNetworks(true, false, true, 120);
		}
	}
	json += "]";
	request->send(200, "application/json", json);
	json = String();
}

void Web_Init(void) {
	wServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response;

		// const bool etag = request->hasHeader("if-None-Match") && request->getHeader("if-None-Match")->value().equals(gitRevShort);
		const bool etag = false;
		if (etag)
			response = request->beginResponse(304);
		else
			response = request->beginResponse_P(200, "text/html", accesspoint_HTML);
		// response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
		// response->addHeader("ETag", gitRevShort);		// use git revision as digest
		request->send(response);
	});

	wServer.onNotFound([](AsyncWebServerRequest *request) {
		request->redirect("/");
	});

	WWWData::registerRoutes(serveProgmemFiles);

	wServer.addHandler(new AsyncCallbackJsonWebHandler("/savedSSIDs", handlePostSavedSSIDs));
	wServer.on("/wificonfig", HTTP_GET, handleGetWiFiConfig);
	wServer.addHandler(new AsyncCallbackJsonWebHandler("/wificonfig", handlePostWiFiConfig));

	wServer.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
		String url = "http://" + Wlan_GetHostname() + ".local";
		String html = "<!DOCTYPE html>";
		#if (LANGUAGE == DE)
			html += "Der ESPuino wird neu gestartet...<br>Management Website: ";
		#else
			html += "ESPuino is being restarted...<br>Management website: ";
		#endif
		html += "<a href=\"" + url + "\">" + url + "</a>";
		html += "<script>async function tryRedirect() {try {var url = \"" + url + "\";const response = await fetch(url);window.location.href = url;} catch (error) {console.log(error);setTimeout(tryRedirect, 2000);}}tryRedirect();</script>";
		request->send(200, "text/html", html);
		Serial.flush();
		ESP.restart();
	});

	wServer.on("/shutdown", HTTP_GET, [](AsyncWebServerRequest *request) {
	#if (LANGUAGE == DE)
		request->send(200, "text/html", "ESPuino wird ausgeschaltet...");
	#else
		request->send(200, "text/html", "ESPuino is being shutdown...");
		#endif
		System_RequestSleep();
	});

	wServer.on("/wifiscan", HTTP_GET, handleWiFiScanRequest);
	// start a first WiFi scan (to get a WiFi list more quickly in webview)
	WiFi.scanNetworks(true, false, true, 120);

	// allow cors for local debug (https://github.com/me-no-dev/ESPAsyncWebServer/issues/1080)
	DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Accept, Content-Type, Authorization");
	DefaultHeaders::Instance().addHeader("Access-Control-Allow-Credentials", "true");
	DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
	wServer.begin();
	Log_Println(httpReady, LOGLEVEL_NOTICE);
}

void Web_Cyclic(void) {
	webserverStart();
	ws.cleanupClients();
}

void notFound(AsyncWebServerRequest *request) {
	request->send(404, "text/plain", "Not found");
}

void webserverStart(void) {
	if (Wlan_IsConnected() && !webserverStarted) {
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
			if (etag)
				response = request->beginResponse(304);
			else {
				if (gFSystem.exists("/.html/index.htm"))
					response = request->beginResponse(gFSystem, "/.html/index.htm", String(), false, templateProcessor);
				else
					response = request->beginResponse_P(200, "text/html", management_HTML, templateProcessor);
			}
			// response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
			// response->addHeader("ETag", gitRevShort);		// use git revision as digest
			request->send(response);
		});

		WWWData::registerRoutes(serveProgmemFiles);

		// Log
		wServer.on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
			request->send(200, "text/plain; charset=utf-8", Log_GetRingBuffer());
		});

		// software/wifi/heap/psram-info
		wServer.on(
			"/info", HTTP_GET, [](AsyncWebServerRequest *request) {
				char buffer[128];
				String info = "ESPuino " + (String) softwareRevision;
				info += "\nESPuino " + (String) gitRevision;
				info += "\nArduino version: " + String(ESP_ARDUINO_VERSION_MAJOR) + '.' + String(ESP_ARDUINO_VERSION_MINOR) + '.' + String(ESP_ARDUINO_VERSION_PATCH);
				info += "\nESP-IDF version: " + String(ESP.getSdkVersion());
				#if (LANGUAGE == DE)
					info += "\nFreier Heap: " + String(ESP.getFreeHeap()) + " Bytes";
					info += "\nGroesster freier Heap-Block: " + String((uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) + " Bytes";
					info += "\nFreier PSRAM: ";
					info += (!psramInit()) ? "nicht verfuegbar" : String(ESP.getFreePsram());
					if (Wlan_IsConnected()) {
						IPAddress myIP = WiFi.localIP();
						info += "\nAktuelle IP: " + String(myIP[0]) + '.' + String(myIP[1]) + '.' + String(myIP[2]) + '.' + String(myIP[3]);
						info += "\nWLAN-Signalstaerke: " + String((int8_t)Wlan_GetRssi()) + " dBm";
					}
				#else
					info += "\nFree heap: " + String(ESP.getFreeHeap()) + " bytes";
					info += "\nLargest free heap-block: " + String((uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) + " bytes";
					info += "\nFree PSRAM: ";
					info += (!psramInit()) ? "not available" : String(ESP.getFreePsram());
					if (Wlan_IsConnected()) {
						IPAddress myIP = WiFi.localIP();
						info += "\nCurrent IP: " + String(myIP[0]) + '.' + String(myIP[1]) + '.' + String(myIP[2]) + '.' + String(myIP[3]);
						info += "\nWiFi signal-strength: " + String((int8_t)Wlan_GetRssi()) + " dBm";
					}
				#endif
				#ifdef BATTERY_MEASURE_ENABLE
					snprintf(buffer, sizeof(buffer), currentVoltageMsg, Battery_GetVoltage());
					info += "\n" + String(buffer);
					snprintf(buffer, sizeof(buffer), currentChargeMsg, Battery_EstimateLevel() * 100);
					info += "\n" + String(buffer);
				#endif
                #ifdef HALLEFFECT_SENSOR_ENABLE
					uint16_t sva = gHallEffectSensor.readSensorValueAverage(true);
					int diff = sva-gHallEffectSensor.NullFieldValue();
					snprintf(buffer, sizeof(buffer), "\nHallEffectSensor NullFieldValue:%d, actual:%d, diff:%d, LastWaitFor_State:%d (waited:%d ms)", gHallEffectSensor.NullFieldValue(), sva, diff, gHallEffectSensor.LastWaitForState(), gHallEffectSensor.LastWaitForStateMS());
					info += buffer;
                #endif
				request->send_P(200, "text/plain", info.c_str());
			});

		// NVS-backup-upload
		wServer.on(
			"/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
				request->send_P(200, "text/html", backupRecoveryWebsite);
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
			}, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
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
					//ESP.restart(); // restart is done via webpage javascript
				}
		});

		// ESP-restart
		wServer.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
			request->send_P(200, "text/html", restartWebsite);
			Serial.flush();
			ESP.restart();
		});

		// ESP-shutdown
		wServer.on("/shutdown", HTTP_GET, [](AsyncWebServerRequest *request) {
			request->send_P(200, "text/html", shutdownWebsite);
			System_RequestSleep();
		});

		// ESP-shutdown
		wServer.on("/rfidnvserase", HTTP_GET, [](AsyncWebServerRequest *request) {
			request->send_P(200, "text/html", eraseRfidNvsWeb);
			Log_Println(eraseRfidNvs, LOGLEVEL_NOTICE);
			gPrefsRfid.clear();
			Web_DumpNvsToSd("rfidTags", backupFile);
		});


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

		wServer.on("/savedSSIDs", HTTP_GET, handleGetSavedSSIDs);
		wServer.addHandler(new AsyncCallbackJsonWebHandler("/savedSSIDs", handlePostSavedSSIDs));

		wServer.addRewrite(new OneParamRewrite("/savedSSIDs/{ssid}", "/savedSSIDs?ssid={ssid}") );
		wServer.on("/savedSSIDs", HTTP_DELETE, handleDeleteSavedSSIDs);
		wServer.on("/activeSSID", HTTP_GET, handleGetActiveSSID);

		wServer.on("/wificonfig", HTTP_GET, handleGetWiFiConfig);
		wServer.addHandler(new AsyncCallbackJsonWebHandler("/wificonfig", handlePostWiFiConfig));

		// current cover image
		wServer.on("/cover", HTTP_GET, handleCoverImageRequest);

		// ESPuino logo
		wServer.on("/logo", HTTP_GET, [](AsyncWebServerRequest *request) {
			Log_Println("logo request", LOGLEVEL_DEBUG);
			if (gFSystem.exists("/.html/logo.png")) {
				request->send(gFSystem, "/.html/logo.png", "image/png");
				return;
			};
			if (gFSystem.exists("/.html/logo.svg")) {
				request->send(gFSystem, "/.html/logo.svg", "image/svg+xml");
				return;
			};
			request->redirect("https://www.espuino.de/espuino/Espuino32.png");
		});
		// ESPuino favicon
		wServer.on("/favicon", HTTP_GET, [](AsyncWebServerRequest *request) {
			Log_Println("favicon request", LOGLEVEL_DEBUG);
			if (gFSystem.exists("/.html/favicon.ico")) {
				request->send(gFSystem, "/.html/favicon.png", "image/x-icon");
				return;
			};
			request->redirect("https://espuino.de/espuino/favicon.ico");
		});
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

		// allow cors for local debug
		DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
		wServer.begin();
		webserverStarted = true;
	}
}

// Used for substitution of some variables/templates of html-files. Is called by webserver's template-engine
String templateProcessor(const String &templ) {
	if (templ == "FTP_USER") {
		return gPrefsSettings.getString("ftpuser", "-1");
	} else if (templ == "FTP_PWD") {
		return gPrefsSettings.getString("ftppassword", "-1");
	} else if (templ == "FTP_USER_LENGTH") {
		return String(ftpUserLength - 1);
	} else if (templ == "FTP_PWD_LENGTH") {
		return String(ftpPasswordLength - 1);
	} else if (templ == "SHOW_FTP_TAB") { // Only show FTP-tab if FTP-support was compiled
		#ifdef FTP_ENABLE
			return "true";
		#else
			return "false";
		#endif
	} else if (templ == "SHOW_BLUETOOTH_TAB") { // Only show Bluetooth-tab if Bluetooth-support was compiled
		#ifdef BLUETOOTH_ENABLE
			return "true";
		#else
			return "false";
		#endif		
	} else if (templ == "INIT_LED_BRIGHTNESS") {
		return String(gPrefsSettings.getUChar("iLedBrightness", 0));
	} else if (templ == "NIGHT_LED_BRIGHTNESS") {
		return String(gPrefsSettings.getUChar("nLedBrightness", 0));
	} else if (templ == "MAX_INACTIVITY") {
		return String(gPrefsSettings.getUInt("mInactiviyT", 0));
	} else if (templ == "INIT_VOLUME") {
		return String(gPrefsSettings.getUInt("initVolume", 0));
	} else if (templ == "CURRENT_VOLUME") {
		return String(AudioPlayer_GetCurrentVolume());
	} else if (templ == "MAX_VOLUME_SPEAKER") {
		return String(gPrefsSettings.getUInt("maxVolumeSp", 0));
	} else if (templ == "MAX_VOLUME_HEADPHONE") {
		return String(gPrefsSettings.getUInt("maxVolumeHp", 0));
#ifdef BATTERY_MEASURE_ENABLE
	#ifdef MEASURE_BATTERY_VOLTAGE
		} else if (templ == "WARNING_LOW_VOLTAGE") {
			return String(gPrefsSettings.getFloat("wLowVoltage", warningLowVoltage));
		} else if (templ == "VOLTAGE_INDICATOR_LOW") {
			return String(gPrefsSettings.getFloat("vIndicatorLow", voltageIndicatorLow));
		} else if (templ == "VOLTAGE_INDICATOR_HIGH") {
			return String(gPrefsSettings.getFloat("vIndicatorHigh", voltageIndicatorHigh));
	#endif
	#ifdef MEASURE_BATTERY_OTHER // placeholder
		} else if (templ == "todo") {
			return "todo";
	#endif
	} else if (templ == "BATTERY_CHECK_INTERVAL") {
		return String(gPrefsSettings.getUInt("vCheckIntv", batteryCheckInterval));
#else
	// TODO: hide battery config
#endif
	} else if (templ == "MQTT_CLIENTID") {
		if (gPrefsSettings.isKey("mqttClientId")) {
			return gPrefsSettings.getString("mqttClientId", "-1");
		} else {
			return "-1";
		}
	} else if (templ == "MQTT_SERVER") {
		if (gPrefsSettings.isKey("mqttServer")) {
			return gPrefsSettings.getString("mqttServer", "-1");
		} else {
			return "-1";
		}
	} else if (templ == "SHOW_MQTT_TAB") { // Only show MQTT-tab if MQTT-support was compiled
		#ifdef MQTT_ENABLE
			return "true";
		#else
			return "false";
		#endif
	} else if (templ == "MQTT_ENABLE") {
		if (Mqtt_IsEnabled()) {
			return "checked=\"checked\"";
		} else {
			return String();
		}
	} else if (templ == "MQTT_USER") {
		if (gPrefsSettings.isKey("mqttUser")) {
			return gPrefsSettings.getString("mqttUser", "-1");
		} else {
			return "-1";
		}
	} else if (templ == "MQTT_PWD") {
		if (gPrefsSettings.isKey("mqttPassword")) {
			return gPrefsSettings.getString("mqttPassword", "-1");
		} else {
			return "-1";
		}
	} else if (templ == "MQTT_USER_LENGTH") {
		return String(mqttUserLength - 1);
	} else if (templ == "MQTT_PWD_LENGTH") {
		return String(mqttPasswordLength - 1);
	} else if (templ == "MQTT_CLIENTID_LENGTH") {
		return String(mqttClientIdLength - 1);
	} else if (templ == "MQTT_SERVER_LENGTH") {
		return String(mqttServerLength - 1);
	} else if (templ == "MQTT_PORT") {
#ifdef MQTT_ENABLE
		return String(gMqttPort);
#endif
	} else if (templ == "BT_DEVICE_NAME") {
		if (gPrefsSettings.isKey("btDeviceName")) {
			return gPrefsSettings.getString("btDeviceName", "");
		}
	} else if (templ == "BT_PIN_CODE") {
		if (gPrefsSettings.isKey("btPinCode")) {
			return gPrefsSettings.getString("btPinCode", "");
		}		
	} else if (templ == "IPv4") {
		return WiFi.localIP().toString();
	} else if (templ == "RFID_TAG_ID") {
		return String(gCurrentRfidTagId);
	} else if (templ == "HOSTNAME") {
		return Wlan_GetHostname();
	}

	return String();
}

// Takes inputs from webgui, parses JSON and saves values in NVS
// If operation was successful (NVS-write is verified) true is returned
bool processJsonRequest(char *_serialJson) {
	if (!_serialJson)  {
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

   JsonObject object = doc.as<JsonObject>();

   if (doc.containsKey("general")) {
		uint8_t iVol = doc["general"]["iVol"].as<uint8_t>();
		uint8_t mVolSpeaker = doc["general"]["mVolSpeaker"].as<uint8_t>();
		uint8_t mVolHeadphone = doc["general"]["mVolHeadphone"].as<uint8_t>();
		uint8_t iBright = doc["general"]["iBright"].as<uint8_t>();
		uint8_t nBright = doc["general"]["nBright"].as<uint8_t>();
		uint8_t iTime = doc["general"]["iTime"].as<uint8_t>();
		float vWarning = doc["general"]["vWarning"].as<float>();
		float vIndLow = doc["general"]["vIndLow"].as<float>();
		float vIndHi = doc["general"]["vIndHi"].as<float>();
		uint8_t vInt = doc["general"]["vInt"].as<uint8_t>();

		gPrefsSettings.putUInt("initVolume", iVol);
		gPrefsSettings.putUInt("maxVolumeSp", mVolSpeaker);
		gPrefsSettings.putUInt("maxVolumeHp", mVolHeadphone);
		gPrefsSettings.putUChar("iLedBrightness", iBright);
		gPrefsSettings.putUChar("nLedBrightness", nBright);
		gPrefsSettings.putUInt("mInactiviyT", iTime);
		gPrefsSettings.putFloat("wLowVoltage", vWarning);
		gPrefsSettings.putFloat("vIndicatorLow", vIndLow);
		gPrefsSettings.putFloat("vIndicatorHigh", vIndHi);
		gPrefsSettings.putUInt("vCheckIntv", vInt);

		// Check if settings were written successfully
		if (gPrefsSettings.getUInt("initVolume", 0) != iVol ||
			gPrefsSettings.getUInt("maxVolumeSp", 0) != mVolSpeaker ||
			gPrefsSettings.getUInt("maxVolumeHp", 0) != mVolHeadphone ||
			gPrefsSettings.getUChar("iLedBrightness", 0) != iBright ||
			gPrefsSettings.getUChar("nLedBrightness", 0) != nBright ||
			gPrefsSettings.getUInt("mInactiviyT", 0) != iTime ||
			gPrefsSettings.getFloat("wLowVoltage", 999.99) != vWarning ||
			gPrefsSettings.getFloat("vIndicatorLow", 999.99) != vIndLow ||
			gPrefsSettings.getFloat("vIndicatorHigh", 999.99) != vIndHi ||
			gPrefsSettings.getUInt("vCheckIntv", 17777) != vInt) {
				return false;
		}
		Battery_Init();
	} else if (doc.containsKey("ftp")) {
		const char *_ftpUser = doc["ftp"]["ftpUser"];
		const char *_ftpPwd = doc["ftp"]["ftpPwd"];

		gPrefsSettings.putString("ftpuser", (String)_ftpUser);
		gPrefsSettings.putString("ftppassword", (String)_ftpPwd);

		if (!(String(_ftpUser).equals(gPrefsSettings.getString("ftpuser", "-1")) ||
			  String(_ftpPwd).equals(gPrefsSettings.getString("ftppassword", "-1")))) {
			return false;
		}
	} else if (doc.containsKey("ftpStatus")) {
		uint8_t _ftpStart = doc["ftpStatus"]["start"].as<uint8_t>();
		if (_ftpStart == 1) { // ifdef FTP_ENABLE is checked in Ftp_EnableServer()
			Ftp_EnableServer();
		}
	} else if (doc.containsKey("mqtt")) {
		uint8_t _mqttEnable = doc["mqtt"]["mqttEnable"].as<uint8_t>();
		const char *_mqttClientId = object["mqtt"]["mqttClientId"];
		const char *_mqttServer = object["mqtt"]["mqttServer"];
		const char *_mqttUser = doc["mqtt"]["mqttUser"];
		const char *_mqttPwd = doc["mqtt"]["mqttPwd"];
		uint16_t _mqttPort = doc["mqtt"]["mqttPort"].as<uint16_t>();

		gPrefsSettings.putUChar("enableMQTT", _mqttEnable);
		gPrefsSettings.putString("mqttClientId", (String)_mqttClientId);
		gPrefsSettings.putString("mqttServer", (String)_mqttServer);
		gPrefsSettings.putString("mqttUser", (String)_mqttUser);
		gPrefsSettings.putString("mqttPassword", (String)_mqttPwd);
		gPrefsSettings.putUInt("mqttPort", _mqttPort);

		if ((gPrefsSettings.getUChar("enableMQTT", 99) != _mqttEnable) ||
			(!String(_mqttServer).equals(gPrefsSettings.getString("mqttServer", "-1")))) {
			return false;
		}
	} else if (doc.containsKey("bluetooth")) {
		// bluetooth settings
		const char *_btDeviceName = doc["bluetooth"]["deviceName"];
		gPrefsSettings.putString("btDeviceName", (String)_btDeviceName);
		const char *btPinCode = doc["bluetooth"]["pinCode"];
		gPrefsSettings.putString("btPinCode", (String)btPinCode);
	} else if (doc.containsKey("rfidMod")) {
		const char *_rfidIdModId = object["rfidMod"]["rfidIdMod"];
		uint8_t _modId = object["rfidMod"]["modId"];
		char rfidString[12];
		if (_modId <= 0) {
			gPrefsRfid.remove(_rfidIdModId);
		} else {
			snprintf(rfidString, sizeof(rfidString) / sizeof(rfidString[0]), "%s0%s0%s%u%s0", stringDelimiter, stringDelimiter, stringDelimiter, _modId, stringDelimiter);
			gPrefsRfid.putString(_rfidIdModId, rfidString);

			String s = gPrefsRfid.getString(_rfidIdModId, "-1");
			if (s.compareTo(rfidString)) {
				return false;
			}
		}
		Web_DumpNvsToSd("rfidTags", backupFile); // Store backup-file every time when a new rfid-tag is programmed
	} else if (doc.containsKey("rfidAssign")) {
		const char *_rfidIdAssinId = object["rfidAssign"]["rfidIdMusic"];
		char _fileOrUrlAscii[MAX_FILEPATH_LENTGH];
		convertFilenameToAscii(object["rfidAssign"]["fileOrUrl"], _fileOrUrlAscii);
		uint8_t _playMode = object["rfidAssign"]["playMode"];
		char rfidString[275];
		snprintf(rfidString, sizeof(rfidString) / sizeof(rfidString[0]), "%s%s%s0%s%u%s0", stringDelimiter, _fileOrUrlAscii, stringDelimiter, stringDelimiter, _playMode, stringDelimiter);
		gPrefsRfid.putString(_rfidIdAssinId, rfidString);
		#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
			strncpy(gOldRfidTagId, "X", cardIdStringSize-1);     // Set old rfid-id to crap in order to allow to re-apply a new assigned rfid-tag exactly once
		#endif

		String s = gPrefsRfid.getString(_rfidIdAssinId, "-1");
		if (s.compareTo(rfidString)) {
			return false;
		}
		Web_DumpNvsToSd("rfidTags", backupFile); // Store backup-file every time when a new rfid-tag is programmed
	} else if (doc.containsKey("ping")) {
		Web_SendWebsocketData(0, 20);
		return false;
	} else if (doc.containsKey("controls")) {
		if (object["controls"].containsKey("set_volume")) {
			uint8_t new_vol = doc["controls"]["set_volume"].as<uint8_t>();
			AudioPlayer_VolumeToQueueSender(new_vol, true);
		} if (object["controls"].containsKey("action")) {
			uint8_t cmd = doc["controls"]["action"].as<uint8_t>();
			Cmd_Action(cmd);
		}
	} else if (doc.containsKey("trackinfo")) {
		Web_SendWebsocketData(0, 30);
	} else if (doc.containsKey("coverimg")) {
		Web_SendWebsocketData(0, 40);
	} else if (doc.containsKey("volume")) {
		Web_SendWebsocketData(0, 50);
	}

	return true;
}

// Sends JSON-answers via websocket
void Web_SendWebsocketData(uint32_t client, uint8_t code) {
	if (!webserverStarted)
		return;
	char *jBuf = (char *) x_calloc(255, sizeof(char));

	const size_t CAPACITY = JSON_OBJECT_SIZE(1) + 200;
	StaticJsonDocument<CAPACITY> doc;
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
		//object["battery"] = Battery_GetVoltage();
	} else if (code == 30) {
		JsonObject entry = object.createNestedObject("trackinfo");
		entry["pausePlay"] = gPlayProperties.pausePlay;
		entry["currentTrackNumber"] = gPlayProperties.currentTrackNumber + 1;
		entry["numberOfTracks"] = gPlayProperties.numberOfTracks;
		entry["volume"] = AudioPlayer_GetCurrentVolume();
		entry["name"] = gPlayProperties.title;
	} else if (code == 40) {
		object["coverimg"] = "coverimg";
	} else if (code == 50) {
		object["volume"] = AudioPlayer_GetCurrentVolume();
	};

	serializeJson(doc, jBuf, 255);

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
		//client connected
		Log_Printf(LOGLEVEL_DEBUG, "ws[%s][%u] connect", server->url(), client->id());
		//client->printf("Hello Client %u :)", client->id());
		//client->ping();
	} else if (type == WS_EVT_DISCONNECT) {
		//client disconnected
		Log_Printf(LOGLEVEL_DEBUG, "ws[%s][%u] disconnect", server->url(), client->id());
	} else if (type == WS_EVT_ERROR) {
		//error was received from the other end
		Log_Printf(LOGLEVEL_DEBUG, "ws[%s][%u] error(%u): %s", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
	} else if (type == WS_EVT_PONG) {
		//pong message was received (in response to a ping request maybe)
		Log_Printf(LOGLEVEL_DEBUG, "ws[%s][%u] pong[%u]: %s", server->url(), client->id(), len, (len) ? (char *)data : "");
	} else if (type == WS_EVT_DATA) {
		//data packet
		AwsFrameInfo *info = (AwsFrameInfo *)arg;
		if (info && info->final && info->index == 0 && info->len == len && client && len > 0) {
			//the whole message is in a single frame and we got all of it's data
			//Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

			if (processJsonRequest((char *)data)) {
				if (data && (strncmp((char *)data, "getTrack", 8))) {   // Don't send back ok-feedback if track's name is requested in background
					Web_SendWebsocketData(client->id(), 1);
				}
			}

			if (info->opcode == WS_TEXT) {
				data[len] = 0;
				//Serial.printf("%s\n", (char *)data);
			} else {
				for (size_t i = 0; i < info->len; i++) {
					Serial.printf("%02x ", data[i]);
				}
				//Serial.printf("\n");
			}
		}
	}
}

void explorerCreateParentDirectories(const char* filePath) {
	char tmpPath[MAX_FILEPATH_LENTGH];
	char *rest;

	rest = strchr(filePath, '/');
	while (rest) {
		if (rest-filePath != 0){
			memcpy(tmpPath, filePath, rest-filePath);
			tmpPath[rest-filePath] = '\0';
			Log_Printf(LOGLEVEL_DEBUG, "creating dir \"%s\"\n", tmpPath);
			gFSystem.mkdir(tmpPath);
		}
		rest = strchr(rest+1, '/');
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
			AsyncWebParameter *param = request->getParam("path");
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
		for (uint32_t i = 0; i < nr_of_buffers; i++){
			size_in_buffer[i] = 0;
			buffer_full[i] = false;
		}

		// Create Task for handling the storage of the data
		xTaskCreatePinnedToCore(
			explorerHandleFileStorageTask, /* Function to implement the task */
			"fileStorageTask",             /* Name of the task */
			4000,                          /* Stack size in words */
			filePath,                      /* Task input parameter */
			2 | portPRIVILEGE_BIT,         /* Priority of the task */
			&fileStorageTaskHandle,        /* Task handle. */
			1                              /* Core where the task should run */
		);
	}

	if (len) {
		// wait till buffer is ready
		while (buffer_full[index_buffer_write]){
			vTaskDelay(2u); 
		}

		size_t len_to_write = len;
		size_t space_left = chunk_size - size_in_buffer[index_buffer_write];
		if (space_left < len_to_write){
			len_to_write = space_left;
		}
		// write content to buffer
		memcpy(buffer[index_buffer_write]+size_in_buffer[index_buffer_write], data, len_to_write);
		size_in_buffer[index_buffer_write] += len_to_write;

		// check if buffer is filled. If full, signal that ready and change buffers
		if (size_in_buffer[index_buffer_write] == chunk_size){
			// signal, that buffer is ready. Increment index
			buffer_full[index_buffer_write] = true;
			index_buffer_write = (index_buffer_write + 1) % nr_of_buffers;

			// if still content left, put it into next buffer
			if (len_to_write < len) {
				// wait till new buffer is ready
				while (buffer_full[index_buffer_write]){
					vTaskDelay(2u);
				}
				size_t len_left_to_write = len-len_to_write;
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
		TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
		TIMERG0.wdt_feed=1;                       // feed dog
		TIMERG0.wdt_wprotect=0;                   // write protect
		// feed dog 1
		TIMERG1.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
		TIMERG1.wdt_feed=1;                       // feed dog
		TIMERG1.wdt_wprotect=0;                   // write protect
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
	uint32_t maxUploadDelay = 20;    // After this delay (in seconds) task will be deleted as transfer is considered to be finally broken

	BaseType_t uploadFileNotification;
	uint32_t uploadFileNotificationValue;
	uploadFile = gFSystem.open((char *)parameter, "w");
	uploadFile.setBufferSize(chunk_size);


	// pause some tasks to get more free CPU time for the upload
	vTaskSuspend(AudioTaskHandle);
	Led_TaskPause(); 
	Rfid_TaskPause();

	for (;;) {
		// check buffer is full with enough data or all data already sent
		uploadFileNotification = xTaskNotifyWait(0, 0, &uploadFileNotificationValue, 0);
		if ((buffer_full[index_buffer_read]) || (uploadFileNotification == pdPASS)) {

			while (buffer_full[index_buffer_read]){
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
				Log_Printf(LOGLEVEL_INFO, fileWritten, (char *)parameter, bytesNok+bytesOk, (millis() - transferStartTimestamp), (bytesNok+bytesOk)/(millis() - transferStartTimestamp));
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
	uint32_t listStartTimestamp = millis();
	//DynamicJsonDocument jsonBuffer(8192);
	#ifdef BOARD_HAS_PSRAM
		SpiRamJsonDocument jsonBuffer(65636);
	#else
		StaticJsonDocument<8192> jsonBuffer;
	#endif

	String serializedJsonString;
	AsyncWebParameter *param;
	char filePath[MAX_FILEPATH_LENTGH];
	JsonArray obj = jsonBuffer.createNestedArray();
	File root;
	if (request->hasParam("path")) {
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
		if (!startsWith( MyfileName.c_str() , (char *)"/.")) {
			JsonObject entry = obj.createNestedObject();
			convertAsciiToUtf8(MyfileName.c_str(), filePath);
			std::string path = filePath;
			std::string fileName = path.substr(path.find_last_of("/") + 1);
			entry["name"] = fileName;
			entry["dir"].set(isDir);
		}
		MyfileName = root.getNextFileName(&isDir);
	}
	root.close();

	serializeJson(obj, serializedJsonString);
	Log_Printf(LOGLEVEL_DEBUG, "build filelist finished: %lu ms", (millis() - listStartTimestamp));
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
	request->_tempObject = (void*)fileObj;

	AsyncWebServerResponse *response = request->beginResponse(dataType, fileObj->dataFile.size(), [request](uint8_t *buffer, size_t maxlen, size_t index) -> size_t {
		fileBlk *fileObj = (fileBlk*)request->_tempObject;
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
	AsyncWebParameter *param;
	char filePath[MAX_FILEPATH_LENTGH];
	if (request->hasParam("path")) {
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
	AsyncWebParameter *param;
	char filePath[MAX_FILEPATH_LENTGH];
	if (request->hasParam("path")) {
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
	AsyncWebParameter *srcPath;
	AsyncWebParameter *dstPath;
	char srcFullFilePath[MAX_FILEPATH_LENTGH];
	char dstFullFilePath[MAX_FILEPATH_LENTGH];
	if (request->hasParam("srcpath") && request->hasParam("dstpath")) {
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
	char filePath[MAX_FILEPATH_LENTGH];
	if (request->hasParam("path") && request->hasParam("playmode")) {
		param = request->getParam("path");
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

void handleGetSavedSSIDs(AsyncWebServerRequest *request) {
	AsyncJsonResponse *response = new AsyncJsonResponse(true);
	JsonArray json_ssids = response->getRoot();
	static String ssids[10];

	size_t len = Wlan_GetSSIDs(ssids, 10);
	for (int i = 0; i < len; i++) {
		json_ssids.add(ssids[i]);
	}

	response->setLength();
	request->send(response);
}

void handlePostSavedSSIDs(AsyncWebServerRequest *request, JsonVariant &json) {
	const JsonObject& jsonObj = json.as<JsonObject>();

	struct WiFiSettings networkSettings;

	// TODO: we truncate ssid and password, which is better than not checking at all, but still silently failing
	strncpy(networkSettings.ssid, (const char*) jsonObj["ssid"], 32);
	networkSettings.ssid[32] = '\0';
	strncpy(networkSettings.password, (const char*) jsonObj["pwd"], 64);
	networkSettings.password[64] = '\0';

	networkSettings.use_static_ip = (bool) jsonObj["static"];

	if (jsonObj.containsKey("static_addr")) {
		networkSettings.static_addr = (uint32_t) IPAddress().fromString((const char*) jsonObj["static_addr"]);
	}
	if (jsonObj.containsKey("static_gateway")) {
		networkSettings.static_gateway = (uint32_t) IPAddress().fromString((const char*) jsonObj["static_gateway"]);
	}
	if (jsonObj.containsKey("static_subnet")) {
		networkSettings.static_subnet = (uint32_t) IPAddress().fromString((const char*) jsonObj["static_subnet"]);
	}
	if (jsonObj.containsKey("static_dns1")) {
		networkSettings.static_dns1 = (uint32_t) IPAddress().fromString((const char*) jsonObj["static_dns1"]);
	}
	if (jsonObj.containsKey("static_dns2")) {
		networkSettings.static_dns2 = (uint32_t) IPAddress().fromString((const char*) jsonObj["static_dns2"]);
	}

	bool succ = Wlan_AddNetworkSettings(networkSettings);

	if (succ) {
		request->send(200, "text/plain; charset=utf-8", networkSettings.ssid);
	} else {
		request->send(500, "text/plain; charset=utf-8", "error adding network");
	}	
}

void handleDeleteSavedSSIDs(AsyncWebServerRequest *request) {
	AsyncWebParameter* p = request->getParam("ssid");
	String ssid = p->value();

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
	obj["scanwifionstart"].set(scanWifiOnStart);

	response->setLength();
	request->send(response);
}

void handlePostWiFiConfig(AsyncWebServerRequest *request, JsonVariant &json){
	const JsonObject& jsonObj = json.as<JsonObject>();

	// always perform perform a WiFi scan on startup?
	static bool alwaysScan = (bool) jsonObj["scanwifionstart"];
	gPrefsSettings.putBool("ScanWiFiOnStart", alwaysScan);

	// hostname
	String strHostname = jsonObj["hostname"];
	size_t len = strHostname.length();
	const char *hostname = strHostname.c_str();

	// validation: first char alphanumerical, then alphanumerical or '-', last char alphanumerical
	// These rules are mainly for mDNS purposes, a "pretty" hostname could have far fewer restrictions
	bool validated = true;
	if(len < 2 || len > 32) {
		validated = false;
	}

	if(!isAlphaNumeric(hostname[0]) || !isAlphaNumeric(hostname[len-1])) {
		validated = false;
	}

	for(int i = 0; i < len; i++) {
		if(!isAlphaNumeric(hostname[i]) && hostname[i] != '-') {
			validated = false;
			break;
		}
	}

	if (!validated) {
		request->send(400, "text/plain; charset=utf-8", "hostname validation failed");
		return;
	}

	bool succ = Wlan_SetHostname(String(hostname));
	if (succ) {
		request->send(200, "text/plain; charset=utf-8", hostname);
	} else {
		request->send(500, "text/plain; charset=utf-8", "error setting hostname");
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
	if(wrote != len) {
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
	char buf;
	File tmpFile = gFSystem.open(_filename);

	if (!tmpFile) {
		Log_Println(errorReadingTmpfile, LOGLEVEL_ERROR);
		return;
	}

	Led_SetPause(true);
	while (tmpFile.available() > 0) {
		buf = tmpFile.read();
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
					memcpy(nvsEntry[0].nvsEntry, token, strlen(token));
					nvsEntry[0].nvsEntry[strlen(token)] = '\0';
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

// Dumps all RFID-entries from NVS into a file on SD-card
bool Web_DumpNvsToSd(const char *_namespace, const char *_destFile) {
	Led_SetPause(true);          // Workaround to prevent exceptions due to Neopixel-signalisation while NVS-write
	esp_partition_iterator_t pi; // Iterator for find
	const esp_partition_t *nvs;  // Pointer to partition struct
	esp_err_t result = ESP_OK;
	const char *partname = "nvs";
	uint8_t pagenr = 0;   // Page number in NVS
	uint8_t i;            // Index in Entry 0..125
	uint8_t bm;           // Bitmap for an entry
	uint32_t offset = 0;  // Offset in nvs partition
	uint8_t namespace_ID; // Namespace ID found

	pi = esp_partition_find(ESP_PARTITION_TYPE_DATA,   // Get partition iterator for
							ESP_PARTITION_SUBTYPE_ANY, // this partition
							partname);
	if (pi) {
		nvs = esp_partition_get(pi);        // Get partition struct
		esp_partition_iterator_release(pi); // Release the iterator
		Log_Printf(LOGLEVEL_DEBUG, "Partition %s found, %d bytes", partname, nvs->size);
	} else {
		Log_Printf(LOGLEVEL_ERROR, "Partition %s not found!", partname);
		return NULL;
	}
	namespace_ID = FindNsID(nvs, _namespace); // Find ID of our namespace in NVS
	File backupFile = gFSystem.open(_destFile, FILE_WRITE);
	if (!backupFile) {
		return false;
	}
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
						String s = gPrefsRfid.getString((const char *)buf.Entry[i].Key);
						backupFile.printf("%s%s%s%s\n", stringOuterDelimiter, buf.Entry[i].Key, stringOuterDelimiter, s.c_str());
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

	backupFile.close();
	Led_SetPause(false);

	return true;
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
			Log_Println("no cover image for SD-card audio", LOGLEVEL_DEBUG);
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
	AsyncWebServerResponse *response = request->beginChunkedResponse(mimeType, [coverFile,imageSize](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {

		if (maxLen > 1024) {
			maxLen = 1024;
		}

		File file = coverFile; // local copy of file pointer
		size_t leftToWrite = imageSize - index;
		if (! leftToWrite) {
			return 0; //end of transfer
		}
		size_t willWrite = (leftToWrite > maxLen)?maxLen:leftToWrite;
		file.read(buffer, willWrite);
		index += willWrite;
		return willWrite;
	});
	response->addHeader("Cache Control", "no-cache, must-revalidate");
	request->send(response);
}