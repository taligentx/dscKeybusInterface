/*
    DSC Keybus Interface

    Functions used by the KeybusReader sketch to decode and print Keybus data, with
    documentation of each message - including portions that are currently undecoded
    and unknown.  Contributions to improve decoding are always welcome!

    https://github.com/taligentx/dscKeybusInterface

    panelData[] and moduleData[] store panel and keypad/module data in an array: command [0], stop bit by itself [1],
    followed by the remaining data.

    panelData[] example:
      Byte 0     Byte 2   Byte 3   Byte 4   Byte 5
      00000101 0 10000001 00000001 10010001 11000111 [0x05] Partition 1: Ready Backlight - Partition ready | Partition 2: disabled
               ^Byte 1 (stop bit)  ^Bit 7          ^Bit 0

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

 #include "dscKeybus.h"


/*
 *  printPanelMessage() checks the first byte of a message from the
 *  panel (panelData[0]) to process known commands.
 *
 *  Structure decoding status refers to whether all bits of the message have
 *  a known purpose.
 *
 *  Content decoding status refers to whether all values of the message are known.
 */
void dscKeybusInterface::printPanelMessage() {

  // Checks for errors on panel commands with CRC data
  switch (panelData[0]) {
    case 0x05:         // Skips panel commands without CRC data
    case 0x11:
    case 0x1B:
    case 0x1C:
    case 0x22:
    case 0x28:
    case 0x33:
    case 0x39:
    case 0x41:
    case 0x4C:
    case 0x57:
    case 0x58:
    case 0x70:
    case 0x94:
    case 0x9E:
    case 0xD5:
    case 0xE6: break;  // 0xE6 is checked separately as only some of its subcommands use CRC
    default: {         // Checks remaining panel commands
      if (!validCRC()) {
        stream->print(F("[CRC Error]"));
        return;
      }
    }
  }

  // Processes known panel commands from the first byte of the panel message: panelData[0]
  switch (panelData[0]) {
    case 0x05: printPanel_0x05(); return;           // Panel status: partitions 1-4 | Structure: complete | Content: *incomplete
    case 0x0A:
    case 0x0F: printPanel_0x0A_0F(); return;        // Panel status in alarm/programming, partitions 1-2 | Structure: complete | Content: *incomplete
    case 0x11: printPanel_0x11(); return;           // Module supervision query | Structure: complete | Content: complete
    case 0x16: printPanel_0x16(); return;           // Panel configuration | Structure: *incomplete | Content: *incomplete
    case 0x1B: printPanel_0x1B(); return;           // Panel status: partitions 5-8 | Structure: complete | Content: *incomplete
    case 0x1C: printPanel_0x1C(); return;           // Verify keypad Fire/Auxiliary/Panic | Structure: complete | Content: complete
    case 0x22:
    case 0x28:
    case 0x33:
    case 0x39: printPanel_0x22_28_33_39(); return;  // Zone expanders 0-3 query | Structure: complete | Content: complete
    case 0x27: printPanel_0x27(); return;           // Panel status and zones 1-8 status | Structure: complete | Content: *incomplete
    case 0x2D: printPanel_0x2D(); return;           // Panel status and zones 9-16 status | Structure: complete | Content: *incomplete
    case 0x34: printPanel_0x34(); return;           // Panel status and zones 17-24 status | Structure: complete | Content: *incomplete
    case 0x3E: printPanel_0x3E(); return;           // Panel status and zones 25-32 status | Structure: complete | Content: *incomplete
    case 0x41: printPanel_0x41(); return;           // Wireless module query | Structure: complete | Content: complete
    case 0x4C: printPanel_0x4C(); return;           // Module tamper query | Structure: complete | Content: *incomplete
    case 0x57: printPanel_0x57(); return;           // Wireless key query | Structure: complete | Content: *incomplete
    case 0x58: printPanel_0x58(); return;           // Module status query | Structure: complete | Content: *incomplete
    case 0x5D:
    case 0x63: printPanel_0x5D_63(); return;        // Flash panel lights: status and zones 1-32, partitions 1-2 | Structure: complete | Content: complete
    case 0x64: printPanel_0x64(); return;           // Beep, partition 1 | Structure: complete | Content: complete
    case 0x69: printPanel_0x69(); return;           // Beep, partition 2 | Structure: complete | Content: complete
    case 0x6E: printPanel_0x6E(); return;           // LCD keypad display | Structure: complete | Content: complete
    case 0x70: printPanel_0x70(); return;           // LCD keypad data query | Structure: complete | Content: complete
    case 0x75: printPanel_0x75(); return;           // Tone, partition 1 | Structure: complete | Content: complete
    case 0x7A: printPanel_0x7A(); return;           // Tone, partition 2 | Structure: complete | Content: complete
    case 0x7F: printPanel_0x7F(); return;           // Buzzer, partition 1 | Structure: complete | Content: complete
    case 0x82: printPanel_0x82(); return;           // Buzzer, partition 2 | Structure: complete | Content: complete
    case 0x87: printPanel_0x87(); return;           // PGM outputs | Structure: complete | Content: complete
    case 0x8D: printPanel_0x8D(); return;           // RF module programming - it sends data from panel to RF module after programming entry is done | Structure: *incomplete | Content: *incomplete
    case 0x94: printPanel_0x94(); return;           // Requesting and getting data from RF module to the panel | Structure: *incomplete | Content: *incomplete
    case 0x9E: printPanel_0x9E(); return;           // DLS query | Structure: complete | Content: complete
    case 0xA5: printPanel_0xA5(); return;           // Date, time, system status messages - partitions 1-2 | Structure: *incomplete | Content: *incomplete
    case 0xAA: printPanel_0xAA(); return;           // Event buffer messages | Structure: complete | Content: *incomplete
    case 0xB1: printPanel_0xB1(); return;           // Enabled zones 1-32 | Structure: complete | Content: complete
    case 0xBB: printPanel_0xBB(); return;           // Bell | Structure: *incomplete | Content: *incomplete
    case 0xC3: printPanel_0xC3(); return;           // Keypad and dialer status | Structure: *incomplete | Content: *incomplete
    case 0xCE: printPanel_0xCE(); return;           // Panel status | Structure: *incomplete | Content: *incomplete
    case 0xD5: printPanel_0xD5(); return;           // Keypad zone query | Structure: complete | Content: complete
    case 0xE6: printPanel_0xE6(); return;           // Extended status command split into multiple subcommands to handle up to 8 partitions/64 zones | Structure: *incomplete | Content: *incomplete
    case 0xEB: printPanel_0xEB(); return;           // Date, time, system status messages - partitions 1-8 | Structure: *incomplete | Content: *incomplete
    case 0xEC: printPanel_0xEC(); return;           // Event buffer messages | Structure: *incomplete | Content: *incomplete
    default: {
      stream->print("Unknown data");
      return;
    }
  }
}


// Processes keypad and module notifications and responses to panel queries
void dscKeybusInterface::printModuleMessage() {
  switch (moduleData[0]) {
    case 0xBB: printModule_0xBB(); return;  // Keypad fire alarm | Structure: complete | Content: complete
    case 0xDD: printModule_0xDD(); return;  // Keypad auxiliary alarm | Structure: complete | Content: complete
    case 0xEE: printModule_0xEE(); return;  // Keypad panic alarm | Structure: complete | Content: complete
  }

  stream->print(F("[Module/0x"));
  if (moduleCmd < 16) stream->print("0");
  stream->print(moduleCmd, HEX);

  if (moduleCmd == 0xE6) {
    stream->print(".");
    if (moduleSubCmd < 16) stream->print("0");
    stream->print(moduleSubCmd, HEX);
  }
  stream->print(F("] "));

  // Keypad and module responses to panel queries
  switch (moduleCmd) {
    case 0x05:
    case 0x0A:
    case 0x0F:
    case 0x1B:
    case 0x27:
    case 0x2D:
    case 0x3E: printModule_Status(); return;    // Module notifications sent during panel status commands | Structure: *incomplete | Content: *incomplete
    case 0x11: printModule_0x11(); return;      // Module supervision query response | Structure: *incomplete | Content: *incomplete
    case 0x41: printModule_0x41(); return;      // Wireless module query response | Structure: *incomplete | Content: *incomplete
    case 0x4C: printModule_0x4C(); return;      // Module tamper query response | Structure: *incomplete | Content: *incomplete
    case 0x57: printModule_0x57(); return;      // Wireless key query response | Structure: *incomplete | Content: *incomplete
    case 0x58: printModule_0x58(); return;      // Module status query response | Structure: *incomplete | Content: *incomplete
    case 0x70: printModule_0x70(); return;      // LCD keypad data entry | Structure: complete | Content: complete
    case 0x94: printModule_0x94(); return;      // Module programming response | Structure: *incomplete | Content: *incomplete
    case 0xD5: printModule_0xD5(); return;      // Keypad zone query response | Structure: *incomplete | Content: *incomplete
    case 0x22:
    case 0x28:
    case 0x33:
    case 0x39: printModule_Expander(); return;  // Zone expanders 1-3 response
    case 0xE6: {
      switch (moduleSubCmd) {
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x20:
        case 0x21: printModule_Status(); return;    // Module notifications sent during installer programming
        case 0x08:
        case 0x0A:
        case 0x0C:
        case 0x0E: printModule_Expander(); return;  // Zone expanders 4-7 response
      }
    }
    default: stream->print("Unknown data");
  }
}


/*
 *  Keypad status lights for panel commands: 0x05, 0x0A, 0x1B, 0x27, 0x2D, 0x34, 0x3E, 0x5D
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  This decodes the following byte as a status message by default.
 */
void dscKeybusInterface::printPanelLights(byte panelByte, bool printMessage) {
  if (panelData[panelByte] == 0) stream->print(F("none "));
  else {
    if (bitRead(panelData[panelByte], 0)) stream->print(F("Ready "));
    if (bitRead(panelData[panelByte], 1)) stream->print(F("Armed "));
    if (bitRead(panelData[panelByte], 2)) stream->print(F("Memory "));
    if (bitRead(panelData[panelByte], 3)) stream->print(F("Bypass "));
    if (bitRead(panelData[panelByte], 4)) stream->print(F("Trouble "));
    if (bitRead(panelData[panelByte], 5)) stream->print(F("Program "));
    if (bitRead(panelData[panelByte], 6)) stream->print(F("Fire "));
    if (bitRead(panelData[panelByte], 7)) stream->print(F("Backlight "));
  }

  if (printMessage) {
    stream->print(F("- "));
    printPanelMessages(panelByte + 1);
  }
}


/*
 *  Status messages for panel commands: 0x05, 0x0A, 0x1B, 0x27, 0x2D, 0x34, 0x3E
 *  Structure decoding: complete
 *  Content decoding: *incomplete
 */
void dscKeybusInterface::printPanelMessages(byte panelByte) {
  switch (panelData[panelByte]) {
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
    case 0x10: stream->print(F("Keypad lockout")); break;
    case 0x11: stream->print(F("Partition in alarm")); break;
    case 0x12: stream->print(F("Battery check in progress")); break;
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
    //case 0xC7: stream->print(F("Partition not available")); break;
    case 0xC8: stream->print(F("*2: Service required menu")); break;
    case 0xCD: stream->print(F("Downloading in progress")); break;
    case 0xCE: stream->print(F("Active camera monitor selection")); break;
    case 0xD0: stream->print(F("*2: Keypads with low batteries")); break;
    case 0xD1: stream->print(F("*2: Keyfobs with low batteries")); break;
    case 0xD4: stream->print(F("*2: Zones with RF Delinquency")); break;
    case 0xE4: stream->print(F("*8: Installer programming, 3 digits")); decimalInput = false; break;
    case 0xE5: stream->print(F("Keypad slot assignment")); break;
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
      if (panelData[panelByte] < 10) stream->print("0");
      stream->print(panelData[panelByte], HEX);
      break;
  }
}


/*
 *  Status messages set 0x00 for panel commands: 0xA5, 0xAA, 0xCE, 0xEB, 0xEC
 *  Structure decoding: complete
 *  Content decoding: likely incomplete - observed messages from logs have been decoded, but there are gaps in
      the numerical list of messages.
 *
 *  These commands use 1 byte for the status message, and appear to use bits 0,1 of the preceding byte to
 *  select from multiple sets of status messages, split into printPanelStatus0...printPanelStatus2.
 */
void dscKeybusInterface::printPanelStatus0(byte panelByte) {
  bool decoded = true;
  switch (panelData[panelByte]) {
    /*
     *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
     *  10100101 0 00011000 01001111 10110000 11101100 01001001 11111111 11110000 [0xA5] 2018.03.29 16:59 | Partition 1 | Duress alarm
     *  10100101 0 00011000 01001111 11001110 10111100 01001010 11111111 11011111 [0xA5] 2018.03.30 14:47 | Partition 1 | Disarmed after alarm in memory
     *  10100101 0 00011000 01001111 11001010 01000100 01001011 11111111 01100100 [0xA5] 2018.03.30 10:17 | Partition 1 | Partition in alarm
     *  10100101 0 00011000 01010000 01001001 10111000 01001100 11111111 01011001 [0xA5] 2018.04.02 09:46 | Partition 1 | Zone expander supervisory alarm
     *  10100101 0 00011000 01010000 01001010 00000000 01001101 11111111 10100011 [0xA5] 2018.04.02 10:00 | Partition 1 | Zone expander supervisory restored
     *  10100101 0 00011000 01001111 01110010 10011100 01001110 11111111 01100111 [0xA5] 2018.03.27 18:39 | Partition 1 | Keypad Fire alarm
     *  10100101 0 00011000 01001111 01110010 10010000 01001111 11111111 01011100 [0xA5] 2018.03.27 18:36 | Partition 1 | Keypad Aux alarm
     *  10100101 0 00011000 01001111 01110010 10001000 01010000 11111111 01010101 [0xA5] 2018.03.27 18:34 | Partition 1 | Keypad Panic alarm
     *  10100101 0 00010110 00010111 11101101 00010000 01010000 10010001 10110000 [0xA5] 2016.05.31 13:04 | Keypad Panic alarm
     *  10100101 0 00010001 01101101 01100000 00000100 01010001 11111111 11010111 [0xA5] 2011.11.11 00:01 | Partition 1 | Auxiliary input alarm
     *  10100101 0 00011000 01001111 01110010 10011100 01010010 11111111 01101011 [0xA5] 2018.03.27 18:39 | Partition 1 | Keypad Fire alarm restored
     *  10100101 0 00011000 01001111 01110010 10010000 01010011 11111111 01100000 [0xA5] 2018.03.27 18:36 | Partition 1 | Keypad Aux alarm restored
     *  10100101 0 00011000 01001111 01110010 10001000 01010100 11111111 01011001 [0xA5] 2018.03.27 18:34 | Partition 1 | Keypad Panic alarm restored
     *  10100101 0 00011000 01001111 11110110 00110100 10011000 11111111 11001101 [0xA5] 2018.03.31 22:13 | Partition 1 | Keypad lockout
     *  10100101 0 00011000 01001111 11101011 10100100 10111110 11111111 01011000 [0xA5] 2018.03.31 11:41 | Partition 1 | Armed partial: Zones bypassed
     *  10100101 0 00011000 01001111 11101011 00011000 10111111 11111111 11001101 [0xA5] 2018.03.31 11:06 | Partition 1 | Armed special: quick-arm/auto-arm/keyswitch/wireless key/DLS
     *  10100101 0 00010001 01101101 01100000 00101000 11100101 11111111 10001111 [0xA5] 2011.11.11 00:10 | Partition 1 | Auto-arm cancelled
     *  10100101 0 00011000 01001111 11110111 01000000 11100110 11111111 00101000 [0xA5] 2018.03.31 23:16 | Partition 1 | Disarmed special: keyswitch/wireless key/DLS
     *  10100101 0 00011000 01001111 01101111 01011100 11100111 11111111 10111101 [0xA5] 2018.03.27 15:23 | Partition 1 | Panel battery trouble
     *  10100101 0 00011000 01001111 10110011 10011000 11101000 11111111 00111110 [0xA5] 2018.03.29 19:38 | Partition 1 | Panel AC power trouble
     *  10100101 0 00011000 01001111 01110100 01010000 11101001 11111111 10111000 [0xA5] 2018.03.27 20:20 | Partition 1 | Bell trouble
     *  10100101 0 00011000 01001111 11000000 10001000 11101100 11111111 00111111 [0xA5] 2018.03.30 00:34 | Partition 1 | Telephone line trouble
     *  10100101 0 00011000 01001111 01101111 01110000 11101111 11111111 11011001 [0xA5] 2018.03.27 15:28 | Partition 1 | Panel battery restored
     *  10100101 0 00011000 01010000 00100000 01011000 11110000 11111111 01110100 [0xA5] 2018.04.01 00:22 | Partition 1 | Panel AC power restored
     *  10100101 0 00011000 01001111 01110100 01011000 11110001 11111111 11001000 [0xA5] 2018.03.27 20:22 | Partition 1 | Bell restored
     *  10100101 0 00011000 01001111 11000000 10001000 11110100 11111111 01000111 [0xA5] 2018.03.30 00:34 | Partition 1 | Telephone line restored
     *  10100101 0 00011000 01001111 11100001 01011000 11111111 11111111 01000011 [0xA5] 2018.03.31 01:22 | Partition 1 | System test
     *  Byte 0   1    2        3        4        5        6        7        8
     */
    // 0x09 - 0x28: Zone alarm, zones 1-32
    // 0x29 - 0x48: Zone alarm restored, zones 1-32
    case 0x49: stream->print(F("Duress alarm")); break;
    case 0x4A: stream->print(F("Disarmed: Alarm memory")); break;
    case 0x4B: stream->print(F("Recent closing alarm")); break;
    case 0x4C: stream->print(F("Zone expander supervisory alarm")); break;
    case 0x4D: stream->print(F("Zone expander supervisory restored")); break;
    case 0x4E: stream->print(F("Keypad Fire alarm")); break;
    case 0x4F: stream->print(F("Keypad Aux alarm")); break;
    case 0x50: stream->print(F("Keypad Panic alarm")); break;
    case 0x51: stream->print(F("PGM2 input alarm")); break;
    case 0x52: stream->print(F("Keypad Fire alarm restored")); break;
    case 0x53: stream->print(F("Keypad Aux alarm restored")); break;
    case 0x54: stream->print(F("Keypad Panic alarm restored")); break;
    case 0x55: stream->print(F("PGM2 input alarm restored")); break;
    // 0x56 - 0x75: Zone tamper, zones 1-32
    // 0x76 - 0x95: Zone tamper restored, zones 1-32
    case 0x98: stream->print(F("Keypad lockout")); break;
    // 0x99 - 0xBD: Armed: Access codes 1-34, 40-42
    case 0xBE: stream->print(F("Armed: Partial")); break;
    case 0xBF: stream->print(F("Armed: Special")); break;
    // 0xC0 - 0xE4: Disarmed: Access codes 1-34, 40-42
    case 0xE5: stream->print(F("Auto-arm cancelled")); break;
    case 0xE6: stream->print(F("Disarmed: Special")); break;
    case 0xE7: stream->print(F("Panel battery trouble")); break;
    case 0xE8: stream->print(F("Panel AC power trouble")); break;
    case 0xE9: stream->print(F("Bell trouble")); break;
    case 0xEA: stream->print(F("Fire zone trouble")); break;
    case 0xEB: stream->print(F("Panel aux supply trouble")); break;
    case 0xEC: stream->print(F("Telephone line trouble")); break;
    case 0xEF: stream->print(F("Panel battery restored")); break;
    case 0xF0: stream->print(F("Panel AC power restored")); break;
    case 0xF1: stream->print(F("Bell restored")); break;
    case 0xF2: stream->print(F("Fire zone trouble restored")); break;
    case 0xF3: stream->print(F("Panel aux supply restored")); break;
    case 0xF4: stream->print(F("Telephone line restored")); break;
    case 0xF7: stream->print(F("Phone 1 FTC")); break;
    case 0xF8: stream->print(F("Phone 2 FTC")); break;
    case 0xF9: stream->print(F("Event buffer threshold")); break;  //75% full since last DLS upload
    case 0xFA: stream->print(F("DLS lead-in")); break;
    case 0xFB: stream->print(F("DLS lead-out")); break;
    case 0xFE: stream->print(F("Periodic test transmission")); break;
    case 0xFF: stream->print(F("System test")); break;
    default: decoded = false;
  }
  if (decoded) return;

  /*
   *  Zone alarm, zones 1-32
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00011000 01001111 01001001 11011000 00001001 11111111 00110101 [0xA5] 2018.03.26 09:54 | Partition 1 | Zone alarm: 1
   *  10100101 0 00011000 01001111 01001010 00100000 00001110 11111111 10000011 [0xA5] 2018.03.26 10:08 | Partition 1 | Zone alarm: 6
   *  10100101 0 00011000 01001111 10010100 11001000 00010000 11111111 01110111 [0xA5] 2018.03.28 20:50 | Partition 1 | Zone alarm: 8
   *  Byte 0   1    2        3        4        5        6        7        8
   */
  if (panelData[panelByte] >= 0x09 && panelData[panelByte] <= 0x28) {
    stream->print("Zone alarm: ");
    printNumberOffset(panelByte, -8);
    return;
  }

  /*
   *  Zone alarm restored, zones 1-32
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00011000 01001111 10010100 11001100 00101001 11111111 10010100 [0xA5] 2018.03.28 20:51 | Partition 1 | Zone alarm restored: 1
   *  10100101 0 00011000 01001111 10010100 11010100 00101110 11111111 10100001 [0xA5] 2018.03.28 20:53 | Partition 1 | Zone alarm restored: 6
   *  10100101 0 00011000 01001111 10010100 11010000 00110000 11111111 10011111 [0xA5] 2018.03.28 20:52 | Partition 1 | Zone alarm restored: 8
   *  Byte 0   1    2        3        4        5        6        7        8
   */
  if (panelData[panelByte] >= 0x29 && panelData[panelByte] <= 0x48) {
    stream->print("Zone alarm restored: ");
    printNumberOffset(panelByte, -40);
    return;
  }

  /*
   *  Zone tamper, zones 1-32
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00000001 01000100 00100010 01011100 01010110 11111111 10111101 [0xA5] 2001.01.01 02:23 | Partition 1 | Zone tamper: 1
   *  10100101 0 00010001 01101101 01101011 10010000 01011011 11111111 01111000 [0xA5] 2011.11.11 11:36 | Partition 1 | Zone tamper: 6
   *  11101011 0 10000000 00100000 00101010 00010111 11010000 00000000 01010111 11111111 11110010 [0xEB] 2020.10.16 23:52 | Partition 8 | Zone tamper: 2
   *  Byte 0   1    2        3        4        5        6        7        8        9        10
   */
  if (panelData[panelByte] >= 0x56 && panelData[panelByte] <= 0x75) {
    stream->print("Zone tamper: ");
    printNumberOffset(panelByte, -85);
    return;
  }

  /*
   *  Zone tamper restored, zones 1-32
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00000001 01000100 00100010 01011100 01110110 11111111 11011101 [0xA5] 2001.01.01 02:23 | Partition 1 | Zone tamper restored: 1
   *  10100101 0 00010001 01101101 01101011 10010000 01111011 11111111 10011000 [0xA5] 2011.11.11 11:36 | Partition 1 | Zone tamper restored: 6
   *  Byte 0   1    2        3        4        5        6        7        8
   */
  if (panelData[panelByte] >= 0x76 && panelData[panelByte] <= 0x95) {
    stream->print("Zone tamper restored: ");
    printNumberOffset(panelByte, -117);
    return;
  }

  /*
   *  Armed by access codes 1-34, 40-42
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00011000 01001101 00001000 10010000 10011001 11111111 00111010 [0xA5] 2018.03.08 08:36 | Partition 1 | Armed: User code 1
   *  10100101 0 00011000 01001101 00001000 10111100 10111011 11111111 10001000 [0xA5] 2018.03.08 08:47 | Partition 1 | Armed: Master code 40
   *  Byte 0   1    2        3        4        5        6        7        8
   */
  if (panelData[panelByte] >= 0x99 && panelData[panelByte] <= 0xBD) {
    byte dscCode = panelData[panelByte] - 0x98;
    stream->print(F("Armed: "));
    printPanelAccessCode(dscCode);
    return;
  }

  /*
   *  Disarmed by access codes 1-34, 40-42
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00011000 01001101 00001000 11101100 11000000 11111111 10111101 [0xA5] 2018.03.08 08:59 | Partition 1 | Disarmed: User code 1
   *  10100101 0 00011000 01001101 00001000 10110100 11100010 11111111 10100111 [0xA5] 2018.03.08 08:45 | Partition 1 | Disarmed: Master code 40
   *  Byte 0   1    2        3        4        5        6        7        8
   */
  if (panelData[panelByte] >= 0xC0 && panelData[panelByte] <= 0xE4) {
    byte dscCode = panelData[panelByte] - 0xBF;
    stream->print(F("Disarmed: "));
    printPanelAccessCode(dscCode);
    return;
  }

  stream->print("Unknown data");
}


