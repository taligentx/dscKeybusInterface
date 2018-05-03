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


void dscKeybusInterface::printKeypadBinary(bool printSpaces) {
  for (byte keypadByte = 0; keypadByte < keypadByteCount; keypadByte++) {
    if (keypadByte == 1) stream->print(keypadData[keypadByte]);  //Prints the stop bit
    else {
      for (byte mask = 0x80; mask; mask >>= 1) {
        if (mask & keypadData[keypadByte]) stream->print("1");
        else stream->print("0");
      }
    }
    if (printSpaces && (keypadByte != keypadByteCount - 1 || displayTrailingBits)) stream->print(" ");
  }

  if (displayTrailingBits) {
    byte trailingBits = (keypadBitCount - 1) % 8;
    if (trailingBits > 0) {
      for (int i = trailingBits - 1; i >= 0; i--) {
        stream->print(bitRead(keypadData[keypadByteCount], i));
      }
    }
  }
}

/*
 *  Print messages
 */


void dscKeybusInterface::printPanelCommand() {
  // Prints the hex value of command byte 0
  stream->print(F("0x"));
  if (panelData[0] < 16) stream->print("0");
  stream->print(panelData[0], HEX);
}


void dscKeybusInterface::printPanelMessage() {
  switch (panelData[0]) {
    case 0x05: printPanel_0x05(); break;
    case 0x27: printPanel_0x27(); break;
    case 0x0A: printPanel_0x0A(); break;
    case 0x11: printPanel_0x11(); break;
    case 0x16: printPanel_0x16(); break;
    case 0x1C: printPanel_0x1C(); break;
    case 0x4C: printPanel_0x4C(); break;
    case 0x58: printPanel_0x58(); break;
    case 0x5D: printPanel_0x5D(); break;
    case 0x64: printPanel_0x64(); break;
    case 0x75: printPanel_0x75(); break;
    case 0x7F: printPanel_0x7F(); break;
    case 0x87: printPanel_0x87(); break;
    case 0x8D: printPanel_0x8D(); break;
    case 0x94: printPanel_0x94(); break;
    case 0xA5: printPanel_0xA5(); break;
    case 0xB1: printPanel_0xB1(); break;
    case 0xBB: printPanel_0xBB(); break;
    case 0xC3: printPanel_0xC3(); break;
    case 0xD5: printPanel_0xD5(); break;
    default: {
      stream->print(F("Unrecognized command "));
      if (!validCRC()) {
        stream->print(F("[No CRC or CRC Error]"));
        break;
      }
      else stream->print(F("[CRC OK]"));
      break;
    }
  }
}


void dscKeybusInterface::printKeypadMessage() {
  stream->print(F("[Keypad] "));

  switch (keypadData[0]) {
    case 0xFF:  {
      if (keypadData[4] == 0xFE) {
        printKeypad_0xFF_Byte4_0xFE();
        break;
      }
      if (keypadData[5] == 0xFB) {
        printKeypad_0xFF_Byte5_0xFB();
        break;
      }
      if (panelData[0] == 0xD5) {
        printKeypad_0xFF_Panel_0xD5();
        break;
      }
      else {
        if (keypadData[2] != 0xFF) {
          printKeypad_0xFF_Byte2();
        }
        if (keypadData[3] != 0xFF) {
          printKeypad_0xFF_Byte3();
        }
      }
      break;
    }
    case 0x77: printKeypad_0x77(); break;
    case 0xBB: printKeypad_0xBB(); break;
    case 0xDD: printKeypad_0xDD(); break;
  }
}


/*
 *  Print keypad messages
 */


/*
 *  Keypad: Keybus notification, panel responds with 0x4C query
 *
 *  11111111 1 11111111 11111111 11111110 11111111 [Keypad] Keybus notification
 *  01001100 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0x4C] Keybus query
 */
void dscKeybusInterface::printKeypad_0xFF_Byte4_0xFE() {
  stream->print(F("Keybus notification"));
}


/*
 *  Keypad: status update notification, panel responds with 0xD5 query
 *
 *  11111111 1 11111111 11111111 11111111 11111011 [Keypad] Status notification
 *  11010101 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0xD5] Keypad zone query
 */
