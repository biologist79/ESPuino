#include "RfidConfig.h"
#include "settings.h"
#include "Log.h"
#include "System.h"
#include "MemX.h"
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#define MFRC522_firmware_referenceV0_0
#define MFRC522_firmware_referenceV1_0
#define MFRC522_firmware_referenceV2_0
#define FM17522_firmware_reference
#include <MFRC522_I2C.h>
#include <PN5180.h>
#include <PN5180ISO14443.h>

// Global variable for the current RFID reader type
RfidReaderType activeRfidReaderType = RfidReaderType::TYPE_AUTO_DETECT; // Default to auto-detect

// Initialize RFID reader configuration from NVS
void RfidConfig_Init(void) {
    // Try to load the RFID reader type from NVS
    activeRfidReaderType = static_cast<RfidReaderType>(gPrefsSettings.getUChar("rfidReaderType", RFID_READER_TYPE_RUNTIME));// Default to auto-detect
    
    // TODO: shall we check, if we detect the selected reader type? If not, we can still try to auto-detect it, but we should log a warning about the mismatch.
    
    // If auto-detect is selected, try to detect the reader
    if (activeRfidReaderType == 0) {
        Log_Println("RFID: Auto-detecting reader type...", LOGLEVEL_INFO);
        RfidReaderType detectedType = RfidConfig_AutoDetectReader();
        if (detectedType != 0) {
            activeRfidReaderType = detectedType;
            // Save the detected type to NVS for future use
            // TODO: maybe don't overwrite it?
            gPrefsSettings.putUChar("rfidReaderType", activeRfidReaderType);
            Log_Printf(LOGLEVEL_INFO, "RFID: Detected reader type: %d", activeRfidReaderType);
        } else {
            Log_Println("RFID: Failed to auto-detect reader type, using default", LOGLEVEL_ERROR);
            activeRfidReaderType = RfidReaderType::TYPE_PN5180; // Default to PN5180
        }
    }

    Log_Printf(LOGLEVEL_INFO, "RFID: Using reader type: %d (%s)", 
               activeRfidReaderType, 
               activeRfidReaderType == RfidReaderType::TYPE_MFRC522_SPI ? "MFRC522 (SPI)" :
               activeRfidReaderType == RfidReaderType::TYPE_MFRC522_I2C ? "MFRC522 (I2C)" :
               activeRfidReaderType == RfidReaderType::TYPE_PN5180 ? "PN5180" : "Unknown");
}

// Get the current RFID reader type
RfidReaderType RfidConfig_GetReaderType(void) {
    return activeRfidReaderType;
}

// Auto-detect the RFID reader type
RfidReaderType RfidConfig_AutoDetectReader(void) {
    // start with the most common reader (PN5180) and then check for MFRC522 variants.
    if (RfidConfig_IsReaderAvailable(RfidReaderType::TYPE_PN5180)) {
        Log_Println("RFID: PN5180 reader detected", LOGLEVEL_INFO);
        return RfidReaderType::TYPE_PN5180;
    }

    if (RfidConfig_IsReaderAvailable(RfidReaderType::TYPE_MFRC522_SPI)) {
        Log_Println("RFID: MFRC522 (SPI) reader detected", LOGLEVEL_INFO);
        return RfidReaderType::TYPE_MFRC522_SPI;
    }

    if (RfidConfig_IsReaderAvailable(RfidReaderType::TYPE_MFRC522_I2C)) {
        Log_Println("RFID: MFRC522 (I2C) reader detected", LOGLEVEL_INFO);
        return RfidReaderType::TYPE_MFRC522_I2C;
    }

    Log_Println("RFID: No reader detected", LOGLEVEL_ERROR);
    return RfidReaderType::TYPE_AUTO_DETECT; // No reader detected
}

// Check if a specific reader type is available
bool RfidConfig_IsReaderAvailable(RfidReaderType readerType) {
    switch (readerType) {
        case RfidReaderType::TYPE_MFRC522_SPI: // MFRC522 (SPI)
            // Try to initialize MFRC522 via SPI and check if it responds
            {
                Log_Println("RFID: Checking MFRC522 (SPI)...", LOGLEVEL_DEBUG);
                MFRC522 mfrc522(RFID_CS, RST_PIN);
                SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
                mfrc522.PCD_Init();
                byte version = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
                SPI.end();
                Log_Printf(LOGLEVEL_DEBUG, "RFID: MFRC522 (SPI) version=0x%02X", version);

                // Check if version is valid for MFRC522 (0x91, 0x92, 0x82, etc.)
                // Valid versions are typically 0x80-0x9x range. Exclude invalid values.
                return ((version >= 0x80) && (version <= 0x9F));
            }
            
        case RfidReaderType::TYPE_MFRC522_I2C: // MFRC522 (I2C)
            // Try to initialize MFRC522 via I2C and check if it responds
            {
                Log_Println("RFID: Checking MFRC522 (I2C)...", LOGLEVEL_DEBUG);
                Wire.begin();
                MFRC522_I2C mfrc522(MFRC522_ADDR, RST_PIN, &Wire);
                mfrc522.PCD_Init();
                byte version = mfrc522.PCD_ReadRegister(MFRC522_I2C::VersionReg);
                Wire.end();
                Log_Printf(LOGLEVEL_DEBUG, "RFID: MFRC522 (I2C) version=0x%02X", version);

                // Check if version is valid for MFRC522 (0x91, 0x92, 0x82, etc.)
                // Valid versions are typically 0x80-0x9x range. Exclude invalid values.
                return ((version >= 0x80) && (version <= 0x9F));
            }
            
        case RfidReaderType::TYPE_PN5180: // PN5180
            // Try to initialize PN5180 and check if it responds
            {
                Log_Println("RFID: Checking PN5180...", LOGLEVEL_DEBUG);
                PN5180 pn5180(RFID_CS, RFID_BUSY, RFID_RST);
                SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
                pn5180.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
                pn5180.reset();

                uint8_t buffer[2] = {0, 0};
                bool ok = pn5180.readEEprom(FIRMWARE_VERSION, buffer, sizeof(buffer));
                SPI.end();
                Log_Printf(LOGLEVEL_DEBUG, "RFID: PN5180 firmware read ok=%d version=%d.%d", ok ? 1 : 0, buffer[1], buffer[0]);

                // PN5180 is present if EEPROM read succeeds AND firmware version is valid.
                // Valid PN5180 versions have major version (buffer[1]) in range 1-99 (0x01-0x63) and not all 0xFF.
                // This prevents false positives from unconnected SPI bus or other devices.
                bool present = ok && (buffer[1] >= 0x01 && buffer[1] <= 0x63) && !(buffer[0] == 0xFF && buffer[1] == 0xFF);
                if (!present) {
                    Log_Println("RFID: PN5180 not present or unreadable", LOGLEVEL_DEBUG);
                }
                return present;
            }

        default:
            return false;
    }
}
