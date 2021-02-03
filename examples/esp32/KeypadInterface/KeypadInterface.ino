/*
 *  DSC Keypad Interface 1.0 (esp32)
 *
 *  Interfaces directly to a DSC PowerSeries keypad (without a DSC panel) to enable use of
 *  DSC keypads as physical inputs for any general purpose.
 *
 *  This interface uses a different wiring setup from the standard Keybus interface, adding
 *  an NPN transistor on dscClockPin.  The DSC keypads require a 12v DC power source, though
 *  lower voltages down to 7v may work for key presses (the LEDs will be dim).
 *
 *  Supported features:
 *    - Read keypad key button presses, including fire/aux/panic alarms
 *    - Set keypad lights: Ready, Armed, Trouble, Memory, Bypass, Fire, Program, Backlight, Zones 1-8
 *
 *  Release notes:
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Keypad R --- 12v DC
 *
 *      DSC Keypad B --- Ground
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
#define dscKeypad

#include <dscKeybusInterface.h>

#define dscClockPin 18  // 4,13,16-39
#define dscReadPin  19  // 4,13,16-39
#define dscWritePin 21  // 4,13,16-33

dscKeypadInterface dsc(dscClockPin, dscReadPin, dscWritePin);
bool lightOff;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  dsc.begin();
  Serial.print(F("Keybus..."));
  unsigned long keybusTime = millis();
  while (millis() - keybusTime < 4000) {  // Waits for the keypad to be powered on
    if (!digitalRead(dscReadPin)) keybusTime = millis();
    yield();
  }
  Serial.println(F("connected."));
  Serial.println(F("DSC Keybus Interface is online."));
}

void loop() {

  // Sets keypad lights via serial, send "-" before a light key to turn off
  if (Serial.available() > 0) {
    char input = Serial.read();
    switch (input) {
      case '-': lightOff = true; break;
      case '1': dsc.Zone1 = setLight(); break;
      case '2': dsc.Zone2 = setLight(); break;
      case '3': dsc.Zone3 = setLight(); break;
      case '4': dsc.Zone4 = setLight(); break;
      case '5': dsc.Zone5 = setLight(); break;
      case '6': dsc.Zone6 = setLight(); break;
      case '7': dsc.Zone7 = setLight(); break;
      case '8': dsc.Zone8 = setLight(); break;
      case 'r':
      case 'R': dsc.Ready = setLight(); break;
      case 'a':
      case 'A': dsc.Armed = setLight(); break;
      case 'm':
      case 'M': dsc.Memory = setLight(); break;
      case 'b':
      case 'B': dsc.Bypass = setLight(); break;
      case 't':
      case 'T': dsc.Trouble = setLight(); break;
      case 'p':
      case 'P': dsc.Program = setLight(); break;
      case 'f':
      case 'F': dsc.Fire = setLight(); break;
      case 'l':
      case 'L': dsc.Backlight = setLight(); break;
      default: break;
    }
  }

  dsc.loop();

  // Checks for a keypad key press
  if (dsc.KeyAvailable) {
    dsc.KeyAvailable = false;
    switch (dsc.Key) {
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
    }
  }
}


// Sets keypad lights state
bool setLight() {
  if (lightOff) {
    lightOff = false;
    return false;
  }
  else return true;
}
