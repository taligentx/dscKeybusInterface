/*
 *  DSC Status
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
 *  https://github.com/taligentx/
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
    dsc.statusChanged = false;  // Reset the status flag

    if (dsc.troubleStatusChanged) {
      dsc.troubleStatusChanged = false;
      if (dsc.troubleStatus) Serial.println(F("Trouble status on"));
      else Serial.println(F("Trouble status restored"));
    }

    if (dsc.partitionArmedChanged) {
      dsc.partitionArmedChanged = false;
      if (dsc.partitionArmed) {
        Serial.print(dsc.dscTime);
        Serial.print(F(" | Partition armed"));
        if (dsc.partitionArmedAway) Serial.println(F(" away"));
        if (dsc.partitionArmedStay) Serial.println(F(" stay"));
      }
      else {
        Serial.print(dsc.dscTime);
        Serial.println(F(" | Partition disarmed"));
      }
    }

    if (dsc.partitionAlarmChanged) {
      dsc.partitionAlarmChanged = false;
      if (dsc.partitionAlarm) {
        Serial.print(dsc.dscTime);
        Serial.println(F(" | Partition in alarm"));
      }
    }

    if (dsc.exitDelayChanged) {
      dsc.exitDelayChanged = false;
      if (dsc.exitDelay) Serial.println(F("Exit delay in progress"));
    }

    if (dsc.entryDelayChanged) {
      dsc.entryDelayChanged = false;
      if (dsc.entryDelay) Serial.println(F("Entry delay in progress"));
    }

    if (dsc.batteryTroubleChanged) {
      dsc.batteryTroubleChanged = false;
      Serial.print(dsc.dscTime);
      if (dsc.batteryTrouble) Serial.println(F(" | Panel battery trouble"));
      else Serial.println(F(" | Panel battery restored"));
    }

    if (dsc.powerTroubleChanged) {
      dsc.powerTroubleChanged = false;
      Serial.print(dsc.dscTime);
      if (dsc.powerTrouble) Serial.println(F(" | Panel AC power trouble"));
      else Serial.println(F(" | Panel AC power restored"));
    }

    if (dsc.keypadFireAlarm) {
      dsc.keypadFireAlarm = false;
      Serial.print(dsc.dscTime);
      Serial.println(F(" | Keypad fire alarm"));
    }

    if (dsc.keypadAuxAlarm) {
      dsc.keypadAuxAlarm = false;
      Serial.print(dsc.dscTime);
      Serial.println(F(" | Keypad aux alarm"));
    }

    if (dsc.keypadPanicAlarm) {
      dsc.keypadPanicAlarm = false;
      Serial.print(dsc.dscTime);
      Serial.println(F(" | Keypad panic alarm"));
    }

    if (dsc.openZonesGroup1Changed) {
      dsc.openZonesGroup1Changed = false;
      for (byte zoneCount = 0; zoneCount < 8; zoneCount++) {
        if (dsc.openZonesChanged[zoneCount]) {
          if (dsc.openZones[zoneCount]) {
            Serial.print(F("Zone open: "));
            Serial.println(zoneCount + 1);
          }
          else {
            Serial.print(F("Zone restored: "));
            Serial.println(zoneCount + 1);
          }
        }
      }
    }

    if (dsc.alarmZonesGroup1Changed) {
      dsc.alarmZonesGroup1Changed = false;
      for (byte zoneCount = 0; zoneCount < dscZones; zoneCount++) {
        if (dsc.alarmZonesChanged[zoneCount]) {
          dsc.alarmZonesChanged[zoneCount] = false;
          if (dsc.alarmZones[zoneCount]) {
            Serial.print(dsc.dscTime);
            Serial.print(F(" | Zone alarm: "));
            Serial.println(zoneCount + 1);
          }
          else {
            Serial.print(dsc.dscTime);
            Serial.print(F(" | Zone alarm restored: "));
            Serial.println(zoneCount + 1);
          }
        }
      }
    }
  }
}
