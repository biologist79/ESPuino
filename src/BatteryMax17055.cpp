#include <Arduino.h>
#include "Battery.h"
#include "settings.h"

#ifdef MEASURE_BATTERY_MAX17055
	#include "Log.h"
	#include "Mqtt.h"
	#include "Led.h"
	#include "System.h"
	#include <Wire.h>
	#include <Arduino-MAX17055_Driver.h>

	float batteryLow = s_batteryLow;
	float batteryCritical = s_batteryCritical;
	uint16_t cycles = 0;

	MAX17055 sensor;

	extern TwoWire i2cBusTwo;

	void Battery_InitInner() {
		bool por = false;
		sensor.init(s_batteryCapacity, s_emptyVoltage, s_recoveryVoltage, s_batteryChemistry, s_vCharge, s_resistSensor, por, &i2cBusTwo, &delay);
		cycles = gPrefsSettings.getUShort("MAX17055_cycles", 0x0000);
		snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f", (char *)"Cycles saved in NVS:", cycles / 100.0);
		Log_Println(Log_Buffer, LOGLEVEL_DEBUG);

		// if power was lost, restore model params
		if (por) {
			// TODO i18n necessary?
			Log_Println("Battery detected power loss - loading fuel gauge parameters.", LOGLEVEL_NOTICE);
			uint16_t rComp0 = gPrefsSettings.getUShort("rComp0", 0xFFFF);
			uint16_t tempCo = gPrefsSettings.getUShort("tempCo", 0xFFFF);
			uint16_t fullCapRep = gPrefsSettings.getUShort("fullCapRep", 0xFFFF);
			uint16_t fullCapNom = gPrefsSettings.getUShort("fullCapNom", 0xFFFF);

			Log_Println("Loaded MAX17055 battery model parameters from NVS:", LOGLEVEL_DEBUG);
			snprintf(Log_Buffer, Log_BufferLength, "%s: 0x%.4x", (char *)"rComp0", rComp0);
			Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
			snprintf(Log_Buffer, Log_BufferLength, "%s: 0x%.4x", (char *)"tempCo", tempCo);
			Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
			snprintf(Log_Buffer, Log_BufferLength, "%s: 0x%.4x", (char *)"fullCapRep", fullCapRep);
			Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
			snprintf(Log_Buffer, Log_BufferLength, "%s: 0x%.4x", (char *)"fullCapNom", fullCapNom);
			Log_Println(Log_Buffer, LOGLEVEL_DEBUG);

			if ((rComp0 & tempCo & fullCapRep & fullCapNom) != 0xFFFF) {
				Log_Println("Successfully loaded fuel gauge parameters.", LOGLEVEL_NOTICE);
				sensor.restoreLearnedParameters(rComp0, tempCo, fullCapRep, cycles, fullCapNom);
			} else {
				Log_Println("Failed loading fuel gauge parameters.", LOGLEVEL_NOTICE);
			}
		} else {
			Log_Println("Battery continuing normal operation", LOGLEVEL_DEBUG);
		}

		Log_Println("MAX17055 init done. Battery configured with the following settings:", LOGLEVEL_DEBUG);
		float val = sensor.getCapacity();
		snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f mAh", (char *)"Design Capacity", val);
		Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
		val = sensor.getEmptyVoltage() / 100.0;
		snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f V", (char *)"Empty Voltage", val);
		Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
		uint16_t modelCfg = sensor.getModelCfg();
		snprintf(Log_Buffer, Log_BufferLength, "%s: 0x%.4x", (char *)"ModelCfg Value", modelCfg);
		Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
		uint16_t cycles = sensor.getCycles();
		snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f", (char *)"Cycles", cycles / 100.0);
		Log_Println(Log_Buffer, LOGLEVEL_DEBUG);

		float vBatteryLow = gPrefsSettings.getFloat("batteryLow", 999.99);
		if (vBatteryLow <= 999) {
			batteryLow = vBatteryLow;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f %%", (char *)FPSTR(batteryLowFromNVS), batteryLow);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		} else {
			gPrefsSettings.putFloat("batteryLow", batteryLow);
		}

		float vBatteryCritical = gPrefsSettings.getFloat("batteryCritical", 999.99);
		if (vBatteryCritical <= 999) {
			batteryCritical = vBatteryCritical;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f %%", (char *)FPSTR(batteryCriticalFromNVS), batteryCritical);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		} else {
			gPrefsSettings.putFloat("batteryCritical", batteryCritical);
		}
	}

	void Battery_CyclicInner() {
		// It is recommended to save the learned capacity parameters every time bit 6 of the Cycles register toggles
		uint16_t sensorCycles = sensor.getCycles();
		// sensorCycles = 0xFFFF likely means read error
		if (sensor.getPresent() && sensorCycles != 0xFFFF && uint16_t(cycles + 0x0040) <= sensorCycles) {
			Log_Println("Battery Cycle passed 64%, store MAX17055 learned parameters", LOGLEVEL_DEBUG);
			uint16_t rComp0;
			uint16_t tempCo;
			uint16_t fullCapRep;
			uint16_t fullCapNom;
			sensor.getLearnedParameters(rComp0, tempCo, fullCapRep, sensorCycles, fullCapNom);
			gPrefsSettings.putUShort("rComp0", rComp0);
			gPrefsSettings.putUShort("tempCo", tempCo);
			gPrefsSettings.putUShort("fullCapRep", fullCapRep);
			gPrefsSettings.putUShort("MAX17055_cycles", sensorCycles);
			gPrefsSettings.putUShort("fullCapNom", fullCapNom);
			cycles = sensorCycles;
		}
	}

	float Battery_GetVoltage(void) {
		return sensor.getInstantaneousVoltage();
	}

	void Battery_PublishMQTT() {
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

	void Battery_LogStatus(void) {
		float voltage = Battery_GetVoltage();
		snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f V", (char *)FPSTR(currentVoltageMsg), voltage);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);

		float soc = Battery_EstimateLevel() * 100;
		snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f %%", (char *)FPSTR(currentChargeMsg), soc);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);

		float avgCurr = sensor.getAverageCurrent();
		snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f mA", (char *)FPSTR(batteryCurrentMsg), avgCurr);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);

		float temperature = sensor.getTemperature();
		snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f °C", (char *)FPSTR(batteryTempMsg), temperature);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);

		// pretty useless because of low resolution
		// float maxCurrent = sensor.getMaxCurrent();
		// snprintf(Log_Buffer, Log_BufferLength, "%s: %.4f mA", "Max current to battery since last check", maxCurrent);
		// Log_Println(Log_Buffer, LOGLEVEL_INFO);
		// float minCurrent = sensor.getMinCurrent();
		// snprintf(Log_Buffer, Log_BufferLength, "%s: %.4f mA", "Min current to battery since last check", minCurrent);
		// Log_Println(Log_Buffer, LOGLEVEL_INFO);
		// sensor.resetMaxMinCurrent();

		float cycles = sensor.getCycles() / 100.0;
		snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f", (char *)FPSTR(batteryCyclesMsg), cycles);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);
	}

	float Battery_EstimateLevel(void) {
		return sensor.getSOC() / 100;
	}

	bool Battery_IsLow(void) {
		float soc = sensor.getSOC();
		if (soc > 100.0) {
			Log_Println("Battery percentage reading invalid, try again.", LOGLEVEL_DEBUG);
			soc = sensor.getSOC();
		}

		return soc < batteryLow;
	}

	bool Battery_IsCritical(void) {
		float soc = sensor.getSOC();
		if (soc > 100.0) {
			Log_Println("Battery percentage reading invalid, try again.", LOGLEVEL_DEBUG);
			soc = sensor.getSOC();
		}

		return soc < batteryCritical;
	}
#endif
