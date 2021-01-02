#include "Arduino.h"

//######################### INFOS ####################################
/* This is a develboard-specific config-file for *Wemos Lolin32*. Specific doesn't mean it's only working with this board.
   Lolin32 is the predecessor of Lolin D32.
   PCB: None so far
   Infos: https://www.wemos.cc/en/latest/d32/d32_pro.html
   Schematics: https://www.wemos.cc/en/latest/_static/files/sch_d32_pro_v2.0.0.pdf
   Caveats: GPIO35 (battery monitoring) + SD can't be changed, it's built in (and because of the SD-pinout used, it is not compatible with MMC-mode)
   Status:
    tested with 2xSPI: RC522 & SD (by biologist79)
*/

//################## GPIO-configuration ##############################
#ifdef SD_MMC_1BIT_MODE
    // NOT SUPPORTED BY D32 pro as 15 / 14 / 2 doesn't match D32 pro's SD-pinout
#else
    // uSD-card-reader (via SPI) => Cannot be changed, it's built in!
    #define SPISD_CS                     4          // GPIO for chip select (SD)
    #ifndef SINGLE_SPI_ENABLE
        #define SPISD_MOSI              23          // GPIO for master out slave in (SD) => not necessary for single-SPI
        #define SPISD_MISO              19          // GPIO for master in slave ou (SD) => not necessary for single-SPI
        #define SPISD_SCK               18          // GPIO for clock-signal (SD) => not necessary for single-SPI
    #endif
#endif

// RFID (via SPI)
#define RST_PIN                         99          // Not necessary but has to be set anyway; so let's use a dummy-number
#define RFID_CS                         21          // GPIO for chip select (RFID)
#define RFID_MOSI                       13          // GPIO for master out slave in (RFID)
#define RFID_MISO                       15          // GPIO for master in slave out (RFID)
#define RFID_SCK                        14          // GPIO for clock-signal (RFID)

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
#define DREHENCODER_DT                  39          // 39 = 'VN'
#define DREHENCODER_BUTTON              32          // Button is used to switch Tonuino on and off

// Control-buttons
#define PAUSEPLAY_BUTTON                36          // GPIO to detect pause/play; 36 = 'VP'
#define NEXT_BUTTON                     33          // GPIO to detect next
#define PREVIOUS_BUTTON                 2           // GPIO to detect previous (Important: as of 19.11.2020 changed from 33 to 2)

// (optional) Power-control
#define POWER                           5           // GPIO used to drive transistor-circuit, that switches off peripheral devices while ESP32-deepsleep

// (optional) Neopixel
#define LED_PIN                         12          // GPIO for Neopixel-signaling

// (optinal) Headphone-detection
#ifdef HEADPHONE_ADJUST_ENABLE
    #define HP_DETECT                   22          // GPIO that detects, if there's a plug in the headphone jack or not
#endif

// (optional) Monitoring of battery-voltage via ADC
#ifdef MEASURE_BATTERY_VOLTAGE
    #define VOLTAGE_READ_PIN            35          // GPIO used to monitor battery-voltage. Cannot be changed, it's built in
#endif

// (optional) For measuring battery-voltage a voltage-divider is already onboard. Connect a LiPo and use it!
#ifdef MEASURE_BATTERY_VOLTAGE
    uint8_t rdiv1 = 100;                            // Cannot be changed, it's built in
    uint16_t rdiv2 = 100;                           // Cannot be changed, it's built in
#endif