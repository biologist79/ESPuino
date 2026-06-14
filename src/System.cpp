#include <Arduino.h>
#include "settings.h"

#include "System.h"

#include "Audio.h"
#include "AudioPlayer.h"
#include "Bluetooth.h"
#include "Display.h"
#include "Ftp.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "Mqtt.h"
#include "Port.h"
#include "Power.h"
#include "Rfid.h"
#include "SdCard.h"
#include "Web.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>
#include <esp_random.h>

constexpr const char prefsRfidNamespace[] = "rfidTags"; // Namespace used to save IDs of rfid-tags
constexpr const char prefsSettingsNamespace[] = "settings"; // Namespace used for generic settings

Preferences gPrefsRfid;
Preferences gPrefsSettings;

std::atomic<uint32_t> System_LastTimeActiveTimestamp {0u}; // Timestamp of last user-interaction
std::atomic<uint32_t> System_SleepTimerStartTimestamp {0u}; // Flag if sleep-timer is active
std::atomic<bool> System_GoToSleep = false; // Flag for turning uC immediately into deepsleep
std::atomic<bool> System_Sleeping = false; // Flag for turning into deepsleep is in progress
std::atomic<bool> System_Rebooting = false; // Flag for rebooting is in progress
std::atomic<bool> System_LockControls = false; // Flag if buttons and rotary encoder is locked
uint8_t System_MaxInactivityTime = 10u; // Time in minutes, after uC is put to deep sleep because of inactivity (and modified later via GUI)
uint8_t System_SleepTimer = 30u; // Sleep timer in minutes that can be optionally used (and modified later via MQTT or RFID)

// Operation Mode
std::atomic<uint8_t> System_OperationMode;

void System_SleepHandler(void);
void System_DeepSleepManager(void);
void System_RebootHandler(void);

// Init only NVS required for LPCD
void System_Init_Rfid_Prefs(void) {
	gPrefsRfid.begin(prefsRfidNamespace);
}

void System_Init(void) {
	srand(esp_random());

	gPrefsSettings.begin(prefsSettingsNamespace);

	// Get maximum inactivity-time from NVS
	uint32_t nvsMInactivityTime = gPrefsSettings.getUInt("mInactiviyT", System_MaxInactivityTime);
	if (nvsMInactivityTime) {
		System_MaxInactivityTime = nvsMInactivityTime;
		Log_Printf(LOGLEVEL_INFO, restoredMaxInactivityFromNvs, nvsMInactivityTime);
	} else {
		gPrefsSettings.putUInt("mInactiviyT", System_MaxInactivityTime);
		Log_Println(wroteMaxInactivityToNvs, LOGLEVEL_ERROR);
	}

	System_OperationMode = gPrefsSettings.getUChar("operationMode", OPMODE_NORMAL);
}

void System_Cyclic(void) {
	System_SleepHandler();
	System_DeepSleepManager();
	System_RebootHandler();
}

void System_UpdateActivityTimer(void) {
	System_LastTimeActiveTimestamp = millis();
}

void System_RequestSleep(void) {
	System_GoToSleep = true;
}

bool System_SetSleepTimer(uint8_t minutes) {
	bool sleepTimerEnabled = false;

	if (minutes == 0 || (System_SleepTimerStartTimestamp.load() > 0 && (System_SleepTimer == minutes))) {
		System_SleepTimerStartTimestamp.store(0u);
		System_SleepTimer = 0u;
		Led_SetNightmode(false);
		Log_Println(modificatorSleepd, LOGLEVEL_NOTICE);
	} else {
		System_SleepTimerStartTimestamp.store(millis());
		System_SleepTimer = minutes;
		sleepTimerEnabled = true;

		Led_SetNightmode(true);
		if (minutes == 15) {
			Log_Println(modificatorSleepTimer15, LOGLEVEL_NOTICE);
		} else if (minutes == 30) {
			Log_Println(modificatorSleepTimer30, LOGLEVEL_NOTICE);
		} else if (minutes == 60) {
			Log_Println(modificatorSleepTimer60, LOGLEVEL_NOTICE);
		} else {
			Log_Println(modificatorSleepTimer120, LOGLEVEL_NOTICE);
		}
	}

#ifdef MQTT_ENABLE
	publishMqtt(topicSleepTimer, static_cast<uint32_t>(System_GetSleepTimer()), false);
#endif

	return sleepTimerEnabled;
}

void System_DisableSleepTimer(void) {
	System_SleepTimerStartTimestamp.store(0u);
	Led_SetNightmode(false);
}

bool System_IsSleepTimerEnabled(void) {
	return (System_SleepTimerStartTimestamp.load() > 0u || gPlayProperties.sleepAfterCurrentTrack || gPlayProperties.sleepAfterPlaylist || gPlayProperties.playUntilTrackNumber);
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
			Log_Println(restartAfterOperationModeChange, LOGLEVEL_INFO);
			ESP.restart();
		}
	}
}

