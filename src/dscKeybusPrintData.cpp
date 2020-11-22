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

/*
 *  printPanelMessage() checks the first byte of a message from the panel for
 *  known commands
 */


void dscKeybusInterface::printPanelMessage() {
  switch (panelData[0]) {
    case 0x05: printPanel_0x05(); return;  // Panel status: partitions 1-4
    case 0x0A: printPanel_0x0A(); return;  // Panel status in alarm/programming, partitions 1-4
    case 0x11: printPanel_0x11(); return;  // Module supervision query
    case 0x16: printPanel_0x16(); return;  // Zone wiring
    case 0x1B: printPanel_0x1B(); return;  // Panel status: partitions 5-8
    case 0x1C: printPanel_0x1C(); return;  // Verify keypad Fire/Auxiliary/Panic
    case 0x27: printPanel_0x27(); return;  // Panel status and zones 1-8 status
    case 0x28: printPanel_0x28(); return;  // Zone expander zones 9-16 query
    case 0x2D: printPanel_0x2D(); return;  // Panel status and zones 9-16 status
    case 0x33: printPanel_0x33(); return;  // Zone expander zones 17-24 query
    case 0x34: printPanel_0x34(); return;  // Panel status and zones 17-24 status
    case 0x39: printPanel_0x39(); return;  // Zone expander zones 25-32 query
    case 0x3E: printPanel_0x3E(); return;  // Panel status and zones 25-32 status
    case 0x4C: printPanel_0x4C(); return;  // Unknown Keybus query
    case 0x58: printPanel_0x58(); return;  // Unknown Keybus query
    case 0x5D: printPanel_0x5D(); return;  // Flash panel lights: status and zones 1-32, partition 1
    case 0x63: printPanel_0x63(); return;  // Flash panel lights: status and zones 1-32, partition 2
    case 0x64: printPanel_0x64(); return;  // Beep, partition 1
    case 0x69: printPanel_0x69(); return;  // Beep, partition 2
    case 0x75: printPanel_0x75(); return;  // Tone, partition 1
    case 0x7A: printPanel_0x7A(); return;  // Tone, partition 2
    case 0x7F: printPanel_0x7F(); return;  // Buzzer, partition 1
    case 0x82: printPanel_0x82(); return;  // Buzzer, partition 2
    case 0x87: printPanel_0x87(); return;  // Panel outputs
    case 0x8D: printPanel_0x8D(); return;  // User code programming key response, codes 17-32
    case 0x94: printPanel_0x94(); return;  // Unknown - immediate after entering *5 programming
    case 0xA5: printPanel_0xA5(); return;  // Date, time, system status messages - partitions 1-2
    case 0xB1: printPanel_0xB1(); return;  // Enabled zones 1-32
    case 0xBB: printPanel_0xBB(); return;  // Bell
    case 0xC3: printPanel_0xC3(); return;  // Keypad status
    case 0xCE: printPanel_0xCE(); return;  // Unknown command
    case 0xD5: printPanel_0xD5(); return;  // Keypad zone query
    case 0xE6: printPanel_0xE6(); return;  // Extended status commands: partitions 3-8, zones 33-64
    case 0xEB: printPanel_0xEB(); return;  // Date, time, system status messages - partitions 1-8
    default: {
      stream->print(F("Unknown data"));
      if (!validCRC()) {
        stream->print(F("[No CRC or CRC Error]"));
        return;
      }
      else stream->print(F("[CRC OK]"));
      return;
    }
  }
}


void dscKeybusInterface::printModuleMessage() {
  switch (moduleData[0]) {
    case 0x77: printModule_0x77(); return;  // Keypad fire alarm
    case 0xBB: printModule_0xBB(); return;  // Keypad auxiliary alarm
    case 0xDD: printModule_0xDD(); return;  // Keypad panic alarm
  }

  // Module responses to panel queries
  switch (currentCmd) {
    case 0x11: printModule_Panel_0x11(); return;  // Module supervision query response
    case 0xD5: printModule_Panel_0xD5(); return;  // Keypad zone query response
  }

  // Keypad and module status update notifications
  if (moduleData[4] != 0xFF || moduleData[5] != 0xFF) {
    printModule_Notification();
    return;
  }

  // Keypad keys
  printModule_Keys();
}


/*
 *  Print panel messages
 */


 // Keypad lights for commands 0x05, 0x0A, 0x1B, 0x27, 0x2D, 0x34, 0x3E, 0x5D
 void dscKeybusInterface::printPanelLights(byte panelByte) {
  if (panelData[panelByte] == 0) stream->print(F("none "));
  else {
    if (bitRead(panelData[panelByte],0)) stream->print(F("Ready "));
    if (bitRead(panelData[panelByte],1)) stream->print(F("Armed "));
    if (bitRead(panelData[panelByte],2)) stream->print(F("Memory "));
    if (bitRead(panelData[panelByte],3)) stream->print(F("Bypass "));
    if (bitRead(panelData[panelByte],4)) stream->print(F("Trouble "));
    if (bitRead(panelData[panelByte],5)) stream->print(F("Program "));
    if (bitRead(panelData[panelByte],6)) stream->print(F("Fire "));
    if (bitRead(panelData[panelByte],7)) stream->print(F("Backlight "));
  }
 }


// Messages for commands 0x05, 0x0A, 0x1B, 0x27, 0x2D, 0x34, 0x3E
void dscKeybusInterface::printPanelMessages(byte panelByte) {
  switch (panelData[panelByte]) {
    case 0x01: stream->print(F("Partition ready")); break;
    case 0x02: stream->print(F("Stay zones open")); break;
    case 0x03: stream->print(F("Zones open")); break;
    case 0x04: stream->print(F("Armed stay")); break;
    case 0x05: stream->print(F("Armed away")); break;
    case 0x07: stream->print(F("Failed to arm")); break;
    case 0x08: stream->print(F("Exit delay in progress")); break;
    case 0x09: stream->print(F("Arming with no entry delay")); break;
    case 0x0B: stream->print(F("Quick exit in progress")); break;
    case 0x0C: stream->print(F("Entry delay in progress")); break;
    case 0x0D: stream->print(F("Zone open after alarm")); break;
    case 0x10: stream->print(F("Keypad lockout")); break;
    case 0x11: stream->print(F("Partition in alarm")); break;
    case 0x14: stream->print(F("Auto-arm in progress")); break;
    case 0x15: stream->print(F("Arming with bypassed zones")); break;
    case 0x16: stream->print(F("Armed with no entry delay")); break;
    case 0x19: stream->print(F("Disarmed after alarm in memory")); break;
    case 0x22: stream->print(F("Partition busy")); break;
    case 0x33: stream->print(F("Command output in progress")); break;
    case 0x3D: stream->print(F("Disarmed after alarm in memory")); break;
    case 0x3E: stream->print(F("Partition disarmed")); break;
    case 0x40: stream->print(F("Keypad blanking")); break;
    case 0x8A: stream->print(F("Activate stay/away zones")); break;
    case 0x8B: stream->print(F("Quick exit")); break;
    case 0x8E: stream->print(F("Function not available")); break;
    case 0x8F: stream->print(F("Invalid access code")); break;
    case 0x9E: stream->print(F("Enter * function code")); break;
    case 0x9F: stream->print(F("Enter access code")); break;
    case 0xA0: stream->print(F("Zone bypass")); break;  // *1
    case 0xA1: stream->print(F("Trouble menu")); break;
    case 0xA2: stream->print(F("Alarm memory")); break;
    case 0xA3: stream->print(F("Door chime enabled")); break;
    case 0xA4: stream->print(F("Door chime disabled")); break;
    case 0xA5: stream->print(F("Enter master code")); break;
    case 0xA6: stream->print(F("Access codes")); break;
    case 0xA7: stream->print(F("Enter new code")); break;
    case 0xA9: stream->print(F("User functions")); break;
    case 0xAA: stream->print(F("Time and date")); break;
    case 0xAB: stream->print(F("Auto-arm time")); break;
    case 0xAC: stream->print(F("Auto-arm enabled")); break;
    case 0xAD: stream->print(F("Auto-arm disabled")); break;
    case 0xAF: stream->print(F("System test")); break;
    case 0xB0: stream->print(F("Enable DLS")); break;
    case 0xB2: stream->print(F("Command output")); break;
    case 0xB7: stream->print(F("Enter installer code")); break;
    case 0xB8: stream->print(F("Key * while armed")); break;
    case 0xB9: stream->print(F("Zone tamper menu")); break;
    case 0xBA: stream->print(F("Zones with low batteries")); break;
    case 0xC6: stream->print(F("Zone fault menu")); break;
    case 0xC8: stream->print(F("Service required menu")); break;
    case 0xD0: stream->print(F("Handheld keypads with low batteries")); break;
    case 0xD1: stream->print(F("Wireless keys with low batteries")); break;
    case 0xE4: stream->print(F("Installer programming")); break;
    case 0xE5: stream->print(F("Keypad slot assignment")); break;
    case 0xE6: stream->print(F("Input 2 digits")); break;
    case 0xE7: stream->print(F("Input 3 digits")); break;
    case 0xE8: stream->print(F("Input 4 digits")); break;
    case 0xEA: stream->print(F("Reporting code: 2 digits")); break;
    case 0xEB: stream->print(F("Telephone number account code: 4 digits")); break;
    case 0xEC: stream->print(F("Input 6 digits")); break;
    case 0xED: stream->print(F("Input 32 digits")); break;
    case 0xEE: stream->print(F("Input 1 option per zone")); break;
    case 0xEF: stream->print(F("Module supervision field")); break;
    case 0xF0: stream->print(F("Function key 1")); break;
    case 0xF1: stream->print(F("Function key 2")); break;
    case 0xF2: stream->print(F("Function key 3")); break;
    case 0xF3: stream->print(F("Function key 4")); break;
    case 0xF4: stream->print(F("Function key 5")); break;
    case 0xF5: stream->print(F("Wireless module placement test")); break;
    case 0xF7: stream->print(F("Installer programming subsection")); break;
    case 0xF8: stream->print(F("Keypad programming")); break;
    default:
      stream->print(F("Unknown data"));
      stream->print(F(": 0x"));
      if (panelData[panelByte] < 10) stream->print(F("0"));
      stream->print(panelData[panelByte], HEX);
      break;
  }
}