void dscKeybusInterface::printKeypad_0xFF_Byte5_0xFB() {
  stream->print(F("Status notification"));
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
void dscKeybusInterface::printKeypad_0xFF_Panel_0xD5() {
  bool firstData = true;
  for (byte keypadByte = 2; keypadByte <= 9; keypadByte++) {
    byte slotData = keypadData[keypadByte];
    if (slotData < 0xFF) {
      if (firstData) stream->print(F("Slot "));
      else stream->print(F(" | Slot "));
      stream->print(keypadByte - 1);
      if ((slotData & 0x03) == 0x03 && (slotData & 0x30) == 0) stream->print(F(" zone open"));
      if ((slotData & 0x03) == 0 && (slotData & 0x30) == 0x30) stream->print(F(" zone closed"));
      firstData = false;
    }
  }
}


/*
 *  Keypad: buttons and panel 0x11 keypad slot query response for slots 1-4
 * 
 *  The panel rejects unknown keypad data - all keys that are accepted by
 *  the panel are included here.
 *
 *  11111111 1 00000101 11111111 11111111 11111111 [Keypad] 1
 *  11111111 1 00101101 11111111 11111111 11111111 [Keypad] #
 *  11111111 1 00111111 11111111 11111111 11111111 11111111 [Keypad] Slot 1
 */
void dscKeybusInterface::printKeypad_0xFF_Byte2() {
  bool decodedKey = true;
  switch (keypadData[2]) {
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
    case 0x33: stream->print(F("Valid key, unknown function")); break;
    case 0x34: stream->print(F("Valid key, unknown function")); break;
    case 0x39: stream->print(F("Valid key, unknown function")); break;
    case 0x3E: stream->print(F("Valid key, unknown function")); break;
    case 0x41: stream->print(F("Arm stay 2")); break;
    case 0x46: stream->print(F("Valid key, unknown function")); break;
    case 0x4B: stream->print(F("Arm away 2")); break;
    case 0x4C: stream->print(F("Valid key, unknown function")); break;
    case 0x52: stream->print(F("Valid key, unknown function - 0xBB Byte 3: 0x00")); break;
    case 0x57: stream->print(F("Valid key, unknown function - 0xBB Byte 3: 0x01")); break;
    case 0x58: stream->print(F("Valid key, unknown function - 0xBB Byte 3: 0x02")); break;
    case 0x5D: stream->print(F("Valid key, unknown function - 0xBB Byte 3: 0x03")); break;
    case 0x63: stream->print(F("Valid key, unknown function - 0xBB Byte 3: 0x04")); break;
    case 0x64: stream->print(F("Valid key, unknown function - 0xBB Byte 3: 0x05")); break;
    case 0x69: stream->print(F("Valid key, unknown function - 0xBB Byte 3: 0x06")); break;
    case 0x6E: stream->print(F("Valid key, unknown function - 0xBB Byte 3: 0x07")); break;
    case 0x70: stream->print(F("Valid key, unknown function - 0x05 Byte 3: 0x0E unknown message")); break;
    case 0x75: stream->print(F("Valid key, unknown function")); break;
    case 0x7A: stream->print(F("Valid key, unknown function")); break;
    case 0x7F: stream->print(F("Valid key, unknown function")); break;
    case 0x82: stream->print(F("Valid key, unknown function - 0x05: * pressed")); break;
    case 0x87: stream->print(F("Valid key, unknown function - 0x05: * pressed")); break;
    case 0x88: stream->print(F("Valid key, unknown function - 0x05: * pressed")); break;
    case 0x8D: stream->print(F("Valid key, unknown function")); break;
    case 0x93: stream->print(F("Valid key, unknown function")); break;
    case 0x94: stream->print(F("Valid key, unknown function")); break;
    case 0x99: stream->print(F("Valid key, unknown function")); break;
    case 0x9E: stream->print(F("Valid key, unknown function - 0x05: * pressed")); break;
    case 0xA0: stream->print(F("Valid key, unknown function - 0x05: * pressed")); break;
    case 0xA5: stream->print(F("Valid key, unknown function - 0x05: * pressed")); break;
    case 0xAA: stream->print(F("Valid key, unknown function - 0x05: * pressed")); break;
    case 0xAF: stream->print(F("Arm stay")); break;
    case 0xB1: stream->print(F("Arm away")); break;
    case 0xB6: stream->print(F("Valid key, unknown function - 0x05: command output 1, 0x0A programming status")); break;
    case 0xBB: stream->print(F("Door chime configuration")); break;
    case 0xBC: stream->print(F("*6 System Test")); break;
    case 0xC3: stream->print(F("*1 Zone Bypass Programming")); break;
    case 0xC4: stream->print(F("*2 Trouble Menu")); break;
    case 0xC9: stream->print(F("*3 Alarm Memory Display")); break;
    case 0xCE: stream->print(F("*5 Programming, requires master code")); break;
    case 0xD0: stream->print(F("*6 Programming, requires master code")); break;
    case 0xD5: stream->print(F("Valid key, unknown function - 0x05: command output 1")); break;
    case 0xDA: stream->print(F("Reset")); break;
    case 0xDF: stream->print(F("Valid key, unknown function - 0xBB Byte 3: 0x00")); break;
    case 0xE1: stream->print(F("Exit")); break;
    case 0xE6: stream->print(F("Valid key, unknown function - long beep, 0x05: invalid option")); break;
    case 0xEB: stream->print(F("Valid key, unknown function - long beep")); break;
    case 0xEC: stream->print(F("Valid key, unknown function - long beep, 0x05 Byte 3: 0x0E unknown message")); break;
    case 0xF2: stream->print(F("Valid key, unknown function")); break;
    case 0xF7: stream->print(F("Left Arrow")); break;
    case 0xF8: stream->print(F("Valid key, unknown function")); break;
    case 0xFD: stream->print(F("Valid key, unknown function")); break;
    default: decodedKey = false; break;
  }
  if (decodedKey) return;

  stream->print(F("Slots active: "));
    if ((keypadData[2] & 0xC0) == 0) stream->print(F("1 "));
    if ((keypadData[2] & 0x30) == 0) stream->print(F("2 "));
    if ((keypadData[2] & 0x0C) == 0) stream->print(F("3 "));
    if ((keypadData[2] & 0x03) == 0) stream->print(F("4 "));
}


/*
 *  Keypad: panel 0x11 keypad slot query response for slots 5-8
 *
 *  11111111 1 11111111 11111100 11111111 11111111 11111111 [Keypad] Slot 8
 */
void dscKeybusInterface::printKeypad_0xFF_Byte3() {
  if (keypadData[2] == 0xFF) stream->print(F("Slots active: "));
    if ((keypadData[3] & 0xC0) == 0) stream->print(F("5 "));
    if ((keypadData[3] & 0x30) == 0) stream->print(F("6"));
    if ((keypadData[3] & 0x0C) == 0) stream->print(F("7 "));
    if ((keypadData[3] & 0x03) == 0) stream->print(F("8 "));
}


/*
 *  Keypad: Fire alarm
 *
 *  01110111 1 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Fire alarm
 */
void dscKeybusInterface::printKeypad_0x77() {
  stream->print(F("Fire alarm"));
}


/*
 *  Keypad: Auxiliary alarm
 *
 *  10111011 1 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Aux alarm
 */
void dscKeybusInterface::printKeypad_0xBB() {
  stream->print(F("Auxiliary alarm"));
}


/*
 *  Keypad: Panic alarm
 *
 *  11011101 1 11111111 11111111 11111111 11111111 11111111 11111111 [Keypad] Panic alarm
 */
void dscKeybusInterface::printKeypad_0xDD() {
  stream->print(F("Panic alarm"));
}


/*
 *  Print panel messages
 */

/*
 *  0x05: Status
 *  Interval: constant
 *  CRC: no
 *  Byte 4: TBD
 *  Byte 5: TBD
 *
 *  00000101 0 10000001 00000001 10010001 11000111 [0x05] Status lights: Ready Backlight | Partition ready
 *  00000101 0 10010000 00000011 10010001 11000111 [0x05] Status lights: Trouble Backlight | Partition not ready
 *  00000101 0 10001010 00000100 10010001 11000111 [0x05] Status lights: Armed Bypass Backlight | Armed stay
 *  00000101 0 10000010 00000101 10010001 11000111 [0x05] Status lights: Armed Backlight | Armed away
 *  00000101 0 10001011 00001000 10010001 11000111 [0x05] Status lights: Ready Armed Bypass Backlight | Exit delay in progress
 *  00000101 0 10000010 00001100 10010001 11000111 [0x05] Status lights: Armed Backlight | Entry delay in progress
 *  00000101 0 10000001 00010000 10010001 11000111 [0x05] Status lights: Ready Backlight | Keypad lockout
 *  00000101 0 10000010 00010001 10010001 11000111 [0x05] Status lights: Armed Backlight | Partition in alarm
 *  00000101 0 10000001 00110011 10010001 11000111 [0x05] Status lights: Ready Backlight | Partition busy
 *  00000101 0 10000001 00111110 10010001 11000111 [0x05] Status lights: Ready Backlight | Partition disarmed
 *  00000101 0 10000001 01000000 10010001 11000111 [0x05] Status lights: Ready Backlight | Keypad blanked
 *  00000101 0 10000001 10001111 10010001 11000111 [0x05] Status lights: Ready Backlight | Invalid access code
 *  00000101 0 10000000 10011110 10010001 11000111 [0x05] Status lights: Backlight | Quick armed pressed
 *  00000101 0 10000001 10100011 10010001 11000111 [0x05] Status lights: Ready Backlight | Door chime enabled
 *  00000101 0 10000001 10100100 10010001 11000111 [0x05] Status lights: Ready Backlight | Door chime disabled

 *  00000101 0 10000010 00001101 10010001 11000111 [0x05] Status lights: Armed Backlight   *  Delay zone tripped after previous alarm tripped
 *  00000101 0 10000000 00111101 10010001 11000111 [0x05] Status lights: Backlight   *  Disarmed after previous alarm tripped
 *  00000101 0 10000000 00100010 10010001 11000111 [0x05] Status lights: Backlight   *  Disarmed after previous alarm tripped +4s
 *  00000101 0 10000010 10100110 10010001 11000111 [0x05] Status lights: Armed Backlight   *  In *5 programming
 */
void dscKeybusInterface::printPanel_0x05() {
  stream->print(F("Status lights: "));
  if (panelData[2] == 0) stream->print(F("none "));
  else {
    if (bitRead(panelData[2],0)) stream->print(F("Ready "));
    if (bitRead(panelData[2],1)) stream->print(F("Armed "));
    if (bitRead(panelData[2],2)) stream->print(F("Memory "));
    if (bitRead(panelData[2],3)) stream->print(F("Bypass "));
    if (bitRead(panelData[2],4)) stream->print(F("Trouble "));
    if (bitRead(panelData[2],5)) stream->print(F("Program "));
    if (bitRead(panelData[2],6)) stream->print(F("Fire "));
    if (bitRead(panelData[2],7)) stream->print(F("Backlight "));
  }

  switch (panelData[3]) {
    case 0x01: stream->print(F("| Partition ready")); break;
    case 0x03: stream->print(F("| Partition not ready")); break;
    case 0x04: stream->print(F("| Armed stay")); break;
    case 0x05: stream->print(F("| Armed away")); break;
    case 0x07: stream->print(F("| Failed to arm")); break;
    case 0x08: stream->print(F("| Exit delay in progress")); break;
    case 0x09: stream->print(F("| Arming without entry delay")); break;
    case 0x0B: stream->print(F("| Quick exit in progress")); break;
    case 0x0C: stream->print(F("| Entry delay in progress")); break;
    case 0x10: stream->print(F("| Keypad lockout")); break;
    case 0x11: stream->print(F("| Partition in alarm")); break;
    case 0x14: stream->print(F("| Auto-arm in progress")); break;
    case 0x16: stream->print(F("| Armed without entry delay")); break;
    case 0x33: stream->print(F("| Partition busy")); break;
    case 0x3D: stream->print(F("| Disarmed after alarm in memory")); break;
    case 0x3E: stream->print(F("| Partition disarmed")); break;
    case 0x40: stream->print(F("| Keypad blanked")); break;
    case 0x8A: stream->print(F("| Activate stay/away zones")); break;
    case 0x8B: stream->print(F("| Quick exit")); break;
    case 0x8E: stream->print(F("| Invalid option")); break;
    case 0x8F: stream->print(F("| Invalid access code")); break;
    case 0x9E: stream->print(F("| *  pressed")); break;
    case 0x9F: stream->print(F("| Command output 1")); break;
    case 0xA1: stream->print(F("| *2: Trouble menu")); break;
    case 0xA2: stream->print(F("| *3: Alarm memory display")); break;
    case 0xA3: stream->print(F("| Door chime enabled")); break;
    case 0xA4: stream->print(F("| Door chime disabled")); break;
    case 0xA5: stream->print(F("| Enter master code")); break;
    case 0xA6: stream->print(F("| *5: Access codes")); break;
    case 0xA7: stream->print(F("| *5: Enter new code")); break;
    case 0xA9: stream->print(F("| *6: User functions")); break;
    case 0xAA: stream->print(F("| *6: Time and Date")); break;
    case 0xAB: stream->print(F("| *6: Auto-arm time")); break;
    case 0xAC: stream->print(F("| *6: Auto-arm enabled")); break;
    case 0xAD: stream->print(F("| *6: Auto-arm disabled")); break;
    case 0xAF: stream->print(F("| *6: System test")); break;
    case 0xB0: stream->print(F("| *6: Enable DLS")); break;
    case 0xB2: stream->print(F("| *7: Command output")); break;
    case 0xB7: stream->print(F("| Enter installer code")); break;
    case 0xB8: stream->print(F("| *  pressed while armed")); break;
    case 0xB9: stream->print(F("| *2: Zone tamper menu")); break;
    case 0xBA: stream->print(F("| *2: Zones with low batteries")); break;
    case 0xC6: stream->print(F("| *2: Zone fault menu")); break;
    case 0xC8: stream->print(F("| *2: Service required menu")); break;
    case 0xD0: stream->print(F("| *2: Handheld keypads with low batteries")); break;
    case 0xD1: stream->print(F("| *2: Wireless keys with low batteries")); break;
    case 0xE4: stream->print(F("| *8: Main menu")); break;
    case 0xEE: stream->print(F("| *8: Submenu")); break;
    default:
      stream->print(F("| Unrecognized command, byte 3: 0x"));
      if (panelData[3] < 10) stream->print(F("0"));
      stream->print(panelData[3], HEX);
      break;
  }
}


/*
 *  0x27: Status with zones
 *  Interval: 4m
 *  CRC: yes
 *  Byte 4: TBD
 *  Byte 5: TBD
 *
 *  00100111 0 10000001 00000001 10010001 11000111 00000000 00000001 [0x27] Status lights: Ready Backlight | Zones lights: none   *  Unarmed, zones closed
 *  00100111 0 10000001 00000001 10010001 11000111 00000010 00000011 [0x27] Status lights: Ready Backlight | Zones lights: 2   *  Unarmed, zone 2 open
 *  00100111 0 10001010 00000100 10010001 11000111 00000000 00001101 [0x27] Status lights: Armed Bypass Backlight | Zones lights: none   *  Armed stay   *  Periodic while armed
 *  00100111 0 10001010 00000100 11111111 11111111 00000000 10110011 [0x27] Status lights: Armed Bypass Backlight | Zones lights: none  /Armed stay +1s
 *  00100111 0 10000010 00000101 10010001 11000111 00000000 00000110 [0x27] Status lights: Armed Backlight | Zones lights: none   *  Armed away   *  Periodic while armed
 *  00100111 0 10000010 00000101 11111111 11111111 00000000 10101100 [0x27] Status lights: Armed Backlight | Zones lights: none   *  Armed away +1s
 *  00100111 0 10000010 00001100 10010001 11000111 00000001 00001110 [0x27] Status lights: Armed Backlight | Zones lights: 1   *  Delay zone 1 tripped, entrance delay
 *  00100111 0 10000010 00010001 10010001 11000111 00000001 00010011 [0x27] Status lights: Armed Backlight | Zones lights: 1   *  Periodic after delay zone 1 tripped, alarm on   *  Periodic after fire alarm, alarm on
 *  00100111 0 10000010 00001101 10010001 11000111 00000001 00001111 [0x27] Status lights: Armed Backlight | Zones lights: 1   *  Immediate after delay zone 1 tripped after previous alarm tripped
 *  00100111 0 10000010 00010001 11011011 11111111 00000010 10010110 [0x27] Status lights: Armed Backlight | Zones lights: 2   *  Instant zone 2 tripped away
 *  00100111 0 00000001 00000001 11111111 11111111 00000000 00100111 [0x27] Status lights: Ready | Open zones: none   *  Immediate after power on after panel reset
 *  00100111 0 10010001 00000001 11111111 11111111 00000000 10110111 [0x27] Status lights: Ready Trouble Backlight | Open zones: none   *  +15s after exit *8
 *  00100111 0 10010001 00000001 10100000 00000000 00000000 01011001 [0x27] Status lights: Ready Trouble Backlight | Open zones: none   *  +33s after power on after panel reset
 *  00100111 0 10010000 00000011 11111111 11111111 00111111 11110111 [0x27] Status lights: Trouble Backlight | Open zones: 1 2 3 4 5 6   *  +122s after power on after panel reset
 *  00100111 0 10010000 00000011 10010001 11000111 00111111 01010001 [0x27] Status lights: Trouble Backlight | Open zones: 1 2 3 4 5 6   *  +181s after power on after panel reset
 */
void dscKeybusInterface::printPanel_0x27() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Status lights: "));
  if (panelData[2] == 0) stream->print(F("none "));
  else {
    if (bitRead(panelData[2],0)) stream->print(F("Ready "));
    if (bitRead(panelData[2],1)) stream->print(F("Armed "));
    if (bitRead(panelData[2],2)) stream->print(F("Memory "));
    if (bitRead(panelData[2],3)) stream->print(F("Bypass "));
    if (bitRead(panelData[2],4)) stream->print(F("Trouble "));
    if (bitRead(panelData[2],5)) stream->print(F("Program "));
    if (bitRead(panelData[2],6)) stream->print(F("Fire "));
    if (bitRead(panelData[2],7)) stream->print(F("Backlight "));
  }

  switch (panelData[3]) {
    case 0x01: stream->print(F("| Partition ready ")); break;
    case 0x03: stream->print(F("| Partition not ready ")); break;
    case 0x04: stream->print(F("| Armed stay ")); break;
    case 0x05: stream->print(F("| Armed away ")); break;
    case 0x08: stream->print(F("| Exit delay in progress ")); break;
    case 0x0C: stream->print(F("| Entry delay in progress ")); break;
    case 0x10: stream->print(F("| Keypad lockout ")); break;
    case 0x11: stream->print(F("| Partition in alarm ")); break;
    case 0x33: stream->print(F("| Partition busy ")); break;
    case 0x3E: stream->print(F("| Partition disarmed ")); break;
    case 0x40: stream->print(F("| Keypad blanked ")); break;
    case 0x8F: stream->print(F("| Invalid access code ")); break;
  }

  stream->print(F("| Open zones: "));
  if (panelData[6] == 0) stream->print(F("none"));
  else {
    if (bitRead(panelData[6],0)) stream->print(F("1 "));
    if (bitRead(panelData[6],1)) stream->print(F("2 "));
    if (bitRead(panelData[6],2)) stream->print(F("3 "));
    if (bitRead(panelData[6],3)) stream->print(F("4 "));
    if (bitRead(panelData[6],4)) stream->print(F("5 "));
    if (bitRead(panelData[6],5)) stream->print(F("6 "));
    if (bitRead(panelData[6],6)) stream->print(F("7 "));
    if (bitRead(panelData[6],7)) stream->print(F("8 "));
  }
}


