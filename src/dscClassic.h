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

#ifndef dscClassic_h
#define dscClassic_h

#include <Arduino.h>

const byte dscPartitions = 1;   // Maximum number of partitions - requires 19 bytes of memory per partition
const byte dscZones = 1;        // Maximum number of zone groups, 8 zones per group - requires 6 bytes of memory per zone group
const byte dscReadSize = 2;     // Maximum bytes of a Keybus command

#if defined(__AVR__)
const byte dscBufferSize = 10;  // Number of commands to buffer if the sketch is busy - requires dscReadSize + 2 bytes of memory per command
#elif defined(ESP8266) || defined(ESP32)
const byte dscBufferSize = 50;
#endif

// Exit delay target states
#define DSC_EXIT_STAY 1
#define DSC_EXIT_AWAY 2
#define DSC_EXIT_NO_ENTRY_DELAY 3


class dscClassicInterface {

  public:

    // Initializes writes as disabled by default
    dscClassicInterface(byte setClockPin, byte setReadPin, byte setPC16Pin, byte setWritePin = 255, const char * setAccessCode = "");

    // Interface control
    void begin(Stream &_stream = Serial);             // Initializes the stream output to Serial by default
    bool loop();                                      // Returns true if valid panel data is available
    void stop();                                      // Disables the clock hardware interrupt and data timer interrupt
    void resetStatus();                               // Resets the state of all status components as changed for sketches to get the current status

    // Writes a single key - nonblocking unless a previous write is in progress
    void write(const char receivedKey);

    // Writes multiple keys from a char array
    //
    // By default, this is nonblocking unless there is a previous write in progress - in this case, the sketch must keep the char
    // array defined at least until the write is complete.
    //
    // If the char array is ephemeral, check if the write is complete by checking writeReady or set blockingWrite to true to
    // block until the write is complete.
    void write(const char * receivedKeys, bool blockingWrite = false);

    // Write control
    static byte writePartition;                       // Set to a partition number for virtual keypad
    bool writeReady;                                  // True if the library is ready to write a key

    // Prints output to the stream interface set in begin()
    void printPanelBinary(bool printSpaces = true);   // Includes spaces between bytes by default
    void printPanelCommand();                         // Prints the panel command as hex
    void printPanelMessage();                         // Prints the decoded panel message
    void printModuleBinary(bool printSpaces = true);  // Includes spaces between bytes by default
    void printModuleMessage();                        // Prints the decoded keypad or module message

    // These can be configured in the sketch setup() before begin()
    bool hideKeypadDigits;          // Controls if keypad digits are hidden for publicly posted logs (default: false)
    static bool processModuleData;  // Controls if keypad and module data is processed and displayed (default: false)

    // Status tracking
    bool statusChanged;                   // True after any status change
    bool pauseStatus;                     // Prevent status from showing as changed, set in sketch to control when to update status
    bool keybusConnected, keybusChanged;  // True if data is detected on the Keybus
    bool trouble, troubleChanged;
    bool keypadFireAlarm, keypadAuxAlarm, keypadPanicAlarm;
    bool ready[dscPartitions], readyChanged[dscPartitions];
    bool armed[dscPartitions], armedAway[dscPartitions], armedStay[dscPartitions];
    bool noEntryDelay[dscPartitions], armedChanged[dscPartitions];
    bool alarm[dscPartitions], alarmChanged[dscPartitions];
    bool exitDelay[dscPartitions], exitDelayChanged[dscPartitions];
    byte exitState[dscPartitions], exitStateChanged[dscPartitions];
    bool fire[dscPartitions], fireChanged[dscPartitions];
    bool openZonesStatusChanged;
    byte openZones[dscZones], openZonesChanged[dscZones];    // Zone status is stored in an array using 1 bit per zone, up to 64 zones
    bool alarmZonesStatusChanged;
    byte alarmZones[dscZones], alarmZonesChanged[dscZones];  // Zone alarm status is stored in an array using 1 bit per zone, up to 64 zones
    bool pgmOutputsStatusChanged;
    byte pgmOutputs[1], pgmOutputsChanged[1];
    bool armedLight, memoryLight, bypassLight, troubleLight, beep;
    static volatile bool readyLight, lightBlink;
    bool readyBlink, armedBlink, memoryBlink, bypassBlink, troubleBlink;

