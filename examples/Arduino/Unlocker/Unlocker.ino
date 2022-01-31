/*
 *  DSC Unlocker 1.3 (Arduino)
 *
 *  Checks all possible 4-digit installer codes until a valid code is found, including handling keypad
 *  lockout if enabled.  The valid code is output to serial as well as repeatedly flashed with the
 *  built-in LED - each digit is indicated by the number of blinks, a long blink indicates "0".
 *
 *  If keypad lockout is enabled on the panel, the sketch waits for the lockout to expire before
 *  continuing the code search.  The physical keypads may beep when this occurs, the keypads can
 *  be disconnected for silence during the code search.
 *
 *    - Keypad lockout can be skipped if a valid access code to arm/disarm the panel is entered in
 *      the sketch settings.  The sketch will trigger an arm/disarm cycle before the keypad
 *      lockout - this will result in beeps from the physical keypads each arm/disarm cycle.
 *
 *    - If a valid access code is not available and the current configuration on the panel is not
 *      needed, the keypad lockout can also be skipped by using a relay to automatically power cycle
 *      the panel while the panel is set to factory default (on the PC1864, by connecting a jumper
 *      wire from PGM1 to Z1). This has been tested with the commonly available relay boards using
 *      the Songle SRD-05VDC-SL-C relay (~1USD shipped).
 *
 *  Example maximum unlocking times on the PC1864:
 *    - Without keypad lockout: ~8h20m
 *    - With 6 attempt/5 minute keypad lockout: ~6d3h13m
 *    - With 6 attempt keypad lockout and skipping via arm/disarm reset: ~10hr18m
 *    - With 6 attempt keypad lockout and skipping via relay reset: ~19h27m
 *
 *  Note that if the panel was configured remotely (using the panel communicator), it is possible for the
 *  installer to lock out local programming - if so, this sketch will not be able to determine the code.
 *
 *  This example sketch also demonstrates how to check the panel status for states that are not tracked in
 *  the library using dsc.status[partition] (as seen in dscKeybusPrintData.cpp-printPanelMessages())
 *  and how check for specific panel commands using dsc.panelData[] (as seen in
 *  dscKeybusPrintData.cpp-printPanelMessage()).
 *
 *  Usage:
 *    - Run the sketch while the security system does not need to be armed - keypads will be unavailable
 *      during the search.
 *    - Press any key via serial to pause and resume the code search.
 *    - If the sketch is restarted, the code search can be resumed at the last known position by setting
 *      `startingCode`.
 *
 *  Release notes:
 *    1.3 - Add skipping keypad lockout by arm/disarm cycle with a valid access code
 *    1.2 - Check for valid panel state at startup
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- Arduino Vin pin
 *
 *      Alternatively if using a relay, external power is required instead of using DSC Aux(+):
 *      External DC power supply --- Arduino DC power jack
 *
 *      DSC Aux(-) --- Arduino Ground
 *
 *                                         +--- dscClockPin (Arduino Uno: 2,3)
 *      DSC Yellow --- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (Arduino Uno: 2-12)
 *      DSC Green ---- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *      Virtual keypad:
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (Arduino Uno: 2-12)
 *            Ground --- NPN emitter --/
 *
 *  Virtual keypad uses an NPN transistor to pull the data line low - most small signal NPN transistors should
 *  be suitable, for example:
 *   -- 2N3904
 *   -- BC547, BC548, BC549
 *
 *  Relay (Songle SRD-05VDC-SL-C based board) - optional to skip keypad lockout:
 *          DSC power adapter (one wire) --- DSC AC
 *          DSC power adapter (one wire) --- Relay board COM (common)
 *      Relay board NC (normally closed) --- DSC AC
 *            Relay board trigger signal --- dscRelayPin (Arduino Uno: 2-12)
 *                        Relay board 5v --- Arduino 5v pin
 *                       Relay board GND --- Ground
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  This example code is in the public domain.
 */

#include <dscKeybusInterface.h>

// Settings
const char* accessCode = "";   // Optional, skips keypad lockout if enabled by arming/disarming the panel
const int   startingCode = 0;  // Starting installer code to test (without leading zeros) if the process is interrupted

// Configures the Keybus interface with the specified pins
#define dscClockPin 3  // Arduino Uno hardware interrupt pin: 2,3
#define dscReadPin  5  // Arduino Uno: 2-12
#define dscWritePin 6  // Arduino Uno: 2-12
#define dscRelayPin 7  // Arduino Uno: 2-12 - Optional, leave this pin disconnected if not using a relay

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
bool pauseTest = false;
byte testCount = 1;
byte lockoutThreshold;


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  // Pin setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(dscRelayPin, OUTPUT);
  digitalWrite(dscRelayPin, LOW);

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();
  Serial.print(F("DSC Keybus Interface...."));

  // Loops until partition 1 is ready for key presses in status "Partition ready" (0x01),
  // "Stay/away zones open" (0x02), or "Zones open" (0x03)
  while (!dsc.status[0] || dsc.status[0] > 0x03) dsc.loop();
  Serial.println(F("connected."));
}


