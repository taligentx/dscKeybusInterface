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

byte dscKeybusInterface::dscClockPin;
byte dscKeybusInterface::dscReadPin;
byte dscKeybusInterface::dscWritePin;
char dscKeybusInterface::writeKey;
byte dscKeybusInterface::writePartition;
byte dscKeybusInterface::writeByte;
byte dscKeybusInterface::writeBit;
byte dscKeybusInterface::writeModuleBit;
bool dscKeybusInterface::virtualKeypad;
bool dscKeybusInterface::processModuleData;
byte dscKeybusInterface::panelData[dscReadSize];
byte dscKeybusInterface::panelByteCount;
byte dscKeybusInterface::panelBitCount;
unsigned long dscKeybusInterface::cmdWaitTime;
volatile bool dscKeybusInterface::writeKeyPending;
volatile bool dscKeybusInterface::writeModulePending;
volatile bool dscKeybusInterface::pendingDeviceUpdate;
volatile byte dscKeybusInterface::moduleData[dscReadSize];
volatile bool dscKeybusInterface::moduleDataCaptured;
volatile byte dscKeybusInterface::moduleByteCount;
volatile byte dscKeybusInterface::moduleBitCount;
volatile bool dscKeybusInterface::writeAlarm;
volatile bool dscKeybusInterface::writeAsterisk;
volatile bool dscKeybusInterface::wroteAsterisk;
volatile bool dscKeybusInterface::bufferOverflow;
volatile byte dscKeybusInterface::panelBufferLength;
volatile byte dscKeybusInterface::currentModuleIdx;
volatile byte dscKeybusInterface::moduleBufferLength;
volatile byte dscKeybusInterface::panelBuffer[dscBufferSize][dscReadSize];
volatile byte dscKeybusInterface::panelBufferBitCount[dscBufferSize];
volatile byte dscKeybusInterface::panelBufferByteCount[dscBufferSize];
byte dscKeybusInterface::moduleSlots[6];
volatile byte dscKeybusInterface::writeModuleBuffer[6];
volatile byte dscKeybusInterface::pendingZoneStatus[6];
moduleType dscKeybusInterface::modules[maxModules];
byte dscKeybusInterface::moduleIdx;
byte dscKeybusInterface::inIdx;
byte dscKeybusInterface::outIdx;
byte dscKeybusInterface::maxFields05; 
byte dscKeybusInterface::maxFields11;
byte dscKeybusInterface::maxZones;
byte dscKeybusInterface::panelVersion;
bool dscKeybusInterface::enableModuleSupervision;
volatile byte  dscKeybusInterface::updateQueue[updateQueueSize];
volatile byte dscKeybusInterface::isrPanelData[dscReadSize];
volatile byte dscKeybusInterface::isrPanelByteCount;
volatile byte dscKeybusInterface::isrPanelBitCount;
volatile byte dscKeybusInterface::isrPanelBitTotal;
volatile byte dscKeybusInterface::isrModuleData[dscReadSize];
volatile byte dscKeybusInterface::isrModuleByteCount;
volatile byte dscKeybusInterface::isrModuleBitCount;
volatile byte dscKeybusInterface::isrModuleBitTotal;
volatile byte dscKeybusInterface::currentCmd;
volatile byte dscKeybusInterface::moduleCmd;
volatile byte dscKeybusInterface::moduleSubCmd;
volatile byte dscKeybusInterface::statusCmd;
volatile unsigned long dscKeybusInterface::clockHighTime;
volatile unsigned long dscKeybusInterface::keybusTime;



#if defined(ESP32)
hw_timer_t *timer0 = NULL;
portMUX_TYPE timer0Mux = portMUX_INITIALIZER_UNLOCKED;
#endif


dscKeybusInterface::dscKeybusInterface(byte setClockPin, byte setReadPin, byte setWritePin) {
  dscClockPin = setClockPin;
  dscReadPin = setReadPin;
  dscWritePin = setWritePin;
  if (dscWritePin != 255) virtualKeypad = true;
  writeReady = false;
  processRedundantData = true;
  displayTrailingBits = false;
  processModuleData = false;
  writePartition = 1;
  pauseStatus = false;
  maxZones=32;
  panelVersion=2; //newer.  Version 2 uses older alternate zone addressing
  maxFields05=4;
  maxFields11=4;
  enableModuleSupervision=false;
  
  for (int x=0;x<6;x++) { //clear all statuses
     pendingZoneStatus[x]=0xff;
     moduleSlots[x]=0xff;
  }
 

}


void dscKeybusInterface::begin(Stream &_stream) {
  pinMode(dscClockPin, INPUT);
  pinMode(dscReadPin, INPUT);
  if (virtualKeypad) pinMode(dscWritePin, OUTPUT);
  stream = &_stream;



  // Platform-specific timers trigger a read of the data line 250us after the Keybus clock changes

  // Arduino Timer1 calls ISR(TIMER1_OVF_vect) from dscClockInterrupt() and is disabled in the ISR for a one-shot timer
  #if defined(__AVR__)
  TCCR1A = 0;
  TCCR1B = 0;
  TIMSK1 |= (1 << TOIE1);

  // esp8266 timer1 calls dscDataInterrupt() from dscClockInterrupt() as a one-shot timer
  #elif defined(ESP8266)
  timer1_isr_init();
  timer1_attachInterrupt(dscDataInterrupt);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);

  // esp32 timer0 calls dscDataInterrupt() from dscClockInterrupt()
  #elif defined(ESP32)
  timer0 = timerBegin(0, 80, true);
  timerStop(timer0);
  timerAttachInterrupt(timer0, &dscDataInterrupt, true);
  timerAlarmWrite(timer0, 250, true);
  timerAlarmEnable(timer0);
  #endif

  // Generates an interrupt when the Keybus clock rises or falls - requires a hardware interrupt pin on Arduino
  attachInterrupt(digitalPinToInterrupt(dscClockPin), dscClockInterrupt, CHANGE);
  //adjust values to match system maximums
  
  //if maxzones is changed on setup, we adjust max values here. There is a sanity check to prevent misconfiguration later
  if (maxZones > 32 && panelVersion >=3) {
     maxFields05=6;
     maxFields11=6;
  } else {
     maxFields05=4;
     maxFields11=4;
  }
  
}


