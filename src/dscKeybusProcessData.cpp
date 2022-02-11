/*
    DSC Keybus Interface

    Functions used by all sketches to track the security system status.  These
    are the most commonly used status data by the example sketches.

    Sketches can be extended to support all Keybus data (as seen in
    dscKeybusPrintData.cpp) by checking panelData[] - see the Unlocker
    sketch for an example of this.

    If there is a status that is not currently being tracked and would
    be useful in most applications, post an issue/PR:

    https://github.com/taligentx/dscKeybusInterface

    This library is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dscKeybus.h"


// Resets the state of all status components as changed for sketches to get the current status
void dscKeybusInterface::resetStatus() {
  statusChanged = true;
  keybusChanged = true;
  troubleChanged = true;
  powerChanged = true;
  batteryChanged = true;
  for (byte partition = 0; partition < dscPartitions; partition++) {
    readyChanged[partition] = true;
    armedChanged[partition] = true;
    alarmChanged[partition] = true;
    fireChanged[partition] = true;
    disabled[partition] = true;
  }
  openZonesStatusChanged = true;
  alarmZonesStatusChanged = true;
  for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
    openZonesChanged[zoneGroup] = 0xFF;
    alarmZonesChanged[zoneGroup] = 0xFF;
  }
  pgmOutputsChanged[0] = 0xFF;
  pgmOutputsChanged[1] = 0x3F;
}


// Sets the panel time
bool dscKeybusInterface::setTime(unsigned int year, byte month, byte day, byte hour, byte minute, const char* accessCode, byte timePartition) {

  // Loops if a previous write is in progress
  while(writeKeyPending || writeKeysPending) {
    loop();
    #if defined(ESP8266)
    yield();
    #endif
  }

  if (!ready[0]) return false;  // Skips if partition 1 is not ready

  if (hour > 23 || minute > 59 || month > 12 || day > 31 || year > 2099 || (year > 99 && year < 1900)) return false;  // Skips if input date/time is invalid
  static char timeEntry[21];
  strcpy(timeEntry, "*6");
  strcat(timeEntry, accessCode);
  strcat(timeEntry, "1");

  char timeChar[3];
  if (hour < 10) strcat(timeEntry, "0");
  itoa(hour, timeChar, 10);
  strcat(timeEntry, timeChar);

  if (minute < 10) strcat(timeEntry, "0");
  itoa(minute, timeChar, 10);
  strcat(timeEntry, timeChar);

  if (month < 10) strcat(timeEntry, "0");
  itoa(month, timeChar, 10);
  strcat(timeEntry, timeChar);

  if (day < 10) strcat(timeEntry, "0");
  itoa(day, timeChar, 10);
  strcat(timeEntry, timeChar);

  if (year >= 2000) year -= 2000;
  else if (year >= 1900) year -= 1900;
  if (year < 10) strcat(timeEntry, "0");
  itoa(year, timeChar, 10);
  strcat(timeEntry, timeChar);

  strcat(timeEntry, "#");

  if (writePartition != timePartition) {
    byte previousPartition = writePartition;
    writePartition = timePartition;
    write(timeEntry);
    while(writeKeyPending || writeKeysPending) {
      loop();
      #if defined(ESP8266)
      yield();
      #endif
    }
    writePartition = previousPartition;
  }
  else write(timeEntry);

  return true;
}


// Processes status commands: 0x05 (Partitions 1-4) and 0x1B (Partitions 5-8)
void dscKeybusInterface::processPanelStatus() {

  // Trouble status
  if (panelData[3] <= 0x06) {  // Ignores trouble light status in intermittent states
    if (bitRead(panelData[2],4)) trouble = true;
    else trouble = false;
    if (trouble != previousTrouble) {
      previousTrouble = trouble;
      troubleChanged = true;
      if (!pauseStatus) statusChanged = true;
    }
  }

  // Sets partition counts based on the status command and generation of panel
  byte partitionStart = 0;
  byte partitionCount = 0;
  if (panelData[0] == 0x05) {
    partitionStart = 0;
    if (keybusVersion1) partitionCount = 2;  // Handles earlier panels that support up to 2 partitions
    else partitionCount = 4;
    if (dscPartitions < partitionCount) partitionCount = dscPartitions;
  }
  else if (dscPartitions > 4 && panelData[0] == 0x1B) {
    partitionStart = 4;
    partitionCount = 8;
    if (dscPartitions < partitionCount) partitionCount = dscPartitions;
  }

  // Sets status per partition
  for (byte partitionIndex = partitionStart; partitionIndex < partitionCount; partitionIndex++) {
    byte statusByte, messageByte;
    if (partitionIndex < 4) {
      statusByte = (partitionIndex * 2) + 2;
      messageByte = (partitionIndex * 2) + 3;
    }
    else {
      statusByte = ((partitionIndex - 4) * 2) + 2;
      messageByte = ((partitionIndex - 4) * 2) + 3;
    }

    // Partition disabled status
    if (panelData[messageByte] == 0xC7) {
      disabled[partitionIndex] = true;
      processReadyStatus(partitionIndex, false);
    }
    else disabled[partitionIndex] = false;
    if (disabled[partitionIndex] != previousDisabled[partitionIndex]) {
      previousDisabled[partitionIndex] = disabled[partitionIndex];
      disabledChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
    }

    // Status lights
    lights[partitionIndex] = panelData[statusByte];
    if (lights[partitionIndex] != previousLights[partitionIndex]) {
      previousLights[partitionIndex] = lights[partitionIndex];
      if (!pauseStatus) statusChanged = true;
    }

    // Status messages
    status[partitionIndex] = panelData[messageByte];
    if (status[partitionIndex] != previousStatus[partitionIndex]) {
      previousStatus[partitionIndex] = status[partitionIndex];
      if (!pauseStatus) statusChanged = true;
    }

    // Fire status
    if (panelData[messageByte] < 0x12) {  // Ignores fire light status in intermittent states
      if (bitRead(panelData[statusByte],6)) fire[partitionIndex] = true;
      else fire[partitionIndex] = false;
      if (fire[partitionIndex] != previousFire[partitionIndex]) {
        previousFire[partitionIndex] = fire[partitionIndex];
        fireChanged[partitionIndex] = true;
        if (!pauseStatus) statusChanged = true;
      }
    }

    // Messages
    switch (panelData[messageByte]) {

      // Ready
      case 0x01:         // Partition ready
      case 0x02: {       // Stay/away zones open
        processReadyStatus(partitionIndex, true);
        processEntryDelayStatus(partitionIndex, false);

        armedStay[partitionIndex] = false;
        armedAway[partitionIndex] = false;
        armed[partitionIndex] = false;
        if (armed[partitionIndex] != previousArmed[partitionIndex]) {
          previousArmed[partitionIndex] = armed[partitionIndex];
          armedChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }

        processAlarmStatus(partitionIndex, false);
        break;
      }

      // Zones open
      case 0x03: {
        processReadyStatus(partitionIndex, false);
        processEntryDelayStatus(partitionIndex, false);
        break;
      }

      // Armed
      case 0x04:         // Armed stay
      case 0x05: {       // Armed away
        if (panelData[messageByte] == 0x04) {
          armedStay[partitionIndex] = true;
          armedAway[partitionIndex] = false;
        }
        else {
          armedStay[partitionIndex] = false;
          armedAway[partitionIndex] = true;
        }

        writeArm[partitionIndex] = false;

        armed[partitionIndex] = true;
        if (armed[partitionIndex] != previousArmed[partitionIndex] || armedStay[partitionIndex] != previousArmedStay[partitionIndex]) {
          previousArmed[partitionIndex] = armed[partitionIndex];
          previousArmedStay[partitionIndex] = armedStay[partitionIndex];
          armedChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }

        processReadyStatus(partitionIndex, false);
        processExitDelayStatus(partitionIndex, false);
        exitState[partitionIndex] = 0;
        processEntryDelayStatus(partitionIndex, false);
        break;
      }

      // Exit delay in progress
      case 0x08: {
        writeArm[partitionIndex] = false;

        processExitDelayStatus(partitionIndex, true);

        if (exitState[partitionIndex] != DSC_EXIT_NO_ENTRY_DELAY) {
          if (bitRead(lights[partitionIndex],3)) exitState[partitionIndex] = DSC_EXIT_STAY;
          else exitState[partitionIndex] = DSC_EXIT_AWAY;
          if (exitState[partitionIndex] != previousExitState[partitionIndex]) {
            previousExitState[partitionIndex] = exitState[partitionIndex];
            exitDelayChanged[partitionIndex] = true;
            exitStateChanged[partitionIndex] = true;
            if (!pauseStatus) statusChanged = true;
          }
        }

        processReadyStatus(partitionIndex, true);
        break;
      }

      // Arming with no entry delay
      case 0x09: {
        processReadyStatus(partitionIndex, true);
        exitState[partitionIndex] = DSC_EXIT_NO_ENTRY_DELAY;
        break;
      }

      // Entry delay in progress
      case 0x0C: {
        processReadyStatus(partitionIndex, false);
        processEntryDelayStatus(partitionIndex, true);
        break;
      }

      // Partition in alarm
      case 0x11: {
        processReadyStatus(partitionIndex, false);
        processEntryDelayStatus(partitionIndex, false);
        processAlarmStatus(partitionIndex, true);
        break;
      }

      // Arming with bypassed zones
      case 0x15: {
        processReadyStatus(partitionIndex, true);
        break;
      }

      // Partition armed with no entry delay
      case 0x06:
      case 0x16: {
        armed[partitionIndex] = true;

        // Sets an armed mode if not already set, used if interface is initialized while the panel is armed
        if (!armedStay[partitionIndex] && !armedAway[partitionIndex]) {
          if (panelData[messageByte] == 0x06) {
            armedStay[partitionIndex] = true;
            previousArmedStay[partitionIndex] = armedStay[partitionIndex];
          }
          else armedAway[partitionIndex] = true;
        }

        processNoEntryDelayStatus(partitionIndex, true);
        processReadyStatus(partitionIndex, false);
        break;
      }

      // Partition disarmed
      case 0x3D:
      case 0x3E: {
        if (panelData[messageByte] == 0x3E) {  // Sets ready only during Partition disarmed
          processReadyStatus(partitionIndex, true);
        }

        processExitDelayStatus(partitionIndex, false);
        exitState[partitionIndex] = 0;
        processEntryDelayStatus(partitionIndex, false);
        processArmed(partitionIndex, false);
        processAlarmStatus(partitionIndex, false);
        break;
      }

      // Invalid access code
      case 0x8F: {
        if (!armed[partitionIndex]) processReadyStatus(partitionIndex, true);
        break;
      }

      // Enter * function code
      case 0x9E:
      case 0xB8: {
        if (starKeyWait[partitionIndex]) {  // Resets the flag that waits for panel status 0x9E, 0xB8 after '*' is pressed
          starKeyWait[partitionIndex] = false;
          starKeyCheck = false;
          writeKeyPending = false;
        }
        processReadyStatus(partitionIndex, false);
        break;
      }

      // Enter access code
      case 0x9F: {
        if (writeArm[partitionIndex]) {  // Ensures access codes are only sent when an arm command is sent through this interface
          writeArm[partitionIndex] = false;
          accessCodePrompt = true;
          if (!pauseStatus) statusChanged = true;
        }

        processReadyStatus(partitionIndex, false);
        break;
      }

      default: {
        processReadyStatus(partitionIndex, false);
        break;
      }
    }
  }
}


void dscKeybusInterface::processPanel_0x16() {
  if (!validCRC()) return;

  // Panel version
  panelVersion = ((panelData[3] >> 4) * 10) + (panelData[3] & 0x0F);
}


// Panel status and zones 1-8 status
void dscKeybusInterface::processPanel_0x27() {
  if (!validCRC()) return;

  // Messages
  for (byte partitionIndex = 0; partitionIndex < 2; partitionIndex++) {
    byte messageByte = (partitionIndex * 2) + 3;

    // Armed
    if (panelData[messageByte] == 0x04 || panelData[messageByte] == 0x05) {

      processReadyStatus(partitionIndex, false);

      if (panelData[messageByte] == 0x04) {
        armedStay[partitionIndex] = true;
        armedAway[partitionIndex] = false;
      }
      else if (panelData[messageByte] == 0x05) {
        armedStay[partitionIndex] = false;
        armedAway[partitionIndex] = true;
      }

      armed[partitionIndex] = true;
      if (armed[partitionIndex] != previousArmed[partitionIndex] || armedStay[partitionIndex] != previousArmedStay[partitionIndex]) {
        previousArmed[partitionIndex] = armed[partitionIndex];
        previousArmedStay[partitionIndex] = armedStay[partitionIndex];
        armedChanged[partitionIndex] = true;
        if (!pauseStatus) statusChanged = true;
      }

      processExitDelayStatus(partitionIndex, false);
      exitState[partitionIndex] = 0;
    }

    // Armed with no entry delay
    else if (panelData[messageByte] == 0x06 || panelData[messageByte] == 0x16) {
      noEntryDelay[partitionIndex] = true;

      // Sets an armed mode if not already set, used if interface is initialized while the panel is armed
      if (!armedStay[partitionIndex] && !armedAway[partitionIndex]) armedStay[partitionIndex] = true;

      armed[partitionIndex] = true;
      if (armed[partitionIndex] != previousArmed[partitionIndex]) {
        previousArmed[partitionIndex] = armed[partitionIndex];
        previousArmedStay[partitionIndex] = armedStay[partitionIndex];
        armedChanged[partitionIndex] = true;
        if (!pauseStatus) statusChanged = true;
      }

      processExitDelayStatus(partitionIndex, false);
      exitState[partitionIndex] = 0;
      processReadyStatus(partitionIndex, false);
    }
  }

  // Zones 1-8 status is stored in openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  processZoneStatus(0, 6);
}


// Zones 9-16 status is stored in openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
void dscKeybusInterface::processPanel_0x2D() {
  if (!validCRC()) return;
  if (dscZones < 2) return;
  processZoneStatus(1, 6);
}


// Zones 17-24 status is stored in openZones[2] and openZonesChanged[2]: Bit 0 = Zone 17 ... Bit 7 = Zone 24
void dscKeybusInterface::processPanel_0x34() {
  if (!validCRC()) return;
  if (dscZones < 3) return;
  processZoneStatus(2, 6);
}


// Zones 25-32 status is stored in openZones[3] and openZonesChanged[3]: Bit 0 = Zone 25 ... Bit 7 = Zone 32
void dscKeybusInterface::processPanel_0x3E() {
  if (!validCRC()) return;
  if (dscZones < 4) return;
  processZoneStatus(3, 6);
}


/*
 *  PGM outputs 1-14 status is stored in pgmOutputs[]
 *
 *  pgmOutputs[0], Bit 0 = PGM 1 ... Bit 7 = PGM 8
 *  pgmOutputs[1], Bit 0 = PGM 9 ... Bit 5 = PGM 14
 */
