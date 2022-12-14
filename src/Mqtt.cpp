#include <Arduino.h>
#include <WiFi.h>
#include "settings.h"
#include "Mqtt.h"
#include "AudioPlayer.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "System.h"
#include "Queues.h"
#include "Wlan.h"
#include "revision.h"

#ifdef MQTT_ENABLE
	#define MQTT_SOCKET_TIMEOUT 1 // https://github.com/knolleary/pubsubclient/issues/403
	#include <PubSubClient.h>
#endif

// MQTT-helper
#ifdef MQTT_ENABLE
	static WiFiClient Mqtt_WifiClient;
	static PubSubClient Mqtt_PubSubClient(Mqtt_WifiClient);
	// Please note: all of them are defaults that can be changed later via GUI
	String gMqttClientId = DEVICE_HOSTNAME; // ClientId for the MQTT-server, must be server wide unique (if not found in NVS this one will be taken)
	String gMqttServer = "192.168.2.43";    // IP-address of MQTT-server (if not found in NVS this one will be taken)
	String gMqttUser = "mqtt-user";         // MQTT-user
	String gMqttPassword = "mqtt-password"; // MQTT-password
	uint16_t gMqttPort = 1883;              // MQTT-Port
#endif

// MQTT
static bool Mqtt_Enabled = true;

static void Mqtt_ClientCallback(const char *topic, const byte *payload, uint32_t length);
static bool Mqtt_Reconnect(void);
static void Mqtt_PostWiFiRssi(void);