void dscKeybusInterface::stop() {

  // Disables Arduino Timer1 interrupts
  #if defined(__AVR__)
  TIMSK1 = 0;

  // Disables esp8266 timer1
  #elif defined(ESP8266)
  timer1_disable();
  timer1_detachInterrupt();

  // Disables esp32 timer0
  #elif defined(ESP32)
  timerAlarmDisable(timer0);
  timerEnd(timer0);
  #endif

  // Disables the Keybus clock pin interrupt
  detachInterrupt(digitalPinToInterrupt(dscClockPin));

  // Resets the panel capture data and counters
  panelBufferLength = 0;
  for (byte i = 0; i < dscReadSize; i++) isrPanelData[i] = 0;
  isrPanelBitTotal = 0;
  isrPanelBitCount = 0;
  isrPanelByteCount = 0;

  // Resets the keypad and module capture data and counters
  for (byte i = 0; i < dscReadSize; i++) isrModuleData[i] = 0;
  isrModuleBitTotal = 0;
  isrModuleBitCount = 0;
  isrModuleByteCount = 0;
}



bool dscKeybusInterface::loop() {

  // Checks if Keybus data is detected and sets a status flag if data is not detected for 3s
  #if defined(ESP32)
  portENTER_CRITICAL(&timer0Mux);
  #else
  noInterrupts();
  #endif

  if (millis() - keybusTime > 3000) keybusConnected = false;  // dataTime is set in dscDataInterrupt() when the clock resets
  else keybusConnected = true;

  
  #if defined(ESP32)
  portEXIT_CRITICAL(&timer0Mux);
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
  if (writeKeysPending) writeKeys(writeKeysArray);

  // Skips processing if the panel data buffer is empty
  if (panelBufferLength == 0) return false;

  // Copies data from the buffer to panelData[]
  static byte panelBufferIndex = 1;
  byte dataIndex = panelBufferIndex - 1;
  for (byte i = 0; i < dscReadSize; i++) panelData[i] = panelBuffer[dataIndex][i];
  panelBitCount = panelBufferBitCount[dataIndex];
  panelByteCount = panelBufferByteCount[dataIndex];
  panelBufferIndex++;

  // Resets counters when the buffer is cleared
  #if defined(ESP32)
  portENTER_CRITICAL(&timer0Mux);
  #else
  noInterrupts();
  #endif

  if (panelBufferIndex > panelBufferLength) {
    panelBufferIndex = 1;
    panelBufferLength = 0;
  }

  #if defined(ESP32)
  portEXIT_CRITICAL(&timer0Mux);
  #else
  interrupts();
  #endif

  // Waits at startup for the 0x05 status command or a command with valid CRC data to eliminate spurious data.
  static bool firstClockCycle = true;
  if (firstClockCycle) {
    if ((validCRC() || panelData[0] == 0x05) && panelData[0] != 0) {
      firstClockCycle = false;
      writeReady = true;

      //sanity check - we make sure that our module responses fit the cmd size
        if (panelByteCount < 9) {
            maxFields05=4; 
            maxFields11=4; 
            panelVersion=2;
        } else {
            maxFields05=6; 
            maxFields11=6; 
            panelVersion=3;
        }
     //ok we should know what panel version and zones we have so we can init the module data with the correct slot info
     updateModules();

    }
    else return false;
  }

  // Sets writeReady status
  if (!writeKeyPending && !writeKeysPending) writeReady = true;
  else writeReady = false;
  
  // Skips redundant data sent constantly while in installer programming
  static byte previousCmd0A[dscReadSize];
  static byte previousCmdE6_20[dscReadSize];
  switch (panelData[0]) {
    case 0x0A:  // Status in programming
      if (redundantPanelData(previousCmd0A, panelData)) return false;
      break;

    case 0xE6:
      if (panelData[2] == 0x20 && redundantPanelData(previousCmdE6_20, panelData)) return false;  // Status in programming, zone lights 33-64
      break;
  }
  if (dscPartitions > 4) {
    static byte previousCmdE6_03[dscReadSize];
    if (panelData[0] == 0xE6 && panelData[2] == 0x03 && redundantPanelData(previousCmdE6_03, panelData, 8)) return false;  // Status in alarm/programming, partitions 5-8
  }

  // Skips redundant data from periodic commands sent at regular intervals, by default this data is processed
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
        if (redundantPanelData(previousCmd11, panelData)) return true;
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



bool dscKeybusInterface::handlePanel() { return loop(); }

bool dscKeybusInterface::handleModule() {
  if (!moduleDataCaptured) return false;
  moduleDataCaptured = false;

  if (moduleBitCount < 8) return false;

  // Skips periodic keypad slot query responses
  if (!processRedundantData && currentCmd == 0x11) {
    
    bool redundantData = true;
    byte checkedBytes = dscReadSize;
    static byte previousSlotData[dscReadSize];
    for (byte i = 0; i < checkedBytes; i++) {
      if (previousSlotData[i] != moduleData[i]) {
        redundantData = false;
        break;
      }
    }
    if (redundantData) 
        return false;
    else {
      for (byte i = 0; i < dscReadSize; i++) previousSlotData[i] = moduleData[i];
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

// Sets up writes for a single key
void dscKeybusInterface::write(const char receivedKey) {
    
  static unsigned long waitTime;
  waitTime=millis();
  // Loops if a previous write is in progress
  while(writeKeyPending || writeKeysPending) {
    loop();
    #if defined(ESP8266)
    yield();
    #endif
	if (millis() - waitTime > 10000) { // timeout after no response from * write
		writeKeysPending = false;
		wroteAsterisk = false;  // Resets the flag that delays writing after '*' is pressed
        writeAsterisk = false;
        writeKeyPending = false;
		return;
	}
	
  }
  
  setWriteKey(receivedKey);
}


// Sets up writes for multiple keys sent as a char array
void dscKeybusInterface::write(const char *receivedKeys, bool blockingWrite) {
    
  static unsigned long waitTime;
  waitTime=millis();
  // Loops if a previous write is in progress
  while(writeKeyPending || writeKeysPending) {
    loop();
    #if defined(ESP8266)
    yield();
    #endif
	if (millis() - waitTime > 10000) { // timeout after no response from * write
		writeKeysPending = false;
		wroteAsterisk = false;  // Resets the flag that delays writing after '*' is pressed
        writeAsterisk = false;
        writeKeyPending = false;
		return;
	}
  }

  writeKeysArray = receivedKeys;

  if (writeKeysArray[0] != '\0') {
    writeKeysPending = true;
    writeReady = false;
  }

  // Optionally loops until the write is complete
  if (blockingWrite) {
    while (writeKeysPending) {
      writeKeys(writeKeysArray);
      loop();
      #if defined(ESP8266)
      yield();
      #endif
	  if (millis() - waitTime > 10000) { // timeout after no response for 3 seconds from * write
		writeKeysPending = false;
		wroteAsterisk = false;  // Resets the flag that delays writing after '*' is pressed
        writeAsterisk = false;
        writeKeyPending = false;
		return;
	  }
	  
    }
  }
  else writeKeys(writeKeysArray);
}


// Writes multiple keys from a char array
void dscKeybusInterface::writeKeys(const char *writeKeysArray) {
  static byte writeCounter = 0;
  if (!writeKeyPending && writeKeysPending && writeCounter < strlen(writeKeysArray)) {
    if (writeKeysArray[writeCounter] != '\0') {
      setWriteKey(writeKeysArray[writeCounter]);
      writeCounter++;
      if (writeKeysArray[writeCounter] == '\0') {
        writeKeysPending = false;
        writeCounter = 0;
      }
    }
  }
}


// Specifies the key value to be written by dscClockInterrupt() and selects the write partition.  This includes a 500ms
// delay after alarm keys to resolve errors when additional keys are sent immediately after alarm keys.
void dscKeybusInterface::setWriteKey(const char receivedKey) {
  static unsigned long previousTime;
  static bool setPartition;

  // Sets the write partition if set by virtual keypad key '/'
  if (setPartition) {
    setPartition = false;
    if (receivedKey >= '1' && receivedKey <= '8' ) {
		writePartition = receivedKey - 48;
		if (disabled[writePartition - 1]) writePartition=1; //prevent writes to disabled partitions
			 
    }
    return;
  }

  // Sets the binary to write for virtual keypad keys
  if (!writeKeyPending && (millis() - previousTime > 500 || millis() <= 500)) {
    bool validKey = true;
    switch (receivedKey) {
      case '/': setPartition = true; validKey = false; break;
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
      case '*': writeKey = 0x28; writeAsterisk = true; break; //was true but causes lockup on unused partions
      case '#': writeKey = 0x2D; break;
      case 'F':
      case 'f': writeKey = 0x77; writeAlarm = true; break;                    // Keypad fire alarm
      case 's':
      case 'S': writeKey = 0xAF; writeArm[writePartition - 1] = true; break;  // Arm stay
      case 'w':
      case 'W': writeKey = 0xB1; writeArm[writePartition - 1] = true; break;  // Arm away
      case 'n':
      case 'N': writeKey = 0xB6; writeArm[writePartition - 1] = true; break;  // Arm with no entry delay (night arm)
      case 'A':
      case 'a': writeKey = 0xBB; writeAlarm = true; break;                    // Keypad auxiliary alarm
      case 'c':
      case 'C': writeKey = 0xBB; break;                                       // Door chime
      case 'r':
      case 'R': writeKey = 0xDA; break;                                       // Reset
      case 'P':
      case 'p': writeKey = 0xDD; writeAlarm = true; break;                    // Keypad panic alarm
      case 'x':
      case 'X': writeKey = 0xE1; break;                                       // Exit
      case '[': writeKey = 0xD5; break;                                       // Command output 1
      case ']': writeKey = 0xDA; break;                                       // Command output 2
      case '{': writeKey = 0x70; break;                                       // Command output 3
      case '}': writeKey = 0xEC; break;                                       // Command output 4
      default: {
        validKey = false;
        break;
      }
    }

    // Skips writing to partitions not specified in dscKeybusInterface.h
    if (dscPartitions < writePartition) return;

    // Sets the writing position in dscClockInterrupt() for the currently set partition
    switch (writePartition) {
      case 1:
      case 5: {
        writeByte = 2;
        writeBit = 9;
        break;
      }
      case 2:
      case 6: {
        writeByte = 3;
        writeBit = 17;
        break;
      }
      case 3:
      case 7: {
        writeByte = 8;
        writeBit = 57;
        break;
      }
      case 4:
      case 8: {
        writeByte = 9;
        writeBit = 65;
        break;
      }
      default: {
        writeByte = 2;
        writeBit = 9;
        break;
      }
    }

    if (writeAlarm) previousTime = millis();  // Sets a marker to time writes after keypad alarm keys
    if (validKey) {
      writeKeyPending = true;                 // Sets a flag indicating that a write is pending, cleared by dscClockInterrupt()
      writeReady = false;
    }
  }
}
#if defined(__AVR__)
bool dscKeybusInterface::redundantPanelData(byte previousCmd[], volatile byte currentCmd[], byte checkedBytes) {
#elif defined(ESP8266)
bool ICACHE_RAM_ATTR dscKeybusInterface::redundantPanelData(byte previousCmd[], volatile byte currentCmd[], byte checkedBytes) {
#elif defined(ESP32)
bool  IRAM_ATTR dscKeybusInterface::redundantPanelData(byte previousCmd[], volatile byte currentCmd[], byte checkedBytes) {
#endif
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
 return redundantData;
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

#if defined(__AVR__)
byte dscKeybusInterface::getPendingUpdate() { 
#elif defined(ESP8266)
byte ICACHE_RAM_ATTR dscKeybusInterface::getPendingUpdate() { 
#elif defined(ESP32)
byte  IRAM_ATTR dscKeybusInterface::getPendingUpdate() { 
#endif
//check for a pending update flag and clear it
	if (inIdx == outIdx) return 0;
    byte addr = updateQueue[outIdx];
	outIdx = (outIdx + 1) % updateQueueSize;
	return addr;
}


void dscKeybusInterface::setSupervisorySlot(byte address,bool set=true) {
       //set our response data for the 0x11 supervisory request
       if (panelVersion < 3) {
            switch (address) {
            //11111111 1 00111111 11111111 11111111 11111111 11111111 11111100 11111111 16
            //11111111 1 00111111 11111111 11111111 00111111 11111111 11111111 11111111 13
            // 1111111 1 00111111 11111111 00111111 11111111 11111111 11111111 11111111 slots 9
            //11111111 1 00111111 11111111 11111111 11111111 11111111 11111100 11111111 slots 16
             //for older versions we need to set 2 slots since they expect groups of 4 zones per slot
            case 9:   moduleSlots[2]=set?moduleSlots[2]&0x3f:moduleSlots[2]|~0x3f; //pc5108 
                      moduleSlots[2]=set?moduleSlots[2]&0xcf:moduleSlots[2]|~0xcf;
                      break; 
            case 10:  moduleSlots[2]=set?moduleSlots[2]&0xf3:moduleSlots[2]|~0xf3; //pc5108
                      moduleSlots[2]=set?moduleSlots[2]&0xfc:moduleSlots[2]|~0xfc;
                      break; 
            case 11:  moduleSlots[3]=set?moduleSlots[3]&0x3f:moduleSlots[3]|~0x3f;//pc5108
                      moduleSlots[3]=set?moduleSlots[3]&0xcf:moduleSlots[3]|~0xcf;
                      break; 
            case 18:  moduleSlots[3]=set?moduleSlots[3]&0xfc:moduleSlots[3]|~0xfc;break;  // pc5208 relay board reports as 18
            default: return;
        }
           
       } else {
        switch (address) {
            //11111111 1 00111111 11111111 11111111 11111111 11111111 11111100 11111111 16
            //11111111 1 00111111 11111111 11111111 00111111 11111111 11111111 11111111 13
            // 1111111 1 00111111 11111111 00111111 11111111 11111111 11111111 11111111 slots 9
            //11111111 1 00111111 11111111 11111111 11111111 11111111 11111100 11111111 slots 16
            case 9:   moduleSlots[2]=set?moduleSlots[2]&0x3f:moduleSlots[2]|~0x3f;break; //pc5108 
            case 10:  moduleSlots[2]=set?moduleSlots[2]&0xcf:moduleSlots[2]|~0xcf;break; //pc5108
            case 11:  moduleSlots[2]=set?moduleSlots[2]&0xf3:moduleSlots[2]|~0xf3;break; //pc5108
            case 12:  if  (maxZones>32) {moduleSlots[2]=set?moduleSlots[2]&0xfc:moduleSlots[2]|~0xfc;}break; //pc5108
            case 13:  if  (maxZones>32) {moduleSlots[3]=set?moduleSlots[3]&0x3f:moduleSlots[3]|~0x3f;}break; //pc5108
            case 14:  if  (maxZones>32) {moduleSlots[3]=set?moduleSlots[3]&0xcf:moduleSlots[3]|~0xcf;}break; //pc5108
            case 16:  if  (maxZones>32) {moduleSlots[5]=set?moduleSlots[5]&0x3f:moduleSlots[5]|~0x3f;}break; //pc5108 (shows on slot24)// reports as 16 in panel
            //reports as 18 in panel
            case 18:  moduleSlots[3]=set?moduleSlots[3]&0xfc:moduleSlots[3]|~0xfc;break;  // pc5208 relay board reports as 18
            default: return;
        }
       }
    
}

void dscKeybusInterface::addRequestToQueue(byte address) {
            if (!address) return;
            updateQueue[inIdx]=address;
            inIdx=(inIdx + 1) % updateQueueSize;
}

zoneMaskType dscKeybusInterface::getUpdateMask(byte address) {

        //get our request byte and mask to send data for the zone that we need to publish info on. This gets sent on the 05 command
        //11111111 1 11111111 11111111 11111111 11111111 11111111 01111111 11111111 11111111 (12)
        //11111111 1 11111111 11111111 10111111 11111111 11111111 11111111 11111111 11111111 (9)
        //11111111 1 11111111 11111111 10111111 11111111 version 2 zone 9 -16
        //11111111 1 11111111 11111111 11101111 11111111  version 2 system zone 27-32

        zoneMaskType zm;
        switch (address) {
            case 9:  zm.idx=2;zm.mask=0xbf; break; //5208
            case 10: zm.idx=2;zm.mask=0xdf; break; //5208
            case 11: zm.idx=2;zm.mask=0xef; break;//5208
            case 12: if (maxZones>32) {zm.idx=5;zm.mask=0x7f;} break;
            case 13: if (maxZones>32) {zm.idx=5;zm.mask=0xbf;} break;
            case 14: if (maxZones>32) {zm.idx=5;zm.mask=0xdf;} break;
            case 16: if (maxZones>32) {zm.idx=5;zm.mask=0xef;} break;//5208 sends to slot 15
            case 18: zm.idx=2;zm.mask=0xfe; break; //relay board 5108 sends to slot 16
            default: zm.idx=0;zm.mask=0;
        }
        return zm;
}


//clears all emulated zones on the panel 
void dscKeybusInterface:: clearZoneRanges() {
for (int x=0;x<moduleIdx;x++) {
        modules[x].faultBuffer[0]=0x55;
        modules[x].faultBuffer[1]=0;
        modules[x].faultBuffer[2]=0x55;
        modules[x].faultBuffer[3]=0;
        modules[x].faultBuffer[4]= 0xaa ;  //cksum for 01010101 00000000 
        if (!modules[x].zoneStatusByte) continue;
        pendingZoneStatus[modules[x].zoneStatusByte]&=modules[x].zoneStatusMask; //set update slot
        addRequestToQueue(modules[x].address);  //update queue to indicate pending request
      }
}

//once we know what panelversion we have, we can then update the modules with the correct info here
void dscKeybusInterface::updateModules() {
    for (int x=0;x<moduleIdx;x++) {
        zoneMaskType zm=getUpdateMask(modules[x].address);
        if (!zm.idx) {
            //we don't have an update idx so it means our address is invalid. clear it
            modules[x].address=0;
        } else {
            modules[x].zoneStatusByte=zm.idx;
            modules[x].zoneStatusMask=zm.mask;
            if (enableModuleSupervision)
                setSupervisorySlot(modules[x].address,true);
        }
    }
}

//add new expander modules and init zone fields
void dscKeybusInterface::addModule(byte address) {
    
 if (!address) return;
 if (address > 12 && maxZones <=32) return;
 if (moduleIdx < maxModules ) {
   modules[moduleIdx].address=address;
  // for (int x=0;x<4;x++) modules[moduleIdx].fields[x]=0x55;//set our zones as closed by default (01 per channel)
   memset(modules[moduleIdx].fields,0x55,4);
   moduleIdx++;
 }
}

void dscKeybusInterface::addRelayModule() {
    if (enableModuleSupervision)
        setSupervisorySlot(18,true);
}

void dscKeybusInterface::removeModule(byte address) {
    int idx;
    for (idx=0;idx<moduleIdx;idx++) {
        if (modules[idx].address==address) break;
    }
    if (idx==moduleIdx) return;
   modules[idx].address=0;
   setSupervisorySlot(address,false); //remove it from the supervisory response

}

void dscKeybusInterface::setZoneFault(byte zone,bool fault) {
      
    byte address=0;
    byte channel=0;
    bool change=false;
    
    //we try and do as much setup here so that the ISR functions do the mimimal work.
    
    if (zone > maxZones) return;
    
    //get address and channel from zone range
        if (zone > 8 && zone < 17) {
            address=9;
            channel=zone-9;
        } else if (zone > 16 && zone < 25) {
            address=10;
            channel=zone-17;
        } else if (zone > 24 && zone < 33) {
            address=11;
            channel=zone-25;
        } else if (zone > 32 && zone < 41) {
            address=12;
            channel=zone-33;
        } else if (zone > 40 && zone < 49) {
            address=13;
            channel=zone-41;
        } else if (zone > 48 && zone < 57 ) {
            address=14;
            channel=zone-49;
        } else if (zone > 56 && zone < 65) {
            address=16;
            channel=zone-57;
        } 
    
     
    
    if (!address ) return; //invalid zone, so return
 
 
    //see if we are emulating this zone range 
    int idx;
    for (idx=0;idx<moduleIdx;idx++) {
        if (modules[idx].address==address) break;
    }

    if (idx==moduleIdx) return;  //address not found in our emulation list so return
    
    uint8_t chk1=0xff;
    uint8_t chk2=0xff;
           

    
    if (channel < 4) { //set / reset bits according to fault value (open=11,closed=01)
        channel=channel*2;
        modules[idx].fields[0]=fault?modules[idx].fields[0] | (zoneOpen << channel):modules[idx].fields[0] & ~(zoneClosed << channel);
        chk1=((modules[idx].fields[0] >> 4)+(modules[idx].fields[0]&0x0f)+(modules[idx].fields[1]>>4)+(modules[idx].fields[1]&0x0f)) % 0x10;
    } else {
        channel=(channel-4)*2;
        modules[idx].fields[2]=fault?modules[idx].fields[2] | (zoneOpen << (channel)):modules[idx].fields[2] & ~(zoneClosed << (channel));
        chk2=((modules[idx].fields[2]>>4)+(modules[idx].fields[2]&0x0f)+(modules[idx].fields[3]>>4)+(modules[idx].fields[3]&0x0f)) % 0x10;
    }

    //for (int x=0;x<5;x++)  modules[idx].faultBuffer[x]=0xFF;//clear buffer
    memset(modules[idx].faultBuffer,0xFF,5);

    if ( modules[idx].fields[0] != modules[idx].fields[1]) { //see if our current low channels changed from previous. 
         modules[idx].faultBuffer[0]=modules[idx].fields[0]; //populate faultbuffer with response data for low channel
         modules[idx].faultBuffer[1]=modules[idx].fields[1];
         modules[idx].faultBuffer[4]=(chk1 << 4) | 0x0F; 
         modules[idx].fields[1]=modules[idx].fields[0];  //copy current channel values to previous
         change=true;
    }
    if (modules[idx].fields[2] != modules[idx].fields[3]) {  //check high channels
         modules[idx].faultBuffer[2]=modules[idx].fields[2];
         modules[idx].faultBuffer[3]=modules[idx].fields[3];
         modules[idx].faultBuffer[4]= (modules[idx].faultBuffer[4] & 0xf0) | chk2 ; 
         modules[idx].fields[3]=modules[idx].fields[2];  //copy current channel values to previous
         change=true;
    }
    if (!change) return;  //nothing changed in our zones so return
    if (modules[idx].zoneStatusByte) { 
        pendingZoneStatus[modules[idx].zoneStatusByte]&=modules[idx].zoneStatusMask; //set update slot
        addRequestToQueue(address);  //update queue to indicate pending request
    }
}
 
#if defined(__AVR__)
void dscKeybusInterface::dscKeybusInterface::processModuleResponse(byte cmd) {
#elif defined(ESP8266)
void ICACHE_RAM_ATTR dscKeybusInterface::dscKeybusInterface::processModuleResponse(byte cmd) {
#elif defined(ESP32)
void IRAM_ATTR dscKeybusInterface::dscKeybusInterface::processModuleResponse(byte cmd) {
#endif

/*
11111111 1 11111111 11111111 10111111 11111111 11111111 11111111 11111111 11111111 9
11111111 1 11111111 11111111 11011111 11111111 11111111 11111111 11111111 11111111 10
11111111 1 11111111 11111111 11101111 11111111 11111111 11111111 11111111 11111111 11
11111111 1 11111111 11111111 11111111 11111111 11111111 01111111 11111111 11111111 12
11111111 1 11111111 11111111 11111111 11111111 11111111 10111111 11111111 11111111 13
11111111 1 11111111 11111111 11111111 11111111 11111111 11011111 11111111 11111111 14
11111111 1 11111111 11111111 11111111 11111111 11111111 11101111 11111111 11111111 16
*/
     byte address=0;
     switch (cmd) {
       case 0x05:   if (!getPendingUpdate()) return;  //if nothing in queue for a zone update return
                    moduleBufferLength=maxFields05;
                    //for(int x=0;x<maxFields05;x++) writeModuleBuffer[x]=pendingZoneStatus[x];
                    memcpy((void*) writeModuleBuffer,(void*) pendingZoneStatus,maxFields05);
                    writeModulePending=true;
                    break;
//11111111 1 00111111 11111111 11111111 11111111 11111111 11111100 11111111 device 16 in slot 24  
//11111111 1 00111111 11111111 11110011 11111111 11111111 11111111 11111111  slot 11   
//11111111 1 00111111 11111111 00111111 11111111 11111111 11111111 11111111    slot 9     
       case 0x11:   if (!enableModuleSupervision) return;
                    moduleBufferLength=maxFields11;
                    //for(int x=0;x<maxFields11;x++) writeModuleBuffer[x]=moduleSlots[x];
                    memcpy((void*) writeModuleBuffer,(void*) pendingZoneStatus,maxFields11);
                    writeModulePending=true;
                    break;
       case 0x28:   address=9;break;  // the address will depend on the panel request command.
       case 0x33:   address=10;break;
       case 0x39:   address=11;break;
       default:     return;            
    }
    moduleCmd=cmd; //set command to respond on
    moduleSubCmd=0;
    currentModuleIdx=0;
    writeModuleBit=9; //set bit location where we start sending our own data on the command
    if (!address) return; //cmds 0x11/0x05 return here
    
    int idx;   
    for (idx=0;idx<moduleIdx;idx++) {  //get the buffer data from the module record that matches the address we need
        if (modules[idx].address==address) break;
    }
    if (idx==moduleIdx) return; //not found so not for us
   // for(int x=0;x<5;x++) writeModuleBuffer[x]=modules[idx].faultBuffer[x]; //get the fault data for that emulated board
    memcpy((void*)writeModuleBuffer,(void*) modules[idx].faultBuffer,5); //get the fault data for that emulated board
    moduleBufferLength=5;
    pendingZoneStatus[modules[idx].zoneStatusByte]|=~modules[idx].zoneStatusMask; //clear update slot
    writeModulePending=true;    //set flag that we need to write buffer data 
 
}


#if defined(__AVR__)
void dscKeybusInterface::processModuleResponse_0xE6(byte subcmd) {
#elif defined(ESP8266)
void  ICACHE_RAM_ATTR dscKeybusInterface::processModuleResponse_0xE6(byte subcmd) {
#elif defined(ESP32)
void IRAM_ATTR dscKeybusInterface::processModuleResponse_0xE6(byte subcmd) {
#endif

    byte address=0;
    switch (subcmd) {
       case 0x8:   address=12;break;
       case 0xA:   address=13;break;
       case 0xC:   address=14;break;
       case 0xE:   address=16;break;
       default:     return;            
    }
    moduleCmd=0xE6;
    moduleSubCmd=subcmd;
    currentModuleIdx=0;
    writeModuleBit=17;
    int idx;  
    for (idx=0;idx<moduleIdx;idx++) {
        if (modules[idx].address==address) break;
    }
    if (idx==moduleIdx) return; //not found so return
    //for(int x=0;x<5;x++) writeModuleBuffer[x]=modules[idx].faultBuffer[x]; //wet get our zone fault data 
    memcpy((void*)writeModuleBuffer,(void*)modules[idx].faultBuffer,5);
    moduleBufferLength=5;
    pendingZoneStatus[modules[idx].zoneStatusByte]|=~modules[idx].zoneStatusMask; //clear update slot
    writeModulePending=true;   //set flag to send it
}

// Called as an interrupt when the DSC clock changes to write data for virtual keypad and setup timers to read
// data after an interval.
#if defined(__AVR__)
void dscKeybusInterface::dscClockInterrupt() {
#elif defined(ESP8266)
void ICACHE_RAM_ATTR dscKeybusInterface::dscClockInterrupt() {
#elif defined(ESP32)
void IRAM_ATTR dscKeybusInterface::dscClockInterrupt() {
#endif

  // Data sent from the panel and keypads/modules has latency after a clock change (observed up to 160us for keypad data).
  // The following sets up a timer for both Arduino/AVR and Arduino/esp8266 that will call dscDataInterrupt() in
  // 250us to read the data line.

  // AVR Timer1 calls dscDataInterrupt() via ISR(TIMER1_OVF_vect) when the Timer1 counter overflows
  #if defined(__AVR__)
  TCNT1=61535;            // Timer1 counter start value, overflows at 65535 in 250us
  TCCR1B |= (1 << CS10);  // Sets the prescaler to 1

  // esp8266 timer1 calls dscDataInterrupt() in 250us
  #elif defined(ESP8266)
  timer1_write(1250);

  // esp32 timer0 calls dscDataInterrupt() in 250us
  #elif defined(ESP32)
  timerStart(timer0);
  portENTER_CRITICAL(&timer0Mux);
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
      static bool writeCmd = false;

      if (writePartition <= 4 && statusCmd == 0x05) writeCmd = true;
      else if (writePartition > 4 && statusCmd == 0x1B) writeCmd = true;
      else writeCmd = false;

      // Writes a F/A/P alarm key and repeats the key on the next immediate command from the panel (0x1C verification)
      if ((writeAlarm && writeKeyPending) || writeRepeat) {

        // Writes the first bit by shifting the alarm key data right 7 bits and checking bit 0
        if (isrPanelBitTotal == 1) {
          if (!((writeKey >> 7) & 0x01)) {
            digitalWrite(dscWritePin, HIGH);
          }
          writeStart = true;  // Resolves a timing issue where some writes do not begin at the correct bit
        }

        // Writes the remaining alarm key data
        else if (writeStart && isrPanelBitTotal > 1 && isrPanelBitTotal <= 8) {
          if (!((writeKey >> (8 - isrPanelBitTotal)) & 0x01)) digitalWrite(dscWritePin, HIGH);
          // Resets counters when the write is complete
          if (isrPanelBitTotal == 8) {
            writeKeyPending = false;
            writeStart = false;
            writeAlarm = false;

            // Sets up a repeated write for alarm keys
            if (!writeRepeat) writeRepeat = true;
            else writeRepeat = false;
          }
        }
      }

      // Writes a regular key unless waiting for a response to the '*' key or the panel is sending a query command
      else if (!writeModulePending && writeKeyPending && !wroteAsterisk && isrPanelByteCount == writeByte && writeCmd) {
        // Writes the first bit by shifting the key data right 7 bits and checking bit 0
        if (isrPanelBitTotal == writeBit) {
          if (!((writeKey >> 7) & 0x01)) digitalWrite(dscWritePin, HIGH);
          writeStart = true;  // Resolves a timing issue where some writes do not begin at the correct bit
        }

        // Writes the remaining alarm key data
        else if (writeStart && isrPanelBitTotal > writeBit && isrPanelBitTotal <= writeBit + 7) {
          if (!((writeKey >> (7 - isrPanelBitCount)) & 0x01)) digitalWrite(dscWritePin, HIGH);

          // Resets counters when the write is complete
          if (isrPanelBitTotal == writeBit + 7) {
            if (writeAsterisk) wroteAsterisk = true;  // Delays writing after pressing '*' until the panel is ready
            else writeKeyPending = false;
            writeStart = false;
          }
        }
      } 
      else if (isrPanelData[0]==moduleCmd && writeModulePending ) {
        // Writes the first bit by shifting the key data right 7 bits and checking bit 0
        if (isrPanelBitTotal == writeModuleBit) {
          if (moduleSubCmd && moduleSubCmd!=isrPanelData[2]) return; //if e6 and not correct subcommand, we exit
          if (!((writeModuleBuffer[currentModuleIdx] >> 7) & 0x01)) digitalWrite(dscWritePin, HIGH);
          writeStart = true;  // Resolves a timing issue where some writes do not begin at the correct bit
        }

        // Writes the remaining module data
        else if (writeStart && isrPanelBitTotal > writeModuleBit && isrPanelBitTotal <= writeModuleBit + (moduleBufferLength * 8)) {
           if (!((writeModuleBuffer[currentModuleIdx] >> (7 - isrPanelBitCount)) & 0x01)) digitalWrite(dscWritePin, HIGH);
          // Resets counters when the write is complete
          if (isrPanelBitTotal == writeModuleBit + (moduleBufferLength * 8)) {
                writeStart = false;
                writeModulePending=false;
          }  else if (isrPanelBitCount==7) {
              currentModuleIdx++;
              if (currentModuleIdx==moduleBufferLength) {
                   writeStart = false;
                   writeModulePending=false;
                   moduleCmd=0;
              }
          }
        }
      }
      
    }
  }
  #if defined(ESP32)
  portEXIT_CRITICAL(&timer0Mux);
  #endif
}


// Interrupt function called after 250us by dscClockInterrupt() using AVR Timer1, disables the timer and calls
// dscDataInterrupt() to read the data line
#if defined(__AVR__)
ISR(TIMER1_OVF_vect) {
  TCCR1B = 0;  // Disables Timer1
  dscKeybusInterface::dscDataInterrupt();
}
#endif


// Interrupt function called by AVR Timer1, esp8266 timer1, and esp32 timer0 after 250us to read the data line
#if defined(__AVR__)
void dscKeybusInterface::dscDataInterrupt() {
#elif defined(ESP8266)
void ICACHE_RAM_ATTR dscKeybusInterface::dscDataInterrupt() {
#elif defined(ESP32)
void IRAM_ATTR dscKeybusInterface::dscDataInterrupt() {
  timerStop(timer0);
  portENTER_CRITICAL(&timer0Mux);
#endif

  static bool skipData = false;
  static bool goodCmd = false;
  static unsigned long cmdTime;

  // Panel sends data while the clock is high
  if (digitalRead(dscClockPin) == HIGH) {

    // Stops processing Keybus data at the dscReadSize limit
    if (isrPanelByteCount >= dscReadSize) skipData = true;

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
          case 0x05: statusCmd = 0x05;
          case 0x11: 
          case 0x28: 
          case 0x33:
          case 0xEB: 
          case 0x39: processModuleResponse(isrPanelData[0]);break;
          case 0x0A: statusCmd = 0x05; break;
          case 0x1B: statusCmd = 0x1B; break;
          default: statusCmd = 0; break;
        
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
      if (isrPanelBitTotal == 16) {
            switch (isrPanelData[0]) {
                case 0xE6: processModuleResponse_0xE6(isrPanelData[2]);break;//check subcommand
                
            }
       }
      isrPanelBitTotal++;
    }
  }

  // Keypads and modules send data while the clock is low
  else {
    static bool moduleDataDetected = false;

    // Keypad and module data is not buffered and skipped if the panel data buffer is filling
    if (processModuleData && isrModuleByteCount < dscReadSize && panelBufferLength <= 1) {

      // Data is captured in each byte by shifting left by 1 bit and writing to bit 0
      if (isrModuleBitCount < 8) {
        isrModuleData[isrModuleByteCount] <<= 1;
        if (digitalRead(dscReadPin) == HIGH) {
          isrModuleData[isrModuleByteCount] |= 1;
        }
        else moduleDataDetected = true;  // Keypads and modules send data by pulling the data line low
      }

      // Stores the stop bit by itself in byte 1 - this aligns the Keybus bytes with moduleData[] bytes
      if (isrModuleBitTotal == 7) {
        isrModuleData[1] = 1;  // Sets the stop bit manually to 1 in byte 1
        isrModuleBitCount = 0;
        isrModuleByteCount += 2;
      }

      // Increments the bit counter if the byte is incomplete
      else if (isrModuleBitCount < 7) {
        isrModuleBitCount++;
      }

      // Byte is complete, set the counters for the next byte
      else {
        isrModuleBitCount = 0;
        isrModuleByteCount++;
      }

      isrModuleBitTotal++;
    }

    // Saves data and resets counters after the clock cycle is complete (high for at least 1ms)
    if (clockHighTime > 1000) {
      keybusTime = millis();

      // Skips incomplete and redundant data from status commands - these are sent constantly on the keybus at a high
      // rate, so they are always skipped.  Checking is required in the ISR to prevent flooding the buffer.
      if (isrPanelBitTotal < 8) skipData = true;
      else switch (isrPanelData[0]) {
        static byte previousCmd05[dscReadSize];
        static byte previousCmd1B[dscReadSize];
        case 0x05:  // Status: partitions 1-4
		  if (redundantPanelData(previousCmd05, isrPanelData, isrPanelByteCount)){
			 if (!goodCmd && (millis() - cmdTime) > cmdWaitTime) {
				 skipData=false;
				 goodCmd=true;
			 }
			 else skipData = true;
		  } else {
			 cmdTime = millis();
			 skipData=true;
			 goodCmd=false;
		  }
          break;

        case 0x1B:  // Status: partitions 5-8
          if (redundantPanelData(previousCmd1B, isrPanelData, isrPanelByteCount)){
			 if (!goodCmd && (millis() - cmdTime) > cmdWaitTime) {
				 skipData=false;
				 goodCmd=true;
			 }
			 else skipData = true;
		  } else {
			 cmdTime = millis();
			 skipData=true;
			 goodCmd=false;
		  }
          break;
      }

      // Stores new panel data in the panel buffer
      currentCmd = isrPanelData[0];
      if (panelBufferLength == dscBufferSize) bufferOverflow = true;
      else if (!skipData && panelBufferLength < dscBufferSize) {
        for (byte i = 0; i < dscReadSize; i++) panelBuffer[panelBufferLength][i] = isrPanelData[i];
        panelBufferBitCount[panelBufferLength] = isrPanelBitTotal;
        panelBufferByteCount[panelBufferLength] = isrPanelByteCount;
        panelBufferLength++;
      }

      // Resets the panel capture data and counters
      for (byte i = 0; i < dscReadSize; i++) isrPanelData[i] = 0;
      isrPanelBitTotal = 0;
      isrPanelBitCount = 0;
      isrPanelByteCount = 0;
      skipData = false;

      if (processModuleData) {

        // Stores new keypad and module data - this data is not buffered
        if (moduleDataDetected) {
          moduleDataDetected = false;
          moduleDataCaptured = true;  // Sets a flag for handleModule()
          for (byte i = 0; i < dscReadSize; i++) moduleData[i] = isrModuleData[i];
          moduleBitCount = isrModuleBitTotal;
          moduleByteCount = isrModuleByteCount;
        }

        // Resets the keypad and module capture data and counters
        for (byte i = 0; i < dscReadSize; i++) isrModuleData[i] = 0;
        isrModuleBitTotal = 0;
        isrModuleBitCount = 0;
        isrModuleByteCount = 0;
      }
    }
  }
  #if defined(ESP32)
  portEXIT_CRITICAL(&timer0Mux);
  #endif
}