void dscKeybusInterface::processPanel_0x87() {
  if (!validCRC()) return;

  pgmOutputs[0] = panelData[3] & 0x03;
  pgmOutputs[0] |= panelData[2] << 2;
  pgmOutputs[1] = panelData[2] >> 6;
  pgmOutputs[1] |= (panelData[3] & 0xF0) >> 2;

  for (byte pgmByte = 0; pgmByte < 2; pgmByte++) {
    byte pgmChanged = pgmOutputs[pgmByte] ^ previousPgmOutputs[pgmByte];

    if (pgmChanged != 0) {
      previousPgmOutputs[pgmByte] = pgmOutputs[pgmByte];
      pgmOutputsStatusChanged = true;
      if (!pauseStatus) statusChanged = true;

      for (byte pgmBit = 0; pgmBit < 8; pgmBit++) {
        if (bitRead(pgmChanged, pgmBit)) bitWrite(pgmOutputsChanged[pgmByte], pgmBit, 1);
      }
    }
  }
}


void dscKeybusInterface::processPanel_0xA5() {
  if (!validCRC()) return;

  processTime(2);

  // Timestamp
  if (panelData[6] == 0 && panelData[7] == 0) {
    statusChanged = true;
    timestampChanged = true;
    return;
  }

  byte partition = panelData[3] >> 6;
  switch (panelData[5] & 0x03) {
    case 0x00: processPanelStatus0(partition, 6); break;
    case 0x01: processPanelStatus1(partition, 6); break;
    case 0x02: processPanelStatus2(partition, 6); break;
  }
}


