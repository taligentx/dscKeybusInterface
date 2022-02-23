/*
 *  Pushover Push Notification 1.0 (esp32)
 *
 *  Processes the security system status and demonstrates how to send a push notification when the status has changed.
 *  This example sends notifications via Pushover: https://www.pushover.net
 *
 *  Usage:
 *    1. Set the WiFi SSID and password in the sketch.
 *    2. Create a Pushover account: https://www.pushover.net
 *    3. Copy the user key to pushoverUserKey.
 *    4. Create a Pushover application to get an API token: https://pushover.net/apps/build
 *    5. Copy the API token to pushoverAPIToken.
 *    6. Upload the sketch.
 *
 *  Release notes:
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- 5v voltage regulator --- esp32 development board 5v pin
 *
 *      DSC Aux(-) --- esp32 Ground
 *
 *                                         +--- dscClockPin  // Default: 18
 *      DSC Yellow --- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin   // Default: 19
 *      DSC Green ---- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *      Classic series only, PGM configured for PC-16 output:
 *      DSC PGM ---+-- 1k ohm resistor --- DSC Aux(+)
 *                 |
 *                 |                       +--- dscPC16Pin   // Default: 17
 *                 +-- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  This example code is in the public domain.
 */

// DSC Classic series: uncomment for PC1500/PC1550 support (requires PC16-OUT configuration per README.md)
//#define dscClassicSeries

#include <WiFiClientSecure.h>
#include <dscKeybusInterface.h>

// Settings
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* pushoverUserKey = "";   // Set the user key generated in the Pushover account settings
const char* pushoverAPIToken = "";  // Set the API token generated in the Pushover account settings
const char* messagePrefix = "[Security system] ";  // Set a prefix for all messages

// Configures the Keybus interface with the specified pins.
#define dscClockPin 18  // 4,13,16-39
#define dscReadPin  19  // 4,13,16-39
#define dscPC16Pin  17  // DSC Classic Series only, 4,13,16-39

// HTTPS root certificate for api.pushover.net: DigiCert Global Root CA, expires 2031.11.10
const char pushoverCertificateRoot[] = R"=EOF=(
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
WiFiClientSecure ipClient;
bool wifiConnected = true;


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  Serial.print(F("WiFi...."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  ipClient.setCACert(pushoverCertificateRoot);
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

  // Sends a push notification on startup to verify connectivity
  Serial.print(F("Pushover...."));
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
          char pushMessage[22] = "Disarmed: Partition ";
          appendPartition(partition, pushMessage);  // Appends the push message with the partition number
          sendMessage(pushMessage);
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
              char pushMessage[15] = "Zone alarm: ";
              char zoneNumber[3];
              itoa((zoneBit + 1 + (zoneGroup * 8)), zoneNumber, 10); // Determines the zone number
              strcat(pushMessage, zoneNumber);
              sendMessage(pushMessage);
            }
            else {
              char pushMessage[24] = "Zone alarm restored: ";
              char zoneNumber[3];
              itoa((zoneBit + 1 + (zoneGroup * 8)), zoneNumber, 10); // Determines the zone number
              strcat(pushMessage, zoneNumber);
              sendMessage(pushMessage);
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
  ipClient.setHandshakeTimeout(30);  // Workaround for https://github.com/espressif/arduino-esp32/issues/6165

  if (!ipClient.connect("api.pushover.net", 443)) return false;
  ipClient.println(F("POST /1/messages.json HTTP/1.1"));
  ipClient.println(F("Host: api.pushover.net"));
  ipClient.println(F("User-Agent: ESP32"));
  ipClient.println(F("Accept: */*"));
  ipClient.println(F("Content-Type: application/json"));
  ipClient.print(F("Content-Length: "));
  ipClient.println(strlen(pushoverAPIToken) + strlen(pushoverUserKey) + strlen(messagePrefix) + strlen(messageContent) + 35);
  ipClient.println();
  ipClient.print(F("{\"token\":\""));
  ipClient.print(pushoverAPIToken);
  ipClient.print(F("\",\"user\":\""));
  ipClient.print(pushoverUserKey);
  ipClient.print(F("\",\"message\":\""));
  ipClient.print(messagePrefix);
  ipClient.print(messageContent);
  ipClient.print(F("\"}"));

  // Waits for a response
  unsigned long previousMillis = millis();
  while (!ipClient.available()) {
    dsc.loop();
    if (millis() - previousMillis > 3000) {
      Serial.println();
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
    Serial.println(F("Push notification error, response:"));
    Serial.print(statusCode);
    while (ipClient.available()) Serial.print((char)ipClient.read());
    Serial.println();
    ipClient.stop();
    return false;
  }
}


void appendPartition(byte sourceNumber, char* pushMessage) {
  char partitionNumber[2];
  itoa(sourceNumber + 1, partitionNumber, 10);
  strcat(pushMessage, partitionNumber);
}
