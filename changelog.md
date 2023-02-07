## New (modules)
* 22.04.2021: Introduced refactoring-branch
* xx.05.2021: Fixing/stabilizing code
* 08.06.2021: Added global support for PA/HP-enable
* 15.06.2021: Added interrupt-handling to PCA9555
* 22.06.2021: Changed ESP32's partition-layout in order to provider bigger NVS-storage
* 30.06.2021: Added directive `CACHED_PLAYLIST_ENABLE` for faster playlist-generation
* 08.07.2021: Added support to monitor WiFi's RSSI
* 09.07.2021: Making branch `refactoring` the the master
* 09.07.2021: Making master the new branch `old` (not maintained any longer!)
* 13.07.2021: Adding OTA-support via webGUI
* 23.07.2021: Adding new playmode: from local .m3u-file (files or webstreams)
* 30.09.2021: Added feature `PAUSE_WHEN_RFID_REMOVED` for RC522 after having it already for PN5180 (thanks @elmar-ops for contribution)
* 27.10.2021: Added feature `SAVE_PLAYPOS_BEFORE_SHUTDOWN`. When enabled last playposition for audiobook is saved when shutdown is initiated. Without having this feature enabled, it's necessary to press pause first, in order to do this manually.
* 28.10.2021: Added feature `SAVE_PLAYPOS_WHEN_RFID_CHANGE`. When enabled last playposition for audiobook is saved when new RFID-tag is applied. Without having this feature enabled, it's necessary to press pause first, in order to do this manually.
* 13.11.2021: Command `CMD_TELL_IP_ADDRESS` can now be assigned to buttons in order to get information about the currently used IP-address via speech.
* 29.01.2022: Directive `INVERT_POWER` can now be used it invert power-GPIO. And port-expander can now be used for power.
* 28.02.2022: Directive `MEASURE_BATTERY_MAX17055` added. Provides support for [MAX17055](https://www.maximintegrated.com/en/products/power/battery-management/MAX17055.html).
* 31.08.2022: Directive `DONT_ACCEPT_SAME_RFID_TWICE` added. Blocks unwanted reapplies of the same rfid-tag (in case it's not a modification-card or rfid-tag is unknown in NVS).
* 02.10.2022: ESPuino is now able to stream audio to external BT-devices. This is currently in testing. Big thanks to @tueddy for providing this feature!
* 18.10.2022: New playmode: pick random subdirectory of a given directory and play it's content alphabetic ordered
* 08.01.2023: New feature: `PAUSE_ON_MIN_VOLUME`. Playback is paused automatically if volume reaches minVolume (usually 0).

## Old (monolithic main.cpp)
* 11.07.2020: Added support for reversed Neopixel addressing.
* 09.10.2020: mqttUser / mqttPassword can now be configured via webgui.
* 16.10.2020: Added English as supported lanuage.
* 01.11.2020: Added directive `SD_NOT_MANDATORY_ENABLE`: for debugging puposes SD can be bypassed at boot.
* 17.11.2020: Introduced a distinct volume for headphone for optional headphone-PCB.
* 20.11.2020: Added directive `MEASURE_BATTERY_VOLTAGE`: monitoring battery's voltage is now supported.
* 25.11.2020: WiFi can now be enabled/disabled instantly by pressing two buttons.
* 28.11.2020: Battery's voltage can now be visualized by Neopixel by short-press of rotary encoder's button.
* 28.11.2020: Added directive `PLAY_LAST_RFID_AFTER_REBOOT`: ESPuino will recall the last RFID played after reboot.
* 05.12.2020: Added filebrowser to webgui (thanks @mariolukas for contribution!)
* 05.12.2020: Moved all user-relevant settings to src/settings.h
* 06.12.2020: Added PCB for Wemos Lolin32
* 08.12.2020: Reworked MQTT-timeout-Management
* 09.12.2020: mDNS-feature added. If ESPuino's name is "ESPuino", you can use `ESPuino.local` instead it's of IP-address.
* 11.12.2020: Revised GUI-design (thanks @mariolukas for contribution!) + (untested) PCB added for Wemos Lolin D32 + gerberfiles for headphone-PCB
* 18.12.2020: Added SD-MMC 1 Bit-mode (`SD_MMC_1BIT_MODE`). This mode needs one GPIO less and provides almost doubled speed (compared to SPI) for FTP-transfers (thanks @tueddy for contribution!)
* 18.12.2020: Added support for RFID-reader PN5180 (`RFID_READER_TYPE_PN5180`). PN5180 has better RFID-range/sensitivity and can read ISO-15693 / iCode SLIX2-tags aka 'Tonies' (thanks @tueddy for contribution!)
* 20.12.2020: Due to memory-issues with webstreams, FTP needs to be activated by pressing pause+next-button now
<br />More to come...
* 23.12.2020: User-config is now split into general part (settings.h) and develboard-specific part (e.g. settings-lolin32.h)
* 13.01.2021: Added fileexlorer to webgui (thanks @grch87 for contribution!). Now files and directories can be renamed, uploaded and deleted via webgui.
* 17.01.2021: Added directive `STATIC_IP_ENABLE`: (optional) static IPv4-configuration
* 18.01.2021: Added directive `PN5180_ENABLE_LPCD`: awake from deepsleep with PN5180 is now possible (but needs another GPIO)
* 25.01.2021: Added directive `USE_LAST_VOLUME_AFTER_REBOOT`: Remembers volume used at last shutdown after reboot. This overwrites initial volume from GUI.
* 28.01.2021: Removed cached RFID-filebrowser and replaced by realtime-browser
* 01.02.2021: Introducing PCB: Lolin32 with SD_MMC + PN5180
* 06.02.2021: German umlauts now supported. When uploading via FTP make sure to change charset to CP437.
* 09.02.2021: Added support for bluetooth-sink (a2dp). Thanks @grch87 & @elmar-ops for providing this feature!
* 25.02.2021: Added support for dynamic button-layout. Rotary-encoder is now optional and up to five buttons can be used.
* 25.02.2021: Actions can be freely assigned to buttons (multi-button, single-button (short), single-button (long))
* 25.02.2021: Added support for webcontrol: basic control (volume, play/pause, next, previous, first, last track) can now be controlled via webgui.
* 25.02.2021: Added support for .m4a and .wav-files.
* 26.02.2021: Shutdown via webgui is now available.
* 05.03.2021: Added support for remote control via infrared. Make sure to enable `IR_CONTROL_ENABLE` to use this feature and don't forget to assign corresponding rc-commands of *your* remote control to actions.
* 19.03.2021: Added support for port-expander PCA9555. Can be used for everything, that is "button-like": buttons, headphone-detect, PN5180.IRQ.
* 28.03.2021: Added support for fileseek. With commands `CMD_SEEK_FORWARDS` and `CMD_SEEK_BACKWARDS` it's possible to jump a number of seconds defined in `jumpOffset`.
* 30.03.2021: Added support for stereo/mono via `PLAY_MONO_SPEAKER`. If active, mono is used while headphones remain stereo (if `HEADPHONE_ADJUST_ENABLE` is active).
* no more updates will follow!