// Status messages for commands 0xA5, 0xEB
void dscKeybusInterface::printPanelStatus0(byte panelByte) {
  bool decoded = true;
  switch (panelData[panelByte]) {
    /*
     *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
     *  10100101 0 00011000 01001111 10110000 11101100 01001001 11111111 11110000 [0xA5] 03/29/2018 16:59 | Duress alarm
     *  10100101 0 00011000 01001111 11001110 10111100 01001010 11111111 11011111 [0xA5] 03/30/2018 14:47 | Disarmed after alarm in memory
     *  10100101 0 00011000 01001111 11001010 01000100 01001011 11111111 01100100 [0xA5] 03/30/2018 10:17 | Partition in alarm
     *  10100101 0 00011000 01010000 01001001 10111000 01001100 11111111 01011001 [0xA5] 04/02/2018 09:46 | Zone expander supervisory alarm
     *  10100101 0 00011000 01010000 01001010 00000000 01001101 11111111 10100011 [0xA5] 04/02/2018 10:00 | Zone expander supervisory restored
     *  10100101 0 00011000 01001111 01110010 10011100 01001110 11111111 01100111 [0xA5] 03/27/2018 18:39 | Keypad Fire alarm
     *  10100101 0 00011000 01001111 01110010 10010000 01001111 11111111 01011100 [0xA5] 03/27/2018 18:36 | Keypad Aux alarm
     *  10100101 0 00011000 01001111 01110010 10001000 01010000 11111111 01010101 [0xA5] 03/27/2018 18:34 | Keypad Panic alarm  // PC1555MX
     *  10100101 0 00010110 00010111 11101101 00010000 01010000 10010001 10110000 [0xA5] 2016.05.31 13:04 | Keypad Panic alarm  // PC1864
     *  10100101 0 00010001 01101101 01100000 00000100 01010001 11111111 11010111 [0xA5] 11/11/2011 00:01 | Keypad status check?   // Power-on +124s, keypad sends status update immediately after this
     *  10100101 0 00011000 01001111 01110010 10011100 01010010 11111111 01101011 [0xA5] 03/27/2018 18:39 | Keypad Fire alarm restored
     *  10100101 0 00011000 01001111 01110010 10010000 01010011 11111111 01100000 [0xA5] 03/27/2018 18:36 | Keypad Aux alarm restored
     *  10100101 0 00011000 01001111 01110010 10001000 01010100 11111111 01011001 [0xA5] 03/27/2018 18:34 | Keypad Panic alarm restored
     *  10100101 0 00011000 01001111 11110110 00110100 10011000 11111111 11001101 [0xA5] 03/31/2018 22:13 | Keypad lockout
     *  10100101 0 00011000 01001111 11101011 10100100 10111110 11111111 01011000 [0xA5] 03/31/2018 11:41 | Armed partial: Zones bypassed
     *  10100101 0 00011000 01001111 11101011 00011000 10111111 11111111 11001101 [0xA5] 03/31/2018 11:06 | Armed special: quick-arm/auto-arm/keyswitch/wireless key/DLS
     *  10100101 0 00010001 01101101 01100000 00101000 11100101 11111111 10001111 [0xA5] 11/11/2011 00:10 | Auto-arm cancelled
     *  10100101 0 00011000 01001111 11110111 01000000 11100110 11111111 00101000 [0xA5] 03/31/2018 23:16 | Disarmed special: keyswitch/wireless key/DLS
     *  10100101 0 00011000 01001111 01101111 01011100 11100111 11111111 10111101 [0xA5] 03/27/2018 15:23 | Panel battery trouble
     *  10100101 0 00011000 01001111 10110011 10011000 11101000 11111111 00111110 [0xA5] 03/29/2018 19:38 | AC power failure  // Sent after delay in *8 [370]
     *  10100101 0 00011000 01001111 01110100 01010000 11101001 11111111 10111000 [0xA5] 03/27/2018 20:20 | Bell trouble
     *  10100101 0 00011000 01001111 11000000 10001000 11101100 11111111 00111111 [0xA5] 03/30/2018 00:34 | Telephone line trouble
     *  10100101 0 00011000 01001111 01101111 01110000 11101111 11111111 11011001 [0xA5] 03/27/2018 15:28 | Panel battery restored
     *  10100101 0 00011000 01010000 00100000 01011000 11110000 11111111 01110100 [0xA5] 04/01/2018 00:22 | AC power restored  // Sent after delay in *8 [370]
     *  10100101 0 00011000 01001111 01110100 01011000 11110001 11111111 11001000 [0xA5] 03/27/2018 20:22 | Bell restored
     *  10100101 0 00011000 01001111 11000000 10001000 11110100 11111111 01000111 [0xA5] 03/30/2018 00:34 | Telephone line restored
     *  10100101 0 00011000 01001111 11100001 01011000 11111111 11111111 01000011 [0xA5] 03/31/2018 01:22 | System test
     */
    // 0x09 - 0x28: Zone alarm, zones 1-32
    // 0x29 - 0x48: Zone alarm restored, zones 1-32
    case 0x49: stream->print(F("Duress alarm")); break;
    case 0x4A: stream->print(F("Disarmed after alarm in memory")); break;
    case 0x4B: stream->print(F("Partition in alarm")); break;
    case 0x4C: stream->print(F("Zone expander supervisory alarm")); break;
    case 0x4D: stream->print(F("Zone expander supervisory restored")); break;
    case 0x4E: stream->print(F("Keypad Fire alarm")); break;
    case 0x4F: stream->print(F("Keypad Aux alarm")); break;
    case 0x50: stream->print(F("Keypad Panic alarm")); break;
    case 0x51: stream->print(F("Auxiliary input alarm")); break;
    case 0x52: stream->print(F("Keypad Fire alarm restored")); break;
    case 0x53: stream->print(F("Keypad Aux alarm restored")); break;
    case 0x54: stream->print(F("Keypad Panic alarm restored")); break;
    case 0x55: stream->print(F("Auxilary input alarm restored")); break;
    // 0x56 - 0x75: Zone tamper, zones 1-32
    // 0x76 - 0x95: Zone tamper restored, zones 1-32
    case 0x98: stream->print(F("Keypad lockout")); break;
    // 0x99 - 0xBD: Armed by access code
    case 0xBE: stream->print(F("Armed partial: Zones bypassed")); break;
    case 0xBF: stream->print(F("Armed special: quick-arm/auto-arm/keyswitch/wireless key/DLS")); break;
    // 0xC0 - 0xE4: Disarmed by access code
    case 0xE5: stream->print(F("Auto-arm cancelled")); break;
    case 0xE6: stream->print(F("Disarmed special: keyswitch/wireless key/DLS")); break;
    case 0xE7: stream->print(F("Panel battery trouble")); break;
    case 0xE8: stream->print(F("Panel AC power failure")); break;
    case 0xE9: stream->print(F("Bell trouble")); break;
    case 0xEA: stream->print(F("Power on +16s")); break;
    case 0xEC: stream->print(F("Telephone line trouble")); break;
    case 0xEF: stream->print(F("Panel battery restored")); break;
    case 0xF0: stream->print(F("Panel AC power restored")); break;
    case 0xF1: stream->print(F("Bell restored")); break;
    case 0xF4: stream->print(F("Telephone line restored")); break;
    case 0xFF: stream->print(F("System test")); break;
    default: decoded = false;
  }
  if (decoded) return;

  /*
   *  Zone alarm, zones 1-32
   *
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00011000 01001111 01001001 11011000 00001001 11111111 00110101 [0xA5] 03/26/2018 09:54 | Zone alarm: 1
   *  10100101 0 00011000 01001111 01001010 00100000 00001110 11111111 10000011 [0xA5] 03/26/2018 10:08 | Zone alarm: 6
   *  10100101 0 00011000 01001111 10010100 11001000 00010000 11111111 01110111 [0xA5] 03/28/2018 20:50 | Zone alarm: 8
   */
  if (panelData[panelByte] >= 0x09 && panelData[panelByte] <= 0x28) {
    stream->print(F("Zone alarm: "));
    stream->print(panelData[panelByte] - 0x08);
    return;
  }

  /*
   *  Zone alarm restored, zones 1-32
   *
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00011000 01001111 10010100 11001100 00101001 11111111 10010100 [0xA5] 03/28/2018 20:51 | Zone alarm restored: 1
   *  10100101 0 00011000 01001111 10010100 11010100 00101110 11111111 10100001 [0xA5] 03/28/2018 20:53 | Zone alarm restored: 6
   *  10100101 0 00011000 01001111 10010100 11010000 00110000 11111111 10011111 [0xA5] 03/28/2018 20:52 | Zone alarm restored: 8
   */
  if (panelData[panelByte] >= 0x29 && panelData[panelByte] <= 0x48) {
    stream->print(F("Zone alarm restored: "));
    stream->print(panelData[panelByte] - 0x28);
    return;
  }

  /*
   *  Zone tamper, zones 1-32
   *
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00000001 01000100 00100010 01011100 01010110 11111111 10111101 [0xA5] 01/01/2001 02:23 | Zone tamper: 1
   *  10100101 0 00000001 01000100 00100010 01011100 01010111 11111111 10111101 [0xA5] 01/01/2001 02:23 | Zone tamper: 2
   *  10100101 0 00010001 01101101 01101011 10010000 01011011 11111111 01111000 [0xA5] 11/11/2011 11:36 | Zone tamper: 6
   */
  if (panelData[panelByte] >= 0x56 && panelData[panelByte] <= 0x75) {
    stream->print(F("Zone tamper: "));
    stream->print(panelData[panelByte] - 0x55);
    return;
  }

  /*
   *  Zone tamper restored, zones 1-32
   *
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00000001 01000100 00100010 01011100 01110110 11111111 11011101 [0xA5] 01/01/2001 02:23 | Zone tamper restored: 1
   *  10100101 0 00000001 01000100 00100010 01011100 01111000 11111111 11011101 [0xA5] 01/01/2001 02:23 | Zone tamper restored: 2
   *  10100101 0 00010001 01101101 01101011 10010000 01111011 11111111 10011000 [0xA5] 11/11/2011 11:36 | Zone tamper restored: 6
   */
  if (panelData[panelByte] >= 0x76 && panelData[panelByte] <= 0x95) {
    stream->print(F("Zone tamper restored: "));
    stream->print(panelData[panelByte] - 0x75);
    return;
  }

  /*
   *  Armed by access code
   *
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00011000 01001101 00001000 10010000 10011001 11111111 00111010 [0xA5] 03/08/2018 08:36 | Armed by user code 1
   *  10100101 0 00011000 01001101 00001000 10111100 10111011 11111111 10001000 [0xA5] 03/08/2018 08:47 | Armed by master code 40
   */
  if (panelData[panelByte] >= 0x99 && panelData[panelByte] <= 0xBD) {
    byte dscCode = panelData[panelByte] - 0x98;
    if (dscCode >= 35) dscCode += 5;
    stream->print(F("Armed by "));
    switch (dscCode) {
      case 33: stream->print(F("duress ")); break;
      case 34: stream->print(F("duress ")); break;
      case 40: stream->print(F("master ")); break;
      case 41: stream->print(F("supervisor ")); break;
      case 42: stream->print(F("supervisor ")); break;
      default: stream->print(F("user ")); break;
    }
    stream->print(F("code "));
    stream->print(dscCode);
    return;
  }

  /*
   *  Disarmed by access code
   *
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00011000 01001101 00001000 11101100 11000000 11111111 10111101 [0xA5] 03/08/2018 08:59 | Disarmed by user code 1
   *  10100101 0 00011000 01001101 00001000 10110100 11100010 11111111 10100111 [0xA5] 03/08/2018 08:45 | Disarmed by master code 40
   */
  if (panelData[panelByte] >= 0xC0 && panelData[panelByte] <= 0xE4) {
    byte dscCode = panelData[panelByte] - 0xBF;
    if (dscCode >= 35) dscCode += 5;
    stream->print(F("Disarmed by "));
    switch (dscCode) {
      case 33: stream->print(F("duress ")); break;
      case 34: stream->print(F("duress ")); break;
      case 40: stream->print(F("master ")); break;
      case 41: stream->print(F("supervisor ")); break;
      case 42: stream->print(F("supervisor ")); break;
      default: stream->print(F("user ")); break;
    }
    stream->print(F("code "));
    stream->print(dscCode);
    return;
  }

  stream->print(F("Unknown data"));
}


// Status messages for commands 0xA5, 0xEB
void dscKeybusInterface::printPanelStatus1(byte panelByte) {
  switch (panelData[panelByte]) {
    /*
     *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
     *  10100101 0 00011000 01001111 11001010 10001001 00000011 11111111 01100001 [0xA5] 03/30/2018 10:34 | Cross zone alarm
     *  10100101 0 00010001 01101101 01101010 00000001 00000100 11111111 10010001 [0xA5] 11/11/2011 10:00 | Delinquency alarm
     *  10100101 0 00010001 01101101 01100000 10101001 00100100 00000000 01010000 [0xA5] 11/11/2011 00:42 | Auto-arm cancelled by duress code 33
     *  10100101 0 00010001 01101101 01100000 10110101 00100101 00000000 01011101 [0xA5] 11/11/2011 00:45 | Auto-arm cancelled by duress code 34
     *  10100101 0 00010001 01101101 01100000 00101001 00100110 00000000 11010010 [0xA5] 11/11/2011 00:10 | Auto-arm cancelled by master code 40
     *  10100101 0 00010001 01101101 01100000 10010001 00100111 00000000 00111011 [0xA5] 11/11/2011 00:36 | Auto-arm cancelled by supervisor code 41
     *  10100101 0 00010001 01101101 01100000 10111001 00101000 00000000 01100100 [0xA5] 11/11/2011 00:46 | Auto-arm cancelled by supervisor code 42
     *  10100101 0 00011000 01001111 10100000 10011101 00101011 00000000 01110100 [0xA5] 03/29/2018 00:39 | Armed by auto-arm
     *  10100101 0 00011000 01001101 00001010 00001101 10101100 00000000 11001101 [0xA5] 03/08/2018 10:03 | Exit *8 programming
     *  10100101 0 00011000 01001101 00001001 11100001 10101101 00000000 10100001 [0xA5] 03/08/2018 09:56 | Enter *8
     *  10100101 0 00010001 01101101 01100010 11001101 11010000 00000000 00100010 [0xA5] 11/11/2011 02:51 | Command output 4
     *  10100101 0 00010110 01010110 00101011 11010001 11010010 00000000 11011111 [0xA5] 2016.05.17 11:52 | Armed with no entry delay cancelled
     */
    case 0x03: stream->print(F("Cross zone alarm")); return;
    case 0x04: stream->print(F("Delinquency alarm")); return;
    case 0x24: stream->print(F("Auto-arm cancelled by duress code 33")); return;
    case 0x25: stream->print(F("Auto-arm cancelled by duress code 34")); return;
    case 0x26: stream->print(F("Auto-arm cancelled by master code 40")); return;
    case 0x27: stream->print(F("Auto-arm cancelled by supervisor code 41")); return;
    case 0x28: stream->print(F("Auto-arm cancelled by supervisor code 42")); return;
    case 0x2B: stream->print(F("Armed by auto-arm")); return;
    // 0x6C - 0x8B: Zone fault restored, zones 1-32
    // 0x8C - 0xAB: Zone fault, zones 1-32
    case 0xAC: stream->print(F("Exit installer programming")); return;
    case 0xAD: stream->print(F("Enter installer programming")); return;
    // 0xB0 - 0xCF: Zones bypassed, zones 1-32
    case 0xD0: stream->print(F("Command output 4")); return;
    case 0xD2: stream->print(F("Armed with no entry delay cancelled")); return;
  }

  /*
   *  Zone fault restored, zones 1-32
   *
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00010001 01101101 01101011 01000001 01101100 11111111 00111010 [0xA5] 11/11/2011 11:16 | Zone fault restored: 1
   *  10100101 0 00010001 01101101 01101011 01010101 01101101 11111111 01001111 [0xA5] 11/11/2011 11:21 | Zone fault restored: 2
   *  10100101 0 00010001 01101101 01101011 10000101 01101111 11111111 10000001 [0xA5] 11/11/2011 11:33 | Zone fault restored: 4
   *  10100101 0 00010001 01101101 01101011 10001001 01110000 11111111 10000110 [0xA5] 11/11/2011 11:34 | Zone fault restored: 5
   */
  if (panelData[panelByte] >= 0x6C && panelData[panelByte] <= 0x8B) {
    stream->print(F("Zone fault restored: "));
    stream->print(panelData[panelByte] - 0x6B);
    return;
  }

  /*
   *  Zone fault, zones 1-32
   *
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00010001 01101101 01101011 00111101 10001100 11111111 01010110 [0xA5] 11/11/2011 11:15 | Zone fault: 1
   *  10100101 0 00010001 01101101 01101011 01010101 10001101 11111111 01101111 [0xA5] 11/11/2011 11:21 | Zone fault: 2
   *  10100101 0 00010001 01101101 01101011 10000001 10001111 11111111 10011101 [0xA5] 11/11/2011 11:32 | Zone fault: 3
   *  10100101 0 00010001 01101101 01101011 10001001 10010000 11111111 10100110 [0xA5] 11/11/2011 11:34 | Zone fault: 4
   */
  if (panelData[panelByte] >= 0x8C && panelData[panelByte] <= 0xAB) {
    stream->print(F("Zone fault: "));
    stream->print(panelData[panelByte] - 0x8B);
    return;
  }

  /*
   *  Zones bypassed, zones 1-32
   *
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00011000 01001111 10110001 10101001 10110001 00000000 00010111 [0xA5] 03/29/2018 17:42 | Zone bypassed: 2
   *  10100101 0 00011000 01001111 10110001 11000001 10110101 00000000 00110011 [0xA5] 03/29/2018 17:48 | Zone bypassed: 6
   */
  if (panelData[panelByte] >= 0xB0 && panelData[panelByte] <= 0xCF) {
    stream->print(F("Zone bypassed: "));
    stream->print(panelData[panelByte] - 0xAF);
    return;
  }

  stream->print(F("Unknown data"));
}