bool System_IsWebControlAllowed(void) {
	return (System_OperationMode != OPMODE_BLUETOOTH_SINK); // && System_OperationMode != OPMODE_BLUETOOTH_SOURCE
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
	uint32_t m = millis();
	uint32_t lastActive = System_LastTimeActiveTimestamp.load();
	uint32_t sleepStart = System_SleepTimerStartTimestamp.load();

	// Only check inactivity if the limit is greater than 0
	if (System_MaxInactivityTime > 0 && m >= lastActive) {
		if (m - lastActive >= (System_MaxInactivityTime * 60000u)) {
			Log_Println(goToSleepDueToIdle, LOGLEVEL_INFO);
			System_RequestSleep();
		}
	}

	if (sleepStart > 0 && m >= sleepStart) {
		if (m - sleepStart >= (System_SleepTimer * 60000u)) {
			Log_Println(goToSleepDueToTimer, LOGLEVEL_INFO);
			System_RequestSleep();
		}
	}
}

// prepare power down
void System_PreparePowerDown(void) {
	Web_Exit();
	AudioPlayer_Exit();

#if defined(RFID_READER_TYPE_RUNTIME)
	Rfid_Exit();
#endif

// Disable amps in order to avoid ugly noises when powering off
#ifdef GPIO_PA_EN
	Log_Println("shutdown amplifier..", LOGLEVEL_NOTICE);
	Port_Write(GPIO_PA_EN, false, false);
#endif
#ifdef GPIO_HP_EN
	Log_Println("shutdown headphone..", LOGLEVEL_NOTICE);
	Port_Write(GPIO_HP_EN, false, false);
#endif
	Ftp_Exit();
	Mqtt_Exit();
	Led_Exit();
	Bluetooth_Exit();

	if (gPrefsSettings.getBool("recoverVolBoot", false)) {
		gPrefsSettings.putUInt("previousVolume", AudioPlayer_GetCurrentVolume());
	}
#ifdef OLED_ENABLE
	Display_Exit(); // sends display-off command before power rail drops; prevents SDA glitch
#endif
	SdCard_Exit();

	Serial.flush();
}

void System_Restart(void) {
	System_Rebooting = true;
}

void System_RebootHandler(void) {
	if (System_Rebooting) {
		// prepare power down (shutdown common modules)
		System_PreparePowerDown();
		// restart the ESP-32
		Log_Println("restarting..", LOGLEVEL_NOTICE);
		ESP.restart();
	}
}

// Puts uC to deep-sleep if flag is set
void System_DeepSleepManager(void) {
	if (System_GoToSleep) {
		if (System_Sleeping) {
			return;
		}

		System_Sleeping = true;
		Log_Println(goToSleepNow, LOGLEVEL_NOTICE);
		// prepare power down (shutdown common modules)
		System_PreparePowerDown();
		// switch off power
		Power_PeripheralOff();
		// time to settle down..
		delay(200);
#ifdef PORT_EXPANDER_ENABLE
		Port_Exit();
#endif
		// goto sleep now
		Log_Println("deep-sleep, good night.......", LOGLEVEL_NOTICE);
		esp_deep_sleep_start();
	}
}

void System_PauseTasksDuringUpload(bool pause) {
	if (pause) {
		AudioPlayer_NotifyUploadStart();
		Rfid_TaskPause();
		Led_TaskPause();
	} else {
		Led_TaskResume();
		Rfid_TaskResume();
		AudioPlayer_NotifyUploadEnd();
	}
}

// Print the wake-up reason why ESP32 is awake now
void System_ShowWakeUpReason() {
	esp_sleep_wakeup_cause_t wakeup_reason;
	wakeup_reason = esp_sleep_get_wakeup_cause();

	switch (wakeup_reason) {
		case ESP_SLEEP_WAKEUP_EXT0:
			Log_Println("Wakeup caused by push-button", LOGLEVEL_NOTICE);
			break;
		case ESP_SLEEP_WAKEUP_EXT1:
			Log_Println("Wakeup caused by low power card-detection", LOGLEVEL_NOTICE);
			break;
		case ESP_SLEEP_WAKEUP_TIMER:
			Log_Println("Wakeup caused by timer", LOGLEVEL_NOTICE);
			break;
		case ESP_SLEEP_WAKEUP_TOUCHPAD:
			Log_Println("Wakeup caused by touchpad", LOGLEVEL_NOTICE);
			break;
		case ESP_SLEEP_WAKEUP_ULP:
			Log_Println("Wakeup caused by ULP-program", LOGLEVEL_NOTICE);
			break;
		default:
			Log_Printf(LOGLEVEL_NOTICE, "Wakeup was not caused by deepsleep: %d", wakeup_reason);
			break;
	}
}

bool System_IsColdStart() {
	return (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED);
}

void System_esp_print_tasks(void) {
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
	char *pbuffer = x_calloc(2048, 1);
	vTaskGetRunTimeStats(pbuffer);
	Serial.printf("=====\n%s\n=====", pbuffer);
	free(pbuffer);
#else
	Serial.println("Enable CONFIG_FREERTOS_USE_TRACE_FACILITY to use vTaskGetRunTimeStats()!");
#endif
}
