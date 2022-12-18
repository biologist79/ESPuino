#pragma once

// FilePathLength
#define MAX_FILEPATH_LENTGH 256

constexpr char stringDelimiter[] = "#";      // Character used to encapsulate data in linear NVS-strings (don't change)
constexpr char stringOuterDelimiter[] = "^"; // Character used to encapsulate encapsulated data along with RFID-ID in backup-file

inline bool isNumber(const char *str)
{
	byte i = 0;

	while (*(str + i) != '\0') {
		if (!isdigit(*(str + i++))) {
			return false;
		}
	}

	if (i > 0) {
		return true;
	} else{
		return false;
	}
}

// Checks if string starts with prefix
// Returns true if so
inline bool startsWith(const char *str, const char *pre) {
	if (strlen(pre) < 1) {
		return false;
	}

	return !strncmp(str, pre, strlen(pre));
}

// Checks if string ends with suffix
// Returns true if so
inline bool endsWith(const char *str, const char *suf) {
	const char *a = str + strlen(str);
	const char *b = suf + strlen(suf);

	while (a != str && b != suf) {
		if (*--a != *--b) {
			break;
		}
	}

	return b == suf && *a == *b;
}

inline void convertUtf8ToAscii(String utf8String, char *asciiString) {

	int k = 0;
	bool f_C3_seen = false;

	for (int i = 0; i < utf8String.length() && k < MAX_FILEPATH_LENTGH - 1; i++) {

		if (utf8String[i] == 195) { // C3
			f_C3_seen = true;
			continue;
		} else {
			if (f_C3_seen == true) {
				f_C3_seen = false;
				switch (utf8String[i]) {
					case 0x84:
						asciiString[k++] = 0x8e;
						break; // Ä
					case 0xa4:
						asciiString[k++] = 0x84;
						break; // ä
					case 0x9c:
						asciiString[k++] = 0x9a;
						break; // Ü
					case 0xbc:
						asciiString[k++] = 0x81;
						break; // ü
					case 0x96:
						asciiString[k++] = 0x99;
						break; // Ö
					case 0xb6:
						asciiString[k++] = 0x94;
						break; // ö
					case 0x9f:
						asciiString[k++] = 0xe1;
						break; // ß
					default:
						asciiString[k++] = 0xdb; // Unknown...
				}
			} else {
				asciiString[k++] = utf8String[i];
			}
		}
	}

	asciiString[k] = 0;
}

inline void convertAsciiToUtf8(String asciiString, char *utf8String) {

	int k = 0;

	for (int i = 0; i < asciiString.length() && k < MAX_FILEPATH_LENTGH - 2; i++) {

		switch (asciiString[i]) {
			case 0x8e:
				utf8String[k++] = 0xc3;
				utf8String[k++] = 0x84;
				break; // Ä
			case 0x84:
				utf8String[k++] = 0xc3;
				utf8String[k++] = 0xa4;
				break; // ä
			case 0x9a:
				utf8String[k++] = 0xc3;
				utf8String[k++] = 0x9c;
				break; // Ü
			case 0x81:
				utf8String[k++] = 0xc3;
				utf8String[k++] = 0xbc;
				break; // ü
			case 0x99:
				utf8String[k++] = 0xc3;
				utf8String[k++] = 0x96;
				break; // Ö
			case 0x94:
				utf8String[k++] = 0xc3;
				utf8String[k++] = 0xb6;
				break; // ö
			case 0xe1:
				utf8String[k++] = 0xc3;
				utf8String[k++] = 0x9f;
				break; // ß
			default:
				utf8String[k++] = asciiString[i];
		}
	}

	utf8String[k] = 0;
}

// Release previously allocated memory
inline void freeMultiCharArray(char **arr, const uint32_t cnt) {
	for (uint32_t i = 0; i <= cnt; i++) {
		/*snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(freePtr), *(arr+i));
		Log_Println(Log_Buffer, LOGLEVEL_DEBUG);*/
		free(*(arr + i));
	}
	*arr = NULL;
}
