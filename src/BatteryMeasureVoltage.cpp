#include <Arduino.h>
#include "settings.h"
#include "Log.h"
#include "Battery.h"
#include "Mqtt.h"
#include "Led.h"
#include "System.h"

// Only enable measurements if valid GPIO is used
#if defined(MEASURE_BATTERY_VOLTAGE) && (VOLTAGE_READ_PIN >= 0 && VOLTAGE_READ_PIN <= 39)
	constexpr uint16_t maxAnalogValue = 4095u; // Highest value given by analogRead(); don't change!

	float warningLowVoltage = s_warningLowVoltage;
	float warningCriticalVoltage = s_warningCriticalVoltage;
	float voltageIndicatorLow = s_voltageIndicatorLow;
	float voltageIndicatorHigh = s_voltageIndicatorHigh;

	void Battery_InitInner() {
		// Get voltages from NVS for Neopixel
		float vLowIndicator = gPrefsSettings.getFloat("vIndicatorLow", 999.99);
		if (vLowIndicator <= 999) {
			voltageIndicatorLow = vLowIndicator;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f V", (char *)FPSTR(voltageIndicatorLowFromNVS), vLowIndicator);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		} else { // preseed if not set
			gPrefsSettings.putFloat("vIndicatorLow", voltageIndicatorLow);
		}

		float vHighIndicator = gPrefsSettings.getFloat("vIndicatorHigh", 999.99);
		if (vHighIndicator <= 999) {
			voltageIndicatorHigh = vHighIndicator;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f V", (char *)FPSTR(voltageIndicatorHighFromNVS), vHighIndicator);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		} else {
			gPrefsSettings.putFloat("vIndicatorHigh", voltageIndicatorHigh);
		}

		float vLowWarning = gPrefsSettings.getFloat("wLowVoltage", 999.99);
		if (vLowWarning <= 999) {
			warningLowVoltage = vLowWarning;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f V", (char *)FPSTR(warningLowVoltageFromNVS), vLowWarning);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		} else {
			gPrefsSettings.putFloat("wLowVoltage", warningLowVoltage);
		}

		float vCriticalWarning = gPrefsSettings.getFloat("wCritVoltage", 999.99);
		if (vCriticalWarning <= 999) {
			warningCriticalVoltage = vCriticalWarning;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f V", (char *)FPSTR(warningCriticalVoltageFromNVS), vCriticalWarning);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		} else {
			gPrefsSettings.putFloat("wCritVoltage", warningCriticalVoltage);
		}
	}


	void Battery_CyclicInner() {
		// no special cyclic task necessary for voltage measure
	}

	// The average of several analog reads will be taken to reduce the noise (Note: One analog read takes ~10µs)
	float Battery_GetVoltage(void) {
		float factor = 1 / ((float) rdiv2 / (rdiv2 + rdiv1));
		float averagedAnalogValue = 0;
		uint8_t i;
		for (i = 0; i <= 19; i++) {
			averagedAnalogValue += (float)analogRead(VOLTAGE_READ_PIN);
		}
		averagedAnalogValue /= 20.0;
		return (averagedAnalogValue / maxAnalogValue) * referenceVoltage * factor + offsetVoltage;
	}

	void Battery_PublishMQTT(){
	#ifdef MQTT_ENABLE
		float voltage = Battery_GetVoltage();
		char vstr[6];
		snprintf(vstr, 6, "%.2f", voltage);
		publishMqtt((char *)FPSTR(topicBatteryVoltage), vstr, false);

		float soc = Battery_EstimateLevel() * 100;
		snprintf(vstr, 6, "%.2f", soc);
		publishMqtt((char *)FPSTR(topicBatterySOC), vstr, false);
	#endif
	}

	void Battery_LogStatus(void){
		float voltage = Battery_GetVoltage();
		snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f V", (char *)FPSTR(currentVoltageMsg), voltage);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);

		float soc = Battery_EstimateLevel() * 100;
		snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f %%", (char *)FPSTR(currentChargeMsg), soc);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);
	}

	float Battery_EstimateLevel(void) {
		float currentVoltage = Battery_GetVoltage();
		float vDiffIndicatorRange = voltageIndicatorHigh - voltageIndicatorLow;
		float vDiffCurrent = currentVoltage - voltageIndicatorLow;
		float estimatedLevel = vDiffCurrent / vDiffIndicatorRange;
		if (estimatedLevel < 0) {   // Don't return value < 0.0
			return 0.0F;
		}
		return (estimatedLevel > 1) ? 1.0F : estimatedLevel;      // Don't return value > 1.0
	}

	bool Battery_IsLow(void) {
		return Battery_GetVoltage() < warningLowVoltage;
	}

	bool Battery_IsCritical(void) {
		return Battery_GetVoltage() < warningCriticalVoltage;
	}

#else
	#ifdef MEASURE_BATTERY_VOLTAGE
		// add some dummy impls to make CI happy

		float warningLowVoltage = 0.4f;
		float warningCriticalVoltage = 0.1f;
		float voltageIndicatorLow = 3.0f;
		float voltageIndicatorHigh = 4.2f;

		void Battery_InitInner(void){}
		void Battery_CyclicInner(void){}
		float Battery_GetVoltage(void){return 4.2;}
		void Battery_PublishMQTT(void){}
		void Battery_LogStatus(void){}
		float Battery_EstimateLevel(void) {return 42.0;}
		bool Battery_IsLow(void) {return false;}
		bool Battery_IsCritical(void) {return false;}
	#endif
#endif
