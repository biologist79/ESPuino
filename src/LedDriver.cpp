#include <Arduino.h>
#include "settings.h"

#ifdef NEOPIXEL_ENABLE
	#include "LedDriver.h"
	#include "Log.h"

	#include <esp_heap_caps.h>

// SPI: 2.5 MHz, every LED bit is sent as 3 SPI bits (0 -> 100, 1 -> 110), i.e. 400/800 ns high time
static constexpr uint32_t spiClockHz = 2500000;
static constexpr uint8_t spiBytesPerColorByte = 3;
// RMT: 100 ns resolution, same waveform as SPI
static constexpr uint32_t rmtResolutionHz = 10000000;
static constexpr uint16_t rmtShortTicks = 4;
static constexpr uint16_t rmtLongTicks = 8;
// Unchanged frames are not sent again, but refreshed at least this often (heals a corrupted pixel)
static constexpr uint32_t refreshIntervalMs = 1000;
static constexpr TickType_t transferTimeout = pdMS_TO_TICKS(100);

	#if CONFIG_IDF_TARGET_ESP32
// Arduino's HSPI. The default SPI (RFID) uses VSPI; the SD card only uses HSPI in SPI mode.
static constexpr spi_host_device_t ledSpiHost = SPI2_HOST;
	#else
// Arduino's HSPI on S2/S3. The default SPI (RFID) uses FSPI (= SPI2).
static constexpr spi_host_device_t ledSpiHost = SPI3_HOST;
	#endif

// SPI bit pattern for each nibble (4 LED bits -> 12 SPI bits, MSB first)
struct SpiNibbleTable {
	uint16_t pattern[16];
	constexpr SpiNibbleTable()
		: pattern() {
		for (uint8_t nibble = 0; nibble < 16; nibble++) {
			uint16_t bits = 0;
			for (int8_t bit = 3; bit >= 0; bit--) {
				bits = (bits << 3) | (((nibble >> bit) & 1) ? 0b110 : 0b100);
			}
			pattern[nibble] = bits;
		}
	}
};
static constexpr SpiNibbleTable spiNibbles;

static inline uint8_t scale8(uint8_t value, uint8_t scale) {
	return (uint16_t(value) * (1 + uint16_t(scale))) >> 8;
}

static inline uint8_t scale8Video(uint8_t value, uint8_t scale) {
	return ((uint16_t(value) * scale) >> 8) + ((value && scale) ? 1 : 0);
}

CRGB &CRGB::nscale8(uint8_t scale) {
	r = scale8(r, scale);
	g = scale8(g, scale);
	b = scale8(b, scale);
	return *this;
}

// Port of FastLED's hsv2rgb_rainbow() (default settings: moderate yellow boost, no green scaling)
CRGB &CRGB::setHSV(uint8_t hue, uint8_t sat, uint8_t val) {
	const uint8_t offset8 = (hue & 0x1F) << 3;
	const uint8_t third = scale8(offset8, 256 / 3); // max 85
	const uint8_t twoThirds = scale8(offset8, (256 * 2) / 3); // max 170

	switch (hue >> 5) {
		case 0: // red -> orange
			r = 255 - third;
			g = third;
			b = 0;
			break;
		case 1: // orange -> yellow
			r = 171;
			g = 85 + third;
			b = 0;
			break;
		case 2: // yellow -> green
			r = 171 - twoThirds;
			g = 170 + third;
			b = 0;
			break;
		case 3: // green -> aqua
			r = 0;
			g = 255 - third;
			b = third;
			break;
		case 4: // aqua -> blue
			r = 0;
			g = 171 - twoThirds;
			b = 85 + twoThirds;
			break;
		case 5: // blue -> purple
			r = third;
			g = 0;
			b = 255 - third;
			break;
		case 6: // purple -> pink
			r = 85 + third;
			g = 0;
			b = 171 - third;
			break;
		default: // pink -> red
			r = 170 + third;
			g = 0;
			b = 85 - third;
			break;
	}

	if (sat != 255) {
		if (sat == 0) {
			r = g = b = 255;
		} else {
			const uint8_t desat = scale8Video(255 - sat, 255 - sat);
			const uint8_t satScale = 255 - desat;
			r = scale8(r, satScale) + desat;
			g = scale8(g, satScale) + desat;
			b = scale8(b, satScale) + desat;
		}
	}

	if (val != 255) {
		val = scale8Video(val, val);
		if (val == 0) {
			r = g = b = 0;
		} else {
			nscale8(val);
		}
	}
	return *this;
}

