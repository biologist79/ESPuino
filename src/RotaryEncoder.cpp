#include <Arduino.h>
#include "settings.h"

#include "RotaryEncoder.h"

#include "AudioPlayer.h"
#include "Bluetooth.h"
#include "Button.h"
#include "Cmd.h"
#include "Log.h"
#include "System.h"

#ifdef USEROTARY_ENABLE
	#include <ESP32Encoder.h>
#endif

// An override written before this feature existed does not define it (settings-override.h replaces
// settings.h wholesale), so fall back rather than break those builds.
#ifndef JUMP_OFFSET_ROTARY
	#define JUMP_OFFSET_ROTARY 10
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
	// Tracks whether the currently-held modifier (if any) was resolved to CMD_SEEK_PREVIEW, i.e. whether
	// this hold-gesture is a seek-preview session rather than the generic per-detent-command dispatch below.
	static bool previewGestureActive = false;
	static uint8_t lastModifier = BUTTON_NONE;

	#ifdef INCLUDE_ROTARY_IN_CONTROLS_LOCK
	if (System_AreControlsLocked()) {
		if (previewGestureActive) {
			AudioPlayer_SeekPreviewCommit(); // don't leave a preview hanging if controls get locked mid-gesture
			previewGestureActive = false;
			lastModifier = BUTTON_NONE;
		}
		encoder.clearCount();
		return;
	}
	#endif

	// A button held down while turning acts as a modifier: the turn drives that button's rotary action
	// (brightness, seek, ...) instead of the volume. getCount() is stateful across ticks -- it keeps a
	// half-step that has not completed a detent yet -- so discard whatever has accumulated whenever the
	// modifier changes, otherwise a gesture inherits rotation from before the button went down (and the
	// volume inherits rotation from during the gesture).
	const uint8_t modifier = Button_GetHeldModifier();
	if (modifier != lastModifier) {
		if (previewGestureActive) {
			// Release (or switch to a different modifier) commits immediately -- don't make the user wait
			// out the idle-delay just because they let go of the button.
			AudioPlayer_SeekPreviewCommit();
			previewGestureActive = false;
		}
		lastModifier = modifier;
		encoder.clearCount();
		if (modifier != BUTTON_NONE) {
			// Either direction mapping to CMD_SEEK_PREVIEW makes the whole hold a preview session -- a
			// preview is inherently bidirectional, so a button with only one direction assigned to it
			// still previews both ways (the other direction falls back from volume to preview too, since
			// there is no other sensible meaning for "turn while holding this preview-mapped button").
			previewGestureActive = (Button_GetRotaryAction(modifier, true) == CMD_SEEK_PREVIEW) || (Button_GetRotaryAction(modifier, false) == CMD_SEEK_PREVIEW);
			if (previewGestureActive) {
				AudioPlayer_SeekPreviewStart();
			}
		}
		return;
	}

	int32_t encoderValue = encoder.getCount();
	// Only if initial run or value has changed. And only after "full step" of rotary encoder
	if ((encoderValue != 0) && (encoderValue % 2 == 0)) {
		System_UpdateActivityTimer(); // Set inactivity back if rotary encoder was used
		// just reset the encoder here, so we get a new delta next time
		encoder.clearCount();

		const int32_t detents = encoderValue / 2;

		if (modifier != BUTTON_NONE) {
			if (previewGestureActive) {
				// Swallow the held button's own short/long action, same as the generic gesture path below.
				Button_MarkModifierUsed(modifier);
				AudioPlayer_SeekPreviewAdjust(detents);
				return;
			}

			const uint8_t cmd = Button_GetRotaryAction(modifier, detents > 0);
			if (cmd != CMD_NOTHING) {
				// Tell Button.cpp to swallow this button's own short/long action -- otherwise letting go fires
				// e.g. play/pause, and holding past intervalToLongPress fires the long action mid-gesture.
				Button_MarkModifierUsed(modifier);

				if (cmd == CMD_SEEK_FORWARDS || cmd == CMD_SEEK_BACKWARDS) {
					// Seek is a magnitude, not a step: firing the command once per detent would jump jumpOffset
					// (30s by default) *each time*, so a flick of the encoder scrubs minutes. Apply the smaller
					// per-detent rotary step directly instead. Direction comes from the configured command, so
					// a user who maps CW to backwards still gets backwards.
					const int32_t step = gPrefsSettings.getUChar("rotSeekStep", JUMP_OFFSET_ROTARY);
					const int32_t magnitude = abs(detents) * step;
					AudioPlayer_AddSeekOffset(static_cast<int16_t>((cmd == CMD_SEEK_FORWARDS) ? magnitude : -magnitude));
					return;
				}

				for (int32_t i = 0; i < abs(detents); i++) {
					Cmd_Action(cmd);
				}
				return;
			}
		}

		if ((OPMODE_NORMAL == System_GetOperationMode()) || (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode())) {
			auto newVol = AudioPlayer_GetCurrentVolume() + detents;
			AudioPlayer_SetVolume(newVol);
		} else {
			auto newVol = Bluetooth_GetCurrentVolume() + detents;
			Bluetooth_SetVolume(newVol);
		}
		return;
	}
#endif
}
