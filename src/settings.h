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
