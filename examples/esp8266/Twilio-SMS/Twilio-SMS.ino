/*
 *  Twilio SMS Notification 1.2 (esp8266)
 *
 *  Processes the security system status and demonstrates how to send an SMS text message when the status has
 *  changed.  This example sends SMS text messages via Twilio: https://www.twilio.com
 *
 *  Release notes:
 *  1.2 - Check if WiFi disconnects and wait to send updates until reconnection
 *        Add appendPartition() to simplify sketch
 *        esp8266 Arduino Core version check for BearSSL
 *  1.1 - Set authentication method for BearSSL in esp8266 Arduino Core 2.5.0+
 *        Added notifications - Keybus connected, armed status, zone alarm status
 *  1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
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
 *  Many thanks to ColinNG for contributing this example: https://github.com/ColinNG
 *
 *  This example code is in the public domain.
 */

#include <ESP8266WiFi.h>
#include <dscKeybusInterface.h>

// WiFi settings
const char* wifiSSID = "";
const char* wifiPassword = "";

// Twilio settings
const char* AccountSID = "";	// Set the account SID from the Twilio Account Dashboard
const char* AuthToken = "";		// Set the auth token from the Twilio Account Dashboard
const char* Base64EncodedAuth = "";	 // macOS/Linux terminal: $ echo -n "AccountSID:AuthToken" | base64 -w 0
const char* From = "";	// i.e. 16041234567
const char* To = "";		// i.e. 16041234567

// Configures the Keybus interface with the specified pins.
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)

// Initialize components
dscKeybusInterface dsc(dscClockPin, dscReadPin);
WiFiClientSecure pushClient;
bool wifiConnected = false;


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.print(F("WiFi connected: "));
  Serial.println(WiFi.localIP());

  // Sets authentication method for BearSSL in esp8266 Arduino Core 2.5.0+
  #if HAS_ESP8266_VERSION_NUMERIC
    if (esp8266::coreVersionNumeric() >= 20500000) pushClient.setInsecure();
  #endif

  // Sends a message on startup to verify connectivity
  if (sendPush("Security system initializing")) Serial.println(F("Initialization SMS sent successfully."));
  else Serial.println(F("Initialization SMS failed to send."));

  // Starts the Keybus interface
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

  dsc.loop();

  if (dsc.statusChanged) {      // Checks if the security system status has changed
    dsc.statusChanged = false;  // Reset the status tracking flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // loop() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
    if (dsc.bufferOverflow) {
      Serial.println(F("Keybus buffer overflow"));
      dsc.bufferOverflow = false;
    }

    // Checks if the interface is connected to the Keybus
    if (dsc.keybusChanged) {
      dsc.keybusChanged = false;  // Resets the Keybus data status flag
      if (dsc.keybusConnected) sendPush("Security system connected");
      else sendPush("Security system disconnected");
    }

    // Checks status per partition
    for (byte partition = 0; partition < dscPartitions; partition++) {

      // Skips processing if the partition is disabled or in installer programming
      if (dsc.disabled[partition]) continue;

      // Checks armed status
      if (dsc.armedChanged[partition]) {
        dsc.armedChanged[partition] = false;  // Resets the partition armed status flag
        if (dsc.armed[partition]) {

          char pushMessage[40];
          if (dsc.armedAway[partition]) {
            strcpy(pushMessage, "Security system armed away: partition ");
          }
          else if (dsc.armedStay[partition]) {
            strcpy(pushMessage, "Security system armed stay: partition ");
          }
          appendPartition(partition, pushMessage);  // Appends the push message with the partition number
          sendPush(pushMessage);

        }
        else {
          char pushMessage[39] = "Security system disarmed: partition ";
          appendPartition(partition, pushMessage);  // Appends the push message with the partition number
          sendPush(pushMessage);
        }
      }

      // Checks alarm triggered status
      if (dsc.alarmChanged[partition]) {
        dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag

        char pushMessage[38] = "Security system in alarm: Partition ";
        appendPartition(partition, pushMessage);  // Appends the push message with the partition number

        if (dsc.alarm[partition]) sendPush(pushMessage);
        else sendPush("Security system disarmed after alarm");
      }

      // Checks fire alarm status
      if (dsc.fireChanged[partition]) {
        dsc.fireChanged[partition] = false;  // Resets the fire status flag

        char pushMessage[40] = "Security system fire alarm: Partition ";
        appendPartition(partition, pushMessage);  // Appends the push message with the partition number

        if (dsc.fire[partition]) sendPush(pushMessage);
        else sendPush("Security system fire alarm restored");
      }
    }

    // Checks for zones in alarm
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
            if (bitRead(dsc.alarmZones[zoneGroup], zoneBit)) {       // Zone alarm
              char pushMessage[24] = "Security zone alarm: ";
              char zoneNumber[3];
              itoa((zoneBit + 1 + (zoneGroup * 8)), zoneNumber, 10); // Determines the zone number
              strcat(pushMessage, zoneNumber);
              sendPush(pushMessage);
            }
            else {
              char pushMessage[33] = "Security zone alarm restored: ";
              char zoneNumber[3];
              itoa((zoneBit + 1 + (zoneGroup * 8)), zoneNumber, 10); // Determines the zone number
              strcat(pushMessage, zoneNumber);
              sendPush(pushMessage);
            }
          }
        }
      }
    }

    // Checks for AC power status
    if (dsc.powerChanged) {
      dsc.powerChanged = false;  // Resets the battery trouble status flag
      if (dsc.powerTrouble) sendPush("Security system AC power trouble");
      else sendPush("Security system AC power restored");
    }

    // Checks for keypad fire alarm status
    if (dsc.keypadFireAlarm) {
      dsc.keypadFireAlarm = false;  // Resets the keypad fire alarm status flag
      sendPush("Security system fire alarm button pressed");
    }

    // Checks for keypad aux auxiliary alarm status
    if (dsc.keypadAuxAlarm) {
      dsc.keypadAuxAlarm = false;  // Resets the keypad auxiliary alarm status flag
      sendPush("Security system aux alarm button pressed");
    }

    // Checks for keypad panic alarm status
    if (dsc.keypadPanicAlarm) {
      dsc.keypadPanicAlarm = false;  // Resets the keypad panic alarm status flag
      sendPush("Security system panic alarm button pressed");
    }
  }
}