/*
 *  Status messages set 0x01 for panel commands: 0xA5, 0xAA, 0xCE, 0xEB, 0xEC
 *  Structure decoding: complete
 *  Content decoding: likely incomplete - observed messages from logs have been decoded, but there are gaps in
 *  the numerical list of messages.
 *
 *  These commands use 1 byte for the status message, and appear to use bits 0,1 of the preceding byte to
 *  select from multiple sets of status messages, split into printPanelStatus0...printPanelStatus3.
 */
void dscKeybusInterface::printPanelStatus1(byte panelByte) {
  switch (panelData[panelByte]) {
    /*
     *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
     *  10100101 0 00011000 01001111 11001010 10001001 00000011 11111111 01100001 [0xA5] 2018.03.30 10:34 | Partition 1 | Cross zone alarm
     *  10100101 0 00010001 01101101 01101010 00000001 00000100 11111111 10010001 [0xA5] 2011.11.11 10:00 | Partition 1 | Delinquency alarm
     *  10100101 0 00010001 01101101 01100000 10101001 00100100 00000000 01010000 [0xA5] 2011.11.11 00:42 | Partition 1 | Duress code 33
     *  10100101 0 00010001 01101101 01100000 10110101 00100101 00000000 01011101 [0xA5] 2011.11.11 00:45 | Partition 1 | Duress code 34
     *  10100101 0 00010001 01101101 01100000 00101001 00100110 00000000 11010010 [0xA5] 2011.11.11 00:10 | Partition 1 | Master code 40
     *  10100101 0 00010001 01101101 01100000 10010001 00100111 00000000 00111011 [0xA5] 2011.11.11 00:36 | Partition 1 | Supervisor code 41
     *  10100101 0 00010001 01101101 01100000 10111001 00101000 00000000 01100100 [0xA5] 2011.11.11 00:46 | Partition 1 | Supervisor code 42
     *  10100101 0 00011000 01001111 10100000 10011101 00101011 00000000 01110100 [0xA5] 2018.03.29 00:39 | Partition 1 | Armed: Auto-arm
     *  10100101 0 00011000 01001101 00001010 00001101 10101100 00000000 11001101 [0xA5] 2018.03.08 10:03 | Partition 1 | Exit *8 programming
     *  10100101 0 00011000 01001101 00001001 11100001 10101101 00000000 10100001 [0xA5] 2018.03.08 09:56 | Partition 1 | Enter *8 programming
     *  10100101 0 00010001 01101101 01100010 11001101 11010000 00000000 00100010 [0xA5] 2011.11.11 02:51 | Partition 1 | Command output 4
     *  10100101 0 00010110 01010110 00101011 11010001 11010010 00000000 11011111 [0xA5] 2016.05.17 11:52 | Partition 1 | Armed with no entry delay cancelled
     *  Byte 0   1    2        3        4        5        6        7        8
     */
    case 0x03: stream->print(F("Cross zone alarm")); return;
    case 0x04: stream->print(F("Delinquency alarm")); return;
    case 0x05: stream->print(F("Late to close")); return;
    // 0x24 - 0x28: Access codes 33-34, 40-42
    case 0x29: stream->print(F("Downloading forced answer")); return;
    case 0x2B: stream->print(F("Armed: Auto-arm")); return;
    // 0x2C - 0x4B: Zone battery restored, zones 1-32
    // 0x4C - 0x6B: Zone battery low, zones 1-32
    // 0x6C - 0x8B: Zone fault restored, zones 1-32
    // 0x8C - 0xAB: Zone fault, zones 1-32
    case 0xAC: stream->print(F("Exit installer programming")); return;
    case 0xAD: stream->print(F("Enter installer programming")); return;
    case 0xAE: stream->print(F("Walk test end")); return;
    case 0xAF: stream->print(F("Walk test begin")); return;
    // 0xB0 - 0xCF: Zones bypassed, zones 1-32
    case 0xD0: stream->print(F("Command output 4")); return;
    case 0xD1: stream->print(F("Exit fault pre-alert")); return;
    case 0xD2: stream->print(F("Armed: Entry delay")); return;
    case 0xD3: stream->print(F("Downlook remote trigger")); return;
  }

  /*
   *  Access codes 33-34, 40-42
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00010001 01101101 01100000 10101001 00100100 00000000 01010000 [0xA5] 2011.11.11 00:42 | Partition 1 | Duress code 33
   *  10100101 0 00010001 01101101 01100000 10110101 00100101 00000000 01011101 [0xA5] 2011.11.11 00:45 | Partition 1 | Duress code 34
   *  10100101 0 00010001 01101101 01100000 00101001 00100110 00000000 11010010 [0xA5] 2011.11.11 00:10 | Partition 1 | Master code 40
   *  10100101 0 00010001 01101101 01100000 10010001 00100111 00000000 00111011 [0xA5] 2011.11.11 00:36 | Partition 1 | Supervisor code 41
   *  10100101 0 00010001 01101101 01100000 10111001 00101000 00000000 01100100 [0xA5] 2011.11.11 00:46 | Partition 1 | Supervisor code 42
   *  Byte 0   1    2        3        4        5        6        7        8
   */
  if (panelData[panelByte] >= 0x24 && panelData[panelByte] <= 0x28) {
    byte dscCode = panelData[panelByte] - 0x03;
    printPanelAccessCode(dscCode);
    return;
  }

  /*
   *  Zone battery restored, zones 1-32
   */
  if (panelData[panelByte] >= 0x2C && panelData[panelByte] <= 0x4B) {
    stream->print(F("Zone battery restored: "));
    printNumberOffset(panelByte, -43);
    return;
  }

  /*
   *  Zone low battery, zones 1-32
   */
  if (panelData[panelByte] >= 0x4C && panelData[panelByte] <= 0x6B) {
    stream->print(F("Zone battery low: "));
    printNumberOffset(panelByte, -75);
    return;
  }

  /*
   *  Zone fault restored, zones 1-32
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00010001 01101101 01101011 01000001 01101100 11111111 00111010 [0xA5] 2011.11.11 11:16 | Partition 1 | Zone fault restored: 1
   *  10100101 0 00010001 01101101 01101011 01010101 01101101 11111111 01001111 [0xA5] 2011.11.11 11:21 | Partition 1 | Zone fault restored: 2
   *  10100101 0 00010001 01101101 01101011 10000101 01101111 11111111 10000001 [0xA5] 2011.11.11 11:33 | Partition 1 | Zone fault restored: 4
   *  10100101 0 00010001 01101101 01101011 10001001 01110000 11111111 10000110 [0xA5] 2011.11.11 11:34 | Partition 1 | Zone fault restored: 5
   *  Byte 0   1    2        3        4        5        6        7        8
   */
  if (panelData[panelByte] >= 0x6C && panelData[panelByte] <= 0x8B) {
    stream->print(F("Zone fault restored: "));
    printNumberOffset(panelByte, -107);
    return;
  }

  /*
   *  Zone fault, zones 1-32
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00010001 01101101 01101011 00111101 10001100 11111111 01010110 [0xA5] 2011.11.11 11:15 | Partition 1 | Zone fault: 1
   *  10100101 0 00010001 01101101 01101011 01010101 10001101 11111111 01101111 [0xA5] 2011.11.11 11:21 | Partition 1 | Zone fault: 2
   *  10100101 0 00010001 01101101 01101011 10000001 10001111 11111111 10011101 [0xA5] 2011.11.11 11:32 | Partition 1 | Zone fault: 4
   *  10100101 0 00010001 01101101 01101011 10001001 10010000 11111111 10100110 [0xA5] 2011.11.11 11:34 | Partition 1 | Zone fault: 5
   *  Byte 0   1    2        3        4        5        6        7        8
   */
  if (panelData[panelByte] >= 0x8C && panelData[panelByte] <= 0xAB) {
    stream->print(F("Zone fault: "));
    printNumberOffset(panelByte, -139);
    return;
  }

  /*
   *  Zones bypassed, zones 1-32
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00011000 01001111 10110001 10101001 10110001 00000000 00010111 [0xA5] 2018.03.29 17:42 | Partition 1 | Zone bypassed: 2
   *  10100101 0 00011000 01001111 10110001 11000001 10110101 00000000 00110011 [0xA5] 2018.03.29 17:48 | Partition 1 | Zone bypassed: 6
   *  Byte 0   1    2        3        4        5        6        7        8
   */
  if (panelData[panelByte] >= 0xB0 && panelData[panelByte] <= 0xCF) {
    stream->print(F("Zone bypassed: "));
    printNumberOffset(panelByte, -175);
    return;
  }

  stream->print("Unknown data");
}


/*
 *  Status messages set 0x02 for panel commands: 0xAA, 0xCE, 0xEB, 0xEC
 *  Structure decoding: complete
 *  Content decoding: likely incomplete - observed messages from logs have been decoded, but there are gaps in
 *  the numerical list of messages.
 *
 *  These commands use 1 byte for the status message, and appear to use bits 0,1 of the preceding byte to
 *  select from multiple sets of status messages, split into printPanelStatus0...printPanelStatus3.
 */
void dscKeybusInterface::printPanelStatus2(byte panelByte) {
  switch (panelData[panelByte]) {

    /*
     *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
     *  10100101 0 00011000 01001111 10101111 10000110 00101010 00000000 01101011 [0xA5] 2018.03.29 15:33 | Partition 1 | Quick exit
     *  10100101 0 00010001 01101101 01110101 00111010 01100011 00000000 00110101 [0xA5] 2011.11.11 21:14 | Partition 1 | Keybus fault restored
     *  10100101 0 00011000 01001111 11110111 01110110 01100110 00000000 11011111 [0xA5] 2018.03.31 23:29 | Partition 1 | Enter *1 zone bypass programming
     *  10100101 0 00010001 01101101 01100010 11001110 01101001 00000000 10111100 [0xA5] 2011.11.11 02:51 | Partition 1 | Command output 3
     *  10100101 0 00011000 01010000 01000000 00000010 10001100 00000000 11011011 [0xA5] 2018.04.02 00:00 | Partition 1 | Loss of system time
     *  10100101 0 00011000 01001111 10101110 00001110 10001101 00000000 01010101 [0xA5] 2018.03.29 14:03 | Partition 1 | Power on
     *  10100101 0 00011000 01010000 01000000 00000010 10001110 00000000 11011101 [0xA5] 2018.04.02 00:00 | Partition 1 | Panel factory default
     *  10100101 0 00011000 01001111 11101010 10111010 10010011 00000000 01000011 [0xA5] 2018.03.31 10:46 | Partition 1 | Disarmed by keyswitch
     *  10100101 0 00011000 01001111 11101010 10101110 10010110 00000000 00111010 [0xA5] 2018.03.31 10:43 | Partition 1 | Armed by keyswitch
     *  10100101 0 00011000 01001111 10100000 01100010 10011000 00000000 10100110 [0xA5] 2018.03.29 00:24 | Partition 1 | Armed by quick-arm
     *  10100101 0 00010001 01101101 01100000 00101110 10011001 00000000 01001010 [0xA5] 2011.11.11 00:11 | Partition 1 | Activate stay/away zones
     *  10100101 0 00011000 01001111 00101101 00011010 10011010 00000000 11101101 [0xA5] 2018.03.25 13:06 | Partition 1 | Armed: stay
     *  10100101 0 00011000 01001111 00101101 00010010 10011011 00000000 11100110 [0xA5] 2018.03.25 13:04 | Partition 1 | Armed: away
     *  10100101 0 00011000 01001111 00101101 10011010 10011100 00000000 01101111 [0xA5] 2018.03.25 13:38 | Partition 1 | Armed with no entry delay
     *  10100101 0 00011000 01001111 00101100 11011110 11000011 00000000 11011001 [0xA5] 2018.03.25 12:55 | Partition 1 | Enter *5 programming
     *  10100101 0 00011000 01001111 00101110 00000010 11100110 00000000 00100010 [0xA5] 2018.03.25 14:00 | Partition 1 | Enter *6 programming
     *  Byte 0   1    2        3        4        5        6        7        8
     */
    case 0x2A: stream->print(F("Quick exit")); return;
    case 0x63: stream->print(F("Keybus fault restored")); return;
    case 0x64: stream->print(F("Keybus fault")); return;
    case 0x66: stream->print(F("*1: Zone bypass")); return;
    // 0x67 - 0x69: *7: Command output 1-3
    case 0x8C: stream->print(F("Cold start")); return;
    case 0x8D: stream->print(F("Warm start")); return;
    case 0x8E: stream->print(F("Panel factory default")); return;
    case 0x91: stream->print(F("Swinger shutdown")); return;
    case 0x93: stream->print(F("Disarmed: Keyswitch")); return;
    case 0x96: stream->print(F("Armed: Keyswitch")); return;
    case 0x97: stream->print(F("Armed: Keypad away")); return;
    case 0x98: stream->print(F("Armed: Quick-arm")); return;
    case 0x99: stream->print(F("Activate stay/away zones")); return;
    case 0x9A: stream->print(F("Armed: Stay")); return;
    case 0x9B: stream->print(F("Armed: Away")); return;
    case 0x9C: stream->print(F("Armed: No entry delay")); return;
    // 0x9E - 0xC2: *1: Access codes 1-34, 40-42
    // 0xC3 - 0xC5: *5: Access codes 40-42
    // 0xC6 - 0xE5: Access codes 1-34, 40-42
    // 0xE6 - 0xE8: *6: Access codes 40-42
    // 0xE9 - 0xF0: Keypad restored: Slots 1-8
    // 0xF1 - 0xF8: Keypad trouble: Slots 1-8
    // 0xF9 - 0xFE: Zone expander restored: 1-6
    case 0xFF: stream->print(F("Zone expander trouble: 1")); return;
  }

  /*
   *  *7: Command output 1-3
   */
  if (panelData[panelByte] >= 0x67 && panelData[panelByte] <= 0x69) {
    stream->print(F("Command output: "));
    printNumberOffset(panelByte, -0x66);
    return;
  }

  /*
   *  *1: Access codes 1-34, 40-42
   */
  if (panelData[panelByte] >= 0x9E && panelData[panelByte] <= 0xC2) {
    byte dscCode = panelData[panelByte] - 0x9D;
    stream->print(F("*1: "));
    printPanelAccessCode(dscCode);
    return;
  }

  /*
   *  *5: Access codes 40-42
   */
  if (panelData[panelByte] >= 0xC3 && panelData[panelByte] <= 0xC5) {
    byte dscCode = panelData[panelByte] - 0xA0;
    stream->print(F("*5: "));
    printPanelAccessCode(dscCode);
    return;
  }

  /*
   *  Access codes 1-32
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00010001 01101101 01100000 00111110 11000110 00000000 10000111 [0xA5] 2011.11.11 00:15 | Partition 1 | User code 1
   *  10100101 0 00010001 01101101 01100000 01111010 11100101 00000000 11100010 [0xA5] 2011.11.11 00:30 | Partition 1 | User code 32
   *  Byte 0   1    2        3        4        5        6        7        8
   */
  if (panelData[panelByte] >= 0xC6 && panelData[panelByte] <= 0xE5) {
    byte dscCode = panelData[panelByte] - 0xC5;
    printPanelAccessCode(dscCode);
    return;
  }

  /*
   *  *6: Access codes 40-42
   */
  if (panelData[panelByte] >= 0xE6 && panelData[panelByte] <= 0xE8) {
    byte dscCode = panelData[panelByte] - 0xC3;
    stream->print(F("*6: "));
    printPanelAccessCode(dscCode);
    return;
  }

  /*
   *  Keypad restored: Slots 1-8
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00010001 01101101 01110100 10001110 11101001 11111111 00001101 [0xA5] 2011.11.11 20:35 | Partition 1 | Keypad restored: Slot 1
   *  10100101 0 00010001 01101101 01110100 00110010 11110000 11111111 10111000 [0xA5] 2011.11.11 20:12 | Partition 1 | Keypad restored: Slot 8
   *  Byte 0   1    2        3        4        5        6        7        8
   */
  if (panelData[panelByte] >= 0xE9 && panelData[panelByte] <= 0xF0) {
    stream->print(F("Keypad restored: Slot "));
    printNumberOffset(panelByte, -232);
    return;
  }

  /*
   *  Keypad trouble: Slots 1-8
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00010001 01101101 01110100 10000110 11110001 11111111 00001101 [0xA5] 2011.11.11 20:33 | Partition 1 | Keypad trouble: Slot 1
   *  10100101 0 00010001 01101101 01110100 00101110 11111000 11111111 10111100 [0xA5] 2011.11.11 20:11 | Partition 1 | Keypad trouble: Slot 8
   *  Byte 0   1    2        3        4        5        6        7        8
   */
  if (panelData[panelByte] >= 0xF1 && panelData[panelByte] <= 0xF8) {
    stream->print(F("Keypad trouble: Slot "));
    printNumberOffset(panelByte, -240);
    return;
  }

  /*
   *  Zone expander restored: 1-6
   */
  if (panelData[panelByte] >= 0xF9 && panelData[panelByte] <= 0xFE) {
    stream->print(F("Zone expander restored: "));
    printNumberOffset(panelByte, -248);
    return;
  }

  stream->print("Unknown data");
}


/*
 *  Status messages set 0x03 for panel commands: 0xAA, 0xCE, 0xEB, 0xEC
 *  Structure decoding: complete
 *  Content decoding: likely incomplete - observed messages from logs have been decoded, but there are gaps in
 *  the numerical list of messages.
 *
 *  These commands use 1 byte for the status message, and appear to use bits 0,1 of the preceding byte to
 *  select from multiple sets of status messages, split into printPanelStatus0...printPanelStatus3.
 *
 *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
 *  10100101 0 00100000 00101010 01100000 10111111 01000010 11111111 01001111 [0xA5] 2020.10.19 00:47 | PC/RF5132: Tamper
 *  10100101 0 00100000 00101010 01100000 10111111 01000001 11111111 01001110 [0xA5] 2020.10.19 00:47 | PC/RF5132: Tamper restored
 *  10100101 0 00100000 00001001 10000000 11010011 01000100 11111111 01100100 [0xA5] 2020.02.12 00:52 | PC5208: Tamper
 *  10100101 0 00100000 00101010 11000000 11011111 01010001 11111111 11011110 [0xA5] 2020.10.22 00:55 | Module tamper restored: Slot 16
 *  10100101 0 00100000 00101010 11000000 11011111 01010010 11111111 11011111 [0xA5] 2020.10.22 00:55 | Module tamper: Slot 16
 *  Byte 0   1    2        3        4        5        6        7        8
 */
void dscKeybusInterface::printPanelStatus3(byte panelByte) {
  switch (panelData[panelByte]) {
    case 0x05: stream->print(F("PC/RF5132: Supervisory restored")); return;
    case 0x06: stream->print(F("PC/RF5132: Supervisory trouble")); return;
    case 0x09: stream->print(F("PC5204: Supervisory restored")); return;
    case 0x0A: stream->print(F("PC5204: Supervisory trouble")); return;
    case 0x17: stream->print(F("Zone expander restored: 7")); return;
    case 0x18: stream->print(F("Zone expander trouble: 7")); return;
    // 0x25 - 0x2C: Keypad tamper restored, slots 1-8
    // 0x2D - 0x34: Keypad tamper, slots 1-8
    // 0x35 - 0x3A: Module tamper restored, slots 9-14
    // 0x3B - 0x40: Module tamper, slots 9-14
    case 0x41: stream->print(F("PC/RF5132: Tamper restored")); return;
    case 0x42: stream->print(F("PC/RF5132: Tamper")); return;
    case 0x43: stream->print(F("PC5208: Tamper restored")); return;
    case 0x44: stream->print(F("PC5208: Tamper")); return;
    case 0x45: stream->print(F("PC5204: Tamper restored")); return;
    case 0x46: stream->print(F("PC5204: Tamper")); return;
    case 0x51: stream->print(F("Zone expander tamper restored: 7")); return;
    case 0x52: stream->print(F("Zone expander tamper: 7")); return;
    case 0xB3: stream->print(F("PC5204: Battery restored")); return;
    case 0xB4: stream->print(F("PC5204: Battery trouble")); return;
    case 0xB5: stream->print(F("PC5204: Aux supply restored")); return;
    case 0xB6: stream->print(F("PC5204: Aux supply trouble")); return;
    case 0xB7: stream->print(F("PC5204: Output 1 restored")); return;
    case 0xB8: stream->print(F("PC5204: Output 1 trouble")); return;
    case 0xFF: stream->print(F("Extended status")); return;
  }

  /*
   *  Zone expander trouble: 2-6
   */
  if (panelData[panelByte] <= 0x04) {
    stream->print(F("Zone expander trouble: "));
    printNumberOffset(panelByte, 2);
    return;
  }

  /*
   *  Keypad tamper restored: 1-8
   */
  if (panelData[panelByte] >= 0x25 && panelData[panelByte] <= 0x2C) {
    stream->print(F("Keypad tamper restored: "));
    printNumberOffset(panelByte, -0x24);
    return;
  }

  /*
   *  Keypad tamper: 1-8
   */
  if (panelData[panelByte] >= 0x2D && panelData[panelByte] <= 0x34) {
    stream->print(F("Keypad tamper: "));
    printNumberOffset(panelByte, -0x2C);
    return;
  }

  /*
   *  Zone expander tamper restored: 1-6
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00100000 00101010 11000110 00101011 00110101 11111111 00010100 [0xA5] 2020.10.22 06:10 | Zone expander tamper restored: 1
   *  10100101 0 00100000 00101010 11000000 00101111 00111010 11111111 00010111 [0xA5] 2020.10.22 00:11 | Zone expander tamper restored: 6
   *  11101011 0 00000000 00100000 00101010 11000110 00101000 00000011 00110101 11111111 01011010 [0xEB] 2020.10.22 06:10 | Zone expander tamper restored: 1
   *  Byte 0   1    2        3        4        5        6        7        8        9        10
   */
  if (panelData[panelByte] >= 0x35 && panelData[panelByte] <= 0x3A) {
    stream->print(F("Zone expander tamper restored: "));
    printNumberOffset(panelByte, -52);
    return;
  }

  /*
   *  Zone expander tamper: 1-6
   *
   *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
   *  10100101 0 00100000 00101010 11000110 00101011 00111011 11111111 00011010 [0xA5] 2020.10.22 06:10 | Zone expander tamper: 1
   *  10100101 0 00100000 00101010 11000000 00101111 01000000 11111111 00011101 [0xA5] 2020.10.22 00:11 | Zone expander tamper: 6
   *  11101011 0 00000000 00100000 00101010 11000110 00101000 00000011 00111011 11111111 01100000 [0xEB] 2020.10.22 06:10 | Zone expander tamper: 1
   *  Byte 0   1    2        3        4        5        6        7        8        9        10
   */
  if (panelData[panelByte] >= 0x3B && panelData[panelByte] <= 0x40) {
    stream->print(F("Zone expander tamper: "));
    printNumberOffset(panelByte, -58);
    return;
  }

  stream->print("Unknown data");
}


