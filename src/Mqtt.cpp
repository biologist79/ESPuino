#include <Arduino.h>
#include "settings.h"

#include "Mqtt.h"

#include "AudioPlayer.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "Queues.h"
#include "System.h"
#include "Wlan.h"
#include "mqtt_client.h"
#include "revision.h"

#include <Rfid.h>
#include <WiFi.h>
#include <charconv>
#include <limits>
#include <string_view>

// MQTT-helper
#ifdef MQTT_ENABLE
static WiFiClient Mqtt_WifiClient;
static esp_mqtt_client_handle_t mqtt_client = NULL;
// Please note: all of them are defaults that can be changed later via GUI
String gBaseTopic = ""; // Base topic for MQTT (if not found in NVS this one will be taken)
String gDeviceId = device_id; // Device ID that is used for MQTT (if not found in NVS this one will be taken)
String gMqttClientId = gDeviceId; // ClientId for the MQTT-server, must be server wide unique (if not found in NVS this one will be taken)
String gMqttServer = "192.168.2.43"; // IP-address of MQTT-server (if not found in NVS this one will be taken)
String gMqttUser = "mqtt-user"; // MQTT-user
String gMqttPassword = "mqtt-password"; // MQTT-password
uint16_t gMqttPort = 1883; // MQTT-Port
constexpr uint8_t MQTT_TOPIC_MAX_LENGTH = 128u; // Maximal length of MQTT-topic
#endif

// helper to replace <MAC> with actual MAC-address (no colons, lowercase)
static String ReplaceMacToken(const String &in) {
	if (in.indexOf("<MAC>") == -1) {
		return in;
	}
	String mac = WiFi.macAddress(); // returns AA:BB:CC:DD:EE:FF
	mac.replace(":", "");
	mac.toLowerCase();
	String out = in;
	out.replace("<MAC>", mac);
	return out;
}

static void storeAndSetDeviceId(const String &id) {
	// store in NVS and update runtime variable (with <MAC> resolved)
	gPrefsSettings.putString("mqttDeviceId", id);
	gDeviceId = ReplaceMacToken(id);
	// client id usually equals device id
	gMqttClientId = gDeviceId;
}

// MQTT
static bool Mqtt_Enabled = true;

#ifdef MQTT_ENABLE
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void Mqtt_ClientCallback(const char *topic_buf, uint32_t topic_length, const char *payload_buf, uint32_t payload_length);
#endif

