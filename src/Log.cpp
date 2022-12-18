#include <Arduino.h>
#include "settings.h"
#include "Log.h"
#include "MemX.h"
#include "LogRingBuffer.h"

// Serial-logging buffer
uint8_t Log_BufferLength = 200;
char *Log_Buffer;

static LogRingBuffer *Log_RingBuffer = NULL;

void Log_Init(void){
	Serial.begin(115200);
	Log_RingBuffer = new LogRingBuffer();
	Log_Buffer = (char *) x_calloc(Log_BufferLength, sizeof(char)); // Buffer for all log-messages
}

/* Wrapper-function for serial-logging (with newline)
   _logBuffer: char* to log
   _minLogLevel: loglevel configured for this message.
   If (SERIAL_LOGLEVEL <= _minLogLevel) message will be logged
*/
void Log_Println(const char *_logBuffer, const uint8_t _minLogLevel) {
	if (SERIAL_LOGLEVEL >= _minLogLevel) {
		uint32_t ctime = millis();
		Serial.printf("[ %u ]  ", ctime);
		Serial.println(_logBuffer);
		Log_RingBuffer->print("[ ");
		Log_RingBuffer->print(ctime);
		Log_RingBuffer->print(" ]  ");
		Log_RingBuffer->println(_logBuffer);
	}
}

/* Wrapper-function for serial-logging (without newline) */
void Log_Print(const char *_logBuffer, const uint8_t _minLogLevel, bool printTimestamp) {
	if (SERIAL_LOGLEVEL >= _minLogLevel) {
		if (printTimestamp) {
			uint32_t ctime = millis();
			Serial.printf("[ %u ]  ", ctime);
			Serial.print(_logBuffer);
			Log_RingBuffer->print("[ ");
			Log_RingBuffer->print(ctime);
			Log_RingBuffer->print(" ]  ");
		} else {
			Serial.print(_logBuffer);

		}
		Log_RingBuffer->print(_logBuffer);
	}
}

String Log_GetRingBuffer(void) {
	return Log_RingBuffer->get();
}
