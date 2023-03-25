#ifndef __ESPUINO_SETTINGS_H__
    	#define __ESPUINO_SETTINGS_H__
        #include "Arduino.h"
        #include "values.h"
	#if __has_include("../config.h")
		#include "../config.h"
	#endif
#if __has_include("settings-override.h")
    	#include "settings-override.h"
#else
	//#################### Various settings ##############################
	// FTP
	// Nothing to be configured here...
	// Default user/password is esp32/esp32 but can be changed via webgui

	#ifdef CONFIG_MEASURE_BATTERY
		constexpr uint8_t s_batteryCheckInterval = 10; // How often battery is measured (in minutes) (can be changed via GUI!)
	#endif

	#ifdef CONFIG_MEASURE_BATTERY_VOLTAGE
		// (optional) Default-voltages for battery-monitoring via Neopixel; can be changed later via WebGUI
		constexpr float s_warningLowVoltage = 3.4;                      // If battery-voltage is <= this value, a cyclic warning will be indicated by Neopixel (can be changed via GUI!)
		constexpr float s_warningCriticalVoltage = 3.1;                 // If battery-voltage is <= this value, assume battery near-empty. Set to 0V to disable.
		constexpr float s_voltageIndicatorLow = 3.0;                    // Lower range for Neopixel-voltage-indication (0 leds) (can be changed via GUI!)
		constexpr float s_voltageIndicatorHigh = 4.2;                   // Upper range for Neopixel-voltage-indication (all leds) (can be changed via GUI!)
	#endif

	#ifdef CONFIG_MEASURE_BATTERY_MAX17055
		constexpr float s_batteryLow = 15.0;            // low percentage
		constexpr float s_batteryCritical = 5.0;        // critical percentage

		constexpr uint16_t s_batteryCapacity = 6000;    // design cap of battery (mAh)
		constexpr uint16_t s_emptyVoltage = 300;        // empty voltage in 10mV
		constexpr uint16_t s_recoveryVoltage = 360;     // recovery voltage in 10mV
		constexpr uint8_t  s_batteryChemistry = 0x60;   // 0 = Li-Ion, 0x20 = NCR, 0x60 = LiFePO4
		constexpr float s_resistSensor = 0.01;          // current sense resistor, currently non-default values might lead to problems
		constexpr bool s_vCharge = false;                   // true if charge voltage is greater than 4.275V
	#endif

	// enable I2C if necessary
	#if defined(CONFIG_RFID_READER_MFRC522_I2C) || defined(CONFIG_PORT_EXPANDER) || defined(CONFIG_MEASURE_BATTERY_MAX17055)
		#define I2C_2_ENABLE
	#endif

	// (optinal) Headphone-detection (leave unchanged if in doubts...)
	#ifdef CONFIG_HEADPHONE_ADJUST
		constexpr uint16_t headphoneLastDetectionDebounce = 1000; // Debounce-interval in ms when plugging in headphone
	#endif

	// Seekmode-configuration
	constexpr uint8_t jumpOffset = 30;                            // Offset in seconds to jump for commands CMD_SEEK_FORWARDS / CMD_SEEK_BACKWARDS

	// (optional) Topics for MQTT
	#ifdef MQTT_ENABLE
		constexpr uint16_t mqttRetryInterval = 60;                // Try to reconnect to MQTT-server every (n) seconds if connection is broken
		constexpr uint8_t mqttMaxRetriesPerInterval = 1;          // Number of retries per time-interval (mqttRetryInterval). mqttRetryInterval 60 / mqttMaxRetriesPerInterval 1 => once every 60s
		#define DEVICE_HOSTNAME "ESP32-ESPuino"         // Name that is used for MQTT
		constexpr const char topicSleepCmnd[] PROGMEM = "Cmnd/ESPuino/Sleep";
		constexpr const char topicSleepState[] PROGMEM = "State/ESPuino/Sleep";
		constexpr const char topicRfidCmnd[] PROGMEM = "Cmnd/ESPuino/Rfid";
		constexpr const char topicRfidState[] PROGMEM = "State/ESPuino/Rfid";
		constexpr const char topicTrackState[] PROGMEM = "State/ESPuino/Track";
		constexpr const char topicTrackControlCmnd[] PROGMEM = "Cmnd/ESPuino/TrackControl";
		constexpr const char topicCoverChangedState[] PROGMEM = "State/ESPuino/CoverChanged";
		constexpr const char topicLoudnessCmnd[] PROGMEM = "Cmnd/ESPuino/Loudness";
		constexpr const char topicLoudnessState[] PROGMEM = "State/ESPuino/Loudness";
		constexpr const char topicSleepTimerCmnd[] PROGMEM = "Cmnd/ESPuino/SleepTimer";
		constexpr const char topicSleepTimerState[] PROGMEM = "State/ESPuino/SleepTimer";
		constexpr const char topicState[] PROGMEM = "State/ESPuino/State";
		constexpr const char topicCurrentIPv4IP[] PROGMEM = "State/ESPuino/IPv4";
		constexpr const char topicLockControlsCmnd[] PROGMEM ="Cmnd/ESPuino/LockControls";
		constexpr const char topicLockControlsState[] PROGMEM ="State/ESPuino/LockControls";
		constexpr const char topicPlaymodeState[] PROGMEM = "State/ESPuino/Playmode";
		constexpr const char topicRepeatModeCmnd[] PROGMEM = "Cmnd/ESPuino/RepeatMode";
		constexpr const char topicRepeatModeState[] PROGMEM = "State/ESPuino/RepeatMode";
		constexpr const char topicLedBrightnessCmnd[] PROGMEM = "Cmnd/ESPuino/LedBrightness";
		constexpr const char topicLedBrightnessState[] PROGMEM = "State/ESPuino/LedBrightness";
		constexpr const char topicWiFiRssiState[] PROGMEM = "State/ESPuino/WifiRssi";
		constexpr const char topicSRevisionState[] PROGMEM = "State/ESPuino/SoftwareRevision";
		#ifdef CONFIG_MEASURE_BATTERY
		constexpr const char topicBatteryVoltage[] PROGMEM = "State/ESPuino/Voltage";
		constexpr const char topicBatterySOC[] PROGMEM     = "State/ESPuino/Battery";
		#endif
	#endif

	// !!! MAKE SURE TO EDIT PLATFORM SPECIFIC settings-****.h !!!
	#ifdef CONFIG_BOARD_LOLIN32
		#include "settings-lolin32.h"                       // Contains all user-relevant settings for Wemos Lolin32
	#elif CONFIG_BOARD_AUDIO_KIT
		#include "settings-espa1s.h"                        // Contains all user-relevant settings for ESP32-A1S Audiokit
	#elif CONFIG_BOARD_LOLIN_D32
		#include "settings-lolin_d32.h"                     // Contains all user-relevant settings for Wemos Lolin D32
	#elif CONFIG_BOARD_LOLIN_D32_PRO
		#include "settings-lolin_d32_pro.h"                 // Contains all user-relevant settings for Wemos Lolin D32 pro
	#elif CONFIG_BOARD_TTGO_T8
		#include "settings-ttgo_t8.h"                       // Contains all user-relevant settings for Lilygo TTGO T8 1.7
	#elif CONFIG_BOARD_COMPLETE
		#include "settings-complete.h"                      // Contains all user-relevant settings for ESPuino complete
	#elif CONFIG_BOARD_NODEMCU_DEVKIT
		#include "settings-azdelivery_sdmmc.h"              // Pre-configured settings for AZ Delivery ESP32 NodeMCU / Devkit C (https://forum.espuino.de/t/az-delivery-esp32-nodemcu-devkit-c-mit-sd-mmc-und-pn5180-als-rfid-leser/634)
	#elif CONFIG_BOARD_MUSE_PROTO
		#include "settings-muse_proto.h"                     // Pre-configured settings for Raspiaudio ESPMuse Proto Board with I2C RFID Reader (https://raspiaudio.com/produit/muse-proto)
	#endif

	//#define ENABLE_ESPUINO_DEBUG                            // Needs modification of platformio.ini (https://forum.espuino.de/t/rfid-mit-oder-ohne-task/353/21); better don't enable unless you know what you're doing :-)
	#endif //settings_override
#endif
