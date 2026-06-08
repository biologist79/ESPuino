// NAME: PN5180.h
//
// DESC: NFC Communication with NXP Semiconductors PN5180 module for Arduino.
//
// Copyright (c) 2018 by Andreas Trappmann. All rights reserved.
//
// This file is part of the PN5180 library for the Arduino environment.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
#ifndef PN5180_H
#define PN5180_H

#include <SPI.h>

// PN5180 Registers
#define SYSTEM_CONFIG       (0x00)
#define IRQ_ENABLE          (0x01)
#define IRQ_STATUS          (0x02)
#define IRQ_CLEAR           (0x03)
#define TRANSCEIVE_CONTROL  (0x04)
#define TIMER1_RELOAD       (0x0c)
#define TIMER1_CONFIG       (0x0f)
#define RX_WAIT_CONFIG      (0x11)
#define CRC_RX_CONFIG       (0x12)
#define RX_STATUS           (0x13)
#define TX_WAIT_CONFIG      (0x17)
#define TX_CONFIG           (0x18)
#define CRC_TX_CONFIG       (0x19)
#define RF_STATUS           (0x1d)
#define SYSTEM_STATUS       (0x24)
#define TEMP_CONTROL        (0x25)
#define AGC_REF_CONFIG      (0x26)


// PN5180 EEPROM Addresses
#define DIE_IDENTIFIER      (0x00)
#define PRODUCT_VERSION     (0x10)
#define FIRMWARE_VERSION    (0x12)
#define EEPROM_VERSION      (0x14)
#define IRQ_PIN_CONFIG      (0x1A)

//PN5180 EEPROM Addresses - LPCD (Low Power Card Detection)
#define DPC_XI              (0x5C) // DPC AGC Trim Value

#define LPCD_REFERENCE_VALUE            (0x34)  // LPCD Gear number
#define LPCD_FIELD_ON_TIME              (0x36)  // LPCD RF on time (μs) = 62 + (8 * LPCD_FIELD_ON_TIME)
#define LPCD_THRESHOLD                  (0x37)  // LPCD wakes up if current AGC > AGC reference + LCPD_THRESHOLD (03..08: very sensitive, 40..50: very robust)
#define LPCD_REFVAL_GPO_CONTROL         (0x38)  // LPCD Reference Value Selectopn and GPO control
#define LPCD_GPO_TOGGLE_BEFORE_FIELD_ON (0x39)  // 
#define LPCD_GPO_TOGGLE_AFTER_FIELD_ON  (0x3A)  // 

enum PN5180TransceiveStat {
  PN5180_TS_Idle = 0,
  PN5180_TS_WaitTransmit = 1,
  PN5180_TS_Transmitting = 2,
  PN5180_TS_WaitReceive = 3,
  PN5180_TS_WaitForData = 4,
  PN5180_TS_Receiving = 5,
  PN5180_TS_LoopBack = 6,
  PN5180_TS_RESERVED = 7
};

// PN5180 IRQ_STATUS
#define RX_IRQ_STAT         	(1<<0)  // End of RF receiption IRQ
#define TX_IRQ_STAT         	(1<<1)  // End of RF transmission IRQ
#define IDLE_IRQ_STAT       	(1<<2)  // IDLE IRQ
#define RFOFF_DET_IRQ_STAT  	(1<<6)  // RF Field OFF detection IRQ
#define RFON_DET_IRQ_STAT   	(1<<7)  // RF Field ON detection IRQ
#define TX_RFOFF_IRQ_STAT   	(1<<8)  // RF Field OFF in PCD IRQ
#define TX_RFON_IRQ_STAT    	(1<<9)  // RF Field ON in PCD IRQ
#define RX_SOF_DET_IRQ_STAT 	(1<<14) // RF SOF Detection IRQ
#define GENERAL_ERROR_IRQ_STAT 	(1<<17) // General error IRQ
#define LPCD_IRQ_STAT 			(1<<19) // LPCD Detection IRQ

#define MIFARE_CLASSIC_KEYA 0x60  // Mifare Classic key A
#define MIFARE_CLASSIC_KEYB 0x61  // Mifare Classic key B

class PN5180 {
private:
  uint8_t PN5180_NSS;   // active low
  uint8_t PN5180_BUSY;
  uint8_t PN5180_RST;
  SPIClass& PN5180_SPI;
  int8_t PN5180_SCK;
  int8_t PN5180_MISO;
  int8_t PN5180_MOSI;

  SPISettings SPI_SETTINGS;
  static uint8_t readBufferStatic16[16];
  uint8_t* readBufferDynamic508 = NULL;
public:
  PN5180(uint8_t SSpin, uint8_t BUSYpin, uint8_t RSTpin, SPIClass& spi=SPI);
  ~PN5180();

  void begin(int8_t sck=-1, int8_t miso=-1, int8_t mosi=-1, int8_t SSpin=-1);
  void end();
  void setSPISettingsFrecuency(uint32_t frecuency);

  /*
   * PN5180 direct commands with host interface
   */
public:
  /* cmd 0x00 */
  bool writeRegister(uint8_t reg, uint32_t value);
  /* cmd 0x01 */
  bool writeRegisterWithOrMask(uint8_t addr, uint32_t mask);
  /* cmd 0x02 */
  bool writeRegisterWithAndMask(uint8_t addr, uint32_t mask);

  /* cmd 0x04 */
  bool readRegister(uint8_t reg, uint32_t *value);

  /* cmd 0x06 */
  bool writeEEprom(uint8_t addr, const uint8_t *buffer, uint8_t len);
  /* cmd 0x07 */
  bool readEEprom(uint8_t addr, uint8_t *buffer, int len);

  /* cmd 0x09 */
  bool sendData(const uint8_t *data, int len, uint8_t validBits = 0);
  /* cmd 0x0a */
  uint8_t * readData(int len);
  bool readData(int len, uint8_t *buffer);
  /* prepare LPCD registers */
  bool prepareLPCD();
  /* cmd 0x0B */
  bool switchToLPCD(uint16_t wakeupCounterInMs);
  /* cmd 0x0C */
  int16_t mifareAuthenticate(uint8_t blockno, const uint8_t *key, uint8_t keyType, const uint8_t *uid);
  /* cmd 0x11 */
  bool loadRFConfig(uint8_t txConf, uint8_t rxConf);

  /* cmd 0x16 */
  bool setRF_on();
  /* cmd 0x17 */
  bool setRF_off();

  bool sendCommand(uint8_t *sendBuffer, size_t sendBufferLen, uint8_t *recvBuffer, size_t recvBufferLen);

  /*
   * Helper functions
   */
public:
  void reset();

  uint16_t commandTimeout = 500;
  uint32_t getIRQStatus();
  bool clearIRQStatus(uint32_t irqMask);

  PN5180TransceiveStat getTransceiveState();

  /*
   * Private methods, called within an SPI transaction
   */
private:
  bool transceiveCommand(uint8_t *sendBuffer, size_t sendBufferLen, uint8_t *recvBuffer = 0, size_t recvBufferLen = 0);

};

#endif /* PN5180_H */
