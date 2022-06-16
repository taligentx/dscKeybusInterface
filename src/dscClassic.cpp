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

#include "dscClassic.h"

#if defined(ESP32)
portMUX_TYPE dscClassicInterface::timer1Mux = portMUX_INITIALIZER_UNLOCKED;
hw_timer_t * dscClassicInterface::timer1 = NULL;
#endif


dscClassicInterface::dscClassicInterface(byte setClockPin, byte setReadPin, byte setPC16Pin, byte setWritePin, const char * setAccessCode) {
  dscClockPin = setClockPin;
  dscReadPin = setReadPin;
  dscPC16Pin = setPC16Pin;
  dscWritePin = setWritePin;
  bufferHead = 1;
  bufferTail = 0;
  panelBufferLength = 0;
  if (dscWritePin != 0xFF) virtualKeypad = true;
  writeReady = false;
  writePartition = 1;
  pauseStatus = false;
  beepLong = false;
  accessCodeStay = setAccessCode;
  strcpy(accessCodeAway, accessCodeStay);
  strcat(accessCodeAway, "*1");
  strcpy(accessCodeNight, "*9");
  strcat(accessCodeNight, accessCodeStay);
  processModuleData = true;
}


void dscClassicInterface::begin(Stream &_stream) {
  pinMode(dscClockPin, INPUT);
  pinMode(dscReadPin, INPUT);
  pinMode(dscPC16Pin, INPUT);
  if (virtualKeypad) pinMode(dscWritePin, OUTPUT);
  stream = &_stream;

  // Platform-specific timers trigger a read of the data line 250us after the Keybus clock changes:

  // Arduino/AVR Timer1 calls ISR(TIMER1_OVF_vect) from dscClockInterrupt() and is disabled in the ISR for a one-shot timer
  #if defined(__AVR__)
  TCCR1A = 0;
  TCCR1B = 0;
  TIMSK1 |= (1 << TOIE1);

  // esp8266 timer1 calls dscDataInterrupt() from dscClockInterrupt() as a one-shot timer
  #elif defined(ESP8266)
  timer1_isr_init();
  timer1_attachInterrupt(dscDataInterrupt);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);

  // esp32 timer1 calls dscDataInterrupt() from dscClockInterrupt()
  #elif defined(ESP32)
  timer1 = timerBegin(1, 80, true);
  timerStop(timer1);
  timerAttachInterrupt(timer1, &dscDataInterrupt, true);
  timerAlarmWrite(timer1, 250, true);
  timerAlarmEnable(timer1);
  #endif

  // Generates an interrupt when the Keybus clock rises or falls - requires a hardware interrupt pin on Arduino/AVR
  attachInterrupt(digitalPinToInterrupt(dscClockPin), dscClockInterrupt, CHANGE);
}


void dscClassicInterface::stop() {

  // Disables Arduino/AVR Timer1 interrupts
  #if defined(__AVR__)
  TIMSK1 = 0;

  // Disables esp8266 timer1
  #elif defined(ESP8266)
  timer1_disable();
  timer1_detachInterrupt();

  // Disables esp32 timer1
  #elif defined(ESP32)
  timerAlarmDisable(timer1);
  timerEnd(timer1);
  #endif

  // Disables the Keybus clock pin interrupt
  detachInterrupt(digitalPinToInterrupt(dscClockPin));

  // Resets the panel capture data and counters
  panelBufferLength = 0;
  for (byte i = 0; i < dscReadSize; i++) {
    isrPanelData[i] = 0;
    isrPC16Data[i] = 0;
    isrModuleData[i] = 0;
  }
  isrPanelBitTotal = 0;
  isrPanelBitCount = 0;
  isrPanelByteCount = 0;

  // Resets the keypad and module data counters
  isrModuleBitTotal = 0;
  isrModuleBitCount = 0;
  isrModuleByteCount = 0;
}

// Resets the state of all status components as changed for sketches to get the current status
void dscClassicInterface::resetStatus() {
  statusChanged = true;
  keybusChanged = true;
  troubleChanged = true;
  readyChanged[0] = true;
  armedChanged[0] = true;
  alarmChanged[0] = true;
  fireChanged[0] = true;
  openZonesStatusChanged = true;
  alarmZonesStatusChanged = true;
  openZonesChanged[0] = 0xFF;
  alarmZonesChanged[0] = 0xFF;
  pgmOutputsChanged[0] = true;
}

/***********************************************************************************************
 * Helpers for processing
 ***********************************************************************************************/

void dscClassicInterface::isLightBlinking(bool &light, bool &blinking, unsigned long &timeOn, unsigned long &timeOff, unsigned long now, unsigned int onTrig, unsigned int offTrig) {
  if (light) {
    timeOn = now;
    if (now - timeOff < offTrig) {
        blinking = true;
    }
    else {
      blinking = false;
    }
  }
  else {
    timeOff = now;
    if (now - timeOn > onTrig) {
      blinking = false;
    }
  }
}


void dscClassicInterface::processKeyHold(bool status, bool &state, unsigned long &timeLast, unsigned long now) {
  if (status) {
    if (now - timeLast > 1000) {
      state = true;
      timeLast = now;
      if (!pauseStatus) statusChanged = true;
    }
  }
}


void dscClassicInterface::processAStatus(bool status, bool &state, bool &previousState, bool &stateChanged) {
  state = status;
  if (state != previousState) {
    previousState = state;
    stateChanged = true;
    if (!pauseStatus) statusChanged = true;
  }
}


void dscClassicInterface::processReadyStatus(bool status) {
  processAStatus(status, ready[0], previousReady, readyChanged[0]);
}


void dscClassicInterface::processAlarmStatus(bool status) {
  processAStatus(status, alarm[0], previousAlarm, alarmChanged[0]);
}


void dscClassicInterface::processExitDelayStatus(bool status) {
  processAStatus(status, exitDelay[0], previousExitDelay, exitDelayChanged[0]);
}


void dscClassicInterface::processArmedStatus(bool armedStatus) {
  armedStay[0] = armedStatus;
  armedAway[0] = armedStatus;
  processAStatus(status, armed[0], previousArmed, armedChanged[0]);
}


bool dscClassicInterface::handleModule() {
  if (!moduleDataCaptured || !beep) return false;
  moduleDataCaptured = false;

  if (moduleBitCount < 8) return false;

  return true;
}


byte dscClassicInterface::decodeModuleDigit(byte keyPressed) {
  switch (keyPressed) {
    case 0xBE: return '1'; break;
    case 0xDE: return '2'; break;
    case 0xEE: return '3'; break;
    case 0xBD: return '4'; break;
    case 0xDD: return '5'; break;
    case 0xED: return '6'; break;
    case 0xBB: return '7'; break;
    case 0xDB: return '8'; break;
    case 0xEB: return '9'; break;
    case 0xD7: return '0'; break;
    case 0xB7: return '*'; break;
    case 0xE7: return '#'; break;
    case 0x3F: return 'F'; break;
    case 0x5F: return 'A'; break;
    case 0x6F: return 'P'; break;
  }
}

/***********************************************************************************************
 * Entry Point
 ***********************************************************************************************/

