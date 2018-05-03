# DSC Keybus Interface
This library enables interfacing Arduino and esp8266 microcontrollers to DSC PowerSeries security systems, including decoding the system status and writing as a virtual keypad.  The included examples demonstrate reading Keybus data, monitoring status, and sending notifications on system events using MQTT, push notifications, and email.

For example, using this library with a $3USD NodeMCU/Wemos D1 Mini and [Homebridge](https://github.com/nfarina/homebridge) allows for notifications and control of the security system through Apple HomeKit and Siri:
![dscHomeKit](https://user-images.githubusercontent.com/12835671/39588413-5a99099a-4ec1-11e8-9a2e-e332fa2d6379.jpg)

## Status
This is an early release and supports the DSC PC1555MX (PowerSeries 632).  Captured Keybus data is needed for the PC1616/PC1832/PC1864 series.

## Examples
* KeybusReader: Decodes and prints data from the Keybus to a serial interface, including reading from serial for the virtual keypad.

  This is primarily to help decode the Keybus protocol - for the PC1555MX, I've decoded a substantial portion of the commands seen in the [DSC IT-100 Developers Guide](http://cms.dsc.com/download.php?t=1&id=16238) (which also means that a very basic IT-100 emulator is possible).  The notable exceptions are the thermostat, smoke alarm, and wireless commands as I do not have these modules. 

  See `src/dscKeybusPrintData.cpp` for all currently known Keybus protocol commands.  Issues and pull requests with additions/corrections are welcome!

* Status: Processes and prints the security system status to a serial interface, including reading from serial for the virtual keypad.  This demonstrates how to determine if the security system status has changed and what has changed, and how to take action based on those changes.  For now, only a subset of all decoded commands are being tracked for status.

* Status-Homebridge-mqttthing: Processes the security system status and allows for control using Apple HomeKit, including the iOS Home app and Siri.  This uses MQTT to interface with Homebridge and homebridge-mqttthing for HomeKit integration and demonstrates using the armed and alarm states for the HomeKit securitySystem object, as well as the zone states for the contactSensor objects.
  * [Homebridge](https://github.com/nfarina/homebridge)
  * [homebridge-mqttthing](https://github.com/arachnetech/homebridge-mqttthing)
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
 * There are many system states that have been decoded but do not have separate variables to keep track of their status - this is to keep memory usage low, but feel free to open an issue or pull request for additional system states that would be useful.
 * The zone count is currently hardcoded to limit memory usage - to monitor more than 8 zones, change `dscZones` in `dscKeybusInterface.h` to support 16 or 32 zones.
 
