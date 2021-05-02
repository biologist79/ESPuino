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

#ifdef PLAY_LAST_RFID_AFTER_REBOOT
    bool recoverLastRfid = true;
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
    // Get last RFID-tag applied from NVS
    void recoverLastRfidPlayed(void) {
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

void setup()
{
    Log_Init();
    Rfid_WakeupCheck();
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
        i2cBusTwo.begin(ext_IIC_DATA, ext_IIC_CLK, 40000);
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
    Serial.println(F(" Rfid-controlled musicplayer\n"));
    Serial.println(F(" Rev 20210502-1\n"));

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

    Queues_Init();
    Ftp_Init();
    AudioPlayer_Init();
    Mqtt_Init();
    Battery_Init();
    Button_Init();
    Rfid_Init();
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
    Serial.printf("PSRAM: %u bytes\n", ESP.getPsramSize());
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
    Port_Cyclic();
    Button_Cyclic();
    System_Cyclic();
    Rfid_PreferenceLookupHandler();

    #ifdef PLAY_LAST_RFID_AFTER_REBOOT
        recoverLastRfidPlayed();
    #endif

    IrReceiver_Cyclic();

    vTaskDelay(5u);
}
