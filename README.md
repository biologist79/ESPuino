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

## Firmwares

Ready-to-use firmwares are available for [download](https://github.com/biologist79/ESPuino-Firmware) for several
HALs with or without bluetooth support enabled. These are provided for
[master](https://github.com/biologist79/ESPuino-Firmware/tree/main/Firmwares/master) and
[dev-branch](https://github.com/biologist79/ESPuino-Firmware/tree/main/Firmwares/dev).

## News

> :warning: Type to rfid (PN5180, RC522-spi, RC522-i2c is now being autodetected at start)

> :warning: Due to memory restrictions and complexity, ESPuino doesn't run safely on ESP32
without PSRAM. So please make sure to use an ESP32-WROVER!

> :warning: As of January 8, 2026, MQTT topics have changed! Further infos [here](https://forum.espuino.de/t/neues-namensschema-fuer-mqtt/2553/34).

## Current development

There is a [development branch (dev)](https://github.com/biologist79/ESPuino/tree/dev) that contains
new features, that will be introduced and tested there first until they become part of the master
branch. Feel free to test but be advised this could be unstable.

## ESPuino - what's that?

The concept of ESPuino is a [more-or-less small enclosure](https://forum.espuino.de/t/zeigt-her-eure-espuinos/554/)
that contains a speaker so it can play music and audiobooks. Here is an example of a 3D-printed
unit, the [Biobox 3d](https://forum.espuino.de/t/biobox-3d/3130):

![Biobox 3d](https://forum.espuino.de/uploads/default/original/2X/d/d8e0904fd78723dbc1b5c039ab9da2d778c7c987.jpeg "Biobox 3d")

Primarily the device is controlled using RFID tags (usually cards), similar to those used for access control
(ski lifts, office access, etc.). To uniquely identify an RFID card, it carries a fixed ID.
ESPuino reads that ID (a 12-digit number) with an RFID card reader and triggers an action associated
on it. Which action to take is taught beforehand via ESPuino’s web interface: you upload audio
files to a microSD card, pick the desired audio file (or an entire folder) in the web interface’s
file browser and link it to the RFID card you placed. You mostly just click through the
web interface while manual input is possible (but usually unnecessary). When the RFID tag is presented
again, ESPuino loads the mapping and starts the desired playback. Present a different RFID tag and
the playback associated with that tag starts. In addition to audio files from the local microSD
card, web streams can be played, too. In Bluetooth mode you can also stream to the ESPuino from
your phone using [A2DP](https://de.wikipedia.org/wiki/A2DP), for example.

Further control elements on ESPuino are buttons and a [rotary encoder](https://forum.espuino.de/t/drehencoder-by-espuino/2414).
Buttons can be assigned dozens of different actions via the web interface — even double assignments (short
press vs. long press) are possible. By default, turning the rotary encoder controls the volume, but it
includes an integrated button, that's usually used to switch on ESPuino and
starts measurement of battery's voltage which is instantly indicated via LEDs (Neopixel). Additionally,
holding down a configured button while turning the encoder runs that button's own assigned rotary action
instead - e.g. seeking within a track or adjusting Neopixel brightness - freely configurable per
button and turning direction via the web interface. By default,
ESPuino is designed for three buttons and one rotary encoder, but this can be varied: from 0 up to 5
buttons, and no rotaty encoder or one rotaty encoder. If no rotary encoder is present, volume
control is handled, for example, via buttons.

A key feedback element is color-programmable LEDs (Neopixels), typically used as a
[Neopixel ring](https://duckduckgo.com/?t=ffab&q=neopixel+ring&ia=images&iax=images). They can
show, for example, track progress (if half the LEDs are lit, the track is half done), volume,
battery level, and error states — and
[much, much more](https://forum.espuino.de/t/was-zeigt-der-neopixel-des-espuino-alles-an/86)! ESPuino
also works without Neopixels, but that is not recommended because they convey a lot of information.

There is also an optional [headphone jack](https://forum.espuino.de/t/kopfhoererplatine-basierend-auf-ms6324-und-tda1308-bzw-lm4808m/1099)
that mutes the speaker when headphones are plugged in. Alternatively, Bluetooth headphones can be used.

ESPuino also communicates with the outside world: besides the web interface, it supports
[MQTT](https://de.wikipedia.org/wiki/MQTT), a [REST API](https://github.com/biologist79/ESPuino/blob/master/REST-API.yaml)
and an FTP server. New audio files can be copied to the microSD card via the web interface or
FTP — so the microSD card does not need to be removed.

## Hardware setup

You could start on a breadboard with jumper wires. However, the simplier way is to kickstart right away
with a PCB that was especially developed for ESPuino. There are [several available](https://forum.espuino.de/c/hardware/pcbs/11),
but ["ESPuino Complete"](https://forum.espuino.de/t/espuino-complete/3817) can be considered being the
latest one. This pcb can obtained via the [forum](https://forum.espuino.de/). It covers a charger
for LiFePO4 or LiPo batteries (for sure also runs with USB-C only) and provides connectors for up to 5 buttons,
rfid-reader, rotary encoder, i2c external, Neopixel, USB-C, µSD, speaker, [headphones PCB](https://forum.espuino.de/t/kopfhoererplatine-basierend-auf-ms6324-und-tda1308-bzw-lm4808m/1099) and some custom stuff.
[Port-expander](https://www.nxp.com/docs/en/data-sheet/PCA9555.pdf) and [MAX98357a](https://www.analog.com/en/products/MAX98357A.html)
are integrated, too. A [buck boost converter](https://www.ti.com/product/TPS63000) provides
stable 3.3 V while battery's voltage is [supervised](https://www.sg-micro.com/product/SGM809)
in order to prevent it from deep discharge. However, NEVER(!) use a lithium battery without a
further protection circuit that's already part of the battery pack!

![Complete](https://forum.espuino.de/uploads/default/original/2X/7/750a5af3cf71bc7ef35f9adc7054c981f169f96b.jpeg "Complete")

> :warning: However, feel free to develop PCBs yourself. But again, be advised your ESP32 needs PSRAM in order to run ESPuino properly.

Optionally a [headphone-pcb](https://forum.espuino.de/t/kopfhoererplatine-basierend-auf-ms6324-und-tda1308/1099/)
can be attached to [Complete](https://forum.espuino.de/t/espuino-complete/3817) in order to connect headphones.
While headphone's plugged in, speaker is automatically disabled.

## Getting started (for users)

- [Much more documentation in german
  language](https://forum.espuino.de/c/dokumentation/anleitungen/10).
- There are already ready-to-use firmwares for [download available](https://github.com/biologist79/ESPuino-Firmware) that
  probably already fit your needs. Further informations can be found [here](https://forum.espuino.de/t/fertige-espuino-firmwares-zum-runterladen/3941). [Complete](https://forum.espuino.de/t/espuino-complete/3817) is always already shipped with running firmware, so your ESPuino should start right away.
- ESPuino opens a wifi access point that you need to join. For further details please refer [the first start](https://forum.espuino.de/t/der-erste-start-deines-espuino/29).
- Please make sure to read the [documentation of the web interface](https://forum.espuino.de/t/dokumentation-webinterface/2807).

## Getting started (for developers)

- In case you want (or need) to compile your own firmware, first you need to install Microsoft's [Visual Studio Code](https://code.visualstudio.com/).
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
  complete).
- Edit `src/settings.h` according your needs.
- Edit board-specific (`HAL`) config-file (e.g. `settings-complete.h` for Complete PCB) and switch to
  that HAL in VSC. If you're running a board that is not listed there: start with `settings-custom.h`
  and change it according your needs.
- Connect your PCB/develboard via USB, click the alien-head icon in the left sidebar, choose the project
  task that matches your desired HAL and run `Upload and Monitor`. All libraries necessary are
  fetched automatically and compilation of the code gets started. After that, your ESP32 is flashed
  with the firmware.
- Now have a look at the serial output at the bottom of Visual Studio Code's window. At the first
  run there might appear a few error messages (related to missing entries in NVS). Don't worry, this
  is just normal. However, make sure the µSD card is detected as this is mandatory for ESPuino to boot!
- If everything is fine, at the first run, ESPuino should open an access-point and ESPuino offers a
  captive portal (available in your browser's language, if supported) that is shown on your computer.
  If that's not the case, join a WiFi called
  "ESPuino" and enter `http://192.168.4.1` to your webbrowser. Enter WiFi credentials and the
  hostname there (or in the captive portal). Before saving, you can test the entered credentials
  directly from this page: ESPuino tries connecting to your WiFi in the background while the setup
  access point stays up, and reports success or a specific failure reason (e.g. wrong password,
  network not found) right there - no need to save, reboot and hope. After saving the configuration,
  restart ESPuino.
- After reboot ESPuino tries to join your WiFi (with the credentials previously entered). If that
  was successful, an IP is shown in the serial console. You can access ESPuino's web interface using a
  webbrowser via this IP; make sure to allow Javascript. If the mDNS feature is active in
  `src/settings.h`, you can use the hostname configured extended by .local instead the IP. So if you
  configured `espuino` as hostname, you can use `http://espuino.local` for web interface and FTP.
- Via FTP and web interface you can upload data (expect a throughput of roughly 650 kB/s via the web interface).
- FTP needs to be activated after boot if you need it! Don't forget to assign action
  `ENABLE_FTP_SERVER` in `settings.h` to be able to activate it. Neopixel flashes green (1x) if
  enabling was successful. It'll be disabled automatically after next reboot. Means: you have to
  enable it every time you need it (if reboot was in between). Sounds annoying and maybe it is,
  but's running this way in order to have more heap-memory available (for webstream) if FTP isn't
  needed.
- Via webbrowser you can configure various settings and pair RFID tags with actions. If
  MQTT/FTP-support was not compiled, their config tabs won't appear.

## SD-card: SPI or SD-MMC (1 bit)-mode? (for developers)

Having the µSD card working is mandatory, ESPuino doesn't start without working SD card (at least
unless `NO_SDCARD` hasn't been enabled previously). However, there are two modes available to
interface µSD cards: SPI and SDMMC (1 bit). Be advised that SDMMC is twice as fast as SPI and
needs one GPIO less. So basically it's a no-brainer and SDMMC is used for "ESPuino Complete".

## Which RFID-reader: RC522 or PN5180?

RC522 is in a way the ESPuino standard: it's cheap and works, but RFID tags have to be placed close
to the reader. PN5180 instead has better RFID range/sensitivity and can read ISO-15693 / iCode
SLIX2-tags aka 'Tonies' (you need a password to read Tonies), too. You can also wake up ESPuino with
the a RFID tag (after flashing PN5180 with a new firmware). This feature is called LPCD.
Disadvantages PN5180: it's more expensive and needs more GPIOs (6/7 instead of 4). In my opinion
it's worth it! Refer to PN5180's wiring section below for further information. Hint: if using 3.3 V
only make sure to connect these 3.3 V to PN5180's 5 V AND 3.3 V. Sounds weird but it's necessary.

## 3.3 V only or 5 V, too? (for developers)

ESP32 itself runs at 3.3 V only. But what about the periphery? Spoiler: "Complete" internally runs at
3.3 V only.

- My opinion: 3.3 V! Because it's simple. If you plan to use battery mode with LiPo/LiFePO4,
  there's no 5 V available (unless USB is connected or you make use of a boost converter).
- MAX98357A: provides more power at 5 V but also runs at 3.3 V. Anyway: it's still loud enough (in
  my opinion).
- Neopixel: specification says it needs 5 V but runs at 3.3 V absolutely fine.
- RC522: needs 3.3 V (don't ever power with 5 V!)
- PN5180: at 3.3 V make sure to connect both 5 V and 3.3 V pins to 3.3 V. If 5 V is available all
  the time: connect 5 V to 5 V and 3.3 V to 3.3 V.
- SD card: needs 3.3 V but if voltage regulator is onboard, it can be connected to 5 V as well
- Rotary encoder: 3.3 V (don't power with 5 V! Encoder doesn't care if connected to 3.3 or 5 V, but
  GPIOs of ESP32 do!)

## WiFi

WiFi is mandatory for web interface, FTP, MQTT and webradio. However, it can be temporarily or
permanently disabled (and ESPuino remembers this state after the next restart). There are two ways
to (re-)enable/disable WiFi:

- Use a special [modification card](https://forum.espuino.de/t/was-sind-modifikationskarten/37) that
  can be configured via web interface.
- Assign action `CMD_TOGGLE_WIFI_STATUS` to a button (or multi-button). This toggles the current
  WiFi status.

## Bluetooth

> :warning: **Bluetooth can now run in parallel with WiFi**, so streaming webradio via
  Bluetooth or accessing the web interface while a Bluetooth mode is enabled is possible.
  However, this combination is still not well tested and both stacks running at once puts
  considerably more pressure on the ESP32's limited RAM, so memory problems (up to crashes)
  are possible - use with caution. Switching between Bluetooth modes (or back to normal
  mode) still requires a restart.

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

## Port expander (for developers)

There might be situations where you run out of GPIOs. To address this, a port expander
[PCA9555](https://www.nxp.com/docs/en/data-sheet/PCA9555.pdf) can be used to extend the number of
input channels (output mode is only supported in special cases). PCA9555 provides 2 ports with 8
channels each - so 16 channels in total. To activate PCA9555 you need to enable
`PORT_EXPANDER_ENABLE`. Like GPIOs in your develboard-specific settings-file, you can assign
numbers. Range is `100` (port 0 channel 0) -> `115` (port 1 channel 7). Via `expanderI2cAddress` the
port expander's I2C-address can be changed. It's `0x20` if pins `A0`, `A1`, `A2` are wired to GND.

## After ESPuino is connected to your WiFi

After making ESPuino part of your LAN/WiFi, the 'regular' web interface is available at the IP assigned by
your router (or the configured hostname). Using this interface you can:

- configure WiFi
- make bindings between RFID tag, file/directory/URL and playback mode
- make bindings between RFID tag and a modification type
- configure MQTT and its topics (if enabled)
- configure FTP (if enabled)
- configure initial volume, minimum volume, maximum volume (speaker / headphone), brightness of
  Neopixel (night mode / default) and inactivity time
- configure voltage levels for battery mode (and optionally let ESPuino shut down automatically on
  critically low voltage)
- configure type of rfid reader (however, not necessary because of [autodetection](https://forum.espuino.de/t/autoerkennung-von-rfid-reader/4453))
- view logs / status / current track / cover art (if available)
- control player
- run modifications (like modification card)
- upload audiofiles (called web transfer)
- do OTA updates (ESP32-WROVER with 16 MB of flash memory only)
- import + delete NVS-RFID-assigments
- restart + shutdown ESPuino

> :information_source: As you apply a RFID tag to the RFID reader, the corresponding ID is pushed to
  the web interface automatically. So there's no need to enter such IDs manually (unless you want to). The
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

| Type      | Action     |
| ------------- | ------------- |
| `Single track` | Plays one track one time |
| `Single track (loop)` | Plays one track forever |
| `Single track of a directory (random). Followed by sleep` | Picks and plays one single track out of a directory and falls asleep subsequently. Neopixel gets dimmed.|
| `Audiobook` | Single file or playlist/folder; last play position (file and playlist) is saved (when pushing pause or moving to another track) and reused next time |
| `Audiobook (loop)` | Same as audiobook but loops forever |
| `Audiobook with subdirectories (recursive)` | Same as audiobook, but with subdirectories |
| `Folder/playlist (sorted)` | Plays all tracks in order from a folder one time |
| `Folder/playlist (random order)` | Plays all tracks in random order from a folder one time |
| `Folder/playlist (sorted)` | Plays all tracks in order from a folder forever|
| `Folder/playlist (random order)` | Plays all tracks in random order from a folder forever |
| `All tracks of a random subdirectory (sorted)` | Plays of tracks in order of a randomly picked subdirectory of a given directory |
| `All tracks of a random subdirectory (random order)` | Plays all tracks in random order of a randomly picked subdirectory of a given directory |
| `All tracks of a directory + subdirectories (random, recursive)` | Same as All tracks of a random subdirectory (random order) but with subdirectories |
| `All tracks of a directory + subdirectories (sorted, recursive)` | Same as All tracks of a random subdirectory (sorted) but with subdirectories |
| `Webradio` | always only one "track": plays a webstream |
| `List (files from SD and/or webstreams) from local .m3u-File` | Can be one or more files / webradio stations with local .m3u as sourcefile |

### Modification RFID tags

There are special RFID tags, that don't start music by themselves but can modify ESPuinos behaviour. If applied
a second time, it's previous action/modification will be reversed.

So first make sure to start the music then use a modification card in order to apply your desired
modification:

| Type      | Action     |
| ------------- | ------------- |
| Lock/unlock all buttons | Locks / unlocks all buttons (might be useful for toddlers|
| Sleep after 5/30/60/120 minutes | ESPuino falls asleep automatically after the given period |
| Sleep after end of current track | ESPuino falls asleep after the current track |
| Sleep after end of playlist | ESPuino falls asleep after the current playlist |
| Sleep after five tracks | ESPuino falls asleep after five tracks |
| Dim Neopixel | Dim Neopixel to nightmode |
| Loop track | Loops / unloops current track |
| Loop playlist | Loops / unloops current playlist |
| Toggle WiFi (enable/disable) | Enables / disables WiFi. Please note: disabling WiFi while webstream is active will stop a running webstream instantly! |
| Toggle Bluetooth sink (enable/disable) | Restarts ESPuino immediately. In this mode you can stream to your ESPuino via BT whereas websteam / SD is not available. Hint: if you lost this modification card you can 'escape' this mode with an RFID tag that's unknown to ESPuino. |
| Toggle Bluetooth source (enable/disable) | Restarts ESPuino immediately. In this mode your ESPuino can stream via BT to an external device whereas websteam / SD is not available. Hint: if you lost this modification card you can 'escape' this mode with an RFID tag that's unknown to ESPuino.|
| Toggle through the different modes | Normal => BT-Sink => BT-Source => Normal |
| Speech output of IP-address or current time | Speech output of IP address or current time |
| Toggle Ambient Light | Enables / disables ambient light |

> :information_source: All sleep modes do dimming (Neopixel) automatically because it's supposed to
  be used in the evening when going to bed. Well, at least that's my children's indication :-)
>
> :information_source: Track and playlist loop mode can both be activated at the same time, but
  unless track loop isn't deactivated, playlist loop won't be effective

### Neopixel LEDs (optional)

Indicates various things. Don't forget configuration of number of LEDs via via webinterface.
Most ESPuino designs make use of Neopixel rings, but a linear strip is also possible. Even a single
LED can be used.

> :information_source: Some Neopixels use a reversed addressing which leads to the 'problem', that
  all effects are shown counter clockwise. If you want to change that behaviour, just change it via webinterface.

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
  configured via web interface. Optionally, ESPuino can be told to shut down automatically once the
  voltage drops below a configurable critical level (off by default; enable it via web interface).
- Short press of rotary encoder's button provides battery-voltage visualisation via Neopixel. Upper
  und lower voltage cut-offs can be adjusted via web interface. So for example if lower voltage is set to 3.2
  V and upper voltage to 4.2 V, 50% of the LEDs indicate a voltage of 3.7 V.

### Buttons

> :warning: This section describes my default-design: 3 buttons + rotary-encoder. Feel free to
  change number of buttons (up to 5) and button-actions according your needs in `settings.h` and
  your develboard-specific config-file (e.g. `settings-complete.h`). At maximum you can activate five
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
- **Any button** (held) + **Rotary Encoder** (turning): runs that button's own configured rotary
  action (e.g. seek within the track, adjust Neopixel brightness) instead of changing the volume -
  freely assignable per button and turning direction via the web interface, off (`--`) by default
  for most buttons

#### Virtual RFID cards

Any of the button actions can also be assigned to [virtual RFID cards](https://forum.espuino.de/t/virtual-rfid-cards/3218/2).
Those cards then can be assigned on the web interface like normal cards.
To select a virtual RFID card, just press the configured button action,
the virtual RFID automatically gets filled in in the web interface.

### Audiobook mode

This mode is different from the others because the last playback position is saved, when...

- next track starts.
- first/previous/last track requested by button.
- pause was pressed.
- track is over.
- playlist is over (position is reset to the first track and file position 0).
- As per default last playback position is not saved when applying a new RFID tag. You can change this
  this behaviour via webinterface.
- As per default last playback position is not saved when doing shutdown. You can change this
  this behaviour via webinterface.

### FTP (optional)

- FTP needs to be activated after boot! Neopixel flashes green (1x) if enabling
  was successful. It'll be disabled automatically after next reboot. Means: you have to enable it
  every time you need it (if reboot was in between). Sounds annoying and maybe it is, but it's
  running this way in order to save heap memory when FTP isn't needed.
- Why FTP? In order to avoid exposing the SD card or disassembling ESPuino all the time for
  adding new music, it's possible to transfer music to the SD card using FTP. Another possibility
  is to do via web interface (webtransfer).
- Default user and password are set to `esp32` / `esp32` but can be changed via web interface.
- Secured FTP is not available. So make sure to disable SSL/TLS.
- Software: my recommendation is [Filezilla](https://filezilla-project.org/) as it's free and
  available for multiple platforms.
- Please note: if music is played in parallel, the transfer rate decreases! So consider to stop
  playback when doing file transfers.

### Energy saving

As already described in the modifications section, there are different sleep modes available.
Additionally the ESP32 controller will be put to deep sleep after 10 minutes of inactivity
(configurable via `maxInactivityTime`) unless ESPuino doesn't play music, has a FTP client connected
and any input via buttons. Every button interaction resets the counter.

### Backups

As all assignments between RFID IDs and actions (playback mode, file to play, ...) is saved in ESP32's
NVS, the problem is that it's all gone when the ESP is broken. So that's where a backup comes in
handy. Every time you change or add a new assignment between a RFID tag and an action via web interface, a
backup file is saved on the SD card. The file's name can be changed via `backupFile`. So better
don't delete it! Using the web interface you can use the upload form to import such a file.

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
  sample config for openHAB2. However, meanwhile openHAB5 is available and all the stuff described
  can also be configured via web interface. Be advised that openHAB is pretty complex and you have to spend
  some time to get familiar with it.

However, there's also stuff available for [Home Assistant](https://forum.espuino.de/t/home-assistant-integration/3763).

#### MQTT topics

Feel free to use your own smarthome environments (Home Assistant, Node Red...). The MQTT topics available are
described as follows.

> :information_source: Topics follow the pattern: `[<base_topic>/]device_id/topic[/setter_token]`. Command topics use the optional setter token appended to the topic (for example: `<baseTopic>/<deviceId>/<topic>/<setter_token>`, e.g. `home/ESPuino-01/loudness/set`), while state topics are published on the topic without the setter token (for example: `<baseTopic>/<deviceId>/<topic>`, e.g. `home/ESPuino-01/loudness`). For example, to change the volume to `8`, publish `8` to the command topic for `topicLoudness` (e.g. `.../loudness/set`); ESPuino will confirm by publishing the value on the state topic for `topicLoudness` (`.../loudness`). To get hands on MQTT I recommend this [one](https://www.hivemq.com/mqtt-essentials/) as introduction (covers more than you need for ESPuino).

| topic-variable          | range           | meaning                                                                                                                                                |
| ----------------------- | --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| topicSleep              | Cmnd: 0 or OFF; State: ON or OFF | Cmnd: power off (send '0' or 'OFF'); State: sends current power state                                                                                 |
| topicRfid               | Cmnd/State: 12 digits | Cmnd: emulate an RFID tag by sending a 12-digit id (e.g. `123789456089`); State: current RFID tag id                                                   |
| topicTrack              | String          | State: current track info (e.g. "(2/10) /mp3/.../file.mp3")                                                                                         |
| topicTrackControl       | 1 -> 9          | Cmnd: playback control (`1`=stop; `3`=play/pause; `4`=next; `5`=prev; `6`=first; `7`=last; `8`=next folder; `9`=previous folder)                     |
| topicCoverChanged       | (flag)          | State: indicates cover image may have changed (load only if visible)                                                                                |
| topicLoudness           | Cmnd/State: 0 -> 21 | Set/report loudness (depends on minVolume / maxVolume)                                                                                               |
| topicSleepTimer         | Cmnd/State: EOP / EOT / EO5T / 1->2^32 / 0 | Cmnd: set sleep timer (EOP/EOT/EO5T or minutes; 0 to deactivate). State: current timer value (e.g. `EOP`, `EOT`, `EO5T`, `0`, ...) |
| topicState              | Online, Offline | `Online` when powering on, `Offline` when powering off                                                                                                 |
| topicCurrentIPv4IP      | IPv4-string     | Sends ESPuino's IP-address (e.g. `192.168.2.78`)                                                                                                       |
| topicPausePlay          | idle, play, pause | Sends playback state: `idle` (no playback), `play` (playing), `pause` (paused)                                                                         |
| topicLockControls       | Cmnd/State: ON or OFF | Set/report whether physical controls are locked                                                                                                        |
| topicPlaymode           | 0 - 10          | State: current playback mode (single track, audiobook...; see [playback modes](#playback-modes))                                                        |
| topicRepeatMode         | Cmnd/State: 0 - 3 | Set/report repeat mode (`0`=no; `1`=track; `2`=playlist; `3`=both)                                                                                    |
| topicLedBrightness      | Cmnd/State: 0 - 255 | Set/report Neopixel brightness                                                                                                                         |
| topicBatteryVoltage     | float           | Voltage (e.g. 3.81)                                                                                                                                    |
| topicBatterySOC         | float           | Current battery charge in percent (e.g. 83.0)                                                                                                          |
| topicWiFiRssi           | int             | State: numeric WiFi signal-strength (dBm)                                                                                                             |
| topicSRevision          | String          | State: software revision string                                                                                                                        |

### REST API

Even [REST API](./REST-API.yaml) is available with various GET, POST, DELETE and PUT-calls.

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
