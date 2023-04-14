# ESPuino - rfid-based musiccontroller based on ESP32 with I2S-DAC-support

## Forum
* EN: I've set up a primarily German-speaking community with much documentation. Also an international corner for non-German-speakers is available at https://forum.espuino.de. Github-Login can be used there but it's not mandatory.
* DE: Ich habe ein primär deutschsprachiges Forum aufgesetzt, welches ich mit reichlich Doku versehen habe. Würde mich freuen, euch dort zu sehen: https://forum.espuino.de. Ihr könnt euch dort mit eurem Github-Login einloggen, jedoch auch "normal" anmelden. Dokumentation findet ihr insbesondere hier: https://forum.espuino.de/c/dokumentation/anleitungen/10
## Build status
![build workflow](https://github.com/biologist79/ESPuino/actions/workflows/build.yml/badge.svg)

## Current Development
A [dev](https://github.com/biologist79/ESPuino/tree/dev)-branch was created. Any new features will be introduced and tested there first until they become part of the master-branch. Feel free to test but be advised this could be unstable.

## Known bugs
* For ESPuinos making use of SPI to connect SD, there's an unsolved problem that occasionally leads to incomplete file-transfers via webtransfer or FTP-transfer. Solution: use SD_MMC instead (by the way: it's faster and needs one GPIO less).
## ESPuino - what's that?
The basic idea of ESPuino is to use RFID-tags to direct a music-player. Even for kids this concept is simple: place an RFID-object (card, character) on top of a box and the music starts to play stuff from SD or webradio. Place another RFID-object on it and anything else is played. Simple as that.

This project is based on the popular microcontroller [ESP32 by Espressif](https://www.espressif.com/en/products/hardware/esp32/overview). Why? It's powerful and having WiFi-support out-of-the-box enables further features like an integrated webserver, smarthome-integration via MQTT, webradio and FTP-server. And nonetheless Bluetooth, too! However, my primary focus was to port the project to a modular base: mp3-decoding is done in software and the digital music-output is done via the popular [I2S-protocol](https://en.wikipedia.org/wiki/I%C2%B2S). So we need a [DAC](https://en.wikipedia.org/wiki/Digital-to-analog_converter) to make an analog signal of it: I did all my tests with [MAX98357A](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp/pinouts), [UDA1334](https://www.adafruit.com/product/3678), [MS6324](https://forum.espuino.de/t/kopfhoererplatine-basierend-auf-ms6324-und-tda1308/1099/) and [PCM5102a](https://github.com/biologist79/ESPuino/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308). General advice: ESPuino makes use of library [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S/); so everything that's supposed to work with this library should work with ESPuino, too (but maybe not right out of the box). Especially this is true for [ES8388](https://github.com/schreibfaul1/ESP32-audioI2S/blob/master/examples/ESP32_ES8388/ESP32_ES8388.ino).

## Hardware-setup
You could start on a breadboard with jumper wires but I *strongly* recommend to start right away with a PCB like [ESPuino-mini 4L](https://forum.espuino.de/t/espuino-mini-4layer/1661). Several PCBs are available. Please click on the links below for more informations and pictures.
* [Lolin D32 / Lolin D32 pro + PN5180/RC522 + port-expander (SMD)](https://forum.espuino.de/t/espuino-minid32-pro-lolin-d32-d32-pro-mit-sd-mmc-und-port-expander-smd/866)
* [Lolin32 + SD_MMC + PN5180/RC522 (THT)](https://forum.espuino.de/t/lolin32-mit-sd-sd-mmc-und-pn5180-als-rfid-leser/77/)
* [NodeMCU ESP32 + SD_MMC + PN5180/RC522 (THT)](https://forum.espuino.de/t/az-delivery-esp32-nodemcu-devkit-c-mit-sd-mmc-und-pn5180-als-rfid-leser/634)
* And some more... please have a look [here](https://forum.espuino.de/c/hardware/pcbs/11).
* All of these platforms are compatible with [headphone-pcb #1](https://forum.espuino.de/t/kopfhoererplatine-basierend-auf-ms6324-und-tda1308/1099/) and [headphone-pcb #2](https://forum.espuino.de/t/kopfhoererplatine-basierend-auf-uda1334-pj306b/705) and [headphone-pcb #3](https://github.com/biologist79/ESPuino/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308).

## Getting Started
* [Much much documentation in german language](https://forum.espuino.de/c/dokumentation/anleitungen/10).
* You need to install Microsoft's [Visual Studio Code](https://code.visualstudio.com/).
* Install [Platformio Plugin](https://platformio.org/install/ide?install=vscode) into [Visual Studio Code](https://code.visualstudio.com/) and make sure to have a look at the [documentation](https://docs.platformio.org/en/latest/integration/ide/pioide.html). Step-by-step-manual is available [here](https://randomnerdtutorials.com/vs-code-platformio-ide-esp32-esp8266-arduino/.)
* Install [Git](https://git-scm.com/downloads) and make a copy ("clone") my repository to your local computer using `git clone https://github.com/biologist79/ESPuino.git`. Using git you can keep your local repository easily up to date without doing copy'n'paste. To keep it up to date run `git pull origin master`. Further infos [here](https://stackoverflow.com/questions/1443210/updating-a-local-repository-with-changes-from-a-github-repository) and [here](https://forum.espuino.de/t/espuino-in-platformio-anlegen-und-mit-git-aktuell-halten/891).
* (Optional) Install [Gitlens](https://marketplace.visualstudio.com/items?itemName=eamodio.gitlens) as plugin (to have advanced Git-support).
* Now, that the git-repository is saved locally, import this folder into Platformio as a project.
* Select the [desired environment](https://forum.espuino.de/t/projekt-und-profilwechsel-in-visual-studio-code/768) (e.g. lolin_d32_pro_sdmmc_pe).
* Edit `src/settings.h` according your needs.
* Edit board-specific (`HAL`) config-file (e.g. `settings-lolin_d32_pro_sdmmc_pe.h` for Lolin D32/D32 pro). If you're running a board that is not listed there: start with `settings-custom.h` and change it according your needs.
* Connect your develboard via USB, click the alien-head to the left, choose the project-task that matches your desired HAL and run `Upload and Monitor`. All libraries necessary should be fetched in background now followed by code-compilation. After that, your ESP32 is flashed with the firmware. Depending on your develboard it might me necessary to push a button in order to allow ESP32 to enter flashmode (not necessary für Lolin32, D32 und D32 pro).
* Now have a look at the serial-output at the bottom of Visual Studio Code's window. At the first run there might appear a few error-messages (related to missing entries in NVS). Don't worry, this is just normal. However, make sure SD is running as this is mandatory!
* If everything ran fine, at the first run, ESPuino should open an access-point with the name "ESPuino". Join this WiFi with your computer (or mobile) and enter `http://192.168.4.1` to your webbrowser. Enter WiFi-credentials and the hostname. After saving the configuraton, restart ESPuino.
* After reboot ESPuino tries to join your WiFi (with the credentials previously entered). If that was successful, an IP is shown in the serial-console. You can call ESPuino's GUI using a webbrowser via this IP; make sure to allow Javascript. If mDNS-feature is active in `src/settings.h`, you can use the hostname configured extended by .local instead the IP. So if you configured `espuino` as hostname, you can use `http://espuino.local` for webgui and FTP.
* Via FTP and webGUI you can upload data (but don't expect it to be super fast).
* FTP needs to be activated after boot if you need it! Don't forget to assign action `ENABLE_FTP_SERVER` in `settings.h` to be able to activate it. Neopixel flashes green (1x) if enabling was successful. It'll be disabled automatically after next reboot. Means: you have to enable it every time you need it (if reboot was in between). Sounds annoying and maybe it is, but's running this way in order to have more heap-memory available (for webstream) if FTP isn't necessary.
* Via webbrowser you can configure various settings and pair RFID-tags with actions. If MQTT/FTP-support was not compiled, their config-tabs won't appear.

## SD-card: SPI or SD-MMC (1 bit)-mode?
Having SD working is mandatory, ESPuino doesn't start without working SD! However, there are two modes available to connect SD-cards: SPI and SDMMC (1 bit). Advice: don't use SPI as there's a bug that often leading to broken files due to interrupted file-transfers. Beside of that, SDMMC is still twice as fast as SPI and needs one GPIO less.

## Which RFID-reader: RC522 or PN5180?
RC522 is so to say the ESPuino-standard. It's cheap and works, but RFID-tag has to be placed near the reader. PN5180 instead has better RFID range/sensitivity and can read ISO-15693 / iCode SLIX2-tags aka 'Tonies' (you need a password to read Tonies), too. You can also wake up ESPuino with the a ISO-14443 card (after flashing PN5180 with a new firmware). This feature is called LPCD. Disadvantages PN5180: it's more expensive and needs more GPIOs (6/7 instead of 4). In my opinion it's worth it! Refer PN5180's wire-section below for further informations. Hint: if using 3.3V only make sure to connect these 3.3V to PN5180's 5V AND 3.3V. Sounds weird but it's necessary.

## 3.3 or 5V?
ESP32 runs at 3.3 V only. But what about the periphery?
* 3.3V! Because: if you plan to use battery-mode with LiPo/LiFePO4, there's no 5 V available (unless USB is connected or you make use of a boost-converter).
That's why my focus is on 3.3 V only. If you want to use 5 V instead - do so, but be advised it might not be compatible with battery-mode.
* MAX98357a: provides more power at 5 V but also runs at 3.3 V. Anyway: it's still loud enough (in my opinion).
* Neopixel: specification says it needs 5 V but runs at 3.3 V as well.
* RC522: needs 3.3 V (don't ever power with 5 V!)
* PN5180: at 3.3 V make sure to connect both 5 V and 3.3 V-pins to 3.3 V. If 5 V is available all the time: connect 5 V to 5 V and 3.3 V to 3.3 V.
* SD: needs 3.3 V but if voltage-regulator is onboard, it can be connected to 5 V as well
* Rotary encoder: 3.3 V (don't power with 5 V! Encoder doesn't care if connected to 3.3 or 5 V, but GPIOs of ESP32 do!)

## WiFi
WiFi is mandatory for webgui, FTP, MQTT and webradio. However, WiFi can be temporarily or permanently disabled (and ESPuino remembers this state after the next restart). There are two ways to (re-)enable/disable WiFi:
* Use a special [modification-card](https://forum.espuino.de/t/was-sind-modifikationskarten/37) that can be configured via webgui.
* Assign action `CMD_TOGGLE_WIFI_STATUS` to a button (or multi-button). This toggles the current WiFi-status.

## Bluetooth
ESPuino can be used as bluetooth-sink (a2dp-sink). In this mode you can stream (e.g. from a mobile device) via bluetooth audio to your espuino. This mode can be enabled/disabled via a RFID-modification-card or by assigning action `CMD_TOGGLE_BLUETOOTH_MODE` to a button (or multi-button). Applying this will restart ESPuino immediately. Activated bluetooth is indicated by four slow rotating *blue* LEDs. Please note: due to memory-restrictions it's not possible to run bluetooth in parallel with WiFi. This finally means that you cannot access webgui while this mode is enabled.

ESPuino can also be used to stream audio (a2dp-source) to a bluetooth headset or external bluetooth speakers. This mode can be enabled/disabled via a RFID-modification-card or by assigning action `CMD_TOGGLE_BLUETOOTH_SOURCE_MODE` to a button (or multi-button). Applying this will restart ESPuino immediately. Activated bluetooth is indicated by four slow rotating *blue-violet* LEDs. After the bluetooth headset is connected LEDs turn to blue. Please note: due to memory-restrictions it's not possible to run bluetooth in parallel with WiFi. This finally means that you cannot stream webradio via bluetooth or access webgui while this mode is enabled.

## Port-expander
There might be situations where you run out of GPIOs. To address this, port-expander [PCA9555](https://www.nxp.com/docs/en/data-sheet/PCA9555.pdf) can be used to extend number of input-channels (output-mode is only supported in special cases). PCA9555 provides 2 ports with 8 channels each - so 16 channels in total. To activate PCA9555 you need to enable `PORT_EXPANDER_ENABLE`. Like GPIOs in your develboard-specific settings-file, you can assign numbers. Range is 100->115 where 100: port 0 channel 0 -> 107: port 0 channel 7; 108: port 1 channel 0 -> 115: port 1 channel 7. Via `expanderI2cAddress` port-expander's I2C-address can be changed. It's `0x20` if all A0, A1, A2 are wired to GND.<br />

## After ESPuino is connected to your WiFi
After making ESPuino part of your LAN/WiFi, the 'regular' webgui is available at the IP assigned by your router (or the configured hostname). Using this GUI you can:
* configure WiFi
* make bindings between RFID-tag, file/directory/URL and playMode
* make bindings between RFID-tag and a modification-type
* configure MQTT (if enabled)
* configure FTP (if enabled)
* configure initial volume, maximum volume (speaker / headphone), brightness of Neopixel (nightmode / default) and inactivity-time
* view logs / status / current track
* control player
* run modifications (like modification-card)
* upload audiofiles (called webtransfer)
* do OTA-updates (ESP32s with 16 MB of flash-memory only)
* import + delete NVS-RFID-assigments
* restart + shutdown ESPuino

Webgui #1:
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI1.jpg" width="300">

Webgui #2:
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI2.jpg" width="300">

Webgui #3:
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI3.jpg" width="300">

Webgui #4:
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI4.jpg" width="300">

Webgui #5:
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI5.jpg" width="300">

Webgui #6:
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI6.jpg" width="300">

Webgui #7:
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI7.jpg" width="300">

Webgui: websocket broken:
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI_connection_broken.jpg" width="300">

Webgui: action ok:
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt_GUI_action_ok.jpg" width="300">

Please note: as you apply a RFID-tag to the RFID-reader, the corresponding ID is pushed to the GUI. So there's no need to enter such IDs manually (unless you want to). Filepath is filled out automatically by selecting a file/directory in the filebrowser.

## Interacting with ESPuino
### Playmodes
It's not just simply playing music; different playmodes are supported:
* `Single track` => plays one track one time
* `Single track (loop)` => plays one track forever
* `Single track of a directory (random). Followed by sleep` => picks and plays one single track out of a directory and falls asleep subsequently. Neopixel gets dimmed.
* `Audiobook`=> single file or playlist/folder; last play-position (file and playlist) is saved (when pushing pause or moving to another track) and re-used next time
* `Audiobook (loop)` => same as audiobook but loops forever
* `Folder/playlist (alph. sorted)` => plays all tracks in alph. order from a folder one time
* `Folder/playlist (random order)` => plays all tracks in random order from a folder one time
* `Folder/playlist (alph. sorted)` => plays all tracks in alph. order from a folder forever
* `Folder/playlist (random order)` => plays all tracks in random order from a folder forever
* `All tracks of a random subdirectory (sorted alph.)` => plays of tracks in alph. order of a randomly picked subdirectory of a given directory
* `Webradio` => always only one "track": plays a webstream
* `List (files from SD and/or webstreams) from local .m3u-File` => can be one or more files / webradio-stations with local .m3u as sourcefile

### Modification RFID-tags
There are special RFID-tags, that don't start music by themself but can modify things. If applied a second time, it's previous action/modification will be reversed. Please note: all sleep-modes do dimming (Neopixel) automatically because it's supposed to be used in the evening when going to bed. Well, at least that's my children's indication :-) So first make sure to start the music then use a modification-card in order to apply your desired modification:
* lock/unlock all buttons
* sleep after 5/30/60/120 minutes
* sleep after end of current track
* sleep after end of playlist
* sleep after five tracks
* dimm neopixel
* current track in loop-mode (is "stronger" than playlist-loop but doesn't overwrite it!)
* playlist in loop-mode
* track und playlist loop-mode can both be activated at the same time, but unless track-loop isn't deactivated, playlist-loop won't be effective
* Toggle WiFi (enable/disable) => disabling WiFi while webstream is active will stop a running webstream instantly!
* Toggle Bluetooth sink (enable/disable) => restarts ESPuino immediately. In this mode you can stream to your ESPuino via BT.
* Toggle Bluetooth source (enable/disable) => restarts ESPuino immediately. In this mode your ESPuino can stream via BT to an external device.

### Neopixel-ring (optional)
Indicates different things. Don't forget configuration of number of LEDs via #define NUM_LEDS
* While booting: every second LED (rotating orange)
* Unable to mount SD: LEDs flashing red (will remain forever unless SD-card is available or `SHUTDOWN_IF_SD_BOOT_FAILS` is active)
* IDLE: four LEDs slow rotating (white if WiFi connected; green if WiFi disabled or ESPuino is about to connect to WiFi)
* BLUETOOTH: four LEDs slow rotating coloured blue
* ERROR: all LEDs flashing red (1x) if an action was not accepted
* OK: all LEDs flashing green (1x) if an action was accepted
* BUSY: violet; four fast rotating LEDs when generating a playlist. Duration depends on the number of files in your playlist.
* track-progress: rainbow; number of LEDs relative to play-progress
* playlist-progress: blue; appears only shortly in playlist-mode with the beginning every new track; number of LEDs relative to progress
* webstream: two slow rotating LEDs that change their colours rainbow-wise as the stream proceeds
* volume: green => red-gradient; number of LEDs relative from current to max volume
* switching off: red-circle that grows until long-press-time is reached
* buttons locked: track-progress-LEDs coloured red
* paused: track-progress-LEDs coloured orange
* rewind: if single-track-loop is activated a LED-rewind is performed when restarting the given track
* (Optional) Undervoltage: flashes three times red if battery-voltage is too low. This voltage-level can be configured via GUI.
* (Optional) Short press of rotary encoder's button provides battery-voltage visualisation via Neopixel. Upper und lower voltage cut-offs can be adjusted via GUI. So for example if lower voltage is set to 3.2 V and upper voltage to 4.2 V, 50% of the LEDs indicate a voltage of 3.7 V.

Please note: some Neopixels use a reversed addressing which leads to the 'problem', that all effects are shown
counter clockwise. If you want to change that behaviour, just enable `NEOPIXEL_REVERSE_ROTATION`.

### Buttons
Important: this section describes my default-design: 3 buttons + rotary-encoder. Feel free to change number of buttons (up to 5) and button-actions according your needs in `settings.h` and your develboard-specific config-file (e.g. `settings-lolin32.h`). At maximum you can activate five buttons + rotary-encoder.
Minimum duration for long press (to distinguish vom short press) in ms is defined by `intervalToLongPress`. All actions available are listed in `src/values.h`. If using GPIO >= 34 make sure to add a external pullup-resistor (10 k).
* previous (short): previous track / beginning of the first track if pressed while first track is playing
* previous (long): first track of playlist
* next (short): next track of playlist
* next (long): last track of playlist
* pause/play (short/long): pause/play
* rotary encoder (turning): vol +/-
* rotary encoder (button long): switch off (only when on)
* rotary encoder (button short): switch on (when switched off)
* rotary encoder (button short): show battery-voltage via Neopixel (when switched on and `MEASURE_BATTERY_VOLTAGE` is active)
* previous (long; keep pressed) + next (short) + release (both): toggle WiFi enabled/disabled

### Music-play
* Music starts to play right away after a valid RFID-tag was applied (if it's known to ESPuino).
* If `PLAY_LAST_RFID_AFTER_REBOOT` is active, ESPuino will remember the last RFID applied => music-autoplay.
* If a folder should be played that contains many mp3s, the playlist-generation can take a few seconds.
* For all playmodes that are not single tracks or webradio a filecache is available to speed up playlist-generation. The cache is generated as you apply the corresponding RFID-tag for the first time. Use `CACHED_PLAYLIST_ENABLE` to enable it - I really recommend to use it.
* A file's name including path isn't allowed exceed 255 characters.
* While playlist is generated Neopixel indicates BUSY-mode.
* After last track was played, Neopixel indicates IDLE-mode.

### Audiobook-mode
This mode is different from the others because the last playposition is saved. Playposition is saved when...
* next track starts.
* first/previous/last track requested by button.
* pause was pressed.
* track is over.
* playlist is over (playposition is set back to the first track and file-position 0).
* As per default last playposition is not saved when applying a new RFID-tag. You can enable this using `SAVE_PLAYPOS_WHEN_RFID_CHANGE`.
* As per default last playposition is not saved when doing shutdown. You can enable this using `SAVE_PLAYPOS_BEFORE_SHUTDOWN`.

### FTP (optional)
* FTP needs to be activated after boot! Don't forget to assign action `ENABLE_FTP_SERVER` in `settings.h` or use a modification-card to to activate it! Neopixel flashes green (1x) if enabling was successful. It'll be disabled automatically after next reboot. Means: you have to enable it every time you need it (if reboot was in between). Sounds annoying and maybe it is, but's running this way in order to save heap-memory when FTP isn't needed.
* Why FTP? Well: in order to avoid exposing µSD-card or disassembling ESPuino all the time for adding new music, it's possible to transfer music to the µSD-card using FTP. Another possibility is to do via webGUI (webtransfer).
* Default-user and password are set to `esp32` / `esp32` but can be changed via GUI.
* Make sure to set the max. number of parallel connections to ONE in your FTP-client and the charset to CP437. CP437 is important if you want to use german umlauts (öäüß).
* Secured FTP is not available. So make sure to disable SSL/TLS.
* Software: my recommendation is [Filezilla](https://filezilla-project.org/) as it's free and available for multiple platforms.
* Don't expect a super fast data-transfer; it's around 185 kB/s (SPI-mode) and 310-360 kB/s (MMC-mode).
* Please note: if music is played in parallel, this rate decrases dramatically! So better stop playback when doing file-transfers.

### Energy saving
As already described in the modify-section, there are different sleepmodes available. Additionaly µC will be put to deepsleep after 10 minutes of inactivity (configurable my maxInactivityTime) unless ESPuino doesn't play music, has a FTP-client connected and any input via buttons. Every button-interaction resets the counter.

### MQTT (optional)
Everything that can be controlled via RFID-tags and buttons, can also be controlled via MQTT (excepting toggling WiFi-status as this doesn't make sense). All manual interactions (buttons, RFID-tags) are also sent to MQTT in parallel, so everything is always in-sync (unless Wifi/MQTT-connection is broken). In my home-setup I'm using [openHAB](https://www.openhab.org/) to "encapsulate" MQTT into a nice GUI, that's accessible via APP + web. I [described](https://github.com/biologist79/ESPuino/tree/master/openHAB) a sample-config for openHAB2. However, meanwhile openHAB3 is available and all the stuff described can also be configured via GUI. Be advised that openHAB is pretty complex and you have to spend some time to get familiar with it.

### Backups
As all assignments between RFID-IDs and actions (playmode, file to play...) is saved in ESP's NVS, the problem is that it's all gone when the ESP is broken. So that's where a backup comes into play. So every time you change or add a new assignment between a RFID-tag and an action via GUI, a backup-file is saved on the µSD-card. The file's name can be changed via `backupFile`. So better don't delete it! Using the webgui you can use the upload-form to import such a file.

## Smarthome (optional)
As already described, MQTT is supported. In order to use it it's necessary to run a MQTT-broker; [Mosquitto](https://mosquitto.org/) for instance. After connecting to it, ESPuino subscribes to all command-topics. State-topics are used to push states to the broker in order to inform others if anything changed (change of volume, new playlist, new track... name it). Others, like openHAB, subscribe to state-topics end send commands via command-topics. So it's not just limited to openHAB. It's just necessary to use a platform, that supports MQTT. For further informations (and pictures) refer the [subfolder](https://github.com/biologist79/ESPuino/tree/master/openHAB).

## MQTT-topics and their ranges
Feel free to use your own smarthome-environments (instead of openHAB). The MQTT-topics available are described as follows. Please note: if you want to send a command to ESPuino, you have to use a cmnd-topic whereas ESPuino pushes its states back via state-topics. So guess you want to change the volume to 8 you have to send this number via topic-variable `topicLoudnessCmnd`. Immediately after doing to, ESPuino sends a conformation of this command using `topicLoudnessState`. To get hands on MQTT I recommend this [one](https://www.hivemq.com/mqtt-essentials/) as introducton (covers more than you need for ESPuino).


| topic-variable          | range           | meaning                                                                        |
| ----------------------- | --------------- | ------------------------------------------------------------------------------ |
| topicSleepCmnd          | 0 or OFF        | Power off ESPuino immediately                                                  |
| topicSleepState         | ON or OFF       | Sends ESPuino's last state                                                     |
| topicRfidCmnd           | 12 digits       | Set number of RFID-tag which 'emulates' an RFID-tag (e.g. `123789456089`)      |
| topicRfidState          | 12 digits       | ID of current RFID-tag (if not a modification-card)                            |
| topicTrackState         | String          | Sends current track number, total number of tracks and full path of curren track. E.g. "(2/10) /mp3/kinderlieder/Ri ra rutsch.mp3" |
| topicTrackControlCmnd   | 1 -> 7          | `1`=stop; `2`=unused!; `3`=play/pause; `4`=next; `5`=prev; `6`=first; `7`=last |
| topicCoverChangedState  |                 | Indicated that the cover image has potentially changed. For performance reasons the application should load the image only if it's visible to the user |
| topicLoudnessCmnd       | 0 -> 21         | Set loudness (depends on minVolume / maxVolume)                                |
| topicLoudnessState      | 0 -> 21         | Sends loudness (depends on minVolume / maxVolume                               |
| topicSleepTimerCmnd     | EOP             | Power off after end to playlist                                                |
|                         | EOT             | Power off after end of track                                                   |
|                         | EO5T            | Power off after end of five tracks                                             |
|                         | 1 -> 2^32       | Duration in minutes to power off                                               |
|                         | 0               | Deactivate timer (if active)                                                   |
| topicSleepTimerState    | various         | Sends active timer (`EOP`, `EOT`, `EO5T`, `0`, ...)                            |
| topicState              | Online, Offline | `Online` when powering on, `Offline` when powering off                         |
| topicCurrentIPv4IP      | IPv4-string     | Sends ESPuino's IP-address (e.g. `192.168.2.78`)                               |
| topicLockControlsCmnd   | ON, OFF         | Set if controls (buttons, rotary encoder) should be locked                     |
| topicLockControlsState  | ON, OFF         | Sends if controls (buttons, rotary encoder) are locked                         |
| topicPlaymodeState      | 0 - 10          | Sends current playmode (single track, audiobook...; see playmodes)             |
| topicRepeatModeCmnd     | 0 - 3           | Set repeat-mode: `0`=no; `1`=track; `2`=playlist; `3`=both                     |
| topicRepeatModeState    | 0 - 3           | Sends repeat-mode                                                              |
| topicLedBrightnessCmnd  | 0 - 255         | Set brightness of Neopixel                                                     |
| topicLedBrightnessState | 0 - 255         | Sends brightness of Neopixel                                                   |
| topicBatteryVoltage     | float           | Voltage (e.g. 3.81)                                                            |
| topicBatterySOC         | float           | Current battery charge in percent (e.g. 83.0)                                  |
| topicWiFiRssiState      | int             | Numeric WiFi signal-strength (dBm)                                             |
| topicSRevisionState     | String          | Software-revision                                                              |
