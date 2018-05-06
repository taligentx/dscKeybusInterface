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

byte dscKeybusInterface::dscClockPin;
byte dscKeybusInterface::dscReadPin;
byte dscKeybusInterface::dscWritePin;
char dscKeybusInterface::writeKey;
bool dscKeybusInterface::virtualKeypad;
bool dscKeybusInterface::processKeypadData;
volatile bool dscKeybusInterface::writeAlarm;
volatile bool dscKeybusInterface::writeAsterisk;
volatile bool dscKeybusInterface::wroteAsterisk;
volatile bool dscKeybusInterface::writeReady;
volatile bool dscKeybusInterface::dataComplete;
volatile bool dscKeybusInterface::dataOverflow;
volatile byte dscKeybusInterface::panelData[];
volatile byte dscKeybusInterface::panelByteCount;
volatile byte dscKeybusInterface::panelBitCount;
volatile byte dscKeybusInterface::isrPanelData[dscReadSize];
volatile byte dscKeybusInterface::isrPanelByteCount;
volatile byte dscKeybusInterface::isrPanelBitCount;
volatile byte dscKeybusInterface::isrPanelBitTotal;
volatile byte dscKeybusInterface::keypadData[];
volatile bool dscKeybusInterface::keypadDataCaptured;
volatile byte dscKeybusInterface::keypadByteCount;
volatile byte dscKeybusInterface::keypadBitCount;
volatile byte dscKeybusInterface::isrKeypadData[dscReadSize];
volatile byte dscKeybusInterface::isrKeypadByteCount;
volatile byte dscKeybusInterface::isrKeypadBitCount;
volatile byte dscKeybusInterface::isrKeypadBitTotal;
volatile unsigned long dscKeybusInterface::clockHighTime;


dscKeybusInterface::dscKeybusInterface(byte setClockPin, byte setReadPin, byte setWritePin) {
  dscClockPin = setClockPin;
  dscReadPin = setReadPin;
  dscWritePin = setWritePin;
  if (dscWritePin != 255) virtualKeypad = true;
  writeReady = true;
  processRedundantData = false;
  displayTrailingBits = false;
  processKeypadData = false;
}


void dscKeybusInterface::begin(Stream &_stream) {
  pinMode(dscClockPin, INPUT);
  pinMode(dscReadPin, INPUT);
  if (virtualKeypad) pinMode(dscWritePin, OUTPUT);
  stream = &_stream;

  // Platform-specific timers trigger a read of the data line 250us after the keybus clock changes

  // Arduino Timer2 calls ISR(TIMER2_OVF_vect) from dscClockInterrupt() and is disabled in the ISR for a one-shot timer
  #if defined(__AVR__)
  TCCR2A = 0;
  TCCR2B = 0;
  TIMSK2 |= (1 << TOIE2);

  // esp8266 timer1 calls dscDataInterrupt() from dscClockInterrupt() as a one-shot timer
  #elif defined(ESP8266)
  timer1_isr_init();
  timer1_attachInterrupt(dscDataInterrupt);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  #endif

  attachInterrupt(digitalPinToInterrupt(dscClockPin), dscClockInterrupt, CHANGE);
}


// Sets up writes if multiple keys are sent as a char array
void dscKeybusInterface::write(const char * receivedKeys) {
  writeKeysArray = receivedKeys;
  if (writeKeysArray[0] != '\0') writeKeysPending = true;
  writeKeys(writeKeysArray);
}


void dscKeybusInterface::writeKeys(const char * writeKeysArray) {
  static byte writeCounter = 0;
  if (writeKeysPending && writeReady && writeCounter < strlen(writeKeysArray)) {
    if (writeKeysArray[writeCounter] != '\0') {
      write(writeKeysArray[writeCounter]);
      writeCounter++;
      if (writeKeysArray[writeCounter] == '\0') {
        writeKeysPending = false;
        writeCounter = 0;
      }
    }
  }
}


