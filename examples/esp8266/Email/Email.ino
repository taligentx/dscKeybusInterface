/*
 *  Email Notification 1.0 (esp8266)
 *
 *  Processes the security system status and demonstrates how to send an email when the status has changed.  Configure
 *  the email SMTP server settings in sendEmail().
 *
 *  Email is sent using SMTPS (port 465) with SSL for encryption - this is necessary on the ESP8266 until STARTTLS can
 *  be supported.  For example, this will work with Gmail after changing the account settings to allow less secure
 *  apps: https://support.google.com/accounts/answer/6010255
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

WiFiClientSecure smtpClient;

// Configures the Keybus interface with the specified pins.
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
dscKeybusInterface dsc(dscClockPin, dscReadPin);


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) delay(100);
  Serial.print(F("WiFi connected: "));
  Serial.println(WiFi.localIP());

  // Sends an email on startup to verify connectivity
  if (sendEmail("Security system initializing", "")) Serial.println(F("Initialization email sent successfully."));
  else Serial.println(F("Initialization email failed to send."));

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
      sendEmail("Security system fire alarm button pressed", "");
    }

    if (dsc.keypadAuxAlarm) {
      dsc.keypadAuxAlarm = false;  // Resets the keypad auxiliary alarm status flag
      sendEmail("Security system aux alarm button pressed", "");
    }

    if (dsc.keypadPanicAlarm) {
      dsc.keypadPanicAlarm = false;  // Resets the keypad panic alarm status flag
      sendEmail("Security system panic alarm button pressed", "");
    }

    if (dsc.powerChanged) {
      dsc.powerChanged = false;  // Resets the power trouble status flag
      if (dsc.powerTrouble) sendEmail("Security system AC power trouble", "");
      else sendEmail("Security system AC power restored", "");
    }

    // Checks status per partition
    for (byte partition = 0; partition < dscPartitions; partition++) {

      if (dsc.alarmChanged[partition]) {
        dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag

        char emailBody[12] = "Partition ";
        char partitionNumber[2];
        itoa(partition + 1, partitionNumber, 10);
        strcat(emailBody, partitionNumber);

        if (dsc.alarm[partition]) sendEmail("Security system in alarm", emailBody);
        else sendEmail("Security system disarmed after alarm", emailBody);
      }

      if (dsc.fireChanged[partition]) {
        dsc.fireChanged[partition] = false;  // Resets the fire status flag

        char emailBody[12] = "Partition ";
        char partitionNumber[2];
        itoa(partition + 1, partitionNumber, 10);
        strcat(emailBody, partitionNumber);

        if (dsc.fire[partition]) sendEmail("Security system fire alarm", emailBody);
        else sendEmail("Security system fire alarm restored", emailBody);
      }
    }
  }
}


// sendEmail() takes the email subject and body as separate parameters.  Configure the settings for your SMTP
// server - the login and password must be base64 encoded. For example, on the macOS/Linux terminal:
// $ echo -n 'mylogin@example.com' | base64
bool sendEmail(const char* emailSubject, const char* emailBody) {
  if (!smtpClient.connect("smtp.example.com", 465)) return false;       // Set the SMTP server address - for example: smtp.gmail.com
  if(!smtpValidResponse()) return false;
  smtpClient.println(F("HELO ESP8266"));
  if(!smtpValidResponse()) return false;
  smtpClient.println(F("AUTH LOGIN"));
  if(!smtpValidResponse()) return false;
  smtpClient.println(F("myBase64encodedLogin"));                        // Set the SMTP server login in base64
  if(!smtpValidResponse()) return false;
  smtpClient.println(F("myBase64encodedPassword"));                     // Set the SMTP server password in base64
  if(!smtpValidResponse()) return false;
  smtpClient.println(F("MAIL FROM:<sender@example.com>"));              // Set the sender address
  if(!smtpValidResponse()) return false;
  smtpClient.println(F("RCPT TO:<recipient@example.com>"));             // Set the recipient address - repeat to add multiple recipients
  if(!smtpValidResponse()) return false;
  smtpClient.println(F("RCPT TO:<recipient2@example.com>"));            // An optional additional recipient
  if(!smtpValidResponse()) return false;
  smtpClient.println(F("DATA"));
  if(!smtpValidResponse()) return false;
  smtpClient.println(F("From: Security System <sender@example.com>"));  // Set the sender displayed in the email header
  smtpClient.println(F("To: Recipient <recipient@example.com>"));       // Set the recipient displayed in the email header
  smtpClient.print(F("Subject: "));
  smtpClient.println(emailSubject);
  smtpClient.println();                                                 // Required blank line between the header and body
  smtpClient.println(emailBody);
  smtpClient.println(F("."));
  if(!smtpValidResponse()) return false;
  smtpClient.println(F("QUIT"));
  if(!smtpValidResponse()) return false;
  smtpClient.stop();
  return true;
}


bool smtpValidResponse() {

  // Waits for a response
  unsigned long previousMillis = millis();
  while (!smtpClient.available()) {
    dsc.handlePanel();  // Processes Keybus data while waiting on the SMTP response
    if (millis() - previousMillis > 3000) {
      Serial.println(F("Connection timed out waiting for a response."));
      smtpClient.stop();
      return false;
    }
    yield();
  }

  // Checks the first character of the SMTP reply code - the command was successful if the reply code begins
  // with "2" or "3"
  char replyCode = smtpClient.read();

  // Successful, reads the remainder of the response to clear the client buffer
  if (replyCode == '2' || replyCode == '3') {
    while (smtpClient.available()) smtpClient.read();
    return true;
  }

  // Unsuccessful, prints the response to serial to help debug
  else {
    Serial.println(F("Email send error, response:"));
    Serial.print(replyCode);
    while (smtpClient.available()) Serial.print((char)smtpClient.read());
    Serial.println();
    smtpClient.println(F("QUIT"));
    smtpValidResponse();
    smtpClient.stop();
    return false;
  }
}
