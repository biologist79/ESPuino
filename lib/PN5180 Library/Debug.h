// NAME: Debug.h
//
// DESC: Helper methods for debugging
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
#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG
extern uint8_t _pn5180_debugIndent;
extern uint8_t _pn5180_debugIndentN;
extern bool _pn5180_debugNL;
extern uint8_t _pn5180_debugSilent;

// These macros are helper macros to make the see the debug macros like function calls so they do not alter any current code block structures when the macro contains if/else/break, etc.
#define __DEBUG_BEGIN__   do{if(DEBUG){
#define __DEBUG_END__     }}while(0)

// DEBUG print with indention macros:
//
// USAGE:
//   1. At the top of any function use PN5180DEBUG_PRINTLN(...) or PN5180DEBUG_PRINTF(...)or PN5180DEBUG_PRINT(...) to output that you've entered the function.
//   2. If you didn't use PN5180DEBUG_PRINTLN, then follow it with PN5180DEBUG_PRINTLN() without parameters.
//   3. Use PN5180DEBUG_ENTER, which will increment the indent level.
//   4. Before every return call in the method, use PN5180DEBUG_EXIT to decrement the indent level.
//   5. Use PN5180DEBUG_OFF and PN5180DEBUG_ON to block debug output for a call tree.
//      You can see this used in the PN5180::setRF_on() which blcks the thousands repeated debug messages to readRegister() and tranceiveCommand() functions while allowing those functions to continue to issue debug messages when called from other functions.
//
// NOTES:
//   1. Always make sure you end any secuence of _PRINT* macros with a _PRINTNL. See example below for putputting the register value.
//   2. If you directly use PN5180DEBUG_INDENT, it will break the indention macros.
//   3. NOTE: The older PN5180DEBUG macro is kept for backward compatibility.
//   4. Code that uses Serial.print() function calls outside these macros will not have a prepended "| " at the beginning of each line, which helps you identify the code being debugged from other code.
//
// EXAMPLE: (from PN5180.cpp)
//   bool PN5180::readRegister(uint8_t reg, uint32_t *value) {
//     PN5180DEBUG_PRINTF(F("PN5180::readRegister(reg=0x%s, *value=%d"), formatHex(reg), (uint8_t*)value);
//     PN5180DEBUG_PRINTLN();
//     PN5180DEBUG_ENTER;
//   
//     uint8_t cmd[] = { PN5180_READ_REGISTER, reg };
// 
//     transceiveCommand(cmd, sizeof(cmd), (uint8_t*)value, 4);
// 
//     PN5180DEBUG(F("Register value=0x"));
//     PN5180DEBUG(formatHex(*value));
//     PN5180DEBUG_PRINTLN();
// 
//     PN5180DEBUG_EXIT;
//     return true;
//   }
//
// SAMPLE OUTPUT using PN5180 library
// ==================================
// Uploaded: Oct 28 2024 04:22:04
// PN5180 ISO14443 Demo Sketch
// | PN5180::begin(sck=19, miso=20, mosi=18, ss=-1)
// |  Custom SPI pinout: SS=21, MOSI=18, MISO=20, SCK=19
// ----------------------------------
// PN5180 Hard-Reset...
// | PN5180::reset()
// |  PN5180::getIRQStatus()
// |   Read IRQ-Status register...
// |   PN5180::readRegister(reg=0x02, *value=1082650700
// |    PN5180::transceiveCommand(*sendBuffer, sendBufferLen=2, *recvBuffer, recvBufferLen=4)
// |     Sending SPI frame: '04 02'
// |     Receiving SPI frame...
// |     Received: '04 00 00 00'
// |    Register value=0x00000004
// |   IRQ-Status=0x00000004
// ----------------------------------

#define PN5180DEBUG_OFF          __DEBUG_BEGIN__ ++_pn5180_debugSilent; __DEBUG_END__
#define PN5180DEBUG_ON           __DEBUG_BEGIN__ _pn5180_debugSilent-=((_pn5180_debugSilent>0)?1:0); __DEBUG_END__
#define PN5180DEBUG_ENTER        __DEBUG_BEGIN__ ++_pn5180_debugIndent; __DEBUG_END__
#define PN5180DEBUG_EXIT         __DEBUG_BEGIN__ _pn5180_debugIndent-=((_pn5180_debugIndent>0)?1:0); __DEBUG_END__
#define PN5180DEBUG_INDENT       __DEBUG_BEGIN__ Serial.print("| "); for (int _pn5180_debugIndentN=0; _pn5180_debugIndentN<_pn5180_debugIndent; _pn5180_debugIndentN++) Serial.print(" "); _pn5180_debugNL=false; __DEBUG_END__
#define PN5180DEBUG_PRINTLN(...) __DEBUG_BEGIN__ if (!_pn5180_debugSilent) { if (_pn5180_debugNL) PN5180DEBUG_INDENT; Serial.println(__VA_ARGS__); _pn5180_debugNL=true; }; __DEBUG_END__
#define PN5180DEBUG_PRINTF(...)  __DEBUG_BEGIN__ if (!_pn5180_debugSilent) { if (_pn5180_debugNL) PN5180DEBUG_INDENT; Serial.printf(__VA_ARGS__); }; __DEBUG_END__
#define PN5180DEBUG_PRINT(...)   __DEBUG_BEGIN__ if (!_pn5180_debugSilent) { if (_pn5180_debugNL) PN5180DEBUG_INDENT; Serial.print(__VA_ARGS__); }; __DEBUG_END__
#define PN5180DEBUG(msg)         PN5180DEBUG_PRINT(msg)
#else
#define PN5180DEBUG_OFF
#define PN5180DEBUG_ON
#define PN5180DEBUG_ENTER
#define PN5180DEBUG_EXIT
#define PN5180DEBUG_INDENT
#define PN5180DEBUG_PRINTLN(...)
#define PN5180DEBUG_PRINTF(...)
#define PN5180DEBUG_PRINT(...)
#define PN5180DEBUG(msg)
#endif

#ifdef DEBUG
extern char * formatHex(const uint8_t val);
extern char * formatHex(const uint16_t val);
extern char * formatHex(const uint32_t val);
#endif

#endif /* DEBUG_H */
