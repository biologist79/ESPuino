#include <Arduino.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include "settings.h"
#include "Rfid.h"
#include "Log.h"
#include "MemX.h"
#include "Queues.h"
#include "System.h"

#ifdef RFID_READER_TYPE_PN5180
    #include <PN5180.h>
    #include <PN5180ISO14443.h>
    #include <PN5180ISO15693.h>
#endif

#define RFID_PN5180_STATE_INIT 0u

#define RFID_PN5180_NFC14443_STATE_RESET 1u
#define RFID_PN5180_NFC14443_STATE_SETUPRF 2u
#define RFID_PN5180_NFC14443_STATE_READCARD 3u

#define RFID_PN5180_NFC15693_STATE_RESET 4u
#define RFID_PN5180_NFC15693_STATE_SETUPRF 5u
#define RFID_PN5180_NFC15693_STATE_DISABLEPRIVACYMODE 6u
#define RFID_PN5180_NFC15693_STATE_GETINVENTORY 7u

extern unsigned long Rfid_LastRfidCheckTimestamp;

#ifdef RFID_READER_TYPE_PN5180
    static void Rfid_Task(void *parameter);
    static void Rfid_Read(void);

    void Rfid_Init(void) {
        #ifdef PN5180_ENABLE_LPCD
            // disable pin hold from deep sleep
            gpio_deep_sleep_hold_dis();
            gpio_hold_dis(gpio_num_t(RFID_CS));  // NSS
            gpio_hold_dis(gpio_num_t(RFID_RST)); // RST
        #endif

        // Create task for rfid
        xTaskCreatePinnedToCore(
            Rfid_Task,   /* Function to implement the task */
            "Rfid_Task", /* Name of the task */
            1500,        /* Stack size in words */
            NULL,        /* Task input parameter */
            1,           /* Priority of the task */
            NULL,        /* Task handle. */
            0            /* Core where the task should run */
        );
    }

    void Rfid_Cyclic(void) {
        // Implemented via task
    }

    void Rfid_Task(void *parameter) {
        for (;;) {
            Rfid_Read();
            vTaskDelay(5u);
        }
    }

    void Rfid_Read(void) {
        static PN5180ISO14443 nfc14443(RFID_CS, RFID_BUSY, RFID_RST);
        static PN5180ISO15693 nfc15693(RFID_CS, RFID_BUSY, RFID_RST);
        static uint8_t stateMachine = RFID_PN5180_STATE_INIT;
        byte cardId[cardIdSize], lastCardId[cardIdSize];
        uint8_t uid[10];
        String cardIdString;
        bool cardReceived = false;

        if (RFID_PN5180_STATE_INIT == stateMachine) {
            nfc14443.begin();
            nfc14443.reset();
            // show PN5180 reader version
            uint8_t firmwareVersion[2];
            nfc14443.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
            Serial.print(F("Firmware version="));
            Serial.print(firmwareVersion[1]);
            Serial.print(".");
            Serial.println(firmwareVersion[0]);

            // activate RF field
            delay(4);
            Log_Println((char *) FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);

        // 1. check for an ISO-14443 card
        } else if (RFID_PN5180_NFC14443_STATE_RESET == stateMachine) {
            nfc14443.reset();
        } else if (RFID_PN5180_NFC14443_STATE_SETUPRF == stateMachine) {
            nfc14443.setupRF();
        } else if (RFID_PN5180_NFC14443_STATE_READCARD == stateMachine) {
            if (nfc14443.readCardSerial(uid) >= 4u) {
                cardReceived = true;
            }

        // 2. check for an ISO-15693 card
        } else if (RFID_PN5180_NFC15693_STATE_RESET == stateMachine) {
            nfc15693.reset();
        } else if (RFID_PN5180_NFC15693_STATE_SETUPRF == stateMachine) {
            nfc15693.setupRF();
        } else if (RFID_PN5180_NFC15693_STATE_DISABLEPRIVACYMODE == stateMachine) {
            // check for ICODE-SLIX2 password protected tag
            // put your privacy password here, e.g.:
            // https://de.ifixit.com/Antworten/Ansehen/513422/nfc+Chips+f%C3%BCr+tonies+kaufen
            uint8_t password[] = {0x01, 0x02, 0x03, 0x04};
            ISO15693ErrorCode myrc = nfc15693.disablePrivacyMode(password);
            if (ISO15693_EC_OK == myrc) {
                Serial.println(F("disabling privacy-mode successful"));
            }
        } else if (RFID_PN5180_NFC15693_STATE_GETINVENTORY == stateMachine) {
            // try to read ISO15693 inventory
            ISO15693ErrorCode rc = nfc15693.getInventory(uid);
            if (rc == ISO15693_EC_OK) {
                cardReceived = true;
            }
        }

        // send card to queue
        if (cardReceived) {
            memcpy(cardId, uid, cardIdSize);

            // check for different card id
            if (memcmp((const void *)cardId, (const void *)lastCardId, sizeof(cardId)) == 0) {
                // reset state machine
                stateMachine = RFID_PN5180_NFC14443_STATE_RESET;
                return;
            }

            memcpy(lastCardId, cardId, cardIdSize);

            Log_Print((char *) FPSTR(rfidTagDetected), LOGLEVEL_NOTICE);
            for (uint8_t i = 0u; i < cardIdSize; i++) {
                snprintf(Log_Buffer, Log_BufferLength, "%02x%s", cardId[i], (i < cardIdSize - 1u) ? "-" : "\n");
                Log_Print(Log_Buffer, LOGLEVEL_NOTICE);
            }

            for (uint8_t i = 0u; i < cardIdSize; i++) {
                char num[4];
                snprintf(num, sizeof(num), "%03d", cardId[i]);
                cardIdString += num;
            }

            xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0);
        }

        stateMachine++;

        if (stateMachine > RFID_PN5180_NFC15693_STATE_GETINVENTORY) {
            stateMachine = RFID_PN5180_NFC14443_STATE_RESET;
        }
    }

    void Rfid_Exit(void) {
        // goto low power card detection mode
        #ifdef PN5180_ENABLE_LPCD
            static PN5180 nfc(RFID_CS, RFID_BUSY, RFID_RST);
            nfc.begin();
            // show PN5180 reader version
            uint8_t firmwareVersion[2];
            nfc.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
            Serial.print(F("Firmware version="));
            Serial.print(firmwareVersion[1]);
            Serial.print(".");
            Serial.println(firmwareVersion[0]);
            // check firmware version: PN5180 firmware < 4.0 has several bugs preventing the LPCD mode
            // you can flash latest firmware with this project: https://github.com/abidxraihan/PN5180_Updater_ESP32
            if (firmwareVersion[1] < 4) {
                Serial.println(F("This PN5180 firmware does not work with LPCD! use firmware >= 4.0"));
                return;
            }
            Serial.println(F("prepare low power card detection..."));
            nfc.prepareLPCD();
            nfc.clearIRQStatus(0xffffffff);
            Serial.print(F("PN5180 IRQ PIN: "));
            Serial.println(Port_Read(RFID_IRQ));
            // turn on LPCD
            uint16_t wakeupCounterInMs = 0x3FF; //  must be in the range of 0x0 - 0xA82. max wake-up time is 2960 ms.
            if (nfc.switchToLPCD(wakeupCounterInMs)) {
                Serial.println(F("switch to low power card detection: success"));
                // configure wakeup pin for deep-sleep wake-up, use ext1
                esp_sleep_enable_ext1_wakeup((1ULL << (RFID_IRQ)), ESP_EXT1_WAKEUP_ANY_HIGH);
                // freeze pin states in deep sleep
                gpio_hold_en(gpio_num_t(RFID_CS));  // CS/NSS
                gpio_hold_en(gpio_num_t(RFID_RST)); // RST
                gpio_deep_sleep_hold_en();
            } else {
                Serial.println(F("switchToLPCD failed"));
            }
        #endif
    }

    // wake up from LPCD, check card is present. This works only for ISO-14443 compatible cards
    void Rfid_WakeupCheck(void) {
        #ifdef PN5180_ENABLE_LPCD
            static PN5180ISO14443 nfc14443(RFID_CS, RFID_BUSY, RFID_RST);
            nfc14443.begin();
            nfc14443.reset();
            nfc14443.setupRF();
            if (!nfc14443.isCardPresent()) {
                nfc14443.clearIRQStatus(0xffffffff);
                Serial.print(F("Logic level at PN5180' IRQ-PIN: "));
                Serial.println(Port_Read(RFID_IRQ));
                // turn on LPCD
                uint16_t wakeupCounterInMs = 0x3FF; //  needs to be in the range of 0x0 - 0xA82. max wake-up time is 2960 ms.
                if (nfc14443.switchToLPCD(wakeupCounterInMs)) {
                    Log_Println((char *) FPSTR(lowPowerCardSuccess), LOGLEVEL_INFO);
                    // configure wakeup pin for deep-sleep wake-up, use ext1
                    esp_sleep_enable_ext1_wakeup((1ULL << (RFID_IRQ)), ESP_EXT1_WAKEUP_ANY_HIGH);
                    // freeze pin states in deep sleep
                    gpio_hold_en(gpio_num_t(RFID_CS));  // CS/NSS
                    gpio_hold_en(gpio_num_t(RFID_RST)); // RST
                    gpio_deep_sleep_hold_en();
                    Log_Println((char *) FPSTR(wakeUpRfidNoIso14443), LOGLEVEL_ERROR);
                    esp_deep_sleep_start();
                } else {
                    Serial.println(F("switchToLPCD failed"));
                }
            }
        #endif
    }

#endif