/*
 *  Status messages set 0x04 for panel commands: 0xEB, 0xEC
 *  Structure decoding: complete
 *  Content decoding: likely incomplete - observed messages from logs have been decoded, but there are gaps in
 *  the numerical list of messages.
 *
 *  These commands use 1 byte for the status message, and appear to use the preceding byte to select
 *  from multiple sets of status messages, split into printPanelStatus4...printPanelStatus1B.
 *
 *  Command   Partition YYY1YYY2   MMMMDD DDDHHHHH MMMMMM             Status             CRC
 *  11101011 0 00000001 00011000 00011000 10001111 00101000 00000100 00000000 10010001 01101000 [0xEB] 2018.06.04 15:10 | Partition 1 | Zone alarm: 33
 *  11101011 0 00000001 00000001 00000100 01100000 00010100 00000100 01000000 10000001 00101010 [0xEB] 2001.01.03 00:05 | Partition 1 | Zone tamper: 33
 *  11101011 0 00000001 00000001 00000100 01100000 00001000 00000100 01011111 10000001 00111101 [0xEB] 2001.01.03 00:02 | Partition 1 | Zone tamper: 64
 *  11101011 0 00000001 00000001 00000100 01100000 00011000 00000100 01100000 11111111 11001100 [0xEB] 2001.01.03 00:06 | Partition 1 | Zone tamper restored: 33
 *  Byte 0   1    2        3        4        5        6        7        8        9        10
 */
void dscKeybusInterface::printPanelStatus4(byte panelByte) {
  switch (panelData[panelByte]) {
    case 0x86: stream->print(F("Periodic test with trouble")); return;
    case 0x87: stream->print(F("Exit fault")); return;
    case 0x89: stream->print(F("Alarm cancelled")); return;
  }

  if (panelData[panelByte] <= 0x1F) {
    stream->print("Zone alarm: ");
    printNumberOffset(panelByte, 33);
  }

  else if (panelData[panelByte] >= 0x20 && panelData[panelByte] <= 0x3F) {
    stream->print("Zone alarm restored: ");
    printNumberOffset(panelByte, 1);
  }

  else if (panelData[panelByte] >= 0x40 && panelData[panelByte] <= 0x5F) {
    stream->print("Zone tamper: ");
    printNumberOffset(panelByte, -31);
  }

  else if (panelData[panelByte] >= 0x60 && panelData[panelByte] <= 0x7F) {
    stream->print("Zone tamper restored: ");
    printNumberOffset(panelByte, -63);
  }

  else stream->print("Unknown data");
}


/*
 *  Status messages set 0x05 for panel commands: 0xEB, 0xEC
 *  Structure decoding: complete
 *  Content decoding: likely incomplete - observed messages from logs have been decoded, but there are gaps in
 *  the numerical list of messages.
 *
 *  These commands use 1 byte for the status message, and appear to use the preceding byte to select
 *  from multiple sets of status messages, split into printPanelStatus4...printPanelStatus1B.
 */
void dscKeybusInterface::printPanelStatus5(byte panelByte) {

  /*
   *  Armed by access codes 35-95
   *  0x00 - 0x04: Access codes 35-39
   *  0x05 - 0x39: Access codes 43-95
   */
  if (panelData[panelByte] <= 0x39) {
    byte dscCode = panelData[panelByte] + 0x23;
    stream->print(F("Armed: "));
    printPanelAccessCode(dscCode, false);
    return;
  }

  /*
   *  Disarmed by access codes 35-95
   *  0x3A - 0x3E: Access codes 35-39
   *  0x3F - 0x73: Access codes 43-95
   */
  if (panelData[panelByte] >= 0x3A && panelData[panelByte] <= 0x73) {
    byte dscCode = panelData[panelByte] - 0x17;
    stream->print(F("Disarmed: "));
    printPanelAccessCode(dscCode, false);
    return;
  }

  stream->print("Unknown data");
}


/*
 *  Status messages set 0x14 for panel commands: 0xEB, 0xEC
 *  Structure decoding: complete
 *  Content decoding: likely incomplete - observed messages from logs have been decoded, but there are gaps in
 *  the numerical list of messages.
 *
 *  These commands use 1 byte for the status message, and appear to use the preceding byte to select
 *  from multiple sets of status messages, split into printPanelStatus4...printPanelStatus1B.
 */
void dscKeybusInterface::printPanelStatus14(byte panelByte) {
  switch (panelData[panelByte]) {
    case 0xC0: stream->print(F("TLink com fault")); return;
    case 0xC2: stream->print(F("Tlink network fault")); return;
    case 0xC4: stream->print(F("TLink receiver trouble")); return;
    case 0xC5: stream->print(F("TLink receiver restored")); return;
  }

  printUnknownData();
}


/*
 *  Status messages set 0x16 for panel commands: 0xEB, 0xEC
 *  Structure decoding: complete
 *  Content decoding: likely incomplete - observed messages from logs have been decoded, but there are gaps in
 *  the numerical list of messages.
 *
 *  These commands use 1 byte for the status message, and appear to use the preceding byte to select
 *  from multiple sets of status messages, split into printPanelStatus4...printPanelStatus1B.
 */
void dscKeybusInterface::printPanelStatus16(byte panelByte) {
  switch (panelData[panelByte]) {
    case 0x80: stream->print(F("Trouble acknowledged")); return;
    case 0x81: stream->print(F("RF delinquency trouble")); return;
    case 0x82: stream->print(F("RF delinquency restore")); return;
  }

  printUnknownData();
}


/*
 *  Status messages set 0x17 for panel commands: 0xEB, 0xEC
 *  Structure decoding: complete
 *  Content decoding: likely incomplete - observed messages from logs have been decoded, but there are gaps in
 *  the numerical list of messages.
 *
 *  These commands use 1 byte for the status message, and appear to use the preceding byte to select
 *  from multiple sets of status messages, split into printPanelStatus4...printPanelStatus1B.
 */
void dscKeybusInterface::printPanelStatus17(byte panelByte) {

  /*
   *  *1: Access codes 35-95
   *  0x4A - 0x83: *1: Access codes 35-39, 43-95
   */
  if (panelData[panelByte] >= 0x4A && panelData[panelByte] <= 0x83) {
    byte dscCode = panelData[panelByte] - 0x27;
    stream->print(F("*1: "));
    printPanelAccessCode(dscCode, false);
    return;
  }

  /*
   *  *2: Access codes 1-95
   *  0x00 - 0x24: *2: Access code 1-32, 40-42
   *  0x84 - 0xBD: *2: Access codes 35-39, 43-95
   */
  if (panelData[panelByte] <= 0x24) {
    byte dscCode = panelData[panelByte] + 1;
    stream->print(F("*2: "));
    printPanelAccessCode(dscCode);
    return;
  }

  if (panelData[panelByte] >= 0x84 && panelData[panelByte] <= 0xBD) {
    byte dscCode = panelData[panelByte] - 0x61;
    stream->print(F("*2: "));
    printPanelAccessCode(dscCode, false);
    return;
  }

  /*
   *  *3: Access codes 1-95
   *  0x25 - 0x49: *3: Access code 1-32, 40-42
   *  0xBE - 0xF7: *3: Access codes 35-39, 43-95
   */
  if (panelData[panelByte] >= 0x25 && panelData[panelByte] <= 0x49) {
    byte dscCode = panelData[panelByte] - 0x24;
    stream->print(F("*3: "));
    printPanelAccessCode(dscCode);
    return;
  }

  if (panelData[panelByte] >= 0xBE && panelData[panelByte] <= 0xF7) {
    byte dscCode = panelData[panelByte] - 0x9B;
    stream->print(F("*3: "));
    printPanelAccessCode(dscCode, false);
    return;
  }

  stream->print("Unknown data");
}


/*
 *  Status messages set 0x18 for panel commands: 0xEB, 0xEC
 *  Structure decoding: complete
 *  Content decoding: likely incomplete - observed messages from logs have been decoded, but there are gaps in
 *  the numerical list of messages.
 *
 *  These commands use 1 byte for the status message, and appear to use the preceding byte to select
 *  from multiple sets of status messages, split into printPanelStatus4...printPanelStatus1B.
 */
void dscKeybusInterface::printPanelStatus18(byte panelByte) {

  /*
   *  *7/User/Auto-arm cancel by access codes 35-95
   *
   *  0x00 - 0x04: *7, * Access codes 35-39
   *  0x05 - 0x39: *7, * Access codes 43-95
   */
  if (panelData[panelByte] <= 0x39) {
    byte dscCode = panelData[panelByte] + 0x23;
    printPanelAccessCode(dscCode, false);
    return;
  }

  /*
   *  *5: Access codes 1-95
   *
   *  0x3A - 0x60: *5: Access codes 1-39
   *  0x61 - 0x95: *5: Access codes 43-95
   */
  if (panelData[panelByte] >= 0x3A && panelData[panelByte] <= 0x95) {
    byte dscCode = panelData[panelByte] - 0x39;
    stream->print(F("*5: "));
    printPanelAccessCode(dscCode, false);
    return;
  }

  /*
   *  *6: Access codes 1-95
   *
   *  0x96 - 0xBC: *6: Access codes 1-39
   *  0xBD - 0xF1: *6: Access codes 43-95
   */
  if (panelData[panelByte] >= 0x96 && panelData[panelByte] <= 0xF1) {
    byte dscCode = panelData[panelByte] - 0x95;
    stream->print(F("*6: "));
    printPanelAccessCode(dscCode, false);
    return;
  }

  stream->print("Unknown data");
}


/*
 *  Status messages set 0x1B for panel commands: 0xEB, 0xEC
 *  Structure decoding: complete
 *  Content decoding: likely incomplete - observed messages from logs have been decoded, but there are gaps in
 *  the numerical list of messages.
 *
 *  These commands use 1 byte for the status message, and appear to use the preceding byte to select
 *  from multiple sets of status messages, split into printPanelStatus4...printPanelStatus1B.
 */
void dscKeybusInterface::printPanelStatus1B(byte panelByte) {
  switch (panelData[panelByte]) {
    case 0xF1: stream->print(F("System reset transmission")); return;
  }

  printUnknownData();
}


/*
 *  0x05: Status - partitions 1-4
 *  Interval: constant
 *  CRC: no
 *  Structure decoding: complete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Partition 1 lights, printPanelLights()
 *  Byte 3: Partition 1 status, printPanelMessages()
 *  Byte 4: Partition 2 lights
 *  Byte 5: Partition 2 status
 *
 *  Later generation panels:
 *  Byte 6: Partition 3 lights
 *  Byte 7: Partition 3 status
 *  Byte 8: Partition 4 lights
 *  Byte 9: Partition 4 status
 *
 *  Early generation panels:
 *  Command    1:Lights 1:Status 2:Lights 2:Status
 *  00000101 0 10000001 00000001 10010001 11000111 [0x05] Partition 1: Ready Backlight - Partition ready | Partition 2: disabled
 *  00000101 0 10010000 00000011 10010001 11000111 [0x05] Partition 1: Trouble Backlight - Zones open | Partition 2: disabled
 *  00000101 0 10001010 00000100 10010001 11000111 [0x05] Partition 1: Armed Bypass Backlight - Armed stay | Partition 2: disabled
 *  00000101 0 10000010 00000101 10010001 11000111 [0x05] Partition 1: Armed Backlight - Armed away | Partition 2: disabled
 *  00000101 0 10000010 00010001 10010001 11000111 [0x05] Partition 1: Armed Backlight - Partition in alarm | Partition 2: disabled
 *  00000101 0 10000001 00111110 10010001 11000111 [0x05] Partition 1: Ready Backlight - Partition disarmed | Partition 2: disabled
 *  Byte 0   1    2        3        4        5
 *
 *  Later generation panels:
 *  Command    1:Lights 1:Status 2:Lights 2:Status 3:Lights 3:Status 4:Lights 4:Status
 *  00000101 0 10000000 00000011 10000010 00000101 10000010 00000101 00000000 11000111 [0x05] Partition 1: Backlight - Zones open | Partition 2: Armed Backlight - Armed away | Partition 3: Armed Backlight - Armed away | Partition 4: disabled
 *  Byte 0   1    2        3        4        5        6        7        8        9
 */
void dscKeybusInterface::printPanel_0x05() {
  printPanelPartitionStatus(1, 3, 5);
  if (!keybusVersion1) {
      stream->print(" | ");
      printPanelPartitionStatus(3, 7, 9);
  }
}


/*
 *  0x0A_0F: Status in programming, partitions 1-2
 *  Interval: constant in *8 programming
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Partition lights, printPanelLights()
 *  Byte 3: Partition status, printPanelMessages()
 *  Byte 4: Zone lights 1-8
 *  Byte 5: Zone lights 9-16
 *  Byte 6: Zone lights 17-24
 *  Byte 7: Zone lights 25-32
 *  Byte 8: Zone lights for *5 access codes 33,34,41,42
 *  Byte 9: CRC
 *
 *  Command    1:Lights 1:Status Zones1+  Zones9+  Zones17+ Zones25+            CRC
 *  00001010 0 10000010 11100100 00000000 00000000 00000000 00000000 00000000 01110000 [0x0A] Armed Backlight - *8: Main menu | Zone lights: none
 *  00001010 0 10000001 11101110 01100101 00000000 00000000 00000000 00000000 11011110 [0x0A] Ready Backlight - *8: Input required: 1 option per zone | Zone lights: 1 3 6 7
 *  Byte 0   1    2        3        4        5        6        7        8        9
 */
void dscKeybusInterface::printPanel_0x0A_0F() {
  byte partition = 0;
  switch (panelData[0]) {
    case 0x0A: partition = 1; break;
    case 0x0F: partition = 2; break;
  }

  printPanelPartitionStatus(partition, 3, 3);

  printZoneLights();
  bool zoneLights = printPanelZones(4, 1);

  if (panelData[8] != 0 && panelData[8] != 128) {
    zoneLights = true;
    if (bitRead(panelData[8], 0)) printNumberSpace(33);
    if (bitRead(panelData[8], 1)) printNumberSpace(34);
    if (bitRead(panelData[8], 3)) printNumberSpace(41);
    if (bitRead(panelData[8], 4)) printNumberSpace(42);
  }

  if (!zoneLights) stream->print(F("none"));
}


/*
 *  0x11: Module supervision query
 *  Interval: 30s
 *  CRC: no
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Bytes 2-6: 10101010
 *
 *  Later generation panels:
 *  Bytes 7-8: 10101010
 *
 *  Early generation panels:
 *  00010001 0 10101010 10101010 10101010 10101010 10101010 [0x11] Module supervision query
 *  11111111 1 00111111 11111111 11111111 11111111 11111111 [Module/0x11] Keypad slots: 1
 *  11111111 1 11111111 11111100 11111111 11111111 11111111 [Module/0x11] Keypad slots: 8
 *  Byte 0   1    2        3        4        5        6
 *
 *  Later generation panels:
 *  00010001 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0x11] Module supervision query
 *  11111111 1 00111111 11111111 11111111 11001111 11111111 11111111 11111111 [Module/0x11] Keypad slots: 1 | Zone expander: 6
 *  Byte 0   1    2        3        4        5        6        7        8
 */
void dscKeybusInterface::printPanel_0x11() {
  stream->print(F("Module supervision query"));
}


/*
 *  0x16: Panel configuration
 *  Interval: 4min
 *  CRC: yes
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Unknown data, 00001110 observed on all tested panels
 *  Byte 3: Panel version
 *  Byte 4 bits 0,1: Zone wiring resistors configuration
 *  Byte 4 bit 2: Unknown data
 *  Byte 4 bit 3: Code length flag
 *  Byte 4 bit 4: *8 programming mode flag
 *  Byte 4 bits 5-7: Unknown data
 *  Byte 5: CRC
 *
 *  Command             Version             CRC
 *  00010110 0 00001110 00100011 11010010 00011001 [0x16] Panel version: v2.3 | Zone wiring: EOL | Code length: 4 digits | *8 programming: no
 *  00010110 0 00001110 00100011 11100001 00101000 [0x16] Panel version: v2.3 | Zone wiring: NC | Code length: 4 digits | *8 programming: yes
 *  00010110 0 00001110 00100011 11100110 00101101 [0x16] Panel version: v2.3 | Zone wiring: EOL | Code length: 4 digits | *8 programming: yes
 *  00010110 0 00001110 00100011 11110010 00111001 [0x16] Panel version: v2.3 | Zone wiring: EOL | Code length: 4 digits | *8 programming: no
 *  00010110 0 00001110 00010000 11110011 00100111 [0x16] Panel version: v1.0 | Zone wiring: DEOL | Code length: 4 digits | *8 programming: no
 *  00010110 0 00001110 01000001 11110101 01011010 [0x16] Panel version: v4.1 | Zone wiring: NC | Code length: 4 digits | *8 programming: no
 *  00010110 0 00001110 01000010 10110101 00011011 [0x16] Panel version: v4.2 | Zone wiring: NC | Code length: 4 digits | *8 programming: no
 *  00010110 0 00001110 01000010 10110001 00010111 [0x16] Panel version: v4.2 | Zone wiring: NC | Code length: 4 digits | *8 programming: no
 *  Byte 0   1    2        3        4        5
 */
void dscKeybusInterface::printPanel_0x16() {

  if (panelData[2] == 0x0E) {

    // Panel version
    stream->print(F("Panel version: v"));
    stream->print(panelData[3] >> 4);
    stream->print(".");
    stream->print(panelData[3] & 0x0F);

    // Zone wiring
    stream->print(F(" | Zone wiring: "));
    switch (panelData[4] & 0x03) {
      case 0x01: stream->print(F("NC ")); break;
      case 0x02: stream->print(F("EOL ")); break;
      case 0x03: stream->print(F("DEOL ")); break;
    }

    // Code length
    stream->print(F("| Code length: "));
    if (panelData[4] & 0x08) stream->print(F("6"));
    else stream->print(F("4"));
    stream->print(F(" digits "));

    // *8 programming mode status
    stream->print(F("| *8 programming: "));
    if (panelData[4] & 0x10) stream->print(F("no "));
    else stream->print(F("yes "));
  }

  else printUnknownData();
}


/*
 *  0x1B: Status - partitions 5-8
 *  Interval: constant
 *  CRC: no
 *  Structure decoding: complete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Partition 5 lights, printPanelLights()
 *  Byte 3: Partition 5 status, printPanelMessages()
 *  Byte 4: Partition 6 lights
 *  Byte 5: Partition 6 status
 *  Byte 6: Partition 7 lights
 *  Byte 7: Partition 7 status
 *  Byte 8: Partition 8 lights
 *  Byte 9: Partition 8 status
 *
 *  Command    5:Lights 5:Status 6:Lights 6:Status 7:Lights 7:Status 8:Lights 8:Status
 *  00011011 0 10010001 00000001 00010000 11000111 00010000 11000111 00010000 11000111 [0x1B] Partition 5: Ready Trouble Backlight - Partition ready | Partition 6: disabled | Partition 7: disabled | Partition 8: disabled
 *  Byte 0   1    2        3        4        5        6        7        8        9
 */
void dscKeybusInterface::printPanel_0x1B() {
  printPanelPartitionStatus(5, 3, 9);
}


/*
 *  0x1C: Verify keypad Fire/Auxiliary/Panic
 *  Interval: immediate after keypad button press
 *  CRC: no
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  01110111 1 11111111 11111111 11111111 11111111 [Keypad] Fire alarm
 *  00011100 0  [0x1C] Verify keypad Fire/Auxiliary/Panic
 *  01110111 1  [Keypad] Fire alarm
 */
void dscKeybusInterface::printPanel_0x1C() {
  stream->print(F("Verify keypad Fire/Auxiliary/Panic"));
}


/*
 *  0x22: Zone expander 0 (PC/RF5132) query
 *  Interval: after zone expander status notification
 *  CRC: no
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Bytes 2-6: 11111111
 *
 *  11111111 1 11111111 11111111 01111111 11111111 [Module/0x05] Zone expander notification: 0
 *  00100010 0 11111111 11111111 11111111 11111111 11111111 [0x22] Zone expander query: 0
 *  11111111 1 01011001 01010101 01010101 01010101 10000100 [Module/0x22] Zone expander: 0 | Zones changed: 2 tamper
 *
 *  11111111 1 11111111 11111111 10111111 11111111 [Module/0x05] Zone expander notification: 1
 *  00101000 0 11111111 11111111 11111111 11111111 11111111 [0x28] Zone expander query: 1
 *  11111111 1 01010111 01010101 11111111 11111111 01101111 [Module/0x28] Zone expander: 1 | Zones changed: 9 open (D/EOL)
 *  Byte 0   1    2        3        4        5        6
 */
void dscKeybusInterface::printPanel_0x22_28_33_39() {
  byte expander = 0;
  switch (panelData[0]) {
    case 0x22: expander = 0; break;
    case 0x28: expander = 1; break;
    case 0x33: expander = 2; break;
    case 0x39: expander = 3; break;
  }
  stream->print(F("Zone expander query: "));
  stream->print(expander);
}


