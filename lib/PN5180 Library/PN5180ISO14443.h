// NAME: PN5180ISO14443.h
//
// DESC: ISO14443 protocol on NXP Semiconductors PN5180 module for Arduino.
//
// Copyright (c) 2019 by Dirk Carstensen. All rights reserved.
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
#ifndef PN5180ISO14443_H
#define PN5180ISO14443_H

#include "PN5180.h"


class PN5180ISO14443 : public PN5180 {

public:
  PN5180ISO14443(uint8_t SSpin, uint8_t BUSYpin, uint8_t RSTpin, SPIClass& spi=SPI);
  
private:
  uint16_t rxBytesReceived();
  uint32_t GetNumberOfBytesReceivedAndValidBits();
public:
  // Mifare TypeA
  int8_t activateTypeA(uint8_t *buffer, uint8_t kind);
  bool mifareBlockRead(uint8_t blockno,uint8_t *buffer);
  uint8_t mifareBlockWrite16(uint8_t blockno, const uint8_t *buffer);
  bool mifareHalt();
  /*
   * Helper functions
   */
public:   
  bool setupRF();
  int8_t readCardSerial(uint8_t *buffer);    
  bool isCardPresent();    
};

#endif /* PN5180ISO14443_H */
