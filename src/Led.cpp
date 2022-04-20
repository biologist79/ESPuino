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

#ifdef NEOPIXEL_ENABLE
    #include <FastLED.h>

    #define LED_INITIAL_BRIGHTNESS 16u
    #define LED_INITIAL_NIGHT_BRIGHTNESS 2u

    #define LED_INDICATOR_SET(indicator) ((Led_Indicators) |= (1u << ((uint8_t)indicator)))
    #define LED_INDICATOR_IS_SET(indicator) (((Led_Indicators) & (1u << ((uint8_t)indicator))) > 0u)
    #define LED_INDICATOR_CLEAR(indicator) ((Led_Indicators) &= ~(1u << ((uint8_t)indicator)))

    static uint32_t Led_Indicators = 0u;

    bool Led_Pause = false; // Used to pause Neopixel-signalisation (while NVS-writes as this leads to exceptions; don't know why)

    uint8_t Led_InitialBrightness = LED_INITIAL_BRIGHTNESS;
    uint8_t Led_Brightness = LED_INITIAL_BRIGHTNESS;
    uint8_t Led_NightBrightness = LED_INITIAL_NIGHT_BRIGHTNESS;

    void LedTask(void *parameter);
    uint8_t Led_Address(uint8_t number);
    CRGB leds[NUM_LEDS];

// moved from task - otherwise strange results
uint8_t ledChgCounter = 0; // Counts up whith ledChgInterval
uint8_t ledSlowCounter = 0; // Counts up whith ledSlowInterval
uint8_t ledStaticCounter = 0; // Can be used in Special Functions like Shutdown-Button
unsigned long lastChgTimestamp = 0;
unsigned long lastSlowTimestamp = 0;
uint8_t lastLedBrightness = Led_Brightness;
uint8_t lastPos = gPlayProperties.currentRelPos;
bool notificationProgress = true;
bool showEvenError = false;
bool turnedOffLeds = false;
uint16_t ledChgInterval = 100; // time in msecs, adjust for responsiveness of LED Actions (minimum ??)
uint16_t ledSlowInterval = 800; // Intervalfor visual changes of "normal" Modes ie. not Notifications

    // Only enable measurements if valid GPIO is used
    #ifdef MEASURE_BATTERY_VOLTAGE
        #if (VOLTAGE_READ_PIN >= 0 && VOLTAGE_READ_PIN <= 39)
            #define ENABLE_BATTERY_MEASUREMENTS
        #endif
    #endif
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
        uint8_t nvsNLedBrightness = gPrefsSettings.getUChar("nLedBrightness", 0);
        if (nvsNLedBrightness) {
            Led_NightBrightness = nvsNLedBrightness;
            snprintf(Log_Buffer, Log_BufferLength, "%s: %d", (char *) FPSTR(restoredInitialBrightnessForNmFromNvs), nvsNLedBrightness);
            Log_Println(Log_Buffer, LOGLEVEL_INFO);
        } else {
            gPrefsSettings.putUChar("nLedBrightness", Led_NightBrightness);
            Log_Println((char *) FPSTR(wroteNmBrightnessToNvs), LOGLEVEL_ERROR);
        }

        FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
        FastLED.setBrightness(Led_Brightness);

        xTaskCreatePinnedToCore(
            LedTask,   /* Function to implement the task */
            "LedTask", /* Name of the task */
            1512,       /* Stack size in words */
            NULL,       /* Task input parameter */
            1,          /* Priority of the task */
            NULL,       /* Task handle. */
            0           /* Core where the task should run */
        );

        FastLED.clear(true);
    #endif
}

