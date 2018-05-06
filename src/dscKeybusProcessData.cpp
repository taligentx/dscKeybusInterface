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


void dscKeybusInterface::processPanel_0x05() {

  // Status lights
  if (panelData[2] != 0) {
    if (bitRead(panelData[2],4)) {    // Trouble
      troubleStatus = true;
      if (troubleStatus != previousTroubleStatus && panelData[3] < 0x05) {  // Ignores trouble light status in intermittent states
        previousTroubleStatus = troubleStatus;
        troubleStatusChanged = true;
        statusChanged = true;
      }
    }
    else {
      troubleStatus = false;
      if (troubleStatus != previousTroubleStatus && panelData[3] < 0x05) {  // Ignores trouble light status in intermittent states
        previousTroubleStatus = troubleStatus;
        troubleStatusChanged = true;
        statusChanged = true;
      }
    }
  }

  // Messages
  switch (panelData[3]) {
    case 0x01: {       // Partition ready
      exitDelay = false;
      previousExitDelay = false;
      entryDelay = false;
      previousEntryDelay = false;
      break;
    }
    case 0x03: {       // Partition not ready
      exitDelay = false;
      previousExitDelay = false;
      entryDelay = false;
      previousEntryDelay = false;
      break;
    }
    case 0x04: {       // Armed stay
      exitDelay = false;
      previousExitDelay = false;
      entryDelay = false;
      previousEntryDelay = false;
      break;
    }
    case 0x05: {       // Armed away
      exitDelay = false;
      previousExitDelay = false;
      entryDelay = false;
      previousEntryDelay = false;
      break;
    }
    case 0x08: {       // Exit delay in progress
      exitDelay = true;
      if (exitDelay != previousExitDelay) {
        previousExitDelay = exitDelay;
        exitDelayChanged = true;
        statusChanged = true;
      }
      break;
    }
    case 0x0C: {       // Entry delay in progress
      entryDelay = true;
      if (entryDelay != previousEntryDelay) {
        previousEntryDelay = entryDelay;
        entryDelayChanged = true;
        statusChanged = true;
      }
      break;
    }
    case 0x11: {       // Partition in alarm
      entryDelay = false;
      previousEntryDelay = false;
      entryDelay = false;
      previousEntryDelay = false;
      break;
    }
    case 0x3E: {       // Partition disarmed
      exitDelay = false;
      previousExitDelay = false;
      entryDelay = false;
      previousEntryDelay = false;
      break;
    }
    case 0x9E: {       // '*' pressed
      wroteAsterisk = false;  // Resets the flag that delays writing after '*' is pressed
      writeAsterisk = false;
      writeReady = true;
      break;
    }
  }
}


