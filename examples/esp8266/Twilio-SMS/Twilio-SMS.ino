/*
 *  Twilio SMS Notification 1.3 (esp8266)
 *
 *  Processes the security system status and demonstrates how to send an SMS text message when the status has
 *  changed.  This example sends SMS text messages via Twilio: https://www.twilio.com
 *
 *  Release notes:
 *    1.3 - Updated esp8266 wiring diagram for 33k/10k resistors
 *    1.2 - Check if WiFi disconnects and wait to send updates until reconnection
 *          Add appendPartition() to simplify sketch
 *          esp8266 Arduino Core version check for BearSSL
 *    1.1 - Set authentication method for BearSSL in esp8266 Arduino Core 2.5.0+
 *          Added notifications - Keybus connected, armed status, zone alarm status
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
 *  Many thanks to ColinNG for contributing this example: https://github.com/ColinNG
 *
 *  This example code is in the public domain.
 */

#include <ESP8266WiFi.h>
#include <dscKeybusInterface.h>

// Settings
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* AccountSID = "";	// Set the account SID from the Twilio Account Dashboard
const char* AuthToken = "";		// Set the auth token from the Twilio Account Dashboard
const char* Base64EncodedAuth = "";	 // macOS/Linux terminal: $ echo -n "AccountSID:AuthToken" | base64 -w 0
const char* From = "";	// i.e. 16041234567
const char* To = "";		// i.e. 16041234567
const char* messagePrefix = "[Security system] ";  // Set a prefix for all messages

// Configures the Keybus interface with the specified pins.
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin  D2  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)

// Initialize components
dscKeybusInterface dsc(dscClockPin, dscReadPin);
WiFiClientSecure ipClient;
bool wifiConnected = true;


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  Serial.print(F("WiFi"));
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print(F("connected: "));
  Serial.println(WiFi.localIP());
  ipClient.setInsecure();

  // Sends a message on startup to verify connectivity
  Serial.print(F("Twilio...."));
  if (sendMessage("Initializing")) Serial.println(F("connected."));
  else Serial.println(F("connection error."));

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
      if (dsc.keybusConnected) sendMessage("Connected");
      else sendMessage("Disconnected");
    }

    // Checks status per partition
    for (byte partition = 0; partition < dscPartitions; partition++) {

      // Skips processing if the partition is disabled or in installer programming
      if (dsc.disabled[partition]) continue;

      // Checks armed status
      if (dsc.armedChanged[partition]) {
        if (dsc.armed[partition]) {
          char messageContent[25];

          if (dsc.armedAway[partition] && dsc.noEntryDelay[partition]) strcpy(messageContent, "Armed night: Partition ");
          else if (dsc.armedAway[partition]) strcpy(messageContent, "Armed away: Partition ");
          else if (dsc.armedStay[partition] && dsc.noEntryDelay[partition]) strcpy(messageContent, "Armed night: Partition ");
          else if (dsc.armedStay[partition]) strcpy(messageContent, "Armed stay: Partition ");

          appendPartition(partition, messageContent);  // Appends the message with the partition number
          sendMessage(messageContent);
        }
        else {
          char messageContent[22] = "Disarmed: Partition ";
          appendPartition(partition, messageContent);  // Appends the message with the partition number
          sendMessage(messageContent);
        }
      }

      // Checks exit delay status
      if (dsc.exitDelayChanged[partition]) {
        dsc.exitDelayChanged[partition] = false;  // Resets the exit delay status flag

        if (dsc.exitDelay[partition]) {
          char messageContent[36] = "Exit delay in progress: Partition ";
          appendPartition(partition, messageContent);  // Appends the message with the partition number
          sendMessage(messageContent);
        }
        else if (!dsc.exitDelay[partition] && !dsc.armed[partition]) {
          char messageContent[22] = "Disarmed: Partition ";
          appendPartition(partition, messageContent);  // Appends the message with the partition number
          sendMessage(messageContent);
        }
      }

      // Checks alarm triggered status
      if (dsc.alarmChanged[partition]) {
        dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag

        if (dsc.alarm[partition]) {
          char messageContent[19] = "Alarm: Partition ";
          appendPartition(partition, messageContent);  // Appends the message with the partition number
          sendMessage(messageContent);
        }
        else if (!dsc.armedChanged[partition]) {
          char messageContent[22] = "Disarmed: Partition ";
          appendPartition(partition, messageContent);  // Appends the message with the partition number
          sendMessage(messageContent);
        }
      }
      dsc.armedChanged[partition] = false;  // Resets the partition armed status flag

      // Checks fire alarm status
      if (dsc.fireChanged[partition]) {
        dsc.fireChanged[partition] = false;  // Resets the fire status flag

        if (dsc.fire[partition]) {
          char messageContent[24] = "Fire alarm: Partition ";
          appendPartition(partition, messageContent);  // Appends the message with the partition number
          sendMessage(messageContent);
        }
        else {
          char messageContent[33] = "Fire alarm restored: Partition ";
          appendPartition(partition, messageContent);  // Appends the message with the partition number
          sendMessage(messageContent);
        }
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
              char messageContent[15] = "Zone alarm: ";
              char zoneNumber[3];
              itoa((zoneBit + 1 + (zoneGroup * 8)), zoneNumber, 10); // Determines the zone number
              strcat(messageContent, zoneNumber);
              sendMessage(messageContent);
            }
            else {
              char messageContent[24] = "Zone alarm restored: ";
              char zoneNumber[3];
              itoa((zoneBit + 1 + (zoneGroup * 8)), zoneNumber, 10); // Determines the zone number
              strcat(messageContent, zoneNumber);
              sendMessage(messageContent);
            }
          }
        }
      }
    }

    // Checks trouble status
    if (dsc.troubleChanged) {
      dsc.troubleChanged = false;  // Resets the trouble status flag
      if (dsc.trouble) sendMessage("Trouble status on");
      else sendMessage("Trouble status restored");
    }

    // Checks for AC power status
    if (dsc.powerChanged) {
      dsc.powerChanged = false;  // Resets the battery trouble status flag
      if (dsc.powerTrouble) sendMessage("AC power trouble");
      else sendMessage("AC power restored");
    }

    // Checks panel battery status
    if (dsc.batteryChanged) {
      dsc.batteryChanged = false;  // Resets the battery trouble status flag
      if (dsc.batteryTrouble) sendMessage("Panel battery trouble");
      else sendMessage("Panel battery restored");
    }

    // Checks for keypad fire alarm status
    if (dsc.keypadFireAlarm) {
      dsc.keypadFireAlarm = false;  // Resets the keypad fire alarm status flag
      sendMessage("Keypad Fire alarm");
    }

    // Checks for keypad aux auxiliary alarm status
    if (dsc.keypadAuxAlarm) {
      dsc.keypadAuxAlarm = false;  // Resets the keypad auxiliary alarm status flag
      sendMessage("Keypad Aux alarm");
    }

    // Checks for keypad panic alarm status
    if (dsc.keypadPanicAlarm) {
      dsc.keypadPanicAlarm = false;  // Resets the keypad panic alarm status flag
      sendMessage("Keypad Panic alarm");
    }
  }
}