/*
 *  0x0A: Status in programming
 *  Interval: constant in *  programming
 *  CRC: yes
 *  Byte 3: matches 0x05 byte 3 and 0x27 byte 3, messages
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
  stream->print(F("Status lights: "));
  if (panelData[2] == 0) stream->print(F("none "));
  else {
    if (bitRead(panelData[2],0)) stream->print(F("Ready "));
    if (bitRead(panelData[2],1)) stream->print(F("Armed "));
    if (bitRead(panelData[2],2)) stream->print(F("Memory "));
    if (bitRead(panelData[2],3)) stream->print(F("Bypass "));
    if (bitRead(panelData[2],4)) stream->print(F("Trouble "));
    if (bitRead(panelData[2],5)) stream->print(F("Program "));
    if (bitRead(panelData[2],6)) stream->print(F("Fire "));
    if (bitRead(panelData[2],7)) stream->print(F("Backlight "));
  }

  bool zoneLights = false;
  stream->print(F("| Zone lights: "));
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

  if (panelData[8] != 0) {
    zoneLights = true;
    if (bitRead(panelData[8],0)) stream->print(F("33 "));
    if (bitRead(panelData[8],1)) stream->print(F("34 "));
    if (bitRead(panelData[8],3)) stream->print(F("41 "));
    if (bitRead(panelData[8],4)) stream->print(F("42 "));
  }

  if (!zoneLights) stream->print(F("none"));
}


/*
 *  0x11: Keypad query (possible general keybus query)
 *  Interval: 30s
 *  CRC: no
 *
 *  00010001 0 10101010 10101010 10101010 10101010 10101010 [0x11] Keypad slot query
 *  11111111 1 00111111 11111111 11111111 11111111 11111111 [Keypad] Slot 1
 *  11111111 1 11111111 11111100 11111111 11111111 11111111 [Keypad] Slot 8
 */