/*
 *  0xE6: Extended status, partitions 1-8
 *  Panels: PC5020, PC1616, PC1832, PC1864
 *
 *  0xE6 commands are split into multiple subcommands to handle up to 8 partitions/64 zones.
 */
void dscKeybusInterface::processPanel_0xE6() {
  if (!validCRC()) return;

  switch (panelData[2]) {
    case 0x09: processPanel_0xE6_0x09(); return;  // Zones 33-40 status
    case 0x0B: processPanel_0xE6_0x0B(); return;  // Zones 41-48 status
    case 0x0D: processPanel_0xE6_0x0D(); return;  // Zones 49-56 status
    case 0x0F: processPanel_0xE6_0x0F(); return;  // Zones 57-64 status
    case 0x1A: processPanel_0xE6_0x1A(); return;  // Panel status
  }
}


// Zones 33-40 status is stored in openZones[4] and openZonesChanged[4]: Bit 0 = Zone 33 ... Bit 7 = Zone 40
void dscKeybusInterface::processPanel_0xE6_0x09() {
  if (dscZones > 4) processZoneStatus(4, 3);
}


// Zones 41-48 status is stored in openZones[5] and openZonesChanged[5]: Bit 0 = Zone 41 ... Bit 7 = Zone 48
void dscKeybusInterface::processPanel_0xE6_0x0B() {
  if (dscZones > 5) processZoneStatus(5, 3);
}