// Status messages for commands 0xA5, 0xEB
void dscKeybusInterface::printPanelStatus2(byte panelByte) {
  switch (panelData[panelByte]) {

    /*
     *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
     *  10100101 0 00011000 01001111 10101111 10000110 00101010 00000000 01101011 [0xA5] 03/29/2018 15:33 | Quick exit
     *  10100101 0 00010001 01101101 01110101 00111010 01100011 00000000 00110101 [0xA5] 11/11/2011 21:14 | Keybus fault restored
     *  10100101 0 00011000 01001111 11110111 01110110 01100110 00000000 11011111 [0xA5] 03/31/2018 23:29 | Enter *1 zone bypass programming
     *  10100101 0 00010001 01101101 01100010 11001110 01101001 00000000 10111100 [0xA5] 11/11/2011 02:51 | Command output 3
     *  10100101 0 00011000 01010000 01000000 00000010 10001100 00000000 11011011 [0xA5] 04/02/2018 00:00 | Loss of system time
     *  10100101 0 00011000 01001111 10101110 00001110 10001101 00000000 01010101 [0xA5] 03/29/2018 14:03 | Power on
     *  10100101 0 00011000 01010000 01000000 00000010 10001110 00000000 11011101 [0xA5] 04/02/2018 00:00 | Panel factory default
     *  10100101 0 00011000 01001111 11101010 10111010 10010011 00000000 01000011 [0xA5] 03/31/2018 10:46 | Disarmed by keyswitch
     *  10100101 0 00011000 01001111 11101010 10101110 10010110 00000000 00111010 [0xA5] 03/31/2018 10:43 | Armed by keyswitch
     *  10100101 0 00011000 01001111 10100000 01100010 10011000 00000000 10100110 [0xA5] 03/29/2018 00:24 | Armed by quick-arm
     *  10100101 0 00010001 01101101 01100000 00101110 10011001 00000000 01001010 [0xA5] 11/11/2011 00:11 | Activate stay/away zones
     *  10100101 0 00011000 01001111 00101101 00011010 10011010 00000000 11101101 [0xA5] 03/25/2018 13:06 | Armed: stay
     *  10100101 0 00011000 01001111 00101101 00010010 10011011 00000000 11100110 [0xA5] 03/25/2018 13:04 | Armed: away
     *  10100101 0 00011000 01001111 00101101 10011010 10011100 00000000 01101111 [0xA5] 03/25/2018 13:38 | Armed with no entry delay
     *  10100101 0 00011000 01001111 00101100 11011110 11000011 00000000 11011001 [0xA5] 03/25/2018 12:55 | Enter *5 programming
     *  10100101 0 00011000 01001111 00101110 00000010 11100110 00000000 00100010 [0xA5] 03/25/2018 14:00 | Enter *6 programming
     */
    case 0x2A: stream->print(F("Quick exit")); return;
    case 0x63: stream->print(F("Keybus fault restored")); return;
    case 0x66: stream->print(F("Enter *1 zone bypass programming")); return;
    case 0x67: stream->print(F("Command output 1")); return;
    case 0x68: stream->print(F("Command output 2")); return;
    case 0x69: stream->print(F("Command output 3")); return;
    case 0x8C: stream->print(F("System time loss")); return;
    case 0x8D: stream->print(F("Power on")); return;
    case 0x8E: stream->print(F("Panel factory default")); return;
    case 0x93: stream->print(F("Disarmed by keyswitch")); return;
    case 0x96: stream->print(F("Armed by keyswitch")); return;
    case 0x97: stream->print(F("Armed by keypad away")); return;
    case 0x98: stream->print(F("Armed by quick-arm")); return;
    case 0x99: stream->print(F("Activate stay/away zones")); return;
    case 0x9A: stream->print(F("Armed: stay")); return;
    case 0x9B: stream->print(F("Armed: away")); return;
    case 0x9C: stream->print(F("Armed with no entry delay")); return;
    case 0xC3: stream->print(F("Enter *5 programming")); return;
    // 0xC6 - 0xE5: Auto-arm cancelled by user code
    case 0xE6: stream->print(F("Enter *6 programming")); return;
    // 0xE9 - 0xF0: Supervisory restored, keypad slots 1-8
    // 0xF1 - 0xF8: Supervisory trouble, keypad slots 1-8
  }

  /*
   *  Auto-arm cancelled by user code
   *
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00010001 01101101 01100000 00111110 11000110 00000000 10000111 [0xA5] 11/11/2011 00:15 | Auto-arm cancelled by user code 1
   *  10100101 0 00010001 01101101 01100000 01111010 11100101 00000000 11100010 [0xA5] 11/11/2011 00:30 | Auto-arm cancelled by user code 32
   */
  if (panelData[panelByte] >= 0xC6 && panelData[panelByte] <= 0xE5) {
    stream->print(F("Auto-arm cancelled by user code "));
    stream->print(panelData[panelByte] - 0xC5);
    return;
  }

  /*
   *  Supervisory restored, keypad slots 1-8
   *
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00010001 01101101 01110100 10001110 11101001 11111111 00001101 [0xA5] 11/11/2011 20:35 | Supervisory - module detected: Keypad slot 1
   *  10100101 0 00010001 01101101 01110100 00110010 11110000 11111111 10111000 [0xA5] 11/11/2011 20:12 | Supervisory - module detected: Keypad slot 8
   */
  if (panelData[panelByte] >= 0xE9 && panelData[panelByte] <= 0xF0) {
    stream->print(F("Module detected: Keypad slot "));
    stream->print(panelData[panelByte] - 0xE8);
    return;
  }

  /*
   *  Supervisory trouble, keypad slots 1-8
   *
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00010001 01101101 01110100 10000110 11110001 11111111 00001101 [0xA5] 11/11/2011 20:33 | Supervisory - module trouble: Keypad slot 1
   *  10100101 0 00010001 01101101 01110100 00101110 11111000 11111111 10111100 [0xA5] 11/11/2011 20:11 | Supervisory - module trouble: Keypad slot 8
   */
  if (panelData[panelByte] >= 0xF1 && panelData[panelByte] <= 0xF8) {
    stream->print(F("Module trouble: Keypad slot "));
    stream->print(panelData[panelByte] - 0xF0);
    return;
  }

  stream->print(F("Unknown data"));
}


// Status messages for commands 0xA5, 0xEB
void dscKeybusInterface::printPanelStatus3(byte panelByte) {
  stream->print(F("Unknown data"));
  stream->print(F(" :"));
  stream->print(panelByte, HEX);
}


// Status messages for command 0xEB
void dscKeybusInterface::printPanelStatus4(byte panelByte) {
  if (panelData[panelByte] <= 0x1F) {
    stream->print(F("Zone alarm: "));
    stream->print(panelData[panelByte] + 33);
    return;
  }

  if (panelData[panelByte] >= 0x20 && panelData[panelByte] <= 0x3F) {
    stream->print(F("Zone alarm restored: "));
    stream->print(panelData[panelByte] + 1);
    return;
  }

  if (panelData[panelByte] >= 0x40 && panelData[panelByte] <= 0x5F) {
    stream->print(F("Zone tamper: "));
    stream->print(panelData[panelByte] - 31);
    return;
  }

  if (panelData[panelByte] >= 0x60 && panelData[panelByte] <= 0x7F) {
    stream->print(F("Zone tamper restored: "));
    stream->print(panelData[panelByte] - 63);
    return;
  }


  stream->print(F("Unknown data"));
}


// Status messages for command 0xEB
void dscKeybusInterface::printPanelStatus14(byte panelByte) {
  if (panelData[panelByte] >= 0x40 && panelData[panelByte] <= 0x5F) {
    stream->print(F("Zone fault restored: "));
    stream->print(panelData[panelByte] - 31);
    return;
  }

  if (panelData[panelByte] >= 0x60 && panelData[panelByte] <= 0x7F) {
    stream->print(F("Zone fault: "));
    stream->print(panelData[panelByte] - 63);
    return;
  }


  stream->print(F("Unknown data"));
}


// Prints individual bits as a number for partitions and zones
void dscKeybusInterface::printPanelBitNumbers(byte panelByte, byte startNumber) {
  for (byte bit = 0; bit < 8; bit++) {
    if (bitRead(panelData[panelByte],bit)) {
      stream->print(startNumber + bit);
      stream->print(F(" "));
    }
  }
}


/*
 *  0x05: Status - partitions 1-4
 *  Interval: constant
 *  CRC: no
 *  Byte 2: Partition 1 lights
 *  Byte 3: Partition 1 status
 *  Byte 4: Partition 2 lights
 *  Byte 5: Partition 2 status
 *
 *  PC5020/PC1616/PC1832/PC1864:
 *  Byte 6: Partition 3 lights
 *  Byte 7: Partition 3 status
 *  Byte 8: Partition 4 lights
 *  Byte 9: Partition 4 status
 *
 *  // PC1555MX, PC5015
 *  00000101 0 10000001 00000001 10010001 11000111 [0x05] Partition 1 | Lights: Ready Backlight | Partition ready | Partition 2: disabled
 *  00000101 0 10010000 00000011 10010001 11000111 [0x05] Status lights: Trouble Backlight | Partition not ready
 *  00000101 0 10001010 00000100 10010001 11000111 [0x05] Status lights: Armed Bypass Backlight | Armed stay
 *  00000101 0 10000010 00000101 10010001 11000111 [0x05] Status lights: Armed Backlight | Armed away
 *  00000101 0 10001011 00001000 10010001 11000111 [0x05] Status lights: Ready Armed Bypass Backlight | Exit delay in progress
 *  00000101 0 10000010 00001100 10010001 11000111 [0x05] Status lights: Armed Backlight | Entry delay in progress
 *  00000101 0 10000001 00010000 10010001 11000111 [0x05] Status lights: Ready Backlight | Keypad lockout
 *  00000101 0 10000010 00010001 10010001 11000111 [0x05] Status lights: Armed Backlight | Partition in alarm
 *  00000101 0 10000001 00110011 10010001 11000111 [0x05] Status lights: Ready Backlight | Partition busy
 *  00000101 0 10000001 00111110 10010001 11000111 [0x05] Status lights: Ready Backlight | Partition disarmed
 *  00000101 0 10000001 01000000 10010001 11000111 [0x05] Status lights: Ready Backlight | Keypad blanking
 *  00000101 0 10000001 10001111 10010001 11000111 [0x05] Status lights: Ready Backlight | Invalid access code
 *  00000101 0 10000000 10011110 10010001 11000111 [0x05] Status lights: Backlight | Quick armed pressed
 *  00000101 0 10000001 10100011 10010001 11000111 [0x05] Status lights: Ready Backlight | Door chime enabled
 *  00000101 0 10000001 10100100 10010001 11000111 [0x05] Status lights: Ready Backlight | Door chime disabled

 *  00000101 0 10000010 00001101 10010001 11000111 [0x05] Status lights: Armed Backlight   *  Delay zone tripped after previous alarm tripped
 *  00000101 0 10000000 00111101 10010001 11000111 [0x05] Status lights: Backlight   *  Disarmed after previous alarm tripped
 *  00000101 0 10000000 00100010 10010001 11000111 [0x05] Status lights: Backlight   *  Disarmed after previous alarm tripped +4s
 *  00000101 0 10000010 10100110 10010001 11000111 [0x05] Status lights: Armed Backlight   *  In *5 programming
 *
 *  // PC5020, PC1616, PC1832, PC1864
 *  00000101 0 10000000 00000011 10000010 00000101 10000010 00000101 00000000 11000111 [0x05] Status lights: Backlight | Zones open
 */