/*
 *  0x27: Status with zones 1-8
 *  Interval: 4m
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Partition 1 lights, printPanelLights()
 *  Byte 3: Partition 1 status, printPanelMessages()
 *  Byte 4: Partition 2 lights
 *  Byte 5: Partition 2 status
 *  Byte 6: Zones 1-8
 *  Byte 7: CRC
 *
 *  Command    1:Lights 1:Status 2:Lights 3:Status Zones1+    CRC
 *  00100111 0 10000001 00000001 10010001 11000111 00000000 00000001 [0x27] Partition 1: Ready Backlight - Partition ready | Partition 2: disabled | Zones 1-8 open: none
 *  00100111 0 10000001 00000001 10010001 11000111 00000010 00000011 [0x27] Partition 1: Ready Backlight - Partition ready | Partition 2: disabled | Zones 1-8 open: 2
 *  00100111 0 10001010 00000100 10010001 11000111 00000000 00001101 [0x27] Partition 1: Armed Bypass Backlight - Armed stay | Partition 2: disabled | Zones 1-8 open: none
 *  00100111 0 10001010 00000100 11111111 11111111 00000000 10110011 [0x27] Partition 1: Armed Bypass Backlight - Armed stay | Zones 1-8 open: none
 *  00100111 0 10000010 00000101 10010001 11000111 00000000 00000110 [0x27] Partition 1: Armed Backlight - Armed away | Partition 2: disabled | Zones 1-8 open: none
 *  00100111 0 10000010 00000101 11111111 11111111 00000000 10101100 [0x27] Partition 1: Armed Backlight - Armed away | Zones 1-8 open: none
 *  00100111 0 10000010 00001100 10010001 11000111 00000001 00001110 [0x27] Partition 1: Armed Backlight - Entry delay in progress | Partition 2: disabled | Zones 1-8 open: 1
 *  00100111 0 10000010 00010001 10010001 11000111 00000001 00010011 [0x27] Partition 1: Armed Backlight - Partition in alarm | Partition 2: disabled | Zones 1-8 open: 1
 *  00100111 0 10000010 00001101 10010001 11000111 00000001 00001111 [0x27] Partition 1: Armed Backlight - Opening after alarm | Partition 2: disabled | Zones 1-8 open: 1
 *  00100111 0 10000010 00010001 11011011 11111111 00000010 10010110 [0x27] Partition 1: Armed Backlight - Partition in alarm | Zones 1-8 open: 2
 *  00100111 0 00000001 00000001 11111111 11111111 00000000 00100111 [0x27] Partition 1: Ready - Partition ready | Zones 1-8 open: none
 *  00100111 0 10010001 00000001 11111111 11111111 00000000 10110111 [0x27] Partition 1: Ready Trouble Backlight - Partition ready | Zones 1-8 open: none
 *  00100111 0 10010001 00000001 10100000 00000000 00000000 01011001 [0x27] Partition 1: Ready Trouble Backlight - Partition ready | Partition 2: Program Backlight - Unknown data: 0x00 | Zones 1-8 open: none
 *  00100111 0 10010000 00000011 11111111 11111111 00111111 11110111 [0x27] Partition 1: Trouble Backlight - Zones open | Zones 1-8 open: 1 2 3 4 5 6
 *  00100111 0 10010000 00000011 10010001 11000111 00111111 01010001 [0x27] Partition 1: Trouble Backlight - Zones open | Partition 2: disabled | Zones 1-8 open: 1 2 3 4 5 6
 *  00100111 0 10000000 00000011 10000010 00000101 00011101 01001110 [0x27] Partition 1: Backlight - Zones open | Partition 2: Armed Backlight - Armed away | Zones 1-8 open: 1 3 4 5
 *  Byte 0   1    2        3        4        5        6        7
 */
void dscKeybusInterface::printPanel_0x27() {
  printPanelPartitionStatus(1, 3, 5);

  stream->print(F(" | Zones 1-8 open: "));
  printPanelBitNumbers(6, 1);
}


/*
 *  0x2D: Status with zones 9-16
 *  Interval: 4m
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Partition 1 lights, printPanelLights()
 *  Byte 3: Partition 1 status, printPanelMessages()
 *  Byte 4: Partition 2 lights
 *  Byte 5: Partition 2 status
 *  Byte 6: Zones 9-16
 *  Byte 7: CRC
 *
 *  Command    1:Lights 1:Status 2:Lights 3:Status Zones9+    CRC
 *  00101101 0 10000000 00000011 10000001 11000111 00000001 11111001 [0x2D] Partition 1: Backlight - Zones open | Partition 2: disabled | Zones 9-16 open: 9
 *  00101101 0 10000000 00000011 10000010 00000101 00000000 00110111 [0x2D] Partition 1: Backlight - Zones open | Partition 2: Armed Backlight - Armed away | Zones 9-16 open: none
 *  Byte 0   1    2        3        4        5        6        7
 */
void dscKeybusInterface::printPanel_0x2D() {
  printPanelPartitionStatus(1, 3, 5);

  stream->print(F(" | Zones 9-16 open: "));
  printPanelBitNumbers(6, 9);
}


/*
 *  0x34: Status with zones 17-24
 *  Interval: 4m
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Partition 1 lights, printPanelLights()
 *  Byte 3: Partition 1 status, printPanelMessages()
 *  Byte 4: Partition 2 lights
 *  Byte 5: Partition 2 status
 *  Byte 6: Zones 17-24
 *  Byte 7: CRC
 */
void dscKeybusInterface::printPanel_0x34() {
  printPanelPartitionStatus(1, 3, 5);

  stream->print(F(" | Zones 17-24 open: "));
  printPanelBitNumbers(6, 17);
}


/*
 *  0x3E: Status with zones 25-32
 *  Interval: 4m
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Partition 1 lights, printPanelLights()
 *  Byte 3: Partition 1 status, printPanelMessages()
 *  Byte 4: Partition 2 lights
 *  Byte 5: Partition 2 status
 *  Byte 6: Zones 25-32
 *  Byte 7: CRC
 */
void dscKeybusInterface::printPanel_0x3E() {
  printPanelPartitionStatus(1, 3, 5);

  stream->print(F(" | Zones 25-32 open: "));
  printPanelBitNumbers(6, 25);
}


/*
 *  0x41: Wireless module query
 *  Interval: immediate on wireless module notification
 *  CRC: no
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Bytes 2-10: 11111111
 *
 *  11111111 1 11111111 11111111 11110111 11111111 [Module/0x05] Wireless module battery notification
 *  01000001 0 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [0x41] Wireless module query
 *  11111111 1 11111111 11011111 11111111 11111111 11111111 11111111 11111111 11111111 11011000 [Module/0x41] Wireless module | Battery low zones: 11
 *  Byte 0   1    2        3        4        5        6        7        8        9        10
 */
void dscKeybusInterface::printPanel_0x41() {
  stream->print(F("Wireless module query"));
}


/*
 *  0x4C: Module tamper query
 *  Interval: immediate on module tamper notification & after exiting *8 programming
 *  CRC: no
 *  Structure decoding: complete
 *  Content decoding: *incomplete
 *
 *  Bytes 2-12: 10101010
 *
 *  Later generation panels:
 *  Bytes 13-14: 10101010
 *
 *  11111111 1 11111111 11111111 11111110 11111111 [Module/0x05] Module tamper notification
 *  01001100 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0x4C] Module tamper query
 *  11111111 1 00111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Keypad tamper: Slot 1
 *  Byte 0   1    2        3        4        5        6        7        8        9        10       11       12
 *
 *  Later generation panels:
 *  01001100 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0x4C] Module tamper query
 *  11111111 1 11111111 11111111 11111111 11110000 00111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Module tamper: Slot 9
 *  Byte 0   1    2        3        4        5        6        7        8        9        10       11       12       13       14
 */
void dscKeybusInterface::printPanel_0x4C() {
  stream->print(F("Module tamper query"));
}


/*
 *  0x57: Wireless key query
 *  Interval: immediate on wireless key notification
 *  CRC: no
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Bytes 2-10: 11111111
 *
 *  11111111 1 11111111 11111111 11111111 10111111 [Module/0x05] Wireless key notification
 *  01010111 0 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [0x57] Wireless key query
 *  Byte 0   1    2        3        4        5        6        7        8        9        10
 */
void dscKeybusInterface::printPanel_0x57() {
  stream->print(F("Wireless key query"));
}


/*
 *  0x58: Module status query - valid response produces 0xA5 command, byte 6 0xB1-0xC0
 *  Interval: immediate on module status notification and at power on after panel reset
 *  CRC: no
 *  Structure decoding: complete
 *  Content decoding: *incomplete
 *
 *  Bytes 2-5: 10101010
 *
 *  11111111 1 11111111 11111111 11111111 11011111 [Module/0x05] Module status notification
 *  01011000 0 10101010 10101010 10101010 10101010 [0x58] Module status query
 *  11111111 1 11111100 11111111 11111111 11111111 [Module/0x58] PC5204: Battery restored
 *  Byte 0   1    2        3        4        5
 */
void dscKeybusInterface::printPanel_0x58() {
  stream->print(F("Module status query"));
}


/*
 *  0x5D_63: Flash panel lights: status and zones 1-32, partitions 1-2
 *  Interval: 30s
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 2: Status lights
 *  Byte 3: Zones 1-8
 *  Byte 4: Zones 9-16
 *  Byte 5: Zones 17-24
 *  Byte 6: Zones 25-32
 *  Byte 7: CRC
 *
 *  Command    1:Lights Zones1+  Zones9+  Zones17+ Zones25+   CRC
 *  01011101 0 00000000 00000000 00000000 00000000 00000000 01011101 [0x5D] Partition 1 | Status lights flashing: none | Zones 1-32 flashing: none
 *  01011101 0 00100000 00000000 00000000 00000000 00000000 01111101 [0x5D] Partition 1 | Status lights flashing: Program | Zones 1-32 flashing: none
 *  01011101 0 00000000 00100000 00000000 00000000 00000000 01111101 [0x5D] Partition 1 | Status lights flashing: none | Zones 1-32 flashing: 6
 *  01011101 0 00000100 00100000 00000000 00000000 00000000 10000001 [0x5D] Partition 1 | Status lights flashing: Memory | Zones 1-32 flashing: 6
 *  01011101 0 00000000 00000000 00000001 00000000 00000000 01011110 [0x5D] Partition 1 | Status lights flashing: none | Zones 1-32 flashing: 9
 *
 *  01100011 0 00000000 00000000 00000000 00000000 00000000 01100011 [0x63] Partition 2 | Status lights flashing: none | Zones 1-32 flashing: none
 *  01100011 0 00000100 10000000 00000000 00000000 00000000 11100111 [0x63] Partition 2 | Status lights flashing: Memory | Zones 1-32 flashing: 8
 *  Byte 0   1    2        3        4        5        6        7
 */
void dscKeybusInterface::printPanel_0x5D_63() {
  byte partition = 0;
  switch (panelData[0]) {
    case 0x5D: partition = 1; break;
    case 0x63: partition = 2; break;
  }
  printPartition();
  printNumberSpace(partition);

  printStatusLightsFlashing();
  printPanelLights(2, false);

  stream->print(F("| Zones 1-32 flashing: "));
  printPanelZones(3, 1);
}


/*
 *  0x64: Beep, partition 1
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 2: Beeps number
 *  Byte 3: CRC
 *
 *  Command     Beeps     CRC
 *  01100100 0 00000100 01101000 [0x64] Partition 1 | Beep: 2 beeps
 *  01100100 0 00000110 01101010 [0x64] Partition 1 | Beep: 3 beeps
 *  01100100 0 00001000 01101100 [0x64] Partition 1 | Beep: 4 beeps
 *  01100100 0 00001010 01101110 [0x64] Partition 1 | Beep: 5 beeps
 *  01100100 0 00001100 01110000 [0x64] Partition 1 | Beep: 6 beeps
 *  Byte 0   1    2        3
 */
void dscKeybusInterface::printPanel_0x64() {
  printPartition();
  printNumberSpace(1);
  printPanelBeeps(2);
}


/*
 *  0x69: Beep, partition 2
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 2: Beeps number
 *  Byte 3: CRC
 *
 *  Command     Beeps     CRC
 *  01101001 0 00001100 01110101 [0x69] Partition 2 | Beep: 6 beeps
 *  Byte 0   1    2        3
 */
void dscKeybusInterface::printPanel_0x69() {
  printPartition();
  printNumberSpace(2);
  printPanelBeeps(2);
}


/*
 *  0x6E: LCD keypad display
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 2 bits 0-3: Digit 1
 *  Byte 2 bits 4-7: Digit 2
 *  Byte 3 bits 0-3: Digit 3
 *  Byte 3 bits 4-7: Digit 4
 *  Byte 4 bits 0-3: Digit 5
 *  Byte 4 bits 4-7: Digit 6
 *  Byte 5 bits 0-3: Digit 7
 *  Byte 5 bits 4-7: Digit 8
 *  Byte 6: CRC
 *
 *  Command     Digits   Digits                      CRC
 *  01101110 0 00000001 00000001 00000000 00000000 01110000 [0x6E] Access code: 010100
 *  01101110 0 10101010 10101010 00000000 00000000 11000010 [0x6E] Access code: AAAA00
 *  Byte 0   1    2        3        4        5        6
 */
void dscKeybusInterface::printPanel_0x6E() {
  stream->print(F("LCD display: "));
  if (decimalInput) {
    if (panelData[2] <= 0x63) stream->print("0");
    if (panelData[2] <= 0x09) stream->print("0");
    stream->print(panelData[2], DEC);
  }
  else  {
    for (byte panelByte = 2; panelByte <= 5; panelByte ++) {
      stream->print(panelData[panelByte] >> 4, HEX);
      stream->print(panelData[panelByte] & 0x0F, HEX);
    }
  }
}


/*
 *  0x70: LCD keypad data query
 *  CRC: no
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 2-6: 11111111
 *
 *  Command      Tone     CRC
 *  01110000 0 11111111 11111111 11111111 11111111 11111111 [0x70] LCD keypad data query
 *  Byte 0   1    2        3        4        5        6
 */
void dscKeybusInterface::printPanel_0x70() {
  stream->print(F("LCD keypad data query"));
}


/*
 *  0x75: Tone, partition 1
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 2: Tone pattern
 *  Byte 3: CRC
 *
 *  Command      Tone     CRC
 *  01110101 0 10000000 11110101 [0x75] Partition 1 | Tone: constant tone
 *  01110101 0 00000000 01110101 [0x75] Partition 1 | Tone: none
 *  Byte 0   1    2        3
 */
void dscKeybusInterface::printPanel_0x75() {
  printPartition();
  printNumberSpace(1);
  printPanelTone(2);
}


/*
 *  0x7A: Tone, partition 2
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 2: Tone pattern
 *  Byte 3: CRC
 *
 *  Command      Tone     CRC
 *  01111010 0 00000000 01111010 [0x7A] Partition 2 | Tone: none
 *  Byte 0   1    2        3
 */
void dscKeybusInterface::printPanel_0x7A() {
  printPartition();
  printNumberSpace(2);
  printPanelTone(2);
}


/*
 *  0x7F: Buzzer, partition 1
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 2: Buzzer pattern
 *  Byte 3: CRC
 *
 *  Command     Buzzer    CRC
 *  01111111 0 00000001 10000000 [0x7F] Partition 1 | Buzzer: 1s
 *  Byte 0   1    2        3
 */
void dscKeybusInterface::printPanel_0x7F() {
  printPartition();
  printNumberSpace(1);
  printPanelBuzzer(2);
}


/*
 *  0x82: Buzzer, partition 2
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 2: Buzzer pattern
 *  Byte 3: CRC
 *
 *  Command     Buzzer    CRC
 *  10000010 0 00000010 10000100 [0x82] Partition 2 | Buzzer: 2s
 *  Byte 0   1    2        3
 */
void dscKeybusInterface::printPanel_0x82() {
  printPartition();
  printNumberSpace(2);
  printPanelBuzzer(2);
}


/*
 *  0x87: PGM outputs
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 2 bit 0: PGM 3
 *  Byte 2 bit 1: PGM 4
 *  Byte 2 bit 2: PGM 5
 *  Byte 2 bit 3: PGM 6
 *  Byte 2 bit 4: PGM 7
 *  Byte 2 bit 5: PGM 8
 *  Byte 2 bit 6: PGM 9
 *  Byte 2 bit 7: PGM 10
 *  Byte 3 bit 0: PGM 1
 *  Byte 3 bit 1: PGM 2
 *  Byte 3 bit 2: Activates at midnight
 *  Byte 3 bit 3: Battery check when arming
 *  Byte 3 bit 4: PGM 11
 *  Byte 3 bit 5: PGM 12
 *  Byte 3 bit 6: PGM 13
 *  Byte 3 bit 7: PGM 14
 *  Byte 4: CRC
 *
 *  Command      PGM      PGM      CRC
 *  10000111 0 00000000 00000000 10000111 [0x87] PGM outputs enabled: none
 *  10000111 0 00000000 00000001 10001000 [0x87] PGM outputs enabled: 1
 *  10000111 0 00000001 00000011 10001011 [0x87] PGM outputs enabled: 1 2 3
 *  10000111 0 00010001 00000010 10011010 [0x87] PGM outputs enabled: 2 3 7
 *  10000111 0 00010011 00000000 10011010 [0x87] PGM outputs enabled: 3 4 7
 *  10000111 0 00010011 11110011 10001101 [0x87] PGM outputs enabled: 1 2 3 4 7 11 12 13 14
 *  10000111 0 11111111 11110000 01110110 [0x87] PGM outputs enabled: 3 4 5 6 7 8 9 10 11 12 13 14
 *  10000111 0 11111111 11110010 01111000 [0x87] PGM outputs enabled: 2 3 4 5 6 7 8 9 10 11 12 13 14
 *  Byte 0   1    2        3        4
 */
void dscKeybusInterface::printPanel_0x87() {
  stream->print(F("PGM outputs enabled: "));
  if (panelData[2] == 0 && panelData[3] == 0) stream->print(F("none "));
  else {
    printPanelBitNumbers(3, 1, 0, 1, false);
    printPanelBitNumbers(2, 3, 0, 7, false);
    printPanelBitNumbers(3, 11, 4, 7, false);
  }

  if (panelData[3] & 0x04) stream->print(F("| Midnight "));
  if (panelData[3] & 0x08) stream->print(F("| Battery check"));
}


/*
 *  0x8D: Module programming entry, User code programming key response, codes 17-32
 *  Note: Wireless keys 1-16 are assigned to user codes 17-32
 *  CRC: yes
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Unknown
 *  Byte 3: Unknown
 *  Byte 4: Unknown
 *  Byte 5: Unknown
 *  Byte 6: Unknown
 *  Byte 7: Unknown
 *  Byte 8: Unknown
 *  Byte 9: CRC
 *
 *  Command                                                                     CRC
 *  10001101 0 00110001 00000001 00000000 00010111 11111111 11111111 11111111 11010011 [0x8D] User code programming key response  // Code 17 Key 1
 *  10001101 0 00110001 00000100 00000000 00011000 11111111 11111111 11111111 11010111 [0x8D] User code programming key response  // Code 18 Key 1
 *  10001101 0 00110001 00000100 00000000 00010010 11111111 11111111 11111111 11010001 [0x8D] User code programming key response  // Code 18 Key 2
 *  10001101 0 00110001 00000101 00000000 00111000 11111111 11111111 11111111 11111000 [0x8D] User code programming key response  // Code 18 Key 3
 *  10001101 0 00110001 00000101 00000000 00110100 11111111 11111111 11111111 11110100 [0x8D] User code programming key response  // Code 18 Key 4
 *  10001101 0 00110001 00100101 00000000 00001001 11111111 11111111 11111111 11101001 [0x8D] User code programming key response  // Code 29 Key 0
 *  10001101 0 00110001 00100101 00000000 00000001 11111111 11111111 11111111 11100001 [0x8D] User code programming key response  // Code 29 Key 1
 *  10001101 0 00110001 00110000 00000000 00000000 11111111 11111111 11111111 11101011 [0x8D] User code programming key response  // Message after 4th key entered
 *  10001101 0 00010001 01000001 00000001 00000111 01010101 11111111 11111111 00111010 [0x8D] Wls programming key response	  // Before WLS zone 4 placement test
 *  10001101 0 00010001 01000001 00000001 00001001 01010101 11111111 11111111 00111100 [0x8D] Wls programming key response	  // Before WLS zone 5 placement test
 *  10001101 0 00010001 01000001 00000001 00001011 01010101 11111111 11111111 00111110 [0x8D] Wls programming key response	  // Before WLS zone 6 placement test
 *  10001101 0 00010001 01000001 00000001 00001101 01010101 11111111 11111111 01000000 [0x8D] Wls programming key response   	  // Before WLS zone 7 placement test
 *  10001101 0 00010001 01000001 00000001 00100001 01010101 11111111 11111111 01010100 [0x8D] Wls programming key response	  // Before WLS zone 17 placement test
 *  10001101 0 00010001 01000001 00000001 00111111 01010101 11111111 11111111 01110010 [0x8D] Wls programming key response	  // Before WLS zone 32 placement test
 *  10001101 0 00010001 01000001 00000001 01111111 01010101 11111111 11111111 10110010 [0x8D] Wls programming key response	  // Before WLS zone 64 placement test
 *  10001101 0 00010001 01000011 00000000 00000001 11111111 11111111 11111111 11011111 [0x8D] Wls programming key response	  // Location is good, same for all zones
 *  10001101 0 00010001 01000001 00000001 11111111 11111111 11111111 11111111 11011100 [0x8D] Wls programming key response	  // Wireless zone is not-assigned
 *  10001101 0 00010001 10100100 00000000 00000011 11111111 11111111 11111111 01000010 [0x8D] Wls programming key response	  // Enter 03 for [804][61] function 1
 *  10001101 0 00010001 10100101 00000000 00000100 11111111 11111111 11111111 01000100 [0x8D] Wls programming key response	  // Enter 04 for [804][61] function 2
 *  10001101 0 00010001 10100101 00000000 00000011 11111111 11111111 11111111 01000011 [0x8D] Wls programming key response	  // Enter 03 for [804][61] function 2
 *  10001101 0 00010001 10101000 00000000 00000011 11111111 11111111 11111111 01000110 [0x8D] Wls programming key response	  // Enter 03 for [804][62] function 1
 *  10001101 0 00010001 10101001 00000000 00000100 11111111 11111111 11111111 01001000 [0x8D] Wls programming key response	  // Enter 04 for [804][62] function 2
 *  10001101 0 00010001 11000000 00000000 00000011 11111111 11111111 11111111 01011110 [0x8D] Wls programming key response	  // Enter 03 for [804][68] function 1
 *  10001101 0 00010001 11000011 00000000 00110000 11111111 11111111 11111111 10001110 [0x8D] Wls programming key response	  // Enter 30 for [804][68] function 4
 *  10001101 0 00010001 00111000 00000000 00010000 11111111 11111111 11111111 11100011 [0x8D] Wls programming key response	  // Enter 10 for [804][81] Wls supervisory window
 *  10001101 0 00010001 00111000 00000000 10010110 11111111 11111111 11111111 01101001 [0x8D] Wls programming key response	  // Enter 96 for [804][81] Wls supervisory window
 *  10001101 0 00010001 11000100 00000000 00000001 11111111 11111111 11111111 01100000 [0x8D] Wls programming key response	  // Enter 01 for [804][69] Keyfob 1 partition assigment
 *  10001101 0 00010001 11000101 00000000 00000001 11111111 11111111 11111111 01100001 [0x8D] Wls programming key response	  // Enter 01 for [804][69] Keyfob 2 partition assigment
 *  10001101 0 00010001 11000110 00000000 00000010 11111111 11111111 11111111 01100011 [0x8D] Wls programming key response	  // Enter 02 for [804][69] Keyfob 3 partition assigment
 *  10001101 0 00010001 11010100 00000000 11111111 11111111 11111111 11111111 01101110 [0x8D] Wls programming key response	  // All 1-8 enabled in [804][82] supervision options
 *  10001101 0 00010001 11010100 00000000 11110000 11111111 11111111 11111111 01011111 [0x8D] Wls programming key response	  // 5-8 zones enabled in [804][82] supervisiory options
 *  10001101 0 00010001 11010101 00000000 00001111 11111111 11111111 11111111 01111111 [0x8D] Wls programming key response	  // 1-4 zones enabled in [804][83] supervisiory options
 *  10001101 0 00010001 11010111 00000000 01111111 11111111 11111111 11111111 11110001 [0x8D] Wls programming key response	  // 1-7 zones enabled in [804][85] supervisiory options
 *  10001101 0 00010001 00111010 00000000 01000000 11111111 11111111 11111111 00010101 [0x8D] Wls programming key response	  // Only option 7 enabled in [804][90] options
 *  10001101 0 00010001 00111001 00000000 00001000 11111111 11111111 11111111 11011100 [0x8D] Wls programming key response	  // Set RF jamming zone 08 in [804][93] subsection
 *  10001101 0 00010001 00111001 00000000 00000111 11111111 11111111 11111111 11011011 [0x8D] Wls programming key response	  // Set RF jamming zone 07 in [804][93] subsection
 *  Byte 0   1    2        3        4        5        6        7        8        9
 */
