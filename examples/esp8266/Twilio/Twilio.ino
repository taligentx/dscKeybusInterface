/*
 *  Twilio Push Notification 1.0 (esp8266)
 *
 *  Processes the security system status and demonstrates how to send a push notification when the status has changed.
 *  This example sends notifications via Twilio: https://www.twilio.com
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
#include <dscKeybusInterface.h>

const char* wifiSSID = "";
const char* wifiPassword = "";

// Twilio SMS Settings
const char* AccountSID = "";	// Set the access token generated in the Twilio account settings
const char* AuthToken = "";		// 
const char* Base64EncodedAuth = "";	// echo -n "[AccountSID]:[AuthToken]" | openssl base64 -base64
const char* From = "";	// i.e. 16041234567
const char* To = "";		// i.e. 16041234567

WiFiClientSecure pushClient;

// Configures the Keybus interface with the specified pins.
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
dscKeybusInterface dsc(dscClockPin, dscReadPin);


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) delay(100);
  Serial.print(F("WiFi connected: "));
  Serial.println(WiFi.localIP());

  // Sends a push notification on startup to verify connectivity
  if (sendPush("Security system initializing")) Serial.println(F("Initialization push notification sent successfully."));
  else Serial.println(F("Initialization push notification failed to send."));

  // Starts the Keybus interface
  dsc.begin();

  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {
  if (dsc.handlePanel() && dsc.statusChanged) {  // Processes data only when a valid Keybus command has been read
    dsc.statusChanged = false;                   // Resets the status flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // handlePanel() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
    if (dsc.bufferOverflow) Serial.println(F("Keybus buffer overflow"));
    dsc.bufferOverflow = false;

    if (dsc.keypadFireAlarm) {
      dsc.keypadFireAlarm = false;  // Resets the keypad fire alarm status flag
      sendPush("Security system fire alarm button pressed");
    }

    if (dsc.keypadAuxAlarm) {
      dsc.keypadAuxAlarm = false;  // Resets the keypad auxiliary alarm status flag
      sendPush("Security system aux alarm button pressed");
    }

    if (dsc.keypadPanicAlarm) {
      dsc.keypadPanicAlarm = false;  // Resets the keypad panic alarm status flag
      sendPush("Security system panic alarm button pressed");
    }

    if (dsc.powerChanged) {
      dsc.powerChanged = false;  // Resets the battery trouble status flag
      if (dsc.powerTrouble) sendPush("Security system AC power trouble");
      else sendPush("Security system AC power restored");
    }

    // Checks status per partition
    for (byte partition = 0; partition < dscPartitions; partition++) {

      if (dsc.alarmChanged[partition]) {
        dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag

        char pushMessage[38] = "Security system in alarm, partition ";
        char partitionNumber[2];
        itoa(partition + 1, partitionNumber, 10);
        strcat(pushMessage, partitionNumber);

        if (dsc.alarm[partition]) sendPush(pushMessage);
        else sendPush("Security system disarmed after alarm");
      }

      if (dsc.fireChanged[partition]) {
        dsc.fireChanged[partition] = false;  // Resets the fire status flag

        char pushMessage[40] = "Security system fire alarm, partition ";
        char partitionNumber[2];
        itoa(partition + 1, partitionNumber, 10);
        strcat(pushMessage, partitionNumber);

        if (dsc.fire[partition]) sendPush(pushMessage);
        else sendPush("Security system fire alarm restored");
      }
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
    dsc.handlePanel();
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
    Serial.println(F("Push notification error, response:"));
    Serial.print(statusCode);
    while (pushClient.available()) Serial.print((char)pushClient.read());
    Serial.println();
    pushClient.stop();
    return false;
  }
}


