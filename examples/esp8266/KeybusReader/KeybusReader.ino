/*
 *  DSC Keybus Reader 1.0 (esp8266)
 *
 *  Decodes and prints data from the Keybus to a serial interface, including reading from serial for the virtual
 *  keypad.  This is primarily to help decode the Keybus protocol - see the Status examples to put the interface
 *  to productive use.
 *
 *  Wiring:
 *      DSC Aux(-) --- esp8266 ground
 *
 *                                         +--- dscClockPin (esp8266: D1, D2, D8)
 *      DSC Yellow --- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (esp8266: D1, D2, D8)
 *      DSC Green ---- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (esp8266: D1, D2, D8)
 *            Ground --- NPN emitter --/
 *
 *  Power (when disconnected from USB):
 *      DSC Aux(+) ---+--- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
 *                    |
 *                    +--- 3.3v voltage regulator --- esp8266 bare module VCC pin (ESP-12, etc)
 *
 *  Virtual keypad uses an NPN transistor to pull the data line low - most small signal NPN transistors should
 *  be suitable, for example:
 *   -- 2N3904
 *   -- BC547, BC548, BC549
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  This example code is in the public domain.
 */

#include <dscKeybusInterface.h>

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // Optional configuration
  dsc.hideKeypadDigits = false;      // Controls if keypad digits are hidden for publicly posted logs (default: false)
  dsc.processRedundantData = false;  // Controls if repeated periodic commands are processed and displayed (default: true)
  dsc.processModuleData = true;      // Controls if keypad and module data is processed and displayed (default: false)
  dsc.displayTrailingBits = false;   // Controls if bits read as the clock is reset are displayed, appears to be spurious data (default: false)

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();

  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {

  // Reads from serial input and writes to the Keybus as a virtual keypad
  if (Serial.available() > 0 && dsc.writeReady) {
      dsc.write(Serial.read());
  }

  if (dsc.handlePanel()) {

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // handlePanel() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
    if (dsc.bufferOverflow) Serial.println(F("Keybus buffer overflow"));
    dsc.bufferOverflow = false;

    // Prints panel data
    printTimestamp();
    Serial.print(" ");
    dsc.printPanelBinary();   // Optionally prints without spaces: printPanelBinary(false);
    Serial.print(" [");
    dsc.printPanelCommand();  // Prints the panel command as hex
    Serial.print("] ");
    dsc.printPanelMessage();  // Prints the decoded message
    Serial.println();

    // Prints keypad and module data when valid panel data is printed
    if (dsc.handleModule()) {
      printTimestamp();
      Serial.print(" ");
      dsc.printModuleBinary();   // Optionally prints without spaces: printKeybusBinary(false);
      Serial.print(" ");
      dsc.printModuleMessage();  // Prints the decoded message
      Serial.println();
    }
  }

  // Prints keypad and module data when valid panel data is not available
  else if (dsc.handleModule()) {
    printTimestamp();
    Serial.print(" ");
    dsc.printModuleBinary();  // Optionally prints without spaces: printKeybusBinary(false);
    Serial.print(" ");
    dsc.printModuleMessage();
    Serial.println();
  }
}

// Prints a timestamp in seconds (with 2 decimal precision) - this is useful to determine when
// the panel sends a group of messages immediately after each other due to an event.
void printTimestamp() {
  float timeStamp = millis() / 1000.0;
  if (timeStamp < 10) Serial.print("    ");
  else if (timeStamp < 100) Serial.print("   ");
  else if (timeStamp < 1000) Serial.print("  ");
  else if (timeStamp < 10000) Serial.print(" ");
  Serial.print(timeStamp, 2);
  Serial.print(F(":"));
}

