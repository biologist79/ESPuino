#include <Arduino.h>
#include "settings.h"

#include "Led.h"

#include "AudioPlayer.h"
#include "Battery.h"
#include "Bluetooth.h"
#include "Button.h"
#include "Log.h"
#include "Port.h"
#include "System.h"
#include "Wlan.h"

#include <WiFi.h>
#include <esp_task_wdt.h>

#ifdef NEOPIXEL_ENABLE
	#include <FastLED.h>

	#define LED_INITIAL_BRIGHTNESS		 16u
	#define LED_INITIAL_NIGHT_BRIGHTNESS 2u

	#define LED_INDICATOR_SET(indicator)	((Led_Indicators) |= (1u << ((uint8_t) indicator)))
	#define LED_INDICATOR_IS_SET(indicator) (((Led_Indicators) & (1u << ((uint8_t) indicator))) > 0u)
	#define LED_INDICATOR_CLEAR(indicator)	((Led_Indicators) &= ~(1u << ((uint8_t) indicator)))

	#ifndef LED_OFFSET
		#define LED_OFFSET 0
	#elif LED_OFFSET < 0 || LED_OFFSET >= NUM_INDICATOR_LEDS
		#error LED_OFFSET must be between 0 and NUM_INDICATOR_LEDS-1
	#endif

	// Time in milliseconds the volume indicator is visible
	#define LED_VOLUME_INDICATOR_RETURN_DELAY 1000U
	#define LED_VOLUME_INDICATOR_NUM_CYCLES	  (LED_VOLUME_INDICATOR_RETURN_DELAY / 20)

extern t_button gButtons[7]; // next + prev + pplay + rotEnc + button4 + button5 + dummy-button
extern uint8_t gShutdownButton;

static uint32_t Led_Indicators = 0u;

static bool Led_Pause = false; // Used to pause Neopixel-signalisation (while NVS-writes as this leads to exceptions; don't know why)

static uint8_t Led_InitialBrightness = LED_INITIAL_BRIGHTNESS;
static uint8_t Led_Brightness = LED_INITIAL_BRIGHTNESS;
static uint8_t Led_NightBrightness = LED_INITIAL_NIGHT_BRIGHTNESS;
constexpr uint8_t Led_IdleDotDistance = NUM_INDICATOR_LEDS / NUM_LEDS_IDLE_DOTS;

static CRGBArray<NUM_INDICATOR_LEDS + NUM_CONTROL_LEDS> leds;
static CRGBSet indicator(leds(0, NUM_INDICATOR_LEDS - 1));
static CRGBSet controlLeds(leds(NUM_INDICATOR_LEDS, NUM_INDICATOR_LEDS + NUM_CONTROL_LEDS - 1));

TaskHandle_t Led_TaskHandle;
static void Led_Task(void *parameter);
static uint8_t Led_Address(uint8_t number);

