// clang-format off
#include "settings.h"

#ifdef OLED_ENABLE

#include "AudioPlayer.h"
#include "Battery.h"
#include "Display.h"
#include "Log.h"
#include "Wlan.h"
#include "values.h"

#include <U8g2lib.h>
#include <Wire.h>

extern TwoWire i2cBusTwo;

static uint8_t Display_I2cByteCb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_BYTE_INIT:
            i2cBusTwo.setClock(400000UL);
            break;
        case U8X8_MSG_BYTE_SEND:
            i2cBusTwo.write(static_cast<const uint8_t *>(arg_ptr), arg_int);
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            i2cBusTwo.beginTransmission(u8x8_GetI2CAddress(u8x8) >> 1);
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            i2cBusTwo.endTransmission();
            break;
        default:
            return 0;
    }
    return 1;
}

static U8G2_SH1106_128X64_NONAME_F_HW_I2C s_u8g2(U8G2_R0, U8X8_PIN_NONE);
static bool s_displayOk = false;

// -------- title scroll state --------
static char s_lastRawTitle[256] = "";

enum class ScrollPhase : uint8_t { PAUSE_START, SCROLLING, PAUSE_END };
static ScrollPhase s_scrollPhase      = ScrollPhase::PAUSE_START;
static uint32_t    s_scrollPhaseStart = 0;
static int16_t     s_scrollPos        = 0;
static uint32_t    s_lastScrollStep   = 0;

// font_6x13: 6 px wide → 21 chars fit in 126 px ≤ 128 px
static constexpr uint8_t  kLineChars     = 21;
static constexpr uint16_t kScrollPauseMs = 2000;
static constexpr uint16_t kScrollStepMs  = 280;

// -------- volume-bar state --------
static uint8_t  s_lastVol      = 0xFF;  // 0xFF = not yet sampled
static uint32_t s_volChangedAt = 0;
static constexpr uint32_t kVolBarDurationMs = 2500;

// -------- login splash state --------
static uint32_t s_idleSince    = 0;        // millis() when we first entered idle
static uint8_t  s_lastPlayMode = 0xFF;     // detect idle transition
static constexpr uint32_t kBootDurationMs  = 3000;  // boot screen
static constexpr uint32_t kLoginDurationMs = 3500;  // login animation
// Boot screen: first two lines appear, then "Booting." / ".." / "..."
static constexpr uint32_t kBootLine1Ms  = 0;
static constexpr uint32_t kBootLine2Ms  = 750;
static constexpr uint32_t kBootDotsMs   = 1000;  // dots start cycling from here
static constexpr uint32_t kBootDotCycle = 600;   // ms per dot step
// Login animation offsets are relative to kBootDurationMs
static constexpr uint32_t kUserStart  = 500;
static constexpr uint32_t kUserStep   = 220;
static constexpr uint32_t kPassStart  = 1400;
static constexpr uint32_t kPassStep   = 220;


// Convert a UTF-8 string to ISO-8859-1 (Latin-1) in-place equivalent.
// Characters outside Latin-1 (U+0100 and above) are replaced with '?'.
// All German umlauts (ä ö ü Ä Ö Ü ß) are within Latin-1 and round-trip losslessly.
static void utf8ToLatin1(const char *src, char *dst, size_t dstSize) {
    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di < dstSize - 1; ) {
        uint8_t c = static_cast<uint8_t>(src[si]);
        if (c < 0x80) {                         // ASCII – copy directly
            dst[di++] = static_cast<char>(c);
            si++;
        } else if (c == 0xC2 &&
                   static_cast<uint8_t>(src[si+1]) >= 0x80 &&
                   static_cast<uint8_t>(src[si+1]) <= 0xBF) {
            // U+0080..U+00BF: second byte is the Latin-1 code point
            dst[di++] = src[si + 1];
            si += 2;
        } else if (c == 0xC3 &&
                   static_cast<uint8_t>(src[si+1]) >= 0x80 &&
                   static_cast<uint8_t>(src[si+1]) <= 0xBF) {
            // U+00C0..U+00FF: second byte + 0x40 is the Latin-1 code point
            dst[di++] = static_cast<char>(static_cast<uint8_t>(src[si + 1]) + 0x40u);
            si += 2;
        } else {
            // Beyond Latin-1 or malformed – skip the whole sequence
            dst[di++] = '?';
            si++;
            while (static_cast<uint8_t>(src[si]) >= 0x80 &&
                   static_cast<uint8_t>(src[si]) <  0xC0) {
                si++;
            }
        }
    }
    dst[di] = '\0';
}

