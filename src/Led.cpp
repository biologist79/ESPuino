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
	constexpr uint8_t Led_IdleDotDistance = NUM_LEDS / NUM_LEDS_IDLE_DOTS;

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
#ifdef NEOPIXEL_ENABLE
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

	void Led_DrawIdleDots(CRGB* leds, uint8_t offset, CRGB::HTMLColorCode color) {
		for (uint8_t i=0; i<NUM_LEDS_IDLE_DOTS; i++) {
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
#endif

#ifdef NEOPIXEL_ENABLE
	static void Led_Task(void *parameter) {
		static uint8_t hlastVolume = AudioPlayer_GetCurrentVolume();
		static double lastPos = gPlayProperties.currentRelPos;
		static bool showEvenError = false;
		static bool turnedOffLeds = false;
		static bool singleLedStatus = false;
		static uint8_t ledPosWebstream = 0;
		static uint8_t webstreamColor = 0;
		static uint8_t lastLedBrightness = Led_Brightness;
		static CRGB::HTMLColorCode idleColor;
		static CRGB::HTMLColorCode speechColor = CRGB::Yellow;
		static CRGB leds[NUM_LEDS];
		FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
		FastLED.setBrightness(Led_Brightness);
		FastLED.setDither(DISABLE_DITHER);

		static LedAnimationType activeAnimation = LedAnimationType::NoNewAnimation;
		static LedAnimationType nextAnimation = LedAnimationType::NoNewAnimation;
		static bool animationActive = false;
		static int32_t animationTimer;
		static uint32_t animationIndex;
		static uint32_t subAnimationIndex;
		static uint32_t staticLastBarLenghtPlaylist;
		static uint32_t staticLastTrack;
		static float staticBatteryLevel;


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

			uint32_t taskDelay = 20;

			// check indications and set led-mode
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
			#ifdef BATTERY_MEASURE_ENABLE
			} else if (LED_INDICATOR_IS_SET(LedIndicatorType::VoltageWarning)) {
				LED_INDICATOR_CLEAR(LedIndicatorType::VoltageWarning);
				nextAnimation = LedAnimationType::VoltageWarning;
			} else if (LED_INDICATOR_IS_SET(LedIndicatorType::Voltage)) {
				nextAnimation = LedAnimationType::BatteryMeasurement;
			#endif
			} else if (hlastVolume != AudioPlayer_GetCurrentVolume()) {
				nextAnimation = LedAnimationType::Volume;
			} else if (LED_INDICATOR_IS_SET(LedIndicatorType::Rewind)) {
				LED_INDICATOR_CLEAR(LedIndicatorType::Rewind);
				nextAnimation = LedAnimationType::Rewind;
			} else if (LED_INDICATOR_IS_SET(LedIndicatorType::PlaylistProgress)) {
				nextAnimation = LedAnimationType::Playlist;
			} else if (gPlayProperties.playlistFinished) {
				nextAnimation = LedAnimationType::Idle; // todo: what is valid?
			} else if (gPlayProperties.currentSpeechActive) {
				nextAnimation = LedAnimationType::Speech;
			} else if (gPlayProperties.pausePlay) {
				nextAnimation = LedAnimationType::Pause;
			} else if (gPlayProperties.isWebstream) {
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

			// check for instant transition
			if (nextAnimation < activeAnimation) {
				animationActive = false; // abort current animation
				animationTimer = 0;
			}
			// do normal transitions
			if ((!animationActive) && (animationTimer <= 0)) {
				activeAnimation = nextAnimation; // set new animation
				animationIndex = 0;
			}

			// apply brightness-changes
			if (lastLedBrightness != Led_Brightness) {
				FastLED.setBrightness(Led_Brightness);
				lastLedBrightness = Led_Brightness;
			}

			if (animationTimer <= 0) {
				switch (activeAnimation) {
					// --------------------------------------------------
					// Bootup - Animation
					// --------------------------------------------------
					case LedAnimationType::Boot: {
						animationTimer = 500;

						if (millis() > 10000) {
							fill_solid(leds, NUM_LEDS, CRGB::Red);
							if (showEvenError) {
								// and then draw in the black dots
								for (uint8_t i=0;i<NUM_LEDS;i +=2) {
									leds[i] = CRGB::Black;
								}
							}
						} else {
							fill_solid(leds, NUM_LEDS, CRGB::Black);
							const uint8_t startLed = (showEvenError) ? 1 : 0;
							for (uint8_t i=startLed;i<NUM_LEDS;i+=2) {
								leds[i] = CRGB::Orange;
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
								animationTimer = 5;
							} else {
								if (singleLedStatus) {
									leds[0] = CRGB::Red;
								}
								FastLED.show();
								singleLedStatus = !singleLedStatus;
								animationTimer = 50;
							}
							animationActive = false;
						} else {
							if ((millis() - gButtons[gShutdownButton].firstPressedTimestamp >= intervalToLongPress) && (animationIndex >= NUM_LEDS)) {
								animationTimer = 50;
								// don't end animation, we already reached the shutdown.
							} else {
								if (animationIndex == 0) {
									FastLED.clear();
								}
								if (animationIndex < NUM_LEDS) {
									leds[Led_Address(animationIndex)] = CRGB::Red;
									if (gButtons[gShutdownButton].currentState) {
										animationTimer = 5;
										animationActive = false;
									} else {
										animationTimer = intervalToLongPress / NUM_LEDS;
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
						CRGB signalColor = CRGB::Green;
						uint16_t onTime = 400;
						animationActive = true;
						if (activeAnimation == LedAnimationType::Error) {
							signalColor = CRGB::Red;
							onTime = 200;
						}

						if (NUM_LEDS == 1) {
							FastLED.clear();
							if (singleLedStatus) {
								leds[0] = signalColor;
							}
							FastLED.show();
							singleLedStatus = !singleLedStatus;

							if (animationIndex < 5) {
								animationIndex++;
								animationTimer = 100;
							} else {
								animationActive = false;
							}
						} else {
							fill_solid(leds, NUM_LEDS, signalColor);
							FastLED.show();
							if (animationIndex > 0) {
								animationActive = false;
							} else {
								animationIndex++;
								animationTimer = onTime;
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
						const uint32_t ledValue =  map(AudioPlayer_GetCurrentVolume(), 0,
										AudioPlayer_GetMaxVolume(), 0,
										NUM_LEDS * DIMMABLE_STATES);
						const uint8_t fullLeds = ledValue / DIMMABLE_STATES;
						const uint8_t lastLed = ledValue % DIMMABLE_STATES;

						FastLED.clear();

						if (NUM_LEDS == 1) {
							const uint8_t hue = 85 - (90 *
								((double)AudioPlayer_GetCurrentVolume() /
								(double)AudioPlayer_GetMaxVolumeSpeaker()));
							leds[0].setHue(hue);
						} else {
							/*
							* (Inverse) color-gradient from green (85) back to (still)
							* red (250) using unsigned-cast.
							*/
							for (int led = 0; led < fullLeds; led++) {
								const uint8_t hue = (-86.0f) / (NUM_LEDS-1) * led + 85.0f;
								leds[Led_Address(led)].setHue(hue);
							}
							if (lastLed > 0) {
								const uint8_t hue = (-86.0f) / (NUM_LEDS-1) * fullLeds + 85.0f;
								leds[Led_Address(fullLeds)].setHue(hue);
								leds[Led_Address(fullLeds)] = Led_DimColor(leds[Led_Address(fullLeds)], lastLed);
							}
						}
						FastLED.show();

						// reset animation if volume changes
						if (hlastVolume != AudioPlayer_GetCurrentVolume()) {
							hlastVolume = AudioPlayer_GetCurrentVolume();
							animationIndex = 0;
						}

						if (animationIndex < LED_VOLUME_INDICATOR_NUM_CYCLES) {
							animationTimer = 20;
							animationIndex ++;
						} else {
							animationActive = false;
						}
					} break;

					// --------------------------------------------------
					// Animation of Voltage Warning
					// --------------------------------------------------
					case LedAnimationType::VoltageWarning: {
						// Single + Multiple LEDs: flashes red three times if battery-voltage is low
						animationActive = true;

						FastLED.clear();
						if (animationIndex % 2 == 0) {
							fill_solid(leds, NUM_LEDS, CRGB::Red);
						}
						FastLED.show();

						if (animationIndex < 6) {
							animationIndex++;
							animationTimer = 200;
						} else {
							animationActive = false;
						}
					} break;

					// --------------------------------------------------
					// Animation of Battery Measurement
					// --------------------------------------------------
					#ifdef BATTERY_MEASURE_ENABLE
						case LedAnimationType::BatteryMeasurement: {
							LED_INDICATOR_CLEAR(LedIndicatorType::Voltage);
							// Single-LED: indicates voltage coloured between gradient green (high) => red (low)
							// Multi-LED: number of LEDs indicates voltage-level with having green >= 60% ; orange < 60% + >= 30% ; red < 30%
							float batteryLevel = Battery_EstimateLevel();
							if (batteryLevel < 0) { // If voltage is too low or no battery is connected
								LED_INDICATOR_SET(LedIndicatorType::Error);
								break;
							} else {
								if (animationIndex == 0) {
									staticBatteryLevel = batteryLevel;
									animationActive = true;
									FastLED.clear();
								}
								if (NUM_LEDS == 1) {
									if (staticBatteryLevel < 0.3) {
										leds[0] = CRGB::Red;
									} else if (staticBatteryLevel < 0.6) {
										leds[0] = CRGB::Orange;
									} else {
										leds[0] = CRGB::Green;
									}
									FastLED.show();

									animationTimer = 20*100;
									animationActive = false;
								} else {
									uint8_t numLedsToLight = staticBatteryLevel * NUM_LEDS;
									if (numLedsToLight > NUM_LEDS) {    // Can happen e.g. if no battery is connected
										numLedsToLight = NUM_LEDS;
									}

									if (animationIndex < numLedsToLight) {
										if (staticBatteryLevel < 0.3) {
											leds[Led_Address(animationIndex)] = CRGB::Red;
										} else if (staticBatteryLevel < 0.6) {
											leds[Led_Address(animationIndex)] = CRGB::Orange;
										} else {
											leds[Led_Address(animationIndex)] = CRGB::Green;
										}
										FastLED.show();

										animationIndex ++;
										animationTimer = 20;
									} else {
										animationTimer = 20*100;
										animationActive = false;
									}
								}
							}
						} break;
					#endif

					// --------------------------------------------------
					// Animation of Rewind
					// --------------------------------------------------
					case LedAnimationType::Rewind: {
						if (NUM_LEDS >= 4) {
							animationActive = true;

							if (animationIndex < (NUM_LEDS)) {
								leds[Led_Address(NUM_LEDS - 1 - animationIndex)] = CRGB::Black;
								FastLED.show();
								animationTimer = 30;
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
								const uint32_t ledValue =  map(gPlayProperties.currentTrackNumber, 0, gPlayProperties.numberOfTracks - 1, 0, NUM_LEDS * DIMMABLE_STATES);
								const uint8_t fullLeds = ledValue / DIMMABLE_STATES;
								const uint8_t lastLed = ledValue % DIMMABLE_STATES;

								if (LED_INDICATOR_IS_SET(LedIndicatorType::PlaylistProgress)) {
									LED_INDICATOR_CLEAR(LedIndicatorType::PlaylistProgress);
									// only animate diff, if triggered again
									if (animationActive) {
										// forward
										if (staticLastTrack < gPlayProperties.currentTrackNumber) {
											if (animationIndex > 1) {
												animationIndex = 1;
												subAnimationIndex = staticLastBarLenghtPlaylist;
											}
										// backwards
										} else if (staticLastTrack > gPlayProperties.currentTrackNumber) {
											if (staticLastBarLenghtPlaylist < fullLeds) {
												animationIndex = 1;
												subAnimationIndex = staticLastBarLenghtPlaylist;
											} else {
												animationIndex = 42;
												subAnimationIndex = staticLastBarLenghtPlaylist;
											}
										}
									}
									staticLastTrack = gPlayProperties.currentTrackNumber;
								}

								if (animationIndex == 0 ) {
									animationActive = true;
									subAnimationIndex = 0;
									animationIndex++;
								}

								animationTimer = 30;
								uint8_t barLength = 0;
								switch (animationIndex) {
									case 1:
										barLength = subAnimationIndex;
										if (subAnimationIndex >= fullLeds) {
											animationIndex++;
											subAnimationIndex = 0;
										} else {
											subAnimationIndex++;
										}
									break;
									case 2:
										// wait
										subAnimationIndex ++;
										if (subAnimationIndex >= 50) {
											animationIndex++;
											subAnimationIndex = fullLeds;
										}
										barLength = fullLeds;
									break;
									case 3:
										// negative
										barLength = subAnimationIndex;
										if (subAnimationIndex == 0) {
											animationIndex++;
										} else {
											subAnimationIndex--;
										}
									break;
									case 42:
										// move back to target and wait there
										barLength = subAnimationIndex;
										if (subAnimationIndex <= fullLeds) {
											animationIndex = 2;
										} else {
											subAnimationIndex--;
										}
									break;
									default:
										// done
										animationActive = false;
									break;
								}

								// draw bar
								FastLED.clear();
								for (uint8_t i = 0; i < barLength; i++) {
									leds[Led_Address(i)] = CRGB::Blue;
								}
								if (barLength == fullLeds && lastLed > 0) {
									leds[Led_Address(barLength)] = Led_DimColor(CRGB::Blue, lastLed);
								}
								staticLastBarLenghtPlaylist = barLength;
								FastLED.show();
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
							} else if (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode()) {
								if (Bluetooth_Source_Connected()) {
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
										if (gPlayProperties.currentSpeechActive) {
											idleColor = speechColor;
										}
									}
								}
							}
							FastLED.clear();
							Led_DrawIdleDots(leds, animationIndex, idleColor);
							FastLED.show();
							animationTimer = 50*10;
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
						if (NUM_LEDS == 1) {
							FastLED.clear();
							singleLedStatus = !singleLedStatus;
							if (singleLedStatus) {
								leds[0] = CRGB::BlueViolet;
							}
							FastLED.show();
							animationTimer = 100;
							animationActive = false;
						} else {
							if (animationIndex < NUM_LEDS) {
								FastLED.clear();
								Led_DrawIdleDots(leds, animationIndex, idleColor);
								FastLED.show();
								animationTimer = 50;
								animationIndex++;
							} else {
								animationActive = false;
							}
						}
					} break;

					// --------------------------------------------------
					// Animation of Progress
					// --------------------------------------------------
					case LedAnimationType::Speech:
					case LedAnimationType::Pause: { // TODO: separate animations?
						FastLED.clear();
						CRGB::HTMLColorCode generalColor = CRGB::Orange;
						if (gPlayProperties.isWebstream) {
							if (gPlayProperties.currentSpeechActive) {
								generalColor = speechColor;
							}
							if (NUM_LEDS == 1) {
								leds[0] = generalColor;
							} else {
								leds[Led_Address(ledPosWebstream)] = generalColor;
								leds[(Led_Address(ledPosWebstream) + NUM_LEDS / 2) % NUM_LEDS] = generalColor;
							}
						} else {
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
							Led_DrawIdleDots(leds, pauseOffset, generalColor);
						}
						FastLED.show();
						animationTimer = 10;
					} break;

					case LedAnimationType::Progress: {
						if (gPlayProperties.currentRelPos != lastPos || animationIndex == 0) {
							animationIndex = 1;
							lastPos = gPlayProperties.currentRelPos;
							FastLED.clear();
							if (NUM_LEDS == 1) {
								leds[0].setHue((uint8_t)(85 - ((double)90 / 100) * gPlayProperties.currentRelPos));
							} else {
								const uint32_t ledValue = map(gPlayProperties.currentRelPos, 0, 98, 0, NUM_LEDS * DIMMABLE_STATES);
								const uint8_t fullLeds = ledValue / DIMMABLE_STATES;
								const uint8_t lastLed = ledValue % DIMMABLE_STATES;
								for (uint8_t led = 0; led < fullLeds; led++) {
									if (System_AreControlsLocked()) {
										leds[Led_Address(led)] = CRGB::Red;
									} else if (!gPlayProperties.pausePlay) { // Hue-rainbow
										leds[Led_Address(led)].setHue((uint8_t)(((float)PROGRESS_HUE_END - (float)PROGRESS_HUE_START) / (NUM_LEDS-1) * led + PROGRESS_HUE_START));
									}
								}
								if (lastLed > 0) {
									if (System_AreControlsLocked()) {
										leds[Led_Address(fullLeds)] = CRGB::Red;
									} else {
										leds[Led_Address(fullLeds)].setHue((uint8_t)(((float)PROGRESS_HUE_END - (float)PROGRESS_HUE_START) / (NUM_LEDS-1) * fullLeds + PROGRESS_HUE_START));
									}
									leds[Led_Address(fullLeds)] = Led_DimColor(leds[Led_Address(fullLeds)], lastLed);
								}
							}
							FastLED.show();
							animationTimer = 10;
							animationActive = false;
						}
					} break;

					case LedAnimationType::Webstream: {
						// do things a little bit different for Webstream as there's no progress available
						if (animationIndex == 0) {
							animationIndex = 1;
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
							}
							FastLED.show();
							animationTimer = 5 * 950;
							animationActive = false;
						}
					} break;

					default:
						FastLED.clear();
						FastLED.show();
						animationTimer = 50;
					break;
				}
			}

			// get the time to wait
			if ((animationTimer > 0) && (animationTimer < taskDelay)) {
				taskDelay = animationTimer;
			}
			animationTimer -= taskDelay;
			vTaskDelay(portTICK_RATE_MS * taskDelay);
		}
		vTaskDelete(NULL);
	}
#endif