bool dscClassicInterface::loop() {

  #if defined(ESP8266) || defined(ESP32)
  yield();
  #endif

  // Checks if Keybus data is detected and sets a status flag if data is not detected for 3s
  #if defined(ESP32)
  portENTER_CRITICAL(&timer1Mux);
  #else
  noInterrupts();
  #endif

  if (millis() - keybusTime > 3000) keybusConnected = false;  // keybusTime is set in dscDataInterrupt() when the clock resets
  else keybusConnected = true;

  #if defined(ESP32)
  portEXIT_CRITICAL(&timer1Mux);
  #else
  interrupts();
  #endif

  if (previousKeybus != keybusConnected) {
    previousKeybus = keybusConnected;
    keybusChanged = true;
    if (!pauseStatus) statusChanged = true;
    if (!keybusConnected) return true;
  }

  // Writes keys when multiple keys are sent as a char array
  if (writeKeysPending) writeKeys(NULL, 0);

  #if defined(ESP32)
  portENTER_CRITICAL(&timer1Mux);
  #else
  noInterrupts();
  #endif

  byte bufferNext = (bufferTail + 1) % dscBufferSize;
  bool bufferEmpty = (bufferNext == bufferHead);

  if (!bufferEmpty) {
    // Copies data from the buffer to panelData[]
    for (byte i = 0; i < dscReadSize; i++) {
      panelData[i] = panelBuffer[bufferNext][i];
      pc16Data[i] = pc16Buffer[bufferNext][i];
    }
    panelBitCount = panelBufferBitCount[bufferNext];
    panelByteCount = panelBufferByteCount[bufferNext];

    bufferTail = bufferNext;
    panelBufferLength--;
  }
  else panelBufferLength = 0;

  #if defined(ESP32)
  portEXIT_CRITICAL(&timer1Mux);
  #else
  interrupts();
  #endif

  // Skips processing if the panel data buffer is empty
  if (bufferEmpty) return false;

  // Waits at startup for valid data
  if (panelByteCount < 2) return false;

  // Sets writeReady status
  writeReady = (!writeKeyPending && !writeKeysPending);

  return processPanelLightsData(); // Keypad lights - maps Classic series keypad lights to PowerSeries keypad light order for sketch compatibility
}


#define pdReadyLit( _x_ ) bitRead((_x_), 0)
#define pdArmedLit( _x_ ) bitRead((_x_), 1)
#define pdMemoryLit( _x_ ) bitRead((_x_), 2)
#define pdBypassLit( _x_ ) bitRead((_x_), 3)
#define pdTroubleLit( _x_ ) bitRead((_x_), 4)
#define pdProgramLit( _x_ ) bitRead((_x_), 5)
#define pdFireLit( _x_ ) bitRead((_x_), 6)
#define pdBeepOn( _x_ ) bitRead((_x_), 7)


bool dscClassicInterface::processPanelLightsData() {
  unsigned long now = millis();
  byte &data = panelData[0];

  readyLight = pdReadyLit(data);
  armedLight = pdArmedLit(data);
  memoryLight = pdMemoryLit(data);
  bypassLight = pdBypassLit(data);
  troubleLight = pdTroubleLit(data);
  programLight = pdProgramLit(data);
  fireLight = pdFireLit(data);
  beep = pdBeepOn(data);

  lights[0] = data & 0x7F;

  if (lights[0] != previousLights) {
    previousLights = lights[0];

    if (!pauseStatus) statusChanged = true;
  }

  // Checks for memory light blinking
  static unsigned long memoryLightTimeOn = 0;
  static unsigned long memoryLightTimeOff = 0;
  isLightBlinking(memoryLight, memoryBlink, memoryLightTimeOn, memoryLightTimeOff, now, 600, 600);

  // Checks for armed light blinking
  static unsigned long armedLightTimeOn = 0;
  static unsigned long armedLightTimeOff = 0;
  isLightBlinking(armedLight, armedBlink, armedLightTimeOn, armedLightTimeOff, now, 1200, 600);

  // Checks for bypass light blinking
  static unsigned long bypassLightTimeOn = 0;
  static unsigned long bypassLightTimeOff = 0;
  isLightBlinking(bypassLight, bypassBlink, bypassLightTimeOn, bypassLightTimeOff, now, 1200, 600);

  // Checks for trouble light blinking
  static unsigned long troubleLightTimeOn = 0;
  static unsigned long troubleLightTimeOff = 0;
  isLightBlinking(troubleLight, troubleBlink, troubleLightTimeOn, troubleLightTimeOff, now, 1200, 600);

  lightBlink = memoryBlink || armedBlink || bypassBlink || troubleBlink;

  // Checks for beep status
  static unsigned long beepTimeOn = 0;
  static unsigned long beepTimeOff = 0;
  if (beep) {
    beepTimeOn = now;
  }
  else {
    if (beepTimeOff < beepTimeOn) beepTimeOff = now;
    beepLong = beepTimeOff - beepTimeOn > 600;
  }
  armedLightSteady = now - armedLightTimeOn > 600;
  beepOld = now - beepTimeOff > 2000;
  return processPanelPGMData();    // PC-16 status
}


