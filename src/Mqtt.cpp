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
#include "revision.h"

#include <Rfid.h>
#include <WiFi.h>

#ifdef MQTT_ENABLE
	#define MQTT_SOCKET_TIMEOUT 1 // https://github.com/knolleary/pubsubclient/issues/403
	#include <PubSubClient.h>
#endif

#include <charconv>
#include <limits>
#include <string_view>

// MQTT-helper
#ifdef MQTT_ENABLE
static WiFiClient Mqtt_WifiClient;
static PubSubClient Mqtt_PubSubClient(Mqtt_WifiClient);
// Please note: all of them are defaults that can be changed later via GUI
String gMqttClientId = DEVICE_HOSTNAME; // ClientId for the MQTT-server, must be server wide unique (if not found in NVS this one will be taken)
String gMqttServer = "192.168.2.43"; // IP-address of MQTT-server (if not found in NVS this one will be taken)
String gMqttUser = "mqtt-user"; // MQTT-user
String gMqttPassword = "mqtt-password"; // MQTT-password
uint16_t gMqttPort = 1883; // MQTT-Port
#endif

// MQTT
static bool Mqtt_Enabled = true;

#ifdef MQTT_ENABLE
static void Mqtt_ClientCallback(const char *topic, const byte *payload, uint32_t length);
static bool Mqtt_Reconnect(void);
static void Mqtt_PostWiFiRssi(void);
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

	// Get MQTT-clientid from NVS
	String nvsMqttClientId = gPrefsSettings.getString("mqttClientId", "-1");
	if (!nvsMqttClientId.compareTo("-1")) {
		gPrefsSettings.putString("mqttClientId", gMqttClientId);
		Log_Println(wroteMqttClientIdToNvs, LOGLEVEL_ERROR);
	} else {
		gMqttClientId = nvsMqttClientId;
		Log_Printf(LOGLEVEL_INFO, restoredMqttClientIdFromNvs, nvsMqttClientId.c_str());
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
		Mqtt_PubSubClient.setServer(gMqttServer.c_str(), gMqttPort);
		Mqtt_PubSubClient.setCallback(Mqtt_ClientCallback);
	}
#else
	Mqtt_Enabled = false;
#endif
}

void Mqtt_Cyclic(void) {
#ifdef MQTT_ENABLE
	if (Mqtt_Enabled && Wlan_IsConnected()) {
		Mqtt_Reconnect();
		Mqtt_PubSubClient.loop();
		Mqtt_PostWiFiRssi();
	}
#endif
}

void Mqtt_Exit(void) {
#ifdef MQTT_ENABLE
	Log_Println("shutdown MQTT..", LOGLEVEL_NOTICE);
	publishMqtt(topicState, "Offline", false);
	publishMqtt(topicTrackState, "---", false);
	Mqtt_PubSubClient.disconnect();
#endif
}

bool Mqtt_IsEnabled(void) {
	return Mqtt_Enabled;
}

/* Wrapper-functions for MQTT-publish */
bool publishMqtt(const char *topic, const char *payload, bool retained) {
#ifdef MQTT_ENABLE
	if (strcmp(topic, "") != 0) {
		if (Mqtt_PubSubClient.connected()) {
			Mqtt_PubSubClient.publish(topic, payload, retained);
			// delay(100);
			return true;
		}
	}
#endif

	return false;
}

bool publishMqtt(const char *topic, int32_t payload, bool retained) {
#ifdef MQTT_ENABLE
	char buf[11];
	snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%d", payload);
	return publishMqtt(topic, buf, retained);
#else
	return false;
#endif
}

bool publishMqtt(const char *topic, unsigned long payload, bool retained) {
#ifdef MQTT_ENABLE
	char buf[11];
	snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%lu", payload);
	return publishMqtt(topic, buf, retained);
#else
	return false;
#endif
}