    /*  panelData[], pc16Data[], and moduleData[] store panel and keypad/module data in an array. These can
     *  be accessed directly in the sketch to get data that is not already tracked in the library.  See
     *  dscClassic.cpp for the currently known DSC commands and data.
     *
     *  panelData[] example:
     *    Byte 0     Byte 2   Byte 3   Byte 4   Byte 5
     *    00000101 0 10000001 00000001 10010001 11000111 [0x05] Partition 1: Ready Backlight - Partition ready | Partition 2: disabled
     *             ^ Byte 1 (stop bit)
     */
    static byte panelData[dscReadSize];
    static byte pc16Data[dscReadSize];
    static volatile byte moduleData[dscReadSize];

    // status[] and lights[] store the current status message and LED state.  These can be accessed directly in the
    // sketch to get data that is not already tracked in the library.  See printPanelMessages() and
    // printPanelLights() in dscClassic.cpp to see how this data translates to the status message and LED status.
    byte status[dscPartitions];
    byte lights[dscPartitions];

    // Process keypad and module data, returns true if data is available
    bool handleModule();

    // True if dscBufferSize needs to be increased
    static volatile bool bufferOverflow;

    // Timer interrupt function to capture data - declared as public for use by AVR Timer2
    static void dscDataInterrupt();

    // Sketch cross-compatibility - these elements are not currently used for the Classic series
    byte accessCode[dscPartitions];
    bool accessCodeChanged[dscPartitions];
    bool accessCodePrompt;
    bool decimalInput;
    bool powerTrouble, powerChanged;
    bool batteryTrouble, batteryChanged;
    bool disabled[dscPartitions], disabledChanged[dscPartitions];
    bool entryDelay[dscPartitions], entryDelayChanged[dscPartitions];
    byte panelVersion;
    bool displayTrailingBits;
    bool timestampChanged;
    byte hour, minute, day, month;
    int year;
    bool setTime(unsigned int year, byte month, byte day, byte hour, byte minute, const char* accessCode, byte timePartition = 1);

  private:

    void processPanelStatus();
    void processReadyStatus(bool status);
    void processArmed(bool status);
    void processArmedStatus(bool status);
    void processAlarmStatus(bool status);
    void processExitDelayStatus(bool status);
    void writeKeys(const char * writeKeysArray);
    void setWriteKey(const char receivedKey);
    static void dscClockInterrupt();
    static bool redundantPanelData(byte previousCmd[], volatile byte currentCmd[], byte checkedBytes = dscReadSize);

    #if defined(ESP32)
    #if ESP_IDF_VERSION_MAJOR < 4
    static hw_timer_t * timer0;
    #endif
    static portMUX_TYPE timer0Mux;
    #endif

    Stream* stream;
    const char * writeKeysArray;
    const char * accessCodeStay;
    char accessCodeAway[7];
    char accessCodeNight[7];
    bool writeKeysPending;
    bool writeArm;
    bool previousTrouble;
    bool previousKeybus;
    byte previousLights, previousStatus;
    bool previousReady;
    bool previousExitDelay, previousEntryDelay, exitDelayArmed, exitDelayTriggered;
    byte previousExitState;
    bool previousArmed, previousArmedStay, previousArmedAway;
    bool previousAlarm;
    bool alarmTriggered, previousAlarmTriggered;
    byte zonesTriggered;
    bool previousFire;
    byte previousOpenZones, previousAlarmZones;
    byte previousPgmOutput;
    bool troubleBit, armedBypassBit, armedBit, alarmBit;

    static byte dscClockPin;
    static byte dscReadPin;
    static byte dscPC16Pin;
    static byte dscWritePin;
    static byte writeByte, writeBit;
    static bool virtualKeypad;
    static char writeKey;
    static byte panelBitCount, panelByteCount;
    static volatile bool writeKeyPending, writeKeyWait;
    static volatile bool writeAlarm, starKeyDetected, starKeyCheck, starKeyWait;
    static volatile bool moduleDataCaptured;
    static volatile unsigned long clockHighTime, keybusTime, writeCompleteTime;
    static volatile byte panelBufferLength;
    static volatile byte panelBuffer[dscBufferSize][dscReadSize], pc16Buffer[dscBufferSize][dscReadSize];
    static volatile byte panelBufferBitCount[dscBufferSize], panelBufferByteCount[dscBufferSize];
    static volatile byte moduleBitCount, moduleByteCount;
    static volatile byte moduleCmd;
    static volatile byte isrPanelData[dscReadSize], isrPC16Data[dscReadSize], isrPanelBitTotal, isrPanelBitCount, isrPanelByteCount;
    static volatile byte isrModuleData[dscReadSize], isrModuleBitTotal, isrModuleBitCount, isrModuleByteCount;
};

#endif // dscClassic_h