bool dscClassicInterface::processPanelPGMData() {
  unsigned long now = millis();

  troubleBit = bitRead(pc16Data[0], 0);
  armedBypassBit = bitRead(pc16Data[0], 1);
  armedBit = bitRead(pc16Data[0], 2);
  armed2Bit = bitRead(pc16Data[0], 3);
  kpPanicBit = bitRead(pc16Data[0], 4);
  kpAuxBit = bitRead(pc16Data[0], 5);
  kpFireBit = bitRead(pc16Data[0], 6);
  alarmBit = bitRead(pc16Data[0], 7);

  fireBit = bitRead(pc16Data[1], 0);

  // Trouble status
  processAStatus(troubleBit, trouble, previousTrouble, troubleChanged);

  // Fire status
  processAStatus(fireBit, fire[0], previousFire, fireChanged[0]);

  // Keypad Fire alarm
  static unsigned long previousFireAlarm = 0;
  processKeyHold(kpFireBit, keypadFireAlarm, previousFireAlarm, now);

  // Keypad Aux alarm
  static unsigned long previousAuxAlarm = 0;
  processKeyHold(kpAuxBit, keypadAuxAlarm, previousAuxAlarm, now);

  // Keypad Panic alarm
  static unsigned long previousPanicAlarm = 0;
  processKeyHold(kpPanicBit, keypadPanicAlarm, previousPanicAlarm, now);

  // Armed status
  static bool armedStayTriggered;
  if (armedBit) {
    armed[0] = true;
    exitDelayArmed = true;
    if (bypassLight || armedBypassBit) {
      armedStay[0] = true;
      armedStayTriggered = true;
      armedAway[0] = false;
    }
    else if (armedStayTriggered) {
      if (!beep && !alarmBit && beepLong && beepOld) {
        armedStay[0] = false;
        armedAway[0] = true;
      }
    }
    else {
      armedStay[0] = false;
      armedAway[0] = true;
    }

    if (armedBlink) {
      noEntryDelay[0] = true;
      exitState[0] = DSC_EXIT_NO_ENTRY_DELAY;
    }

    // Reset ready status
    processReadyStatus(false);
  }
  else {
    armedStayTriggered = false;
    processArmedStatus(false);
    processAlarmStatus(false);
  }

  if (armed[0] != previousArmed || armedStay[0] != previousArmedStay || armedAway[0] != previousArmedAway) {
    previousArmed = armed[0];
    previousArmedStay = armedStay[0];
    previousArmedAway = armedAway[0];
    armedChanged[0] = true;
    if (!pauseStatus) statusChanged = true;
  }

  // Ready status
  if (readyLight && !armedBit) {
    processReadyStatus(true);
    processArmedStatus(false);
    processAlarmStatus(false);
    exitDelayArmed = false;
    previousAlarmTriggered = false;
    starKeyDetected = false;
    if (!armedBlink) noEntryDelay[0] = false;

    if (armedLight) {
      processExitDelayStatus(true);
      exitDelayTriggered = true;
      if (exitState[0] != DSC_EXIT_NO_ENTRY_DELAY) {
        if (bypassLight) exitState[0] = DSC_EXIT_STAY;
        else exitState[0] = DSC_EXIT_AWAY;
        if (exitState[0] != previousExitState) {
          previousExitState = exitState[0];
          exitDelayChanged[0] = true;
          exitStateChanged[0] = true;
          if (!pauseStatus) statusChanged = true;
        }
      }
    }

    // Reset armed status
    else if (!exitDelayArmed && !armedBlink && armedLightSteady) {
      processExitDelayStatus(false);
      exitState[0] = 0;
    }
  }
  else {
    if (panelData[1] || (dscZones == 2 && panelData[2])) {
      processReadyStatus(false);
    }

    if (exitDelayArmed && !armedBit) {
      processReadyStatus(false);
      exitDelayArmed = false;
    }
    if (exitDelay[0] && armedBit) {
      processExitDelayStatus(false);
    }
  }

  // Alarm status - requires PGM output section 24 configured to option 08: Strobe Output
  if (lights[0] != 0) {
    if (alarmBit && !memoryBlink) {
      processReadyStatus(false);
      processAlarmStatus(true);
      alarmTriggered = true;
    }
    else if (!memoryBlink && !armedChanged[0]) {
      processAlarmStatus(false);
      if (alarmTriggered) {
        alarmTriggered = false;
        previousAlarmTriggered = true;
      }
    }
  }
  return processModuleState();     // Keypad input
}


// Debounce the keypress by reading at beep start until beep stop (2 separate states)
// allows feedback on validity of the keypress
bool dscClassicInterface::processModuleState() {
  static byte keyPressed = 0;

  if (keyPressed) {
    if (!beep) {
      processModuleDigit(keyPressed, !beepLong);
      keyPressed = 0;
      writeKeyWait = false;
    }
  }
  else if (moduleData[0]) {
    if (beep) {
      keyPressed = decodeModuleDigit(moduleData[0]);
    }
  }
  return processPanelOpenZones();  // Open zones
}


// Processes data from the panel
bool dscClassicInterface::processPanelOpenZones() {
  bool processLights = (!previousAlarmTriggered && !memoryBlink && !bypassBlink && !troubleBlink && !starKeyDetected);
  for (byte i = 1; i < panelByteCount; i++) {
    if (processLights) processPanelLightsZones(i);   // Zones Light status
    processPanelPGMZones(i);                         // Alarm zones status
  }
  return processPanelStatus();     // Overall state
}


// Processes status
bool dscClassicInterface::processPanelStatus() {

  switch(status[0]) {
    case 0xA1:                                         // *2: Trouble
      // do nothing yet
      //break;

    case 0xB7:                                         // *8 Enter installer code
    case 0xF7:                                         // *8: Installer programming, 2 digits
    case 0x8F:                                         // Invalid access code
    case 0xE4:                                         // *8 Main Menu
    case 0xF8:                                         // Entered keypad programming, code is valid
    case 0x7F:                                         // Long Beep
      // do nothing yet
      break;

    default:
      // Status - sets the status to match PowerSeries status codes for sketch compatibility
      if (memoryBlink && bypassBlink && troubleBlink) {
        status[0] = 0xE4;                                // Programming
      }
      else {
        if (readyChanged[0]) {
          if (ready[0]) status[0] = 0x01;                                     // Partition ready
          else if (openZonesStatusChanged && openZones[0]) status[0] = 0x03;  // Zones open
        }

        if (armedChanged[0]) {
          if (armed[0]) {
            if (armedAway[0]) status[0] = 0x05;         // Armed away
            else if (armedStay[0]) status[0] = 0x04;    // Armed stay

            if (noEntryDelay[0]) status[0] = 0x06;      // Armed with no entry delay
          }
          else status[0] = 0x3E;                        // Disarmed
        }

        if (alarmChanged[0]) {
          if (alarm[0]) status[0] = 0x11;               // Alarm
          else if (!armedChanged[0]) status[0] = 0x3E;  // Disarmed
        }

        if (exitDelayChanged[0]) {
          if (exitDelay[0]) status[0] = 0x08;           // Exit delay in progress
          else if (!armed[0]) status[0] = 0x3E;         // Disarmed
        }

        if (status[0] == 0x3E) {                        // Disarmed
         if (ready[0]) status[0] = 0x01;                // Ready
         else if (openZones[0]) status[0] = 0x03;       // Open Zones
        }
      }
      break;
  }

  if (status[0] != previousStatus) {
    previousStatus = status[0];
    if (!pauseStatus) statusChanged = true;
  }
  return true;
}


void dscClassicInterface::processPanelLightsZones(byte dataOffset) {
  byte zoneOffset = dataOffset - 1;
  byte &data = panelData[dataOffset];
  byte &open = openZones[zoneOffset];
  byte &prevOpen = previousOpenZones[zoneOffset];
  byte &triggered = zonesTriggered[zoneOffset];
  byte &openChanged = openZonesChanged[zoneOffset];

  if (!alarmBit || exitDelay[0]) {
    for (byte bit = 7; bit <= 7; bit--) {
      if (!bitRead(triggered, bit)) {
        bitWrite(open, bit, bitRead(data, bit));
      }
    }
  }

  byte zonesChanged = open ^ prevOpen;
  if (zonesChanged != 0) {
    prevOpen = open;
    openZonesStatusChanged = true;
    if (!pauseStatus) statusChanged = true;

    openChanged |= zonesChanged;
  }
}


void dscClassicInterface::processPanelPGMZones(byte dataOffset) {
  byte zoneOffset = dataOffset - 1;
  byte &data = pc16Data[dataOffset];
  byte &open = openZones[zoneOffset];
  byte &alarm = alarmZones[zoneOffset];
  byte &prevOpen = previousOpenZones[zoneOffset];
  byte &triggered = zonesTriggered[zoneOffset];
  byte &prevAlarm = previousAlarmZones[zoneOffset];
  byte &alarmChanged = alarmZonesChanged[zoneOffset];
  byte &openChanged = openZonesChanged[zoneOffset];

  // PC1500: z1 z2 z3 z4 z5   z6
  // PC2500: z1 z2 z3 z4 z5-8 /
  // PC3000: z1 z2 z3 z4 z5-8 z9-16
  // TODO: support the multi zone properly
  alarm = data;
  triggered |= data;

  byte zonesChanged = alarm ^ prevAlarm;
  if (zonesChanged != 0) {
    prevAlarm = alarm;
    alarmZonesStatusChanged = true;
    if (!pauseStatus) statusChanged = true;

    alarmChanged |= zonesChanged;

    // Handles zones closed during an alarm
    if (alarmBit) {
      open = alarm;
      openChanged = 0xFF;
      openZonesStatusChanged = true;
      prevOpen = open;
    }
  }
}


