from pathlib import Path

globals()["Import"]("env")
env = globals()["env"]


def replace_exact(path, old, new):
    text = path.read_text(encoding="utf-8")
    if new in text:
        return
    if text.count(old) != 1:
        raise RuntimeError(
            f"FastLED I2S patch no longer matches {path}; update the pinned patch before building"
        )
    path.write_text(text.replace(old, new), encoding="utf-8")


fastled = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "FastLED"
engine = fastled / "src/platforms/esp/32/drivers/i2s/channel_engine_i2s_esp32dev.cpp.hpp"
peripheral = fastled / "src/platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_esp.cpp.hpp"

if not engine.is_file() or not peripheral.is_file():
    raise RuntimeError(f"Pinned FastLED dependency is missing from {fastled}")

replace_exact(
    engine,
    "namespace fl {\n\n",
    "namespace fl {\n\n"
    "// Keep the one-shot I2S FIFO fed with low samples throughout the WS2812\n"
    "// reset interval.\n"
    "constexpr size_t kWs2812ResetTailBytes =\n"
    "    (TIMING_WS2812_800KHZ::RESET * 6400000ULL / 1000000ULL) * sizeof(u16);\n\n",
)

replace_exact(
    engine,
    """    // Size the scratch buffer: max per-lane byte count across all
    // in-flight channels. Three regions (see packScratchBuffer):
    //   [0, 2W)        — 32-bit DMA samples (4 bytes per pulse), the
    //                    region actually handed to transmit()
    //   [2W, 3W)       — raw wave8 pulse pairs (2 bytes per pulse)
    //   [3W, 3W + 16b) — 16-lane-strided transpose input
    // where W = wave8I2s1EncodedFrameSize(bytes_per_lane).
""",
    """    // Size the scratch buffer for the 16-bit parallel DMA stream plus reset
    // preamble/tail and the lane-strided transpose input.
""",
)

replace_exact(
    engine,
    """    const size_t wave8_size = wave8I2s1EncodedFrameSize(bytes_per_lane);
    const size_t output_size = wave8I2s1Encoded32FrameSize(bytes_per_lane);
    const size_t input_size = 16 * bytes_per_lane;
    const size_t required = output_size + wave8_size + input_size;
""",
    """    const size_t wave8_size = wave8I2s1EncodedFrameSize(bytes_per_lane);
    const size_t input_size = 16 * bytes_per_lane;
    const size_t required = wave8_size + 2 * kWs2812ResetTailBytes + input_size;
""",
)

replace_exact(
    engine,
    """    // Two stages (FastLED#3569 root-cause fix):
    //   1. `encodeChannelWave8_i2s1` — shared 16-wide transpose kernel,
    //      2 bytes per pulse period, written to the middle region.
    //   2. `wave8I2s1ExpandTo32Samples` — rewrite each pulse pair into
    //      one 32-bit sample with lanes at bits 8..23, because I2S1 in
    //      `tx_bits_mod = 32` LCD mode clocks one u32 per pixel clock
    //      and presents sample bit (n + 8) on DATA_OUT(n).
""",
    """    // The shared transpose kernel already produces one 16-bit parallel word
    // per pulse period. Feed those words directly to I2S LCD mode.
""",
)

replace_exact(
    engine,
    """    const size_t input_size = 16 * bytes_per_lane;
    const size_t wave8_size = wave8I2s1EncodedFrameSize(bytes_per_lane);
    const size_t output_size = wave8I2s1Encoded32FrameSize(bytes_per_lane);
    if (mScratchSize < output_size + wave8_size + input_size) {
        // Belt-and-suspenders: defense against a stale scratch buffer
        // surviving a show() that used a much smaller frame.
        return 0;
    }
    fl::u8* const output = mScratchBuffer;                             // 32-bit DMA samples
    fl::u8* const wave8_tmp = mScratchBuffer + output_size;           // raw pulse pairs
    fl::u8* const input = mScratchBuffer + output_size + wave8_size;  // lane-strided input
""",
    """    const size_t input_size = 16 * bytes_per_lane;
    const size_t wave8_size = wave8I2s1EncodedFrameSize(bytes_per_lane);
    if (mScratchSize < wave8_size + 2 * kWs2812ResetTailBytes + input_size) {
        // Belt-and-suspenders: defense against a stale scratch buffer
        // surviving a show() that used a much smaller frame.
        return 0;
    }
    fl::u8* const output = mScratchBuffer + kWs2812ResetTailBytes;
    fl::u8* const input = output + wave8_size + kWs2812ResetTailBytes;
""",
)

