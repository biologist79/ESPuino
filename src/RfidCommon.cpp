#include <Arduino.h>
#include "settings.h"

#include "AudioPlayer.h"
#include "Cmd.h"
#include "Common.h"
#include "Log.h"
#include "MemX.h"
#include "Mqtt.h"
#include "Queues.h"
#include "Rfid.h"
#include "RfidConfig.h"
#include "System.h"
#include "Web.h"

unsigned long Rfid_LastRfidCheckTimestamp = 0;
char gCurrentRfidTagId[cardIdStringSize] = ""; // No crap here as otherwise it could be shown in GUI
char gOldRfidTagId[cardIdStringSize] = "X"; // Init with crap

bool Rfid_FormatCardId(const uint8_t *cardId, size_t cardIdLen, char *target, size_t targetLen) {
	if (cardId == nullptr || target == nullptr || targetLen == 0) {
		return false;
	}
	target[0] = '\0';

	if (cardIdLen != cardIdSize || targetLen < cardIdStringSize) {
		return false;
	}

	size_t pos = 0;
	for (size_t i = 0; i < cardIdSize; i++) {
		const size_t remaining = targetLen - pos;
		const int written = snprintf(target + pos, remaining, "%03u", static_cast<unsigned>(cardId[i]));
		if (written < 0 || static_cast<size_t>(written) >= remaining) {
			target[0] = '\0';
			return false;
		}
		pos += static_cast<size_t>(written);
	}

	if (pos != cardIdStringSize - 1u) {
		target[0] = '\0';
		return false;
	}

	target[cardIdStringSize - 1u] = '\0';
	return true;
}

// Tries to lookup RFID-tag-string in NVS and extracts parameter from it if found
void Rfid_PreferenceLookupHandler(void) {
#if defined(RFID_READER_TYPE_RUNTIME)
	BaseType_t rfidStatus;
	char rfidTagId[cardIdStringSize];
	char _file[255];
	uint32_t _lastPlayPos = 0;
	uint16_t _trackLastPlayed = 0;
	uint32_t _playMode = 1;

	rfidStatus = xQueueReceive(gRfidCardQueue, &rfidTagId, 0);
	if (rfidStatus == pdPASS) {
		System_UpdateActivityTimer();
		strncpy(gCurrentRfidTagId, rfidTagId, cardIdStringSize - 1);
		Log_Printf(LOGLEVEL_INFO, "%s: %s", rfidTagReceived, gCurrentRfidTagId);
		Web_SendWebsocketData(0, WebsocketCodeType::CurrentRfid); // Push new rfidTagId to all websocket-clients
		String s = "-1";
		if (!System_RfidPrefsAvailable()) {
			Log_Println("RFID NVS namespace is not available; treating card as unknown", LOGLEVEL_ERROR);
		} else if (gPrefsRfid.isKey(gCurrentRfidTagId)) {
			s = gPrefsRfid.getString(gCurrentRfidTagId, "-1"); // Try to lookup rfidId in NVS
		}
		if (!s.compareTo("-1")) {
			Log_Println(rfidTagUnknownInNvs, LOGLEVEL_ERROR);
			System_IndicateError();
			// allow to escape from bluetooth mode with an unknown card, switch back to normal mode
			System_SetOperationMode(OPMODE_NORMAL);
			return;
		}

		char *token;
		uint8_t i = 1;
		token = strtok((char *) s.c_str(), stringDelimiter);
		while (token != NULL) { // Try to extract data from string after lookup
			if (i == 1) {
				strncpy(_file, token, sizeof(_file) / sizeof(_file[0]));
			} else if (i == 2) {
				_lastPlayPos = strtoul(token, NULL, 10);
			} else if (i == 3) {
				_playMode = strtoul(token, NULL, 10);
			} else if (i == 4) {
				_trackLastPlayed = strtoul(token, NULL, 10);
			}
			i++;
			token = strtok(NULL, stringDelimiter);
		}

		if (i != 5) {
			Log_Println(errorOccuredNvs, LOGLEVEL_ERROR);
			System_IndicateError();
		} else {
			// Only pass file to queue if strtok revealed 3 items
			if (_playMode >= 100) {
				// Modification-cards can change some settings (e.g. introducing track-looping or sleep after track/playlist).
				Cmd_Action(_playMode);
			} else {
				if (gPlayProperties.dontAcceptRfidTwice) {
					if (strncmp(gCurrentRfidTagId, gOldRfidTagId, 12) == 0) {
						Log_Printf(LOGLEVEL_ERROR, dontAccepctSameRfid, gCurrentRfidTagId);
						// System_IndicateError(); // Enable to have shown error @neopixel every time
						return;
					} else {
						strncpy(gOldRfidTagId, gCurrentRfidTagId, 12);
					}
				}
	#ifdef MQTT_ENABLE
				publishMqtt(topicRfid, gCurrentRfidTagId, false);
	#endif

	#ifdef BLUETOOTH_ENABLE
				// if music rfid was read, go back to normal mode
				if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) {
					System_SetOperationMode(OPMODE_NORMAL);
				}
	#endif

				AudioPlayer_SetPlaylist(_file, _lastPlayPos, _playMode, _trackLastPlayed);
			}
		}
	}
#endif
}

void Rfid_ResetOldRfid() {
	strncpy(gOldRfidTagId, "X", cardIdStringSize - 1);
}

#if defined(RFID_READER_TYPE_RUNTIME)
extern TaskHandle_t rfidTaskHandle;
#endif

void Rfid_TaskPause(void) {
#if defined(RFID_READER_TYPE_RUNTIME)
	if (rfidTaskHandle != NULL) {
		vTaskSuspend(rfidTaskHandle);
	}
#endif
}
void Rfid_TaskResume(void) {
#if defined(RFID_READER_TYPE_RUNTIME)
	if (rfidTaskHandle != NULL) {
		Rfid_TaskReset(); // Reset state machine to initial state
		vTaskResume(rfidTaskHandle);
	}
#endif
}