// Zones 49-56 status is stored in openZones[6] and openZonesChanged[6]: Bit 0 = Zone 49 ... Bit 7 = Zone 56
void dscKeybusInterface::processPanel_0xE6_0x0D() {
  if (dscZones > 6) processZoneStatus(6, 3);
}


// Zones 57-64 status is stored in openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
void dscKeybusInterface::processPanel_0xE6_0x0F() {
  if (dscZones > 7) processZoneStatus(7, 3);
}


// Panel status
void dscKeybusInterface::processPanel_0xE6_0x1A() {

  // Panel AC power trouble
  if (panelData[6] & 0x10) powerTrouble = true;
  else powerTrouble = false;

  if (powerTrouble != previousPower) {
    previousPower = powerTrouble;
    powerChanged = true;
    if (!pauseStatus) statusChanged = true;
  }
}


void dscKeybusInterface::processPanel_0xEB() {
  if (!validCRC()) return;
  if (dscPartitions < 3) return;

  processTime(3);

  byte partition;
  switch (panelData[2]) {
    case 0x01: partition = 1; break;
    case 0x02: partition = 2; break;
    case 0x04: partition = 3; break;
    case 0x08: partition = 4; break;
    case 0x10: partition = 5; break;
    case 0x20: partition = 6; break;
    case 0x40: partition = 7; break;
    case 0x80: partition = 8; break;
    default: partition = 0; break;
  }

  switch (panelData[7] & 0x07) {
    case 0x00: processPanelStatus0(partition, 8); break;
    case 0x01: processPanelStatus1(partition, 8); break;
    case 0x02: processPanelStatus2(partition, 8); break;
    case 0x04: processPanelStatus4(partition, 8); break;
    case 0x05: processPanelStatus5(partition, 8); break;
  }
}


