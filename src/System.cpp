#include <Arduino.h>
#include "settings.h"
#include "System.h"
#include "AudioPlayer.h"
#include "Rfid.h"
#include "Led.h"
#include "Log.h"
#include "Mqtt.h"
#include "SdCard.h"
#include "Port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "Audio.h"
#include "Power.h"

constexpr const char prefsRfidNamespace[] PROGMEM = "rfidTags";     // Namespace used to save IDs of rfid-tags
constexpr const char prefsSettingsNamespace[] PROGMEM = "settings"; // Namespace used for generic settings

Preferences gPrefsRfid;
Preferences gPrefsSettings;

unsigned long System_LastTimeActiveTimestamp = 0u;  // Timestamp of last user-interaction
unsigned long System_SleepTimerStartTimestamp = 0u; // Flag if sleep-timer is active
bool System_GoToSleep = false;                      // Flag for turning uC immediately into deepsleep
bool System_Sleeping = false;                       // Flag for turning into deepsleep is in progress
bool System_LockControls = false;                   // Flag if buttons and rotary encoder is locked
uint8_t System_MaxInactivityTime = 10u;             // Time in minutes, after uC is put to deep sleep because of inactivity (and modified later via GUI)
uint8_t System_SleepTimer = 30u;                    // Sleep timer in minutes that can be optionally used (and modified later via MQTT or RFID)

// Operation Mode
volatile uint8_t System_OperationMode;

void System_SleepHandler(void);
void System_DeepSleepManager(void);

void System_Init(void) {
	srand(esp_random());

	gPrefsRfid.begin((char *) FPSTR(prefsRfidNamespace));
	gPrefsSettings.begin((char *) FPSTR(prefsSettingsNamespace));

	// Get maximum inactivity-time from NVS
	uint32_t nvsMInactivityTime = gPrefsSettings.getUInt("mInactiviyT", 0);
	if (nvsMInactivityTime) {
		System_MaxInactivityTime = nvsMInactivityTime;
		snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(restoredMaxInactivityFromNvs), nvsMInactivityTime);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);
	} else {
		gPrefsSettings.putUInt("mInactiviyT", System_MaxInactivityTime);
		Log_Println((char *) FPSTR(wroteMaxInactivityToNvs), LOGLEVEL_ERROR);
	}

	System_OperationMode = gPrefsSettings.getUChar("operationMode", OPMODE_NORMAL);
}

void System_Cyclic(void) {
	System_SleepHandler();
	System_DeepSleepManager();
}

void System_UpdateActivityTimer(void) {
	System_LastTimeActiveTimestamp = millis();
}

void System_RequestSleep(void) {
	System_GoToSleep = true;
}

bool System_IsSleepRequested(void) {
	return System_GoToSleep;
}

