# ESPuino-PCB based on Wemos' Lolin32 with SD_MCC + PN5180

## Introduction
After providing PCBs based on RC522 / SD-SPI it was about time to integrate "the new stuff". New stuff means SD_MMC which leads to significant better performance when it comes to HTTP/FTP-transfer and PN5180 which shows up a much better sensitivity. And finally, as option being configurable by jumper, PN5180 can be used to wake up ESPuino from deepsleep.

## Features
* Fits Wemos Lolin32 (not Lolin D32, Lolin D32 pro or Lolin 32 lite!)
* Outer diameter: 56 x 93mm (exactly like the first Lolin32-PCB)
* JST-PH 2.0-connectors for buttons, rotary encoder, Neopixel, RFID, reset and battery (not 2.54mm pitch!)
* 2.54mm-connectors for MAX98357a and µSD-card-reader.
* Mosfet-circuit that switches off MAX98357a, Neopixel, headphone-pcb, RFID-reader and µSD-card-reader automatically when deepsleep is active.
* All peripherals are solely driven at 3.3V! Keep this especially in mind when choosing µSD-reader. If in doubts use one without voltage-regulator (link below).
* If [headphone-pcb](https://github.com/biologist79/ESPuino/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308) is used, MAX98357a is automatically muted when there's a headphone plugged in and vice versa.
* If `HEADPHONE_ADJUST_ENABLE` is enabled and a headphone is plugged in, an alternative maximum volume is activated. I added this feature because [headphone-pcb](https://github.com/biologist79/ESPuino/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308) makes use of an amp that (probably) "allows" children to damage ears. This maximum volume can be set and re-adjusted via webgui.
* Reset-pinheader
* Serial-pinheader
* Jumper to configure whether PN5180 is switched off in deepleep or not.

## Prerequisites
* If no [headphone-pcb](https://github.com/biologist79/ESPuino/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308) is connected, make sure `HEADPHONE_ADJUST_ENABLE` is disabled.
* I used 130/130 kOhms-resistors as voltage-divider for `MEASURE_BATTERY_VOLTAGE`. However, make sure to use a multimeter to determine their exact values in order to achieve a better battery-measurement (was 129 kOhms in my case). They can be configured in `settings-lolin32.h` as `rdiv1` and `rdiv2`.
* In my tests, measured values were around 0.1 V too low. If you encounter such a difference you can adjust the `offsetVoltage` accordingly. But make sure to measure in battery-mode (disconnect USB!).
* `referenceVoltage` is the voltage between 3.3 V and GND on the develboard in battery-mode
* Make sure to edit `settings.h` (HAL=1) and `settings-lolin32.h` according your needs (see table below).
* Enable `SD_MMC_1BIT_MODE` and `RFID_READER_TYPE_PN5180`.
* If you want to wake up ESPuino from deepsleep via PN5180: `PN5180_ENABLE_LPCD` needs to be enabled as well.
* Disable `RFID_READER_TYPE_MFRC522_SPI` and `SINGLE_SPI_ENABLE`.
* Don't forget to add jumper for jp1! Connect the upper and the middle pin if you don't want to use LPCD. In this case PN5180 is switched off while deepsleep is active. Connect the middle and the lower pin in order to power PN5180 continously (which is mandatory for LPCD)
* Make sure [Pullup-resistor was removed](https://github.com/biologist79/ESPuino/blob/master/pictures/Pullup-removal.jpg) for GPIO2/MISO.
* Enable `USEROTARY_ENABLE`.
* Enable buttons for previous, next, pause/play.

## PCB-Wiring (2 SPI-instances: PN5180 + SD_MMC + 3 buttons + rotary encoder)
Please refer [Schematics](Pictures/Schematics.pdf) for reference.<br />
| ESP32 (GPIO)  | Hardware              | Pin    | Comment                                                      |
| ------------- | --------------------- | ------ | ------------------------------------------------------------ |
| MFET 3.3V     | SD-reader             | 3.3V   |                                                              |
| GND           | SD-reader             | GND    |                                                              |
| xx            | SD-reader             | CS     | not necessary!                                               |
| 15            | SD-reader             | MOSI   |                                                              |
| 2             | SD-reader             | MISO   |                                                              |
| 14            | SD-reader             | SCK    |                                                              |
| MFET 3.3V     | RFID-reader           | 5V     |                                                              |
| MFET 3.3V     | RFID-reader           | 3.3V   |                                                              |
| GND           | RFID-reader           | GND    |                                                              |
| 21            | RFID-reader           | NSS    |                                                              |
| 23            | RFID-reader           | MOSI   |                                                              |
| 19            | RFID-reader           | MISO   |                                                              |
| 18            | RFID-reader           | SCK    |                                                              |
| 22            | RFID-reader           | RST    |                                                              |
| 16            | RFID-reader           | BUSY   |                                                              |
| 39 (VN)       | RFID-reader           | IRQ    | Used for LCPD                                                |
| MFET 3.3V     | MAX98357              | VIN    |                                                              |
| GND           | MAX98357              | GND    |                                                              |
| 25            | MAX98357              | DIN    |                                                              |
| 27            | MAX98357              | BCLK   |                                                              |
| 26            | MAX98357              | LRC    |                                                              |
| ---           | MAX98357              | SD     | Info: if pulled down to GND amp will turn off                |
| ---           | MAX98357              | GAIN   | Pullup with 100k to 3.3V to reduce gain                      |
| 34            | Rotary encoder        | CLK    | Change CLK with DT if you want to change the direction of RE |
| 35            | Rotary encoder        | DT     | Change CLK with DT if you want to change the direction of RE |
| 32            | Rotary encoder        | BUTTON |                                                              |
| 3.3 V         | Rotary encoder        | +      |                                                              |
| GND           | Rotary encoder        | GND    |                                                              |
| 4             | Button (next)         |        |                                                              |
| GND           | Button (next)         |        |                                                              |
| 36 (VP)       | Button (previous)     |        |                                                              |
| GND           | Button (previous)     |        |                                                              |
| 5             | Button (pause/play)   |        |                                                              |
| GND           | Button (pause/play)   |        |                                                              |
| MFET 3.3V     | Neopixel              | V      |                                                              |
| GND           | Neopixel              | G      |                                                              |
| 12            | Neopixel              | DI     |                                                              |
| 17            | N-channel Mosfet      | Gate   |                                                              |
| 33            | Voltage-divider / BAT |        | Optional: voltage-divider to monitor battery-voltage         |
| 13            | Headphone jack        |        | Optional: if pulled to GND, headphone-volume is set          |

## Things to mention
* In contrast to Lolin D32, Lolin32 doesn't feature an integrated voltage-divider. That's why on the lower left there's a JST-PH2.0-connector to connect the LiPo-battery. Make sure to connect (+) to the left und GND to the right. From there you need to solder two short wires (5cm or so) onto the pcb with a JST-PH2.0-connector attached on the other side. This one needs to be plugged into Lolin32 (see pictures-folder). Please note: Lolin's JST-PH2.0-connector needs (+) left side and GND right side. Don't be confused if black/red-colouring of the JST-wires used seems "weird" because is reversed (black => (+); red => GND).
* Better don't solder Lolin32 directly to the PCB. I recommend to make use of female connectors (as socket) instead (link below).
* When ordering a LiPo-battery, make sure to use [one](https://www.eremit.de/p/eremit-3-7v-2500mah-lipo-104050-jst-ph-2-0mm) with deep discharge protection! This is really really really important!!!

## Hardware-setup
The heart of my project is an ESP32 on a [Wemos Lolin32 development-board](https://www.ebay.de/itm/4MB-Flash-WEMOS-Lolin32-V1-0-0-WIFI-Bluetooth-Card-Based-ESP-32-ESP-WROOM-32/162716855489). Make sure to install the drivers for the USB/Serial-chip (CP2102 e.g.).
* [MAX98357A (like Adafruit's)](https://de.aliexpress.com/item/32999952454.html)
* [µSD-card-reader 3.3V only](https://www.ebay.de/itm/Micro-SPI-Kartenleser-Card-Reader-2GB-SD-8GB-SDHC-Card-3-3V-ESP8266-Arduino-NEU/333796577968)
* [RFID-reader](https://www.amazon.de/CUHAWUDBA-Pn5180-Iso15693-Frequenz-Schreiber-Wie-Gezeigt/dp/B07VT1GZ4S?tag=espuino-21)
* [RFID-tags](https://www.amazon.de/AZDelivery-Keycard-56MHz-Schlüsselkarte-Karte/dp/B07TVJPTM7?tag=espuino-21)
* [Neopixel-ring](https://de.aliexpress.com/item/32673883645.html)
* [Rotary Encoder](https://de.aliexpress.com/item/33041814942.html)
* [Buttons](https://de.aliexpress.com/item/32896285438.html)
* [Speaker](https://www.visaton.de/de/produkte/chassiszubehoer/breitband-systeme/fr-7-4-ohm)
* µSD-card: doesn't have to be a super-fast one; uC is limiting the throughput. Tested 32GB without any problems.
* [JSP PH-2.0-connectors](https://de.aliexpress.com/item/32968344273.html)
* [Female connector](https://de.aliexpress.com/item/32724478308.html)
* [(optional) IDC-connector female 6pin for headphone-pcb](https://de.aliexpress.com/item/33029492417.html)
* [(optional) IDC-connector male 6pin for headphone-pcb](https://de.aliexpress.com/item/1005001400147026.html)
* [(optional) Mikro switch](https://www.ebay.de/itm/10x-Mini-Taster-Drucktaster-klein-Mikroschalter-6x6x5mm-Arduino-Raspberry-Pi/333273061003)
* [LiPo-battery (2500 mAh) with connector JST PH 2.0mm](https://www.eremit.de/p/eremit-3-7v-2500mah-lipo-104050-jst-ph-2-0mm)

## Parts
* 1x IRL3103 (N-channel MOSFET)
* 1x NDP6020P (P-channel MOSFET)
* 1x 1k resistor
* 2x 10k resistor
* 2x 100k resistor
* 2x 130k resistor (can be replaced by 100k; don't use 390k+130k as printed in the circuit => use 2x130k instead!)
* 5x JST-PH2.0-connector (2 Pins)
* 1x JST-PH2.0-connector (3 Pins)
* 1x JST-PH2.0-connector (5 Pins)
* 1x JST-PH2.0-connector (10 Pins)
* 1x Jumper (2.54 mm)
* Female connector as socket for Lolin32
* (optional for headphone-PCB) 1x IDC-connecor female (6pin)
* (optional for headphone-PCB) 1x IDC-connecor male (6pin)
* (optional) 1x 10 uF capacitor (2.0mm-pitch)

## Where to order?
I ordered my PCBs at [jlcpcb](https://jlcpcb.com/). You have to order at least 5 pcs, which is only at 2$ + shipping. It took two weeks to arrive. If you want to have a look at the PCBs first (without having KiCad installed), visit [Gerberlook](https://www.gerblook.org/) and upload `gerber.zip` from the Gerberfiles-folder.

## Do I need to install KiCad?
Unless you don't want to change anything: no! All you need to provide are the gerberfiles (`gerber.zip`) to your manufactur (e.g. [jlcpcb](https://jlcpcb.com/)). However, all Kicad-files used are provided as well.