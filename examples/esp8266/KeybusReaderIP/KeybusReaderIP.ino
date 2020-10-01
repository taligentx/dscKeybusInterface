/*
 *  DSC Keybus Reader IP 1.1 (esp8266)
 *
 *  Decodes and prints data from the Keybus to a TCP connection including virtual keyboard over IP. This is
 *  primarily to help decode the Keybus protocol - see the Status example to put the interface to productive use.
 *
 *  Usage:
 *    1. Set WiFi settings and upload the sketch.
 *    2. For macOS/Linux, use netcat to connect: nc dsc.local 80
 *
 *  Release notes:
 *    1.1 - Updated esp8266 wiring diagram for 33k/10k resistors
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
 *
 *      DSC Aux(-) --- esp8266 Ground
 *
 *                                         +--- dscClockPin (esp8266: D1, D2, D8)
 *      DSC Yellow --- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (esp8266: D1, D2, D8)
 *      DSC Green ---- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (esp8266: D1, D2, D8)
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

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <dscKeybusInterface.h>

// Settings
const char* wifiSSID = "";
const char* wifiPassword = "";
char dnsHostname[] = "dsc";  // Sets the domain name - if set to "dsc", access via: http://dsc.local
int serverPort = 80;

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)

// Initialize components
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
WiFiServer wifiServer(serverPort);
WiFiClient client;
bool newClient = true;


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) delay(100);
  Serial.print(F("WiFi connected: "));
  Serial.println(WiFi.localIP());

  if (!MDNS.begin(dnsHostname)) {
    Serial.println("Error setting up MDNS responder.");
    while (1) {
      delay(1000);
    }
  }

  wifiServer.begin();
  Serial.print(F("Server started: "));
  Serial.print(dnsHostname);
  Serial.print(F(".local:"));
  Serial.println(serverPort);

  // Optional configuration
  dsc.hideKeypadDigits = false;      // Controls if keypad digits are hidden for publicly posted logs (default: false)
  dsc.processRedundantData = false;  // Controls if repeated periodic commands are processed and displayed (default: true)
  dsc.processModuleData = true;      // Controls if keypad and module data is processed and displayed (default: false)
  dsc.displayTrailingBits = false;   // Controls if bits read as the clock is reset are displayed, appears to be spurious data (default: false)

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), begin(client) for IP.
  dsc.begin(client);
  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {

  MDNS.update();

  dsc.loop(); //call it to process buffer when client is disconnected

  client = wifiServer.available();
  if (client) {

    while (client.connected()) {

      // Once client is connected, tell it is connected (once!)
      if (newClient) {
        Serial.println("Client connected");
        client.printf("Connected to dscKeybusReader\n");
        newClient = false;
      }

      // Reads from IP input and writes to the Keybus as a virtual keypad
      while (client.available() > 0) {
        char c = static_cast<char>(client.read());
        dsc.write(c);
        yield();
      }

      if (dsc.loop()) {
        // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
        // loop more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
        if (dsc.bufferOverflow) {
          client.print(F("Keybus buffer overflow"));
          dsc.bufferOverflow = false;
        }

        // Prints panel data
        printTimestamp();
        client.print(" ");
        dsc.printPanelBinary();   // Optionally prints without spaces: printPanelBinary(false);
        client.print(" [");
        dsc.printPanelCommand();  // Prints the panel command as hex
        client.print("] ");
        dsc.printPanelMessage();  // Prints the decoded message
        client.printf("\n");

        // Prints keypad and module data when valid panel data is printed
        if (dsc.handleModule()) {
          printTimestamp();
          client.print(" ");
          dsc.printModuleBinary();   // Optionally prints without spaces: printKeybusBinary(false);
          client.print(" ");
          dsc.printModuleMessage();  // Prints the decoded message
          client.printf("\n");
        }
      }

      // Prints keypad and module data when valid panel data is not available
      else if (dsc.handleModule()) {
        printTimestamp();
        client.print(" ");
        dsc.printModuleBinary();  // Optionally prints without spaces: printKeybusBinary(false);
        client.print(" ");
        dsc.printModuleMessage();
        client.printf("\n");
      }

      MDNS.update();
    }
    client.stop();
    newClient = true;
    Serial.println("Client disconnected");
  }
}

// Prints a timestamp in seconds (with 2 decimal precision) - this is useful to determine when
// the panel sends a group of messages immediately after each other due to an event.
void printTimestamp() {
  float timeStamp = millis() / 1000.0;
  if (timeStamp < 10) client.print("    ");
  else if (timeStamp < 100) client.print("   ");
  else if (timeStamp < 1000) client.print("  ");
  else if (timeStamp < 10000) client.print(" ");
  client.print(timeStamp, 2);
  client.print(F(":"));
}