// Specifies the key value to be written by dscClockInterrupt().  This includes a 500ms delay after alarm keys
// to resolve errors when additional keys are sent immediately after alarm keys.
void dscKeybusInterface::write(const char receivedKey) {
  static unsigned long previousTime;
  if (writeReady && millis() - previousTime > 500) {
    bool validKey = true;
    switch (receivedKey) {
      case '0': writeKey = 0x00; break;
      case '1': writeKey = 0x05; break;
      case '2': writeKey = 0x0A; break;
      case '3': writeKey = 0x0F; break;
      case '4': writeKey = 0x11; break;
      case '5': writeKey = 0x16; break;
      case '6': writeKey = 0x1B; break;
      case '7': writeKey = 0x1C; break;
      case '8': writeKey = 0x22; break;
      case '9': writeKey = 0x27; break;
      case '*': writeKey = 0x28; writeAsterisk = true; break;
      case '#': writeKey = 0x2D; break;
      case 'F':
      case 'f': writeKey = 0x77; writeAlarm = true; break;  // Keypad fire alarm
      case 's':
      case 'S': writeKey = 0xAF; break;                     // Arm stay
      case 'w':
      case 'W': writeKey = 0xB1; break;                     // Arm away
      case 'A':
      case 'a': writeKey = 0xBB; writeAlarm = true; break;  // Keypad auxiliary alarm
      case 'c':
      case 'C': writeKey = 0xBB; break;                     // Door chime
      case 'r':
      case 'R': writeKey = 0xDA; break;                     // Reset
      case 'P':
      case 'p': writeKey = 0xDD; writeAlarm = true; break;  // Keypad panic alarm
      case 'x':
      case 'X': writeKey = 0xE1; break;                     // Exit
      case '>': writeKey = 0xF2; break;                     // Right arrow?
      case '<': writeKey = 0xF7; break;                     // Left arrow
      default: {
        validKey = false;
        break;
      }
    }
    if (writeAlarm) previousTime = millis();
    if (validKey) writeReady = false;
  }
}


bool dscKeybusInterface::validCRC() {
  byte byteCount = (panelBitCount - 1) / 8;
  int dataSum = 0;
  for (byte panelByte = 0; panelByte < byteCount; panelByte++) {
    if (panelByte != 1) dataSum += panelData[panelByte];
  }
  if (dataSum % 256 == panelData[byteCount]) return true;
  else return false;
}


bool dscKeybusInterface::handlePanel() {
  if (writeKeysPending) writeKeys(writeKeysArray);

  if (!dataComplete) return false;
  dataComplete = false;

  if (dataOverflow) {
    dataOverflow = false;
    return false;
  }

  if (panelBitCount < 8) return false;

  // Waits at startup for the 0x05 status command or a command with a valid CRC data to eliminate spurious data.
  static bool firstClockCycle = true;
  if (firstClockCycle) {
    if (validCRC() || panelData[0] == 0x05) firstClockCycle = false;
    else return false;
    }

  // Skips redundant data from status commands - these are sent constantly on the keybus at a high rate so they are
  // always skipped
  switch (panelData[0]) {
    static byte previousCmd05[dscReadSize];
    static byte previousCmd0A[dscReadSize];
    case 0x05:  //Status
      if (redundantPanelData(previousCmd05)) return false;
      break;

    case 0x0A:  // Status in * programming
      if (redundantPanelData(previousCmd0A)) return false;
      break;
  }

  // Skips redundant data from periodic commands - these are sent at regular intervals and skipping is a configurable
  // option and the default behavior to help see new Keybus data when decoding the protocol
  if (!processRedundantData) {
    static byte previousCmd11[dscReadSize];
    static byte previousCmd16[dscReadSize];
    static byte previousCmd27[dscReadSize];
    static byte previousCmd2D[dscReadSize];
    static byte previousCmd34[dscReadSize];
    static byte previousCmd3E[dscReadSize];
    static byte previousCmd5D[dscReadSize];
    static byte previousCmd63[dscReadSize];
    static byte previousCmdB1[dscReadSize];
    switch (panelData[0]) {
      case 0x11:  // Keypad slot query
        if (redundantPanelData(previousCmd11)) return false;
        break;

      case 0x16:  // Zone wiring
        if (redundantPanelData(previousCmd16)) return false;
        break;

      case 0x27:  // Status with zone 1-8 info
        if (redundantPanelData(previousCmd27)) return false;
        break;

      case 0x2D:  // Status with zone 9-16 info
        if (redundantPanelData(previousCmd2D)) return false;
        break;

      case 0x34:  // Status with zone 17-24 info
        if (redundantPanelData(previousCmd34)) return false;
        break;

      case 0x3E:  // Status with zone 25-32 info
        if (redundantPanelData(previousCmd3E)) return false;
        break;

      case 0x5D:  // Flashing panel lights
        if (redundantPanelData(previousCmd5D)) return false;
        break;

      case 0x63:  // Flashing panel lights?
        if (redundantPanelData(previousCmd63)) return false;
        break;

      case 0xB1:  // Enabled zones
        if (redundantPanelData(previousCmdB1)) return false;
        break;
    }
  }

  // Processes valid panel data
  switch (panelData[0]) {
    case 0x05: processPanel_0x05(); break;
    case 0x27: processPanel_0x27(); break;
    case 0x2D: processPanel_0x2D(); break;
    case 0x34: processPanel_0x34(); break;
    case 0x3E: processPanel_0x3E(); break;
    case 0xA5: processPanel_0xA5(); break;
  }

  return true;
}


