/*
 *  VirtualKeypad-Blynk 1.3 (esp32)
 *
 *  Provides a virtual keypad interface for the free Blynk (https://www.blynk.cc) app on iOS and Android, similar
 *  to a physical DSC LED keypad.  Note that while the Blynk app has an LCD to display the partition status, the
 *  sketch currently does not emulate the menu navigation features of the DSC LCD keypads (PK5500, etc).
 *
 *  Usage:
 *    1. Scan one of the following QR codes from within the Blynk app for an example keypad layout:
 *      16 zones: https://user-images.githubusercontent.com/12835671/42364287-41ca6662-80c0-11e8-85e7-d579b542568d.png
 *      32 zones: https://user-images.githubusercontent.com/12835671/42364293-4512b720-80c0-11e8-87bd-153c4e857b4e.png
 *    2. Navigate to Project Settings > Devices > DSC Keybus Interface > DSC KeybusInterface.
 *    3. Select "Refresh" to generate a new auth token.
 *    4. Go back to Project Settings, copy the auth token, and paste it in an email or message to yourself.
 *    5. Add the auth token to the sketch below.
 *    6. Upload the sketch.
 *
 *  Installing Blynk as a local server (https://github.com/blynkkk/blynk-server) is recommended to keep control of the
 *  security system internal to your network.  This also lets you use as many widgets as needed for free - local
 *  servers can setup users with any amount of Blynk Energy.  Using the default Blynk cloud service with the above
 *  example layouts requires more of Blynk's Energy units than available on the free usage tier.
 *
 *  The Blynk layout can be customized with widgets using these virtual pin mappings:
 *    V0 - Keypad 0 ... V9 - Keypad 9
      V10 - Keypad
      V11 - Keypad #
      V12 - Keypad stay
      V13 - Keypad away
      V14 - Keypad fire alarm
      V15 - Keypad aux alarm
      V16 - Keypad panic alarm
      V30 - Partition number menu
      V31 - PGM 1 ... V39 - PGM9
      V40 - LCD
      V41 - PGM 10 ... V45 - PGM14
      V50 - LED Ready
      V51 - LED Armed
      V52 - LED Memory
      V53 - LED Bypass
      V54 - LED Trouble
      V55 - LED Program
      V56 - LED Fire
      V61 - Zone 1 ... V124 - Zone 64
 *
 *  Release notes:
 *    1.3 - Display zone lights in alarm memory and programming
 *          Add PGM outputs 1-14 status
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

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <dscKeybusInterface.h>

#define BLYNK_PRINT Serial

// Settings
char wifiSSID[] = "";
char wifiPassword[] = "";
char blynkAuthToken[] = "";  // Token generated from within the Blynk app
char blynkServer[] = "";     // Blynk local server address
int blynkPort = 8080;        // Blynk local server port

// Configures the Keybus interface with the specified pins - dscWritePin is
// optional, leaving it out disables the virtual keypad
#define dscClockPin 18  // esp32: 4,13,16-39
#define dscReadPin  19  // esp32: 4,13,16-39
#define dscWritePin 21  // esp32: 4,13,16-33

// Initialize components
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
bool wifiConnected = true;
bool partitionChanged, pausedZones;
byte systemZones[dscZones], programZones[dscZones], previousProgramZones[dscZones];
const char* ledOpenZonesColor = "#23C48E";     // Green
const char* ledProgramZonesColor = "#FDD322";  // Orange
const char* ledAlarmZonesColor = "#D3435C";    // Red
const char* ledPGMColor = "#23C48E";           // Green
WidgetLED ledPGM1(V31);
WidgetLED ledPGM2(V32);
WidgetLED ledPGM3(V33);
WidgetLED ledPGM4(V34);
WidgetLED ledPGM5(V35);
WidgetLED ledPGM6(V36);
WidgetLED ledPGM7(V37);
WidgetLED ledPGM8(V38);
WidgetLED ledPGM9(V39);
WidgetLCD lcd(V40);
WidgetLED ledPGM10(V41);
WidgetLED ledPGM11(V42);
WidgetLED ledPGM12(V43);
WidgetLED ledPGM13(V44);
WidgetLED ledPGM14(V45);
WidgetLED ledReady(V50);
WidgetLED ledArmed(V51);
WidgetLED ledMemory(V52);
WidgetLED ledBypass(V53);
WidgetLED ledTrouble(V54);
WidgetLED ledProgram(V55);
WidgetLED ledFire(V56);
WidgetLED ledZone1(V61);
WidgetLED ledZone2(V62);
WidgetLED ledZone3(V63);
WidgetLED ledZone4(V64);
WidgetLED ledZone5(V65);
WidgetLED ledZone6(V66);
WidgetLED ledZone7(V67);
WidgetLED ledZone8(V68);
WidgetLED ledZone9(V69);
WidgetLED ledZone10(V70);
WidgetLED ledZone11(V71);
WidgetLED ledZone12(V72);
WidgetLED ledZone13(V73);
WidgetLED ledZone14(V74);
WidgetLED ledZone15(V75);
WidgetLED ledZone16(V76);
WidgetLED ledZone17(V77);
WidgetLED ledZone18(V78);
WidgetLED ledZone19(V79);
WidgetLED ledZone20(V80);
WidgetLED ledZone21(V81);
WidgetLED ledZone22(V82);
WidgetLED ledZone23(V83);
WidgetLED ledZone24(V84);
WidgetLED ledZone25(V85);
WidgetLED ledZone26(V86);
WidgetLED ledZone27(V87);
WidgetLED ledZone28(V88);
WidgetLED ledZone29(V89);
WidgetLED ledZone30(V90);
WidgetLED ledZone31(V91);
WidgetLED ledZone32(V92);
WidgetLED ledZone33(V93);
WidgetLED ledZone34(V94);
WidgetLED ledZone35(V95);
WidgetLED ledZone36(V96);
WidgetLED ledZone37(V97);
WidgetLED ledZone38(V98);
WidgetLED ledZone39(V99);
WidgetLED ledZone40(V100);
WidgetLED ledZone41(V101);
WidgetLED ledZone42(V102);
WidgetLED ledZone43(V103);
WidgetLED ledZone44(V104);
WidgetLED ledZone45(V105);
WidgetLED ledZone46(V106);
WidgetLED ledZone47(V107);
WidgetLED ledZone48(V108);
WidgetLED ledZone49(V109);
WidgetLED ledZone50(V110);
WidgetLED ledZone51(V111);
WidgetLED ledZone52(V112);
WidgetLED ledZone53(V113);
WidgetLED ledZone54(V114);
WidgetLED ledZone55(V115);
WidgetLED ledZone56(V116);
WidgetLED ledZone57(V117);
WidgetLED ledZone58(V118);
WidgetLED ledZone59(V119);
WidgetLED ledZone60(V120);
WidgetLED ledZone61(V121);
WidgetLED ledZone62(V122);
WidgetLED ledZone63(V123);
WidgetLED ledZone64(V124);


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  Serial.print(F("WiFi..."));
  WiFi.mode(WIFI_STA);
  Blynk.begin(blynkAuthToken, wifiSSID, wifiPassword, blynkServer, blynkPort);
  while (WiFi.status() != WL_CONNECTED) yield();
  Serial.print(F("connected: "));
  Serial.println(WiFi.localIP());
  Serial.print(F("Blynk..."));
  while (!Blynk.connected()) {
    Blynk.run();
    yield();
  }
  Serial.print(F("connected: "));
  Serial.println(blynkServer);

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();
  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {

  // Updates status if WiFi drops and reconnects
  if (!wifiConnected && WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi reconnected");
    wifiConnected = true;
    dsc.pauseStatus = false;
    dsc.statusChanged = true;
  }
  else if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    Serial.println("WiFi disconnected");
    wifiConnected = false;
    dsc.pauseStatus = true;
  }

  Blynk.run();

  // Processes status data that is not handled natively by the library
  if (dsc.loop()) processStatus();

  if (dsc.statusChanged) {      // Checks if the security system status has changed
    dsc.statusChanged = false;  // Reset the status tracking flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // loop() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
    if (dsc.bufferOverflow) {
      Serial.println(F("Keybus buffer overflow"));
      dsc.bufferOverflow = false;
    }

    // Checks status for the currently viewed partition
    byte partition = dsc.writePartition - 1;

    // Checks if the interface is connected to the Keybus
    if (dsc.keybusChanged) {
      dsc.keybusChanged = false;                 // Resets the Keybus data status flag
      if (dsc.keybusConnected) {
        Serial.println(F("Keybus connected"));
        Blynk.syncVirtual(V30);
        lcd.print(0, 0, "             [ ]");
        lcd.print(14, 0, dsc.writePartition);
        lcd.print(0, 1, "                ");
        setLights(partition, true);
        setStatus(partition, true);
      }
      else {
        Serial.println(F("Keybus disconnected"));
        lcd.print(0, 0, "Keybus          ");
        lcd.print(0, 1, "disconnected    ");
      }
    }

    setLights(partition, false);
    setStatus(partition, false);

    if (dsc.fireChanged[partition]) {
      dsc.fireChanged[partition] = false;  // Resets the fire status flag
      printFire(partition);
    }

    // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones
    //   openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
    //   openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
    //   ...
    //   openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
    if (dsc.openZonesStatusChanged && !pausedZones) {
      dsc.openZonesStatusChanged = false;                           // Resets the open zones status flag
      for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
        for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
          if (bitRead(dsc.openZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual open zone status flag
            byte zoneLight = zoneBit + 1 + (zoneGroup * 8);
            bitWrite(dsc.openZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual open zone status flag

            if (bitRead(dsc.openZones[zoneGroup], zoneBit)) {
              if (bitRead(dsc.alarmZones[zoneGroup], zoneBit)) {
                setZoneLight(zoneLight, true, ledAlarmZonesColor);    // Alarm zone open
              }
              else setZoneLight(zoneLight, true, ledOpenZonesColor);  // Zone open
              bitWrite(systemZones[zoneGroup], zoneBit, 1);
            }

            else {
              setZoneLight(zoneLight, false, NULL);  // Zone closed
              bitWrite(systemZones[zoneGroup], zoneBit, 0);
            }
          }
        }
      }
    }

    // Zone alarm status is stored in the alarmZones[] and alarmZonesChanged[] arrays using 1 bit per zone, up to 64 zones
    //   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
    //   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
    //   ...
    //   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
    if (dsc.alarmZonesStatusChanged) {
      dsc.alarmZonesStatusChanged = false;                           // Resets the alarm zones status flag
      for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
        for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
          if (bitRead(dsc.alarmZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual alarm zone status flag
            byte zoneLight = zoneBit + 1 + (zoneGroup * 8);
            bitWrite(dsc.alarmZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual alarm zone status flag
            if (bitRead(dsc.alarmZones[zoneGroup], zoneBit)) {
              setZoneLight(zoneLight, true, ledAlarmZonesColor);  // Sets zone alarm color
            }
            else {
              setZoneColor(zoneLight, ledOpenZonesColor);  // Sets open zone color
            }
          }
        }
      }
    }

    // PGM outputs 1-14 status is stored in the pgmOutputs[] and pgmOutputsChanged[] arrays using 1 bit per PGM output:
    //   pgmOutputs[0] and pgmOutputsChanged[0]: Bit 0 = PGM 1 ... Bit 7 = PGM 8
    //   pgmOutputs[1] and pgmOutputsChanged[1]: Bit 0 = PGM 9 ... Bit 5 = PGM 14
    if (dsc.pgmOutputsStatusChanged) {
      dsc.pgmOutputsStatusChanged = false;  // Resets the PGM outputs status flag
      for (byte pgmGroup = 0; pgmGroup < 2; pgmGroup++) {
        for (byte pgmBit = 0; pgmBit < 8; pgmBit++) {
          if (bitRead(dsc.pgmOutputsChanged[pgmGroup], pgmBit)) {  // Checks an individual PGM output status flag
            bitWrite(dsc.pgmOutputsChanged[pgmGroup], pgmBit, 0);  // Resets the individual PGM output status flag
            if (bitRead(dsc.pgmOutputs[pgmGroup], pgmBit)) {       // PGM enabled
              switch (pgmBit + 1 + (pgmGroup * 8)) {
                case 1: ledPGM1.setColor(ledPGMColor); ledPGM1.on(); break;
                case 2: ledPGM2.setColor(ledPGMColor); ledPGM2.on(); break;
                case 3: ledPGM3.setColor(ledPGMColor); ledPGM3.on(); break;
                case 4: ledPGM4.setColor(ledPGMColor); ledPGM4.on(); break;
                case 5: ledPGM5.setColor(ledPGMColor); ledPGM5.on(); break;
                case 6: ledPGM6.setColor(ledPGMColor); ledPGM6.on(); break;
                case 7: ledPGM7.setColor(ledPGMColor); ledPGM7.on(); break;
                case 8: ledPGM8.setColor(ledPGMColor); ledPGM8.on(); break;
                case 9: ledPGM9.setColor(ledPGMColor); ledPGM9.on(); break;
                case 10: ledPGM10.setColor(ledPGMColor); ledPGM10.on(); break;
                case 11: ledPGM11.setColor(ledPGMColor); ledPGM11.on(); break;
                case 12: ledPGM12.setColor(ledPGMColor); ledPGM12.on(); break;
                case 13: ledPGM13.setColor(ledPGMColor); ledPGM13.on(); break;
                case 14: ledPGM14.setColor(ledPGMColor); ledPGM14.on(); break;
              }
            }
            else {                                                 // PGM disabled
              switch (pgmBit + 1 + (pgmGroup * 8)) {
                case 1: ledPGM1.off(); break;
                case 2: ledPGM2.off(); break;
                case 3: ledPGM3.off(); break;
                case 4: ledPGM4.off(); break;
                case 5: ledPGM5.off(); break;
                case 6: ledPGM6.off(); break;
                case 7: ledPGM7.off(); break;
                case 8: ledPGM8.off(); break;
                case 9: ledPGM9.off(); break;
                case 10: ledPGM10.off(); break;
                case 11: ledPGM11.off(); break;
                case 12: ledPGM12.off(); break;
                case 13: ledPGM13.off(); break;
                case 14: ledPGM14.off(); break;
              }
            }
          }
        }
      }
    }

    if (dsc.powerChanged) {
      dsc.powerChanged = false;  // Resets the power trouble status flag
      if (dsc.powerTrouble) {
        lcd.print(0, 0, "Power        ");
        lcd.print(0, 1, "trouble         ");
      }
      else {
        lcd.print(0, 0, "Power        ");
        lcd.print(0, 1, "restored        ");
      }
    }

    if (dsc.batteryChanged) {
      dsc.batteryChanged = false;  // Resets the battery trouble status flag
      if (dsc.batteryTrouble) {
        lcd.print(0, 0, "Battery     ");
        lcd.print(0, 1, "trouble         ");
      }
      else {
        lcd.print(0, 0, "Battery     ");
        lcd.print(0, 1, "restored        ");
      }
    }
  }
}


void setStatus(byte partition, bool forceUpdate) {
  static byte previousStatus[dscPartitions];
  if (!partitionChanged && !forceUpdate && dsc.status[partition] == previousStatus[partition]) return;
  previousStatus[partition] = dsc.status[partition];

  switch (dsc.status[partition]) {
    case 0x01: lcd.print(0, 0, "Partition    ");
               lcd.print(0, 1, "ready           ");
               if (pausedZones) resetZones(); break;
    case 0x02: lcd.print(0, 0, "Stay         ");
               lcd.print(0, 1, "zones open      ");
               if (pausedZones) resetZones(); break;
    case 0x03: lcd.print(0, 0, "Zones open   ");
               lcd.print(0, 1, "                ");
               if (pausedZones) resetZones(); break;
    case 0x04: lcd.print(0, 0, "Armed:       ");
               lcd.print(0, 1, "Stay            ");
               if (pausedZones) resetZones(); break;
    case 0x05: lcd.print(0, 0, "Armed:       ");
               lcd.print(0, 1, "Away            ");
               if (pausedZones) resetZones(); break;
    case 0x06: lcd.print(0, 0, "Armed:       ");
               lcd.print(0, 1, "No entry delay  ");
               if (pausedZones) resetZones(); break;
    case 0x07: lcd.print(0, 0, "Failed       ");
               lcd.print(0, 1, "to arm          "); break;
    case 0x08: lcd.print(0, 0, "Exit delay   ");
               lcd.print(0, 1, "in progress     ");
               if (pausedZones) resetZones(); break;
    case 0x09: lcd.print(0, 0, "Arming:      ");
               lcd.print(0, 1, "No entry delay  "); break;
    case 0x0B: lcd.print(0, 0, "Quick exit   ");
               lcd.print(0, 1, "in progress     "); break;
    case 0x0C: lcd.print(0, 0, "Entry delay  ");
               lcd.print(0, 1, "in progress     "); break;
    case 0x0D: lcd.print(0, 0, "Entry delay  ");
               lcd.print(0, 1, "after alarm     "); break;
    case 0x0E: lcd.print(0, 0, "Not          ");
               lcd.print(0, 1, "available       "); break;
    case 0x10: lcd.print(0, 0, "Keypad       ");
               lcd.print(0, 1, "lockout         "); break;
    case 0x11: lcd.print(0, 0, "Partition    ");
               lcd.print(0, 1, "in alarm        "); break;
    case 0x12: lcd.print(0, 0, "Battery check");
               lcd.print(0, 1, "in progress     "); break;
    case 0x14: lcd.print(0, 0, "Auto-arm     ");
               lcd.print(0, 1, "in progress     "); break;
    case 0x15: lcd.print(0, 0, "Arming with  ");
               lcd.print(0, 1, "bypass zones    "); break;
    case 0x16: lcd.print(0, 0, "Armed:       ");
               lcd.print(0, 1, "No entry delay  ");
               if (pausedZones) resetZones(); break;
    case 0x19: lcd.print(0, 0, "Alarm        ");
               lcd.print(0, 1, "occurred        "); break;
    case 0x22: lcd.print(0, 0, "Recent       ");
               lcd.print(0, 1, "closing         "); break;
    case 0x2F: lcd.print(0, 0, "Keypad LCD   ");
               lcd.print(0, 1, "test            "); break;
    case 0x33: lcd.print(0, 0, "Command      ");
               lcd.print(0, 1, "output active   "); break;
    case 0x3D: lcd.print(0, 0, "Alarm        ");
               lcd.print(0, 1, "occurred        "); break;
    case 0x3E: lcd.print(0, 0, "Disarmed     ");
               lcd.print(0, 1, "                "); break;
    case 0x40: lcd.print(0, 0, "Keypad       ");
               lcd.print(0, 1, "blanked         "); break;
    case 0x8A: lcd.print(0, 0, "Activate     ");
               lcd.print(0, 1, "stay/away zones "); break;
    case 0x8B: lcd.print(0, 0, "Quick exit   ");
               lcd.print(0, 1, "                "); break;
    case 0x8E: lcd.print(0, 0, "Invalid      ");
               lcd.print(0, 1, "option          "); break;
    case 0x8F: lcd.print(0, 0, "Invalid      ");
               lcd.print(0, 1, "access code     "); break;
    case 0x9E: lcd.print(0, 0, "Enter *      ");
               lcd.print(0, 1, "function code   "); break;
    case 0x9F: lcd.print(0, 0, "Enter        ");
               lcd.print(0, 1, "access code     "); break;
    case 0xA0: lcd.print(0, 0, "*1:          ");
               lcd.print(0, 1, "Zone bypass menu"); break;
    case 0xA1: lcd.print(0, 0, "*2:          ");
               lcd.print(0, 1, "Trouble menu    "); break;
    case 0xA2: lcd.print(0, 0, "*3:          ");
               lcd.print(0, 1, "Alarm memory    "); break;
    case 0xA3: lcd.print(0, 0, "Door         ");
               lcd.print(0, 1, "chime enabled   "); break;
    case 0xA4: lcd.print(0, 0, "Door         ");
               lcd.print(0, 1, "chime disabled  "); break;
    case 0xA5: lcd.print(0, 0, "Enter        ");
               lcd.print(0, 1, "master code     "); break;
    case 0xA6: lcd.print(0, 0, "*5:          ");
               lcd.print(0, 1, "Access codes    "); break;
    case 0xA7: lcd.print(0, 0, "Enter        ");
               lcd.print(0, 1, "4-digit code    "); break;
    case 0xA9: lcd.print(0, 0, "*6:          ");
               lcd.print(0, 1, "User functions  "); break;
    case 0xAA: lcd.print(0, 0, "*6:          ");
               lcd.print(0, 1, "Time and date   "); break;
    case 0xAB: lcd.print(0, 0, "*6:          ");
               lcd.print(0, 1, "Auto-arm time   "); break;
    case 0xAC: lcd.print(0, 0, "*6:          ");
               lcd.print(0, 1, "Auto-arm on     "); break;
    case 0xAD: lcd.print(0, 0, "*6:          ");
               lcd.print(0, 1, "Auto-arm off    "); break;
    case 0xAF: lcd.print(0, 0, "*6:          ");
               lcd.print(0, 1, "System test     "); break;
    case 0xB0: lcd.print(0, 0, "*6:          ");
               lcd.print(0, 1, "Enable DLS      "); break;
    case 0xB2: lcd.print(0, 0, "*7:          ");
               lcd.print(0, 1, "Command output  "); break;
    case 0xB7: lcd.print(0, 0, "Enter        ");
               lcd.print(0, 1, "installer code  "); break;
    case 0xB8: lcd.print(0, 0, "Enter *      ");
               lcd.print(0, 1, "function code   "); break;
    case 0xB9: lcd.print(0, 0, "*2:          ");
               lcd.print(0, 1, "Zone tamper menu"); break;
    case 0xBA: lcd.print(0, 0, "*2: Zones    ");
               lcd.print(0, 1, "low battery     "); break;
    case 0xBC: lcd.print(0, 0, "New          ");
               lcd.print(0, 1, "6-digit code    "); break;
    case 0xC6: lcd.print(0, 0, "*2:          ");
               lcd.print(0, 1, "Zone fault menu "); break;
    case 0xC7: lcd.print(0, 0, "Partition    ");
               lcd.print(0, 1, "disabled        "); break;
    case 0xC8: lcd.print(0, 0, "*2:          ");
               lcd.print(0, 1, "Service required"); break;
    case 0xCE: lcd.print(0, 0, "Active camera");
               lcd.print(0, 1, "monitor select. "); break;
    case 0xD0: lcd.print(0, 0, "*2: Keypads  ");
               lcd.print(0, 1, "low battery     "); break;
    case 0xD1: lcd.print(0, 0, "*2: Keyfobs  ");
               lcd.print(0, 1, "low battery     "); break;
    case 0xE4: lcd.print(0, 0, "*8:          ");
               lcd.print(0, 1, "Installer menu  "); break;
    case 0xE5: lcd.print(0, 0, "Keypad       ");
               lcd.print(0, 1, "slot assignment "); break;
    case 0xE6: lcd.print(0, 0, "Input:       ");
               lcd.print(0, 1, "2 digits        "); break;
    case 0xE7: lcd.print(0, 0, "Input:       ");
               lcd.print(0, 1, "3 digits        "); break;
    case 0xE8: lcd.print(0, 0, "Input:       ");
               lcd.print(0, 1, "4 digits        "); break;
    case 0xEA: lcd.print(0, 0, "Reporting    ");
               lcd.print(0, 1, "code: 2 digits  "); break;
    case 0xEB: lcd.print(0, 0, "Reporting    ");
               lcd.print(0, 1, "code: 4 digits  "); break;
    case 0xEC: lcd.print(0, 0, "Input:       ");
               lcd.print(0, 1, "6 digits        "); break;
    case 0xED: lcd.print(0, 0, "Input:       ");
               lcd.print(0, 1, "32 digits       "); break;
    case 0xEE: lcd.print(0, 0, "Input: 1     ");
               lcd.print(0, 1, "option per zone "); break;
    case 0xEF: lcd.print(0, 0, "Module       ");
               lcd.print(0, 1, "supervision     "); break;
    case 0xF0: lcd.print(0, 0, "Function     ");
               lcd.print(0, 1, "key 1           "); break;
    case 0xF1: lcd.print(0, 0, "Function     ");
               lcd.print(0, 1, "key 2           "); break;
    case 0xF2: lcd.print(0, 0, "Function     ");
               lcd.print(0, 1, "key 3           "); break;
    case 0xF3: lcd.print(0, 0, "Function     ");
               lcd.print(0, 1, "key 4           "); break;
    case 0xF4: lcd.print(0, 0, "Function     ");
               lcd.print(0, 1, "key 5           "); break;
    case 0xF5: lcd.print(0, 0, "Wireless mod.");
               lcd.print(0, 1, "place. test     "); break;
    case 0xF6: lcd.print(0, 0, "Activate     ");
               lcd.print(0, 1, "device for test "); break;
    case 0xF7: lcd.print(0, 0, "*8 PGM       ");
               lcd.print(0, 1, "subsection      "); break;
    case 0xF8: lcd.print(0, 0, "Keypad       ");
               lcd.print(0, 1, "programming     "); break;
    case 0xFA: lcd.print(0, 0, "Input:       ");
               lcd.print(0, 1, "6 digits        "); break;
    default: return;
  }
}


void printFire(byte partition) {
  if (dsc.fire[partition]) {
    lcd.print(0, 0, "Fire         ");
    lcd.print(0, 1, "alarm           ");
    lcd.print(6, 1, partition + 1);
  }
  else {
    lcd.print(0, 0, "Fire         ");
    lcd.print(0, 1, "alarm    off    ");
    lcd.print(6, 1, partition + 1);
  }
}


BLYNK_WRITE(V0) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('0');
  Blynk.virtualWrite(V0, 0);
}

BLYNK_WRITE(V1) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('1');
  Blynk.virtualWrite(V1, 0);
}

BLYNK_WRITE(V2) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('2');
  Blynk.virtualWrite(V2, 0);
}

BLYNK_WRITE(V3) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('3');
  Blynk.virtualWrite(V3, 0);
}

BLYNK_WRITE(V4) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('4');
  Blynk.virtualWrite(V4, 0);
}

BLYNK_WRITE(V5) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('5');
  Blynk.virtualWrite(V5, 0);
}

BLYNK_WRITE(V6) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('6');
  Blynk.virtualWrite(V6, 0);
}

BLYNK_WRITE(V7) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('7');
  Blynk.virtualWrite(V7, 0);
}

BLYNK_WRITE(V8) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('8');
  Blynk.virtualWrite(V8, 0);
}

BLYNK_WRITE(V9) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('9');
  Blynk.virtualWrite(V9, 0);
}

BLYNK_WRITE(V10) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('*');
  Blynk.virtualWrite(V10, 0);
}

BLYNK_WRITE(V11) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('#');
  Blynk.virtualWrite(V11, 0);
}

BLYNK_WRITE(V12) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('s');
  Blynk.virtualWrite(V12, 0);
}

BLYNK_WRITE(V13) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('w');
  Blynk.virtualWrite(V13, 0);
}

BLYNK_WRITE(V14) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('f');
  Blynk.virtualWrite(V14, 0);
}

BLYNK_WRITE(V15) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('a');
  Blynk.virtualWrite(V15, 0);
}

BLYNK_WRITE(V16) {
  int buttonPressed = param.asInt();
  if (buttonPressed) dsc.write('p');
  Blynk.virtualWrite(V16, 0);
}

BLYNK_WRITE(V30) {
  switch (param.asInt()) {
    case 1: {
        dsc.writePartition = 1;
        changedPartition(dsc.writePartition - 1);
        break;
      }
    case 2: {
        dsc.writePartition = 2;
        changedPartition(dsc.writePartition - 1);
        break;
      }
    case 3: {
        dsc.writePartition = 3;
        changedPartition(dsc.writePartition - 1);
        break;
      }
    case 4: {
        dsc.writePartition = 4;
        changedPartition(dsc.writePartition - 1);
        break;
      }
    case 5: {
        dsc.writePartition = 5;
        changedPartition(dsc.writePartition - 1);
        break;
      }
    case 6: {
        dsc.writePartition = 6;
        changedPartition(dsc.writePartition - 1);
        break;
      }
    case 7: {
        dsc.writePartition = 7;
        changedPartition(dsc.writePartition - 1);
        break;
      }
    case 8: {
        dsc.writePartition = 8;
        changedPartition(dsc.writePartition - 1);
        break;
      }
    default: {
        dsc.writePartition = 1;
        changedPartition(dsc.writePartition - 1);
        break;
      }
  }
}


void changedPartition(byte partition) {
  partitionChanged = true;
  lcd.print(13, 0, "[ ]");
  lcd.print(14, 0, dsc.writePartition);
  setStatus(partition, true);
  setLights(partition, true);
  partitionChanged = false;
}


void setLights(byte partition, bool forceUpdate) {
  static byte previousLights[dscPartitions];
  byte lightsChanged = dsc.lights[partition] ^ previousLights[partition];
  if (lightsChanged || forceUpdate) {
    previousLights[partition] = dsc.lights[partition];
    for (byte lightBit = 0; lightBit <= 6; lightBit++) {
      if (bitRead(lightsChanged, lightBit) || forceUpdate) {
        if (bitRead(dsc.lights[partition], lightBit)) {
          switch (lightBit) {
            case 0: ledReady.on(); break;
            case 1: ledArmed.on(); break;
            case 2: ledMemory.on(); break;
            case 3: ledBypass.on(); break;
            case 4: ledTrouble.on(); break;
            case 5: ledProgram.on(); break;
            case 6: ledFire.on(); break;
          }
        }
        else {
          switch (lightBit) {
            case 0: ledReady.off(); break;
            case 1: ledArmed.off(); break;
            case 2: ledMemory.off(); break;
            case 3: ledBypass.off(); break;
            case 4: ledTrouble.off(); break;
            case 5: ledProgram.off(); break;
            case 6: ledFire.off(); break;
          }
        }
      }
    }
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
        processProgramZones(4, ledProgramZonesColor);
      }
      break;
    case 0x5D:
      if ((dsc.panelData[2] & 0x04) == 0x04) {  // Alarm memory zones 1-32
        if (pausedZones) {
          processProgramZones(3, ledAlarmZonesColor);
        }
      }
      break;
    case 0xE6:
      if (dsc.panelData[2] == 0x18 && (dsc.panelData[4] & 0x04) == 0x04) {
        //processMemoryZones(4);  // Alarm memory zones 33-64
      }
      break;
  }
}


void resetZones() {
  pausedZones = false;
  dsc.openZonesStatusChanged = true;
  for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
    previousProgramZones[zoneGroup] = 0;
    for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
      if (bitRead(programZones[zoneGroup], zoneBit)) {
        bitWrite(dsc.openZonesChanged[zoneGroup], zoneBit, 1);
      }
      if (bitRead(systemZones[zoneGroup], zoneBit)) {
        bitWrite(dsc.openZonesChanged[zoneGroup], zoneBit, 1);
      }
    }
  }
}


void pauseZones() {
  pausedZones = true;
  for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
    for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
      byte zoneLight = zoneBit + 1 + (zoneGroup * 8);
      if (bitRead(systemZones[zoneGroup], zoneBit)) {
        setZoneLight(zoneLight, false, NULL);
      }
    }
  }
}


void processProgramZones(byte startByte, const char* ledColor) {
  for (byte zoneGroup = 0; zoneGroup < 4; zoneGroup++) {
    programZones[zoneGroup] = dsc.panelData[zoneGroup + startByte];
    byte zonesChanged = programZones[zoneGroup] ^ previousProgramZones[zoneGroup];
    if (zonesChanged) {
      previousProgramZones[zoneGroup] = programZones[zoneGroup];
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(zonesChanged, zoneBit)) {
          byte zoneLight = zoneBit + 1 + (zoneGroup * 8);
          if (bitRead(programZones[zoneGroup], zoneBit)) {
            setZoneLight(zoneLight, true, ledColor);
          }
          else setZoneLight(zoneLight, false, NULL);
        }
      }
    }
  }
}


void setZoneColor(byte ledZone, const char* ledColor) {
  switch (ledZone) {
    case 1: ledZone1.setColor(ledColor); break;
    case 2: ledZone2.setColor(ledColor); break;
    case 3: ledZone3.setColor(ledColor); break;
    case 4: ledZone4.setColor(ledColor); break;
    case 5: ledZone5.setColor(ledColor); break;
    case 6: ledZone6.setColor(ledColor); break;
    case 7: ledZone7.setColor(ledColor); break;
    case 8: ledZone8.setColor(ledColor); break;
    case 9: ledZone9.setColor(ledColor); break;
    case 10: ledZone10.setColor(ledColor); break;
    case 11: ledZone11.setColor(ledColor); break;
    case 12: ledZone12.setColor(ledColor); break;
    case 13: ledZone13.setColor(ledColor); break;
    case 14: ledZone14.setColor(ledColor); break;
    case 15: ledZone15.setColor(ledColor); break;
    case 16: ledZone16.setColor(ledColor); break;
    case 17: ledZone17.setColor(ledColor); break;
    case 18: ledZone18.setColor(ledColor); break;
    case 19: ledZone19.setColor(ledColor); break;
    case 20: ledZone20.setColor(ledColor); break;
    case 21: ledZone21.setColor(ledColor); break;
    case 22: ledZone22.setColor(ledColor); break;
    case 23: ledZone23.setColor(ledColor); break;
    case 24: ledZone24.setColor(ledColor); break;
    case 25: ledZone25.setColor(ledColor); break;
    case 26: ledZone26.setColor(ledColor); break;
    case 27: ledZone27.setColor(ledColor); break;
    case 28: ledZone28.setColor(ledColor); break;
    case 29: ledZone29.setColor(ledColor); break;
    case 30: ledZone30.setColor(ledColor); break;
    case 31: ledZone31.setColor(ledColor); break;
    case 32: ledZone32.setColor(ledColor); break;
    case 33: ledZone33.setColor(ledColor); break;
    case 34: ledZone34.setColor(ledColor); break;
    case 35: ledZone35.setColor(ledColor); break;
    case 36: ledZone36.setColor(ledColor); break;
    case 37: ledZone37.setColor(ledColor); break;
    case 38: ledZone38.setColor(ledColor); break;
    case 39: ledZone39.setColor(ledColor); break;
    case 40: ledZone40.setColor(ledColor); break;
    case 41: ledZone41.setColor(ledColor); break;
    case 42: ledZone42.setColor(ledColor); break;
    case 43: ledZone43.setColor(ledColor); break;
    case 44: ledZone44.setColor(ledColor); break;
    case 45: ledZone45.setColor(ledColor); break;
    case 46: ledZone46.setColor(ledColor); break;
    case 47: ledZone47.setColor(ledColor); break;
    case 48: ledZone48.setColor(ledColor); break;
    case 49: ledZone49.setColor(ledColor); break;
    case 50: ledZone50.setColor(ledColor); break;
    case 51: ledZone51.setColor(ledColor); break;
    case 52: ledZone52.setColor(ledColor); break;
    case 53: ledZone53.setColor(ledColor); break;
    case 54: ledZone54.setColor(ledColor); break;
    case 55: ledZone55.setColor(ledColor); break;
    case 56: ledZone56.setColor(ledColor); break;
    case 57: ledZone57.setColor(ledColor); break;
    case 58: ledZone58.setColor(ledColor); break;
    case 59: ledZone59.setColor(ledColor); break;
    case 60: ledZone60.setColor(ledColor); break;
    case 61: ledZone61.setColor(ledColor); break;
    case 62: ledZone62.setColor(ledColor); break;
    case 63: ledZone63.setColor(ledColor); break;
    case 64: ledZone64.setColor(ledColor); break;
  }
}


void setZoneLight(byte ledZone, bool ledOn, const char* ledColor) {
  if (ledOn) {
    switch (ledZone) {
      case 1: ledZone1.setColor(ledColor); ledZone1.on(); break;
      case 2: ledZone2.setColor(ledColor); ledZone2.on(); break;
      case 3: ledZone3.setColor(ledColor); ledZone3.on(); break;
      case 4: ledZone4.setColor(ledColor); ledZone4.on(); break;
      case 5: ledZone5.setColor(ledColor); ledZone5.on(); break;
      case 6: ledZone6.setColor(ledColor); ledZone6.on(); break;
      case 7: ledZone7.setColor(ledColor); ledZone7.on(); break;
      case 8: ledZone8.setColor(ledColor); ledZone8.on(); break;
      case 9: ledZone9.setColor(ledColor); ledZone9.on(); break;
      case 10: ledZone10.setColor(ledColor); ledZone10.on(); break;
      case 11: ledZone11.setColor(ledColor); ledZone11.on(); break;
      case 12: ledZone12.setColor(ledColor); ledZone12.on(); break;
      case 13: ledZone13.setColor(ledColor); ledZone13.on(); break;
      case 14: ledZone14.setColor(ledColor); ledZone14.on(); break;
      case 15: ledZone15.setColor(ledColor); ledZone15.on(); break;
      case 16: ledZone16.setColor(ledColor); ledZone16.on(); break;
      case 17: ledZone17.setColor(ledColor); ledZone17.on(); break;
      case 18: ledZone18.setColor(ledColor); ledZone18.on(); break;
      case 19: ledZone19.setColor(ledColor); ledZone19.on(); break;
      case 20: ledZone20.setColor(ledColor); ledZone20.on(); break;
      case 21: ledZone21.setColor(ledColor); ledZone21.on(); break;
      case 22: ledZone22.setColor(ledColor); ledZone22.on(); break;
      case 23: ledZone23.setColor(ledColor); ledZone23.on(); break;
      case 24: ledZone24.setColor(ledColor); ledZone24.on(); break;
      case 25: ledZone25.setColor(ledColor); ledZone25.on(); break;
      case 26: ledZone26.setColor(ledColor); ledZone26.on(); break;
      case 27: ledZone27.setColor(ledColor); ledZone27.on(); break;
      case 28: ledZone28.setColor(ledColor); ledZone28.on(); break;
      case 29: ledZone29.setColor(ledColor); ledZone29.on(); break;
      case 30: ledZone30.setColor(ledColor); ledZone30.on(); break;
      case 31: ledZone31.setColor(ledColor); ledZone31.on(); break;
      case 32: ledZone32.setColor(ledColor); ledZone32.on(); break;
      case 33: ledZone33.setColor(ledColor); ledZone33.on(); break;
      case 34: ledZone34.setColor(ledColor); ledZone34.on(); break;
      case 35: ledZone35.setColor(ledColor); ledZone35.on(); break;
      case 36: ledZone36.setColor(ledColor); ledZone36.on(); break;
      case 37: ledZone37.setColor(ledColor); ledZone37.on(); break;
      case 38: ledZone38.setColor(ledColor); ledZone38.on(); break;
      case 39: ledZone39.setColor(ledColor); ledZone39.on(); break;
      case 40: ledZone40.setColor(ledColor); ledZone40.on(); break;
      case 41: ledZone41.setColor(ledColor); ledZone41.on(); break;
      case 42: ledZone42.setColor(ledColor); ledZone42.on(); break;
      case 43: ledZone43.setColor(ledColor); ledZone43.on(); break;
      case 44: ledZone44.setColor(ledColor); ledZone44.on(); break;
      case 45: ledZone45.setColor(ledColor); ledZone45.on(); break;
      case 46: ledZone46.setColor(ledColor); ledZone46.on(); break;
      case 47: ledZone47.setColor(ledColor); ledZone47.on(); break;
      case 48: ledZone48.setColor(ledColor); ledZone48.on(); break;
      case 49: ledZone49.setColor(ledColor); ledZone49.on(); break;
      case 50: ledZone50.setColor(ledColor); ledZone50.on(); break;
      case 51: ledZone51.setColor(ledColor); ledZone51.on(); break;
      case 52: ledZone52.setColor(ledColor); ledZone52.on(); break;
      case 53: ledZone53.setColor(ledColor); ledZone53.on(); break;
      case 54: ledZone54.setColor(ledColor); ledZone54.on(); break;
      case 55: ledZone55.setColor(ledColor); ledZone55.on(); break;
      case 56: ledZone56.setColor(ledColor); ledZone56.on(); break;
      case 57: ledZone57.setColor(ledColor); ledZone57.on(); break;
      case 58: ledZone58.setColor(ledColor); ledZone58.on(); break;
      case 59: ledZone59.setColor(ledColor); ledZone59.on(); break;
      case 60: ledZone60.setColor(ledColor); ledZone60.on(); break;
      case 61: ledZone61.setColor(ledColor); ledZone61.on(); break;
      case 62: ledZone62.setColor(ledColor); ledZone62.on(); break;
      case 63: ledZone63.setColor(ledColor); ledZone63.on(); break;
      case 64: ledZone64.setColor(ledColor); ledZone64.on(); break;
    }
  }
  else {
    switch (ledZone) {
      case 1: ledZone1.off(); break;
      case 2: ledZone2.off(); break;
      case 3: ledZone3.off(); break;
      case 4: ledZone4.off(); break;
      case 5: ledZone5.off(); break;
      case 6: ledZone6.off(); break;
      case 7: ledZone7.off(); break;
      case 8: ledZone8.off(); break;
      case 9: ledZone9.off(); break;
      case 10: ledZone10.off(); break;
      case 11: ledZone11.off(); break;
      case 12: ledZone12.off(); break;
      case 13: ledZone13.off(); break;
      case 14: ledZone14.off(); break;
      case 15: ledZone15.off(); break;
      case 16: ledZone16.off(); break;
      case 17: ledZone17.off(); break;
      case 18: ledZone18.off(); break;
      case 19: ledZone19.off(); break;
      case 20: ledZone20.off(); break;
      case 21: ledZone21.off(); break;
      case 22: ledZone22.off(); break;
      case 23: ledZone23.off(); break;
      case 24: ledZone24.off(); break;
      case 25: ledZone25.off(); break;
      case 26: ledZone26.off(); break;
      case 27: ledZone27.off(); break;
      case 28: ledZone28.off(); break;
      case 29: ledZone29.off(); break;
      case 30: ledZone30.off(); break;
      case 31: ledZone31.off(); break;
      case 32: ledZone32.off(); break;
      case 33: ledZone33.off(); break;
      case 34: ledZone34.off(); break;
      case 35: ledZone35.off(); break;
      case 36: ledZone36.off(); break;
      case 37: ledZone37.off(); break;
      case 38: ledZone38.off(); break;
      case 39: ledZone39.off(); break;
      case 40: ledZone40.off(); break;
      case 41: ledZone41.off(); break;
      case 42: ledZone42.off(); break;
      case 43: ledZone43.off(); break;
      case 44: ledZone44.off(); break;
      case 45: ledZone45.off(); break;
      case 46: ledZone46.off(); break;
      case 47: ledZone47.off(); break;
      case 48: ledZone48.off(); break;
      case 49: ledZone49.off(); break;
      case 50: ledZone50.off(); break;
      case 51: ledZone51.off(); break;
      case 52: ledZone52.off(); break;
      case 53: ledZone53.off(); break;
      case 54: ledZone54.off(); break;
      case 55: ledZone55.off(); break;
      case 56: ledZone56.off(); break;
      case 57: ledZone57.off(); break;
      case 58: ledZone58.off(); break;
      case 59: ledZone59.off(); break;
      case 60: ledZone60.off(); break;
      case 61: ledZone61.off(); break;
      case 62: ledZone62.off(); break;
      case 63: ledZone63.off(); break;
      case 64: ledZone64.off(); break;
    }
  }
}
