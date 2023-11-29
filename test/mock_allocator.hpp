#pragma once

#include <Arduino.h>

// mock allocator
struct UnitTestAllocator {
	void *allocate(size_t size) {
		void *ret;
		if (psramInit()) {
			ret = ps_malloc(size);
		} else {
			ret = malloc(size);
		}
		if (ret) {
			allocCount++;
		}
		return ret;
	}

	void deallocate(void *ptr) {
		free(ptr);
		deAllocCount++;
	}

	void *reallocate(void *ptr, size_t new_size) {
		void *ret;
		if (psramInit()) {
			ret = ps_realloc(ptr, new_size);
		} else {
			ret = realloc(ptr, new_size);
		}
		if (ret) {
			reAllocCount++;
		}
		return ret;
	}
};