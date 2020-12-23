#include "Arduino.h"

//######################### INFOS ####################################
/* This is a develboard-specific config-file for *AI Tinker ESP32-A1S-AudioKit*. It's highly customized and almost certainly
   not suitable for a different develboards.
   Has a lot of stuff already onboard but needs some soldering rework as there are not all GPIOs exposed
   PCB: Not necessary.
   Infos: https://github.com/Ai-Thinker-Open/ESP32-A1S-AudioKit
   Status: untested
*/


//################## GPIO-configuration ##############################
// uSD-card-reader (via SPI)
#define SPISD_CS                        13          // GPIO for chip select (SD)
#ifndef SINGLE_SPI_ENABLE
    #define SPISD_MOSI                  15          // GPIO for master out slave in (SD) => not necessary for single-SPI
    #define SPISD_MISO                   2          // GPIO for master in slave ou (SD) => not necessary for single-SPI
    #define SPISD_SCK                   14          // GPIO for clock-signal (SD) => not necessary for single-SPI
#endif

#define MFRC522_RST_PIN                 12          // needed for i2c-comm  MTDI on JTAG
#define MFRC522_ADDR                    0x28        // default Address of MFRC522
#define ext_IIC_CLK                     23          // 14-pin-header
#define ext_IIC_DATA                    18          // 14-pin-header

// I2S (DAC)
#define I2S_DOUT                        25          // Digital out (I2S)
#define I2S_BCLK                        27          // BCLK (I2S)
#define I2S_LRC                         26          // LRC (I2S)

// I2C (AC101)
#define IIC_CLK                         32          // internal
#define IIC_DATA                        33          // internal

// Amp enable
#define GPIO_PA_EN                  GPIO_NUM_21          // internal
#define GPIO_SEL_PA_EN              GPIO_SEL_21

// Rotary encoder
#define DREHENCODER_CLK                  5          // If you want to reverse encoder's direction, just switch GPIOs of CLK with DT (in software or hardware)
#define DREHENCODER_DT                  18          // Info: Lolin D32 / Lolin D32 pro 35 are using 35 for battery-voltage-monitoring!
#define DREHENCODER_BUTTON               4          // Button is used to switch Tonuino on and off

// Control-buttons
#define PAUSEPLAY_BUTTON                36          // GPIO to detect pause/play
#define NEXT_BUTTON                    199          // GPIO to detect next
#define PREVIOUS_BUTTON                198          // GPIO to detect previous (Important: as of 19.11.2020 changed from 33 to 2)

// Power-control
#define POWER                           19          // GPIO used to drive transistor-circuit, that switches off peripheral devices while ESP32-deepsleep

// (optional) Neopixel
#define LED_PIN                         23          // GPIO for Neopixel-signaling

// (optinal) Headphone-detection
#ifdef HEADPHONE_ADJUST_ENABLE
    #define HP_DETECT                   39          // GPIO that detects, if there's a plug in the headphone jack or not
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