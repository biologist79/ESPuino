#include "Arduino.h"

//########################## MODULES #################################
#define MDNS_ENABLE                 // When enabled, you don't have to handle with Tonuino's IP-address. If hostname is set to "tonuino", you can reach it via tonuino.local
#define MQTT_ENABLE                 // Make sure to configure mqtt-server and (optionally) username+pwd
#define FTP_ENABLE                  // Enables FTP-server
#define NEOPIXEL_ENABLE             // Don't forget configuration of NUM_LEDS if enabled
#define NEOPIXEL_REVERSE_ROTATION   // Some Neopixels are adressed/soldered counter-clockwise. This can be configured here.
#define LANGUAGE 1                  // 1 = deutsch; 2 = english
//#define HEADPHONE_ADJUST_ENABLE     // Used to adjust (lower) volume for optional headphone-pcb (refer maxVolumeSpeaker / maxVolumeHeadphone)
#define SHUTDOWN_IF_SD_BOOT_FAILS   // Will put ESP to deepsleep if boot fails due to SD. Really recommend this if there's in battery-mode no other way to restart ESP! Interval adjustable via deepsleepTimeAfterBootFails.
#define MEASURE_BATTERY_VOLTAGE     // Enables battery-measurement via GPIO (ADC) and voltage-divider
//#define PLAY_LAST_RFID_AFTER_REBOOT // When restarting Tonuino, the last RFID that was active before, is recalled and played

//#define BLUETOOTH_ENABLE          // Doesn't work currently (so don't enable) as there's not enough DRAM available

//################## select SD card mode #############################
//#define SD_MMC_1BIT_MODE            // run SD card in SD-MMC 1Bit mode 
//#define SINGLE_SPI_ENABLE         // If only one SPI-instance should be used instead of two (not yet working!)

//################## select RFID reader ##############################
#define RFID_READER_TYPE_MFRC522        // use MFRC522
//#define RFID_READER_TYPE_PN5180         


//################## GPIO-configuration ##############################
#ifdef SD_MMC_1BIT_MODE
    // uSD-card-reader (via SD-MMC 1Bit)
    //
    // SD_MMC uses fixed pins
    //  MOSI    15
    //  SCKK    14
    //  MISO    2   // hardware pullup may required
#else
    // uSD-card-reader (via SPI)
    #define SPISD_CS                        15          // GPIO for chip select (SD)
    #ifndef SINGLE_SPI_ENABLE
        #define SPISD_MOSI                  13          // GPIO for master out slave in (SD) => not necessary for single-SPI
        #define SPISD_MISO                  16          // GPIO for master in slave ou (SD) => not necessary for single-SPI
        #define SPISD_SCK                   14          // GPIO for clock-signal (SD) => not necessary for single-SPI
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
#define PREVIOUS_BUTTON                 2           // GPIO to detect previous (Important: as of 19.11.2020 changed from 33 to 2)

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



//#################### Various settings ##############################
// Loglevels available (don't change!)
#define LOGLEVEL_ERROR                  1           // only errors
#define LOGLEVEL_NOTICE                 2           // errors + important messages
#define LOGLEVEL_INFO                   3           // infos + errors + important messages
#define LOGLEVEL_DEBUG                  4           // almost everything

// Serial-logging-configuration
const uint8_t serialDebug = LOGLEVEL_DEBUG;          // Current loglevel for serial console

// Buttons (better leave unchanged if in doubts :-))
uint8_t buttonDebounceInterval = 50;                // Interval in ms to software-debounce buttons
uint16_t intervalToLongPress = 700;                 // Interval in ms to distinguish between short and long press of previous/next-button

// RFID
#define RFID_SCAN_INTERVAL 300                      // Interval-time in ms (how often is RFID read?)

// Automatic restart
#ifdef SHUTDOWN_IF_SD_BOOT_FAILS
    uint32_t deepsleepTimeAfterBootFails = 20;      // Automatic restart takes place if boot was not successful after this period (in seconds)
#endif

// FTP
// Nothing to be configured here...
// Default user/password is esp32/esp32 but can be changed via webgui

// Tonuino will create a WiFi if joing existing WiFi was not possible. Name can be configured here.
static const char accessPointNetworkSSID[] PROGMEM = "Tonuino";     // Access-point's SSID

// Where to store the backup-file for NVS-records
static const char backupFile[] PROGMEM = "/backup.txt"; // File is written every time a (new) RFID-assignment via GUI is done

