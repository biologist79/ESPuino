#include <Arduino.h>
#include "settings.h"

#include "Battery.h"

#include "AudioPlayer.h"
#include "Led.h"
#include "Log.h"
#include "Mqtt.h"
#include "Power.h"
#include "Rfid.h"
#include "System.h"

#ifdef BATTERY_MEASURE_ENABLE
uint8_t batteryCheckInterval = s_batteryCheckInterval;
// True once the spoken low-battery warning was issued for the current discharge; only consulted when
// the user asked for a single announcement instead of one per measurement interval.
static bool batteryWarnAnnounced = false;

void Battery_Init(void) {
	analogSetPinAttenuation(VOLTAGE_READ_PIN, inputAttenuation);
	uint32_t vInterval = gPrefsSettings.getUInt("vCheckIntv", 17777);
	if (vInterval != 17777) {
		batteryCheckInterval = vInterval;
		Log_Printf(LOGLEVEL_INFO, batteryCheckIntervalFromNVS, vInterval);
	} else {
		gPrefsSettings.putUInt("vCheckIntv", batteryCheckInterval);
	}

	Battery_InitInner();

	if (gPrefsSettings.getBool("shutdownBatCrit", false) && Battery_IsCritical()) {
		Battery_LogStatus();

		Log_Println(batteryCriticalMsg, LOGLEVEL_NOTICE);
		// turn off peripheral power just to be sure, even if it should not yet have been turned on
		Power_PeripheralOff();
		// Power down and enter deepsleep
		// Don't call System_RequestSleep() here: If the battery is critial, we want to avoid as much init work as possible
		// and also any blinking lights or sounds. The goal is to just stay off.
		// Additionally, LPCD will not be enabled. This is intentional to avoid battery drain.
		delay(200);
		esp_deep_sleep_start();
	}
}

// Measures battery as per interval or after bootup (after allowing a few seconds to settle down)
void Battery_Cyclic(void) {
	static uint32_t lastBatteryCheckTimestamp = 0;
	if (batteryCheckInterval > 0 && ((millis() - lastBatteryCheckTimestamp >= batteryCheckInterval * 60000) || (!lastBatteryCheckTimestamp && millis() >= 10000))) {
		Battery_CyclicInner();
		Battery_PublishMQTT();
		Battery_LogStatus();

		if (Battery_IsLow()) {
			Log_Println(batteryLowMsg, LOGLEVEL_ERROR);
			Led_Indicate(LedIndicatorType::VoltageWarning);
			// Spoken warning on top of the blinking LEDs: while a story is playing nobody looks at the
			// ring, and children least of all. Off by default; the file is whatever the user points at.
			if (gPrefsSettings.getBool("batWarnSound", false)) {
				if (!batteryWarnAnnounced || !gPrefsSettings.getBool("batWarnOnce", false)) {
					const String warnFile = gPrefsSettings.getString("batWarnFile", "");
					// Only count it as announced when it actually played. A paused or idle player is a bad
					// moment, not a job done -- marking it here would silently use up the single
					// announcement and leave the user wondering why nothing ever came. A missing file logs
					// an error at every interval instead, which is the right outcome for a typo in the path.
					if (AudioPlayer_PlayAnnouncement(warnFile.c_str())) {
						batteryWarnAnnounced = true;
					}
				}
			}
		} else {
			// Re-arm for the next discharge once the battery is back above the warning threshold.
			batteryWarnAnnounced = false;
		}

		if (gPrefsSettings.getBool("shutdownBatCrit", false) && Battery_IsCritical()) {
			Log_Println(batteryCriticalMsg, LOGLEVEL_ERROR);
			System_RequestSleep();
		}

		lastBatteryCheckTimestamp = millis();
	}
}
#else // Battery Measure disabled, add dummy methods
void Battery_Cyclic(void) {
}
void Battery_Init(void) {
}
#endif
