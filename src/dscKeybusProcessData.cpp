/*
    DSC Keybus Interface

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

#include "dscKeybusInterface.h"


// Resets the state of all status components as changed for sketches to get the current status
void dscKeybusInterface::resetStatus() {
  statusChanged = true;
  keybusChanged = true;
  //troubleChanged = true;
  powerChanged = true;
  batteryChanged = true;
  for (byte partition = 0; partition < dscPartitions; partition++) {
    readyChanged[partition] = true;
    armedChanged[partition] = true;
    alarmChanged[partition] = true;
    fireChanged[partition] = true;
	troubleChanged = true;
    disabled[partition] = true;
	
  }
  openZonesStatusChanged = true;
  alarmZonesStatusChanged = true;
  for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
    openZonesChanged[zoneGroup] = 0xFF;
    alarmZonesChanged[zoneGroup] = 0xFF;
  }
}

// Sets the panel time
bool dscKeybusInterface::setTime(unsigned int year, byte month, byte day, byte hour, byte minute, const char* accessCode) {

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
  write(timeEntry);

  return true;
}


// Processes status commands: 0x05 (Partitions 1-4) and 0x1B (Partitions 5-8)
void dscKeybusInterface::processPanelStatus() {


  // Trouble status
  if (panelData[3] <= 0x05) {  // Ignores trouble light status in intermittent states
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
    if (panelByteCount < 9) partitionCount = 2;  // Handles earlier panels that support up to 2 partitions
    else partitionCount = 4;
    if (dscPartitions < partitionCount) partitionCount = dscPartitions;
  }
  else if (dscPartitions > 4 && panelData[0] == 0x1B) {
    partitionStart = 4;
    partitionCount = 8;
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
    if (panelData[messageByte] == 0xC7) disabled[partitionIndex] = true;
    else disabled[partitionIndex] = false;

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
	    ready[partitionIndex] = true;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
		noEntryDelay[partitionIndex] = false;
        entryDelay[partitionIndex] = false;
        if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
          previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
          entryDelayChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
		
        armedStay[partitionIndex] = false;
        armedAway[partitionIndex] = false;
        armed[partitionIndex] = false;
        if (armed[partitionIndex] != previousArmed[partitionIndex]) {
          previousArmed[partitionIndex] = armed[partitionIndex];
          armedChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }

        alarm[partitionIndex] = false;
        if (alarm[partitionIndex] != previousAlarm[partitionIndex]) {
          previousAlarm[partitionIndex] = alarm[partitionIndex];
          alarmChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
        break;
      }

      // Zones open
      case 0x03: {
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
		noEntryDelay[partitionIndex] = false;
        entryDelay[partitionIndex] = false;
        if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
          previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
          entryDelayChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
        break;
      }

      // Armed
      case 0x04:         // Armed stay
      case 0x05: {       // Armed away
	   writeArm[partitionIndex] = false;
	   if (bitRead(panelData[statusByte],1) ) { // look for armed light being set to ensure valid arm message
        if (panelData[messageByte] == 0x04) {
          armedStay[partitionIndex] = true;
          armedAway[partitionIndex] = false;
        }
        else {
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
	  }
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }

        exitDelay[partitionIndex] = false;
        if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
          previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
          exitDelayChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }

        exitState[partitionIndex] = 0;

        entryDelay[partitionIndex] = false;
        if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
          previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
          entryDelayChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
        break;
      }

      // Exit delay in progress
      case 0x08: {
        writeArm[partitionIndex] = false;
        accessCodePrompt = false;
        exitDelay[partitionIndex] = true;
        if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
          previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
          exitDelayChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }

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

        ready[partitionIndex] = true;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
        break;
      }

      // Arming with no entry delay
      case 0x09: {
		ready[partitionIndex] = true;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }

        exitState[partitionIndex] = DSC_EXIT_NO_ENTRY_DELAY;
        break;
      }

      // Entry delay in progress
      case 0x0C: {
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }

        entryDelay[partitionIndex] = true;
        if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
          previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
          entryDelayChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
        break;
      }

      // Partition in alarm
      case 0x11: {
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }

        entryDelay[partitionIndex] = false;
        if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
          previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
          entryDelayChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
	  
        alarm[partitionIndex] = true;
        if (alarm[partitionIndex] != previousAlarm[partitionIndex]) {
          previousAlarm[partitionIndex] = alarm[partitionIndex];
          alarmChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
        break;
     
	  }
      // Arming with bypassed zones
      case 0x15: {
        ready[partitionIndex] = true;
		if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
	    }
        break;
      }

      // Partition armed with no entry delay
	  case 0x06:
      case 0x16: {
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

        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
        break;
      
	  }
      
      // possible power failure. Do not set state unavailable
	  case 0x17: break; 
      
      // Partition disarmed
      case 0x3D:
      case 0x3E: {
        if (panelData[messageByte] == 0x3E) {  // Sets ready only during Partition disarmed
          ready[partitionIndex] = true;
          if (ready[partitionIndex] != previousReady[partitionIndex]) {
            previousReady[partitionIndex] = ready[partitionIndex];
            readyChanged[partitionIndex] = true;
            if (!pauseStatus) statusChanged = true;
          }
        }

        exitDelay[partitionIndex] = false;
        if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
          previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
          exitDelayChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }

        exitState[partitionIndex] = 0;
		noEntryDelay[partitionIndex] = false;
        entryDelay[partitionIndex] = false;
        if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
          previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
          entryDelayChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
		
	    armedStay[partitionIndex] = false;
        armedAway[partitionIndex] = false;
        armed[partitionIndex] = false;
        if (armed[partitionIndex] != previousArmed[partitionIndex]) {
          previousArmed[partitionIndex] = armed[partitionIndex];
          armedChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }

        alarm[partitionIndex] = false;
        if (alarm[partitionIndex] != previousAlarm[partitionIndex]) {
          previousAlarm[partitionIndex] = alarm[partitionIndex];
          alarmChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
        break;
      }

      // Invalid access code
      case 0x8F: {
        if (!armed[partitionIndex]) {
          ready[partitionIndex] = true;
          if (ready[partitionIndex] != previousReady[partitionIndex]) {
            previousReady[partitionIndex] = ready[partitionIndex];
            readyChanged[partitionIndex] = true;
            if (!pauseStatus) statusChanged = true;
          }
        }
        break;
      }

      // Enter * function code
      case 0x9E: {
        wroteAsterisk = false;  // Resets the flag that delays writing after '*' is pressed
        writeAsterisk = false;
        writeKeyPending = false;
		
        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
        break;
      }

      // Enter access code
	    case 0x9F: {
        if (writeArm[partitionIndex]) {  // Ensures access codes are only sent when an arm command is sent through this interface
          accessCodePrompt = true;
          if (!pauseStatus) statusChanged = true;
        }

        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
        break;
      }
	 
      default: {
		ready[partitionIndex] = false;
		if (ready[partitionIndex] != previousReady[partitionIndex]) {
		previousReady[partitionIndex] = ready[partitionIndex];
		readyChanged[partitionIndex] = true;
		if (!pauseStatus) statusChanged = true;
	  }
        break;
      }
    }
	
  }
  
}



// Panel status and zones 1-8 status
void dscKeybusInterface::processPanel_0x27() {
  if (!validCRC()) return;

  // Messages
  for (byte partitionIndex = 0; partitionIndex < 2; partitionIndex++) {
    byte messageByte = (partitionIndex * 2) + 3;

    // Armed
    if (panelData[messageByte] == 0x04 || panelData[messageByte] == 0x05) {
      ready[partitionIndex] = false;
      if (ready[partitionIndex] != previousReady[partitionIndex]) {
        previousReady[partitionIndex] = ready[partitionIndex];
        readyChanged[partitionIndex] = true;
        if (!pauseStatus) statusChanged = true;
      }

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

      exitDelay[partitionIndex] = false;
      if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
        previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
        exitDelayChanged[partitionIndex] = true;
        if (!pauseStatus) statusChanged = true;
      }

      exitState[partitionIndex] = 0;
	}

    // Armed with no entry delay
    else if (panelData[messageByte] == 0x16 || panelData[messageByte] == 0x06) {
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

      exitDelay[partitionIndex] = false;
      if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
        previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
        exitDelayChanged[partitionIndex] = true;
        if (!pauseStatus) statusChanged = true;
      }

      exitState[partitionIndex] = 0;

      ready[partitionIndex] = false;
      if (ready[partitionIndex] != previousReady[partitionIndex]) {
        previousReady[partitionIndex] = ready[partitionIndex];
        readyChanged[partitionIndex] = true;
        if (!pauseStatus) statusChanged = true;
      }
    }
  }

  // Open zones 1-8 status is stored in openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  openZones[0] = panelData[6];
  byte zonesChanged = openZones[0] ^ previousOpenZones[0];
  if (zonesChanged != 0) {
    previousOpenZones[0] = openZones[0];
    openZonesStatusChanged = true;
    if (!pauseStatus) statusChanged = true;

    for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
      if (bitRead(zonesChanged, zoneBit)) {
        bitWrite(openZonesChanged[0], zoneBit, 1);
        if (bitRead(panelData[6], zoneBit)) bitWrite(openZones[0], zoneBit, 1);
        else bitWrite(openZones[0], zoneBit, 0);
      }
    }
  }
}



// Zones 9-16 status
void dscKeybusInterface::processPanel_0x2D() {
  if (!validCRC()) return;
  if (dscZones < 2) return;

  // Open zones 9-16 status is stored in openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
  openZones[1] = panelData[6];
  byte zonesChanged = openZones[1] ^ previousOpenZones[1];
  if (zonesChanged != 0) {
    previousOpenZones[1] = openZones[1];
    openZonesStatusChanged = true;
    if (!pauseStatus) statusChanged = true;

    for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
      if (bitRead(zonesChanged, zoneBit)) {
        bitWrite(openZonesChanged[1], zoneBit, 1);
        if (bitRead(panelData[6], zoneBit)) bitWrite(openZones[1], zoneBit, 1);
        else bitWrite(openZones[1], zoneBit, 0);
      }
    }
  }
}


// Zones 17-24 status
void dscKeybusInterface::processPanel_0x34() {
  if (!validCRC()) return;
  if (dscZones < 3) return;

  // Open zones 17-24 status is stored in openZones[2] and openZonesChanged[2]: Bit 0 = Zone 17 ... Bit 7 = Zone 24
  openZones[2] = panelData[6];
  byte zonesChanged = openZones[2] ^ previousOpenZones[2];
  if (zonesChanged != 0) {
    previousOpenZones[2] = openZones[2];
    openZonesStatusChanged = true;
    if (!pauseStatus) statusChanged = true;

    for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
      if (bitRead(zonesChanged, zoneBit)) {
        bitWrite(openZonesChanged[2], zoneBit, 1);
        if (bitRead(panelData[6], zoneBit)) bitWrite(openZones[2], zoneBit, 1);
        else bitWrite(openZones[2], zoneBit, 0);
      }
    }
  }
}


// Zones 25-32 status
void dscKeybusInterface::processPanel_0x3E() {
  if (!validCRC()) return;
  if (dscZones < 4) return;

  // Open zones 25-32 status is stored in openZones[3] and openZonesChanged[3]: Bit 0 = Zone 25 ... Bit 7 = Zone 32
  openZones[3] = panelData[6];
  byte zonesChanged = openZones[3] ^ previousOpenZones[3];
  if (zonesChanged != 0) {
    previousOpenZones[3] = openZones[3];
    openZonesStatusChanged = true;
    if (!pauseStatus) statusChanged = true;

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
  if (dscYear3 >= 7) year += 1900;
  else year += 2000;
  month = panelData[3] << 2; month >>= 4;
  byte dscDay1 = panelData[3] << 6; dscDay1 >>= 3;
  byte dscDay2 = panelData[4] >> 5;
  day = dscDay1 | dscDay2;
  hour = panelData[4] & 0x1F;
  minute = panelData[5] >> 2;

  // Timestamp
  if (panelData[6] == 0 && panelData[7] == 0) {
    statusChanged = true;
    timestampChanged = true;
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
  if (dscYear3 >= 7) year += 1900;
  else year += 2000;
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
  if ( panelData[0] == 0xA5) {
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
        powerChanged = true;
        if (!pauseStatus) statusChanged = true;
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
        powerChanged = true;
        if (!pauseStatus) statusChanged = true;
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

    armedAway[partitionIndex] = false;
    armedStay[partitionIndex] = false;
    armed[partitionIndex] = false;

    if (armed[partitionIndex] != previousArmed[partitionIndex]) {
      previousArmed[partitionIndex] = armed[partitionIndex];
      armedChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
    }

    alarm[partitionIndex] = false;
    if (alarm[partitionIndex] != previousAlarm[partitionIndex]) {
      previousAlarm[partitionIndex] = alarm[partitionIndex];
      alarmChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
    }

    entryDelay[partitionIndex] = false;
    if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
      previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
      entryDelayChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
    }
    return;
  }

  // Partition in alarm
  if (panelData[panelByte] == 0x4B) {
    alarm[partitionIndex] = true;
    if (alarm[partitionIndex] != previousAlarm[partitionIndex]) {
      previousAlarm[partitionIndex] = alarm[partitionIndex];
      alarmChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
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
    if (alarm[partitionIndex] != previousAlarm[partitionIndex]) {
      previousAlarm[partitionIndex] = alarm[partitionIndex];
      alarmChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
    }

    entryDelay[partitionIndex] = false;
    if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
      previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
      entryDelayChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
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
            if (!pauseStatus) statusChanged = true;
          }
        }
        else if (zoneCount >= 8 && zoneCount < 16) {
          bitWrite(alarmZones[1], (zoneCount - 8), 1);
          if (bitRead(previousAlarmZones[1], (zoneCount - 8)) != 1) {
            bitWrite(previousAlarmZones[1], (zoneCount - 8), 1);
            bitWrite(alarmZonesChanged[1], (zoneCount - 8), 1);
            alarmZonesStatusChanged = true;
            if (!pauseStatus) statusChanged = true;
          }
        }
        else if (zoneCount >= 16 && zoneCount < 24) {
          bitWrite(alarmZones[2], (zoneCount - 16), 1);
          if (bitRead(previousAlarmZones[2], (zoneCount - 16)) != 1) {
            bitWrite(previousAlarmZones[2], (zoneCount - 16), 1);
            bitWrite(alarmZonesChanged[2], (zoneCount - 16), 1);
            alarmZonesStatusChanged = true;
            if (!pauseStatus) statusChanged = true;
          }
        }
        else if (zoneCount >= 24 && zoneCount < 32) {
          bitWrite(alarmZones[3], (zoneCount - 24), 1);
          if (bitRead(previousAlarmZones[3], (zoneCount - 24)) != 1) {
            bitWrite(previousAlarmZones[3], (zoneCount - 24), 1);
            bitWrite(alarmZonesChanged[3], (zoneCount - 24), 1);
            alarmZonesStatusChanged = true;
            if (!pauseStatus) statusChanged = true;
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
            if (!pauseStatus) statusChanged = true;
          }
        }
        else if (zoneCount >= 8 && zoneCount < 16) {
          bitWrite(alarmZones[1], (zoneCount - 8), 0);
          if (bitRead(previousAlarmZones[1], (zoneCount - 8)) != 0) {
            bitWrite(previousAlarmZones[1], (zoneCount - 8), 0);
            bitWrite(alarmZonesChanged[1], (zoneCount - 8), 1);
            alarmZonesStatusChanged = true;
            if (!pauseStatus) statusChanged = true;
          }
        }
        else if (zoneCount >= 16 && zoneCount < 24) {
          bitWrite(alarmZones[2], (zoneCount - 16), 0);
          if (bitRead(previousAlarmZones[2], (zoneCount - 16)) != 0) {
            bitWrite(previousAlarmZones[2], (zoneCount - 16), 0);
            bitWrite(alarmZonesChanged[2], (zoneCount - 16), 1);
            alarmZonesStatusChanged = true;
            if (!pauseStatus) statusChanged = true;
          }
        }
        else if (zoneCount >= 24 && zoneCount < 32) {
          bitWrite(alarmZones[3], (zoneCount - 24), 0);
          if (bitRead(previousAlarmZones[3], (zoneCount - 24)) != 0) {
            bitWrite(previousAlarmZones[3], (zoneCount - 24), 0);
            bitWrite(alarmZonesChanged[3], (zoneCount - 24), 1);
            alarmZonesStatusChanged = true;
            if (!pauseStatus) statusChanged = true;
          }
        }
      }
    }
    return;
  }

  // Armed by access code
  if (panelData[panelByte] >= 0x99 && panelData[panelByte] <= 0xBD) {
    accessCode[partitionIndex] = panelData[panelByte] - 0x98;
    if (accessCode[partitionIndex] >= 35) accessCode[partitionIndex] += 5;
    if (accessCode[partitionIndex] != previousAccessCode[partitionIndex]) {
      previousAccessCode[partitionIndex] = accessCode[partitionIndex];
      accessCodeChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
    }
    return;
  }

  // Disarmed by access code
  if (panelData[panelByte] >= 0xC0 && panelData[panelByte] <= 0xE4) {
    accessCode[partitionIndex] = panelData[panelByte] - 0xBF;
    if (accessCode[partitionIndex] >= 35) accessCode[partitionIndex] += 5;
    if (accessCode[partitionIndex] != previousAccessCode[partitionIndex]) {
      previousAccessCode[partitionIndex] = accessCode[partitionIndex];
      accessCodeChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
    }
    return;
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

    exitDelay[partitionIndex] = false;
    if (exitDelay[partitionIndex] != previousExitDelay[partitionIndex]) {
      previousExitDelay[partitionIndex] = exitDelay[partitionIndex];
      exitDelayChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
    }

    exitState[partitionIndex] = 0;

    ready[partitionIndex] = false;
    if (ready[partitionIndex] != previousReady[partitionIndex]) {
      previousReady[partitionIndex] = ready[partitionIndex];
      readyChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
    }
    return;
  }

  if (panelData[panelByte] == 0xA5) {
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
		  
        noEntryDelay[partitionIndex] = true;

        ready[partitionIndex] = false;
        if (ready[partitionIndex] != previousReady[partitionIndex]) {
          previousReady[partitionIndex] = ready[partitionIndex];
          readyChanged[partitionIndex] = true;
          if (!pauseStatus) statusChanged = true;
        }
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
    if (alarm[partitionIndex] != previousAlarm[partitionIndex]) {
      previousAlarm[partitionIndex] = alarm[partitionIndex];
      alarmChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
    }

    entryDelay[partitionIndex] = false;
    if (entryDelay[partitionIndex] != previousEntryDelay[partitionIndex]) {
      previousEntryDelay[partitionIndex] = entryDelay[partitionIndex];
      entryDelayChanged[partitionIndex] = true;
      if (!pauseStatus) statusChanged = true;
    }

    byte maxZones = dscZones * 8;
    if (maxZones > 32) maxZones -= 32;
    else return;
    for (byte zoneCount = 0; zoneCount < maxZones; zoneCount++) {
      if (panelData[panelByte] == zoneCount) {
        if (zoneCount < 8) {
          bitWrite(alarmZones[4], zoneCount, 1);
          bitWrite(alarmZonesChanged[4], zoneCount, 1);
          if (!pauseStatus) statusChanged = true;
        }
        else if (zoneCount >= 8 && zoneCount < 16) {
          bitWrite(alarmZones[5], (zoneCount - 8), 1);
          bitWrite(alarmZonesChanged[5], (zoneCount - 8), 1);
          if (!pauseStatus) statusChanged = true;
        }
        else if (zoneCount >= 16 && zoneCount < 24) {
          bitWrite(alarmZones[6], (zoneCount - 16), 1);
          bitWrite(alarmZonesChanged[6], (zoneCount - 16), 1);
          if (!pauseStatus) statusChanged = true;
        }
        else if (zoneCount >= 24 && zoneCount < 32) {
          bitWrite(alarmZones[7], (zoneCount - 24), 1);
          bitWrite(alarmZonesChanged[7], (zoneCount - 24), 1);
          if (!pauseStatus) statusChanged = true;
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
          if (!pauseStatus) statusChanged = true;
        }
        else if (zoneCount >= 8 && zoneCount < 16) {
          bitWrite(alarmZones[5], (zoneCount - 8), 0);
          bitWrite(alarmZonesChanged[5], (zoneCount - 8), 1);
          if (!pauseStatus) statusChanged = true;
        }
        else if (zoneCount >= 16 && zoneCount < 24) {
          bitWrite(alarmZones[6], (zoneCount - 16), 0);
          bitWrite(alarmZonesChanged[6], (zoneCount - 16), 1);
          if (!pauseStatus) statusChanged = true;
        }
        else if (zoneCount >= 24 && zoneCount < 32) {
          bitWrite(alarmZones[7], (zoneCount - 24), 0);
          bitWrite(alarmZonesChanged[7], (zoneCount - 24), 1);
          if (!pauseStatus) statusChanged = true;
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
      if (!pauseStatus) statusChanged = true;

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
      if (!pauseStatus) statusChanged = true;

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
      if (!pauseStatus) statusChanged = true;

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
      if (!pauseStatus) statusChanged = true;

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


