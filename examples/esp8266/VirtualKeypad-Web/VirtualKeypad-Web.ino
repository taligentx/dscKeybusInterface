/*
 *  VirtualKeypad-Web 1.3 (esp8266)
 *
 *  Provides a virtual keypad web interface using the esp8266 as a standalone web server, including
 *  alarm memory, programming zone lights, and viewing the event buffer.  To access the event buffer,
 *  enter *6, enter an access code, then press the keypad "Enter" button.
 *
 *  Usage:
 *    1. Install the following libraries directly from each Github repository:
 *         ESPAsyncWebServer: https://github.com/me-no-dev/ESPAsyncWebServer
 *         ESPAsyncTCP: https://github.com/me-no-dev/ESPAsyncTCP
 *
 *    2. Install ESP8266FS to enable uploading web server files to the esp8266:
 *         https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html#uploading-files-to-file-system
 *
 *    3. Install the following libraries, available in the Arduino IDE Library Manager and
 *       the Platform.io Library Registry:
 *         ArduinoJson: https://github.com/bblanchon/ArduinoJson
 *         Chrono: https://github.com/SofaPirate/Chrono
 *
 *    4. Set the WiFi SSID and password in the sketch.
 *    5. If desired, update the DNS hostname in the sketch.  By default, this is set to
 *       "dsc" and the web interface will be accessible at: http://dsc.local
 *    6. Set the esp8266 flash size to use 1M SPIFFS.
 *         Arduino IDE: Tools > Flash Size > 4M (1M SPIFFS)
 *    7. Upload the sketch.
 *    8. Upload the SPIFFS data containing the web server files:
 *         Arduino IDE: Tools > ESP8266 Sketch Data Upload
 *    9. Access the virtual keypad web interface by the IP address displayed through
 *       the serial output or http://dsc.local (for clients and networks that support mDNS).
 *
 *  Release notes:
 *    1.3 - Add event buffer display
 *          Display zone lights in alarm memory and programming
 *          Added AC power status, reset, quick exit
 *          Removed UI <> buttons, backlight icon, PGM
 *    1.2 - Updated esp8266 wiring diagram for 33k/10k resistors
 *    1.1 - New: Fire, alarm, panic, stay arm, away arm, door chime buttons are now functional
 *          Bugfix: Set mDNS to update in loop()
 *          Updated: ArduinoOTA no longer included by default
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
 *  Many thanks to Elektrik1 for contributing this example: https://github.com/Elektrik1
 *
 *  This example code is in the public domain.
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <dscKeybusInterface.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <Chrono.h>

// Settings
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* dnsHostname = "dsc";  // Sets the domain name - if set to "dsc", access via: http://dsc.local
const byte  dscPartition = 1;     // Set the partition for the keypad

// Configures the Keybus interface with the specified pins
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin  D2  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)

// Initialize components
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Chrono ws_ping_pong(Chrono::SECONDS);
bool partitionChanged, pausedZones, extendedBuffer;
byte systemZones[dscZones], programZones[dscZones];
byte partition = dscPartition - 1;
bool forceUpdate = false;


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  Serial.print(F("WiFi..."));
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

  SPIFFS.begin();
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.print(F("Web server started: http://"));
  Serial.print(dnsHostname);
  Serial.println(F(".local"));

  dsc.begin();
  dsc.writePartition = dscPartition;
  ws_ping_pong.stop();

  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {

  MDNS.update();

  //ping-pong WebSocket to keep connection open
  if (ws_ping_pong.isRunning() && ws_ping_pong.elapsed() > 5 * 60) {
    ws.pingAll();
    ws_ping_pong.restart();
  }

  bool validCommand = false;
  if (dsc.loop()) {
    validCommand = true;
    processStatus();  // Processes status data that is not handled natively by the library
  }

  if (validCommand && (dsc.statusChanged || forceUpdate)) {  // Processes data only when a valid Keybus command has been read
    dsc.statusChanged = false;  // Resets the status flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // loop() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
    if (dsc.bufferOverflow) {
      Serial.println(F("Keybus buffer overflow"));
      dsc.bufferOverflow = false;
    }

    setLights(partition);
    setStatus(partition);

    // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones
    //   openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
    //   openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
    //   ...
    //   openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
    if ((dsc.openZonesStatusChanged && !pausedZones) || forceUpdate) {
      dsc.openZonesStatusChanged = false;  // Resets the open zones status flag
      for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
        dsc.openZonesChanged[zoneGroup] = 0;
        systemZones[zoneGroup] = dsc.openZones[zoneGroup];
      }

      if (ws.count()) {
        char outas[512];
        StaticJsonDocument<200> doc;
        JsonObject root = doc.to<JsonObject>();
        root["open_zone_0"] = dsc.openZones[0];
        root["open_zone_1"] = dsc.openZones[1];
        root["open_zone_2"] = dsc.openZones[2];
        root["open_zone_3"] = dsc.openZones[3];
        root["open_zone_4"] = dsc.openZones[4];
        root["open_zone_5"] = dsc.openZones[5];
        root["open_zone_6"] = dsc.openZones[6];
        root["open_zone_7"] = dsc.openZones[7];
        serializeJson(root, outas);
        ws.textAll(outas);
      }
    }

    // Zone alarm status is stored in the alarmZones[] and alarmZonesChanged[] arrays using 1 bit per zone, up to 64 zones
    //   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
    //   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
    //   ...
    //   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
    if (dsc.alarmZonesStatusChanged || forceUpdate) {
      dsc.alarmZonesStatusChanged = false;                           // Resets the alarm zones status flag
      for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
        dsc.alarmZonesChanged[zoneGroup] = 0;
      }

      if (ws.count()) {
        char outas[512];
        StaticJsonDocument<200> doc;
        JsonObject root = doc.to<JsonObject>();
        root["alarm_zone_0"] = dsc.alarmZones[0];
        root["alarm_zone_1"] = dsc.alarmZones[1];
        root["alarm_zone_2"] = dsc.alarmZones[2];
        root["alarm_zone_3"] = dsc.alarmZones[3];
        root["alarm_zone_4"] = dsc.alarmZones[4];
        root["alarm_zone_5"] = dsc.alarmZones[5];
        root["alarm_zone_6"] = dsc.alarmZones[6];
        root["alarm_zone_7"] = dsc.alarmZones[7];
        serializeJson(root, outas);
        ws.textAll(outas);
      }
    }

    if (dsc.powerChanged || forceUpdate) {
      dsc.powerChanged = false;  // Resets the power trouble status flag
      if (dsc.powerTrouble) {
        if (ws.count()) {
        char outas[128];
        StaticJsonDocument<200> doc;
        JsonObject root = doc.to<JsonObject>();
        root["power_status"] = 0;
        serializeJson(root, outas);
        ws.textAll(outas);
        }
      }
      else {
        if (ws.count()) {
        char outas[128];
        StaticJsonDocument<200> doc;
        JsonObject root = doc.to<JsonObject>();
        root["power_status"] = 1;
        serializeJson(root, outas);
        ws.textAll(outas);
        }
      }
    }

    if (forceUpdate) {
      forceUpdate = false;
    }
  }
}


void setStatus(byte partition) {
  static byte lastStatus[8];
  if (!partitionChanged && dsc.status[partition] == lastStatus[partition] && !forceUpdate) return;
  lastStatus[partition] = dsc.status[partition];

  if (ws.count()) {
    char outas[128];
    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();

    switch (dsc.status[partition]) {
      case 0x01: root["lcd_upper"] = "Partition    ";
                 root["lcd_lower"] = "ready           ";
                 if (pausedZones) resetZones(); break;
      case 0x02: root["lcd_upper"] = "Stay         ";
                 root["lcd_lower"] = "zones open      ";
                 if (pausedZones) resetZones(); break;
      case 0x03: root["lcd_upper"] = "Zones open   ";
                 root["lcd_lower"] = "&nbsp;";
                 if (pausedZones) resetZones(); break;
      case 0x04: root["lcd_upper"] = "Armed:       ";
                 root["lcd_lower"] = "Stay            ";
                 if (pausedZones) resetZones(); break;
      case 0x05: root["lcd_upper"] = "Armed:       ";
                 root["lcd_lower"] = "Away            ";
                 if (pausedZones) resetZones(); break;
      case 0x06: root["lcd_upper"] = "Armed:       ";
                 root["lcd_lower"] = "No entry delay  ";
                 if (pausedZones) resetZones(); break;
      case 0x07: root["lcd_upper"] = "Failed       ";
                 root["lcd_lower"] = "to arm          "; break;
      case 0x08: root["lcd_upper"] = "Exit delay   ";
                 root["lcd_lower"] = "in progress     ";
                 if (pausedZones) resetZones(); break;
      case 0x09: root["lcd_upper"] = "Arming:      ";
                 root["lcd_lower"] = "No entry delay  "; break;
      case 0x0B: root["lcd_upper"] = "Quick exit   ";
                 root["lcd_lower"] = "in progress     "; break;
      case 0x0C: root["lcd_upper"] = "Entry delay  ";
                 root["lcd_lower"] = "in progress     "; break;
      case 0x0D: root["lcd_upper"] = "Entry delay  ";
                 root["lcd_lower"] = "after alarm     "; break;
      case 0x0E: root["lcd_upper"] = "Not          ";
                 root["lcd_lower"] = "available       "; break;
      case 0x10: root["lcd_upper"] = "Keypad       ";
                 root["lcd_lower"] = "lockout         "; break;
      case 0x11: root["lcd_upper"] = "Partition    ";
                 root["lcd_lower"] = "in alarm        "; break;
      case 0x12: root["lcd_upper"] = "Battery check";
                 root["lcd_lower"] = "in progress     "; break;
      case 0x14: root["lcd_upper"] = "Auto-arm     ";
                 root["lcd_lower"] = "in progress     "; break;
      case 0x15: root["lcd_upper"] = "Arming with  ";
                 root["lcd_lower"] = "bypass zones    "; break;
      case 0x16: root["lcd_upper"] = "Armed:       ";
                 root["lcd_lower"] = "No entry delay  ";
                 if (pausedZones) resetZones(); break;
      case 0x17: root["lcd_upper"] = "Power saving ";
                 root["lcd_lower"] = "Keypad blanked  "; break;
      case 0x19: root["lcd_upper"] = "Alarm        ";
                 root["lcd_lower"] = "occurred        "; break;
      case 0x22: root["lcd_upper"] = "Recent       ";
                 root["lcd_lower"] = "closing         "; break;
      case 0x2F: root["lcd_upper"] = "Keypad LCD   ";
                 root["lcd_lower"] = "test            "; break;
      case 0x33: root["lcd_upper"] = "Command      ";
                 root["lcd_lower"] = "output active   "; break;
      case 0x3D: root["lcd_upper"] = "Alarm        ";
                 root["lcd_lower"] = "occurred        "; break;
      case 0x3E: root["lcd_upper"] = "Disarmed     ";
                 root["lcd_lower"] = "&nbsp;"; break;
      case 0x40: root["lcd_upper"] = "Keypad       ";
                 root["lcd_lower"] = "blanked         "; break;
      case 0x8A: root["lcd_upper"] = "Activate     ";
                 root["lcd_lower"] = "stay/away zones "; break;
      case 0x8B: root["lcd_upper"] = "Quick exit   ";
                 root["lcd_lower"] = "&nbsp;"; break;
      case 0x8E: root["lcd_upper"] = "Invalid      ";
                 root["lcd_lower"] = "option          "; break;
      case 0x8F: root["lcd_upper"] = "Invalid      ";
                 root["lcd_lower"] = "access code     "; break;
      case 0x9E: root["lcd_upper"] = "Enter *      ";
                 root["lcd_lower"] = "function code   "; break;
      case 0x9F: root["lcd_upper"] = "Enter        ";
                 root["lcd_lower"] = "access code     "; break;
      case 0xA0: root["lcd_upper"] = "*1:          ";
                 root["lcd_lower"] = "Zone bypass     "; break;
      case 0xA1: root["lcd_upper"] = "*2:          ";
                 root["lcd_lower"] = "Trouble menu    "; break;
      case 0xA2: root["lcd_upper"] = "*3:          ";
                 root["lcd_lower"] = "Alarm memory    "; break;
      case 0xA3: root["lcd_upper"] = "Door         ";
                 root["lcd_lower"] = "chime enabled   "; break;
      case 0xA4: root["lcd_upper"] = "Door         ";
                 root["lcd_lower"] = "chime disabled  "; break;
      case 0xA5: root["lcd_upper"] = "Enter        ";
                 root["lcd_lower"] = "master code     "; break;
      case 0xA6: root["lcd_upper"] = "*5:          ";
                 root["lcd_lower"] = "Access codes    "; break;
      case 0xA7: root["lcd_upper"] = "*5 Enter new ";
                 root["lcd_lower"] = "4-digit code    "; break;
      case 0xA9: root["lcd_upper"] = "*6:          ";
                 root["lcd_lower"] = "User functions  "; break;
      case 0xAA: root["lcd_upper"] = "*6:          ";
                 root["lcd_lower"] = "Time and date   "; break;
      case 0xAB: root["lcd_upper"] = "*6:          ";
                 root["lcd_lower"] = "Auto-arm time   "; break;
      case 0xAC: root["lcd_upper"] = "*6:          ";
                 root["lcd_lower"] = "Auto-arm on     "; break;
      case 0xAD: root["lcd_upper"] = "*6:          ";
                 root["lcd_lower"] = "Auto-arm off    "; break;
      case 0xAF: root["lcd_upper"] = "*6:          ";
                 root["lcd_lower"] = "System test     "; break;
      case 0xB0: root["lcd_upper"] = "*6:          ";
                 root["lcd_lower"] = "Enable DLS      "; break;
      case 0xB2: root["lcd_upper"] = "*7:          ";
                 root["lcd_lower"] = "Command output  "; break;
      case 0xB7: root["lcd_upper"] = "Enter        ";
                 root["lcd_lower"] = "installer code  "; break;
      case 0xB8: root["lcd_upper"] = "Enter *      ";
                 root["lcd_lower"] = "function code   "; break;
      case 0xB9: root["lcd_upper"] = "*2:          ";
                 root["lcd_lower"] = "Zone tamper menu"; break;
      case 0xBA: root["lcd_upper"] = "*2: Zones    ";
                 root["lcd_lower"] = "low battery     "; break;
      case 0xBC: root["lcd_upper"] = "*5 Enter new ";
                 root["lcd_lower"] = "6-digit code    "; break;
      case 0xC6: root["lcd_upper"] = "*2:          ";
                 root["lcd_lower"] = "Zone fault menu "; break;
      case 0xC7: root["lcd_upper"] = "Partition    ";
                 root["lcd_lower"] = "disabled        "; break;
      case 0xC8: root["lcd_upper"] = "*2:          ";
                 root["lcd_lower"] = "Service required"; break;
      case 0xCE: root["lcd_upper"] = "Active camera";
                 root["lcd_lower"] = "monitor select. "; break;
      case 0xD0: root["lcd_upper"] = "*2: Keypads  ";
                 root["lcd_lower"] = "low battery     "; break;
      case 0xD1: root["lcd_upper"] = "*2: Keyfobs  ";
                 root["lcd_lower"] = "low battery     "; break;
      case 0xD4: root["lcd_upper"] = "*2: Sensors  ";
                 root["lcd_lower"] = "RF Delinquency  "; break;
      case 0xE4: root["lcd_upper"] = "*8: Installer";
                 root["lcd_lower"] = "menu, 3 digits  "; break;
      case 0xE5: root["lcd_upper"] = "Keypad       ";
                 root["lcd_lower"] = "slot assignment "; break;
      case 0xE6: root["lcd_upper"] = "Input:       ";
                 root["lcd_lower"] = "2 digits        "; break;
      case 0xE7: root["lcd_upper"] = "Input:       ";
                 root["lcd_lower"] = "3 digits        "; break;
      case 0xE8: root["lcd_upper"] = "Input:       ";
                 root["lcd_lower"] = "4 digits        "; break;
      case 0xE9: root["lcd_upper"] = "Input:       ";
                 root["lcd_lower"] = "5 digits        "; break;
      case 0xEA: root["lcd_upper"] = "Input HEX:   ";
                 root["lcd_lower"] = "2 digits        "; break;
      case 0xEB: root["lcd_upper"] = "Input HEX:   ";
                 root["lcd_lower"] = "4 digits        "; break;
      case 0xEC: root["lcd_upper"] = "Input HEX:   ";
                 root["lcd_lower"] = "6 digits        "; break;
      case 0xED: root["lcd_upper"] = "Input HEX:   ";
                 root["lcd_lower"] = "32 digits       "; break;
      case 0xEE: root["lcd_upper"] = "Input: 1     ";
                 root["lcd_lower"] = "option per zone "; break;
      case 0xEF: root["lcd_upper"] = "Module       ";
                 root["lcd_lower"] = "supervision     "; break;
      case 0xF0: root["lcd_upper"] = "Function     ";
                 root["lcd_lower"] = "key 1           "; break;
      case 0xF1: root["lcd_upper"] = "Function     ";
                 root["lcd_lower"] = "key 2           "; break;
      case 0xF2: root["lcd_upper"] = "Function     ";
                 root["lcd_lower"] = "key 3           "; break;
      case 0xF3: root["lcd_upper"] = "Function     ";
                 root["lcd_lower"] = "key 4           "; break;
      case 0xF4: root["lcd_upper"] = "Function     ";
                 root["lcd_lower"] = "key 5           "; break;
      case 0xF5: root["lcd_upper"] = "Wireless mod.";
                 root["lcd_lower"] = "placement test  "; break;
      case 0xF6: root["lcd_upper"] = "Activate     ";
                 root["lcd_lower"] = "device for test "; break;
      case 0xF7: root["lcd_upper"] = "*8: Installer";
                 root["lcd_lower"] = "menu, 2 digits  "; break;
      case 0xF8: root["lcd_upper"] = "Keypad       ";
                 root["lcd_lower"] = "programming     "; break;
      case 0xFA: root["lcd_upper"] = "Input:       ";
                 root["lcd_lower"] = "6 digits        "; break;
      default: root["lcd_lower"] = dsc.status[partition];
    }
    serializeJson(root, outas);
    ws.textAll(outas);
  }
}


void setLights(byte partition) {
  static byte previousLights = 0;

  if ((dsc.lights[partition] != previousLights || forceUpdate) && ws.count()) {
    char outas[128];
    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();
    root["status_lights"] = dsc.lights[partition];
    serializeJson(root, outas);
    ws.textAll(outas);
    previousLights = dsc.lights[partition];
  }
}


// Processes status data not natively handled within the library
void processStatus() {
  switch (dsc.panelData[0]) {
    case 0x05:
      if ((dsc.panelData[3] == 0x9E || dsc.panelData[3] == 0xB8) && !pausedZones) {
        pauseZones();
      }
      break;
    case 0x0A:
      if ((dsc.panelData[3] == 0x9E || dsc.panelData[3] == 0xB8) && !pausedZones) {
        pauseZones();
      }
      if (pausedZones) processProgramZones(4);
      break;
    case 0x5D:
      if ((dsc.panelData[2] & 0x04) == 0x04) {  // Alarm memory zones 1-32
        if (pausedZones) processProgramZones(3);
      }
      break;
    case 0xAA: if (pausedZones) processEventBufferAA(); break;
    case 0xE6:
      switch (dsc.panelData[2]) {
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x20:
        case 0x21: if (pausedZones) processProgramZones(5); break;         // Programming zone lights 33-64
        case 0x18: if (pausedZones && (dsc.panelData[4] & 0x04) == 0x04) processProgramZones(5); break;  // Alarm memory zones 33-64
      }
      break;
    case 0xEC: if (pausedZones) processEventBufferEC(); break;
  }
}


void processProgramZones(byte startByte) {
  byte byteCount = 0;
  byte zoneStart = 0;
  if (startByte == 5) zoneStart = 4;

  for (byte zoneGroup = zoneStart; zoneGroup < zoneStart + 4; zoneGroup++) {
    programZones[zoneGroup] = dsc.panelData[startByte + byteCount];
    byteCount++;
  }

  if (ws.count()) {
    char outas[512];
    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();
    root["program_zone_0"] = programZones[0];
    root["program_zone_1"] = programZones[1];
    root["program_zone_2"] = programZones[2];
    root["program_zone_3"] = programZones[3];
    root["program_zone_4"] = programZones[4];
    root["program_zone_5"] = programZones[5];
    root["program_zone_6"] = programZones[6];
    root["program_zone_7"] = programZones[7];
    serializeJson(root, outas);
    ws.textAll(outas);
  }
}


void processEventBufferAA() {
  if (extendedBuffer) return;  // Skips 0xAA data when 0xEC extended event buffer data is available

  char eventInfo[45] = "Event: ";
  char charBuffer[4];
  itoa(dsc.panelData[7], charBuffer, 10);
  if (dsc.panelData[7] < 10) strcat(eventInfo, "00");
  else if (dsc.panelData[7] < 100) strcat(eventInfo, "0");
  strcat(eventInfo, charBuffer);
  strcat(eventInfo, " | ");

  byte dscYear3 = dsc.panelData[2] >> 4;
  byte dscYear4 = dsc.panelData[2] & 0x0F;
  byte dscMonth = dsc.panelData[2 + 1] << 2; dscMonth >>= 4;
  byte dscDay1 = dsc.panelData[2 + 1] << 6; dscDay1 >>= 3;
  byte dscDay2 = dsc.panelData[2 + 2] >> 5;
  byte dscDay = dscDay1 | dscDay2;
  byte dscHour = dsc.panelData[2 + 2] & 0x1F;
  byte dscMinute = dsc.panelData[2 + 3] >> 2;

  if (dscYear3 >= 7) strcat(eventInfo, "19");
  else strcat(eventInfo, "20");
  itoa(dscYear3, charBuffer, 10);
  strcat(eventInfo, charBuffer);
  itoa(dscYear4, charBuffer, 10);
  strcat(eventInfo, charBuffer);
  strcat(eventInfo, ".");
  if (dscMonth < 10) strcat(eventInfo, "0");
  itoa(dscMonth, charBuffer, 10);
  strcat(eventInfo, charBuffer);
  strcat(eventInfo, ".");
  if (dscDay < 10) strcat(eventInfo, "0");
  itoa(dscDay, charBuffer, 10);
  strcat(eventInfo, charBuffer);
  strcat(eventInfo, " ");
  if (dscHour < 10) strcat(eventInfo, "0");
  itoa(dscHour, charBuffer, 10);
  strcat(eventInfo, charBuffer);
  strcat(eventInfo, ":");
  if (dscMinute < 10) strcat(eventInfo, "0");
  itoa(dscMinute, charBuffer, 10);
  strcat(eventInfo, charBuffer);

  strcat(eventInfo, " | Partition ");
  itoa(dsc.panelData[3] >> 6, charBuffer, 10);
  strcat(eventInfo, charBuffer);

  if (ws.count()) {
    char outas[128];
    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();
    root["event_info"] = eventInfo;
    serializeJson(root, outas);
    ws.textAll(outas);
  }

  switch (dsc.panelData[5] & 0x03) {
    case 0x00: printPanelStatus0(6); break;
    case 0x01: printPanelStatus1(6); break;
    case 0x02: printPanelStatus2(6); break;
    case 0x03: printPanelStatus3(6); break;
  }
}


void processEventBufferEC() {
  if (!extendedBuffer) extendedBuffer = true;

  char eventInfo[45] = "Event: ";
  char charBuffer[4];
  int eventNumber = dsc.panelData[9] + ((dsc.panelData[4] >> 6) * 256);
  itoa(eventNumber, charBuffer, 10);
  if (eventNumber < 10) strcat(eventInfo, "00");
  else if (eventNumber < 100) strcat(eventInfo, "0");
  strcat(eventInfo, charBuffer);
  strcat(eventInfo, " | ");

  byte dscYear3 = dsc.panelData[3] >> 4;
  byte dscYear4 = dsc.panelData[3] & 0x0F;
  byte dscMonth = dsc.panelData[4] << 2; dscMonth >>= 4;
  byte dscDay1 = dsc.panelData[4] << 6; dscDay1 >>= 3;
  byte dscDay2 = dsc.panelData[5] >> 5;
  byte dscDay = dscDay1 | dscDay2;
  byte dscHour = dsc.panelData[5] & 0x1F;
  byte dscMinute = dsc.panelData[6] >> 2;

  if (dscYear3 >= 7) strcat(eventInfo, "19");
  else strcat(eventInfo, "20");
  itoa(dscYear3, charBuffer, 10);
  strcat(eventInfo, charBuffer);
  itoa(dscYear4, charBuffer, 10);
  strcat(eventInfo, charBuffer);
  strcat(eventInfo, ".");
  if (dscMonth < 10) strcat(eventInfo, "0");
  itoa(dscMonth, charBuffer, 10);
  strcat(eventInfo, charBuffer);
  strcat(eventInfo, ".");
  if (dscDay < 10) strcat(eventInfo, "0");
  itoa(dscDay, charBuffer, 10);
  strcat(eventInfo, charBuffer);
  strcat(eventInfo, " ");
  if (dscHour < 10) strcat(eventInfo, "0");
  itoa(dscHour, charBuffer, 10);
  strcat(eventInfo, charBuffer);
  strcat(eventInfo, ":");
  if (dscMinute < 10) strcat(eventInfo, "0");
  itoa(dscMinute, charBuffer, 10);
  strcat(eventInfo, charBuffer);

  if (dsc.panelData[2] != 0) {
    strcat(eventInfo, " | Partition ");

    byte bitCount = 0;
    for (byte bit = 0; bit <= 7; bit++) {
      if (bitRead(dsc.panelData[2], bit)) {
        itoa((bitCount + 1), charBuffer, 10);
      }
      bitCount++;
    }
    strcat(eventInfo, charBuffer);
  }

  if (ws.count()) {
    char outas[128];
    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();
    root["event_info"] = eventInfo;
    serializeJson(root, outas);
    ws.textAll(outas);
  }

  switch (dsc.panelData[7]) {
    case 0x00: printPanelStatus0(8); break;
    case 0x01: printPanelStatus1(8); break;
    case 0x02: printPanelStatus2(8); break;
    case 0x03: printPanelStatus3(8); break;
    case 0x04: printPanelStatus4(8); break;
    case 0x05: printPanelStatus5(8); break;
    case 0x14: printPanelStatus14(8); break;
    case 0x16: printPanelStatus16(8); break;
    case 0x17: printPanelStatus17(8); break;
    case 0x18: printPanelStatus18(8); break;
    case 0x1B: printPanelStatus1B(8); break;
  }
}


void printPanelStatus0(byte panelByte) {
  bool decoded = true;
  if (!ws.count()) return;

  char outas[128];
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();
  switch (dsc.panelData[panelByte]) {
    case 0x49: root["lcd_upper"] = "Duress alarm";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0x4A: root["lcd_upper"] = "Disarmed:";
               root["lcd_lower"] = "Alarm memory"; break;
    case 0x4B: root["lcd_upper"] = "Recent";
               root["lcd_lower"] = "closing alarm"; break;
    case 0x4C: root["lcd_upper"] = "Zone expander";
               root["lcd_lower"] = "suprvis. alarm"; break;
    case 0x4D: root["lcd_upper"] = "Zone expander";
               root["lcd_lower"] = "suprvis. restore"; break;
    case 0x4E: root["lcd_upper"] = "Keypad Fire";
               root["lcd_lower"] = "alarm"; break;
    case 0x4F: root["lcd_upper"] = "Keypad Aux";
               root["lcd_lower"] = "alarm"; break;
    case 0x50: root["lcd_upper"] = "Keypad Panic";
               root["lcd_lower"] = "alarm"; break;
    case 0x51: root["lcd_upper"] = "Auxiliary input";
               root["lcd_lower"] = "alarm"; break;
    case 0x52: root["lcd_upper"] = "Keypad Fire";
               root["lcd_lower"] = "alarm restored"; break;
    case 0x53: root["lcd_upper"] = "Keypad Aux";
               root["lcd_lower"] = "alarm restored"; break;
    case 0x54: root["lcd_upper"] = "Keypad Panic";
               root["lcd_lower"] = "alarm restored"; break;
    case 0x55: root["lcd_upper"] = "Auxiliary input";
               root["lcd_lower"] = "alarm restored"; break;
    case 0x98: root["lcd_upper"] = "Keypad";
               root["lcd_lower"] = "lockout"; break;
    case 0xBE: root["lcd_upper"] = "Armed:";
               root["lcd_lower"] = "Partial"; break;
    case 0xBF: root["lcd_upper"] = "Armed:";
               root["lcd_lower"] = "Special"; break;
    case 0xE5: root["lcd_upper"] = "Auto-arm";
               root["lcd_lower"] = "cancelled"; break;
    case 0xE6: root["lcd_upper"] = "Disarmed:";
               root["lcd_lower"] = "Special"; break;
    case 0xE7: root["lcd_upper"] = "Panel battery";
               root["lcd_lower"] = "trouble"; break;
    case 0xE8: root["lcd_upper"] = "Panel AC power";
               root["lcd_lower"] = "trouble"; break;
    case 0xE9: root["lcd_upper"] = "Bell trouble";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0xEA: root["lcd_upper"] = "Fire zone";
               root["lcd_lower"] = "trouble"; break;
    case 0xEB: root["lcd_upper"] = "Panel aux supply";
               root["lcd_lower"] = "trouble"; break;
    case 0xEC: root["lcd_upper"] = "Telephone line";
               root["lcd_lower"] = "trouble"; break;
    case 0xEF: root["lcd_upper"] = "Panel battery";
               root["lcd_lower"] = "restored"; break;
    case 0xF0: root["lcd_upper"] = "Panel AC power";
               root["lcd_lower"] = "restored"; break;
    case 0xF1: root["lcd_upper"] = "Bell restored";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0xF2: root["lcd_upper"] = "Fire zone";
               root["lcd_lower"] = "trouble restored"; break;
    case 0xF3: root["lcd_upper"] = "Panel aux supply";
               root["lcd_lower"] = "restored"; break;
    case 0xF4: root["lcd_upper"] = "Telephone line";
               root["lcd_lower"] = "restored"; break;
    case 0xF7: root["lcd_upper"] = "Phone 1 FTC";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0xF8: root["lcd_upper"] = "Phone 2 FTC";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0xF9: root["lcd_upper"] = "Event buffer";
               root["lcd_lower"] = "threshold"; break;
    case 0xFA: root["lcd_upper"] = "DLS lead-in";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0xFB: root["lcd_upper"] = "DLS lead-out";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0xFE: root["lcd_upper"] = "Periodic test";
               root["lcd_lower"] = "transmission"; break;
    case 0xFF: root["lcd_upper"] = "System test";
               root["lcd_lower"] = "&nbsp;"; break;
    default: decoded = false;
  }

  char lcdMessage[20];
  char charBuffer[4];

  if (dsc.panelData[panelByte] >= 0x09 && dsc.panelData[panelByte] <= 0x28) {
    strcpy(lcdMessage, "Zone alarm: ");
    itoa(dsc.panelData[panelByte] - 8, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "&nbsp;";
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x29 && dsc.panelData[panelByte] <= 0x48) {
    root["lcd_upper"] = "Zone alarm";
    strcpy(lcdMessage, "restored: ");
    itoa(dsc.panelData[panelByte] - 40, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x56 && dsc.panelData[panelByte] <= 0x75) {
    strcpy(lcdMessage, "Zone tamper: ");
    itoa(dsc.panelData[panelByte] - 85, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "&nbsp;";
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x76 && dsc.panelData[panelByte] <= 0x95) {
    root["lcd_upper"] = "Zone tamper";
    strcpy(lcdMessage, "restored: ");
    itoa(dsc.panelData[panelByte] - 117, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x99 && dsc.panelData[panelByte] <= 0xBD) {
    root["lcd_upper"] = "Armed:";
    byte dscCode = dsc.panelData[panelByte] - 0x98;
    if (dscCode >= 35) dscCode += 5;
    if (dscCode == 40) strcpy(lcdMessage, "Master code ");
    else strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0xC0 && dsc.panelData[panelByte] <= 0xE4) {
    root["lcd_upper"] = "Disarmed:";
    byte dscCode = dsc.panelData[panelByte] - 0xBF;
    if (dscCode >= 35) dscCode += 5;
    if (dscCode == 40) strcpy(lcdMessage, "Master code ");
    else strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (!decoded) {
    root["lcd_upper"] = "Unknown data";
    root["lcd_lower"] = "&nbsp;";
  }
  serializeJson(root, outas);
  ws.textAll(outas);
}


void printPanelStatus1(byte panelByte) {
  bool decoded = true;
  if (!ws.count()) return;

  char outas[128];
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  switch (dsc.panelData[panelByte]) {
    case 0x03: root["lcd_upper"] = "Cross zone";
               root["lcd_lower"] = "alarm"; break;
    case 0x04: root["lcd_upper"] = "Delinquency";
               root["lcd_lower"] = "alarm"; break;
    case 0x05: root["lcd_upper"] = "Late to close";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0x29: root["lcd_upper"] = "Downloading";
               root["lcd_lower"] = "forced answer"; break;
    case 0x2B: root["lcd_upper"] = "Armed:";
               root["lcd_lower"] = "Auto-arm"; break;
    case 0xAC: root["lcd_upper"] = "Exit installer";
               root["lcd_lower"] = "programming"; break;
    case 0xAD: root["lcd_upper"] = "Enter installer";
               root["lcd_lower"] = "programming"; break;
    case 0xAE: root["lcd_upper"] = "Walk test";
               root["lcd_lower"] = "end"; break;
    case 0xAF: root["lcd_upper"] = "Walk test";
               root["lcd_lower"] = "begin"; break;
    case 0xD0: root["lcd_upper"] = "Command";
               root["lcd_lower"] = "output 4"; break;
    case 0xD1: root["lcd_upper"] = "Exit fault";
               root["lcd_lower"] = "pre-alert"; break;
    case 0xD2: root["lcd_upper"] = "Armed:";
               root["lcd_lower"] = "Entry delay"; break;
    case 0xD3: root["lcd_upper"] = "Downlook remote";
               root["lcd_lower"] = "trigger"; break;
    default: decoded = false;
  }

  char lcdMessage[20];
  char charBuffer[4];
  if (dsc.panelData[panelByte] >= 0x24 && dsc.panelData[panelByte] <= 0x28) {
    byte dscCode = dsc.panelData[panelByte] - 0x03;
    if (dscCode >= 35) dscCode += 5;
    if (dscCode == 40) strcpy(lcdMessage, "Master code ");
    else strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "&nbsp;";
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x2C && dsc.panelData[panelByte] <= 0x4B) {
    root["lcd_upper"] = "Zone battery";
    strcpy(lcdMessage, "restored: ");
    itoa(dsc.panelData[panelByte] - 43, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x4C && dsc.panelData[panelByte] <= 0x6B) {
    root["lcd_upper"] = "Zone battery";
    strcpy(lcdMessage, "low: ");
    itoa(dsc.panelData[panelByte] - 75, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x6C && dsc.panelData[panelByte] <= 0x8B) {
    root["lcd_upper"] = "Zone fault";
    strcpy(lcdMessage, "restored: ");
    itoa(dsc.panelData[panelByte] - 107, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x8C && dsc.panelData[panelByte] <= 0xAB) {
    strcpy(lcdMessage, "Zone fault: ");
    itoa(dsc.panelData[panelByte] - 139, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "&nbsp;";
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0xB0 && dsc.panelData[panelByte] <= 0xCF) {
    strcpy(lcdMessage, "Zone bypass: ");
    itoa(dsc.panelData[panelByte] - 175, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "&nbsp;";
    decoded = true;
  }

  if (!decoded) {
    root["lcd_upper"] = "Unknown data";
    root["lcd_lower"] = "&nbsp;";
  }
  serializeJson(root, outas);
  ws.textAll(outas);
}


void printPanelStatus2(byte panelByte) {
  bool decoded = true;
  if (!ws.count()) return;

  char outas[128];
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();
  switch (dsc.panelData[panelByte]) {
    case 0x2A: root["lcd_upper"] = "Quick exit";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0x63: root["lcd_upper"] = "Keybus fault";
               root["lcd_lower"] = "restored"; break;
    case 0x64: root["lcd_upper"] = "Keybus fault";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0x66: root["lcd_upper"] = "*1: Zone bypass";
               root["lcd_lower"] = "programming"; break;
    case 0x67: root["lcd_upper"] = "Command";
               root["lcd_lower"] = "output 1"; break;
    case 0x68: root["lcd_upper"] = "Command";
               root["lcd_lower"] = "output 2"; break;
    case 0x69: root["lcd_upper"] = "Command";
               root["lcd_lower"] = "output 3"; break;
    case 0x8C: root["lcd_upper"] = "Cold start";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0x8D: root["lcd_upper"] = "Warm start";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0x8E: root["lcd_upper"] = "Panel factory";
               root["lcd_lower"] = "default"; break;
    case 0x91: root["lcd_upper"] = "Swinger shutdown";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0x93: root["lcd_upper"] = "Disarmed:";
               root["lcd_lower"] = "Keyswitch"; break;
    case 0x96: root["lcd_upper"] = "Armed:";
               root["lcd_lower"] = "Keyswitch"; break;
    case 0x97: root["lcd_upper"] = "Armed:";
               root["lcd_lower"] = "Keypad away"; break;
    case 0x98: root["lcd_upper"] = "Armed:";
               root["lcd_lower"] = "Quick-arm"; break;
    case 0x99: root["lcd_upper"] = "Activate";
               root["lcd_lower"] = "stay/away zones"; break;
    case 0x9A: root["lcd_upper"] = "Armed:";
               root["lcd_lower"] = "Stay"; break;
    case 0x9B: root["lcd_upper"] = "Armed:";
               root["lcd_lower"] = "Away"; break;
    case 0x9C: root["lcd_upper"] = "Armed:";
               root["lcd_lower"] = "No entry delay"; break;
    case 0xFF: root["lcd_upper"] = "Zone expander";
               root["lcd_lower"] = "trouble: 1"; break;
    default: decoded = false;
  }

  char lcdMessage[20];
  char charBuffer[4];

  if (dsc.panelData[panelByte] >= 0x9E && dsc.panelData[panelByte] <= 0xC2) {
    byte dscCode = dsc.panelData[panelByte] - 0x9D;
    root["lcd_upper"] = "*1: ";
    if (dscCode >= 35) dscCode += 5;
    if (dscCode == 40) strcpy(lcdMessage, "Master code ");
    else strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0xC3 && dsc.panelData[panelByte] <= 0xC5) {
    byte dscCode = dsc.panelData[panelByte] - 0xA0;
    root["lcd_upper"] = "*5: ";
    if (dscCode >= 35) dscCode += 5;
    if (dscCode == 40) strcpy(lcdMessage, "Master code ");
    else strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0xC6 && dsc.panelData[panelByte] <= 0xE5) {
    byte dscCode = dsc.panelData[panelByte] - 0xC5;
    if (dscCode >= 35) dscCode += 5;
    if (dscCode == 40) strcpy(lcdMessage, "Master code ");
    else strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "&nbsp;";
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0xE6 && dsc.panelData[panelByte] <= 0xE8) {
    byte dscCode = dsc.panelData[panelByte] - 0xC3;
    root["lcd_upper"] = "*6: ";
    if (dscCode >= 35) dscCode += 5;
    if (dscCode == 40) strcpy(lcdMessage, "Master code ");
    else strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0xE9 && dsc.panelData[panelByte] <= 0xF0) {
    root["lcd_upper"] = "Keypad restored:";
    strcpy(lcdMessage, "Slot ");
    itoa(dsc.panelData[panelByte] - 232, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0xF1 && dsc.panelData[panelByte] <= 0xF8) {
    root["lcd_upper"] = "Keypad trouble:";
    strcpy(lcdMessage, "Slot ");
    itoa(dsc.panelData[panelByte] - 240, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0xF9 && dsc.panelData[panelByte] <= 0xFE) {
    strcpy(lcdMessage, "Zone expander ");
    itoa(dsc.panelData[panelByte] - 248, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "restored";
    decoded = true;
  }

  if (!decoded) {
    root["lcd_upper"] = "Unknown data";
    root["lcd_lower"] = "&nbsp;";
  }
  serializeJson(root, outas);
  ws.textAll(outas);
}


void printPanelStatus3(byte panelByte) {
  bool decoded = true;
  if (!ws.count()) return;

  char outas[128];
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  switch (dsc.panelData[panelByte]) {
    case 0x05: root["lcd_upper"] = "PC/RF5132:";
               root["lcd_lower"] = "Suprvis. restore"; break;
    case 0x06: root["lcd_upper"] = "PC/RF5132:";
               root["lcd_lower"] = "Suprvis. trouble"; break;
    case 0x09: root["lcd_upper"] = "PC5204:";
               root["lcd_lower"] = "Suprvis. restore"; break;
    case 0x0A: root["lcd_upper"] = "PC5204:";
               root["lcd_lower"] = "Suprvis. trouble"; break;
    case 0x17: root["lcd_upper"] = "Zone expander 7";
               root["lcd_lower"] = "restored"; break;
    case 0x18: root["lcd_upper"] = "Zone expander 7";
               root["lcd_lower"] = "trouble"; break;
    case 0x41: root["lcd_upper"] = "PC/RF5132:";
               root["lcd_lower"] = "Tamper restored"; break;
    case 0x42: root["lcd_upper"] = "PC/RF5132: Tamper";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0x43: root["lcd_upper"] = "PC5208: Tamper";
               root["lcd_lower"] = "restored"; break;
    case 0x44: root["lcd_upper"] = "PC5208: Tamper";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0x45: root["lcd_upper"] = "PC5204: Tamper";
               root["lcd_lower"] = "restored"; break;
    case 0x46: root["lcd_upper"] = "PC5204: Tamper";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0x51: root["lcd_upper"] = "Zone expander 7";
               root["lcd_lower"] = "tamper restored"; break;
    case 0x52: root["lcd_upper"] = "Zone expander 7";
               root["lcd_lower"] = "tamper"; break;
    case 0xB3: root["lcd_upper"] = "PC5204:";
               root["lcd_lower"] = "Battery restored"; break;
    case 0xB4: root["lcd_upper"] = "PC5204:";
               root["lcd_lower"] = "Battery trouble"; break;
    case 0xB5: root["lcd_upper"] = "PC5204: Aux";
               root["lcd_lower"] = "supply restored"; break;
    case 0xB6: root["lcd_upper"] = "PC5204: Aux";
               root["lcd_lower"] = "supply trouble"; break;
    case 0xB7: root["lcd_upper"] = "PC5204: Output 1";
               root["lcd_lower"] = "restored"; break;
    case 0xB8: root["lcd_upper"] = "PC5204: Output 1";
               root["lcd_lower"] = "trouble"; break;
    case 0xFF: root["lcd_upper"] = "Extended status";
               root["lcd_lower"] = "&nbsp;"; break;
    default: decoded = false;
  }

  char lcdMessage[20];
  char charBuffer[4];

  if (dsc.panelData[panelByte] <= 0x04) {
    strcpy(lcdMessage, "Zone expander ");
    itoa(dsc.panelData[panelByte] + 2, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "trouble";
    return;
  }

  if (dsc.panelData[panelByte] >= 0x35 && dsc.panelData[panelByte] <= 0x3A) {
    strcpy(lcdMessage, "Zone expander ");
    itoa(dsc.panelData[panelByte] - 52, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "tamper restored";
    return;
  }

  if (dsc.panelData[panelByte] >= 0x3B && dsc.panelData[panelByte] <= 0x40) {
    strcpy(lcdMessage, "Zone expander ");
    itoa(dsc.panelData[panelByte] - 58, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "tamper";
    return;
  }

  if (!decoded) {
    root["lcd_upper"] = "Unknown data";
    root["lcd_lower"] = "&nbsp;";
  }
  serializeJson(root, outas);
  ws.textAll(outas);
}


void printPanelStatus4(byte panelByte) {
  bool decoded = true;
  if (!ws.count()) return;

  char outas[128];
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  switch (dsc.panelData[panelByte]) {
    case 0x86: root["lcd_upper"] = "Periodic test";
               root["lcd_lower"] = "with trouble"; break;
    case 0x87: root["lcd_upper"] = "Exit fault";
               root["lcd_lower"] = "&nbsp;"; break;
    case 0x89: root["lcd_upper"] = "Alarm cancelled";
               root["lcd_lower"] = "&nbsp;"; break;
    default: decoded = false;
  }

  char lcdMessage[20];
  char charBuffer[4];

  if (dsc.panelData[panelByte] <= 0x1F) {
    strcpy(lcdMessage, "Zone alarm: ");
    itoa(dsc.panelData[panelByte] + 33, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "&nbsp;";
    decoded = true;
  }

  else if (dsc.panelData[panelByte] >= 0x20 && dsc.panelData[panelByte] <= 0x3F) {
    root["lcd_upper"] = "Zone alarm";
    strcpy(lcdMessage, "restored: ");
    itoa(dsc.panelData[panelByte] + 1, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  else if (dsc.panelData[panelByte] >= 0x40 && dsc.panelData[panelByte] <= 0x5F) {
    strcpy(lcdMessage, "Zone tamper: ");
    itoa(dsc.panelData[panelByte] - 31, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "&nbsp;";
    decoded = true;
  }

  else if (dsc.panelData[panelByte] >= 0x60 && dsc.panelData[panelByte] <= 0x7F) {
    root["lcd_upper"] = "Zone tamper";
    strcpy(lcdMessage, "restored: ");
    itoa(dsc.panelData[panelByte] - 63, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (!decoded) {
    root["lcd_upper"] = "Unknown data";
    root["lcd_lower"] = "&nbsp;";
  }
  serializeJson(root, outas);
  ws.textAll(outas);
}


void printPanelStatus5(byte panelByte) {
  bool decoded = true;
  if (!ws.count()) return;

  char outas[128];
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  char lcdMessage[20];
  char charBuffer[4];

  if (dsc.panelData[panelByte] <= 0x39) {
    byte dscCode = dsc.panelData[panelByte] + 0x23;
    root["lcd_upper"] = "Armed: ";
    if (dscCode >= 40) dscCode += 3;
    strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x3A && dsc.panelData[panelByte] <= 0x73) {
    byte dscCode = dsc.panelData[panelByte] - 0x17;
    root["lcd_upper"] = "Disarmed: ";
    if (dscCode >= 40) dscCode += 3;
    strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (!decoded) {
    root["lcd_upper"] = "Unknown data";
    root["lcd_lower"] = "&nbsp;";
  }
  serializeJson(root, outas);
  ws.textAll(outas);
}


void printPanelStatus14(byte panelByte) {
  bool decoded = true;
  if (!ws.count()) return;

  char outas[128];
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  switch (dsc.panelData[panelByte]) {
    case 0xC0: root["lcd_upper"] = "TLink";
               root["lcd_lower"] = "com fault"; break;
    case 0xC2: root["lcd_upper"] = "Tlink";
               root["lcd_lower"] = "network fault"; break;
    case 0xC4: root["lcd_upper"] = "TLink receiver";
               root["lcd_lower"] = "trouble"; break;
    case 0xC5: root["lcd_upper"] = "TLink receiver";
               root["lcd_lower"] = "restored"; break;
    default: decoded = false;
  }

  if (!decoded) {
    root["lcd_upper"] = "Unknown data";
    root["lcd_lower"] = "&nbsp;";
  }
  serializeJson(root, outas);
  ws.textAll(outas);
}


void printPanelStatus16(byte panelByte) {
  bool decoded = true;
  if (!ws.count()) return;

  char outas[128];
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  switch (dsc.panelData[panelByte]) {
    case 0x80: root["lcd_upper"] = "Trouble";
               root["lcd_lower"] = "acknowledged"; break;
    case 0x81: root["lcd_upper"] = "RF delinquency";
               root["lcd_lower"] = "trouble"; break;
    case 0x82: root["lcd_upper"] = "RF delinquency";
               root["lcd_lower"] = "restore"; break;
    default: decoded = false;
  }

  if (!decoded) {
    root["lcd_upper"] = "Unknown data";
    root["lcd_lower"] = "&nbsp;";
  }
  serializeJson(root, outas);
  ws.textAll(outas);
}


void printPanelStatus17(byte panelByte) {
  bool decoded = true;
  if (!ws.count()) return;

  char outas[128];
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  char lcdMessage[20];
  char charBuffer[4];

  if (dsc.panelData[panelByte] >= 0x4A && dsc.panelData[panelByte] <= 0x83) {
    byte dscCode = dsc.panelData[panelByte] - 0x27;
    root["lcd_upper"] = "*1: ";
    if (dscCode >= 40) dscCode += 3;
    strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] <= 0x24) {
    byte dscCode = dsc.panelData[panelByte] + 1;
    root["lcd_upper"] = "*2: ";
    if (dscCode >= 40) dscCode += 3;
    strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x84 && dsc.panelData[panelByte] <= 0xBD) {
    byte dscCode = dsc.panelData[panelByte] - 0x61;
    root["lcd_upper"] = "*2: ";
    if (dscCode >= 40) dscCode += 3;
    strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x25 && dsc.panelData[panelByte] <= 0x49) {
    byte dscCode = dsc.panelData[panelByte] - 0x24;
    root["lcd_upper"] = "*3: ";
    if (dscCode >= 40) dscCode += 3;
    strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0xBE && dsc.panelData[panelByte] <= 0xF7) {
    byte dscCode = dsc.panelData[panelByte] - 0x9B;
    root["lcd_upper"] = "*3: ";
    if (dscCode >= 40) dscCode += 3;
    strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (!decoded) {
    root["lcd_upper"] = "Unknown data";
    root["lcd_lower"] = "&nbsp;";
  }
  serializeJson(root, outas);
  ws.textAll(outas);
}


void printPanelStatus18(byte panelByte) {
  bool decoded = true;
  if (!ws.count()) return;

  char outas[128];
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  char lcdMessage[20];
  char charBuffer[4];

  if (dsc.panelData[panelByte] <= 0x39) {
    byte dscCode = dsc.panelData[panelByte] + 0x23;
    if (dscCode >= 40) dscCode += 3;
    strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_upper"] = lcdMessage;
    root["lcd_lower"] = "&nbsp;";
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x3A && dsc.panelData[panelByte] <= 0x95) {
    byte dscCode = dsc.panelData[panelByte] - 0x39;
    root["lcd_upper"] = "*5: ";
    if (dscCode >= 40) dscCode += 3;
    strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (dsc.panelData[panelByte] >= 0x96 && dsc.panelData[panelByte] <= 0xF1) {
    byte dscCode = dsc.panelData[panelByte] - 0x95;
    root["lcd_upper"] = "*6: ";
    if (dscCode >= 40) dscCode += 3;
    strcpy(lcdMessage, "Access code ");
    itoa(dscCode, charBuffer, 10);
    strcat(lcdMessage, charBuffer);
    root["lcd_lower"] = lcdMessage;
    decoded = true;
  }

  if (!decoded) {
    root["lcd_upper"] = "Unknown data";
    root["lcd_lower"] = "&nbsp;";
  }
  serializeJson(root, outas);
  ws.textAll(outas);
}


void printPanelStatus1B(byte panelByte) {
  bool decoded = true;
  if (!ws.count()) return;

  char outas[128];
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  switch (dsc.panelData[panelByte]) {
    case 0xF1: root["lcd_upper"] = "System reset";
               root["lcd_lower"] = "transmission"; break;
    default: decoded = false;
  }

  if (!decoded) {
    root["lcd_upper"] = "Unknown data";
    root["lcd_lower"] = "&nbsp;";
  }
  serializeJson(root, outas);
  ws.textAll(outas);
}


void pauseZones() {
  pausedZones = true;
  if (ws.count()) {
    char outas[512];
    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();
    root["open_zone_0"] = 0;
    root["open_zone_1"] = 0;
    root["open_zone_2"] = 0;
    root["open_zone_3"] = 0;
    root["open_zone_4"] = 0;
    root["open_zone_5"] = 0;
    root["open_zone_6"] = 0;
    root["open_zone_7"] = 0;
    serializeJson(root, outas);
    ws.textAll(outas);
  }
}


void resetZones() {
  pausedZones = false;
  dsc.openZonesStatusChanged = true;
  if (ws.count()) {
    char outas[128];
    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();
    root["event_info"] = "";
    serializeJson(root, outas);
    ws.textAll(outas);
  }
}


void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->printf("{\"connected_id\": %u}", client->id());
    forceUpdate = true;
    client->ping();
    ws_ping_pong.restart();
  }

  else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
    if (ws.count() <= 0) {
      ws_ping_pong.stop();
    }
  }

  else if (type == WS_EVT_ERROR) {
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  }

  else if (type == WS_EVT_PONG) {
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char*)data : "");
  }

  else if (type == WS_EVT_DATA) {
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if (info->final && info->index == 0 && info->len == len) {
      //the whole message is in a single frame and we got all of it's data
      //Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

      if (info->opcode == WS_TEXT) {
        for (size_t i = 0; i < info->len; i++) {
          msg += (char) data[i];
        }
      }

      if (info->opcode == WS_TEXT) {
        StaticJsonDocument<200> doc;
        auto err = deserializeJson(doc, msg);
        if (!err) {
          JsonObject root = doc.as<JsonObject>();
          if (root.containsKey("btn_single_click")) {
            char *tmp = (char *)root["btn_single_click"].as<char*>();
            char * const sep_at = strchr(tmp, '_');
            if (sep_at != NULL)            {
              *sep_at = '\0';
              dsc.write(sep_at + 1);
            }
          }
        }
      }
    }

    else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0) {
        if (info->num == 0) {
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        }
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

      if (info->opcode == WS_TEXT) {
        for (size_t i = 0; i < info->len; i++) {
          msg += (char) data[i];
        }
      }
      else {
        char buff[3];
        for (size_t i = 0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }

      Serial.printf("%s\n", msg.c_str());

      if ((info->index + len) == info->len) {
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final) {
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT) client->text("I got your text message");
          else client->binary("I got your binary message");
        }
      }
    }
  }
}
