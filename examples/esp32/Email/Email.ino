/*
 *  Email Notification 1.1 (esp32)
 *
 *  Processes the security system status and demonstrates how to send an email when the status has changed.  Configure
 *  the email SMTP server settings in sendEmail().
 *
 *  Email is sent using SMTPS (port 465) with SSL for encryption - this is necessary on the ESP32 until STARTTLS can
 *  be supported.  For example, this will work with Gmail after changing the account settings to allow less secure
 *  apps: https://support.google.com/accounts/answer/6010255
 *
 *  Release notes:
 *    1.1 - Added DSC Classic series support
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
const char* messagePrefix = "[Security system] ";  // Set a prefix for all messages

// Configures the Keybus interface with the specified pins.
#define dscClockPin 18  // 4,13,16-39
#define dscReadPin  19  // 4,13,16-39
#define dscPC16Pin  17  // DSC Classic Series only, 4,13,16-39

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
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print(F("connected: "));
  Serial.println(WiFi.localIP());

  // Sends a message on startup to verify connectivity
  Serial.print(F("Email...."));
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

    // Checks status per partition
    for (byte partition = 0; partition < dscPartitions; partition++) {

      // Skips processing if the partition is disabled or in installer programming
      if (dsc.disabled[partition]) continue;

      // Checks alarm triggered status
      if (dsc.alarmChanged[partition]) {
        dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag

        if (dsc.alarm[partition]) {
          char messageContent[19] = "Alarm: Partition ";
          appendPartition(partition, messageContent);  // Appends the message with the partition number
          sendMessage(messageContent);
        }
        else {
          char messageContent[34] = "Disarmed after alarm: Partition ";
          appendPartition(partition, messageContent);  // Appends the message with the partition number
          sendMessage(messageContent);
        }
      }

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


// sendMessage() takes the email subject and body as separate parameters.  Configure the settings for your SMTP
// server - the login and password must be base64 encoded. For example, on the macOS/Linux terminal:
// $ echo -n 'mylogin@example.com' | base64 -w 0
bool sendMessage(const char* messageContent) {
  ipClient.setHandshakeTimeout(30);  // Workaround for https://github.com/espressif/arduino-esp32/issues/6165
  if (!ipClient.connect("smtp.example.com", 465)) return false;       // Set the SMTP server address - for example: smtp.gmail.com
  if(!smtpValidResponse()) return false;
  ipClient.println(F("HELO ESP32"));
  if(!smtpValidResponse()) return false;
  ipClient.println(F("AUTH LOGIN"));
  if(!smtpValidResponse()) return false;
  ipClient.println(F("myBase64encodedLogin"));                        // Set the SMTP server login in base64
  if(!smtpValidResponse()) return false;
  ipClient.println(F("myBase64encodedPassword"));                     // Set the SMTP server password in base64
  if(!smtpValidResponse()) return false;
  ipClient.println(F("MAIL FROM:<sender@example.com>"));              // Set the sender address
  if(!smtpValidResponse()) return false;
  ipClient.println(F("RCPT TO:<recipient@example.com>"));             // Set the recipient address - repeat to add multiple recipients
  if(!smtpValidResponse()) return false;
  ipClient.println(F("RCPT TO:<recipient2@example.com>"));            // An optional additional recipient
  if(!smtpValidResponse()) return false;
  ipClient.println(F("DATA"));
  if(!smtpValidResponse()) return false;
  ipClient.println(F("From: Security System <sender@example.com>"));  // Set the sender displayed in the email header
  ipClient.println(F("To: Recipient <recipient@example.com>"));       // Set the recipient displayed in the email header
  ipClient.print(F("Subject: "));
  ipClient.print(messagePrefix);
  ipClient.println(messageContent);
  ipClient.println();                                                 // Required blank line between the header and body
  ipClient.print(messagePrefix);
  ipClient.println(messageContent);
  ipClient.println(F("."));
  if(!smtpValidResponse()) return false;
  ipClient.println(F("QUIT"));
  if(!smtpValidResponse()) return false;
  ipClient.stop();
  return true;
}


bool smtpValidResponse() {

  // Waits for a response
  unsigned long previousMillis = millis();
  while (!ipClient.available()) {
    dsc.loop();  // Processes Keybus data while waiting on the SMTP response
    if (millis() - previousMillis > 3000) {
      Serial.println();
      Serial.println(F("Connection timed out waiting for a response."));
      ipClient.stop();
      return false;
    }
  }

  // Checks the first character of the SMTP reply code - the command was successful if the reply code begins
  // with "2" or "3"
  char replyCode = ipClient.read();

  // Successful, reads the remainder of the response to clear the client buffer
  if (replyCode == '2' || replyCode == '3') {
    while (ipClient.available()) ipClient.read();
    return true;
  }

  // Unsuccessful, prints the response to serial to help debug
  else {
    Serial.println();
    Serial.println(F("Email send error, response:"));
    Serial.print(replyCode);
    while (ipClient.available()) Serial.print((char)ipClient.read());
    Serial.println();
    ipClient.println(F("QUIT"));
    smtpValidResponse();
    ipClient.stop();
    return false;
  }
}


void appendPartition(byte sourceNumber, char* pushMessage) {
  char partitionNumber[2];
  itoa(sourceNumber + 1, partitionNumber, 10);
  strcat(pushMessage, partitionNumber);
}
