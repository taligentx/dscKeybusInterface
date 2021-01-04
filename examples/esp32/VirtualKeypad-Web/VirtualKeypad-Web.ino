/*
 *  VirtualKeypad-Web 1.0 (esp32)
 *
 *  Provides a virtual keypad web interface using the esp32 as a standalone web server.
 *
 *  Usage:
 *    1. Install the following libraries directly from each Github repository:
 *         ESPAsyncWebServer: https://github.com/me-no-dev/ESPAsyncWebServer
 *         AsyncTCP: https://github.com/me-no-dev/AsyncTCP
 *    2. Install the following libraries, available in the Arduino IDE Library Manager and
 *       the Platform.io Library Registry:
 *         ArduinoJson: https://github.com/bblanchon/ArduinoJson
 *         Chrono: https://github.com/SofaPirate/Chrono
 *    3. Set the WiFi SSID and password in the sketch.
 *    4. If desired, update the DNS hostname in the sketch.  By default, this is set to
 *       "dsc" and the web interface will be accessible at: http://dsc.local
 *    5. Upload the sketch.
 *    6. Upload the SPIFFS data containing the web server files:
 *         Arduino IDE: Tools > ESP32 Sketch Data Upload
 *    7. Access the virtual keypad web interface by the IP address displayed through
 *       the serial output or http://dsc.local (for clients and networks that support mDNS).
 *
 *  Release notes:
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
 *  Many thanks to Elektrik1 for contributing this example: https://github.com/Elektrik1
 *
 *  This example code is in the public domain.
 */


#include <WiFi.h>
#include <ESPmDNS.h>
#include <dscKeybusInterface.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SPIFFSEditor.h>
#include <ArduinoJson.h>
#include <Chrono.h>

// Settings
char wifiSSID[] = "";
char wifiPassword[] = "";
char dnsHostname[] = "dsc";  // Sets the domain name - if set to "dsc", access via: http://dsc.local
byte dscPartition = 1;       // Set the partition for the keypad

// Configures the Keybus interface with the specified pins
#define dscClockPin 18  // esp32: 4,13,16-39
#define dscReadPin  19  // esp32: 4,13,16-39
#define dscWritePin 21  // esp32: 4,13,16-33

// Initialize components
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Chrono ws_ping_pong(Chrono::SECONDS);
bool partitionChanged, pausedZones;
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
                 root["lcd_lower"] = "                ";
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
                 root["lcd_lower"] = "                "; break;
      case 0x40: root["lcd_upper"] = "Keypad       ";
                 root["lcd_lower"] = "blanked         "; break;
      case 0x8A: root["lcd_upper"] = "Activate     ";
                 root["lcd_lower"] = "stay/away zones "; break;
      case 0x8B: root["lcd_upper"] = "Quick exit   ";
                 root["lcd_lower"] = "                "; break;
      case 0x8E: root["lcd_upper"] = "Invalid      ";
                 root["lcd_lower"] = "option          "; break;
      case 0x8F: root["lcd_upper"] = "Invalid      ";
                 root["lcd_lower"] = "access code     "; break;
      case 0x9E: root["lcd_upper"] = "Enter *      ";
                 root["lcd_lower"] = "function code   "; break;
      case 0x9F: root["lcd_upper"] = "Enter        ";
                 root["lcd_lower"] = "access code     "; break;
      case 0xA0: root["lcd_upper"] = "*1:          ";
                 root["lcd_lower"] = "Zone bypass menu"; break;
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
      case 0xA7: root["lcd_upper"] = "Enter        ";
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
      case 0xBC: root["lcd_upper"] = "New          ";
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
      case 0xE4: root["lcd_upper"] = "*8:          ";
                 root["lcd_lower"] = "Installer menu  "; break;
      case 0xE5: root["lcd_upper"] = "Keypad       ";
                 root["lcd_lower"] = "slot assignment "; break;
      case 0xE6: root["lcd_upper"] = "Input:       ";
                 root["lcd_lower"] = "2 digits        "; break;
      case 0xE7: root["lcd_upper"] = "Input:       ";
                 root["lcd_lower"] = "3 digits        "; break;
      case 0xE8: root["lcd_upper"] = "Input:       ";
                 root["lcd_lower"] = "4 digits        "; break;
      case 0xEA: root["lcd_upper"] = "Reporting    ";
                 root["lcd_lower"] = "code: 2 digits  "; break;
      case 0xEB: root["lcd_upper"] = "Reporting    ";
                 root["lcd_lower"] = "code: 4 digits  "; break;
      case 0xEC: root["lcd_upper"] = "Input:       ";
                 root["lcd_lower"] = "6 digits        "; break;
      case 0xED: root["lcd_upper"] = "Input:       ";
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
                 root["lcd_lower"] = "place. test     "; break;
      case 0xF6: root["lcd_upper"] = "Activate     ";
                 root["lcd_lower"] = "device for test "; break;
      case 0xF7: root["lcd_upper"] = "*8 PGM       ";
                 root["lcd_lower"] = "subsection      "; break;
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
      if (pausedZones) {
        processProgramZones(4);
      }
      break;
    case 0x5D:
      if ((dsc.panelData[2] & 0x04) == 0x04) {  // Alarm memory zones 1-32
        if (pausedZones) {
          processProgramZones(3);
        }
      }
      break;
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
        case 0x18: if ((dsc.panelData[4] & 0x04) == 0x04) yield(); break;  // Alarm memory zones 33-64
      }
      break;
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
