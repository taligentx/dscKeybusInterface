/*
 *  Telegram Bot 1.2 (esp32)
 *
 *  Processes the security system status and allows for control via a Telegram bot: https://www.telegram.org
 *
 *  Setup:
 *    1. Install the UniversalTelegramBot library from the Arduino IDE/PlatformIO library manager:
 *         https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
 *    2. Install the ArduinoJSON library from the Arduino IDE/PlatformIO library manager:
 *         https://github.com/bblanchon/ArduinoJson
 *    3. Set the WiFi SSID and password in the sketch.
 *    4. Set an access code in the sketch to manage the security system.
 *    5. Send a message to BotFather: https://t.me/botfather
 *    6. Create a new bot through BotFather: /newbot
 *    7. Copy the bot token to the sketch in telegramBotToken.
 *    8. Send a message to the newly created bot to start a conversation.
 *    9. Send a message to @myidbot: https://telegram.me/myidbot
 *   10. Get your user ID: /getid
 *   11. Copy the user ID to the sketch in telegramUserID.
 *   12. Upload the sketch.
 *
 *  Usage:
 *    - Set the partition number to manage: /X (where X = 1-8)
 *    - Arm stay: /armstay
 *    - Arm away: /armaway
 *    - Arm night (no entry delay): /armnight
 *    - Disarm: /disarm
 *
 *  Release notes:
 *    1.2 - Workaround for upstream esp32 TLS handshake issue https://github.com/espressif/arduino-esp32/issues/6165
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
 *      Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin  // Default: 21
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

// DSC Classic series: uncomment for PC1500/PC1550 support (requires PC16-OUT configuration per README.md)
//#define dscClassicSeries

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <dscKeybusInterface.h>

// Settings
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* accessCode = "";        // An access code is required to disarm/night arm and may be required to arm or enable command outputs based on panel configuration.
const char* telegramBotToken = "";  // Set the Telegram bot access token
const char* telegramUserID = "";    // Set the Telegram chat user ID
const char* messagePrefix = "[Security system] ";  // Set a prefix for all messages

// Configures the Keybus interface with the specified pins.
#define dscClockPin 18  // 4,13,16-39
#define dscReadPin  19  // 4,13,16-39
#define dscPC16Pin  17  // DSC Classic Series only, 4,13,16-39
#define dscWritePin 21  // 4,13,16-33

// Initialize components
#ifndef dscClassicSeries
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
#else
dscClassicInterface dsc(dscClockPin, dscReadPin, dscPC16Pin, dscWritePin, accessCode);
#endif
WiFiClientSecure ipClient;
UniversalTelegramBot telegramBot(telegramBotToken, ipClient);
const int telegramCheckInterval = 1000;
bool wifiConnected = true;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  Serial.print(F("WiFi...."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  ipClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);
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

  // Sends a message on startup to verify connectivity
  Serial.print(F("Telegram...."));
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

  // Checks for incoming Telegram messages
  static unsigned long telegramPreviousTime;
  if (millis() - telegramPreviousTime > telegramCheckInterval) {
    ipClient.setHandshakeTimeout(30);  // Workaround for https://github.com/espressif/arduino-esp32/issues/6165

    byte telegramMessages = telegramBot.getUpdates(telegramBot.last_message_received + 1);
    while (telegramMessages) {
      handleTelegram(telegramMessages);
      telegramMessages = telegramBot.getUpdates(telegramBot.last_message_received + 1);
    }
    telegramPreviousTime = millis();
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

    // Sends the access code when needed by the panel for arming or command outputs
    if (dsc.accessCodePrompt) {
      dsc.accessCodePrompt = false;
      dsc.write(accessCode);
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

      // Publishes exit delay status
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


void handleTelegram(byte telegramMessages) {
  static byte partition = 0;

  for (byte i = 0; i < telegramMessages; i++) {

    // Checks if a partition number 1-8 has been sent and sets the partition
    if (telegramBot.messages[i].text[1] >= 0x31 && telegramBot.messages[i].text[1] <= 0x38) {
      partition = telegramBot.messages[i].text[1] - 49;
      char messageContent[17] = "Set: Partition ";
      appendPartition(partition, messageContent);  // Appends the message with the partition number
      sendMessage(messageContent);
    }

    // Resets status if attempting to change the armed mode while armed or not ready
    if (telegramBot.messages[i].text != "/disarm" && !dsc.ready[partition]) {
      dsc.armedChanged[partition] = true;
      dsc.statusChanged = true;
      return;
    }

    // Arm stay
    if (telegramBot.messages[i].text == "/armstay" && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
      dsc.writePartition = partition + 1;  // Sets writes to the partition number
      dsc.write('s');
    }

    // Arm away
    else if (telegramBot.messages[i].text == "/armaway" && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
      dsc.writePartition = partition + 1;  // Sets writes to the partition number
      dsc.write('w');
    }

    // Arm night
    else if (telegramBot.messages[i].text == "/armnight" && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
      dsc.writePartition = partition + 1;  // Sets writes to the partition number
      dsc.write('n');
    }

    // Disarm
    else if (telegramBot.messages[i].text == "/disarm" && (dsc.armed[partition] || dsc.exitDelay[partition] || dsc.alarm[partition])) {
      dsc.writePartition = partition + 1;  // Sets writes to the partition number
      dsc.write(accessCode);
    }
  }
}


bool sendMessage(const char* messageContent) {
  ipClient.setHandshakeTimeout(30);  // Workaround for https://github.com/espressif/arduino-esp32/issues/6165
  byte messageLength = strlen(messagePrefix) + strlen(messageContent) + 1;
  char message[messageLength];
  strcpy(message, messagePrefix);
  strcat(message, messageContent);
  if (telegramBot.sendMessage(telegramUserID, message, "")) return true;
  else return false;
}


void appendPartition(byte sourceNumber, char* message) {
  char partitionNumber[2];
  itoa(sourceNumber + 1, partitionNumber, 10);
  strcat(message, partitionNumber);
}