// Strip directory prefix and file extension from a path-style title.
static void stripPathAndExt(const char *src, char *dst, size_t dstSize) {
    const char *base = strrchr(src, '/');
    base = (base != nullptr) ? base + 1 : src;
    strncpy(dst, base, dstSize - 1);
    dst[dstSize - 1] = '\0';
    char *dot = strrchr(dst, '.');
    if (dot != nullptr && strlen(dot) >= 2 && strlen(dot) <= 5) {
        *dot = '\0';
    }
}

// Find the first " - " in src. Returns its index or SIZE_MAX if not found.
static size_t findDash(const char *src, size_t len) {
    for (size_t i = 0; i + 2 < len; i++) {
        if (src[i] == ' ' && src[i+1] == '-' && src[i+2] == ' ') {
            return i;
        }
    }
    return SIZE_MAX;
}

// Copy up to n chars from src into dst (always null-terminates).
static void copyLine(char *dst, const char *src, size_t n) {
    strncpy(dst, src, n);
    dst[n] = '\0';
}

// Try to split title on " - " boundaries across up to 3 lines.
// Searches the entire string — the dash can be anywhere.
// Returns number of lines filled (0 = no dash found, caller should hard-wrap).
static uint8_t splitOnDashes(const char *title, size_t len,
                              char *line1, char *line2, char *line3) {
    line1[0] = line2[0] = line3[0] = '\0';

    size_t d1 = findDash(title, len);
    if (d1 == SIZE_MAX) return 0; // no " - " at all

    size_t p1Len    = d1;
    const char *p2  = title + d1 + 3;
    size_t      p2Len = len - d1 - 3;

    if (p1Len <= kLineChars && p2Len <= kLineChars) {
        // Both parts fit cleanly: 2 lines
        copyLine(line1, title, p1Len);
        copyLine(line2, p2, p2Len);
        return 2;
    }

    if (p1Len <= kLineChars && p2Len <= kLineChars * 2u) {
        // Part1 fits on line1, part2 needs lines 2+3
        copyLine(line1, title, p1Len);
        // Try a second dash within part2
        size_t d2 = findDash(p2, p2Len);
        if (d2 != SIZE_MAX && d2 <= kLineChars && (p2Len - d2 - 3) <= kLineChars) {
            copyLine(line2, p2, d2);
            copyLine(line3, p2 + d2 + 3, p2Len - d2 - 3);
        } else {
            copyLine(line2, p2, kLineChars);
            copyLine(line3, p2 + kLineChars, kLineChars);
        }
        return 3;
    }

    if (p1Len <= kLineChars * 2u && p2Len <= kLineChars) {
        // Part1 needs lines 1+2, part2 goes on line3
        copyLine(line1, title, kLineChars);
        copyLine(line2, title + kLineChars, p1Len - kLineChars);
        copyLine(line3, p2, p2Len);
        return 3;
    }

    // Parts don't fit cleanly — give up, caller will hard-wrap
    return 0;
}

// Three display lines × 21 chars = 63-char static window.
static constexpr uint8_t kWindowChars3 = kLineChars * 3u;

// Draw the track title across three lines with scrolling for very long titles.
static void drawCentred(const char *s, uint8_t y) {
    int x = static_cast<int>((128 - s_u8g2.getStrWidth(s)) / 2);
    if (x < 0) x = 0;
    s_u8g2.drawStr(x, y, s);
}

