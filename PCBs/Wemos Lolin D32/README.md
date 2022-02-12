# ESPuino-PCB based on Wemos' Lolin D32

## IMPORTANT
It's not recommended to use this one anymore for new ESPuinos as there's a bug with file-writings being interrupted with SD via SPI (reason unknown). Please use [this one](https://forum.espuino.de/t/espuino-minid32-pro-lolin-d32-pro-mit-sd-mmc-und-port-expander-smd/866) instead. Initially designed for D32 pro, but can be used for D32 as well. Please be aware that IO16 + IO17 are not used by this PCB.
## Features
* Fits Wemos Lolin D32 (not Lolin32, Lolin D32 pro or Lolin 32 lite!)
* Outer diameter: 53 x 81mm
* JST-PH 2.0-connectors for buttons (2 pins), rotary encoder (5 pins), Neopixel (3 pins), RFID (6 pins) and reset (2 pins). Please note: all of them are NOT 2.54mm pitch but 2.0mm!)
* 2.54mm-connectors for MAX98357a and µSD-card-reader. In contrast to my pictures: better solder them directly onto the PCB without having a female-connector (as socket) in between. In turned out that especially plugging MAX98357a into a female connector can lead to connectivity-issues!
* Mosfet-circuit that switches off MAX98357a, Neopixel, headphone-pcb, RFID-reader and µSD-card-reader automatically when deepsleep is active
* All peripherals are solely driven at 3.3V, as 5V isn't available in battery-mode. Keep this especially in mind when choosing µSD-reader. If in doubts use one without voltage-regulator (link below).
* If [headphone-pcb](https://github.com/biologist79/ESPuino/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308) is used, MAX98357a is automatically muted when there's a headphone plugged in and vice versa.
* If `HEADPHONE_ADJUST_ENABLE` is set and a headphone is plugged in, an alternative maximum volume is activated. I added this feature because [headphone-pcb](https://github.com/biologist79/ESPuino/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308) makes use of an amp that (probably) "allows" children to damage ears. This maximum volume can be set and re-adjusted via webgui.
* Reset-button

## Prerequisites
* If no [headphone-pcb](https://github.com/biologist79/ESPuino/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308) is connected, make sure `HEADPHONE_ADJUST_ENABLE` is disabled.
* Make sure to edit `settings.h` (HAL=3) and `settings-lolin_d32.h` according your needs.
* Disable `SD_MMC_1BIT_MODE` and `SINGLE_SPI_ENABLE` as these are not supported by this PCB.
* Enable `RFID_READER_TYPE_MFRC522_SPI` as other RFID-reader-types are not supported by this PCB.
* Enable `USEROTARY_ENABLE`.
* Enable buttons for previous, next, pause/play.

## PCB-Wiring (2 SPI-instances: RC522 + SD + 3 buttons + rotary encoder)
Uses two SPI-instances. The first one for the RFID-reader and the second for SD-card-reader. Please refer [Schematics](Pictures/Schematics.pdf) for reference.<br />
| ESP32 (GPIO)  | Hardware              | Pin    | Comment                                                      |
| ------------- | --------------------- | ------ | ------------------------------------------------------------ |
| MFET 3.3V     | SD-reader             | 3.3V   |                                                              |
| GND           | SD-reader             | GND    |                                                              |
| 15            | SD-reader             | CS     |                                                              |
| 13            | SD-reader             | MOSI   |                                                              |
| 16            | SD-reader             | MISO   |                                                              |
| 14            | SD-reader             | SCK    |                                                              |
| MFET 3.3V     | RFID-reader           | 3.3V   |                                                              |
| GND           | RFID-reader           | GND    |                                                              |
| 21            | RFID-reader           | CS/SDA |                                                              |
| 23            | RFID-reader           | MOSI   |                                                              |
| 19            | RFID-reader           | MISO   |                                                              |
| 18            | RFID-reader           | SCK    |                                                              |
| MFET 3.3V     | MAX98357              | VIN    |                                                              |
| GND           | MAX98357              | GND    |                                                              |
| 25            | MAX98357              | DIN    |                                                              |
| 27            | MAX98357              | BCLK   |                                                              |
| 26            | MAX98357              | LRC    |                                                              |
| ---           | MAX98357              | SD     | Info: if pulled down to GND amp will turn off                |
| ---           | MAX98357              | GAIN   | Pullup with 100k to 3.3V to reduce gain                      |
| 34            | Rotary encoder        | CLK    | Change CLK with DT if you want to change the direction of RE |
| 33            | Rotary encoder        | DT     | Change CLK with DT if you want to change the direction of RE |
| 32            | Rotary encoder        | BUTTON |                                                              |
| 3.3 V         | Rotary encoder        | +      |                                                              |
| GND           | Rotary encoder        | GND    |                                                              |
| 4             | Button (next)         |        |                                                              |
| GND           | Button (next)         |        |                                                              |
| 2             | Button (previous)     |        |                                                              |
| GND           | Button (previous)     |        |                                                              |
| 5             | Button (pause/play)   |        |                                                              |
| GND           | Button (pause/play)   |        |                                                              |
| MFET 3.3V     | Neopixel              | V      |                                                              |
| GND           | Neopixel              | G      |                                                              |
| 12            | Neopixel              | DI     |                                                              |
| 17            | N-channel Mosfet      | Gate   |                                                              |
| 35            | Voltage-divider / BAT |        |                                                              |
| 22            | Headphone jack        |        | Optional: if pulled to ground, headphone-volume is set       |

## Things to mention
* Better don't solder Lolin D32 directly to the PCB. I recommend to make use of female connectors (as socket) instead (link below).
* When ordering a LiPo-battery, make sure to use [one](https://www.eremit.de/p/eremit-3-7v-2500mah-lipo-104050-jst-ph-2-0mm) with deep discharge protection! This is really really really important!!!
* Doesn't support SD-MMC and RFID-reader PN5180

## Hardware-setup
The heart of my project is an ESP32 on a [Wemos Lolin D32 development-board](https://de.aliexpress.com/item/32808551116.html). Make sure to install the drivers for the USB/Serial-chip (CP2102 e.g.).
* [MAX98357A (like Adafruit's)](https://de.aliexpress.com/item/32999952454.html)
* [µSD-card-reader 3.3V only](https://www.ebay.de/itm/Micro-SPI-Kartenleser-Card-Reader-2GB-SD-8GB-SDHC-Card-3-3V-ESP8266-Arduino-NEU/333796577968)
* [RFID-reader](https://www.amazon.de/AZDelivery-Reader-Arduino-Raspberry-gratis/dp/B074S8MRQ7)
* [RFID-tags](https://www.amazon.de/AZDelivery-Keycard-56MHz-Schlüsselkarte-Karte/dp/B07TVJPTM7)
* [Neopixel-ring](https://de.aliexpress.com/item/32673883645.html)
* [Rotary Encoder](https://de.aliexpress.com/item/33041814942.html)
* [Buttons](https://de.aliexpress.com/item/32896285438.html)
* [Speaker](https://www.visaton.de/de/produkte/chassiszubehoer/breitband-systeme/fr-7-4-ohm)
* µSD-card: doesn't have to be a super-fast one; uC is limiting the throughput. Tested 32GB without any problems.
* [JSP PH-2.0-connectors](https://de.aliexpress.com/item/32968344273.html)
* [Female connector](https://de.aliexpress.com/item/32724478308.html)
* [(optional) IDC-connector female 6pin for headphone-pcb](https://de.aliexpress.com/item/33029492417.html)
* [(optional) IDC-connector male 6pin for headphone-pcb](https://de.aliexpress.com/item/1005001400147026.html)
* [LiPo-battery (2500 mAh) with connector JST PH 2.0mm](https://www.eremit.de/p/eremit-3-7v-2500mah-lipo-104050-jst-ph-2-0mm)

## Parts
* 1x IRL3103 (N-channel MOSFET) (instead of IRF530NPbF as IRL3103 has lower Vgs)
* 1x NDP6020P (P-channel MOSFET)
* 1x 1k resistor
* 1x 10k resistor
* 2x 100k resistor
* 4x JST-PH2.0-connector (2 Pins)
* 1x JST-PH2.0-connector (3 Pins)
* 1x JST-PH2.0-connector (5 Pins)
* 1x JST-PH2.0-connector (6 Pins)
* Female connector as socket for Lolin32
* (optional for headphone-PCB) 1x IDC-connecor female (6pin)
* (optional for headphone-PCB) 1x IDC-connecor male (6pin)

## Where to order?
* I ordered my PCBs at [jlcpcb](https://jlcpcb.com/). You have to order at least 5 pcs, which is only at 2$ + shipping. It took two weeks to arrive. If you want to have a look at the PCBs first (without having KiCad installed), visit [Gerberlook](https://www.gerblook.org/) and upload `gerber.zip` from the Gerberfiles-folder.

## Do I need to install KiCad?
Unless you don't want to change anything: no! All you need to provide are the gerberfiles (`gerber.zip`) to your manufactur (e.g. [jlcpcb](https://jlcpcb.com/)). However, all Kicad-files used are provided as well.