void dscKeybusInterface::printPanel_0x11() {
  stream->print(F("Keypad slot query"));
}


/*
 *  0x16: Zone wiring
 *  Interval: 4min
 *  CRC: yes
 *  Byte 2: TBD
 *  Byte 3: TBD
 *  Byte 4 bits 2-7: TBD
 *
 *  00010110 0 00001110 00100011 11010001 00011001 [0x16] Zone wiring: NC | Exit *8 programming
 *  00010110 0 00001110 00100011 11010010 00011001 [0x16] Zone wiring: EOL | Exit *8 programming
 *  00010110 0 00001110 00100011 11010011 00011001 [0x16] Zone wiring: DEOL | Exit *8 programming
 *  00010110 0 00001110 00100011 11100001 00101000 [0x16] Zone wiring: NC | In *8
 *  00010110 0 00001110 00100011 11100110 00101101 [0x16] Zone wiring: EOL | Enter *8 programming
 *  00010110 0 00001110 00100011 11110010 00111001 [0x16] Zone wiring: EOL | Armed, Exit *8 +15s, Power-on +2m
 *  00010110 0 00001110 00100011 11110111 00111101 [0x16] Zone wiring: DEOL | Interval 4m
 */
void dscKeybusInterface::printPanel_0x16() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }
  if (panelData[2] == 0x0E && panelData[3] == 0x23) {
    switch (panelData[4] & 0x03) {
      case 0x01: stream->print(F("Zone wiring: NC ")); break;
      case 0x02: stream->print(F("Zone wiring: EOL ")); break;
      case 0x03: stream->print(F("Zone wiring: DEOL ")); break;
    }

    switch (panelData[4] >> 2) {
      case 0x34: stream->print(F("| Exit *8 programming")); break;
      case 0x39: stream->print(F("| *8 programming")); break;
      case 0x3C: stream->print(F("| Armed, Exit *8 +15s, Power-on +2m")); break;
      case 0x3D: stream->print(F("| Interval 4m")); break;
      default: stream->print(F("Unrecognized command: Add to 0x16")); break;
    }
  }
  else stream->print(F("Unrecognized command: Add to 0x16"));
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
 *  0x58: ???
 *  Interval: immediate after power on after panel reset
 *  CRC: no
 *
 *  01011000 0 10101010 10101010 10101010 10101010 [0x58] Unknown command: Power-on +0s
 */
