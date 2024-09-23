#include <Arduino.h>

#include "Common.h"

// Base64 decoder adapted from https://stackoverflow.com/a/37109258
static const int B64index[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 63, 62, 62, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 63, 0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

/// @brief decodes a base64 string
/// @param input_buffer pointer to the base64 string buffer
/// @param output_buffer pointer to the output buffer (can be the same as the input buffer)
/// @param input_length length of the base64 string (without \0 terminator byte)
/// @return number of bytes written to the output buffer
size_t b64decode(const void *input_buffer, void *output_buffer, const size_t input_length) {
	if (input_length == 0) {
		return 0;
	}

	unsigned char *p = (unsigned char *) input_buffer;
	size_t j = 0, pad1 = input_length % 4 || p[input_length - 1] == '=', pad2 = pad1 && (input_length % 4 > 2 || p[input_length - 2] != '=');
	const size_t last = (input_length - pad1) / 4 << 2;
	// size_t output_size = last / 4 * 3 + pad1 + pad2;
	unsigned char *str = (unsigned char *) output_buffer;

	for (size_t i = 0; i < last; i += 4) {
		int n = B64index[p[i]] << 18 | B64index[p[i + 1]] << 12 | B64index[p[i + 2]] << 6 | B64index[p[i + 3]];
		str[j++] = n >> 16;
		str[j++] = n >> 8 & 0xFF;
		str[j++] = n & 0xFF;
	}
	if (pad1) {
		int n = B64index[p[last]] << 18 | B64index[p[last + 1]] << 12;
		str[j++] = n >> 16;
		if (pad2) {
			n |= B64index[p[last + 2]] << 6;
			str[j++] = n >> 8 & 0xFF;
		}
	}

	return j;
}
