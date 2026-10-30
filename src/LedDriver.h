#pragma once

// Small driver for WS2812-type LEDs (replaces FastLED). Only covers what Led.cpp needs:
// - CRGB/CRGBSet with the same semantics as FastLED's, so the animations stay unchanged
// - global brightness + color correction, computed exactly like FastLED (no dithering)
// - output via SPI+DMA (preferred) or RMT (fallback, e.g. if the SPI host is used by the SD card)
// The HSV conversion is a port of FastLED's hsv2rgb_rainbow() (MIT license, (c) FastLED contributors).

#include <driver/rmt_tx.h>
#include <driver/spi_master.h>
#include <stdint.h>

// Color orders, same octal encoding as FastLED (byte n of the output = channel at digit n).
enum EOrder : uint16_t {
	RGB = 0012,
	RBG = 0021,
	GRB = 0102,
	GBR = 0120,
	BRG = 0201,
	BGR = 0210
};

// Supported chipsets: all of them use the 800 kHz WS2812 timing.
enum LedChipset : uint8_t {
	WS2812,
	WS2812B,
	WS2813,
	WS2815,
	SK6812,
	NEOPIXEL
};

enum LedColorCorrection : uint32_t {
	TypicalSMD5050 = 0xFFB0F0,
	UncorrectedColor = 0xFFFFFF
};

struct CRGB {
	union {
		struct {
			uint8_t r;
			uint8_t g;
			uint8_t b;
		};
		uint8_t raw[3];
	};

	// Only the colors ESPuino uses; values match FastLED/HTML. 32 bit wide as these are stored in NVS.
	typedef enum : uint32_t {
		Black = 0x000000,
		Blue = 0x0000FF,
		BlueViolet = 0x8A2BE2,
		DeepSkyBlue = 0x00BFFF,
		Green = 0x008000,
		Orange = 0xFFA500,
		Red = 0xFF0000,
		White = 0xFFFFFF,
		Yellow = 0xFFFF00
	} HTMLColorCode;

	CRGB()
		: r(0)
		, g(0)
		, b(0) { }
	CRGB(uint8_t red, uint8_t green, uint8_t blue)
		: r(red)
		, g(green)
		, b(blue) { }
	CRGB(uint32_t colorCode)
		: r((colorCode >> 16) & 0xFF)
		, g((colorCode >> 8) & 0xFF)
		, b(colorCode & 0xFF) { }

	// Scales all channels by (scale + 1) / 256, like FastLED's nscale8()
	CRGB &nscale8(uint8_t scale);
	// Full saturation and value
	CRGB &setHue(uint8_t hue) { return setHSV(hue, 255, 255); }
	CRGB &setHSV(uint8_t hue, uint8_t sat, uint8_t val);

	bool operator==(const CRGB &rhs) const { return r == rhs.r && g == rhs.g && b == rhs.b; }
	bool operator!=(const CRGB &rhs) const { return !(*this == rhs); }
};

// A view on a range of LEDs (like FastLED's CRGBSet): index access, size() and fill by assignment
class CRGBSet {
public:
	CRGBSet(CRGB *leds, int numLeds)
		: mLeds(leds)
		, mNumLeds(numLeds) { }

	CRGB &operator[](int index) { return mLeds[index]; }
	int size() const { return mNumLeds; }
	CRGBSet &operator=(const CRGB &color) {
		for (int i = 0; i < mNumLeds; i++) {
			mLeds[i] = color;
		}
		return *this;
	}

private:
	CRGB *mLeds;
	int mNumLeds;
};

class LedDriver {
public:
	static constexpr bool isSupported(LedChipset chipset) { return chipset <= NEOPIXEL; }

	// leds must stay valid as long as the driver is used. With preferSpi, the SPI host Arduino calls
	// HSPI is used (must not be in use by anything else), otherwise or if that fails RMT.
	bool begin(uint8_t pin, EOrder order, CRGB *leds, uint16_t numLeds, bool preferSpi);

	void setBrightness(uint8_t brightness);
	uint8_t getBrightness() const { return mBrightness; }
	void setCorrection(uint32_t correction);
	// Limits how often show() actually starts a transfer; 0 disables the limit
	void setMaxRefreshRate(uint16_t hz) { mMinIntervalUs = hz ? (1000000UL / hz) : 0; }

	// Sends the buffer (asynchronously; returns while the transfer is still running)
	void show();
	// Sets all LEDs to black; with writeData the strip is updated and the transfer completed
	void clear(bool writeData = false);
	bool usesSpi() const { return mBackend == Backend::Spi; }

private:
	enum class Backend : uint8_t {
		None,
		Spi,
		Rmt
	};

	bool beginSpi(uint8_t pin);
	bool beginRmt(uint8_t pin);
	void transmit(bool force);
	void waitDone();
	void updateAdjustment();

	CRGB *mLeds = nullptr;
	uint16_t mNumLeds = 0;
	Backend mBackend = Backend::None;
	uint8_t mChannel[3] = {0, 1, 2}; // channel for output byte 0..2
	uint8_t mBrightness = 255;
	uint32_t mCorrection = UncorrectedColor;
	uint8_t mAdjust[3] = {255, 255, 255}; // per channel scale (brightness * correction)
	uint32_t mMinIntervalUs = 0;
	uint32_t mLastShowUs = 0;
	uint32_t mLastSentMs = 0;
	bool mTransferPending = false;
	uint8_t *mFrame = nullptr; // scaled and ordered bytes of the last frame (only read by the CPU)
	uint8_t *mRmtBuffer = nullptr; // copy of mFrame being sent via RMT
	uint8_t *mSpiBuffer = nullptr; // frame encoded as SPI bit stream (DMA capable)
	size_t mSpiBufferLen = 0;
	spi_device_handle_t mSpiDevice = nullptr;
	spi_transaction_t mSpiTransaction = {};
	rmt_channel_handle_t mRmtChannel = nullptr;
	rmt_encoder_handle_t mRmtEncoder = nullptr;
};