void dscKeybusInterface::printPanel_0x58() {
  stream->print(F("Unknown command: Power-on +0s"));
}


/*
 *  0x5D: Flash panel lights, status and zones 1-32
 *  Interval: 30s
 *  CRC: yes
 *
 *  01011101 0 00000000 00000000 00000000 00000000 00000000 01011101 [0x5D] Flashing lights: none
 *  01011101 0 00100000 00000000 00000000 00000000 00000000 01111101 [0x5D] Flashing: Program
 *  01011101 0 00000000 00100000 00000000 00000000 00000000 01111101 [0x5D] Flashing zones: 6
 *  01011101 0 00000100 00100000 00000000 00000000 00000000 10000001 [0x5D] Flashing: Memory | Zones: 6
 */
void dscKeybusInterface::printPanel_0x5D() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }

  stream->print(F("Flashing status: "));
  if (panelData[2] != 0) {
    if (bitRead(panelData[2],0)) stream->print(F("Ready "));
    if (bitRead(panelData[2],1)) stream->print(F("Armed "));
    if (bitRead(panelData[2],2)) stream->print(F("Memory "));
    if (bitRead(panelData[2],3)) stream->print(F("Bypass "));
    if (bitRead(panelData[2],4)) stream->print(F("Trouble "));
    if (bitRead(panelData[2],5)) stream->print(F("Program "));
    if (bitRead(panelData[2],6)) stream->print(F("Fire "));
    if (bitRead(panelData[2],7)) stream->print(F("Backlight "));
  }
  else stream->print(F("none "));

  bool zoneLights = false;
  if (panelData[2] != 0) stream->print(F("| Zones: "));
  else stream->print(F("| Flashing zones: "));
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
 *  0x64: Beep - one-time
 *  CRC: yes
 *
 *  01100100 0 00001100 01110000 [0x64] Beep: 6 beeps
 */
void dscKeybusInterface::printPanel_0x64() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }
  stream->print(F("Beep: "));
  switch (panelData[2]) {
    case 0x04: stream->print(F("2 beeps")); break;
    case 0x06: stream->print(F("3 beeps")); break;
    case 0x08: stream->print(F("4 beeps")); break;
    case 0x0C: stream->print(F("6 beeps")); break;
    default: stream->print(F("Unrecognized command: Add to 0x64")); break;
  }
}


/*
 *  0x75: Beep - repeated
 *  CRC: yes
 *
 *  01110101 0 10000000 11110101 [0x75] Beep pattern: solid tone
 *  01110101 0 00000000 01110101 [0x75] Beep pattern: off
 */
void dscKeybusInterface::printPanel_0x75() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }
  stream->print(F("Beep pattern: "));
  switch (panelData[2]) {
    case 0x00: stream->print(F("off")); break;
    case 0x11: stream->print(F("single beep (exit delay)")); break;
    case 0x31: stream->print(F("triple beep (exit delay)")); break;
    case 0x80: stream->print(F("solid tone")); break;
    case 0xB1: stream->print(F("triple beep (entrance delay)")); break;
    default: stream->print(F("Unrecognized command: Add to 0x75")); break;
  }
}


/*
 *  0x7F: Beep - one-time
 *  CRC: yes
 *
 *  01111111 0 00000001 10000000 [0x7F] Beep: long beep
 */
void dscKeybusInterface::printPanel_0x7F() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }
  switch (panelData[2]) {
    case 0x01: stream->print(F("Beep: long beep")); break;
    case 0x02: stream->print(F("Beep: long beep | Failed to arm")); break;
    default: stream->print(F("Unrecognized command: Add to 0x7F")); break;
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
 */
void dscKeybusInterface::printPanel_0x87() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }
  stream->print(F("Panel output:"));
  switch (panelData[2]) {
    case 0x00: stream->print(F(" Bell off")); break;
    case 0xFF: stream->print(F(" Bell on")); break;
    default: stream->print(F("Unrecognized command: Add to 0x87")); break;
  }

  if ((panelData[3] & 0x0F) <= 0x03) {
    if (bitRead(panelData[3],0)) stream->print(F(" | PGM1 on"));
    else stream->print(F(" | PGM1 off"));

    if (bitRead(panelData[3],1)) stream->print(F(" | PGM2 on"));
    else stream->print(F(" | PGM2 off"));
  }
  else stream->print(F("Unrecognized command: Add to 0x87"));
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
    default: stream->print(F("Unrecognized command, add to 0x94"));
  }
}