void dscKeybusInterface::printPanel_0x8D() {
  stream->print(F("Module programming entry"));
}


/*
 *  0x94: Used to request programming data from modules
 *  Interval: immediate after entering *5 access code programming, after entering section 801-809 subsection number
 *  CRC: no
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Unknown
 *  Byte 3: Unknown
 *  Byte 4: Unknown
 *  Byte 5: Unknown
 *  Byte 6: Unknown
 *  Byte 7: Unknown
 *  Byte 8: Unknown
 *  Byte 9: Unknown
 *  Byte 10: CRC
 *
 *  Command                                                                              CRC
 *  10010100 0 00010001 00000000 00000000 10100101 00000000 00000000 00000000 00010111 10100000 [0x94] Unknown data
 *  10010100 0 00010001 00000000 00000000 10100101 00000000 00000000 00000000 01001100 11111100 [0x94] Unknown data
 *  Byte 0   1    2        3        4        5        6        7        8        9        10
 */
void dscKeybusInterface::printPanel_0x94() {
  stream->print(F("Module programming request"));
}


/*
 *  0x9E: DLS query
 *  Structure decoding: complete
 *  Content decoding: complete
 *  CRC: no
 *
 *  Bytes 2-6: 11111111
 *
 *  10011110 0 11111111 11111111 11111111 11111111 11111111 [0x9E] DLS query
 *  Byte 0   1    2        3        4        5        6
 */
void dscKeybusInterface::printPanel_0x9E() {
  stream->print(F("DLS query"));
}


/*
 *  0xA5: Date, time, system status messages - partitions 1-2
 *  CRC: yes
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2 bit 0-3: Year digit 2
 *  Byte 2 bit 4-7: Year digit 1
 *  Byte 3 bit 0-1: Day digit part 1
 *  Byte 3 bit 2-5: Month
 *  Byte 3 bit 6-7: Partition
 *  Byte 4 bit 0-4: Hour
 *  Byte 4 bit 5-7: Day digit part 2
 *  Byte 5 bit 0-1: Selects set of status commands
 *  Byte 5 bit 2-7: Minute
 *  Byte 6: Status, printPanelStatus0...printPanelStatus3
 *  Byte 7: Unknown
 *  Byte 8: CRC
 *
 *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status             CRC
 *  10100101 0 00011000 00001110 11101101 10000000 00000000 00000000 00111000 [0xA5] 2018.03.23 13:32 | Timestamp
 *  10100101 0 00011000 01001111 11001010 01000100 01001011 11111111 01100100 [0xA5] 2018.03.30 10:17 | Partition 1 | Partition in alarm
 *  10100101 0 00011000 01010000 01001001 10111000 01001100 11111111 01011001 [0xA5] 2018.04.02 09:46 | Partition 1 | Zone expander supervisory alarm
 *  10100101 0 00011000 01010000 01001010 00000000 01001101 11111111 10100011 [0xA5] 2018.04.02 10:00 | Partition 1 | Zone expander supervisory restored
 *  10100101 0 00011000 01001111 01110010 10011100 01001110 11111111 01100111 [0xA5] 2018.03.27 18:39 | Partition 1 | Keypad Fire alarm
 *  Byte 0   1    2        3        4        5        6        7        8
 */
void dscKeybusInterface::printPanel_0xA5() {
  printPanelTime(2);

  if (panelData[6] == 0 && panelData[7] == 0) {
    stream->print(F(" | Timestamp"));
    return;
  }

  stream->print(" | ");
  switch (panelData[3] >> 6) {
    case 0x01: printPartition(); printNumberSpace(1); stream->print("| "); break;
    case 0x02: printPartition(); printNumberSpace(2); stream->print("| "); break;
  }

  switch (panelData[5] & 0x03) {
    case 0x00: printPanelStatus0(6); return;
    case 0x01: printPanelStatus1(6); return;
    case 0x02: printPanelStatus2(6); return;
    case 0x03: printPanelStatus3(6); return;
  }
}


/*
 *  0xAA: Event buffer
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: *incomplete
 *
 *  Byte 2 bit 0-3: Year digit 2
 *  Byte 2 bit 4-7: Year digit 1
 *  Byte 3 bit 0-1: Day digit part 1
 *  Byte 3 bit 2-5: Month
 *  Byte 3 bit 6-7: Partition
 *  Byte 4 bit 0-4: Hour
 *  Byte 4 bit 5-7: Day digit part 2
 *  Byte 5 bit 0-1: Selects set of status commands
 *  Byte 5 bit 2-7: Minute
 *  Byte 6: Status, printPanelStatus0...printPanelStatus3
 *  Byte 7: Event number
 *  Byte 8: CRC
 *
 *  Command    YYY1YYY2   MMMMDD DDDHHHHH MMMMMM    Status   Event#    CRC
 *  10101010 0 00100001 01000110 00001000 00100100 00011100 11111111 01011000 [0xAA] Event: 255 | 2021.01.16 08:09 | Zone alarm: 20
 *  10101010 0 00100000 01100110 10001101 00111000 10011001 00001000 10010110 [0xAA] Event: 008 | 2020.09.20 13:14 | Armed by user code 1
 *  10101010 0 00100000 01100110 10001100 11010000 10111111 00000110 01010001 [0xAA] Event: 006 | 2020.09.20 12:52 | Armed special: quick-arm/auto-arm/keyswitch/wireless key/DLS
 *  10101010 0 00100000 01100110 10001100 11010000 11100010 00000010 01110000 [0xAA] Event: 002 | 2020.09.20 12:52 | Disarmed by master code 40
 *  10101010 0 00100000 01100110 10001100 11010000 00001011 00000101 10011100 [0xAA] Event: 005 | 2020.09.20 12:52 | Zone alarm: 3
 *  10101010 0 00100000 01100110 10001100 11010000 01001011 00000100 11011011 [0xAA] Event: 004 | 2020.09.20 12:52 | Partition in alarm
 *  10101010 0 00100000 01100110 10001100 11010110 11100110 00000000 01111000 [0xAA] Event: 000 | 2020.09.20 12:53 | Enter *6 programming
 *  10101010 0 00100000 01100110 10101101 10010001 00101011 00000011 10011100 [0xAA] Event: 003 | 2020.09.21 13:36 | Armed by auto-arm
 *  10101010 0 00100000 01100110 10101101 11000101 10101101 00000010 01010001 [0xAA] Event: 002 | 2020.09.21 13:49 | Enter *8 programming
 *  10101010 0 00100000 01100110 10101101 11000101 10101100 00000001 01001111 [0xAA] Event: 001 | 2020.09.21 13:49 | Exit *8 programming
 *  10101010 0 00010110 10011010 00000000 00110110 11000100 00000001 01010101 [0xAA] Event: 001 | 2016.06.16 00:13 | Partition 2 | Unknown data // *5 by code 41
 *  Byte 0   1    2        3        4        5        6        7        8
 *
 */
void dscKeybusInterface::printPanel_0xAA() {

  stream->print(F("Event: "));
  if (panelData[7] < 10) stream->print("00");
  else if (panelData[7] < 100) stream->print("0");
  stream->print(panelData[7]);
  stream->print(" | ");

  printPanelTime(2);

  stream->print(" | ");
  switch (panelData[3] >> 6) {
    case 0x01: printPartition(); printNumberSpace(1); stream->print("| "); break;
    case 0x02: printPartition(); printNumberSpace(2); stream->print("| "); break;
  }

  switch (panelData[5] & 0x03) {
    case 0x00: printPanelStatus0(6); return;
    case 0x01: printPanelStatus1(6); return;
    case 0x02: printPanelStatus2(6); return;
    case 0x03: printPanelStatus3(6); return;
  }
}


/*
 *  0xB1: Enabled zones 1-32, partitions 1,2
 *  Interval: 4m
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 2: Partition 1 enabled zones 1-8
 *  Byte 3: Partition 1 enabled zones 9-16
 *  Byte 4: Partition 1 enabled zones 17-24
 *  Byte 5: Partition 1 enabled zones 25-32
 *  Byte 6: Partition 2 enabled zones 1-8
 *  Byte 7: Partition 2 enabled zones 9-16
 *  Byte 8: Partition 2 enabled zones 17-24
 *  Byte 9: Partition 2 enabled zones 25-32
 *  Byte 10: CRC
 *
 *  Command    1:Zone1+ Zones9+  Zones17+ Zones25+ 2:Zone1+ Zones9+  Zones17+ Zones25+   CRC
 *  10110001 0 11111111 00000000 00000000 00000000 00000000 00000000 00000000 00000000 10110000 [0xB1] Enabled zones 1-32 | Partition 1: 1 2 3 4 5 6 7 8 | Partition 2: none
 *  10110001 0 10010001 10001010 01000001 10100100 00000000 00000000 00000000 00000000 10110001 [0xB1] Enabled zones 1-32 | Partition 1: 1 5 8 10 12 16 17 23 27 30 32 | Partition 2: none
 *  10110001 0 11111111 00000000 00000000 11111111 00000000 00000000 00000000 00000000 10101111 [0xB1] Enabled zones 1-32 | Partition 1: 1 2 3 4 5 6 7 8 25 26 27 28 29 30 31 32 | Partition 2: none
 *  10110001 0 01111111 11111111 00000000 00000000 10000000 00000000 00000000 00000000 10101111 [0xB1] Enabled zones 1-32 | Partition 1: 1 2 3 4 5 6 7 9 10 11 12 13 14 15 16 | Partition 2: 8
 *  Byte 0   1    2        3        4        5        6        7        8        9        10
 */
void dscKeybusInterface::printPanel_0xB1() {
  stream->print(F("Enabled zones 1-32 | Partition 1: "));
  printPanelZones(2, 1);

  stream->print(F("| Partition 2: "));
  printPanelZones(6, 1);
}


/*
 *  0xBB: Bell
 *  Interval: immediate after alarm tripped except silent zones
 *  CRC: yes
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2 bit 0-4: Unknown
 *  Byte 2 bit 5: Bell status
 *  Byte 2 bit 6-7: Unknown
 *  Byte 3: Unknown
 *  Byte 4: CRC
 *
 *  Command                        CRC
 *  10111011 0 00100000 00000000 11011011 [0xBB] Bell: on
 *  10111011 0 00100000 00000101 11100000 [0xBB] Bell: on  // PC5015
 *  10111011 0 00000000 00000000 10111011 [0xBB] Bell: off
 *  Byte 0   1    2        3        4
 */
void dscKeybusInterface::printPanel_0xBB() {
  stream->print(F("Bell: "));
  if (bitRead(panelData[2], 5)) stream->print(F("on"));
  else stream->print(F("off"));
}


/*
 *  0xC3: Keypad and dialer status
 *  Interval: 30s (PC1616/PC1832/PC1864)
 *  CRC: yes
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2: bit 0-2 unknown
 *  Byte 2: bit 3 active when dialer attempt begin
 *  Byte 2: bit 4 dialer enabled (always true on old-gen?)
 *  Byte 2: bit 5 keypad lockout active
 *  Byte 2: bit 6-7 unknown
 *  Byte 3: Unknown, always observed as 11111111
 *  Byte 4: CRC
 *
 *  Command                        CRC
 *  11000011 0 00001000 11111111 11001010 [0xC3] Unknown data
 *  11000011 0 00010000 11111111 11010010 [0xC3] Unknown data
 *  11000011 0 00110000 11111111 11110010 [0xC3] Keypad lockout
 *  11000011 0 00000000 11111111 11000010 [0xC3] Keypad ready
 *  Byte 0   1    2        3        4
 */
void dscKeybusInterface::printPanel_0xC3() {
  if (panelData[3] == 0xFF) {

    if (panelData[2] & 0x01 || panelData[2] & 0x02 || panelData[2] & 0x04 || panelData[2] & 0x40 || panelData[2] & 0x80) printUnknownData();
	else {
	  stream->print(F("Dialer: "));
      if (panelData[2] & 0x10) stream->print(F("enabled"));
      else stream->print(F("disabled"));

      if (panelData[2] & 0x08) stream->print(F(" | Dialer call attempt"));
      if (panelData[2] & 0x20) stream->print(F(" | Keypad lockout"));
	}
  }
  else printUnknownData();
}


/*
 *  0xCE: Panel status
 *  CRC: yes
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Unknown
 *  Byte 3 bit 0-1: Selects set of status commands
 *  Byte 3 bit 2-7: Unknown
 *  Byte 4: Status, printPanelStatus0...printPanelStatus2
 *  Byte 5: Unknown
 *  Byte 6: Unknown
 *  Byte 7: CRC
 *
 * Command                       Status                      CRC
 * 11001110 0 00100000 00000000 11101001 00000000 00000000 11010111 [0xCE] Bell trouble
 * 11001110 0 00100000 00011101 10001100 00000000 00000000 10010111 [0xCE] Zone fault: 1
 * 11001110 0 00100000 10001100 01010000 00000000 00000000 11001010 [0xCE] Keypad Panic alarm
 *  Byte 0   1    2        3        4        5        6        7
 *
 * Command                      Unknown                      CRC
 * 11001110 0 00000001 10100000 00000000 00000000 00000000 01101111 [0xCE] Unknown data [Byte 2/0x01]  // Partition 1 exit delay
 * 11001110 0 00000001 10110001 00000000 00000000 00000000 10000000 [0xCE] Unknown data [Byte 2/0x01]  // Partition 1 armed stay
 * 11001110 0 00000001 10110011 00000000 00000000 00000000 10000010 [0xCE] Unknown data [Byte 2/0x01]  // Partition 1 armed away
 * 11001110 0 00000001 10100100 00000000 00000000 00000000 01110011 [0xCE] Unknown data [Byte 2/0x01]  // Partition 2 armed away
 * 11001110 0 01000000 11111111 11111111 11111111 11111111 00001010 [0xCE] Unknown data [Byte 2/0x40]  // Unknown data
 * 11001110 0 01000000 11111111 11111111 11111111 11111111 00001010 [0xCE] Unknown data [Byte 2/0x40]  // LCD: System is in Alarm
 *  Byte 0   1    2        3        4        5        6        7
 */
void dscKeybusInterface::printPanel_0xCE() {
  if (panelData[2] & 0x20) {
    switch (panelData[3] & 0x03) {
      case 0x00: printPanelStatus0(4); return;
      case 0x01: printPanelStatus1(4); return;
      case 0x02: printPanelStatus2(4); return;
      case 0x03: printPanelStatus3(4); return;
    }
  }
  else {
    printUnknownData();
    stream->print(F(" [Byte 2/0x"));
    if (panelData[2] < 16) stream->print("0");
    stream->print(panelData[2], HEX);
    stream->print(F("] "));
  }
}


/*
 *  0xD5: Keypad zone query
 *  CRC: no
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Bytes 2-9: 1010101010
 *
 *  11111111 1 11111111 11111111 11111111 11111011 [Module/0x05] Keypad zone status notification
 *  11010101 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0xD5] Keypad zone query
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 00001111 [Module/0xD5] [Keypad] Slot 8 zone open
 *  Byte 0   1    2        3        4        5        6        7        8        9
 */
void dscKeybusInterface::printPanel_0xD5() {
  stream->print(F("Keypad zone query"));
}


/*
 *  0xE6: Extended status, partitions 1-8
 *  Panels: PC5020, PC1616, PC1832, PC1864
 *
 *  0xE6 commands are split into multiple subcommands to handle up to 8 partitions/64 zones.
 *
 *  Byte 2: Subcommand
 */
void dscKeybusInterface::printPanel_0xE6() {
  switch (panelData[2]) {
    case 0x08:
    case 0x0A:
    case 0x0C:
    case 0x0E: break;  // Skips panel commands without CRC data
    default: {         // Checks remaining panel commands
      if (!validCRC()) {
        stream->print(F("[CRC Error]"));
        return;
      }
    }
  }

  switch (panelData[2]) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x20:
    case 0x21: printPanel_0xE6_0x01_06_20_21(); break;  // Partitions 1-8 status in programming | Structure: *incomplete | Content: *incomplete
    case 0x08:
    case 0x0A:
    case 0x0C:
    case 0x0E: printPanel_0xE6_0x08_0A_0C_0E(); break;  // Zone expander 4-7 query | Structure: complete | Content: complete
    case 0x09: printPanel_0xE6_0x09(); break;  // Zones 33-40 status | Structure: complete | Content: complete
    case 0x0B: printPanel_0xE6_0x0B(); break;  // Zones 41-48 status | Structure: complete | Content: complete
    case 0x0D: printPanel_0xE6_0x0D(); break;  // Zones 49-56 status | Structure: complete | Content: complete
    case 0x0F: printPanel_0xE6_0x0F(); break;  // Zones 57-64 status | Structure: complete | Content: complete
    case 0x17: printPanel_0xE6_0x17(); break;  // Flash panel lights: status and zones 1-32, partitions 1-8 | Structure: complete | Content: complete
    case 0x18: printPanel_0xE6_0x18(); break;  // Flash panel lights: status and zones 33-64, partitions 1-8 | Structure: complete | Content: complete
    case 0x19: printPanel_0xE6_0x19(); break;  // Beep, partitions 3-8 | Structure: complete | Content: complete
    case 0x1A: printPanel_0xE6_0x1A(); break;  // Panel status | Structure: *incomplete | Content: *incomplete
    case 0x1D: printPanel_0xE6_0x1D(); break;  // Tone, partitions 3-8 | Structure: complete | Content: complete
    case 0x1F: printPanel_0xE6_0x1F(); break;  // Buzzer, partitions 3-8 | Structure: complete | Content: complete
    case 0x2B: printPanel_0xE6_0x2B(); break;  // Enabled zones 1-32, partitions 1-8 | Structure: complete | Content: complete
    case 0x2C: printPanel_0xE6_0x2C(); break;  // Enabled zones 33-64, partitions 1-8 | Structure: complete | Content: complete
    case 0x41: printPanel_0xE6_0x41(); break;  // Status in access code programming, zone lights 65-95| Structure: *incomplete | Content: *incomplete
    default: stream->print("Unknown data");
  }
}


/*
 *  0xE6.01 - 0xE6.06: Status in alarm/programming, partitions 1-8
 *  CRC: yes
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Command  Subcommand  Lights   Status  Zones33+ Zones41+ Zones49+ Zones57+            CRC
 *  11100110 0 00000001 10000010 11100100 00000000 00000000 00000000 00000000 00000000 01001101 [0xE6.01] Partition 3: Armed Backlight - *8: Installer programming
 *  11100110 0 00000010 10000010 11100100 00000000 00000000 00000000 00000000 00000000 01001110 [0xE6.02] Partition 4: Armed Backlight - *8: Installer programming
 *  11100110 0 00000011 10000001 11111000 00000000 00000000 00000000 00000000 00000000 01100010 [0xE6.03] Partition 5: Ready Backlight - Keypad programming
 *  11100110 0 00000100 10000010 11100100 00000000 00000000 00000000 00000000 00000000 01010000 [0xE6.04] Partition 6: Armed Backlight - *8: Installer programming
 *  11100110 0 00000101 10000010 11100100 00000000 00000000 00000000 00000000 00000000 01010001 [0xE6.05] Partition 7: Armed Backlight - *8: Installer programming
 *  11100110 0 00000110 10000010 11100100 00000000 00000000 00000000 00000000 10000000 11010010 [0xE6.06] Partition 8: Armed Backlight - *8: Installer programming
 *  11100110 0 00100000 10000000 10011110 00000000 00000000 00000000 00000000 10000000 10100100 [0xE6.20] Partition 1: Backlight - Enter * function code | Zone lights: none
 *  11100110 0 00100000 10000000 10011111 00000000 00000000 00000000 00000000 10000000 10100101 [0xE6.20] Partition 1: Backlight - Enter access code | Zone lights: none
 *  11100110 0 00100000 10000001 11111000 00000000 00000000 00000000 00000000 10000000 11111111 [0xE6.20] Partition 1: Ready Backlight - Keypad programming | Zone lights: none
 *  11100110 0 00100000 10000010 10100110 10000000 00000001 00000000 00000000 10000000 00101111 [0xE6.20] Partition 1: Armed Backlight - *5: Access codes | Zones 33-64 lights: 40 41
 *  Byte 0   1    2        3        4        5        6        7        8        9        10
 */
void dscKeybusInterface::printPanel_0xE6_0x01_06_20_21() {
  byte partition = 0;
  switch(panelData[2]) {
    case 0x01: partition = 3; break;
    case 0x02: partition = 4; break;
    case 0x03: partition = 5; break;
    case 0x04: partition = 6; break;
    case 0x05: partition = 7; break;
    case 0x06: partition = 8; break;
    case 0x20: partition = 1; break;
    case 0x21: partition = 2; break;
  }

  printPanelPartitionStatus(partition, 4, 4);

  if (panelData[9] & 0x80) {
    printZoneLights(false);
    printPanelZones(5, 33);
  }
  else {
    printZoneLights();
    printPanelZones(5,1);
  }
}


