#pragma once

constexpr char stringDelimiter[] = "#"; // Character used to encapsulate data in linear NVS-strings (don't change)
constexpr char stringOuterDelimiter[] = "^"; // Character used to encapsulate encapsulated data along with RFID-ID in backup-file

size_t b64decode(const void *input_buffer, void *output_buffer, const size_t input_length);

inline bool isNumber(const char *str) {
	int i = 0;

	while (*(str + i) != '\0') {
		if (!isdigit(*(str + i++))) {
			return false;
		}
	}

	if (i > 0) {
		return true;
	} else {
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

inline void convertAsciiToUtf8(String asciiString, char *utf8String, size_t utf8StringSize) {

	int k = 0;

	for (int i = 0; i < asciiString.length() && k < utf8StringSize - 2; i++) {

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
