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
* 29.10.2023: Previous master branch forked into branch Arduino1. Previous dev branch is new master branch now. Development of this branch (Arduino1) is discontinued officially.
