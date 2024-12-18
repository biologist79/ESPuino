# Changelog

## DEV-branch

* 18.12.2024: Use saved SSID as LAST_SSID, when no successfull connection was possible #355, thanks to @nona00 !
* 18.12.2024: Check if #POWER defined, thanks to @QDaniel !
* 18.12.2024: Prepare for Arduino 3.1.0
* 16.12.2024: Add verbose log to indicate, that the restart is on purpose (#356), thanks to @thebino !
* 16.12.2024: LPCD: Only wake up if card is present in NVS (#352), thanks to @fox34 ! 
* 25.11.2024: Moving some settings to the Web UI (#350), thanks to @trainbird !
* 17.11.2024: Simplify 'SdCard_ParseM3UPlaylist' and fix parsing, thanks to @h4kun4m4t4t4 !
* 29.10.2024: Update Audio lib
* 29.10.2024: Enable CONFIG_ESP32_REV_MIN_3 to save some IRAM
* 22.10.2024: Cleaning up `management.html` (#348), thanks to @trainbird !
* 22.10.2024: Update Arduino-IRremote to support the ESP 3.0 core
* 29.09.2024: Removal of support for non-PSRAM HALs (Lolin32, LolinD32, DevkitC) and ESP32-A1S
* 29.09.2024: Prepare for Arduino 3.0.5
* 27.09.2024: PlatformIO package 6.9.0

## Version 2.3 (23.09.2024)

* 19.09.2024: Document the virtual cards in the UI #333, thanks to @caco3 !
* 18.09.2024: Re-designed tools page (#337), thanks to @caco3 !
* 16.09.2024: Fix sleep after playlist not toggling nightmode #336, thanks to @dneukirchen !
* 12.09.2024: PlatformIO package 6.8.1 (Arduino 2.0.17, ESP-IDF 4.4.7)
* 12.09.2024: Change default settings to match "ESPuino-mini 4Layer" board
* 11.09.2024: Fix string's expected length for rfid-cmnd via mqtt
* 11.09.2024: Ignore mqtt messages with length zero
* 10.09.2024: Porting to new web server library completed
* 09.06.2024: Consume about 50% less CPU time in Audio library
* 09.06.2024: Replace 3rd party library toastr (#331), thanks to @trainbird !
* 09.06.2024: Modernized webserver & optimized JSON delivery (#332)
* 20.05.2024: PlatformIO package 6.7.0 (Arduino 2.0.16, ESP-IDF 4.4.7)
* 20.05.2024: Disable bluetooth support for A1S (IRAM too small)
* 09.05.2024: Upgrade to Bootstrap v5.0 + Web-UI Enhancements (#328), thanks to @caco3 !
* 23.04.2024: Enhance UI minification (#323), thanks to @caco3 !
* 22.04.2024: Added virtual RFID-Cards (#319), thanks to @caco3 !
* 18.04.2024: Correct calculation of newFilePos for SEEK_POS_PERCENT, thanks to @sfields!
* 18.04.2024: Correct calculation of gPlayProperties.currentRelPos, thanks to @sfields!
* 18.04.2024: Get title from-VORBISCOMMENT and ID3, thanks to @sfields!
* 18.04.2024: Correct log for SSID name, thanks to @sfields!
* 18.04.2024: Add button to expand/shrink file list (#317), thanks to @caco3 !
* 18.04.2024: Fix remote UI debugging (#316), thanks to @caco3 !
* 14.04.2024: Embed swagger UI #314, thanks to @caco3 !
* 07.04.2024: Equalizer settings to WebUI #313, thanks to @trainbird !
* 28.03.2024: Audio lib, fix a memory leak with flac files
* 28.03.2024: Fix some warnings with upcoming Arduino 3 (ESP-IDF v5.1)
* 16.03.2024: Reduce memory usage by measurement of JSON payload length
* 15.03.2024: MQTT receive: Bug fix and avoid payload copy (#309), thanks to @eikel
* 15.03.2024: Add coverimage support for ogg, vorbis, opus (and some flac), thanks to @sfields !
* 19.02.2024: Add coverimage support for M4A files in Web-UI
* 18.02.2024: Add coverimage support for flac files in Web-UI, thanks to @sfields !
* 15.02.2024: Rework playlist generation (#275), thanks to @laszloh !
* 15.02.2024: Update sorting strings (#306), thanks to @freddy36 !
* 15.02.2024: Set title and navbar heading to hostname (#305), thanks to @freddy36 !
* 15.02.2024: Include mac address in info (#304), thanks to @freddy36 !
* 12.02.2024: Use natural sort (#296), thanks to @freddy36 !
* 09.02.2024: Remove explorerCreateParentDirectories (#301), thanks to @freddy36 !
* 08.02.2024: Remove convertFilenameToAscii (#298), Playlist fixes (#299), thanks to @freddy36 !
* 08.02.2024: Apple touch icon
* 07.02.2024: Fix dangling pointer in explorerHandleFileStorageTask (#297), thanks to @freddy36 !
* 07.02.2024: Reduce WiFi memory consumption (#295), thanks to @laszloh !
* 05.02.2024: Increase JSON buffer for /debug endpoint
* 02.02.2024: Improve progress calculation/indication, thanks to @freddy36 !
* 01.02.2024: Use NVS interator when reading RFID keys, save 4KB RAM , thanks to @laszloh !
* 01.02.2024: Use dynamically allocated buffer for file upload, save 32KB RAM , thanks to @laszloh !
* 30.01.2024: Add French Language (#292), thanks to @damda58 !
* 30.01.2024: Fix some audio dropouts with high-quality OPUS files
* 30.01.2024: Keep original mtime for gzip compressed files, avoiding uneccesary Web.o rebuilds (#293), thanks to @freddy36 !
* 23.01.2024: Updated documentation on LPCD setting, thanks to @derpicknicker1 !
* 21.01.2024: Build (tag 2.2) is broken when NEOPIXEL_ENABLE is not set #289, thanks to @RolinBert !
* 21.01.2024: PN5180: Save about 500 Bytes of RAM

## Version 2.2 (17.01.2024)

* 14.01.2024: Modify CMD_DIMM_LEDS_NIGHTMODE to toggle night light mode (#287), thanks to @laszloh !
* 11.01.2024: More free heap on boards with PSRAM (CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL)
* 10.01.2024: LPCD for ISO-15693 cards
* 05.01.2024: REST-API specification, thanks to @pauel !
* 02.01.2024: Partly revert: Fix webstream playlist abort when track fails (#285), thanks to @sfields !
* 02.01.2024: Show critical voltage slider only if SHUTDOWN_ON_BAT_CRITICAL defined
* 29.12.2023: Show station logo in Web-UI (if available)
* 29.12.2023: /rfid endpoint: List all assigned nvs entries (#280)
* 29.12.2023: Enhanced capabilities for task runtime debugging
* 28.12.2023: Bugfix 1: DONT_ACCEPT_SAME_RFID_TWICE_ENABLE and web frontend, thanks to @sfields !
* 28.12.2023: Add configuration for critical (shutoff) voltage in Web-UI, thanks to @SZenglein !
* 28.12.2023: New official PlatformIO package 6.5.0 (Arduino 2.0.14, ESP-IDF 4.4.6)
* 28.12.2023: Improve port expander handling (#283), thanks to @r-schmidt !
* 23.12.2023: Update audio library, avoid waiting in i2s_channel_write() to have more time in other tasks #636
* 22.12.2023: Web-UI: Preselection of root folder after load to avoid nasty warning when uploading files
* 17.12.2023: Update audio library, fixes click-noise seeking in file
* 13.12.2023: Immediately go to sleep if battery is critical (#274), thanks to @SZenglein !
* 12.12.2023: Long press behaviour, execute cmd directly after longpress-time (#279), thanks to @Joe91 !
* 12.12.2023: Fix false-positive error (Audio playback timeout)
* 10.12.2023: Distribute vTaskDelay() in main loop to avoid rare audio dropouts
* 10.12.2023: Fix wrong states on PE output pins (and SD-card failure on restart) #278, thanks to @36b6fp6s !
* 09.12.2023: Fix webstream playlist abort when track fails (#276), thanks to @laszloh !
* 07.12.2023: Show RC522 firmware version at startup, same as PN5180
* 04.12.2023: fix stuttering sound with some WAV & MP3 files, thanks to @wolle !
* 04.12.2023: change trackprogress communication from websocket to http to improve stability
* 04.12.2023: Remove some convertAsciiToUtf8() #272
* 30.11.2023: Fix a nullptr access after trying to replay an invalid filename (#271), thanks to Olaf!
* 29.11.2023: Updated audio library to play more MP3s, faster track change & delivery of the cover image
* 25.11.2023: Save some cpu time in audio task by only updating the playtime statistics every 250ms
* 22.11.2023: Web-UI: Search for files feature #268
* 21.11.2023: New command CMD_TOGGLE_MODE to switch Normal => BT-Sink => BT-Source
* 21.11.2023: Bugfix: Some commands e.g. Play/Pause did not work in BT-Source mode
* 21.11.2023: Faster pairing animation in BT-Source mode to better distinguish between the two BT modes
* 19.11.2023: Give audiotask a higher task priority, fixes crackling sound
* 19.11.2023: bugfix_same_rfid_twice init #262, see Github comments
* 17.11.2023: Show track progress, current playtime & duration in web-ui (#267)
* 16.11.2023: Fix delay with getLocalTime()
* 14.11.2023: Multi Wlan improvements (#266), thanks to @laszloh !
* 13.11.2023: Update third party libraries
* 08.11.2023: Fix HTML syntax errors, found with static code analysis tool "HtmlHint"
* 08.11.2023: Better logging for boards without PSRAM, fewer logs when compiling with NO_SDCARD
* 07.11.2023: Set timezone after startup, thanks to @Joe91 !

## Version 2.1 (07.11.2023)

* 04.11.2023: LPCD: wakeup check for ISO-14443 cards also with IRQ connected to port-expander
* 04.11.2023: Bugfix for showing wrong loglevel
* 03.11.2023: Web-ui: Fix overlapping info/log after pressing refresh button
* 01.11.2023: Fix folder upload with special chars & playtime formatting
* 31.10.2023: Code Formatting via clang-format #264, thanks to @laszloh & @fschrempf !!
* 27.10.2023: PlatformIO package 6.4.0, Arduino version stays at 2.0.11
* 26.10.2023: LPCD, RFID_IRQ on port-expander: fix compiler warning
* 26.10.2023: portTICK_RATE_MS is deprecated, use portTICK_PERIOD_MS instead (<https://github.com/espressif/esp-idf/issues/51>)
* 26.10.2023: Cppcheck: Fix some warnings/hints
* 22.10.2023: Bugfix PortExpander: beginTransmission()/endTransmission() must be balanced
* 21.10.2023: Enhanced logging: Show Loglevel
* 14.10.2023: New define NO_SDCARD, enable to start without any SD card, e.g. for a webplayer only.
* 05.10.2023: Enable "Arduino as component" by default
* 02.10.2023: Optimize Arduino as component (Disable BLE, BT-HFP & SPI ethernet)
* 25.09.2023: Handle unicode characters in NVS backup, write UTF-8 BOM in backup.txt
* 23.09.2023: Fix some log messages, update FTP & A2DP libraries
* 22.09.2023: Bugfix_same_rfid_twice init (#262) - Thanks to @Joe91 !!
* 06.09.2023: Arduino as component #261 - Thanks to @Joe91 !!
* 01.09.2023: Regression: start an initial WiFi scan on startup, fix redirect to captive portal
* 31.08.2023: Show nvs rfid assignments in web-ui & allow to delete single tags
* 31.08.2023: invoke "/rfid" endpoint (#260)
* 22.08.2023: Unify endpoints for accesspoint.html/management.html (#258)
* 15.08.2023: Web-ui: Change /restart endpoint to POST, refresh button for info & log modal
* 12.08.2023: Web-ui improvements
* 08.08.2023: check & avoid excessive "PING" calls from web-ui
* 08.08.2023: Configurable volumecurve #256, thanks to @Niko & @Wolle
* 08.08.2023: Refactor "/info" endpoint to make frontend/backend more independant #257
* 07.08.2023: LOG message for starting mDNS service
* 04.08.2023: Support for .oga audio files (Ogg Vorbis Audio container)
* 04.08.2023: Web-UI: Replace the template processor (#253)
* 02.08.2023: Bugfix M3U-playlist + PREVIOUSTRACK, thanks to @Niko!
* 31.07.2023: Restart system: avoid peripheral power down while restarting
* 31.07.2023: increase stacksize for audio-task (audio-streams with SSL)
* 22.07.2023: New command CMD_TELL_CURRENT_TIME for announcing the current time
* 21.07.2023: Show overall playtime total in web-ui
* 21.07.2023: Refactor shutdown procedure, add new command CMD_RESTARTSYSTEM
* 20.07.2023: Allow to configure the timezone in settings.h
* 19.07.2023: Show received timestamp of NTP request in log
* 11.07.2023: Fix regression, SD-card was not working in SPI mode
* 09.07.2023: New ftp server by peterus #254. Thanks to @Joe91 for contribution !!
* 08.07.2023: Allow to compile without SD-card (webplayer only)
* 04.07.2023: fix volume jumping at startup when initVolume <> factory default (3u)
* 29.06.2023: Update PN5180 library to fix compilation with DCORE_DEBUG_LEVEL=5
* 25.06.2023: Some spelling corrections, thanks to @SciLor !
* 23.06.2023: CMD_TELL_IP_ADDRESS: IP as text (replace thousand separator with word "point")
* 23.06.2023: web-upload improvement (#249). Thanks to @Joe91 !!
* 23.06.2023: Refactor web: Move dependant html-code from web.cpp to management.html (#250)
* 20.06.2023: PlatformIO package 6.3.2
* 20.06.2023: Move ScanWiFiOnStart to global WiFi configuration (#248)
* 18.06.2023: Stricter hostname validation (#246). Thanks to @SZenglein !!
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
* 26.05.2023: Error in log: `[E][Preferences.cpp:96]` remove(): nvs_erase_key fail
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

## Version 1

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