void dscKeybusInterface::printPanel_0x05() {
  if (panelData[3] == 0xC7) {
    stream->print(F(" | Partition 1: disabled"));
  }
  else {
    stream->print(F("Partition 1: "));
    printPanelLights(2);
    stream->print(F("- "));
    printPanelMessages(3);
  }

  if (panelData[5] == 0xC7) {
    stream->print(F(" | Partition 2: disabled"));
  }
  else {
    stream->print(F(" | Partition 2: "));
    printPanelLights(4);
    stream->print(F("- "));
    printPanelMessages(5);
  }

  if (panelByteCount > 9) {
    if (panelData[7] == 0xC7) {
      stream->print(F(" | Partition 3: disabled"));
    }
    else {
      stream->print(F(" | Partition 3: "));
      printPanelLights(6);
      stream->print(F("- "));
      printPanelMessages(7);
    }

    if (panelData[9] == 0xC7) {
      stream->print(F(" | Partition 4: disabled"));
    }
    else {
      stream->print(F(" | Partition 4: "));
      printPanelLights(8);
      stream->print(F("- "));
      printPanelMessages(9);
    }
  }
}


/*
 *  0x0A: Status in alarm, programming
 *  Interval: constant in *8 programming
 *  CRC: yes
 *  Byte 2: Partition 1 lights
 *  Byte 3: Partition 1 status
 *  Byte 4-7: Zone lights
 *  Byte 8: Zone lights for *5 access codes 33,34,41,42
 *
 *  00001010 0 10000010 11100100 00000000 00000000 00000000 00000000 00000000 01110000 [0x0A] Status lights: Armed | Zone lights: none
 *  00001010 0 10000001 11101110 01100101 00000000 00000000 00000000 00000000 11011110 [0x0A] Status lights: Ready | Zone lights: 1 3 6 7
 */
void dscKeybusInterface::printPanel_0x0A() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  printPanelLights(2);
  stream->print(F("- "));
  printPanelMessages(3);

  bool zoneLights = false;
  stream->print(F(" | Zone lights: "));
  for (byte panelByte = 4; panelByte < 8; panelByte++) {
    if (panelData[panelByte] != 0) {
      zoneLights = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + 1) + ((panelByte-4) *  8));
          stream->print(" ");
        }
      }
    }
  }

  if (panelData[8] != 0 && panelData[8] != 128) {
    zoneLights = true;
    if (bitRead(panelData[8],0)) stream->print(F("33 "));
    if (bitRead(panelData[8],1)) stream->print(F("34 "));
    if (bitRead(panelData[8],3)) stream->print(F("41 "));
    if (bitRead(panelData[8],4)) stream->print(F("42 "));
  }

  if (!zoneLights) stream->print(F("none"));
}


/*
 *  0x11: Module supervision query
 *  Interval: 30s
 *  CRC: no
 *
 *  00010001 0 10101010 10101010 10101010 10101010 10101010 [0x11] Module supervision query  // PC1555MX
 *  11111111 1 00111111 11111111 11111111 11111111 11111111 [Keypad] Slot 1                  // PC1555MX
 *  11111111 1 11111111 11111100 11111111 11111111 11111111 [Keypad] Slot 8                  // PC1555MX
 *  00010001 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0x11] Module supervision query  // PC5010
 *  11111111 1 00111111 11111111 11111111 11001111 11111111 11111111 11111111 [Keypad] Slots active: 1         // PC5010
 */
void dscKeybusInterface::printPanel_0x11() {
  stream->print(F("Module supervision query"));
}


/*
 *  0x16: Zone wiring
 *  Interval: 4min
 *  CRC: yes
 *  Byte 2: TBD, identical with PC1555MX, PC5015, PC1832
 *  Byte 3: TBD, different between PC1555MX, PC5015, PC1832
 *  Byte 4 bits 2-7: TBD, identical with PC1555MX and PC5015
 *
 *  00010110 0 00001110 00100011 11010001 00011001 [0x16] PC1555MX | Zone wiring: NC | Exit *8 programming
 *  00010110 0 00001110 00100011 11010010 00011001 [0x16] PC1555MX | Zone wiring: EOL | Exit *8 programming
 *  00010110 0 00001110 00100011 11010011 00011001 [0x16] PC1555MX | Zone wiring: DEOL | Exit *8 programming
 *  00010110 0 00001110 00100011 11100001 00101000 [0x16] PC1555MX | Zone wiring: NC | In *8
 *  00010110 0 00001110 00100011 11100110 00101101 [0x16] PC1555MX | Zone wiring: EOL | Enter *8 programming
 *  00010110 0 00001110 00100011 11110010 00111001 [0x16] PC1555MX | Zone wiring: EOL | Armed, Exit *8 +15s, Power-on +2m
 *  00010110 0 00001110 00100011 11110111 00111101 [0x16] PC1555MX | Zone wiring: DEOL | Interval 4m
 *  00010110 0 00001110 00010000 11110011 00100111 [0x16] PC5015 | Zone wiring: DEOL | Armed, Exit *8 +15s, Power-on +2m
 *  00010110 0 00001110 01000001 11110101 01011010 [0x16] PC1832 | Zone wiring: NC | Interval 4m
 *  00010110 0 00001110 01000010 10110101 00011011 [0x16] PC1864 | Zone wiring: NC | Interval 4m
 *  00010110 0 00001110 01000010 10110001 00010111 [0x16] PC1864 | Zone wiring: NC | Armed
 */
void dscKeybusInterface::printPanel_0x16() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  if (panelData[2] == 0x0E) {
      
    stream->print(F("Panel version: "));
    stream->print(panelData[3],HEX);
/*
    switch (panelData[3]) {
      case 0x10: stream->print(F("PC5015 ")); break;
      case 0x11: stream->print(F("PC5016 ")); break;
      case 0x22: stream->print(F("PC1565 ")); break;
      case 0x23: stream->print(F("PC1555MX ")); break;
      case 0x41: stream->print(F("PC1832 ")); break;
      case 0x42: stream->print(F("PC1864 ")); break;
      default: stream->print(F("Unknown panel ")); break;
    }
*/
    switch (panelData[4] & 0x03) {
      case 0x01: stream->print(F("| Zone wiring: NC ")); break;
      case 0x02: stream->print(F("| Zone wiring: EOL ")); break;
      case 0x03: stream->print(F("| Zone wiring: DEOL ")); break;
    }

    switch (panelData[4] >> 2) {
      case 0x2C: stream->print(F("| Armed")); break;
      case 0x2D: stream->print(F("| Interval 4m")); break;
      case 0x34: stream->print(F("| Exit installer programming")); break;
      case 0x39: stream->print(F("| Installer programming")); break;
      case 0x3C: stream->print(F("| Armed, Exit *8 +15s, Power-on +2m")); break;
      case 0x3D: stream->print(F("| Interval 4m")); break;
      default: stream->print(F("| Unknown data")); break;
    }
  }
  else stream->print(F("Unknown data"));
}


/*
 *  0x1B: Status - partitions 5-8
 *  Interval: constant
 *  CRC: no
 *  Byte 2: Partition 5 lights
 *  Byte 3: Partition 5 status
 *  Byte 4: Partition 6 lights
 *  Byte 5: Partition 6 status
 *  Byte 6: Partition 7 lights
 *  Byte 7: Partition 7 status
 *  Byte 8: Partition 8 lights
 *  Byte 9: Partition 8 status
 *
 *  00011011 0 10010001 00000001 00010000 11000111 00010000 11000111 00010000 11000111 [0x1B]
 */
void dscKeybusInterface::printPanel_0x1B() {

  if (panelData[3] == 0xC7) {
    stream->print(F("Partition 5: disabled"));
  }
  else {
    stream->print(F("Partition 5: "));
    printPanelLights(2);
    stream->print(F("- "));
    printPanelMessages(3);
  }

  if (panelData[5] == 0xC7) {
    stream->print(F(" | Partition 6: disabled"));
  }
  else {
    stream->print(F(" | Partition 6: "));
    printPanelLights(4);
    stream->print(F("- "));
    printPanelMessages(5);
  }

  if (panelData[7] == 0xC7) {
    stream->print(F(" | Partition 7: disabled"));
  }
  else {
    stream->print(F(" | Partition 7: "));
    printPanelLights(6);
    stream->print(F("- "));
    printPanelMessages(7);
  }

  if (panelData[9] == 0xC7) {
    stream->print(F(" | Partition 8: disabled"));
  }
  else {
    stream->print(F(" | Partition 8: "));
    printPanelLights(8);
    stream->print(F("- "));
    printPanelMessages(9);
  }
}


/*
 *  0x1C: Verify keypad Fire/Auxiliary/Panic
 *  Interval: immediate after keypad button press
 *  CRC: no
 *
 *  01110111 1 11111111 11111111 11111111 11111111 [Keypad] Fire alarm
 *  00011100 0  [0x1C] Verify keypad Fire/Auxiliary/Panic
 *  01110111 1  [Keypad] Fire alarm
 */
void dscKeybusInterface::printPanel_0x1C() {
  stream->print(F("Verify keypad Fire/Auxiliary/Panic"));
}


/*
 *  0x27: Status with zones 1-8
 *  Interval: 4m
 *  CRC: yes
 *  Byte 2: Partition 1 lights
 *  Byte 3: Partition 1 status
 *  Byte 4: Partition 2 lights
 *  Byte 5: Partition 2 status
 *  Byte 6: Zones 1-8
 *
 *  00100111 0 10000001 00000001 10010001 11000111 00000000 00000001 [0x27] Status lights: Ready Backlight | Zones lights: none   // Unarmed, zones closed
 *  00100111 0 10000001 00000001 10010001 11000111 00000010 00000011 [0x27] Status lights: Ready Backlight | Zones lights: 2   // Unarmed, zone 2 open
 *  00100111 0 10001010 00000100 10010001 11000111 00000000 00001101 [0x27] Status lights: Armed Bypass Backlight | Zones lights: none  // Armed stay  // Periodic while armed
 *  00100111 0 10001010 00000100 11111111 11111111 00000000 10110011 [0x27] Status lights: Armed Bypass Backlight | Zones lights: none  // Armed stay +1s
 *  00100111 0 10000010 00000101 10010001 11000111 00000000 00000110 [0x27] Status lights: Armed Backlight | Zones lights: none  // Armed away  // Periodic while armed
 *  00100111 0 10000010 00000101 11111111 11111111 00000000 10101100 [0x27] Status lights: Armed Backlight | Zones lights: none  // Armed away +1s
 *  00100111 0 10000010 00001100 10010001 11000111 00000001 00001110 [0x27] Status lights: Armed Backlight | Zones lights: 1  // Delay zone 1 tripped, entrance delay
 *  00100111 0 10000010 00010001 10010001 11000111 00000001 00010011 [0x27] Status lights: Armed Backlight | Zones lights: 1  // Periodic after delay zone 1 tripped, alarm on   *  Periodic after fire alarm, alarm on
 *  00100111 0 10000010 00001101 10010001 11000111 00000001 00001111 [0x27] Status lights: Armed Backlight | Zones lights: 1  // Immediate after delay zone 1 tripped after previous alarm tripped
 *  00100111 0 10000010 00010001 11011011 11111111 00000010 10010110 [0x27] Status lights: Armed Backlight | Zones lights: 2  // Instant zone 2 tripped away
 *  00100111 0 00000001 00000001 11111111 11111111 00000000 00100111 [0x27] Status lights: Ready | Zones open 1-8: none  // Immediate after power on after panel reset
 *  00100111 0 10010001 00000001 11111111 11111111 00000000 10110111 [0x27] Status lights: Ready Trouble Backlight | Zones open 1-8: none  // +15s after exit *8
 *  00100111 0 10010001 00000001 10100000 00000000 00000000 01011001 [0x27] Status lights: Ready Trouble Backlight | Zones open 1-8: none  // +33s after power on after panel reset
 *  00100111 0 10010000 00000011 11111111 11111111 00111111 11110111 [0x27] Status lights: Trouble Backlight | Zones open: 1 2 3 4 5 6  // +122s after power on after panel reset
 *  00100111 0 10010000 00000011 10010001 11000111 00111111 01010001 [0x27] Status lights: Trouble Backlight | Zones open: 1 2 3 4 5 6  // +181s after power on after panel reset
 *  00100111 0 10000000 00000011 10000010 00000101 00011101 01001110 [0x27] Status lights: Backlight | Zones open | Zones 1-8 open: 1 3 4 5  // PC1832
 */
