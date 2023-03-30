#ifndef __ESPUINO_SETTINGS_MUSEPROTO_H__
#define __ESPUINO_SETTINGS_MUSEPROTO_H__
    #include "Arduino.h"

    //######################### INFOS ####################################
    /*
     * Make sure to read instructions:
     * https://forum.espuino.de/t/anleitung-raspiaudio-muse-proto-anfaenger-espuino/1598
     *
     * Support for this device is limited, and only available by the community in the
     * forum. (See thread above)
     *
     * This is a device-specific settings file for the RASPIAUDIO Muse Proto
     * offered by https://raspiaudio.com/produit/muse-proto and available at
     * multiple sources. It uses
     *
     * * On-Board Neopixel (1 Pixel)
     * * PN532 NFC Reader, using I2C, GPIOs 12 (CLK) and 32 (DATA)
     * * KY-040 Rotary Encoder with on-board pull ups on GPIOs 36, 39 (Printed VP/VN)
     * * Three buttons without pull ups on GPIOs 18, 19, 0
     * * Batteries measured using the on-board battery controller
     * * Using the on-board 3W output for externals speakers
     *
    */

    //################## GPIO-configuration ##############################
    // Please note: GPIOs 34, 35, 36, 39 are input-only and don't have pullup-resistors.
    // So if connecting a button to these, make sure to add a 10k-pullup-resistor for each button.
    // Further infos: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

    // (optional) Neopixel
    #define LED_PIN                         22          // GPIO for Neopixel-signaling

    // (optinal) Headphone-detection
    #ifdef CONFIG_HEADPHONE_ADJUST
        //#define DETECT_HP_ON_HIGH                       // Per default headphones are supposed to be connected if HT_DETECT is LOW. DETECT_HP_ON_HIGH will change this behaviour to HIGH.
        #define HP_DETECT                   27          // GPIO that detects, if there's a plug in the headphone jack or not
    #endif

    // (optional) Monitoring of battery-voltage via ADC
    #ifdef CONFIG_MEASURE_BATTERY_VOLTAGE
        #define VOLTAGE_READ_PIN            33          // GPIO used to monitor battery-voltage. Change to 35 if you're using Lolin D32 or Lolin D32 pro as it's hard-wired there!
        constexpr float referenceVoltage = 3.35;                  // Voltage between 3.3V and GND-pin at the develboard in battery-mode (disconnect USB!)
        constexpr float offsetVoltage = 0.1;                      // If voltage measured by ESP isn't 100% accurate, you can add an correction-value here
    #endif

    // (optional) For measuring battery-voltage a voltage-divider is necessary. Their values need to be configured here.
    #ifdef CONFIG_MEASURE_BATTERY_VOLTAGE
        constexpr uint16_t rdiv1 = 129;                              // Rdiv1 of voltage-divider (kOhms) (measure exact value with multimeter!)
        constexpr uint16_t rdiv2 = 129;                              // Rdiv2 of voltage-divider (kOhms) (measure exact value with multimeter!) => used to measure voltage via ADC!
    #endif

    // (Optional) remote control via infrared
    #ifdef CONFIG_IR_CONTROL
        #define IRLED_PIN                   22              // GPIO where IR-receiver is connected (only tested with VS1838B)
        #define IR_DEBOUNCE                 200             // Interval in ms to wait at least for next signal (not used for actions volume up/down)

        // Actions available. Use your own remote control and have a look at the console for "Command=0x??". E.g. "Protocol=NEC Address=0x17F Command=0x68 Repeat gap=39750us"
        // Make sure to define a hex-code not more than once as this will lead to a compile-error
        // https://forum.espuino.de/t/neues-feature-fernsteuerung-per-infrarot-fernbedienung/265
        #define RC_PLAY                     0x68            // command for play
        #define RC_PAUSE                    0x67            // command for pause
        #define RC_NEXT                     0x6b            // command for next track of playlist
        #define RC_PREVIOUS                 0x6a            // command for previous track of playlist
        #define RC_FIRST                    0x6c            // command for first track of playlist
        #define RC_LAST                     0x6d            // command for last track of playlist
        #define RC_VOL_UP                   0x1a            // Command for volume up (one step)
        #define RC_VOL_DOWN                 0x1b            // Command for volume down (one step)
        #define RC_MUTE                     0x1c            // Command to mute ESPuino
        #define RC_SHUTDOWN                 0x2a            // Command for deepsleep
        #define RC_BLUETOOTH                0x72            // Command to enable/disable bluetooth
        #define RC_FTP                      0x65            // Command to enable FTP-server
    #endif
#endif
