# DSC Keybus Interface
This library directly interfaces Arduino and esp8266 microcontrollers to [DSC PowerSeries](http://www.dsc.com/dsc-security-products/g/PowerSeries/4) security systems for integration with home automation, notifications on system events, and usage as a virtual keypad.  The included examples demonstrate monitoring armed/alarm/zone/fire/trouble states, integrating with Home Assistant/Apple HomeKit/Homey, sending push notifications/email, and reading/decoding the Keybus protocol.

For example, an Arduino Uno (with an ethernet/wifi module) or the inexpensive NodeMCU and Wemos D1 Mini modules ($3USD shipped) can enable mobile devices to control the security system:

* Apple Home and Siri:
  ![HomeKit](https://user-images.githubusercontent.com/12835671/39588413-5a99099a-4ec1-11e8-9a2e-e332fa2d6379.jpg)

* [Home Assistant](https://www.home-assistant.io):
  ![HomeAssistant](https://user-images.githubusercontent.com/12835671/42108879-7362ccf6-7ba1-11e8-902e-d6cb25483a00.png)

* [Blynk](https://www.blynk.cc) virtual keypad:

  ![dsc-blynk](https://user-images.githubusercontent.com/12835671/42364975-add27c94-80c2-11e8-8a55-9d6d168ff8c1.png)
  
* Standalone Async HTTP server with WebSockets:

  <img src="https://raw.githubusercontent.com/Elektrik1/dscKeybusInterface/master/extras/Screenshots/HTTP_firefox_pc.png" width="30%" />
  <img src="https://raw.githubusercontent.com/Elektrik1/dscKeybusInterface/master/extras/Screenshots/HTTP_chrome_android_portrait.jpg" width="30%" />
  <img src="https://raw.githubusercontent.com/Elektrik1/dscKeybusInterface/master/extras/Screenshots/HTTP_chrome_android_landscape.jpg" width="30%" />

## Features
* Status tracking of armed/alarm/fire states for partitions 1-8
* Status tracking of zones 1-64
* Virtual keypad: Enables writing keys to the panel for partitions 1-8
* Data buffering: Helps prevent missing Keybus data when the sketch is busy
* Non-blocking code: Allows sketches to run as quickly as possible without using `delay` or `delayMicroseconds`
* Tested DSC panels: PC1555MX, PC5015, PC1616, PC1832, PC1864.  All PowerSeries panels are supported, post an issue if you have a different panel (PC5020, etc) and have tested the interface to update this list.
* Supported boards:
  - Arduino: Uno, Mega, Leonardo, Mini, Micro, Nano, Pro, Pro Mini (ATmega328P, ATmega2560, and ATmega32U4-based boards at 16Mhz)
  - esp8266: NodeMCU, Wemos D1 Mini, ESP12, etc

## Release notes
* 1.1
  - New: Zones 33-64 tamper and fault decoding
  - New: Push notification example using [Twilio](https://www.twilio.com), thanks to [ColingNG](https://github.com/ColinNg) for the contribution!
  - Bugfix: Zones 17-32 status incorrectly stored
* 1.0
  - New: [Blynk](https://www.blynk.cc) virtual keypad example sketch and app layout examples
  - New: Virtual keypad support for PGM terminals 1-4 command output
  - New: Status `dsc.keybusConnected` to check if data is being received from the DSC panel
  - New: Auxiliary input alarm decoding
* 0.4
  - New: Virtual keypad support for partitions 3-8, thanks to [jvitkauskas](https://github.com/jvitkauskas) for contributing the necessary logs
  - New: Support ATmega32U4-based Arduino boards (switched to AVR Timer1)
  - Changed: Simplified example names, configurations, added version numbers
  - Bugfix: Virtual keypad writes with partitions 5-8 enabled
  - Bugfix: F/A/P alarm key writes with `processModuleData` disabled
  - Bugfix: HomeAssistant example `configuration.yaml` error for `alarm_control_panel`
* 0.3
  - New: Status for partitions 2-8, zones 33-64
  - New: Virtual keypad support for partition 2
  - New: [Athom Homey](https://www.athom.com/en/) integration example sketch, contributed by [MagnusPer](https://github.com/MagnusPer)
  - New: PCB layouts, contributed by [sjlouw](https://github.com/sj-louw)
  - New: Configurable number of partitions and zones to customize memory usage: `dscPartitions` and `dscZones` in `dscKeybusInterface.h`
  - New: KeybusReader decoding of commands `0xE6` and `0xEB` 
  - Changed: Split examples by platform
  - Changed: Arduino sketches no longer use pin 4 to avoid a conflict with the SD card on Ethernet shields. 
  - Changed: MQTT examples updated with username and password fields
  - Changed: `processRedundantData` now true by default to prevent storing repetitive data, reduces memory usage.
  - Note: This release changes the library methods to accomodate multiple partitions, existing sketches will need to be updated to match the new example sketches.
* 0.2
  - New: Status for zones 9-32
  - New: [Home Assistant](https://www.home-assistant.io) integration example sketch
  - New: Panel data buffering, adds `dscBufferSize` to `dscKeybusInterface.h` to allow configuration of how many panel commands are buffered to customize memory usage (uses 18 bytes of memory per command buffered).
* 0.1 - Initial release

## Installation
* Arduino IDE: Search for `DSC` in the Library Manager - `Sketch > Include Library > Manage Libraries`
  ![ArduinoIDE](https://user-images.githubusercontent.com/12835671/41826133-cfa55334-77ec-11e8-8ee1-b482cdb696b2.png)
* PlatformIO IDE: Search for `DSC` in the [PlatformIO Library Registry](https://platformio.org/lib/show/5499/dscKeybusInterface)
  ![PlatformIO](https://user-images.githubusercontent.com/12835671/41826138-d5852b62-77ec-11e8-805d-7c861a329e43.png)
* PlatformIO CLI: `platformio lib install "dscKeybusInterface"`
* Alternatively, `git clone` or download the repo .zip to the Arduino/PlatformIO library directory to keep track of the latest changes.

## Examples
The included examples demonstrate how to use the library and can be used as-is or adapted to integrate with other software.  Post an issue/pull request if you've developed (and would like to share) a sketch/integration that others can use.

* Status: Processes and prints the security system status to a serial interface, including reading from serial for the virtual keypad.  This demonstrates how to determine if the security system status has changed, what has changed, and how to take action based on those changes.  Post an issue/pull request if you have a use for additional system states - for now, only a subset of all decoded commands are being tracked for status to limit memory usage:
  * Partitions armed away/stay/disarmed
  * Partitions in alarm
  * Partitions exit delay in progress
  * Partitions entry delay in progress
  * Partitions fire alarm
  * Zones open/closed
  * Zones in alarm
  * Keypad fire/auxiliary/panic alarm
  * Panel AC power
  * Panel battery
  * Panel trouble
  * Keybus connected

* Homebridge-MQTT: Integrates with Apple HomeKit, including the iOS Home app and Siri.  This uses MQTT to interface with [Homebridge](https://github.com/nfarina/homebridge) and [homebridge-mqttthing](https://github.com/arachnetech/homebridge-mqttthing) and demonstrates using the armed and alarm states for the HomeKit securitySystem object, zone states for the contactSensor objects, and fire alarm states for the smokeSensor object.

* HomeAssistant-MQTT: Integrates with [Home Assistant](https://www.home-assistant.io) via MQTT.  This uses the armed and alarm states for the HomeAssistant [Alarm Control Panel](https://www.home-assistant.io/components/alarm_control_panel.mqtt) component, as well as zone, fire alarm, and trouble states for the [Binary Sensor](https://www.home-assistant.io/components/binary_sensor.mqtt) component.

* Homey: Integrates with [Athom Homey](https://www.athom.com/en/) and the [Homeyduino](https://github.com/athombv/homey-arduino-library/) library, including armed, alarm, fire, and zone states.

* Pushbullet (esp8266-only):  Demonstrates how to send a push notification when the status has changed. This example sends notifications via [Pushbullet](https://www.pushbullet.com) and requires the esp8266 for SSL support.

* Email (esp8266-only): Demonstrates how to send an email when the status has changed. Email is sent using SMTPS (port 465) with SSL for encryption - this is necessary on the ESP8266 until STARTTLS can be supported.  For example, this will work with Gmail after changing the account settings to [allow less secure apps](https://support.google.com/accounts/answer/6010255).

* VirtualKeypad-Blynk (esp8266-only): Provides a virtual keypad interface for the free [Blynk](https://www.blynk.cc) app on iOS and Android.  Scan one of the following QR codes from within the Blynk app for an example keypad layout:
  - [Virtual keypad with 16 zones](https://user-images.githubusercontent.com/12835671/42364287-41ca6662-80c0-11e8-85e7-d579b542568d.png)
  - [Virtual keypad with 32 zones](https://user-images.githubusercontent.com/12835671/42364293-4512b720-80c0-11e8-87bd-153c4e857b4e.png)

  Note: Installing [Blynk as a local server](https://github.com/blynkkk/blynk-server) is recommended to keep control of the security system internal to your network.  This also lets you use as many widgets as needed for free - local servers can setup users with any amount of Blynk Energy.  Using the default Blynk cloud service with the above example layouts requires more of Blynk's Energy units than available on the free usage tier.

* KeybusReader: Decodes and prints data from the Keybus to a serial interface, including reading from serial for the virtual keypad.

  This is primarily to help decode the Keybus protocol - at this point, I've decoded a substantial portion of the commands seen in the [DSC IT-100 Developers Guide](http://cms.dsc.com/download.php?t=1&id=16238) (which also means that a very basic IT-100 emulator is possible).  The notable exceptions are the thermostat and wireless commands as I do not have these modules.

  See `src/dscKeybusPrintData.cpp` for all currently known Keybus protocol commands and messages.  Issues and pull requests with additions/corrections are welcome!

## Wiring

```
DSC Aux(-) --- Arduino/esp8266 ground

                                   +--- dscClockPin (Arduino Uno: 2,3 / esp8266: D1,D2,D8)
DSC Yellow --- 15k ohm resistor ---|
                                   +--- 10k ohm resistor --- Ground

                                   +--- dscReadPin (Arduino Uno: 2-12 / esp8266: D1,D2,D8)
DSC Green ---- 15k ohm resistor ---|
                                   +--- 10k ohm resistor --- Ground

Virtual keypad (optional):
DSC Green ---- NPN collector --\
                                |-- NPN base --- 1k ohm resistor --- dscWritePin (Arduino Uno: 2-12 / esp8266: D1,D2,D8)
      Ground --- NPN emitter --/

Power (when disconnected from USB):
DSC Aux(+) ---+--- Arduino Vin pin
              |
              +--- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
              |
              +--- 3.3v voltage regulator --- esp8266 bare module VCC pin (ESP-12, etc)
```

## Wiring Notes
* The DSC Keybus operates at ~12.6v, a pair of resistors per data line will bring this down to an appropriate voltage for both Arduino and esp8266.
* Arduino: connect the DSC Yellow (Clock) line to a [hardware interrupt pin](https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/) - for the Uno, these are pins 2 and 3.  The DSC Green (Data) line can be connected to the remaining digital pins 2-12.
* esp8266: connect the DSC lines to GPIO pins that are normally low to avoid putting spurious data on the Keybus: D1 (GPIO5), D2 (GPIO4) and D8 (GPIO15).
* Virtual keypad uses an NPN transistor and a resistor to write to the Keybus.  Most small signal NPN transistors should be suitable, for example:
  * 2N3904
  * BC547, BC548, BC549
  * That random NPN at the bottom of your parts bin (my choice)

## Virtual keypad
This allows a sketch to send keys to the DSC panel to emulate the physical DSC keypads and enables full control of the panel from the sketch or other software.

Keys are sent to partition 1 by default and can be changed to a different partition.  The following keys can be sent to the panel - see the examples for usage:

* Keypad: `0-9 * #`
* Arm stay (requires access code if quick arm is disabled): `s`
* Arm away (requires access code if quick arm is disabled): `w`
* Arm with no entry delay (requires access code): `n`
* Fire alarm: `f`
* Auxiliary alarm: `a`
* Panic alarm: `p`
* Door chime: `c`
* Reset: `r`
* Exit: `x`
* Change partition: `/` + `partition number` or set `writePartition` to the partition number.  Examples:
  * Switch to partition 2 and send keys: `/2` + `1234`
  * Switch back to partition 1: `/1`
  * Set directly in sketch: `dsc.writePartition = 8;`
* Command output 1: `[`
* Command output 2: `]`
* Command output 3: `{`
* Command output 4: `}`

## DSC Configuration
Panel options affecting this interface, configured by `*8 + installer code`:
* Swinger shutdown: PC1555MX/5015 section 370, PC1616/PC1832/PC1864 section 377.  By default, the panel will limit the number of alarm commands sent in a single armed cycle to 3 - for example, a zone alarm being triggered multiple times will stop reporting after 3 alerts.  This is to avoid sending alerts repeatedly to a monitoring station, and also affects this interface - this limit can be adjusted or disabled in this configuration section.

  This section also sets the delay in reporting AC power failure to 30 minutes by default and can be set to 000 for no delay.  

## Notes
* Memory usage can be adjusted based on the number of partitions, zones, and data buffer size specified in `src/dscKeybusInterface.h`.  Default settings:
  * Arduino: up to 4 partitions, 32 zones, 10 buffered commands
  * esp8266: up to 8 partitions, 64 zones, 50 buffered commands

* PCB layouts are available in `extras/PCB Layouts` - thanks to [sjlouw](https://github.com/sj-louw) for contributing these designs!

* Support for the esp32 and other platforms depends on adjusting the code to use their platform-specific timers.  In addition to hardware interrupts to capture the DSC clock, this library uses platform-specific timer interrupts to capture the DSC data line in a non-blocking way 250us after the clock changes (without using `delayMicroseconds()`).  This is necessary because the clock and data are asynchronous - I've observed keypad data delayed up to 160us after the clock falls.

## References
[AVR Freaks - DSC Keybus Protocol](https://www.avrfreaks.net/forum/dsc-keybus-protocol): An excellent discussion on how data is sent on the Keybus.

[stagf15/DSC_Panel](https://github.com/stagf15/DSC_Panel): A library that nearly works for the PC1555MX but had timing and data errors.  Writing this library from scratch was primarily a programming exercise, otherwise it should be possible to patch the DSC_Panel library.

[dougkpowers/pc1550-interface](https://github.com/dougkpowers/pc1550-interface): An interface for the DSC Classic series.