void Mqtt_Init() {
#ifdef MQTT_ENABLE
	// Get MQTT-enable from NVS
	uint8_t nvsEnableMqtt = gPrefsSettings.getUChar("enableMQTT", 99);
	switch (nvsEnableMqtt) {
		case 99:
			gPrefsSettings.putUChar("enableMQTT", Mqtt_Enabled);
			Log_Println(wroteMqttFlagToNvs, LOGLEVEL_ERROR);
			break;
		case 1:
			Mqtt_Enabled = nvsEnableMqtt;
			Log_Printf(LOGLEVEL_INFO, restoredMqttActiveFromNvs, nvsEnableMqtt);
			break;
		case 0:
			Mqtt_Enabled = nvsEnableMqtt;
			Log_Printf(LOGLEVEL_INFO, restoredMqttDeactiveFromNvs, nvsEnableMqtt);
			break;
	}

	// Get MQTT-clientid / device id from NVS
	String nvsMqttDeviceId = gPrefsSettings.getString("mqttDeviceId", "-1");
	if (!nvsMqttDeviceId.compareTo("-1")) {
		// not found: store default (from settings.h) and persist
		gPrefsSettings.putString("mqttDeviceId", gDeviceId);
		Log_Println(wroteMqttClientIdToNvs, LOGLEVEL_ERROR);
	} else {
		// If device id contains <MAC> it'll be replaced
		gDeviceId = ReplaceMacToken(nvsMqttDeviceId);
		gMqttClientId = gDeviceId;
		Log_Printf(LOGLEVEL_INFO, restoredMqttClientIdFromNvs, nvsMqttDeviceId.c_str());
	}

	// Also make sure client id follows device id if not set explicitly
	String nvsMqttClientId = gPrefsSettings.getString("mqttClientId", "-1");
	if (!nvsMqttClientId.compareTo("-1")) {
		gPrefsSettings.putString("mqttClientId", gMqttClientId);
	} else {
		gMqttClientId = nvsMqttClientId;
	}

	// Get MQTT-server from NVS
	String nvsMqttServer = gPrefsSettings.getString("mqttServer", "-1");
	if (!nvsMqttServer.compareTo("-1")) {
		gPrefsSettings.putString("mqttServer", gMqttServer);
		Log_Println(wroteMqttServerToNvs, LOGLEVEL_ERROR);
	} else {
		gMqttServer = nvsMqttServer;
		Log_Printf(LOGLEVEL_INFO, restoredMqttServerFromNvs, nvsMqttServer.c_str());
	}

	// Get Base topic from NVS
	String nvsMqttBaseTopic = gPrefsSettings.getString("mqttBaseTopic", "-1");
	if (!nvsMqttBaseTopic.compareTo("-1")) {
		gPrefsSettings.putString("mqttBaseTopic", gBaseTopic);
		Log_Println(wroteMqttServerToNvs, LOGLEVEL_ERROR);
	} else {
		gBaseTopic = nvsMqttBaseTopic;
		Log_Printf(LOGLEVEL_INFO, "restored BaseTopic from NVS: %s", nvsMqttBaseTopic.c_str());
	}

	// Get MQTT-user from NVS
	String nvsMqttUser = gPrefsSettings.getString("mqttUser", "-1");
	if (!nvsMqttUser.compareTo("-1")) {
		gPrefsSettings.putString("mqttUser", (String) gMqttUser);
		Log_Println(wroteMqttUserToNvs, LOGLEVEL_ERROR);
	} else {
		gMqttUser = nvsMqttUser;
		Log_Printf(LOGLEVEL_INFO, restoredMqttUserFromNvs, nvsMqttUser.c_str());
	}

	// Get MQTT-password from NVS
	String nvsMqttPassword = gPrefsSettings.getString("mqttPassword", "-1");
	if (!nvsMqttPassword.compareTo("-1")) {
		gPrefsSettings.putString("mqttPassword", (String) gMqttPassword);
		Log_Println(wroteMqttPwdToNvs, LOGLEVEL_ERROR);
	} else {
		gMqttPassword = nvsMqttPassword;
		Log_Printf(LOGLEVEL_INFO, restoredMqttPwdFromNvs, nvsMqttPassword.c_str());
	}

	// Get MQTT-password from NVS
	uint32_t nvsMqttPort = gPrefsSettings.getUInt("mqttPort", 99999);
	if (nvsMqttPort == 99999) {
		gPrefsSettings.putUInt("mqttPort", gMqttPort);
	} else {
		gMqttPort = nvsMqttPort;
		Log_Printf(LOGLEVEL_INFO, restoredMqttPortFromNvs, gMqttPort);
	}

	// Only enable MQTT if requested
	if (Mqtt_Enabled) {
		esp_mqtt_client_config_t mqtt_cfg = {};

		mqtt_cfg.credentials.client_id = gMqttClientId.c_str();
		mqtt_cfg.broker.address.hostname = gMqttServer.c_str();
		mqtt_cfg.broker.address.transport = esp_mqtt_transport_t::MQTT_TRANSPORT_OVER_TCP;
		mqtt_cfg.broker.address.port = gMqttPort;
		if ((gMqttUser.length() > 0u) && (gMqttPassword.length()) > 0u) {
			mqtt_cfg.credentials.username = gMqttUser.c_str();
			mqtt_cfg.credentials.authentication.password = gMqttPassword.c_str();
		}
		mqtt_cfg.task.priority = 1; // default is 5, keep it below the audio

		mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
		esp_mqtt_client_register_event(mqtt_client, esp_mqtt_event_id_t::MQTT_EVENT_ANY, mqtt_event_handler, NULL);

		// don't start the task yet, wait for WiFi to be connected
	}
#else
	Mqtt_Enabled = false;
#endif
}