void dscKeybusInterface::processPanelStatus0(byte partition, byte panelByte) {

  // Processes status messages that are not partition-specific
  if (panelData[0] == 0xA5) {
    switch (panelData[panelByte]) {

      // Keypad Fire alarm
      case 0x4E: {
        keypadFireAlarm = true;
        if (!pauseStatus) statusChanged = true;
        return;
      }

      // Keypad Aux alarm
      case 0x4F: {
        keypadAuxAlarm = true;
        if (!pauseStatus) statusChanged = true;
        return;
      }

      // Keypad Panic alarm
      case 0x50: {
        keypadPanicAlarm = true;
        if (!pauseStatus) statusChanged =true;
        return;
      }

      // Panel battery trouble
      case 0xE7: {
        batteryTrouble = true;
        batteryChanged = true;
        if (!pauseStatus) statusChanged = true;
        return;
      }

      // Panel AC power failure
      case 0xE8: {
        powerTrouble = true;
        if (powerTrouble != previousPower) {
          previousPower = powerTrouble;
          powerChanged = true;
          if (!pauseStatus) statusChanged = true;
        }
        return;
      }

      // Panel battery restored
      case 0xEF: {
        batteryTrouble = false;
        batteryChanged = true;
        if (!pauseStatus) statusChanged = true;
        return;
      }

      // Panel AC power restored
      case 0xF0: {
        powerTrouble = false;
        if (powerTrouble != previousPower) {
          previousPower = powerTrouble;
          powerChanged = true;
          if (!pauseStatus) statusChanged = true;
        }
        return;
      }

	    default: if (partition == 0) return;
    }
  }

  // Processes partition-specific status
  if (partition > dscPartitions) return;  // Ensures that only the configured number of partitions are processed
  byte partitionIndex = partition - 1;

  // Disarmed
  if (panelData[panelByte] == 0x4A ||                                    // Disarmed after alarm in memory
      panelData[panelByte] == 0xE6 ||                                    // Disarmed special: keyswitch/wireless key/DLS
      (panelData[panelByte] >= 0xC0 && panelData[panelByte] <= 0xE4)) {  // Disarmed by access code

    noEntryDelay[partitionIndex] = false;
    processArmed(partitionIndex, false);
    processAlarmStatus(partitionIndex, false);
    processEntryDelayStatus(partitionIndex, false);
    return;
  }

  // Recent closing alarm
  if (panelData[panelByte] == 0x4B) {
    processAlarmStatus(partitionIndex, true);
    return;
  }

  // Zone alarm, zones 1-32
  // Zone alarm status is stored using 1 bit per zone in alarmZones[] and alarmZonesChanged[]:
  //   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  //   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
  //   ...
  //   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
  if (panelData[panelByte] >= 0x09 && panelData[panelByte] <= 0x28) {
    processAlarmStatus(partitionIndex, true);
    processEntryDelayStatus(partitionIndex, false);
    processAlarmZones(panelByte, 0, 0x09, 1);
    return;
  }

  // Zone alarm restored, zones 1-32
  // Zone alarm status is stored using 1 bit per zone in alarmZones[] and alarmZonesChanged[]:
  //   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  //   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
  //   ...
  //   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
  if (panelData[panelByte] >= 0x29 && panelData[panelByte] <= 0x48) {
    processAlarmZones(panelByte, 0, 0x29, 0);
    return;
  }

  // Armed by access codes 1-34, 40-42
  if (panelData[panelByte] >= 0x99 && panelData[panelByte] <= 0xBD) {
    byte dscCode = panelData[panelByte] - 0x98;
    processPanelAccessCode(partitionIndex, dscCode);
    return;
  }

  // Disarmed by access codes 1-34, 40-42
  if (panelData[panelByte] >= 0xC0 && panelData[panelByte] <= 0xE4) {
    byte dscCode = panelData[panelByte] - 0xBF;
    processPanelAccessCode(partitionIndex, dscCode);
    return;
  }
}


