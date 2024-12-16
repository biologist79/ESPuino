// !!! MAKE SURE TO EDIT settings.h !!!
#include <Arduino.h>
#include "settings.h" // Contains all user-relevant settings (general)

#include "main.h"

#include "AudioPlayer.h"
#include "Battery.h"
#include "Bluetooth.h"
#include "Button.h"
#include "Cmd.h"
#include "Common.h"
#include "Ftp.h"
#include "HallEffectSensor.h"
#include "IrReceiver.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "Mqtt.h"
#include "Port.h"
#include "Power.h"
#include "Queues.h"
#include "Rfid.h"
#include "RotaryEncoder.h"
#include "SdCard.h"
#include "System.h"
#include "Web.h"
#include "Wlan.h"
#include "revision.h"

#include <Wire.h>

bool gPlayLastRfIdWhenWiFiConnected = false;
bool gTriedToConnectToHost = false;

static constexpr const char *logo = R"literal(
 _____   ____    ____            _
| ____| / ___|  |  _ \   _   _  (_)  _ __     ___
|  _|   \__  \  | |_) | | | | | | | | '_ \   / _ \
| |___   ___) | |  __/  | |_| | | | | | | | | (_) |
|_____| |____/  |_|      \__,_| |_| |_| |_|  \___/
         Rfid-controlled musicplayer


)literal";

// avoid PSRAM check while wake-up from deepsleep
bool testSPIRAM(void) {
	return true;
}

#ifdef PLAY_LAST_RFID_AFTER_REBOOT
bool recoverLastRfid = true;
bool recoverBootCount = true;
bool resetBootCount = false;
uint32_t bootCount = 0;
#endif

////////////

// I2C
#ifdef I2C_2_ENABLE
TwoWire i2cBusTwo = TwoWire(1);
#endif

#ifdef PLAY_LAST_RFID_AFTER_REBOOT
// If a problem occurs, remembering last rfid can lead into a boot loop that's hard to escape of.
// That reason for a mechanism is necessary to prevent this.
// At start of a boot, bootCount is incremented by one and after 30s decremented because
// uptime of 30s is considered as "successful boot".
void recoverBootCountFromNvs(void) {
	if (recoverBootCount) {
		recoverBootCount = false;
		resetBootCount = true;
		bootCount = gPrefsSettings.getUInt("bootCount", 999);

		if (bootCount == 999) { // first init
			bootCount = 1;
			gPrefsSettings.putUInt("bootCount", bootCount);
		} else if (bootCount >= 3) { // considered being a bootloop => don't recover last rfid!
			bootCount = 1;
			gPrefsSettings.putUInt("bootCount", bootCount);
			gPrefsSettings.putString("lastRfid", "-1"); // reset last rfid
			Log_Println(bootLoopDetected, LOGLEVEL_ERROR);
			recoverLastRfid = false;
		} else { // normal operation
			gPrefsSettings.putUInt("bootCount", ++bootCount);
		}
	}

	if (resetBootCount && millis() >= 30000) { // reset bootcount
		resetBootCount = false;
		bootCount = 0;
		gPrefsSettings.putUInt("bootCount", bootCount);
		Log_Println(noBootLoopDetected, LOGLEVEL_INFO);
	}
}

// Get last RFID-tag applied from NVS
void recoverLastRfidPlayedFromNvs(bool force) {
	if (recoverLastRfid || force) {
		if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) { // Don't recover if BT-mode is desired
			recoverLastRfid = false;
			return;
		}
		recoverLastRfid = false;
		String lastRfidPlayed = gPrefsSettings.getString("lastRfid", "-1");
		if (!lastRfidPlayed.compareTo("-1")) {
			Log_Println(unableToRestoreLastRfidFromNVS, LOGLEVEL_INFO);
		} else {
			xQueueSend(gRfidCardQueue, lastRfidPlayed.c_str(), 0);
			gPlayLastRfIdWhenWiFiConnected = !force;
			Log_Printf(LOGLEVEL_INFO, restoredLastRfidFromNVS, lastRfidPlayed.c_str());
		}
	}
}
#endif

