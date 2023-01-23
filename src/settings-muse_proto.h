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
    #ifdef SD_MMC_1BIT_MODE
        // uSD-card-reader (via SD-MMC 1Bit)
        //
        // SD_MMC uses fixed pins
        //  (MOSI)    15  CMD
        //  (SCK)     14  SCK
        //  (MISO)     2  D0
    #else
        // uSD-card-reader (via SPI)
        #define SPISD_CS                    15          // GPIO for chip select (SD)
        #ifndef SINGLE_SPI_ENABLE
            #define SPISD_MOSI              13          // GPIO for master out slave in (SD) => not necessary for single-SPI
            #define SPISD_MISO              16          // GPIO for master in slave ou (SD) => not necessary for single-SPI
            #define SPISD_SCK               14          // GPIO for clock-signal (SD) => not necessary for single-SPI
        #endif
    #endif

    // RFID (via SPI)
    #define RST_PIN                         99          // Not necessary but has to be set anyway; so let's use a dummy-number
    #define RFID_CS                         99          // GPIO for chip select (RFID)
    #define RFID_MOSI                       99          // GPIO for master out slave in (RFID)
    #define RFID_MISO                       99          // GPIO for master in slave out (RFID)
    #define RFID_SCK                        99          // GPIO for clock-signal (RFID)

    // RFID (via I2C)
    #if defined(RFID_READER_TYPE_MFRC522_I2C)
        #define MFRC522_RST_PIN                 4          // needed for initialisation -> MTDI on JTAG header
    #endif

    #ifdef RFID_READER_TYPE_PN5180
        #define RFID_BUSY                   16          // PN5180 BUSY PIN
        #define RFID_RST                    22          // PN5180 RESET PIN
        #define RFID_IRQ                    39          // PN5180 IRQ PIN (only needed for low power card detection)
    #endif

    // I2S (DAC)
    #define I2S_DOUT                        26          // Digital out (I2S)
    #define I2S_BCLK                        5          // BCLK (I2S)
    #define I2S_LRC                         25          // LRC (I2S)

    // Rotary encoder
    #ifdef USEROTARY_ENABLE
        //#define REVERSE_ROTARY                        // To reverse encoder's direction; switching CLK / DT in hardware does the same
        #define ROTARYENCODER_CLK           36          // SENSOR-VP
        #define ROTARYENCODER_DT            39          // SENSOR_VN
    #endif

    // Amp enable (optional)
    #define GPIO_PA_EN                      21         // To enable amp for loudspeaker (GPIO or port-channel)

    // Control-buttons (set to 99 to DISABLE; 0->39 for GPIO; 100->115 for port-expander)
    #define NEXT_BUTTON                      18         // Button 0: GPIO to detect next
    #define PREVIOUS_BUTTON                  19          // Button 1: GPIO to detect previous (Important: as of 19.11.2020 changed from 33 to 2; make sure to change in SD-MMC-mode)
    #define PAUSEPLAY_BUTTON                 0          // Button 2: GPIO to detect pause/play
    #define ROTARYENCODER_BUTTON            99          // (set to 99 to disable; 0->39 for GPIO; 100->115 for port-expander)
    #define BUTTON_4                        99          // Button 4: unnamed optional button
    #define BUTTON_5                        99          // Button 5: unnamed optional button

    // Channels of port-expander can be read cyclic or interrupt-driven. It's strongly recommended to use the interrupt-way!
    // Infos: https://forum.espuino.de/t/einsatz-des-port-expanders-pca9555/306
    #ifdef PORT_EXPANDER_ENABLE
        #define PE_INTERRUPT_PIN            99          // GPIO that is used to receive interrupts from port-expander
    #endif

    // I2C-configuration (necessary for RC522 or RC532 [only via i2c - not spi!] or port-expander)
    #ifdef I2C_2_ENABLE
        #define ext_IIC_CLK                12           // i2c-SCL (clock)
        #define ext_IIC_DATA               32           // i2c-SDA (data)
    #endif

    // Wake-up button => this also is the interrupt-pin if port-expander is enabled!
    // Please note: only RTC-GPIOs (0, 4, 12, 13, 14, 15, 25, 26, 27, 32, 33, 34, 35, 36, 39, 99) can be used! Set to 99 to DISABLE.
    // Please note #2: this button can be used as interrupt-pin for port-expander. If so, all pins connected to port-expander can wake up ESPuino.
    #define WAKEUP_BUTTON                   PAUSEPLAY_BUTTON // Defines the button that is used to wake up ESPuino from deepsleep.

    // (optional) Power-control
    #define POWER                           17          // GPIO used to drive transistor-circuit, that switches off peripheral devices while ESP32-deepsleep
    #ifdef POWER
        //#define INVERT_POWER                          // If enabled, use inverted logic for POWER circuit, that means peripherals are turned off by writing HIGH
    #endif

    // (optional) Neopixel
    #define LED_PIN                         22          // GPIO for Neopixel-signaling

    // (optinal) Headphone-detection
    #ifdef HEADPHONE_ADJUST_ENABLE
        //#define DETECT_HP_ON_HIGH                       // Per default headphones are supposed to be connected if HT_DETECT is LOW. DETECT_HP_ON_HIGH will change this behaviour to HIGH.
        #define HP_DETECT                   27          // GPIO that detects, if there's a plug in the headphone jack or not
    #endif

    // (optional) Monitoring of battery-voltage via ADC
    #ifdef MEASURE_BATTERY_VOLTAGE
        #define VOLTAGE_READ_PIN            33          // GPIO used to monitor battery-voltage. Change to 35 if you're using Lolin D32 or Lolin D32 pro as it's hard-wired there!
        constexpr float referenceVoltage = 3.35;                  // Voltage between 3.3V and GND-pin at the develboard in battery-mode (disconnect USB!)
        constexpr float offsetVoltage = 0.1;                      // If voltage measured by ESP isn't 100% accurate, you can add an correction-value here
    #endif

    // (optional) For measuring battery-voltage a voltage-divider is necessary. Their values need to be configured here.
    #ifdef MEASURE_BATTERY_VOLTAGE
        constexpr uint16_t rdiv1 = 129;                              // Rdiv1 of voltage-divider (kOhms) (measure exact value with multimeter!)
        constexpr uint16_t rdiv2 = 129;                              // Rdiv2 of voltage-divider (kOhms) (measure exact value with multimeter!) => used to measure voltage via ADC!
    #endif

    // (optional) hallsensor. Make sure the GPIO defined doesn't overlap with existing configuration. Please note: only user-support is provided for this feature.
    #ifdef HALLEFFECT_SENSOR_ENABLE
        #define HallEffectSensor_PIN        34  	// GPIO that is used for hallsensor (ADC); user-support: https://forum.espuino.de/t/magnetische-hockey-tags/1449/35
    #endif

    // (Optional) remote control via infrared
    #ifdef IR_CONTROL_ENABLE
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
