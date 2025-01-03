#include <Arduino.h>
#include "settings.h"

#include "RotaryEncoder.h"

#include "AudioPlayer.h"
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

void RotaryEncoder_Readjust(void) {
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
		// just reset the encoder here, so we get a new delta next time
		encoder.clearCount();
		auto currentVol = AudioPlayer_GetCurrentVolume();
		AudioPlayer_VolumeToQueueSender(currentVol + (encoderValue / 2), false);
		return;
	}
#endif
}
