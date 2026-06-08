# PN5180-Library

Arduino Uno / Arduino ESP-32 library for PN5180-NFC Module from NXP Semiconductors

![PN5180-NFC module](./doc/PN5180-NFC.png)
![PN5180 Schematics](./doc/FritzingLayout.jpg)

Release Notes:

Version 2.3.7 - 01.09.2025
	* ISO14443: Explicitly allow unknown manufacturer ID 0xFF, thanks to tom !

Version 2.3.6 - 03.06.2025
	*  Allow to change SPI frequency #16, thanks to @saidmoya12 !
	
Version 2.3.5 - 15.05.2025

	*  Less blocking delays when using other tasks #15, thanks to @joe91 !

Version 2.3.4 - 31.10.2024

	* Better debug #13, thanks to @mjmeans !
	* Fix warning "will be initialized after [-Wreorder]"
	* suppress [-Wunused-variable]
	
Version 2.3.3 - 27.10.2024

	* Bugfix start with default SPI
	
Version 2.3.2 - 27.10.2024

	* Allow to use custom spi pins #12, thanks to @mjmeans !
	* Create .gitattributes #10, thanks to @mjmeans 
	* replace errno, which is often a macro, thanks to @egnor

Version 2.3.1 - 01.10.2024

	* create a release with new version numbering

Version 2.3 - 29.05.2024

	* cppcheck: make some params const
	* transceiveCommand: restore state of SS in case of an error 
	
Version 2.2 - 13.01.2024

	* Add code to allow authentication with Mifare Classic cards, thanks to @golyalpha !
	* Unify and simplify command creation
	* Smaller memory footprint if reading UID only (save ~500 Bytes)
	* Refactor SPI transaction into transceiveCommand()

Version 2.1 - 01.04.2023

	* readMultipleBlock(), getInventoryMultiple() and getInventoryPoll() has been implemented, thanks to @laplacier !
	* multiple ISO-15693 tags can be read at once, thanks to @laplacier ! 
	* fixed some bugs and warnings with c++17
	
Version 2.0 - 17.10.2022

	* allow to instantiate with custom SPI class, thanks to  @mwick83!
	
Version 1.9 - 05.10.2021

	* avoid endless loop in reset()
	
Version 1.8 - 05.04.2021

	* Revert previous changes, SPI class was copied and caused problems
	* Speedup reading (shorter delays) in reset/transceive commands
	* better initialization for ISO-14443 cards, see https://www.nxp.com.cn/docs/en/application-note/AN12650.pdf
	
Version 1.7 - 27.03.2021

	* allow to setup with other SPIClass than default SPI, thanks to @tyllmoritz !
	
Version 1.6 - 31.01.2021

	* fix compiler warnings for platform.io
	* add LPCD (low power card detection) example for ESP-32 (with deep sleep tp save battery power)

	Version 1.5 - 07.12.2020

	* ISO-14443 protocol, basic support for Mifaire cards
	* Low power card detection
	* handle transceiveCommand timeout

Version 1.4 - 13.11.2019

	* ICODE SLIX2 specific commands, see https://www.nxp.com/docs/en/data-sheet/SL2S2602.pdf
	* Example usage, currently outcommented

Version 1.3 - 21.05.2019

	* Initialized Reset pin with HIGH
	* Made readBuffer static
	* Typo fixes
	* Data type corrections for length parameters

Version 1.2 - 28.01.2019

	* Cleared Option bit in PN5180ISO15693::readSingleBlock and ::writeSingleBlock

Version 1.1 - 26.10.2018

	* Cleanup, bug fixing, refactoring
	* Automatic check for Arduino vs. ESP-32 platform via compiler switches
	* Added open pull requests
	* Working on documentation

Version 1.0.x - 21.09.2018

	* Initial versions