void dscKeybusInterface::processPanel_0x27() {
  if (!validCRC()) return;

  // Messages
  switch (panelData[3]) {
    case 0x04: {       // Armed stay
      partitionArmed = true;
      partitionArmedStay = true;
      partitionArmedAway = false;
      exitDelay = false;
      if (partitionArmed != previousPartitionArmed) {
        previousPartitionArmed = partitionArmed;
        partitionArmedChanged = true;
        statusChanged = true;
      }
      break;
    }
    case 0x05: {       // Armed away
      partitionArmed = true;
      partitionArmedStay = false;
      partitionArmedAway = true;
      exitDelay = false;
      if (partitionArmed != previousPartitionArmed) {
        previousPartitionArmed = partitionArmed;
        partitionArmedChanged = true;
        statusChanged = true;
      }
      break;
    }
  }

  // Open zones 1-8 status is stored in openZones[0] and openZonesChanged[0]
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

  // Open zones 9-16 status is stored in openZones[1] and openZonesChanged[1]
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

  // Open zones 17-24 status is stored in openZones[2] and openZonesChanged[2]
  openZones[1] = panelData[6];
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

  // Open zones 25-32 status is stored in openZones[3] and openZonesChanged[3]
  openZones[1] = panelData[6];
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
  year = 2000 + (dscYear3 * 10) + dscYear4;
  month = panelData[3] << 2; month >>=4;
  byte dscDay1 = panelData[3] << 6; dscDay1 >>= 3;
  byte dscDay2 = panelData[4] >> 5;
  day = dscDay1 | dscDay2;
  hour = panelData[4] & 0x1F;
  minute = panelData[5] >> 2;

  dscTime[0] = '0' + month / 10;
  dscTime[1] = '0' + month % 10;
  dscTime[2] = '/';
  dscTime[3] = '0' + day / 10;
  dscTime[4] = '0' + day % 10;
  dscTime[5] = '/';
  dscTime[6] = '0' + year / 1000;
  dscTime[7] = '0' + ((year / 100) % 10);
  dscTime[8] = '0' + ((year / 10) % 10);
  dscTime[9] = '0' + year % 10;
  dscTime[10] = ' ';
  dscTime[11] = '0' + hour / 10;
  dscTime[12] = '0' + hour % 10;
  dscTime[13] = ':';
  dscTime[14] = '0' + minute / 10;
  dscTime[15] = '0' + minute % 10;
  dscTime[16] = '\0';

  // Timestamp
  if (panelData[6] == 0 && panelData[7] == 0) {
    statusChanged = true;
    timeAvailable = true;
    return;
  }

  if (panelData[7] == 0xFF) processPanel_0xA5_Byte7_0xFF();
  if (panelData[7] == 0) processPanel_0xA5_Byte7_0x00();
}


