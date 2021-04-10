#ifndef __ESPUINO_SETTINGS_H__
#define __ESPUINO_SETTINGS_H__
    #include "Arduino.h"
    #include "values.h"

    //######################### INFOS ####################################
    // This is the general configfile for ESPuino-configuration.

    //################## HARDWARE-PLATFORM ###############################
    /* Make sure to also edit the configfile, that is specific for your platform.
    If in doubts (your develboard is not listed) use HAL 1
    1: Wemos Lolin32             => settings-lolin32.h
    2: ESP32-A1S Audiokit        => settings-espa1s.h
    3: Wemos Lolin D32           => settings-lolin_D32.h
    4: Wemos Lolin D32 pro       => settings-lolin_D32_pro.h
    5: Lilygo T8 (V1.7)          => settings-ttgo_t8.h
    99: custom                   => settings-custom.h
    more to come...
    */
    #ifndef HAL             // Will be set by platformio.ini. If using Arduini-IDE you have to set HAL according your needs!
        #define HAL 1       // HAL 1 = LoLin32, 2 = ESP32-A1S-AudioKit, 3 = Lolin D32, 4 = Lolin D32 pro; 99 = custom
    #endif


    //########################## MODULES #################################
    //#define PORT_EXPANDER_ENABLE          // When enabled, buttons can be connected via port-expander PCA9555
    #define MDNS_ENABLE                     // When enabled, you don't have to handle with ESPuino's IP-address. If hostname is set to "ESPuino", you can reach it via ESPuino.local
    #define MQTT_ENABLE                     // Make sure to configure mqtt-server and (optionally) username+pwd
    #define FTP_ENABLE                      // Enables FTP-server; DON'T FORGET TO ACTIVATE AFTER BOOT BY PRESSING PAUSE + NEXT-BUTTONS (IN PARALLEL)!
    #define NEOPIXEL_ENABLE                 // Don't forget configuration of NUM_LEDS if enabled
    //#define NEOPIXEL_REVERSE_ROTATION     // Some Neopixels are adressed/soldered counter-clockwise. This can be configured here.
    #define LANGUAGE 1                      // 1 = deutsch; 2 = english
    //#define STATIC_IP_ENABLE              // Enables static IP-configuration (change static ip-section accordingly)
    //#define HEADPHONE_ADJUST_ENABLE       // Used to adjust (lower) volume for optional headphone-pcb (refer maxVolumeSpeaker / maxVolumeHeadphone) and to enable stereo (if PLAY_MONO_SPEAKER is set)
    #define PLAY_MONO_SPEAKER               // If only one speaker is used enabling mono should make sense. Please note: headphones is always stereo (if HEADPHONE_ADJUST_ENABLE is active)
    #define SHUTDOWN_IF_SD_BOOT_FAILS       // Will put ESP to deepsleep if boot fails due to SD. Really recommend this if there's in battery-mode no other way to restart ESP! Interval adjustable via deepsleepTimeAfterBootFails.
    #define MEASURE_BATTERY_VOLTAGE         // Enables battery-measurement via GPIO (ADC) and voltage-divider
    //#define PLAY_LAST_RFID_AFTER_REBOOT   // When restarting ESPuino, the last RFID that was active before, is recalled and played
    //#define USE_LAST_VOLUME_AFTER_REBOOT  // Remembers the volume used at last shutdown after reboot
    #define USEROTARY_ENABLE                // If rotary-encoder is used (don't forget to review WAKEUP_BUTTON if you disable this feature!)
    #define BLUETOOTH_ENABLE                // If enabled and bluetooth-mode is active, you can stream to your ESPuino via bluetooth (a2dp-sink).
    //#define IR_CONTROL_ENABLE             // Enables remote control


    //################## select SD card mode #############################
    //#define SD_MMC_1BIT_MODE              // run SD card in SD-MMC 1Bit mode
    //#define SINGLE_SPI_ENABLE             // If only one SPI-instance should be used instead of two (not yet working!)


    //################## select RFID reader ##############################
    #define RFID_READER_TYPE_MFRC522_SPI    // use MFRC522 via SPI
    //#define RFID_READER_TYPE_MFRC522_I2C  // use MFRC522 via I2C
    //#define RFID_READER_TYPE_PN5180       // use PN5180
    //#define RFID_READER_TYPE_PN532_SPI        // use PN532 via SPI
    //#define RFID_READER_TYPE_PN532_I2C        // use PN532 via I2C
    //#define RFID_READER_TYPE_PN532_UART       // use PN532 via UART

    #ifdef RFID_READER_TYPE_MFRC522_I2C
        #define MFRC522_ADDR 0x28           // default I2C-address of MFRC522
    #endif

    #ifdef RFID_READER_TYPE_PN5180
        //#define PN5180_ENABLE_LPCD        // Wakes up ESPuino if RFID-tag was applied while deepsleep is active.
    #endif

    #if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(RFID_READER_TYPE_MFRC522_SPI)
        uint8_t rfidGain = 0x07 << 4;      // Sensitivity of RC522. For possible values see reference: https://forum.espuino.de/uploads/default/original/1X/9de5f8d35cbc123c1378cad1beceb3f51035cec0.png
    #endif


    //############# Port-expander-configuration ######################
    #ifdef PORT_EXPANDER_ENABLE
        const uint8_t portsToRead = 2;      // PCA9555 has two ports à 8 channels. If 8 channels are sufficient, set to 1 and only use the first port!
        uint8_t expanderI2cAddress = 0x20;  // I2C-address of PCA9555
    #endif

    //################## BUTTON-Layout ##################################
    /* German documentation: https://forum.espuino.de/t/das-dynamische-button-layout/224
    Please note the following numbers as you need to know them in order to define actions for buttons.
    Even if you don't use all of them, their numbers won't change
        0: NEXT_BUTTON
        1: PREVIOUS_BUTTON
        2: PAUSEPLAY_BUTTON
        3: DREHENCODER_BUTTON
        4: BUTTON_4
        5: BUTTON_5

    Don't forget to enable/configure those buttons you want to use in your develboard-specific config (e.g. settings-custom.h)

    Single-buttons [can be long or short] (examples):
        BUTTON_0_SHORT => Button 0 (NEXT_BUTTON) pressed shortly
        BUTTON_3_SHORT => Button 3 (DREHENCODER_BUTTON) pressed shortly
        BUTTON_4_LONG => Button 4 (BUTTON_4) pressed long

    Multi-buttons [short only] (examples):
        BUTTON_MULTI_01 => Buttons 0+1 (NEXT_BUTTON + PREVIOUS_BUTTON) pressed in parallel
        BUTTON_MULTI_23 => Buttons 0+2 (NEXT_BUTTON + PAUSEPLAY_BUTTON) pressed in parallel

    Actions:
        To all of those buttons, an action can be assigned freely.
        Please have a look at values.h to look up actions available (>=100 can be used)
        If you don't want to assign an action or you don't use a given button: CMD_NOTHING has to be set
    */
    // *****BUTTON*****        *****ACTION*****
    #define BUTTON_0_SHORT    CMD_NEXTTRACK
    #define BUTTON_1_SHORT    CMD_PREVTRACK
    #define BUTTON_2_SHORT    CMD_PLAYPAUSE
    #define BUTTON_3_SHORT    CMD_MEASUREBATTERY
    #define BUTTON_4_SHORT    CMD_NOTHING
    #define BUTTON_5_SHORT    CMD_NOTHING

    #define BUTTON_0_LONG     CMD_LASTTRACK
    #define BUTTON_1_LONG     CMD_FIRSTTRACK
    #define BUTTON_2_LONG     CMD_PLAYPAUSE
    #define BUTTON_3_LONG     CMD_SLEEPMODE
    #define BUTTON_4_LONG     CMD_NOTHING
    #define BUTTON_5_LONG     CMD_NOTHING

    #define BUTTON_MULTI_01   TOGGLE_WIFI_STATUS
    #define BUTTON_MULTI_02   ENABLE_FTP_SERVER
    #define BUTTON_MULTI_03   CMD_NOTHING
    #define BUTTON_MULTI_04   CMD_NOTHING
    #define BUTTON_MULTI_05   CMD_NOTHING
    #define BUTTON_MULTI_12   CMD_NOTHING
    #define BUTTON_MULTI_13   CMD_NOTHING
    #define BUTTON_MULTI_14   CMD_NOTHING
    #define BUTTON_MULTI_15   CMD_NOTHING
    #define BUTTON_MULTI_23   CMD_NOTHING
    #define BUTTON_MULTI_24   CMD_NOTHING
    #define BUTTON_MULTI_25   CMD_NOTHING
    #define BUTTON_MULTI_34   CMD_NOTHING
    #define BUTTON_MULTI_35   CMD_NOTHING
    #define BUTTON_MULTI_45   CMD_NOTHING

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
    #define RFID_SCAN_INTERVAL 100                      // Interval-time in ms (how often is RFID read?)

    // Automatic restart
    #ifdef SHUTDOWN_IF_SD_BOOT_FAILS
        uint32_t deepsleepTimeAfterBootFails = 20;      // Automatic restart takes place if boot was not successful after this period (in seconds)
    #endif

    // FTP
    // Nothing to be configured here...
    // Default user/password is esp32/esp32 but can be changed via webgui

    // ESPuino will create a WiFi if joing existing WiFi was not possible. Name can be configured here.
    static const char accessPointNetworkSSID[] PROGMEM = "ESPuino";     // Access-point's SSID
    static const char nameBluetoothDevice[] PROGMEM = "ESPuino";        // Name of your ESPuino as Bluetooth-device

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

    // Seekmode-configuration
    uint8_t jumpOffset = 30;                            // Offset in seconds to jump for commands CMD_SEEK_FORWARDS / CMD_SEEK_BACKWARDS

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
#endif