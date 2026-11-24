#include <Arduino.h>
#include "settings.h"

#include "Log.h"

#include "MemX.h"

#ifndef LOG_BUFFER_SIZE
	#define LOG_BUFFER_SIZE 2048u // default ring buffer size
#endif

// Ring buffer for the log shown in the web interface. It is written by tasks on both cores and read by
// the web server, so every access holds Log_Mutex. A task suspended while holding it (e.g. Led_Task
// during an upload) must not block all logging: if the mutex can't be taken in time, the message only
// goes to the serial console.
static char *Log_Buffer = nullptr;
static size_t Log_BufferIndex = 0;
static bool Log_BufferFull = false;
static SemaphoreHandle_t Log_Mutex = nullptr;
static constexpr TickType_t Log_MutexTimeout = pdMS_TO_TICKS(20);

void Log_Init(void) {
	Serial.begin(115200);
	Log_Buffer = static_cast<char *>(psramFound() ? ps_malloc(LOG_BUFFER_SIZE) : malloc(LOG_BUFFER_SIZE));
	Log_Mutex = xSemaphoreCreateMutex();
}

static const char *getLoglevel(const uint8_t logLevel) {
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

// Caller must hold Log_Mutex
static void Log_AppendToBuffer(const char *text) {
	for (; *text; text++) {
		Log_Buffer[Log_BufferIndex++] = *text;
		if (Log_BufferIndex >= LOG_BUFFER_SIZE) {
			Log_BufferIndex = 0;
			Log_BufferFull = true;
		}
	}
}

// Writes one message (optionally with timestamp, followed by suffix) to serial and, in one piece, to the ring buffer
static void Log_Write(const char *text, const uint8_t _minLogLevel, bool printTimestamp, const char *suffix) {
	char header[24] = "";
	if (printTimestamp) {
		snprintf(header, sizeof(header), "%s [%" PRIu32 "] ", getLoglevel(_minLogLevel), millis());
	}
	Serial.print(header);
	Serial.print(text);
	Serial.print(suffix);

	if ((Log_Buffer == nullptr) || (Log_Mutex == nullptr) || xPortInIsrContext()) {
		return;
	}
	if (xSemaphoreTake(Log_Mutex, Log_MutexTimeout) != pdTRUE) {
		return;
	}
	Log_AppendToBuffer(header);
	Log_AppendToBuffer(text);
	Log_AppendToBuffer(suffix);
	xSemaphoreGive(Log_Mutex);
}

/* Wrapper-function for serial-logging (with newline)
   _logBuffer: char* to log
   _minLogLevel: loglevel configured for this message.
   If (SERIAL_LOGLEVEL <= _minLogLevel) message will be logged
*/
void Log_Println(const char *_logBuffer, const uint8_t _minLogLevel) {
	if (SERIAL_LOGLEVEL >= _minLogLevel) {
		Log_Write(_logBuffer, _minLogLevel, true, "\r\n");
	}
}

/* Wrapper-function for serial-logging (without newline) */
void Log_Print(const char *_logBuffer, const uint8_t _minLogLevel, bool printTimestamp) {
	if (SERIAL_LOGLEVEL >= _minLogLevel) {
		Log_Write(_logBuffer, _minLogLevel, printTimestamp, "");
	}
}

int Log_Printf(const uint8_t _minLogLevel, const char *format, ...) {
	char loc_buf[201]; // Allow a maximum buffer of 200 characters in a single log message

	int len;
	va_list arg;
	va_start(arg, format);

	// use the local buffer and trunctate string if it's larger
	len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
	va_end(arg);

	if (SERIAL_LOGLEVEL >= _minLogLevel) {
		// long string was trunctated
		Log_Write(loc_buf, _minLogLevel, true, (len > sizeof(loc_buf) - 1) ? "...\n" : "\n");
	}

	return std::min<int>(len, sizeof(loc_buf) - 1);
}

String Log_GetRingBuffer(void) {
	String logString;
	if ((Log_Buffer == nullptr) || (Log_Mutex == nullptr) || (xSemaphoreTake(Log_Mutex, Log_MutexTimeout) != pdTRUE)) {
		return logString;
	}
	// oldest part first: from the write position to the end (only once wrapped), then the start
	if (logString.reserve(Log_BufferFull ? LOG_BUFFER_SIZE : Log_BufferIndex)) {
		if (Log_BufferFull) {
			logString.concat(Log_Buffer + Log_BufferIndex, LOG_BUFFER_SIZE - Log_BufferIndex);
		}
		logString.concat(Log_Buffer, Log_BufferIndex);
	}
	xSemaphoreGive(Log_Mutex);
	return logString;
}