/*
 *  0xA5: Date, time, system status messages
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
   *
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
  if (dscMonth < 10) stream->print("0");
  stream->print(dscMonth);
  stream->print(F("/"));
  if (dscDay < 10) stream->print("0");
  stream->print(dscDay);
  stream->print(F("/20"));
  stream->print(dscYear3);
  stream->print(dscYear4);
  stream->print(" ");
  if (dscHour < 10) stream->print("0");
  stream->print(dscHour);
  stream->print(F(":"));
  if (dscMinute < 10) stream->print("0");
  stream->print(dscMinute);

  if (panelData[6] == 0 && panelData[7] == 0) {
    stream->print(F(" | Timestamp"));
    return;
  }

  if (panelData[7] == 0xFF) printPanel_0xA5_Byte7_0xFF();
  if (panelData[7] == 0) printPanel_0xA5_Byte7_0x00();
}


void dscKeybusInterface::printPanel_0xA5_Byte7_0xFF() {
  bool decodeComplete = true;

  // Process data separately based on byte 5 bits 0 and 1
  switch (panelData[5] & 0x03) {

    // Byte 5: xxxxxx00
    case 0x00: {
      switch (panelData[6]) {

          /*
           *  10100101 0 00011000 01001111 10110000 11101100 01001001 11111111 11110000 [0xA5] 03/29/2018 16:59 | Duress alarm
           *  10100101 0 00011000 01001111 11001110 10111100 01001010 11111111 11011111 [0xA5] 03/30/2018 14:47 | Disarmed after alarm in memory
           *  10100101 0 00011000 01001111 11001010 01000100 01001011 11111111 01100100 [0xA5] 03/30/2018 10:17 | Partition in alarm
           *  10100101 0 00011000 01010000 01001001 10111000 01001100 11111111 01011001 [0xA5] 04/02/2018 09:46 | Zone expander supervisory alarm
           *  10100101 0 00011000 01010000 01001010 00000000 01001101 11111111 10100011 [0xA5] 04/02/2018 10:00 | Zone expander supervisory restored
           *  10100101 0 00011000 01001111 01110010 10011100 01001110 11111111 01100111 [0xA5] 03/27/2018 18:39 | Keypad Fire alarm
           *  10100101 0 00011000 01001111 01110010 10010000 01001111 11111111 01011100 [0xA5] 03/27/2018 18:36 | Keypad Aux alarm
           *  10100101 0 00011000 01001111 01110010 10001000 01010000 11111111 01010101 [0xA5] 03/27/2018 18:34 | Keypad Panic alarm
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
          case 0x49: stream->print(F(" | Duress alarm")); break;
          case 0x4A: stream->print(F(" | Disarmed after alarm in memory")); break;
          case 0x4B: stream->print(F(" | Partition in alarm")); break;
          case 0x4C: stream->print(F(" | Zone expander supervisory alarm")); break;
          case 0x4D: stream->print(F(" | Zone expander supervisory restored")); break;
          case 0x4E: stream->print(F(" | Keypad Fire alarm")); break;
          case 0x4F: stream->print(F(" | Keypad Aux alarm")); break;
          case 0x50: stream->print(F(" | Keypad Panic alarm")); break;
          case 0x51: stream->print(F(" | Keypad status check?")); break;
          case 0x52: stream->print(F(" | Keypad Fire alarm restored")); break;
          case 0x53: stream->print(F(" | Keypad Aux alarm restored")); break;
          case 0x54: stream->print(F(" | Keypad Panic alarm restored")); break;
          case 0x98: stream->print(F(" | Keypad lockout")); break;
          case 0xBE: stream->print(F(" | Armed partial: Zones bypassed")); break;
          case 0xBF: stream->print(F(" | Armed special: quick-arm/auto-arm/keyswitch/wireless key/DLS")); break;
          case 0xE5: stream->print(F(" | Auto-arm cancelled")); break;
          case 0xE6: stream->print(F(" | Disarmed special: keyswitch/wireless key/DLS")); break;
          case 0xE7: stream->print(F(" | Panel battery trouble")); break;
          case 0xE8: stream->print(F(" | Panel AC power failure")); break;
          case 0xE9: stream->print(F(" | Bell trouble")); break;
          case 0xEA: stream->print(F(" | Power on +16s")); break;
          case 0xEC: stream->print(F(" | Telephone line trouble")); break;
          case 0xEF: stream->print(F(" | Panel battery restored")); break;
          case 0xF0: stream->print(F(" | Panel AC power restored")); break;
          case 0xF1: stream->print(F(" | Bell restored")); break;
          case 0xF4: stream->print(F(" | Telephone line restored")); break;
          case 0xFF: stream->print(F(" | System test")); break;
          default: decodeComplete = false;
      }

      if (decodeComplete) break;

      /*
       *  Zone alarm, zones 1-32
       *
       *  10100101 0 00011000 01001111 01001001 11011000 00001001 11111111 00110101 [0xA5] 03/26/2018 09:54 | Zone alarm: 1
       *  10100101 0 00011000 01001111 01001010 00100000 00001110 11111111 10000011 [0xA5] 03/26/2018 10:08 | Zone alarm: 6
       *  10100101 0 00011000 01001111 10010100 11001000 00010000 11111111 01110111 [0xA5] 03/28/2018 20:50 | Zone alarm: 8
       */
      if (panelData[6] >= 0x09 && panelData[6] <= 0x28) {
        stream->print(F(" | Zone alarm: "));
        stream->print(panelData[6] - 0x08);
        decodeComplete = true;
        break;
      }

      /*
       *  Zone alarm restored, zones 1-32
       *
       *  10100101 0 00011000 01001111 10010100 11001100 00101001 11111111 10010100 [0xA5] 03/28/2018 20:51 | Zone alarm restored: 1
       *  10100101 0 00011000 01001111 10010100 11010100 00101110 11111111 10100001 [0xA5] 03/28/2018 20:53 | Zone alarm restored: 6
       *  10100101 0 00011000 01001111 10010100 11010000 00110000 11111111 10011111 [0xA5] 03/28/2018 20:52 | Zone alarm restored: 8
       */
      if (panelData[6] >= 0x29 && panelData[6] <= 0x48) {
        stream->print(F(" | Zone alarm restored: "));
        stream->print(panelData[6] - 0x28);
        decodeComplete = true;
        break;
      }

      /*
       *  Zone tamper, zones 1-32
       *
       *  10100101 0 00000001 01000100 00100010 01011100 01010110 11111111 10111101 [0xA5] 01/01/2001 02:23 | Zone tamper: 1
       *  10100101 0 00000001 01000100 00100010 01011100 01010111 11111111 10111101 [0xA5] 01/01/2001 02:23 | Zone tamper: 2
       *  10100101 0 00010001 01101101 01101011 10010000 01011011 11111111 01111000 [0xA5] 11/11/2011 11:36 | Zone tamper: 6
       */
      if (panelData[6] >= 0x56 && panelData[6] <= 0x75) {
        stream->print(F(" | Zone tamper: "));
        stream->print(panelData[6] - 0x55);
        decodeComplete = true;
        break;
      }

      /*
       *  Zone tamper restored, zones 1-32
       *
       *  10100101 0 00000001 01000100 00100010 01011100 01110110 11111111 11011101 [0xA5] 01/01/2001 02:23 | Zone tamper restored: 1
       *  10100101 0 00000001 01000100 00100010 01011100 01111000 11111111 11011101 [0xA5] 01/01/2001 02:23 | Zone tamper restored: 2
       *  10100101 0 00010001 01101101 01101011 10010000 01111011 11111111 10011000 [0xA5] 11/11/2011 11:36 | Zone tamper restored: 6
       */
      if (panelData[6] >= 0x76 && panelData[6] <= 0x95) {
        stream->print(F(" | Zone tamper restored: "));
        stream->print(panelData[6] - 0x75);
        decodeComplete = true;
        break;
      }

      /*
       *  Armed by access code
       *
       *  10100101 0 00011000 01001101 00001000 10010000 10011001 11111111 00111010 [0xA5] 03/08/2018 08:36 | Armed by user code 1
       *  10100101 0 00011000 01001101 00001000 10111100 10111011 11111111 10001000 [0xA5] 03/08/2018 08:47 | Armed by master code 40
       */
      if (panelData[6] >= 0x99 && panelData[6] <= 0xBD) {
        byte dscCode = panelData[6] - 0x98;
        if (dscCode >= 35) dscCode += 5;
        stream->print(F(" | Armed by "));
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
        decodeComplete = true;
        break;
      }

      /*
       *  Disarmed by access code
       *
       *  10100101 0 00011000 01001101 00001000 11101100 11000000 11111111 10111101 [0xA5] 03/08/2018 08:59 | Disarmed by user code 1
       *  10100101 0 00011000 01001101 00001000 10110100 11100010 11111111 10100111 [0xA5] 03/08/2018 08:45 | Disarmed by master code 40
       */
      if (panelData[6] >= 0xC0 && panelData[6] <= 0xE4) {
        byte dscCode = panelData[6] - 0xBF;
        if (dscCode >= 35) dscCode += 5;
        stream->print(F(" | Disarmed by "));
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
        decodeComplete = true;
        break;
      }

      decodeComplete = false;
      break;
    }

    // Byte 5: xxxxxx01
    case 0x01: {
      /*
       *  10100101 0 00011000 01001111 11001010 10001001 00000011 11111111 01100001 [0xA5] 03/30/2018 10:34 | Cross zone alarm
       *  10100101 0 00010001 01101101 01101010 00000001 00000100 11111111 10010001 [0xA5] 11/11/2011 10:00 | Delinquency alarm
       */
      switch (panelData[6]) {
        case 0x03: stream->print(F(" | Cross zone alarm")); break;
        case 0x04: stream->print(F(" | Delinquency alarm")); break;
        default: decodeComplete = false;
      }
      if (decodeComplete) break;

      /*
       *  Zone fault restored, zones 1-32
       *
       *  10100101 0 00010001 01101101 01101011 01000001 01101100 11111111 00111010 [0xA5] 11/11/2011 11:16 | Zone fault restored: 1
       *  10100101 0 00010001 01101101 01101011 01010101 01101101 11111111 01001111 [0xA5] 11/11/2011 11:21 | Zone fault restored: 2
       *  10100101 0 00010001 01101101 01101011 10000101 01101111 11111111 10000001 [0xA5] 11/11/2011 11:33 | Zone fault restored: 4
       *  10100101 0 00010001 01101101 01101011 10001001 01110000 11111111 10000110 [0xA5] 11/11/2011 11:34 | Zone fault restored: 5
       */
      if (panelData[6] >= 0x6C && panelData[6] <= 0x8B) {
        stream->print(F(" | Zone fault restored: "));
        stream->print(panelData[6] - 0x6B);
        decodeComplete = true;
        break;
      }

      /*
       *  Zone fault, zones 1-32
       *
       *  10100101 0 00010001 01101101 01101011 00111101 10001100 11111111 01010110 [0xA5] 11/11/2011 11:15 | Zone fault: 1
       *  10100101 0 00010001 01101101 01101011 01010101 10001101 11111111 01101111 [0xA5] 11/11/2011 11:21 | Zone fault: 2
       *  10100101 0 00010001 01101101 01101011 10000001 10001111 11111111 10011101 [0xA5] 11/11/2011 11:32 | Zone fault: 3
       *  10100101 0 00010001 01101101 01101011 10001001 10010000 11111111 10100110 [0xA5] 11/11/2011 11:34 | Zone fault: 4
       */
      if (panelData[6] >= 0x8C && panelData[6] <= 0xAB) {
        stream->print(F(" | Zone fault: "));
        stream->print(panelData[6] - 0x8B);
        decodeComplete = true;
        break;
      }

      decodeComplete = false;
      break;
    }

    // Byte 5: xxxxxx10
    case 0x02: {

      /*
       *  Supervisory trouble, keypad slots 1-8
       *
       *  10100101 0 00010001 01101101 01110100 10000110 11110001 11111111 00001101 [0xA5] 11/11/2011 20:33 | Supervisory - module trouble: Keypad slot 1
       *  10100101 0 00010001 01101101 01110100 00101110 11111000 11111111 10111100 [0xA5] 11/11/2011 20:11 | Supervisory - module trouble: Keypad slot 8
       */
      if (panelData[6] >= 0xF1 && panelData[6] <= 0xF8) {
        stream->print(F(" | Supervisory - module trouble: Keypad slot "));
        stream->print(panelData[6] - 0xF0);
        decodeComplete = true;
        break;
      }

      /*
       *  Supervisory restored, keypad slots 1-8
       *
       *  10100101 0 00010001 01101101 01110100 10001110 11101001 11111111 00001101 [0xA5] 11/11/2011 20:35 | Supervisory - module detected: Keypad slot 1
       *  10100101 0 00010001 01101101 01110100 00110010 11110000 11111111 10111000 [0xA5] 11/11/2011 20:12 | Supervisory - module detected: Keypad slot 8
       */
      if (panelData[6] >= 0xE9 && panelData[6] <= 0xF0) {
        stream->print(F(" | Supervisory - module detected: Keypad slot "));
        stream->print(panelData[6] - 0xE8);
        decodeComplete = true;
        break;
      }

      decodeComplete = false;
      break;
    }

    // Byte 5: xxxxxx11
    case 0x03: {
      decodeComplete = false;
      break;
    }
  }

  if (decodeComplete) return;
  else {
    stream->print(F(" | Unrecognized data, add to 0xA5_Byte7_0xFF, Byte 6: 0x"));
    if (panelData[6] < 10) stream->print(F("0"));
    stream->print(panelData[6], HEX);
  }
}


