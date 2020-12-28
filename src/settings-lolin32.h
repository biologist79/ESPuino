#include "Arduino.h"

//######################### INFOS ####################################
/* This is a develboard-specific config-file for *Wemos Lolin32*. Specific doesn't mean it's only working with this board.
   Lolin32 is the predecessor of Lolin D32.
   PCB: https://github.com/biologist79/Tonuino-ESP32-I2S/tree/master/PCBs/Wemos%20Lolin32
   Infos: https://arduino-projekte.info/wemos-lolin32/
   Caveats: None
   Status:
    tested with 2x SPI: RC522 & SD (by biologist79)
    tested with 1x SPI: PN5180, SD (MMC) (by tueddy and biologist79) => don't enable SINGLE_SPI_ENABLE
*/

//################## GPIO-configuration ##############################
#ifdef SD_MMC_1BIT_MODE
    // uSD-card-reader (via SD-MMC 1Bit)
    //
    // SD_MMC uses fixed pins
    //  MOSI    15
    //  SCK     14
    //  MISO    2
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
#define RFID_CS                         21          // GPIO for chip select (RFID)
#define RFID_MOSI                       23          // GPIO for master out slave in (RFID)
#define RFID_MISO                       19          // GPIO for master in slave out (RFID)
#define RFID_SCK                        18          // GPIO for clock-signal (RFID)

#ifdef RFID_READER_TYPE_PN5180
    #define RFID_BUSY                   16          // PN5180 BUSY PIN
    #define RFID_RST                    22          // PN5180 RESET PIN
#endif
// I2S (DAC)
#define I2S_DOUT                        25          // Digital out (I2S)
#define I2S_BCLK                        27          // BCLK (I2S)
#define I2S_LRC                         26          // LRC (I2S)

// Rotary encoder
#define DREHENCODER_CLK                 34          // If you want to reverse encoder's direction, just switch GPIOs of CLK with DT (in software or hardware)
#define DREHENCODER_DT                  35          // Info: Lolin D32 / Lolin D32 pro 35 are using 35 for battery-voltage-monitoring!
#define DREHENCODER_BUTTON              32          // Button is used to switch Tonuino on and off

// Control-buttons
#define PAUSEPLAY_BUTTON                5           // GPIO to detect pause/play
#define NEXT_BUTTON                     4           // GPIO to detect next
#define PREVIOUS_BUTTON                 2           // GPIO to detect previous (Important: as of 19.11.2020 changed from 33 to 2; make sure to change in SD-MMC-mode)

// (optional) Power-control
#define POWER                           17          // GPIO used to drive transistor-circuit, that switches off peripheral devices while ESP32-deepsleep

// (optional) Neopixel
#define LED_PIN                         12          // GPIO for Neopixel-signaling

// (optinal) Headphone-detection
#ifdef HEADPHONE_ADJUST_ENABLE
    #define HP_DETECT                   22          // GPIO that detects, if there's a plug in the headphone jack or not
#endif

// (optional) Monitoring of battery-voltage via ADC
#ifdef MEASURE_BATTERY_VOLTAGE
    #define VOLTAGE_READ_PIN            33          // GPIO used to monitor battery-voltage. Change to 35 if you're using Lolin D32 or Lolin D32 pro as it's hard-wired there!
#endif

// (optional) For measuring battery-voltage a voltage-divider is necessary. Their values need to be configured here.
#ifdef MEASURE_BATTERY_VOLTAGE
    uint8_t rdiv1 = 129;                               // Rdiv1 of voltage-divider (kOhms) (measure exact value with multimeter!)
    uint16_t rdiv2 = 389;                              // Rdiv2 of voltage-divider (kOhms) (measure exact value with multimeter!) => used to measure voltage via ADC!
#endif