/*
 *  0xE6.08,0A,0C,0E: Zone expander 4-7 query
 *  Interval: immediate after zone expander status notification
 *  CRC: no
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Bytes 3-8: 11111111
 *
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 01111111 11111111 11111111 [Module/0x05] Zone expander notification: 4
 *  11100110 0 00001000 11111111 11111111 11111111 11111111 11111111 11111111 [0xE6.08] Zone expander query: 4
 *  11111111 1 11111111 11000011 11001111 11111111 11111111 10101111 11111111 [Module/0xE6] Zone expander: 4 | Zones changed: 34 open
 *
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 10111111 11111111 11111111 [Module/0x05] Zone expander notification: 5
 *  11100110 0 00001010 11111111 11111111 11111111 11111111 11111111 11111111 [0xE6.0A] Zone expander query: 5
 *  11111111 1 11111111 11000011 11001111 11111111 11111111 10101111 11111111 [Module/0xE6] Zone expander: 5 | Zones changed: 42 open
 *
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11011111 11111111 11111111 [Module/0x05] Zone expander notification: 6
 *  11100110 0 00001100 11111111 11111111 11111111 11111111 11111111 11111111 [0xE6.0C] Zone expander query: 6
 *  11111111 1 11111111 11000011 11001111 11111111 11111111 10101111 11111111 [Module/0xE6] Zone expander: 6 | Zones changed: 50 open
 *
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11101111 11111111 11111111 [Module/0x05] Zone expander notification: 7
 *  11100110 0 00001110 11111111 11111111 11111111 11111111 11111111 11111111 [0xE6.0E] Zone expander query: 7
 *  11111111 1 11111111 11000011 11001111 11111111 11111111 10101111 11111111 [Module/0xE6] Zone expander: 7 | Zones changed: 58 open
 *  Byte 0   1    2        3        4        5        6        7        8
 */
void dscKeybusInterface::printPanel_0xE6_0x08_0A_0C_0E() {
  byte expander = 0;
  switch (panelData[2]) {
    case 0x08: expander = 4; break;
    case 0x0A: expander = 5; break;
    case 0x0C: expander = 6; break;
    case 0x0E: expander = 7; break;
  }
  stream->print(F("Zone expander query: "));
  stream->print(expander);
}


/*
 *  0xE6.09: Zones 33-40 status
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 3 bit 0-7: Zone 33-40
 *  Byte 4: CRC
 *
 *  Command  Subcommand Zones33+   CRC
 *  11100110 0 00001001 00000000 11101111 [0xE6.09] Zones 33-40 open: none
 *  Byte 0   1    2        3        4
 */
void dscKeybusInterface::printPanel_0xE6_0x09() {
  stream->print(F("Zones 33-40 open: "));
  printPanelBitNumbers(3, 33);
}


/*
 *  0xE6.0B: Zones 41-48 status
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 3 bit 0-7: Zone 41-48
 *  Byte 4: CRC
 *
 *  Command  Subcommand Zones41+   CRC
 *  11100110 0 00001011 00000000 11110001 [0xE6.0B] Zones 41-48 open: none
 *  Byte 0   1    2        3        4
 */
void dscKeybusInterface::printPanel_0xE6_0x0B() {
  stream->print(F("Zones 41-48 open: "));
  printPanelBitNumbers(3, 41);
}


/*
 *  0xE6.0D: Zones 49-56 status
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 3 bit 0-7: Zone 49-56
 *  Byte 4: CRC
 *
 *  Command  Subcommand Zones49+   CRC
 *  11100110 0 00001101 00000000 11110011 [0xE6.0D] Zones 49-56 open: none
 *  Byte 0   1    2        3        4
 */
void dscKeybusInterface::printPanel_0xE6_0x0D() {
  stream->print(F("Zones 49-56 open: "));
  printPanelBitNumbers(3, 49);
}


/*
 *  0xE6.0F: Zones 57-64 status
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 3 bit 0-7: Zone 57-64
 *  Byte 4: CRC
 *
 *  Command  Subcommand Zones57+   CRC
 *  11100110 0 00001111 00000000 11110101 [0xE6.0F] Zones 57-64 open: none
 *  Byte 0   1    2        3        4
 */
void dscKeybusInterface::printPanel_0xE6_0x0F() {
  stream->print(F("Zones 57-64 open: "));
  printPanelBitNumbers(3, 57);
}


/*
 *  0xE6.17: Flash panel lights: status and zones 1-32, partitions 1-8
 *  Interval: intermittent
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 3: Partition
 *  Byte 4: Status lights flashing
 *  Byte 5: Zones 1-8
 *  Byte 6: Zones 9-16
 *  Byte 7: Zones 17-24
 *  Byte 8: Zones 25-32
 *  Byte 9: CRC
 *
 *  Command    Subcmd  Partition  Lights  Zones1+  Zones9+  Zones17+ Zones25+   CRC
 *  11100110 0 00010111 00000100 00000000 00000100 00000000 00000000 00000000 00000101 [0xE6.17] Partition 3 | Status lights flashing: none | Zones 1-32 flashing: 3
 *  Byte 0   1    2        3        4        5        6        7        8        9
 */
void dscKeybusInterface::printPanel_0xE6_0x17() {
  printPartition();
  printPanelBitNumbers(3, 1);

  printStatusLightsFlashing();
  printPanelLights(4, false);

  stream->print(F("| Zones 1-32 flashing: "));
  printPanelZones(5, 1);
}


/*
 *  0xE6.18: Flash panel lights: status and zones 33-64, partitions 1-8
 *  Interval: intermittent
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 3: Partition
 *  Byte 4: Status lights flashing
 *  Byte 5: Zones 33-40
 *  Byte 6: Zones 41-48
 *  Byte 7: Zones 49-56
 *  Byte 8: Zones 57-64
 *  Byte 9: CRC
 *
 *  Command    Subcmd  Partition  Lights  Zones33+ Zones41+ Zones49+ Zones57+   CRC
 *  11100110 0 00011000 00000001 00000000 00000000 00000000 00000000 00000000 11111111 [0xE6.18] Partition 1 | Status lights flashing: none | Zones 33-64 flashing: none
 *  11100110 0 00011000 00000001 00000000 00000001 00000000 00000000 00000000 00000000 [0xE6.18] Partition 1 | Status lights flashing: none | Zones 33-64 flashing: 33
 *  11100110 0 00011000 00000001 00000100 00000000 00000000 00000000 10000000 10000011 [0xE6.18] Partition 1 | Status lights flashing: Memory | Zones 33-64 flashing: 64
 *  Byte 0   1    2        3        4        5        6        7        8        9
 */
void dscKeybusInterface::printPanel_0xE6_0x18() {
  printPartition();
  printPanelBitNumbers(3, 1);

  printStatusLightsFlashing();
  printPanelLights(4, false);

  stream->print(F("| Zones 33-64 flashing: "));
  printPanelZones(5, 33);
}


/*
 *  0xE6.19: Beep, partitions 3-8
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 3: Partition number
 *  Byte 4: Beeps number
 *  Byte 5: CRC
 *
 *  Command    Subcmd  Partition  Beeps     CRC
 *  11100110 0 00011001 00000100 00000110 00001001 [0xE6.19] Partition 3 | Beep: 3 beeps
 *  11100110 0 00011001 00001000 00001100 00010011 [0xE6.19] Partition 4 | Beep: 6 beeps
 *  Byte 0   1    2        3        4        5
 */
void dscKeybusInterface::printPanel_0xE6_0x19() {
  printPartition();
  printPanelBitNumbers(3, 1);
  printPanelBeeps(4);
}


/*
 *  0xE6.1A: Panel status
 *  CRC: yes
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 3: Unknown
 *  Byte 4: Partitions in alarm
 *  Byte 5: Unknown
 *  Byte 6 bit 0-2: Unknown
 *  Byte 6 bit 3: Loss of system time status
 *  Byte 6 bit 4: AC power trouble status
 *  Byte 6 bit 5: Unknown
 *  Byte 6 bit 6: Fail to communicate status
 *  Byte 6 bit 7: Fire alarm
 *  Byte 7: Unknown
 *  Byte 8: Unknown
 *  Byte 9: Unknown
 *  Byte 10: CRC
 *
 *  Command    Subcmd             Alarm                                                  CRC
 *  11100110 0 00011010 01000000 00000001 00000000 00010001 00000000 00000000 00000000 01010010 [0xE6.1A] Partitions in alarm: 1 | AC power trouble
 *  11100110 0 00011010 01000000 00000000 00000000 00011001 00000000 00000000 00000000 01011001 [0xE6.1A] Partitions in alarm: none | Loss of system time | AC power trouble
 *  11100110 0 00011010 01000000 00000000 00000000 00001001 00000000 00000000 00000000 01001001 [0xE6.1A] Partitions in alarm: none | Loss of system time  // All partitions exit delay in progress, disarmed
 *  11100110 0 00011010 01000000 00010001 00000000 00001001 00000000 00000000 00000000 01011010 [0xE6.1A] Partitions in alarm: 1 5 | Loss of system time   // Keypad panic alarm
 *  11100110 0 00011010 01000000 00000010 00000000 00001001 00000000 00000000 00000000 01001011 [0xE6.1A] Partitions in alarm: 2 | Loss of system time
 *  11100110 0 00011010 01000000 00000001 00000000 00001001 00000000 00000000 00000000 01001010 [0xE6.1A] Partitions in alarm: 1 | Loss of system time
 *  11100110 0 00011010 01000000 10000000 00000000 00001001 00000000 00000000 00000000 11001001 [0xE6.1A] Partitions in alarm: 8 | Loss of system time
 *  Byte 0   1    2        3        4        5        6        7        8        9        10
 */
void dscKeybusInterface::printPanel_0xE6_0x1A() {

  stream->print(F("Partitions in alarm: "));
  printPanelBitNumbers(4, 1);

  if (panelData[6] & 0x08) stream->print(F("| Loss of system time "));
  if (panelData[6] & 0x10) stream->print(F("| AC power trouble "));
  if (panelData[6] & 0x40) stream->print(F("| Fail to communicate "));
  if (panelData[6] & 0x80) stream->print(F("| Fire alarm "));
}


/*
 *  0xE6.1D: Tone, partitions 3-8
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 3: Partition number
 *  Byte 4: Tone
 *  Byte 5: CRC
 *
 *  Command    Subcmd  Partition   Tone     CRC
 *  11100110 0 00011101 00000100 00000000 00000111 [0xE6.1D] Partition 3 | Tone: off
 *  11100110 0 00011101 00001000 10000000 10001011 [0xE6.1D] Partition 4 | Tone: constant tone
 *  Byte 0   1    2        3        4        5
 */
void dscKeybusInterface::printPanel_0xE6_0x1D() {
  printPartition();
  printPanelBitNumbers(3, 1);
  printPanelTone(4);
}


/*
 *  0xE6.1F: Buzzer, partitions 3-8
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 3: Partition number
 *  Byte 4: Buzzer pattern
 *  Byte 5: CRC
 *
 *  Command    Subcmd  Partition  Buzzer    CRC
 *  11100110 0 00011111 00001000 00000001 00001110 [0xE6.1F] Partition 4 | Buzzer: 1s
 *  Byte 0   1    2        3        4        5
 */
void dscKeybusInterface::printPanel_0xE6_0x1F() {
  printPartition();
  printPanelBitNumbers(3, 1);
  printPanelBuzzer(4);
}


/*
 *  0xE6.2B: Enabled zones 1-32, partitions 3-8
 *  Interval: 60s
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 3: Partition number
 *  Byte 4: Zones enabled 1-8
 *  Byte 5: Zones enabled 9-16
 *  Byte 6: Zones enabled 17-24
 *  Byte 7: Zones enabled 25-32
 *  Byte 8: CRC
 *
 *  Command    Subcmd  Partition Zones1+  Zones9+  Zones17+ Zones25+   CRC
 *  11100110 0 00101011 00000100 00000100 00000000 00000000 00000000 00011001 [0xE6.2B] Partition 3 | Enabled zones 1-32: 3
 *  11100110 0 00101011 00001000 00001000 00000000 00000000 00000000 00100001 [0xE6.2B] Partition 4 | Enabled zones 1-32: 4
 *  Byte 0   1    2        3        4        5        6        7        8
 */
void dscKeybusInterface::printPanel_0xE6_0x2B() {
  printPartition();
  printPanelBitNumbers(3, 1);

  stream->print(F("| Enabled zones 1-32: "));
  printPanelZones(4, 1);
}


/*
 *  0xE6.2C: Enabled zones 33-64, partitions 1-8
 *  Interval: 60s
 *  CRC: yes
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 3: Partition number
 *  Byte 4: Zones enabled 33-40
 *  Byte 5: Zones enabled 41-48
 *  Byte 6: Zones enabled 49-56
 *  Byte 7: Zones enabled 57-64
 *  Byte 8: CRC
 *
 *  Command    Subcmd  Partition Zones33+ Zones41+ Zones49+ Zones57+   CRC
 *  11100110 0 00101100 00000001 00000000 00000000 00000000 00000000 00010011 [0xE6.2C] Partition 1 | Enabled zones 33-64: none
 *  11100110 0 00101100 00000010 00000000 00000000 00000000 00000000 00010100 [0xE6.2C] Partition 2 | Enabled zones 33-64: none
 *  Byte 0   1    2        3        4        5        6        7        8
 */
void dscKeybusInterface::printPanel_0xE6_0x2C() {
  printPartition();
  printPanelBitNumbers(3, 1);

  stream->print(F("| Enabled zones 33-64: "));
  printPanelZones(4, 33);
}


/*
 *  0xE6.41: Status in programming, zone lights 65-95
 *  CRC: yes
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  (log data samples needed)
 */
void dscKeybusInterface::printPanel_0xE6_0x41() {
  printStatusLights();
  printPanelLights(3);

  printZoneLights();
  printPanelZones(5, 65);
}


/*
 *  0xEB: Date, time, system status messages - partitions 1-8
 *  CRC: yes
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Partition
 *  Byte 3 bit 0-3: Year digit 2
 *  Byte 3 bit 4-7: Year digit 1
 *  Byte 4 bit 0-1: Day digit part 1
 *  Byte 4 bit 2-5: Month
 *  Byte 4 bit 6-7: Unknown
 *  Byte 5 bit 0-4: Hour
 *  Byte 5 bit 5-7: Day digit part 2
 *  Byte 6 bit 0-1: Unknown
 *  Byte 6 bit 2-7: Minute
 *  Byte 7: Selects set of status commands
 *  Byte 8: Status, printPanelStatus0...printPanelStatus1X
 *  Byte 9: Unknown
 *  Byte 10: CRC
 *
 *            Partition YYY1YYY2   MMMMDD DDDHHHHH MMMMMM             Status             CRC
 *  11101011 0 00000001 00011000 00011000 10001010 00101100 00000000 10111011 00000000 10001101 [0xEB] 2018.06.04 10:11 | Partition 1 | Armed by master code 40
 *  11101011 0 00000001 00011000 00011000 10001010 00111000 00000000 10111011 00000000 10011001 [0xEB] 2018.06.04 10:14 | Partition 1 | Armed by master code 40
 *  11101011 0 00000001 00011000 00011000 10001010 00111000 00000010 10011011 00000000 01111011 [0xEB] 2018.06.04 10:14 | Partition 1 | Armed: away
 *  11101011 0 00000001 00011000 00011000 10001010 00110100 00000000 11100010 00000000 10111100 [0xEB] 2018.06.04 10:13 | Partition 1 | Disarmed by master code 40
 *  11101011 0 00000001 00011000 00011000 10001111 00101000 00000100 00000000 10010001 01101000 [0xEB] 2018.06.04 15:10 | Partition 1 | Zone alarm: 33
 *  11101011 0 00000001 00000001 00000100 01100000 00010100 00000100 01000000 10000001 00101010 [0xEB] 2001.01.03 00:05 | Partition 1 | Zone tamper: 33
 *  11101011 0 00000001 00000001 00000100 01100000 00001000 00000100 01011111 10000001 00111101 [0xEB] 2001.01.03 00:02 | Partition 1 | Zone tamper: 64
 *  11101011 0 00000001 00000001 00000100 01100000 00011000 00000100 01100000 11111111 11001100 [0xEB] 2001.01.03 00:06 | Partition 1 | Zone tamper restored: 33
 *  11101011 0 00000000 00000001 00000100 01100000 01001000 00010100 01100000 10000001 10001101 [0xEB] 2001.01.03 00:18 | Zone fault: 33
 *  11101011 0 00000000 00000001 00000100 01100000 01001100 00010100 01000000 11111111 11101111 [0xEB] 2001.01.03 00:19 | Zone fault restored: 33
 *  11101011 0 00000000 00000001 00000100 01100000 00001100 00010100 01011111 11111111 11001110 [0xEB] 2001.01.03 00:03 | Zone fault restored: 64
 *  11101011 0 00000001 00011000 00011000 00100000 01101100 00010111 00100010 00000000 11100001 [0xEB] 2018.06.01 00:27 | Partition 1 |  // Unknown data, immediate after entering *2 menu
 *  Byte 0   1    2        3        4        5        6        7        8        9        10
 */
void dscKeybusInterface::printPanel_0xEB() {
  printPanelTime(3);

  if (panelData[2] == 0) stream->print(" | ");
  else {
    stream->print(F(" | Partition "));
    printPanelBitNumbers(2, 1);
    stream->print("| ");
  }

  switch (panelData[7]) {
    case 0x00: printPanelStatus0(8); return;
    case 0x01: printPanelStatus1(8); return;
    case 0x02: printPanelStatus2(8); return;
    case 0x03: printPanelStatus3(8); return;
    case 0x04: printPanelStatus4(8); return;
    case 0x05: printPanelStatus5(8); return;
    case 0x14: printPanelStatus14(8); return;
    case 0x16: printPanelStatus16(8); return;
    case 0x17: printPanelStatus17(8); return;
    case 0x18: printPanelStatus18(8); return;
    case 0x1B: printPanelStatus1B(8); return;
  }

  printUnknownData();
}


/*
 *  0xEC: Event buffer - partitions 1-8
 *  CRC: yes
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Partition
 *  Byte 3 bit 0-3: Year digit 2
 *  Byte 3 bit 4-7: Year digit 1
 *  Byte 4 bit 0-1: Day digit part 1
 *  Byte 4 bit 2-5: Month
 *  Byte 4 bit 6-7: Event number multiplier (* 256)
 *  Byte 5 bit 0-4: Hour
 *  Byte 5 bit 5-7: Day digit part 2
 *  Byte 6 bit 0-1: Unknown
 *  Byte 6 bit 2-7: Minute
 *  Byte 7: Selects set of status commands
 *  Byte 8: Status, printPanelStatus0...printPanelStatus1X
 *  Byte 9: Event number
 *  Byte 10: CRC
 *
 *            Partition YYY1YYY2   MMMMDD DDDHHHHH MMMMMM             Status   Event#    CRC
 *  11101100 0 00000000 00100000 00101101 01100011 10111100 00000001 10101100 00001011 00010000 [0xEC] Event: 011 | 2020.11.11 03:47 | Exit *8 programming
 *  11101100 0 00000000 00100000 00110000 00100000 00000000 00000011 00001010 00001001 01110010 [0xEC] Event: 009 | 2020.12.01 00:00 | PC5204: Supervisory trouble
 *  11101100 0 00000000 00010110 00011000 10100001 01000000 00000010 10010001 11111100 10001010 [0xEC] Event: 252 | 2016.06.05 01:16 | Swinger shutdown
 *  11101100 0 00000000 00010110 01011000 10100001 00101000 00000010 10010001 00000100 10111010 [0xEC] Event: 260 | 2016.06.05 01:10 | Swinger shutdown
 *  11101100 0 00000000 00001011 01000100 00100000 00000000 11111111 11111111 01110100 11001101 [0xEC] Event: 372 | 200B.01.01 00:00 | No entry
 *  Byte 0   1    2        3        4        5        6        7        8        9        10
 *
 */
void dscKeybusInterface::printPanel_0xEC() {
  int eventNumber = panelData[9] + ((panelData[4] >> 6) * 256);
  stream->print(F("Event: "));
  if (eventNumber < 10) stream->print("00");
  else if (eventNumber < 100) stream->print("0");
  stream->print(eventNumber);
  stream->print(" | ");

  printPanelTime(3);
  stream->print(" | ");

  if (panelData[2] != 0) {
    printPartition();
    printPanelBitNumbers(2, 1);
    stream->print("| ");
  }

  switch (panelData[7]) {
    case 0x00: printPanelStatus0(8); return;
    case 0x01: printPanelStatus1(8); return;
    case 0x02: printPanelStatus2(8); return;
    case 0x03: printPanelStatus3(8); return;
    case 0x04: printPanelStatus4(8); return;
    case 0x05: printPanelStatus5(8); return;
    case 0x14: printPanelStatus14(8); return;
    case 0x16: printPanelStatus16(8); return;
    case 0x17: printPanelStatus17(8); return;
    case 0x18: printPanelStatus18(8); return;
    case 0x1B: printPanelStatus1B(8); return;
    case 0xFF: stream->print(F("No entry")); return;
  }

  printUnknownData();
}


/*
 *  Keypad: Fire alarm
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 0: 10111011
 *
 *  10111011 1 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Fire alarm
 */
void dscKeybusInterface::printModule_0xBB() {
  stream->print(F("[Keypad] Fire alarm"));
}


/*
 *  Keypad: Auxiliary alarm
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 0: 11011101
 *
 *  11011101 1 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Aux alarm
 */
void dscKeybusInterface::printModule_0xDD() {
  stream->print(F("[Keypad] Auxiliary alarm"));
}


/*
 *  Keypad: Panic alarm
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 0: 11101110
 *
 *  11101110 1 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Panic alarm
 */
void dscKeybusInterface::printModule_0xEE() {
  stream->print(F("[Keypad] Panic alarm"));
}