void dscKeybusInterface::printPanel_0xA5_Byte7_0x00() {
  bool decodeComplete = true;

  // Process data based on byte 5, bits 0 and 1
  switch (panelData[5] & 0x03) {

    // Byte 5: xxxxxx00
    case 0x00: {
      decodeComplete = false;
      break;
    }

    // Byte 5: xxxxxx01
    case 0x01: {
      switch (panelData[6]) {

        /*
         *  10100101 0 00010001 01101101 01100000 10101001 00100100 00000000 01010000 [0xA5] 11/11/2011 00:42 | Auto-arm cancelled by duress code 33
         *  10100101 0 00010001 01101101 01100000 10110101 00100101 00000000 01011101 [0xA5] 11/11/2011 00:45 | Auto-arm cancelled by duress code 34
         *  10100101 0 00010001 01101101 01100000 00101001 00100110 00000000 11010010 [0xA5] 11/11/2011 00:10 | Auto-arm cancelled by master code 40
         *  10100101 0 00010001 01101101 01100000 10010001 00100111 00000000 00111011 [0xA5] 11/11/2011 00:36 | Auto-arm cancelled by supervisor code 41
         *  10100101 0 00010001 01101101 01100000 10111001 00101000 00000000 01100100 [0xA5] 11/11/2011 00:46 | Auto-arm cancelled by supervisor code 42
         *  10100101 0 00011000 01001111 10100000 10011101 00101011 00000000 01110100 [0xA5] 03/29/2018 00:39 | Armed by auto-arm
         *  10100101 0 00011000 01001101 00001010 00001101 10101100 00000000 11001101 [0xA5] 03/08/2018 10:03 | Exit *8 programming
         *  10100101 0 00011000 01001101 00001001 11100001 10101101 00000000 10100001 [0xA5] 03/08/2018 09:56 | Enter *8
         *  10100101 0 00010001 01101101 01100010 11001101 11010000 00000000 00100010 [0xA5] 11/11/2011 02:51 | Command output 4
         */
        case 0x24: stream->print(F(" | Auto-arm cancelled by duress code 33")); break;
        case 0x25: stream->print(F(" | Auto-arm cancelled by duress code 34")); break;
        case 0x26: stream->print(F(" | Auto-arm cancelled by master code 40")); break;
        case 0x27: stream->print(F(" | Auto-arm cancelled by supervisor code 41")); break;
        case 0x28: stream->print(F(" | Auto-arm cancelled by supervisor code 42")); break;
        case 0x2B: stream->print(F(" | Armed by auto-arm")); break;
        case 0xAC: stream->print(F(" | Exit *8 programming")); break;
        case 0xAD: stream->print(F(" | Enter *8 programming")); break;
        //  0xB0 - 0xCF: Zones bypassed
        case 0xD0: stream->print(F(" | Command output 4")); break;
        default: decodeComplete = false;
      }
      if (decodeComplete) break;

      /*
       *  Zones bypassed
       *
       *  10100101 0 00011000 01001111 10110001 10101001 10110001 00000000 00010111 [0xA5] 03/29/2018 17:42 | Bypassed zone 2
       *  10100101 0 00011000 01001111 10110001 11000001 10110101 00000000 00110011 [0xA5] 03/29/2018 17:48 | Bypassed zone 6
       */
      if (panelData[6] >= 0xB0 && panelData[6] <= 0xCF) {
        stream->print(F(" | Zone bypassed: "));
        stream->print(panelData[6] - 0xAF);
        decodeComplete = true;
        break;
      }

      decodeComplete = false;
      break;
    }

    // Byte 5: xxxxxx10
    case 0x02: {
      switch (panelData[6]) {

        /*
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
         *  10100101 0 00011000 01001111 00101101 10011010 10011100 00000000 01101111 [0xA5] 03/25/2018 13:38 | Armed without entry delay
         *  10100101 0 00011000 01001111 00101100 11011110 11000011 00000000 11011001 [0xA5] 03/25/2018 12:55 | Enter *5 programming
         *  10100101 0 00011000 01001111 00101110 00000010 11100110 00000000 00100010 [0xA5] 03/25/2018 14:00 | Enter *6 programming
         */
        case 0x2A: stream->print(F(" | Quick exit")); break;
        case 0x63: stream->print(F(" | Keybus fault restored")); break;
        case 0x66: stream->print(F(" | Enter *1 zone bypass programming")); break;
        case 0x67: stream->print(F(" | Command output 1")); break;
        case 0x68: stream->print(F(" | Command output 2")); break;
        case 0x69: stream->print(F(" | Command output 3")); break;
        case 0x8C: stream->print(F(" | Loss of system time")); break;
        case 0x8D: stream->print(F(" | Power on")); break;
        case 0x8E: stream->print(F(" | Panel factory default")); break;
        case 0x93: stream->print(F(" | Disarmed by keyswitch")); break;
        case 0x96: stream->print(F(" | Armed by keyswitch")); break;
        case 0x97: stream->print(F(" | Armed by keypad away")); break;
        case 0x98: stream->print(F(" | Armed by quick-arm")); break;
        case 0x99: stream->print(F(" | Activate stay/away zones")); break;
        case 0x9A: stream->print(F(" | Armed: stay")); break;
        case 0x9B: stream->print(F(" | Armed: away")); break;
        case 0x9C: stream->print(F(" | Armed without entry delay")); break;
        case 0xC3: stream->print(F(" | Enter *5 programming")); break;
        case 0xE6: stream->print(F(" | Enter *6 programming")); break;
        default: decodeComplete = false;
      }
      if (decodeComplete) break;

      /*
       *  Auto-arm cancelled
       *
       *  10100101 0 00010001 01101101 01100000 00111110 11000110 00000000 10000111 [0xA5] 11/11/2011 00:15 | Auto-arm cancelled by user code 1
       *  10100101 0 00010001 01101101 01100000 01111010 11100101 00000000 11100010 [0xA5] 11/11/2011 00:30 | Auto-arm cancelled by user code 32
       */
      if (panelData[6] >= 0xC6 && panelData[6] <= 0xE5) {
        stream->print(F(" | Auto-arm cancelled by user code "));
        stream->print(panelData[6] - 0xC5);
        decodeComplete = true;
        break;
      }

      break;
    }

    // Byte 5: xxxxxx11
    case 0x03: {
      decodeComplete = false;
      break;
    }
  }
  if (decodeComplete) return;
  stream->print(F(" | Unrecognized data, add to 0xA5_Byte7_0x00, Byte 6: 0x"));
  if (panelData[6] < 10) stream->print(F("0"));
  stream->print(panelData[6], HEX);
}


