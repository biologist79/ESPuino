#include <Arduino.h>
#include "settings.h"

#include "AudioPlayer.h"
#include "Cmd.h"
#include "Common.h"
#include "Log.h"
#include "MediaHub.h"
#include "MemX.h"
#include "Mqtt.h"
#include "Queues.h"
#include "Rfid.h"
#include "RfidConfig.h"
#include "System.h"
#include "Web.h"

#include <atomic>

unsigned long Rfid_LastRfidCheckTimestamp = 0;
char gCurrentRfidTagId[cardIdStringSize] = ""; // No crap here as otherwise it could be shown in GUI
char gOldRfidTagId[cardIdStringSize] = "X"; // Init with crap

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
		if (gPrefsRfid.isKey(gCurrentRfidTagId)) {
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
				strncpy(_file, token, sizeof(_file) - 1);
				_file[sizeof(_file) - 1] = '\0';
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
						// If pause is active, resume playback when the same RFID is put on again.
						if (gPlayProperties.pausePlay && gPlayProperties.resumeOnSameRfid) {
							Log_Printf(LOGLEVEL_INFO, "Same RFID while paused -> resume playback (%s)", gCurrentRfidTagId);
							AudioPlayer_SetTrackControl(PAUSEPLAY);
							return;
						}
						Log_Printf(LOGLEVEL_ERROR, dontAccepctSameRfid, gCurrentRfidTagId);
						// System_IndicateError(); // Enable to have shown error @neopixel every time
						return;
					} else {
						strncpy(gOldRfidTagId, gCurrentRfidTagId, 12);
						// Arm the lock-reset now that a new tag was accepted. This must not depend on playback
						// actually starting, otherwise a tag whose first track fails immediately stays locked forever.
						AudioPlayer_ArmRfidResetOnIdle();
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

				if (_playMode == MEDIAHUB) {
					// Dispatch to MediaHub before the normal AudioPlayer logic gets
					// a chance to interpret _file (concept: mediahub-konzept.md §8).
					MediaHub_HandleCardTapped(gCurrentRfidTagId, _file, _lastPlayPos, _trackLastPlayed);
				} else {
					AudioPlayer_SetPlaylist(_file, _lastPlayPos, _playMode, _trackLastPlayed);
				}
			}
		}
	}
#endif
}

void Rfid_ResetOldRfid() {
	strncpy(gOldRfidTagId, "X", cardIdStringSize - 1);
}

// Set by Rfid_ResetLastTag(), consumed by the reader task. The reader's "same card re-applied"
// buffer is task-local, so it can only be cleared from within the task itself.
static std::atomic<bool> gResetLastTagRequested {false};

// Forget the tag that was last seen/accepted, in both places that remember it: gOldRfidTagId (the
// dontAcceptRfidTwice dedup) and the reader task's own last-card buffer (the pauseIfRfidRemoved dedup).
// Call this whenever something *other than the reader* changes what a tag means -- an assignment being
// written, deleted or restored, or playback being started from the web UI.
//
// Without this, re-applying a tag whose assignment just changed is short-circuited before the NVS lookup
// ever happens: in pauseIfRfidRemoved-mode the reader turns it into a play/pause toggle on the playlist
// that is still loaded, so the *old* book resumes. Both dedups live only in RAM, which is why a reboot or
// a deep-sleep cycle appears to "fix" it.
void Rfid_ResetLastTag() {
	Rfid_ResetOldRfid();
	gResetLastTagRequested.store(true, std::memory_order_relaxed);
}

bool Rfid_ConsumeLastTagReset() {
	return gResetLastTagRequested.exchange(false, std::memory_order_relaxed);
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