void dscKeybusInterface::processPanel_0xA5_Byte7_0xFF() {
  bool decodeComplete = true;

  // Process data based on byte 5 bits 0 and 1
  switch (panelData[5] & 0x03) {

    // Byte 5: xxxxxx00
    case 0x00: {
      switch (panelData[6]) {
          case 0x4A: {       // Disarmed after alarm in memory
            partitionAlarm = false;
            partitionAlarmChanged = true;
            statusChanged = true;
            break;
          }
          case 0x4B: {       // Partition in alarm
            partitionAlarm = true;
            partitionAlarmChanged = true;
            statusChanged = true;
            break;
          }
          case 0x4E: {       // Keypad Fire alarm
            keypadFireAlarm = true;
            statusChanged = true;
            break;
          }
          case 0x4F: {       // Keypad Aux alarm
            keypadAuxAlarm = true;
            statusChanged = true;
            break;
          }
          case 0x50: {       // Keypad Panic alarm
            keypadPanicAlarm = true;
            statusChanged =true;
            break;
          }
          case 0xE6: {       // Disarmed special: keyswitch/wireless key/DLS
            partitionArmed = false;
            partitionArmedAway = false;
            partitionArmedStay = false;
            previousPartitionArmed = false;
            armedNoEntryDelay = false;
            partitionArmedChanged = true;
            statusChanged = true;
            break;
          }
          case 0xE7: {       // Panel battery trouble
            batteryTrouble = true;
            batteryTroubleChanged = true;
            statusChanged = true;
            break;
          }
          case 0xE8: {       // Panel AC power failure
            powerTrouble = true;
            powerTroubleChanged = true;
            statusChanged = true;
            break;
          }
          case 0xEF: {       // Panel battery restored
            batteryTrouble = false;
            batteryTroubleChanged = true;
            statusChanged = true;
            break;
          }
          case 0xF0: {       // Panel AC power restored
            powerTrouble = false;
            powerTroubleChanged = true;
            statusChanged = true;
            break;
          }
          default: decodeComplete = false;
      }

      if (decodeComplete) break;

      // Zone alarm, zones 1-32: panelData[6] 0x09-0x28
      // Zone alarm status is stored using 1 bit per zone in alarmZones[] and alarmZonesChanged[]
      if (panelData[6] >= 0x09 && panelData[6] <= 0x28) {
        alarmZonesStatusChanged = true;
        for (byte zoneCount = 0; zoneCount < 32; zoneCount++) {
          if (panelData[6] == 0x09 + zoneCount) {
            if (zoneCount < 8) {
              bitWrite(alarmZones[0], zoneCount, 1);
              bitWrite(alarmZonesChanged[0], zoneCount, 1);
              statusChanged = true;
            }
            else if (zoneCount >= 8 && zoneCount < 16) {
              bitWrite(alarmZones[1], (zoneCount - 8), 1);
              bitWrite(alarmZonesChanged[1], (zoneCount - 8), 1);
              statusChanged = true;
            }
            else if (zoneCount >= 16 && zoneCount < 24) {
              bitWrite(alarmZones[2], (zoneCount - 16), 1);
              bitWrite(alarmZonesChanged[2], (zoneCount - 16), 1);
              statusChanged = true;
            }
            else if (zoneCount >= 24 && zoneCount < 32) {
              bitWrite(alarmZones[3], (zoneCount - 24), 1);
              bitWrite(alarmZonesChanged[3], (zoneCount - 24), 1);
              statusChanged = true;
            }
          }
        }

        decodeComplete = true;
        break;
      }

      // Zone alarm restored, zones 1-32
      // Zone alarm status is stored using 1 bit per zone in alarmZones[] and alarmZonesChanged[]
      if (panelData[6] >= 0x29 && panelData[6] <= 0x48) {
        alarmZonesStatusChanged = true;
        for (byte zoneCount = 0; zoneCount < 32; zoneCount++) {
          if (panelData[6] == 0x29 + zoneCount) {
            if (zoneCount < 8) {
              bitWrite(alarmZones[0], zoneCount, 0);
              bitWrite(alarmZonesChanged[0], zoneCount, 1);
              statusChanged = true;
            }
            else if (zoneCount >= 8 && zoneCount < 16) {
              bitWrite(alarmZones[1], (zoneCount - 8), 0);
              bitWrite(alarmZonesChanged[1], (zoneCount - 8), 1);
              statusChanged = true;
            }
            else if (zoneCount >= 16 && zoneCount < 24) {
              bitWrite(alarmZones[2], (zoneCount - 16), 0);
              bitWrite(alarmZonesChanged[2], (zoneCount - 16), 1);
              statusChanged = true;
            }
            else if (zoneCount >= 24 && zoneCount < 32) {
              bitWrite(alarmZones[3], (zoneCount - 24), 0);
              bitWrite(alarmZonesChanged[3], (zoneCount - 24), 1);
              statusChanged = true;
            }
          }
        }
        decodeComplete = true;
        break;
      }

      // Disarmed by access code
      if (panelData[6] >= 0xC0 && panelData[6] <= 0xE4) {
        partitionArmed = false;
        partitionArmedAway = false;
        partitionArmedStay = false;
        previousPartitionArmed = false;
        armedNoEntryDelay = false;
        partitionArmedChanged = true;
        statusChanged = true;
        decodeComplete = true;
        break;
      }

      decodeComplete = false;
      break;
    }
  }
  if (decodeComplete) return;
}


void dscKeybusInterface::processPanel_0xA5_Byte7_0x00() {
  bool decodeComplete = true;

  // Process data based on byte 5, bits 0 and 1
  switch (panelData[5] & 0x03) {

    // Byte 5: xxxxxx10
    case 0x02: {
      switch (panelData[6]) {
        case 0x99: {       // Activate stay/away zones
          partitionArmed = true;
          partitionArmedStay = false;
          partitionArmedAway = true;
          partitionArmedChanged = true;
          statusChanged = true;
          break;
        }
        case 0x9A: break;  // Armed: stay
        case 0x9B: break;  // Armed: away
        case 0x9C: {       // Armed without entry delay
          armedNoEntryDelay = true;
          break;
        }
        default: decodeComplete = false;
      }
      break;
    }
  }
  if (decodeComplete) return;
}
