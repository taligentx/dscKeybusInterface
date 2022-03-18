/*
 *  Twilio SMS Notification 1.5 (esp8266)
 *
 *  Processes the security system status and demonstrates how to send an SMS text message when the status has
 *  changed.  This example sends SMS text messages via Twilio: https://www.twilio.com
 *
 *  Release notes:
 *    1.5 - Add TLS root certificate for Twilio
 *          Encode authorization data in base64 directly within the sketch
 *    1.4 - Added DSC Classic series support
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
 *                                         +--- dscClockPin  // Default: D1, GPIO 5
 *      DSC Yellow --- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin  // Default: D2, GPIO 4
 *      DSC Green ---- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *      Classic series only, PGM configured for PC-16 output:
 *      DSC PGM ---+-- 1k ohm resistor --- DSC Aux(+)
 *                 |
 *                 |                       +--- dscPC16Pin   // Default: D7, GPIO 13
 *                 +-- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  Many thanks to ColinNG for contributing this example: https://github.com/ColinNG
 *
 *  This example code is in the public domain.
 */

// DSC Classic series: uncomment for PC1500/PC1550 support (requires PC16-OUT configuration per README.md)
//#define dscClassicSeries

#include <ESP8266WiFi.h>
#include <dscKeybusInterface.h>
#include "base64.h"

// Settings
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* AccountSID = "";    // Set the account SID from the Twilio Account Dashboard
const char* AuthToken = "";     // Set the auth token from the Twilio Account Dashboard
const char* From = "";          // From phone number, starting with the country code without the + sign: 18005551234
const char* To = "";            // To phone number, starting with the country code without the + sign: 18005551234
const char* messagePrefix = "[Security system] ";  // Set a prefix for all messages

// Configures the Keybus interface with the specified pins.
#define dscClockPin D1  // GPIO 5
#define dscReadPin  D2  // GPIO 4
#define dscPC16Pin  D7  // DSC Classic Series only, GPIO 13

// HTTPS root certificate for api.twilio.com: DigiCert Global Root CA, expires 2031.11.10
const char twilioCertificateRoot[] = R"=EOF=(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)=EOF=";

// Initialize components
#ifndef dscClassicSeries
dscKeybusInterface dsc(dscClockPin, dscReadPin);
#else
dscClassicInterface dsc(dscClockPin, dscReadPin, dscPC16Pin);
#endif
X509List twilioCert(twilioCertificateRoot);
WiFiClientSecure ipClient;
bool wifiConnected = true;
char twilioAuth[128];
size_t twilioAuthLength = 128;
char encodedTwilioAuth[128], encodedMessagePrefix[128], encodedMessageContent[480];


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  Serial.print(F("WiFi...."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  ipClient.setTrustAnchors(&twilioCert);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print(F("connected: "));
  Serial.println(WiFi.localIP());

  Serial.print(F("NTP time...."));
  configTime(0, 0, "pool.ntp.org");
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(2000);
    now = time(nullptr);
  }
  Serial.println(F("synchronized."));

  // Encodes authentication in base64 and message prefix in URL encoding
  strcat(twilioAuth, AccountSID);
  strcat(twilioAuth, ":");
  strcat(twilioAuth, AuthToken);
  base64::encode(String(twilioAuth)).toCharArray(encodedTwilioAuth, 128);
  encodeURL(messagePrefix, encodedMessagePrefix);

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
    // loop() more often, or increase dscBufferSize in the library: src/dscKeybus.h or src/dscClassic.h
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
          char messageContent[30];

          if (dsc.armedAway[partition] && dsc.noEntryDelay[partition]) strcpy(messageContent, "Armed night away: Partition ");
          else if (dsc.armedAway[partition]) strcpy(messageContent, "Armed away: Partition ");
          else if (dsc.armedStay[partition] && dsc.noEntryDelay[partition]) strcpy(messageContent, "Armed night stay: Partition ");
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
  encodeURL(messageContent, encodedMessageContent);  // Encodes message content in URL encoding

  if (!ipClient.connect("api.twilio.com", 443)) return false;
  ipClient.print(F("POST https://api.twilio.com/2010-04-01/Accounts/"));
  ipClient.print(AccountSID);
  ipClient.println(F("/Messages.json HTTP/1.1"));
  ipClient.print(F("Authorization: Basic "));
  ipClient.println(encodedTwilioAuth);
  ipClient.println(F("Host: api.twilio.com"));
  ipClient.println(F("User-Agent: ESP8266"));
  ipClient.println(F("Accept: */*"));
  ipClient.println(F("Content-Type: application/x-www-form-urlencoded"));
  ipClient.print(F("Content-Length: "));
  ipClient.println(strlen(To) + strlen(From) + strlen(encodedMessagePrefix) + strlen(encodedMessageContent) + 21);
  ipClient.println("Connection: Close");
  ipClient.println();
  ipClient.print(F("To=%2B"));
  ipClient.print(To);
  ipClient.print(F("&From=%2B"));
  ipClient.print(From);
  ipClient.print(F("&Body="));
  ipClient.print(encodedMessagePrefix);
  ipClient.print(encodedMessageContent);

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
    Serial.println();
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


// Helper for encodeURL()
static char encodeHex(char c) {
  return "0123456789ABCDEF"[c & 0x0F];
}


// Encodes a char array to URL encoded using '+' for spaces as required for application/x-www-form-urlencoded
char *encodeURL(const char *src, char *dst) {
   char c, *d = dst;
   while (c = *src++) {
     if (c == ' ') {
      *d++ = '+';
      continue;
     }
     else if (!('a' <= c && c <= 'z')
         && !('A' <= c && c <= 'Z')
         && !('0' <= c && c <= '9')) {
       *d++ = '%';
       *d++ = encodeHex(c >> 4);
       c = encodeHex(c);
     }
     *d++ = c;
   }
   *d = '\0';
   return dst;
}
