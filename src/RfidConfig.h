#pragma once

#include <Arduino.h>
#include "Log.h"

enum RfidReaderType : uint8_t {
    TYPE_AUTO_DETECT = 0, // Auto-detect reader type at runtime
    TYPE_MFRC522_SPI = 1,
    TYPE_MFRC522_I2C = 2,
    TYPE_PN5180 = 3
};

// Initialize RFID reader configuration from NVS
void RfidConfig_Init(void);

// Get the current RFID reader type
RfidReaderType RfidConfig_GetReaderType(void);

// Auto-detect the RFID reader type
RfidReaderType RfidConfig_AutoDetectReader(void);

// Check if a specific reader type is available
bool RfidConfig_IsReaderAvailable(RfidReaderType readerType);