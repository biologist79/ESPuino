// !!! MAKE SURE TO EDIT settings.h !!!
#include <Arduino.h>
#include <Wire.h>
#include "settings.h" // Contains all user-relevant settings (general)

#include "AudioPlayer.h"
#include "Battery.h"
#include "Bluetooth.h"
#include "Button.h"
#include "Cmd.h"
#include "Common.h"
#include "Ftp.h"
#include "IrReceiver.h"
#include "Led.h"
#include "Log.h"
#include "Mqtt.h"
#include "MemX.h"
#include "Port.h"
#include "Queues.h"
#include "Rfid.h"
#include "RotaryEncoder.h"
#include "SdCard.h"
#include "System.h"
#include "Web.h"
#include "Wlan.h"
#include "revision.h"
#include "Power.h"

#ifdef PLAY_LAST_RFID_AFTER_REBOOT
    bool recoverLastRfid = true;
    bool recoverBootCount = true;
    bool resetBootCount = false;
    uint32_t bootCount = 0;
#endif

////////////

#if (HAL == 2)
    #include "AC101.h"
    static TwoWire i2cBusOne = TwoWire(0);
    static AC101 ac(&i2cBusOne);
#endif

// I2C
#if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(PORT_EXPANDER_ENABLE)
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

            if (bootCount == 999) {         // first init
                bootCount = 1;
                gPrefsSettings.putUInt("bootCount", bootCount);
            } else if (bootCount >= 3) {    // considered being a bootloop => don't recover last rfid!
                bootCount = 1;
                gPrefsSettings.putUInt("bootCount", bootCount);
                gPrefsSettings.putString("lastRfid", "-1");     // reset last rfid
                Log_Println((char *) FPSTR(bootLoopDetected), LOGLEVEL_ERROR);
                recoverLastRfid = false;
            } else {                        // normal operation
                gPrefsSettings.putUInt("bootCount", ++bootCount);
            }
        }

        if (resetBootCount && millis() >= 30000) {      // reset bootcount
            resetBootCount = false;
            bootCount = 0;
            gPrefsSettings.putUInt("bootCount", bootCount);
            Log_Println((char *) FPSTR(noBootLoopDetected), LOGLEVEL_INFO);
        }
    }

    // Get last RFID-tag applied from NVS
    void recoverLastRfidPlayedFromNvs(void) {
        if (recoverLastRfid) {
            if (System_GetOperationMode() == OPMODE_BLUETOOTH) { // Don't recover if BT-mode is desired
                recoverLastRfid = false;
                return;
            }
            recoverLastRfid = false;
            String lastRfidPlayed = gPrefsSettings.getString("lastRfid", "-1");
            if (!lastRfidPlayed.compareTo("-1")) {
                Log_Println((char *) FPSTR(unableToRestoreLastRfidFromNVS), LOGLEVEL_INFO);
            } else {
                char *lastRfid = x_strdup(lastRfidPlayed.c_str());
                xQueueSend(gRfidCardQueue, lastRfid, 0);
                snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(restoredLastRfidFromNVS), lastRfidPlayed.c_str());
                Log_Println(Log_Buffer, LOGLEVEL_INFO);
            }
        }
    }
#endif

// Print the wake-up reason why ESP32 is awake now
void printWakeUpReason() {
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println(F("Wakeup caused by push button"));
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            Serial.println(F("Wakeup caused by low power card detection"));
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println(F("Wakeup caused by timer"));
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            Serial.println(F("Wakeup caused by touchpad"));
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            Serial.println(F("Wakeup caused by ULP program"));
            break;
        default:
            Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
            break;
    }
}

