/*
    VirtualKeypad HTTP (esp8266)

    Provides a virtual keypad interface on standalone esp8266 Async HTTP Server using WebSockets.

    Wiring:
        DSC Aux(-) --- esp8266 ground

                                           +--- dscClockPin (esp8266: D1, D2, D8)
        DSC Yellow --- 15k ohm resistor ---|
                                           +--- 10k ohm resistor --- Ground

                                           +--- dscReadPin (esp8266: D1, D2, D8)
        DSC Green ---- 15k ohm resistor ---|
                                           +--- 10k ohm resistor --- Ground

    Virtual keypad (optional):
        DSC Green ---- NPN collector --\
                                        |-- NPN base --- 1k ohm resistor --- dscWritePin (esp8266: D1, D2, D8)
              Ground --- NPN emitter --/

    Power (when disconnected from USB):
        DSC Aux(+) ---+--- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
                      |
                      +--- 3.3v voltage regulator --- esp8266 bare module VCC pin (ESP-12, etc)

    Virtual keypad uses an NPN transistor to pull the data line low - most small signal NPN transistors should
    be suitable, for example:
     -- 2N3904
     -- BC547, BC548, BC549

    Issues and (especially) pull requests are welcome:
    https://github.com/taligentx/dscKeybusInterface

    This example code is in the public domain.
*/

#include <ESP8266WiFi.h>
#include <dscKeybusInterface.h>

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <ArduinoJson.h>


char wifiSSID[] = "";
char wifiPassword[] = "";

// Configures the Keybus interface with the specified pins - dscWritePin is
// optional, leaving it out disables the virtual keypad
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)

byte ligths_sent = 0x00;
byte last_open_zones[8];

byte partition = 0;

bool force_send_status_for_new_client = false;

dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
static AsyncClient * aClient = NULL;


bool partitionChanged;
byte viewPartition = 1;
const char* lcdPartition = "Partition ";

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->printf("{\"connected_id\": %u}", client->id());
    force_send_status_for_new_client = true;

    client->ping();

  }
}

void setup() {
  Serial.begin(115200);
 
  WiFi.disconnect();

  WiFi.mode(WIFI_STA);

  WiFi.begin(wifiSSID, wifiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.println(WiFi.localIP());

  SPIFFS.begin();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);


  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });


  MDNS.begin("DSC");

  server.begin();

  MDNS.addService("http", "tcp", 80);

  ArduinoOTA.begin();


  dsc.begin();


}


