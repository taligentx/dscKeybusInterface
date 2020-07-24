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

#ifndef dscKeybusInterface_h
#define dscKeybusInterface_h

#include <Arduino.h>


#if defined(__AVR__)
const byte dscPartitions = 4;   // Maximum number of partitions - requires 19 bytes of memory per partition
const byte dscZones = 4;        // Maximum number of zone groups, 8 zones per group - requires 6 bytes of memory per zone group
const byte dscBufferSize = 10;  // Number of commands to buffer if the sketch is busy - requires dscReadSize + 2 bytes of memory per command
#elif defined(ESP8266) || defined(ESP32)
const byte dscPartitions = 8;
const byte dscZones = 8;
const byte dscBufferSize = 50;
#endif

// Maximum bytes of a Keybus command
const byte dscReadSize = 16;

// Exit delay target states
#define DSC_EXIT_STAY 1
#define DSC_EXIT_AWAY 2
#define DSC_EXIT_NO_ENTRY_DELAY 3


class dscKeybusInterface {

  public:

    // Initializes writes as disabled by default
    dscKeybusInterface(byte setClockPin, byte setReadPin, byte setWritePin = 255);

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
    bool processRedundantData;      // Controls if repeated periodic commands are processed and displayed (default: false)
    static bool processModuleData;  // Controls if keypad and module data is processed and displayed (default: false)
    bool displayTrailingBits;       // Controls if bits read as the clock is reset are displayed, appears to be spurious data (default: false)

    // Panel time
    bool timestampChanged;          // True after the panel sends a timestamped message
    byte hour, minute, day, month;
    int year;

    // Sets panel time, the year can be sent as either 2 or 4 digits, returns true if the panel is ready to set the time
    bool setTime(unsigned int year, byte month, byte day, byte hour, byte minute, const char* accessCode);

    // Status tracking
    bool statusChanged;                   // True after any status change
    bool pauseStatus;                     // Prevent status from showing as changed, set in sketch to control when to update status
    bool keybusConnected, keybusChanged;  // True if data is detected on the Keybus
    byte accessCode[dscPartitions];
    bool accessCodeChanged[dscPartitions];
    bool accessCodePrompt;                // True if the panel is requesting an access code
    bool trouble, troubleChanged;
    bool powerTrouble, powerChanged;
    bool batteryTrouble, batteryChanged;
    bool keypadFireAlarm, keypadAuxAlarm, keypadPanicAlarm;
    bool ready[dscPartitions], readyChanged[dscPartitions];
    bool disabled[dscPartitions];
    bool armed[dscPartitions], armedAway[dscPartitions], armedStay[dscPartitions];
    bool noEntryDelay[dscPartitions], armedChanged[dscPartitions];
    bool alarm[dscPartitions], alarmChanged[dscPartitions];
    bool exitDelay[dscPartitions], exitDelayChanged[dscPartitions];
    byte exitState[dscPartitions], exitStateChanged[dscPartitions];
    bool entryDelay[dscPartitions], entryDelayChanged[dscPartitions];
    bool fire[dscPartitions], fireChanged[dscPartitions];
    bool openZonesStatusChanged;
    byte openZones[dscZones], openZonesChanged[dscZones];    // Zone status is stored in an array using 1 bit per zone, up to 64 zones
    bool alarmZonesStatusChanged;
    byte alarmZones[dscZones], alarmZonesChanged[dscZones];  // Zone alarm status is stored in an array using 1 bit per zone, up to 64 zones

    // panelData[] and moduleData[] store panel and keypad data in an array: command [0], stop bit by itself [1],
    // followed by the remaining data.  These can be accessed directly in the sketch to get data that is not already
    // tracked in the library.  See dscKeybusPrintData-RTOS.c for the currently known DSC commands and data.
    //
    // panelData[] example:
    //   Byte 0     Byte 2   Byte 3   Byte 4   Byte 5
    //   00000101 0 10000001 00000001 10010001 11000111 [0x05] Status lights: Ready Backlight | Partition ready
    //            ^ Byte 1 (stop bit)
    static byte panelData[dscReadSize];
    static volatile byte moduleData[dscReadSize];

    // status[] and lights[] store the current status message and LED state for each partition.  These can be accessed
    // directly in the sketch to get data that is not already tracked in the library.  See printPanelMessages() and
    // printPanelLights() in dscKeybusPrintData.cpp to see how this data translates to the status message and LED status.
    byte status[dscPartitions];
    byte lights[dscPartitions];

    // Process keypad and module data, returns true if data is available
    bool handleModule();

    // True if dscBufferSize needs to be increased
    static volatile bool bufferOverflow;

    // Timer interrupt function to capture data - declared as public for use by AVR Timer2
    static void dscDataInterrupt();

    // Deprecated
    bool handlePanel();               // Returns true if valid panel data is available.  Relabeled to loop()

  private:

