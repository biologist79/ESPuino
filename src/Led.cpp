#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "settings.h"
#include "AudioPlayer.h"
#include "Battery.h"
#include "Button.h"
#include "Led.h"
#include "Log.h"
#include "System.h"
#include "Wlan.h"
#include "Bluetooth.h"
#include "Port.h"

#ifdef NEOPIXEL_ENABLE
	#include <FastLED.h>

	#define LED_INITIAL_BRIGHTNESS 16u
	#define LED_INITIAL_NIGHT_BRIGHTNESS 2u

	#define LED_INDICATOR_SET(indicator) ((Led_Indicators) |= (1u << ((uint8_t)indicator)))
	#define LED_INDICATOR_IS_SET(indicator) (((Led_Indicators) & (1u << ((uint8_t)indicator))) > 0u)
	#define LED_INDICATOR_CLEAR(indicator) ((Led_Indicators) &= ~(1u << ((uint8_t)indicator)))

	#ifndef LED_OFFSET
		#define LED_OFFSET 0
	#elif LED_OFFSET < 0 || LED_OFFSET >= NUM_LEDS
		#error LED_OFFSET must be between 0 and NUM_LEDS-1
	#endif

	// Time in milliseconds the volume indicator is visible
	#define LED_VOLUME_INDICATOR_RETURN_DELAY	1000U
	#define LED_VOLUME_INDICATOR_NUM_CYCLES		(LED_VOLUME_INDICATOR_RETURN_DELAY / 20)

	extern t_button gButtons[7];    // next + prev + pplay + rotEnc + button4 + button5 + dummy-button
	extern uint8_t gShutdownButton;

	static uint32_t Led_Indicators = 0u;

	static bool Led_Pause = false; // Used to pause Neopixel-signalisation (while NVS-writes as this leads to exceptions; don't know why)

	static uint8_t Led_InitialBrightness = LED_INITIAL_BRIGHTNESS;
	static uint8_t Led_Brightness = LED_INITIAL_BRIGHTNESS;
	static uint8_t Led_NightBrightness = LED_INITIAL_NIGHT_BRIGHTNESS;
	static uint8_t Led_IdleDotDistance = NUM_LEDS / NUM_LEDS_IDLE_DOTS;

	static void Led_Task(void *parameter);
	static uint8_t Led_Address(uint8_t number);
#endif

void Led_Init(void) {
	#ifdef NEOPIXEL_ENABLE
		// Get some stuff from NVS...
		// Get initial LED-brightness from NVS
		uint8_t nvsILedBrightness = gPrefsSettings.getUChar("iLedBrightness", 0);
		if (nvsILedBrightness) {
			Led_InitialBrightness = nvsILedBrightness;
			Led_Brightness = nvsILedBrightness;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %d", (char *) FPSTR(initialBrightnessfromNvs), nvsILedBrightness);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		} else {
			gPrefsSettings.putUChar("iLedBrightness", Led_InitialBrightness);
			Log_Println((char *) FPSTR(wroteInitialBrightnessToNvs), LOGLEVEL_ERROR);
		}

		// Get night LED-brightness from NVS
		uint8_t nvsNLedBrightness = gPrefsSettings.getUChar("nLedBrightness", 255);
		if (nvsNLedBrightness != 255) {
			Led_NightBrightness = nvsNLedBrightness;
			snprintf(Log_Buffer, Log_BufferLength, "%s: %d", (char *) FPSTR(restoredInitialBrightnessForNmFromNvs), nvsNLedBrightness);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		} else {
			gPrefsSettings.putUChar("nLedBrightness", Led_NightBrightness);
			Log_Println((char *) FPSTR(wroteNmBrightnessToNvs), LOGLEVEL_ERROR);
		}

		xTaskCreatePinnedToCore(
			Led_Task,   /* Function to implement the task */
			"Led_Task", /* Name of the task */
			1512,       /* Stack size in words */
			NULL,       /* Task input parameter */
			1,          /* Priority of the task */
			NULL,       /* Task handle. */
			1           /* Core where the task should run */
		);
	#endif
}

