/*
 *  DSC Keybus Reader IP 1.3 (esp8266)
 *
 *  Decodes and prints data from the Keybus to a TCP connection including virtual keyboard over IP. This is
 *  primarily to help decode the Keybus protocol - see the Status example to put the interface to productive use.
 *
 *  Usage:
 *    1. Set WiFi settings and upload the sketch.
 *    2. For macOS/Linux: telnet dsc.local
 *
 *  Release notes:
 *    1.3 - Added DSC Classic series support
 *    1.2 - Updated to connect via telnet
 *          Handle spurious data while keybus is disconnected
 *          Removed redundant data processing
 *    1.1 - Updated esp8266 wiring diagram for 33k/10k resistors
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
 *
 *      DSC Aux(-) --- esp8266 Ground
 *
 *                                         +--- dscClockPin  // Default: D1, GPIO 5
 *      DSC Yellow --- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin  // Default: D2, GPIO 4
 *      DSC Green ---- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *      Classic series only, PGM configured for PC-16 output:
 *      DSC PGM ---+-- 1k ohm resistor --- DSC Aux(+)
 *                 |
 *                 |                       +--- dscPC16Pin   // Default: D7, GPIO 13
 *                 +-- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *      Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin  // Default: D8, GPIO 15
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
 *  Many thanks to aboulfad for contributing this example: https://github.com/aboulfad
 *
 *  This example code is in the public domain.
 */

// DSC Classic series: uncomment for PC1500/PC1550 support (requires PC16-OUT configuration per README.md)
//#define dscClassicSeries

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <dscKeybusInterface.h>

// Settings
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* dnsHostname = "dsc";  // Sets the domain name - if set to "dsc", access via: dsc.local
const int   serverPort = 23;

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin D1  // GPIO 5
#define dscReadPin  D2  // GPIO 4
#define dscPC16Pin  D7  // DSC Classic Series only, GPIO 13
#define dscWritePin D8  // GPIO 15

// Initialize components
#ifndef dscClassicSeries
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
#else
dscClassicInterface dsc(dscClockPin, dscReadPin, dscPC16Pin, dscWritePin);
#endif
WiFiServer ipServer(serverPort);
WiFiClient ipClient;


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  Serial.print(F("WiFi...."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print(F("connected: "));
  Serial.println(WiFi.localIP());

  if (!MDNS.begin(dnsHostname)) {
    Serial.println("Error setting up MDNS responder.");
    while (1) {
      delay(1000);
    }
  }

  ipServer.begin();
  Serial.print(F("Server started: "));
  Serial.print(dnsHostname);
  Serial.print(F(".local:"));
  Serial.println(serverPort);

  // Optional configuration
  dsc.hideKeypadDigits = false;      // Controls if keypad digits are hidden for publicly posted logs (default: false)
  dsc.processModuleData = true;      // Controls if keypad and module data is processed and displayed (default: false)
  dsc.displayTrailingBits = false;   // Controls if bits read as the clock is reset are displayed, appears to be spurious data (default: false)

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), begin(client) for IP.
  dsc.begin(ipClient);
  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {

  MDNS.update();

  dsc.loop(); //call it to process buffer when client is disconnected

  // Checks if the interface is connected to the Keybus
  if (dsc.keybusChanged) {
    dsc.keybusChanged = false;                 // Resets the Keybus data status flag
    if (dsc.keybusConnected) Serial.println(F("Keybus connected"));
    else Serial.println(F("Keybus disconnected"));
  }

  ipClient = ipServer.available();
  if (ipClient) {
    static bool newClient = true;

    while (ipClient.connected()) {

      // Once client is connected, tell it is connected (once!)
      if (newClient) {
        Serial.println("Client connected");
        ipClient.printf("Connected to DSC Keybus Reader\r\n");
        newClient = false;
      }

      // Reads from IP input and writes to the Keybus as a virtual keypad
      if (ipClient.available() > 0) {
        if (ipClient.peek() == 0xFF) {  // Checks for Telnet options negotiation data
          for (byte i = 0; i < 3; i++) ipClient.read();
        } else {
          char c = static_cast<char>(ipClient.read());
          dsc.write(c);
        }
      }

      if (dsc.loop()) {

        // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
        // loop more often, or increase dscBufferSize in the library: src/dscKeybus.h or src/dscClassic.h
        if (dsc.bufferOverflow) {
          ipClient.print(F("Keybus buffer overflow"));
          dsc.bufferOverflow = false;
        }

        // Prints panel data
        if (dsc.keybusConnected) {
          printTimestamp();
          ipClient.print(" ");
          dsc.printPanelBinary();   // Optionally prints without spaces: printPanelBinary(false);
          ipClient.print(" [");
          dsc.printPanelCommand();  // Prints the panel command as hex
          ipClient.print("] ");
          dsc.printPanelMessage();  // Prints the decoded message
          ipClient.printf("\r\n");
        }

        // Prints keypad and module data when valid panel data is printed
        if (dsc.handleModule()) printModule();
      }

      // Prints keypad and module data when valid panel data is not available
      else if (dsc.keybusConnected && dsc.handleModule()) printModule();

      MDNS.update();
      yield();
    }

    ipClient.stop();
    newClient = true;
    Serial.println("Client disconnected");
  }
}


// Prints keypad and module data
void printModule() {
  printTimestamp();
  ipClient.print(" ");
  dsc.printModuleBinary();  // Optionally prints without spaces: printKeybusBinary(false);
  ipClient.print(" ");
  dsc.printModuleMessage();
  ipClient.printf("\r\n");
}


// Prints a timestamp in seconds (with 2 decimal precision) - this is useful to determine when
// the panel sends a group of messages immediately after each other due to an event.
void printTimestamp() {
  float timeStamp = millis() / 1000.0;
  if (timeStamp < 10) ipClient.print("    ");
  else if (timeStamp < 100) ipClient.print("   ");
  else if (timeStamp < 1000) ipClient.print("  ");
  else if (timeStamp < 10000) ipClient.print(" ");
  ipClient.print(timeStamp, 2);
  ipClient.print(F(":"));
}