void dscKeybusInterface::printPanel_0x27() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Partition 1: "));
  printPanelLights(2);
  stream->print(F("- "));
  printPanelMessages(3);

  if (panelData[5] == 0xC7) {
    stream->print(F(" | Partition 2: disabled"));
  }
  else if (panelData[5] != 0xFF) {
    stream->print(F(" | Partition 2: "));
    printPanelLights(4);
    stream->print(F("- "));
    printPanelMessages(5);
  }

  stream->print(F(" | Zones 1-8 open: "));
  if (panelData[6] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(6,1);
  }
}


/*
 *  0x28: Zone expander zones 9-16 query
 *  Interval: after zone expander status notification
 *  CRC: no
 *
 *  11111111 1 11111111 11111111 10111111 11111111 [Zone Expander] Status notification
 *  00101000 0 11111111 11111111 11111111 11111111 11111111 [0x28] Zone expander query
 *  11111111 1 01010111 01010101 11111111 11111111 01101111 [Zone Expander] Status
 */
void dscKeybusInterface::printPanel_0x28() {
  stream->print(F("Zone expander zones 9-16 query"));
}


/*
 *  0x2D: Status with zones 9-16
 *  Interval: 4m
 *  CRC: yes
 *  Byte 2: Partition 1 lights
 *  Byte 3: Partition 1 status
 *  Byte 4: Partition 2 lights
 *  Byte 5: Partition 2 status
 *  Byte 6: Zones 9-16
 *
 *  00101101 0 10000000 00000011 10000001 11000111 00000001 11111001 [0x2D] Status lights: Backlight | Partition not ready | Open zones: 9
 *  00101101 0 10000000 00000011 10000010 00000101 00000000 00110111 [0x2D] Status lights: Backlight | Zones open | Zones 9-16 open: none  // PC1832
 */
void dscKeybusInterface::printPanel_0x2D() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Partition 1: "));
  printPanelLights(2);
  stream->print(F("- "));
  printPanelMessages(3);

  if (panelData[5] == 0xC7) {
    stream->print(F(" | Partition 2: disabled"));
  }
  else if (panelData[5] != 0xFF) {
    stream->print(F(" | Partition 2: "));
    printPanelLights(4);
    stream->print(F("- "));
    printPanelMessages(5);
  }

  stream->print(F(" | Zones 9-16 open: "));
  if (panelData[6] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(6,9);
  }
}


/*
 *  0x33: Zone expander zones 17-24 query
 *  Interval:
 *  CRC:
 *
 *  11111111 1 11111111 11111111 10111111 11111111 [Zone Expander] Status notification
 *  00101000 0 11111111 11111111 11111111 11111111 11111111 [0x28] Zone expander query
 *  11111111 1 01010111 01010101 11111111 11111111 01101111 [Zone Expander] Status
 */
void dscKeybusInterface::printPanel_0x33() {
  stream->print(F("Zone expander zones 17-24 query"));
}


/*
 *  0x34: Status with zones 17-24
 *  Interval: 4m
 *  CRC: yes
 *  Byte 2: Partition 1 lights
 *  Byte 3: Partition 1 status
 *  Byte 4: Partition 2 lights
 *  Byte 5: Partition 2 status
 *  Byte 6: Zones 17-24
 */
void dscKeybusInterface::printPanel_0x34() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Partition 1: "));
  printPanelLights(2);
  stream->print(F("- "));
  printPanelMessages(3);

  if (panelData[5] == 0xC7) {
    stream->print(F(" | Partition 2: disabled"));
  }
  else if (panelData[5] != 0xFF) {
    stream->print(F(" | Partition 2: "));
    printPanelLights(4);
    stream->print(F("- "));
    printPanelMessages(5);
  }

  stream->print(F(" | Zones 17-24 open: "));
  if (panelData[6] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(6,17);
  }
}


/*
 *  0x39: Zone expander zones 25-32 query
 *  Interval:
 *  CRC:
 *
 *  11111111 1 11111111 11111111 10111111 11111111 [Zone Expander] Status notification
 *  00101000 0 11111111 11111111 11111111 11111111 11111111 [0x28] Zone expander query
 *  11111111 1 01010111 01010101 11111111 11111111 01101111 [Zone Expander] Status
 */
void dscKeybusInterface::printPanel_0x39() {
  stream->print(F("Zone expander zones 25-32 query"));
}


/*
 *  0x3E: Status with zones 25-32
 *  Interval: 4m
 *  CRC: yes
 *  Byte 2: Partition 1 lights
 *  Byte 3: Partition 1 status
 *  Byte 4: Partition 2 lights
 *  Byte 5: Partition 2 status
 *  Byte 6: Zones 25-32
 */
void dscKeybusInterface::printPanel_0x3E() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Partition 1: "));
  printPanelLights(2);
  stream->print(F("- "));
  printPanelMessages(3);

  if (panelData[5] == 0xC7) {
    stream->print(F(" | Partition 2: disabled"));
  }
  else if (panelData[5] != 0xFF) {
    stream->print(F(" | Partition 2: "));
    printPanelLights(4);
    stream->print(F("- "));
    printPanelMessages(5);
  }

  stream->print(F(" | Zones 25-32 open: "));
  if (panelData[6] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(6,25);
  }
}


/*
 *  0x4C: Keybus query
 *  Interval: immediate after exiting *8 programming, immediate on keypad query
 *  CRC: no
 *
 *  11111111 1 11111111 11111111 11111110 11111111 [Keypad] Keybus notification
 *  01001100 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0x4C] Keybus query
 */
void dscKeybusInterface::printPanel_0x4C() {
  stream->print(F("Keybus query"));
}


/*
 *  0x58: Keybus query - valid response produces 0xA5 command, byte 6 0xB1-0xC0
 *  Interval: immediate after power on after panel reset
 *  CRC: no
 *
 *  11111111 1 11111111 11111111 11111111 11011111
 *  01011000 0 10101010 10101010 10101010 10101010 [0x58] Keybus query
 *  11111111 1 11111100 11111111 11111111 11111111
 *  10100101 0 00011000 01010101 01000000 11010111 10110011 11111111 11011011 [0xA5] 05/10/2018 00:53 | Unknown data, add to 0xA5_Byte7_0xFF, Byte 6: 0xB3
 */
void dscKeybusInterface::printPanel_0x58() {
  stream->print(F("Keybus query"));
}


/*
 *  0x5D: Flash panel lights: status and zones 1-32, partition 1
 *  Interval: 30s
 *  CRC: yes
 *  Byte 2: Status lights
 *  Byte 3: Zones 1-8
 *  Byte 4: Zones 9-16
 *  Byte 5: Zones 17-24
 *  Byte 6: Zones 25-32
 *
 *  01011101 0 00000000 00000000 00000000 00000000 00000000 01011101 [0x5D] Partition 1 | Status lights flashing: none | Zones 1-32 flashing: none
 *  01011101 0 00100000 00000000 00000000 00000000 00000000 01111101 [0x5D] Partition 1 | Status lights flashing: Program | Zones 1-32 flashing: none
 *  01011101 0 00000000 00100000 00000000 00000000 00000000 01111101 [0x5D] Partition 1 | Status lights flashing: none  | Zones 1-32 flashing: none 6
 *  01011101 0 00000100 00100000 00000000 00000000 00000000 10000001 [0x5D] Partition 1 | Status lights flashing: Memory | Zones 1-32 flashing: 6
 *  01011101 0 00000000 00000000 00000001 00000000 00000000 01011110 [0x5D] Partition 1 | Status lights flashing: none | Zones flashing: 9
 */
void dscKeybusInterface::printPanel_0x5D() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Partition 1 | Status lights flashing: "));
  printPanelLights(2);

  bool zoneLights = false;
  stream->print(F("| Zones 1-32 flashing: "));
  for (byte panelByte = 3; panelByte <= 6; panelByte++) {
    if (panelData[panelByte] != 0) {
      zoneLights = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + 1) + ((panelByte-3) *  8));
          stream->print(" ");
        }
      }
    }
  }
  if (!zoneLights) stream->print(F("none"));
}


/*
 *  0x63: Flash panel lights: status and zones 1-32, partition 2
 *  Interval: 30s
 *  CRC: yes
 *  Byte 2: Status lights
 *  Byte 3: Zones 1-8
 *  Byte 4: Zones 9-16
 *  Byte 5: Zones 17-24
 *  Byte 6: Zones 25-32
 *
 *  01100011 0 00000000 00000000 00000000 00000000 00000000 01100011 [0x63] Partition 2 | Status lights flashing: none | Zones 1-32 flashing: none
 *  01100011 0 00000100 10000000 00000000 00000000 00000000 11100111 [0x63] Partition 2 | Status lights flashing:Memory | Zones 1-32 flashing: 8
 */
void dscKeybusInterface::printPanel_0x63() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Partition 2 | Status lights flashing: "));
  printPanelLights(2);

  bool zoneLights = false;
  stream->print(F("| Zones 1-32 flashing: "));
  for (byte panelByte = 3; panelByte <= 6; panelByte++) {
    if (panelData[panelByte] != 0) {
      zoneLights = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + 1) + ((panelByte-3) *  8));
          stream->print(" ");
        }
      }
    }
  }
  if (!zoneLights) stream->print(F("none"));
}


/*
 *  0x64: Beep, partition 1
 *  CRC: yes
 *
 *  01100100 0 00001100 01110000 [0x64] Partition 1 | Beep: 6 beeps
 */
void dscKeybusInterface::printPanel_0x64() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Partition 1 | Beep: "));
  switch (panelData[2]) {
    case 0x04: stream->print(F("2 beeps")); break;
    case 0x06: stream->print(F("3 beeps")); break;
    case 0x08: stream->print(F("4 beeps")); break;
    case 0x0C: stream->print(F("6 beeps")); break;
    default: stream->print(F("Unknown data")); break;
  }
}


/*
 *  0x69: Beep, partition 2
 *  CRC: yes
 *
 *  01101001 0 00001100 01110101 [0x69] Partition 2 | Beep: 6 beeps
 */
void dscKeybusInterface::printPanel_0x69() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Partition 2 | Beep: "));
  switch (panelData[2]) {
    case 0x04: stream->print(F("2 beeps")); break;
    case 0x06: stream->print(F("3 beeps")); break;
    case 0x08: stream->print(F("4 beeps")); break;
    case 0x0C: stream->print(F("6 beeps")); break;
    default: stream->print(F("Unknown data")); break;
  }
}


/*
 *  0x75: Tone, partition 1
 *  CRC: yes
 *
 *  01110101 0 10000000 11110101 [0x75] Partition 1 | Tone: constant tone
 *  01110101 0 00000000 01110101 [0x75] Partition 1 | Tone: off
 */
void dscKeybusInterface::printPanel_0x75() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Partition 1 | Tone: "));
  switch (panelData[2]) {
    case 0x00: stream->print(F("off")); break;
    case 0x11: stream->print(F("1 beep, 1s interval")); break;
    case 0x31: stream->print(F("3 beeps, 1s interval")); break;
    case 0x80: stream->print(F("constant tone")); break;
    case 0xB1: stream->print(F("constant tone + 3 beeps, 1s interval")); break;
    default: stream->print(F("Unknown data")); break;
  }
}


/*
 *  0x7A: Tone, partition 2
 *  CRC: yes
 *
 *  01111010 0 00000000 01111010 [0x7A] Partition 2 | Tone: off
 */