bool System_SetSleepTimer(uint8_t minutes) {
	bool sleepTimerEnabled = false;

	if (System_SleepTimerStartTimestamp && (System_SleepTimer == minutes)) {
		System_SleepTimerStartTimestamp = 0u;
		Led_ResetToInitialBrightness();
		Log_Println((char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
		#ifdef MQTT_ENABLE
			publishMqtt((char *) FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
		#endif
	} else {
		System_SleepTimer = minutes;
		System_SleepTimerStartTimestamp = millis();
		sleepTimerEnabled = true;

		Led_ResetToNightBrightness();
		if (minutes == 15) {
			Log_Println((char *)FPSTR(modificatorSleepTimer15), LOGLEVEL_NOTICE);
		} else if (minutes == 30) {
			Log_Println((char *)FPSTR(modificatorSleepTimer30), LOGLEVEL_NOTICE);
		} else if (minutes == 60) {
			Log_Println((char *)FPSTR(modificatorSleepTimer60), LOGLEVEL_NOTICE);
		} else {
			Log_Println((char *)FPSTR(modificatorSleepTimer120), LOGLEVEL_NOTICE);
		}

		#ifdef MQTT_ENABLE
			publishMqtt((char *) FPSTR(topicSleepTimerState), System_SleepTimer, false);
			publishMqtt((char *) FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
		#endif
	}

	return sleepTimerEnabled;
}

void System_DisableSleepTimer(void) {
	System_SleepTimerStartTimestamp = 0u;
	Led_ResetToInitialBrightness();
}

bool System_IsSleepTimerEnabled(void) {
	return (System_SleepTimerStartTimestamp > 0u || gPlayProperties.sleepAfterCurrentTrack || gPlayProperties.sleepAfterPlaylist || gPlayProperties.playUntilTrackNumber);
}

uint32_t System_GetSleepTimerTimeStamp(void) {
	return System_SleepTimerStartTimestamp;
}

bool System_IsSleepPending(void) {
	return System_Sleeping;
}

uint8_t System_GetSleepTimer(void) {
	return System_SleepTimer;
}

void System_SetLockControls(bool value) {
	System_LockControls = value;
}

void System_ToggleLockControls(void) {
	System_LockControls = !System_LockControls;
}

bool System_AreControlsLocked(void) {
	return System_LockControls;
}

void System_IndicateError(void) {
	Led_Indicate(LedIndicatorType::Error);
}

void System_IndicateOk(void) {
	Led_Indicate(LedIndicatorType::Ok);
}

// Writes to NVS, if bluetooth or "normal" mode is desired
void System_SetOperationMode(uint8_t opMode) {
	uint8_t currentOperationMode = gPrefsSettings.getUChar("operationMode", OPMODE_NORMAL);
	if (currentOperationMode != opMode) {
		if (gPrefsSettings.putUChar("operationMode", opMode)) {
			ESP.restart();
		}
	}
}

uint8_t System_GetOperationMode(void) {
	return System_OperationMode;
}

// Reads from NVS, if bluetooth or "normal" mode is desired
uint8_t System_GetOperationModeFromNvs(void) {
	return gPrefsSettings.getUChar("operationMode", OPMODE_NORMAL);
}

// Sets deep-sleep-flag if max. inactivity-time is reached
void System_SleepHandler(void) {
	unsigned long m = millis();
	if (m >= System_LastTimeActiveTimestamp && (m - System_LastTimeActiveTimestamp >= (System_MaxInactivityTime * 1000u * 60u))) {
		Log_Println((char *) FPSTR(goToSleepDueToIdle), LOGLEVEL_INFO);
		System_RequestSleep();
	} else if (System_SleepTimerStartTimestamp > 00) {
		if (m - System_SleepTimerStartTimestamp >= (System_SleepTimer * 1000u * 60u)) {
			Log_Println((char *) FPSTR(goToSleepDueToTimer), LOGLEVEL_INFO);
			System_RequestSleep();
		}
	}
}

// Puts uC to deep-sleep if flag is set
void System_DeepSleepManager(void) {
	if (System_GoToSleep) {
		if (System_Sleeping) {
			return;
		}

		System_Sleeping = true;
		Log_Println((char *) FPSTR(goToSleepNow), LOGLEVEL_NOTICE);

		// Make sure last playposition for audiobook is saved when playback is active while shutdown was initiated
		#ifdef SAVE_PLAYPOS_BEFORE_SHUTDOWN
			if (!gPlayProperties.pausePlay && (gPlayProperties.playMode == AUDIOBOOK || gPlayProperties.playMode == AUDIOBOOK_LOOP)) {
				AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);
				while (!gPlayProperties.pausePlay) {    // Make sure to wait until playback is paused in order to be sure that playposition saved in NVS
					vTaskDelay(portTICK_RATE_MS * 100u);
				}
			}
		#endif

		// Disable amps in order to avoid ugly noises when powering off
		#ifdef GPIO_PA_EN
			Port_Write(GPIO_PA_EN, false, false);
		#endif
		#ifdef GPIO_HP_EN
			Port_Write(GPIO_HP_EN, false, false);
		#endif

		Mqtt_Exit();
		Led_Exit();

		#ifdef USE_LAST_VOLUME_AFTER_REBOOT
			gPrefsSettings.putUInt("previousVolume", AudioPlayer_GetCurrentVolume());
		#endif
		SdCard_Exit();

		Serial.flush();
		// switch off power
		Power_PeripheralOff();
		delay(200);
		#if defined (RFID_READER_TYPE_MFRC522_SPI) || defined (RFID_READER_TYPE_MFRC522_I2C) || defined(RFID_READER_TYPE_PN5180)
			Rfid_Exit();
		#endif
		#ifdef PORT_EXPANDER_ENABLE
			Port_Exit();
		#endif
		Log_Println((char *) F("deep-sleep, good night......."), LOGLEVEL_NOTICE);
		esp_deep_sleep_start();
	}
}

// Shows warning after upgrade
void System_ShowUpgradeWarning(void) {
	uint32_t nvsShowUpgradeWarningCount = gPrefsSettings.getUInt("wcountrefact", 0);
	if (!nvsShowUpgradeWarningCount) {
		gPrefsSettings.putUInt("wcountrefact", 1);
		Log_Println((char *) FPSTR(warningRefactoring), LOGLEVEL_ERROR);
	} else if (nvsShowUpgradeWarningCount < 5) {
		gPrefsSettings.putUInt("wcountrefact", ++nvsShowUpgradeWarningCount);
		Log_Println((char *) FPSTR(warningRefactoring), LOGLEVEL_ERROR);
	}
}

// Print the wake-up reason why ESP32 is awake now
void System_ShowWakeUpReason() {
	esp_sleep_wakeup_cause_t wakeup_reason;
	wakeup_reason = esp_sleep_get_wakeup_cause();

	switch (wakeup_reason) {
		case ESP_SLEEP_WAKEUP_EXT0:
			Log_Println((char *) F("Wakeup caused by push-button"), LOGLEVEL_NOTICE);
			break;
		case ESP_SLEEP_WAKEUP_EXT1:
			Log_Println((char *) F("Wakeup caused by low power card-detection"), LOGLEVEL_NOTICE);
			break;
		case ESP_SLEEP_WAKEUP_TIMER:
			Log_Println((char *) F("Wakeup caused by timer"), LOGLEVEL_NOTICE);
			break;
		case ESP_SLEEP_WAKEUP_TOUCHPAD:
			Log_Println((char *) F("Wakeup caused by touchpad"), LOGLEVEL_NOTICE);
			break;
		case ESP_SLEEP_WAKEUP_ULP:
			Log_Println((char *) F("Wakeup caused by ULP-program"), LOGLEVEL_NOTICE);
			break;
		default:
			snprintf(Log_Buffer, Log_BufferLength, "Wakeup was not caused by deepsleep: %d", wakeup_reason);
			Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
			break;
	}
}

#ifdef ENABLE_ESPUINO_DEBUG
	void System_esp_print_tasks(void) {
		char *pbuffer = (char *)calloc(2048, 1);
		vTaskGetRunTimeStats(pbuffer);
		Serial.printf("=====\n%s\n=====", pbuffer);
		free(pbuffer);
	}
#endif