// animation-functions prototypes
AnimationReturnType Animation_PlaylistProgress(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_BatteryMeasurement(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_Volume(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_Progress(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_Boot(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_Shutdown(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_Error(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_Ok(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_VoltageWarning(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_Webstream(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_Rewind(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_Idle(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_Busy(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_Pause(const bool startNewAnimation, CRGBSet &leds);
AnimationReturnType Animation_Speech(const bool startNewAnimation, CRGBSet &leds);
#endif

void Led_Init(void) {
#ifdef NEOPIXEL_ENABLE
	// Get some stuff from NVS...
	// Get initial LED-brightness from NVS
	uint8_t nvsILedBrightness = gPrefsSettings.getUChar("iLedBrightness", 0);
	if (nvsILedBrightness) {
		Led_InitialBrightness = nvsILedBrightness;
		Led_Brightness = nvsILedBrightness;
		Log_Printf(LOGLEVEL_INFO, initialBrightnessfromNvs, nvsILedBrightness);
	} else {
		gPrefsSettings.putUChar("iLedBrightness", Led_InitialBrightness);
		Log_Println(wroteInitialBrightnessToNvs, LOGLEVEL_ERROR);
	}

	// Get night LED-brightness from NVS
	uint8_t nvsNLedBrightness = gPrefsSettings.getUChar("nLedBrightness", 255);
	if (nvsNLedBrightness != 255) {
		Led_NightBrightness = nvsNLedBrightness;
		Log_Printf(LOGLEVEL_INFO, restoredInitialBrightnessForNmFromNvs, nvsNLedBrightness);
	} else {
		gPrefsSettings.putUChar("nLedBrightness", Led_NightBrightness);
		Log_Println(wroteNmBrightnessToNvs, LOGLEVEL_ERROR);
	}

	xTaskCreatePinnedToCore(
		Led_Task, /* Function to implement the task */
		"Led_Task", /* Name of the task */
		1512, /* Stack size in words */
		NULL, /* Task input parameter */
		1, /* Priority of the task */
		&Led_TaskHandle, /* Task handle. */
		1 /* Core where the task should run */
	);
#endif
}

void Led_Exit(void) {
#ifdef NEOPIXEL_ENABLE
	Log_Println("shutdown LED..", LOGLEVEL_NOTICE);
	if (Led_TaskHandle) {
		vTaskDelete(Led_TaskHandle);
		Led_TaskHandle = NULL;
	}
	// Turn off LEDs in order to avoid LEDs still glowing when ESP32 is in deepsleep
	FastLED.clear(true);
#endif
}

void Led_Indicate(LedIndicatorType value) {
#ifdef NEOPIXEL_ENABLE
	LED_INDICATOR_SET(value);
#endif
}

void Led_SetPause(boolean value) {
#ifdef NEOPIXEL_ENABLE
	Led_Pause = value;
#endif
}

// Used to reset brightness to initial value after prevously active sleepmode was left
void Led_ResetToInitialBrightness(void) {
#ifdef NEOPIXEL_ENABLE
	if (Led_Brightness == Led_NightBrightness || Led_Brightness == 0) { // Only reset to initial value if brightness wasn't intentionally changed (or was zero)
		Led_Brightness = Led_InitialBrightness;
		Log_Println(ledsDimmedToInitialValue, LOGLEVEL_INFO);
	}
#endif
#ifdef BUTTONS_LED
	Port_Write(BUTTONS_LED, HIGH, false);
#endif
}

void Led_ResetToNightBrightness(void) {
#ifdef NEOPIXEL_ENABLE
	Led_Brightness = Led_NightBrightness;
	Log_Println(ledsDimmedToNightmode, LOGLEVEL_INFO);
#endif
#ifdef BUTTONS_LED
	Port_Write(BUTTONS_LED, LOW, false);
#endif
}

uint8_t Led_GetBrightness(void) {
#ifdef NEOPIXEL_ENABLE
	return Led_Brightness;
#else
	return 0u;
#endif
}

void Led_SetBrightness(uint8_t value) {
#ifdef NEOPIXEL_ENABLE
	Led_Brightness = value;
	#ifdef BUTTONS_LED
	Port_Write(BUTTONS_LED, value <= Led_NightBrightness ? LOW : HIGH, false);
	#endif
#endif
}

// Calculates physical address for a virtual LED address. This handles reversing the rotation direction of the ring and shifting the starting LED
#ifdef NEOPIXEL_ENABLE
uint8_t Led_Address(uint8_t number) {
	#ifdef NEOPIXEL_REVERSE_ROTATION
		#if LED_OFFSET > 0
	return number <= LED_OFFSET - 1 ? LED_OFFSET - 1 - number : NUM_INDICATOR_LEDS + LED_OFFSET - 1 - number;
		#else
	return NUM_INDICATOR_LEDS - 1 - number;
		#endif
	#else
		#if LED_OFFSET > 0
	return number >= NUM_INDICATOR_LEDS - LED_OFFSET ? number + LED_OFFSET - NUM_INDICATOR_LEDS : number + LED_OFFSET;
		#else
	return number;
		#endif
	#endif
}
#endif

#ifdef NEOPIXEL_ENABLE
void Led_DrawControls() {
	#if NUM_CONTROL_LEDS > 0
	static CRGB::HTMLColorCode controlLedColors[NUM_CONTROL_LEDS] = CONTROL_LEDS_COLORS;
	for (uint8_t controlLed = 0; controlLed < NUM_CONTROL_LEDS; controlLed++) {
		controlLeds[controlLed] = controlLedColors[controlLed];
	}
	#endif
}
#endif

void Led_SetButtonLedsEnabled(boolean value) {
#ifdef BUTTONS_LED
	Port_Write(BUTTONS_LED, value ? HIGH : LOW, false);
#endif
}

#ifdef NEOPIXEL_ENABLE
CRGB Led_DimColor(CRGB color, uint8_t brightness) {
	const uint8_t factor = uint16_t(brightness * __UINT8_MAX__) / DIMMABLE_STATES;
	return color.nscale8(factor);
}
CRGB::HTMLColorCode Led_GetIdleColor() {
	CRGB::HTMLColorCode idleColor = CRGB::Black;
	if ((OPMODE_BLUETOOTH_SINK == System_GetOperationMode()) || (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode())) {
		if (Bluetooth_Device_Connected()) {
			idleColor = CRGB::Blue;
		} else {
			idleColor = CRGB::BlueViolet;
		}
	} else {
		if (Wlan_ConnectionTryInProgress()) {
			idleColor = CRGB::Orange;
		} else {
			idleColor = CRGB::Green;
			if (Wlan_IsConnected()) {
				idleColor = CRGB::White;
			}
		}
	}
	return idleColor;
}

void Led_DrawIdleDots(CRGBSet &leds, uint8_t offset, CRGB::HTMLColorCode color) {
	for (uint8_t i = 0; i < NUM_LEDS_IDLE_DOTS; i++) {
		leds[(Led_Address(offset) + i * Led_IdleDotDistance) % leds.size()] = color;
	}
}

bool CheckForPowerButtonAnimation() {
	if (gShutdownButton < (sizeof(gButtons) / sizeof(gButtons[0])) - 1) { // Only show animation, if CMD_SLEEPMODE was assigned to BUTTON_n_LONG + button is pressed
		if (gButtons[gShutdownButton].isPressed && (millis() - gButtons[gShutdownButton].firstPressedTimestamp >= 150) && gButtonInitComplete) {
			return true;
		}
	}
	return false;
}
#endif

#ifdef NEOPIXEL_ENABLE
static void Led_Task(void *parameter) {
	static uint8_t lastLedBrightness = Led_Brightness;
	FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, leds.size()).setCorrection(TypicalSMD5050);
	FastLED.setBrightness(Led_Brightness);
	FastLED.setDither(DISABLE_DITHER);

	LedAnimationType activeAnimation = LedAnimationType::NoNewAnimation;
	LedAnimationType nextAnimation = LedAnimationType::NoNewAnimation;
	bool animationActive = false;
	int32_t animationTimer = 0;

	for (;;) {
		// special handling
		if (Led_Pause) { // Workaround to prevent exceptions while NVS-writes take place
			vTaskDelay(portTICK_PERIOD_MS * 10);
			continue;
		}

		Led_DrawControls();

		uint32_t taskDelay = 20;
		bool startNewAnimation = false;

		// check indications and set led-mode
		// this mode will then be animated if the priority and the current animation state fit
		if (!LED_INDICATOR_IS_SET(LedIndicatorType::BootComplete)) {
			nextAnimation = LedAnimationType::Boot;
		} else if (CheckForPowerButtonAnimation()) {
			nextAnimation = LedAnimationType::Shutdown;
		} else if (LED_INDICATOR_IS_SET(LedIndicatorType::Error)) {
			LED_INDICATOR_CLEAR(LedIndicatorType::Error);
			nextAnimation = LedAnimationType::Error;
		} else if (LED_INDICATOR_IS_SET(LedIndicatorType::Ok)) {
			LED_INDICATOR_CLEAR(LedIndicatorType::Ok);
			nextAnimation = LedAnimationType::Ok;
		} else if (LED_INDICATOR_IS_SET(LedIndicatorType::VoltageWarning)) {
			LED_INDICATOR_CLEAR(LedIndicatorType::VoltageWarning);
			nextAnimation = LedAnimationType::VoltageWarning;
		} else if (LED_INDICATOR_IS_SET(LedIndicatorType::Voltage)) {
			nextAnimation = LedAnimationType::BatteryMeasurement;
		} else if (LED_INDICATOR_IS_SET(LedIndicatorType::VolumeChange)) {
			nextAnimation = LedAnimationType::Volume;
		} else if (LED_INDICATOR_IS_SET(LedIndicatorType::Rewind)) {
			LED_INDICATOR_CLEAR(LedIndicatorType::Rewind);
			nextAnimation = LedAnimationType::Rewind;
		} else if (LED_INDICATOR_IS_SET(LedIndicatorType::PlaylistProgress)) {
			nextAnimation = LedAnimationType::Playlist;
		} else if (gPlayProperties.currentSpeechActive) {
			nextAnimation = LedAnimationType::Speech;
		} else if (gPlayProperties.playlistFinished) {
			nextAnimation = LedAnimationType::Idle;
		} else if (gPlayProperties.pausePlay && !gPlayProperties.isWebstream) {
			nextAnimation = LedAnimationType::Pause;
		} else if (gPlayProperties.isWebstream) { // also animate pause in the webstream animation
			nextAnimation = LedAnimationType::Webstream;
		} else if ((gPlayProperties.playMode != BUSY) && (gPlayProperties.playMode != NO_PLAYLIST)) {
			nextAnimation = LedAnimationType::Progress;
		} else if (gPlayProperties.playMode == NO_PLAYLIST) {
			nextAnimation = LedAnimationType::Idle;
		} else if (gPlayProperties.playMode == BUSY) {
			nextAnimation = LedAnimationType::Busy;
		} else {
			nextAnimation = LedAnimationType::NoNewAnimation; // should not happen
		}

		// check for instant transition if the requested animation has a higher priority then the current one
		if (nextAnimation < activeAnimation) {
			animationActive = false; // abort current animation
			animationTimer = 0;
		}
		// transition to new animation
		if ((!animationActive) && (animationTimer <= 0)) {
			activeAnimation = nextAnimation; // set new animation
			startNewAnimation = true;
		}

		// apply brightness-changes
		if (lastLedBrightness != Led_Brightness) {
			FastLED.setBrightness(Led_Brightness);
			lastLedBrightness = Led_Brightness;
		}

		// when there is no delay anymore we have to animate something
		if (animationTimer <= 0) {
			AnimationReturnType ret;
			// animate the current animation
			switch (activeAnimation) {
				case LedAnimationType::Boot:
					ret = Animation_Boot(startNewAnimation, indicator);
					break;

				case LedAnimationType::Shutdown:
					ret = Animation_Shutdown(startNewAnimation, indicator);
					break;

				case LedAnimationType::Error:
					ret = Animation_Error(startNewAnimation, indicator);
					break;

				case LedAnimationType::Ok:
					ret = Animation_Ok(startNewAnimation, indicator);
					break;

				case LedAnimationType::Volume:
					ret = Animation_Volume(startNewAnimation, indicator);
					break;

				case LedAnimationType::VoltageWarning:
					ret = Animation_VoltageWarning(startNewAnimation, indicator);
					break;

				case LedAnimationType::BatteryMeasurement:
					ret = Animation_BatteryMeasurement(startNewAnimation, indicator);
					break;

				case LedAnimationType::Rewind:
					ret = Animation_Rewind(startNewAnimation, indicator);
					break;

				case LedAnimationType::Playlist:
					ret = Animation_PlaylistProgress(startNewAnimation, indicator);
					break;

				case LedAnimationType::Idle:
					ret = Animation_Idle(startNewAnimation, indicator);
					break;

				case LedAnimationType::Busy:
					ret = Animation_Busy(startNewAnimation, indicator);
					break;

				case LedAnimationType::Speech:
					ret = Animation_Speech(startNewAnimation, indicator);
					break;

				case LedAnimationType::Pause:
					ret = Animation_Pause(startNewAnimation, indicator);
					break;

				case LedAnimationType::Progress:
					ret = Animation_Progress(startNewAnimation, indicator);
					break;

				case LedAnimationType::Webstream:
					ret = Animation_Webstream(startNewAnimation, indicator);
					break;

				default:
					indicator = CRGB::Black;
					FastLED.show();
					ret.animationActive = false;
					ret.animationDelay = 50;
					break;
			}
			// apply delay and state from animation
			animationActive = ret.animationActive;
			animationTimer = ret.animationDelay;
			if (ret.animationRefresh) {
				FastLED.show();
			}
		}

		// get the time to wait and delay the task
		if ((animationTimer > 0) && (animationTimer < taskDelay)) {
			taskDelay = animationTimer;
		}
		animationTimer -= taskDelay;
		vTaskDelay(portTICK_PERIOD_MS * taskDelay);
	}
	vTaskDelete(NULL);
}
#endif

#ifdef NEOPIXEL_ENABLE
// ---------------------------------------------------------------------
// ---------------        ANIMATION-METHODS        ---------------------
// ---------------------------------------------------------------------
// * each function has to be non-blocking.
// * all states are kept in this function.
// * all funcitons return the desired delay and if they are still active
// * the function will be called the next time when the returned delay has
// passed
// * the optional Start Flag signals that the animation is started new

// --------------------------------
// BOOT-UP Animation
// --------------------------------
AnimationReturnType Animation_Boot(const bool startNewAnimation, CRGBSet &leds) {
	(void) startNewAnimation; // start is not used
	// static vars
	static bool showEvenError = false;

	// 10 s without success?
	if (millis() > 10000) {
		leds = CRGB::Red;
		if (showEvenError) {
			// and then draw in the black dots
			for (uint8_t i = 0; i < leds.size(); i += 2) {
				leds[i] = CRGB::Black;
			}
		}
	} else {
		leds = CRGB::Black;
		const uint8_t startLed = (showEvenError) ? 1 : 0;
		for (uint8_t i = startLed; i < leds.size(); i += 2) {
			leds[i] = CRGB::Orange;
		}
	}
	showEvenError = !showEvenError;

	return AnimationReturnType(false, 500, true); // always wait 500 ms
}

// --------------------------------
// Shutdown Animation
// --------------------------------
AnimationReturnType Animation_Shutdown(const bool startNewAnimation, CRGBSet &leds) {
	// return values
	bool animationActive = true;
	int32_t animationDelay = 0;
	// static vars
	static bool singleLedStatus = false;
	static uint32_t animationIndex = 0;
	if (startNewAnimation) {
		animationIndex = 0;
	}

	if constexpr (NUM_INDICATOR_LEDS == 1) {
		leds = CRGB::Black;
		if (millis() - gButtons[gShutdownButton].firstPressedTimestamp <= intervalToLongPress) {
			leds[0] = CRGB::Red;
			animationDelay = 5;
		} else {
			if (singleLedStatus) {
				leds[0] = CRGB::Red;
			}
			singleLedStatus = !singleLedStatus;
			animationDelay = 50;
		}
		animationActive = false;
	} else {
		if ((millis() - gButtons[gShutdownButton].firstPressedTimestamp >= intervalToLongPress) && (animationIndex >= leds.size())) {
			animationDelay = 50;
			if (!gButtons[gShutdownButton].isPressed) {
				// increase animation index to bail out, if we had a kombi-button
				animationIndex++;
				if (animationIndex >= leds.size() + 3) {
					animationActive = false; // this is approx. 150ms after the button is released
				}
			}
		} else {
			if (animationIndex == 0) {
				leds = CRGB::Black;
			}
			if (animationIndex < leds.size()) {
				leds[Led_Address(animationIndex)] = CRGB::Red;
				if (gButtons[gShutdownButton].currentState) {
					animationDelay = 5;
					animationActive = false;
				} else {
					animationDelay = intervalToLongPress / leds.size();
				}
				animationIndex++;
			}
		}
	}
	return AnimationReturnType(animationActive, animationDelay, true);
}

// --------------------------------
// Error Animation
// --------------------------------
AnimationReturnType Animation_Error(const bool startNewAnimation, CRGBSet &leds) {
	// return values
	bool animationActive = true;
	uint16_t animationDelay = 0;
	// static vars
	static bool singleLedStatus = false;
	static uint16_t animationIndex = 0;
	if (startNewAnimation) {
		animationIndex = 0;
	}

	if constexpr (NUM_INDICATOR_LEDS == 1) {
		leds = CRGB::Black;
		if (singleLedStatus) {
			leds[0] = CRGB::Red;
		}
		singleLedStatus = !singleLedStatus;

		if (animationIndex < 5) {
			animationIndex++;
			animationDelay = 100;
		} else {
			animationActive = false;
		}
	} else {
		leds = CRGB::Red;
		if (animationIndex > 0) {
			animationActive = false;
		} else {
			animationIndex++;
			animationDelay = 200;
		}
	}
	return AnimationReturnType(animationActive, animationDelay, true);
}
// --------------------------------
// OK Animation
// --------------------------------
AnimationReturnType Animation_Ok(const bool startNewAnimation, CRGBSet &leds) {
	// return values
	bool animationActive = true;
	uint16_t animationDelay = 0;
	// static vars
	static bool singleLedStatus = false;
	static uint16_t animationIndex = 0;
	if (startNewAnimation) {
		animationIndex = 0;
	}

	if constexpr (NUM_INDICATOR_LEDS == 1) {
		leds = CRGB::Black;
		if (singleLedStatus) {
			leds[0] = CRGB::Green;
		}
		singleLedStatus = !singleLedStatus;

		if (animationIndex < 5) {
			animationIndex++;
			animationDelay = 100;
		} else {
			animationActive = false;
		}
	} else {
		leds = CRGB::Green;
		if (animationIndex > 0) {
			animationActive = false;
		} else {
			animationIndex++;
			animationDelay = 400;
		}
	}
	return AnimationReturnType(animationActive, animationDelay, true);
}

// --------------------------------
// VoltageWarning Animation
// --------------------------------
// Single + Multiple LEDs: flashes red three times if battery-voltage is low
AnimationReturnType Animation_VoltageWarning(const bool startNewAnimation, CRGBSet &leds) {
	// return values
	bool animationActive = true;
	uint16_t animationDelay = 0;
	// static vars
	static uint16_t animationIndex = 0;

	if (startNewAnimation) {
		animationIndex = 0;
	}

	leds = CRGB::Black;
	if (animationIndex % 2 == 0) {
		leds = CRGB::Red;
	}

	if (animationIndex < 6) {
		animationIndex++;
		animationDelay = 200;
	} else {
		animationActive = false;
	}
	return AnimationReturnType(animationActive, animationDelay, true);
}

// --------------------------------
// Webstream Animation
// --------------------------------
// Animates the progress and Pause of a Webstream
AnimationReturnType Animation_Webstream(const bool startNewAnimation, CRGBSet &leds) {
	// return values
	bool animationActive = false;
	bool refresh = false;
	int32_t animationDelay = 0;
	// static vars
	static uint8_t ledPosWebstream = 0;
	static uint8_t webstreamColor = 0;
	static uint16_t timerProgress = 0;

	// pause-animation
	if (gPlayProperties.pausePlay) {
		leds = CRGB::Black;
		CRGB::HTMLColorCode generalColor = CRGB::Orange;
		if (OPMODE_BLUETOOTH_SINK == System_GetOperationMode()) {
			generalColor = CRGB::Blue;
		}
		if constexpr (NUM_INDICATOR_LEDS == 1) {
			leds[0] = generalColor;
		} else {
			leds[Led_Address(ledPosWebstream)] = generalColor;
			leds[(Led_Address(ledPosWebstream) + leds.size() / 2) % leds.size()] = generalColor;
		}
		animationDelay = 10;
		timerProgress = 0; // directly show new animation after pause
		refresh = true;
	} else {
		if (startNewAnimation || timerProgress == 0) {
			leds = CRGB::Black;
			timerProgress = 100;
			if (ledPosWebstream + 1 < leds.size()) {
				ledPosWebstream++;
			} else {
				ledPosWebstream = 0;
			}
			if (System_AreControlsLocked()) {
				leds[Led_Address(ledPosWebstream)] = CRGB::Red;
				if constexpr (NUM_INDICATOR_LEDS > 1) {
					leds[(Led_Address(ledPosWebstream) + leds.size() / 2) % leds.size()] = CRGB::Red;
				}
			} else {
				if constexpr (NUM_INDICATOR_LEDS == 1) {
					leds[0].setHue(webstreamColor++);
				} else {
					if (OPMODE_BLUETOOTH_SINK == System_GetOperationMode()) {
						leds[Led_Address(ledPosWebstream)] = CRGB::Blue;
						leds[(Led_Address(ledPosWebstream) + leds.size() / 2) % leds.size()] = CRGB::Blue;
					} else {
						leds[Led_Address(ledPosWebstream)].setHue(webstreamColor);
						leds[(Led_Address(ledPosWebstream) + leds.size() / 2) % leds.size()].setHue(webstreamColor++);
					}
				}
			}
			refresh = true;
		}
		timerProgress--;
		animationDelay = 45;
		if (timerProgress > 0) {
			animationActive = true;
		}
	}
	return AnimationReturnType(animationActive, animationDelay, refresh);
}

// --------------------------------
// Rewind Animation
// --------------------------------
AnimationReturnType Animation_Rewind(const bool startNewAnimation, CRGBSet &leds) {
	// return values
	bool animationActive = false;
	bool refresh = false;
	int32_t animationDelay = 0;
	// static vars
	static uint16_t animationIndex = 0;
	if (startNewAnimation) {
		animationIndex = 0;
	}

	if constexpr (NUM_INDICATOR_LEDS >= 4) {
		animationActive = true;

		if (animationIndex < (leds.size())) {
			leds[Led_Address(leds.size() - 1 - animationIndex)] = CRGB::Black;
			refresh = true;
			animationDelay = 30;
			animationIndex++;
		} else {
			animationActive = false;
		}
	}
	return AnimationReturnType(animationActive, animationDelay, refresh);
}

// --------------------------------
// Idle Animation
// --------------------------------
AnimationReturnType Animation_Idle(const bool startNewAnimation, CRGBSet &leds) {
	// return values
	int32_t animationDelay = 0;
	bool animationActive = true;
	bool refresh = true;
	// static vars
	static int16_t ledIndex = 0;
	// this can be removed to always continue on the last position in idle
	if (startNewAnimation) {
		ledIndex = 0;
	}

	if (ledIndex < leds.size()) {
		CRGB::HTMLColorCode idleColor = Led_GetIdleColor();
		leds = CRGB::Black;
		Led_DrawIdleDots(leds, ledIndex, idleColor);
		if (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode()) {
			// animate a bit faster in BT-Source to distinguish between the bluetooth modes
			animationDelay = 30 * 10;
		} else {
			animationDelay = 50 * 10;
		}
		ledIndex++;
	} else {
		animationActive = false;
		refresh = false;
		ledIndex = 0;
	}
	return AnimationReturnType(animationActive, animationDelay, refresh);
}

// --------------------------------
// Busy Animation
// --------------------------------
AnimationReturnType Animation_Busy(const bool startNewAnimation, CRGBSet &leds) {
	// return values
	bool animationActive = true;
	int32_t animationDelay = 0;
	// static vars
	static bool singleLedStatus = false;
	static uint16_t animationIndex = 0;
	if (startNewAnimation) {
		animationIndex = 0;
	}
	if constexpr (NUM_INDICATOR_LEDS == 1) {
		leds = CRGB::Black;
		singleLedStatus = !singleLedStatus;
		if (singleLedStatus) {
			leds[0] = CRGB::BlueViolet;
		}
		animationDelay = 100;
		animationActive = false;
	} else {
		if (animationIndex < leds.size()) {
			leds = CRGB::Black;
			CRGB::HTMLColorCode idleColor = Led_GetIdleColor();
			Led_DrawIdleDots(leds, animationIndex, idleColor);
			animationDelay = 50;
			animationIndex++;
		} else {
			animationActive = false;
		}
	}
	return AnimationReturnType(animationActive, animationDelay, true);
}

// --------------------------------
// Pause Animation
// --------------------------------
// Animates the pause if no Webstream is active
AnimationReturnType Animation_Pause(const bool startNewAnimation, CRGBSet &leds) {
	(void) startNewAnimation; // start is not used

	leds = CRGB::Black;
	CRGB::HTMLColorCode generalColor = CRGB::Orange;
	if (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode()) {
		generalColor = CRGB::Blue;
	}

	uint8_t pauseOffset = 0;
	if (OFFSET_PAUSE_LEDS) {
		pauseOffset = ((leds.size() / NUM_LEDS_IDLE_DOTS) / 2) - 1;
	}
	Led_DrawIdleDots(leds, pauseOffset, generalColor);

	return AnimationReturnType(false, 10, true);
}

// --------------------------------
// Speech Animation
// --------------------------------
// only draw yellow pause-dots
AnimationReturnType Animation_Speech(const bool startNewAnimation, CRGBSet &leds) {
	(void) startNewAnimation; // start is not used

	leds = CRGB::Black;
	uint8_t pauseOffset = 0;
	if (OFFSET_PAUSE_LEDS) {
		pauseOffset = ((leds.size() / NUM_LEDS_IDLE_DOTS) / 2) - 1;
	}
	Led_DrawIdleDots(leds, pauseOffset, CRGB::Yellow);

	return AnimationReturnType(false, 10, true);
}

// --------------------------------
// Progress in Track Animation
// --------------------------------
AnimationReturnType Animation_Progress(const bool startNewAnimation, CRGBSet &leds) {
	// return values
	int32_t animationDelay = 0;
	// static values
	static double lastPos = 0.0f;

	if (gPlayProperties.currentRelPos != lastPos || startNewAnimation) {
		lastPos = gPlayProperties.currentRelPos;
		leds = CRGB::Black;
		if constexpr (NUM_INDICATOR_LEDS == 1) {
			leds[0].setHue((uint8_t) (85 - ((double) 90 / 100) * gPlayProperties.currentRelPos));
		} else {
			const uint32_t ledValue = std::clamp<uint32_t>(map(gPlayProperties.currentRelPos, 0, 98, 0, leds.size() * DIMMABLE_STATES), 0, leds.size() * DIMMABLE_STATES);
			const uint8_t fullLeds = ledValue / DIMMABLE_STATES;
			const uint8_t lastLed = ledValue % DIMMABLE_STATES;
			for (uint8_t led = 0; led < fullLeds; led++) {
				if (System_AreControlsLocked()) {
					leds[Led_Address(led)] = CRGB::Red;
				} else if (!gPlayProperties.pausePlay) { // Hue-rainbow
					leds[Led_Address(led)].setHue((uint8_t) (((float) PROGRESS_HUE_END - (float) PROGRESS_HUE_START) / (leds.size() - 1) * led + PROGRESS_HUE_START));
				}
			}
			if (lastLed > 0) {
				if (System_AreControlsLocked()) {
					leds[Led_Address(fullLeds)] = CRGB::Red;
				} else {
					leds[Led_Address(fullLeds)].setHue((uint8_t) (((float) PROGRESS_HUE_END - (float) PROGRESS_HUE_START) / (leds.size() - 1) * fullLeds + PROGRESS_HUE_START));
				}
				leds[Led_Address(fullLeds)] = Led_DimColor(leds[Led_Address(fullLeds)], lastLed);
			}
		}
		animationDelay = 10;
	}
	return AnimationReturnType(false, animationDelay, true);
}

// --------------------------------
// Volume-Change Animation
// --------------------------------
// - Single-LED: led indicates loudness between green (low) => red (high)
// - Multiple-LEDs: number of LEDs indicate loudness; gradient is shown between
//   green (low) => red (high)
AnimationReturnType Animation_Volume(const bool startNewAnimation, CRGBSet &leds) {
	// return-values
	int32_t animationDelay = 0;
	bool animationActive = true;
	// static values
	static uint16_t cyclesWaited = 0;

	// wait for further volume changes within next 20ms for 50 cycles = 1s
	const uint32_t ledValue = std::clamp<uint32_t>(map(AudioPlayer_GetCurrentVolume(), 0, AudioPlayer_GetMaxVolume(), 0, leds.size() * DIMMABLE_STATES), 0, leds.size() * DIMMABLE_STATES);
	const uint8_t fullLeds = ledValue / DIMMABLE_STATES;
	const uint8_t lastLed = ledValue % DIMMABLE_STATES;

	leds = CRGB::Black;

	if constexpr (NUM_INDICATOR_LEDS == 1) {
		const uint8_t hue = 85 - (90 * ((double) AudioPlayer_GetCurrentVolume() / (double) AudioPlayer_GetMaxVolumeSpeaker()));
		leds[0].setHue(hue);
	} else {
		/*
		 * (Inverse) color-gradient from green (85) back to (still)
		 * red (250) using unsigned-cast.
		 */
		for (int led = 0; led < fullLeds; led++) {
			const uint8_t hue = (-86.0f) / (leds.size() - 1) * led + 85.0f;
			leds[Led_Address(led)].setHue(hue);
		}
		if (lastLed > 0) {
			const uint8_t hue = (-86.0f) / (leds.size() - 1) * fullLeds + 85.0f;
			leds[Led_Address(fullLeds)].setHue(hue);
			leds[Led_Address(fullLeds)] = Led_DimColor(leds[Led_Address(fullLeds)], lastLed);
		}
	}

	// reset animation if volume changes
	if (LED_INDICATOR_IS_SET(LedIndicatorType::VolumeChange) || startNewAnimation) {
		LED_INDICATOR_CLEAR(LedIndicatorType::VolumeChange);
		cyclesWaited = 0;
	}

	if (cyclesWaited < LED_VOLUME_INDICATOR_NUM_CYCLES) {
		animationDelay = 20;
		cyclesWaited++;
	} else {
		animationActive = false;
		cyclesWaited = 0;
	}
	return AnimationReturnType(animationActive, animationDelay, true);
}

// --------------------------------
// PLAYLIST-PROGRESS Animation
// --------------------------------
AnimationReturnType Animation_PlaylistProgress(const bool startNewAnimation, CRGBSet &leds) {
	// return-values
	static bool animationActive = false; // signals if the animation is currently active
	int32_t animationDelay = 0;
	// static variables for animation
	static LedPlaylistProgressStates animationState = LedPlaylistProgressStates::Done; // Statemachine-variable of this animation
	static uint32_t animationCounter = 0; // counter-variable to loop through leds or to wait
	static uint32_t staticLastBarLenghtPlaylist = 0; // variable to remember the last length of the progress-bar (for connecting animations)
	static uint32_t staticLastTrack = 0; // variable to remember the last track (for connecting animations)

	if constexpr (NUM_INDICATOR_LEDS >= 4) {
		if (gPlayProperties.numberOfTracks > 1 && gPlayProperties.currentTrackNumber < gPlayProperties.numberOfTracks) {
			const uint32_t ledValue = std::clamp<uint32_t>(map(gPlayProperties.currentTrackNumber, 0, gPlayProperties.numberOfTracks - 1, 0, leds.size() * DIMMABLE_STATES), 0, leds.size() * DIMMABLE_STATES);
			const uint8_t fullLeds = ledValue / DIMMABLE_STATES;
			const uint8_t lastLed = ledValue % DIMMABLE_STATES;

			if (LED_INDICATOR_IS_SET(LedIndicatorType::PlaylistProgress)) {
				LED_INDICATOR_CLEAR(LedIndicatorType::PlaylistProgress);
				// check if we need to animate a transition between from an already running animation
				// only animate diff, if triggered again
				if (!startNewAnimation) {
					// forward progress
					if (staticLastTrack < gPlayProperties.currentTrackNumber) {
						if (animationState > LedPlaylistProgressStates::FillBar) {
							animationState = LedPlaylistProgressStates::FillBar;
							animationCounter = staticLastBarLenghtPlaylist;
						}
						// backwards progress
					} else if (staticLastTrack > gPlayProperties.currentTrackNumber) {
						if (staticLastBarLenghtPlaylist < fullLeds) {
							animationState = LedPlaylistProgressStates::FillBar;
							animationCounter = staticLastBarLenghtPlaylist;
						} else {
							animationState = LedPlaylistProgressStates::EmptyBarToTarget;
							animationCounter = staticLastBarLenghtPlaylist;
						}
					}
				}
				staticLastTrack = gPlayProperties.currentTrackNumber;
			}

			if (startNewAnimation) {
				animationActive = true;
				animationCounter = 0;
				animationState = LedPlaylistProgressStates::FillBar;
			}

			animationDelay = 30;
			uint8_t barLength = 0;
			switch (animationState) {
				case LedPlaylistProgressStates::FillBar:
					barLength = animationCounter;
					if (animationCounter >= fullLeds) {
						animationState = LedPlaylistProgressStates::Wait;
						animationCounter = 0;
					} else {
						animationCounter++;
					}
					break;
				case LedPlaylistProgressStates::Wait:
					// wait
					animationCounter++;
					if (animationCounter >= 50) {
						animationState = LedPlaylistProgressStates::EmptyBar;
						animationCounter = fullLeds;
					}
					barLength = fullLeds;
					break;
				case LedPlaylistProgressStates::EmptyBar:
					// negative
					barLength = animationCounter;
					if (animationCounter == 0) {
						animationState = LedPlaylistProgressStates::Done;
					} else {
						animationCounter--;
					}
					break;
				case LedPlaylistProgressStates::EmptyBarToTarget:
					// move back to target and wait there
					barLength = animationCounter;
					if (animationCounter <= fullLeds) {
						animationState = LedPlaylistProgressStates::Wait;
						animationCounter = 0;
					} else {
						animationCounter--;
					}
					break;
				default:
					// done
					animationActive = false;
					break;
			}

			// draw bar
			leds = CRGB::Black;
			for (uint8_t i = 0; i < barLength; i++) {
				leds[Led_Address(i)] = CRGB::Blue;
			}
			if (barLength == fullLeds && lastLed > 0) {
				leds[Led_Address(barLength)] = Led_DimColor(CRGB::Blue, lastLed);
			}
			staticLastBarLenghtPlaylist = barLength;
		}
	} else {
		// nothing to show. Just clear indicator
		LED_INDICATOR_CLEAR(LedIndicatorType::PlaylistProgress);
	}

	return AnimationReturnType(animationActive, animationDelay, true);
}

// --------------------------------
// BATTERY_MEASUREMENT Animation
// --------------------------------
// Single-LED: indicates voltage coloured between gradient green (high) => red (low)
// Multi-LED: number of LEDs indicates voltage-level with having green >= 60% ; orange < 60% + >= 30% ; red < 30%
AnimationReturnType Animation_BatteryMeasurement(const bool startNewAnimation, CRGBSet &leds) {
	// return-values
	static bool animationActive = false;
	bool refresh = false;
	int32_t animationDelay = 0;
	// static variables for animation
	static float staticBatteryLevel = 0.0f; // variable to store the measured battery-voltage
	static uint32_t filledLedCount = 0; // loop variable to animate led-bar

	LED_INDICATOR_CLEAR(LedIndicatorType::Voltage);

	if (startNewAnimation) {
	#ifdef BATTERY_MEASURE_ENABLE
		float batteryLevel = Battery_EstimateLevel();
	#else
		float batteryLevel = 1.0f;
	#endif
		if (batteryLevel < 0.0f) { // If voltage is too low or no battery is connected
			LED_INDICATOR_SET(LedIndicatorType::Error);
			return AnimationReturnType(); // abort to indicate error
		}
		staticBatteryLevel = batteryLevel;
		filledLedCount = 0;
		animationActive = true;
		leds = CRGB::Black;
		refresh = true;
	}
	if constexpr (NUM_INDICATOR_LEDS == 1) {
		if (staticBatteryLevel < 0.3) {
			leds[0] = CRGB::Red;
		} else if (staticBatteryLevel < 0.6) {
			leds[0] = CRGB::Orange;
		} else {
			leds[0] = CRGB::Green;
		}
		refresh = true;

		animationDelay = 20 * 100;
		animationActive = false;
	} else {
		uint8_t numLedsToLight = std::clamp<uint8_t>(staticBatteryLevel * leds.size(), 0, leds.size());

		// fill all needed LEDs
		if (filledLedCount < numLedsToLight) {
			if (staticBatteryLevel < 0.3) {
				leds[Led_Address(filledLedCount)] = CRGB::Red;
			} else if (staticBatteryLevel < 0.6) {
				leds[Led_Address(filledLedCount)] = CRGB::Orange;
			} else {
				leds[Led_Address(filledLedCount)] = CRGB::Green;
			}
			refresh = true;

			filledLedCount++;
			animationDelay = 20;
		} else { // Wait a little
			animationDelay = 20 * 100;
			animationActive = false;
		}
	}
	return AnimationReturnType(animationActive, animationDelay, refresh);
}
#endif

void Led_TaskPause(void) {
#ifdef NEOPIXEL_ENABLE
	vTaskSuspend(Led_TaskHandle);
	FastLED.clear(true);
#endif
}

void Led_TaskResume(void) {
#ifdef NEOPIXEL_ENABLE
	vTaskResume(Led_TaskHandle);
#endif
}
