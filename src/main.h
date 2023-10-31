#pragma once

extern bool gPlayLastRfIdWhenWiFiConnected;
extern bool gTriedToConnectToHost;

#ifdef PLAY_LAST_RFID_AFTER_REBOOT
extern void recoverLastRfidPlayedFromNvs(bool force = false);
#endif
