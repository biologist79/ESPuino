#include "Arduino.h"

//######################### INFOS ####################################
// This is the general configfile for ESPuino-configuration.

//################## HARDWARE-PLATFORM ###############################
/* Make sure to also edit the configfile, that is specific for your platform.
   If in doubts (your develboard is not listed) use HAL 1
   1: Wemos Lolin32             => settings-lolin32.h
   2: ESP32-A1S Audiokit        => settings-espa1s.h
   3: Wemos Lolin D32           => settings-lolin_D32.h
   4: Wemos Lolin D32 pro       => settings-lolin_D32_pro.h
   more to come...
*/
 #define HAL 1                // HAL 1 = LoLin32, 2 = ESP32-A1S-AudioKit, 3 = Lolin D32, 4 = Lolin D32 pro


//########################## MODULES #################################
#define MDNS_ENABLE                     // When enabled, you don't have to handle with ESPuino's IP-address. If hostname is set to "ESPuino", you can reach it via ESPuino.local
#define MQTT_ENABLE                     // Make sure to configure mqtt-server and (optionally) username+pwd
#define FTP_ENABLE                      // Enables FTP-server; DON'T FORGET TO ACTIVATE AFTER BOOT BY PRESSING PAUSE + NEXT-BUTTONS (IN PARALLEL)!
#define NEOPIXEL_ENABLE                 // Don't forget configuration of NUM_LEDS if enabled
//#define NEOPIXEL_REVERSE_ROTATION     // Some Neopixels are adressed/soldered counter-clockwise. This can be configured here.
#define LANGUAGE 1                      // 1 = deutsch; 2 = english
//#define STATIC_IP_ENABLE              // Enables static IP-configuration (change static ip-section accordingly)
//#define HEADPHONE_ADJUST_ENABLE       // Used to adjust (lower) volume for optional headphone-pcb (refer maxVolumeSpeaker / maxVolumeHeadphone)
#define SHUTDOWN_IF_SD_BOOT_FAILS       // Will put ESP to deepsleep if boot fails due to SD. Really recommend this if there's in battery-mode no other way to restart ESP! Interval adjustable via deepsleepTimeAfterBootFails.
#define MEASURE_BATTERY_VOLTAGE         // Enables battery-measurement via GPIO (ADC) and voltage-divider
//#define PLAY_LAST_RFID_AFTER_REBOOT   // When restarting ESPuino, the last RFID that was active before, is recalled and played
//#define USE_LAST_VOLUME_AFTER_REBOOT  // Remembers the volume used at last shutdown after reboot

#define BLUETOOTH_ENABLE          // Doesn't work currently (so don't enable) as there's not enough DRAM available


//################## select SD card mode #############################
//#define SD_MMC_1BIT_MODE            // run SD card in SD-MMC 1Bit mode
//#define SINGLE_SPI_ENABLE         // If only one SPI-instance should be used instead of two (not yet working!) (Works on ESP32-A1S with RFID via I2C)


//################## select RFID reader ##############################
#define RFID_READER_TYPE_MFRC522_SPI        // use MFRC522 via SPI
//#define RFID_READER_TYPE_MFRC522_I2C        // use MFRC522 via I2C
//#define RFID_READER_TYPE_PN5180			  // use PN5180
//#define PN5180_ENABLE_LPCD                    // enable PN5180 low power card detection: wake up on card detection


//#################### Various settings ##############################
// Loglevels available (don't change!)
#define LOGLEVEL_ERROR                  1           // only errors
#define LOGLEVEL_NOTICE                 2           // errors + important messages
#define LOGLEVEL_INFO                   3           // infos + errors + important messages
#define LOGLEVEL_DEBUG                  4           // almost everything

// Serial-logging-configuration
const uint8_t serialDebug = LOGLEVEL_DEBUG;          // Current loglevel for serial console

// Static ip-configuration
#ifdef STATIC_IP_ENABLE
    IPAddress local_IP(192, 168, 2, 100);           // ESPuino's IP
    IPAddress gateway(192, 168, 2, 1);              // IP of the gateway/router
    IPAddress subnet(255, 255, 255, 0);             // Netmask of your network (/24 => 255.255.255.0)
    IPAddress primaryDNS(192, 168, 2, 1);           // DNS-server of your network; in private networks it's usually the gatewy's IP
#endif

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

// ESPuino will create a WiFi if joing existing WiFi was not possible. Name can be configured here.
static const char accessPointNetworkSSID[] PROGMEM = "ESPuino";     // Access-point's SSID

// Where to store the backup-file for NVS-records
static const char backupFile[] PROGMEM = "/backup.txt"; // File is written every time a (new) RFID-assignment via GUI is done


//#################### Settings for optional Modules##############################
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

// (optinal) Headphone-detection (leave unchanged if in doubts...)
#ifdef HEADPHONE_ADJUST_ENABLE
    uint16_t headphoneLastDetectionDebounce = 1000; // Debounce-interval in ms when plugging in headphone
#endif

// (optional) Topics for MQTT
#ifdef MQTT_ENABLE
    uint16_t mqttRetryInterval = 60;                // Try to reconnect to MQTT-server every (n) seconds if connection is broken
    uint8_t mqttMaxRetriesPerInterval = 1;          // Number of retries per time-interval (mqttRetryInterval). mqttRetryInterval 60 / mqttMaxRetriesPerInterval 1 => once every 60s
    #define DEVICE_HOSTNAME "ESP32-ESPuino"         // Name that is used for MQTT
    static const char topicSleepCmnd[] PROGMEM = "Cmnd/ESPuino/Sleep";
    static const char topicSleepState[] PROGMEM = "State/ESPuino/Sleep";
    static const char topicRfidCmnd[] PROGMEM = "Cmnd/ESPuino/Rfid";
    static const char topicRfidState[] PROGMEM = "State/ESPuino/Rfid";
    static const char topicTrackState[] PROGMEM = "State/ESPuino/Track";
    static const char topicTrackControlCmnd[] PROGMEM = "Cmnd/ESPuino/TrackControl";
    static const char topicLoudnessCmnd[] PROGMEM = "Cmnd/ESPuino/Loudness";
    static const char topicLoudnessState[] PROGMEM = "State/ESPuino/Loudness";
    static const char topicSleepTimerCmnd[] PROGMEM = "Cmnd/ESPuino/SleepTimer";
    static const char topicSleepTimerState[] PROGMEM = "State/ESPuino/SleepTimer";
    static const char topicState[] PROGMEM = "State/ESPuino/State";
    static const char topicCurrentIPv4IP[] PROGMEM = "State/ESPuino/IPv4";
    static const char topicLockControlsCmnd[] PROGMEM ="Cmnd/ESPuino/LockControls";
    static const char topicLockControlsState[] PROGMEM ="State/ESPuino/LockControls";
    static const char topicPlaymodeState[] PROGMEM = "State/ESPuino/Playmode";
    static const char topicRepeatModeCmnd[] PROGMEM = "Cmnd/ESPuino/RepeatMode";
    static const char topicRepeatModeState[] PROGMEM = "State/ESPuino/RepeatMode";
    static const char topicLedBrightnessCmnd[] PROGMEM = "Cmnd/ESPuino/LedBrightness";
    static const char topicLedBrightnessState[] PROGMEM = "State/ESPuino/LedBrightness";
    #ifdef MEASURE_BATTERY_VOLTAGE
        static const char topicBatteryVoltage[] PROGMEM = "State/ESPuino/Voltage";
    #endif
#endif
