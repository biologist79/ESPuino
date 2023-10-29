#include <Arduino.h>
#include "settings.h"

#include "Battery.h"

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
	Log_Printf(LOGLEVEL_DEBUG, "Cycles saved in NVS: %.2f", cycles / 100.0);

	// if power was lost, restore model params
	if (por) {
		// TODO i18n necessary?
		Log_Println("Battery detected power loss - loading fuel gauge parameters.", LOGLEVEL_NOTICE);
		uint16_t rComp0 = gPrefsSettings.getUShort("rComp0", 0xFFFF);
		uint16_t tempCo = gPrefsSettings.getUShort("tempCo", 0xFFFF);
		uint16_t fullCapRep = gPrefsSettings.getUShort("fullCapRep", 0xFFFF);
		uint16_t fullCapNom = gPrefsSettings.getUShort("fullCapNom", 0xFFFF);

		Log_Println("Loaded MAX17055 battery model parameters from NVS:", LOGLEVEL_DEBUG);
		Log_Printf(LOGLEVEL_DEBUG, "rComp0: 0x%.4x", rComp0);
		Log_Printf(LOGLEVEL_DEBUG, "tempCo: 0x%.4x", tempCo);
		Log_Printf(LOGLEVEL_DEBUG, "fullCapRep: 0x%.4x", fullCapRep);
		Log_Printf(LOGLEVEL_DEBUG, "fullCapNom: 0x%.4x", fullCapNom);

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
	Log_Printf(LOGLEVEL_DEBUG, "Design Capacity: %.2f mAh", val);
	val = sensor.getEmptyVoltage() / 100.0;
	Log_Printf(LOGLEVEL_DEBUG, "Empty Voltage: %.2f V", val);
	uint16_t modelCfg = sensor.getModelCfg();
	Log_Printf(LOGLEVEL_DEBUG, "ModelCfg Value: 0x%.4x", modelCfg);
	uint16_t cycles = sensor.getCycles();
	Log_Printf(LOGLEVEL_DEBUG, "Cycles: %.2f", cycles / 100.0);

	float vBatteryLow = gPrefsSettings.getFloat("batteryLow", 999.99);
	if (vBatteryLow <= 999) {
		batteryLow = vBatteryLow;
		Log_Printf(LOGLEVEL_INFO, batteryLowFromNVS, batteryLow);
	} else {
		gPrefsSettings.putFloat("batteryLow", batteryLow);
	}

	float vBatteryCritical = gPrefsSettings.getFloat("batteryCritical", 999.99);
	if (vBatteryCritical <= 999) {
		batteryCritical = vBatteryCritical;
		Log_Printf(LOGLEVEL_INFO, batteryCriticalFromNVS, batteryCritical);
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
	publishMqtt(topicBatteryVoltage, vstr, false);

	float soc = Battery_EstimateLevel() * 100;
	snprintf(vstr, 6, "%.2f", soc);
	publishMqtt(topicBatterySOC, vstr, false);
	#endif
}

void Battery_LogStatus(void) {
	Log_Printf(LOGLEVEL_INFO, currentVoltageMsg, Battery_GetVoltage());
	Log_Printf(LOGLEVEL_INFO, currentChargeMsg, Battery_EstimateLevel() * 100);
	Log_Printf(LOGLEVEL_INFO, batteryCurrentMsg, sensor.getAverageCurrent());
	Log_Printf(LOGLEVEL_INFO, batteryTempMsg, sensor.getTemperature());

	// pretty useless because of low resolution
	// Log_Printf(LOGLEVEL_INFO, "Max current to battery since last check: %.4f mA", sensor.getMaxCurrent());
	// Log_Printf(LOGLEVEL_INFO, "Min current to battery since last check: %.4f mA", sensor.getMinCurrent());
	// sensor.resetMaxMinCurrent();

	Log_Printf(LOGLEVEL_INFO, batteryCyclesMsg, sensor.getCycles() / 100.0);
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