void dscKeybusInterface::printPanel_0x7A() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Partition 2 | Tone: "));
  switch (panelData[2]) {
    case 0x00: stream->print(F("off")); break;
    case 0x11: stream->print(F("1 beep, 1s interval")); break;
    case 0x31: stream->print(F("3 beeps, 1s interval")); break;
    case 0x80: stream->print(F("constant tone")); break;
    case 0xB1: stream->print(F("constant tone + 3 beeps, 1s interval")); break;
    default: stream->print(F("Unknown data")); break;
  }
}


/*
 *  0x7F: Buzzer, partition 1
 *  CRC: yes
 *
 *  01111111 0 00000001 10000000 [0x7F] Buzzer: 1s
 */
void dscKeybusInterface::printPanel_0x7F() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Partition 1 | "));
  switch (panelData[2]) {
    case 0x01: stream->print(F("Buzzer: 1s")); break;
    case 0x02: stream->print(F("Buzzer: 2s")); break;
    default: stream->print(F("Unknown data")); break;
  }
}


/*
 *  0x82: Buzzer, partition 2
 *  CRC: yes
 *
 *  01111111 0 00000001 10000000 [0x82] Buzzer: 1s
 */
void dscKeybusInterface::printPanel_0x82() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Partition 2 | "));
  switch (panelData[2]) {
    case 0x01: stream->print(F("Buzzer: 1s")); break;
    case 0x02: stream->print(F("Buzzer: 2s")); break;
    default: stream->print(F("Unknown data")); break;
  }
}


/*
 *  0x87: Panel outputs
 *  CRC: yes
 *
 *  10000111 0 00000000 00000000 10000111 [0x87] Panel output: Bell off | PGM1 off | PGM2 off
 *  10000111 0 11111111 11110000 01110110 [0x87] Panel output: Bell on | PGM1 off | PGM2 off
 *  10000111 0 11111111 11110010 01111000 [0x87] Panel output: Bell on | PGM1 off | PGM2 on
 *  10000111 0 00000000 00000001 10001000 [0x87] Panel output: Bell off | PGM1 on | PGM2 off
 *  10000111 0 00000000 00001000 10001111 [0x87] Panel output: Bell off | Unknown command: Add to 0x87
 */
void dscKeybusInterface::printPanel_0x87() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Panel output:"));
  switch (panelData[2] & 0xF0) {
    case 0xF0: stream->print(F(" Bell on")); break;
    default: stream->print(F(" Bell off")); break;
  }

  if ((panelData[3] & 0x0F) <= 0x03) {
    if (bitRead(panelData[3],0)) stream->print(F(" | PGM1 on"));
    else stream->print(F(" | PGM1 off"));

    if (bitRead(panelData[3],1)) stream->print(F(" | PGM2 on"));
    else stream->print(F(" | PGM2 off"));
  }
  else stream->print(F(" | Unknown data"));

  if ((panelData[2] & 0x0F) != 0x0F) {
    if (bitRead(panelData[2],0)) stream->print(F(" | PGM3 on"));
    else stream->print(F(" | PGM3 off"));

    if (bitRead(panelData[2],1)) stream->print(F(" | PGM4 on"));
    else stream->print(F(" | PGM4 off"));
  }
}


/*
 *  0x8D: User code programming key response, codes 17-32
 *  CRC: yes
 *  Byte 2: TBD
 *  Byte 3: TBD
 *  Byte 4: TBD
 *  Byte 5: TBD
 *  Byte 6: TBD
 *  Byte 7: TBD
 *  Byte 8: TBD
 *
 *  10001101 0 00110001 00000001 00000000 00010111 11111111 11111111 11111111 11010011 [0x8D]   // Code 17 Key 1
 *  10001101 0 00110001 00000100 00000000 00011000 11111111 11111111 11111111 11010111 [0x8D]   // Code 18 Key 1
 *  10001101 0 00110001 00000100 00000000 00010010 11111111 11111111 11111111 11010001 [0x8D]   // Code 18 Key 2
 *  10001101 0 00110001 00000101 00000000 00111000 11111111 11111111 11111111 11111000 [0x8D]   // Code 18 Key 3
 *  10001101 0 00110001 00000101 00000000 00110100 11111111 11111111 11111111 11110100 [0x8D]   // Code 18 Key 4
 *  10001101 0 00110001 00100101 00000000 00001001 11111111 11111111 11111111 11101001 [0x8D]   // Code 29 Key 0
 *  10001101 0 00110001 00100101 00000000 00000001 11111111 11111111 11111111 11100001 [0x8D]   // Code 29 Key 1
 *  10001101 0 00110001 00110000 00000000 00000000 11111111 11111111 11111111 11101011 [0x8D]   // Message after 4th key entered
 */
void dscKeybusInterface::printPanel_0x8D() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("User code programming key response"));
}


/*
 *  0x94: ???
 *  Interval: immediate after entering *5 access code programming
 *  CRC: no
 *  Byte 2: TBD
 *  Byte 3: TBD
 *  Byte 4: TBD
 *  Byte 5: TBD
 *  Byte 6: TBD
 *  Byte 7: TBD
 *  Byte 8: TBD
 *  Byte 9: TBD
 *
 *  10010100 0 00010001 00000000 00000000 10100101 00000000 00000000 00000000 00010111 10100000 [0x94] Unknown command 1
 *  10010100 0 00010001 00000000 00000000 10100101 00000000 00000000 00000000 01001100 11111100 [0x94] Unknown command 2
 */
void dscKeybusInterface::printPanel_0x94() {
  switch (panelData[9]) {
    case 0x17: stream->print(F("Unknown command 1")); break;
    case 0x4C: stream->print(F("Unknown command 2")); break;
    default: stream->print(F("Unknown data"));
  }
}


/*
 *  0xA5: Date, time, system status messages - partitions 1-2
 *  CRC: yes
 */
void dscKeybusInterface::printPanel_0xA5() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  /*
   *  Date and time
   *  Interval: 4m
   *             YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
   *  10100101 0 00011000 00001110 11101101 10000000 00000000 00000000 00111000 [0xA5] 03/23/2018 13:32 | Timestamp
   */
  byte dscYear3 = panelData[2] >> 4;
  byte dscYear4 = panelData[2] & 0x0F;
  byte dscMonth = panelData[3] << 2; dscMonth >>=4;
  byte dscDay1 = panelData[3] << 6; dscDay1 >>= 3;
  byte dscDay2 = panelData[4] >> 5;
  byte dscDay = dscDay1 | dscDay2;
  byte dscHour = panelData[4] & 0x1F;
  byte dscMinute = panelData[5] >> 2;

  if (dscYear3 >= 7) stream->print(F("19"));
  else stream->print(F("20"));
  stream->print(dscYear3);
  stream->print(dscYear4);
  stream->print(F("."));
  if (dscMonth < 10) stream->print("0");
  stream->print(dscMonth);
  stream->print(F("."));
  if (dscDay < 10) stream->print("0");
  stream->print(dscDay);
  stream->print(F(" "));
  if (dscHour < 10) stream->print("0");
  stream->print(dscHour);
  stream->print(F(":"));
  if (dscMinute < 10) stream->print("0");
  stream->print(dscMinute);

  if (panelData[6] == 0 && panelData[7] == 0) {
    stream->print(F(" | Timestamp"));
    return;
  }

  switch (panelData[3] >> 6) {
    case 0x00: stream->print(F(" | ")); break;
    case 0x01: stream->print(F(" | Partition 1 | ")); break;
    case 0x02: stream->print(F(" | Partition 2 | ")); break;
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
 *  Bytes 2-5: partition 1
 *  Bytes 6-9: partition 2
 *
 *  10110001 0 11111111 00000000 00000000 00000000 00000000 00000000 00000000 00000000 10110000 [0xB1] Enabled zones - Partition 1: 1 2 3 4 5 6 7 8 | Partition 2: none
 *  10110001 0 10010001 10001010 01000001 10100100 00000000 00000000 00000000 00000000 10110001 [0xB1] Enabled zones - Partition 1: 1 5 8 10 12 16 17 23 27 30 32 | Partition 2: none
 *  10110001 0 11111111 00000000 00000000 11111111 00000000 00000000 00000000 00000000 10101111 [0xB1] Enabled zones - Partition 1: 1 2 3 4 5 6 7 8 25 26 27 28 29 30 31 32 | Partition 2: none
 *  10110001 0 01111111 11111111 00000000 00000000 10000000 00000000 00000000 00000000 10101111 [0xB1] Enabled zones - Partition 1: 1 2 3 4 5 6 7 9 10 11 12 13 14 15 16 | Partition 2: 8
 */
void dscKeybusInterface::printPanel_0xB1() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  bool enabledZones = false;
  stream->print(F("Enabled zones 1-32 | Partition 1: "));
  for (byte panelByte = 2; panelByte <= 5; panelByte++) {
    if (panelData[panelByte] != 0) {
      enabledZones = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + 1) + ((panelByte - 2) * 8));
          stream->print(" ");
        }
      }
    }
  }
  if (!enabledZones) stream->print(F("none "));

  enabledZones = false;
  stream->print(F("| Partition 2: "));
  for (byte panelByte = 6; panelByte <= 9; panelByte++) {
    if (panelData[panelByte] != 0) {
      enabledZones = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + 1) + ((panelByte - 6) * 8));
          stream->print(" ");
        }
      }
    }
  }
  if (!enabledZones) stream->print(F("none"));
}


/*
 *  0xBB: Bell
 *  Interval: immediate after alarm tripped except silent zones
 *  CRC: yes
 *
 *  10111011 0 00100000 00000000 11011011 [0xBB] Bell: on
 *  10111011 0 00000000 00000000 10111011 [0xBB] Bell: off
 */
void dscKeybusInterface::printPanel_0xBB() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Bell: "));
  if (bitRead(panelData[2],5)) stream->print(F("on"));
  else stream->print(F("off"));
}


/*
 *  0xC3: Keypad status
 *  Interval: 30s (PC1616/PC1832/PC1864)
 *  CRC: yes
 *
 *  11000011 0 00010000 11111111 11010010 [0xC3] Unknown command 1: Power-on +33s
 *  11000011 0 00110000 11111111 11110010 [0xC3] Keypad lockout
 *  11000011 0 00000000 11111111 11000010 [0xC3] Keypad ready
 */
void dscKeybusInterface::printPanel_0xC3() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  if (panelData[3] == 0xFF) {
    switch (panelData[2]) {
      case 0x00: stream->print(F("Keypad ready")); break;
      case 0x10: stream->print(F("Unknown command 1: Power-on +33s")); break;
      case 0x30:
      case 0x40: stream->print(F("Keypad lockout")); break;
      default: stream->print(F("Unknown data")); break;
    }
  }
  else stream->print(F("Unknown data"));
}


/*
 *  0xCE: Unknown command
 *  CRC: yes
 *
 * 11001110 0 00000001 10100000 00000000 00000000 00000000 01101111 [0xCE]  // Partition 1 exit delay
 * 11001110 0 00000001 10110001 00000000 00000000 00000000 10000000 [0xCE]  // Partition 1 armed stay
 * 11001110 0 00000001 10110011 00000000 00000000 00000000 10000010 [0xCE]  // Partition 1 armed away
 * 11001110 0 00000001 10100100 00000000 00000000 00000000 01110011 [0xCE]  // Partition 2 armed away
 * 11001110 0 01000000 11111111 11111111 11111111 11111111 00001010 [0xCE]  // Unknown data
 * 11001110 0 00100000 10001100 01010000 00000000 00000000 11001010 [0xCE] Unknown data  // LCD: System is in Alarm // Panic key
 * 11001110 0 01000000 11111111 11111111 11111111 11111111 00001010 [0xCE]  // LCD: System is in Alarm
 */
void dscKeybusInterface::printPanel_0xCE() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  switch (panelData[2]) {
    case 0x01: {
      switch (panelData[3]) {
        case 0xA0: stream->print(F("Partition 1,2 exit delay, partition 1,2 disarmed")); break;
        case 0xA4: stream->print(F("Partition 2 armed away")); break;
        case 0xB1: stream->print(F("Partition 1 armed stay")); break;
        case 0xB3: stream->print(F("Partition 1 armed away")); break;
        default: stream->print(F("Unknown data")); break;
      }
      break;
    }
    case 0x40: stream->print(F("Unknown data")); break;
    default: stream->print(F("Unknown data")); break;
  }
}


/*
 *  0xD5: Keypad zone query
 *  CRC: no
 *
 *  11111111 1 11111111 11111111 11111111 11111011 [Keypad] Status notification
 *  11010101 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0xD5] Keypad zone query
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 00001111 [Keypad] Slot 8
 */
void dscKeybusInterface::printPanel_0xD5() {
  stream->print(F("Keypad zone query"));
}


/*
 *  0xE6: Status, partitions 1-8
 *  CRC: yes
 *  Panels: PC5020, PC1616, PC1832, PC1864
 */