bool publishMqtt(const char *topic, uint32_t payload, bool retained) {
#ifdef MQTT_ENABLE
	char buf[11];
	snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%u", payload);
	return publishMqtt(topic, buf, retained);
#else
	return false;
#endif
}

// Cyclic posting of WiFi-signal-strength
void Mqtt_PostWiFiRssi(void) {
#ifdef MQTT_ENABLE
	static uint32_t lastMqttRssiTimestamp = 0;

	if (!lastMqttRssiTimestamp || (millis() - lastMqttRssiTimestamp >= 60000)) {
		lastMqttRssiTimestamp = millis();
		publishMqtt(topicWiFiRssiState, Wlan_GetRssi(), false);
	}
#endif
}

/* Connects/reconnects to MQTT-Broker unless connection is not already available.
	Manages MQTT-subscriptions.
*/
bool Mqtt_Reconnect() {
#ifdef MQTT_ENABLE
	static uint32_t mqttLastRetryTimestamp = 0u;
	uint8_t connect = false;
	uint8_t i = 0;

	if (!mqttLastRetryTimestamp || millis() - mqttLastRetryTimestamp >= mqttRetryInterval * 1000) {
		mqttLastRetryTimestamp = millis();
	} else {
		return false;
	}

	while (!Mqtt_PubSubClient.connected() && i < mqttMaxRetriesPerInterval) {
		i++;
		Log_Printf(LOGLEVEL_NOTICE, tryConnectMqttS, gMqttServer.c_str());

		// Try to connect to MQTT-server. If username AND password are set, they'll be used
		if ((gMqttUser.length() < 1u) || (gMqttPassword.length()) < 1u) {
			Log_Println(mqttWithoutPwd, LOGLEVEL_NOTICE);
			if (Mqtt_PubSubClient.connect(gMqttClientId.c_str())) {
				connect = true;
			}
		} else {
			Log_Println(mqttWithPwd, LOGLEVEL_NOTICE);
			if (Mqtt_PubSubClient.connect(gMqttClientId.c_str(), gMqttUser.c_str(), gMqttPassword.c_str(), topicState, 0, false, "Offline")) {
				connect = true;
			}
		}
		if (connect) {
			Log_Println(mqttOk, LOGLEVEL_NOTICE);

			// Deepsleep-subscription
			Mqtt_PubSubClient.subscribe(topicSleepCmnd);

			// RFID-Tag-ID-subscription
			Mqtt_PubSubClient.subscribe(topicRfidCmnd);

			// Loudness-subscription
			Mqtt_PubSubClient.subscribe(topicLoudnessCmnd);

			// Sleep-Timer-subscription
			Mqtt_PubSubClient.subscribe(topicSleepTimerCmnd);

			// Next/previous/stop/play-track-subscription
			Mqtt_PubSubClient.subscribe(topicTrackControlCmnd);

			// Lock controls
			Mqtt_PubSubClient.subscribe(topicLockControlsCmnd);

			// Current repeat-Mode
			Mqtt_PubSubClient.subscribe(topicRepeatModeCmnd);

			// LED-brightness
			Mqtt_PubSubClient.subscribe(topicLedBrightnessCmnd);

			// Publish current state
			publishMqtt(topicState, "Online", false);
			publishMqtt(topicTrackState, gPlayProperties.title, false);
			publishMqtt(topicCoverChangedState, "", false);
			publishMqtt(topicLoudnessState, AudioPlayer_GetCurrentVolume(), false);
			publishMqtt(topicSleepTimerState, System_GetSleepTimerTimeStamp(), false);
			publishMqtt(topicLockControlsState, System_AreControlsLocked(), false);
			publishMqtt(topicPlaymodeState, gPlayProperties.playMode, false);
			publishMqtt(topicLedBrightnessState, Led_GetBrightness(), false);
			publishMqtt(topicCurrentIPv4IP, Wlan_GetIpAddress().c_str(), false);
			publishMqtt(topicRepeatModeState, AudioPlayer_GetRepeatMode(), false);

			char revBuf[12];
			strncpy(revBuf, softwareRevision + 19, sizeof(revBuf) - 1);
			revBuf[sizeof(revBuf) - 1] = '\0';
			publishMqtt(topicSRevisionState, revBuf, false);

			return Mqtt_PubSubClient.connected();
		} else {
			Log_Printf(LOGLEVEL_ERROR, mqttConnFailed, Mqtt_PubSubClient.state(), i, mqttMaxRetriesPerInterval);
		}
	}
	return false;
#else
	return false;
#endif
}

