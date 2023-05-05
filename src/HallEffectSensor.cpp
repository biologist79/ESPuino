// NIKO-AT  04.12.2022  Project ESPuino new feature: https://forum.espuino.de/t/magnetische-hockey-tags/1449/35
#include <Arduino.h>
#include "settings.h"

#include "HallEffectSensor.h"

#include "Log.h"
#include "System.h"

#ifdef HALLEFFECT_SENSOR_ENABLE

	// debugging
	#define HallEffectDebug_DISABLED // remove _DISABLED

	// gPrefs VarNames
	#define NULLFIELDVALUENAME "HES_NullValue"

HallEffectSensor gHallEffectSensor;

HallEffectSensor::HallEffectSensor() {
	pinMode(HallEffectSensor_PIN, INPUT);
};

// private
void HallEffectSensor::LoadNullFieldValueFromNVS() {
	nullFieldValue = gPrefsSettings.getUShort(NULLFIELDVALUENAME, 0);
	Log_Printf(LOGLEVEL_INFO, "HallEffectSensor> NullFieldValue from NVS:%d", nullFieldValue);
}

bool HallEffectSensor::SaveToNullFieldValueNVS(uint16_t val) {
	uint16_t diff = abs(val - nullFieldValue);
	bool res = (gPrefsSettings.putUShort(NULLFIELDVALUENAME, val) == 2);
	const char *resMsg = "ERROR";
	if (res) {
		resMsg = "OK";
		nullFieldValue = val;
	}
	Log_Printf(LOGLEVEL_INFO, "HallEffectSensor> Save NullFieldValue in NVS> Result:%s> NullValue:%d, diff:%d", resMsg, val, diff);
	return res;
}

void HallEffectSensor::StartDebugging() {
	maxVal = 0;
	minVal = 9999;
	diffVal = 0;
	lastCalibrationSubCallMS = 0;
	calibrationFctCalls = 0;
};

// public
void HallEffectSensor::init() {
	LoadNullFieldValueFromNVS();
	#ifdef HallEffectDebug
	gHallEffectSensor.calibration();
	#else
	int sva = gHallEffectSensor.readSensorValueAverage(true);
	// check calibration
	int diff = abs(sva - nullFieldValue);
	if ((nullFieldValue < 1) || ((diff > 5) && (diff < HallEffectMaxDiffValueReCalibrate))) {
		SaveToNullFieldValueNVS(sva);
	} else if (diff >= HallEffectMinDiffValue) {
		diff = sva - nullFieldValue;
		byte nr = 1;
		if (diff > 0) {
			nr = 2;
		}
		Log_Printf(LOGLEVEL_INFO, "HallEffectSensor> No AutoCalibration> TAG side:%d detected", nr);
	} else {
		Log_Printf(LOGLEVEL_INFO, "HallEffectSensor> No AutoCalibration> Difference too small> diff:%d", diff);
	}
	#endif
}

void HallEffectSensor::cyclic() {
	#ifdef HallEffectDebug
	if ((calibrationFctCalls < maxCalibrationFctCalls) && (millis() > (lastCalibrationSubCallMS + calibrationFctCallsPauseMS))) {
		readSensorValueAverage(false);
		calibrationFctCalls++;
		Log_Printf(LOGLEVEL_INFO, "%d. calibration-loop of HallEffectSensor (make sure NO TAG is/was near reader!) ...", calibrationFctCalls);
		lastCalibrationSubCallMS = millis();
	}
	if (calibrationFctCalls == maxCalibrationFctCalls) { // Log results
		uint16_t sva = (maxVal + minVal) / 2;
		Log_Printf(LOGLEVEL_INFO, "Finished calibration of HallEffectSensor> HES_Uav:%d (min:%d, max:%d, (diff/2):%d, probes:%d)", sva, minVal, maxVal, diffVal / 2, calibrationProbes);
		calibrationFctCalls++; // stop logging results
		#ifdef HallEffectDebug
		calibration();
		delay(100);
		#endif
	}
	#endif
};

uint16_t HallEffectSensor::readSensorValueAverage(bool doLog) {
	// read analog value HallEffectAverageProbes times and return average
	int sum = 0;
	int cnt = 0;
	for (int i = 0; i < HallEffectAverageProbesMS * 40; i++) {
		uint16_t v = analogRead(HallEffectSensor_PIN);
		sum += v;
		cnt++;
		delayMicroseconds(25);
	}
	uint16_t av = sum / cnt;
	// update statistic
	if (av > maxVal) {
		maxVal = av;
	}
	if (av < minVal) {
		minVal = av;
	}
	diffVal = maxVal - minVal;
	calibrationProbes++;
	if (doLog) {
		Log_Printf(LOGLEVEL_INFO, "Read HallEffectSensor> value:%d", av);
	}
	return av;
};

bool HallEffectSensor::saveActualFieldValue2NVS() {
	uint16_t sva = gHallEffectSensor.readSensorValueAverage(true);
	bool res = SaveToNullFieldValueNVS(sva);
	LoadNullFieldValueFromNVS();
	return res;
};

uint8_t HallEffectSensor::waitForState(uint16_t waitMS) {
	// wait for magnetic north or south
	lastWaitForState = 0;
	unsigned long int startms = millis();
	;
	uint16_t sav;
	do {
		sav = readSensorValueAverage(true);
		if (sav < nullFieldValue - HallEffectMinDiffValue) {
			lastWaitForState = 1;
		} else if (sav > nullFieldValue + HallEffectMinDiffValue) {
			lastWaitForState = 2;
		} else {
			delay(50);
		}
	} while ((lastWaitForState == 0) && (millis() < (startms + waitMS)));
	lastWaitForStateMS = millis() - startms;
	Log_Printf(LOGLEVEL_INFO, "HallEffectSensor waitForState> fieldValue:%d => state:%d, (duration:%d ms)", sav, lastWaitForState, lastWaitForStateMS);
	return lastWaitForState;
};

uint16_t HallEffectSensor::NullFieldValue() {
	return nullFieldValue;
}

int HallEffectSensor::LastWaitForStateMS() {
	return lastWaitForStateMS;
}

int8_t HallEffectSensor::LastWaitForState() {
	return lastWaitForState;
}

#endif
