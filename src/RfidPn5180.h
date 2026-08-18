#pragma once

#include <array>
#include <stdint.h>

using SlixPrivacyPassword = std::array<uint8_t, 4>;

// ICODE-SLIX2 privacy password defaults / storage key.
// Keep these here so PN5180 runtime and web settings use the same values.
static constexpr char SLIX_PRIVACY_PASSWORD_NVS_KEY[] = "slixPwd";
static constexpr SlixPrivacyPassword SLIX_PRIVACY_PASSWORD_DEFAULT = {
	{0x0F, 0x0F, 0x0F, 0x0F}
};

// Get/set the password currently used by the PN5180 task. The value is RAM-only;
// persistence remains handled by the RFID preferences in Web.cpp.
SlixPrivacyPassword RfidPn5180_GetSlixPrivacyPassword(void);
void RfidPn5180_SetSlixPrivacyPassword(const SlixPrivacyPassword &password);

// Returns the PN5180 firmware version cached in RAM during PN5180 initialization.
// No hardware access is performed here; false means no version has been read yet.
bool RfidPn5180_GetFirmwareVersion(uint8_t &major, uint8_t &minor);
