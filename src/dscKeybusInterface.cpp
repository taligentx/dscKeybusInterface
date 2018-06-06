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
byte dscKeybusInterface::writePartition;
bool dscKeybusInterface::virtualKeypad;
bool dscKeybusInterface::processKeypadData;
byte dscKeybusInterface::panelData[dscReadSize];
byte dscKeybusInterface::panelByteCount;
byte dscKeybusInterface::panelBitCount;
volatile bool dscKeybusInterface::writeReady;
volatile bool dscKeybusInterface::dataOverflow;
volatile byte dscKeybusInterface::keybusData[dscReadSize];
volatile bool dscKeybusInterface::keybusDataCaptured;
volatile byte dscKeybusInterface::keybusByteCount;
volatile byte dscKeybusInterface::keybusBitCount;
volatile bool dscKeybusInterface::writeAlarm;
volatile bool dscKeybusInterface::writeAsterisk;
volatile bool dscKeybusInterface::wroteAsterisk;
volatile bool dscKeybusInterface::bufferOverflow;
volatile byte dscKeybusInterface::panelBufferLength;
volatile byte dscKeybusInterface::panelBuffer[dscBufferSize][dscReadSize];
volatile byte dscKeybusInterface::panelBitCountBuffer[dscBufferSize];
volatile byte dscKeybusInterface::panelByteCountBuffer[dscBufferSize];
volatile byte dscKeybusInterface::isrPanelData[dscReadSize];
volatile byte dscKeybusInterface::isrPanelByteCount;
volatile byte dscKeybusInterface::isrPanelBitCount;
volatile byte dscKeybusInterface::isrPanelBitTotal;
volatile byte dscKeybusInterface::isrKeybusData[dscReadSize];
volatile byte dscKeybusInterface::isrKeybusByteCount;
volatile byte dscKeybusInterface::isrKeybusBitCount;
volatile byte dscKeybusInterface::isrKeybusBitTotal;
volatile byte dscKeybusInterface::currentCmd;
volatile byte dscKeybusInterface::statusCmd;
volatile unsigned long dscKeybusInterface::clockHighTime;


dscKeybusInterface::dscKeybusInterface(byte setClockPin, byte setReadPin, byte setWritePin) {
  dscClockPin = setClockPin;
  dscReadPin = setReadPin;
  dscWritePin = setWritePin;
  if (dscWritePin != 255) virtualKeypad = true;
  writeReady = true;
  processRedundantData = true;
  displayTrailingBits = false;
  processKeypadData = false;
  writePartition = 1;
}


void dscKeybusInterface::begin(Stream &_stream) {
  pinMode(dscClockPin, INPUT);
  pinMode(dscReadPin, INPUT);
  if (virtualKeypad) pinMode(dscWritePin, OUTPUT);
  stream = &_stream;

  // Platform-specific timers trigger a read of the data line 250us after the Keybus clock changes

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

  // Generates an interrupt when the Keybus clock rises or falls - requires a hardware interrupt pin on Arduino
  attachInterrupt(digitalPinToInterrupt(dscClockPin), dscClockInterrupt, CHANGE);
}


