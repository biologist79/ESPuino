## DEV-branch
* 20.06.2023: PlatformIO package 6.3.2
* 20.06.2023: Move ScanWiFiOnStart to global WiFi configuration (#248) 
* 18.06.2023: Stricter hostname validation (#246). Thank's to @SZenglein !!
* 18.06.2023: Some Web-UI improvements (#247) 
* 17.06.2023: CMD_TOGGLE_WIFI_STATUS: Escape from BT-mode, WiFi cannot coexist with BT and can cause a crash
* 13.06.2023: Bluetooth configuration tab in web-interface (#244)
* 13.06.2023: Introduce new playmode "RANDOM_SUBDIRECTORY_OF_DIRECTORY_ALL_TRACKS_OF_DIR_RANDOM"
* 07.06.2023: Use bssid to connect to best WIFI and added option to always perform a WIFI-scan #241
* 06.06.2023: List available WiFi's in accesspoint view
* 06.06.2023: Remove support for Arduino 1.0.6 & playlist cache
* 05.06.2023: FastLED 3.6.0, PlatformIO package 6.3.1
* 01.06.2023: Improve neopixel display in Bluetooth speaker mode (BT-Sink) (#239)
* 01.06.2023: Reduce floating the log with redundant RSSI-messages: Log RSSI value only if it has changed by > 3 dBm
* 31.05.2023: Wifi: Log BSSID in scan and connection success, Wifi fixes (#240)
* 26.05.2023: Fix compiler warnings if MQTT_ENABLE is not defined
* 26.05.2023: Error in log: [E][Preferences.cpp:96] remove(): nvs_erase_key fail
* 23.05.2023: Migration from single-wifi setup: Initialize "use_static_ip"
* 22.05.2023: DEV branch has stricter compiler rules, fix some of (mostly harmless) warnings
* 22.05.2023: BTSource: fix Stack canary watchpoint triggered (BtAppT)
* 19.05.2023: Show Arduino version at startup & in web-info
* 19.05.2023: Add support for saving multiple wifi networks (#221)
* 09.05.2023: Bluetooth.cpp: Fix logging crash in sink mode (#238)
* 06.05.2023: README.md: Improve formatting, style, language and content
* 03.05.2023: Add support for control LEDs to be connected at the end of the NeoPixel Ring (#198)
* 27.04.2023: Remove usage of pgmspace.h header 
* 27.04.2023: Move message formatting into LogMessages 
* 26.04.2023: Configure button with active HIGH
* 20.04.2023: Speedup playlist generation & file listing by factor > 20 
* 14.04.2023: Removing HAL for ESPmuse (stale development)
* 12.04.2023: Restrict value ranges to guard array access
* 11.04.2023: Merge pull request #226 from fschrempf/bugfix_rfid_assign
* 08.04.2023: Enforcing uniform codestyle
* 08.04.2023: Allow to escape from bluetooth mode with an unknown card, switch back to normal mode
* 03.04.2023: Merge pull request #223 from laszloh/bugfix/snprintf_warning, fix for snprintf trunctation warnings
* 03.04.2023: Add missing translations
* 31.03.2023: build.yml: Trigger workflow for all branches
* 27.03.2023: Merge pull request #219 from laszloh/feature/modern_cpp
* 25.03.2023: Transition to c++17
* 24.03.2023: Lost feature „PLAY_LAST_RFID_AFTER_REBOOT“ 
* 23.03.2023: Change volume animation to triggered mode
* 22.03.2023: Merge pull request #215 from fschrempf/execute-commands
* 22.03.2023: Change rotary encoder to delta mode
* 22.03.2023: Fix errorous indicator in case no audio is playing
* 22.03.2023: Fix for stuck LED-Shutdown animation
* 21.03.2023: Pause some tasks for speedup & smoother HTTP upload
* 21.03.2023: Feature: command-execution on control-site of webserver
* 21.03.2023: Merge pull request #210 from fschrempf/led_rework_cleaned_frieder
* 21.03.2023: Set default environment to arduino2 on dev-branch
* 18.03.2023: Bugfix: Corrupted files after HTTP-upload
* 18.03.2023: Major rework of LED animation task
* 13.03.2023: Fix for playing files via Web-UI for Arduino >=2.0.5
* 13.03.2023: Fix captive portal, Add fallback text AP setup
* 10.03.2023: Merge pull request #204 from laszloh/bugfix/heap_handling 
* 10.03.2023: Always free playlist memory when SdCard_ReturnPlaylist is called
* 10.03.2023: Fix heap corruption due to insufficent memory allocation
* 10.03.2023: Harmonize return value of SdCard_ReturnPlaylist
* 10.03.2023: Correct the name of the "sort" function to randomize 
* 10.03.2023: Create DEV(eloper) branch


## Master

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
