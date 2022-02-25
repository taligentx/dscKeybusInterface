/*
 *  DSC Keypad Interface 1.4 (esp32)
 *
 *  Emulates a DSC panel to directly interface DSC PowerSeries or Classic series keypads as physical
 *  input devices for any general purpose, without needing a DSC panel.
 *
 *  PowerSeries keypad features:
 *    - Read keypad key button presses, including fire/aux/panic alarm keys: dsc.key
 *    - Set keypad lights: Ready, Armed, Trouble, Memory, Bypass, Fire, Program, Backlight, Zones 1-8: dsc.lightReady, dsc.lightZone1, etc
 *    - Set keypad beeps, 1-128: dsc.beep(3)
 *    - Set keypad buzzer in seconds, 1-255: dsc.tone(5)
 *    - Set keypad tone pattern with a number of beeps, an optional constant tone, and the interval in seconds between beeps:
 *        2 beeps, no constant tone, 4 second interval: dsc.tone(2, false, 4)
 *        3 beeps, constant tone, 2 second interval: dsc.tone(3, true, 2)
 *        Disable the tone: dsc.tone() or dsc.tone(0, false, 0)
 *    - Set LCD keypad messages (on cmd 0x05/byte3) with entering HEX input into serial console:
 *        According to printPanelMessages in dscKeybusPrintData.cpp, through it doesn't seem to fully match
 *        Change Function keys 1-5 with entering: 0x70 - 0x74
 *        Change LCD keypad time by entering: 0x2A (slight delay before LCD will show to input time data)
 *        Change LCD Brightness/contrast/buzzer level by entering: 0x29 and scrolling to desired setting then pressing (*)
 *
 *  Classic keypad features:
 *    - Read keypad key button presses, including fire/aux/panic alarm keys: dsc.key
 *    - Set keypad lights: Ready, Armed, Trouble, Memory, Bypass, Fire, Program, Zones 1-8: dsc.lightReady, dsc.lightZone1, etc
 *
 *  This interface uses a different wiring setup from the standard Keybus interface, adding
 *  an NPN transistor on dscClockPin.  The DSC keypads require a 12v DC power source, though
 *  lower voltages down to 7v may work for key presses (the LEDs will be dim).
 *
 *  Release notes:
 *    1.4 - Added ability to change LCD keypad messages
 *    1.3 - Add Classic keypad support - PC2550RK
 *    1.2 - Add Classic keypad support - PC1500RK
 *    1.1 - Add keypad beep, buzzer, constant tone
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Keypad R --- 12v DC
 *
 *      DSC Keypad B --- esp32 ground
 *
 *      DSC Keypad Y ---+--- 1k ohm resistor --- 12v DC
 *                      |
 *                      +--- NPN collector --\
 *                                            |-- NPN base --- 1k ohm resistor --- dscClockPin  // esp32: 18
 *                  Ground --- NPN emitter --/
 *
 *      DSC Keypad G ---+--- 1k ohm resistor --- 12v DC
 *                      |
 *                      +--- 33k ohm resistor ---+--- dscReadPin  // esp32: 19
 *                      |                        |
 *                      |                        +--- 10k ohm resistor --- Ground
 *                      |
 *                      +--- NPN collector --\
 *                                            |-- NPN base --- 1k ohm resistor --- dscWritePin  // esp32: 21
 *                  Ground --- NPN emitter --/
 *
 *  The keypad interface uses NPN transistors to pull the clock and data lines low - most small
 *  signal NPN transistors should be suitable, for example:
 *    - 2N3904
 *    - BC547, BC548, BC549
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  This example code is in the public domain.
 */

// Set the keypad type
#define dscKeypad
//#define dscClassicKeypad

#include <dscKeybusInterface.h>

// Configures the Keybus interface with the specified pins
#define dscClockPin 18  // 4,13,16-39
#define dscReadPin  19  // 4,13,16-39
#define dscWritePin 21  // 4,13,16-33

// Initialize components
#ifdef dscKeypad
dscKeypadInterface dsc(dscClockPin, dscReadPin, dscWritePin);
#else
dscClassicKeypadInterface dsc(dscClockPin, dscReadPin, dscWritePin);
#endif
bool lightOff, lightBlink, inputReceived;
const byte inputLimit = 50;
char input[inputLimit];
byte beepLength, buzzerLength, toneLength;


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  Serial.print(F("Keybus...."));
  dsc.begin();
  Serial.println(F("connected."));
  Serial.println(F("DSC Keypad Interface is online."));
}

