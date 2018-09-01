/*
    DSC Keybus Interface

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

#include "dscKeybusInterface.h"


// Processes 0x05 and 0x1B commands
void dscKeybusInterface::processPanelStatus() {

  // Trouble status
  if (bitRead(panelData[2],4)) trouble = true;
  else trouble = false;
  if (trouble != previousTrouble && (panelData[3] < 0x05 || panelData[3] == 0xC7)) {  // Ignores trouble light status in intermittent states
    previousTrouble = trouble;
    troubleChanged = true;
    statusChanged = true;
  }

  byte partitionStart = 0;
  byte partitionCount = 0;
  if (panelData[0] == 0x05) {
    partitionStart = 0;
    if (panelByteCount < 9) partitionCount = 2;
    else partitionCount = 4;
    if (dscPartitions < partitionCount) partitionCount = dscPartitions;
  }
  else if (dscPartitions > 4 && panelData[0] == 0x1B) {
    partitionStart = 4;
    partitionCount = 8;
  }

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

    // Status lights
    lights[partitionIndex] = panelData[statusByte];
    if (lights[partitionIndex] != previousLights[partitionIndex]) {
      previousLights[partitionIndex] = lights[partitionIndex];
      statusChanged = true;
    }

    // Status messages
    status[partitionIndex] = panelData[messageByte];
    if (status[partitionIndex] != previousStatus[partitionIndex]) {
      previousStatus[partitionIndex] = status[partitionIndex];
      statusChanged = true;
    }

    // Fire status
    if (bitRead(panelData[statusByte],6)) fire[partitionIndex] = true;
    else fire[partitionIndex] = false;
    if (fire[partitionIndex] != previousFire[partitionIndex] && panelData[messageByte] < 0x12) {  // Ignores fire light status in intermittent states
      previousFire[partitionIndex] = fire[partitionIndex];
      fireChanged[partitionIndex] = true;
      statusChanged = true;
    }


    // Messages
    switch (panelData[messageByte]) {
      case 0x01:         // Partition ready
      case 0x02: {       // Stay/away zones open
        ready[partitionIndex] = true;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          statusChanged = true;
        }
        break;
      }
      case 0x03: {       // Zones open
        exitDelay[partitionIndex] = false;
        if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
          previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
          exitDelayChanged[partitionIndex] = true;
          statusChanged = true;
        }
        entryDelay[partitionIndex] = false;
        if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
          previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
          entryDelayChanged[partitionIndex] = true;
          statusChanged = true;
        }
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          statusChanged = true;
        }
        break;
      }

      case 0x04:         // Armed stay
      case 0x05: {       // Armed away
        writeArm[partitionIndex] = false;
        exitDelay[partitionIndex] = false;
        if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
          previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
          exitDelayChanged[partitionIndex] = true;
          statusChanged = true;
        }
        entryDelay[partitionIndex] = false;
        if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
          previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
          entryDelayChanged[partitionIndex] = true;
          statusChanged = true;
        }
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          statusChanged = true;
        }
        break;
      }

      case 0x08: {       // Exit delay in progress
        writeArm[partitionIndex] = false;
        exitDelay[partitionIndex] = true;
        if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
          previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
          exitDelayChanged[partitionIndex] = true;
          statusChanged = true;
        }
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          statusChanged = true;
        }
        break;
      }

      case 0x0C: {       // Entry delay in progress
        entryDelay[partitionIndex] = true;
        if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
          previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
          entryDelayChanged[partitionIndex] = true;
          statusChanged = true;
        }
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          statusChanged = true;
        }
        break;
      }

      case 0x11: {       // Partition in alarm
        exitDelay[partitionIndex] = false;
        if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
          previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
          exitDelayChanged[partitionIndex] = true;
          statusChanged = true;
        }
        entryDelay[partitionIndex] = false;
        if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
          previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
          entryDelayChanged[partitionIndex] = true;
          statusChanged = true;
        }
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          statusChanged = true;
        }
        break;
      }

      case 0x3E: {       // Partition disarmed
        exitDelay[partitionIndex] = false;
        if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
          previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
          exitDelayChanged[partitionIndex] = true;
          statusChanged = true;
        }
        entryDelay[partitionIndex] = false;
        if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
          previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
          entryDelayChanged[partitionIndex] = true;
          statusChanged = true;
        }
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          statusChanged = true;
        }
        break;
      }

      case 0x9E: {       // Enter * function code
        wroteAsterisk = false;  // Resets the flag that delays writing after '*' is pressed
        writeAsterisk = false;
        writeReady = true;
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          statusChanged = true;
        }
        break;
      }

      case 0x9F: {
        if (writeArm[partitionIndex]) {  // Ensures access codes are only sent when an arm command is sent through this interface
          accessCodePrompt = true;
          statusChanged = true;
        }
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          statusChanged = true;
        }
        break;
      }

      default: {
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          statusChanged = true;
        }
        break;
      }
    }
  }
}


void dscKeybusInterface::processPanel_0x27() {
  if (!validCRC()) return;

  for (byte partitionIndex = 0; partitionIndex < 2; partitionIndex++) {
    byte messageByte = (partitionIndex * 2) + 3;

    // Messages
    if (panelData[messageByte] == 0x04 || panelData[messageByte] == 0x05) {
      if (panelData[messageByte] == 0x04) {
        armedStay[partitionIndex] = true;
        armedAway[partitionIndex] = false;
      }
      else if (panelData[messageByte] == 0x05) {
        armedStay[partitionIndex] = false;
        armedAway[partitionIndex] = true;
      }

      exitDelay[partitionIndex] = false;
      if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
        previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
        exitDelayChanged[partitionIndex] = true;
        statusChanged = true;
      }
      armed[partitionIndex] = true;
      if (previousArmed[partitionIndex] != true) {
        previousArmed[partitionIndex] = true;
        armedChanged[partitionIndex] = true;
        statusChanged = true;
      }
    }
  }

  // Open zones 1-8 status is stored in openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  openZones[0] = panelData[6];
  byte zonesChanged = openZones[0] ^ previousOpenZones[0];
  if (zonesChanged != 0) {
    previousOpenZones[0] = openZones[0];
    openZonesStatusChanged = true;
    statusChanged = true;

    for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
      if (bitRead(zonesChanged, zoneBit)) {
        bitWrite(openZonesChanged[0], zoneBit, 1);
        if (bitRead(panelData[6], zoneBit)) bitWrite(openZones[0], zoneBit, 1);
        else bitWrite(openZones[0], zoneBit, 0);
      }
    }
  }
}


void dscKeybusInterface::processPanel_0x2D() {
  if (!validCRC()) return;
  if (dscZones < 2) return;

  // Open zones 9-16 status is stored in openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
  openZones[1] = panelData[6];
  byte zonesChanged = openZones[1] ^ previousOpenZones[1];
  if (zonesChanged != 0) {
    previousOpenZones[1] = openZones[1];
    openZonesStatusChanged = true;
    statusChanged = true;

    for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
      if (bitRead(zonesChanged, zoneBit)) {
        bitWrite(openZonesChanged[1], zoneBit, 1);
        if (bitRead(panelData[6], zoneBit)) bitWrite(openZones[1], zoneBit, 1);
        else bitWrite(openZones[1], zoneBit, 0);
      }
    }
  }
}


void dscKeybusInterface::processPanel_0x34() {
  if (!validCRC()) return;
  if (dscZones < 3) return;

  // Open zones 17-24 status is stored in openZones[2] and openZonesChanged[2]: Bit 0 = Zone 17 ... Bit 7 = Zone 24
  openZones[2] = panelData[6];
  byte zonesChanged = openZones[2] ^ previousOpenZones[2];
  if (zonesChanged != 0) {
    previousOpenZones[2] = openZones[2];
    openZonesStatusChanged = true;
    statusChanged = true;

    for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
      if (bitRead(zonesChanged, zoneBit)) {
        bitWrite(openZonesChanged[2], zoneBit, 1);
        if (bitRead(panelData[6], zoneBit)) bitWrite(openZones[2], zoneBit, 1);
        else bitWrite(openZones[2], zoneBit, 0);
      }
    }
  }
}


void dscKeybusInterface::processPanel_0x3E() {
  if (!validCRC()) return;
  if (dscZones < 4) return;

  // Open zones 25-32 status is stored in openZones[3] and openZonesChanged[3]: Bit 0 = Zone 25 ... Bit 7 = Zone 32
  openZones[3] = panelData[6];
  byte zonesChanged = openZones[3] ^ previousOpenZones[3];
  if (zonesChanged != 0) {
    previousOpenZones[3] = openZones[3];
    openZonesStatusChanged = true;
    statusChanged = true;

    for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
      if (bitRead(zonesChanged, zoneBit)) {
        bitWrite(openZonesChanged[3], zoneBit, 1);
        if (bitRead(panelData[6], zoneBit)) bitWrite(openZones[3], zoneBit, 1);
        else bitWrite(openZones[3], zoneBit, 0);
      }
    }
  }
}


void dscKeybusInterface::processPanel_0xA5() {
  if (!validCRC()) return;

  byte dscYear3 = panelData[2] >> 4;
  byte dscYear4 = panelData[2] & 0x0F;
  year = (dscYear3 * 10) + dscYear4;
  month = panelData[3] << 2; month >>= 4;
  byte dscDay1 = panelData[3] << 6; dscDay1 >>= 3;
  byte dscDay2 = panelData[4] >> 5;
  day = dscDay1 | dscDay2;
  hour = panelData[4] & 0x1F;
  minute = panelData[5] >> 2;

  // Timestamp
  if (panelData[6] == 0 && panelData[7] == 0) {
    statusChanged = true;
    timeAvailable = true;
    return;
  }

  byte partition = panelData[3] >> 6;
  switch (panelData[5] & 0x03) {
    case 0x00: processPanelStatus0(partition, 6); break;
    case 0x02: processPanelStatus2(partition, 6); break;
  }
}


void dscKeybusInterface::processPanel_0xEB() {
  if (!validCRC()) return;
  if (dscPartitions < 3) return;

  byte dscYear3 = panelData[3] >> 4;
  byte dscYear4 = panelData[3] & 0x0F;
  year = (dscYear3 * 10) + dscYear4;
  month = panelData[4] << 2; month >>=4;
  byte dscDay1 = panelData[4] << 6; dscDay1 >>= 3;
  byte dscDay2 = panelData[5] >> 5;
  day = dscDay1 | dscDay2;
  hour = panelData[5] & 0x1F;
  minute = panelData[6] >> 2;

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
    case 0x02: processPanelStatus2(partition, 8); break;
    case 0x04: processPanelStatus4(partition, 8); break;
  }
}



void dscKeybusInterface::processPanelStatus0(byte partition, byte panelByte) {

  // Processes status messages that are not partition-specific
  if (partition == 0 && panelData[0] == 0xA5) {
    switch (panelData[panelByte]) {
      case 0x4E: {       // Keypad Fire alarm
        keypadFireAlarm = true;
        statusChanged = true;
        return;
      }
      case 0x4F: {       // Keypad Aux alarm
        keypadAuxAlarm = true;
        statusChanged = true;
        return;
      }
      case 0x50: {       // Keypad Panic alarm
        keypadPanicAlarm = true;
        statusChanged =true;
        return;
      }
      case 0xE7: {       // Panel battery trouble
        batteryTrouble = true;
        batteryChanged = true;
        statusChanged = true;
        return;
      }
      case 0xE8: {       // Panel AC power failure
        powerTrouble = true;
        powerChanged = true;
        statusChanged = true;
        return;
      }
      case 0xEF: {       // Panel battery restored
        batteryTrouble = false;
        batteryChanged = true;
        statusChanged = true;
        return;
      }
      case 0xF0: {       // Panel AC power restored
        powerTrouble = false;
        powerChanged = true;
        statusChanged = true;
        return;
      }
      default: return;
    }
  }

  // Processes partition-specific status
  if (partition > dscPartitions) return;  // Ensures that only the configured number of partitions are processed
  byte partitionIndex = partition - 1;

  if (panelData[panelByte] == 0x4A ||                                    // Disarmed after alarm in memory
      panelData[panelByte] == 0xE6 ||                                    // Disarmed special: keyswitch/wireless key/DLS
      (panelData[panelByte] >= 0xC0 && panelData[panelByte] <= 0xE4)) {  // Disarmed by access code

    armed[partitionIndex] = false;
    armedAway[partitionIndex] = false;
    armedStay[partitionIndex] = false;
    noEntryDelay[partitionIndex] = false;
    alarm[partitionIndex] = false;

    if (previousAlarm[partitionIndex] != false) {
      previousAlarm[partitionIndex] = false;
      alarmChanged[partitionIndex] = true;
      statusChanged = true;
    }

    if (previousArmed[partitionIndex] != false) {
      previousArmed[partitionIndex] = false;
      armedChanged[partitionIndex] = true;
      statusChanged = true;
    }
    return;
  }

  if (panelData[panelByte] == 0x4B) {  // Partition in alarm
    alarm[partitionIndex] = true;
    if (previousAlarm[partitionIndex] != true) {
      previousAlarm[partitionIndex] = true;
      alarmChanged[partitionIndex] = true;
      statusChanged = true;
    }
    return;
  }

  // Zone alarm, zones 1-32
  // Zone alarm status is stored using 1 bit per zone in alarmZones[] and alarmZonesChanged[]:
  //   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  //   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
  //   ...
  //   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
  if (panelData[panelByte] >= 0x09 && panelData[panelByte] <= 0x28) {
    alarm[partitionIndex] = true;
    if (previousAlarm[partitionIndex] != true) {
      previousAlarm[partitionIndex] = true;
      alarmChanged[partitionIndex] = true;
      statusChanged = true;
    }

    byte maxZones = dscZones * 8;
    if (maxZones > 32) maxZones = 32;
    for (byte zoneCount = 0; zoneCount < maxZones; zoneCount++) {
      if (panelData[panelByte] == 0x09 + zoneCount) {
        if (zoneCount < 8) {
          bitWrite(alarmZones[0], zoneCount, 1);
          if (bitRead(previousAlarmZones[0], zoneCount) != 1) {
            bitWrite(previousAlarmZones[0], zoneCount, 1);
            bitWrite(alarmZonesChanged[0], zoneCount, 1);
            alarmZonesStatusChanged = true;
            statusChanged = true;
          }
        }
        else if (zoneCount >= 8 && zoneCount < 16) {
          bitWrite(alarmZones[1], (zoneCount - 8), 1);
          if (bitRead(previousAlarmZones[1], (zoneCount - 8)) != 1) {
            bitWrite(previousAlarmZones[1], (zoneCount - 8), 1);
            bitWrite(alarmZonesChanged[1], (zoneCount - 8), 1);
            alarmZonesStatusChanged = true;
            statusChanged = true;
          }
        }
        else if (zoneCount >= 16 && zoneCount < 24) {
          bitWrite(alarmZones[2], (zoneCount - 16), 1);
          if (bitRead(previousAlarmZones[2], (zoneCount - 16)) != 1) {
            bitWrite(previousAlarmZones[2], (zoneCount - 16), 1);
            bitWrite(alarmZonesChanged[2], (zoneCount - 16), 1);
            alarmZonesStatusChanged = true;
            statusChanged = true;
          }
        }
        else if (zoneCount >= 24 && zoneCount < 32) {
          bitWrite(alarmZones[3], (zoneCount - 24), 1);
          if (bitRead(previousAlarmZones[3], (zoneCount - 24)) != 1) {
            bitWrite(previousAlarmZones[3], (zoneCount - 24), 1);
            bitWrite(alarmZonesChanged[3], (zoneCount - 24), 1);
            alarmZonesStatusChanged = true;
            statusChanged = true;
          }
        }
      }
    }
    return;
  }

  // Zone alarm restored, zones 1-32
  // Zone alarm status is stored using 1 bit per zone in alarmZones[] and alarmZonesChanged[]:
  //   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  //   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
  //   ...
  //   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
  if (panelData[panelByte] >= 0x29 && panelData[panelByte] <= 0x48) {

    byte maxZones = dscZones * 8;
    if (maxZones > 32) maxZones = 32;
    for (byte zoneCount = 0; zoneCount < maxZones; zoneCount++) {
      if (panelData[panelByte] == 0x29 + zoneCount) {
        if (zoneCount < 8) {
          bitWrite(alarmZones[0], zoneCount, 0);
          if (bitRead(previousAlarmZones[0], zoneCount) != 0) {
            bitWrite(previousAlarmZones[0], zoneCount, 0);
            bitWrite(alarmZonesChanged[0], zoneCount, 1);
            alarmZonesStatusChanged = true;
            statusChanged = true;
          }
        }
        else if (zoneCount >= 8 && zoneCount < 16) {
          bitWrite(alarmZones[1], (zoneCount - 8), 0);
          if (bitRead(previousAlarmZones[1], (zoneCount - 8)) != 0) {
            bitWrite(previousAlarmZones[1], (zoneCount - 8), 0);
            bitWrite(alarmZonesChanged[1], (zoneCount - 8), 1);
            alarmZonesStatusChanged = true;
            statusChanged = true;
          }
        }
        else if (zoneCount >= 16 && zoneCount < 24) {
          bitWrite(alarmZones[2], (zoneCount - 16), 0);
          if (bitRead(previousAlarmZones[2], (zoneCount - 16)) != 0) {
            bitWrite(previousAlarmZones[2], (zoneCount - 16), 0);
            bitWrite(alarmZonesChanged[2], (zoneCount - 16), 1);
            alarmZonesStatusChanged = true;
            statusChanged = true;
          }
        }
        else if (zoneCount >= 24 && zoneCount < 32) {
          bitWrite(alarmZones[3], (zoneCount - 24), 0);
          if (bitRead(previousAlarmZones[3], (zoneCount - 24)) != 0) {
            bitWrite(previousAlarmZones[3], (zoneCount - 24), 0);
            bitWrite(alarmZonesChanged[3], (zoneCount - 24), 1);
            alarmZonesStatusChanged = true;
            statusChanged = true;
          }
        }
      }
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
    exitDelay[partitionIndex] = false;
    if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
      previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
      exitDelayChanged[partitionIndex] = true;
      statusChanged = true;
    }
    if (previousArmed[partitionIndex] != true) {
      previousArmed[partitionIndex] = true;
      armedChanged[partitionIndex] = true;
      statusChanged = true;
    }
    return;
  }

  if (panelData[panelByte] == 0xA5) {
    switch (panelData[panelByte]) {
      case 0x99: {        // Activate stay/away zones
        armed[partitionIndex] = true;
        armedAway[partitionIndex] = true;
        armedStay[partitionIndex] = false;
        armedChanged[partitionIndex] = true;
        statusChanged = true;
        return;
      }
      case 0x9C: {        // Armed without entry delay
        noEntryDelay[partitionIndex] = true;
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
    alarmZonesStatusChanged = true;
    alarm[partitionIndex] = true;
    if (previousAlarm[partitionIndex] != true) {
      previousAlarm[partitionIndex] = true;
      alarmChanged[partitionIndex] = true;
      statusChanged = true;
    }

    byte maxZones = dscZones * 8;
    if (maxZones > 32) maxZones -= 32;
    else return;
    for (byte zoneCount = 0; zoneCount < maxZones; zoneCount++) {
      if (panelData[panelByte] == zoneCount) {
        if (zoneCount < 8) {
          bitWrite(alarmZones[4], zoneCount, 1);
          bitWrite(alarmZonesChanged[4], zoneCount, 1);
          statusChanged = true;
        }
        else if (zoneCount >= 8 && zoneCount < 16) {
          bitWrite(alarmZones[5], (zoneCount - 8), 1);
          bitWrite(alarmZonesChanged[5], (zoneCount - 8), 1);
          statusChanged = true;
        }
        else if (zoneCount >= 16 && zoneCount < 24) {
          bitWrite(alarmZones[6], (zoneCount - 16), 1);
          bitWrite(alarmZonesChanged[6], (zoneCount - 16), 1);
          statusChanged = true;
        }
        else if (zoneCount >= 24 && zoneCount < 32) {
          bitWrite(alarmZones[7], (zoneCount - 24), 1);
          bitWrite(alarmZonesChanged[7], (zoneCount - 24), 1);
          statusChanged = true;
        }
      }
    }
    return;
  }

  // Zone alarm restored, zones 33-64
  // Zone alarm status is stored using 1 bit per zone in alarmZones[] and alarmZonesChanged[]:
  //   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  //   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
  //   ...
  //   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
  if (panelData[panelByte] >= 0x20 && panelData[panelByte] <= 0x3F) {
    alarmZonesStatusChanged = true;

    byte maxZones = dscZones * 8;
    if (maxZones > 32) maxZones -= 32;
    else return;
    for (byte zoneCount = 0; zoneCount < maxZones; zoneCount++) {
      if (panelData[panelByte] == 0x20 + zoneCount) {
        if (zoneCount < 8) {
          bitWrite(alarmZones[4], zoneCount, 0);
          bitWrite(alarmZonesChanged[4], zoneCount, 1);
          statusChanged = true;
        }
        else if (zoneCount >= 8 && zoneCount < 16) {
          bitWrite(alarmZones[5], (zoneCount - 8), 0);
          bitWrite(alarmZonesChanged[5], (zoneCount - 8), 1);
          statusChanged = true;
        }
        else if (zoneCount >= 16 && zoneCount < 24) {
          bitWrite(alarmZones[6], (zoneCount - 16), 0);
          bitWrite(alarmZonesChanged[6], (zoneCount - 16), 1);
          statusChanged = true;
        }
        else if (zoneCount >= 24 && zoneCount < 32) {
          bitWrite(alarmZones[7], (zoneCount - 24), 0);
          bitWrite(alarmZonesChanged[7], (zoneCount - 24), 1);
          statusChanged = true;
        }
      }
    }
    return;
  }

}


// Processes zones 33-64 status
void dscKeybusInterface::processPanel_0xE6() {
  if (!validCRC()) return;

  switch (panelData[2]) {
    case 0x09: processPanel_0xE6_0x09(); return;
    case 0x0B: processPanel_0xE6_0x0B(); return;
    case 0x0D: processPanel_0xE6_0x0D(); return;
    case 0x0F: processPanel_0xE6_0x0F(); return;
  }
}


// Open zones 33-40 status is stored in openZones[4] and openZonesChanged[4]: Bit 0 = Zone 33 ... Bit 7 = Zone 40
void dscKeybusInterface::processPanel_0xE6_0x09() {
  if (dscZones > 4) {
    openZones[4] = panelData[3];
    byte zonesChanged = openZones[4] ^ previousOpenZones[4];
    if (zonesChanged != 0) {
      previousOpenZones[4] = openZones[4];
      openZonesStatusChanged = true;
      statusChanged = true;

      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(zonesChanged, zoneBit)) {
          bitWrite(openZonesChanged[4], zoneBit, 1);
          if (bitRead(panelData[3], zoneBit)) bitWrite(openZones[4], zoneBit, 1);
          else bitWrite(openZones[4], zoneBit, 0);
        }
      }
    }
  }
}


// Open zones 41-48 status is stored in openZones[5] and openZonesChanged[5]: Bit 0 = Zone 41 ... Bit 7 = Zone 48
void dscKeybusInterface::processPanel_0xE6_0x0B() {
  if (dscZones > 5) {
    openZones[5] = panelData[3];
    byte zonesChanged = openZones[5] ^ previousOpenZones[5];
    if (zonesChanged != 0) {
      previousOpenZones[5] = openZones[5];
      openZonesStatusChanged = true;
      statusChanged = true;

      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(zonesChanged, zoneBit)) {
          bitWrite(openZonesChanged[5], zoneBit, 1);
          if (bitRead(panelData[3], zoneBit)) bitWrite(openZones[5], zoneBit, 1);
          else bitWrite(openZones[5], zoneBit, 0);
        }
      }
    }
  }
}


// Open zones 49-56 status is stored in openZones[6] and openZonesChanged[6]: Bit 0 = Zone 49 ... Bit 7 = Zone 56
void dscKeybusInterface::processPanel_0xE6_0x0D() {
  if (dscZones > 6) {
    openZones[6] = panelData[3];
    byte zonesChanged = openZones[6] ^ previousOpenZones[6];
    if (zonesChanged != 0) {
      previousOpenZones[6] = openZones[6];
      openZonesStatusChanged = true;
      statusChanged = true;

      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(zonesChanged, zoneBit)) {
          bitWrite(openZonesChanged[6], zoneBit, 1);
          if (bitRead(panelData[3], zoneBit)) bitWrite(openZones[6], zoneBit, 1);
          else bitWrite(openZones[6], zoneBit, 0);
        }
      }
    }
  }
}


// Open zones 57-64 status is stored in openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
void dscKeybusInterface::processPanel_0xE6_0x0F() {
  if (dscZones > 7) {
    openZones[7] = panelData[3];
    byte zonesChanged = openZones[7] ^ previousOpenZones[7];
    if (zonesChanged != 0) {
      previousOpenZones[7] = openZones[7];
      openZonesStatusChanged = true;
      statusChanged = true;

      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(zonesChanged, zoneBit)) {
          bitWrite(openZonesChanged[7], zoneBit, 1);
          if (bitRead(panelData[3], zoneBit)) bitWrite(openZones[7], zoneBit, 1);
          else bitWrite(openZones[7], zoneBit, 0);
        }
      }
    }
  }
}