void setup() {
    Log_Init();
    Queues_Init();

    // Make sure all wakeups can be enabled *before* initializing RFID, which can enter sleep immediately
    Button_Init();  // To preseed internal button-storage with values
    #ifdef PN5180_ENABLE_LPCD
        Rfid_Init();
    #endif

    System_Init();

    // Init 2nd i2c-bus if RC522 is used with i2c or if port-expander is enabled
    #if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(PORT_EXPANDER_ENABLE)
        i2cBusTwo.begin(ext_IIC_DATA, ext_IIC_CLK);
        delay(50);
        Log_Println((char *) FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);
    #endif

    #ifdef PORT_EXPANDER_ENABLE     // Needs i2c first
        Port_Init();
    #endif

    // If port-expander is used, port_init has to be called first, as power can be (possibly) done by port-expander
    Power_Init();

    Battery_Init();

    // Init audio before power on to avoid speaker noise
    AudioPlayer_Init();

    // All checks that could send us to sleep are done, power up fully
    Power_PeripheralOn();

    memset(&gPlayProperties, 0, sizeof(gPlayProperties));
    gPlayProperties.playlistFinished = true;

    #ifdef PLAY_MONO_SPEAKER
        gPlayProperties.newPlayMono = true;
        gPlayProperties.currentPlayMono = true;
    #endif

    Led_Init();

    #if (HAL == 2)
        i2cBusOne.begin(IIC_DATA, IIC_CLK, 40000);

        while (not ac.begin()) {
            Serial.println(F("AC101 Failed!"));
            delay(1000);
        }
        Serial.println(F("AC101 via I2C - OK!"));

        pinMode(22, OUTPUT);
        digitalWrite(22, HIGH);

        #if (GPIO_PA_EN >= 0 && GPIO_PA_EN <= 39)
            pinMode(GPIO_PA_EN, OUTPUT);
            digitalWrite(GPIO_PA_EN, HIGH);
        #endif
        Serial.println(F("Built-in amplifier enabled\n"));
    #endif

    // Needs power first
    SdCard_Init();

    // welcome message
    Serial.println(F(""));
    Serial.println(F("  _____   ____    ____            _                 "));
    Serial.println(F(" | ____| / ___|  |  _ \\   _   _  (_)  _ __     ___  "));
    Serial.println(F(" |  _|   \\__  \\  | |_) | | | | | | | | '_ \\   / _ \\"));
    Serial.println(F(" | |___   ___) | |  __/  | |_| | | | | | | | | (_) |"));
    Serial.println(F(" |_____| |____/  |_|      \\__,_| |_| |_| |_|  \\___/ "));
    Serial.print(F(" Rfid-controlled musicplayer\n\n"));
    Serial.printf("%s\n\n", softwareRevision);
    Serial.println("ESP-IDF version: " + String(ESP.getSdkVersion()));

    // print wake-up reason
    printWakeUpReason();
	// print SD card info
    SdCard_PrintInfo();

    Ftp_Init();
    Mqtt_Init();
    #ifndef PN5180_ENABLE_LPCD
        Rfid_Init();
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

    snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(freeHeapAfterSetup), ESP.getFreeHeap());
    Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
    snprintf(Log_Buffer, Log_BufferLength, "PSRAM: %u bytes", ESP.getPsramSize());
    Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
    snprintf(Log_Buffer, Log_BufferLength, "Flash-size: %u bytes", ESP.getFlashChipSize());
    Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
    if (Wlan_IsConnected()) {
        snprintf(Log_Buffer, Log_BufferLength, "RSSI: %d dBm", Wlan_GetRssi());
        Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
    }
    System_ShowUpgradeWarning();
}

void loop() {
    Rfid_Cyclic();

    if (OPMODE_BLUETOOTH == System_GetOperationMode()) {
        Bluetooth_Cyclic();
    } else {
        Wlan_Cyclic();
        Web_Cyclic();
        Ftp_Cyclic();
        RotaryEncoder_Cyclic();
        Mqtt_Cyclic();
    }

    AudioPlayer_Cyclic();
    Battery_Cyclic();
    //Port_Cyclic(); // called by button (controlled via hw-timer)
    Button_Cyclic();
    System_Cyclic();
    Rfid_PreferenceLookupHandler();

    #ifdef PLAY_LAST_RFID_AFTER_REBOOT
        recoverBootCountFromNvs();
        recoverLastRfidPlayedFromNvs();
    #endif

    IrReceiver_Cyclic();
    vTaskDelay(portTICK_RATE_MS * 5u);
}
