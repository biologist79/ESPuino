#pragma once

#define HallEffectMinDiffValue			  100 // I had a minimum change of 250, so reliable (factor 2.5) sensitive detection | Delta analog read max. +- 10
#define HallEffectMaxDiffValueReCalibrate 50 // Auto ReCalibration after start, when no HockeyDualfunctionTag is near reader
#define HallEffectAverageProbesMS		  40 // average wird HallEffectAverageProbesMS milliseconds ermittelt
#define HallEffectWaitMS				  750 // max. definable Value: 65.535  (uint16_t) affects responsetime, when normal Tag (no HockeyMagneticTag) is used, 1000 ms is a good value  for me

class HallEffectSensor {
public:
	HallEffectSensor();
	void init();
	void cyclic();
	bool saveActualFieldValue2NVS();
	uint16_t NullFieldValue();
	int LastWaitForStateMS();
	int8_t LastWaitForState();
	uint16_t readSensorValueAverage(bool doLog);
	uint8_t waitForState(uint16_t waitMS); // 0=no magneticTag, 1=negativ magField, 2=positive magField
private:
	// calibration
	static const uint16_t calibrationFctCallsPauseMS = 0;
	static const uint16_t maxCalibrationFctCalls = 1;
	unsigned long int lastCalibrationSubCallMS = 0;
	int maxVal = 0;
	int minVal = 9999;
	int diffVal = 0;
	int calibrationFctCalls = maxCalibrationFctCalls + 1;
	int calibrationProbes = 0;
	// vars
	int lastWaitForStateMS = -1;
	int8_t lastWaitForState = -1;
	uint16_t nullFieldValue;
	bool SaveToNullFieldValueNVS(uint16_t val);
	void LoadNullFieldValueFromNVS();
	void StartDebugging(); // debug only
};

extern HallEffectSensor gHallEffectSensor;
