#pragma once

char *x_calloc(uint32_t _allocSize, uint32_t _unitSize);
char *x_malloc(uint32_t _allocSize);
char *x_strdup(const char *_str);
char *x_strndup(const char *_str, uint32_t _len);