bool dscKeybusInterface::handlePanel() {

  // Writes keys when multiple keys are sent as a char array
  if (writeKeysPending) writeKeys(writeKeysArray);

  // Skips processing if the panel data buffer is empty
  if (panelBufferLength == 0) return false;

  // Copies data from the buffer to panelData[]
  static byte panelBufferIndex = 1;
  byte dataIndex = panelBufferIndex - 1;
  for (byte i = 0; i < dscReadSize; i++) panelData[i] = panelBuffer[dataIndex][i];
  panelBitCount = panelBitCountBuffer[dataIndex];
  panelByteCount = panelByteCountBuffer[dataIndex];
  panelBufferIndex++;

  // Resets counters when the buffer is cleared
  noInterrupts();
  if (panelBufferIndex > panelBufferLength) {
    panelBufferIndex = 1;
    panelBufferLength = 0;
  }
  interrupts();

  // Waits at startup for the 0x05 status command or a command with valid CRC data to eliminate spurious data.
  static bool firstClockCycle = true;
  if (firstClockCycle) {
    if (validCRC() || panelData[0] == 0x05) firstClockCycle = false;
    else return false;
  }

  static byte previousCmd0A[dscReadSize];
  static byte previousCmdE6_20[dscReadSize];
  switch (panelData[0]) {
    case 0x0A:  // Status in programming
      if (redundantPanelData(previousCmd0A, panelData)) return false;
      break;

    case 0xE6:  // Status in programming, zone lights 33-64
      if (panelData[2] == 0x20 && redundantPanelData(previousCmdE6_20, panelData)) return false;
      break;
  }

  // Skips redundant data from periodic commands sent at regular intervals, skipping is a configurable
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
    static byte previousCmdC3[dscReadSize];
    switch (panelData[0]) {
      case 0x11:  // Keypad slot query
        if (redundantPanelData(previousCmd11, panelData)) return false;
        break;

      case 0x16:  // Zone wiring
        if (redundantPanelData(previousCmd16, panelData)) return false;
        break;

      case 0x27:  // Status with zone 1-8 info
        if (redundantPanelData(previousCmd27, panelData)) return false;
        break;

      case 0x2D:  // Status with zone 9-16 info
        if (redundantPanelData(previousCmd2D, panelData)) return false;
        break;

      case 0x34:  // Status with zone 17-24 info
        if (redundantPanelData(previousCmd34, panelData)) return false;
        break;

      case 0x3E:  // Status with zone 25-32 info
        if (redundantPanelData(previousCmd3E, panelData)) return false;
        break;

      case 0x5D:  // Flash panel lights: status and zones 1-32
        if (redundantPanelData(previousCmd5D, panelData)) return false;
        break;

      case 0x63:  // Flash panel lights: status and zones 33-64
        if (redundantPanelData(previousCmd63, panelData)) return false;
        break;

      case 0xB1:  // Enabled zones 1-32
        if (redundantPanelData(previousCmdB1, panelData)) return false;
        break;

      case 0xC3:  // Unknown command
        if (redundantPanelData(previousCmdC3, panelData)) return false;
        break;
    }
  }

  // Processes valid panel data
  switch (panelData[0]) {
    case 0x05:
    case 0x1B: processPanelStatus(); break;
    case 0x27: processPanel_0x27(); break;
    case 0x2D: processPanel_0x2D(); break;
    case 0x34: processPanel_0x34(); break;
    case 0x3E: processPanel_0x3E(); break;
    case 0xA5: processPanel_0xA5(); break;
    case 0xE6: if (dscPartitions > 2) processPanel_0xE6(); break;
    case 0xEB: if (dscPartitions > 2) processPanel_0xEB(); break;
  }

  return true;
}


bool dscKeybusInterface::handleKeybus() {
  if (!keybusDataCaptured) return false;
  keybusDataCaptured = false;

  if (keybusBitCount < 8) return false;

  // Skips periodic keypad slot query responses
  if (!processRedundantData && currentCmd == 0x11) {
    bool redundantData = true;
    byte checkedBytes = dscReadSize;
    static byte previousSlotData[dscReadSize];
    for (byte i = 0; i < checkedBytes; i++) {
      if (previousSlotData[i] != keybusData[i]) {
        redundantData = false;
        break;
      }
    }
    if (redundantData) return false;
    else {
      for (byte i = 0; i < dscReadSize; i++) previousSlotData[i] = keybusData[i];
      return true;
    }
  }

  // Determines if a keybus message is a response to a panel command
  switch (currentCmd) {
    case 0x11:
    case 0x28:
    case 0xD5: queryResponse = true; break;
    default: queryResponse = false; break;
  }

  return true;
}


// Sets up writes if multiple keys are sent as a char array
void dscKeybusInterface::write(const char * receivedKeys) {
  writeKeysArray = receivedKeys;
  if (writeKeysArray[0] != '\0') writeKeysPending = true;
  writeKeys(writeKeysArray);
}


// Writes multiple keys from a char array
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
  static bool setPartition;
  if (writeReady && millis() - previousTime > 500) {
    bool validKey = true;
    switch (receivedKey) {
      case '/': setPartition = true; validKey = false; break;
      case '0': writeKey = 0x00; break;
      case '1': {
        if (setPartition) {
          writePartition = 1;
          setPartition = false;
          validKey = false;
        }
        else writeKey = 0x05;
        break;
      }
      case '2': {
        if (setPartition) {
          writePartition = 2;
          setPartition = false;
          validKey = false;
        }
        else writeKey = 0x0A;
        break;
      }
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
      case 'S': writeKey = 0xAF; writeArm = true; break;    // Arm stay
      case 'w':
      case 'W': writeKey = 0xB1; writeArm = true; break;    // Arm away
      case 'n':
      case 'N': writeKey = 0xB6; writeArm = true; break;    // Arm with no entry delay (night arm)
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
      default: {
        validKey = false;
        break;
      }
    }
    if (writeAlarm) previousTime = millis();
    if (validKey) writeReady = false;
  }
}


