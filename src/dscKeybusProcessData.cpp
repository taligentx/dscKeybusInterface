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
    if (bitRead(panelData[2],0)) {};  // Ready
    if (bitRead(panelData[2],1)) {};  // Armed
    if (bitRead(panelData[2],2)) {};  // Memory
    if (bitRead(panelData[2],3)) {};  // Bypass
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
    if (bitRead(panelData[2],5)) {};  // Program
    if (bitRead(panelData[2],6)) {};  // Fire
    if (bitRead(panelData[2],7)) {};  // Backlight
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
    case 0x09: break;  // Enter code to arm without entry delay
    case 0x0B: break;  // Quick exit in progress
    case 0x0C: {       // Entry delay in progress
      entryDelay = true;
      if (entryDelay != previousEntryDelay) {
        previousEntryDelay = entryDelay;
        entryDelayChanged = true;
        statusChanged = true;
      }
      break;
    }
    case 0x10: break;  // Keypad lockout
    case 0x11: {       // Partition in alarm
      entryDelay = false;
      previousEntryDelay = false;
      entryDelay = false;
      previousEntryDelay = false;
      break;
    }
    case 0x14: break;  // Auto-arm in progress
    case 0x16: break;  // Armed without entry delay
    case 0x33: break;  // Partition busy
    case 0x3D: break;  // Disarmed after alarm in memory
    case 0x3E: {       // Partition disarmed
      exitDelay = false;
      previousExitDelay = false;
      entryDelay = false;
      previousEntryDelay = false;
      break;
    }
    case 0x40: break;  // Keypad blanked
    case 0x8A: break;  // Activate stay/away zones
    case 0x8B: break;  // Quick exit
    case 0x8E: break;  // Invalid option
    case 0x8F: break;  // Invalid access code
    case 0x9E: {       // '*' pressed
      wroteAsterisk = false;  // Resets the flag that delays writing after '*' is pressed
      writeAsterisk = false;
      writeReady = true;
      break;
    }
    case 0x9F: break;  // Command output 1
    case 0xA1: break;  // *2: Trouble menu
    case 0xA2: break;  // *3: Alarm memory display
    case 0xA3: break;  // Door chime enabled
    case 0xA4: break;  // Door chime disabled
    case 0xA5: break;  // Enter master code
    case 0xA6: break;  // *5: Access codes
    case 0xA7: break;  // *5: Enter new code
    case 0xA9: break;  // *6: User functions
    case 0xAA: break;  // *6: Time and Date
    case 0xAB: break;  // *6: Auto-arm time
    case 0xAC: break;  // *6: Auto-arm enabled
    case 0xAD: break;  // *6: Auto-arm disabled
    case 0xAF: break;  // *6: System test
    case 0xB0: break;  // *6: Enable DLS
    case 0xB2: break;  // *7: Command output
    case 0xB7: break;  // Enter installer code
    case 0xB8: break;  // * pressed while armed
    case 0xB9: break;  // *2: Zone tamper menu
    case 0xBA: break;  // *2: Zones with low batteries
    case 0xC6: break;  // *2: Zone fault menu
    case 0xC8: break;  // *2: Service required menu
    case 0xD0: break;  // *2: Handheld keypads with low batteries
    case 0xD1: break;  // *2: Wireless keys with low batteries
    case 0xE4: break;  // *8: Main menu
    case 0xEE: break;  // *8: Submenu
    default:  break;
  }
}


void dscKeybusInterface::processPanel_0x27() {
  if (!validCRC()) return;

  // Status lights
  if (panelData[2] != 0) {
    if (bitRead(panelData[2],0)) {};  // Ready
    if (bitRead(panelData[2],1)) {};  // Armed
    if (bitRead(panelData[2],2)) {};  // Memory
    if (bitRead(panelData[2],3)) {};  // Bypass
    if (bitRead(panelData[2],4)) {};  // Trouble
    if (bitRead(panelData[2],5)) {};  // Program
    if (bitRead(panelData[2],6)) {};  // Fire
    if (bitRead(panelData[2],7)) {};  // Backlight
  }

  // Messages
  switch (panelData[3]) {
    case 0x01: break;  // Partition ready
    case 0x03: break;  // Partition not ready
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
    case 0x08: break;  // Exit delay in progress
    case 0x09: break;  // Enter code to arm without entry delay
    case 0x0B: break;  // Quick exit in progress
    case 0x0C: break;  // Entry delay in progress
    case 0x10: break;  // Keypad lockout
    case 0x11: break;  // Partition in alarm
    case 0x14: break;  // Auto-arm in progress
    case 0x16: break;  // Armed without entry delay
    case 0x33: break;  // Partition busy
    case 0x3D: break;  // Disarmed after alarm in memory
    case 0x3E: break;  // Partition disarmed
    case 0x40: break;  // Keypad blanked
    case 0x8A: break;  // Activate stay/away zones
    case 0x8B: break;  // Quick exit
    case 0x8E: break;  // Invalid option
    case 0x8F: break;  // Invalid access code
  }

  // Open zones
  openZonesGroup1 = panelData[6];
  byte zonesChanged = openZonesGroup1 ^ previousOpenZonesGroup1;
  if (zonesChanged != 0) {
    previousOpenZonesGroup1 = openZonesGroup1;
    openZonesGroup1Changed = true;
    statusChanged = true;

    for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
      if (bitRead(zonesChanged, zoneBit)) {
        openZonesChanged[zoneBit] = true;
        if (bitRead(panelData[6], zoneBit)) openZones[zoneBit] = true;
        else openZones[zoneBit] = false;
      }
      else openZonesChanged[zoneBit] = false;
    }
  }
}


