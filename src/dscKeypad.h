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

#ifndef dscKeypad_h
#define dscKeypad_h

#include <Arduino.h>

const byte dscReadSize = 16;    // Maximum bytes of a Keybus command

class dscKeypadInterface {

  public:
    dscKeypadInterface(byte setClockPin, byte setReadPin, byte setWritePin);

    // Interface control
    void begin(Stream &_stream = Serial);             // Initializes the stream output to Serial by default
    bool loop();                                      // Returns true if valid panel data is available

    // Keypad key
    byte Key, KeyAvailable;

    bool Ready = true, Armed, Memory, Bypass, Trouble, Program, Fire, Backlight = true;
    bool Zone1, Zone2, Zone3, Zone4, Zone5, Zone6, Zone7, Zone8;

    // Status tracking
    bool keybusConnected, keybusChanged;  // True if data is detected on the Keybus

    // Panel Keybus commands
    byte panelCommand05[5] = {0x05, 0x81, 0x01, 0x10, 0xC7};              // Partition 1: Ready Backlight - Partition ready | Partition 2: disabled
    byte panelCommand16[5] = {0x16, 0x0E, 0x23, 0xF1, 0x38};              // Panel version: v2.3 | Zone wiring: NC | Code length: 4 digits | *8 programming: no
    byte panelCommand27[7] = {0x27, 0x81, 0x01, 0x10, 0xC7, 0x00, 0x80};  // Partition 1: Ready Backlight - Partition ready | Partition 2: disabled | Zones 1-8 open: none
    byte panelCommand4C[12] = {0x4C, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    byte panelCommand5D[7] = {0x5D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5D};  // Partition 1 | Status lights flashing: none | Zones 1-32 flashing: none
    byte panelCommandA5[8] = {0xA5, 0x18, 0x0E, 0xED, 0x80, 0x00, 0x00, 0x38};
    byte panelCommandD5[9] = {0xD5, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    byte panelCommandB1[10] = {0xB1, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xAD};

    /*
     * moduleData[] stores keypad data in an array: command [0], stop bit by itself [1], followed by the
     * remaining data.  These can be accessed directly in the sketch to get data that is not already tracked
     * in the library.  See dscKeybusPrintData.cpp for the currently known DSC commands and data.
     */
    static volatile byte moduleData[dscReadSize];

    // Timer interrupt function to capture data - declared as public for use by AVR Timer1
    static void dscClockInterrupt();

  private:
    Stream* stream;
    byte panelLights = 0x81, panelZones, previousLights = 0x81, previousZones;
    bool startupCycle = true;
    byte commandInterval = 5;   // Sets the milliseconds between panel commands
    unsigned long intervalStart;

    #if defined(ESP32)
    #if ESP_IDF_VERSION_MAJOR < 4
    static hw_timer_t * timer0;
    #endif
    static portMUX_TYPE timer0Mux;
    #endif

    static int clockInterval;
    static byte dscClockPin, dscReadPin, dscWritePin;
    static volatile byte keyData;
    static volatile byte alarmKeyData;
    static volatile bool commandReady, moduleDataDetected, moduleDataCaptured;
    static volatile bool alarmKeyDetected, alarmKeyCaptured, alarmKeyResponsePending;
    static volatile byte clockCycleCount, clockCycleTotal;
    static volatile byte panelCommand[dscReadSize], panelCommandByteCount, panelCommandByteTotal;
    static volatile byte isrPanelBitTotal, isrPanelBitCount;
    static volatile byte isrModuleData[dscReadSize], isrModuleBitTotal, isrModuleBitCount, isrModuleByteCount;
};

#endif // dscKeypad_h
