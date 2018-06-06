_**Note**: This is a development branch for testing: partitions 2-8, zones 33-64, virtual keypad partitions 1 & 2.  The library methods have changed to accommodate multiple partitions and the previous methods have been removed - see the examples sketches for usage.  The new methods should not change in the future and can be considered stable (in theory!)._

# DSC Keybus Interface
This library directly interfaces Arduino and esp8266 microcontrollers to [DSC PowerSeries](http://www.dsc.com/dsc-security-products/g/PowerSeries/4) security systems for integration with home automation, notifications on system events, and usage as a virtual keypad.  The included examples demonstrate monitoring armed/alarm/zone/fire/trouble states, integrating with Home Assistant and Apple HomeKit using MQTT, sending push notifications/email, and reading/decoding Keybus data.

For example, an Arduino Uno (with an ethernet module) or the inexpensive NodeMCU and Wemos D1 Mini modules ($3USD shipped) can be used with [Homebridge](https://github.com/nfarina/homebridge) for notifications and control of the security system through the iOS Home app and Siri:

![dscHomeKit](https://user-images.githubusercontent.com/12835671/39588413-5a99099a-4ec1-11e8-9a2e-e332fa2d6379.jpg)

## Features
* Partitions: 1-8, zones: 1-64
* Virtual keypad: supports sending keys to the panel for partitions 1 and 2
* Data buffering: helps prevent missing Keybus data when the sketch is busy
* Non-blocking code: allows sketches to run as quickly as possible without using `delay` or `delayMicroseconds`.
* Tested panels: PC1555MX, PC5015, PC1616, PC1832, PC1864

## Usage
Download the repo and extract to the Arduino library directory or [install through the Arduino IDE](https://www.arduino.cc/en/Guide/Libraries#toc4): `Sketch > Include Library > Add .ZIP Library`.  Alternatively, `git clone` the repo in the Arduino library directory to keep track of the latest changes - after the code has been tested across different panels, I'll flag the library to be added to the Arduino Library Manager for integrated updates.

## Examples
* KeybusReader: Decodes and prints data from the Keybus to a serial interface, including reading from serial for the virtual keypad.

  This is primarily to help decode the Keybus protocol - at this point, I've decoded a substantial portion of the commands seen in the [DSC IT-100 Developers Guide](http://cms.dsc.com/download.php?t=1&id=16238) (which also means that a very basic IT-100 emulator is possible).  The notable exceptions are the thermostat and wireless commands as I do not have these modules.

  See `src/dscKeybusPrintData.cpp` for all currently known Keybus protocol commands and messages.  Issues and pull requests with additions/corrections are welcome!

* Status: Processes and prints the security system status to a serial interface, including reading from serial for the virtual keypad.  This demonstrates how to determine if the security system status has changed, what has changed, and how to take action based on those changes.  Post an issue/pull request if you have a use for additional commands - for now, only a subset of all decoded commands are being tracked for status:
  * Armed away/stay/disarmed
  * Partition in alarm
  * Zones
  * Zones in alarm
  * Exit delay in progress
  * Entry delay in progress
  * Fire
  * Keypad fire/auxiliary/panic alarm
  * Panel AC power
  * Panel battery
  * Panel trouble

* Status-MQTT-Homebridge: Processes the security system status and allows for control using Apple HomeKit, including the iOS Home app and Siri.  This uses MQTT to interface with [Homebridge](https://github.com/nfarina/homebridge) and [homebridge-mqttthing](https://github.com/arachnetech/homebridge-mqttthing) for HomeKit integration and demonstrates using the armed and alarm states for the HomeKit securitySystem object, zone states for the contactSensor objects, and fire alarm states for the smokeSensor object.

  Note: homebridge-mqttthing seems to have a bug for the smokeSensor object, for now I've issued a fix:  [taligentx/homebridge-mqttthing](https://github.com/taligentx/homebridge-mqttthing)

* Status-MQTT-HomeAssistant: Processes the security system status and allows for control with [Home Assistant](https://www.home-assistant.io) via MQTT.  This uses the armed and alarm states for the HomeAssistant [Alarm Control Panel](https://www.home-assistant.io/components/alarm_control_panel.mqtt) component, as well as fire alarm and zone states for the [Binary Sensor](https://www.home-assistant.io/components/binary_sensor.mqtt) component.

* Status-Homey: Processes the security system status and allows for control using [Athom Homey](https://www.athom.com/en/) and the [Homeyduino](https://github.com/athombv/homey-arduino-library/) library, including armed, alarm, fire, and zone states.

* Status-Pushbullet (esp8266-only):  Processes the security system status and demonstrates how to send a push notification when the status has changed. This example sends notifications via [Pushbullet](https://www.pushbullet.com) and requires the esp8266 for SSL support.

* Status-Email (esp8266-only): Processes the security system status and demonstrates how to send an email when the status has changed. Email is sent using SMTPS (port 465) with SSL for encryption - this is necessary on the ESP8266 until STARTTLS can be supported.  For example, this will work with Gmail after changing the account settings to [allow less secure apps](https://support.google.com/accounts/answer/6010255).

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
* The DSC Keybus operates at 13.6v, a pair of resistors per data line will bring this down to an appropriate voltage for both Arduino and esp8266.
* Arduino: connect the DSC Yellow (Clock) line to a [hardware interrupt pin](https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/) - for the Uno, these are pins 2 and 3.  The DSC Green (Data) line can be connected to the remaining digital pins 2-12.
* esp8266: connect the DSC lines to GPIO pins that are normally low to avoid putting spurious data on the Keybus: D1 (GPIO5), D2 (GPIO4) and D8 (GPIO15).
* Virtual keypad uses an NPN transistor and a resistor to write to the Keybus.  Most small signal NPN transistors should be suitable, for example:
  * 2N3904
  * BC547, BC548, BC549
  * That random NPN at the bottom of your parts bin (my choice)

## Virtual keypad
This allows a sketch to send keys to the DSC panel to emulate the physical DSC keypads and enables full control of the panel from the sketch or other software.

Keys are sent to partition 1 by default and can be changed to a different partition.  The following keys can be sent to the panel (see the examples for usage):

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
* Change partition: `/` + `partition number`
  Examples:
  * Switch to partition 2 and send keys: `/2` + `1234`
  * Switch back to partition 1: `/1`

## DSC Configuration
Panel options affecting this interface, configured by `*8 + installer code`:
* Swinger shutdown: PC1555MX/5015 section 370, PC1616/PC1832/PC1864 section 377.  By default, the panel will limit the number of alarm commands sent in a single armed cycle to 3 - for example, a zone alarm being triggered multiple times will stop reporting after 3 alerts.  This is to avoid sending alerts repeatedly to a monitoring station, and also affects this interface - this limit can be adjusted or disabled in this configuration section.

  This section also sets the delay in reporting AC power failure to 30 minutes by default and can be set to 000 for no delay.  

## Notes
* Support for the esp32 and other platforms depends on adjusting the code to use their platform-specific timers.  In addition to hardware interrupts to capture the DSC clock, this library uses platform-specific timer interrupts to capture the DSC data line in a non-blocking way 250us after the clock changes (without using `delayMicroseconds()`).  This is necessary because the clock and data are asynchronous - I observed keypad data delayed up to 160us after the clock falls.

## References
[AVR Freaks - DSC Keybus Protocol](https://www.avrfreaks.net/forum/dsc-keybus-protocol): An excellent discussion on how data is sent on the Keybus.

[stagf15/DSC_Panel](https://github.com/stagf15/DSC_Panel): A library that nearly works for the PC1555MX but had timing and data errors.  Writing this library from scratch was primarily a programming exercise, otherwise it should be possible to patch the DSC_Panel library.

[dougkpowers/pc1550-interface](https://github.com/dougkpowers/pc1550-interface): An interface for the DSC Classic series.