void dscKeybusInterface::processPanel_0x87() {
  if (!validCRC()) return;

  // Panel output:
  switch (panelData[2]) {
    case 0x00: break;  // Bell off
    case 0xFF: break;  // Bell on
    default: break;
  }

  if ((panelData[3] & 0x0F) <= 0x03) {
    if (bitRead(panelData[3],0)) {};  // PGM1 on
    if (bitRead(panelData[3],1)) {};  // PGM2 on
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
          case 0x49: break;  // Duress alarm
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
          case 0x4C: break;  // Zone expander supervisory alarm
          case 0x4D: break;  // Zone expander supervisory restored
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
          case 0x51: break;  // Keypad status check?
          case 0x52: break;  // Keypad Fire alarm restored
          case 0x53: break;  // Keypad Aux alarm restored
          case 0x54: break;  // Keypad Panic alarm restored
          case 0x98: break;  // Keypad lockout
          case 0xBE: break;  // Armed partial: Zones bypassed
          case 0xBF: break;  // Armed special: quick-arm/auto-arm/keyswitch/wireless key/DLS
          case 0xE5: break;  // Auto-arm cancelled
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
          case 0xE9: break;  // Bell trouble
          case 0xEA: break;  // Power on +16s
          case 0xEC: break;  // Telephone line trouble
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
          case 0xF1: break;  // Bell restored
          case 0xF4: break;  // Telephone line restored
          case 0xFF: break;  // System test
          default: decodeComplete = false;
      }

      if (decodeComplete) break;

      // Zone alarm, zones 1-32: panelData[6] 0x09-0x28
      if (panelData[6] >= 0x09 && panelData[6] <= 0x28) {
        alarmZonesGroup1Changed = true;
        byte maxZones = dscZones;
        if (maxZones > 32) maxZones = 32;
        for (byte zoneCount = 0; zoneCount < maxZones; zoneCount++) {
          if (panelData[6] == 0x09 + zoneCount) {
            alarmZones[zoneCount] = true;
            alarmZonesChanged[zoneCount] = true;
            statusChanged = true;
          }
        }

        decodeComplete = true;
        break;
      }

      // Zone alarm restored, zones 1-32
      if (panelData[6] >= 0x29 && panelData[6] <= 0x48) {
        alarmZonesGroup1Changed = true;
        byte maxZones = dscZones;
        if (maxZones > 32) maxZones = 32;
        for (byte zoneCount = 0; zoneCount < maxZones; zoneCount++) {
          if (panelData[6] == 0x29 + zoneCount) {
            alarmZones[zoneCount] = false;
            alarmZonesChanged[zoneCount] = true;
            statusChanged = true;
          }
        }
        decodeComplete = true;
        break;
      }

      // Zone tamper, zones 1-32
      if (panelData[6] >= 0x56 && panelData[6] <= 0x75) {
        // Zone tamper: (panelData[6] - 0x55);
        decodeComplete = true;
        break;
      }

      // Zone tamper restored, zones 1-32
      if (panelData[6] >= 0x76 && panelData[6] <= 0x95) {
        // Zone tamper restored: (panelData[6] - 0x75);
        decodeComplete = true;
        break;
      }

      // Armed by access code
      if (panelData[6] >= 0x99 && panelData[6] <= 0xBD) {
        byte dscCode = panelData[6] - 0x98;
        if (dscCode >= 35) dscCode += 5;
        // Armed by
        switch (dscCode) {
          case 33: break;  // duress
          case 34: break;  // duress
          case 40: break;  // master
          case 41: break;  // supervisor
          case 42: break;  // supervisor
          default: break;  // user
        }
        // user code (dscCode);
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

        byte dscCode = panelData[6] - 0xBF;
        if (dscCode >= 35) dscCode += 5;

        switch (dscCode) {
          case 33: break;  // duress
          case 34: break;  // duress
          case 40: break;  // master
          case 41: break;  // supervisor
          case 42: break;  // supervisor
          default: break;  // user
        }
        // user code (dscCode);
        decodeComplete = true;
        break;
      }

      decodeComplete = false;
      break;
    }

    // Byte 5: xxxxxx01
    case 0x01: {
      switch (panelData[6]) {
        case 0x03: break;  // Cross zone alarm
        case 0x04: break;  // Delinquency alarm
        default: decodeComplete = false;
      }
      if (decodeComplete) break;

      // Zone fault restored, zones 1-32
      if (panelData[6] >= 0x6C && panelData[6] <= 0x8B) {
        // Zone fault restored: (panelData[6] - 0x6B);
        decodeComplete = true;
        break;
      }

      // Zone fault, zones 1-32
      if (panelData[6] >= 0x8C && panelData[6] <= 0xAB) {
        // Zone fault: (panelData[6] - 0x8B);
        decodeComplete = true;
        break;
      }

      decodeComplete = false;
      break;
    }

    // Byte 5: xxxxxx10
    case 0x02: {

      // Supervisory trouble, keypad slots 1-8
      if (panelData[6] >= 0xF1 && panelData[6] <= 0xF8) {
        // Supervisory - module trouble: Keypad slot (panelData[6] - 0xF0);
        decodeComplete = true;
        break;
      }

      // Supervisory restored, keypad slots 1-8
      if (panelData[6] >= 0xE9 && panelData[6] <= 0xF0) {
        // Supervisory - module detected: Keypad slot (panelData[6] - 0xE8);
        decodeComplete = true;
        break;
      }

      decodeComplete = false;
      break;
    }

    // Byte 5: xxxxxx11
    case 0x03: {
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

    // Byte 5: xxxxxx00
    case 0x00: {
      decodeComplete = false;
      break;
    }

    // Byte 5: xxxxxx01
    case 0x01: {
      switch (panelData[6]) {
        case 0x24: break;  // Auto-arm cancelled by duress code 33
        case 0x25: break;  // Auto-arm cancelled by duress code 34
        case 0x26: break;  // Auto-arm cancelled by master code 40
        case 0x27: break;  // Auto-arm cancelled by supervisor code 41
        case 0x28: break;  // Auto-arm cancelled by supervisor code 42
        case 0x2B: break;  // Armed by auto-arm
        case 0xAC: break;  // Exit *8 programming
        case 0xAD: break;  // Enter *8 programming
        case 0xD0: break;  // Command output 4
        default: decodeComplete = false;
      }
      if (decodeComplete) break;

      // Zones bypassed
      if (panelData[6] >= 0xB0 && panelData[6] <= 0xCF) {
        // Zone bypassed: (panelData[6] - 0xAF);
        decodeComplete = true;
        break;
      }

      decodeComplete = false;
      break;
    }

    // Byte 5: xxxxxx10
    case 0x02: {
      switch (panelData[6]) {
        case 0x2A: break;  // Quick exit
        case 0x63: break;  // Keybus fault restored
        case 0x66: break;  // Enter *1 zone bypass programming
        case 0x67: break;  // Command output 1
        case 0x68: break;  // Command output 2
        case 0x69: break;  // Command output 3
        case 0x8C: break;  // Loss of system time
        case 0x8D: break;  // Power on
        case 0x8E: break;  // Panel factory default
        case 0x93: break;  // Disarmed by keyswitch
        case 0x96: break;  // Armed by keyswitch
        case 0x98: break;  // Armed by quick-arm
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
        case 0xC3: break;  // Enter *5 programming
        case 0xE6: break;  // Enter *6 programming
        default: decodeComplete = false;
      }
      if (decodeComplete) break;

      // Auto-arm cancelled
      if (panelData[6] >= 0xC6 && panelData[6] <= 0xE5) {
        // Auto-arm cancelled by user code (panelData[6] - 0xC5);
        decodeComplete = true;
        break;
      }

      break;
    }

    // Byte 5: xxxxxx11
    case 0x03: {
      decodeComplete = false;
      break;
    }
  }
  if (decodeComplete) return;
}


void dscKeybusInterface::processPanel_0xBB() {
  if (!validCRC()) return;

  // Bell
  if (bitRead(panelData[2],5)) {}; // Bell on
}


void dscKeybusInterface::processPanel_0xC3() {
  if (!validCRC()) return;

  if (panelData[3] == 0xFF) {
    switch (panelData[2]) {
      case 0x10: break;  // Unknown command 1: Power-on +33s
      case 0x30: break;  // Keypad lockout
      default: break;    // Unrecognized data, add to 0xC3
    }
  }
}
