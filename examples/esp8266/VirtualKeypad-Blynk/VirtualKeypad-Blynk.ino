/*
 *  VirtualKeypad-Blynk 1.1 (esp8266)
 *
 *  Provides a virtual keypad interface for the free Blynk (https://www.blynk.cc) app on iOS and Android.
 *
 *  Usage:
 *  1. Scan one of the following QR codes from within the Blynk app for an example keypad layout:
 *    16 zones: https://user-images.githubusercontent.com/12835671/42364287-41ca6662-80c0-11e8-85e7-d579b542568d.png
 *    32 zones: https://user-images.githubusercontent.com/12835671/42364293-4512b720-80c0-11e8-87bd-153c4e857b4e.png
 *  2. Navigate to Project Settings > Devices > DSC Keybus Interface > DSC KeybusInterface.
 *  3. Select "Refresh" to generate a new auth token.
 *  4. Go back to Project Settings, copy the auth token, and paste it in an email or message to yourself.
 *  5. Add the auth token to the sketch below.
 *
 *  Installing Blynk as a local server (https://github.com/blynkkk/blynk-server) is recommended to keep control of the
 *  security system internal to your network.  This also lets you use as many widgets as needed for free - local
 *  servers can setup users with any amount of Blynk Energy.  Using the default Blynk cloud service with the above
 *  example layouts requires more of Blynk's Energy units than available on the free usage tier.
 *
 *  Blynk virtual pin mapping:
 *    V0 - Keypad 0 ... V9 - Keypad 9
 *    V10 - Keypad *
 *    V11 - Keypad #
 *    V12 - Keypad stay
 *    V13 - Keypad away
 *    V14 - Keypad fire alarm
 *    V15 - Keypad aux alarm
 *    V16 - Keypad panic alarm
 *    V30 - Partition number menu
 *    V40 - LCD
 *    V50 - LED Ready
 *    V51 - LED Armed
 *    V52 - LED Memory
 *    V53 - LED Bypass
 *    V54 - LED Trouble
 *    V55 - LED Program
 *    V56 - LED Fire
 *    V61 - Zone 1 ... V124 - Zone 64
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

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <dscKeybusInterface.h>

#define BLYNK_PRINT Serial

char blynkAuthToken[] = "";  // Token generated from within the Blynk app
char blynkServer[] = "";     // Blynk local server address
int blynkPort = 8080;        // Blynk local server port
char wifiSSID[] = "";
char wifiPassword[] = "";

// Configures the Keybus interface with the specified pins - dscWritePin is
// optional, leaving it out disables the virtual keypad
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);

WidgetLCD lcd(V40);
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

bool partitionChanged;
byte viewPartition = 1;
const char* lcdPartition = "Partition ";


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  WiFi.mode(WIFI_STA);
  Blynk.begin(blynkAuthToken, wifiSSID, wifiPassword, blynkServer, blynkPort);

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();

  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {
  Blynk.run();

  if (dsc.handlePanel() && (dsc.statusChanged)) {  // Processes data only when a valid Keybus command has been read
    dsc.statusChanged = false;                     // Resets the status flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // handlePanel() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
    if (dsc.bufferOverflow) Serial.println(F("Keybus buffer overflow"));
    dsc.bufferOverflow = false;

    // Checks status for the currently viewed partition
    byte partition = viewPartition - 1;

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
    if (dsc.openZonesStatusChanged) {
      dsc.openZonesStatusChanged = false;                           // Resets the open zones status flag
      for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
        for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
          if (bitRead(dsc.openZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual open zone status flag
            bitWrite(dsc.openZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual open zone status flag
            if (bitRead(dsc.openZones[zoneGroup], zoneBit)) {
              switch (zoneBit + 1 + (zoneGroup * 8)) {
                case 1: ledZone1.on(); break;
                case 2: ledZone2.on(); break;
                case 3: ledZone3.on(); break;
                case 4: ledZone4.on(); break;
                case 5: ledZone5.on(); break;
                case 6: ledZone6.on(); break;
                case 7: ledZone7.on(); break;
                case 8: ledZone8.on(); break;
                case 9: ledZone9.on(); break;
                case 10: ledZone10.on(); break;
                case 11: ledZone11.on(); break;
                case 12: ledZone12.on(); break;
                case 13: ledZone13.on(); break;
                case 14: ledZone14.on(); break;
                case 15: ledZone15.on(); break;
                case 16: ledZone16.on(); break;
                case 17: ledZone17.on(); break;
                case 18: ledZone18.on(); break;
                case 19: ledZone19.on(); break;
                case 20: ledZone20.on(); break;
                case 21: ledZone21.on(); break;
                case 22: ledZone22.on(); break;
                case 23: ledZone23.on(); break;
                case 24: ledZone24.on(); break;
                case 25: ledZone25.on(); break;
                case 26: ledZone26.on(); break;
                case 27: ledZone27.on(); break;
                case 28: ledZone28.on(); break;
                case 29: ledZone29.on(); break;
                case 30: ledZone30.on(); break;
                case 31: ledZone31.on(); break;
                case 32: ledZone32.on(); break;
                case 33: ledZone33.on(); break;
                case 34: ledZone34.on(); break;
                case 35: ledZone35.on(); break;
                case 36: ledZone36.on(); break;
                case 37: ledZone37.on(); break;
                case 38: ledZone38.on(); break;
                case 39: ledZone39.on(); break;
                case 40: ledZone40.on(); break;
                case 41: ledZone41.on(); break;
                case 42: ledZone42.on(); break;
                case 43: ledZone43.on(); break;
                case 44: ledZone44.on(); break;
                case 45: ledZone45.on(); break;
                case 46: ledZone46.on(); break;
                case 47: ledZone47.on(); break;
                case 48: ledZone48.on(); break;
                case 49: ledZone49.on(); break;
                case 50: ledZone50.on(); break;
                case 51: ledZone51.on(); break;
                case 52: ledZone52.on(); break;
                case 53: ledZone53.on(); break;
                case 54: ledZone54.on(); break;
                case 55: ledZone55.on(); break;
                case 56: ledZone56.on(); break;
                case 57: ledZone57.on(); break;
                case 58: ledZone58.on(); break;
                case 59: ledZone59.on(); break;
                case 60: ledZone60.on(); break;
                case 61: ledZone61.on(); break;
                case 62: ledZone62.on(); break;
                case 63: ledZone63.on(); break;
                case 64: ledZone64.on(); break;
              }
            }
            else {
              switch (zoneBit + 1 + (zoneGroup * 8)) {
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
            bitWrite(dsc.alarmZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual alarm zone status flag
            if (bitRead(dsc.alarmZones[zoneGroup], zoneBit)) {
              switch (zoneBit + 1 + (zoneGroup * 8)) {               // Sets LED color to red on zone alarm
                case 1: ledZone1.setColor("#D3435C"); break;
                case 2: ledZone2.setColor("#D3435C"); break;
                case 3: ledZone3.setColor("#D3435C"); break;
                case 4: ledZone4.setColor("#D3435C"); break;
                case 5: ledZone5.setColor("#D3435C"); break;
                case 6: ledZone6.setColor("#D3435C"); break;
                case 7: ledZone7.setColor("#D3435C"); break;
                case 8: ledZone8.setColor("#D3435C"); break;
                case 9: ledZone9.setColor("#D3435C"); break;
                case 10: ledZone10.setColor("#D3435C"); break;
                case 11: ledZone11.setColor("#D3435C"); break;
                case 12: ledZone12.setColor("#D3435C"); break;
                case 13: ledZone13.setColor("#D3435C"); break;
                case 14: ledZone14.setColor("#D3435C"); break;
                case 15: ledZone15.setColor("#D3435C"); break;
                case 16: ledZone16.setColor("#D3435C"); break;
                case 17: ledZone17.setColor("#D3435C"); break;
                case 18: ledZone18.setColor("#D3435C"); break;
                case 19: ledZone19.setColor("#D3435C"); break;
                case 20: ledZone20.setColor("#D3435C"); break;
                case 21: ledZone21.setColor("#D3435C"); break;
                case 22: ledZone22.setColor("#D3435C"); break;
                case 23: ledZone23.setColor("#D3435C"); break;
                case 24: ledZone24.setColor("#D3435C"); break;
                case 25: ledZone25.setColor("#D3435C"); break;
                case 26: ledZone26.setColor("#D3435C"); break;
                case 27: ledZone27.setColor("#D3435C"); break;
                case 28: ledZone28.setColor("#D3435C"); break;
                case 29: ledZone29.setColor("#D3435C"); break;
                case 30: ledZone30.setColor("#D3435C"); break;
                case 31: ledZone31.setColor("#D3435C"); break;
                case 32: ledZone32.setColor("#D3435C"); break;
                case 33: ledZone33.setColor("#D3435C"); break;
                case 34: ledZone34.setColor("#D3435C"); break;
                case 35: ledZone35.setColor("#D3435C"); break;
                case 36: ledZone36.setColor("#D3435C"); break;
                case 37: ledZone37.setColor("#D3435C"); break;
                case 38: ledZone38.setColor("#D3435C"); break;
                case 39: ledZone39.setColor("#D3435C"); break;
                case 40: ledZone40.setColor("#D3435C"); break;
                case 41: ledZone41.setColor("#D3435C"); break;
                case 42: ledZone42.setColor("#D3435C"); break;
                case 43: ledZone43.setColor("#D3435C"); break;
                case 44: ledZone44.setColor("#D3435C"); break;
                case 45: ledZone45.setColor("#D3435C"); break;
                case 46: ledZone46.setColor("#D3435C"); break;
                case 47: ledZone47.setColor("#D3435C"); break;
                case 48: ledZone48.setColor("#D3435C"); break;
                case 49: ledZone49.setColor("#D3435C"); break;
                case 50: ledZone50.setColor("#D3435C"); break;
                case 51: ledZone51.setColor("#D3435C"); break;
                case 52: ledZone52.setColor("#D3435C"); break;
                case 53: ledZone53.setColor("#D3435C"); break;
                case 54: ledZone54.setColor("#D3435C"); break;
                case 55: ledZone55.setColor("#D3435C"); break;
                case 56: ledZone56.setColor("#D3435C"); break;
                case 57: ledZone57.setColor("#D3435C"); break;
                case 58: ledZone58.setColor("#D3435C"); break;
                case 59: ledZone59.setColor("#D3435C"); break;
                case 60: ledZone60.setColor("#D3435C"); break;
                case 61: ledZone61.setColor("#D3435C"); break;
                case 62: ledZone62.setColor("#D3435C"); break;
                case 63: ledZone63.setColor("#D3435C"); break;
                case 64: ledZone64.setColor("#D3435C"); break;
              }
            }
            else {
              switch (zoneBit + 1 + (zoneGroup * 8)) {               // Restores zone color to green
                case 1: ledZone1.setColor("#23C48E"); break;
                case 2: ledZone2.setColor("#23C48E"); break;
                case 3: ledZone3.setColor("#23C48E"); break;
                case 4: ledZone4.setColor("#23C48E"); break;
                case 5: ledZone5.setColor("#23C48E"); break;
                case 6: ledZone6.setColor("#23C48E"); break;
                case 7: ledZone7.setColor("#23C48E"); break;
                case 8: ledZone8.setColor("#23C48E"); break;
                case 9: ledZone9.setColor("#23C48E"); break;
                case 10: ledZone10.setColor("#23C48E"); break;
                case 11: ledZone11.setColor("#23C48E"); break;
                case 12: ledZone12.setColor("#23C48E"); break;
                case 13: ledZone13.setColor("#23C48E"); break;
                case 14: ledZone14.setColor("#23C48E"); break;
                case 15: ledZone15.setColor("#23C48E"); break;
                case 16: ledZone16.setColor("#23C48E"); break;
                case 17: ledZone17.setColor("#23C48E"); break;
                case 18: ledZone18.setColor("#23C48E"); break;
                case 19: ledZone19.setColor("#23C48E"); break;
                case 20: ledZone20.setColor("#23C48E"); break;
                case 21: ledZone21.setColor("#23C48E"); break;
                case 22: ledZone22.setColor("#23C48E"); break;
                case 23: ledZone23.setColor("#23C48E"); break;
                case 24: ledZone24.setColor("#23C48E"); break;
                case 25: ledZone25.setColor("#23C48E"); break;
                case 26: ledZone26.setColor("#23C48E"); break;
                case 27: ledZone27.setColor("#23C48E"); break;
                case 28: ledZone28.setColor("#23C48E"); break;
                case 29: ledZone29.setColor("#23C48E"); break;
                case 30: ledZone30.setColor("#23C48E"); break;
                case 31: ledZone31.setColor("#23C48E"); break;
                case 32: ledZone32.setColor("#23C48E"); break;
                case 33: ledZone33.setColor("#23C48E"); break;
                case 34: ledZone34.setColor("#23C48E"); break;
                case 35: ledZone35.setColor("#23C48E"); break;
                case 36: ledZone36.setColor("#23C48E"); break;
                case 37: ledZone37.setColor("#23C48E"); break;
                case 38: ledZone38.setColor("#23C48E"); break;
                case 39: ledZone39.setColor("#23C48E"); break;
                case 40: ledZone40.setColor("#23C48E"); break;
                case 41: ledZone41.setColor("#23C48E"); break;
                case 42: ledZone42.setColor("#23C48E"); break;
                case 43: ledZone43.setColor("#23C48E"); break;
                case 44: ledZone44.setColor("#23C48E"); break;
                case 45: ledZone45.setColor("#23C48E"); break;
                case 46: ledZone46.setColor("#23C48E"); break;
                case 47: ledZone47.setColor("#23C48E"); break;
                case 48: ledZone48.setColor("#23C48E"); break;
                case 49: ledZone49.setColor("#23C48E"); break;
                case 50: ledZone50.setColor("#23C48E"); break;
                case 51: ledZone51.setColor("#23C48E"); break;
                case 52: ledZone52.setColor("#23C48E"); break;
                case 53: ledZone53.setColor("#23C48E"); break;
                case 54: ledZone54.setColor("#23C48E"); break;
                case 55: ledZone55.setColor("#23C48E"); break;
                case 56: ledZone56.setColor("#23C48E"); break;
                case 57: ledZone57.setColor("#23C48E"); break;
                case 58: ledZone58.setColor("#23C48E"); break;
                case 59: ledZone59.setColor("#23C48E"); break;
                case 60: ledZone60.setColor("#23C48E"); break;
                case 61: ledZone61.setColor("#23C48E"); break;
                case 62: ledZone62.setColor("#23C48E"); break;
                case 63: ledZone63.setColor("#23C48E"); break;
                case 64: ledZone64.setColor("#23C48E"); break;
              }
            }
          }
        }
      }
    }

    if (dsc.powerChanged) {
      dsc.powerChanged = false;  // Resets the power trouble status flag
      if (dsc.powerTrouble) {
        lcd.clear();
        lcd.print(0,0,"Power trouble");
      }
      else {
        lcd.clear();
        lcd.print(0,0,"Power restored");
      }
    }

    if (dsc.batteryChanged) {
      dsc.batteryChanged = false;  // Resets the battery trouble status flag
      if (dsc.batteryTrouble) {
        lcd.clear();
        lcd.print(0,0,"Battery trouble");
      }
      else {
        lcd.clear();
        lcd.print(0,0,"Battery restored");
      }
    }
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
      viewPartition = 1;
      changedPartition(viewPartition - 1);
      dsc.writePartition = 1;
      break;
    }
    case 2: {
      viewPartition = 2;
      changedPartition(viewPartition - 1);
      dsc.writePartition = 2;
      break;
    }
    case 3: {
      viewPartition = 3;
      changedPartition(viewPartition - 1);
      dsc.writePartition = 3;
      break;
    }
    case 4: {
      viewPartition = 4;
      changedPartition(viewPartition - 1);
      dsc.writePartition = 4;
      break;
    }
    case 5: {
      viewPartition = 5;
      changedPartition(viewPartition - 1);
      dsc.writePartition = 5;
      break;
    }
    case 6: {
      viewPartition = 6;
      changedPartition(viewPartition - 1);
      dsc.writePartition = 6;
      break;
    }
    case 7: {
      viewPartition = 7;
      changedPartition(viewPartition - 1);
      dsc.writePartition = 7;
      break;
    }
    case 8: {
      viewPartition = 8;
      changedPartition(viewPartition - 1);
      dsc.writePartition = 8;
      break;
    }
    default: {
      viewPartition = 1;
      changedPartition(viewPartition - 1);
      dsc.writePartition = 1;
      break;
    }
  }
  byte position = strlen(lcdPartition);
  lcd.print(0,0, "                ");
  lcd.print(0,0, lcdPartition);
  lcd.print(position, 0, viewPartition);
}


void changedPartition(byte partition) {
  partitionChanged = true;
  setStatus(partition);
  setLights(partition);
  partitionChanged = false;
}


void setLights(byte partition) {
  if (bitRead(dsc.lights[partition],0)) ledReady.on();
  else ledReady.off();

  if (bitRead(dsc.lights[partition],1)) ledArmed.on();
  else ledArmed.off();

  if (bitRead(dsc.lights[partition],2)) ledMemory.on();
  else ledMemory.off();

  if (bitRead(dsc.lights[partition],3)) ledBypass.on();
  else ledBypass.off();

  if (bitRead(dsc.lights[partition],4)) ledTrouble.on();
  else ledTrouble.off();

  if (bitRead(dsc.lights[partition],5)) ledProgram.on();
  else ledProgram.off();

  if (bitRead(dsc.lights[partition],6)) ledFire.on();
  else ledFire.off();
}


void setStatus(byte partition) {
  static byte lastStatus[8];
  if (!partitionChanged && dsc.status[partition] == lastStatus[partition]) return;
  lastStatus[partition] = dsc.status[partition];

  switch (dsc.status[partition]) {
    case 0x01: lcd.print(0,1, "                "); lcd.print(0,1, "Ready"); break;
    case 0x02: lcd.print(0,1, "                "); lcd.print(0,1, "Stay zones open"); break;
    case 0x03: lcd.print(0,1, "                "); lcd.print(0,1, "Zones open"); break;
    case 0x04: lcd.print(0,1, "                "); lcd.print(0,1, "Armed stay"); break;
    case 0x05: lcd.print(0,1, "                "); lcd.print(0,1, "Armed away"); break;
    case 0x07: lcd.print(0,1, "                "); lcd.print(0,1, "Failed to arm"); break;
    case 0x08: lcd.print(0,1, "                "); lcd.print(0,1, "Exit delay"); break;
    case 0x09: lcd.print(0,1, "                "); lcd.print(0,1, "No entry delay"); break;
    case 0x0B: lcd.print(0,1, "                "); lcd.print(0,1, "Quick exit"); break;
    case 0x0C: lcd.print(0,1, "                "); lcd.print(0,1, "Entry delay"); break;
    case 0x0D: lcd.print(0,1, "                "); lcd.print(0,1, "Alarm memory"); break;
    case 0x10: lcd.print(0,1, "                "); lcd.print(0,1, "Keypad lockout"); break;
    case 0x11: lcd.print(0,1, "                "); lcd.print(0,1, "Alarm"); break;
    case 0x14: lcd.print(0,1, "                "); lcd.print(0,1, "Auto-arm"); break;
    case 0x16: lcd.print(0,1, "                "); lcd.print(0,1, "No entry delay"); break;
    case 0x22: lcd.print(0,1, "                "); lcd.print(0,1, "Alarm memory"); break;
    case 0x33: lcd.print(0,1, "                "); lcd.print(0,1, "Busy"); break;
    case 0x3D: lcd.print(0,1, "                "); lcd.print(0,1, "Disarmed"); break;
    case 0x3E: lcd.print(0,1, "                "); lcd.print(0,1, "Disarmed"); break;
    case 0x40: lcd.print(0,1, "                "); lcd.print(0,1, "Keypad blanked"); break;
    case 0x8A: lcd.print(0,1, "                "); lcd.print(0,1, "Activate zones"); break;
    case 0x8B: lcd.print(0,1, "                "); lcd.print(0,1, "Quick exit"); break;
    case 0x8E: lcd.print(0,1, "                "); lcd.print(0,1, "Invalid option"); break;
    case 0x8F: lcd.print(0,1, "                "); lcd.print(0,1, "Invalid code"); break;
    case 0x9E: lcd.print(0,1, "                "); lcd.print(0,1, "Enter * code"); break;
    case 0x9F: lcd.print(0,1, "                "); lcd.print(0,1, "Access code"); break;
    case 0xA0: lcd.print(0,1, "                "); lcd.print(0,1, "Zone bypass"); break;
    case 0xA1: lcd.print(0,1, "                "); lcd.print(0,1, "Trouble menu"); break;
    case 0xA2: lcd.print(0,1, "                "); lcd.print(0,1, "Alarm memory"); break;
    case 0xA3: lcd.print(0,1, "                "); lcd.print(0,1, "Door chime on"); break;
    case 0xA4: lcd.print(0,1, "                "); lcd.print(0,1, "Door chime off"); break;
    case 0xA5: lcd.print(0,1, "                "); lcd.print(0,1, "Master code"); break;
    case 0xA6: lcd.print(0,1, "                "); lcd.print(0,1, "Access codes"); break;
    case 0xA7: lcd.print(0,1, "                "); lcd.print(0,1, "Enter new code"); break;
    case 0xA9: lcd.print(0,1, "                "); lcd.print(0,1, "User function"); break;
    case 0xAA: lcd.print(0,1, "                "); lcd.print(0,1, "Time and Date"); break;
    case 0xAB: lcd.print(0,1, "                "); lcd.print(0,1, "Auto-arm time"); break;
    case 0xAC: lcd.print(0,1, "                "); lcd.print(0,1, "Auto-arm on"); break;
    case 0xAD: lcd.print(0,1, "                "); lcd.print(0,1, "Auto-arm off"); break;
    case 0xAF: lcd.print(0,1, "                "); lcd.print(0,1, "System test"); break;
    case 0xB0: lcd.print(0,1, "                "); lcd.print(0,1, "Enable DLS"); break;
    case 0xB2: lcd.print(0,1, "                "); lcd.print(0,1, "Command output"); break;
    case 0xB7: lcd.print(0,1, "                "); lcd.print(0,1, "Installer code"); break;
    case 0xB8: lcd.print(0,1, "                "); lcd.print(0,1, "Enter * code"); break;
    case 0xB9: lcd.print(0,1, "                "); lcd.print(0,1, "Zone tamper"); break;
    case 0xBA: lcd.print(0,1, "                "); lcd.print(0,1, "Zones low batt."); break;
    case 0xC6: lcd.print(0,1, "                "); lcd.print(0,1, "Zone fault menu"); break;
    case 0xC8: lcd.print(0,1, "                "); lcd.print(0,1, "Service required"); break;
    case 0xD0: lcd.print(0,1, "                "); lcd.print(0,1, "Keypads low batt"); break;
    case 0xD1: lcd.print(0,1, "                "); lcd.print(0,1, "Wireless low bat"); break;
    case 0xE4: lcd.print(0,1, "                "); lcd.print(0,1, "Installer menu"); break;
    case 0xE5: lcd.print(0,1, "                "); lcd.print(0,1, "Keypad slot"); break;
    case 0xE6: lcd.print(0,1, "                "); lcd.print(0,1, "Input: 2 digits"); break;
    case 0xE7: lcd.print(0,1, "                "); lcd.print(0,1, "Input: 3 digits"); break;
    case 0xE8: lcd.print(0,1, "                "); lcd.print(0,1, "Input: 4 digits"); break;
    case 0xEA: lcd.print(0,1, "                "); lcd.print(0,1, "Code: 2 digits"); break;
    case 0xEB: lcd.print(0,1, "                "); lcd.print(0,1, "Code: 4 digits"); break;
    case 0xEC: lcd.print(0,1, "                "); lcd.print(0,1, "Input: 6 digits"); break;
    case 0xED: lcd.print(0,1, "                "); lcd.print(0,1, "Input: 32 digits"); break;
    case 0xEE: lcd.print(0,1, "                "); lcd.print(0,1, "Input: option"); break;
    case 0xF0: lcd.print(0,1, "                "); lcd.print(0,1, "Function key 1"); break;
    case 0xF1: lcd.print(0,1, "                "); lcd.print(0,1, "Function key 2"); break;
    case 0xF2: lcd.print(0,1, "                "); lcd.print(0,1, "Function key 3"); break;
    case 0xF3: lcd.print(0,1, "                "); lcd.print(0,1, "Function key 4"); break;
    case 0xF4: lcd.print(0,1, "                "); lcd.print(0,1, "Function key 5"); break;
    case 0xF8: lcd.print(0,1, "                "); lcd.print(0,1, "Keypad program"); break;
    default: return;
  }
}


void printFire(byte partition) {
  if (dsc.fire[partition]) {
    lcd.clear();
    byte position = strlen(lcdPartition);
    lcd.print(0,0, lcdPartition);
    lcd.print(position, 0, partition + 1);
    lcd.print(0,1, "Fire alarm");
  }
  else {
    lcd.clear();
    byte position = strlen(lcdPartition);
    lcd.print(0,0, lcdPartition);
    lcd.print(position, 0, partition + 1);
    lcd.print(0,1, "Fire alarm off");
  }
}