void loop() {
  ArduinoOTA.handle();


  if (dsc.handlePanel() && (dsc.statusChanged || force_send_status_for_new_client)) {  // Processes data only when a valid Keybus command has been read
    dsc.statusChanged = false;                     // Resets the status flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // handlePanel() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
    if (dsc.bufferOverflow) {
      Serial.println("Keybus buffer overflow");
    }
    dsc.bufferOverflow = false;

    // Checks status for the currently viewed partition
    partition = viewPartition - 1;

    setLights(partition);
    setStatus(partition);

    if (dsc.fireChanged[partition]) {
      dsc.fireChanged[partition] = false;  // Resets the fire status flag
      printFire(partition);
    }

    // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones
    //   openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
    //   openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
    //   ...
    //   openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
    if (dsc.openZonesStatusChanged || force_send_status_for_new_client) {
      dsc.openZonesStatusChanged = false;                           // Resets the open zones status flag

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
    if (dsc.alarmZonesStatusChanged || force_send_status_for_new_client) {
      dsc.alarmZonesStatusChanged = false;                           // Resets the alarm zones status flag
      for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
        for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
          if (bitRead(dsc.alarmZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual alarm zone status flag
            bitWrite(dsc.alarmZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual alarm zone status flag
            if (bitRead(dsc.alarmZones[zoneGroup], zoneBit)) {
              /*String out;
                StaticJsonDocument<200> doc;
                JsonObject root = doc.to<JsonObject>();
                root["alam_on_zone"] = zoneBit + 1 + (zoneGroup * 8);
                serializeJson(root, out);
                ws.textAll(out);*/
            } else {
              /* String out;
                StaticJsonDocument<200> doc;
                JsonObject root = doc.to<JsonObject>();
                root["alarm_off_zone"] = zoneBit + 1 + (zoneGroup * 8);
                serializeJson(root, out);
                ws.textAll(out);*/
            }
          }
        }
      }
    }

    if (dsc.powerChanged) {
      dsc.powerChanged = false;  // Resets the power trouble status flag
      if (dsc.powerTrouble) {
        //        lcd.clear();
        //        lcd.print(0, 0, "Power trouble");
      }
      else {
        //        lcd.clear();
        //        lcd.print(0, 0, "Power restored");
      }
    }

    if (dsc.batteryChanged) {
      dsc.batteryChanged = false;  // Resets the battery trouble status flag
      if (dsc.batteryTrouble) {
        //        lcd.clear();
        //        lcd.print(0, 0, "Battery trouble");
      }
      else {
        //        lcd.clear();
        //        lcd.print(0, 0, "Battery restored");
      }
    }
    if (force_send_status_for_new_client) {
      force_send_status_for_new_client = false;
    }
  }
}

//BLYNK_WRITE(V0) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('0');
//  Blynk.virtualWrite(V0, 0);
//}
//
//BLYNK_WRITE(V1) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('1');
//  Blynk.virtualWrite(V1, 0);
//}
//
//BLYNK_WRITE(V2) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('2');
//  Blynk.virtualWrite(V2, 0);
//}
//
//BLYNK_WRITE(V3) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('3');
//  Blynk.virtualWrite(V3, 0);
//}
//
//BLYNK_WRITE(V4) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('4');
//  Blynk.virtualWrite(V4, 0);
//}
//
//BLYNK_WRITE(V5) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('5');
//  Blynk.virtualWrite(V5, 0);
//}
//
//BLYNK_WRITE(V6) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('6');
//  Blynk.virtualWrite(V6, 0);
//}
//
//BLYNK_WRITE(V7) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('7');
//  Blynk.virtualWrite(V7, 0);
//}
//
//BLYNK_WRITE(V8) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('8');
//  Blynk.virtualWrite(V8, 0);
//}
//
//BLYNK_WRITE(V9) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('9');
//  Blynk.virtualWrite(V9, 0);
//}
//
//BLYNK_WRITE(V10) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('*');
//  Blynk.virtualWrite(V10, 0);
//}
//
//BLYNK_WRITE(V11) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('#');
//  Blynk.virtualWrite(V11, 0);
//}
//
//BLYNK_WRITE(V12) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('s');
//  Blynk.virtualWrite(V12, 0);
//}
//
//BLYNK_WRITE(V13) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('w');
//  Blynk.virtualWrite(V13, 0);
//}
//
//BLYNK_WRITE(V14) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('f');
//  Blynk.virtualWrite(V14, 0);
//}
//
//BLYNK_WRITE(V15) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('a');
//  Blynk.virtualWrite(V15, 0);
//}
//
//BLYNK_WRITE(V16) {
//  int buttonPressed = param.asInt();
//  if (buttonPressed) dsc.write('p');
//  Blynk.virtualWrite(V16, 0);
//}
//
//BLYNK_WRITE(V30) {
//  switch (param.asInt()) {
//    case 1: {
//        viewPartition = 1;
//        changedPartition(viewPartition - 1);
//        dsc.writePartition = 1;
//        break;
//      }
//    case 2: {
//        viewPartition = 2;
//        changedPartition(viewPartition - 1);
//        dsc.writePartition = 2;
//        break;
//      }
//    case 3: {
//        viewPartition = 3;
//        changedPartition(viewPartition - 1);
//        dsc.writePartition = 3;
//        break;
//      }
//    case 4: {
//        viewPartition = 4;
//        changedPartition(viewPartition - 1);
//        dsc.writePartition = 4;
//        break;
//      }
//    case 5: {
//        viewPartition = 5;
//        changedPartition(viewPartition - 1);
//        dsc.writePartition = 5;
//        break;
//      }
//    case 6: {
//        viewPartition = 6;
//        changedPartition(viewPartition - 1);
//        dsc.writePartition = 6;
//        break;
//      }
//    case 7: {
//        viewPartition = 7;
//        changedPartition(viewPartition - 1);
//        dsc.writePartition = 7;
//        break;
//      }
//    case 8: {
//        viewPartition = 8;
//        changedPartition(viewPartition - 1);
//        dsc.writePartition = 8;
//        break;
//      }
//    default: {
//        viewPartition = 1;
//        changedPartition(viewPartition - 1);
//        dsc.writePartition = 1;
//        break;
//      }
//  }
//  byte position = strlen(lcdPartition);
//  lcd.print(0, 0, "                ");
//  lcd.print(0, 0, lcdPartition);
//  lcd.print(position, 0, viewPartition);
//}


void changedPartition(byte partition) {
  partitionChanged = true;
  setStatus(partition);
  setLights(partition);
  partitionChanged = false;
}


void setLights(byte partition) {
  if ((dsc.lights[partition] != ligths_sent || force_send_status_for_new_client) && ws.count()) {
    char outas[128];
    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();
    root["status_packet"] = dsc.lights[partition];
    serializeJson(root, outas);
    ws.textAll(outas);
    ligths_sent = dsc.lights[partition];
  }
}


void setStatus(byte partition) {
  static byte lastStatus[8];
  if (!partitionChanged && dsc.status[partition] == lastStatus[partition] && !force_send_status_for_new_client) return;
  lastStatus[partition] = dsc.status[partition];

  if (ws.count()) {
    char outas[128];
    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();


    switch (dsc.status[partition]) {
      case 0x01: root["lcd_lower"] = "Ready"; break;
      case 0x02: root["lcd_lower"] = "Stay zones open"; break;
      case 0x03: root["lcd_lower"] = "Zones open"; break;
      case 0x04: root["lcd_lower"] = "Armed stay"; break;
      case 0x05: root["lcd_lower"] = "Armed away"; break;
      case 0x07: root["lcd_lower"] = "Failed to arm"; break;
      case 0x08: root["lcd_lower"] = "Exit delay"; break;
      case 0x09: root["lcd_lower"] = "No entry delay"; break;
      case 0x0B: root["lcd_lower"] = "Quick exit"; break;
      case 0x0C: root["lcd_lower"] = "Entry delay"; break;
      case 0x0D: root["lcd_lower"] = "Alarm memory"; break;
      case 0x10: root["lcd_lower"] = "Keypad lockout"; break;
      case 0x11: root["lcd_lower"] = "Alarm"; break;
      case 0x14: root["lcd_lower"] = "Auto-arm"; break;
      case 0x16: root["lcd_lower"] = "No entry delay"; break;
      case 0x22: root["lcd_lower"] = "Alarm memory"; break;
      case 0x33: root["lcd_lower"] = "Busy"; break;
      case 0x3D: root["lcd_lower"] = "Disarmed"; break;
      case 0x3E: root["lcd_lower"] = "Disarmed"; break;
      case 0x40: root["lcd_lower"] = "Keypad blanked"; break;
      case 0x8A: root["lcd_lower"] = "Activate zones"; break;
      case 0x8B: root["lcd_lower"] = "Quick exit"; break;
      case 0x8E: root["lcd_lower"] = "Invalid option"; break;
      case 0x8F: root["lcd_lower"] = "Invalid code"; break;
      case 0x9E: root["lcd_lower"] = "Enter * code"; break;
      case 0x9F: root["lcd_lower"] = "Access code"; break;
      case 0xA0: root["lcd_lower"] = "Zone bypass"; break;
      case 0xA1: root["lcd_lower"] = "Trouble menu"; break;
      case 0xA2: root["lcd_lower"] = "Alarm memory"; break;
      case 0xA3: root["lcd_lower"] = "Door chime on"; break;
      case 0xA4: root["lcd_lower"] = "Door chime off"; break;
      case 0xA5: root["lcd_lower"] = "Master code"; break;
      case 0xA6: root["lcd_lower"] = "Access codes"; break;
      case 0xA7: root["lcd_lower"] = "Enter new code"; break;
      case 0xA9: root["lcd_lower"] = "User function"; break;
      case 0xAA: root["lcd_lower"] = "Time and Date"; break;
      case 0xAB: root["lcd_lower"] = "Auto-arm time"; break;
      case 0xAC: root["lcd_lower"] = "Auto-arm on"; break;
      case 0xAD: root["lcd_lower"] = "Auto-arm off"; break;
      case 0xAF: root["lcd_lower"] = "System test"; break;
      case 0xB0: root["lcd_lower"] = "Enable DLS"; break;
      case 0xB2: root["lcd_lower"] = "Command output"; break;
      case 0xB7: root["lcd_lower"] = "Installer code"; break;
      case 0xB8: root["lcd_lower"] = "Enter * code"; break;
      case 0xB9: root["lcd_lower"] = "Zone tamper"; break;
      case 0xBA: root["lcd_lower"] = "Zones low batt."; break;
      case 0xC6: root["lcd_lower"] = "Zone fault menu"; break;
      case 0xC8: root["lcd_lower"] = "Service required"; break;
      case 0xD0: root["lcd_lower"] = "Keypads low batt"; break;
      case 0xD1: root["lcd_lower"] = "Wireless low bat"; break;
      case 0xE4: root["lcd_lower"] = "Installer menu"; break;
      case 0xE5: root["lcd_lower"] = "Keypad slot"; break;
      case 0xE6: root["lcd_lower"] = "Input: 2 digits"; break;
      case 0xE7: root["lcd_lower"] = "Input: 3 digits"; break;
      case 0xE8: root["lcd_lower"] = "Input: 4 digits"; break;
      case 0xEA: root["lcd_lower"] = "Code: 2 digits"; break;
      case 0xEB: root["lcd_lower"] = "Code: 4 digits"; break;
      case 0xEC: root["lcd_lower"] = "Input: 6 digits"; break;
      case 0xED: root["lcd_lower"] = "Input: 32 digits"; break;
      case 0xEE: root["lcd_lower"] = "Input: option"; break;
      case 0xF0: root["lcd_lower"] = "Function key 1"; break;
      case 0xF1: root["lcd_lower"] = "Function key 2"; break;
      case 0xF2: root["lcd_lower"] = "Function key 3"; break;
      case 0xF3: root["lcd_lower"] = "Function key 4"; break;
      case 0xF4: root["lcd_lower"] = "Function key 5"; break;
      case 0xF8: root["lcd_lower"] = "Keypad program"; break;
      default: root["lcd_lower"] = dsc.status[partition];
    }
    serializeJson(root, outas);
    ws.textAll(outas);
  }

}


void printFire(byte partition) {
  if (dsc.fire[partition]) {
    //    lcd.clear();
    //    byte position = strlen(lcdPartition);
    //    lcd.print(0, 0, lcdPartition);
    //    lcd.print(position, 0, partition + 1);
    //    lcd.print(0, 1, "Fire alarm");
  } else {
    //    lcd.clear();
    //    byte position = strlen(lcdPartition);
    //    lcd.print(0, 0, lcdPartition);
    //    lcd.print(position, 0, partition + 1);
    //    lcd.print(0, 1, "Fire alarm off");
  }
}