void setup() {
	Log_Init();
	Queues_Init();

	// Make sure all wakeups can be enabled *before* initializing RFID, which can enter sleep immediately
	Button_Init(); // To preseed internal button-storage with values

#ifdef PN5180_ENABLE_LPCD
	System_Init_LPCD();
	Rfid_Init();
#endif

	System_Init();

// Init 2nd i2c-bus if RC522 is used with i2c or if port-expander is enabled
#ifdef I2C_2_ENABLE
	i2cBusTwo.begin(ext_IIC_DATA, ext_IIC_CLK);
	delay(50);
	Log_Println(rfidScannerReady, LOGLEVEL_DEBUG);
#endif

#ifdef HALLEFFECT_SENSOR_ENABLE
	gHallEffectSensor.init();
#endif

	// Needs i2c first if port-expander is used
	Port_Init();

	// If port-expander is used, port_init has to be called first, as power can be (possibly) done by port-expander
	Power_Init();

	Battery_Init();

	// Init audio before power on to avoid speaker noise
	AudioPlayer_Init();

	// All checks that could send us to sleep are done, power up fully
	Power_PeripheralOn();

	Led_Init();

	// Needs power first
	SdCard_Init();

	// welcome message
	Serial.print(logo);

	// Software-version
	Log_Println(softwareRevision, LOGLEVEL_NOTICE);
	Log_Println(gitRevision, LOGLEVEL_NOTICE);
	Log_Printf(LOGLEVEL_NOTICE, "Arduino version: %d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
	Log_Printf(LOGLEVEL_NOTICE, "ESP-IDF version: %s", ESP.getSdkVersion());

	// print wake-up reason
	System_ShowWakeUpReason();
	// print SD card info
	SdCard_PrintInfo();

	Ftp_Init();
	Mqtt_Init();
#ifndef PN5180_ENABLE_LPCD
	#if defined(RFID_READER_TYPE_MFRC522_SPI) || defined(RFID_READER_TYPE_MFRC522_I2C) || defined(RFID_READER_TYPE_PN5180)
	Rfid_Init();
	#endif
#endif
	RotaryEncoder_Init();
	Wlan_Init();
	Bluetooth_Init();

	if (OPMODE_NORMAL == System_GetOperationMode()) {
		Wlan_Cyclic();
	}

	IrReceiver_Init();
	System_UpdateActivityTimer(); // initial set after boot
	Led_Indicate(LedIndicatorType::BootComplete);

	Log_Printf(LOGLEVEL_DEBUG, "%s: %u", freeHeapAfterSetup, ESP.getFreeHeap());
	if (psramFound()) {
		Log_Printf(LOGLEVEL_DEBUG, "PSRAM: %u bytes", ESP.getPsramSize());
	} else {
		Log_Println("PSRAM: --", LOGLEVEL_DEBUG);
	}
	Log_Printf(LOGLEVEL_DEBUG, "Flash-size: %u bytes", ESP.getFlashChipSize());

	// setup timezone & show internal RTC date/time if available
	setenv("TZ", timeZone, 1);
	tzset();
	struct tm timeinfo;
	if (getLocalTime(&timeinfo, 5)) {
		static char timeStringBuff[255];
		snprintf(timeStringBuff, sizeof(timeStringBuff), dateTimeRTC, timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
		Log_Println(timeStringBuff, LOGLEVEL_DEBUG);
	}

	if (Wlan_IsConnected()) {
		Log_Printf(LOGLEVEL_DEBUG, "RSSI: %d dBm", Wlan_GetRssi());
	}

#ifdef CONTROLS_LOCKED_BY_DEFAULT
	System_SetLockControls(true);
#endif
}

void loop() {
	if (OPMODE_BLUETOOTH_SINK == System_GetOperationMode()) {
		// bluetooth speaker mode
		Bluetooth_Cyclic();
	} else if (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode()) {
		// bluetooth headset mode
		Bluetooth_Cyclic();
		RotaryEncoder_Cyclic();
	} else {
		// normal mode
		Wlan_Cyclic();
		Web_Cyclic();
		Ftp_Cyclic();
		RotaryEncoder_Cyclic();
		Mqtt_Cyclic();
	}
	vTaskDelay(portTICK_PERIOD_MS * 1u);
	AudioPlayer_Cyclic();
	vTaskDelay(portTICK_PERIOD_MS * 1u);
	Battery_Cyclic();
	// Port_Cyclic(); // called by button (controlled via hw-timer)
	Button_Cyclic();
	vTaskDelay(portTICK_PERIOD_MS * 1u);
	System_Cyclic();
	Rfid_PreferenceLookupHandler();

#ifdef PLAY_LAST_RFID_AFTER_REBOOT
	recoverBootCountFromNvs();
	recoverLastRfidPlayedFromNvs();
#endif

	IrReceiver_Cyclic();
	vTaskDelay(portTICK_PERIOD_MS * 2u);

#ifdef HALLEFFECT_SENSOR_ENABLE
	gHallEffectSensor.cyclic();
#endif
}