enum keypadState {
	kps_reset_idle,
    kps_idle,
    kps_pin1, kps_pin2, kps_pin3, kps_pin4, kps_pin_accepted, kps_pin_rejected,
    kps_star,              // *
      kps_trouble_read,    // *2
      kps_memory_read,     // *3
      kps_config_menu,     // *6
        kps_master_pin1, kps_master_pin2, kps_master_pin3, kps_master_pin4, kps_master_pin_accepted, kps_master_pin_rejected,
      kps_installer_menu1, // *8
        kps_install_pin1, kps_install_pin2, kps_install_pin3, kps_install_pin4, kps_install_pin_accepted, kps_install_pin_rejected,
        kps_install_menu2,
          kps_prog_pin1, kps_prog_pin2, kps_prog_pin3, kps_prog_pin_accepted, kps_prog_pin_rejected
};


void dscClassicInterface::processModuleDigit(byte keyPressed, bool accepted) {
  static keypadState state = kps_idle;

  //printModuleDigit(keyPressed, accepted);
  //stream->print(F("processModuleDigit: "));
  //stream->print(state);
  //stream->print(F(" to "));
  switch(state) {
    case kps_reset_idle:
      lightBlink = memoryBlink = armedBlink = bypassBlink = troubleBlink = starKeyDetected = false;
    case kps_idle:
      switch (keyPressed) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0': if(accepted) state = kps_pin1; break;
        case '*': if(accepted) state = kps_star; break;
        case '#': state = kps_reset_idle; break;
        case 'F': state = kps_idle; break;
        case 'A': state = kps_idle; break;
        case 'P': state = kps_idle; break;
      }
      break;
    case kps_pin1:
    case kps_pin2:
    case kps_pin3:
    case kps_pin4:
      switch (keyPressed) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
          switch (state) {
            case kps_pin1: state = accepted ? kps_pin2 : kps_idle; break;
            case kps_pin2: state = accepted ? kps_pin3 : kps_idle; break;
            case kps_pin3: state = accepted ? kps_pin4 : kps_idle; break;
            case kps_pin4: state = accepted ? kps_pin_accepted : kps_idle; break;
          }
          break;
        case '*': state = accepted ? kps_star : kps_pin_rejected; break;
        case '#': state = kps_reset_idle; break;
        case 'F': state = kps_idle; break;
        case 'A': state = kps_idle; break;
        case 'P': state = kps_idle; break;
      }
      break;
    case kps_prog_pin1:
    case kps_prog_pin2:
    case kps_prog_pin3:
      switch (keyPressed) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
          switch (state) {
            case kps_prog_pin1: state = accepted ? kps_prog_pin2 : kps_idle; break;
            case kps_prog_pin2: state = accepted ? kps_prog_pin3 : kps_idle; break;
            case kps_prog_pin3: state = accepted ? kps_prog_pin_accepted : kps_prog_pin_rejected; break;
          }
          break;
        case '*': state = accepted ? kps_star : kps_prog_pin_rejected; break;
        case '#': state = kps_reset_idle; break;
        case 'F': state = kps_idle; break;
        case 'A': state = kps_idle; break;
        case 'P': state = kps_idle; break;
      }
      break;
    case kps_pin_accepted:
    case kps_pin_rejected:
    case kps_install_pin_accepted:
    case kps_master_pin_accepted:
    case kps_prog_pin_accepted:
    case kps_master_pin_rejected:
      switch (keyPressed) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
        case '*':
        case 'F':
        case 'A':
        case 'P': state = kps_idle; break;
        case '#': state = kps_reset_idle; break;
      }
      break;
    case kps_trouble_read:                           // *2: Trouble
      starKeyDetected = true;
      if (keyPressed == '#') state = kps_reset_idle;
      break;
    case kps_memory_read:                            // *3 Memory menu
      starKeyDetected = true;
      if (keyPressed == '#') state = kps_reset_idle;
      break;
    case kps_config_menu:                            // *6 Config Menu
      switch (keyPressed) {
        case '1':
        case '2':
        case '3':
        case '4':                                    // Quick Arm toggle [*], [6], [Master Code], [4]
        case '5':
        case '6':                                    // Door Chime toggle [*], [6], [Master Code], [6]
        case '7':
        case '8':                                    // Alarm Test [*], [6], [Master Code], [8]
        case '9':
        case '0':
        case '*':
        case '#':
        case 'F':
        case 'A':
        case 'P': state = kps_idle; break;
      }
      break;
    case kps_install_pin_rejected:
    case kps_prog_pin_rejected:
      starKeyDetected = true;
      if (keyPressed == '#') state = kps_reset_idle;
      break;
    case kps_star:
      starKeyDetected = true;
      switch (keyPressed) {
        case '1': state = kps_install_pin_rejected; break;                  // *1 Bypass Menu
        case '2': state = accepted ? kps_trouble_read : kps_idle; break;    // *2 Trouble Menu
        case '3': state = accepted ? kps_memory_read : kps_idle; break;     // *3 Memory Menu
        case '4': state = kps_install_pin_rejected; break;
        case '5': state = kps_install_pin_rejected; break;                  // *5 Progamming Pin Codes
        case '6': state = kps_config_menu; break;                           // *6 Config Menu
        case '7': state = kps_install_pin_rejected; break;
        case '8': state = accepted ? kps_installer_menu1 : kps_idle; break; // *8 Installer Menu
        case '9': state = kps_install_pin_rejected; break;                  // *9 Entry Delay off
        case '0': state = kps_idle; break;                                  // *0 Quick Arm
        case '*': state = accepted ? kps_star : kps_install_pin_rejected; break;
        case '#': state = kps_reset_idle; break;
        case 'F': state = kps_idle; break;
        case 'A': state = kps_idle; break;
        case 'P': state = kps_idle; break;
      }
      break;
    case kps_installer_menu1:                        // *8 Installer Menu
      starKeyDetected = true;
      switch (keyPressed) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0': state = accepted ? kps_install_pin1 : kps_install_pin_rejected; break;
        case '*': state = accepted ? kps_star : kps_install_pin_rejected; break;
        case '#': state = kps_reset_idle; break;
        case 'F': state = kps_idle; break;
        case 'A': state = kps_idle; break;
        case 'P': state = kps_idle; break;
      }
      break;
    case kps_install_menu2:                          // *8 Installer Menu - Pin passed - check programming
      starKeyDetected = true;
      switch (keyPressed) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0': state = accepted ? kps_install_pin1 : kps_install_pin_rejected; break;
        case '*': state = accepted ? kps_star : kps_install_pin_rejected; break;
        case '#': state = kps_reset_idle; break;
        case 'F': state = kps_idle; break;
        case 'A': state = kps_idle; break;
        case 'P': state = kps_idle; break;
      }
      break;
    case kps_install_pin1:
    case kps_install_pin2:
    case kps_install_pin3:
    case kps_install_pin4:
      starKeyDetected = true;
      switch (keyPressed) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
          switch (state) {
            case kps_install_pin1: state = accepted ? kps_install_pin2 : kps_install_pin_rejected; break;
            case kps_install_pin2: state = accepted ? kps_install_pin3 : kps_install_pin_rejected; break;
            case kps_install_pin3: state = accepted ? kps_install_pin4 : kps_install_pin_rejected; break;
            case kps_install_pin4: state = accepted ? kps_install_pin_accepted : kps_install_pin_rejected; break;
          }
          break;
        case '*': state = accepted ? kps_star : kps_install_pin_rejected; break;
        case '#': state = kps_reset_idle; break;
        case 'F': state = kps_idle; break;
        case 'A': state = kps_idle; break;
        case 'P': state = kps_idle; break;
      }
      break;
    case kps_master_pin1:
    case kps_master_pin2:
    case kps_master_pin3:
    case kps_master_pin4:
      starKeyDetected = true;
      switch (keyPressed) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
          switch (state) {
            case kps_master_pin1: state = accepted ? kps_master_pin2 : kps_master_pin_rejected; break;
            case kps_master_pin2: state = accepted ? kps_master_pin3 : kps_master_pin_rejected; break;
            case kps_master_pin3: state = accepted ? kps_master_pin4 : kps_master_pin_rejected; break;
            case kps_master_pin4: state = accepted ? kps_master_pin_accepted : kps_master_pin_rejected; break;
          }
          break;
        case '*': state = accepted ? kps_star : kps_master_pin_rejected; break;
        case '#': state = kps_reset_idle; break;
        case 'F': state = kps_idle; break;
        case 'A': state = kps_idle; break;
        case 'P': state = kps_idle; break;
      }
      break;
  }
  //stream->println(state);

  // post state transition status true up
  switch(state) {
    case kps_trouble_read:
      //status[0] = 0xC6;                              // *2: Zone fault menu
      status[0] = 0xA1;                              // *2: Trouble
      break;
    case kps_installer_menu1: // Loops until "Keypad lockout" (0x10), "Invalid access code" (0x8F), or "*8 Main Menu" (0xE4)
    case kps_install_pin1:
      status[0] = 0xB7;                              // *8 Enter installer code
      break;
    case kps_install_pin2:
    case kps_install_pin3:
    case kps_install_pin4:
      status[0] = 0xF7;                              // *8: Installer programming, 2 digits
      break;
    case kps_pin_rejected:
    case kps_master_pin_rejected:
    case kps_install_pin_rejected:
      status[0] = 0x8F;                              // Invalid access code
      break;
    case kps_install_pin_accepted:
      status[0] = 0xE4;                              // *8 Main Menu
      state = kps_install_menu2;
      break;
    case kps_prog_pin_accepted:
      status[0] = 0xF8;                              // Entered keypad programming, code is valid
      break;
    case kps_prog_pin_rejected:
      status[0] = 0x7F;                              // Long Beep
      break;
    case kps_reset_idle:
      lightBlink = memoryBlink = armedBlink = bypassBlink = troubleBlink = starKeyDetected = false;
      state = kps_idle;
      // fall thru to set status
    default:
      status[0] = 0x01;                              // Partition ready -- should allow all other processing to continue
      break;
  }
}