/*
 *  Module data during panel commands 0x05, 0x0A, 0x1B: Panel status
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Keypad keys: 0x05/0x0A Partition 1, 0x1B Partition 5
 *  Byte 3: Keypad keys: 0x05/0x0A Partition 2, 0x1B Partition 6
 *  Byte 4 bit 0: Module tamper notification, panel response: 0x4C Module tamper query
 *  Byte 4 bit 1-2: Unknown
 *  Byte 4 bit 3: Wireless module battery notification, panel response: 0x41 Wireless module query
 *  Byte 4 bit 4: Zone expander 3 notification, panel response: 0x39 Zone expander 3 query
 *  Byte 4 bit 5: Zone expander 2 notification, panel response: 0x33 Zone expander 2 query
 *  Byte 4 bit 6: Zone expander 1 notification, panel response: 0x28 Zone expander 1 query
 *  Byte 4 bit 7: Zone expander 0 notification, panel response: 0x22 Zone expander 0 query
 *  Byte 5 bit 0: Unknown
 *  Byte 5 bit 1: Wireless module unknown notification, panel response: 0xE6.25 then Module0xE6
 *  Byte 5 bit 2: Keypad zone status notification, panel response: 0xD5 Keypad zone query
 *  Byte 5 bit 3-4: Unknown
 *  Byte 5 bit 5: Module status notification, panel response: 0x58 Module status query
 *  Byte 5 bit 6-7: Unknown
 *
 *  Later generation panels:
 *  Byte 6: Unknown
 *  Byte 7 bit 0-2: Unknown
 *  Byte 7 bit 3: Keypad idle notification, panel response: 0x1B, keypad responds on byte 4 with its partition number
 *  Byte 7 bit 4: Zone expander 7 notification, panel response: 0xE6.0E Zone expander 7 query
 *  Byte 7 bit 5: Zone expander 6 notification, panel response: 0xE6.0C Zone expander 6 query
 *  Byte 7 bit 6: Zone expander 5 notification, panel response: 0xE6.0A Zone expander 5 query
 *  Byte 7 bit 7: Zone expander 4 notification, panel response: 0xE6.08 Zone expander 4 query
 *  Byte 8: Keypad keys: 0x05/0x0A Partition 3, 0x1B Partition 7
 *  Byte 9: Keypad keys: 0x05/0x0A Partition 4, 0x1B Partition 8
 *
 *  00000101 0 10000001 00000001 10010001 11000111 [0x05] Partition 1: Ready Backlight - Partition ready | Partition 2: disabled
 *
 *  Module     1:Keys   2:Keys
 *  11111111 1 00000101 11111111 11111111 11111111 [Module/0x05] Partition 1 Key: 1
 *  11111111 1 11111111 00010001 11111111 11111111 [Module/0x05] Partition 2 Key: 4
 *  11111111 1 11111111 11111111 01111111 11111111 [Module/0x05] Zone expander notification: 0
 *  11111111 1 11111111 11111111 10111111 11111111 [Module/0x05] Zone expander notification: 1
 *  11111111 1 11111111 11111111 11011111 11111111 [Module/0x05] Zone expander notification: 2
 *  11111111 1 11111111 11111111 11101111 11111111 [Module/0x05] Zone expander notification: 3
 *  11111111 1 11111111 11111111 11110111 11111111 [Module/0x05] Wireless module battery notification
 *  11111111 1 11111111 11111111 11111110 11111111 [Module/0x05] Module tamper notification
 *  11111111 1 11111111 11111111 11111111 11011111 [Module/0x05] Module status notification
 *  11111111 1 11111111 11111111 11111111 11111011 [Module/0x05] Keypad zone notification
 *  Byte 0   1    2        3        4        5
 *
 *  Later generation panels:
 *
 *  Module     1:Keys   2:Keys                                       3:Keys   4:Keys
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 01111111 11111111 11111111 [Module/0x05] Zone expander notification: 4
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 10111111 11111111 11111111 [Module/0x05] Zone expander notification: 5
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11011111 11111111 11111111 [Module/0x05] Zone expander notification: 6
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11101111 11111111 11111111 [Module/0x05] Zone expander notification: 7
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11110111 11111111 11111111 [Module/0x05] // Unknown notification, observed on PC1832
 *  Byte 0   1    2        3        4        5        6        7        8        9
 *
 *  Module     5:Keys   6:Keys                                       7:Keys   8:Keys
 *  11111111 1 00000101 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x1B] Partition 5 Key: 1
 *  Byte 0   1    2        3        4        5        6        7        8        9
 */
void dscKeybusInterface::printModule_Status() {
  bool printedMessage = false;

  // Keypad keys
  if (printModule_Keys()) printedMessage = true;

  // Keypad partition
  if (moduleCmd == 0x1B && moduleData[4] != 0xFF) {
    if (printedMessage) stream->print("| ");
    stream->print(F("Keypad on partition: "));
    printModuleSlots(1, 4, 4, 0x80, 0, 1, 0, true);
    stream->print(F("going idle"));
    printedMessage = true;
  }
  else {
    // Zone expander notification
    if ((moduleData[4] & 0xF0) != 0xF0 || (moduleByteCount > 6 && (moduleData[7] & 0xF0) != 0xF0)) {
      if (printedMessage) stream->print("| ");
      stream->print(F("Zone expander notification: "));
      printedMessage = true;

      printModuleSlots(0, 4, 4, 0x80, 0x10, 1, 0);
      if (moduleByteCount > 6) {
        printModuleSlots(4, 7, 7, 0x80, 0x20, 1, 0);
        if ((moduleData[7] & 0x10) == 0) printNumberSpace(7);
      }
    }

    // Module tamper notification, panel responds with 0x4C query
    if ((moduleData[4] & 0x01) == 0) {
      if (printedMessage) stream->print("| ");
      stream->print(F("Module tamper notification "));
      printedMessage = true;
    }

    // Wireless module battery notification
    if ((moduleData[4] & 0x08) == 0) {
      if (printedMessage) stream->print("| ");
      stream->print(F("Wireless module battery notification "));
      printedMessage = true;
    }
  }

  // Unknown wireless notification, panel responds with 0xE6.25 then Module0xE6
  if ((moduleData[5] & 0x02) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("Wireless notification "));
    printedMessage = true;
  }

  //Keypad zone notification, panel responds with 0xD5 query
  if ((moduleData[5] & 0x04) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("Keypad zone notification "));
    printedMessage = true;
  }

  // Module status notification, panel responds with 0xD5 query
  if ((moduleData[5] & 0x20) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("Module status notification "));
    printedMessage = true;
  }

  // Module status notification, panel responds with 0xD5 query
  if ((moduleData[5] & 0x40) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("Wireless key notification "));
    printedMessage = true;
  }

  //Unknown keypad notification
  if (moduleByteCount > 6 && (moduleData[7] & 0x08) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("Keypad notification "));
    printedMessage = true;
  }

  if (moduleByteCount > 6 && (moduleData[6] & 0x80) == 0 && (moduleData[6] & 0x60) != 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("Door chime broadcast "));
    printedMessage = true;
  }

  if (moduleByteCount > 6 && (moduleData[6] & 0x60) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("Zone label broadcast "));
    printedMessage = true;
  }

  if (!printedMessage) printUnknownData();
}


/*
 *  Module data during panel command 0x11: Module supervision query
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2 bit 0-1: Keypad slot 4
 *  Byte 2 bit 2-3: Keypad slot 3
 *  Byte 2 bit 4-5: Keypad slot 2
 *  Byte 2 bit 6-7: Keypad slot 1
 *  Byte 3 bit 0-1: Keypad slot 8
 *  Byte 3 bit 2-3: Keypad slot 7
 *  Byte 3 bit 4-5: Keypad slot 6
 *  Byte 3 bit 6-7: Keypad slot 5
 *
 *  Early generation panels:
 *  Byte 4 bit 0-3: Zone expander 2
 *  Byte 4 bit 4-7: Zone expander 1
 *  Byte 5 bit 0-1: PC5208 PGM module (?)
 *  Byte 5 bit 2-3: PC/RF5132
 *  Byte 5 bit 4-7: Zone expander 3
 *  Byte 6 bit 0-5: Unknown
 *  Byte 6 bit 6-7: PC5204
 *
 *  00010001 0 10101010 10101010 10101010 10101010 10101010 [0x11] Module supervision query
 *
 *              Slots
 *  Module     1 2 3 4  5 6 7 8  9   10   11
 *  11111111 1 00111111 11111111 11111111 11111111 11111111 [Module/0x11] Keypad slots: 1
 *  11111111 1 11111111 11111100 00001111 11111111 11111111 [Module/0x11] Keypad slots: 8 | Zone expander: 1
 *  Byte 0   1    2        3        4        5        6
 *
 *  Later generation panels:
 *  Byte 4 bit 0-1: Zone expander 4
 *  Byte 4 bit 2-3: Zone expander 3
 *  Byte 4 bit 4-5: Zone expander 2
 *  Byte 4 bit 6-7: Zone expander 1
 *  Byte 5 bit 0-1: PC5208 PGM module
 *  Byte 5 bit 2-3: PC/RF5132
 *  Byte 5 bit 4-5: Zone expander 6
 *  Byte 5 bit 6-7: Zone expander 5
 *  Byte 6 bit 0-5: Unknown
 *  Byte 6 bit 6-7: PC5204
 *  Byte 7 bit 0-1: Zone expander 7
 *  Byte 7 bit 2-7: Unknown
 *  Byte 8: Unknown
 *
 *  00010001 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0x11] Module supervision query
 *
 *              Slots
 *  Module     1 2 3 4  5 6 7 8  9     12   14                    16
 *  11111111 1 00111111 11111111 00001111 11110011 11111111 11111111 11111111 [Module/0x11] Keypad slots: 1 | Zone expander: 1 2 | PC/RF5132
 *  11111111 1 00111111 11111111 11001111 11111111 11111111 11111100 11111111 [Module/0x11] Keypad slots: 1 | Zone expander: 2 7
 *  11111111 1 11111111 11111100 00111111 11111111 00111111 11111111 11111111 [Module/0x11] Keypad slots: 8 | Zone expander: 1 | PC5204
 *  Byte 0   1    2        3        4        5        6        7        8
 */
void dscKeybusInterface::printModule_0x11() {
  if (moduleData[2] != 0xFF || moduleData[3] != 0xFF) {
    stream->print(F("Keypad slots: "));
    printModuleSlots(1, 2, 3, 0xC0, 0, 2, 0);
  }

  if (moduleData[4] != 0xFF || (moduleData[5] & 0xF0) != 0xF0 || ((moduleByteCount > 7) && (moduleData[7] != 0xFF))) {
    stream->print(F("| Zone expander: "));

    // Later generation panel slots
    if (moduleByteCount > 7) {
      printModuleSlots(1, 4, 5, 0xC0, 0x30, 2, 0);
      if ((moduleData[7] & 0x03) == 0) printNumberSpace(7);
    }

    // Early generation panel slots
    else {
      printModuleSlots(1, 4, 5, 0xF0, 0xF0, 4, 0);
    }
  }

  if ((moduleData[5] & 0x0C) == 0) stream->print(F("| PC/RF5132 "));
  if ((moduleData[5] & 0x03) == 0) stream->print(F("| PC5208 "));
  if ((moduleData[6] & 0xC0) == 0) stream->print(F("| PC5204 "));
}


/*
 *  Module data during panel command 0x41: Wireless module query
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2: Wireless battery low zones 1-8
 *  Byte 3: Wireless battery low zones 9-16
 *  Byte 4: Wireless battery low zones 17-24
 *  Byte 5: Wireless battery low zones 25-32
 *  Byte 6: Wireless battery restored zones 1-8
 *  Byte 7: Wireless battery restored zones 9-16
 *  Byte 8: Wireless battery restored zones 17-24
 *  Byte 9: Wireless battery restored zones 25-32
 *  Byte 10: Unknown
 *
 *  11111111 1 11111111 11111111 11110111 11111111 [Module/0x05] Wireless module battery notification
 *  01000001 0 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [0x41] Wireless module query
 *
 *  Module     Zones1+  Zones9+  Zones17+ Zones25+ Zones1+  Zones9+  Zones17+ Zones25+
 *  11111111 1 11111111 11011111 11111111 11111111 11111111 11111111 11111111 11111111 11011000 [Module/0x41] Wireless module | Battery low zones: 11
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11011111 11111111 11111111 11011000 [Module/0x41] Wireless module | Battery restored zones: 11
 *  Byte 0   1    2        3        4        5        6        7        8        9        10
 */
void dscKeybusInterface::printModule_0x41() {
  stream->print(F("Wireless module "));

  if (printModuleSlots(255, 2, 5, 0x80, 0, 1, 0)) {
    stream->print(F("| Battery low zones: "));
    printModuleSlots(1, 2, 5, 0x80, 0, 1, 0);
  }

  if (printModuleSlots(255, 6, 9, 0x80, 0, 1, 0)) {
    stream->print(F("| Battery restored zones: "));
    printModuleSlots(1, 6, 9, 0x80, 0, 1, 0);
  }
}

/*
 *  Module data during panel command 0x4C: Module tamper query
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Bits 0-3 or 4-7 "0000" indicates a module is still tampered since a previous notification
 *
 *  Byte 2-5 bit 0-1: Keypad slot 2,4,6,8 tamper restore
 *  Byte 2-5 bit 2-3: Keypad slot 2,4,6,8 tamper
 *  Byte 2-5 bit 4-5: Keypad slot 1,3,5,7 tamper restore
 *  Byte 2-5 bit 6-7: Keypad slot 1,3,5,7 tamper
 *  Byte 6-8 bit 0-1: Module slot 10,12,14 tamper restore
 *  Byte 6-8 bit 2-3: Module slot 10,12,14 tamper
 *  Byte 6-8 bit 4-5: Module slot 9,11,13 tamper restore
 *  Byte 6-8 bit 6-7: Module slot 9,11,13 tamper
 *  Byte 9 bit 0-1: PC5208 tamper restore
 *  Byte 9 bit 2-3: PC5208 tamper
 *  Byte 9 bit 4-5: RF5132 tamper restore
 *  Byte 9 bit 6-7: RF5132 tamper
 *  Byte 10 bit 0-3: Unknown
 *  Byte 10 bit 4-5: PC5204 tamper restore
 *  Byte 10 bit 6-7: PC5204 tamper
 *  Byte 11: Unknown
 *  Byte 12: Unknown
 *
 *  Later generation panels:
 *  Byte 13 bit 0-3: Unknown
 *  Byte 13 bit 4-5: Module slot 16 tamper restore
 *  Byte 13 bit 6-7: Module slot 16 tamper
 *  Byte 14: Unknown
 *
 *  11111111 1 11111111 11111111 11111110 11111111 [Module/0x05] Module tamper notification
 *  01001100 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0x4C] Module tamper query
 *
 *  Module   Slots:1  2    3   4    5   6    7   8    9  10   11  12   13  14
 *  11111111 1 00111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Keypad tamper: Slot 1
 *  11111111 1 11001111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Keypad tamper restored: Slot 1
 *  11111111 1 11110011 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Keypad tamper: Slot 2
 *  11111111 1 11111100 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Keypad tamper restored: Slot 2
 *  11111111 1 11111111 11001111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Keypad tamper restored: Slot 3
 *  11111111 1 11111111 11111111 11110011 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Keypad tamper: Slot 6
 *  11111111 1 11111111 11111111 11111111 11110011 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Keypad tamper: Slot 8
 *  11111111 1 11111111 11111111 11111111 11111100 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Keypad tamper restored: Slot 8
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 00111111 11111111 11111111 11111111 [Module/0x4C] RF5132: Tamper
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11001111 11111111 11111111 11111111 [Module/0x4C] RF5132: Tamper restored
 *  Byte 0   1    2        3        4        5        6        7        8        9        10       11       12
 *
 *  Later generation panels:
 *  11111111 1 11111111 11111111 11111110 11111111 11111111 11111111 11111111 11111111 [Module/0x05] Module tamper notification
 *  01001100 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0x4C] Module tamper query
 *
 *  Module   Slots:1  2    3   4    5   6    7   8    9  10   11  12   13  14                                       16
 *  11111111 1 11111111 11111111 11111111 11111111 00111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Module tamper: Slot 9
 *  11111111 1 11111111 11111111 11111111 11111111 11001111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Module tamper restored: Slot 9
 *  11111111 1 11111111 11111111 11111111 11111111 11110011 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Module tamper: Slot 10
 *  11111111 1 11111111 11111111 11111111 11111111 11111100 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Module tamper restored: Slot 10
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11110011 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Module tamper: Slot 14
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111100 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Module tamper restored: Slot 14
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 00111111 11111111 [Module/0x4C] Module tamper: Slot 16
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11001111 11111111 [Module/0x4C] Module tamper restored: Slot 16
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 00001111 11111111 00111111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Module tamper: Slot 11 | RF5132: Tamper
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11001111 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] RF5132: Tamper restored
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11110011 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] PC5208: Tamper
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111100 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] PC5208: Tamper restored
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 00111111 11111111 11111111 11111111 11111111 [Module/0x4C] PC5204: Tamper
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11001111 11111111 11111111 11111111 11111111 [Module/0x4C] PC5204: Tamper restored
 *  11111111 1 00001111 11111111 11111111 11111111 11111111 00001111 11111111 00110000 11111111 11111111 11111111 11111111 11111111 [Module/0x4C] Keypad tamper: Slot 1 | Module tamper: Slot 11 | RF5132: Tamper | PC5208: Tamper
 *  Byte 0   1    2        3        4        5        6        7        8        9        10       11       12       13       14
 */
void dscKeybusInterface::printModule_0x4C() {
  bool printedMessage = false;

  if (printModuleSlots(255, 2, 5, 0xC0, 0, 4, 0)) {
    stream->print(F("Keypad tamper: Slot "));
    printModuleSlots(1, 2, 5, 0xC0, 0, 4, 0);
    printedMessage = true;
  }

  if (printModuleSlots(255, 2, 5, 0xF0, 0, 4, 0x0C)) {
    if (printedMessage) stream->print("| ");
    stream->print(F("Keypad tamper restored: Slot "));
    printModuleSlots(1, 2, 5, 0xF0, 0, 4, 0x0C);
    printedMessage = true;
  }

  if (printModuleSlots(255, 6, 8, 0xC0, 0, 4, 0) || (moduleByteCount > 13 && (moduleData[13] & 0xC0) == 0)) {
    if (printedMessage) stream->print("| ");
    stream->print(F("Module tamper: Slot "));
    printModuleSlots(9, 6, 8, 0xC0, 0, 4, 0);
    if (moduleByteCount > 13 && (moduleData[13] & 0xC0) == 0) printNumberSpace(16);
    printedMessage = true;
  }

  if (printModuleSlots(255, 6, 8, 0xF0, 0, 4, 0x0C) || (moduleByteCount > 13 && (moduleData[13] & 0xF0) == 0xC0)) {
    if (printedMessage) stream->print("| ");
    stream->print(F("Module tamper restored: Slot "));
    printModuleSlots(9, 6, 8, 0xF0, 0, 4, 0x0C);
    if (moduleByteCount > 13 && (moduleData[13] & 0xF0) == 0xC0) printNumberSpace(16);
    printedMessage = true;
  }

  if ((moduleData[9] & 0xC0) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("RF5132: Tamper "));
    printedMessage = true;
  }

  if ((moduleData[9] & 0xF0) == 0xC0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("RF5132: Tamper restored "));
    printedMessage = true;
  }

  if ((moduleData[9] & 0x0C) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("PC5208: Tamper "));
    printedMessage = true;
  }

  if ((moduleData[9] & 0x0F) == 0x0C) {
    if (printedMessage) stream->print("| ");
    stream->print(F("PC5208: Tamper restored "));
    printedMessage = true;
  }

  if ((moduleData[10] & 0xC0) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("PC5204: Tamper "));
    printedMessage = true;
  }

  if ((moduleData[10] & 0xF0) == 0xC0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("PC5204: Tamper restored "));
    printedMessage = true;
  }
}


/*
 *  Module data during panel command 0x57: Wireless key query
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2 bit 0-1: Wireless key 1 battery
 *  Byte 2 bit 2-3: Wireless key 2 battery
 *  Byte 2 bit 4-5: Wireless key 3 battery
 *  Byte 2 bit 6-7: Wireless key 4 battery
 *  Byte 3: Wireless keys 5-8 battery
 *  Byte 4: Wireless keys 9-12 battery
 *  Byte 5: Wireless keys 13-16 battery
 *  Byte 6: Unknown
 *  Byte 7: Unknown
 *  Byte 8: Unknown
 *  Byte 9: Unknown
 *  Byte 10: Unknown
 *
 *  11111111 1 11111111 11111111 11111111 10111111 [Module/0x05] Wireless key notification
 *  01010111 0 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [0x57] Wireless key query
 *
 *  Module     Keys1+   Keys5+   Keys9+   Keys13+
 *  11111111 1 11111101 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11110110 [Module/0x57] Wireless key low battery: 1
 *  11111111 1 11111110 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11110111 [Module/0x57] Wireless key battery restored: 1
 *  11111111 1 11111111 11111111 01111111 11111111 11111111 11111111 11111111 11111111 01111000 [Module/0x57] Wireless key low battery: 12
 *  11111111 1 11111111 11111111 11111111 10111111 11111111 11111111 11111111 11111111 10111000 [Module/0x57] Wireless key battery restored: 16
 *  11111111 1 11111011 11111111 01111111 11111111 11111111 11111111 11111111 11111111 01111000 [Module/0x57] Wireless key low battery: 12 | Wireless key battery restored: 2
 *  Byte 0   1    2        3        4        5        6        7        8        9        10
 */
void dscKeybusInterface::printModule_0x57() {
  bool printedMessage = false;

  if (printModuleSlots(255, 2, 5, 0xC0, 0, 2, 0x02, true)) {
    stream->print(F("Wireless key low battery: "));
    printModuleSlots(1, 2, 5, 0xC0, 0, 2, 0x02, true);
    printedMessage = true;
  }

  if (printModuleSlots(255, 2, 5, 0xC0, 0, 2, 0x01, true)) {
    if (printedMessage) stream->print(F("| "));
    stream->print(F("Wireless key battery restored: "));
    printModuleSlots(1, 2, 5, 0xC0, 0, 2, 0x01, true);
  }
}


/*
 *  Module data during panel command 0x58: Module status query
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Byte 2 bit 0-1: PC5204 battery restored
 *  Byte 2 bit 2-3: PC5204 battery trouble
 *  Byte 2 bit 4-5: PC5204 AC power restored
 *  Byte 2 bit 6-7: PC5204 AC power trouble
 *  Byte 3 bit 0-1: PC5204 output 1 restored
 *  Byte 3 bit 2-3: PC5204 output 1 trouble
 *  Byte 3 bit 4-7: Unknown
 *  Byte 4: Unknown
 *  Byte 5: Unknown
 *
 *  Later generation panels:
 *  Byte 6: Unknown
 *  Byte 7: Unknown
 *  Byte 8: Unknown
 *  Byte 9: Unknown
 *  Byte 10: Unknown
 *  Byte 11: Unknown
 *  Byte 12: Unknown
 *  Byte 13: Unknown
 *
 *  11111111 1 11111111 11111111 11111111 11011111 [Module/0x05] Module status notification
 *  01011000 0 10101010 10101010 10101010 10101010 [0x58] Module status query
 *
 *  Module     PC5204
 *  11111111 1 11111100 11111111 11111111 11111111 [Module/0x58] PC5204: Battery restored
 *  Byte 0   1    2        3        4        5
 *
 *  Later generation panels:
 *  11111111 1 11111111 11111111 11111111 11011111 11111111 11111111 11111111 11111111 [Module/0x05] Module status notification
 *  01011000 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0x58] Module status query
 *
 *  Module     PC5204
 *  11111111 1 00111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x58] PC5204: AC power trouble
 *  Byte 0   1    2        3        4        5        6        7        8        9        10       11       12       13
 */
void dscKeybusInterface::printModule_0x58() {
  bool printedMessage = false;

  if ((moduleData[2] & 0x03) == 0) {
    stream->print(F("PC5204: Battery restored "));
    printedMessage = true;
  }

  if ((moduleData[2] & 0x0C) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("PC5204: Battery trouble "));
    printedMessage = true;
  }

  if ((moduleData[2] & 0x30) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("PC5204: AC power restored "));
    printedMessage = true;
  }

  if ((moduleData[2] & 0xC0) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("PC5204: AC power trouble "));
    printedMessage = true;
  }

  if ((moduleData[3] & 0x03) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("PC5204: Output 1 restored "));
    printedMessage = true;
  }

  if ((moduleData[3] & 0x0C) == 0) {
    if (printedMessage) stream->print("| ");
    stream->print(F("PC5204: Output 1 trouble "));
    printedMessage = true;
  }

  if (!printedMessage) printUnknownData();
}


