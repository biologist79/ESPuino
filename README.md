# ESPuino - RFID-controlled Audio Player based on ESP32 with I2S-DAC Support

![build workflow](https://github.com/biologist79/ESPuino/actions/workflows/test-builds.yml/badge.svg)

## Forum

- EN: I've set up a primarily German-speaking community with much documentation. Also an
  international corner for non-German-speakers is available at <https://forum.espuino.de>.
  GitHub login can be used for signing in there (optional).
- DE: Ich habe ein primär deutschsprachiges Forum aufgesetzt, welches ich mit reichlich Doku
  versehen habe. Würde mich freuen, euch dort zu sehen: <https://forum.espuino.de>. Ihr könnt euch
  dort mit eurem Github-Login einloggen, jedoch auch "normal" anmelden. Dokumentation findet ihr
  insbesondere hier: <https://forum.espuino.de/c/dokumentation/anleitungen/10>.

## News

> :warning: By the end of october 2023, ESPuino switched framework from ESP32-Arduino1 to ESP32-Arduino2.
This brought lots of improvements but as it turned out, due to memory restrictions this version
no longer runs safely on ESP32 without PSRAM. So please make sure to use an ESP32-WROVER!

## Current development

There is a [development branch (dev)](https://github.com/biologist79/ESPuino/tree/dev) that contains
new features, that will be introduced and tested there first until they become part of the master
branch. Feel free to try but be advised this could be unstable.

## ESPuino - what's that?

The basic idea of ESPuino is to use RFID tags to control an audio player. Even for kids this concept
is simple: place a RFID-tagged object (card, toy character, etc.) on top of a box and the music
starts to play stuff from SD card or webradio. Place a different RFID tag on it and something else
is played. Simple as that.

This project is based on the popular microcontroller [ESP32 by
Espressif](https://www.espressif.com/en/products/hardware/esp32/overview). Why? It's powerful and
having WiFi support out-of-the-box enables further features like an integrated webserver,
smarthome-integration via MQTT, webradio and FTP server. And even Bluetooth, too! However, my
primary focus was to port the project to a modular base: MP3-decoding is done in software and the
digital music output is done via the popular [I2S protocol](https://en.wikipedia.org/wiki/I%C2%B2S).
So we need a [DAC](https://en.wikipedia.org/wiki/Digital-to-analog_converter) to make an analog
signal of it: I did all my tests with
[MAX98357A](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp/pinouts),
[UDA1334](https://www.adafruit.com/product/3678),
[MS6324](https://forum.espuino.de/t/kopfhoererplatine-basierend-auf-ms6324-und-tda1308/1099/) and
[PCM5102a](https://github.com/biologist79/ESPuino/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308).
General advice: ESPuino makes use of library
[ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S/); so everything that's supposed to
work with this library should work with ESPuino, too (but maybe not right out-of-the-box).
Especially this is true for
[ES8388](https://github.com/schreibfaul1/ESP32-audioI2S/blob/master/examples/ESP32_ES8388/ESP32_ES8388.ino).

## Hardware setup

You could start on a breadboard with jumper wires but I _strongly_ recommend to start right away
with a PCB that was especially developed for ESPuino. There are several available, but
[ESPuino-mini 4L (SMD)](https://forum.espuino.de/t/espuino-mini-4layer/1661) can be considered as
being the latest generation. Furthermore you need a ESP32-develboard like (or another one that's
pin compatible):

- [D32 pro LiFePO4](https://forum.espuino.de/t/esp32-develboard-d32-pro-lifepo4/1109)
- [E32 LiPo](https://forum.espuino.de/t/esp32-develboard-e32-lipo/1135)
- [Wemos Lolin D32 pro](https://www.wemos.cc/en/latest/d32/d32_pro.html)

> :warning: **Due to memory restrictions meanwhile it's mandatory to use ESP32 with
PSRAM.** This being said you need to make sure that your develboard carries an ESP32-WROVER.
And you should make sure that 16 MB flash memory is available (both is true for all
develboards named above).

Optionally a [headphone-pcb](https://forum.espuino.de/t/kopfhoererplatine-basierend-auf-ms6324-und-tda1308/1099/)
can be attached to [ESPuino-mini 4L (SMD)](https://forum.espuino.de/t/espuino-mini-4layer/1661).

However, feel free to develop PCBs yourself. But again, be advised your ESP32 needs PSRAM in order to
run ESPuino properly.

## Getting started

- [Much more documentation in german
  language](https://forum.espuino.de/c/dokumentation/anleitungen/10).
- You need to install Microsoft's [Visual Studio Code](https://code.visualstudio.com/).
- Install [PlatformIO Plugin](https://platformio.org/install/ide?install=vscode) into [Visual Studio
  Code](https://code.visualstudio.com/) and make sure to have a look at the
  [documentation](https://docs.platformio.org/en/latest/integration/ide/pioide.html).
  Step-by-step-manual is available
  [here](https://randomnerdtutorials.com/vs-code-platformio-ide-esp32-esp8266-arduino/.)
- Install [Git](https://git-scm.com/downloads) and make a copy ("clone") of my repository to your local
  computer using `git clone https://github.com/biologist79/ESPuino.git`. Using Git you can keep your
  local repository easily up to date without doing copy'n'paste. To keep it up to date run `git pull
  origin master`. Further infos
  [here](https://stackoverflow.com/questions/1443210/updating-a-local-repository-with-changes-from-a-github-repository)
  and
  [here](https://forum.espuino.de/t/espuino-in-platformio-anlegen-und-mit-git-aktuell-halten/891).
- (Optional) Install [GitLens](https://marketplace.visualstudio.com/items?itemName=eamodio.gitlens)
  as plugin (to have advanced Git features in VSCode).
- Now, that the Git repository is saved locally, import this folder into Platformio as a project.
- Select the [desired
  environment](https://forum.espuino.de/t/projekt-und-profilwechsel-in-visual-studio-code/768) (e.g.
  lolin_d32_pro_sdmmc_pe).
- Edit `src/settings.h` according your needs.
- Edit board-specific (`HAL`) config-file (e.g. `settings-lolin_d32_pro_sdmmc_pe.h` for Lolin
  D32/D32 pro). If you're running a board that is not listed there: start with `settings-custom.h`
  and change it according your needs.
- Connect your develboard via USB, click the alien-head icon in the left sidebar, choose the project
  task that matches your desired HAL and run `Upload and Monitor`. All libraries necessary are
  fetched automatically and compilation of the code gets started. After that, your ESP32 is flashed
  with the firmware. Depending on your develboard it might be necessary to push a button in order to
  allow ESP32 to enter flash mode (not necessary für Lolin32, D32 und D32 pro).
- Now have a look at the serial output at the bottom of Visual Studio Code's window. At the first
  run there might appear a few error messages (related to missing entries in NVS). Don't worry, this
  is just normal. However, make sure the SD card is detected as this is mandatory!
- If everything ran fine, at the first run, ESPuino should open an access-point and ESPuino offers a
  captive portal that is shown on your computer. If that's not the case, join a WiFi called
  "ESPuino" and enter `http://192.168.4.1` to your webbrowser. Enter WiFi credentials and the
  hostname there (or in the captive portal). After saving the configuration, restart ESPuino.
- After reboot ESPuino tries to join your WiFi (with the credentials previously entered). If that
  was successful, an IP is shown in the serial console. You can access ESPuino's GUI using a
  webbrowser via this IP; make sure to allow Javascript. If the mDNS feature is active in
  `src/settings.h`, you can use the hostname configured extended by .local instead the IP. So if you
  configured `espuino` as hostname, you can use `http://espuino.local` for web GUI and FTP.
- Via FTP and web GUI you can upload data (expect a throughput like 320 kB/s up to 700 kB/s).
- FTP needs to be activated after boot if you need it! Don't forget to assign action
  `ENABLE_FTP_SERVER` in `settings.h` to be able to activate it. Neopixel flashes green (1x) if
  enabling was successful. It'll be disabled automatically after next reboot. Means: you have to
  enable it every time you need it (if reboot was in between). Sounds annoying and maybe it is,
  but's running this way in order to have more heap-memory available (for webstream) if FTP isn't
  needed.
- Via webbrowser you can configure various settings and pair RFID tags with actions. If
  MQTT/FTP-support was not compiled, their config tabs won't appear.

## SD-card: SPI or SD-MMC (1 bit)-mode?

Having the SD card working is mandatory, ESPuino doesn't start without working SD card (at least
unless `NO_SDCARD` hasn't been enabled previously). However, there are two modes available to
interface SD cards: SPI and SDMMC (1 bit). Be advised that SDMMC is twice as fast as SPI and
needs one GPIO less. So basically it's a no-brainer.

## Which RFID-reader: RC522 or PN5180?

RC522 is so to say the ESPuino standard. It's cheap and works, but RFID tags have to be placed close
to the reader. PN5180 instead has better RFID range/sensitivity and can read ISO-15693 / iCode
SLIX2-tags aka 'Tonies' (you need a password to read Tonies), too. You can also wake up ESPuino with
the a ISO-14443 card (after flashing PN5180 with a new firmware). This feature is called LPCD.
Disadvantages PN5180: it's more expensive and needs more GPIOs (6/7 instead of 4). In my opinion
it's worth it! Refer to PN5180's wiring section below for further information. Hint: if using 3.3 V
only make sure to connect these 3.3 V to PN5180's 5 V AND 3.3 V. Sounds weird but it's necessary.

## 3.3 V or 5 V?

ESP32 runs at 3.3 V only. But what about the periphery?

- 3.3 V! Because: if you plan to use battery mode with LiPo/LiFePO4, there's no 5 V available (unless
  USB is connected or you make use of a boost converter). That's why my focus is on 3.3 V only. If
  you want to use 5 V instead - do so, but be advised it might not be compatible with battery mode.
- MAX98357A: provides more power at 5 V but also runs at 3.3 V. Anyway: it's still loud enough (in
  my opinion).
- Neopixel: specification says it needs 5 V but runs at 3.3 V as well.
- RC522: needs 3.3 V (don't ever power with 5 V!)
- PN5180: at 3.3 V make sure to connect both 5 V and 3.3 V pins to 3.3 V. If 5 V is available all
  the time: connect 5 V to 5 V and 3.3 V to 3.3 V.
- SD card: needs 3.3 V but if voltage regulator is onboard, it can be connected to 5 V as well
- Rotary encoder: 3.3 V (don't power with 5 V! Encoder doesn't care if connected to 3.3 or 5 V, but
  GPIOs of ESP32 do!)

## WiFi

WiFi is mandatory for web GUI, FTP, MQTT and webradio. However, WiFi can be temporarily or
permanently disabled (and ESPuino remembers this state after the next restart). There are two ways
to (re-)enable/disable WiFi:

- Use a special [modification card](https://forum.espuino.de/t/was-sind-modifikationskarten/37) that
  can be configured via web GUI.
- Assign action `CMD_TOGGLE_WIFI_STATUS` to a button (or multi-button). This toggles the current
  WiFi status.

## Bluetooth

> :warning: **Due to memory restrictions it's not possible to run Bluetooth in
  parallel with WiFi.** This means that you cannot stream webradio via Bluetooth
  or access the web GUI while this mode is enabled.

### ESPuino as A2DP sink (stream to ESPuino)

ESPuino can be used as Bluetooth sink (A2DP sink). In this mode you can stream audio (e.g. from a
mobile device) via Bluetooth to your ESPuino. This mode can be enabled/disabled via a RFID
modification card or by assigning action `CMD_TOGGLE_BLUETOOTH_MODE` to a button (or multi-button).
Applying this will restart ESPuino immediately. Activated Bluetooth is indicated by four
slow rotating _blue-violet_ LEDs. After the Bluetooth device is paired the LEDs turn to blue.

### ESPuino as A2DP source (stream from ESPuino)

ESPuino can also be used to stream audio (A2DP source) to a Bluetooth headset or external Bluetooth
speakers. This mode can be enabled/disabled via a RFID modification card or by assigning action
`CMD_TOGGLE_BLUETOOTH_SOURCE_MODE` to a button (or multi-button). Applying this will restart ESPuino
immediately. Activated Bluetooth is indicated by four slow rotating _blue-violet_ LEDs. After the
Bluetooth headset is connected LEDs turn to blue.

## Port expander

There might be situations where you run out of GPIOs. To address this, a port expander
[PCA9555](https://www.nxp.com/docs/en/data-sheet/PCA9555.pdf) can be used to extend the number of
input channels (output mode is only supported in special cases). PCA9555 provides 2 ports with 8
channels each - so 16 channels in total. To activate PCA9555 you need to enable
`PORT_EXPANDER_ENABLE`. Like GPIOs in your develboard-specific settings-file, you can assign
numbers. Range is `100` (port 0 channel 0) -> `115` (port 1 channel 7). Via `expanderI2cAddress` the
port expander's I2C-address can be changed. It's `0x20` if pins `A0`, `A1`, `A2` are wired to GND.

## After ESPuino is connected to your WiFi

After making ESPuino part of your LAN/WiFi, the 'regular' web GUI is available at the IP assigned by
your router (or the configured hostname). Using this GUI you can:

- configure WiFi
- make bindings between RFID tag, file/directory/URL and playback mode
- make bindings between RFID tag and a modification type
- configure MQTT (if enabled)
- configure FTP (if enabled)
- configure initial volume, maximum volume (speaker / headphone), brightness of Neopixel (night mode
  / default) and inactivity time
- configure voltage levels for battery mode
- view logs / status / current track
- control player
- run modifications (like modification card)
- upload audiofiles (called web transfer)
- do OTA updates (ESP32 with 16 MB of flash memory only)
- import + delete NVS-RFID-assigments
- restart + shutdown ESPuino

> :information_source: As you apply a RFID tag to the RFID reader, the corresponding ID is pushed to
  the GUI automatically. So there's no need to enter such IDs manually (unless you want to). The
  file path is filled out automatically by selecting a file/directory in the file browser.

<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI1.jpg"
width="30%"></img>
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI2.jpg"
width="30%"></img>
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI3.jpg"
width="30%"></img>
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI4.jpg"
width="30%"></img>
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI5.jpg"
width="30%"></img>
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI6.jpg"
width="30%"></img>
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI7.jpg"
width="30%"></img>
<img
src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt-GUI_connection_broken.jpg"
width="30%"></img>
<img src="https://raw.githubusercontent.com/biologist79/ESPuino/master/pictures/Mgmt_GUI_action_ok.jpg" width="30%"></img>

## Interacting with ESPuino

### Playback modes

It's not just simply playing music; different playback modes are supported:

- `Single track` => plays one track one time
- `Single track (loop)` => plays one track forever
- `Single track of a directory (random). Followed by sleep` => picks and plays one single track out
  of a directory and falls asleep subsequently. Neopixel gets dimmed.
- `Audiobook`=> single file or playlist/folder; last play position (file and playlist) is saved
  (when pushing pause or moving to another track) and reused next time
- `Audiobook (loop)` => same as audiobook but loops forever
- `Folder/playlist (alph. sorted)` => plays all tracks in alph. order from a folder one time
- `Folder/playlist (random order)` => plays all tracks in random order from a folder one time
- `Folder/playlist (alph. sorted)` => plays all tracks in alph. order from a folder forever
- `Folder/playlist (random order)` => plays all tracks in random order from a folder forever
- `All tracks of a random subdirectory (sorted alph.)` => plays of tracks in alph. order of a
  randomly picked subdirectory of a given directory
- `All tracks of a random subdirectory (random order)` => plays all tracks in random order of a
  randomly picked subdirectory of a given directory
- `Webradio` => always only one "track": plays a webstream
- `List (files from SD and/or webstreams) from local .m3u-File` => can be one or more files /
  webradio stations with local .m3u as sourcefile

### Modification RFID tags

There are special RFID tags, that don't start music by themselves but can modify things. If applied
a second time, it's previous action/modification will be reversed.

So first make sure to start the music then use a modification card in order to apply your desired
modification:

- Lock/unlock all buttons
- Sleep after 5/30/60/120 minutes
- Sleep after end of current track
- Sleep after end of playlist
- Sleep after five tracks
- Dim Neopixel
- Loop track
- Loop playlist
- Toggle WiFi (enable/disable) => disabling WiFi while webstream is active will stop a running
  webstream instantly!
- Toggle Bluetooth sink (enable/disable) => restarts ESPuino immediately. In this mode you can
  stream to your ESPuino via BT.
- Toggle Bluetooth source (enable/disable) => restarts ESPuino immediately. In this mode your
  ESPuino can stream via BT to an external device.

> :information_source: All sleep modes do dimming (Neopixel) automatically because it's supposed to
  be used in the evening when going to bed. Well, at least that's my children's indication :-)
>
> :information_source: Track and playlist loop mode can both be activated at the same time, but
  unless track loop isn't deactivated, playlist loop won't be effective

### Neopixel LEDs (optional)

Indicates different things. Don't forget configuration of number of LEDs via `#define NUM_LEDS`.
Most designs use a Neopixel ring, but a linear strip is also possible.

> :information_source: Some Neopixels use a reversed addressing which leads to the 'problem', that
  all effects are shown counter clockwise. If you want to change that behaviour, just enable
  `NEOPIXEL_REVERSE_ROTATION`.

#### Boot

- While booting: every second LED (rotating orange)
- Unable to mount SD: LEDs flashing red (will remain forever unless SD card is available or
  `SHUTDOWN_IF_SD_BOOT_FAILS` is active)

#### Status

- **Idle**: four LEDs slowly rotating (white if WiFi connected; green if WiFi disabled or ESPuino is
  about to connect to WiFi)
- **Bluetooth**: four LEDs slow rotating coloured blue
- **Error**: all LEDs flashing red (1x) if an action was not accepted
- **OK**: all LEDs flashing green (1x) if an action was accepted
- **Power Off**: red-circle that grows until long-press-time is reached
- **Buttons Locked**: track-progress-LEDs coloured red

#### Playback

- **Busy**: violet; four fast rotating LEDs when generating a playlist. Duration depends on the
  number of files in your playlist.
- **Track Progress**: rainbow; number of LEDs relative to play-progress
- **Playlist Progress**: blue; appears only shortly in playlist-mode with the beginning every new
  track; number of LEDs relative to progress
- **Webstream**: two slow rotating LEDs that change their colours rainbow-wise as the stream
  proceeds
- **Volume**: green => red-gradient; number of LEDs relative from current to max volume
- **Paused**: track-progress-LEDs coloured orange
- **Rewind**: if single-track-loop is activated a LED-rewind is performed when restarting the given
  track

#### Battery Status (optional)

- **Undervoltage**: flashes three times red if battery-voltage is too low. This voltage-level can be
  configured via GUI.
- Short press of rotary encoder's button provides battery-voltage visualisation via Neopixel. Upper
  und lower voltage cut-offs can be adjusted via GUI. So for example if lower voltage is set to 3.2
  V and upper voltage to 4.2 V, 50% of the LEDs indicate a voltage of 3.7 V.

### Buttons

> :warning: This section describes my default-design: 3 buttons + rotary-encoder. Feel free to
  change number of buttons (up to 5) and button-actions according your needs in `settings.h` and
  your develboard-specific config-file (e.g. `settings-lolin32.h`). At maximum you can activate five
  buttons + rotary-encoder. Minimum duration for long press (to distinguish vom short press) in ms
  is defined by `intervalToLongPress`. All actions available are listed in `src/values.h`. If using
  GPIO \>= 34 make sure to add a external pullup-resistor (10 k).

- **Previous** (short): previous track / beginning of the first track if pressed while first track
  is playing
- **Previous** (long): first track of playlist
- **Next** (short): next track of playlist
- **Next** (long): last track of playlist
- **Pause/Play** (short/long): pause/play
- **Rotary Encoder** (turning): vol +/-
- **Rotary Encoder** (button long): switch off (only when on)
- **Rotary Encoder** (button short): switch on (when switched off)
- **Rotary Encoder** (button short): show battery-voltage via Neopixel (when switched on and
  `MEASURE_BATTERY_VOLTAGE` is active)
- **Previous** (long; keep pressed) + **Next** (short) + release (both): toggle WiFi
  enabled/disabled

### Music playback

- Music starts to play right away after a valid RFID tag was applied (if it's known to ESPuino).
- If `PLAY_LAST_RFID_AFTER_REBOOT` is active, ESPuino will remember the last RFID applied =>
  music-autoplay.
- If a folder should be played that contains many MP3s, the playlist generation can take a few
  seconds.
- A file's name including path isn't allowed to exceed 255 characters.
- While the playlist is generated Neopixel indicates BUSY-mode.
- After the last track was played, Neopixel indicates IDLE-mode.

### Audiobook mode

This mode is different from the others because the last playback position is saved, when...

- next track starts.
- first/previous/last track requested by button.
- pause was pressed.
- track is over.
- playlist is over (position is reset to the first track and file position 0).
- As per default last playback position is not saved when applying a new RFID tag. You can enable
  this using `SAVE_PLAYPOS_WHEN_RFID_CHANGE`.
- As per default last playback position is not saved when doing shutdown. You can enable this using
  `SAVE_PLAYPOS_BEFORE_SHUTDOWN`.

### FTP (optional)

- FTP needs to be activated after boot! Don't forget to assign action `ENABLE_FTP_SERVER` in
  `settings.h` or use a modification card to activate it! Neopixel flashes green (1x) if enabling
  was successful. It'll be disabled automatically after next reboot. Means: you have to enable it
  every time you need it (if reboot was in between). Sounds annoying and maybe it is, but it's
  running this way in order to save heap memory when FTP isn't needed.
- Why FTP? Well: in order to avoid exposing the SD card or disassembling ESPuino all the time for
  adding new music, it's possible to transfer music to the SD card using FTP. Another possibility
  is to do via web GUI (webtransfer).
- Default user and password are set to `esp32` / `esp32` but can be changed via GUI.
- Secured FTP is not available. So make sure to disable SSL/TLS.
- Software: my recommendation is [Filezilla](https://filezilla-project.org/) as it's free and
  available for multiple platforms.
- Please note: if music is played in parallel, this rate decreases dramatically! So better stop
  playback when doing file transfers.

### Energy saving

As already described in the modifications section, there are different sleep modes available.
Additionally the ESP32 controller will be put to deep sleep after 10 minutes of inactivity
(configurable via `maxInactivityTime`) unless ESPuino doesn't play music, has a FTP client connected
and any input via buttons. Every button interaction resets the counter.

### Backups

As all assignments between RFID IDs and actions (playback mode, file to play, ...) is saved in ESP's
NVS, the problem is that it's all gone when the ESP is broken. So that's where a backup comes in
handy. Every time you change or add a new assignment between a RFID tag and an action via GUI, a
backup file is saved on the SD card. The file's name can be changed via `backupFile`. So better
don't delete it! Using the web GUI you can use the upload form to import such a file.

### Smarthome/MQTT (optional)

Everything that can be controlled via RFID tags and buttons, can also be controlled via MQTT
(excepting toggling WiFi status as this doesn't make sense). All manual interactions (buttons, RFID
tags) are also sent to MQTT in parallel, so everything is always in sync (unless
Wifi/MQTT-connection is broken).

In order to use it it's necessary to run a MQTT broker; [Mosquitto](https://mosquitto.org/) for
instance. After connecting to it, ESPuino subscribes to all command-topics. State-topics are used to
push states to the broker in order to inform others if anything changed (change of volume, new
playlist, new track, you name it).

In my home setup I'm using [openHAB](https://www.openhab.org/) to "encapsulate" MQTT into a nice
GUI, that's accessible via app + web. For further information (and pictures) refer to the [openHAB
directory](https://github.com/biologist79/ESPuino/tree/master/openHAB).

> :information_source: I [described](https://github.com/biologist79/ESPuino/tree/master/openHAB) a
  sample config for openHAB2. However, meanwhile openHAB3 is available and all the stuff described
  can also be configured via GUI. Be advised that openHAB is pretty complex and you have to spend
  some time to get familiar with it.

#### MQTT topics

Feel free to use your own smarthome environments (instead of openHAB). The MQTT topics available are
described as follows.

> :information_source: If you want to send a command to ESPuino, you have to use a cmnd-topic
  whereas ESPuino pushes its states back via state-topics. So guess you want to change the volume to
  8 you have to send this number via topic-variable `topicLoudnessCmnd`. Immediately after doing so,
  ESPuino sends a conformation of this command using `topicLoudnessState`. To get hands on MQTT I
  recommend this [one](https://www.hivemq.com/mqtt-essentials/) as introduction (covers more than
  you need for ESPuino).

| topic-variable          | range           | meaning                                                                                                                                                |
| ----------------------- | --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| topicSleepCmnd          | 0 or OFF        | Power off ESPuino immediately                                                                                                                          |
| topicSleepState         | ON or OFF       | Sends ESPuino's last state                                                                                                                             |
| topicRfidCmnd           | 12 digits       | Set number of RFID tag which 'emulates' an RFID tag (e.g. `123789456089`)                                                                              |
| topicRfidState          | 12 digits       | ID of current RFID tag (if not a modification card)                                                                                                    |
| topicTrackState         | String          | Sends current track number, total number of tracks and full path of curren track. E.g. "(2/10) /mp3/kinderlieder/Ri ra rutsch.mp3"                     |
| topicTrackControlCmnd   | 1 -> 7          | `1`=stop; `2`=unused!; `3`=play/pause; `4`=next; `5`=prev; `6`=first; `7`=last                                                                         |
| topicCoverChangedState  |                 | Indicated that the cover image has potentially changed. For performance reasons the application should load the image only if it's visible to the user |
| topicLoudnessCmnd       | 0 -> 21         | Set loudness (depends on minVolume / maxVolume)                                                                                                        |
| topicLoudnessState      | 0 -> 21         | Sends loudness (depends on minVolume / maxVolume                                                                                                       |
| topicSleepTimerCmnd     | EOP             | Power off after end to playlist                                                                                                                        |
|                         | EOT             | Power off after end of track                                                                                                                           |
|                         | EO5T            | Power off after end of five tracks                                                                                                                     |
|                         | 1 -> 2^32       | Duration in minutes to power off                                                                                                                       |
|                         | 0               | Deactivate timer (if active)                                                                                                                           |
| topicSleepTimerState    | various         | Sends active timer (`EOP`, `EOT`, `EO5T`, `0`, ...)                                                                                                    |
| topicState              | Online, Offline | `Online` when powering on, `Offline` when powering off                                                                                                 |
| topicCurrentIPv4IP      | IPv4-string     | Sends ESPuino's IP-address (e.g. `192.168.2.78`)                                                                                                       |
| topicLockControlsCmnd   | ON, OFF         | Set if controls (buttons, rotary encoder) should be locked                                                                                             |
| topicLockControlsState  | ON, OFF         | Sends if controls (buttons, rotary encoder) are locked                                                                                                 |
| topicPlaymodeState      | 0 - 10          | Sends current playback mode (single track, audiobook...; see [playback modes](#playback-modes))                                                        |
| topicRepeatModeCmnd     | 0 - 3           | Set repeat-mode: `0`=no; `1`=track; `2`=playlist; `3`=both                                                                                             |
| topicRepeatModeState    | 0 - 3           | Sends repeat-mode                                                                                                                                      |
| topicLedBrightnessCmnd  | 0 - 255         | Set brightness of Neopixel                                                                                                                             |
| topicLedBrightnessState | 0 - 255         | Sends brightness of Neopixel                                                                                                                           |
| topicBatteryVoltage     | float           | Voltage (e.g. 3.81)                                                                                                                                    |
| topicBatterySOC         | float           | Current battery charge in percent (e.g. 83.0)                                                                                                          |
| topicWiFiRssiState      | int             | Numeric WiFi signal-strength (dBm)                                                                                                                     |
| topicSRevisionState     | String          | Software-revision                                                                                                                                      |

## Development and Contributions

### Code Formatting

Automatic code formatting via `clang-format` is used. The configuration/rules can be found in
`.clang-format`. If you use Visual Studio Code as IDE, the support for this is automatically
available through the C++ extension.

When editing source code, use Ctrl+Shift+I to run the auto-formatting on the file or Ctrl+K Ctrl+F
for the selected code.

See the [documentation](https://code.visualstudio.com/docs/cpp/cpp-ide#_code-formatting) for more
options like run auto-formatting on saving or editing the file.

The CI (via "GitHub Actions") checks changes when they are pushed in order to keep the source code
clean and provide feedback to the developers and maintainers.

To keep the output of `git blame` clean despite the massive changes when introducing new formatting
rules, we have a `.git-blame-ignore-revs` that lists the commits to ignore. In VSCode this should be
used automatically if you use the "GitLens" extension. Otherwise make sure you apply the config
fragment for git by running `git config --local include.path ../.gitconfig` once in your local
clone.
