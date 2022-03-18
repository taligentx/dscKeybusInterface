/*
 *  TinyGSM SMS Notification 1.1 (esp32)
 *
 *  Processes the security system status and demonstrates how to send an SMS text message when the status has
 *  changed.  This example sends SMS text messages via a TinyGSM-compatible module which can be integrated
 *  into a board (eg. TTGO T-Call) or connected separately (eg. SIM800 module).
 *
 *  Usage:
 *    1. Install the TinyGSM library, available in the Arduino IDE Library Manager and the Platform.io Library
 *       Registry: https://github.com/vshymanskyy/TinyGSM
 *    2. Set the destination phone numbers in the sketch settings.
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
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  Thanks to jvitkauskas for contributing this example: https://github.com/jvitkauskas
 *
 *  This example code is in the public domain.
 */

// DSC Classic series: uncomment for PC1500/PC1550 support (requires PC16-OUT configuration per README.md)
//#define dscClassicSeries

// Configures GSM modem model. Must be done before including TinyGsmClient library.
#define TINY_GSM_MODEM_SIM800

#include <TinyGsmClient.h>
#include <dscKeybusInterface.h>

// Settings
const char* sendToPhoneNumbers[] = {
  "+1234567890",
  "+2345678901"
};

#define phone_number_count (sizeof (sendToPhoneNumbers) / sizeof (const char *))

// Configures the GSM modem interface with the specified pins (eg. TTGO T-Call 1.3 v20190601).
#define MODEM_RST       5
#define MODEM_PWRKEY    4
#define MODEM_POWER_ON 23
#define MODEM_TX       27
#define MODEM_RX       26

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
TinyGsm modem(Serial1);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);

  // Turn on the Modem power first
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Pull down PWRKEY for more than 1 second according to manual requirements
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(100);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1000);
  digitalWrite(MODEM_PWRKEY, HIGH);

  Serial1.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

  while (!modem.isNetworkConnected()) {
    Serial.print(F("GSM...."));
    while (!modem.restart()) {
      Serial.print(".");
    }
    Serial.println();

    Serial.print(F("Waiting for network...."));
    if (modem.waitForNetwork(600000L) && modem.isNetworkConnected()) {
      Serial.println(F("connected."));
    }
    else {
      Serial.println(F("connection error."));
    }
  }

  // Starts the Keybus interface
  dsc.begin();
  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {
  modem.maintain();

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

bool sendMessage(const char* messageContent) {
  bool result = true;

  for (int i = 0; i < phone_number_count; i++) {
     result &= modem.sendSMS(sendToPhoneNumbers[i], messageContent);
  }

  return result;
}

void appendPartition(byte sourceNumber, char* pushMessage) {
  char partitionNumber[2];
  itoa(sourceNumber + 1, partitionNumber, 10);
  strcat(pushMessage, partitionNumber);
}
