// NAME: Debug.cpp
//
// DESC: Helper functions for debugging
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
#include <inttypes.h>
#include "Debug.h"

uint8_t _pn5180_debugIndent;
uint8_t _pn5180_debugIndentN;
bool _pn5180_debugNL = true;
uint8_t _pn5180_debugSilent;

#ifdef DEBUG

static const char hexChar[] = "0123456789ABCDEF";
static char hexBuffer[9];

char * formatHex(const uint8_t val) {
  hexBuffer[0] = hexChar[val >> 4];
  hexBuffer[1] = hexChar[val & 0x0f];
  hexBuffer[2] = '\0';
  return hexBuffer;
}

char * formatHex(const uint16_t val) {
  hexBuffer[0] = hexChar[(val >> 12) & 0x0f];
  hexBuffer[1] = hexChar[(val >> 8) & 0x0f];
  hexBuffer[2] = hexChar[(val >> 4) & 0x0f];
  hexBuffer[3] = hexChar[val & 0x0f];
  hexBuffer[4] = '\0';
  return hexBuffer;
}

char * formatHex(uint32_t val) {
  for (int i=7; i>=0; i--) {
    hexBuffer[i] = hexChar[val & 0x0f];
    val = val >> 4;
  }
  hexBuffer[8] = '\0';
  return hexBuffer;
}

#endif
