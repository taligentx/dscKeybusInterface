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

const byte dscReadSize = 16;   // Maximum size of a Keybus command
const byte dscZones = 8;       // Maximum number of zone groups, 8 zones per group - requires 6 bytes of memory per zone group
const byte dscPartitions = 8;  // Maximum number of partitions - requires 18 bytes of memory per partition

// Number of commands to buffer if the sketch is busy - requires dscReadSize + 2 bytes of memory per command
#if defined(__AVR__)
const byte dscBufferSize = 10;
#elif defined(ESP8266)
const byte dscBufferSize = 50;
#endif


class dscKeybusInterface {

  public:

    // Initializes writes as disabled by default
    dscKeybusInterface(byte setClockPin, byte setReadPin, byte setWritePin = 255);

    void begin(Stream &_stream = Serial);             // Initializes the stream output to Serial by default
    bool handlePanel();                               // Returns true if valid panel data is available
    bool handleKeybus();                              // Returns true if valid keypad or module data is available
    static volatile bool writeReady;                  // True if the library is ready to write a key
    void write(const char receivedKey);               // Writes a single key
    void write(const char * receivedKeys);            // Writes multiple keys from a char array
    void printPanelBinary(bool printSpaces = true);   // Includes spaces between bytes by default
    void printPanelCommand();                         // Prints the panel command as hex
    void printPanelMessage();                         // Prints the decoded panel message
    void printKeybusBinary(bool printSpaces = true);  // Includes spaces between bytes by default
    void printKeybusMessage();                        // Prints the decoded keypad or module message

    // Sets the partition for virtual keypad
    static byte writePartition;

    // These can be configured in the sketch setup() before begin()
    bool hideKeypadDigits;          // Controls if keypad digits are hidden for publicly posted logs (default: false)
    bool processRedundantData;      // Controls if repeated periodic commands are processed and displayed (default: false)
    static bool processKeypadData;  // Controls if keypad and module data is processed and displayed (default: false)
    bool displayTrailingBits;       // Controls if bits read as the clock is reset are displayed, appears to be spurious data (default: false)

    // Panel time
    bool timeAvailable;             // True after the panel sends the first timestamped message
    byte hour, minute, day, month;
    int year;

    // Status tracking
    bool statusChanged;             // True after any status change
    bool accessCodePrompt;
    bool troubleStatus, troubleStatusChanged;
    bool powerTrouble, powerTroubleChanged;
    bool batteryTrouble, batteryTroubleChanged;
    bool keypadFireAlarm, keypadAuxAlarm, keypadPanicAlarm;
    bool partitionsArmed[dscPartitions], partitionsArmedAway[dscPartitions], partitionsArmedStay[dscPartitions];
    bool partitionsNoEntryDelay[dscPartitions], partitionsArmedChanged[dscPartitions];
    bool partitionsAlarm[dscPartitions], partitionsAlarmChanged[dscPartitions];
    bool partitionsExitDelay[dscPartitions], partitionsExitDelayChanged[dscPartitions];
    bool partitionsEntryDelay[dscPartitions], partitionsEntryDelayChanged[dscPartitions];
    bool partitionsFire[dscPartitions], partitionsFireChanged[dscPartitions];
    bool openZonesStatusChanged;
    byte openZones[dscZones], openZonesChanged[dscZones];    // Zone status is stored in an array using 1 bit per zone, up to 64 zones
    bool alarmZonesStatusChanged;
    byte alarmZones[dscZones], alarmZonesChanged[dscZones];  // Zone alarm status is stored in an array using 1 bit per zone, up to 64 zones

    // Panel and keypad data is stored in an array: command [0], stop bit by itself [1], followed by the remaining
    // data.  panelData[] and keybusData[] can be accessed directly within the sketch.
    //
    // panelData[] example:
    //   Byte 0     Byte 2   Byte 3   Byte 4   Byte 5
    //   00000101 0 10000001 00000001 10010001 11000111 [0x05] Status lights: Ready Backlight | Partition ready
    //            ^ Byte 1 (stop bit)
    static byte panelData[dscReadSize];
    static volatile byte keybusData[dscReadSize];

    // True if dscReadSize or dscBufferSize needs to be increased
    static volatile bool dataOverflow, bufferOverflow;

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

    void printKeybus_0x77();
    void printKeybus_0xBB();
    void printKeybus_0xDD();
    void printKeybus_Panel_0x11();
    void printKeybus_Panel_0xD5();
    void printKeybus_Notification();
    void printKeybus_Keys();

    bool validCRC();
    void writeKeys(const char * writeKeysArray);
    static void dscClockInterrupt();
    static bool redundantPanelData(byte previousCmd[], volatile byte currentCmd[], byte checkedBytes = dscReadSize);

    Stream* stream;
    const char* writeKeysArray;
    bool writeKeysPending;
    bool writeArm;
    bool queryResponse;
    bool previousTroubleStatus;
    bool previousPartitionsExitDelay[dscPartitions], previousPartitionsEntryDelay[dscPartitions];
    bool previousPartitionsArmed[dscPartitions], previousPartitionsAlarm[dscPartitions];
    bool previousPartitionsFire[dscPartitions];
    byte previousOpenZones[dscZones], previousAlarmZones[dscZones];

    static byte dscClockPin;
    static byte dscReadPin;
    static byte dscWritePin;
    static bool virtualKeypad;
    static char writeKey;
    static byte panelBitCount, panelByteCount;
    static volatile bool writeAlarm, writeAsterisk, wroteAsterisk;
    static volatile bool keybusDataCaptured;
    static volatile unsigned long clockHighTime;
    static volatile byte panelBufferLength;
    static volatile byte panelBuffer[dscBufferSize][dscReadSize];
    static volatile byte panelBitCountBuffer[dscBufferSize], panelByteCountBuffer[dscBufferSize];
    static volatile byte keybusBitCount, keybusByteCount;
    static volatile byte currentCmd, statusCmd;
    static volatile byte isrPanelData[dscReadSize], isrPanelBitTotal, isrPanelBitCount, isrPanelByteCount;
    static volatile byte isrKeybusData[dscReadSize], isrKeybusBitTotal, isrKeybusBitCount, isrKeybusByteCount;
};

#endif  // dscKeybusInterface_h