void loop() {

  inputSerial();  // Stores Serial data in input[], requires a newline character (NL, CR, or both)

  /*
   *  Sets keypad status via serial with the listed keys. Light status uses custom
   *  values for control: off, on, blink (example: dsc.lightReady = blink;)
   *
   *  Light on: Send the keys listed below.  Turning on the armed light: "a"
   *  Light off: Send "-" before a light key to turn it off.  Turning off the zone 4 light: "-4"
   *  Light blink: Send "!" before a light key to blink.  Blinking the ready light: "!r"
   *  Beep: Send "b" followed by the number of beeps, 1-128.  Setting 2 beeps: "b2"
   *  Buzzer: Send "z" followed by the buzzer length in seconds, 1-255.  Setting the buzzer to 5 seconds: "z5"
   *  Tone pattern: Send "n" followed by the number of beeps 1-7, constant tone true "t" or false "f", interval between beeps 1-15s
   *        Setting a tone pattern with 2 beeps, no constant tone, 4 second interval: "n2f4"
   *        Setting a tone pattern with 3 beeps, constant tone, 2 second interval: "n3t2"
   *        Disabling the tone pattern: "n"
   */
  if (inputReceived) {
    inputReceived = false;

    #if defined(dscKeypad)
    if (String(input).startsWith("0x")) dsc.panelCommand05[2] = strtoul(input, NULL, 16);
    else {
      for (byte i = 0; i < strlen(input); i++) {
        switch (input[i]) {
          case 'r': case 'R': dsc.lightReady = setLight(); break;
          case 'a': case 'A': dsc.lightArmed = setLight(); break;
          case 'm': case 'M': dsc.lightMemory = setLight(); break;
          case 'y': case 'Y': dsc.lightBypass = setLight(); break;
          case 't': case 'T': dsc.lightTrouble = setLight(); break;
          case 'p': case 'P': dsc.lightProgram = setLight(); break;
          case 'f': case 'F': dsc.lightFire = setLight(); break;
          case 'l': case 'L': dsc.lightBacklight = setLight(); break;
          case '1': dsc.lightZone1 = setLight(); break;
          case '2': dsc.lightZone2 = setLight(); break;
          case '3': dsc.lightZone3 = setLight(); break;
          case '4': dsc.lightZone4 = setLight(); break;
          case '5': dsc.lightZone5 = setLight(); break;
          case '6': dsc.lightZone6 = setLight(); break;
          case '7': dsc.lightZone7 = setLight(); break;
          case '8': dsc.lightZone8 = setLight(); break;
          case 'b': case 'B': sendBeeps(i); i += beepLength; break;
          case 'n': case 'N': sendTone(i); i+= toneLength; break;
          case 'z': case 'Z': sendBuzzer(i); i+= buzzerLength; break;
          case '-': lightOff = true; break;
          case '!': lightBlink = true; break;
          default: break;
        }
      }
    }
    #else
    for (byte i = 0; i < strlen(input); i++) {
      switch (input[i]) {
        case 'r': case 'R': dsc.lightReady = setLight(); break;
        case 'a': case 'A': dsc.lightArmed = setLight(); break;
        case 'm': case 'M': dsc.lightMemory = setLight(); break;
        case 'y': case 'Y': dsc.lightBypass = setLight(); break;
        case 't': case 'T': dsc.lightTrouble = setLight(); break;
        case 'p': case 'P': dsc.lightProgram = setLight(); break;
        case 'f': case 'F': dsc.lightFire = setLight(); break;
        case 'l': case 'L': dsc.lightBacklight = setLight(); break;
        case '1': dsc.lightZone1 = setLight(); break;
        case '2': dsc.lightZone2 = setLight(); break;
        case '3': dsc.lightZone3 = setLight(); break;
        case '4': dsc.lightZone4 = setLight(); break;
        case '5': dsc.lightZone5 = setLight(); break;
        case '6': dsc.lightZone6 = setLight(); break;
        case '7': dsc.lightZone7 = setLight(); break;
        case '8': dsc.lightZone8 = setLight(); break;
        case 'b': case 'B': sendBeeps(i); i += beepLength; break;
        case 'n': case 'N': sendTone(i); i+= toneLength; break;
        case 'z': case 'Z': sendBuzzer(i); i+= buzzerLength; break;
        case '-': lightOff = true; break;
        case '!': lightBlink = true; break;
        default: break;
      }
    }
    #endif
  }

  dsc.loop();

  // Checks for a keypad key press
  if (dsc.keyAvailable) {
    dsc.keyAvailable = false;
    switch (dsc.key) {
      case 0x00: Serial.println("0"); break;
      case 0x05: Serial.println("1"); break;
      case 0x0A: Serial.println("2"); break;
      case 0x0F: Serial.println("3"); break;
      case 0x11: Serial.println("4"); break;
      case 0x16: Serial.println("5"); break;
      case 0x1B: Serial.println("6"); break;
      case 0x1C: Serial.println("7"); break;
      case 0x22: Serial.println("8"); break;
      case 0x27: Serial.println("9"); break;
      case 0x28: Serial.println("*"); break;
      case 0x2D: Serial.println("#"); break;
      case 0x82: Serial.println(F("Enter")); break;
      case 0xAF: Serial.println(F("Arm: Stay")); break;
      case 0xB1: Serial.println(F("Arm: Away")); break;
      case 0xBB: Serial.println(F("Door chime")); break;
      case 0xDA: Serial.println(F("Reset")); break;
      case 0xE1: Serial.println(F("Quick exit")); break;
      case 0xF7: Serial.println(F("Menu navigation")); break;
      case 0x0B: Serial.println(F("Fire alarm")); break;
      case 0x0D: Serial.println(F("Aux alarm")); break;
      case 0x0E: Serial.println(F("Panic alarm")); break;
      default: break;
    }
  }
}