bool sendMessage(const char* messageContent) {

  // Connects and sends the message as x-www-form-urlencoded
  if (!ipClient.connect("api.twilio.com", 443)) return false;
  ipClient.print(F("POST https://api.twilio.com/2010-04-01/Accounts/"));
  ipClient.print(AccountSID);
  ipClient.println(F("/Messages.json HTTP/1.1"));
  ipClient.print(F("Authorization: Basic "));
  ipClient.println(Base64EncodedAuth);
  ipClient.println(F("Host: api.twilio.com"));
  ipClient.println(F("User-Agent: ESP8266"));
  ipClient.println(F("Accept: */*"));
  ipClient.println(F("Content-Type: application/x-www-form-urlencoded"));
  ipClient.print(F("Content-Length: "));
  ipClient.println(strlen(To) + strlen(From) + strlen(messagePrefix) + strlen(messageContent) + 18);  // Length including data
  ipClient.println("Connection: Close");
  ipClient.println();
  ipClient.print(F("To=+"));
  ipClient.print(To);
  ipClient.print(F("&From=+"));
  ipClient.print(From);
  ipClient.print(F("&Body="));
  ipClient.print(messagePrefix);
  ipClient.println(messageContent);

  // Waits for a response
  unsigned long previousMillis = millis();
  while (!ipClient.available()) {
    dsc.loop();
    if (millis() - previousMillis > 3000) {
      Serial.println(F("Connection timed out waiting for a response."));
      ipClient.stop();
      return false;
    }
  }

  // Reads the response until the first space - the next characters will be the HTTP status code
  while (ipClient.available()) {
    if (ipClient.read() == ' ') break;
  }

  // Checks the first character of the HTTP status code - the message was sent successfully if the status code
  // begins with "2"
  char statusCode = ipClient.read();

  // Successful, reads the remaining response to clear the client buffer
  if (statusCode == '2') {
    while (ipClient.available()) ipClient.read();
    ipClient.stop();
    return true;
  }

  // Unsuccessful, prints the response to serial to help debug
  else {
    Serial.println(F("SMS messaging error, response:"));
    Serial.print(statusCode);
    while (ipClient.available()) Serial.print((char)ipClient.read());
    Serial.println();
    ipClient.stop();
    return false;
  }
}


void appendPartition(byte sourceNumber, char* messageContent) {
  char partitionNumber[2];
  itoa(sourceNumber + 1, partitionNumber, 10);
  strcat(messageContent, partitionNumber);
}
