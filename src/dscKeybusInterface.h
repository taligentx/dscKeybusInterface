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

const byte dscZones = 8;      // Number of zones to process: 8, 16, 32
const byte dscReadSize = 13;  // Keybus data size limit

class dscKeybusInterface {

  public:

    // Initializes writes as disabled by default
    dscKeybusInterface(byte setClockPin, byte setReadPin, byte setWritePin = 255);

    void begin(Stream &_stream = Serial);             // Initializes the stream output to Serial by default
    bool handlePanel();                               // Returns true if valid panel data is available
    bool handleKeypad();                              // Returns true if valid keypad data is available
    static volatile bool writeReady;                  // Check if this is true before writing a key
    void write(const char receivedKey);               // Writes a single key
    void write(const char * receivedKeys);            // Writes multiple keys from a char array
    void printPanelBinary(bool printSpaces = true);   // Includes spaces between bytes by default
    void printPanelCommand();
    void printPanelMessage();
    void printKeypadBinary(bool printSpaces = true);  // Includes spaces between bytes by default
    void printKeypadMessage();

    // These can be configured in the sketch setup() before begin()
    bool processRedundantData;
    bool displayTrailingBits;

    // Panel time
    bool timeAvailable;             // True after the panel sends the first timestamp message
    char dscTime[17];               // Panel time in MM/DD/YYYY HH:MM format
    byte hour, minute, day, month;
    int year;

    // Status
    bool statusChanged;             // True after any status change
    bool partitionArmed, partitionArmedAway, partitionArmedStay, armedNoEntryDelay, partitionArmedChanged;
    bool partitionAlarm, partitionAlarmChanged;
    bool keypadFireAlarm, keypadAuxAlarm, keypadPanicAlarm;
    bool troubleStatus, troubleStatusChanged, previousTroubleStatus;
    bool exitDelay, exitDelayChanged, previousExitDelay;
    bool entryDelay, entryDelayChanged, previousEntryDelay;
    bool batteryTrouble, batteryTroubleChanged;
    bool powerTrouble, powerTroubleChanged;
    byte openZonesGroup1, previousOpenZonesGroup1;
    bool openZonesGroup1Changed, openZones[dscZones], openZonesChanged[dscZones];
    bool alarmZones[dscZones], alarmZonesChanged[dscZones], alarmZonesGroup1Changed;

    //
    static volatile byte panelData[dscReadSize];
    static volatile byte keypadData[dscReadSize];
    static bool processKeypadData;

    static void dscDataInterrupt();

  private:
    static void dscClockInterrupt();
    bool validCRC();
    bool redundantPanelData(byte previousCmd[]);
    void writeKeys(const char * writeKeysArray);

    void printPanel_0x05();
    void printPanel_0x27();
    void printPanel_0x0A();
    void printPanel_0x11();
    void printPanel_0x16();
    void printPanel_0x1C();
    void printPanel_0x4C();
    void printPanel_0x58();
    void printPanel_0x5D();
    void printPanel_0x64();
    void printPanel_0x75();
    void printPanel_0x7F();
    void printPanel_0x87();
    void printPanel_0x8D();
    void printPanel_0x94();
    void printPanel_0xA5();
    void printPanel_0xA5_Byte7_0xFF();
    void printPanel_0xA5_Byte7_0x00();
    void printPanel_0xB1();
    void printPanel_0xBB();
    void printPanel_0xC3();
    void printPanel_0xD5();

    void processPanel_0x05();
    void processPanel_0x27();
    void processPanel_0x0A();
    void processPanel_0x11();
    void processPanel_0x16();
    void processPanel_0x1C();
    void processPanel_0x4C();
    void processPanel_0x58();
    void processPanel_0x5D();
    void processPanel_0x64();
    void processPanel_0x75();
    void processPanel_0x7F();
    void processPanel_0x87();
    void processPanel_0x8D();
    void processPanel_0x94();
    void processPanel_0xA5();
    void processPanel_0xA5_Byte7_0xFF();
    void processPanel_0xA5_Byte7_0x00();
    void processPanel_0xB1();
    void processPanel_0xBB();
    void processPanel_0xC3();
    void processPanel_0xD5();

    void printKeypad_0x77();
    void printKeypad_0xBB();
    void printKeypad_0xDD();
    void printKeypad_0xFF_Byte2();
    void printKeypad_0xFF_Byte3();
    void printKeypad_0xFF_Byte4_0xFE();
    void printKeypad_0xFF_Byte5_0xFB();
    void printKeypad_0xFF_Panel_0xD5();

    Stream* stream;
    const char* writeKeysArray;
    bool writeKeysPending;

    static byte dscClockPin;
    static byte dscReadPin;
    static byte dscWritePin;
    static bool virtualKeypad;
    static char writeKey;
    static volatile bool writeAlarm, writeAsterisk, wroteAsterisk;
    static volatile bool keypadDataCaptured;
    static volatile unsigned long clockHighTime;
    static volatile bool dataComplete, dataOverflow;
    static volatile byte panelBitCount, panelByteCount;
    static volatile byte keypadBitCount, keypadByteCount;
    static volatile byte isrPanelData[dscReadSize], isrPanelBitTotal, isrPanelBitCount, isrPanelByteCount;
    static volatile byte isrKeypadData[dscReadSize], isrKeypadBitTotal, isrKeypadBitCount, isrKeypadByteCount;
};

#endif  // dscKeybusInterface_h