/***********************************************************************************************
 * Write Characters as keypad
 ***********************************************************************************************/

// Sets up writes for a single key
void dscClassicInterface::write(const char receivedKey) {

  // Blocks if a previous write is in progress
  while(writeKeyPending || writeKeysPending) {
    loop();
    #if defined(ESP8266)
    yield();
    #endif
  }

  if (strlen(accessCodeStay) < 4) {
    setWriteKey(receivedKey);
  }
  else {
    switch(receivedKey) {
      case 's':
      case 'S': write(accessCodeStay); break;
      case 'w':
      case 'W': write(accessCodeAway); break;
      case 'n':
      case 'N': write(accessCodeNight); break;
      default: setWriteKey(receivedKey);
    }
  }
}


// Sets up writes for multiple keys sent as a char array
void dscClassicInterface::write(const char *receivedKeys, bool blockingWrite) {
  byte len = strlen(receivedKeys);

  if (len == 1) {
    write(receivedKeys[0]);
    return;
  }

  // Blocks if a previous write is in progress
  while(writeKeyPending || writeKeysPending) {
    loop();
    #if defined(ESP8266)
    yield();
    #endif
  }

  // strlen > 1 so 1 char always exists and is not null, 1 char string would already send, so should be > 2
  writeKeysPending = true;
  writeReady = false;
  writeKeys(receivedKeys, len);

  // Optionally blocks until the write is complete, necessary if the received keys char array is ephemeral
  if (blockingWrite) {
    while (writeKeysPending) {
      loop();
      #if defined(ESP8266)
      yield();
      #endif
    }
  }
}


// Writes multiple keys from a char array
void dscClassicInterface::writeKeys(const char *writeKeysStr, byte len) {
  static byte writeCounter = 0;
  static byte writeLen = 0;
  static const char *writeStr = NULL;

  if (!writeCounter) {
    if (writeKeysStr != NULL && len) {
      writeLen = len;
      writeStr = writeKeysStr;
    }
  }

  if (writeStr != NULL && writeKeysPending && writeCounter < writeLen) {
    if (setWriteKey(writeStr[writeCounter])) {
      writeCounter++;
      if (writeCounter >= writeLen) {
        writeKeysPending = false;
        writeCounter = 0;
        writeStr = NULL;
      }
    }
  }
}


// Specifies the key value to be written by dscClockInterrupt() and selects the write partition.  This includes a 500ms
// delay after alarm keys to resolve errors when additional keys are sent immediately after alarm keys.
bool dscClassicInterface::setWriteKey(const char receivedKey) {
  // Sets the binary to write for virtual keypad keys
  static unsigned long previousTime;
  unsigned long now = millis();

  if (!writeKeyPending  && (now - previousTime > 500 || now <= 500)) {
    bool validKey = true;

    // Sets binary for virtual keypad keys
    switch (receivedKey) {
      case '0': writeKey = 0xD7; break;
      case '1': writeKey = 0xBE; break;
      case '2': writeKey = 0xDE; break;
      case '3': writeKey = 0xEE; break;
      case '4': writeKey = 0xBD; break;
      case '5': writeKey = 0xDD; break;
      case '6': writeKey = 0xED; break;
      case '7': writeKey = 0xBB; break;
      case '8': writeKey = 0xDB; break;
      case '9': writeKey = 0xEB; break;
      case '*': writeKey = 0xB7; break;
      case '#': writeKey = 0xE7; break;
      case 'F':
      case 'f': writeKey = 0x3F; writeAlarm = true; break;          // Keypad fire alarm
      case 'A':
      case 'a': writeKey = 0x5F; writeAlarm = true; break;          // Keypad auxiliary alarm
      case 'P':
      case 'p': writeKey = 0x6F; writeAlarm = true; break;          // Keypad panic alarm
      default: {
        validKey = false;
        break;
      }
    }

    if (writeAlarm) previousTime = now;       // Sets a marker to time writes after keypad alarm keys
    if (validKey) {
      writeKeyPending = true;                 // Sets a flag indicating that a write is pending, cleared by dscClockInterrupt()
      writeReady = false;
    }
	return true;
  }
  return false;
}

