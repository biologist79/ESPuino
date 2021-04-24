#include <Arduino.h>
#include "settings.h"
#include "Rfid.h"
#include "Log.h"
#include "MemX.h"
#include "Queues.h"
#include "System.h"
#include "i2c.h"

#if defined RFID_READER_TYPE_MFRC522_SPI || defined RFID_READER_TYPE_MFRC522_I2C

#ifdef RFID_READER_TYPE_MFRC522_SPI
#include <MFRC522.h>
#endif
#ifdef RFID_READER_TYPE_MFRC522_I2C
#include <MFRC522_I2C.h>
#endif

extern unsigned long Rfid_LastRfidCheckTimestamp;

#ifdef RFID_READER_TYPE_MFRC522_I2C

static MFRC522_I2C mfrc522(MFRC522_ADDR, MFRC522_RST_PIN, &i2cBusTwo);
#endif
#ifdef RFID_READER_TYPE_MFRC522_SPI
static MFRC522 mfrc522(RFID_CS, RST_PIN);
#endif

void Rfid_Init(void)
{
#ifdef RFID_READER_TYPE_MFRC522_SPI
    SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
    SPI.setFrequency(1000000);
#endif

    // Init RC522 Card-Reader
#if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(RFID_READER_TYPE_MFRC522_SPI)
    mfrc522.PCD_Init();
    mfrc522.PCD_SetAntennaGain(rfidGain);
    delay(50);
    Log_Println((char *)FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);
#endif
}

void Rfid_Cyclic(void)
{
    byte cardId[cardIdSize];
    String cardIdString;

    if ((millis() - Rfid_LastRfidCheckTimestamp) >= RFID_SCAN_INTERVAL)
    {
        Rfid_LastRfidCheckTimestamp = millis();
        // Reset the loop if no new card is present on the sensor/reader. This saves the entire process when idle.

        if (!mfrc522.PICC_IsNewCardPresent())
        {
            return;
        }

        // Select one of the cards
        if (!mfrc522.PICC_ReadCardSerial())
        {
            return;
        }

        //mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();

        memcpy(cardId, mfrc522.uid.uidByte, cardIdSize);

        Log_Print((char *)FPSTR(rfidTagDetected), LOGLEVEL_NOTICE);
        for (uint8_t i = 0u; i < cardIdSize; i++)
        {
            snprintf(Log_Buffer, Log_BufferLength, "%02x%s", cardId[i], (i < cardIdSize - 1u) ? "-" : "\n");
            Log_Print(Log_Buffer, LOGLEVEL_NOTICE);
        }

        for (uint8_t i = 0u; i < cardIdSize; i++)
        {
            char num[4];
            snprintf(num, sizeof(num), "%03d", cardId[i]);
            cardIdString += num;
        }

        xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0);
    }
}

void Rfid_Exit(void)
{
}

void Rfid_WakeupCheck(void)
{
}

#endif