bool dscKeybusInterface::redundantPanelData(byte previousCmd[]) {
  bool redundantData = true;
  byte checkedBytes = dscReadSize;
  if (panelData[0] == 0x05) checkedBytes = 6;  // Excludes the trailing bit for 0x05 that frequently flips
  for (byte i = 0; i < checkedBytes; i++) {
    if (previousCmd[i] != panelData[i]) {
      redundantData = false;
      break;
    }
  }
  if (redundantData) return true;
  else {
    for (byte i = 0; i < dscReadSize; i++) previousCmd[i] = panelData[i];
    return false;
  }
}


bool dscKeybusInterface::handleKeypad() {
  if (keypadDataCaptured) keypadDataCaptured = false;
  else return false;

  if (keypadBitCount < 8) return false;

  // Skips periodic keypad slot query responses
  if (!processRedundantData && panelData[0] == 0x11) {
    bool redundantData = true;
    byte checkedBytes = dscReadSize;
    static byte previousSlotData[dscReadSize];
    for (byte i = 0; i < checkedBytes; i++) {
      if (previousSlotData[i] != keypadData[i]) {
        redundantData = false;
        break;
      }
    }
    if (redundantData) return false;
    else {
      for (byte i = 0; i < dscReadSize; i++) previousSlotData[i] = keypadData[i];
      return true;
    }
  }
  return true;
}


