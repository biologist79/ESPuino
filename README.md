# Tonuino based on ESP32 with I2S-DAC-support

## NEWS
Currently I'm working on a new Tonuino that is completely based on 3.3V and makes use of an (optional) [headphone-pcb](https://github.com/biologist79/Tonuino-ESP32-I2S/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TMD1308). As uC-develboard a Lolin32 is used and it's (optionally) battery-powered. So stay tuned...

## Disclaimer
This is a **fork** of the popular [Tonuino-project](https://github.com/xfjx/TonUINO) which means, that it only shares the basic concept of controlling a music-player by RFID-tags and buttons. **Said this I want to rule out, that the code-basis is completely different and developed by myself**. So there might be features, that are supported by my fork whereas others are missing or implemented differently. For sure both share that it's non-profit, DIY and developed on [Arduino](https://www.arduino.cc/).


## What's different (basically)?
The original project makes use of microcontrollers (uC) like Arduino nano (which is the [Microchip AVR-platform](https://de.wikipedia.org/wiki/Microchip_AVR) behind the scenes). Music-decoding is done in hardware using [DFPlayer mini](https://wiki.dfrobot.com/DFPlayer_Mini_SKU_DFR0299) which offers a uSD-card-slot and an integrated amp as well. Control of this unit is done by a serial-interconnect with a uC using the API provided.

The core of my implementation is based on the popular [ESP32 by Espressif](https://www.espressif.com/en/products/hardware/esp32/overview). Having WiFi-support out-of-the-box makes it possible to provide further features like an integrated FTP-server (to feed the player with music), smarthome-integration via MQTT and webradio. However, my primary focus was to port the project to a modular base. Said this mp3-decoding is done in software with a dedicated uSD-card-slot and music-output is done via I2S-protocol. I did all my tests on [Adafruit's MAX98357A](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp/pinouts). Hopefully, not only in theory, other DACs that support I2S can be used as well.

## Basic concept/handling
The basic idea of Tonuino (and my fork, respectively) is to provide a way, to use the Arduino-platform for a music-control-concept that supports locally stored music-files instead of being fully cloud-dependend. This basically means that RFID-tags are used to direct a music-player. Even for kids this concept is simple: place an RFID-object (card, character) on top of a box and the music starts to play. Place another RFID-object on it and anything else is played. Simple as that.

## Hardware-setup
The heart of my project is an ESP32 on a [Wemos Lolin32 development-board](https://www.ebay.de/itm/4MB-Flash-WEMOS-Lolin32-V1-0-0-WIFI-Bluetooth-Card-Based-ESP-32-ESP-WROOM-32/162716855489). If ordered in China (Aliexpress, eBay e.g.) it's pretty cheap (around 4€) but even in Europe it's only around 8€. Make sure to install the drivers for the USB/Serial-chip (CP2102 e.g.). If unsure have a look at eBay or Aliexpress for "Lolin 32".
* [MAX98357A (like Adafruit's)](https://www.ebay.de/itm/MAX98357-Amplifier-Breakout-Interface-I2S-Class-D-Module-For-ESP32-Raspberry-Pi/174319322988)
* [uSD-card-reader 3.3V + 5V](https://www.amazon.de/AZDelivery-Reader-Speicher-Memory-Arduino/dp/B077MB17JB)
* [uSD-card-reader 3.3V only](https://www.ebay.de/itm/Micro-SPI-Kartenleser-Card-Reader-2GB-SD-8GB-SDHC-Card-3-3V-ESP8266-Arduino-NEU/333796577968)
* [RFID-reader](https://www.amazon.de/AZDelivery-Reader-Arduino-Raspberry-gratis/dp/B074S8MRQ7)
* [RFID-tags](https://www.amazon.de/AZDelivery-Keycard-56MHz-Schlüsselkarte-Karte/dp/B07TVJPTM7)
* [Neopixel-ring](https://www.ebay.de/itm/LED-Ring-24-x-5050-RGB-LEDs-WS2812-integrierter-Treiber-NeoPixel-kompatibel/282280571841)
* [Rotary Encoder](https://www.amazon.de/gp/product/B07T3672VK)
* [Buttons](https://de.aliexpress.com/item/32896285438.html)
* [Speaker](https://www.visaton.de/de/produkte/chassiszubehoer/breitband-systeme/fr-7-4-ohm)
* uSD-card: doesn't have to be a super-fast one; uC is limiting the throughput. Tested 32GB without any problems.

Most of them can be ordered cheaper directly in China. It's just a give an short impression of the hardware; feel free to order where ever you want to. These are not affiliate-links.

## Getting Started
I recommend Microsoft's [Visual Studio Code](https://code.visualstudio.com/) alongside with [Platformio Plugin](https://platformio.org/install/ide?install=vscode). Since my project on Github contains [platformio.ini](platformio.ini), libraries used should be fetched automatically. Please note: if you use another ESP32-develboard (Lolin32 e.g.) you might have to change "env:" in platformio.ini to the corresponding value. Documentation can be found [here](https://docs.platformio.org/en/latest/projectconf.html). After that it might be necessary to adjust the names of the GPIO-pins in the upper #define-section of my code.
In the upper section of main.cpp you can specify the modules that should be compiled into the code.
Please note: if MQTT is enabled it's still possible to deactivate it via webgui.

## Wiring (2 SPI-instances)
A lot of wiring is necessary to get ESP32-Tonuino working. After my first experiments I soldered the stuff on a board in order to avoid wild-west-cabling. Especially for the interconnect between uC and uSD-card-reader make sure to use short wires (like 10cm or so)! Important: you can easily connect another I2S-DACs by just connecting them in parallel to the I2S-pins (DIN, BCLK, LRC). This is true for example if you plan to integrate a [line/headphone-pcb](https://www.adafruit.com/product/3678). In general, this runs fine. But unfortunately especially this board lacks of a headphone jack, that takes note if a plug is inserted or not. Best way is to use a [headphone jack](https://www.conrad.de/de/p/cliff-fcr1295-klinken-steckverbinder-3-5-mm-buchse-einbau-horizontal-polzahl-3-stereo-schwarz-1-st-705830.html) that has a pin that is pulled to GND, if there's no plug and vice versa. Using for example a MOSFET-circuit, this GND-signal can be inverted in a way, that MAX98357.SD is pulled down to GND if there's a plug. Doing that will turn off the speaker immediately if there's a plug and vice versa. Have a look at the PCB-folder in order to view the detailed solution. Here's an example for such a [headphone-pcb](https://github.com/biologist79/Tonuino-ESP32-I2S/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TMD1308) that makes use of GND.

| ESP32 (GPIO)  | Hardware              | Pin    | Comment                                                      |
| ------------- | --------------------- | ------ | ------------------------------------------------------------ |
| 5 V           | SD-reader             | VCC    | Connect to p-channel MOSFET for power-saving when uC is off  |
| GND           | SD-reader             | GND    |                                                              |
| 15            | SD-reader             | CS     |                                                              |
| 13            | SD-reader             | MOSI   |                                                              |
| 16            | SD-reader             | MISO   |                                                              |
| 14            | SD-reader             | SCK    |                                                              |
| 17            | RFID-reader           | 3.3V   | Connect directly to GPIO 17 for power-saving when uC is off  |
| GND           | RFID-reader           | GND    |                                                              |
| 21            | RFID-reader           | CS/SDA |                                                              |
| 23            | RFID-reader           | MOSI   |                                                              |
| 19            | RFID-reader           | MISO   |                                                              |
| 18            | RFID-reader           | SCK    |                                                              |
| 5 V           | MAX98357              | VIN    | Connect to p-channel MOSFET for power-saving when uC is off  |
| GND           | MAX98357              | GND    |                                                              |
| 25            | MAX98357              | DIN    |                                                              |
| 27            | MAX98357              | BCLK   |                                                              |
| 26            | MAX98357              | LRC    |                                                              |
| ---           | MAX98357              | SD     | Info: if pulled down to GND amp will turn off                |
| 34            | Rotary encoder        | CLR    | Invert CLR with DT if you want to change the direction of RE |
| 35            | Rotary encoder        | DT     | Invert CLR with DT if you want to change the direction of RE |
| 32            | Rotary encoder        | BUTTON |                                                              |
| 3.3 V         | Rotary encoder        | +      |                                                              |
| GND           | Rotary encoder        | GND    |                                                              |
| 4             | Button (next)         |        |                                                              |
| GND           | Button (next)         |        |                                                              |
| 2             | Button (previous)     |        |                                                              |
| GND           | Button (previous)     |        |                                                              |
| 5             | Button (pause/play)   |        |                                                              |
| GND           | Button (pause/play)   |        |                                                              |
| 5 V           | Neopixel              | 5 V    | Connect to p-channel MOSFET for power-saving when uC is off  |
| GND           | Neopixel              | GND    |                                                              |
| 12            | Neopixel              | DI     | Might be necessary to use a logic-converter 3.3 => 5V        |
| 17            | (e.g.) BC337 (via R5) | Base   | Don't forget R5!                                             |
| 33            | Voltage-divider / BAT |        | Optional: Voltage-divider to monitor battery-voltage         |


Optionally, GPIO 17 can be used to drive a NPN-transistor (BC337-40) that pulls a p-channel MOSFET (IRF9520) to GND in order to switch on/off 5V-current. Transistor-circuit is described [here](https://dl6gl.de/schalten-mit-transistoren.html): Just have a look at Abb. 4. Values of the resistors I used: R1: 10k, R2: omitted(!), R4: 10k, R5: 4,7k. <br />
Also tested this successfully for a 3.3V-setup with IRF530NPBF (N-channel MOSFET) and NDP6020P (P-channel MOSFET). Resistor-values: R1: 100k, R2: omitted(!), R4: 100k, R5: 4,7k. A 3.3V-setup is helpful if you want to battery-power your Tonuino and 5V is not available in battery-mode. For example this is the case when using Wemos Lolin32 with only having LiPo connected. <br />
Advice: When powering a SD-card-reader solely with 3.3V, make sure to use one WITHOUT a voltage regulator. Or at least one with a pin dedicated for 3.3V (bypassing voltage regulator). This is because if 3.3V go through the voltage regulator a small voltage-drop will be introduced, which may lead to SD-malfunction as the resulting voltage is a bit too low. Vice versa if you want to connect your reader solely to 5V, make sure to have one WITH a voltage regulator :-).

## Wiring (1 SPI-instance) [EXPERIMENTAL!]
Basically the same as using 2 SPI-instances but...
In this case RFID-reader + SD-reader share SPI's SCK, MISO and MOSI. But make sure to use different CS-pins.

| ESP32 (GPIO)  | Hardware              | Pin    | Comment                                                      |
| ------------- | --------------------- | ------ | ------------------------------------------------------------ |
| 5 V           | SD-reader             | VCC    | Connect to p-channel MOSFET for power-saving when uC is off  |
| GND           | SD-reader             | GND    |                                                              |
| 15            | SD-reader             | CS     | Don't share with RFID!                                       |
| 17            | RFID-reader           | 3.3V   | Connect directly to GPIO 17 for power-saving when uC is off  |
| GND           | RFID-reader           | GND    |                                                              |
| 21            | RFID-reader           | CS/SDA | Don't share with SD!                                         |
| 23            | RFID+SD-reader        | MOSI   |                                                              |
| 19            | RFID+SD-reader        | MISO   |                                                              |
| 18            | RFID+SD-reader        | SCK    |                                                              |
| 5 V           | MAX98357              | VIN    | Connect to p-channel MOSFET for power-saving when uC is off  |
| GND           | MAX98357              | GND    |                                                              |
| 25            | MAX98357              | DIN    |                                                              |
| 27            | MAX98357              | BCLK   |                                                              |
| 26            | MAX98357              | LRC    |                                                              |
| ---           | MAX98357              | SD     | Info: if pulled down to GND amp will turn off                |
| 34            | Rotary encoder        | CLR    | Invert CLR with DT if you want to change the direction of RE |
| 35            | Rotary encoder        | DT     | Invert CLR with DT if you want to change the direction of RE |
| 32            | Rotary encoder        | BUTTON |                                                              |
| 3.3 V         | Rotary encoder        | +      |                                                              |
| GND           | Rotary encoder        | GND    |                                                              |
| 4             | Button (next)         |        |                                                              |
| GND           | Button (next)         |        |                                                              |
| 2             | Button (previous)     |        |                                                              |
| GND           | Button (previous)     |        |                                                              |
| 5             | Button (pause/play)   |        |                                                              |
| GND           | Button (pause/play)   |        |                                                              |
| 5 V           | Neopixel              | 5 V    | Connect to p-channel MOSFET for power-saving when uC is off  |
| GND           | Neopixel              | GND    |                                                              |
| 12            | Neopixel              | DI     | Might be necessary to use a logic-converter 3.3 => 5V        |
| 17            | (e.g.) BC337 (via R5) | Base   | Don't forget R5!                                             |
| 33            | Voltage-divider / BAT |        | Optional: Voltage-divider to monitor battery-voltage         |

## Wiring (custom) / different pinout
When using a develboard with for example SD-card-reader already integrated (Lolin D32 Pro), the pinouts described above my not fit; feel free to change them according your needs. Additionaly some boards may use one or some of the GPIOs I used for their internal purposes and that reason for are maybe not exposed via pin-headers. However, having them exposed doesn't mean they can be used without limits. This is because some GPIOs have to be logical LOW or HIGH at start for example and this is probably not the case when connecting stuff to it. Feel free to adjust the GPIOs proposed by me (but be adviced it could take a while to get it running). If you encounter problems please refer the board's manual first. <br />
Keep in mind the RFID-lib I used is intended for default-SPI-pins only (SCK, MISO, MOSI). [Here](https://github.com/biologist79/Tonuino-ESP32-I2S/tree/master/Hardware-Plaforms/ESP32-A1S-Audiokit) I described a solution for a board with many GPIOs used internally and a very limited number of GPIOs exposed. That's why I had to use different SPI-GPIOs for RFID as well. Please note I used a slightly modified [RFID-lib](https://github.com/biologist79/Tonuino-ESP32-I2S/tree/master/Hardware-Plaforms/ESP32-A1S-Audiokit/lib/MFRC522) there.

## Prerequisites / tipps
* choose if optional modules (MQTT, FTP, Neopixel) should be compiled/enabled
* for debugging-purposes serialDebug can be set to ERROR, NOTICE, INFO or DEBUG.
* if MQTT=yes, set the IP of the MQTT-server accordingly and check the MQTT-topics (states and commands)
* if Neopixel enabled: set NUM_LEDS to the LED-number of your Neopixel-ring and define the Neopixel-type using `#define CHIPSET`
* If you're using Arduino-IDE please make sure to change ESP32's partition-layout to `No OTA (2MB APP/2MB Spiffs)` as otherwise the sketch won't fit into the flash-memory.
* Please keep in mind that working SD is mandatory. Unless `SD_NOT_MANDATORY_ENABLE` is not set, Tonuino will never fully start up if SD is not working. Only use `SD_NOT_MANDATORY_ENABLE` for debugging as for normal operational mode, not having SD working doesn't make sense.
* If you want to monitor the battery-voltage, make sure to enable `MEASURE_BATTERY_VOLTAGE`. Use a voltage-divider as voltage of a LiPo is way too high for ESP32 (only 3.3V supported!). For my tests I connected VBat with a serial connection of 130k + 390k resistors (VBat--130k--X--390k--GND). X is the measure-point where to connect the GPIO to.
* If you're using a headphone-pcb with a [headphone jack](https://www.conrad.de/de/p/cliff-fcr1295-klinken-steckverbinder-3-5-mm-buchse-einbau-horizontal-polzahl-3-stereo-schwarz-1-st-705830.html) that has a pin to indicate if there's a plug, you can use this signal along with the feature `HEADPHONE_ADJUST_ENABLE` to limit the maximum headphone-voltage automatically. As per default you have to invert this signal and connect it to GPIO22.
* Enabling `SHUTDOWN_IF_SD_BOOT_FAILS` is really recommended if you run your Tonuino in battery-mode without having a restart-button exposed to the outside of the Tonuino's enclosure. Because otherwise there's no way to restart your Tonuino and the error-state will remain until battery is empty (or you open the enclosure, hehe).
* compile and upload the sketch

## Starting Tonuino-ESP32 first time
After plugging in it takes a few seconds until neopixel indicates that Tonuino is ready (by four (slow) rotating LEDs; white if Wifi enabled and blue if disabled). If uC was not able to connect to WiFi, an access-point (named Tonuino) is opened and after connecting this WiFi, a [configuration-Interface](http://192.168.4.1) is available via webbrowser. Enter WiFI-credentials + the hostname (Tonuio's name), save them and restart the uC. Then reconnect to your "regular" WiFi. Now you're ready to go: start learning RFID-tags via GUI. To reach the GUI enter the IP stated in the serial console or use the hostname. For example if you're using a Fritzbox as router and entered tonuino as hostname in the previous configuration-step, in your webbrowser tonuino.fritz.box should work. After doing rfid-learnings, place your RFID-tag next to the RFID-reader and the music (or whatever else you choosed) should start to play. While the playlist is generated/processed, fast-rotating LEDs are shown to indicate that Tonuino is busy. The more tracks a playlist/directory contains the longer this step will take. <br >
Please note: hostname can be used to call webgui or FTP-server. I tested it with a Fritzbox 7490 and worked fine. Make sure you don't use a name that already exists in you local network (LAN).

## WiFi
WiFi is mandatory for webgui, FTP and MQTT. However, WiFi can be temporarily or permanently disabled. There are two ways to do that:
* Use a special modification-card that can be configured via webgui
* Press previous-key (and keep pressed!) + press next-button in parallel shortly. Now release both.
This toggles the current WiFi-status which means: if it's currently enabled, it will be disabled instantly and vice versa. Please note: This WiFi-status will remain until you change it again. Having Wifi enabled is indicated in idle-mode (no playlist active) with four white slow rotating LEDs whereas disabled WiFi is represented by those ones colored blue.
## After Tonuino-ESP32 is connected to your WiFi
After getting Tonuino part of your LAN/WiFi, the 'regular' webgui is available at the IP assigned by your router (or the configured hostname). Using this GUI, you can configure:
* WiFi
* Binding between RFID-tag, file/directory/URL and playMode
* Binding between RFID-tag and a modification-type
* MQTT
* FTP
* Initial volume, maximum volume (speaker / headphone), brightness of Neopixel (nightmode / default) and inactivity-time

Webgui #1:
<img src="https://raw.githubusercontent.com/biologist79/Tonuino-ESP32-I2S/master/pictures/Mgmt-GUI1.jpg" width="300">

Webgui #2:
<img src="https://raw.githubusercontent.com/biologist79/Tonuino-ESP32-I2S/master/pictures/Mgmt-GUI2.jpg" width="300">

Webgui #3:
<img src="https://raw.githubusercontent.com/biologist79/Tonuino-ESP32-I2S/master/pictures/Mgmt-GUI3.jpg" width="300">

Webgui #4:
<img src="https://raw.githubusercontent.com/biologist79/Tonuino-ESP32-I2S/master/pictures/Mgmt-GUI4.jpg" width="300">

Webgui #5:
<img src="https://raw.githubusercontent.com/biologist79/Tonuino-ESP32-I2S/master/pictures/Mgmt-GUI5.jpg" width="300">

Webgui: websocket broken:
<img src="https://raw.githubusercontent.com/biologist79/Tonuino-ESP32-I2S/master/pictures/Mgmt-GUI_connection_broken.jpg" width="300">

Webgui: action ok:
<img src="https://raw.githubusercontent.com/biologist79/Tonuino-ESP32-I2S/master/pictures/Mgmt_GUI_action_ok.jpg" width="300">

Please note: as you apply a RFID-tag to the RFID-reader, the corresponding ID is pushed to the GUI (and flashes a few times; so you can't miss it). So there's no need to enter such IDs manually (unless you want to).

## Interacting with Tonuino
### Playmodes
It's not just simply playing music; different playmodes are supported:
* single track
* single track (loop)
* audiobook (single file or playlist; last play-position (file and playlist) is saved and re-used next time)
* audiobook (loop)
* folder/playlist (alph. sorted)
* folder/playlist (random order)
* folder/playlist (alph. sorted) as loop
* folder/playlist (random order) as loop
* webradio (always only one "track")

### Modification RFID-tags
There are special RFID-tags, that don't start music by themself but can modify things. If applied a second time, it's previous action will be reversed. Please note: all sleep-modes do dimming automatically because it's supposed to be used in the evening when going to bed. Well, at least that's my children's indication :-) So first make sure to start the music then use a modification-card in order to apply your desired modification:
* lock/unlock all buttons
* sleep after 5/30/60/120 minutes
* sleep after end of current track
* sleep after end of playlist
* sleep after five tracks
* dimm neopixel
* current track in loop-mode (is "stronger" than playlist-loop but doesn't overwrite it!)
* playlist in loop-mode
* track und playlist loop-mode can both be activated at the same time, but unless track-loop isn't deactivated, playlist-loop won't be effective
* Toggle WiFi (enable/disable) => disabling WiFi while webstream is active will stop the webstream instantly!

### Neopixel-ring (optional)
Indicates different things. Don't forget configuration of number of LEDs via #define NUM_LEDS
* While booting: 1/2 LEDs rotating orange
* Unable to mount SD: LEDs flashing red (will remain forever unless SD-card is available)
* IDLE: four LEDs slow rotating (white if WiFi enabled; blue if WiFi disabled)
* ERROR: all LEDs flashing red (1x)
* OK: all LEDs flashing green (1x)
* BUSY: violet; four fast rotating LEDs
* track-progress: rainbow; number of LEDs relative to play-progress
* playlist-progress: blue; appears only shortly in playlist-mode with every new track; number of LEDs relative to progress
* volume: green => red-gradient; number of LEDs relative from actual to max volume
* switching off: red; circle that grows until long-press-time is reached
* buttons locked: track-progress-LEDs coloured red
* paused: track-progress-LEDs coloured orange
* rewind: if single-track-loop is activated a LED-rewind is performed when restarting the given track
* (Optional) Undervoltage: flashes three times red if battery-voltage is too low

Please note: some Neopixels use a reversed addressing which leads to the 'problem', that all effects are shown
counter clockwise. If you want to change that behaviour, just enable `NEOPIXEL_REVERSE_ROTATION`.

### Buttons
Some buttons have different actions if pressed long or short. Minimum duration for long press in ms is defined by `intervalToLongPress`.
* previous (short): previous track / beginning of the first track if pressed while first track is playing
* previous (long): first track of playlist
* next (short): next track of playlist
* next (long): last track of playlist
* pause/play (short/long): pause/play
* rotary encoder (turning): vol +/-
* rotary encoder (button long): switch off (only when on)
* rotary encoder (button short): switch on (only when off)
* previous (long; keep pressed) + next (short): toggle WiFi enabled/disabled

### Music-play
* music starts to play right after valid RFID-tag was applied
* if a folder should be played that contains many mp3s, the playlist generation can take a few seconds. Please note that a file's name including path cannot exceed 255 characters.
* while playlist is generated Neopixel indicates BUSY-mode
* after last track was played, Neopixel indicates IDLE-mode
* in audiobook-mode, last play-position is remembered (position in actual file and number of track, respectively) when a new track begins of if pause-button was hit
* in webstream-mode the stream will instantly stop if WiFi-status is toggled to disabled. Tonuino will return to idle-mode and wait for new RFID-tag.

### Audiobook-mode
This mode is different from the other ones because the last playposition is saved. Playposition is saved when...
* next track starts
* first/previous/last track requested by button
* pause was pressed
* playlist is over (playposition is set back to the first track and file-position 0)

### Webinterface-configuration
After having Tonuino running on your ESP32 in your local WiFi, the webinterface-configuration is accessable. Using this GUI you can configure:
* Wifi-configuration (Wifi-SSID, Wifi-password, Tonuino's name (for nameserver))
* Link between RFID-tag and corresponding action
* MQTT-configuration (broker's IP)
* FTP-configuration (username and password)
* General-configuration (volume (speaker + headphone), neopixel-brightness (night-mode + initial), sleep after inactivity)

### FTP (optional)
In order to avoid exposing uSD-card or disassembling the Tonuino all the time for adding new music, it's possible to transfer music onto the uSD-card using FTP. Please make sure to set the max. number of parallel connections to ONE in your FTP-client. My recommendation is [Filezilla](https://filezilla-project.org/). But don't expect fast data-transfer. Initially it was around 145 kB/s but after modifying ftp-server-lib (changing from 4 kB static-buffer to 16 kB heap-buffer) I saw rates improving a bit. Please note: if music is played in parallel, this rate decrases dramatically! So better stop playback then doing a FTP-transfer. However, playback sounds normal if a FTP-upload is performed in parallel. Default-user and password are set via `ftpUser` and `ftpPassword` but can be changed later via GUI.

### Files / ID3-tags (IMPORTANT!)
Make sure to not use filenames that contain German 'Umlaute'. I've been told this is also true for mp3's ID3-tags.

### Energy saving
As already described in the modify-section, there are different sleepmodes available. Additionaly uC will be put into deepsleep after 10 minutes of inactivity (configurable my maxInactivityTime) unless Tonuino doesn't play music, has a FTP-client connected and any input via buttons. Every button-interaction resets the counter to the initial value.

### MQTT (optional)
Everything that can be controlled via RFID-tags and buttons, can also be controlled via MQTT (excepting toggling WiFi-status). All manual interactions (buttons, RFID-tags) are also sent to MQTT in parallel, so everything is always in-sync (unless Wifi/MQTT-connection is broken). In my home-setup I'm using [openHAB](https://www.openhab.org/) to encapsulate MQTT into a nice GUI, that's accessible via APP + web. ToDo: Publish sample-configurations for openHAB.

### Supported file/stream-types
Please refer [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S), as this is the library I used for music-decoding.

### Backups
As all assignments between RFID-IDs and actions (playmode, file to play...) is saved in ESP's NVS, the problem is that it's all gone when the ESP is broken. So that's where a backup comes into play. So every time you change or add a new assignment between a RFID-tag and an action via GUI, a backup-file is saved on the uSD-card. The file's name can be changed via `backupFile`. Again using the GUI you can use the upload-form to import such a file. To be honest: Sometimes I had some issues with Firefox doing this whereas Safari turned out to do it right. Don't know why :-(.

## Smarthome (optional)
As already described, MQTT is supported. In order to use it it's necessary to run a MQTT-broker; [Mosquitto](https://mosquitto.org/) for instance. After connecting to it, Tonuino subscribes to all command-topics. State-topics are used to push states to the broker in order to inform others if anything changed (change of volume, new playlist, new track... name it). Others, like openHAB, subscribe to state-topics end send commands via command-topics. So it's not just limited to openHAB. It's just necessary to use a platform, that supports MQTT. For further informations refer the [subfolder](https://github.com/biologist79/Tonuino-ESP32-I2S/tree/master/openHAB).

## MQTT-topics and their ranges
Feel free to use your own smarthome-environments (instead of openHAB). The MQTT-topics available are described as follows. Please note: if you want to send a command to Tonuino, you have to use a cmnd-topic whereas Tonuino pushes its states back via state-topics. So guess to want to change the volume to 8 you have to send this number via topic-variable `topicLoudnessCmnd`. Immediately after doing to, Tonuino sends a conformation of this command using `topicLoudnessState`.


| topic-variable          | range           | meaning                                                                        |
| ----------------------- | --------------- | ------------------------------------------------------------------------------ |
| topicSleepCmnd          | 0 or OFF        | Power off Tonuino immediately                                                  |
| topicSleepState         | ON or OFF       | Sends Tonuino's current/last state                                             |
| topicTrackCmnd          | 12 digits       | Set number of RFID-tag which 'emulates' an RFID-tag (e.g. `123789456089`)      |
| topicTrackState         | 12 digits       | Sends number of last RFID-tag applied                                          |
| topicTrackControlCmnd   | 1 -> 7          | `1`=stop; `2`=unused!; `3`=play/pause; `4`=next; `5`=prev; `6`=first; `7`=last |
| topicLoudnessCmnd       | 0 -> 21         | Set loudness (depends on minVolume / maxVolume)                                |
| topicLoudnessState      | 0 -> 21         | Sends loudness (depends on minVolume / maxVolume                               |
| topicSleepTimerCmnd     | EOP             | Power off after end to playlist                                                |
|                         | EOT             | Power off after end of track                                                   |
|                         | EO5T            | Power off after end of five tracks                                             |
|                         | 1 -> 2^32       | Duration in minutes to power off                                               |
|                         | 0               | Deactivate timer (if active)                                                   |
| topicSleepTimerState    | various         | Sends active timer (`EOP`, `EOT`, `EO5T`, `0`, ...)                            |
| topicState              | Online, Offline | `Online` when powering on, `Offline` when powering off                         |
| topicCurrentIPv4IP      | IPv4-string     | Sends Tonuino's IP-address (e.g. `192.168.2.78`)                               |
| topicLockControlsCmnd   | ON, OFF         | Set if controls (buttons, rotary encoder) should be locked                     |
| topicLockControlsState  | ON, OFF         | Sends if controls (buttons, rotary encoder) are locked                         |
| topicPlaymodeState      | 0 - 10          | Sends current playmode (single track, audiobook...; see playmodes)             |
| topicRepeatModeCmnd     | 0 - 3           | Set repeat-mode: `0`=no; `1`=track; `2`=playlist; `3`=both                     |
| topicRepeatModeState    | 0 - 3           | Sends repeat-mode                                                              |
| topicLedBrightnessCmnd  | 0 - 255         | Set brightness of Neopixel                                                     |
| topicLedBrightnessState | 0 - 255         | Sends brightness of Neopixel                                                   |
| topicBatteryVoltage     | float           | Voltage (e.g. 3.81)                                                            |