/*
 *  Module data during panel command 0x70: LCD keypad data query
 *  Structure decoding: complete
 *  Content decoding: complete
 *
 *  Byte 2 bits 0-3: Digit 1
 *  Byte 2 bits 4-7: Digit 2
 *  Byte 3 bits 0-3: Digit 3
 *  Byte 3 bits 4-7: Digit 4
 *  Byte 4 bits 0-3: Digit 5
 *  Byte 4 bits 4-7: Digit 6
 *  Byte 5 bits 0-3: Digit 7
 *  Byte 5 bits 4-7: Digit 8
 *  Byte 6: Unknown
 *
 *  11111111 1 11111111 10101010 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x05] Partition 2 Key: Submit data
 *  01110000 0 11111111 11111111 11111111 11111111 11111111 [0x70] LCD keypad data query
 *
 *  Module      Digit    Digit    Digit    Digit
 *  11111111 1 00010101 00010101 00000000 00000000 00101010 [Module/0x70]
 *  Byte 0   1    2        3        4        5        6
 */
void dscKeybusInterface::printModule_0x70() {
  stream->print(F("LCD keypad data entry: "));
  if (decimalInput) {
    if (moduleData[2] <= 0x63) stream->print("0");
    if (moduleData[2] <= 0x09) stream->print("0");
    stream->print(moduleData[2], DEC);
  }
  else {
    for (byte moduleByte = 2; moduleByte <= 5; moduleByte ++) {
      stream->print(moduleData[moduleByte] >> 4, HEX);
      stream->print(moduleData[moduleByte] & 0x0F, HEX);
    }
  }
}


/*
 *  Module data during panel command 0x94: Requesting module programming data
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 */
void dscKeybusInterface::printModule_0x94() {
  stream->print(F("Module programming response"));
}


/*
 *  Module data during panel command: 0xD5 Keypad zone query
 *  Structure decoding: *incomplete
 *  Content decoding: *incomplete
 *
 *  Bytes 2-9: Keypad slots 1-8
 *  Bit 0-1: Keypad zone closed
 *  Bit 2-3: Unknown
 *  Bit 4-5: Keypad zone open
 *  Bit 6-7: Unknown
 *
 *  11111111 1 11111111 11111111 11111111 11111011 [Module/0x05] Keypad zone status notification
 *  11010101 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0xD5] Keypad zone query
 *
 *  Module      Slot 1   Slot 2   Slot 3   Slot 4   Slot 5   Slot 6   Slot 7   Slot 8
 *  11111111 1 00000011 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0xD5] Keypad slot 1: Zone open
 *  11111111 1 00001111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0xD5] Keypad slot 1: Zone open    // Exit *8 programming
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 00001111 [Module/0xD5] Keypad slot 8: Zone open    // Exit *8 programming
 *  11111111 1 11110000 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0xD5] Keypad slot 1: Zone closed  // Zone closed while unconfigured
 *  11111111 1 11110011 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0xD5] Keypad slot 1               // Zone closed while unconfigured after opened once
 *  11111111 1 00110000 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0xD5] Keypad slot 1: Zone closed  // NC
 *  11111111 1 00111100 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0xD5] Keypad slot 1: Zone closed  // After exiting *8 programming after NC
 *  Byte 0   1    2        3        4        5        6        7        8        9
 */
void dscKeybusInterface::printModule_0xD5() {
  stream->print(F("Keypad "));
  bool firstData = true;
  for (byte moduleByte = 2; moduleByte <= 9; moduleByte++) {
    byte slotData = moduleData[moduleByte];
    if (slotData < 0xFF) {
      if (firstData) {
        stream->print(F("Slot "));
        firstData = false;
      }
      else stream->print(F(" | Slot "));

      stream->print(moduleByte - 1);
      if ((slotData & 0x03) == 0x03 && (slotData & 0x30) == 0) stream->print(F(": Zone open"));
      if ((slotData & 0x03) == 0 && (slotData & 0x30) == 0x30) stream->print(F(": Zone closed"));
    }
  }
}


/*
 *  Keypad keys
 *
 *  All panels, command 0x05:
 *  Byte 2: Partition 1 keys
 *  Byte 3: Partition 2 keys
 *
 *  Later generation panels, command 0x05:
 *  Byte 2: Partition 1 keys
 *  Byte 3: Partition 2 keys
 *  Byte 8: Partition 3 keys
 *  Byte 9: Partition 4 keys
 *
 *  Later generation panels, command 0x1B:
 *  Byte 2: Partition 5 keys
 *  Byte 3: Partition 6 keys
 *  Byte 8: Partition 7 keys
 *  Byte 9: Partition 8 keys
 *
 *  Early generation panels:
 *
 *  Module      1:Keys   2:Keys
 *  11111111 1 00000101 11111111 11111111 11111111 [Module/0x05] Partition 1 Key: 1
 *  11111111 1 00101101 11111111 11111111 11111111 [Module/0x05] Partition 1 Key: #
 *  11111111 1 11111111 00010001 11111111 11111111 [Module/0x05] Partition 2 Key: 4
 *  Byte 0   1    2        3        4        5
 *
 *  Later generation panels:
 *
 *  Module      1:Keys   2:Keys                                       3:Keys   4:Keys
 *  11111111 1 00101000 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x05] Partition 1 Key: *
 *  11111111 1 11111111 00010001 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x05] Partition 2 Key: 4
 *  Byte 0   1    2        3        4        5        6        7        8        9
 *
 *  Module      5:Keys   6:Keys                                       7:Keys   8:Keys
 *  11111111 1 00000101 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0x1B] Partition 5 Key: 1
 *  Byte 0   1    2        3        4        5        6        7        8        9
 *
 *  Module               1:Keys   2:Keys                                       3:Keys   4:Keys
 *  11111111 1 11111111 11110111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Module/0xE6.20] Partition 1 Key: Menu navigation
 *  Byte 0   1    2        3        4        5        6        7        8        9        10
 *
 */
bool dscKeybusInterface::printModule_Keys() {
  bool printedMessage = false;

  byte partitionNumber = 1;
  byte startByte = 2;
  byte endByte = 9;
  switch (moduleCmd) {
    case 0x1B: partitionNumber = 5; break;
    case 0x27:
    case 0x2D:
    case 0x3E: endByte = 3; break;
    case 0xE6: {
        startByte = 3;
        endByte = 3;
        switch (moduleSubCmd) {
            case 0x01: partitionNumber = 3; break;
            case 0x02: partitionNumber = 4; break;
            case 0x03: partitionNumber = 5; break;
            case 0x04: partitionNumber = 6; break;
            case 0x05: partitionNumber = 7; break;
            case 0x06: partitionNumber = 8; break;
            case 0x20: partitionNumber = 1; break;
            case 0x21: partitionNumber = 2; break;
        }
        break;
    }
  }

  for (byte testByte = startByte; testByte <= endByte; testByte++) {
    if (testByte == 4 && moduleByteCount <= 6) return printedMessage;
    else if (testByte == 4) testByte = 8;

    if (moduleData[testByte] != 0xFF) {
        if (hideKeypadDigits && moduleData[testByte] <= 0x27) {
            stream->print(F("[Digit] "));
        }
        else {
          stream->print(F("Partition "));
          printNumberSpace(partitionNumber);
          stream->print(F("Key: "));
          printModule_KeyCodes(testByte);
        }
        printedMessage = true;
    }
    partitionNumber++;
  }

  return printedMessage;
}


// Keypad key values
void dscKeybusInterface::printModule_KeyCodes(byte keyByte) {
  switch (moduleData[keyByte]) {
    case 0x00: printNumberSpace(0); break;
    case 0x05: printNumberSpace(1); break;
    case 0x0A: printNumberSpace(2); break;
    case 0x0F: printNumberSpace(3); break;
    case 0x11: printNumberSpace(4); break;
    case 0x16: printNumberSpace(5); break;
    case 0x1B: printNumberSpace(6); break;
    case 0x1C: printNumberSpace(7); break;
    case 0x22: printNumberSpace(8); break;
    case 0x27: printNumberSpace(9); break;
    case 0x28: stream->print(F("* ")); break;
    case 0x2D: stream->print(F("# ")); break;
    case 0x46: stream->print(F("Wireless key disarm ")); break; //isn't send if wls keys uses access codes 17-32 is enabled
    case 0x52: stream->print(F("Identified voice prompt help ")); break;
    case 0x6E: stream->print(F("Global away arm ")); break;
    case 0x70: stream->print(F("Command output 3 ")); break;
    case 0x7A: stream->print(F("Time and date programming ")); break;
    case 0x75: stream->print(F("Entered *1/*2/*3 ? ")); break;
    case 0x82: stream->print(F("Enter ")); break;
    case 0x87: stream->print(F("Right arrow ")); break;
    case 0x88: stream->print(F("Left arrow ")); break;
    case 0x8D: stream->print(F("Bypass recall ")); break;
    case 0x93: stream->print(F("Recall bypass group ")); break;
    case 0x94: stream->print(F("Global label broadcast ")); break;
    case 0x99: stream->print(F("Function key [25] Future Use ")); break;
    case 0xA5: stream->print(F("Receive data ")); break;
    case 0xAA: stream->print(F("Submit data ")); break;
    case 0xAF: stream->print(F("Arm: Stay ")); break;
    case 0xB1: stream->print(F("Arm: Away ")); break;
    case 0xB6: stream->print(F("Arm: No entry delay ")); break;
    case 0xBB: stream->print(F("Door chime configuration ")); break;
    case 0xBC: stream->print(F("*6 System test ")); break;
    case 0xC3: stream->print(F("*1 Zone bypass programming ")); break;
    case 0xC4: stream->print(F("*2 Trouble menu ")); break;
    case 0xC9: stream->print(F("*3 Alarm memory display ")); break;
    case 0xCE: stream->print(F("*5 Programming ")); break;
    case 0xD0: stream->print(F("*6 Programming ")); break;
    case 0xD5: stream->print(F("Command output 1 ")); break;
    case 0xDA: stream->print(F("Reset / Command output 2 ")); break;
    case 0xDF: stream->print(F("Global stay arm ")); break;
    case 0xE1: stream->print(F("Quick exit ")); break;
    case 0xE6: stream->print(F("Activate stay/away zones ")); break;
    case 0xEB: stream->print(F("LCD pixel test ")); break;
    case 0xEC: stream->print(F("Command output 4 ")); break;
    case 0xF2: stream->print(F("Global disarm ")); break;  // Possible overlap with "General voice prompt help"
    case 0xF7: stream->print(F("Menu navigation ")); break;
  }
}


/*
 *  Zone expander zone status module response for panel commands: 0x22, 0x28, 0x33, 0x39, 0xE6.8, 0xE6.A, 0xE6.C, 0xE6.E
 */
void dscKeybusInterface::printModule_Expander() {
  byte startByte = 2;
  byte startZone = 1;

  stream->print(F("Zone expander: "));
  switch (moduleCmd) {
    case 0x22: printNumberSpace(0); startZone = 1; break;
    case 0x28: printNumberSpace(1); startZone = 9; break;
    case 0x33: printNumberSpace(2); startZone = 17; break;
    case 0x39: printNumberSpace(3); startZone = 25; break;
    case 0xE6: {
      startByte = 3;
      switch (moduleSubCmd) {
        case 0x08: printNumberSpace(4); startZone = 33; break;
        case 0x0A: printNumberSpace(5); startZone = 41; break;
        case 0x0C: printNumberSpace(6); startZone = 49; break;
        case 0x0E: printNumberSpace(7); startZone = 57; break;
      }
      break;
    }
  }

  if (moduleData[startByte] != moduleData[startByte + 1] || moduleData[startByte + 2] != moduleData[startByte + 3]) {
    stream->print(F("| Zones changed: "));

    byte zoneNumber = startZone;
    for (byte zoneByte = startByte; zoneByte <= startByte + 2; zoneByte+= 2) {
      byte zoneMaskShift = 0;
      for (byte zoneMask = 0x03; zoneMask != 0; zoneMask <<= 2) {
        if ((moduleData[zoneByte] & zoneMask) != (moduleData[zoneByte + 1] & zoneMask)) {
          printNumberSpace(zoneNumber);

          switch ((moduleData[zoneByte] & zoneMask) >> zoneMaskShift) {
            case 0: stream->print(F("open ")); break;
            case 1: stream->print(F("closed ")); break;
            case 2: stream->print(F("tamper ")); break;
            case 3: stream->print(F("open (D/EOL) ")); break;
          }
        }
        zoneNumber++;
        zoneMaskShift += 2;
      }
    }
  }
}


/*
 *  Prints slots and zones for module responses to panel commands: 0x11, 0x41, 0x4C, 0x57
 *
 *  If outputNumber is set to 0, printModuleSlots() will return 'true' if any of the specified
 *  bytes contains data - this is used to selectively print a label only if data is present.
 */
bool dscKeybusInterface::printModuleSlots(byte outputNumber, byte startByte, byte endByte, byte startMask, byte endMask, byte bitShift, byte matchValue, bool reverse) {
  for (byte testByte = startByte; testByte <= endByte; testByte++) {
    byte matchShift = 8 - bitShift;
    for (byte testMask = startMask; testMask != 0; testMask >>= bitShift) {
      if (testByte == endByte && testMask < endMask) return false;

      byte testData = moduleData[testByte];

      // Reverses the bit order
      if (reverse) {
        testData = 0;
        for (byte i = 0; i < 8; i++) testData |= ((moduleData[testByte] >> i) & 1) << (7 - i);
      }

      if ((testData & testMask) >> matchShift == matchValue) {
        if (outputNumber == 255) return true;
        else printNumberSpace(outputNumber);
      }

      if (outputNumber != 255) outputNumber++;
      matchShift -= bitShift;
    }
  }

  return false;
}


/*
 *  Panel lights and status message for commands: 0x05, 0x1B, 0x27, 0x2D, 0x34, 0x3E
 */
void dscKeybusInterface::printPanelPartitionStatus(byte startPartition, byte startByte, byte endByte) {
  byte partitionCount = startPartition;
  for (byte statusByte = startByte; statusByte <= endByte; statusByte += 2) {
    if (partitionCount > startPartition) stream->print(" | ");

    printPartition();
    stream->print(partitionCount);
    stream->print(": ");

    if (panelData[statusByte] == 0 || panelData[statusByte] == 0xC7 || panelData[statusByte] == 0xFF) stream->print(F("disabled"));
    else printPanelLights(statusByte - 1);

    partitionCount++;
  }
}


/*
 *  Date and time for panel commands: 0xA5, 0xAA, 0xEB, 0xEC
 *  Structure decoding: complete
 *  Content decoding: complete
 */
void dscKeybusInterface::printPanelTime(byte panelByte) {
  byte dscYear3 = panelData[panelByte] >> 4;
  byte dscYear4 = panelData[panelByte] & 0x0F;
  byte dscMonth = panelData[panelByte + 1] << 2; dscMonth >>= 4;
  byte dscDay1 = panelData[panelByte + 1] << 6; dscDay1 >>= 3;
  byte dscDay2 = panelData[panelByte + 2] >> 5;
  byte dscDay = dscDay1 | dscDay2;
  byte dscHour = panelData[panelByte + 2] & 0x1F;
  byte dscMinute = panelData[panelByte + 3] >> 2;

  if (dscYear3 >= 7) stream->print(F("19"));
  else stream->print(F("20"));
  stream->print(dscYear3);
  stream->print(dscYear4, HEX);
  stream->print(".");
  if (dscMonth < 10) stream->print("0");
  stream->print(dscMonth);
  stream->print(".");
  if (dscDay < 10) stream->print("0");
  stream->print(dscDay);
  stream->print(" ");
  if (dscHour < 10) stream->print("0");
  stream->print(dscHour);
  stream->print(F(":"));
  if (dscMinute < 10) stream->print("0");
  stream->print(dscMinute);
}


/*
 *  Prints access codes for printPanelStatus0()...printPanelStatus1B() status messages
 *  Structure decoding: complete
 *  Content decoding: complete
 */
void dscKeybusInterface::printPanelAccessCode(byte dscCode, bool accessCodeIncrease) {

  if (accessCodeIncrease) {
    if (dscCode >= 35) dscCode += 5;
  }
  else {
    if (dscCode >= 40) dscCode += 3;
  }


  switch (dscCode) {
    case 40: stream->print(F("Master ")); break;
    default: stream->print(F("Access ")); break;
  }
  stream->print(F("code "));
  stream->print(dscCode);
}


/*
 *  Beeps number for panel commands: 0x64, 0x69, 0xE6_0x19
 *  Structure decoding: complete
 *  Content decoding: complete
 */
void dscKeybusInterface::printPanelBeeps(byte panelByte) {
  stream->print(F("| Beep: "));
  stream->print(panelData[panelByte] / 2);
  stream->print(F(" beeps"));
}


/*
 *  Tone pattern for panel commands: 0x75, 0x7A, 0xE6_0x20
 *  Structure decoding: complete
 *  Content decoding: complete
 */
void dscKeybusInterface::printPanelTone(byte panelByte) {
  stream->print(F("| Tone: "));

  bool printedMessage = false;
  if (panelData[panelByte] == 0) {
    stream->print(F("none"));
    return;
  }

  if ((panelData[panelByte] & 0x80) == 0x80) {
    stream->print(F("constant tone "));
    printedMessage = true;
  }

  if ((panelData[panelByte] & 0x70) != 0) {
    if (printedMessage) stream->print("| ");
    stream->print((panelData[panelByte] & 0x70) >> 4);
    stream->print(F(" beep "));
  }

  if ((panelData[panelByte] & 0x0F) != 0) {
    stream->print("| ");
    stream->print(panelData[panelByte] & 0x0F);
    stream->print(F("s interval"));
  }
}


/*
 *  Buzzer pattern for panel commands: 0x7F, 0x82
 *  Structure decoding: complete
 *  Content decoding: complete
 */
void dscKeybusInterface::printPanelBuzzer(byte panelByte) {
  stream->print(F("| Buzzer: "));
  stream->print(panelData[panelByte]);
  stream->print("s");
}


/*
 *  Zones for panel commands: 0x0A, 0x5D, 0x63, 0xB1, 0xE6.17, 0xE6.18, 0xE6.20, 0xE6.2B, 0xE6.2C, 0xE6.41
 *  Structure decoding: complete
 *  Content decoding: complete
 */
bool dscKeybusInterface::printPanelZones(byte inputByte, byte startZone) {
  bool zonesEnabled = false;
  for (byte panelByte = inputByte; panelByte <= inputByte + 3; panelByte++) {
    if (panelData[panelByte] != 0) {
      zonesEnabled = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + startZone) + ((panelByte - inputByte) * 8));
          stream->print(" ");
        }
      }
    }
  }
  if (!zonesEnabled && panelData[0] != 0x0A && panelData[0] != 0x0F) stream->print(F("none"));

  return zonesEnabled;
}


void dscKeybusInterface::printPartition() {
  stream->print(F("Partition "));
}


void dscKeybusInterface::printUnknownData() {
  stream->print("Unknown data");
}


void dscKeybusInterface::printStatusLights() {
  stream->print(F("Status lights: "));
}


void dscKeybusInterface::printZoneLights(bool lowerRange) {
  if (lowerRange) stream->print(F(" | Zones 1-32 lights: "));
  else  stream->print(F(" | Zones 33-64 lights: "));
}


void dscKeybusInterface::printStatusLightsFlashing() {
  stream->print(F("| Status lights flashing: "));
}


void dscKeybusInterface::printNumberSpace(byte number) {
  stream->print(number);
  stream->print(" ");
}


void dscKeybusInterface::printNumberOffset(byte panelByte, int numberOffset) {
    stream->print(panelData[panelByte] + numberOffset);
}


// Prints individual bits as a number for partitions and zones
void dscKeybusInterface::printPanelBitNumbers(byte panelByte, byte startNumber, byte startBit, byte stopBit, bool printNone) {
  if (printNone && panelData[panelByte] == 0) stream->print(F("none "));
  else {
    byte bitCount = 0;
    for (byte bit = startBit; bit <= stopBit; bit++) {
      if (bitRead(panelData[panelByte], bit)) {
        stream->print(startNumber + bitCount);
        stream->print(" ");
      }
      bitCount++;
    }
  }
}


// Prints the panel message as binary with an optional parameter to print spaces between bytes
void dscKeybusInterface::printPanelBinary(bool printSpaces) {
  for (byte panelByte = 0; panelByte < panelByteCount; panelByte++) {
    if (panelByte == 1) stream->print(panelData[panelByte]);  // Prints the stop bit
    else {
      for (byte mask = 0x80; mask; mask >>= 1) {
        if (mask & panelData[panelByte]) stream->print("1");
        else stream->print("0");
      }
    }
    if (printSpaces && (panelByte != panelByteCount - 1 || displayTrailingBits)) stream->print(" ");
  }

  if (displayTrailingBits) {
    byte trailingBits = (panelBitCount - 1) % 8;
    if (trailingBits > 0) {
      for (int i = trailingBits - 1; i >= 0; i--) {
        stream->print(bitRead(panelData[panelByteCount], i));
      }
    }
  }
}


// Prints the module message as binary with an optional parameter to print spaces between bytes
void dscKeybusInterface::printModuleBinary(bool printSpaces) {
  for (byte moduleByte = 0; moduleByte < moduleByteCount; moduleByte++) {
    if (moduleByte == 1) stream->print(moduleData[moduleByte]);  // Prints the stop bit
    else if (hideKeypadDigits
            && (moduleByte == 2 || moduleByte == 3 || moduleByte == 8 || moduleByte == 9)
            && (moduleData[2] <= 0x27 || moduleData[3] <= 0x27 || moduleData[8] <= 0x27 || moduleData[9] <= 0x27)
            && !queryResponse)
              stream->print(F("........"));  // Hides keypad digits
    else {
      for (byte mask = 0x80; mask; mask >>= 1) {
        if (mask & moduleData[moduleByte]) stream->print("1");
        else stream->print("0");
      }
    }
    if (printSpaces && (moduleByte != moduleByteCount - 1 || displayTrailingBits)) stream->print(" ");
  }

  if (displayTrailingBits) {
    byte trailingBits = (moduleBitCount - 1) % 8;
    if (trailingBits > 0) {
      for (int i = trailingBits - 1; i >= 0; i--) {
        stream->print(bitRead(moduleData[moduleByteCount], i));
      }
    }
  }
}


// Prints the panel command as hex
void dscKeybusInterface::printPanelCommand() {
  stream->print(F("0x"));
  if (panelData[0] < 16) stream->print("0");
  stream->print(panelData[0], HEX);

  if (panelData[0] == 0xE6) {
    stream->print(".");
    if (panelData[2] < 16) stream->print("0");
    stream->print(panelData[2], HEX);
  }
}