bool LedDriver::begin(uint8_t pin, EOrder order, CRGB *leds, uint16_t numLeds, bool preferSpi) {
	if ((mBackend != Backend::None) || (leds == nullptr) || (numLeds == 0)) {
		return false;
	}
	mLeds = leds;
	mNumLeds = numLeds;
	mChannel[0] = (order >> 6) & 0x3;
	mChannel[1] = (order >> 3) & 0x3;
	mChannel[2] = order & 0x3;

	mFrame = static_cast<uint8_t *>(heap_caps_calloc(numLeds * 3, 1, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
	if (mFrame == nullptr) {
		return false;
	}
	if (preferSpi && beginSpi(pin)) {
		mBackend = Backend::Spi;
	} else if (beginRmt(pin)) {
		mBackend = Backend::Rmt;
	} else {
		heap_caps_free(mFrame);
		mFrame = nullptr;
		Log_Println("LED: no output driver available", LOGLEVEL_ERROR);
		return false;
	}
	Log_Printf(LOGLEVEL_DEBUG, "LED: %u LEDs via %s", numLeds, usesSpi() ? "SPI" : "RMT");
	updateAdjustment();
	return true;
}

bool LedDriver::beginSpi(uint8_t pin) {
	// padded to whole words; the trailing zero bits just keep the line low
	mSpiBufferLen = (size_t(mNumLeds) * 3 * spiBytesPerColorByte + 3) & ~size_t(3);
	mSpiBuffer = static_cast<uint8_t *>(heap_caps_calloc(mSpiBufferLen, 1, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
	if (mSpiBuffer == nullptr) {
		return false;
	}

	spi_bus_config_t busConfig = {};
	busConfig.mosi_io_num = pin;
	busConfig.miso_io_num = -1;
	busConfig.sclk_io_num = -1;
	busConfig.quadwp_io_num = -1;
	busConfig.quadhd_io_num = -1;
	busConfig.max_transfer_sz = mSpiBufferLen;
	if (spi_bus_initialize(ledSpiHost, &busConfig, SPI_DMA_CH_AUTO) != ESP_OK) {
		heap_caps_free(mSpiBuffer);
		mSpiBuffer = nullptr;
		return false;
	}

	spi_device_interface_config_t deviceConfig = {};
	deviceConfig.clock_speed_hz = spiClockHz;
	deviceConfig.mode = 0;
	deviceConfig.spics_io_num = -1;
	deviceConfig.queue_size = 1;
	if (spi_bus_add_device(ledSpiHost, &deviceConfig, &mSpiDevice) != ESP_OK) {
		spi_bus_free(ledSpiHost);
		heap_caps_free(mSpiBuffer);
		mSpiBuffer = nullptr;
		return false;
	}
	// the LEDs are the only device on this host: keep the bus instead of locking it for every transfer
	spi_device_acquire_bus(mSpiDevice, portMAX_DELAY);
	return true;
}

bool LedDriver::beginRmt(uint8_t pin) {
	mRmtBuffer = static_cast<uint8_t *>(heap_caps_calloc(size_t(mNumLeds) * 3, 1, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
	if (mRmtBuffer == nullptr) {
		return false;
	}
	rmt_tx_channel_config_t channelConfig = {};
	channelConfig.gpio_num = static_cast<gpio_num_t>(pin);
	channelConfig.clk_src = RMT_CLK_SRC_DEFAULT;
	channelConfig.resolution_hz = rmtResolutionHz;
	channelConfig.trans_queue_depth = 1;
	#if SOC_RMT_SUPPORT_DMA
	channelConfig.mem_block_symbols = 256;
	channelConfig.flags.with_dma = true;
	#else
	// two memory blocks halve the number of refill interrupts
	channelConfig.mem_block_symbols = 2 * SOC_RMT_MEM_WORDS_PER_CHANNEL;
	#endif
	if (rmt_new_tx_channel(&channelConfig, &mRmtChannel) != ESP_OK) {
		channelConfig.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
		channelConfig.flags.with_dma = false;
		if (rmt_new_tx_channel(&channelConfig, &mRmtChannel) != ESP_OK) {
			heap_caps_free(mRmtBuffer);
			mRmtBuffer = nullptr;
			return false;
		}
	}

	rmt_bytes_encoder_config_t encoderConfig = {};
	encoderConfig.bit0.level0 = 1;
	encoderConfig.bit0.duration0 = rmtShortTicks;
	encoderConfig.bit0.level1 = 0;
	encoderConfig.bit0.duration1 = rmtLongTicks;
	encoderConfig.bit1.level0 = 1;
	encoderConfig.bit1.duration0 = rmtLongTicks;
	encoderConfig.bit1.level1 = 0;
	encoderConfig.bit1.duration1 = rmtShortTicks;
	encoderConfig.flags.msb_first = true;
	if ((rmt_new_bytes_encoder(&encoderConfig, &mRmtEncoder) != ESP_OK) || (rmt_enable(mRmtChannel) != ESP_OK)) {
		if (mRmtEncoder != nullptr) {
			rmt_del_encoder(mRmtEncoder);
			mRmtEncoder = nullptr;
		}
		rmt_del_channel(mRmtChannel);
		mRmtChannel = nullptr;
		heap_caps_free(mRmtBuffer);
		mRmtBuffer = nullptr;
		return false;
	}
	return true;
}

void LedDriver::setBrightness(uint8_t brightness) {
	mBrightness = brightness;
	updateAdjustment();
}

void LedDriver::setCorrection(uint32_t correction) {
	mCorrection = correction;
	updateAdjustment();
}

// Same as FastLED's computeAdjustment() with uncorrected color temperature
void LedDriver::updateAdjustment() {
	const CRGB correction(mCorrection);
	for (uint8_t channel = 0; channel < 3; channel++) {
		const uint8_t cc = correction.raw[channel];
		mAdjust[channel] = (mBrightness && cc) ? ((uint32_t(cc) + 1) * 256 * mBrightness) >> 16 : 0;
	}
}

void LedDriver::waitDone() {
	if (!mTransferPending) {
		return;
	}
	if (mBackend == Backend::Spi) {
		spi_transaction_t *transaction;
		spi_device_get_trans_result(mSpiDevice, &transaction, transferTimeout);
	} else {
		rmt_tx_wait_all_done(mRmtChannel, transferTimeout);
	}
	mTransferPending = false;
}

void LedDriver::transmit(bool force) {
	if (mBackend == Backend::None) {
		return;
	}
	// mFrame is never read by the hardware, so an unchanged frame returns here without any waiting
	bool changed = force;
	uint8_t *out = mFrame;
	for (uint16_t led = 0; led < mNumLeds; led++) {
		for (uint8_t slot = 0; slot < 3; slot++) {
			const uint8_t channel = mChannel[slot];
			const uint8_t value = scale8(mLeds[led].raw[channel], mAdjust[channel]);
			changed |= (*out != value);
			*out++ = value;
		}
	}
	if (!changed && ((millis() - mLastSentMs) < refreshIntervalMs)) {
		return;
	}

	if (mMinIntervalUs) {
		const uint32_t elapsedUs = micros() - mLastShowUs;
		if (elapsedUs < mMinIntervalUs) {
			vTaskDelay(pdMS_TO_TICKS((mMinIntervalUs - elapsedUs + 999) / 1000));
		}
	}
	// the send buffers are read by the hardware until the running transfer has finished
	waitDone();
	mLastShowUs = micros();
	mLastSentMs = millis();

	const size_t frameLen = size_t(mNumLeds) * 3;
	if (mBackend == Backend::Spi) {
		uint8_t *spi = mSpiBuffer;
		for (size_t i = 0; i < frameLen; i++) {
			const uint32_t bits = (uint32_t(spiNibbles.pattern[mFrame[i] >> 4]) << 12) | spiNibbles.pattern[mFrame[i] & 0x0F];
			*spi++ = bits >> 16;
			*spi++ = bits >> 8;
			*spi++ = bits;
		}
		memset(&mSpiTransaction, 0, sizeof(mSpiTransaction));
		mSpiTransaction.length = mSpiBufferLen * 8;
		mSpiTransaction.tx_buffer = mSpiBuffer;
		mTransferPending = (spi_device_queue_trans(mSpiDevice, &mSpiTransaction, transferTimeout) == ESP_OK);
	} else {
		memcpy(mRmtBuffer, mFrame, frameLen);
		rmt_transmit_config_t transmitConfig = {};
		mTransferPending = (rmt_transmit(mRmtChannel, mRmtEncoder, mRmtBuffer, frameLen, &transmitConfig) == ESP_OK);
	}
}

void LedDriver::show() {
	transmit(false);
}

void LedDriver::clear(bool writeData) {
	if (mLeds == nullptr) {
		return;
	}
	CRGBSet(mLeds, mNumLeds) = CRGB::Black;
	if (writeData) {
		transmit(true);
		waitDone();
	}
}
#endif