// (webgui) File Browser
uint8_t FS_DEPTH = 5;                               // Max. recursion-depth of file tree
const char *DIRECTORY_INDEX_FILE = "/files.json";   // Filename of files.json index file

// (optinal) Neopixel
#ifdef NEOPIXEL_ENABLE
    #define NUM_LEDS                    24          // number of LEDs
    #define CHIPSET                     WS2812B     // type of Neopixel
    #define COLOR_ORDER                 GRB
#endif

// (optional) Default-voltages for battery-monitoring via Neopixel
float warningLowVoltage = 3.4;                      // If battery-voltage is >= this value, a cyclic warning will be indicated by Neopixel (can be changed via GUI!)
uint8_t voltageCheckInterval = 10;                  // How of battery-voltage is measured (in minutes) (can be changed via GUI!)
float voltageIndicatorLow = 3.0;                    // Lower range for Neopixel-voltage-indication (0 leds) (can be changed via GUI!)
float voltageIndicatorHigh = 4.2;                   // Upper range for Neopixel-voltage-indication (all leds) (can be changed via GUI!)

// (optinal) For measuring battery-voltage a voltage-divider is necessary. Their values need to be configured here.
#ifdef MEASURE_BATTERY_VOLTAGE
    uint8_t rdiv1 = 129;                               // Rdiv1 of voltage-divider (kOhms) (measure exact value with multimeter!)
    uint16_t rdiv2 = 389;                              // Rdiv2 of voltage-divider (kOhms) (measure exact value with multimeter!) => used to measure voltage via ADC!
#endif

// (optinal) Headphone-detection (leave unchanged if in doubts...)
#ifdef HEADPHONE_ADJUST_ENABLE
    uint16_t headphoneLastDetectionDebounce = 1000; // Debounce-interval in ms when plugging in headphone
#endif

// (optional) Topics for MQTT
#ifdef MQTT_ENABLE
    uint16_t mqttRetryInterval = 15;                // Try to reconnect to MQTT-server every (n) seconds if connection is broken
    uint8_t mqttMaxRetriesPerInterval = 1;          // Number of retries per time-interval (mqttRetryInterval). mqttRetryInterval 15 / mqttMaxRetriesPerInterval 1 => once every 15s
    #define DEVICE_HOSTNAME "ESP32-Tonuino"                 // Name that that is used for MQTT
    static const char topicSleepCmnd[] PROGMEM = "Cmnd/Tonuino/Sleep";
    static const char topicSleepState[] PROGMEM = "State/Tonuino/Sleep";
    static const char topicTrackCmnd[] PROGMEM = "Cmnd/Tonuino/Track";
    static const char topicTrackState[] PROGMEM = "State/Tonuino/Track";
    static const char topicTrackControlCmnd[] PROGMEM = "Cmnd/Tonuino/TrackControl";
    static const char topicLoudnessCmnd[] PROGMEM = "Cmnd/Tonuino/Loudness";
    static const char topicLoudnessState[] PROGMEM = "State/Tonuino/Loudness";
    static const char topicSleepTimerCmnd[] PROGMEM = "Cmnd/Tonuino/SleepTimer";
    static const char topicSleepTimerState[] PROGMEM = "State/Tonuino/SleepTimer";
    static const char topicState[] PROGMEM = "State/Tonuino/State";
    static const char topicCurrentIPv4IP[] PROGMEM = "State/Tonuino/IPv4";
    static const char topicLockControlsCmnd[] PROGMEM ="Cmnd/Tonuino/LockControls";
    static const char topicLockControlsState[] PROGMEM ="State/Tonuino/LockControls";
    static const char topicPlaymodeState[] PROGMEM = "State/Tonuino/Playmode";
    static const char topicRepeatModeCmnd[] PROGMEM = "Cmnd/Tonuino/RepeatMode";
    static const char topicRepeatModeState[] PROGMEM = "State/Tonuino/RepeatMode";
    static const char topicLedBrightnessCmnd[] PROGMEM = "Cmnd/Tonuino/LedBrightness";
    static const char topicLedBrightnessState[] PROGMEM = "State/Tonuino/LedBrightness";
    #ifdef MEASURE_BATTERY_VOLTAGE
        static const char topicBatteryVoltage[] PROGMEM = "State/Tonuino/Voltage";
    #endif
#endif