static void Display_DrawTitle(const char *rawTitle, uint8_t y1, uint8_t y2, uint8_t y3) {
    char stripped[256];
    stripPathAndExt(rawTitle, stripped, sizeof(stripped));
    char title[256];
    utf8ToLatin1(stripped, title, sizeof(title));
    size_t len = strlen(title);

    if (strncmp(rawTitle, s_lastRawTitle, sizeof(s_lastRawTitle)) != 0) {
        strncpy(s_lastRawTitle, rawTitle, sizeof(s_lastRawTitle) - 1);
        s_lastRawTitle[sizeof(s_lastRawTitle) - 1] = '\0';
        s_scrollPos        = 0;
        s_scrollPhase      = ScrollPhase::PAUSE_START;
        s_scrollPhaseStart = millis();
    }

    if (len == 0) return;

    if (len <= kWindowChars3) {
        char line1[kLineChars + 1];
        char line2[kLineChars + 1];
        char line3[kLineChars + 1];

        uint8_t n = splitOnDashes(title, len, line1, line2, line3);
        if (n >= 1) {
            drawCentred(line1, y1);
            if (n >= 2 && line2[0]) drawCentred(line2, y2);
            if (n >= 3 && line3[0]) drawCentred(line3, y3);
            return;
        }

        // No dash split possible — hard-wrap across up to 3 lines
        strncpy(line1, title, kLineChars); line1[kLineChars] = '\0';
        drawCentred(line1, y1);
        if (len > kLineChars) {
            strncpy(line2, title + kLineChars, kLineChars); line2[kLineChars] = '\0';
            drawCentred(line2, y2);
        }
        if (len > kLineChars * 2u) {
            strncpy(line3, title + kLineChars * 2u, kLineChars); line3[kLineChars] = '\0';
            drawCentred(line3, y3);
        }
        return;
    }

    // Title too long even for 3 lines — scroll a 3-line window
    uint32_t now    = millis();
    int16_t  maxPos = static_cast<int16_t>(len - kWindowChars3);

    switch (s_scrollPhase) {
        case ScrollPhase::PAUSE_START:
            s_scrollPos = 0;
            if (now - s_scrollPhaseStart >= kScrollPauseMs) {
                s_scrollPhase    = ScrollPhase::SCROLLING;
                s_lastScrollStep = now;
            }
            break;
        case ScrollPhase::SCROLLING:
            if (now - s_lastScrollStep >= kScrollStepMs) {
                s_lastScrollStep = now;
                s_scrollPos++;
                if (s_scrollPos >= maxPos) {
                    s_scrollPos        = maxPos;
                    s_scrollPhase      = ScrollPhase::PAUSE_END;
                    s_scrollPhaseStart = now;
                }
            }
            break;
        case ScrollPhase::PAUSE_END:
            s_scrollPos = maxPos;
            if (now - s_scrollPhaseStart >= kScrollPauseMs) {
                s_scrollPos        = 0;
                s_scrollPhase      = ScrollPhase::PAUSE_START;
                s_scrollPhaseStart = now;
            }
            break;
    }

    char line1[kLineChars + 1];
    char line2[kLineChars + 1];
    char line3[kLineChars + 1];
    strncpy(line1, title + s_scrollPos,                  kLineChars); line1[kLineChars] = '\0';
    strncpy(line2, title + s_scrollPos + kLineChars,     kLineChars); line2[kLineChars] = '\0';
    strncpy(line3, title + s_scrollPos + kLineChars * 2, kLineChars); line3[kLineChars] = '\0';
    s_u8g2.drawStr(0, y1, line1);
    s_u8g2.drawStr(0, y2, line2);
    s_u8g2.drawStr(0, y3, line3);
}

// -----------------------------------------------------------------------

void Display_Exit(void) {
    if (!s_displayOk) return;
    s_u8g2.clearBuffer();
    s_u8g2.sendBuffer();
    s_u8g2.setPowerSave(1); // display off, low power — avoids SDA glitch as power rail drops
    s_displayOk = false;
}

void Display_Init(void) {
    // Allow the peripheral power rail time to stabilise after a power cycle.
    // 20 ms is too short when the rail starts from zero (cold boot / deepsleep).
    delay(200);
    i2cBusTwo.beginTransmission(oledI2cAddress);
    if (i2cBusTwo.endTransmission() != 0) {
        Log_Println("OLED: display not found on I2C bus", LOGLEVEL_ERROR);
        return;
    }
    s_u8g2.getU8x8()->byte_cb = Display_I2cByteCb;
    s_u8g2.setI2CAddress(oledI2cAddress << 1);
    s_u8g2.initDisplay();
    s_u8g2.setPowerSave(0);
    s_u8g2.clearBuffer();
    s_u8g2.sendBuffer();
    s_displayOk = true;
    Log_Println("OLED: display initialised", LOGLEVEL_INFO);
}

