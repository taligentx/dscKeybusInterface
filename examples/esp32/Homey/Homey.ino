/*
 *  Homey 1.1 (esp32)
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
 *  Release notes:
 *    1.1 - Added DSC Classic series support
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- 5v voltage regulator --- esp32 development board 5v pin
 *
 *      DSC Aux(-) --- esp32 Ground
 *
 *                                         +--- dscClockPin  // Default: 18
 *      DSC Yellow --- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin   // Default: 19
 *      DSC Green ---- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *      Classic series only, PGM configured for PC-16 output:
 *      DSC PGM ---+-- 1k ohm resistor --- DSC Aux(+)
 *                 |
 *                 |                       +--- dscPC16Pin   // Default: 17
 *                 +-- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *      Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin  // Default: 21
 *            Ground --- NPN emitter --/
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

// DSC Classic series: uncomment for PC1500/PC1550 support (requires PC16-OUT configuration per README.md)
//#define dscClassicSeries

#include <WiFi.h>
#include <Homey.h>
#include <dscKeybusInterface.h>

// Settings
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* accessCode = "";  // An access code is required to disarm/night arm and may be required to arm based on panel configuration.

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin 18  // 4,13,16-39
#define dscReadPin  19  // 4,13,16-39
#define dscPC16Pin  17  // DSC Classic Series only, 4,13,16-39
#define dscWritePin 21  // 4,13,16-33

// Initialize components
#ifndef dscClassicSeries
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
#else
dscClassicInterface dsc(dscClockPin, dscReadPin, dscPC16Pin, dscWritePin, accessCode);
#endif
bool wifiConnected = true;


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  Serial.print(F("WiFi...."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print(F("connected: "));
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

  // Updates status if WiFi drops and reconnects
  if (!wifiConnected && WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi reconnected");
    wifiConnected = true;
    dsc.resetStatus();  // Resets the state of all status components as changed to get the current status
  }
  else if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    Serial.println("WiFi disconnected");
    wifiConnected = false;
  }

  // Run the Homey loop
  Homey.loop();

  dsc.loop();

  if (dsc.statusChanged) {      // Checks if the security system status has changed
    dsc.statusChanged = false;  // Reset the status tracking flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // loop() more often, or increase dscBufferSize in the library: src/dscKeybus.h or src/dscClassic.h
    if (dsc.bufferOverflow) {
      Serial.println(F("Keybus buffer overflow"));
      dsc.bufferOverflow = false;
    }

    // Sends the access code when needed by the panel for arming
    if (dsc.accessCodePrompt) {
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
     dsc.write('s');  // Keypad stay arm

  }
}

// Arm away
void armAway() {
   if (Homey.value.toInt() == 1 && !dsc.armed[0] && !dsc.exitDelay[0]) {  // Read the argument sent from the homey flow
     dsc.write('w');  // Keypad away arm
  }
}

// Disarm
void disarm() {
   if (Homey.value.toInt() == 1 && (dsc.armed[0] || dsc.exitDelay[0])) {
    dsc.write(accessCode);
  }
}
