#pragma once
#include "logmessages.h"

// Loglevels available (don't change!)
#define LOGLEVEL_ERROR 1  // only errors
#define LOGLEVEL_NOTICE 2 // errors + important messages
#define LOGLEVEL_INFO 3   // infos + errors + important messages
#define LOGLEVEL_DEBUG 4  // almost everything

extern uint8_t Log_BufferLength;
extern char *Log_Buffer; // Buffer for all log-messages

/* Wrapper-function for serial-logging (with newline)
   _logBuffer: char* to log
   _minLogLevel: loglevel configured for this message.
   If (_currentLogLevel <= _minLogLevel) message will be logged
*/
void Log_Println(const char *_logBuffer, const uint8_t _minLogLevel);

/* Wrapper-function for serial-logging (without newline) */
void Log_Print(const char *_logBuffer, const uint8_t _minLogLevel, bool printTimestamp);

void Log_Init(void);
String Log_GetRingBuffer(void);