replace_exact(
    engine,
    """    if (!encodeChannelWave8_i2s1(
            fl::span<const fl::u8>(input, input_size),
            bytes_per_lane, num_lanes, mWave8ByteLut,
            fl::span<fl::u8>(wave8_tmp, wave8_size))) {
        return 0;
    }
    if (!wave8I2s1ExpandTo32Samples(
            fl::span<const fl::u8>(wave8_tmp, wave8_size),
            fl::span<fl::u8>(output, output_size))) {
        return 0;
    }
    return output_size;
""",
    """    if (!encodeChannelWave8_i2s1(
            fl::span<const fl::u8>(input, input_size),
            bytes_per_lane, num_lanes, mWave8ByteLut,
            fl::span<fl::u8>(output, wave8_size))) {
        return 0;
    }

    fl::memset(mScratchBuffer, 0, kWs2812ResetTailBytes);
    fl::memset(output + wave8_size, 0, kWs2812ResetTailBytes);
    return wave8_size + 2 * kWs2812ResetTailBytes;
""",
)

replace_exact(
    peripheral,
    """    // Sample-rate configuration — `tx_bits_mod = 32` (Yves's proven
    // value): one 32-bit sample per pixel clock, with DATA_OUT(n)
    // presenting sample bit n+8. The engine feeds this format via
    // `wave8I2s1ExpandTo32Samples()` (one u32 per pulse, lanes at bits
    // 8..23) — see FastLED#3569 for the full mapping derivation.
    i2s->sample_rate_conf.val = 0;
    i2s->sample_rate_conf.tx_bits_mod = 32;
""",
    """    // One 16-bit parallel sample per pixel clock, matching FastLED's proven
    // classic-ESP32 I2S-SPI peripheral configuration.
    i2s->sample_rate_conf.val = 0;
    i2s->sample_rate_conf.tx_bits_mod = 16;
""",
)

replace_exact(
    peripheral,
    """    // FIFO in DMA-serviced mode — Yves baseline (32-bit single-channel)
    // restored after `= 1` triggered spurious DMA interrupts.
    i2s->fifo_conf.val = 0;
    i2s->fifo_conf.tx_fifo_mod_force_en = 1;
    i2s->fifo_conf.tx_fifo_mod = 3;
""",
    """    // 16-bit single-channel FIFO in DMA-serviced mode.
    i2s->fifo_conf.val = 0;
    i2s->fifo_conf.tx_fifo_mod_force_en = 1;
    i2s->fifo_conf.tx_fifo_mod = 1;
""",
)

replace_exact(
    peripheral,
    """    // I2S0 quirk (FastLED#3576 Phase 1 bench): in the same 32-bit mono
    // LCD config, I2S0 presents the emitted half-word on DATA_OUT8..23
    // (one byte higher than I2S1's DATA_OUT0..15) — lane n therefore
    // routes signal OUT(8+n) on port 0 and OUT(n) on port 1.
    const int base_signal = (i2s_device == 0)
        ? static_cast<int>(I2S0O_DATA_OUT0_IDX) + 8
        : static_cast<int>(I2S1O_DATA_OUT0_IDX);
""",
    """    // In 16-bit LCD mode the parallel sample appears on DATA_OUT8..23.
    const int base_signal = ((i2s_device == 0)
        ? static_cast<int>(I2S0O_DATA_OUT0_IDX)
        : static_cast<int>(I2S1O_DATA_OUT0_IDX)) + 8;
""",
)

replace_exact(
    peripheral,
    """    if (int_state & I2S_OUT_EOF_INT_ST_M) {
        self->finishTransmitFromIsr();
""",
    """    if (int_state & I2S_OUT_EOF_INT_ST_M) {
        // The reset-low tail has drained into the FIFO; stop before underrun
        // can replay stale samples as another WS2812 frame.
        i2s->conf.tx_start = 0;
        self->finishTransmitFromIsr();
""",
)

print("Applied classic ESP32 FastLED I2S 16-bit transport fix")
