#include <Arduino.h>

#include "MemX.h"

// Wraps strdup(). Without PSRAM, strdup is called => so heap is used.
// With PSRAM being available, the same is done what strdup() does, but with allocation on PSRAM.
char *x_strdup(const char *_str) {
	if (!psramInit()) {
		return strdup(_str);
	} else {
		char *dst = (char *) ps_malloc(strlen(_str) + 1);
		if (dst == NULL) {
			return NULL;
		}
		strcpy(dst, _str);
		return dst;
	}
}

// Wraps ps_malloc() and malloc(). Selection depends on whether PSRAM is available or not.
char *x_malloc(uint32_t _allocSize) {
	if (psramInit()) {
		return (char *) ps_malloc(_allocSize);
	} else {
		return (char *) malloc(_allocSize);
	}
}

// Wraps ps_calloc() and calloc(). Selection depends on whether PSRAM is available or not.
char *x_calloc(uint32_t _allocSize, uint32_t _unitSize) {
	if (psramInit()) {
		return (char *) ps_calloc(_allocSize, _unitSize);
	} else {
		return (char *) calloc(_allocSize, _unitSize);
	}
}