void dscKeybusInterface::printPanel_0xE6() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  switch (panelData[2]) {
    case 0x03: printPanel_0xE6_0x03(); break;  // Status in alarm/programming, partitions 5-8
  //  case 0x08: printPanel_0xE6_0x08(); break;  // Zone expander zones 33-40 query
    case 0x09: printPanel_0xE6_0x09(); break;  // Zones 33-40 status
  //  case 0x0A: printPanel_0xE6_0x0A(); break;  // Zone expander zones 41-48 query
    case 0x0B: printPanel_0xE6_0x0B(); break;  // Zones 41-48 status
   // case 0x0C: printPanel_0xE6_0x0C(); break;  // Zone expander zones 49-56 query
    case 0x0D: printPanel_0xE6_0x0D(); break;  // Zones 49-56 status
   // case 0x0E: printPanel_0xE6_0x0E(); break;  // Zone expander zones 57-64 query
    case 0x0F: printPanel_0xE6_0x0F(); break;  // Zones 57-64 status
    case 0x17: printPanel_0xE6_0x17(); break;  // Flash panel lights: status and zones 1-32, partitions 1-8
    case 0x18: printPanel_0xE6_0x18(); break;  // Flash panel lights: status and zones 33-64, partitions 1-8
    case 0x19: printPanel_0xE6_0x19(); break;  // Beep, partitions 3-8
    case 0x1A: printPanel_0xE6_0x1A(); break;  // Unknown command
    case 0x1D: printPanel_0xE6_0x1D(); break;  // Tone, partitions 3-8
    case 0x20: printPanel_0xE6_0x20(); break;  // Status in programming, zone lights 33-64
    case 0x2B: printPanel_0xE6_0x2B(); break;  // Enabled zones 1-32, partitions 3-8
    case 0x2C: printPanel_0xE6_0x2C(); break;  // Enabled zones 33-64, partitions 3-8
    case 0x41: printPanel_0xE6_0x41(); break;  // Status in access code programming, zone lights 65-95
    default: stream->print(F("Unknown data"));
  }
}


/*
 *  0xE6_0x03: Status in alarm/programming, partitions 5-8
 */
void dscKeybusInterface::printPanel_0xE6_0x03() {
  printPanelLights(2);
  stream->print(F("- "));
  printPanelMessages(3);
}


/*
 *  0xE6_0x08: Zone expander zones 33-40 query
 */
void dscKeybusInterface::printPanel_0xE6_0x08() {
  stream->print(F("Zone expander zones 33-40 query"));
}


/*
 *  0xE6_0x09: Zones 33-40 status
 */
void dscKeybusInterface::printPanel_0xE6_0x09() {
  stream->print(F("Zones 33-40 open: "));
  if (panelData[3] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(3,33);
  }
}


/*
 *  0xE6_0x0A: Zone expander zones 41-48 query
 */
void dscKeybusInterface::printPanel_0xE6_0x0A() {
  stream->print(F("Zone expander zones 41-48 query"));
}


/*
 *  0xE6_0x0B: Zones 41-48 status
 */
void dscKeybusInterface::printPanel_0xE6_0x0B() {
  stream->print(F("Zones 41-48 open: "));
  if (panelData[3] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(3,41);
  }
}


/*
 *  0xE6_0x0C: Zone expander zones 49-56 query
 */
void dscKeybusInterface::printPanel_0xE6_0x0C() {
  stream->print(F("Zone expander zones 49-56 query"));
}


/*
 *  0xE6_0x0D: Zones 49-56 status
 */
void dscKeybusInterface::printPanel_0xE6_0x0D() {
  stream->print(F("Zones 49-56 open: "));
  if (panelData[3] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(3,49);
  }
}


/*
 *  0xE6_0x0E: Zone expander zones 57-64 query
 */
void dscKeybusInterface::printPanel_0xE6_0x0E() {
  stream->print(F("Zone expander zones 57-64 query"));
}


/*
 *  0xE6_0x0F: Zones 57-64 status
 */
void dscKeybusInterface::printPanel_0xE6_0x0F() {
  stream->print(F("Zones 57-64 open: "));
  if (panelData[3] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(3,57);
  }
}


/*
 *  0xE6_0x17: Flash panel lights: status and zones 1-32, partitions 1-8
 *
 *  11100110 0 00010111 00000100 00000000 00000100 00000000 00000000 00000000 00000101 [0xE6] Partition 3 |  // Zone 3
 */
void dscKeybusInterface::printPanel_0xE6_0x17() {
  stream->print(F("Partition "));
  if (panelData[3] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(3,1);
  }

  stream->print(F("| Status lights flashing: "));
  printPanelLights(4);

  bool zoneLights = false;
  stream->print(F("| Zones 1-32 flashing: "));
  for (byte panelByte = 5; panelByte <= 8; panelByte++) {
    if (panelData[panelByte] != 0) {
      zoneLights = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + 1) + ((panelByte-5) *  8));
          stream->print(" ");
        }
      }
    }
  }
  if (!zoneLights) stream->print(F("none"));
}


/*
 *  0xE6_0x18: Flash panel lights: status and zones 33-64, partitions 1-8
 *
 *  11100110 0 00011000 00000001 00000000 00000001 00000000 00000000 00000000 00000000 [0xE6] Partition 1 |  // Zone 33
 *  11100110 0 00011000 00000001 00000100 00000000 00000000 00000000 10000000 10000011 [0xE6] Partition 1 |  // Zone 64
 */
void dscKeybusInterface::printPanel_0xE6_0x18() {
  stream->print(F("Partition "));
  if (panelData[3] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(3,1);
  }

  stream->print(F("| Status lights flashing: "));
  printPanelLights(4);

  bool zoneLights = false;
  stream->print(F("| Zones 33-64 flashing: "));
  for (byte panelByte = 5; panelByte <= 8; panelByte++) {
    if (panelData[panelByte] != 0) {
      zoneLights = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + 33) + ((panelByte-5) *  8));
          stream->print(" ");
        }
      }
    }
  }
  if (!zoneLights) stream->print(F("none"));
}


/*
 *  0xE6_0x19: Beep, partitions 3-8
 */
void dscKeybusInterface::printPanel_0xE6_0x19() {
  stream->print(F("Partition "));
  if (panelData[3] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(3,1);
  }

  stream->print(F("| Beep: "));
  switch (panelData[4]) {
    case 0x04: stream->print(F("2 beeps")); break;
    case 0x06: stream->print(F("3 beeps")); break;
    case 0x08: stream->print(F("4 beeps")); break;
    case 0x0C: stream->print(F("6 beeps")); break;
    default: stream->print(F("Unknown data")); break;
  }
}


/*
 *  0xE6_0x1A: Unknown data
 *
 *  11100110 0 00011010 01000000 00000000 00000000 00001001 00000000 00000000 00000000 01001001 [0xE6] 0x1A: Unknown data  // All partitions exit delay in progress, disarmed
 *  11100110 0 00011010 01000000 00010001 00000000 00001001 00000000 00000000 00000000 01011010 [0xE6] 0x1A: Unknown data  // Keypad panic alarm
 *  11100110 0 00011010 01000000 00000010 00000000 00001001 00000000 00000000 00000000 01001011 [0xE6] 0x1A: Unknown data  // Partition 2 in alarm
    11100110 0 00011010 01000000 00000001 00000000 00001001 00000000 00000000 00000000 01001010 [0xE6] 0x1A: Unknown data  // Partition 1 in alarm

    10111011 0 00100000 00000000 11011011 [0xBB] Bell: on
    10100101 0 00010110 00010111 11100000 10001000 00010000 10010001 11011011 [0xA5] 2016.05.31 00:34 | Zone alarm: 8
    10000111 0 00000000 00000010 10001001 [0x87] Panel output: Bell off | PGM1 off | PGM2 on | PGM3 off | PGM4 off
    11100110 0 00011010 01000000 10000000 00000000 00001001 00000000 00000000 00000000 11001001 [0xE6] 0x1A: Unknown data
    01110101 0 00000000 01110101 [0x75] Partition 1 | Tone: off
    11100110 0 00011010 01000000 10000000 00000000 00001001 00000000 00000000 00000000 11001001 [0xE6] 0x1A: Unknown data  // Partition 8 in alarm

    10111011 0 00000000 00000000 10111011 [0xBB] Bell: off
    11100110 0 00011010 01000000 00000000 00000000 00001001 00000000 00000000 00000000 01001001 [0xE6] 0x1A: Unknown data  // Partition 8 bell off, in alarm
    01110101 0 00000000 01110101 [0x75] Partition 1 | Tone: off
 */
void dscKeybusInterface::printPanel_0xE6_0x1A() {
  stream->print(F("0x1A: "));
  stream->print(F("Unknown data"));
}


/*
 *  0xE6_0x1D: Tone, partitions 3-8
 */
void dscKeybusInterface::printPanel_0xE6_0x1D() {
  stream->print(F("Partition "));
  if (panelData[3] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(3,1);
  }

  stream->print(F("| Tone: "));
  switch (panelData[4]) {
    case 0x00: stream->print(F("off")); break;
    case 0x11: stream->print(F("1 beep, 1s interval")); break;
    case 0x31: stream->print(F("3 beeps, 1s interval")); break;
    case 0x80: stream->print(F("constant tone")); break;
    case 0xB1: stream->print(F("constant tone + 3 beeps, 1s interval")); break;
    default: stream->print(F("Unknown data")); break;
  }
}


/*
 *  0xE6_0x20: Status in programming, zone lights 33-64
 *  Interval: constant in *8 programming
 *  CRC: yes
 */
void dscKeybusInterface::printPanel_0xE6_0x20() {
  stream->print(F("Status lights: "));
  printPanelLights(3);
  stream->print(F("- "));
  printPanelMessages(4);

  bool zoneLights = false;
  stream->print(F(" | Zone lights: "));
  for (byte panelByte = 5; panelByte <= 8; panelByte++) {
    if (panelData[panelByte] != 0) {
      zoneLights = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + 33) + ((panelByte-5) *  8));
          stream->print(" ");
        }
      }
    }
  }

  if (!zoneLights) stream->print(F("none"));
}


/*
 *  0xE6_0x2B: Enabled zones 1-32, partitions 3-8
 */
void dscKeybusInterface::printPanel_0xE6_0x2B() {
  stream->print(F("Partition "));
  if (panelData[3] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(3,1);
  }

  bool enabledZones = false;
  stream->print(F("| Enabled zones  1-32: "));
  for (byte panelByte = 4; panelByte <= 7; panelByte++) {
    if (panelData[panelByte] != 0) {
      enabledZones = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + 1) + ((panelByte - 4) * 8));
          stream->print(" ");
        }
      }
    }
  }
  if (!enabledZones) stream->print(F("none"));
}


/*
 *  0xE6_0x2C: Enabled zones 33-64, partitions 1-8
 */
void dscKeybusInterface::printPanel_0xE6_0x2C() {
  stream->print(F("Partition "));
  if (panelData[3] == 0) stream->print(F("none"));
  else {
    printPanelBitNumbers(3,1);
  }

  bool enabledZones = false;
  stream->print(F("| Enabled zones 33-64: "));
  for (byte panelByte = 4; panelByte <= 7; panelByte++) {
    if (panelData[panelByte] != 0) {
      enabledZones = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + 33) + ((panelByte - 4) * 8));
          stream->print(" ");
        }
      }
    }
  }
  if (!enabledZones) stream->print(F("none"));
}


/*
 *  0xE6_0x41: Status in programming, zone lights 65-95
 *  CRC: yes
 */
void dscKeybusInterface::printPanel_0xE6_0x41() {
  stream->print(F("Status lights: "));
  printPanelLights(3);
  stream->print(F("- "));
  printPanelMessages(4);

  bool zoneLights = false;
  stream->print(F(" | Zone lights: "));
  for (byte panelByte = 5; panelByte <= 8; panelByte++) {
    if (panelData[panelByte] != 0) {
      zoneLights = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + 65) + ((panelByte-5) *  8));
          stream->print(" ");
        }
      }
    }
  }

  if (!zoneLights) stream->print(F("none"));
}