/***********************************************************************************************
 * Print stream output Functions
 ***********************************************************************************************/

/*
 *  printPanelMessage() decodes and prints panel data from
 *  panelData[] and pc16Data[]
 */
void dscClassicInterface::printPanelMessage() {
  // context based output
  switch(status[0]) {
    case 0xA1:                                       // *2: Trouble
      printTroubleStatus();
      printPC16Status();
      break;
    default:
      printLightStatus();
      printPC16Status();
      printOpenZones();
      printAlarmZones();
      break;
  }
  printPanelState();
}


void dscClassicInterface::printPanelState() {
  stream->print(F("| Panel State: "));
  switch (status[0]) {
    case 0x01: stream->print(F("Partition ready")); break;
    case 0x02: stream->print(F("Stay zones open")); break;
    case 0x03: stream->print(F("Zones open")); break;
    case 0x04: stream->print(F("Armed: Stay")); break;
    case 0x05: stream->print(F("Armed: Away")); break;
    case 0x06: stream->print(F("Armed: Stay with no entry delay")); break;
    case 0x07: stream->print(F("Failed to arm")); break;
    case 0x08: stream->print(F("Exit delay in progress")); break;
    case 0x09: stream->print(F("Arming: No entry delay")); break;
    case 0x0B: stream->print(F("Quick exit in progress")); break;
    case 0x0C: stream->print(F("Entry delay in progress")); break;
    case 0x0D: stream->print(F("Entry delay after alarm")); break;
    case 0x0E: stream->print(F("Function not available")); break;
    // case 0x10: stream->print(F("Keypad lockout")); break;
    case 0x11: stream->print(F("Partition in alarm")); break;
    // case 0x12: stream->print(F("Battery check in progress")); break;
    case 0x14: stream->print(F("Auto-arm in progress")); break;
    case 0x15: stream->print(F("Arming with bypassed zones")); break;
    case 0x16: stream->print(F("Armed: Away with no entry delay")); break;
    case 0x19: stream->print(F("Disarmed: Alarm memory")); break;
    case 0x22: stream->print(F("Disarmed: Recent closing")); break;
    case 0x2F: stream->print(F("Keypad LCD test")); break;
    case 0x33: stream->print(F("Command output in progress")); break;
    case 0x3D: stream->print(F("Disarmed: Alarm memory")); break;
    case 0x3E: stream->print(F("Partition disarmed")); break;
    case 0x17: // Keypad blanking with trouble light flashing
    case 0x40: stream->print(F("Keypad blanking")); break;
    case 0x8A: stream->print(F("Activate stay/away zones")); break;
    case 0x8B: stream->print(F("Quick exit")); break;
    case 0x8E: stream->print(F("Function not available")); break;
    case 0x8F: stream->print(F("Invalid access code")); break;
    case 0x9E: stream->print(F("Enter * function key")); break;
    case 0x9F: stream->print(F("Enter access code")); break;
    case 0xA0: stream->print(F("*1: Zone bypass")); break;
    case 0xA1: stream->print(F("*2: Trouble")); break;
    case 0xA2: stream->print(F("*3: Alarm memory")); break;
    case 0xA3: stream->print(F("Door chime enabled")); break;
    case 0xA4: stream->print(F("Door chime disabled")); break;
    case 0xA5: stream->print(F("Enter master code")); break;
    case 0xA6: stream->print(F("*5: Access codes")); break;
    case 0xA7: stream->print(F("*5: Enter 4-digit code")); break;
    case 0xA9: stream->print(F("*6: User functions")); break;
    case 0xAA: stream->print(F("*6: Time and date")); break;
    case 0xAB: stream->print(F("*6: Auto-arm time")); break;
    case 0xAC: stream->print(F("*6: Auto-arm enabled")); break;
    case 0xAD: stream->print(F("*6: Auto-arm disabled")); break;
    case 0xAF: stream->print(F("*6: System test")); break;
    case 0xB0: stream->print(F("*6: Enable DLS")); break;
    case 0xB2:
    case 0xB3: stream->print(F("*7: Command output")); break;
    case 0xB7: stream->print(F("Enter installer code")); break;
    case 0xB8: stream->print(F("Enter * function key while armed")); break;
    case 0xB9: stream->print(F("*2: Zone tamper menu")); break;
    case 0xBA: stream->print(F("*2: Zones with low batteries")); break;
    case 0xBC: stream->print(F("*5: Enter 6-digit code")); break;
    case 0xBF: stream->print(F("*6: Auto-arm select day")); break;
    case 0xC6: stream->print(F("*2: Zone fault menu")); break;
    // case 0xC7: stream->print(F("Partition not available")); break;
    case 0xC8: stream->print(F("*2: Service required menu")); break;
    case 0xCD: stream->print(F("Downloading in progress")); break;
    case 0xCE: stream->print(F("Active camera monitor selection")); break;
    // case 0xD0: stream->print(F("*2: Keypads with low batteries")); break;
    // case 0xD1: stream->print(F("*2: Keyfobs with low batteries")); break;
    // case 0xD4: stream->print(F("*2: Zones with RF Delinquency")); break;
    case 0xE4: stream->print(F("*8: Installer programming, 3 digits")); decimalInput = false; break;
    // case 0xE5: stream->print(F("Keypad slot assignment")); break;
    case 0xE6: stream->print(F("Input: 2 digits")); break;
    case 0xE7: stream->print(F("Input: 3 digits")); decimalInput = true; break;
    case 0xE8: stream->print(F("Input: 4 digits")); break;
    case 0xE9: stream->print(F("Input: 5 digits")); break;
    case 0xEA: stream->print(F("Input HEX: 2 digits")); break;
    case 0xEB: stream->print(F("Input HEX: 4 digits")); break;
    case 0xEC: stream->print(F("Input HEX: 6 digits")); break;
    case 0xED: stream->print(F("Input HEX: 32 digits")); break;
    case 0xEE: stream->print(F("Input: 1 option per zone")); break;
    case 0xEF: stream->print(F("Module supervision field")); break;
    case 0xF0: stream->print(F("Function key 1")); break;
    case 0xF1: stream->print(F("Function key 2")); break;
    case 0xF2: stream->print(F("Function key 3")); break;
    case 0xF3: stream->print(F("Function key 4")); break;
    case 0xF4: stream->print(F("Function key 5")); break;
    case 0xF5: stream->print(F("Wireless module placement test")); break;
    case 0xF6: stream->print(F("Activate device for test")); break;
    case 0xF7: stream->print(F("*8: Installer programming, 2 digits")); decimalInput = false; break;
    case 0xF8: stream->print(F("Keypad programming")); break;
    case 0xFA: stream->print(F("Input: 6 digits")); break;
    default:
      stream->print(F("Unknown data: 0x"));
      if (status[0] < 10) stream->print('0');
      stream->print(status[0], HEX);
      break;
  }
}