void Mqtt_OnWifiConnected(void) {
#ifdef MQTT_ENABLE
	static bool mqtt_started = false;
	if (!mqtt_started) {
		esp_mqtt_client_start(mqtt_client);
		mqtt_started = true;
	} else {
		esp_mqtt_client_reconnect(mqtt_client);
	}
#endif
}

#ifdef MQTT_ENABLE
const char *Mqtt_GetTopic(const char *topic, bool isStateTopic) {
	static char out_string[MQTT_TOPIC_MAX_LENGTH]; // Static buffer to hold the result

	// Build the topic string with schema: base_topic/device_id/keyword[/set]
	if (gBaseTopic.length() > 0) {
		if (!isStateTopic) {
			snprintf(out_string, MQTT_TOPIC_MAX_LENGTH, "%s/%s/%s/%s", gBaseTopic.c_str(), gDeviceId.c_str(), topic, setter_token);
		} else {
			snprintf(out_string, MQTT_TOPIC_MAX_LENGTH, "%s/%s/%s", gBaseTopic.c_str(), gDeviceId.c_str(), topic);
		}
	} else {
		if (!isStateTopic) {
			snprintf(out_string, MQTT_TOPIC_MAX_LENGTH, "%s/%s/%s", gDeviceId.c_str(), topic, setter_token);
		} else {
			snprintf(out_string, MQTT_TOPIC_MAX_LENGTH, "%s/%s", gDeviceId.c_str(), topic);
		}
	}

	return out_string; // Return the static buffer
}

const char *Mqtt_GetStateTopic(const char *topic) {
	return Mqtt_GetTopic(topic, true);
}
const char *Mqtt_GetCommandTopic(const char *topic) {
	return Mqtt_GetTopic(topic, false);
}
#endif

void Mqtt_Exit(void) {
#ifdef MQTT_ENABLE
	Log_Println("shutdown MQTT..", LOGLEVEL_NOTICE);
	publishMqtt(topicState, "Offline", false);
	publishMqtt(topicTrack, "---", false);
	esp_mqtt_client_disconnect(mqtt_client);
	esp_mqtt_client_stop(mqtt_client);
	esp_mqtt_client_destroy(mqtt_client);
#endif
}

bool Mqtt_IsEnabled(void) {
	return Mqtt_Enabled;
}

/* Wrapper-functions for MQTT-publish */
bool publishMqtt(const char *topic, const char *payload, bool retained) {
#ifdef MQTT_ENABLE
	if (strcmp(topic, "") != 0) {
		int qos = 0;
		int ret = esp_mqtt_client_publish(mqtt_client, Mqtt_GetStateTopic(topic), payload, 0, qos, retained);
		// int ret = esp_mqtt_client_enqueue(mqtt_client, topic, payload, 0, qos, retained, true);
		return ret == 0;
	}
#endif

	return false;
}

bool publishMqtt(const char *topic, int32_t payload, bool retained) {
#ifdef MQTT_ENABLE
	char buf[11];
	snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%ld", payload);
	return publishMqtt(topic, buf, retained);
#else
	return false;
#endif
}

bool publishMqtt(const char *topic, uint32_t payload, bool retained) {
#ifdef MQTT_ENABLE
	char buf[11];
	snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%lu", payload);
	return publishMqtt(topic, buf, retained);
#else
	return false;
#endif
}

template <typename NumberType>
static NumberType toNumber(const std::string str) {
	NumberType result;
	const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);

	// Mimic return behavior of previously used strtoul function
	if (ec == std::errc()) {
		return result;
	}
	if (ec == std::errc::result_out_of_range) {
		return std::numeric_limits<NumberType>::max();
	}
	// ec == std::errc::invalid_argument
	return 0;
}

