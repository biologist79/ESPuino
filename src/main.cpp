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

    // make sure all wakeups can be enabled *before* initializing RFID, which can enter sleep immediately
    #ifdef RFID_READER_TYPE_PN5180
        Button_Init();
        Rfid_Init();
    #endif

    System_Init();

    memset(&gPlayProperties, 0, sizeof(gPlayProperties));
    gPlayProperties.playlistFinished = true;

    #ifdef PLAY_MONO_SPEAKER
        gPlayProperties.newPlayMono = true;
        gPlayProperties.currentPlayMono = true;
    #endif

    // Examples for serialized RFID-actions that are stored in NVS
    // #<file/folder>#<startPlayPositionInBytes>#<playmode>#<trackNumberToStartWith>
    // Please note: There's no need to do this manually (unless you want to)
    /*gPrefsRfid.putString("215123125075", "#/mp3/Kinderlieder#0#6#0");
    gPrefsRfid.putString("169239075184", "#http://radio.koennmer.net/evosonic.mp3#0#8#0");
    gPrefsRfid.putString("244105171042", "#0#0#111#0"); // modification-card (repeat track)
    gPrefsRfid.putString("228064156042", "#0#0#110#0"); // modification-card (repeat playlist)
    gPrefsRfid.putString("212130160042", "#/mp3/Hoerspiele/Yakari/Sammlung2#0#3#0");*/

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

        pinMode(GPIO_PA_EN, OUTPUT);
        digitalWrite(GPIO_PA_EN, HIGH);
        Serial.println(F("Built-in amplifier enabled\n"));
    #endif

    SdCard_Init();

    // Init 2nd i2c-bus if RC522 is used with i2c or if port-expander is enabled
    #if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(PORT_EXPANDER_ENABLE)
        i2cBusTwo.begin(ext_IIC_DATA, ext_IIC_CLK);
        //i2cBusTwo.begin(ext_IIC_DATA, ext_IIC_CLK, 40000);
        delay(50);
        Log_Println((char *) FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);
    #endif

    // welcome message
    Serial.println(F(""));
    Serial.println(F("  _____   ____    ____            _                 "));
    Serial.println(F(" | ____| / ___|  |  _ \\   _   _  (_)  _ __     ___  "));
    Serial.println(F(" |  _|   \\__  \\  | |_) | | | | | | | | '_ \\   / _ \\"));
    Serial.println(F(" | |___   ___) | |  __/  | |_| | | | | | | | | (_) |"));
    Serial.println(F(" |_____| |____/  |_|      \\__,_| |_| |_| |_|  \\___/ "));
    Serial.print(F(" Rfid-controlled musicplayer\n\n"));
    Serial.printf("%s\n\n", softwareRevision);
    Serial.println("ESP-IDF version: " + String(esp_get_idf_version()));

    // print wake-up reason
    printWakeUpReason();

    // show SD card type
    sdcard_type_t cardType = SdCard_GetType();
    Serial.print(F("SD card type: "));
    if (cardType == CARD_MMC) {
        Serial.println(F("MMC"));
    } else if (cardType == CARD_SD) {
        Serial.println(F("SDSC"));
    } else if (cardType == CARD_SDHC) {
        Serial.println(F("SDHC"));
    } else {
        Serial.println(F("UNKNOWN"));
    }

    #ifdef PORT_EXPANDER_ENABLE
        Port_Init();
    #endif
    Ftp_Init();
    AudioPlayer_Init();
    Mqtt_Init();
    Battery_Init();
    #ifndef RFID_READER_TYPE_PN5180
        Button_Init();
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