void dscClassicInterface::printModuleDigit(byte keyPressed, bool accepted) {
  stream->print(F("printModuleDigit: "));

  switch (keyPressed) {
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '0':
    case '*':
    case '#': stream->print((char)(keyPressed)); break;
    case 'F': stream->print(F("Fire alarm")); break;
    case 'A': stream->print(F("Aux alarm")); break;
    case 'P': stream->print(F("Panic alarm")); break;
  }
  stream->println(accepted ? F(" OK") : F(" NO"));
}


void dscClassicInterface::printBits(byte data) {
  for (byte mask = 0x80; mask; mask >>= 1) {
    stream->print((mask & data) > 0);
  }
}


void dscClassicInterface::printLightStatus() {
  stream->print(F("Lights: "));
  byte data = panelData[0];
  if (data) {
    if (pdReadyLit(data))   stream->print(F("Ready "));
    if (pdArmedLit(data))   stream->print(armedBlink ? F("Armed* ") : F("Armed  "));
    if (pdMemoryLit(data))  stream->print(memoryBlink ? F("Memory* ") : F("Memory  "));
    if (pdBypassLit(data))  stream->print(bypassBlink ? F("Bypass* ") : F("Bypass  "));
    if (pdTroubleLit(data)) stream->print(troubleBlink ? F("Trouble* ") : F("Trouble  "));
    if (pdProgramLit(data)) stream->print(F("Program  "));
    if (pdFireLit(data))    stream->print(F("Fire  "));
    if (pdBeepOn(data))     stream->print(beepLong ? F("Beep+ ") : F("Beep  "));
  }
  else stream->print(F("none "));
}


void dscClassicInterface::printPC16Status() {
  stream->print(F("| Status: "));
  byte data = pc16Data[0];
  if (data) {
    if (bitRead(data, 0)) stream->print(F("Trouble "));
    if (bitRead(data, 1)) stream->print(F("Bypassed zones "));
    if (bitRead(data, 2)) stream->print(F("Armed (side A) "));
    if (bitRead(data, 3)) stream->print(F("Armed (side B) "));
    if (bitRead(data, 4)) stream->print(F("Keypad Panic alarm "));
    if (bitRead(data, 5)) stream->print(F("Keypad Aux alarm "));
    if (bitRead(data, 6)) stream->print(F("Keypad Fire alarm "));
    if (bitRead(data, 7)) stream->print(F("Alarm "));
  }
  else stream->print(F("none "));
}


void dscClassicInterface::printTroubleStatus() {
  stream->print(F("Touble Codes: "));
  // Zone 1-8 Lights indicate failure modes
  byte zones = panelData[1];
  for (byte bit = 0; bit < 8; bit++) {
    if (bitRead(zones, bit)) {
      switch(bit) {
        case 0: stream->print(F("Backup Battery fail "));  break; // Backup Battery failed
        case 1: stream->print(F("A.C. Power fail "));      break; // A.C. Power Failure
        case 2: stream->print(F("Day loop fail "));        break; // Day loop failed
        case 3: stream->print(F("Telephone Disconnect ")); break; // Telephone line failed
        case 4: stream->print(F("Communicator fail "));    break; // Communicator failed
        case 5: stream->print(F("Bell fail "));            break; // Bell circuit failed
        case 6: stream->print(F("Smoke Detector fail "));  break; // Smoke detector circuit failed
        case 7: stream->print(F("Clock needs reset "));    break; // Clock needs resetting
      }
    }
  }
}

void dscClassicInterface::printOpenZones() {
  stream->print(F("| Zones open: "));
  bool noZone = true;
  byte z = 1;
  for (byte i = 1; i < panelByteCount; i++) { // use the actual read to determine which zones to display
    byte data = panelData[i];
    if (data) {
      noZone = false;
      for (byte bit = 0; bit < 8; bit++) {
        if (bitRead(data, bit)) {
          stream->print(z);
          stream->print(' ');
        }
        z++;
      }
    } else z += 8;
  }
  if (noZone) stream->print(F("none "));
}


void dscClassicInterface::printAlarmZones() {
  byte data = pc16Data[1];
  if (data & 0x7F) {
    stream->print(F("| Zone alarm: "));
    for (byte bit = 0; bit < 7; bit++) {
      // PC1500: z1 z2 z3 z4 z5   z6
      // PC2500: z1 z2 z3 z4 z5-8 /
      // PC3000: z1 z2 z3 z4 z5-8 z9-16
      // TODO: support the multi zone properly
      if (bitRead(data, bit)) {
        stream->print(bit+1);
        stream->print(' ');
      }
    }
  }
  if (bitRead(data, 7)) {
    stream->print(F("| Fire alarm "));
  }
}


// Processes keypad and module notifications and responses to panel queries
void dscClassicInterface::printModuleMessage() {
  char keyPressed = decodeModuleDigit(moduleData[0]);

  stream->print(F("[Keypad] "));
  switch (keyPressed) {
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '0':
        if (hideKeypadDigits) stream->print(F("[Digit]"));
        else stream->print(keyPressed);
      break;
    case '*': stream->print('*'); break;
    case '#': stream->print('#'); break;
    case 'F': stream->print(F("Fire alarm")); break;
    case 'A': stream->print(F("Aux alarm")); break;
    case 'P': stream->print(F("Panic alarm")); break;
  }
}


// Prints the panel message as binary with an optional parameter to print spaces between bytes
void dscClassicInterface::printPanelBinary(bool printSpaces) {
  for (byte panelByte = 0; panelByte < panelByteCount; panelByte++) {
    printBits(panelData[panelByte]);
	if (printSpaces) stream->print(' ');
  }

  printBits(pc16Data[0]);
  if (printSpaces) stream->print(' ');
  printBits(pc16Data[1]);
}

// Prints the panel message as binary with an optional parameter to print spaces between bytes
void dscClassicInterface::printModuleBinary(bool printSpaces) {
  byte keyPressed = decodeModuleDigit(moduleData[0]);

  switch (keyPressed) {
    case '0': // missing in original?
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if (hideKeypadDigits) stream->print(F("........"));
	  else printBits(moduleData[0]);
      break;
    default: printBits(moduleData[0]); break;
  }
}


// Prints the panel command as hex
void dscClassicInterface::printPanelCommand() {
  stream->print(F("Panel"));
}

#undef pdReadyLit
#undef pdArmedLit
#undef pdMemoryLit
#undef pdBypassLit
#undef pdTroubleLit
#undef pdProgramLit
#undef pdFireLit
#undef pdBeepOn

/***********************************************************************************************
 * Interrupt Handlers
 ***********************************************************************************************/