// Is called if there's a new MQTT-message for us
#ifdef MQTT_ENABLE
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
	// Log_Printf(LOGLEVEL_DEBUG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
	esp_mqtt_event_handle_t event = reinterpret_cast<esp_mqtt_event_handle_t>(event_data);
	esp_mqtt_client_handle_t client = event->client;

	switch ((esp_mqtt_event_id_t) event_id) {
		case MQTT_EVENT_CONNECTED: {
			int qos = 0;

			// Deepsleep-subscription
			esp_mqtt_client_subscribe(client, Mqtt_GetCommandTopic(topicSleep), qos);

			// RFID-"mqtt_debug"-ID-subscription
			esp_mqtt_client_subscribe(client, Mqtt_GetCommandTopic(topicRfid), qos);

			// Loudness-subscription
			esp_mqtt_client_subscribe(client, Mqtt_GetCommandTopic(topicLoudness), qos);

			// Sleep-Timer-subscription
			esp_mqtt_client_subscribe(client, Mqtt_GetCommandTopic(topicSleepTimer), qos);

			// Next/previous/stop/play-track-subscription
			esp_mqtt_client_subscribe(client, Mqtt_GetCommandTopic(topicTrackControl), qos);

			// Lock controls
			esp_mqtt_client_subscribe(client, Mqtt_GetCommandTopic(topicLockControls), qos);

			// Current repeat-Mode
			esp_mqtt_client_subscribe(client, Mqtt_GetCommandTopic(topicRepeatMode), qos);

			// LED-brightness
			esp_mqtt_client_subscribe(client, Mqtt_GetCommandTopic(topicLedBrightness), qos);

			// Publish current state
			publishMqtt(topicState, "Online", false);
			publishMqtt(topicTrack, gPlayProperties.title, false);
			publishMqtt(topicCoverChanged, "", false);
			publishMqtt(topicLoudness, static_cast<uint32_t>(AudioPlayer_GetCurrentVolume()), false);
			publishMqtt(topicSleepTimer, System_GetSleepTimerTimeStamp(), false);
			publishMqtt(topicLockControls, static_cast<uint32_t>(System_AreControlsLocked()), false);
			publishMqtt(topicPlaymode, static_cast<uint32_t>(gPlayProperties.playMode), false);
			publishMqtt(topicPausePlay, static_cast<uint32_t>(gPlayProperties.pausePlay), false);
			publishMqtt(topicLedBrightness, static_cast<uint32_t>(Led_GetBrightness()), false);
			publishMqtt(topicCurrentIPv4IP, Wlan_GetIpAddress().c_str(), false);
			publishMqtt(topicRepeatMode, static_cast<uint32_t>(AudioPlayer_GetRepeatMode()), false);

			char revBuf[16];
			strncpy(revBuf, softwareRevision + 19, sizeof(revBuf) - 1);
			revBuf[sizeof(revBuf) - 1] = '\0';
			publishMqtt(topicSRevision, revBuf, false);

			break;
		}
		case MQTT_EVENT_DISCONNECTED: {
			break;
		}
		case MQTT_EVENT_SUBSCRIBED: {
			break;
		}
		case MQTT_EVENT_UNSUBSCRIBED: {
			break;
		}
		case MQTT_EVENT_PUBLISHED: {
			break;
		}
		case MQTT_EVENT_DATA: {
			Mqtt_ClientCallback(event->topic, event->topic_len, event->data, event->data_len);
			break;
		}
		case MQTT_EVENT_ERROR: {
			Log_Printf(LOGLEVEL_ERROR, "MQTT_EVENT_ERROR. Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
			break;
		}
		default: {
			Log_Printf(LOGLEVEL_INFO, "Other event id:%d", event->event_id);
			break;
		}
	}
}
#endif

