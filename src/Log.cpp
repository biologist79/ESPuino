#include <Arduino.h>
#include "settings.h"

#include "Log.h"

#include "LogRingBuffer.h"
#include "MemX.h"

static LogRingBuffer *Log_RingBuffer = NULL;

void Log_Init(void) {
	Serial.begin(115200);
	Log_RingBuffer = new LogRingBuffer();
}

String getLoglevel(const uint8_t logLevel) {
	switch (logLevel) {
		case LOGLEVEL_ERROR:
			return "E";
		case LOGLEVEL_NOTICE:
			return "N";
		case LOGLEVEL_INFO:
			return "I";
		case LOGLEVEL_DEBUG:
			return "D";
		default:
			return " ";
	}
}

/* Wrapper-function for serial-logging (with newline)
   _logBuffer: char* to log
   _minLogLevel: loglevel configured for this message.
   If (SERIAL_LOGLEVEL <= _minLogLevel) message will be logged
*/
void Log_Println(const char *_logBuffer, const uint8_t _minLogLevel) {
	if (SERIAL_LOGLEVEL >= _minLogLevel) {
		uint32_t ctime = millis();
		const String sLogLevel = getLoglevel(_minLogLevel);
		Serial.printf("%s [%" PRIu32 "] ", sLogLevel.c_str(), ctime);
		Serial.println(_logBuffer);
		Log_RingBuffer->printf("%s [%" PRIu32 "] ", sLogLevel.c_str(), ctime);
		Log_RingBuffer->println(_logBuffer);
	}
}

/* Wrapper-function for serial-logging (without newline) */
void Log_Print(const char *_logBuffer, const uint8_t _minLogLevel, bool printTimestamp) {
	if (SERIAL_LOGLEVEL >= _minLogLevel) {
		if (printTimestamp) {
			uint32_t ctime = millis();
			const String sLogLevel = getLoglevel(_minLogLevel);
			Serial.printf("%s [%" PRIu32 "] ", sLogLevel.c_str(), ctime);
			Serial.print(_logBuffer);
			Log_RingBuffer->printf("%s [%" PRIu32 "] ", sLogLevel.c_str(), ctime);
		} else {
			Serial.print(_logBuffer);
		}
		Log_RingBuffer->print(_logBuffer);
	}
}

int Log_Printf(const uint8_t _minLogLevel, const char *format, ...) {
	char loc_buf[201]; // Allow a maximum buffer of 200 characters in a single log message

	int len;
	va_list arg;
	va_start(arg, format);

	// use the local buffer and trunctate string if it's larger
	len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);

	Log_Print(loc_buf, _minLogLevel, true);
	if (len > sizeof(loc_buf) - 1) {
		// long string was trunctated
		Log_Print("...", _minLogLevel, false);
	}
	Log_Print("\n", _minLogLevel, false);

	va_end(arg);

	return std::min<int>(len, sizeof(loc_buf) - 1);
}

String Log_GetRingBuffer(void) {
	return Log_RingBuffer->get();
}