// Called as an interrupt when the DSC clock changes to write data for virtual keypad and setup timers to read
// data after an interval.
#if defined(__AVR__)
void dscClassicInterface::dscClockInterrupt() {
#elif defined(ESP8266)
void ICACHE_RAM_ATTR dscClassicInterface::dscClockInterrupt() {
#elif defined(ESP32)
void IRAM_ATTR dscClassicInterface::dscClockInterrupt() {
#endif

  // Data sent from the panel and keypads/modules has latency after a clock change (observed up to 160us for
  // keypad data).  The following sets up a timer for each platform that will call dscDataInterrupt() in
  // 250us to read the data line.

  // AVR Timer1 calls dscDataInterrupt() via ISR(TIMER1_OVF_vect) when the Timer1 counter overflows
  #if defined(__AVR__)
  TCNT1=61535;            // Timer1 counter start value, overflows at 65535 in 250us
  TCCR1B |= (1 << CS10);  // Sets the prescaler to 1

  // esp8266 timer1 calls dscDataInterrupt() in 250us
  #elif defined(ESP8266)
  timer1_write(1250);

  // esp32 timer1 calls dscDataInterrupt() in 250us
  #elif defined(ESP32)
  timerStart(timer1);
  portENTER_CRITICAL(&timer1Mux);
  #endif

  static unsigned long previousClockHighTime;
  if (digitalRead(dscClockPin) == HIGH) {
    if (virtualKeypad) digitalWrite(dscWritePin, LOW);  // Restores the data line after a virtual keypad write
    previousClockHighTime = micros();
  }

  else {
    clockHighTime = micros() - previousClockHighTime;  // Tracks the clock high time to find the reset between commands

    // Virtual keypad
    if (virtualKeypad) {
      static bool writeStart = false;

      // Writes a regular key - to first byte of new clock cycle (isrPanelBitTotal < 8)
      if (writeKeyPending && !writeKeyWait) {
        // TODO: use bitRead()
        if (clockHighTime > 2000) {
          if (!((writeKey >> 7) & 0x01)) digitalWrite(dscWritePin, HIGH);
          writeStart = true;
        }
        else if (writeStart && isrPanelBitTotal <= 7) {
          if (!((writeKey >> (7 - isrPanelBitCount)) & 0x01)) digitalWrite(dscWritePin, HIGH);
          if (isrPanelBitTotal == 7) {
            writeKeyWait = true;
            writeKeyPending = false;
            writeCompleteTime = millis();
            writeStart = false;
          }
        }
      }
    }
  }
  #if defined(ESP32)
  portEXIT_CRITICAL(&timer1Mux);
  #endif
}


// Interrupt function called by AVR Timer1, esp8266 timer1, and esp32 timer1 after 250us to read the data line
#if defined(__AVR__)
void dscClassicInterface::dscDataInterrupt() {
#elif defined(ESP8266)
void ICACHE_RAM_ATTR dscClassicInterface::dscDataInterrupt() {
#elif defined(ESP32)
void IRAM_ATTR dscClassicInterface::dscDataInterrupt() {
  timerStop(timer1);
  portENTER_CRITICAL(&timer1Mux);
#endif

  // Stops the copying of data to the ouput buffer
  static byte minReadSize = 2; // 2 bytes - DSC1500 Series
  static bool dupData = true;

  // Panel sends data while the clock is high
  if (digitalRead(dscClockPin) == HIGH) {
    // Stops processing Keybus data at the dscReadSize limit
    if (isrPanelByteCount < dscReadSize) {
      if (isrPanelBitCount < 8) {
        // Data was captured in each byte by shifting left by 1 bit and writing to bit 0
        // converted to shifting the bit to the end of the byte to make all other operations simpler
        isrPanelData[isrPanelByteCount] |= ((digitalRead(dscReadPin) == HIGH) << isrPanelBitCount);
        isrPC16Data[isrPanelByteCount]  |= ((digitalRead(dscPC16Pin) == HIGH) << isrPanelBitCount);
        isrPanelBitCount++;
      }

      // Byte is complete, set the counters for the next byte
      if (isrPanelBitCount > 7) {
        static byte previousPanelData[dscReadSize];
        static byte previousPC16Data[dscReadSize];
        isrPanelBitCount = 0;

        if(isrPanelData[isrPanelByteCount] != previousPanelData[isrPanelByteCount]) {
            dupData = false;
            previousPanelData[isrPanelByteCount] = isrPanelData[isrPanelByteCount];
        }
        if(isrPC16Data[isrPanelByteCount] != previousPC16Data[isrPanelByteCount]) {
            dupData = false;
            previousPC16Data[isrPanelByteCount] = isrPC16Data[isrPanelByteCount];
        }
        isrPanelByteCount++;
      }

      isrPanelBitTotal++;
    }
  }

  // Keypads and modules send data while the clock is low
  else {
    static bool moduleDataDetected = false;

    // Saves data and resets counters after the clock cycle is complete (high for at least 1ms)
    if (clockHighTime > 2000) {
      keybusTime = millis();

      // Skips incomplete data and redundant data
      byte bufferNext = (bufferHead + 1) % dscBufferSize;

      if (bufferNext == bufferTail) bufferOverflow = true;
      // Stores new panel data in the panel buffer
      else if(!dupData) {
        // ensure we got a minimum of the status and Zone 1 before copying data
        if (isrPanelByteCount >= minReadSize) {
          // on PC3000 is a byte larger - will adjust up automatically
          if (minReadSize < isrPanelByteCount && minReadSize < dscReadSize) minReadSize++;

          for (byte i = 0; i < isrPanelByteCount; i++) {
            byte r = isrPanelByteCount-i-1;
            // reverse the order of the data to make all other operations simpler
            // it stablizes the ordering as zones add on like PC3000 [lights][zone1][zone2]...
            panelBuffer[bufferHead][i] = isrPanelData[r];
            pc16Buffer[bufferHead][i] = isrPC16Data[r];
          }
          panelBufferBitCount[bufferHead] = isrPanelBitTotal;
          panelBufferByteCount[bufferHead] = isrPanelByteCount;
          panelBufferLength++;
          bufferHead = bufferNext;
        }
      }

      // Stores new keypad and module data - this data is not buffered
      if (moduleDataDetected) {
        moduleDataDetected = false;
        moduleDataCaptured = true;  // Sets a flag for handleModule()
        moduleData[0] = isrModuleData[0];
        moduleBitCount = isrModuleBitTotal;
        moduleByteCount = isrModuleByteCount;
      }

      // Resets the keypad and module capture data and counters
      // Resets the panel capture data and counters
      for (byte i = 0; i < dscReadSize; i++) isrModuleData[i] = isrPanelData[i] = isrPC16Data[i] = 0;
      isrPanelBitTotal = 0;
      isrPanelBitCount = 0;
      isrPanelByteCount = 0;
      isrModuleBitTotal = 0;
      isrModuleBitCount = 0;
      isrModuleByteCount = 0;
      dupData = true;
    }

    // Keypad and module data is not buffered and skipped if the panel data buffer is filling
    // Only capture the first byte, no other data seems to be present
    if (isrModuleByteCount < 1 && panelBufferLength <= (dscBufferSize/3)) {

      // Data is captured in each byte by shifting left by 1 bit and writing to bit 0
      if (isrModuleBitCount < 8) {
        isrModuleData[isrModuleByteCount] = (isrModuleData[isrModuleByteCount] << 1) | (digitalRead(dscReadPin) == HIGH);
        isrModuleBitCount++;
      }

      // Byte is complete, set the counters for the next byte
      if (isrModuleBitCount > 7) {
        moduleDataDetected |= (isrModuleData[isrModuleByteCount] != 0xFF); // Keypads and modules send data by pulling the data line low
        isrModuleBitCount = 0;
        isrModuleByteCount++;
      }

      isrModuleBitTotal++;
    }
  }
  #if defined(ESP32)
  portEXIT_CRITICAL(&timer1Mux);
  #endif
}