void loop() {

  // Checks if the interface is connected to the Keybus
  if (dsc.keybusChanged) {
    dsc.keybusChanged = false;  // Resets the Keybus data status flag
    if (!dsc.keybusConnected) {
      Serial.println(F("Keybus disconnected"));
      return;
    }
  }

  // Checks for serial input to pause/resume the code search if any key is sent
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c >= 32) {  // Checks for typical keyboard ASCII characters
      if (pauseTest) {
        pauseTest = false;
        printTimestamp();
        Serial.println(F("Resuming code search"));
        while (dsc.status[0] != 0x01 && dsc.status[0] != 0x02 && dsc.status[0] != 0x03) dsc.loop();
      }
      else {
        pauseTest = true;
        printTimestamp();
        dsc.write('#');
        Serial.println(F("Paused code search"));
      }
    }
  }
  if (pauseTest) return;

  // Tests commonly known DSC codes first
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

  // Loop until the partition 1 status is "Partition ready" (0x01), "Stay/away zones open" (0x02),
  // "Zones open" (0x03), or "Enter installer code" (0xB7)
  while (dsc.status[0] != 0x01 && dsc.status[0] != 0x02 && dsc.status[0] != 0x03 && dsc.status[0] != 0xB7) dsc.loop();

  // Skips keypad lockout if an access code is available to arm/disarm the panel
  if (testCount == lockoutThreshold && strlen(accessCode)) {
    testCount = 1;
    dsc.write('#');                                                     // Exit "Enter installer code" prompt
    while (dsc.status[0] != 0x01 && dsc.status[0] != 0x02) dsc.loop();  // Loop until the partition is ready
    dsc.write('s');                                                     // Stay arm
    while (dsc.status[0] != 0x08 && dsc.status[0] != 0x9F) dsc.loop();  // Loop until "Exit delay" or "Enter access code"
    if (dsc.status[0] == 0x9F) {
      dsc.write(accessCode);                                            // Enters the access code to arm if required
    }
    while (dsc.status[0] != 0x08) dsc.loop();                           // Loop until "Exit delay"
    dsc.write(accessCode);                                              // Enters the access code to disarm
    while (dsc.status[0] != 0x01 && dsc.status[0] != 0x02 && dsc.status[0] != 0x3E) dsc.loop();  // Loop until the partition is ready
  }

  // Enters installer programming mode
  if (dsc.status[0] == 0x01 || dsc.status[0] == 0x02 || dsc.status[0] == 0x03 || dsc.status[0] == 0x3E) {
    dsc.write("*8");
    while (dsc.status[0] != 0xB7) dsc.loop();  // Loop until "Enter installer code" (0xB7)
  }

  // Enters the test code on "Enter installer code"
  if (dsc.status[0] == 0xB7) {
    printTimestamp();
    Serial.print(F("Test code: "));
    itoa(testCode, testCodeChar, 10);
    if (testCode < 10) {
      Serial.print(F("000"));
      Serial.println(testCode);
      dsc.write("000");
      dsc.write(testCodeChar);
    }
    else if (testCode < 100) {
      Serial.print(F("00"));
      Serial.println(testCode);
      dsc.write("00");
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

    // Loops until "Keypad lockout" (0x10), "Invalid access code" (0x8F), or "*8 Main Menu" (0xE4)
    while (dsc.status[0] != 0x10 && dsc.status[0] != 0x8F && dsc.status[0] != 0xE4) dsc.loop();

    // Keypad lockout
    if (dsc.status[0] == 0x10) {
      lockoutThreshold = testCount;
      testCount = 1;
      printTimestamp();
      Serial.println(F("Keypad lockout"));

      // Cycles the optional relay
      digitalWrite(dscRelayPin, HIGH);
      delay(1000);
      digitalWrite(dscRelayPin, LOW);

      // Loops until the lockout expires
      while (dsc.status[0] == 0x10) dsc.loop();
      testCode++;
    }

    // Invalid access code
    else if (dsc.status[0] == 0x8F) {
      while (dsc.status[0] != 0xB7) dsc.loop(); // Loop until "Enter installer code" (0xB7)
      testCount++;
      testCode++;
    }

    // *8 Main Menu
    else if (dsc.status[0] == 0xE4) {

      // Attempts to enter keypad programming to check if code is valid for programming
      dsc.write('0');
      dsc.write('0');
      dsc.write('0');

      // Loops until the panel sends a status message (0x05) or a long beep command (0x7F)
      while (dsc.panelData[0] != 0x05 && dsc.panelData[0] != 0x7F) dsc.loop();

      // Long beep, code is not valid for programming
      if (dsc.panelData[0] == 0x7F) {
        printTimestamp();
        Serial.print(F("Non-programming *8 code: "));
        Serial.println(testCode);
        dsc.write('#');
        testCount++;
        testCode++;
      }

      // Entered keypad programming, code is valid - exits the installer menu and outputs the code via serial and LED blinking
      else if (dsc.status[0] == 0xF8) {
        printTimestamp();
        Serial.print(F("Installer code: "));
        Serial.println(testCode);
        dsc.write('#');
        dsc.write('#');

        // Blinks the LED - each digit is indicated by the number of blinks, a long blink indicates "0".
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
              digitalWrite(LED_BUILTIN, HIGH);
              delay(750);
              digitalWrite(LED_BUILTIN, LOW);
              delay(400);
            }
            else {
              for (byte i = 0; i < blinks; i++) {
                digitalWrite(LED_BUILTIN, HIGH);
                delay(100);
                digitalWrite(LED_BUILTIN, LOW);
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
  if (timeStamp < 10) Serial.print("    ");
  else if (timeStamp < 100) Serial.print("   ");
  else if (timeStamp < 1000) Serial.print("  ");
  else if (timeStamp < 10000) Serial.print(" ");
  Serial.print(timeStamp);
  Serial.print(F("s | "));
}