void dscKeybusInterface::processPanelStatus1(byte partition, byte panelByte) {
  if (partition == 0 || partition > dscPartitions) return;
  byte partitionIndex = partition - 1;

  switch (panelData[panelByte]) {

    // Armed with no entry delay
    case 0xD2: {
      processNoEntryDelayStatus(partitionIndex, false);
      return;
    }
  }
}


void dscKeybusInterface::processPanelStatus2(byte partition, byte panelByte) {
  if (partition == 0 || partition > dscPartitions) return;
  byte partitionIndex = partition - 1;

  // Armed: stay and Armed: away
  if (panelData[panelByte] == 0x9A || panelData[panelByte] == 0x9B) {
    if (panelData[panelByte] == 0x9A) {
      armedStay[partitionIndex] = true;
      armedAway[partitionIndex] = false;
    }
    else if (panelData[panelByte] == 0x9B) {
      armedStay[partitionIndex] = false;
      armedAway[partitionIndex] = true;
    }

    armed[partitionIndex] = true;
    if (armed[partitionIndex] != previousArmed[partitionIndex] || armedStay[partitionIndex] != previousArmedStay[partitionIndex]) {
      previousArmed[partitionIndex] = armed[partitionIndex];
      previousArmedStay[partitionIndex] = armedStay[partitionIndex];
      armedChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
    }

    processExitDelayStatus(partitionIndex, false);
    exitState[partitionIndex] = 0;
    processReadyStatus(partitionIndex, false);
    return;
  }

  if (panelData[0] == 0xA5) {
    switch (panelData[panelByte]) {

      // Activate stay/away zones
      case 0x99: {
        armed[partitionIndex] = true;
        armedAway[partitionIndex] = true;
        armedStay[partitionIndex] = false;
        armedChanged[partitionIndex] = true;
        if (!pauseStatus) statusChanged = true;
        return;
      }

      // Armed with no entry delay
      case 0x9C: {
        processNoEntryDelayStatus(partitionIndex, true);
        processReadyStatus(partitionIndex, false);
        return;
      }
    }
  }
}


