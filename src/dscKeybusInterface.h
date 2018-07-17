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

#ifndef dscKeybusInterface_h
#define dscKeybusInterface_h

#include <Arduino.h>


#if defined(__AVR__)
const byte dscPartitions = 4;   // Maximum number of partitions - requires 19 bytes of memory per partition
const byte dscZones = 4;        // Maximum number of zone groups, 8 zones per group - requires 6 bytes of memory per zone group
const byte dscBufferSize = 10;  // Number of commands to buffer if the sketch is busy - requires dscReadSize + 2 bytes of memory per command
#elif defined(ESP8266)
const byte dscPartitions = 8;
const byte dscZones = 8;
const byte dscBufferSize = 50;
#endif

const byte dscReadSize = 16;   // Maximum size of a Keybus command


class dscKeybusInterface {

  public:

    // Initializes writes as disabled by default
    dscKeybusInterface(byte setClockPin, byte setReadPin, byte setWritePin = 255);

    void begin(Stream &_stream = Serial);             // Initializes the stream output to Serial by default
    bool handlePanel();                               // Returns true if valid panel data is available
    bool handleModule();                              // Returns true if valid keypad or module data is available
    static volatile bool writeReady;                  // True if the library is ready to write a key
    void write(const char receivedKey);               // Writes a single key
    void write(const char * receivedKeys);            // Writes multiple keys from a char array
    void printPanelBinary(bool printSpaces = true);   // Includes spaces between bytes by default
    void printPanelCommand();                         // Prints the panel command as hex
    void printPanelMessage();                         // Prints the decoded panel message
    void printModuleBinary(bool printSpaces = true);  // Includes spaces between bytes by default
    void printModuleMessage();                        // Prints the decoded keypad or module message

    // Set to a partition number for virtual keypad
    static byte writePartition;

    // These can be configured in the sketch setup() before begin()
    bool hideKeypadDigits;          // Controls if keypad digits are hidden for publicly posted logs (default: false)
    bool processRedundantData;      // Controls if repeated periodic commands are processed and displayed (default: false)
    static bool processModuleData;  // Controls if keypad and module data is processed and displayed (default: false)
    bool displayTrailingBits;       // Controls if bits read as the clock is reset are displayed, appears to be spurious data (default: false)

    // Panel time
    bool timeAvailable;             // True after the panel sends the first timestamped message
    byte hour, minute, day, month;
    int year;

    // These contain the current LED state and status message for each partition based on command 0x05 for
    // partitions 1-4 and command 0x1B for partitions 5-8.  See printPanelLights() and printPanelMessages()
    // in dscKeybusPrintData.cpp to see how this data translates to the LED status and status message.
    byte status[dscPartitions];
    byte lights[dscPartitions];

    // Status tracking
    bool statusChanged;                   // True after any status change
    bool keybusConnected, keybusChanged;  // True if data is detected on the Keybus
    bool accessCodePrompt;                // True if the panel is requesting an access code
    bool trouble, troubleChanged;
    bool powerTrouble, powerChanged;
    bool batteryTrouble, batteryChanged;
    bool keypadFireAlarm, keypadAuxAlarm, keypadPanicAlarm;
    bool ready[dscPartitions], readyChanged[dscPartitions];
    bool armed[dscPartitions], armedAway[dscPartitions], armedStay[dscPartitions];
    bool noEntryDelay[dscPartitions], armedChanged[dscPartitions];
    bool alarm[dscPartitions], alarmChanged[dscPartitions];
    bool exitDelay[dscPartitions], exitDelayChanged[dscPartitions];
    bool entryDelay[dscPartitions], entryDelayChanged[dscPartitions];
    bool fire[dscPartitions], fireChanged[dscPartitions];
    bool openZonesStatusChanged;
    byte openZones[dscZones], openZonesChanged[dscZones];    // Zone status is stored in an array using 1 bit per zone, up to 64 zones
    bool alarmZonesStatusChanged;
    byte alarmZones[dscZones], alarmZonesChanged[dscZones];  // Zone alarm status is stored in an array using 1 bit per zone, up to 64 zones

    // Panel and keypad data is stored in an array: command [0], stop bit by itself [1], followed by the remaining
    // data.  panelData[] and moduleData[] can be accessed directly within the sketch.
    //
    // panelData[] example:
    //   Byte 0     Byte 2   Byte 3   Byte 4   Byte 5
    //   00000101 0 10000001 00000001 10010001 11000111 [0x05] Status lights: Ready Backlight | Partition ready
    //            ^ Byte 1 (stop bit)
    static byte panelData[dscReadSize];
    static volatile byte moduleData[dscReadSize];

    // True if dscBufferSize needs to be increased
    static volatile bool bufferOverflow;

    // Timer interrupt function to capture data - declared as public for use by AVR Timer2
    static void dscDataInterrupt();

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
    static void dscClockInterrupt();
    static bool redundantPanelData(byte previousCmd[], volatile byte currentCmd[], byte checkedBytes = dscReadSize);

    Stream* stream;
    const char* writeKeysArray;
    bool writeKeysPending;
    bool writeArm[dscPartitions];
    bool queryResponse;
    bool previousTrouble;
    bool previousKeybus;
    byte previousLights[dscPartitions], previousStatus[dscPartitions];
    bool previousReady[dscPartitions];
    bool previousExitDelay[dscPartitions], previousEntryDelay[dscPartitions];
    bool previousArmed[dscPartitions], previousAlarm[dscPartitions];
    bool previousFire[dscPartitions];
    byte previousOpenZones[dscZones], previousAlarmZones[dscZones];

    static byte dscClockPin;
    static byte dscReadPin;
    static byte dscWritePin;
    static byte writeByte, writeBit;
    static bool virtualKeypad;
    static char writeKey;
    static byte panelBitCount, panelByteCount;
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
