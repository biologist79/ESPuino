#ifndef __ESPUINO_SETTINGS_LOLIN32_H__
#define __ESPUINO_SETTINGS_LOLIN32_H__
#include "Arduino.h"

//######################### INFOS ####################################
/* This is a develboard-specific config-file for *Wemos Lolin32*. Specific doesn't mean it's only working with this board.
   Lolin32 is the predecessor of Lolin D32.
   PCB: https://github.com/biologist79/ESPuino/tree/master/PCBs/Wemos%20Lolin32
   PCB: https://forum.espuino.de/t/lolin32-mit-sd-sd-mmc-und-pn5180-als-rfid-leser/77
   Infos: https://arduino-projekte.info/wemos-lolin32/
   Caveats: None
   Status:
    tested with 2x SPI: RC522 & SD (by biologist79)
    tested with 1x SPI: PN5180, SD (MMC) (by tueddy and biologist79) => don't enable CONFIG_SINGLE_SPI
*/

//################## GPIO-configuration ##############################
// Please note: GPIOs 34, 35, 36, 39 are input-only and don't have pullup-resistors.
// So if connecting a button to these, make sure to add a 10k-pullup-resistor for each button.
// Further infos: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

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