void dscKeybusInterface::processPanelStatus4(byte partition, byte panelByte) {
  if (partition == 0 || partition > dscPartitions) return;
  byte partitionIndex = partition - 1;

  // Zone alarm, zones 33-64
  // Zone alarm status is stored using 1 bit per zone in alarmZones[] and alarmZonesChanged[]:
  //   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  //   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
  //   ...
  //   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
  if (panelData[panelByte] <= 0x1F) {
    processAlarmStatus(partitionIndex, true);
    processEntryDelayStatus(partitionIndex, false);
    processAlarmZones(panelByte, 4, 0, 1);
    return;
  }

  // Zone alarm restored, zones 33-64
  // Zone alarm status is stored using 1 bit per zone in alarmZones[] and alarmZonesChanged[]:
  //   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  //   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
  //   ...
  //   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
  if (panelData[panelByte] >= 0x20 && panelData[panelByte] <= 0x3F) {
    processAlarmZones(panelByte, 4, 0x20, 0);
    return;
  }
}


void dscKeybusInterface::processPanelStatus5(byte partition, byte panelByte) {
  if (partition == 0 || partition > dscPartitions) return;
  byte partitionIndex = partition - 1;

  /*
   *  Armed by access codes 35-95
   *  0x00 - 0x04: Access codes 35-39
   *  0x05 - 0x39: Access codes 43-95
   */
  if (panelData[panelByte] <= 0x39) {
    byte dscCode = panelData[panelByte] + 0x23;
    processPanelAccessCode(partitionIndex, dscCode, false);
    return;
  }

  /*
   *  Disarmed by access codes 35-95
   *  0x3A - 0x3E: Access codes 35-39
   *  0x3F - 0x73: Access codes 43-95
   */
  if (panelData[panelByte] >= 0x3A && panelData[panelByte] <= 0x73) {
    byte dscCode = panelData[panelByte] - 0x17;
    processPanelAccessCode(partitionIndex, dscCode, false);
    return;
  }
}


void dscKeybusInterface::processReadyStatus(byte partitionIndex, bool status) {
  ready[partitionIndex] = status;
  if (ready[partitionIndex] != previousReady[partitionIndex]) {
    previousReady[partitionIndex] = ready[partitionIndex];
    readyChanged[partitionIndex] = true;
    if (!pauseStatus) statusChanged = true;
  }
}


void dscKeybusInterface::processAlarmStatus(byte partitionIndex, bool status) {
  alarm[partitionIndex] = status;
  if (alarm[partitionIndex] != previousAlarm[partitionIndex]) {
    previousAlarm[partitionIndex] = alarm[partitionIndex];
    alarmChanged[partitionIndex] = true;
    if (!pauseStatus) statusChanged = true;
  }
}


void dscKeybusInterface::processExitDelayStatus(byte partitionIndex, bool status) {
  exitDelay[partitionIndex] = status;
  if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
    previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
    exitDelayChanged[partitionIndex] = true;
    if (!pauseStatus) statusChanged = true;
  }
}


void dscKeybusInterface::processEntryDelayStatus(byte partitionIndex, bool status) {
  entryDelay[partitionIndex] = status;
  if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
    previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
    entryDelayChanged[partitionIndex] = true;
    if (!pauseStatus) statusChanged = true;
  }
}


void dscKeybusInterface::processNoEntryDelayStatus(byte partitionIndex, bool status) {
  noEntryDelay[partitionIndex] = status;
  if (noEntryDelay[partitionIndex] != previousNoEntryDelay[partitionIndex]) {
    previousNoEntryDelay[partitionIndex] = noEntryDelay[partitionIndex];
    armedChanged[partitionIndex] = true;
    if (!pauseStatus) statusChanged = true;
  }
}


