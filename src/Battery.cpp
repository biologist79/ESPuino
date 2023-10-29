#include <Arduino.h>
#include "settings.h"

#include "Battery.h"

#include "Led.h"
#include "Log.h"
#include "Mqtt.h"
#include "Power.h"
#include "Rfid.h"
#include "System.h"

#ifdef BATTERY_MEASURE_ENABLE
uint8_t batteryCheckInterval = s_batteryCheckInterval;

void Battery_Init(void) {
	uint32_t vInterval = gPrefsSettings.getUInt("vCheckIntv", 17777);
	if (vInterval != 17777) {
		batteryCheckInterval = vInterval;
		Log_Printf(LOGLEVEL_INFO, batteryCheckIntervalFromNVS, vInterval);
	} else {
		gPrefsSettings.putUInt("vCheckIntv", batteryCheckInterval);
	}

	Battery_InitInner();

	#ifdef SHUTDOWN_ON_BAT_CRITICAL
	if (Battery_IsCritical()) {
		Battery_LogStatus();

		Log_Println(batteryCriticalMsg, LOGLEVEL_NOTICE);
		// Power down and enter deepsleep
		System_RequestSleep();
	}
	#endif
}

// Measures battery as per interval or after bootup (after allowing a few seconds to settle down)
void Battery_Cyclic(void) {
	static uint32_t lastBatteryCheckTimestamp = 0;
	if ((millis() - lastBatteryCheckTimestamp >= batteryCheckInterval * 60000) || (!lastBatteryCheckTimestamp && millis() >= 10000)) {
		Battery_CyclicInner();
		Battery_PublishMQTT();
		Battery_LogStatus();

		if (Battery_IsLow()) {
			Log_Println(batteryLowMsg, LOGLEVEL_ERROR);
			Led_Indicate(LedIndicatorType::VoltageWarning);
		}

	#ifdef SHUTDOWN_ON_BAT_CRITICAL
		if (Battery_IsCritical()) {
			Log_Println(batteryCriticalMsg, LOGLEVEL_ERROR);
			System_RequestSleep();
		}
	#endif

		lastBatteryCheckTimestamp = millis();
	}
}
#else // Battery Measure disabled, add dummy methods
void Battery_Cyclic(void) {
}
void Battery_Init(void) {
}
#endif