    void processPanelStatus();
    void processPanelStatus0(byte partition, byte panelByte);
    void processPanelStatus2(byte partition, byte panelByte);
    void processPanelStatus4(byte partition, byte panelByte);
    void processPanel_0x27();
    void processPanel_0x2D();
    void processPanel_0x34();
    void processPanel_0x3E();
    void processPanel_0xA5();
    void processPanel_0xE6();
    void processPanel_0xE6_0x09();
    void processPanel_0xE6_0x0B();
    void processPanel_0xE6_0x0D();
    void processPanel_0xE6_0x0F();
    void processPanel_0xEB();

    void printPanelLights(byte panelByte);
    void printPanelMessages(byte panelByte);
    void printPanelBitNumbers(byte panelByte, byte startNumber);
    void printPanelStatus0(byte panelByte);
    void printPanelStatus1(byte panelByte);
    void printPanelStatus2(byte panelByte);
    void printPanelStatus3(byte panelByte);
    void printPanelStatus4(byte panelByte);
    void printPanelStatus14(byte panelByte);
    void printPanel_0x05();
    void printPanel_0x0A();
    void printPanel_0x11();
    void printPanel_0x16();
    void printPanel_0x1B();
    void printPanel_0x1C();
    void printPanel_0x27();
    void printPanel_0x28();
    void printPanel_0x2D();
    void printPanel_0x34();
    void printPanel_0x3E();
    void printPanel_0x4C();
    void printPanel_0x58();
    void printPanel_0x5D();
    void printPanel_0x63();
    void printPanel_0x64();
    void printPanel_0x69();
    void printPanel_0x75();
    void printPanel_0x7A();
    void printPanel_0x7F();
    void printPanel_0x82();
    void printPanel_0x87();
    void printPanel_0x8D();
    void printPanel_0x94();
    void printPanel_0xA5();
    void printPanel_0xB1();
    void printPanel_0xBB();
    void printPanel_0xC3();
    void printPanel_0xCE();
    void printPanel_0xD5();
    void printPanel_0xE6();
    void printPanel_0xE6_0x03();
    void printPanel_0xE6_0x09();
    void printPanel_0xE6_0x0B();
    void printPanel_0xE6_0x0D();
    void printPanel_0xE6_0x0F();
    void printPanel_0xE6_0x17();
    void printPanel_0xE6_0x18();
    void printPanel_0xE6_0x19();
    void printPanel_0xE6_0x1A();
    void printPanel_0xE6_0x1D();
    void printPanel_0xE6_0x20();
    void printPanel_0xE6_0x2B();
    void printPanel_0xE6_0x2C();
    void printPanel_0xE6_0x41();
    void printPanel_0xEB();

    void printModule_0x77();
    void printModule_0xBB();
    void printModule_0xDD();
    void printModule_Panel_0x11();
    void printModule_Panel_0xD5();
    void printModule_Notification();
    void printModule_Keys();

    bool validCRC();
    void writeKeys(const char * writeKeysArray);
    void setWriteKey(const char receivedKey);
    static void dscClockInterrupt();
    static bool redundantPanelData(byte previousCmd[], volatile byte currentCmd[], byte checkedBytes = dscReadSize);

    Stream* stream;
    const char* writeKeysArray;
    bool writeKeysPending;
    bool writeArm[dscPartitions];
    bool queryResponse;
    bool previousTrouble;
    bool previousKeybus;
    byte previousAccessCode[dscPartitions];
    byte previousLights[dscPartitions], previousStatus[dscPartitions];
    bool previousReady[dscPartitions];
    bool previousExitDelay[dscPartitions], previousEntryDelay[dscPartitions];
    byte previousExitState[dscPartitions];
    bool previousArmed[dscPartitions], previousArmedStay[dscPartitions];
    bool previousAlarm[dscPartitions];
    bool previousFire[dscPartitions];
    byte previousOpenZones[dscZones], previousAlarmZones[dscZones];

    static byte dscClockPin;
    static byte dscReadPin;
    static byte dscWritePin;
    static byte writeByte, writeBit;
    static bool virtualKeypad;
    static char writeKey;
    static byte panelBitCount, panelByteCount;
    static volatile bool writeKeyPending;
    static volatile bool writeAlarm, writeAsterisk, wroteAsterisk;
    static volatile bool moduleDataCaptured;
    static volatile unsigned long clockHighTime, keybusTime;
    static volatile byte panelBufferLength;
    static volatile byte panelBuffer[dscBufferSize][dscReadSize];
    static volatile byte panelBufferBitCount[dscBufferSize], panelBufferByteCount[dscBufferSize];
    static volatile byte moduleBitCount, moduleByteCount;
    static volatile byte currentCmd, statusCmd;
    static volatile byte isrPanelData[dscReadSize], isrPanelBitTotal, isrPanelBitCount, isrPanelByteCount;
    static volatile byte isrModuleData[dscReadSize], isrModuleBitTotal, isrModuleBitCount, isrModuleByteCount;
};

#endif  // dscKeybusInterface_h
