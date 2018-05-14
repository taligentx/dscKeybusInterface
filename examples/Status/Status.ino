/*
 *  DSC Status (Arduino, esp8266)
 *
 *  Processes and prints the security system status to a serial interface, including reading from serial for the
 *  virtual keypad.  This demonstrates how to determine if the security system status has changed and what has
 *  changed, and how to take action based on those changes.
 *
 *  Wiring:
 *      DSC Aux(-) --- Arduino/esp8266 ground
 *
 *                                         +--- dscClockPin (Arduino Uno: 2,3 / esp8266: D1,D2,D8)
 *      DSC Yellow --- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (Arduino Uno: 2-12 / esp8266: D1,D2,D8)
 *      DSC Green ---- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (Arduino Uno: 2-12 / esp8266: D1,D2,D8)
 *            Ground --- NPN emitter --/
 *
 *  Power (when disconnected from USB):
 *      DSC Aux(+) ---+--- Arduino Vin pin
 *                    |
 *                    +--- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
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
 *  This example code is in the public domain.
 */

#include <dscKeybusInterface.h>

// Configures the Keybus interface with the specified pins - dscWritePin is
// optional, leaving it out disables the virtual keypad
#define dscClockPin 3
#define dscReadPin 4
#define dscWritePin 5
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();

  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {

  // Reads from serial input and writes to the Keybus as a virtual keypad
  if (Serial.available() > 0 && dsc.writeReady) {
    dsc.write(Serial.read());
  }

  if (dsc.handlePanel() && dsc.statusChanged) {  // Processes data only when a valid Keybus command has been read
    dsc.statusChanged = false;                   // Resets the status flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // handlePanel() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
    if (dsc.bufferOverflow) Serial.println(F("Keybus buffer overflow"));
    dsc.bufferOverflow = false;

    if (dsc.troubleStatusChanged) {
      dsc.troubleStatusChanged = false;  // Resets the trouble status flag
      if (dsc.troubleStatus) Serial.println(F("Trouble status on"));
      else Serial.println(F("Trouble status restored"));
    }

    if (dsc.partitionArmedChanged) {
      dsc.partitionArmedChanged = false;  // Resets the partition armed status flag
      if (dsc.partitionArmed) {
        Serial.print(F("Partition armed"));
        if (dsc.partitionArmedAway) Serial.println(F(" away"));
        if (dsc.partitionArmedStay) Serial.println(F(" stay"));
      }
      else Serial.println(F("Partition disarmed"));
    }

    if (dsc.partitionAlarmChanged) {
      dsc.partitionAlarmChanged = false;  // Resets the partition alarm status flag
      if (dsc.partitionAlarm) {
        Serial.print(dsc.dscTime);        // Messages in the 0xA5 panel command include a timestamp
        Serial.println(F(" | Partition in alarm"));
      }
    }

    if (dsc.exitDelayChanged) {
      dsc.exitDelayChanged = false;  // Resets the exit delay status flag
      if (dsc.exitDelay) Serial.println(F("Exit delay in progress"));
    }

    if (dsc.entryDelayChanged) {
      dsc.entryDelayChanged = false;  // Resets the entry delay status flag
      if (dsc.entryDelay) Serial.println(F("Entry delay in progress"));
    }

    if (dsc.batteryTroubleChanged) {
      dsc.batteryTroubleChanged = false;  // Resets the battery trouble status flag
      Serial.print(dsc.dscTime);          // Messages in the 0xA5 panel command include a timestamp
      if (dsc.batteryTrouble) Serial.println(F(" | Panel battery trouble"));
      else Serial.println(F(" | Panel battery restored"));
    }

    if (dsc.powerTroubleChanged) {
      dsc.powerTroubleChanged = false;  // Resets the power trouble status flag
      Serial.print(dsc.dscTime);        // Messages in the 0xA5 panel command include a timestamp
      if (dsc.powerTrouble) Serial.println(F(" | Panel AC power trouble"));
      else Serial.println(F(" | Panel AC power restored"));
    }

    if (dsc.fireStatusChanged) {
      dsc.fireStatusChanged = false;  // Resets the fire status flag
      if (dsc.fireStatus) Serial.println(F("Fire alarm on"));
      else Serial.println(F("Fire alarm restored"));
    }

    if (dsc.keypadFireAlarm) {
      dsc.keypadFireAlarm = false;  // Resets the keypad fire alarm status flag
      Serial.print(dsc.dscTime);    // Messages in the 0xA5 panel command include a timestamp
      Serial.println(F(" | Keypad fire alarm"));
    }

    if (dsc.keypadAuxAlarm) {
      dsc.keypadAuxAlarm = false;  // Resets the keypad auxiliary alarm status flag
      Serial.print(dsc.dscTime);   // Messages in the 0xA5 panel command include a timestamp
      Serial.println(F(" | Keypad aux alarm"));
    }

    if (dsc.keypadPanicAlarm) {
      dsc.keypadPanicAlarm = false;  // Resets the keypad panic alarm status flag
      Serial.print(dsc.dscTime);     // Messages in the 0xA5 panel command include a timestamp
      Serial.println(F(" | Keypad panic alarm"));
    }

    // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones
    if (dsc.openZonesStatusChanged) {
      dsc.openZonesStatusChanged = false;                           // Resets the open zones status flag
      for (byte zoneGroup = 0; zoneGroup < 8; zoneGroup++) {
        for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
          if (bitRead(dsc.openZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual open zone status flag
            bitWrite(dsc.openZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual open zone status flag
            if (bitRead(dsc.openZones[zoneGroup], zoneBit)) {
              Serial.print(F("Zone open: "));
              Serial.println(zoneBit + 1 + (zoneGroup * 8));
            }
            else {
              Serial.print(F("Zone restored: "));
              Serial.println(zoneBit + 1 + (zoneGroup * 8));
            }
          }
        }
      }
    }

    // Zone alarm status is stored in the alarmZones[] and alarmZonesChanged[] arrays using 1 bit per zone, up to 64 zones
    if (dsc.alarmZonesStatusChanged) {
      dsc.alarmZonesStatusChanged = false;                           // Resets the alarm zones status flag
      for (byte zoneGroup = 0; zoneGroup < 8; zoneGroup++) {
        for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
          if (bitRead(dsc.alarmZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual alarm zone status flag
            bitWrite(dsc.alarmZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual alarm zone status flag
            if (bitRead(dsc.alarmZones[zoneGroup], zoneBit)) {
              Serial.print(dsc.dscTime);
              Serial.print(F(" | Zone alarm: "));
              Serial.println(zoneBit + 1 + (zoneGroup * 8));
            }
            else {
              Serial.print(dsc.dscTime);
              Serial.print(F(" | Zone alarm restored: "));
              Serial.println(zoneBit + 1 + (zoneGroup * 8));
            }
          }
        }
      }
    }
  }
}