// -----------------------------------------------------------------------
//  IDLE SCREEN (128 × 64)
//    y=13   "LEO INDUSTRIES"            font_6x13
//    y=26   "AUDIO TERMINAL AT-1"       font_6x13
//    y=39   IP address (or "NO WIFI")   font_6x13
//    y=56   "READY_"  (_ blinks 500 ms) font_6x13
//
//  PLAYING SCREEN
//    y=10   Title line 1  (font_6x10, 21 chars)
//    y=20   Title line 2  (font_6x10, 21 chars or blank)
//    y=30   Title line 3  (font_6x10, 21 chars or blank)
//    y=57   B:XX% [left]  0:00/3:45 [centre]  W:ok [right]  (font_5x7)
//
//  VOLUME SCREEN (shown for 2.5 s after any volume change)
//    y=13   "VOLUME" centred            font_6x13
//    y=15   separator
//    y=18–38 bar (21 px tall, x=4..123)
//    y=40   separator
//    y=57   volume number centred       font_6x13
// -----------------------------------------------------------------------


void Display_Cyclic(void) {
    if (!s_displayOk) return;

    static uint32_t s_lastUpdate = 0;
    uint32_t now = millis();
    if (now - s_lastUpdate < 100u) return;
    s_lastUpdate = now;

    uint8_t vol = AudioPlayer_GetCurrentVolume();
    if (s_lastVol == 0xFF) {
        s_lastVol = vol;                    // first sample – no change event
    } else if (vol != s_lastVol) {
        s_lastVol      = vol;
        s_volChangedAt = now;
    }
    bool volScreen = (s_volChangedAt > 0) && (now - s_volChangedAt < kVolBarDurationMs);
    bool idle      = (gPlayProperties.playMode == NO_PLAYLIST);

    s_u8g2.clearBuffer();

    // ---- VOLUME SCREEN (takes over entire display) ----
    if (volScreen) {
        s_u8g2.setFont(u8g2_font_6x13_tf);
        const char *lbl = "VOLUME";
        s_u8g2.drawStr(static_cast<int>((128 - s_u8g2.getStrWidth(lbl)) / 2), 13, lbl);

        // Tall progress bar
        constexpr uint8_t barX = 4;
        constexpr uint8_t barY = 18;
        constexpr uint8_t barW = 120;
        constexpr uint8_t barH = 21;
        s_u8g2.drawFrame(barX, barY, barW, barH);
        uint8_t maxVol = AudioPlayer_GetMaxVolume();
        if (maxVol > 0 && vol > 0) {
            uint8_t filled = static_cast<uint8_t>(
                static_cast<uint16_t>(vol) * (barW - 2) / maxVol);
            s_u8g2.drawBox(barX + 1, barY + 1, filled, barH - 2);
        }

        // Volume number centred below
        char numBuf[6];
        snprintf(numBuf, sizeof(numBuf), "%d", vol);
        s_u8g2.drawStr(static_cast<int>((128 - s_u8g2.getStrWidth(numBuf)) / 2), 57, numBuf);

        s_u8g2.sendBuffer();
        return;
    }

    // ---- IDLE SCREEN ----
    if (idle) {
        // Track when idle started
        if (s_lastPlayMode != NO_PLAYLIST) {
            s_lastPlayMode = NO_PLAYLIST;
            s_idleSince    = now;
        }
        uint32_t idleMs = now - s_idleSince;

        s_u8g2.setFont(u8g2_font_6x13_tf);

        if (idleMs < kBootDurationMs) {
            // ---- BOOT SCREEN ----
            if (idleMs >= kBootLine1Ms) s_u8g2.drawStr(0, 13, "LEO INDUSTRIES");
            if (idleMs >= kBootLine2Ms) s_u8g2.drawStr(0, 26, "AUDIO TERMINAL AT-1");
            if (idleMs >= kBootDotsMs) {
                uint32_t step = ((idleMs - kBootDotsMs) / kBootDotCycle) % 3u;
                const char *dotStrs[] = {"Booting.", "Booting..", "Booting..."};
                s_u8g2.drawStr(0, 45, dotStrs[step]);
            }
        } else if (idleMs < kBootDurationMs + kLoginDurationMs) {
            // ---- LOGIN SPLASH ----
            uint32_t loginMs = idleMs - kBootDurationMs;
            bool cursorOn = (now / 400u) % 2u == 0u;

            // Header
            s_u8g2.drawStr(0, 13, "AT-1 LOGIN");

            // Username line (row 3)
            s_u8g2.drawStr(0, 35, "Username: ");
            uint8_t uChars = 0;
            if (loginMs >= kUserStart) {
                uChars = static_cast<uint8_t>(
                    min((loginMs - kUserStart) / kUserStep + 1u, static_cast<uint32_t>(3)));
            }
            const char *uFull = "leo";
            char uBuf[5];
            memcpy(uBuf, uFull, uChars);
            bool uDone = (uChars >= 3);
            uBuf[uChars] = (uDone ? '\0' : (cursorOn ? '_' : ' '));
            uBuf[uChars + (uDone ? 0 : 1)] = '\0';
            s_u8g2.drawStr(static_cast<int>(s_u8g2.getStrWidth("Username: ")), 35, uBuf);

            // Password line (row 4)
            if (loginMs >= kPassStart - kPassStep) {
                s_u8g2.drawStr(0, 52, "Password: ");
                uint8_t pChars = 0;
                if (loginMs >= kPassStart) {
                    pChars = static_cast<uint8_t>(
                        min((loginMs - kPassStart) / kPassStep + 1u, static_cast<uint32_t>(6)));
                }
                bool pDone = (pChars >= 6);
                char pBuf[8];
                memset(pBuf, '*', pChars);
                pBuf[pChars] = (pDone ? '\0' : (cursorOn ? '_' : ' '));
                pBuf[pChars + (pDone ? 0 : 1)] = '\0';
                s_u8g2.drawStr(static_cast<int>(s_u8g2.getStrWidth("Password: ")), 52, pBuf);
            }
        } else {
            // ---- NORMAL IDLE ----
            s_u8g2.drawStr(0, 13, "LEO INDUSTRIES");
            s_u8g2.drawStr(0, 26, "AUDIO TERMINAL AT-1");
            String ip = Wlan_GetIpAddress();
            s_u8g2.drawStr(0, 39, ip.length() > 0 ? ip.c_str() : "NO WIFI");
            bool cursorOn = (now / 500u) % 2u == 0u;
            s_u8g2.drawStr(0, 56, cursorOn ? "READY_" : "READY ");
        }

        s_u8g2.sendBuffer();
        return;
    }
    s_lastPlayMode = gPlayProperties.playMode;

    // ---- PLAYING SCREEN ----
    // Title: larger font (6x13, still 6px wide so the 21-char wrapping stays valid)
    s_u8g2.setFont(u8g2_font_6x13_tf);
    Display_DrawTitle(gPlayProperties.title, 12, 26, 40);

    // Status bar (small font_5x7): XX% left | elapsed/total centred | "WIFI" right (blank if no wifi)
    s_u8g2.setFont(u8g2_font_5x7_tf);

    // Battery – left
#ifdef BATTERY_MEASURE_ENABLE
    char batBuf[8];
    snprintf(batBuf, sizeof(batBuf), "%d%%",
             static_cast<int>(Battery_EstimateLevel() * 100.0f));
    s_u8g2.drawStr(0, 60, batBuf);
#endif

    // Elapsed / total – centred
    uint32_t elapsed  = AudioPlayer_GetCurrentTime();
    uint32_t duration = AudioPlayer_GetFileDuration();
    char timeBuf[16];
    if (gPlayProperties.isWebstream || duration == 0) {
        snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", elapsed / 60, elapsed % 60);
    } else {
        snprintf(timeBuf, sizeof(timeBuf), "%d:%02d/%d:%02d",
                 elapsed / 60, elapsed % 60,
                 duration / 60, duration % 60);
    }
    s_u8g2.drawStr(static_cast<int>((128 - s_u8g2.getStrWidth(timeBuf)) / 2), 60, timeBuf);

    // WiFi – right ("wifi" when connected, nothing when not)
    if (Wlan_IsConnected()) {
        const char *wifiStr = "WIFI";
        s_u8g2.drawStr(static_cast<int>(128 - s_u8g2.getStrWidth(wifiStr)), 60, wifiStr);
    }

    s_u8g2.sendBuffer();
}

#endif // OLED_ENABLE
