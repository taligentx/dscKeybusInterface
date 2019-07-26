# DSC Keybus Interface
This library directly interfaces Arduino, esp8266, and esp32 microcontrollers to [DSC PowerSeries](http://www.dsc.com/dsc-security-products/g/PowerSeries/4) security systems for integration with home automation, notifications on alarm events, and usage as a virtual keypad.  This enables existing DSC security system installations to retain the features and reliability of a hardwired system while integrating with modern devices and software for under $5USD in components.

The built-in examples can be used as-is or as a base to adapt to other uses:
* Home automation: [Home Assistant](https://www.home-assistant.io), [Apple HomeKit & Siri](https://www.apple.com/ios/home/), [OpenHAB](https://www.openhab.org), [Athom Homey](https://www.athom.com/en/)
* Notifications: [PushBullet](https://www.pushbullet.com), [Twilio SMS](https://www.twilio.com), MQTT, E-mail
* Virtual keypad: Web interface, [Blynk](https://www.blynk.cc) mobile app, installer code unlocking

See the [dscKeybusInterface-RTOS](https://github.com/taligentx/dscKeybusInterface-RTOS) repository for a port of this library to [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos) - this enables a standalone esp8266 HomeKit accessory using [esp-homekit](https://github.com/maximkulkin/esp-homekit).

Screenshots:
* [Apple Home & Siri](https://www.apple.com/ios/home/):  
  ![HomeKit](https://user-images.githubusercontent.com/12835671/61570833-c9bb9780-aa54-11e9-9477-8e0853609e91.png)
* [Home Assistant](https://www.home-assistant.io):  
  ![HomeAssistant](https://user-images.githubusercontent.com/12835671/61985688-f54bfe00-afcf-11e9-82f9-1a41bf8b89a5.png)
* [OpenHAB](https://www.openhab.org):  
  ![OpenHAB](https://user-images.githubusercontent.com/12835671/61560425-daa6e180-aa31-11e9-9efe-0fcb44d2106a.png)
* [Blynk](https://www.blynk.cc) app virtual keypad:  
  ![VirtualKeypad-Blynk](https://user-images.githubusercontent.com/12835671/61568638-9fb0a800-aa49-11e9-94d0-e598431ea2ed.png)
* Web virtual keypad:  
  ![VirtualKeypad-Web](https://user-images.githubusercontent.com/12835671/61570049-e43f4200-aa4f-11e9-96bc-3448b6630990.png)

## Why?
**I Had**: _A DSC security system not being monitored by a third-party service._  
**I Wanted**: _Notification if the alarm triggered._

I was interested in finding a solution that directly accessed the pair of data lines that DSC uses for their proprietary Keybus protocol to send data between the panel, keypads, and other modules.  Tapping into the data lines is an ideal task for a microcontroller and also presented an opportunity to work with the [Arduino](https://www.arduino.cc) and [FreeRTOS](https://www.freertos.org) (via [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos)) platforms.

While there has been excellent [discussion about the DSC Keybus protocol](https://www.avrfreaks.net/forum/dsc-keybus-protocol) and a several existing projects, there were a few issues that remained unsolved:
* Error-prone Keybus data capture.
* Limited data decoding - there was good progress for armed/disarmed states and partial zone status for a single partition, but otherwise most of the data was undecoded (notably missing the alarm triggered state).
* Read-only - unable to control the Keybus to act as a virtual keypad.
* No implementations to do useful work with the data.

Poking around with a logic analyzer and oscilloscope revealed that the errors capturing the Keybus data were timing issues - after resolving the data errors, it was possible to reverse engineer the protocol by capturing the Keybus binary data as the security system handled various events.

## Features
* Monitor the alarm state of all partitions:
  - Alarm triggered, armed/disarmed, entry/exit delay, fire triggered, keypad panic keys
* Monitor zones status:
  - Zones open/closed, zones in alarm
* Monitor system status:
  - Ready, trouble, AC power, battery
* Virtual keypad:
  - Write keys to the panel for all partitions
* Panel time - retrieve current panel date/time and set a new date/time
* Panel installer code unlocking - determine the 4-digit panel installer code
* Direct Keybus interface:
  - Does not require the [DSC IT-100 serial interface](https://www.dsc.com/alarm-security-products/IT-100%20-%20PowerSeries%20Integration%20Module/22).
* Supported security systems:
  - [DSC PowerSeries](https://www.dsc.com/?n=enduser&o=identify)
  - Verified panels: PC585, PC1555MX, PC1565, PC5005, PC5015, PC1616, PC1808, PC1832, PC1864.
  - All PowerSeries series are supported, please [post an issue](https://github.com/taligentx/dscKeybusInterface/issues) if you have a different panel (PC5020, etc) and have tested the interface to update this list.
  - Rebranded DSC PowerSeries (such as some ADT systems) should also work with this interface.
* Supported microcontrollers:
    - [Arduino](https://www.arduino.cc/en/Main/Products):
      * Boards: Uno, Mega, Leonardo, Mini, Micro, Nano, Pro, Pro Mini
      * ATmega328P, ATmega2560, and ATmega32U4-based boards at 16Mhz
    - esp8266:
      * Development boards: NodeMCU v2 or v3, Wemos D1 Mini, etc.
      * Includes [Arduino framework support](https://github.com/esp8266/Arduino) and WiFi for ~$3USD shipped.
    - esp32:
      * Development boards: NodeMCU ESP-32S, Doit ESP32 Devkit v1, Wemos Lolin D32, etc.
      * Includes [Arduino framework support](https://github.com/espressif/arduino-esp32), dual cores, WiFi, and Bluetooth for ~$5USD shipped.
* Designed for reliable data decoding and performance:
  - Pin change and timer interrupts for accurate data capture timing
  - Data buffering: helps prevent lost Keybus data if the sketch is busy
  - Extensive data decoding: the majority of Keybus data as seen in the [DSC IT-100 Data Interface developer's guide](https://cms.dsc.com/download.php?t=1&id=16238) has been reverse engineered and documented in [`src/dscKeybusPrintData.cpp`](https://github.com/taligentx/dscKeybusInterface/blob/master/src/dscKeybusPrintData.cpp).
  - Non-blocking code: Allows sketches to run as quickly as possible without using `delay` or `delayMicroseconds`
* Unsupported security systems:
  - DSC Classic series ([PC1500, PC1550, etc](https://www.dsc.com/?n=enduser&o=identify)) use a different data protocol, though support is possible.
  - DSC Neo series use a higher speed encrypted data protocol (Corbus) that is not currently possible to support.
  - Honeywell, Ademco, and other brands (that are not rebranded DSC systems) use different protocols and are not supported.
* Possible features (PRs welcome!):
  - Virtual zone expander: Add new zones to the DSC panel emulated by the microcontroller based on GPIO pin states or software-based states.  Requires decoding the DSC PC5108 zone expander data.
  - [DSC IT-100](https://cms.dsc.com/download.php?t=1&id=16238) emulation
  - Unlock 6-digit installer codes
  - DSC Classic series support: This protocol is [already decoded](https://github.com/dougkpowers/pc1550-interface), use with this library would require major changes.

## Release notes
* develop
  - New: [OpenHAB](https://www.openhab.org) integration example sketch using MQTT
  - New: `Unlocker` example sketch - determines the panel installer code
  - New: esp32 microcontroller support
  - New: Features for sketches:
      * `ready` and `disabled` track partition status
      * `setTime()` sets the panel date and time
      * `timestampChanged` tracks when the panel sends a timestamp
      * `accessCode` tracks the access code used to arm/disarm
      * `resetStatus()` triggers a full status update of all partitions and zones - for example, after initialization or a lost network connection.
      * `pauseStatus` pauses status updates if set to `true` - for example, holding status changes during a lost network connection
      * `stop()` disables the interface - for example, prior to starting OTA updates
      * `appendPartition()` in example sketches simplifies adding partition numbers to messages
  - New: Handle `*1 bypass/re-activate` used to change stay/away mode while armed
  - Updated: Virtual keypad writes
      * `write()` for multiple keys can now be set to block until the write is complete with an optional parameter if the char array is ephemeral
      * Checking `writeReady` is typically no longer needed in the sketch, the library will block if a previous write is in progress - this can be checked if the sketch needs to wait until the library can perform a nonblocking write
  - Updated: `HomeAssistant-MQTT` sketch now includes night arm and for esp8266/esp32 includes a sensor with partition status messages
  - Updated: Expanded partition state processing to improve panel state detection at startup
  - Deprecated: `handlePanel()` is now `loop()`
  - Bugfix: Resolved keypad aux/panic key, AC power, and battery status on PC585/PC1555MX
  - Bugfix: Resolved `Homebridge-MQTT` sketch not handling HomeKit target states
  - Bugfix: Resolved timing issues when consecutively calling `write`
* 1.2
  - New: Virtual keypad web interface example, thanks to [Elektrik1](https://github.com/Elektrik1) for this contribution!
    - As of esp8266 Arduino Core 2.5.1, you may need to [manually update the esp8266FS plugin](https://github.com/esp8266/arduino-esp8266fs-plugin) for SPIFFS upload.
  - New: Support esp8266 CPU running at 160MHz - this helps sketches using TLS through BearSSL
  - Updated: HomeAssistant-MQTT example includes availability status, thanks to [bjrolfe](https://github.com/bjrolfe) for this contribution!
  - Updated: List of tested DSC panels: PC585, PC1565, PC5005, PC1808
  - Updated: esp8266 power wiring diagrams
  - Updated: esp8266 module list
* 1.1
  - New: Zones 33-64 tamper and fault decoding
  - New: Push notification example using [Twilio](https://www.twilio.com), thanks to [ColingNG](https://github.com/ColinNg) for this contribution!
  - Bugfix: Zones 17-32 status incorrectly stored
* 1.0
  - New: [Blynk](https://www.blynk.cc) virtual keypad example sketch and app layout examples
  - New: Virtual keypad support for PGM terminals 1-4 command output
  - New: Status `keybusConnected` to check if data is being received from the DSC panel
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
  - New: [Athom Homey](https://www.athom.com/en/) integration example sketch, thanks to [MagnusPer](https://github.com/MagnusPer) for this contribution!
  - New: PCB layouts, contributed by [sjlouw](https://github.com/sj-louw)
  - New: Configurable number of partitions and zones to customize memory usage: `dscPartitions` and `dscZones` in `dscKeybusInterface.h`
  - New: KeybusReader decoding of commands `0xE6` and `0xEB` 
  - Changed: Split examples by platform
  - Changed: Arduino sketches no longer use pin 4 to avoid a conflict with the SD card on Ethernet shields. 
  - Changed: MQTT examples updated with username and password fields
  - Changed: `processRedundantData` now true by default to prevent storing repetitive data, reduces memory usage.
  - Note: This release changes the library methods to accommodate multiple partitions, existing sketches will need to be updated to match the new example sketches.
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

* **Status**: Processes and prints the security system status to a serial interface, including reading from serial for the virtual keypad.  This demonstrates how to determine if the security system status has changed, what has changed, and how to take action based on those changes.  Post an issue/pull request if you have a use for additional system states - for now, only a subset of all decoded commands are being tracked for status to limit memory usage:
  * Partitions ready
  * Partitions armed away/stay/disarmed
  * Partitions in alarm
  * Partitions exit delay in progress
  * Partitions entry delay in progress
  * Partitions fire alarm
  * Zones open/closed
  * Zones in alarm
  * Keypad fire/auxiliary/panic alarm
  * Get/set panel date and time
  * User access code number (1-40)
  * Panel AC power
  * Panel battery
  * Panel trouble
  * Keybus connected

* **Homebridge-MQTT**: Integrates with Apple HomeKit, including the iOS Home app and Siri.  This uses MQTT to interface with [Homebridge](https://github.com/nfarina/homebridge) and [homebridge-mqttthing](https://github.com/arachnetech/homebridge-mqttthing) and demonstrates using the armed and alarm states for the HomeKit securitySystem object, zone states for the contactSensor objects, and fire alarm states for the smokeSensor object.
    - The [dscKeybusInterface-RTOS](https://github.com/taligentx/dscKeybusInterface-RTOS) library includes a native HomeKit implementation that runs directly on esp8266, without requiring a separate device running MQTT or Homebridge.

* **HomeAssistant-MQTT**: Integrates with [Home Assistant](https://www.home-assistant.io) via MQTT.  This uses the armed and alarm states for the HomeAssistant [Alarm Control Panel](https://www.home-assistant.io/components/alarm_control_panel.mqtt) component, as well as zone, fire alarm, and trouble states for the [Binary Sensor](https://www.home-assistant.io/components/binary_sensor.mqtt) component.  For esp8266/esp32, a partition status message is also sent as a sensor component.

* **OpenHAB-MQTT**: Integrates with [OpenHAB](https://www.openhab.org) via MQTT.  This uses the [OpenHAB MQTT binding](https://www.openhab.org/addons/bindings/mqtt/) and demonstrates using the panel and partitions states as OpenHAB switches and zone states as OpenHAB contacts.  For esp8266/esp32, a panel status message is also sent as a string to OpenHAB.  See https://github.com/jimtng/dscalarm-mqtt for an integration using the Homie convention for OpenHAB's Homie MQTT component.

* **Homey**: Integrates with [Athom Homey](https://www.athom.com/en/) and the [Homeyduino](https://github.com/athombv/homey-arduino-library/) library, including armed, alarm, and fire states (currently limited to one partition), and zone states.  Thanks to [MagnusPer](https://github.com/MagnusPer) for contributing this example!

* **Pushbullet** (esp8266/esp32-only):  Demonstrates how to send a push notification when the status has changed. This example sends notifications via [Pushbullet](https://www.pushbullet.com) and requires the esp8266/esp32 for SSL support.

* **Twilio-SMS** (esp8266/esp32-only): Demonstrates how to send an SMS text message when the status has changed. This example sends texts via [Twilio](https://www.twilio.com) and requires the esp8266/esp32 for SSL support - thanks to [ColingNG](https://github.com/ColinNg) for contributing this example!

* **Email** (esp8266/esp32-only): Demonstrates how to send an email when the status has changed. Email is sent using SMTPS (port 465) with SSL for encryption - this is necessary on the esp8266/esp32 until STARTTLS can be supported.  For example, this will work with Gmail after changing the account settings to [allow less secure apps](https://support.google.com/accounts/answer/6010255).

  This can be used to send SMS text messages if the number's service provider has an [email to SMS gateway](https://en.wikipedia.org/wiki/SMS_gateway#Email_clients) - examples for the US:
  * T-mobile: 5558675309@tmomail.net
  * Verizon: 5558675309@vtext.com
  * Sprint: 5558675309@messaging.sprintpcs.com
  * AT&T: 5558675309@txt.att.net

* **VirtualKeypad-Blynk** (esp8266/esp32-only): Provides a virtual keypad interface for the free [Blynk](https://www.blynk.cc) app on iOS and Android.  Scan one of the following QR codes from within the Blynk app for an example keypad layout:
  - [Virtual keypad with 16 zones](https://user-images.githubusercontent.com/12835671/42364287-41ca6662-80c0-11e8-85e7-d579b542568d.png)
  - [Virtual keypad with 32 zones](https://user-images.githubusercontent.com/12835671/42364293-4512b720-80c0-11e8-87bd-153c4e857b4e.png)

  Note: Installing [Blynk as a local server](https://github.com/blynkkk/blynk-server) is recommended to keep control of the security system internal to your network.  This also lets you use as many widgets as needed for free - local servers can setup users with any amount of Blynk Energy.  Using the default Blynk cloud service with the above example layouts requires more of Blynk's Energy units than available on the free usage tier.

* **VirtualKeypad-Web** (esp8266-only): Provides a virtual keypad web interface, using the esp8266 itself as a standalone web server.  This example uses the [ESP Async Web Server](https://github.com/me-no-dev/ESPAsyncWebServer) and Websockets - thanks to [Elektrik1](https://github.com/Elektrik1) for contributing this example!

* **Unlocker**: Checks all possible 4-digit installer codes until a valid code is found, including handling keypad lockout if enabled.  The valid code is output to serial as well as repeatedly flashed with the built-in LED.

* **KeybusReader**: Decodes and prints data from the Keybus to a serial interface, including reading from serial for the virtual keypad.  This can be used to help decode the Keybus protocol and is also handy as a troubleshooting tool to verify that data is displayed without errors.

  See [`src/dscKeybusPrintData.cpp`](https://github.com/taligentx/dscKeybusInterface/blob/master/src/dscKeybusPrintData.cpp) for all currently known Keybus protocol commands and messages.  Issues and pull requests with additions/corrections are welcome!

## External examples
* **[dscalarm-mqtt](https://github.com/jimtng/dscalarm-mqtt)**: implementation of the [Homie](https://homieiot.github.io) MQTT convention

## Wiring

```
DSC Aux(+) ---+--- Arduino Vin pin
              |
              +--- 5v voltage regulator --- esp8266 NodeMCU / Wemos D1 Mini 5v pin
                                            esp32 development board 5v pin

DSC Aux(-) --- Arduino/esp8266/esp32 Ground

                    Arduino/esp8266    +--- dscClockPin (Arduino Uno: 2,3 / esp8266: D1,D2,D8)
DSC Yellow ---+--- 15k ohm resistor ---|
              |                        +--- 10k ohm resistor --- Ground
              |
              |         esp32          +--- dscClockPin (esp32: 4,13,16-39)
              +--- 33k ohm resistor ---|
                                       +--- 10k ohm resistor --- Ground

                    Arduino/esp8266    +--- dscReadPin (Arduino Uno: 2-12 / esp8266: D1,D2,D8)
DSC Green ----+--- 15k ohm resistor ---|
              |                        +--- 10k ohm resistor --- Ground
              |
              |         esp32          +--- dscReadPin (esp32: 4,13,16-39)
              +--- 33k ohm resistor ---|
                                       +--- 10k ohm resistor --- Ground
 
Virtual keypad (optional):
DSC Green ---- NPN collector --\
                                |-- NPN base --- 1k ohm resistor --- dscWritePin (Arduino Uno: 2-12 / esp8266: D1,D2,D8 / esp32: 4,13,16-33)
      Ground --- NPN emitter --/
```

* The DSC Keybus operates at ~12.6v, a pair of resistors per data line will bring this down to an appropriate voltage for each microcontroller.
    * Arduino: connect the DSC Yellow (Clock) line to a [hardware interrupt pin](https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/) - for the Uno, these are pins 2 or 3.  The DSC Green (Data) line can be connected to any of the remaining digital pins 2-12.
    * esp8266: connect the DSC lines to GPIO pins that are normally low to avoid putting spurious data on the Keybus: D1 (GPIO5), D2 (GPIO4) and D8 (GPIO15).
    * esp32: connect the DSC lines to GPIO pins that do not send signals at boot: 4, 13, 16-39.  For virtual keypad, use pins 4, 13, 16-33 - pins 34-39 are input only and cannot be used.
* Virtual keypad uses an NPN transistor and a resistor to write to the Keybus.  Most small signal NPN transistors should be suitable, for example:
  * 2N3904
  * BC547, BC548, BC549
  * That random NPN at the bottom of your parts bin (my choice)
* Power:
  * Arduino boards can be powered directly from the DSC panel
  * esp8266/esp32 development boards should use a 5v voltage regulator:
    - LM2596-based step-down buck converter modules are reasonably efficient and commonly available for under $1USD shipped (eBay, Aliexpress, etc).
    - MP2307-based step-down buck converter modules (aka Mini360) are also available but some versions run hot with an efficiency nearly as poor as linear regulators.
    - Linear voltage regulators (LM7805, etc) will work but are inefficient and run hot - these may need a heatsink.
* Connections should be soldered, breadboards can cause issues.

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
* Door chime enable/disable: `c`
* Fire reset: `r`
* Quick exit: `x`
* Change partition: `/` + `partition number` or set `writePartition` to the partition number.  Examples:
  * Switch to partition 2 and send keys: `/2` + `1234`
  * Switch back to partition 1: `/1`
  * Set directly in sketch: `dsc.writePartition = 8;`
* Command output 1: `[`
* Command output 2: `]`
* Command output 3: `{`
* Command output 4: `}`

## DSC Configuration
Panel options affecting this interface, configured by `*8 + installer code` - see the `Unlocker` sketch if your panel's installer code is unknown.  Refer to the DSC installation manual for your panel to configure these options:
* PC1555MX/5015 section `370`, PC1616/PC1832/PC1864 section `377`:
  - Swinger shutdown: By default, the panel will limit the number of alarm commands sent in a single armed cycle to 3 - for example, a zone alarm being triggered multiple times will stop reporting after 3 alerts.  This is to avoid sending alerts repeatedly to a third-party monitoring service, and also affects this interface.  As I do not use a monitoring service, I disable swinger shutdown by setting this to `000`.

  - AC power failure reporting delay: The default delay is 30 minutes and can be set to `000` to immediately report a power failure.  

## Notes
* For OTA updates on esp8266 and esp32, you may need to stop the interface using `dsc.stop();`:
  ```
  void setup() {
    ...
    ArduinoOTA.onStart([]() {
      dsc.stop();
    ...
  ```
* Memory usage can be adjusted based on the number of partitions, zones, and data buffer size specified in [`src/dscKeybusInterface.h`](https://github.com/taligentx/dscKeybusInterface/blob/master/src/dscKeybusInterface.h).  Default settings:
  * Arduino: up to 4 partitions, 32 zones, 10 buffered commands
  * esp8266/esp32: up to 8 partitions, 64 zones, 50 buffered commands

* PCB layouts are available in [`extras/PCB Layouts`](https://github.com/taligentx/dscKeybusInterface/tree/master/extras/PCB%20Layouts) - thanks to [sjlouw](https://github.com/sj-louw) for contributing these designs!

* Support for other platforms depends on adjusting the code to use their platform-specific timers.  In addition to hardware interrupts to capture the DSC clock, this library uses platform-specific timer interrupts to capture the DSC data line in a non-blocking way 250μs after the clock changes (without using `delayMicroseconds()`).  This is necessary because the clock and data are asynchronous - I've observed keypad data delayed up to 160μs after the clock falls.

## Troubleshooting
If you are running into issues:
1. Run the `KeybusReader` example sketch and view the serial output to verify that the interface is capturing data successfully without reporting CRC errors.  
    * If data is not showing up or has errors, check the clock and data line wiring, resistors, and all connections.
2. For virtual keypad, run the `KeybusReader` example sketch and enter keys through serial and verify that the keys appear in the output and that the panel responds.  
    * If keys are not displayed in the output, verify the transistor pinout, base resistor, and wiring connections.
3. Run the `Status` example sketch and view the serial output to verify that the interface displays events from the security system correctly as partitions are armed, zones opened, etc.

## References
[AVR Freaks - DSC Keybus Protocol](https://www.avrfreaks.net/forum/dsc-keybus-protocol): An excellent discussion on how data is sent on the Keybus.

[stagf15/DSC_Panel](https://github.com/stagf15/DSC_Panel): A library that nearly works for the PC1555MX but had timing and data errors.  Writing this library from scratch was primarily a programming exercise, otherwise it should be possible to patch the DSC_Panel library.

[dougkpowers/pc1550-interface](https://github.com/dougkpowers/pc1550-interface): An interface for the DSC Classic series.
