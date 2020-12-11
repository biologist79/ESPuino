# Tonuino-PCB based on Wemos' Lolin D32

## Attention
This PCB is not yet tested!

## Features
* Fits Wemos Lolin D32 (not Lolin32, Lolin D32 pro or Lolin 32 lite!)
* Outer diameter: 55 x 81mm
* JST-PH 2.0-connectors for buttons, rotary encoder, Neopixel, RFID and reset (not 2.54mm pitch!)
* 2.54mm-connectors for MAX98357a and uSD-card-reader
* Mosfet-circuit that switches off MAX98357a, Neopixel, headphone-pcb and uSD-card-reader automatically when deepsleep is active
* All peripherals are solely driven at 3.3V! Keep this especially in mind when choosing uSD-reader. If in doubts use one without voltage-regulator (link below).
* If [headphone-pcb](https://github.com/biologist79/Tonuino-ESP32-I2S/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308) is used, MAX98357a is automatically muted when there's a headphone plugged in and vice versa.
* If `HEADPHONE_ADJUST_ENABLE` is set and a headphone is plugged in, an alternative maximum volume is activated. I added this feature because [headphone-pcb](https://github.com/biologist79/Tonuino-ESP32-I2S/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308) makes use of an amp that (probably) "allows" children to damage ears. This maximum volume can be set and re-adjusted via webgui.
* Reset-button

## Prerequisites
* If no [headphone-pcb](https://github.com/biologist79/Tonuino-ESP32-I2S/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308) is connected, make sure `HEADPHONE_ADJUST_ENABLE` is not active.
* Make sure to change values of voltage divider's restors. They can be configured in `settings.h` as `rdiv1` and `rdiv2` and need to be set to 100k/100k.
* Change `VOLTAGE_READ_PIN` to 35.

## Things to mention
* Better don't solder Lolin D32 directly to the PCB. I recommend to make use of female connectors instead (link below).
* When ordering a LiPo-battery, make sure to use [one](https://www.eremit.de/p/eremit-3-7v-2500mah-lipo-104050-jst-ph-2-0mm) with deep discharge protection! This is really really really important!!!

## Hardware-setup
The heart of my project is an ESP32 on a [Wemos Lolin D32 development-board](https://de.aliexpress.com/item/32808551116.html). Make sure to install the drivers for the USB/Serial-chip (CP2102 e.g.).
* [MAX98357A (like Adafruit's)](https://de.aliexpress.com/item/32999952454.html)
* [uSD-card-reader 3.3V only](https://www.ebay.de/itm/Micro-SPI-Kartenleser-Card-Reader-2GB-SD-8GB-SDHC-Card-3-3V-ESP8266-Arduino-NEU/333796577968)
* [RFID-reader](https://www.amazon.de/AZDelivery-Reader-Arduino-Raspberry-gratis/dp/B074S8MRQ7)
* [RFID-tags](https://www.amazon.de/AZDelivery-Keycard-56MHz-Schlüsselkarte-Karte/dp/B07TVJPTM7)
* [Neopixel-ring](https://de.aliexpress.com/item/32673883645.html)
* [Rotary Encoder](https://de.aliexpress.com/item/33041814942.html)
* [Buttons](https://de.aliexpress.com/item/32896285438.html)
* [Speaker](https://www.visaton.de/de/produkte/chassiszubehoer/breitband-systeme/fr-7-4-ohm)
* uSD-card: doesn't have to be a super-fast one; uC is limiting the throughput. Tested 32GB without any problems.
* [JSP PH-2.0-connectors](https://de.aliexpress.com/item/32968344273.html)
* [Female connector](https://de.aliexpress.com/item/32724478308.html)
* [(optional) IDC-connector female 6pin for headphone-pcb](https://de.aliexpress.com/item/33029492417.html)
* [(optional) IDC-connector male 6pin for headphone-pcb](https://de.aliexpress.com/item/1005001400147026.html)
* [LiPo-battery (2500 mAh) with connector JST PH 2.0mm](https://www.eremit.de/p/eremit-3-7v-2500mah-lipo-104050-jst-ph-2-0mm)

## Parts
* 1x IRF530NPbF (N-channel MOSFET)
* 1x NDP6020P (P-channel MOSFET)
* 1x 1k resistor
* 1x 10k resistor
* 2x 100k resistor
* 4x JST-PH2.0-connector (2 Pins)
* 3x JST-PH2.0-connector (3 Pins)
* 1x JST-PH2.0-connector (5 Pins)
* Female connector as socket for Lolin32, uSD-reader and MAX98357a
* (optional for headphone-PCB) 1x IDC-connecor female (6pin)
* (optional for headphone-PCB) 1x IDC-connecor male (6pin)

## Where to order?
* I ordered my PCBs at [jlcpcb](https://jlcpcb.com/). You have to order at least 5 pcs, which is only at 2$ + shipping. It took two weeks to arrive. If you want to have a look at the PCBs first (without having KiCad installed), visit [Gerberlook](https://www.gerblook.org/) and upload `gerber.zip` from the Gerberfiles-folder.
* Enter 31 x 31mm as dimensions in JLCPCB's order-form.

## Do I need to install KiCad?
Unless you don't want to change anything: no! All you need to provide are the gerberfiles (`gerber.zip`) to your manufactur (e.g. [jlcpcb](https://jlcpcb.com/)). However, all Kicad-files used are provided as well.