void Led_Exit(void) {
	#ifdef NEOPIXEL_ENABLE
		FastLED.clear();
		FastLED.show();
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
		if (Led_Brightness == Led_NightBrightness || Led_Brightness == 0) {	// Only reset to initial value if brightness wasn't intentionally changed (or was zero)
			Led_Brightness = Led_InitialBrightness;
			Log_Println((char *) FPSTR(ledsDimmedToInitialValue), LOGLEVEL_INFO);
		}
	#endif
	#ifdef BUTTONS_LED
		Port_Write(BUTTONS_LED, HIGH, false);
	#endif
}

void Led_ResetToNightBrightness(void) {
	#ifdef NEOPIXEL_ENABLE
		Led_Brightness = Led_NightBrightness;
		Log_Println((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
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
uint8_t Led_Address(uint8_t number) {
	#ifdef NEOPIXEL_REVERSE_ROTATION
		#if LED_OFFSET > 0
			return number <=  LED_OFFSET - 1 ? LED_OFFSET - 1 - number : NUM_LEDS + LED_OFFSET - 1 - number;
		#else
			return NUM_LEDS - 1 - number;
		#endif
	#else
		#if LED_OFFSET > 0
			return number >= NUM_LEDS - LED_OFFSET ?  number + LED_OFFSET - NUM_LEDS : number + LED_OFFSET;
		#else
			return number;
		#endif
	#endif
}

void Led_SetButtonLedsEnabled(boolean value) {
	#ifdef BUTTONS_LED
		Port_Write(BUTTONS_LED, value ? HIGH : LOW, false);
	#endif
}

bool Led_NeedsProgressRedraw(bool lastPlayState, bool lastLockState,
			     bool requestProgressRedraw) {
#ifdef BATTERY_MEASURE_ENABLE
	if (gPlayProperties.pausePlay != lastPlayState ||
	    System_AreControlsLocked() != lastLockState ||
	    requestProgressRedraw ||
	    LED_INDICATOR_IS_SET(LedIndicatorType::VoltageWarning) ||
	    LED_INDICATOR_IS_SET(LedIndicatorType::Voltage) ||
	    !gButtons[gShutdownButton].currentState ||
	    System_IsSleepRequested()) {
#else
	if (gPlayProperties.pausePlay != lastPlayState ||
	    System_AreControlsLocked() != lastLockState ||
	    requestProgressRedraw ||
	    !gButtons[gShutdownButton].currentState ||
	    System_IsSleepRequested()) {
#endif
		return true;
	}

	return false;
}

CRGB Led_ConvertColorToDimmedColor(CRGB color, uint8_t brightness) {
	float factorBrighness = (float)brightness / DIMMABLE_STATES;
	color.red = (color.red * factorBrighness) + 0.5;
	color.green = (color.green * factorBrighness) + 0.5;
	color.blue = (color.blue* factorBrighness) + 0.5;
	return color;
}

void Led_DrawIdleDots(CRGB* leds, uint8_t offset, CRGB::HTMLColorCode color) {
	for (uint8_t i=0; i<NUM_LEDS_IDLE_DOTS; i++){
		leds[(Led_Address(offset)+i*Led_IdleDotDistance)%NUM_LEDS] = color;
	}
}

bool CheckForPowerButtonAnimation() {
	bool powerAnimation = false;
	if (gShutdownButton < (sizeof(gButtons) / sizeof(gButtons[0])) - 1) { // Only show animation, if CMD_SLEEPMODE was assigned to BUTTON_n_LONG + button is pressed
		if (!gButtons[gShutdownButton].currentState && (millis() - gButtons[gShutdownButton].firstPressedTimestamp >= 150) && gButtonInitComplete) {
			powerAnimation = true;
		}
	} else {
		gShutdownButton = (sizeof(gButtons) / sizeof(gButtons[0])) - 1; // If CMD_SLEEPMODE was not assigned to an enabled button, dummy-button is used
		if (!gButtons[gShutdownButton].currentState) {
			gButtons[gShutdownButton].currentState = true;
		}
	}
	return powerAnimation;
}

static void Led_Task(void *parameter) {
	#ifdef NEOPIXEL_ENABLE
		static uint8_t hlastVolume = AudioPlayer_GetCurrentVolume();
		static double lastPos = gPlayProperties.currentRelPos;
		static bool lastPlayState = false;
		static bool lastLockState = false;
		static bool requestClearLeds = false;
		static bool requestProgressRedraw = false;
		static bool showEvenError = false;
		static bool turnedOffLeds = false;
		static bool singleLedStatus = false;
		static uint8_t ledPosWebstream = 0;
		static uint8_t ledSwitchInterval = 5; // time in secs (webstream-only)
		static uint8_t webstreamColor = 0;
		static unsigned long lastSwitchTimestamp = 0;
		static bool redrawProgress = false;
		static uint8_t lastLedBrightness = Led_Brightness;
		static CRGB::HTMLColorCode idleColor;
		static CRGB::HTMLColorCode speechColor = CRGB::Yellow;
		static CRGB::HTMLColorCode generalColor;
		static CRGB leds[NUM_LEDS];
		FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
		FastLED.setBrightness(Led_Brightness);
		FastLED.setDither(DISABLE_DITHER);

		static LedAnimation activeAnnimation = LedAnimationType::NoNewAnimation;
		static LedAnimation nextAnimaiton = LedAnimationType::NoNewAnimation;
		static bool animationActive = false;
		static int32_t animaitonTimer;
		static uint32_t animationIndex;
		static uint32_t subAnimationIndex;
		static uint32_t animationHelperNumberInt;
		static uint32_t animationHelperNumberInt2;
		static float animationHelperNumberFloat;


		for (;;) {
			// special handling
			if (Led_Pause) { // Workaround to prevent exceptions while NVS-writes take place
				vTaskDelay(portTICK_RATE_MS * 10);
				continue;
			}
			if (System_IsSleepRequested()) { // If deepsleep is planned, turn off LEDs first in order to avoid LEDs still glowing when ESP32 is in deepsleep
				if (!turnedOffLeds) {
					FastLED.clear(true);
					turnedOffLeds = true;
				}
				vTaskDelay(portTICK_RATE_MS * 10);
				continue;
			}

			uint32_t animationDelay = 10;
			
			// check indications and set led-mode
			if (!LED_INDICATOR_IS_SET(LedIndicatorType::BootComplete)){
				nextAnimaiton = LedAnimationType::Boot;
			} else if (CheckForPowerButtonAnimation()) { 
				nextAnimaiton = LedAnimationType::Shutdown;
			} else if (LED_INDICATOR_IS_SET(LedIndicatorType::Error)){
				LED_INDICATOR_CLEAR(LedIndicatorType::Error);
				nextAnimaiton = LedAnimationType::Error;
			} else if (LED_INDICATOR_IS_SET(LedIndicatorType::Ok)) {
				LED_INDICATOR_CLEAR(LedIndicatorType::Ok);
				nextAnimaiton = LedAnimationType::Ok;
			#ifdef BATTERY_MEASURE_ENABLE
			} else if (LED_INDICATOR_IS_SET(LedIndicatorType::VoltageWarning)) {
				LED_INDICATOR_CLEAR(LedIndicatorType::VoltageWarning);
				nextAnimaiton = LedAnimationType::VoltageWarning;
			} else if (LED_INDICATOR_IS_SET(LedIndicatorType::Voltage)) {
				LED_INDICATOR_CLEAR(LedIndicatorType::Voltage);
				nextAnimaiton = LedAnimationType::BatteryMeasurement;
			#endif
			} else if (hlastVolume != AudioPlayer_GetCurrentVolume()) {
				hlastVolume = AudioPlayer_GetCurrentVolume();
				nextAnimaiton = LedAnimationType::Volume;
			} else if (LED_INDICATOR_IS_SET(LedIndicatorType::Rewind)) {
				LED_INDICATOR_CLEAR(LedIndicatorType::Rewind);
				nextAnimaiton = LedAnimationType::Rewind;
			} else if (LED_INDICATOR_IS_SET(LedIndicatorType::PlaylistProgress)){
				LED_INDICATOR_CLEAR(LedIndicatorType::PlaylistProgress);
				nextAnimaiton = LedAnimationType::Playlist;
			} else if (gPlayProperties.playlistFinished) {
				nextAnimaiton = LedAnimationType::Idle; // todo: what is valid?
			} else if (gPlayProperties.currentSpeechActive) {
				nextAnimaiton = LedAnimationType::Speech;
			} else if (gPlayProperties.pausePlay) {
				nextAnimaiton = LedAnimationType::Pause;
			} else if (gPlayProperties.isWebstream) {
				nextAnimaiton = LedAnimationType::Webstream;
			} else if ((gPlayProperties.playMode != BUSY) && (gPlayProperties.playMode != NO_PLAYLIST)) {
				nextAnimaiton = LedAnimationType::Progress;
			} else if (gPlayProperties.playMode == NO_PLAYLIST){
				nextAnimaiton = LedAnimationType::Idle;
			} else if (gPlayProperties.playMode == BUSY) {
				nextAnimaiton = LedAnimationType::Busy;
			} else {
				nextAnimaiton = LedAnimationType::NoNewAnimation; // should not happen
			}

			// check for transition
			if (nextAnimaiton != activeAnnimation) {
				if (animationActive && nextAnimaiton < activeAnnimation) {
					animationActive = false; // abort current animation
					animaitonTimer = 0;
				}
			}
			if ((!animationActive) && (animaitonTimer <= 0)) {
				activeAnnimation = nextAnimaiton; // set new animation
				animationIndex = 0;
			}
			// some special transitions: Retrigger Volume
			if (activeAnnimation == LedAnimationType::Volume && nextAnimaiton == LedAnimationType::Volume){
				animaitonTimer = 0;
			}
			// position of playlist is triggered again
			if (activeAnnimation == LedAnimationType::Playlist && nextAnimaiton == LedAnimationType::Playlist){
				animaitonTimer = 0;
				animationIndex = 0;
			}

			// TODO: Remove Brightness from here (use own LED-function)
			if (lastLedBrightness != Led_Brightness) {
				FastLED.setBrightness(Led_Brightness);
				lastLedBrightness = Led_Brightness;
			}

			if (animaitonTimer <= 0) {
				switch(activeAnnimation) {
					// --------------------------------------------------
					// Bootup - Animation
					// --------------------------------------------------
					case LedAnimationType::Boot: {
						animaitonTimer = 500;

						FastLED.clear();
						for (uint8_t led = 0; led < NUM_LEDS; led++) {
							if (showEvenError) {
								if (Led_Address(led) % 2 == 0) {
									if (millis() <= 10000) {
										leds[Led_Address(led)] = CRGB::Orange;
									} else {
										leds[Led_Address(led)] = CRGB::Red;
									}
								}
							} else {
								if (millis() >= 10000) { // Flashes red after 10s (will remain forever if SD cannot be mounted)
									leds[Led_Address(led)] = CRGB::Red;
								} else {
									if (Led_Address(led) % 2 == 1) {
										leds[Led_Address(led)] = CRGB::Orange;
									}
								}
							}
						}
						FastLED.show();
						showEvenError = !showEvenError;
					} break;

					// --------------------------------------------------
					// Power-Button Animation
					// --------------------------------------------------
					case LedAnimationType::Shutdown: {
						animationActive = true;

						if (NUM_LEDS == 1) {
							FastLED.clear();
							if (millis() - gButtons[gShutdownButton].firstPressedTimestamp <= intervalToLongPress) {
								leds[0] = CRGB::Red;
								FastLED.show();
								animaitonTimer = 5;
							} else {
								if (singleLedStatus) {
									leds[0] = CRGB::Red;
								} else {
									leds[0] = CRGB::Black;
								}
								FastLED.show();
								singleLedStatus = !singleLedStatus;
								animaitonTimer = 50;
							}
							animationActive = false;
						} else {
							if ((millis() - gButtons[gShutdownButton].firstPressedTimestamp >= intervalToLongPress) && (animationIndex >= NUM_LEDS)) {
								animaitonTimer = 50;
								// don't end animation, we already reached the shutdown.
							} else {
								if (animationIndex == 0){
									FastLED.clear();
								}
								if (animationIndex < NUM_LEDS){
									leds[Led_Address(animationIndex)] = CRGB::Red;
									if (gButtons[gShutdownButton].currentState) {
										animaitonTimer = 5;
										animationActive = false;
									} else {
										animaitonTimer = intervalToLongPress / NUM_LEDS;
									}
									animationIndex++;
									FastLED.show();
								}
							}
						}
					} break;

					// --------------------------------------------------
					// Animation of Error or OK
					// --------------------------------------------------
					case LedAnimationType::Error: 
					case LedAnimationType::Ok:{
						CRGB signalColor = CRGB::Black;
						uint16_t onTime = 0;
						if (activeAnnimation == LedAnimationType::Ok){
							signalColor = CRGB::Green;
							onTime = 400;
						} else {
							signalColor = CRGB::Red;
							onTime = 200;						
						}

						if (animationIndex == 0){
							animationActive = true;
							requestProgressRedraw = true;
							FastLED.clear();
						}

						if (NUM_LEDS == 1) {
							FastLED.clear();
							if (singleLedStatus) {
								leds[0] = signalColor;
							} else {
								leds[0] = CRGB::Black;
							}
							FastLED.show();
							singleLedStatus = !singleLedStatus;

							if (animationIndex < 5){
								animationIndex++;
								animaitonTimer = 100;
							} else {
								animationActive = false;
							}
						} else {
							for (uint8_t led = 0; led < NUM_LEDS; led++) {
								leds[Led_Address(led)] = signalColor;
							}
							FastLED.show();
							if (animationIndex > 0){
								animationActive = false;
							} else {
								animationIndex++;
								animaitonTimer = onTime;
							}
						}
					} break;

					// --------------------------------------------------
					// Animation of Volume
					// --------------------------------------------------
					case LedAnimationType::Volume: {
						/*
						* - Single-LED: led indicates loudness between green (low) => red (high)
						* - Multiple-LEDs: number of LEDs indicate loudness; gradient is shown between
						*   green (low) => red (high)
						*/
						animationActive = true;

						// wait for further volume changes within next 20ms for 50 cycles = 1s
						uint32_t ledValue =  map(AudioPlayer_GetCurrentVolume(), 0,
										AudioPlayer_GetMaxVolume(), 0,
										NUM_LEDS * DIMMABLE_STATES);
						uint8_t fullLeds = ledValue/DIMMABLE_STATES;
						uint8_t lastLed = ledValue%DIMMABLE_STATES;

						FastLED.clear();

						if (NUM_LEDS == 1) {
							uint8_t hue = 85 - (90 *
								((double)AudioPlayer_GetCurrentVolume() /
								(double)AudioPlayer_GetMaxVolumeSpeaker()));
							leds[0].setHue(hue);
						} else {
							/*
							* (Inverse) color-gradient from green (85) back to (still)
							* red (250) using unsigned-cast.
							*/
							for (int led = 0; led < fullLeds; led++) {
								uint8_t hue = (-86.0f) / (NUM_LEDS-1) * led + 85.0f;
								leds[Led_Address(led)].setHue(hue);
							}
							if (lastLed > 0){
								uint8_t hue = (-86.0f) / (NUM_LEDS-1) * fullLeds + 85.0f;
								leds[Led_Address(fullLeds)].setHue(hue);
								leds[Led_Address(fullLeds)] = Led_ConvertColorToDimmedColor(leds[Led_Address(fullLeds)], lastLed);
							}
						}

						FastLED.show();
						animationActive = false;
						requestProgressRedraw = true;
						animaitonTimer = 20 * LED_VOLUME_INDICATOR_NUM_CYCLES;

					} break;

					// --------------------------------------------------
					// Animation of Voltage Warning
					// --------------------------------------------------
					case LedAnimationType::VoltageWarning: {
						// Single + Multiple LEDs: flashes red three times if battery-voltage is low
						requestProgressRedraw = true;
						animationActive = true;

						CRGB colorToDraw = CRGB::Red;
						if (animationIndex % 2){
							colorToDraw = CRGB::Black;
						}

						FastLED.clear();
						for (uint8_t led = 0; led < NUM_LEDS; led++) {
							leds[Led_Address(led)] = colorToDraw;
						}
						FastLED.show();
					
						if (animationIndex < 6) {
							animationIndex++;
							animaitonTimer = 200;
						} else {
							animationActive = false;
						}
					} break;

					// --------------------------------------------------
					// Animation of Battery Measurement
					// --------------------------------------------------
					case LedAnimationType::BatteryMeasurement: {
						// Single-LED: indicates voltage coloured between gradient green (high) => red (low)
						// Multi-LED: number of LEDs indicates voltage-level with having green >= 60% ; orange < 60% + >= 30% ; red < 30%
						float batteryLevel = Battery_EstimateLevel();
						if (batteryLevel < 0) { // If voltage is too low or no battery is connected
							LED_INDICATOR_SET(LedIndicatorType::Error);
							break;
						} else {
							if (animationIndex == 0) {
								animationHelperNumberFloat = batteryLevel;
								animationActive = true;
								FastLED.clear();
							}
							if (NUM_LEDS == 1) {
								if (animationHelperNumberFloat < 0.3) {
									leds[0] = CRGB::Red;
								} else if (animationHelperNumberFloat < 0.6) {
									leds[0] = CRGB::Orange;
								} else {
									leds[0] = CRGB::Green;
								}
								FastLED.show();
								
								animaitonTimer = 20*100;
								animationActive = false;
							} else {
								uint8_t numLedsToLight = animationHelperNumberFloat * NUM_LEDS;
								if (numLedsToLight > NUM_LEDS) {    // Can happen e.g. if no battery is connected
									numLedsToLight = NUM_LEDS;
								}

								if (animationIndex < numLedsToLight){
									if (animationHelperNumberFloat < 0.3) {
										leds[Led_Address(animationIndex)] = CRGB::Red;
									} else if (animationHelperNumberFloat < 0.6) {
										leds[Led_Address(animationIndex)] = CRGB::Orange;
									} else {
										leds[Led_Address(animationIndex)] = CRGB::Green;
									}
									FastLED.show();

									animationIndex ++;
									animaitonTimer = 20;
								} else {
									animaitonTimer = 20*100;
									animationActive = false;
								}
							}
						}
					} break;

					// --------------------------------------------------
					// Animation of Rewind
					// --------------------------------------------------
					case LedAnimationType::Rewind: {
						if (NUM_LEDS >= 4) {
							animationActive = true;

							if (animationIndex < (NUM_LEDS)) {
								leds[Led_Address(NUM_LEDS - 1 - animationIndex)] = CRGB::Black;
								FastLED.show();
								animaitonTimer = 30;
								animationIndex ++;
							} else {
								animationActive = false;
							}
						}
					} break;

					// --------------------------------------------------
					// Animation of Playlist-Progress
					// --------------------------------------------------
					case LedAnimationType::Playlist: {
						if (NUM_LEDS >= 4) {
							if (gPlayProperties.numberOfTracks > 1 && gPlayProperties.currentTrackNumber < gPlayProperties.numberOfTracks) {
								uint32_t ledValue =  map(gPlayProperties.currentTrackNumber, 0, gPlayProperties.numberOfTracks - 1, 0, NUM_LEDS * DIMMABLE_STATES);
								uint8_t fullLeds = ledValue/DIMMABLE_STATES;
								uint8_t lastLed = ledValue%DIMMABLE_STATES;

								if (animationIndex == 0 ) {
									FastLED.clear();
									animationActive = true;
									subAnimationIndex = 0;
									animationIndex++;
									animationHelperNumberInt = fullLeds;
									animationHelperNumberInt2 = lastLed;
								}
								
								if ((animationIndex == 1) && (subAnimationIndex < animationHelperNumberInt)) {
									leds[Led_Address(subAnimationIndex)] = CRGB::Blue;
									FastLED.show();
									animaitonTimer = 30;
									subAnimationIndex ++;
								} else if ((animationIndex == 1) && (subAnimationIndex == animationHelperNumberInt)){
									leds[Led_Address(animationHelperNumberInt)] = Led_ConvertColorToDimmedColor(CRGB::Blue, animationHelperNumberInt2);
									FastLED.show();
									animaitonTimer = 100*15;
									subAnimationIndex = animationHelperNumberInt + 1;
									animationIndex++;
								} else if ((animationIndex == 2) && (subAnimationIndex > 0)) { 
									leds[Led_Address(subAnimationIndex) - 1] = CRGB::Black;
									FastLED.show();
									animaitonTimer = 30;
									subAnimationIndex--;
								} else {
									animationActive = false;
								}
							}
						}
					} break;

					// --------------------------------------------------
					// Animation of Idle-State
					// --------------------------------------------------
					case LedAnimationType::Idle: {
						animationActive = true;
						if (animationIndex < NUM_LEDS) {
							if (OPMODE_BLUETOOTH_SINK == System_GetOperationMode()) {
								idleColor = CRGB::Blue;
							} else
							if (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode()) {
								if (Bluetooth_Source_Connected()) {
										idleColor = CRGB::Blue;
									} else {
										idleColor = CRGB::BlueViolet;
									}
							} else {
								if (Wlan_ConnectionTryInProgress()) {
									idleColor = CRGB::Orange;
								} else {
									if (Wlan_IsConnected() && gPlayProperties.currentSpeechActive) {
										idleColor = speechColor;
									} else {
										if (Wlan_IsConnected()) {
											idleColor = CRGB::White;
										} else {
											idleColor = CRGB::Green;
										}
									}
								}
							}
							FastLED.clear();
							Led_DrawIdleDots(leds, animationIndex, idleColor);
							FastLED.show();
							animaitonTimer = 50*10;
							animationIndex++;
						} else {
							animationActive = false;
						}
					} break;

					// --------------------------------------------------
					// Animation of Busy-State
					// --------------------------------------------------
					case LedAnimationType::Busy: {
						animationActive = true;
						requestProgressRedraw = true;
						requestClearLeds = true;
						if (NUM_LEDS == 1) {
							FastLED.clear();
							singleLedStatus = !singleLedStatus;
							if (singleLedStatus) {
								leds[0] = CRGB::BlueViolet;
							} else {
								leds[0] = CRGB::Black;
							}
							FastLED.show();
							animaitonTimer = 100;
							animationActive = false;
						} else {
							if(animationIndex < NUM_LEDS) {
								FastLED.clear();
								Led_DrawIdleDots(leds, animationIndex, idleColor);
								FastLED.show();
								animaitonTimer = 50;
								animationIndex++;
							} else {
								animationActive = false;
							}
						}
					} break;

					// --------------------------------------------------
					// Animation of Progress
					// --------------------------------------------------
					case LedAnimationType::Pause: // TODO: separate animations?
					case LedAnimationType::Webstream:
					case LedAnimationType::Speech:
					case LedAnimationType::Progress:  {
						if (Led_NeedsProgressRedraw(lastPlayState, lastLockState,
										requestProgressRedraw)) {
							lastPlayState = gPlayProperties.pausePlay;
							lastLockState = System_AreControlsLocked();
							requestProgressRedraw = false;
							redrawProgress = true;
						}

						if (requestClearLeds) {
							FastLED.clear();
							FastLED.show();
							requestClearLeds = false;
						}

						// Single-LED: led indicates between gradient green (beginning) => red (end)
						// Multiple-LED: growing number of leds indicate between gradient green (beginning) => red (end)
						if (!gPlayProperties.isWebstream) {
							if (gPlayProperties.currentRelPos != lastPos || redrawProgress) {
								redrawProgress = false;
								lastPos = gPlayProperties.currentRelPos;
								FastLED.clear();
								if (NUM_LEDS == 1) {
									leds[0].setHue((uint8_t)(85 - ((double)90 / 100) * gPlayProperties.currentRelPos));
								} else {
									uint32_t ledValue = map(gPlayProperties.currentRelPos, 0, 98, 0, NUM_LEDS * DIMMABLE_STATES);
									uint8_t fullLeds = ledValue/DIMMABLE_STATES;
									uint8_t lastLed = ledValue%DIMMABLE_STATES;
									for (uint8_t led = 0; led < fullLeds; led++) {
										if (System_AreControlsLocked()) {
											leds[Led_Address(led)] = CRGB::Red;
										} else if (!gPlayProperties.pausePlay) { // Hue-rainbow
											leds[Led_Address(led)].setHue((uint8_t)(((float)PROGRESS_HUE_END - (float)PROGRESS_HUE_START) / (NUM_LEDS-1) * led + PROGRESS_HUE_START));
										}
									}
									if(lastLed > 0){
										if (System_AreControlsLocked()) {
											leds[Led_Address(fullLeds)] = CRGB::Red;
										} else {
											leds[Led_Address(fullLeds)].setHue((uint8_t)(((float)PROGRESS_HUE_END - (float)PROGRESS_HUE_START) / (NUM_LEDS-1) * fullLeds + PROGRESS_HUE_START));
										}
										leds[Led_Address(fullLeds)] = Led_ConvertColorToDimmedColor(leds[Led_Address(fullLeds)], lastLed);
									}
								}
								if (gPlayProperties.pausePlay) {
									generalColor = CRGB::Orange;
									if (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode()) {
										generalColor = CRGB::Blue;
									} else
									if (gPlayProperties.currentSpeechActive) {
										generalColor = speechColor;
									}

									uint8_t pauseOffset = 0;
									if (OFFSET_PAUSE_LEDS) {
										pauseOffset = ((NUM_LEDS/NUM_LEDS_IDLE_DOTS)/2)-1;
									}
									FastLED.clear();
									Led_DrawIdleDots(leds, pauseOffset, generalColor);
								}
							}
						} else { // ... but do things a little bit different for Webstream as there's no progress available
							if (lastSwitchTimestamp == 0 || (millis() - lastSwitchTimestamp >= ledSwitchInterval * 1000) || redrawProgress) {
								redrawProgress = false;
								lastSwitchTimestamp = millis();
								FastLED.clear();
								if (ledPosWebstream + 1 < NUM_LEDS) {
									ledPosWebstream++;
								} else {
									ledPosWebstream = 0;
								}
								if (System_AreControlsLocked()) {
									leds[Led_Address(ledPosWebstream)] = CRGB::Red;
									if (NUM_LEDS > 1) {
										leds[(Led_Address(ledPosWebstream) + NUM_LEDS / 2) % NUM_LEDS] = CRGB::Red;
									}
								} else if (!gPlayProperties.pausePlay) {
									if (NUM_LEDS == 1) {
										leds[0].setHue(webstreamColor++);
									} else {
										leds[Led_Address(ledPosWebstream)].setHue(webstreamColor);
										leds[(Led_Address(ledPosWebstream) + NUM_LEDS / 2) % NUM_LEDS].setHue(webstreamColor++);
									}
								} else if (gPlayProperties.pausePlay) {
									generalColor = CRGB::Orange;
									if (gPlayProperties.currentSpeechActive) {
										generalColor = speechColor;
									}
									if (NUM_LEDS == 1) {
										leds[0] = generalColor;
									} else {
										leds[Led_Address(ledPosWebstream)] = generalColor;
										leds[(Led_Address(ledPosWebstream) + NUM_LEDS / 2) % NUM_LEDS] = generalColor;
									}
								}
							}
						}
						FastLED.show();
						animaitonTimer = 10;
					} break;

					default:
						FastLED.clear();
						FastLED.show();
						animaitonTimer = 50;
					break;
				}
			}

			// get the time to wait
			if (animaitonTimer < animationDelay){
				animationDelay = animaitonTimer;
			}
			animaitonTimer -= animationDelay;
			vTaskDelay(portTICK_RATE_MS * animationDelay);
		}
		vTaskDelete(NULL);
	#endif
}
