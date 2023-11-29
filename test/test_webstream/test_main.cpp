#include <Arduino.h>

#include "../mock_allocator.hpp"
#include "playlists/WebstreamPlaylist.hpp"

#include <FS.h>
#include <unity.h>

size_t allocCount = 0;
size_t deAllocCount = 0;
size_t reAllocCount = 0;

size_t heap;
size_t psram;

constexpr const char *webStream = "http://test.com/stream.mp3";
WebstreamPlaylistAlloc<UnitTestAllocator> *webPlaylist;

void setUp(void) {
	// set stuff up here, this function is before a test function
}

void tearDown(void) {
}

void setup_static(void) {
	webPlaylist = new WebstreamPlaylistAlloc<UnitTestAllocator>();
}

void get_free_memory(void) {
	heap = ESP.getFreeHeap();
	psram = ESP.getFreePsram();
}

void test_webstream_alloc(void) {
	webPlaylist->setUrl(webStream);

	TEST_ASSERT_EQUAL_MESSAGE(1, allocCount, "Calls to malloc");
	TEST_ASSERT_EQUAL_MESSAGE(0, deAllocCount, "Calls to free");
	TEST_ASSERT_EQUAL_MESSAGE(0, reAllocCount, "Calls to realloc");
}

void test_webstream_content(void) {
	TEST_ASSERT_EQUAL_STRING_MESSAGE(webStream, webPlaylist->getAbsolutePath().c_str(), "Test if stored value equal constructor value");
}

void test_webstream_change(void) {
	const char *newStream = "http://test2.com/stream.mp3";
	webPlaylist->setUrl(newStream);
	TEST_ASSERT_EQUAL_STRING_MESSAGE(newStream, webPlaylist->getAbsolutePath().c_str(), "Test if stored value equal constructor value");

	// test memory actions
	TEST_ASSERT_EQUAL_MESSAGE(2, allocCount, "Calls to malloc");
	TEST_ASSERT_EQUAL_MESSAGE(1, deAllocCount, "Calls to free");
	TEST_ASSERT_EQUAL_MESSAGE(0, reAllocCount, "Calls to realloc");
}

void test_webstream_dealloc(void) {
	delete webPlaylist;

	TEST_ASSERT_EQUAL_MESSAGE(2, allocCount, "Calls to malloc");
	TEST_ASSERT_EQUAL_MESSAGE(2, deAllocCount, "Calls to free");
	TEST_ASSERT_EQUAL_MESSAGE(0, reAllocCount, "Calls to realloc");
}

void test_free_memory(void) {
	size_t cHeap = ESP.getFreeHeap();
	size_t cPsram = ESP.getFreePsram();

	TEST_ASSERT_INT_WITHIN_MESSAGE(4, heap, cHeap, "Free heap after test (delta = 4 byte)");
	TEST_ASSERT_INT_WITHIN_MESSAGE(4, psram, cPsram, "Free psram after test (delta = 4 byte)");
}

void setup() {
	delay(2000); // service delay
	UNITY_BEGIN();

	get_free_memory();
	setup_static();

	RUN_TEST(test_webstream_alloc);
	RUN_TEST(test_webstream_content);
	RUN_TEST(test_webstream_change);
	RUN_TEST(test_webstream_dealloc);
	RUN_TEST(test_free_memory);

	UNITY_END(); // stop unit testing
}

void loop() {
}
