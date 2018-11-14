/*
 *  Homey 1.0 (esp8266)
 *
 *  Processes the security system status for partition 1 and allows for control using Athom Homey.
 *
 *  Athom Homey: https://www.athom.com/en/
 *  Arduino library for communicating with Homey: https://github.com/athombv/homey-arduino-library
 *
 *  The interface listens for commands in the functions armStay, armAway, disable, and publishes alarm states to the
 *  configured Capabilities homealarm_state, alarm_tamper, alarm_fire (optional).
 *
 *  Zone states are published by Homey.trigger command including the zone number.
 *
 *  Wiring:
 *      DSC Aux(-) --- esp8266 ground
 *
 *                                         +--- dscClockPin (esp8266: D1, D2, D8)
 *      DSC Yellow --- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (esp8266: D1, D2, D8)
 *      DSC Green ---- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (esp8266: D1, D2, D8)
 *            Ground --- NPN emitter --/
 *
 *  Power (when disconnected from USB):
 *      DSC Aux(+) ---+--- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
 *                    |
 *                    +--- 3.3v voltage regulator --- esp8266 bare module VCC pin (ESP-12, etc)
 *
 *  Virtual keypad uses an NPN transistor to pull the data line low - most small signal NPN transistors should
 *  be suitable, for example:
 *   -- 2N3904
 *   -- BC547, BC548, BC549
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  Many thanks to Magnus for contributing this example: https://github.com/MagnusPer
 *
 *  This example code is in the public domain.
 */

#include <ESP8266WiFi.h>
#include <Homey.h>
#include <dscKeybusInterface.h>

const char* wifiSSID = "";
const char* wifiPassword = "";
const char* accessCode = "";  // An access code is required to disarm/night arm and may be required to arm based on panel configuration.

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.print("WiFi connected: ");
  Serial.println(WiFi.localIP());

  // Initiate and starts the Homey interface
  Homey.begin("dscKeybus");
  Homey.setClass("homealarm");
  Homey.addCapability("homealarm_state");
  Homey.addCapability("alarm_tamper");
  Homey.addCapability("alarm_fire");
  Homey.addAction("armStay", armStay);
  Homey.addAction("armAway", armAway);
  Homey.addAction("disarm", disarm);

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();
  Serial.println(F("DSC Keybus Interface is online."));

  // Set init value for tamper and fire alarm status
  Homey.setCapabilityValue("alarm_tamper", false);
  Homey.setCapabilityValue("alarm_fire", false);
}


void loop() {
  // Run the Homey loop
  Homey.loop();

  if (dsc.handlePanel() && dsc.statusChanged) {  // Processes data only when a valid Keybus command has been read
    dsc.statusChanged = false;                   // Reset the status tracking flag

    // Sends the access code when needed by the panel for arming
    if (dsc.accessCodePrompt && dsc.writeReady) {
      dsc.accessCodePrompt = false;
      dsc.write(accessCode);
    }

    // Publish armed status
    if (dsc.armedChanged[0]) {
      dsc.armedChanged[0] = false;  // Resets the partition armed status flag
      if (dsc.armed[0]) {
        if (dsc.armedAway[0]) Homey.setCapabilityValue("homealarm_state", "armed", true);
        if (dsc.armedStay[0]) Homey.setCapabilityValue("homealarm_state", "partially_armed", true);
      }
      else Homey.setCapabilityValue("homealarm_state", "disarmed", true);
    }

    // Publish alarm status
    if (dsc.alarmChanged[0]) {
      dsc.alarmChanged[0] = false;  // Resets the partition alarm status flag
      if (dsc.alarm[0]) Homey.setCapabilityValue("alarm_tamper", true);
      else Homey.setCapabilityValue("alarm_tamper", false);
    }

    // Publish fire alarm status
    if (dsc.fireChanged[0]) {
      dsc.fireChanged[0] = false;  // Resets the fire status flag
      if (dsc.fire[0]) Homey.setCapabilityValue("alarm_fire", true);
      else Homey.setCapabilityValue("alarm_fire", false);
    }

    // Publish zones 1-64 status
    // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones:
    //   openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
    //   openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
    //   ...
    //   openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
    if (dsc.openZonesStatusChanged) {
      dsc.openZonesStatusChanged = false;                           // Resets the open zones status flag
      for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
        for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
          if (bitRead(dsc.openZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual open zone status flag
            bitWrite(dsc.openZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual open zone status flag
            if (bitRead(dsc.openZones[zoneGroup], zoneBit)) {
              Homey.trigger("ZoneOpen", (zoneBit + 1 + (zoneGroup * 8)));
            }
            else {
              Homey.trigger("ZoneRestored", (zoneBit + 1 + (zoneGroup * 8)));
            }
          }
        }
      }
    }

    // Publish alarm zones 1-64
    // Zone alarm status is stored in the alarmZones[] and alarmZonesChanged[] arrays using 1 bit per zone, up to 64 zones
    //   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
    //   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
    //   ...
    //   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
    if (dsc.alarmZonesStatusChanged) {
      dsc.alarmZonesStatusChanged = false;                           // Resets the alarm zones status flag
      for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
        for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
          if (bitRead(dsc.alarmZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual alarm zone status flag
            bitWrite(dsc.alarmZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual alarm zone status flag
            if (bitRead(dsc.alarmZones[zoneGroup], zoneBit)) {
              Homey.trigger("AlarmZoneOpen", (zoneBit + 1 + (zoneGroup * 8)));
            }
            else {
              Homey.trigger("AlarmZoneRestored", (zoneBit + 1 + (zoneGroup * 8)));
            }
          }
        }
      }
    }
  }
}


// Handles messages received from Homey

// Arm stay
void armStay() {
   if (Homey.value.toInt() == 1 && !dsc.armed[0] && !dsc.exitDelay[0]) {  // Read the argument sent from the homey flow
     while (!dsc.writeReady) dsc.handlePanel();  // Continues processing Keybus data until ready to write
     dsc.write('s');  // Keypad stay arm

  }
}


// Arm away
void armAway() {
   if (Homey.value.toInt() == 1 && !dsc.armed[0] && !dsc.exitDelay[0]) {  // Read the argument sent from the homey flow
     while (!dsc.writeReady) dsc.handlePanel();  // Continues processing Keybus data until ready to write
     dsc.write('w');  // Keypad away arm
  }
}


// Disarm
void disarm() {
   if (Homey.value.toInt() == 1 && (dsc.armed[0] || dsc.exitDelay[0])) {
    while (!dsc.writeReady) dsc.handlePanel();  // Continues processing Keybus data until ready to write
    dsc.write(accessCode);
  }
}
