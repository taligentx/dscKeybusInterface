/*
 *  DSC Keybus Reader 1.0 (esp8266)
 *
 *  Decodes and prints data from the Keybus to a TCP connection including virtual keyboard over IP. This is primarily to help decode the Keybus protocol - see the Status examples to put the interface
 *  to productive use.
 *
 *  Wiring:
 *      DSC Aux(+) ---+--- esp8266 NodeMCU Vin pin
 *                    |
 *                    +--- 5v voltage regulator --- esp8266 Wemos D1 Mini 5v pin
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
 *  This example code is in the public domain.
 */
#include "ESP8266WiFi.h"
#include <dscKeybusInterface.h>

//output debug msgs using define DEBUG_ESP_PORT
#ifdef DEBUG_ESP_PORT
  #define DEBUG_MSG(...) DEBUG_ESP_PORT.printf( __VA_ARGS__ )
#else
  #define DEBUG_MSG(...)
#endif

// This define controls wether OTA is used
#define USE_ARDUINO_OTA
#ifdef USE_ARDUINO_OTA
  #include <ArduinoOTA.h>
#endif

// Adjust settings
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* host = "dscip"; //hostname to be used by OTA, accessible on LAN with mdns: dscip.local
WiFiServer wifiServer(80);
WiFiClient client;

bool runOnce = true;

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);

#ifdef USE_ARDUINO_OTA
void initOTA() {
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(host);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    detachInterrupt(digitalPinToInterrupt(dscClockPin));  // Disables the DSC clock interrupt and prevents the DSC data timer interrupt from interfering with OTA

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    DEBUG_MSG("Start updating %s", type.c_str());
  });
  
  ArduinoOTA.onEnd([]() {
    DEBUG_MSG("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG_MSG("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_MSG("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      DEBUG_MSG("Auth Failed\n");
    }
    else if (error == OTA_BEGIN_ERROR) {
      DEBUG_MSG("Begin Failed\n");
    }
    else if (error == OTA_CONNECT_ERROR) {
      DEBUG_MSG("Connect Failed\n");
    }
    else if (error == OTA_RECEIVE_ERROR) {
      DEBUG_MSG("Receive Failed\n");
    }
    else if (error == OTA_END_ERROR) {
      DEBUG_MSG("End Failed\n");
    }
  });
  ArduinoOTA.begin();
}
#endif

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
 
  Serial.print("Connected to WiFi. IP:");
  Serial.println(WiFi.localIP());

  #ifdef USE_ARDUINO_OTA
    initOTA();
  #endif

  wifiServer.begin();

  // Optional configuration
  dsc.hideKeypadDigits = true;      // Controls if keypad digits are hidden for publicly posted logs (default: false)
  dsc.processRedundantData = false;  // Controls if repeated periodic commands are processed and displayed (default: true)
  dsc.processModuleData = true;      // Controls if keypad and module data is processed and displayed (default: false)
  dsc.displayTrailingBits = false;   // Controls if bits read as the clock is reset are displayed, appears to be spurious data (default: false)

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), begin(client) for IP.
  dsc.begin(client);
  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {

  #ifdef USE_ARDUINO_OTA
      ArduinoOTA.handle();
  #endif
  
  dsc.handlePanel(); //call it to process buffer when client is disconnected

  client = wifiServer.available(); 
  if (client) {
    while (client.connected()) {

      // Once client is connected, tell it is connected (once!)
      if (runOnce) {
        client.printf("Client connected to dscKeybusReader\n"); 
        runOnce = false;
      }

      // Reads from IP input and writes to the Keybus as a virtual keypad
      while (client.available() > 0 && dsc.writeReady)
      {
        char c = static_cast<char>(client.read());
        dsc.write(c);
      }

      if (dsc.handlePanel()) {
        // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
        // handlePanel() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
        if (dsc.bufferOverflow) client.print(F("Keybus buffer overflow"));
        dsc.bufferOverflow = false;

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
          // Prints keypad and module data when valid panel data is not available
        else if (dsc.handleModule()) {
          printTimestamp();
          client.print(" ");
          dsc.printModuleBinary();  // Optionally prints without spaces: printKeybusBinary(false);
          client.print(" ");
          dsc.printModuleMessage();
          client.printf("\n");
          }
        }
    }
    client.stop();
    client.printf("Client disconnected from dscKeybusReader\n");
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

