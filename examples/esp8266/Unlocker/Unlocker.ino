/*
 *  DSC Unlocker 1.0 (esp8266)
 *
 *  Feedback requested, post your results at: https://github.com/taligentx/dscKeybusInterface/issues/101
 *
 *  Checks all possible 4-digit installer codes until a valid code is found, including handling keypad
 *  lockout if enabled.  The valid code is output to serial as well as repeatedly flashed with the
 *  built-in LED - each digit is indicated by the number of blinks, a long blink indicates "0".
 *
 *  If keypad lockout has been enabled by the installer, the sketch waits for the lockout to expire
 *  before continuing the code search.  The physical keypads may beep when this occurs, the keypads can
 *  be disconnected for silence during the code search.
 *
 *  Optionally, if the current configuration on the panel is not needed, the keypad lockout can be
 *  skipped to reduce the code search time by using a relay to automatically power cycle the panel while
 *  the panel is set to factory default (on the PC1864, by connecting a jumper wire from PGM1 to Z1). This
 *  has been tested with the commonly available relay boards using the Songle SRD-05VDC-SL-C relay (~1USD
 *  shipped).
 *    - The Wemos Relay Shield v1 is hardwired to connect using the D1 pin and will conflict with this
 *      interface if using the default wiring scheme and plugging directly into the Wemos D1 mini. Wire
 *      the relay board separately (without plugging in directly to the Wemos board) to connect the relay
 *      board D1 pin to a different esp8266 pin (or use a different relay board).
 *
 *  Example maximum unlocking times:
 *    - PC1864 without keypad lockout: ~8h20m
 *    - PC1864 with 5m keypad lockout, 6 codes tested before lockout: ~6d3h13m
 *    - PC1864 with keypad lockout and relay reset: ~19h27m
 *
 *  Note that if the panel was configured remotely (using the panel communicator), it is possible for the
 *  installer to lock out local programming - if so, this sketch will not be able to determine the code.
 *
 *  This example sketch also demonstrates how to check the panel status for states that are not tracked in
 *  the library using dsc.status[partition] (as seen in dscKeybusPrintData.cpp-printPanelMessages())
 *  and how check for specific panel commands using dsc.panelData[] (as seen in
 *  dscKeybusPrintData.cpp-printPanelMessage()).
 *
 *  Release notes:
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
 *
 *      Alternatively, if using a relay, external power is required instead of using the DSC Aux(+):
 *      External USB or 5v DC power supply --- esp8266 development board USB or 5v pin (NodeMCU, Wemos)
 *
 *      DSC Aux(-) --- esp8266 Ground
 *
 *                                         +--- dscClockPin (esp8266: D1, D2, D8)
 *      DSC Yellow --- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (esp8266: D1, D2, D8)
 *      DSC Green ---- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Virtual keypad:
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (esp8266: D1, D2, D8)
 *            Ground --- NPN emitter --/
 *
 *  Virtual keypad uses an NPN transistor to pull the data line low - most small signal NPN transistors should
 *  be suitable, for example:
 *   -- 2N3904
 *   -- BC547, BC548, BC549
 *
 *  Relay (Songle SRD-05VDC-SL-C based board) - optional:
 *          DSC power adapter (one wire) --- DSC AC
 *          DSC power adapter (one wire) --- Relay board COM (common)
 *      Relay board NC (normally closed) --- DSC AC
 *            Relay board trigger signal --- dscRelayPin (esp8266: D5, D6, D7)
 *                        Relay board 5v --- esp8266 development board 5v pin (NodeMCU, Wemos)
 *                       Relay board GND --- Ground
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  This example code is in the public domain.
 */

#include <dscKeybusInterface.h>

// Configures the Keybus interface with the specified pins
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscRelayPin D6  // esp8266: D5, D6, D7 (GPIO 14, 12, 13) - Optional, leave this pin disconnected if not using a relay

// Starting installer code to test (without leading zeros) - this can be changed to start at a different code
// if the process is interrupted
int startingCode = 0;

// Commonly known DSC codes are tested first
int knownCodes[] = {1000, 1182, 1212, 1234, 1500, 1550, 1555, 1626, 1676, 1984, 2500, 2525, 2530, 3000, 3101,
                    4010, 4020, 4112, 5010, 5020, 5050, 5150, 5555, 6321, 6900, 9010};