/*
 *  0xEB: Date, time, system status messages - partitions 1-8
 *  CRC: yes
 *
 *                     YYY1YYY2   MMMMDD DDDHHHHH MMMMMM
 * 11101011 0 00000001 00011000 00011000 10001010 00101100 00000000 10111011 00000000 10001101 [0xEB] 06/04/2018 10:11 | Partition: 1  // Armed stay
 * 11101011 0 00000001 00011000 00011000 10001010 00111000 00000000 10111011 00000000 10011001 [0xEB] 06/04/2018 10:14 | Partition: 1  // Armed away
 * 11101011 0 00000001 00011000 00011000 10001010 00111000 00000010 10011011 00000000 01111011 [0xEB] 06/04/2018 10:14 | Partition: 1  // Armed away
 * 11101011 0 00000001 00011000 00011000 10001010 00110100 00000000 11100010 00000000 10111100 [0xEB] 06/04/2018 10:13 | Partition: 1  // Disarmed
 * 11101011 0 00000001 00011000 00011000 10001111 00101000 00000100 00000000 10010001 01101000 [0xEB] 06/04/2018 15:10 | Partition: 1 | Unknown data, add to printPanelStatus0, Byte 8: 0x00
 * 11101011 0 00000001 00000001 00000100 01100000 00010100 00000100 01000000 10000001 00101010 [0xEB] 2001.01.03 00:05 | Partition 1 | Zone tamper: 33
 * 11101011 0 00000001 00000001 00000100 01100000 00001000 00000100 01011111 10000001 00111101 [0xEB] 2001.01.03 00:02 | Partition 1 | Zone tamper: 64
 * 11101011 0 00000001 00000001 00000100 01100000 00011000 00000100 01100000 11111111 11001100 [0xEB] 2001.01.03 00:06 | Partition 1 | Zone tamper restored: 33
 * 11101011 0 00000000 00000001 00000100 01100000 01001000 00010100 01100000 10000001 10001101 [0xEB] 2001.01.03 00:18 | Zone fault: 33
 * 11101011 0 00000000 00000001 00000100 01100000 01001100 00010100 01000000 11111111 11101111 [0xEB] 2001.01.03 00:19 | Zone fault restored: 33
 * 11101011 0 00000000 00000001 00000100 01100000 00001100 00010100 01011111 11111111 11001110 [0xEB] 2001.01.03 00:03 | Zone fault restored: 64
 */
void dscKeybusInterface::printPanel_0xEB() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  byte dscYear3 = panelData[3] >> 4;
  byte dscYear4 = panelData[3] & 0x0F;
  byte dscMonth = panelData[4] << 2; dscMonth >>=4;
  byte dscDay1 = panelData[4] << 6; dscDay1 >>= 3;
  byte dscDay2 = panelData[5] >> 5;
  byte dscDay = dscDay1 | dscDay2;
  byte dscHour = panelData[5] & 0x1F;
  byte dscMinute = panelData[6] >> 2;

  if (dscYear3 >= 7) stream->print(F("19"));
  else stream->print(F("20"));
  stream->print(dscYear3);
  stream->print(dscYear4);
  stream->print(F("."));
  if (dscMonth < 10) stream->print("0");
  stream->print(dscMonth);
  stream->print(F("."));
  if (dscDay < 10) stream->print("0");
  stream->print(dscDay);
  stream->print(F(" "));
  if (dscHour < 10) stream->print("0");
  stream->print(dscHour);
  stream->print(F(":"));
  if (dscMinute < 10) stream->print("0");
  stream->print(dscMinute);

  if (panelData[2] == 0) stream->print(F(" | "));
  else {
    stream->print(F(" | Partition "));
    printPanelBitNumbers(2,1);
    stream->print(F("| "));
  }

  switch (panelData[7]) {
    case 0x00: printPanelStatus0(8); return;
    case 0x01: printPanelStatus1(8); return;
    case 0x02: printPanelStatus2(8); return;
    case 0x03: printPanelStatus3(8); return;
    case 0x04: printPanelStatus4(8); return;
    case 0x14: printPanelStatus14(8); return;
  }
}


/*
 *  Print keypad and module messages
 */


/*
 *  Keypad: Fire alarm
 *
 *  01110111 1 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Fire alarm
 */
void dscKeybusInterface::printModule_0x77() {
  stream->print(F("[Keypad] Fire alarm"));
}


/*
 *  Keypad: Auxiliary alarm
 *
 *  10111011 1 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Aux alarm
 */
void dscKeybusInterface::printModule_0xBB() {
  stream->print(F("[Keypad] Auxiliary alarm"));
}


/*
 *  Keypad: Panic alarm
 *
 *  11011101 1 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Panic alarm
 */
void dscKeybusInterface::printModule_0xDD() {
  stream->print(F("[Keypad] Panic alarm"));
}


/*
 *  Keybus status notifications
 */

void dscKeybusInterface::printModule_Notification() {
  switch (moduleData[4]) {
    // Zone expander: status update notification, panel responds with 0x28
    // 11111111 1 11111111 11111111 10111111 11111111 [Zone Expander] Status notification
    // 00101000 0 11111111 11111111 11111111 11111111 11111111 [0x28] Zone expander query
    case 0xBF:
      stream->print(F("[Zone Expander] Status notification"));
      break;

    // Keypad: Unknown Keybus notification, panel responds with 0x4C query
    // 11111111 1 11111111 11111111 11111110 11111111 [Keypad] Unknown Keybus notification
    // 01001100 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0x4C] Unknown Keybus query
    case 0xFE:
      stream->print(F("[Keypad] Unknown Keybus notification"));
      break;
  }

  switch (moduleData[5]) {
    // Keypad: zone status update notification, panel responds with 0xD5 query
    // 11111111 1 11111111 11111111 11111111 11111011 [Keypad] Zone status notification
    // 11010101 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0xD5] Keypad zone query
    case 0xFB:
      stream->print(F("[Keypad] Zone status notification"));
      break;
  }
}


/*
 *  Module supervision query response
 *
 *  00010001 0 10101010 10101010 10101010 10101010 10101010 [0x11] Module supervision query
 *  11111111 1 00111111 11111111 11111111 11111111 11111111 [Keypad] Slots active: 1
 */
void dscKeybusInterface::printModule_Panel_0x11() {
  if (moduleData[2] != 0xFF || moduleData[3] != 0xFF) {
    stream->print(F("[Keypad] Slots: "));
    if ((moduleData[2] & 0xC0) == 0) stream->print(F("1 "));
    if ((moduleData[2] & 0x30) == 0) stream->print(F("2 "));
    if ((moduleData[2] & 0x0C) == 0) stream->print(F("3 "));
    if ((moduleData[2] & 0x03) == 0) stream->print(F("4 "));
    if ((moduleData[3] & 0xC0) == 0) stream->print(F("5 "));
    if ((moduleData[3] & 0x30) == 0) stream->print(F("6 "));
    if ((moduleData[3] & 0x0C) == 0) stream->print(F("7 "));
    if ((moduleData[3] & 0x03) == 0) stream->print(F("8 "));
  }

  if (moduleData[4] != 0xFF || (moduleData[5] != 0xFF && moduleData[5] != 0xF3)) {
    stream->print(F(" [Zone Expander] Modules: "));
    if ((moduleData[4] & 0xC0) == 0) stream->print(F("1 "));
    if ((moduleData[4] & 0x30) == 0) stream->print(F("2 "));
    if ((moduleData[4] & 0x0C) == 0) stream->print(F("3 "));
    if ((moduleData[4] & 0x03) == 0) stream->print(F("4 "));
    if ((moduleData[5] & 0xC0) == 0) stream->print(F("5 "));
    if ((moduleData[5] & 0x30) == 0) stream->print(F("6 "));
  }
}


/*
 *  Keypad: Panel 0xD5 zone query response
 *  Bytes 2-9: Keypad slots 1-8
 *  Bits 2,3: TBD
 *  Bits 6,7: TBD
 *
 *  11111111 1 11111111 11111111 11111111 11111011 [Keypad] Status update
 *  11010101 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0xD5] Keypad zone query
 *
 *  11111111 1 00000011 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Slot 1 | Zone open
 *  11111111 1 00001111 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Slot 1 | Zone open  // Exit *8 programming
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 00001111 [Keypad] Slot 8 | Zone open  // Exit *8 programming
 *
 *  11111111 1 11110000 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Slot 1   //Zone closed while unconfigured
 *  11111111 1 11110011 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Slot 1   //Zone closed while unconfigured after opened once
 *  11111111 1 00110000 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Slot 1 | Zone closed // NC
 *  11111111 1 00111100 11111111 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Slot 1 | Zone closed  //After exiting *8 programming after NC
 */
void dscKeybusInterface::printModule_Panel_0xD5() {
  stream->print(F("[Keypad] "));
  bool firstData = true;
  for (byte moduleByte = 2; moduleByte <= 9; moduleByte++) {
    byte slotData = moduleData[moduleByte];
    if (slotData < 0xFF) {
      if (firstData) stream->print(F("Slot "));
      else stream->print(F(" | Slot "));
      stream->print(moduleByte - 1);
      if ((slotData & 0x03) == 0x03 && (slotData & 0x30) == 0) stream->print(F(" zone open"));
      if ((slotData & 0x03) == 0 && (slotData & 0x30) == 0x30) stream->print(F(" zone closed"));
      firstData = false;
    }
  }
}


/*
 *  Keypad: keys
 *
 *  11111111 1 00000101 11111111 11111111 11111111 [Keypad] 1
 *  11111111 1 00101101 11111111 11111111 11111111 [Keypad] #
 */
void dscKeybusInterface::printModule_Keys() {
  stream->print(F("[Keypad] "));

  byte keyByte = 2;
  if (currentCmd == 0x05) {
    if (moduleData[2] != 0xFF) {
      stream->print(F("Partition 1 | Key: "));
    }
    else if (moduleData[3] != 0xFF) {
      stream->print(F("Partition 2 | Key: "));
      keyByte = 3;
    }
    else if (moduleData[8] != 0xFF) {
      stream->print(F("Partition 3 | Key: "));
      keyByte = 8;
    }

    else if (moduleData[9] != 0xFF) {
      stream->print(F("Partition 4 | Key: "));
      keyByte = 9;
    }
  }
  else if (currentCmd == 0x1B) {
    if (moduleData[2] != 0xFF) {
      stream->print(F("Partition 5 | Key: "));
    }
    else if (moduleData[3] != 0xFF) {
      stream->print(F("Partition 6 | Key: "));
      keyByte = 3;
    }
    else if (moduleData[8] != 0xFF) {
      stream->print(F("Partition 7 | Key: "));
      keyByte = 8;
    }

    else if (moduleData[9] != 0xFF) {
      stream->print(F("Partition 8 | Key: "));
      keyByte = 9;
    }
  }

  if (hideKeypadDigits && (moduleData[2] <= 0x27 || moduleData[3] <= 0x27 || moduleData[8] <= 0x27 || moduleData[9] <= 0x27)) {
    stream->print(F("[Digit]"));
    return;
  }

  switch (moduleData[keyByte]) {
    case 0x00: stream->print(F("0")); break;
    case 0x05: stream->print(F("1")); break;
    case 0x0A: stream->print(F("2")); break;
    case 0x0F: stream->print(F("3")); break;
    case 0x11: stream->print(F("4")); break;
    case 0x16: stream->print(F("5")); break;
    case 0x1B: stream->print(F("6")); break;
    case 0x1C: stream->print(F("7")); break;
    case 0x22: stream->print(F("8")); break;
    case 0x27: stream->print(F("9")); break;
    case 0x28: stream->print(F("*")); break;
    case 0x2D: stream->print(F("#")); break;
    case 0x52: stream->print(F("Identified voice prompt help")); break;
    case 0x70: stream->print(F("Command output 3")); break;
    case 0xAF: stream->print(F("Arm stay")); break;
    case 0xB1: stream->print(F("Arm away")); break;
    case 0xB6: stream->print(F("*9 No entry delay arm, requires access code")); break;
    case 0xBB: stream->print(F("Door chime configuration")); break;
    case 0xBC: stream->print(F("*6 System test")); break;
    case 0xC3: stream->print(F("*1 Zone bypass programming")); break;
    case 0xC4: stream->print(F("*2 Trouble menu")); break;
    case 0xC9: stream->print(F("*3 Alarm memory display")); break;
    case 0xCE: stream->print(F("*5 Programming, requires master code")); break;
    case 0xD0: stream->print(F("*6 Programming, requires master code")); break;
    case 0xD5: stream->print(F("Command output 1")); break;
    case 0xDA: stream->print(F("Reset / Command output 2")); break;
    case 0xDF: stream->print(F("General voice prompt help")); break;
    case 0xE1: stream->print(F("Quick exit")); break;
    case 0xE6: stream->print(F("Activate stay/away zones")); break;
    case 0xEB: stream->print(F("Function key [20] Future Use")); break;
    case 0xEC: stream->print(F("Command output 4")); break;
    case 0xF7: stream->print(F("Left/right arrow")); break;
    default:
      stream->print(F("Unknown key: 0x"));
      stream->print(moduleData[keyByte], HEX);
      break;
  }
}


/*
 * Print binary
 */

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


/*
 * Print panel command as hex
 */
void dscKeybusInterface::printPanelCommand() {
  // Prints the hex value of command byte 0
  stream->print(F("0x"));
  if (panelData[0] < 16) stream->print("0");
  stream->print(panelData[0], HEX);
}