void dscKeybusInterface::processZoneStatus(byte zonesByte, byte panelByte) {
  openZones[zonesByte] = panelData[panelByte];
  byte zonesChanged = openZones[zonesByte] ^ previousOpenZones[zonesByte];
  if (zonesChanged != 0) {
    previousOpenZones[zonesByte] = openZones[zonesByte];
    openZonesStatusChanged = true;
    if (!pauseStatus) statusChanged = true;

    for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
      if (bitRead(zonesChanged, zoneBit)) {
        bitWrite(openZonesChanged[zonesByte], zoneBit, 1);
        if (bitRead(panelData[panelByte], zoneBit)) bitWrite(openZones[zonesByte], zoneBit, 1);
        else bitWrite(openZones[zonesByte], zoneBit, 0);
      }
    }
  }
}

void dscKeybusInterface::processTime(byte panelByte) {
  byte dscYear3 = panelData[panelByte] >> 4;
  byte dscYear4 = panelData[panelByte] & 0x0F;
  year = (dscYear3 * 10) + dscYear4;
  if (dscYear3 >= 7) year += 1900;
  else year += 2000;
  month = panelData[panelByte + 1] << 2; month >>=4;
  byte dscDay1 = panelData[panelByte + 1] << 6; dscDay1 >>= 3;
  byte dscDay2 = panelData[panelByte + 2] >> 5;
  day = dscDay1 | dscDay2;
  hour = panelData[panelByte + 2] & 0x1F;
  minute = panelData[panelByte + 3] >> 2;
}


void dscKeybusInterface::processAlarmZones(byte panelByte, byte startByte, byte zoneCountOffset, byte writeValue) {
  byte maxZones = dscZones * 8;
  if (maxZones > 32) {
    if (startByte < 4) maxZones = 32;
    else maxZones -= 32;
  }
  else if (startByte >= 4) return;

  for (byte zoneCount = 0; zoneCount < maxZones; zoneCount++) {
    if (panelData[panelByte] == zoneCount + zoneCountOffset) {
      for (byte zoneRange = 0; zoneRange <= 3; zoneRange++) {
        if (zoneCount >= (zoneRange * 8) && zoneCount < ((zoneRange * 8) + 8)) {
          processAlarmZonesStatus(startByte + zoneRange, zoneCount, writeValue);
        }
      }
    }
  }
}


void dscKeybusInterface::processAlarmZonesStatus(byte zonesByte, byte zoneCount, byte writeValue) {
  byte zonesRange = zonesByte;
  if (zonesRange >= 4) zonesRange -= 4;

  bitWrite(alarmZones[zonesByte], (zoneCount - (zonesRange * 8)), writeValue);

  if (bitRead(previousAlarmZones[zonesByte], (zoneCount - (zonesRange * 8))) != writeValue) {
    bitWrite(previousAlarmZones[zonesByte], (zoneCount - (zonesRange * 8)), writeValue);
    bitWrite(alarmZonesChanged[zonesByte], (zoneCount - (zonesRange * 8)), 1);

    alarmZonesStatusChanged = true;
    if (!pauseStatus) statusChanged = true;
  }
}


void dscKeybusInterface::processArmed(byte partitionIndex, bool armedStatus) {
  armedStay[partitionIndex] = armedStatus;
  armedAway[partitionIndex] = armedStatus;
  armed[partitionIndex] = armedStatus;

  if (armed[partitionIndex] != previousArmed[partitionIndex]) {
    previousArmed[partitionIndex] = armed[partitionIndex];
    armedChanged[partitionIndex] = true;
    if (!pauseStatus) statusChanged = true;
  }
}


void dscKeybusInterface::processPanelAccessCode(byte partitionIndex, byte dscCode, bool accessCodeIncrease) {
  if (accessCodeIncrease) {
    if (dscCode >= 35) dscCode += 5;
  }
  else {
    if (dscCode >= 40) dscCode += 3;
  }

  accessCode[partitionIndex] = dscCode;
  if (accessCode[partitionIndex] != previousAccessCode[partitionIndex]) {
    previousAccessCode[partitionIndex] = accessCode[partitionIndex];
    accessCodeChanged[partitionIndex] = true;
    if (!pauseStatus) statusChanged = true;
  }
}
