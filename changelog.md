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