// Called as an interrupt when the DSC clock changes to write data for virtual keypad and setup timers to read
// data after an interval.
#if defined(__AVR__)
void dscKeybusInterface::dscClockInterrupt() {
#elif defined(ESP8266)
void ICACHE_RAM_ATTR dscKeybusInterface::dscClockInterrupt() {
#endif

  // Data sent from the panel and keypads has latency after a clock change (observed up to 160us for keypad data).
  // The following sets up a timer for both Arduino/AVR and Arduino/esp8266 that will call dscDataInterrupt() in
  // 250us to read the data line.

  // AVR Timer2 calls dscDataInterrupt() via ISR(TIMER2_OVF_vect) when the Timer2 counter overflows
  #if defined(__AVR__)
  TCNT2=0x82;             // Timer2 counter start value, overflows at 0xFF in 250us
  TCCR2B |= (1 << CS20);  // Setting CS20 and CS21 sets the pre-scaler to 32
  TCCR2B |= (1 << CS21);

  // esp8266 timer1 calls dscDataInterrupt() directly as set in begin()
  #elif defined(ESP8266)
  // Timer duration = (clockCyclesPerMicrosecond() / 16) * microseconds
  timer1_write((clockCyclesPerMicrosecond() / 16) * 250);
  #endif


  static volatile unsigned long previousClockHighTime;
  if (digitalRead(dscClockPin) == HIGH) {
    if (virtualKeypad) digitalWrite(dscWritePin, LOW);  // Restores the data line after a virtual keypad write
    previousClockHighTime = micros();
  }

  else {
    clockHighTime = micros() - previousClockHighTime;  // Tracks the clock high time to find the reset between commands

    // Virtual keypad

    // Writes a F/A/P alarm key and repeats the key on the next immediate command from the panel (0x1C verification)
    if (virtualKeypad) {
      static volatile bool writeStart = false;
      static volatile bool writeRepeat = false;
      if ((writeAlarm && !writeReady) || writeRepeat) {
        if (isrKeypadBitTotal == 0) {
          if (!((writeKey >> 7) & 0x01)) digitalWrite(dscWritePin, HIGH);
          writeStart = true;  // Resolves a timing issue where some writes did not begin at the correct bit
        }
        else if (writeStart && isrKeypadBitTotal > 0 && isrKeypadBitTotal <= 7) {
          if (!((writeKey >> (7 - isrKeypadBitCount)) & 0x01)) digitalWrite(dscWritePin, HIGH);
          if (isrKeypadBitTotal == 7) {
            writeReady = true;
            writeStart = false;
            writeAlarm = false;
            if (!writeRepeat) writeRepeat = true;
            else writeRepeat = false;
          }
        }
      }

      // Writes a regular key unless the panel is sending a 0x11, 0xD5, or 0x4C query
      else if (!writeReady &&
               !wroteAsterisk &&
               isrPanelByteCount == 2 &&
               isrPanelData[0] != 0x11 &&
               isrPanelData[0] != 0xD5 &&
               isrPanelData[0] != 0x4C) {
        if (isrPanelBitTotal == 9) {
          if (!((writeKey >> 7) & 0x01)) digitalWrite(dscWritePin, HIGH);
          writeStart = true;  // Resolves a timing issue where some writes did not begin at the correct bit
        }
        else if (writeStart && isrPanelBitTotal > 9 && isrPanelBitTotal <= 16) {
          if (!((writeKey >> (7 - isrPanelBitCount)) & 0x01)) digitalWrite(dscWritePin, HIGH);
          if (isrPanelBitTotal == 16) {
            if (writeAsterisk) wroteAsterisk = true;  // Delays writing after pressing '*' until the panel is ready
            else writeReady = true;
            writeStart = false;
          }
        }
      }
    }
  }
}


// Interrupt function called after 250us by dscClockInterrupt() using AVR Timer2, disables the timer and calls
// dscDataInterrupt() to read the data line
#if defined(__AVR__)
ISR(TIMER2_OVF_vect) {
  TCCR2B = 0;  // Disables Timer2
  dscKeybusInterface::dscDataInterrupt();
}
#endif


// Interrupt function called by AVR Timer2 and esp8266 timer1 after 250us to read the data line
#if defined(__AVR__)
void dscKeybusInterface::dscDataInterrupt() {
#elif defined(ESP8266)
void ICACHE_RAM_ATTR dscKeybusInterface::dscDataInterrupt() {
#endif

  // Processes panel data while the clock is high
  if (digitalRead(dscClockPin) == HIGH) {

    if (isrPanelByteCount >= dscReadSize) {
      dataOverflow = true;
    }
    else {
      if (isrPanelBitCount < 8)
        isrPanelData[isrPanelByteCount] <<= 1;

      if (digitalRead(dscReadPin) == HIGH)
        if (isrPanelBitCount < 8)
          isrPanelData[isrPanelByteCount] |= 1;

      // Stores the stop bit by itself in byte 1 - this aligns the Keybus bytes with panelData[] bytes
      if (isrPanelBitTotal == 8) {
        isrPanelBitCount = 0;
        isrPanelByteCount++;
      }
      else if (isrPanelBitCount < 7)
        isrPanelBitCount++;
      else {
        isrPanelBitCount = 0;
        isrPanelByteCount++;
      }

      isrPanelBitTotal++;
    }
  }

  // Processes keypad data while the clock is low
  else {
    static volatile bool keypadDataDetected = false;
    if (processKeypadData && isrKeypadByteCount < dscReadSize) {
      if (isrKeypadBitCount < 8)
        isrKeypadData[isrKeypadByteCount] <<= 1;

      if (digitalRead(dscReadPin) == HIGH) {
        if (isrKeypadBitCount < 8)
          isrKeypadData[isrKeypadByteCount] |= 1;
      }
      else keypadDataDetected = true;

      // Stores the stop bit by itself in byte 1 - this aligns the Keybus bytes with keypadData[] bytes
      if (isrKeypadBitTotal == 7) {
        isrKeypadData[1] = 1;  // Sets the stop bit manually to 1 in byte 1
        isrKeypadBitCount = 0;
        isrKeypadByteCount += 2;
      }
      else if (isrKeypadBitCount < 7)
        isrKeypadBitCount++;
      else {
        isrKeypadBitCount = 0;
        isrKeypadByteCount++;
      }

      isrKeypadBitTotal++;
    }

    // Saves data and resets counters after the clock cycle is complete (high for at least 1ms)
    if (clockHighTime > 1000) {

      for (byte i = 0; i < dscReadSize; i++) panelData[i] = isrPanelData[i];
      for (byte i = 0; i < dscReadSize; i++) isrPanelData[i] = 0;
      panelBitCount = isrPanelBitTotal;
      panelByteCount = isrPanelByteCount;
      isrPanelBitTotal = 0;
      isrPanelBitCount = 0;
      isrPanelByteCount = 0;

      if (processKeypadData) {
        for (byte i = 0; i < dscReadSize; i++) keypadData[i] = isrKeypadData[i];
        for (byte i = 0; i < dscReadSize; i++) isrKeypadData[i] = 0;
        keypadBitCount = isrKeypadBitTotal;
        keypadByteCount = isrKeypadByteCount;
        isrKeypadBitTotal = 0;
        isrKeypadBitCount = 0;
        isrKeypadByteCount = 0;
      }

      dataComplete = true;
      if (keypadDataDetected) {
        keypadDataDetected = false;
        keypadDataCaptured = true;
      }
    }
  }
}