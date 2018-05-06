# DSC Keybus Interface
This library interfaces Arduino and esp8266 microcontrollers to [DSC PowerSeries](http://www.dsc.com/dsc-security-products/g/PowerSeries/4) security systems, including decoding the system status and writing as a virtual keypad.  The included examples demonstrate reading Keybus data, monitoring status, and sending notifications on system events using MQTT, push notifications, and email.

For example, using this library with the inexpensive NodeMCU and Wemos D1 Mini modules ($3USD shipped) and [Homebridge](https://github.com/nfarina/homebridge) allows for notifications and control of the security system through Apple HomeKit and Siri:

![dscHomeKit](https://user-images.githubusercontent.com/12835671/39588413-5a99099a-4ec1-11e8-9a2e-e332fa2d6379.jpg)

## Status
This is an early release and supports the DSC PC1555MX (PowerSeries 632) up to 32 zones.  Captured Keybus data is needed to confirm zones 9-32 status, zones 9-32 in alarm status, and the PC1616/PC1832/PC1864 series, feel free to [add logs of data](https://github.com/taligentx/dscKeybusInterface/issues/2) from these panels using the KeybusReader example.

## Usage
Download the repo and extract to the Arduino library directory or [install through the Arduino IDE](https://www.arduino.cc/en/Guide/Libraries#toc4): `Sketch > Include Library > Add .ZIP Library`.  Alternatively, clone the repo in the Arduino library directory to keep track of the latest changes - after the code has been tested across different panels, I'll flag the library to be added to the Arduino Library Manager for integrated updates.

## Examples
* KeybusReader: Decodes and prints data from the Keybus to a serial interface, including reading from serial for the virtual keypad.

  This is primarily to help decode the Keybus protocol - for the PC1555MX, I've decoded a substantial portion of the commands seen in the [DSC IT-100 Developers Guide](http://cms.dsc.com/download.php?t=1&id=16238) (which also means that a very basic IT-100 emulator is possible).  The notable exceptions are the thermostat, smoke alarm, and wireless commands as I do not have these modules. 

  See `src/dscKeybusPrintData.cpp` for all currently known Keybus protocol commands and messages.  Issues and pull requests with additions/corrections are welcome!

* Status: Processes and prints the security system status to a serial interface, including reading from serial for the virtual keypad.  This demonstrates how to determine if the security system status has changed, what has changed, and how to take action based on those changes.  Post an issue/pull request if you have a use for additional commands - for now, only a subset of all decoded commands are being tracked for status:
  * Armed away/stay/disarmed
  * Partition in alarm
  * Zones
  * Zones in alarm
  * Exit delay in progress
  * Entry delay in progress
  * Keypad fire/auxiliary/panic alarm
  * Panel AC power
  * Panel battery
  * Panel trouble

* Status-MQTT-Homebridge: Processes the security system status and allows for control using Apple HomeKit, including the iOS Home app and Siri.  This uses MQTT to interface with Homebridge and homebridge-mqttthing for HomeKit integration and demonstrates using the armed and alarm states for the HomeKit securitySystem object, as well as the zone states for the contactSensor objects.
  * [Homebridge](https://github.com/nfarina/homebridge)
  * [homebridge-mqttthing](https://github.com/arachnetech/homebridge-mqttthing)
  * [Mosquitto MQTT broker](https://mosquitto.org)

* Status-MQTT-HomeAssistant: Processes the security system status and allows for control with HomeAssistant via MQTT.  This uses the armed and alarm states for the HomeAssistant Alarm Control Panel component, and the zone states for the Binary Sensor component.
  * [Home Assistant](https://www.home-assistant.io)
  * [Mosquitto MQTT broker](https://mosquitto.org)

* Status-Email: Processes the security system status and demonstrates how to send an email when the status has changed. Email is sent using SMTPS (port 465) with SSL for encryption - this is necessary on the ESP8266 until STARTTLS can be supported.  For example, this will work with Gmail after changing the account settings to [allow less secure apps](https://support.google.com/accounts/answer/6010255).

* Status-Pushbullet:  Processes the security system status and demonstrates how to send a push notification when the status has changed. This example sends notifications via [Pushbullet](https://www.pushbullet.com).

## Wiring
* Pins:
  * Arduino: connect the DSC Yellow (Clock) line to a [hardware interrupt pin](https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/) - for the Uno, these are pins 2 and 3.  The DSC Green (Data) line can be connected to the remaining digital pins 2-12.
  * esp8266: connect the DSC lines to GPIO pins that are normally low to avoid putting spurious data on the Keybus: D1 (GPIO5), D2 (GPIO4) and D8 (GPIO15).
* The DSC Keybus operates at 12v, a pair of resistors per data line will bring this down to an appropriate voltage for the Arduino and esp8266.
* Virtual keypad uses an NPN transistor and a resistor to write to the Keybus.  Most small signal NPN transistors should be suitable, for example:
  * 2N3904
  * BC547, BC548, BC549
  * That random NPN at the bottom of your parts bin (my choice)
  
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
 
 ## Virtual keypad keys
 * Keypad: `0-9 * #` 
 * Stay arm: `s`
 * Away arm: `w`
 * Fire alarm: `f`
 * Auxiliary alarm: `a`
 * Panic alarm: `p`
 * Door chime: `c`
 * Reset (unconfirmed): `r`
 * Exit (unconfirmed): `x`
 * Right arrow (unconfirmed): `>`
 * Left arrow (unconfirmed): `<`
 
 ## Notes
 * Support for the esp32 and other platforms depends on adjusting the code to use their platform-specific timers.  In addition to hardware interrupts to capture the DSC clock, this library uses platform-specific timer interrupts to capture the DSC data line 250us after the clock changes in a non-blocking way (without using `delayMicroseconds()`).  This is necessary because the clock and data are asynchronous - I observed keypad data delayed up to 160us after the clock falls.

## References
[AVR Freaks - DSC Keybus Protocol](https://www.avrfreaks.net/forum/dsc-keybus-protocol): An excellent discussion on how data is sent on the Keybus.

[stagf15/DSC_Panel](https://github.com/stagf15/DSC_Panel): A library that nearly works for the PC1555MX but had timing and data errors.  Writing the DSC Keybus Interface library from scratch was primarily a programming excercise, otherwise it should be possible to patch the DSC_Panel library.

[dougkpowers/pc1550-interface](https://github.com/dougkpowers/pc1550-interface): An interface for the DSC Classic series.