void Led_Exit(void) {
    #ifdef NEOPIXEL_ENABLE
        FastLED.clear(true);
//        FastLED.show();
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

void Led_ResetToInitialBrightness(void) {
    #ifdef NEOPIXEL_ENABLE
        Led_Brightness = Led_InitialBrightness;
        Log_Println((char *) FPSTR(ledsDimmedToInitialValue), LOGLEVEL_INFO);
    #endif
}

void Led_ResetToNightBrightness(void) {
    #ifdef NEOPIXEL_ENABLE
        Led_Brightness = Led_NightBrightness;
        Log_Println((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
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
    #endif
}

// Switches Neopixel-addressing from clockwise to counter clockwise (and vice versa)
uint8_t Led_Address(uint8_t number) {
    #ifdef NEOPIXEL_REVERSE_ROTATION
        return NUM_LEDS - 1 - number;
    #else
        return number;
    #endif
}

void LedTask(void *parameter) {
    #ifdef NEOPIXEL_ENABLE
        static uint8_t hlastVolume = AudioPlayer_GetCurrentVolume();
        static uint8_t numLedsToLight;
        static uint8_t ledPosWebstream = 0;
        static uint8_t webstreamColor = 0;
        static bool redrawChgProgress = true; // used to invoke fast LED actions
        static bool redrawSlowProgress = false; // used to invoke slow LED actions
        static CRGB::HTMLColorCode idleColor;
        static CRGB::HTMLColorCode speechColor = CRGB::Yellow;
        static CRGB::HTMLColorCode generalColor;

		TickType_t xLastWakeTime;
		const TickType_t xFrequency = 210;  // Ticks entsprechen etwa 110msec
		xLastWakeTime = xTaskGetTickCount();
        

        for (;;) {
            FastLED.show();
            // first iterate through Task - at the End DelayUntil to avoid double calls
            redrawChgProgress = false;
            redrawSlowProgress = false;
            if ((millis() - lastChgTimestamp) >= (ledChgInterval-5)) { // give a little more headroom, when Ticks not exact calculated to Time
                if (ledChgCounter < NUM_LEDS-1) {
                    ledChgCounter++;
                } else {
                    ledChgCounter = 0;
                }
                if ((millis() - lastSlowTimestamp) >= (ledSlowInterval-5)) { // give a little more headroom, when Ticks not exact calculated to Time
                    if (ledSlowCounter < NUM_LEDS-1) {
                        ledSlowCounter++;
                    } else {
                        ledSlowCounter = 0;
                    }
                    redrawSlowProgress = true; 
                    lastSlowTimestamp = millis();
                }
                lastChgTimestamp = millis();
                redrawChgProgress = true;
            }
            if (redrawSlowProgress || notificationProgress) {
                    FastLED.clear();
                }

            if (System_IsSleepRequested()) { // If deepsleep is planned, turn off LEDs first in order to avoid LEDs still glowing when ESP32 is in deepsleep
                if (!turnedOffLeds) {
                    FastLED.clear(true);
                    turnedOffLeds = true;
                }
                redrawChgProgress = false;
                redrawSlowProgress = false;
            }

            if (lastLedBrightness != Led_Brightness) {
                FastLED.setBrightness(Led_Brightness);
                lastLedBrightness = Led_Brightness;
            }

            // Multi-LED: rotates orange unless boot isn't complete
            // Single-LED: blinking orange
            if (!LED_INDICATOR_IS_SET(LedIndicatorType::BootComplete) && redrawSlowProgress) {
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
                showEvenError = !showEvenError;
            } else {
                notificationProgress = false;
            }

            // Multi-LED: growing red as long button for sleepmode is pressed.
            // Single-LED: red when pressed and flashing red when long interval-duration is reached
            if (gShutdownButton < (sizeof(gButtons) / sizeof(gButtons[0])) - 1) { // Only show animation, if CMD_SLEEPMODE was assigned to BUTTON_n_LONG + button is pressed
                if (!gButtons[gShutdownButton].currentState && (millis() - gButtons[gShutdownButton].firstPressedTimestamp >= 150) && redrawChgProgress) {
                    if (NUM_LEDS == 1) {
                        if ((millis() - gButtons[gShutdownButton].firstPressedTimestamp <= intervalToLongPress)) {
                            leds[0] = CRGB::Red;
                        } else {
                            if (showEvenError) {
                                leds[0] = CRGB::Red;
                            } else {
                                leds[0] = CRGB::Black;
                            }
                            showEvenError = !showEvenError;
                        }
                    } else {
                        if ((millis() - gButtons[gShutdownButton].firstPressedTimestamp >= intervalToLongPress) && (millis() - gButtons[gShutdownButton].firstPressedTimestamp <= intervalToLongPress+100)) {
                            ledStaticCounter = 0; // start with first LED if Button was pressed
                        }
                        for (uint8_t i=0;i<=ledStaticCounter;i++) {
                            leds[Led_Address(i)] = CRGB::Red;
                        }

                        if (ledStaticCounter == NUM_LEDS) {
                            LED_INDICATOR_CLEAR(LedIndicatorType::Error);
                            notificationProgress = false;
                            redrawChgProgress = true;
                        }
                        else {
                            ledStaticCounter++;
                        }
                    }
                }
            }

            // Multi-LED: all leds flash red 1x
            // Single-LED: led flashes red 5x
            if (LED_INDICATOR_IS_SET(LedIndicatorType::Error) && redrawChgProgress) { // If error occured (e.g. RFID-modification not accepted)
                if (!notificationProgress) {
                    ledStaticCounter = 0;
                    notificationProgress = !notificationProgress;
                }
                if (NUM_LEDS == 1) {
                    if (ledStaticCounter % 2 == 0) {
                        leds[0] = CRGB::Red;
                    } else {
                        leds[0] = CRGB::Black;
                    }
                } else {
                    for (uint8_t led = 0; led < NUM_LEDS; led++) {
                        leds[Led_Address(led)] = CRGB::Red;
                    }
                }

                if (ledStaticCounter == 10) {
                    LED_INDICATOR_CLEAR(LedIndicatorType::Error);
                }
                else {
                    ledStaticCounter++;
                }
            }

            // Multi-LED: all leds flash green 1x
            // Single-LED: led flashes green 5x
            if (LED_INDICATOR_IS_SET(LedIndicatorType::Ok) && redrawChgProgress) { // If action was accepted
                if (!notificationProgress) {
                    ledStaticCounter = 0;
                    notificationProgress = !notificationProgress;
                }

                if (NUM_LEDS == 1) {
                    if (ledStaticCounter % 2 == 0) {
                        leds[0] = CRGB::Green;
                    } else {
                        leds[0] = CRGB::Black;
                    }
                } else {
                    for (uint8_t led = 0; led < NUM_LEDS; led++) {
                        leds[Led_Address(led)] = CRGB::Green;
                    }
                }

                if (ledStaticCounter == 10) {
                    LED_INDICATOR_CLEAR(LedIndicatorType::Ok);
                    notificationProgress = false;
                    redrawChgProgress = true;
                }
                else {
                    ledStaticCounter++;
                }
            }

            #ifdef ENABLE_BATTERY_MEASUREMENTS
            // Single + Multiple LEDs: flashes red three times if battery-voltage is low
            if (LED_INDICATOR_IS_SET(LedIndicatorType::VoltageWarning) && redrawChgProgress) {
                if (!notificationProgress) {
                    ledStaticCounter = 0;
                    notificationProgress = !notificationProgress;
                }

                if (NUM_LEDS == 1) {
                    if (ledStaticCounter % 2 == 0) {
                        leds[0] = CRGB::Red;
                    } else {
                        leds[0] = CRGB::Black;
                    }
                } else {
                    for (uint8_t led = 0; led < NUM_LEDS; led++) {
                        leds[Led_Address(led)] = CRGB::Red;
                    }
                }

                if (ledStaticCounter == 6) { // Indicator needs 6 Iterations to complete. Time needed: ledChgInterval * 6
                    LED_INDICATOR_CLEAR(LedIndicatorType::VoltageWarning);
                    notificationProgress = false;
                    redrawChgProgress = true;
                }
                else {
                    ledStaticCounter++;
                }
            }

            // Single-LED: indicates voltage coloured between gradient green (high) => red (low)
            // Multi-LED: number of LEDs indicates voltage-level with having green >= 60% ; orange < 60% + >= 30% ; red < 30%
            if (LED_INDICATOR_IS_SET(LedIndicatorType::Voltage)) {
                LED_INDICATOR_CLEAR(LedIndicatorType::Voltage);
                float currentVoltage = Battery_GetVoltage();
                float vDiffIndicatorRange = voltageIndicatorHigh - voltageIndicatorLow;
                float vDiffCurrent = currentVoltage - voltageIndicatorLow;

                if (vDiffCurrent < 0) { // If voltage is too low or no battery is connected
                    LED_INDICATOR_SET(LedIndicatorType::Error);
                    break;
                } else {
                    FastLED.clear();
                    if (NUM_LEDS == 1) {
                        if ((float) vDiffCurrent / vDiffIndicatorRange >= 0.6) {
                            leds[0] = CRGB::Green;
                        } else if ((float) vDiffCurrent / vDiffIndicatorRange < 0.6 && (float) vDiffCurrent / vDiffIndicatorRange >= 0.3) {
                            leds[0] = CRGB::Orange;
                        } else {
                            leds[0] = CRGB::Red;
                        }
                        FastLED.show();
                    } else {
                        uint8_t numLedsToLight = ((float)vDiffCurrent / vDiffIndicatorRange) * NUM_LEDS;
                        if (numLedsToLight > NUM_LEDS) {    // Can happen e.g. if no battery is connected
                            numLedsToLight = NUM_LEDS;
                        }
                        for (uint8_t led = 0; led < numLedsToLight; led++) {
                            if (((float)numLedsToLight / NUM_LEDS) >= 0.6) {
                                leds[Led_Address(led)] = CRGB::Green;
                            } else if (((float)numLedsToLight / NUM_LEDS) < 0.6 && ((float)numLedsToLight / NUM_LEDS) >= 0.3) {
                                leds[Led_Address(led)] = CRGB::Orange;
                            } else {
                                leds[Led_Address(led)] = CRGB::Red;
                            }
                        }
                    }

                }
            }
            #endif

            // Single-LED: led indicates loudness between green (low) => red (high)
            // Multiple-LEDs: number of LEDs indicate loudness; gradient is shown between green (low) => red (high)
            if (hlastVolume != AudioPlayer_GetCurrentVolume() && redrawChgProgress) { // If volume has been changed
                if (!notificationProgress) {
                    ledStaticCounter = 0;
                    notificationProgress = !notificationProgress;
                }
                uint8_t numLedsToLight = map(AudioPlayer_GetCurrentVolume(), 0, AudioPlayer_GetMaxVolume(), 0, NUM_LEDS);
                hlastVolume = AudioPlayer_GetCurrentVolume();

                if (NUM_LEDS == 1) {
                    leds[0].setHue((uint8_t)(85 - (90 * ((double)AudioPlayer_GetCurrentVolume() / (double)AudioPlayer_GetMaxVolumeSpeaker()))));
                } else {
                    for (int led = 0; led < numLedsToLight; led++) { // (Inverse) color-gradient from green (85) back to (still) red (250) using unsigned-cast
                        leds[Led_Address(led)].setHue((uint8_t)(85 - ((double)90 / NUM_LEDS) * led));
                    }
                }
                if (ledStaticCounter == 2) { // Show Indicator this long: ledChgInterval * 2
                    LED_INDICATOR_CLEAR(LedIndicatorType::VoltageWarning);
                    notificationProgress = false;
                    redrawChgProgress = true;
                }
                else {
                    ledStaticCounter++;
                }

            }

            // < 4 LEDs: doesn't make sense at all
            // >= 4 LEDs: collapsing ring (blue => black)
            #if NUM_LEDS >=4
            if (LED_INDICATOR_IS_SET(LedIndicatorType::Rewind) && redrawChgProgress) {
                if (!notificationProgress) {
                    ledStaticCounter = NUM_LEDS-1;
                    notificationProgress = !notificationProgress;
                }

                for (uint8_t led = 0; led < (ledStaticCounter-1); led++) {
                    leds[Led_Address(led)] = CRGB::Blue;
                }
                for (uint8_t led = NUM_LEDS-1; led >= ledStaticCounter; led--) {
                    leds[Led_Address(led)] = CRGB::Blue;
                }

                if (ledStaticCounter == 0) {
                    LED_INDICATOR_CLEAR(LedIndicatorType::Rewind);
                    notificationProgress = false;
                    redrawChgProgress = true;
                }
                else {
                    ledStaticCounter--;
                }
            }
            #endif

            // < 4 LEDs: doesn't make sense at all
            // >= 4 LEDs: growing ring (black => blue); relative number of LEDs indicate playlist-progress
            #if NUM_LEDS >=4
            if (LED_INDICATOR_IS_SET(LedIndicatorType::PlaylistProgress) && redrawChgProgress) {
                if (!notificationProgress) {
                    ledStaticCounter = 0;
                    notificationProgress = !notificationProgress;
                }

                if (gPlayProperties.numberOfTracks > 1 && gPlayProperties.currentTrackNumber < gPlayProperties.numberOfTracks) {
                    uint8_t numLedsToLight = map(gPlayProperties.currentTrackNumber, 0, gPlayProperties.numberOfTracks - 1, 0, NUM_LEDS);
                    for (uint8_t i = 0; i < numLedsToLight; i++) {
                        leds[Led_Address(i)] = CRGB::Blue;
                    }
                }
                if (ledStaticCounter == 5) {
                    LED_INDICATOR_CLEAR(LedIndicatorType::PlaylistProgress);
                    notificationProgress = false;
                    redrawChgProgress = true;
                } else {
                    ledStaticCounter--;
                }
            }
            #endif

        if (!notificationProgress && redrawSlowProgress) {
            switch (gPlayProperties.playMode) {
                case NO_PLAYLIST: // If no playlist is active (idle)
                    FastLED.clear();
                    if (System_GetOperationMode() == OPMODE_BLUETOOTH) {
                        idleColor = CRGB::Blue;
                        }
                    else if (Wlan_IsConnected() && gPlayProperties.currentSpeechActive) {
                        idleColor = speechColor;
                        }
                    else if (Wlan_IsConnected()) {
                        idleColor = CRGB::White;
                        }
                    else {
                        idleColor = CRGB::LightGreen;
                        }

                    leds[Led_Address(ledSlowCounter) % NUM_LEDS] = idleColor;
                    leds[(Led_Address(ledSlowCounter) + NUM_LEDS / 4) % NUM_LEDS] = idleColor;
                    leds[(Led_Address(ledSlowCounter) + NUM_LEDS / 2) % NUM_LEDS] = idleColor;
                    leds[(Led_Address(ledSlowCounter) + NUM_LEDS / 4 * 3) % NUM_LEDS] = idleColor;

                    break;

                case BUSY: // If uC is busy (parsing SD-card)
                    if (NUM_LEDS == 1) {
                        showEvenError = !showEvenError;
                        if (showEvenError) {
                            leds[0] = CRGB::BlueViolet;
                        } else {
                            leds[0] = CRGB::Black;
                        }
                    } else {
                            leds[Led_Address(ledSlowCounter) % NUM_LEDS] = CRGB::BlueViolet;
                            leds[(Led_Address(ledSlowCounter) + NUM_LEDS / 4) % NUM_LEDS] = CRGB::BlueViolet;
                            leds[(Led_Address(ledSlowCounter) + NUM_LEDS / 2) % NUM_LEDS] = CRGB::BlueViolet;
                            leds[(Led_Address(ledSlowCounter) + NUM_LEDS / 4 * 3) % NUM_LEDS] = CRGB::BlueViolet;
                        }
                    break;

                default: // If playlist is active (doesn't matter which type)
                    if (!gPlayProperties.playlistFinished) {
                        //noch prüfen
//                        lastPlayState = gPlayProperties.pausePlay;
//                        lastLockState = System_AreControlsLocked();

                        // Single-LED: led indicates between gradient green (beginning) => red (end)
                        // Multiple-LED: growing number of leds indicate between gradient green (beginning) => red (end)
                        if (!gPlayProperties.isWebstream) {
                            if (gPlayProperties.currentRelPos != lastPos && redrawSlowProgress && !gPlayProperties.pausePlay ) {
                                lastPos = gPlayProperties.currentRelPos;
                                if (NUM_LEDS == 1) {
                                    leds[0].setHue((uint8_t)(85 - ((double)90 / 100) * (double)gPlayProperties.currentRelPos));
                                } else {
                                    numLedsToLight = map(gPlayProperties.currentRelPos, 0, 98, 0, NUM_LEDS);
                                    for (uint8_t led = 0; led < numLedsToLight; led++) {
                                        if (System_AreControlsLocked()) {
                                            leds[Led_Address(led)] = CRGB::Red;
                                        } else { // Hue-rainbow
                                            leds[Led_Address(led)].setHue((uint8_t)(85 - ((double)90 / NUM_LEDS) * led));
                                        }
                                    }
                                }
                            }

                            if (gPlayProperties.pausePlay) {
                                    if (NUM_LEDS > 1) {
                                        if (ledSlowCounter % 2 == 0) {
                                            ledStaticCounter = 0;
                                        } else {
                                            ledStaticCounter = 1;
                                        }
                                    }

                                    generalColor = CRGB::Orange;
                                    if (gPlayProperties.currentSpeechActive) {
                                        generalColor = speechColor;
                                    }

                                    leds[Led_Address(0) + ledStaticCounter] = generalColor;
                                    if (NUM_LEDS > 1) {
                                        leds[(Led_Address(NUM_LEDS) / 4) % NUM_LEDS + ledStaticCounter] = generalColor;
                                        leds[(Led_Address(NUM_LEDS) / 2) % NUM_LEDS + ledStaticCounter] = generalColor;
                                        leds[(Led_Address(NUM_LEDS) / 4 * 3) % NUM_LEDS + ledStaticCounter] = generalColor;
                                    }
//                                    break;
                            }
                        }
                        else { // ... but do things a little bit different for Webstream as there's no progress available
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
                }
            }
            vTaskDelayUntil( &xLastWakeTime, xFrequency );
        }
    #endif
}