template <typename NumberType>
static NumberType toNumber(const std::string_view str) {
	NumberType result;
	const auto [ptr, ec] = std::from_chars(str.cbegin(), str.cend(), result);

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
void Mqtt_ClientCallback(const char *topic, const byte *payload, uint32_t length) {
#ifdef MQTT_ENABLE
	// If message's size is zero => discard (https://forum.espuino.de/t/mqtt-broker-verbindung-von-iobroker-schaltet-espuino-aus/3167)
	if (!length) {
		return;
	}
	const std::string_view receivedString {reinterpret_cast<const char *>(payload), length};

	Log_Printf(LOGLEVEL_INFO, mqttMsgReceived, topic, receivedString.size(), receivedString.data());

	// Go to sleep?
	if (strcmp_P(topic, topicSleepCmnd) == 0) {
		if (receivedString == "OFF" || receivedString == "0") {
			System_RequestSleep();
		}
	}
	// New track to play? Take RFID-ID as input
	else if (strcmp_P(topic, topicRfidCmnd) == 0) {
		if (receivedString.size() >= (cardIdStringSize - 1)) {
			xQueueSend(gRfidCardQueue, receivedString.data(), 0);
		} else {
			System_IndicateError();
		}
	}
	// Loudness to change?
	else if (strcmp_P(topic, topicLoudnessCmnd) == 0) {
		unsigned long vol = toNumber<int32_t>(receivedString);
		AudioPlayer_VolumeToQueueSender(vol, true);
	}
	// Modify sleep-timer?
	else if (strcmp_P(topic, topicSleepTimerCmnd) == 0) {
		if (gPlayProperties.playMode == NO_PLAYLIST) { // Don't allow sleep-modications if no playlist is active
			Log_Println(modificatorNotallowedWhenIdle, LOGLEVEL_INFO);
			publishMqtt(topicSleepState, 0, false);
			System_IndicateError();
			return;
		}
		if (receivedString == "EOP") {
			gPlayProperties.sleepAfterPlaylist = true;
			Log_Println(sleepTimerEOP, LOGLEVEL_NOTICE);
			publishMqtt(topicSleepTimerState, "EOP", false);
			Led_SetNightmode(true);
			System_IndicateOk();
			return;
		} else if (receivedString == "EOT") {
			gPlayProperties.sleepAfterCurrentTrack = true;
			Log_Println(sleepTimerEOT, LOGLEVEL_NOTICE);
			publishMqtt(topicSleepTimerState, "EOT", false);
			Led_SetNightmode(true);
			System_IndicateOk();
			return;
		} else if (receivedString == "EO5T") {
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
			publishMqtt(topicSleepTimerState, "EO5T", false);
			Led_SetNightmode(true);
			System_IndicateOk();
			return;
		} else if (receivedString == "0") { // Disable sleep after it was active previously
			if (System_IsSleepTimerEnabled()) {
				System_DisableSleepTimer();
				Log_Println(sleepTimerStop, LOGLEVEL_NOTICE);
				System_IndicateOk();
				Led_SetNightmode(false);
				publishMqtt(topicSleepState, 0, false);
				gPlayProperties.sleepAfterPlaylist = false;
				gPlayProperties.sleepAfterCurrentTrack = false;
				gPlayProperties.playUntilTrackNumber = 0;
			} else {
				Log_Println(sleepTimerAlreadyStopped, LOGLEVEL_INFO);
				System_IndicateError();
			}
			return;
		}
		System_SetSleepTimer(toNumber<uint8_t>(receivedString));
		Log_Printf(LOGLEVEL_NOTICE, sleepTimerSetTo, System_GetSleepTimer());
		System_IndicateOk();

		gPlayProperties.sleepAfterPlaylist = false;
		gPlayProperties.sleepAfterCurrentTrack = false;
	}
	// Track-control (pause/play, stop, first, last, next, previous)
	else if (strcmp_P(topic, topicTrackControlCmnd) == 0) {
		uint8_t controlCommand = toNumber<uint8_t>(receivedString);
		AudioPlayer_TrackControlToQueueSender(controlCommand);
	}

	// Check if controls should be locked
	else if (strcmp_P(topic, topicLockControlsCmnd) == 0) {
		if (receivedString == "OFF") {
			System_SetLockControls(false);
			Log_Println(allowButtons, LOGLEVEL_NOTICE);
			publishMqtt(topicLockControlsState, "OFF", false);
			System_IndicateOk();
		} else if (receivedString == "ON") {
			System_SetLockControls(true);
			Log_Println(lockButtons, LOGLEVEL_NOTICE);
			publishMqtt(topicLockControlsState, "ON", false);
			System_IndicateOk();
		}
	}

	// Check if playmode should be adjusted
	else if (strcmp_P(topic, topicRepeatModeCmnd) == 0) {
		uint8_t repeatMode = toNumber<uint8_t>(receivedString);
		Log_Printf(LOGLEVEL_NOTICE, "Repeat: %d", repeatMode);
		if (gPlayProperties.playMode != NO_PLAYLIST) {
			if (gPlayProperties.playMode == NO_PLAYLIST) {
				publishMqtt(topicRepeatModeState, AudioPlayer_GetRepeatMode(), false);
				Log_Println(noPlaylistNotAllowedMqtt, LOGLEVEL_ERROR);
				System_IndicateError();
			} else {
				switch (repeatMode) {
					case NO_REPEAT:
						gPlayProperties.repeatCurrentTrack = false;
						gPlayProperties.repeatPlaylist = false;
						publishMqtt(topicRepeatModeState, AudioPlayer_GetRepeatMode(), false);
						Log_Println(modeRepeatNone, LOGLEVEL_INFO);
						System_IndicateOk();
						break;

					case TRACK:
						gPlayProperties.repeatCurrentTrack = true;
						gPlayProperties.repeatPlaylist = false;
						publishMqtt(topicRepeatModeState, AudioPlayer_GetRepeatMode(), false);
						Log_Println(modeRepeatTrack, LOGLEVEL_INFO);
						System_IndicateOk();
						break;

					case PLAYLIST:
						gPlayProperties.repeatCurrentTrack = false;
						gPlayProperties.repeatPlaylist = true;
						publishMqtt(topicRepeatModeState, AudioPlayer_GetRepeatMode(), false);
						Log_Println(modeRepeatPlaylist, LOGLEVEL_INFO);
						System_IndicateOk();
						break;

					case TRACK_N_PLAYLIST:
						gPlayProperties.repeatCurrentTrack = true;
						gPlayProperties.repeatPlaylist = true;
						publishMqtt(topicRepeatModeState, AudioPlayer_GetRepeatMode(), false);
						Log_Println(modeRepeatTracknPlaylist, LOGLEVEL_INFO);
						System_IndicateOk();
						break;

					default:
						System_IndicateError();
						publishMqtt(topicRepeatModeState, AudioPlayer_GetRepeatMode(), false);
						break;
				}
			}
		}
	}

	// Check if LEDs should be dimmed
	else if (strcmp_P(topic, topicLedBrightnessCmnd) == 0) {
		Led_SetBrightness(toNumber<uint8_t>(receivedString));
	}

	// Requested something that isn't specified?
	else {
		Log_Printf(LOGLEVEL_ERROR, noValidTopic, topic);
		System_IndicateError();
	}

#endif
}
