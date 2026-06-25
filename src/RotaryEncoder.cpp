#include <Arduino.h>
#include "settings.h"

#include "RotaryEncoder.h"

#include "AudioPlayer.h"
#include "Bluetooth.h"
#include "Log.h"
#include "System.h"

#ifdef USEROTARY_ENABLE
	#include <ESP32Encoder.h>
#endif

// Rotary encoder-configuration
#ifdef USEROTARY_ENABLE
ESP32Encoder encoder;
// Rotary encoder-helper
int32_t lastEncoderValue;
int32_t currentEncoderValue;
int32_t lastVolume = -1; // Don't change -1 as initial-value!
#endif

void RotaryEncoder_Init(void) {
// Init rotary encoder
#ifdef USEROTARY_ENABLE
	if (encoder.isAttached()) {
		encoder.detach();
	}
	#ifdef REVERSE_ROTARY
	const bool bReverse = true;
	#else
	const bool bReverse = false;
	#endif
	if (gPrefsSettings.getBool("rotaryReverse", bReverse)) {
		encoder.attachHalfQuad(ROTARYENCODER_DT, ROTARYENCODER_CLK);
	} else {
		encoder.attachHalfQuad(ROTARYENCODER_CLK, ROTARYENCODER_DT);
	}
	encoder.clearCount();
#endif
}

// Handles volume directed by rotary encoder
void RotaryEncoder_Cyclic(void) {
#ifdef USEROTARY_ENABLE
	#ifdef INCLUDE_ROTARY_IN_CONTROLS_LOCK
	if (System_AreControlsLocked()) {
		encoder.clearCount();
		return;
	}
	#endif

	int32_t encoderValue = encoder.getCount();
	// Only if initial run or value has changed. And only after "full step" of rotary encoder
	if ((encoderValue != 0) && (encoderValue % 2 == 0)) {
		System_UpdateActivityTimer(); // Set inactivity back if rotary encoder was used
		const int32_t deltaImpulses = (encoderValue / 2);
		encoder.clearCount();
		if (AudioPlayer_SeekPreviewIsActive()) {
			AudioPlayer_SeekPreviewAdjustByImpulses(deltaImpulses);
			return;
		}
		if ((OPMODE_NORMAL == System_GetOperationMode()) || (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode())) {
			auto newVol = AudioPlayer_GetCurrentVolume() + deltaImpulses;
			AudioPlayer_SetVolume(newVol);
		} else {
			auto newVol = Bluetooth_GetCurrentVolume() + deltaImpulses;
			Bluetooth_SetVolume(newVol);
		}
		return;
	}
#endif
}
