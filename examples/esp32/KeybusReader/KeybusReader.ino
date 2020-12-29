/*
 *  DSC Keybus Reader 1.2 (esp32)
 *
 *  Decodes and prints data from the Keybus to a serial interface, including reading from serial for the virtual
 *  keypad.  This is primarily to help decode the Keybus protocol - see the Status example to put the interface
 *  to productive use.
 *
 *  Release notes:
 *    1.2 - Handle spurious data while keybus is disconnected
 *          Removed redundant data processing
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- 5v voltage regulator --- esp32 development board 5v pin
 *
 *      DSC Aux(-) --- esp32 Ground
 *
 *                                         +--- dscClockPin (esp32: 4,13,16-39)
 *      DSC Yellow --- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (esp32: 4,13,16-39)
 *      DSC Green ---- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (esp32: 4,13,16-33)
 *            Ground --- NPN emitter --/
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
#define dscClockPin 18  // esp32: 4,13,16-39
#define dscReadPin  19  // esp32: 4,13,16-39
#define dscWritePin 21  // esp32: 4,13,16-33

// Initialize components
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  // Optional configuration
  dsc.hideKeypadDigits = false;      // Controls if keypad digits are hidden for publicly posted logs
  dsc.processModuleData = true;      // Controls if keypad and module data is processed and displayed
  dsc.displayTrailingBits = false;   // Controls if bits read as the clock is reset are displayed, appears to be spurious data

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();
  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {

  // Reads from serial input and writes to the Keybus as a virtual keypad
  if (Serial.available() > 0) dsc.write(Serial.read());

  if (dsc.loop()) {

    if (dsc.statusChanged) {      // Checks if the security system status has changed
      dsc.statusChanged = false;  // Reset the status tracking flag

      // Checks if the interface is connected to the Keybus
      if (dsc.keybusChanged) {
        dsc.keybusChanged = false;                 // Resets the Keybus data status flag
        if (dsc.keybusConnected) Serial.println(F("Keybus connected"));
        else Serial.println(F("Keybus disconnected"));
      }
    }

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // loop() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
    if (dsc.bufferOverflow) {
      Serial.println(F("Keybus buffer overflow"));
      dsc.bufferOverflow = false;
    }

    // Prints panel data
    if (dsc.keybusConnected) {
      printTimestamp();
      Serial.print(" ");
      dsc.printPanelBinary();   // Optionally prints without spaces: printPanelBinary(false);
      Serial.print(" [");
      dsc.printPanelCommand();  // Prints the panel command as hex
      Serial.print("] ");
      dsc.printPanelMessage();  // Prints the decoded message
      Serial.println();

      // Prints keypad and module data when valid panel data is printed
      if (dsc.handleModule()) printModule();
    }
  }

  // Prints keypad and module data when valid panel data is not available
  else if (dsc.keybusConnected && dsc.handleModule()) printModule();
}


// Prints keypad and module data
void printModule() {
  printTimestamp();
  Serial.print(" ");
  dsc.printModuleBinary();   // Optionally prints without spaces: printKeybusBinary(false);
  Serial.print(" ");
  dsc.printModuleMessage();  // Prints the decoded message
  Serial.println();
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