void Mqtt_ClientCallback(const char *topic_buf, uint32_t topic_length, const char *payload_buf, uint32_t payload_length) {
#ifdef MQTT_ENABLE
	// If message's size is zero => discard (https://forum.espuino.de/t/mqtt-broker-verbindung-von-iobroker-schaltet-espuino-aus/3167)
	if (!payload_length || !topic_length) {
		return;
	}

	// Create null-terminated copies
	const std::string topic_str(topic_buf, topic_length); // Copies data + adds '\0'
	const std::string payload_str(payload_buf, payload_length); // Copies data + adds '\0'

	Log_Printf(LOGLEVEL_INFO, mqttMsgReceived, topic_str.c_str(), payload_str.c_str());

	// Define the prefix to match (e.g., "base_topic/device_id/" or "device_id/")
	char prefix[MQTT_TOPIC_MAX_LENGTH];
	if (gBaseTopic.length() > 0) {
		snprintf(prefix, sizeof(prefix), "%s/%s/", gBaseTopic.c_str(), gDeviceId.c_str());
	} else {
		snprintf(prefix, sizeof(prefix), "%s/", gDeviceId.c_str());
	}

	// Check if the topic starts with the prefix
	// also check if the topic ends with "/set" (the setter token)
	if ((strncmp(topic_str.c_str(), prefix, strlen(prefix)) == 0) && (topic_str.length() > strlen(prefix) && topic_str.substr(topic_str.length() - 4) == "/set")) {

		// Remove the prefix from the topic
		std::string reduced_topic_str = std::string(topic_str.c_str() + strlen(prefix));

		// Go to sleep?
		if (reduced_topic_str == topicSleep) {
			if (payload_str == "OFF" || payload_str == "0") {
				System_RequestSleep();
			}
		}
		// New track to play? Take RFID-ID as input
		else if (reduced_topic_str == topicRfid) {
			if (payload_str.size() >= (cardIdStringSize - 1)) {
				xQueueSend(gRfidCardQueue, payload_str.data(), 0);
			} else {
				System_IndicateError();
			}
		}
		// Loudness to change?
		else if (reduced_topic_str == topicLoudness) {
			unsigned long vol = toNumber<uint32_t>(payload_str);
			AudioPlayer_SetVolume(vol, true);
		}
		// Modify sleep-timer?
		else if (reduced_topic_str == topicSleepTimer) {
			if (gPlayProperties.playMode == NO_PLAYLIST) { // Don't allow sleep-modications if no playlist is active
				Log_Println(modificatorNotallowedWhenIdle, LOGLEVEL_INFO);
				publishMqtt(topicSleep, static_cast<uint32_t>(0), false);
				System_IndicateError();
				return;
			}
			if (payload_str == "EOP") {
				gPlayProperties.sleepAfterPlaylist = true;
				Log_Println(sleepTimerEOP, LOGLEVEL_NOTICE);
				publishMqtt(topicSleepTimer, "EOP", false);
				Led_SetNightmode(true);
				System_IndicateOk();
				return;
			} else if (payload_str == "EOT") {
				gPlayProperties.sleepAfterCurrentTrack = true;
				Log_Println(sleepTimerEOT, LOGLEVEL_NOTICE);
				publishMqtt(topicSleepTimer, "EOT", false);
				Led_SetNightmode(true);
				System_IndicateOk();
				return;
			} else if (payload_str == "EO5T") {
				if (gPlayProperties.playMode == NO_PLAYLIST || !gPlayProperties.playlist) {
					Log_Println(modificatorNotallowedWhenIdle, LOGLEVEL_NOTICE);
					System_IndicateError();
					return;
				}
				if ((gPlayProperties.playlist->size() - 1) >= (gPlayProperties.currentTrackNumber + 5)) {
					gPlayProperties.playUntilTrackNumber = gPlayProperties.currentTrackNumber + 5;
				} else {
					gPlayProperties.sleepAfterPlaylist = true; // If +5 tracks is > than active playlist, take end of current playlist
				}
				Log_Println(sleepTimerEO5, LOGLEVEL_NOTICE);
				publishMqtt(topicSleepTimer, "EO5T", false);
				Led_SetNightmode(true);
				System_IndicateOk();
				return;
			} else if (payload_str == "0") { // Disable sleep after it was active previously
				if (System_IsSleepTimerEnabled()) {
					System_DisableSleepTimer();
					Log_Println(sleepTimerStop, LOGLEVEL_NOTICE);
					System_IndicateOk();
					Led_SetNightmode(false);
					publishMqtt(topicSleep, static_cast<uint32_t>(0), false);
					gPlayProperties.sleepAfterPlaylist = false;
					gPlayProperties.sleepAfterCurrentTrack = false;
					gPlayProperties.playUntilTrackNumber = 0;
				} else {
					Log_Println(sleepTimerAlreadyStopped, LOGLEVEL_INFO);
					System_IndicateError();
				}
				return;
			}
			System_SetSleepTimer(toNumber<uint8_t>(payload_str));
			Log_Printf(LOGLEVEL_NOTICE, sleepTimerSetTo, System_GetSleepTimer());
			System_IndicateOk();

			gPlayProperties.sleepAfterPlaylist = false;
			gPlayProperties.sleepAfterCurrentTrack = false;
		}
		// Track-control (pause/play, stop, first, last, next, previous)
		else if (reduced_topic_str == topicTrackControl) {
			uint8_t controlCommand = toNumber<uint8_t>(payload_str);
			AudioPlayer_SetTrackControl(controlCommand);
		}

		// Check if controls should be locked
		else if (reduced_topic_str == topicLockControls) {
			if (payload_str == "OFF") {
				System_SetLockControls(false);
				Log_Println(allowButtons, LOGLEVEL_NOTICE);
				publishMqtt(topicLockControls, "OFF", false);
				System_IndicateOk();
			} else if (payload_str == "ON") {
				System_SetLockControls(true);
				Log_Println(lockButtons, LOGLEVEL_NOTICE);
				publishMqtt(topicLockControls, "ON", false);
				System_IndicateOk();
			}
		}

		// Check if playmode should be adjusted
		else if (reduced_topic_str == topicRepeatMode) {
			uint8_t repeatMode = toNumber<uint8_t>(payload_str);
			Log_Printf(LOGLEVEL_NOTICE, "Repeat: %d", repeatMode);
			if (gPlayProperties.playMode != NO_PLAYLIST) {
				if (gPlayProperties.playMode == NO_PLAYLIST) {
					publishMqtt(topicRepeatMode, static_cast<uint32_t>(AudioPlayer_GetRepeatMode()), false);
					Log_Println(noPlaylistNotAllowedMqtt, LOGLEVEL_ERROR);
					System_IndicateError();
				} else {
					switch (repeatMode) {
						case NO_REPEAT:
							gPlayProperties.repeatCurrentTrack = false;
							gPlayProperties.repeatPlaylist = false;
							publishMqtt(topicRepeatMode, static_cast<uint32_t>(AudioPlayer_GetRepeatMode()), false);
							Log_Println(modeRepeatNone, LOGLEVEL_INFO);
							System_IndicateOk();
							break;

						case TRACK:
							gPlayProperties.repeatCurrentTrack = true;
							gPlayProperties.repeatPlaylist = false;
							publishMqtt(topicRepeatMode, static_cast<uint32_t>(AudioPlayer_GetRepeatMode()), false);
							Log_Println(modeRepeatTrack, LOGLEVEL_INFO);
							System_IndicateOk();
							break;

						case PLAYLIST:
							gPlayProperties.repeatCurrentTrack = false;
							gPlayProperties.repeatPlaylist = true;
							publishMqtt(topicRepeatMode, static_cast<uint32_t>(AudioPlayer_GetRepeatMode()), false);
							Log_Println(modeRepeatPlaylist, LOGLEVEL_INFO);
							System_IndicateOk();
							break;

						case TRACK_N_PLAYLIST:
							gPlayProperties.repeatCurrentTrack = true;
							gPlayProperties.repeatPlaylist = true;
							publishMqtt(topicRepeatMode, static_cast<uint32_t>(AudioPlayer_GetRepeatMode()), false);
							Log_Println(modeRepeatTracknPlaylist, LOGLEVEL_INFO);
							System_IndicateOk();
							break;

						default:
							System_IndicateError();
							publishMqtt(topicRepeatMode, static_cast<uint32_t>(AudioPlayer_GetRepeatMode()), false);
							break;
					}
				}
			}
		}

		// Check if LEDs should be dimmed
		else if (reduced_topic_str == topicLedBrightness) {
			Led_SetBrightness(toNumber<uint8_t>(payload_str));
		}

		// Requested something that isn't specified?
		else {
			Log_Printf(LOGLEVEL_ERROR, noValidTopic, topic_str.c_str());
			System_IndicateError();
		}
	} else {
		// Topic doesn't start with the prefix
		Log_Printf(LOGLEVEL_ERROR, noValidTopic, topic_str.c_str());
		System_IndicateError();
	}

#endif
}