// Initialize components
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
int testCode = 0;
char testCodeChar[5];
int knownCodesCount = sizeof(knownCodes) / sizeof(knownCodes[0]);
int knownCodesCounter = 0;
bool knownCodesComplete;


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // Pin setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(dscRelayPin, OUTPUT);
  digitalWrite(dscRelayPin, LOW);

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();
  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {
  if (!knownCodesComplete) {
    if (knownCodesCounter < knownCodesCount) {
      testCode = knownCodes[knownCodesCounter];
      knownCodesCounter++;
    }
    else {
      knownCodesComplete = true;
      testCode = startingCode;
    }
  }

  // Loop until the partition status is "Partition ready" (0x01), "Stay/away zones open" (0x02),
  // "Zones open" (0x03), or "Enter installer code" (0xB7)
  while (dsc.status[0] != 0x01 && dsc.status[0] != 0x02 && dsc.status[0] != 0x03 && dsc.status[0] != 0xB7) {
    dsc.loop();
    yield();
  }

  // Enter installer programming mode
  if (dsc.status[0] == 0x01 || dsc.status[0] == 0x02 || dsc.status[0] == 0x03) {
    dsc.write("*8");
    while (dsc.status[0] != 0xB7) {
      dsc.loop();  // Loop until "Enter installer code" (0xB7)
      yield();
    }
  }

  // Enter installer code (0xB7)
  if (dsc.status[0] == 0xB7) {
    printTimestamp();
    Serial.print("Test code: ");
    itoa(testCode, testCodeChar, 10);
    if (testCode < 10) {
      Serial.print("000");
      Serial.println(testCode);
      dsc.write('0');
      dsc.write('0');
      dsc.write('0');
      dsc.write(testCodeChar);
    }
    else if (testCode < 100) {
      Serial.print("00");
      Serial.println(testCode);
      dsc.write('0');
      dsc.write('0');
      dsc.write(testCodeChar);
    }
    else if (testCode < 1000) {
      Serial.print("0");
      Serial.println(testCode);
      dsc.write('0');
      dsc.write(testCodeChar);
    }
    else {
      Serial.println(testCode);
      dsc.write(testCodeChar);
    }

    // Loop until the panel states either "Keypad lockout" (0x10), "Invalid access code" (0x8F), or "*8 Main Menu" (0xE4)
    while (dsc.status[0] != 0x10 && dsc.status[0] != 0x8F && dsc.status[0] != 0xE4) {
      dsc.loop();
      yield();
    }

    // Keypad lockout
    if (dsc.status[0] == 0x10) {
      printTimestamp();
      Serial.println("Keypad lockout");

      digitalWrite(dscRelayPin, HIGH);
      delay(1000);
      digitalWrite(dscRelayPin, LOW);

      while (dsc.status[0] == 0x10) {
        dsc.loop();
        yield();
      }
      testCode++;
    }

    // Invalid access code
    else if (dsc.status[0] == 0x8F) {
      while (dsc.status[0] != 0xB7) {
        dsc.loop(); // Waits for "Enter installer code" (0xB7)
        yield();
      }
      testCode++;
    }

    // *8 Main Menu
    else if (dsc.status[0] == 0xE4) {

      // Tries entering keypad programming to check if code is valid for programming
      dsc.write('0');
      dsc.write('0');
      dsc.write('0');

      // Loop until the panel sends a status command (0x05) or a long beep command (0x7F)
      while (dsc.panelData[0] != 0x05 && dsc.panelData[0] != 0x7F) {
        dsc.loop();
        yield();
      }

      // Long beep, code is not valid for programming
      if (dsc.panelData[0] == 0x7F) {
        printTimestamp();
        Serial.print("Non-programming *8 code: ");
        Serial.println(testCode);
        dsc.write('#');
        testCode++;
      }

      // Entered keypad programming, code is valid - exits the installer menu and outputs the code via serial and LED blinking
      else if (dsc.status[0] == 0xF8) {
        printTimestamp();
        Serial.print("Installer code: ");
        Serial.println(testCode);
        dsc.write('#');
        dsc.write('#');

        // LED blinks the installer code repeatedly
        while(true) {
          byte leadingZeros = 0;
          byte leadingZerosTotal = 0;
          if (testCode < 10) {
            leadingZeros = 3;
            leadingZerosTotal = 3;
          }
          else if (testCode < 100) {
            leadingZeros = 2;
            leadingZerosTotal = 2;
          }
          else if (testCode < 1000) {
            leadingZeros = 1;
            leadingZerosTotal = 1;
          }
          for (byte digit = 0; digit < 4; digit++) {
            byte blinks;
            if (leadingZeros > 0) {
              blinks = 0;
              leadingZeros--;
            }
            else blinks = testCodeChar[digit - leadingZerosTotal] - 48;

            if (blinks == 0) {
              digitalWrite(LED_BUILTIN, LOW);
              delay(750);
              digitalWrite(LED_BUILTIN, HIGH);
              delay(400);
            }
            else {
              for (byte i = 0; i < blinks; i++) {
                digitalWrite(LED_BUILTIN, LOW);
                delay(100);
                digitalWrite(LED_BUILTIN, HIGH);
                delay(400);
              }
            }
            delay(1000);
          }
          delay(3000);
        }
      }
    }
  }
}


// Prints a timestamp in seconds
void printTimestamp() {
  unsigned long timeStamp = millis() / 1000;
  Serial.print(timeStamp);
  Serial.print(F("s "));
}