bool sendPush(const char* pushMessage) {

  // Connects and sends the message as x-www-form-urlencoded
  if (!pushClient.connect("api.twilio.com", 443)) return false;
  pushClient.print(F("POST https://api.twilio.com/2010-04-01/Accounts/"));
  pushClient.print(AccountSID);
  pushClient.println(F("/Messages.json HTTP/1.1"));
  pushClient.print(F("Authorization: Basic "));
  pushClient.println(Base64EncodedAuth);
  pushClient.println(F("Host: api.twilio.com"));
  pushClient.println(F("User-Agent: ESP8266"));
  pushClient.println(F("Accept: */*"));
  pushClient.println(F("Content-Type: application/x-www-form-urlencoded"));
  pushClient.print(F("Content-Length: "));
  pushClient.println(strlen(To) + strlen(From) + strlen(pushMessage) + 18);  // Length including data
  pushClient.println("Connection: Close");
  pushClient.println();
  pushClient.print(F("To=+"));
  pushClient.print(To);
  pushClient.print(F("&From=+"));
  pushClient.print(From);
  pushClient.print(F("&Body="));
  pushClient.println(pushMessage);

  // Waits for a response
  unsigned long previousMillis = millis();
  while (!pushClient.available()) {
    dsc.loop();
    if (millis() - previousMillis > 3000) {
      Serial.println(F("Connection timed out waiting for a response."));
      pushClient.stop();
      return false;
    }
    yield();
  }

  // Reads the response until the first space - the next characters will be the HTTP status code
  while (pushClient.available()) {
    if (pushClient.read() == ' ') break;
  }

  // Checks the first character of the HTTP status code - the message was sent successfully if the status code
  // begins with "2"
  char statusCode = pushClient.read();

  // Successful, reads the remaining response to clear the client buffer
  if (statusCode == '2') {
    while (pushClient.available()) pushClient.read();
    pushClient.stop();
    return true;
  }

  // Unsuccessful, prints the response to serial to help debug
  else {
    Serial.println(F("SMS messaging error, response:"));
    Serial.print(statusCode);
    while (pushClient.available()) Serial.print((char)pushClient.read());
    Serial.println();
    pushClient.stop();
    return false;
  }
}


void appendPartition(byte sourceNumber, char* pushMessage) {
  char partitionNumber[2];
  itoa(sourceNumber + 1, partitionNumber, 10);
  strcat(pushMessage, partitionNumber);
}
