/*
 *  Homebridge-MQTT 1.7 (esp8266)
 *
 *  Processes the security system status and allows for control using Apple HomeKit, including the iOS Home app,
 *  Siri, and Google Home.  This uses MQTT to interface with Homebridge and the homebridge-mqttthing plugin for
 *  HomeKit integration and demonstrates using the armed and alarm states for the HomeKit securitySystem object,
 *  zone states for the contactSensor objects, and fire alarm states for the smokeSensor object.
 *
 *  Google Home integration via homebridge-gsh includes arming/disarming by voice with Google Assistant.  For
 *  disarming, set a voice PIN for 2 factor authentication as described in homebridge-gsh settings.
 *
 *  Homebridge: https://github.com/nfarina/homebridge
 *  homebridge-mqttthing: https://github.com/arachnetech/homebridge-mqttthing
 *  homebridge-gsh (Google Home): https://github.com/oznu/homebridge-gsh
 *  Mosquitto MQTT broker: https://mosquitto.org
 *
 *  Usage:
 *    1. Set the WiFi SSID and password in the sketch.
 *    2. Set the security system access code to permit disarming through HomeKit.
 *    3. Set the MQTT server address in the sketch.
 *    4. Copy the example configuration to Homebridge's config.json and customize.
 *    5. Upload the sketch.
 *    6. Restart Homebridge.
 *
 *  Example Homebridge config.json "accessories" configuration:

        {
            "accessory": "mqttthing",
            "type": "securitySystem",
            "name": "Security System Partition 1",
            "url": "http://127.0.0.1:1883",
            "topics":
            {
                "getCurrentState":    "dsc/Get/Partition1",
                "getTargetState":     "dsc/Get/Partition1",
                "setTargetState":     "dsc/Set"
            },
            "targetStateValues": ["1S", "1A", "1N", "1D"]
        },
        {
            "accessory": "mqttthing",
            "type": "securitySystem",
            "name": "Security System Partition 2",
            "url": "http://127.0.0.1:1883",
            "topics":
            {
                "getCurrentState":    "dsc/Get/Partition2",
                "getTargetState":     "dsc/Get/Partition2",
                "setTargetState":     "dsc/Set"
            },
            "targetStateValues": ["2S", "2A", "2N", "2D"]
        },
        {
            "accessory": "mqttthing",
            "type": "smokeSensor",
            "name": "Smoke Alarm Partition 1",
            "url": "http://127.0.0.1:1883",
            "topics":
            {
                "getSmokeDetected": "dsc/Get/Fire1"
            },
            "integerValue": "true"
        },
        {
            "accessory": "mqttthing",
            "type": "smokeSensor",
            "name": "Smoke Alarm Partition 2",
            "url": "http://127.0.0.1:1883",
            "topics":
            {
                "getSmokeDetected": "dsc/Get/Fire2"
            },
            "integerValue": "true"
        },
        {
            "accessory": "mqttthing",
            "type": "contactSensor",
            "name": "Zone 1",
            "url": "http://127.0.0.1:1883",
            "topics":
            {
                "getContactSensorState": "dsc/Get/Zone1"
            },
            "integerValue": "true"
        },
        {
            "accessory": "mqttthing",
            "type": "contactSensor",
            "name": "Zone 2",
            "url": "http://127.0.0.1:1883",
            "topics":
            {
                "getContactSensorState": "dsc/Get/Zone2"
            },
            "integerValue": "true"
        },
        {
            "accessory": "mqttthing",
            "type": "contactSensor",
            "name": "PGM 1",
            "url": "http://127.0.0.1:1883",
            "topics":
            {
                "getContactSensorState": "dsc/Get/PGM1"
            },
            "integerValue": "true"
        },
        {
            "accessory": "mqttthing",
            "type": "contactSensor",
            "name": "PGM 8",
            "url": "http://127.0.0.1:1883",
            "topics":
            {
                "getContactSensorState": "dsc/Get/PGM8"
            },
            "integerValue": "true"
        }

 *  The commands to set the alarm state are setup in Homebridge with the partition number (1-8) as a prefix to the command:
 *    Partition 1 stay arm: "1S"
 *    Partition 1 away arm: "1A"
 *    Partition 2 night arm (arm without an entry delay): "2N"
 *    Partition 2 disarm: "2D"
 *
 *  The interface listens for commands in the configured mqttSubscribeTopic, and publishes partition status in a
 *  separate topic per partition with the configured mqttPartitionTopic appended with the partition number:
 *    Stay arm: "SA"
 *    Away arm: "AA"
 *    Night arm: "NA"
 *    Disarm: "D"
 *    Alarm tripped: "T"
 *
 *  Zone states are published as an integer in a separate topic per zone with the configured mqttZoneTopic appended
 *  with the zone number:
 *    Open: "1"
 *    Closed: "0"
 *
 *  Fire states are published as an integer in a separate topic per partition with the configured mqttFireTopic
 *  appended with the partition number:
 *    Fire alarm: "1"
 *    Fire alarm restored: "0"
 *
 *  PGM outputs states are published as an integer in a separate topic per PGM with the configured mqttPgmTopic appended
 *  with the PGM output number:
 *    Open: "1"
 *    Closed: "0"
 *
 *  Release notes:
 *    1.7 - Fixed exit delay states while multiple partitions are arming
 *    1.6 - Added DSC Classic series support
 *    1.5 - Support switching armed modes while armed
 *    1.4 - Added PGM outputs 1-14 status
 *          Added notes on Google Home integration
 *    1.3 - Updated esp8266 wiring diagram for 33k/10k resistors
 *    1.2 - Resolved handling HomeKit target states
 *          Added status update on initial MQTT connection and reconnection
 *          Added publishState() to simplify sketch
 *          Removed writeReady check, moved into library
 *    1.1 - Add "getTargetState" to the Homebridge config.json example
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
 *      Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin  // Default: D8, GPIO 15
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

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <dscKeybusInterface.h>

// Settings
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* accessCode = "";    // An access code is required to disarm/night arm and may be required to arm or enable command outputs based on panel configuration.
const char* mqttServer = "";    // MQTT server domain name or IP address
const int   mqttPort = 1883;    // MQTT server port
const char* mqttUsername = "";  // Optional, leave blank if not required
const char* mqttPassword = "";  // Optional, leave blank if not required

// MQTT topics - match to Homebridge's config.json
const char* mqttClientName = "dscKeybusInterface";
const char* mqttPartitionTopic = "dsc/Get/Partition";  // Sends armed and alarm status per partition: dsc/Get/Partition1 ... dsc/Get/Partition8
const char* mqttZoneTopic = "dsc/Get/Zone";            // Sends zone status per zone: dsc/Get/Zone1 ... dsc/Get/Zone64
const char* mqttFireTopic = "dsc/Get/Fire";            // Sends fire status per partition: dsc/Get/Fire1 ... dsc/Get/Fire8
const char* mqttPgmTopic = "dsc/Get/PGM";              // Sends PGM status per PGM: dsc/Get/PGM1 ... dsc/Get/PGM14
const char* mqttSubscribeTopic = "dsc/Set";            // Receives messages to write to the panel

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin D1  // GPIO 5
#define dscReadPin  D2  // GPIO 4
#define dscPC16Pin  D7  // DSC Classic Series only, GPIO 13
#define dscWritePin D8  // GPIO 15

// Initialize components
#ifndef dscClassicSeries
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
#else
dscClassicInterface dsc(dscClockPin, dscReadPin, dscPC16Pin, dscWritePin, accessCode);
#endif
WiFiClient ipClient;
PubSubClient mqtt(mqttServer, mqttPort, ipClient);
unsigned long mqttPreviousTime;
char exitState[8];


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

  mqtt.setCallback(mqttCallback);
  if (mqttConnect()) mqttPreviousTime = millis();
  else mqttPreviousTime = 0;

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();
  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {
  mqttHandle();

  dsc.loop();

  if (dsc.statusChanged) {      // Checks if the security system status has changed
    dsc.statusChanged = false;  // Reset the status tracking flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // loop() more often, or increase dscBufferSize in the library: src/dscKeybus.h or src/dscClassic.h
    if (dsc.bufferOverflow) {
      Serial.println(F("Keybus buffer overflow"));
      dsc.bufferOverflow = false;
    }

    // Sends the access code when needed by the panel for arming or command outputs
    if (dsc.accessCodePrompt) {
      dsc.accessCodePrompt = false;
      dsc.write(accessCode);
    }

    // Publishes status per partition
    for (byte partition = 0; partition < dscPartitions; partition++) {

      // Skips processing if the partition is disabled or in installer programming
      if (dsc.disabled[partition]) continue;

      // Publishes armed/disarmed status
      if (dsc.armedChanged[partition]) {
        if (dsc.armed[partition]) {
          exitState[partition] = 0;

          // Night armed away
          if (dsc.armedAway[partition] && dsc.noEntryDelay[partition]) {
            publishState(mqttPartitionTopic, partition, "N", "NA");
          }

          // Armed away
          else if (dsc.armedAway[partition]) {
            publishState(mqttPartitionTopic, partition, "A", "AA");
          }

          // Night armed stay
          else if (dsc.armedStay[partition] && dsc.noEntryDelay[partition]) {
            publishState(mqttPartitionTopic, partition, "N", "NA");
          }

          // Armed stay
          else if (dsc.armedStay[partition]) {
            publishState(mqttPartitionTopic, partition, "S", "SA");
          }
        }

        // Disarmed
        else publishState(mqttPartitionTopic, partition, "D", "D");
      }

      // Checks exit delay status
      if (dsc.exitDelayChanged[partition]) {
        dsc.exitDelayChanged[partition] = false;  // Resets the exit delay status flag

        // Exit delay in progress
        if (dsc.exitDelay[partition]) {

          // Sets the arming target state if the panel is armed externally
          if (exitState[partition] == 0 || dsc.exitStateChanged[partition]) {
            dsc.exitStateChanged[partition] = 0;
            switch (dsc.exitState[partition]) {
              case DSC_EXIT_STAY: {
                exitState[partition] = 'S';
                publishState(mqttPartitionTopic, partition, "S", 0);
                break;
              }
              case DSC_EXIT_AWAY: {
                exitState[partition] = 'A';
                publishState(mqttPartitionTopic, partition, "A", 0);
                break;
              }
              case DSC_EXIT_NO_ENTRY_DELAY: {
                exitState[partition] = 'N';
                publishState(mqttPartitionTopic, partition, "N", 0);
                break;
              }
            }
          }
        }

        // Disarmed during exit delay
        else if (!dsc.armed[partition]) {
          exitState[partition] = 0;
          publishState(mqttPartitionTopic, partition, "D", "D");
        }
      }

      // Publishes alarm triggered status
      if (dsc.alarmChanged[partition]) {
        dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag
        if (dsc.alarm[partition]) {
          publishState(mqttPartitionTopic, partition, 0, "T");
        }
        else if (!dsc.armedChanged[partition]) publishState(mqttPartitionTopic, partition, "D", "D");
      }
      if (dsc.armedChanged[partition]) dsc.armedChanged[partition] = false;  // Resets the partition armed status flag

      // Publishes fire alarm status
      if (dsc.fireChanged[partition]) {
        dsc.fireChanged[partition] = false;  // Resets the fire status flag

        if (dsc.fire[partition]) {
          publishState(mqttFireTopic, partition, 0, "1");  // Fire alarm tripped
        }
        else {
          publishState(mqttFireTopic, partition, 0, "0");  // Fire alarm restored
        }
      }
    }

    // Publishes zones 1-64 status in a separate topic per zone
    // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones:
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

            // Appends the mqttZoneTopic with the zone number
            char zonePublishTopic[strlen(mqttZoneTopic) + 3];
            char zone[3];
            strcpy(zonePublishTopic, mqttZoneTopic);
            itoa(zoneBit + 1 + (zoneGroup * 8), zone, 10);
            strcat(zonePublishTopic, zone);

            if (bitRead(dsc.openZones[zoneGroup], zoneBit)) {
              mqtt.publish(zonePublishTopic, "1", true);            // Zone open
            }
            else mqtt.publish(zonePublishTopic, "0", true);         // Zone closed
          }
        }
      }
    }

    // Publishes PGM outputs 1-14 status in a separate topic per zone
    // PGM status is stored in the pgmOutputs[] and pgmOutputsChanged[] arrays using 1 bit per PGM output:
    //   pgmOutputs[0] and pgmOutputsChanged[0]: Bit 0 = PGM 1 ... Bit 7 = PGM 8
    //   pgmOutputs[1] and pgmOutputsChanged[1]: Bit 0 = PGM 9 ... Bit 5 = PGM 14
    if (dsc.pgmOutputsStatusChanged) {
      dsc.pgmOutputsStatusChanged = false;  // Resets the PGM outputs status flag
      for (byte pgmGroup = 0; pgmGroup < 2; pgmGroup++) {
        for (byte pgmBit = 0; pgmBit < 8; pgmBit++) {
          if (bitRead(dsc.pgmOutputsChanged[pgmGroup], pgmBit)) {  // Checks an individual PGM output status flag
            bitWrite(dsc.pgmOutputsChanged[pgmGroup], pgmBit, 0);  // Resets the individual PGM output status flag

            // Appends the mqttPgmTopic with the PGM number
            char pgmPublishTopic[strlen(mqttPgmTopic) + 3];
            char pgm[3];
            strcpy(pgmPublishTopic, mqttPgmTopic);
            itoa(pgmBit + 1 + (pgmGroup * 8), pgm, 10);
            strcat(pgmPublishTopic, pgm);

            if (bitRead(dsc.pgmOutputs[pgmGroup], pgmBit)) {
              mqtt.publish(pgmPublishTopic, "1", true);           // PGM enabled
            }
            else mqtt.publish(pgmPublishTopic, "0", true);        // PGM disabled
          }
        }
      }
    }

    mqtt.subscribe(mqttSubscribeTopic);
  }
}


// Handles messages received in the mqttSubscribeTopic
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  // Handles unused parameters
  (void)topic;
  (void)length;

  byte partition = 0;
  byte payloadIndex = 0;

  // Checks if a partition number 1-8 has been sent and sets the second character as the payload
  if (payload[0] >= 0x31 && payload[0] <= 0x38) {
    partition = payload[0] - 49;
    payloadIndex = 1;
  }

  // Sets night arm (no entry delay) while armed
  if (payload[payloadIndex] == 'N' && dsc.armed[partition]) {
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write('n');  // Keypad no entry delay
    publishState(mqttPartitionTopic, partition, "N", 0);
    exitState[partition] = 'N';
    return;
  }

  // Disables night arm while armed stay
  if (payload[payloadIndex] == 'S' && dsc.armedStay[partition] && dsc.noEntryDelay[partition]) {
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write('n');  // Keypad no entry delay
    publishState(mqttPartitionTopic, partition, "S", 0);
    exitState[partition] = 'S';
    return;
  }

  // Disables night arm while armed away
  if (payload[payloadIndex] == 'A' && dsc.armedAway[partition] && dsc.noEntryDelay[partition]) {
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write('n');  // Keypad no entry delay
    publishState(mqttPartitionTopic, partition, "A", 0);
    exitState[partition] = 'A';
    return;
  }

  // Changes from arm away to arm stay after the exit delay
  if (payload[payloadIndex] == 'S' && dsc.armedAway[partition]) {
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write("s");
    publishState(mqttPartitionTopic, partition, "S", 0);
    exitState[partition] = 'S';
    return;
  }

  // Changes from arm stay to arm away after the exit delay
  if (payload[payloadIndex] == 'A' && dsc.armedStay[partition]) {
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write("w");
    publishState(mqttPartitionTopic, partition, "A", 0);
    exitState[partition] = 'A';
    return;
  }

  // Resets the HomeKit target state if attempting to change the armed mode while not ready
  if (payload[payloadIndex] != 'D' && !dsc.ready[partition]) {
    dsc.armedChanged[partition] = true;
    dsc.statusChanged = true;
    return;
  }

  // Resets the HomeKit target state if attempting to change the arming mode during the exit delay
  if (payload[payloadIndex] != 'D' && dsc.exitDelay[partition] && exitState[partition] != 0) {
    if (exitState[partition] == 'S') publishState(mqttPartitionTopic, partition, "S", 0);
    else if (exitState[partition] == 'A') publishState(mqttPartitionTopic, partition, "A", 0);
    else if (exitState[partition] == 'N') publishState(mqttPartitionTopic, partition, "N", 0);
  }


  // homebridge-mqttthing STAY_ARM
  if (payload[payloadIndex] == 'S' && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write('s');  // Keypad stay arm
    publishState(mqttPartitionTopic, partition, "S", 0);
    exitState[partition] = 'S';
    return;
  }

  // homebridge-mqttthing AWAY_ARM
  if (payload[payloadIndex] == 'A' && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write('w');  // Keypad away arm
    publishState(mqttPartitionTopic, partition, "A", 0);
    exitState[partition] = 'A';
    return;
  }

  // homebridge-mqttthing NIGHT_ARM
  if (payload[payloadIndex] == 'N' && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write('n');  // Keypad arm with no entry delay
    publishState(mqttPartitionTopic, partition, "N", 0);
    exitState[partition] = 'N';
    return;
  }

  // homebridge-mqttthing DISARM
  if (payload[payloadIndex] == 'D' && (dsc.armed[partition] || dsc.exitDelay[partition] || dsc.alarm[partition])) {
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write(accessCode);
    return;
  }
}


void mqttHandle() {
  if (!mqtt.connected()) {
    unsigned long mqttCurrentTime = millis();
    if (mqttCurrentTime - mqttPreviousTime > 5000) {
      mqttPreviousTime = mqttCurrentTime;
      if (mqttConnect()) {
        Serial.println(F("MQTT disconnected, successfully reconnected."));
        mqttPreviousTime = 0;
      }
      else Serial.println(F("MQTT disconnected, failed to reconnect."));
    }
  }
  else mqtt.loop();
}


bool mqttConnect() {
  Serial.print(F("MQTT...."));
  if (mqtt.connect(mqttClientName, mqttUsername, mqttPassword)) {
    Serial.print(F("connected: "));
    Serial.println(mqttServer);
    dsc.resetStatus();  // Resets the state of all status components as changed to get the current status
  }
  else {
    Serial.print(F("connection error: "));
    Serial.println(mqttServer);
  }
  return mqtt.connected();
}


// Publishes HomeKit target and current states with partition numbers
void publishState(const char* sourceTopic, byte partition, const char* targetSuffix, const char* currentState) {
  char publishTopic[strlen(sourceTopic) + 2];
  char partitionNumber[2];

  // Appends the sourceTopic with the partition number
  itoa(partition + 1, partitionNumber, 10);
  strcpy(publishTopic, sourceTopic);
  strcat(publishTopic, partitionNumber);

  if (targetSuffix != 0) {

    // Prepends the targetSuffix with the partition number
    char targetState[strlen(targetSuffix) + 2];
    strcpy(targetState, partitionNumber);
    strcat(targetState, targetSuffix);

    // Publishes the target state
    mqtt.publish(publishTopic, targetState, true);
  }

  // Publishes the current state
  if (currentState != 0) {
    mqtt.publish(publishTopic, currentState, true);
  }
}
