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

		for (;;) {
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
			// Multi-LED: rotates orange unless boot isn't complete
			// Single-LED: blinking orange
			if (!LED_INDICATOR_IS_SET(LedIndicatorType::BootComplete)) {
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
				vTaskDelay(portTICK_RATE_MS * 500);
				continue;
			}

			if (lastLedBrightness != Led_Brightness) {
				FastLED.setBrightness(Led_Brightness);
				lastLedBrightness = Led_Brightness;
			}

			// Multi-LED: growing red as long button for sleepmode is pressed.
			// Single-LED: red when pressed and flashing red when long interval-duration is reached
			if (gShutdownButton < (sizeof(gButtons) / sizeof(gButtons[0])) - 1) { // Only show animation, if CMD_SLEEPMODE was assigned to BUTTON_n_LONG + button is pressed
				//snprintf(Log_Buffer, Log_BufferLength, "%u", uxTaskGetStackHighWaterMark(NULL));
				//Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
				if (!gButtons[gShutdownButton].currentState && (millis() - gButtons[gShutdownButton].firstPressedTimestamp >= 150) && gButtonInitComplete) {
					if (NUM_LEDS == 1) {
						FastLED.clear();
						if (millis() - gButtons[gShutdownButton].firstPressedTimestamp <= intervalToLongPress) {
							leds[0] = CRGB::Red;
							FastLED.show();
						} else {
							if (singleLedStatus) {
								leds[0] = CRGB::Red;
							} else {
								leds[0] = CRGB::Black;
							}
							FastLED.show();
							singleLedStatus = !singleLedStatus;
							vTaskDelay(portTICK_RATE_MS * 50);
						}
					} else {
						if (millis() - gButtons[gShutdownButton].firstPressedTimestamp >= intervalToLongPress) {
							vTaskDelay(portTICK_RATE_MS * 50);
							continue;
						}
						FastLED.clear();
						for (uint8_t led = 0; led < NUM_LEDS; led++) {
							leds[Led_Address(led)] = CRGB::Red;
							if (gButtons[gShutdownButton].currentState) {
								FastLED.show();
								vTaskDelay(portTICK_RATE_MS * 5);
								break;
							}
							FastLED.show();
							vTaskDelay(intervalToLongPress / NUM_LEDS * portTICK_RATE_MS);
						}
					}
				}
			} else {
				gShutdownButton = (sizeof(gButtons) / sizeof(gButtons[0])) - 1; // If CMD_SLEEPMODE was not assigned to an enabled button, dummy-button is used
				if (!gButtons[gShutdownButton].currentState) {
					gButtons[gShutdownButton].currentState = true;
				}
			}

			// Multi-LED: all leds flash red 1x
			// Single-LED: led flashes red 5x
			if (LED_INDICATOR_IS_SET(LedIndicatorType::Error)) { // If error occured (e.g. RFID-modification not accepted)
				LED_INDICATOR_CLEAR(LedIndicatorType::Error);
				requestProgressRedraw = true;
				FastLED.clear();

				if (NUM_LEDS == 1) {
					for (uint8_t cnt = 0; cnt < 5; cnt++) {
						FastLED.clear();
						if (singleLedStatus) {
							leds[0] = CRGB::Red;
						} else {
							leds[0] = CRGB::Black;
						}
						FastLED.show();
						singleLedStatus = !singleLedStatus;
						vTaskDelay(portTICK_RATE_MS * 100);
					}
				} else {
					for (uint8_t led = 0; led < NUM_LEDS; led++) {
						leds[Led_Address(led)] = CRGB::Red;
					}
					FastLED.show();
					vTaskDelay(portTICK_RATE_MS * 200);
				}
			}

			// Multi-LED: all leds flash green 1x
			// Single-LED: led flashes green 5x
			if (LED_INDICATOR_IS_SET(LedIndicatorType::Ok)) { // If action was accepted
				LED_INDICATOR_CLEAR(LedIndicatorType::Ok);
				requestProgressRedraw = true;
				FastLED.clear();

				if (NUM_LEDS == 1) {
					for (uint8_t cnt = 0; cnt < 5; cnt++) {
						FastLED.clear();
						if (singleLedStatus) {
							leds[0] = CRGB::Green;
						} else {
							leds[0] = CRGB::Black;
						}
						FastLED.show();
						singleLedStatus = !singleLedStatus;
						vTaskDelay(portTICK_RATE_MS * 100);
					}
				} else {
					for (uint8_t led = 0; led < NUM_LEDS; led++) {
						leds[Led_Address(led)] = CRGB::Green;
					}
					FastLED.show();
					vTaskDelay(portTICK_RATE_MS * 400);
				}
			}

		#ifdef BATTERY_MEASURE_ENABLE
			// Single + Multiple LEDs: flashes red three times if battery-voltage is low
			if (LED_INDICATOR_IS_SET(LedIndicatorType::VoltageWarning)) {
				LED_INDICATOR_CLEAR(LedIndicatorType::VoltageWarning);
				requestProgressRedraw = true;
				for (uint8_t i = 0; i < 3; i++) {
					FastLED.clear();

					for (uint8_t led = 0; led < NUM_LEDS; led++) {
						leds[Led_Address(led)] = CRGB::Red;
					}
					FastLED.show();
					vTaskDelay(portTICK_RATE_MS * 200);
					FastLED.clear();

					for (uint8_t led = 0; led < NUM_LEDS; led++) {
						leds[Led_Address(led)] = CRGB::Black;
					}
					FastLED.show();
					vTaskDelay(portTICK_RATE_MS * 200);
				}
			}

			// Single-LED: indicates voltage coloured between gradient green (high) => red (low)
			// Multi-LED: number of LEDs indicates voltage-level with having green >= 60% ; orange < 60% + >= 30% ; red < 30%
			if (LED_INDICATOR_IS_SET(LedIndicatorType::Voltage)) {
				LED_INDICATOR_CLEAR(LedIndicatorType::Voltage);
				float batteryLevel = Battery_EstimateLevel();

				if (batteryLevel < 0) { // If voltage is too low or no battery is connected
					LED_INDICATOR_SET(LedIndicatorType::Error);
					break;
				} else {
					FastLED.clear();
					if (NUM_LEDS == 1) {
						if (batteryLevel < 0.3) {
							leds[0] = CRGB::Red;
						} else if (batteryLevel < 0.6) {
							leds[0] = CRGB::Orange;
						} else {
							leds[0] = CRGB::Green;
						}
						FastLED.show();
					} else {
						uint8_t numLedsToLight = batteryLevel * NUM_LEDS;
						if (numLedsToLight > NUM_LEDS) {    // Can happen e.g. if no battery is connected
							numLedsToLight = NUM_LEDS;
						}
						for (uint8_t led = 0; led < numLedsToLight; led++) {
							if (batteryLevel < 0.3) {
								leds[Led_Address(led)] = CRGB::Red;
							} else if (batteryLevel < 0.6) {
								leds[Led_Address(led)] = CRGB::Orange;
							} else {
								leds[Led_Address(led)] = CRGB::Green;
							}
							FastLED.show();
							vTaskDelay(portTICK_RATE_MS * 20);
						}
						FastLED.show();
					}

					for (uint8_t i = 0; i <= 100; i++) {
						if (hlastVolume != AudioPlayer_GetCurrentVolume() || LED_INDICATOR_IS_SET(LedIndicatorType::Error) || LED_INDICATOR_IS_SET(LedIndicatorType::Ok) || !gButtons[gShutdownButton].currentState || System_IsSleepRequested()) {
							break;
						}

						vTaskDelay(portTICK_RATE_MS * 20);
					}
				}
			}
		#endif

		/*
		 * - Single-LED: led indicates loudness between green (low) => red (high)
		 * - Multiple-LEDs: number of LEDs indicate loudness; gradient is shown between
		 *   green (low) => red (high)
		 */
		if (hlastVolume != AudioPlayer_GetCurrentVolume()) { // If volume has been changed
			uint8_t timeout = LED_VOLUME_INDICATOR_NUM_CYCLES;
			// wait for further volume changes within next 20ms for 50 cycles = 1s
			while (timeout--) {
				uint8_t numLedsToLight = map(AudioPlayer_GetCurrentVolume(), 0,
							     AudioPlayer_GetMaxVolume(), 0,
							     NUM_LEDS);

				// Reset timeout when volume has changed
				if (hlastVolume != AudioPlayer_GetCurrentVolume())
					timeout = LED_VOLUME_INDICATOR_NUM_CYCLES;

				hlastVolume = AudioPlayer_GetCurrentVolume();
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
					for (int led = 0; led < numLedsToLight; led++) {
						uint8_t hue = (-86.0f) / (NUM_LEDS-1) * led + 85.0f;
						leds[Led_Address(led)].setHue(hue);
					}
				}

				FastLED.show();

				// abort volume change indication if necessary
				if (LED_INDICATOR_IS_SET(LedIndicatorType::Error) ||
				    LED_INDICATOR_IS_SET(LedIndicatorType::Ok) ||
				    !gButtons[gShutdownButton].currentState ||
				    System_IsSleepRequested()) {
					break;
				}

				vTaskDelay(portTICK_RATE_MS * 20);
			}

			requestProgressRedraw = true;
		}

		// < 4 LEDs: doesn't make sense at all
		// >= 4 LEDs: collapsing ring (blue => black)
		if (LED_INDICATOR_IS_SET(LedIndicatorType::Rewind)) {
			LED_INDICATOR_CLEAR(LedIndicatorType::Rewind);
			if (NUM_LEDS >= 4) {
				for (uint8_t i = NUM_LEDS - 1; i > 0; i--) {
					leds[Led_Address(i)] = CRGB::Black;
					FastLED.show();
					if (hlastVolume != AudioPlayer_GetCurrentVolume() || lastLedBrightness != Led_Brightness || LED_INDICATOR_IS_SET(LedIndicatorType::Error) || LED_INDICATOR_IS_SET(LedIndicatorType::Ok) || !gButtons[gShutdownButton].currentState || System_IsSleepRequested()) {
						break;
					} else {
						vTaskDelay(portTICK_RATE_MS * 30);
					}
				}
			}
		}

		// < 4 LEDs: doesn't make sense at all
		// >= 4 LEDs: growing ring (black => blue); relative number of LEDs indicate playlist-progress
		if (LED_INDICATOR_IS_SET(LedIndicatorType::PlaylistProgress)) {
			LED_INDICATOR_CLEAR(LedIndicatorType::PlaylistProgress);
			if (NUM_LEDS >= 4) {
				if (gPlayProperties.numberOfTracks > 1 && gPlayProperties.currentTrackNumber < gPlayProperties.numberOfTracks) {
					uint8_t numLedsToLight = map(gPlayProperties.currentTrackNumber, 0, gPlayProperties.numberOfTracks - 1, 0, NUM_LEDS);
					FastLED.clear();
					for (uint8_t i = 0; i < numLedsToLight; i++) {
						leds[Led_Address(i)] = CRGB::Blue;
						FastLED.show();
						#ifdef BATTERY_MEASURE_ENABLE
							if (hlastVolume != AudioPlayer_GetCurrentVolume() || lastLedBrightness != Led_Brightness || LED_INDICATOR_IS_SET(LedIndicatorType::Error) || LED_INDICATOR_IS_SET(LedIndicatorType::Ok) || LED_INDICATOR_IS_SET(LedIndicatorType::VoltageWarning) || LED_INDICATOR_IS_SET(LedIndicatorType::Voltage) || !gButtons[gShutdownButton].currentState || System_IsSleepRequested()) {
						#else
							if (hlastVolume != AudioPlayer_GetCurrentVolume() || lastLedBrightness != Led_Brightness || LED_INDICATOR_IS_SET(LedIndicatorType::Error) || LED_INDICATOR_IS_SET(LedIndicatorType::Ok) || !gButtons[gShutdownButton].currentState || System_IsSleepRequested()) {
						#endif
							break;
						} else {
							vTaskDelay(portTICK_RATE_MS * 30);
						}
					}

					for (uint8_t i = 0; i <= 100; i++)  {
						#ifdef BATTERY_MEASURE_ENABLE
							if (hlastVolume != AudioPlayer_GetCurrentVolume() || lastLedBrightness != Led_Brightness || LED_INDICATOR_IS_SET(LedIndicatorType::Error) || LED_INDICATOR_IS_SET(LedIndicatorType::Ok) || LED_INDICATOR_IS_SET(LedIndicatorType::VoltageWarning) || LED_INDICATOR_IS_SET(LedIndicatorType::Voltage) || !gButtons[gShutdownButton].currentState || System_IsSleepRequested()) {
						#else
							if (hlastVolume != AudioPlayer_GetCurrentVolume() || lastLedBrightness != Led_Brightness || LED_INDICATOR_IS_SET(LedIndicatorType::Error) || LED_INDICATOR_IS_SET(LedIndicatorType::Ok) || !gButtons[gShutdownButton].currentState || System_IsSleepRequested()) {
						#endif
							break;
						} else {
							vTaskDelay(portTICK_RATE_MS * 15);
						}
					}

					for (uint8_t i = numLedsToLight; i > 0; i--) {
						leds[Led_Address(i) - 1] = CRGB::Black;
						FastLED.show();
						#ifdef BATTERY_MEASURE_ENABLE
							if (hlastVolume != AudioPlayer_GetCurrentVolume() || lastLedBrightness != Led_Brightness || LED_INDICATOR_IS_SET(LedIndicatorType::Error) || LED_INDICATOR_IS_SET(LedIndicatorType::Ok) || LED_INDICATOR_IS_SET(LedIndicatorType::VoltageWarning) || LED_INDICATOR_IS_SET(LedIndicatorType::Voltage) || !gButtons[gShutdownButton].currentState || System_IsSleepRequested()) {
						#else
							if (hlastVolume != AudioPlayer_GetCurrentVolume() || lastLedBrightness != Led_Brightness || LED_INDICATOR_IS_SET(LedIndicatorType::Error) || LED_INDICATOR_IS_SET(LedIndicatorType::Ok) || !gButtons[gShutdownButton].currentState || System_IsSleepRequested()) {
						#endif
							break;
						}
						else {
							vTaskDelay(portTICK_RATE_MS * 30);
						}
					}
				}
			}
		}

		// Skip playmodes if shutdown-button is pressed as this leads to ugly indications
		if (!gButtons[gShutdownButton].currentState && gShutdownButton != 99) {
			vTaskDelay(portTICK_RATE_MS * 20);
			continue;
		}

		switch (gPlayProperties.playMode) {
			case NO_PLAYLIST: // If no playlist is active (idle)
				if (hlastVolume == AudioPlayer_GetCurrentVolume() && lastLedBrightness == Led_Brightness) {
					for (uint8_t i = 0; i < NUM_LEDS; i++) {
						if (hlastVolume != AudioPlayer_GetCurrentVolume()) {
							// skip idle pattern if volume has changed
							break;
						}
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
						if (Led_Address(i) == 0) { // White if Wifi is enabled and blue if not
							leds[0] = idleColor;
							leds[NUM_LEDS / 4] = idleColor;
							leds[NUM_LEDS / 2] = idleColor;
							leds[NUM_LEDS / 4 * 3] = idleColor;
						} else {
							leds[Led_Address(i) % NUM_LEDS] = idleColor;
							leds[(Led_Address(i) + NUM_LEDS / 4) % NUM_LEDS] = idleColor;
							leds[(Led_Address(i) + NUM_LEDS / 2) % NUM_LEDS] = idleColor;
							leds[(Led_Address(i) + NUM_LEDS / 4 * 3) % NUM_LEDS] = idleColor;
						}
						FastLED.show();
						for (uint8_t i = 0; i <= 50; i++) {
							#ifdef BATTERY_MEASURE_ENABLE
								if (hlastVolume != AudioPlayer_GetCurrentVolume() || lastLedBrightness != Led_Brightness || LED_INDICATOR_IS_SET(LedIndicatorType::Error) || LED_INDICATOR_IS_SET(LedIndicatorType::Ok) || LED_INDICATOR_IS_SET(LedIndicatorType::VoltageWarning) || LED_INDICATOR_IS_SET(LedIndicatorType::Voltage) || gPlayProperties.playMode != NO_PLAYLIST || !gButtons[gShutdownButton].currentState || System_IsSleepRequested()) {
							#else
								if (hlastVolume != AudioPlayer_GetCurrentVolume() || lastLedBrightness != Led_Brightness || LED_INDICATOR_IS_SET(LedIndicatorType::Error) || LED_INDICATOR_IS_SET(LedIndicatorType::Ok) || gPlayProperties.playMode != NO_PLAYLIST || !gButtons[gShutdownButton].currentState || System_IsSleepRequested()) {
							#endif
								break;
							} else {
								vTaskDelay(portTICK_RATE_MS * 10);
							}
						}
					}
				}
				break;

			case BUSY: // If uC is busy (parsing SD-card)
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
					vTaskDelay(portTICK_RATE_MS * 100);
				} else {
					for (uint8_t i = 0; i < NUM_LEDS; i++) {
						FastLED.clear();
						if (Led_Address(i) == 0) {
							leds[0] = CRGB::BlueViolet;
							leds[NUM_LEDS / 4] = CRGB::BlueViolet;
							leds[NUM_LEDS / 2] = CRGB::BlueViolet;
							leds[NUM_LEDS / 4 * 3] = CRGB::BlueViolet;
						} else {
							leds[Led_Address(i) % NUM_LEDS] = CRGB::BlueViolet;
							leds[(Led_Address(i) + NUM_LEDS / 4) % NUM_LEDS] = CRGB::BlueViolet;
							leds[(Led_Address(i) + NUM_LEDS / 2) % NUM_LEDS] = CRGB::BlueViolet;
							leds[(Led_Address(i) + NUM_LEDS / 4 * 3) % NUM_LEDS] = CRGB::BlueViolet;
						}
						FastLED.show();
						if (gPlayProperties.playMode != BUSY) {
							break;
						}
						vTaskDelay(portTICK_RATE_MS * 50);
					}
				}
				break;

			default: // If playlist is active (doesn't matter which type)
				if (!gPlayProperties.playlistFinished) {
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
								uint8_t numLedsToLight = map(gPlayProperties.currentRelPos, 0, 98, 0, NUM_LEDS);
								for (uint8_t led = 0; led < numLedsToLight; led++) {
									if (System_AreControlsLocked()) {
										leds[Led_Address(led)] = CRGB::Red;
									} else if (!gPlayProperties.pausePlay) { // Hue-rainbow
										leds[Led_Address(led)].setHue((uint8_t)(((float)PROGRESS_HUE_END - (float)PROGRESS_HUE_START) / (NUM_LEDS-1) * led + PROGRESS_HUE_START));
									}
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

								leds[Led_Address(0)] = generalColor;
								if (NUM_LEDS > 1) {
									leds[(Led_Address(NUM_LEDS / 4)) % NUM_LEDS] = generalColor;
									leds[(Led_Address(NUM_LEDS / 2)) % NUM_LEDS] = generalColor;
									leds[(Led_Address(NUM_LEDS / 4 * 3)) % NUM_LEDS] = generalColor;
								}
								break;
							}
						}
					}
					else { // ... but do things a little bit different for Webstream as there's no progress available
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
					vTaskDelay(portTICK_RATE_MS * 5);
				}
			}
			vTaskDelay(portTICK_RATE_MS * 1);
			//esp_task_wdt_reset();
		}
		vTaskDelete(NULL);
	#endif
}