// Parse the number of beeps from the input
void sendBeeps(byte position) {
  char inputNumber[4];
  byte beeps = 0;
  beepLength = 0;

  for (byte i = position + 1; i < strlen(input); i++) {
    if (input[i] >= '0' && input[i] <= '9') {
      inputNumber[beepLength] = input[i];
      beepLength++;
      if (beepLength >= 3) break;
    }
    else break;
  }

  inputNumber[beepLength] = '\0';
  beeps = atoi(inputNumber);
  if (beeps > 128) beeps = 128;

  dsc.beep(beeps);
}


// Parse the buzzer length in seconds from the input
void sendBuzzer(byte position) {
  char inputNumber[4];
  byte buzzerSeconds = 0;
  buzzerLength = 0;

  for (byte i = position + 1; i < strlen(input); i++) {
    if (input[i] >= '0' && input[i] <= '9') {
      inputNumber[buzzerLength] = input[i];
      buzzerLength++;
      if (buzzerLength >= 3) break;
    }
    else break;
  }

  inputNumber[buzzerLength] = '\0';
  buzzerSeconds = atoi(inputNumber);
  dsc.buzzer(buzzerSeconds);
}


// Parse the tone pattern number of beeps, constant tone state, and interval in seconds from the input
void sendTone(byte position) {
  byte beeps = 0, interval = 0, intervalLength = 0;
  char beepNumber[2];
  bool toneState;
  char intervalNumber[3];
  toneLength = 0;

  if (strlen(input) < 4) {
    dsc.tone(0, false, 0);
    return;
  }

  // Parse beeps 0-7
  if (input[position + 1] >= '0' && input[position + 1] <= '9') {
    beepNumber[0] = input[position + 1];
    beeps = atoi(beepNumber);
    if (beeps > 7) beeps = 7;
    toneLength++;
  }
  else return;

  // Parse constant tone value
  switch (input[position + 2]) {
    case 't':
    case 'T': toneState = true; toneLength++; break;
    case 'f':
    case 'F': toneState = false; toneLength++; break;
    default: toneLength--; return;
  }

  // Parse interval
  for (byte i = position + 3; i < strlen(input); i++) {
    if (input[i] >= '0' && input[i] <= '9') {
      intervalNumber[intervalLength] = input[i];
      intervalLength++;
      toneLength++;
      if (intervalLength >= 2) break;
    }
    else break;
  }
  intervalNumber[intervalLength] = '\0';
  interval = atoi(intervalNumber);
  if (interval > 15) interval = 15;

  dsc.tone(beeps, toneState, interval);
}


// Sets keypad lights state - lights use custom values for control: off, on, blink (example: dsc.lightReady = blink;)
Light setLight() {
  if (lightOff) {
    lightOff = false;
    return off;
  }
  else if (lightBlink) {
    lightBlink = false;
    return blink;
  }
  else return on;
}


// Stores Serial data in input[], requires a newline character (NL, CR, or both)
void inputSerial() {
  static byte inputCount = 0;
  if (!inputReceived) {
    while (Serial.available() > 0 && inputCount < inputLimit) {
      input[inputCount] = Serial.read();
      if (input[inputCount] == '\n' || input[inputCount] == '\r') {
        input[inputCount] = '\0';
        inputCount = 0;
        inputReceived = true;
        break;
      }
      else inputCount++;
      yield();
    }
    if (input[0] == '\0') inputReceived = false;
  }
}
