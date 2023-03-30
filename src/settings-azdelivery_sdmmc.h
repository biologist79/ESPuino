#ifndef __ESPUINO_SETTINGS_CUSTOM_H__
#define __ESPUINO_SETTINGS_CUSTOM_H__
    #include "Arduino.h"

    //######################### INFOS ####################################
    /* This is a PCB-specific config-file for *AZ Delivery ESP32 NodeMCU/Devkit C with SD_MMC and PN5180*.
    PCB: https://github.com/biologist79/ESPuino/blob/master/PCBs/AZDelivery_ESP32_NodeMCU/gerber/gerber_rev2.zip
    Forum: https://forum.espuino.de/t/az-delivery-esp32-nodemcu-devkit-c-mit-sd-mmc-und-pn5180-als-rfid-leser/634
    Infos: https://www.amazon.de/AZDelivery-NodeMCU-Development-Nachfolgermodell-ESP8266/dp/B074RGW2VQ
    Schematics PCB: https://github.com/biologist79/ESPuino/blob/master/PCBs/AZDelivery_ESP32_NodeMCU/pictures/Schematics_rev2.pdf
    Caveats: Battery-monitoring is not available and SD ist SD_MMC only
    Status:
        tested with PN5180 + SD_MMC (by biologist79)
    */

    //################## GPIO-configuration ##############################
    // Please note: GPIOs 34, 35, 36, 39 are input-only and don't have pullup-resistors.
    // So if connecting a button to these, make sure to add a 10k-pullup-resistor for each button.
    // Further infos: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

    // Channels of port-expander can be read cyclic or interrupt-driven. It's strongly recommended to use the interrupt-way!
    // Infos: https://forum.espuino.de/t/einsatz-des-port-expanders-pca9555/306
    #ifdef CONFIG_PORT_EXPANDER
        // Not supported
    #endif

    // I2C-configuration (necessary for RC522 [only via i2c - not spi!] or port-expander)
    #if defined(CONFIG_RFID_READER_MFRC522_I2C) || defined(CONFIG_PORT_EXPANDER)
        // Not supported
    #endif

    // (optinal) Headphone-detection
    #ifdef CONFIG_HEADPHONE_ADJUST
        //#define DETECT_HP_ON_HIGH                       // Per default headphones are supposed to be connected if HT_DETECT is LOW. DETECT_HP_ON_HIGH will change this behaviour to HIGH.
        #define HP_DETECT                    4          // GPIO that detects, if there's a plug in the headphone jack or not
    #endif

    // (optional) Monitoring of battery-voltage via ADC
    #ifdef CONFIG_MEASURE_BATTERY_VOLTAGE
        #define VOLTAGE_READ_PIN            99          // GPIO used to monitor battery-voltage.
        constexpr float referenceVoltage = 3.3;         // Voltage between 3.3V and GND-pin in battery-mode (disconnect USB!)
        constexpr float offsetVoltage = 0.0;            // If voltage measured by ESP isn't 100% accurate, you can add an correction-value here
    #endif

    // (Optional) remote control via infrared
    #ifdef CONFIG_IR_CONTROL
        #define IRLED_PIN                   33              // GPIO where IR-receiver is connected (only tested with VS1838B)
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