void Mqtt_Init() {
	#ifdef MQTT_ENABLE
		// Get MQTT-enable from NVS
		uint8_t nvsEnableMqtt = gPrefsSettings.getUChar("enableMQTT", 99);
		switch (nvsEnableMqtt) {
			case 99:
				gPrefsSettings.putUChar("enableMQTT", Mqtt_Enabled);
				Log_Println((char *) FPSTR(wroteMqttFlagToNvs), LOGLEVEL_ERROR);
				break;
			case 1:
				Mqtt_Enabled = nvsEnableMqtt;
				snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(restoredMqttActiveFromNvs), nvsEnableMqtt);
				Log_Println(Log_Buffer, LOGLEVEL_INFO);
				break;
			case 0:
				Mqtt_Enabled = nvsEnableMqtt;
				snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(restoredMqttDeactiveFromNvs), nvsEnableMqtt);
				Log_Println(Log_Buffer, LOGLEVEL_INFO);
				break;
		}

		// Get MQTT-clientid from NVS
		String nvsMqttClientId = gPrefsSettings.getString("mqttClientId", "-1");
		if (!nvsMqttClientId.compareTo("-1")) {
			gPrefsSettings.putString("mqttClientId", gMqttClientId);
			Log_Println((char *) FPSTR(wroteMqttClientIdToNvs), LOGLEVEL_ERROR);
		} else {
			gMqttClientId = nvsMqttClientId;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(restoredMqttClientIdFromNvs), nvsMqttClientId.c_str());
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		}

		// Get MQTT-server from NVS
		String nvsMqttServer = gPrefsSettings.getString("mqttServer", "-1");
		if (!nvsMqttServer.compareTo("-1")) {
			gPrefsSettings.putString("mqttServer", gMqttServer);
			Log_Println((char *) FPSTR(wroteMqttServerToNvs), LOGLEVEL_ERROR);
		} else {
			gMqttServer = nvsMqttServer;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(restoredMqttServerFromNvs), nvsMqttServer.c_str());
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		}

		// Get MQTT-user from NVS
		String nvsMqttUser = gPrefsSettings.getString("mqttUser", "-1");
		if (!nvsMqttUser.compareTo("-1")) {
			gPrefsSettings.putString("mqttUser", (String)gMqttUser);
			Log_Println((char *) FPSTR(wroteMqttUserToNvs), LOGLEVEL_ERROR);
		} else {
			gMqttUser = nvsMqttUser;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(restoredMqttUserFromNvs), nvsMqttUser.c_str());
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		}

		// Get MQTT-password from NVS
		String nvsMqttPassword = gPrefsSettings.getString("mqttPassword", "-1");
		if (!nvsMqttPassword.compareTo("-1")) {
			gPrefsSettings.putString("mqttPassword", (String)gMqttPassword);
			Log_Println((char *) FPSTR(wroteMqttPwdToNvs), LOGLEVEL_ERROR);
		} else {
			gMqttPassword = nvsMqttPassword;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(restoredMqttPwdFromNvs), nvsMqttPassword.c_str());
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		}

		// Get MQTT-password from NVS
		uint32_t nvsMqttPort = gPrefsSettings.getUInt("mqttPort", 99999);
		if (nvsMqttPort == 99999) {
			gPrefsSettings.putUInt("mqttPort", gMqttPort);
		} else {
			gMqttPort = nvsMqttPort;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(restoredMqttPortFromNvs), gMqttPort);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		}

		// Only enable MQTT if requested
		if (Mqtt_Enabled) {
			Mqtt_PubSubClient.setServer(gMqttServer.c_str(), gMqttPort);
			Mqtt_PubSubClient.setCallback(Mqtt_ClientCallback);
		}
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
		publishMqtt((char *) FPSTR(topicState), "Offline", false);
		publishMqtt((char *) FPSTR(topicTrackState), "---", false);
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
				//delay(100);
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
			publishMqtt((char *) FPSTR(topicWiFiRssiState), Wlan_GetRssi(), false);
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
			snprintf(Log_Buffer, Log_BufferLength, "%s %s", (char *) FPSTR(tryConnectMqttS), gMqttServer.c_str());
			Log_Println(Log_Buffer, LOGLEVEL_NOTICE);

			// Try to connect to MQTT-server. If username AND password are set, they'll be used
			if ((gMqttUser.length() < 1u) || (gMqttPassword.length()) < 1u) {
				Log_Println((char *) FPSTR(mqttWithoutPwd), LOGLEVEL_NOTICE);
				if (Mqtt_PubSubClient.connect(gMqttClientId.c_str())) {
					connect = true;
				}
			} else {
				Log_Println((char *) FPSTR(mqttWithPwd), LOGLEVEL_NOTICE);
				if (Mqtt_PubSubClient.connect(gMqttClientId.c_str(), gMqttUser.c_str(), gMqttPassword.c_str(), (char *) FPSTR(topicState), 0, false, "Offline")) {
					connect = true;
				}
			}
			if (connect) {
				Log_Println((char *) FPSTR(mqttOk), LOGLEVEL_NOTICE);

				// Deepsleep-subscription
				Mqtt_PubSubClient.subscribe((char *) FPSTR(topicSleepCmnd));

				// RFID-Tag-ID-subscription
				Mqtt_PubSubClient.subscribe((char *) FPSTR(topicRfidCmnd));

				// Loudness-subscription
				Mqtt_PubSubClient.subscribe((char *) FPSTR(topicLoudnessCmnd));

				// Sleep-Timer-subscription
				Mqtt_PubSubClient.subscribe((char *) FPSTR(topicSleepTimerCmnd));

				// Next/previous/stop/play-track-subscription
				Mqtt_PubSubClient.subscribe((char *) FPSTR(topicTrackControlCmnd));

				// Lock controls
				Mqtt_PubSubClient.subscribe((char *) FPSTR(topicLockControlsCmnd));

				// Current repeat-Mode
				Mqtt_PubSubClient.subscribe((char *) FPSTR(topicRepeatModeCmnd));

				// LED-brightness
				Mqtt_PubSubClient.subscribe((char *) FPSTR(topicLedBrightnessCmnd));

				// Publish current state
				publishMqtt((char *) FPSTR(topicState), "Online", false);
				publishMqtt((char *) FPSTR(topicTrackState), gPlayProperties.title, false);
				publishMqtt((char *) FPSTR(topicCoverChangedState), "", false);
				publishMqtt((char *) FPSTR(topicLoudnessState), AudioPlayer_GetCurrentVolume(), false);
				publishMqtt((char *) FPSTR(topicSleepTimerState), System_GetSleepTimerTimeStamp(), false);
				publishMqtt((char *) FPSTR(topicLockControlsState), System_AreControlsLocked(), false);
				publishMqtt((char *) FPSTR(topicPlaymodeState), gPlayProperties.playMode, false);
				publishMqtt((char *) FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
				publishMqtt((char *) FPSTR(topicCurrentIPv4IP), Wlan_GetIpAddress().c_str(), false);
				publishMqtt((char *) FPSTR(topicRepeatModeState), AudioPlayer_GetRepeatMode(), false);

				char revBuf[12];
				strncpy(revBuf, softwareRevision+19, sizeof(revBuf)-1);
				revBuf[sizeof(revBuf)-1] = '\0';
				publishMqtt((char *) FPSTR(topicSRevisionState), revBuf, false);

				return Mqtt_PubSubClient.connected();
			} else {
				snprintf(Log_Buffer, Log_BufferLength, "%s: rc=%i (%d / %d)", (char *) FPSTR(mqttConnFailed), Mqtt_PubSubClient.state(), i, mqttMaxRetriesPerInterval);
				Log_Println(Log_Buffer, LOGLEVEL_ERROR);
			}
		}
		return false;
	#else
		return false;
	#endif
}