bool dscKeybusInterface::redundantPanelData(byte previousCmd[], volatile byte currentCmd[], byte checkedBytes) {
  bool redundantData = true;
  for (byte i = 0; i < checkedBytes; i++) {
    if (previousCmd[i] != currentCmd[i]) {
      redundantData = false;
      break;
    }
  }
  if (redundantData) return true;
  else {
    for (byte i = 0; i < dscReadSize; i++) previousCmd[i] = currentCmd[i];
    return false;
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


// Called as an interrupt when the DSC clock changes to write data for virtual keypad and setup timers to read
// data after an interval.
#if defined(__AVR__)
void dscKeybusInterface::dscClockInterrupt() {
#elif defined(ESP8266)
void ICACHE_RAM_ATTR dscKeybusInterface::dscClockInterrupt() {
#endif

  // Data sent from the panel and keypads/modules has latency after a clock change (observed up to 160us for keypad data).
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
      static bool writeRepeat = false;

      // Writes a F/A/P alarm key and repeats the key on the next immediate command from the panel (0x1C verification)
      if ((writeAlarm && !writeReady) || writeRepeat) {

        // Writes the first bit by shifting the alarm key data right 7 bits and checking bit 0
        if (isrKeybusBitTotal == 0) {
          if (!((writeKey >> 7) & 0x01)) digitalWrite(dscWritePin, HIGH);
          writeStart = true;  // Resolves a timing issue where some writes do not begin at the correct bit
        }

        // Writes the remaining alarm key data
        else if (writeStart && isrKeybusBitTotal > 0 && isrKeybusBitTotal <= 7) {
          if (!((writeKey >> (7 - isrKeybusBitCount)) & 0x01)) digitalWrite(dscWritePin, HIGH);

          // Resets counters when the write is complete
          if (isrKeybusBitTotal == 7) {
            writeReady = true;
            writeStart = false;
            writeAlarm = false;

            // Sets up a repeated write for alarm keys
            if (!writeRepeat) writeRepeat = true;
            else writeRepeat = false;
          }
        }
      }

      // Writes a regular key unless waiting for a response to the '*' key or the panel is sending a query command
      else if (!writeReady && !wroteAsterisk && isrPanelByteCount == (writePartition + 1) && statusCmd) {
        // Writes the first bit by shifting the key data right 7 bits and checking bit 0
        if (isrPanelBitTotal == (writePartition * 8) + 1) {
          if (!((writeKey >> 7) & 0x01)) digitalWrite(dscWritePin, HIGH);
          writeStart = true;  // Resolves a timing issue where some writes do not begin at the correct bit
        }

        // Writes the remaining alarm key data
        else if (writeStart && isrPanelBitTotal > (writePartition * 8) + 1 && isrPanelBitTotal <= (writePartition * 8) + 8) {
          if (!((writeKey >> (7 - isrPanelBitCount)) & 0x01)) digitalWrite(dscWritePin, HIGH);

          // Resets counters when the write is complete
          if (isrPanelBitTotal == (writePartition * 8) + 8) {
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

  static bool skipData = false;

  // Panel sends data while the clock is high
  if (digitalRead(dscClockPin) == HIGH) {

    // Stops processing Keybus data at the dscReadSize limit
    if (isrPanelByteCount >= dscReadSize) {
      skipData = true;
      dataOverflow = true;
    }

    else {
      if (isrPanelBitCount < 8) {
        // Data is captured in each byte by shifting left by 1 bit and writing to bit 0
        isrPanelData[isrPanelByteCount] <<= 1;
        if (digitalRead(dscReadPin) == HIGH) {
          isrPanelData[isrPanelByteCount] |= 1;
        }
      }

      if (isrPanelBitTotal == 8) {
        // Tests for a status command, used in dscClockInterrupt() to ensure keys are only written during a status command
        switch (isrPanelData[0]) {
          case 0x05:
          case 0x0A:
          case 0x1B: statusCmd = true; break;
          default: statusCmd = false; break;
        }

        // Stores the stop bit by itself in byte 1 - this aligns the Keybus bytes with panelData[] bytes
        isrPanelBitCount = 0;
        isrPanelByteCount++;
      }

      // Increments the bit counter if the byte is incomplete
      else if (isrPanelBitCount < 7) {
        isrPanelBitCount++;
      }

      // Byte is complete, set the counters for the next byte
      else {
        isrPanelBitCount = 0;
        isrPanelByteCount++;
      }

      isrPanelBitTotal++;
    }
  }

  // Keypads and modules send data while the clock is low
  else {
    static bool keybusDataDetected = false;

    // Keypad and module data is not buffered and skipped if the panel data buffer is filling
    if (processKeypadData && isrKeybusByteCount < dscReadSize && panelBufferLength <= 1) {

      // Data is captured in each byte by shifting left by 1 bit and writing to bit 0
      if (isrKeybusBitCount < 8) {
        isrKeybusData[isrKeybusByteCount] <<= 1;
        if (digitalRead(dscReadPin) == HIGH) {
          isrKeybusData[isrKeybusByteCount] |= 1;
        }
        else keybusDataDetected = true;  // Keypads and modules send data by pulling the data line low
      }

      // Stores the stop bit by itself in byte 1 - this aligns the Keybus bytes with keybusData[] bytes
      if (isrKeybusBitTotal == 7) {
        isrKeybusData[1] = 1;  // Sets the stop bit manually to 1 in byte 1
        isrKeybusBitCount = 0;
        isrKeybusByteCount += 2;
      }

      // Increments the bit counter if the byte is incomplete
      else if (isrKeybusBitCount < 7) {
        isrKeybusBitCount++;
      }

      // Byte is complete, set the counters for the next byte
      else {
        isrKeybusBitCount = 0;
        isrKeybusByteCount++;
      }

      isrKeybusBitTotal++;
    }

    // Saves data and resets counters after the clock cycle is complete (high for at least 1ms)
    if (clockHighTime > 1000) {

      // Skips incomplete and redundant data from status commands - these are sent constantly on the keybus at a high
      // rate, so they are always skipped.  Checking is required in the ISR to prevent flooding the buffer.
      if (isrPanelBitTotal < 8) skipData = true;
      else switch (isrPanelData[0]) {
        static byte previousCmd05[dscReadSize];
        static byte previousCmd1B[dscReadSize];
        case 0x05:  // Status: partitions 1-4
          if (redundantPanelData(previousCmd05, isrPanelData, isrPanelByteCount)) skipData = true;
          break;

        case 0x1B:  // Status: partitions 5-8
          if (redundantPanelData(previousCmd1B, isrPanelData, isrPanelByteCount)) skipData = true;
          break;
      }

      // Stores new panel data in the panel buffer
      currentCmd = isrPanelData[0];
      if (panelBufferLength == dscBufferSize) bufferOverflow = true;
      else if (!skipData && panelBufferLength < dscBufferSize) {
        for (byte i = 0; i < dscReadSize; i++) panelBuffer[panelBufferLength][i] = isrPanelData[i];
        panelBitCountBuffer[panelBufferLength] = isrPanelBitTotal;
        panelByteCountBuffer[panelBufferLength] = isrPanelByteCount;
        panelBufferLength++;
      }

      // Resets the panel capture data and counters
      for (byte i = 0; i < dscReadSize; i++) isrPanelData[i] = 0;
      isrPanelBitTotal = 0;
      isrPanelBitCount = 0;
      isrPanelByteCount = 0;
      skipData = false;

      if (processKeypadData) {

        // Stores new keypad and module data - this data is not buffered
        if (keybusDataDetected) {
          keybusDataDetected = false;
          keybusDataCaptured = true;  // Sets a flag for handleKeybus()
          for (byte i = 0; i < dscReadSize; i++) keybusData[i] = isrKeybusData[i];
          keybusBitCount = isrKeybusBitTotal;
          keybusByteCount = isrKeybusByteCount;
        }

        // Resets the keybus capture data and counters
        for (byte i = 0; i < dscReadSize; i++) isrKeybusData[i] = 0;
        isrKeybusBitTotal = 0;
        isrKeybusBitCount = 0;
        isrKeybusByteCount = 0;
      }
    }
  }
}