/*
 *  0xB1: Enabled zones 1-32
 *  Configuration: *8 [202]-[205]
 *  Interval: 4m
 *  CRC: yes
 *
 *  10110001 0 11111111 00000000 00000000 00000000 00000000 00000000 00000000 00000000 10110000 [0xB1] Enabled zones: 1 2 3 4 5 6 7 8
 *  10110001 0 10010001 10001010 01000001 10100100 00000000 00000000 00000000 00000000 10110001 [0xB1] Enabled zones: 1 5 8 10 12 16 17 23 27 30 32
 *  10110001 0 11111111 00000000 00000000 11111111 00000000 00000000 00000000 00000000 10101111 [0xB1] Enabled zones: 1 2 3 4 5 6 7 8 25 26 27 28 29 30 31 32
 */
void dscKeybusInterface::printPanel_0xB1() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }
  bool enabledZones = false;
  stream->print(F("Enabled zones: "));
  for (byte panelByte = 2; panelByte < 10; panelByte++) {
    if (panelData[panelByte] != 0) {
      enabledZones = true;
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(panelData[panelByte],zoneBit)) {
          stream->print((zoneBit + 1) + ((panelByte-2) *  8));
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
 *  0xC3: ???
 *  CRC: yes
 *
 *  11000011 0 00010000 11111111 11010010 [0xC3] Unknown command 1: Power-on +33s
 *  11000011 0 00110000 11111111 11110010 [0xC3] Keypad lockout
 */
void dscKeybusInterface::printPanel_0xC3() {
  if (!validCRC()) {
    stream->print(F("[CRC Error]"));
    return;
  }
  if (panelData[3] == 0xFF) {
    switch (panelData[2]) {
      case 0x10: stream->print(F("Unknown command 1: Power-on +33s")); break;
      case 0x30: stream->print(F("Keypad lockout")); break;
      default: stream->print(F("Unrecognized data, add to 0xC3")); break;
    }
  }
  else stream->print(F("Unrecognized data, add to 0xC3"));
}


/*
 *  0xD5: Keypad zone query
 *  CRC: no
 *
 *  11111111 1 11111111 11111111 11111111 11111011 [Keypad]
 *  11010101 0 10101010 10101010 10101010 10101010 10101010 10101010 10101010 10101010 [0xD5] Keypad zone query
 *  11111111 1 11111111 11111111 11111111 11111111 11111111 11111111 11111111 00001111 [Keypad] Slot 8
 */
void dscKeybusInterface::printPanel_0xD5() {
  stream->print(F("Keypad zone query"));
}