// Is called if there's a new MQTT-message for us
void Mqtt_ClientCallback(const char *topic, const byte *payload, uint32_t length) {
	#ifdef MQTT_ENABLE
		char *receivedString = (char*)x_calloc(length + 1u, sizeof(char));
		memcpy(receivedString, (char *) payload, length);
		char *mqttTopic = x_strdup(topic);

		snprintf(Log_Buffer, Log_BufferLength, "%s: [Topic: %s] [Command: %s]", (char *) FPSTR(mqttMsgReceived), mqttTopic, receivedString);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);

		// Go to sleep?
		if (strcmp_P(topic, topicSleepCmnd) == 0) {
			if ((strcmp(receivedString, "OFF") == 0) || (strcmp(receivedString, "0") == 0)) {
				System_RequestSleep();
			}
		}
		// New track to play? Take RFID-ID as input
		else if (strcmp_P(topic, topicRfidCmnd) == 0) {
			char *_rfidId = x_strdup(receivedString);
			xQueueSend(gRfidCardQueue, _rfidId, 0);
		}
		// Loudness to change?
		else if (strcmp_P(topic, topicLoudnessCmnd) == 0) {
			unsigned long vol = strtoul(receivedString, NULL, 10);
			AudioPlayer_VolumeToQueueSender(vol, true);
		}
		// Modify sleep-timer?
		else if (strcmp_P(topic, topicSleepTimerCmnd) == 0) {
			if (gPlayProperties.playMode == NO_PLAYLIST) { // Don't allow sleep-modications if no playlist is active
				Log_Println((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_INFO);
				publishMqtt((char *) FPSTR(topicSleepState), 0, false);
				System_IndicateError();
				return;
			}
			if (strcmp(receivedString, "EOP") == 0) {
				gPlayProperties.sleepAfterPlaylist = true;
				Log_Println((char *) FPSTR(sleepTimerEOP), LOGLEVEL_NOTICE);
				publishMqtt((char *) FPSTR(topicSleepTimerState), "EOP", false);
				Led_ResetToNightBrightness();
				publishMqtt((char *) FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
				System_IndicateOk();
				return;
			} else if (strcmp(receivedString, "EOT") == 0) {
				gPlayProperties.sleepAfterCurrentTrack = true;
				Log_Println((char *) FPSTR(sleepTimerEOT), LOGLEVEL_NOTICE);
				publishMqtt((char *) FPSTR(topicSleepTimerState), "EOT", false);
				Led_ResetToNightBrightness();
				publishMqtt((char *) FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
				System_IndicateOk();
				return;
			} else if (strcmp(receivedString, "EO5T") == 0) {
				if ((gPlayProperties.numberOfTracks - 1) >= (gPlayProperties.currentTrackNumber + 5)) {
					gPlayProperties.playUntilTrackNumber = gPlayProperties.currentTrackNumber + 5;
				} else {
					gPlayProperties.sleepAfterPlaylist = true;  // If +5 tracks is > than active playlist, take end of current playlist
				}
				Log_Println((char *) FPSTR(sleepTimerEO5), LOGLEVEL_NOTICE);
				publishMqtt((char *) FPSTR(topicSleepTimerState), "EO5T", false);
				Led_ResetToNightBrightness();
				publishMqtt((char *) FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
				System_IndicateOk();
				return;
			} else if (strcmp(receivedString, "0") == 0) {  // Disable sleep after it was active previously
				if (System_IsSleepTimerEnabled()) {
					System_DisableSleepTimer();
					Log_Println((char *) FPSTR(sleepTimerStop), LOGLEVEL_NOTICE);
					System_IndicateOk();
					publishMqtt((char *) FPSTR(topicSleepState), 0, false);
					publishMqtt((char *) FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
					gPlayProperties.sleepAfterPlaylist = false;
					gPlayProperties.sleepAfterCurrentTrack = false;
					gPlayProperties.playUntilTrackNumber = 0;
					return;
				} else {
					Log_Println((char *) FPSTR(sleepTimerAlreadyStopped), LOGLEVEL_INFO);
					System_IndicateError();
					return;
				}
			}
			System_SetSleepTimer((uint8_t)strtoul(receivedString, NULL, 10));
			snprintf(Log_Buffer, Log_BufferLength, "%s: %u Minute(n)", (char *) FPSTR(sleepTimerSetTo), System_GetSleepTimer());
			Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
			publishMqtt((char *) FPSTR(topicSleepTimerState), System_GetSleepTimer(), false);
			System_IndicateOk();

			gPlayProperties.sleepAfterPlaylist = false;
			gPlayProperties.sleepAfterCurrentTrack = false;
		}
		// Track-control (pause/play, stop, first, last, next, previous)
		else if (strcmp_P(topic, topicTrackControlCmnd) == 0) {
			uint8_t controlCommand = strtoul(receivedString, NULL, 10);
			AudioPlayer_TrackControlToQueueSender(controlCommand);
		}

		// Check if controls should be locked
		else if (strcmp_P(topic, topicLockControlsCmnd) == 0) {
			if (strcmp(receivedString, "OFF") == 0) {
				System_SetLockControls(false);
				Log_Println((char *) FPSTR(allowButtons), LOGLEVEL_NOTICE);
				publishMqtt((char *) FPSTR(topicLockControlsState), "OFF", false);
				System_IndicateOk();
			} else if (strcmp(receivedString, "ON") == 0) {
				System_SetLockControls(true);
				Log_Println((char *) FPSTR(lockButtons), LOGLEVEL_NOTICE);
				publishMqtt((char *) FPSTR(topicLockControlsState), "ON", false);
				System_IndicateOk();
			}
		}

		// Check if playmode should be adjusted
		else if (strcmp_P(topic, topicRepeatModeCmnd) == 0) {
			uint8_t repeatMode = strtoul(receivedString, NULL, 10);
			snprintf(Log_Buffer, Log_BufferLength, "Repeat: %d", repeatMode);
			Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
			if (gPlayProperties.playMode != NO_PLAYLIST) {
				if (gPlayProperties.playMode == NO_PLAYLIST) {
					publishMqtt((char *) FPSTR(topicRepeatModeState), AudioPlayer_GetRepeatMode(), false);
					Log_Println((char *) FPSTR(noPlaylistNotAllowedMqtt), LOGLEVEL_ERROR);
					System_IndicateError();
				} else {
					switch (repeatMode) {
						case NO_REPEAT:
							gPlayProperties.repeatCurrentTrack = false;
							gPlayProperties.repeatPlaylist = false;
							publishMqtt((char *) FPSTR(topicRepeatModeState), AudioPlayer_GetRepeatMode(), false);
							Log_Println((char *) FPSTR(modeRepeatNone), LOGLEVEL_INFO);
							System_IndicateOk();
							break;

						case TRACK:
							gPlayProperties.repeatCurrentTrack = true;
							gPlayProperties.repeatPlaylist = false;
							publishMqtt((char *) FPSTR(topicRepeatModeState), AudioPlayer_GetRepeatMode(), false);
							Log_Println((char *) FPSTR(modeRepeatTrack), LOGLEVEL_INFO);
							System_IndicateOk();
							break;

						case PLAYLIST:
							gPlayProperties.repeatCurrentTrack = false;
							gPlayProperties.repeatPlaylist = true;
							publishMqtt((char *) FPSTR(topicRepeatModeState), AudioPlayer_GetRepeatMode(), false);
							Log_Println((char *) FPSTR(modeRepeatPlaylist), LOGLEVEL_INFO);
							System_IndicateOk();
							break;

						case TRACK_N_PLAYLIST:
							gPlayProperties.repeatCurrentTrack = true;
							gPlayProperties.repeatPlaylist = true;
							publishMqtt((char *) FPSTR(topicRepeatModeState), AudioPlayer_GetRepeatMode(), false);
							Log_Println((char *) FPSTR(modeRepeatTracknPlaylist), LOGLEVEL_INFO);
							System_IndicateOk();
							break;

						default:
							System_IndicateError();
							publishMqtt((char *) FPSTR(topicRepeatModeState), AudioPlayer_GetRepeatMode(), false);
							break;
					}
				}
			}
		}

		// Check if LEDs should be dimmed
		else if (strcmp_P(topic, topicLedBrightnessCmnd) == 0) {
			Led_SetBrightness(strtoul(receivedString, NULL, 10));
			publishMqtt((char *) FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
		}

		// Requested something that isn't specified?
		else {
			snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(noValidTopic), topic);
			Log_Println(Log_Buffer, LOGLEVEL_ERROR);
			System_IndicateError();
		}

		free(receivedString);
		free(mqttTopic);
	#endif
}
