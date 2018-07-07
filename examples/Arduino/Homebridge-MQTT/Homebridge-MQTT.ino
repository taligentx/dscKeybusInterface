/*
 *  Homebridge-MQTT 1.0 (Arduino)
 *
 *  Processes the security system status and allows for control using Apple HomeKit, including the iOS Home app and
 *  Siri.  This uses MQTT to interface with Homebridge and the homebridge-mqttthing plugin for HomeKit integration
 *  and demonstrates using the armed and alarm states for the HomeKit securitySystem object, zone states
 *  for the contactSensor objects, and fire alarm states for the smokeSensor object.
 *
 *  Homebridge: https://github.com/nfarina/homebridge
 *  homebridge-mqttthing: https://github.com/arachnetech/homebridge-mqttthing
 *  Mosquitto MQTT broker: https://mosquitto.org
 *
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
 *  Example Homebridge config.json "accessories" configuration:

        {
            "accessory": "mqttthing",
            "type": "securitySystem",
            "name": "Security System Partition 1",
            "url": "http://127.0.0.1:1883",
            "topics":
            {
                "getCurrentState":    "dsc/Get/Partition1",
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
        }

 *  Wiring:
 *      DSC Aux(-) --- Arduino ground
 *
 *                                         +--- dscClockPin (Arduino Uno: 2,3)
 *      DSC Yellow --- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (Arduino Uno: 2-12)
 *      DSC Green ---- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (Arduino Uno: 2-12)
 *            Ground --- NPN emitter --/
 *
 *  Power (when disconnected from USB):
 *      DSC Aux(+) --- Arduino Vin pin
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

#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <dscKeybusInterface.h>

byte mac[] = { 0xAA, 0x61, 0x0A, 0x00, 0x00, 0x01 };  // Set a MAC address unique to the local network

const char* accessCode = "";  // An access code is required to disarm/night arm and may be required to arm based on panel configuration.

const char* mqttServer = "";    // MQTT server domain name or IP address
const int mqttPort = 1883;      // MQTT server port
const char* mqttUsername = "";  // Optional, leave blank if not required
const char* mqttPassword = "";  // Optional, leave blank if not required
const char* mqttClientName = "dscKeybusInterface";
const char* mqttPartitionTopic = "dsc/Get/Partition";  // Sends armed and alarm status per partition: dsc/Get/Partition1 ... dsc/Get/Partition8
const char* mqttZoneTopic = "dsc/Get/Zone";            // Sends zone status per zone: dsc/Get/Zone1 ... dsc/Get/Zone64
const char* mqttFireTopic = "dsc/Get/Fire";            // Sends fire status per partition: dsc/Get/Fire1 ... dsc/Get/Fire8
const char* mqttSubscribeTopic = "dsc/Set";            // Receives messages to write to the panel
unsigned long mqttPreviousTime;

EthernetClient ethClient;
PubSubClient mqtt(mqttServer, mqttPort, ethClient);

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin 3  // Arduino Uno hardware interrupt pin: 2,3
#define dscReadPin 5   // Arduino Uno: 2-12
#define dscWritePin 6  // Arduino Uno: 2-12
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // Initializes ethernet with DHCP
  Serial.print(F("Initializing Ethernet..."));
  while(!Ethernet.begin(mac)) {
      Serial.println(F("DHCP failed.  Retrying..."));
      delay(5000);
  }
  Serial.println(F("success!"));
  Serial.print(F("IP address: "));
  Serial.println(Ethernet.localIP());

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

  if (dsc.handlePanel() && dsc.statusChanged) {  // Processes data only when a valid Keybus command has been read
    dsc.statusChanged = false;                   // Reset the status tracking flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // handlePanel() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
    if (dsc.bufferOverflow) Serial.println(F("Keybus buffer overflow"));
    dsc.bufferOverflow = false;

    // Sends the access code when needed by the panel for arming
    if (dsc.accessCodePrompt && dsc.writeReady) {
      dsc.accessCodePrompt = false;
      dsc.write(accessCode);
    }

    // Publishes status per partition
    for (byte partition = 0; partition < dscPartitions; partition++) {

      // Publishes armed/disarmed status
      if (dsc.armedChanged[partition]) {
        dsc.armedChanged[partition] = false;  // Resets the partition armed status flag

        // Appends the mqttPartitionTopic with the partition number
        char publishTopic[strlen(mqttPartitionTopic) + 1];
        char partitionNumber[2];
        strcpy(publishTopic, mqttPartitionTopic);
        itoa(partition + 1, partitionNumber, 10);
        strcat(publishTopic, partitionNumber);

        if (dsc.armed[partition]) {
          if (dsc.armedAway[partition] && dsc.noEntryDelay[partition]) mqtt.publish(publishTopic, "NA", true);       // Night armed
          else if (dsc.armedAway[partition]) mqtt.publish(publishTopic, "AA", true);                                      // Away armed
          else if (dsc.armedStay[partition] && dsc.noEntryDelay[partition]) mqtt.publish(publishTopic, "NA", true);  // Night armed
          else if (dsc.armedStay[partition]) mqtt.publish(publishTopic, "SA", true);                                      // Stay armed
        }
        else mqtt.publish(publishTopic, "D", true);  // Disarmed
      }

      // Publishes alarm triggered status
      if (dsc.alarmChanged[partition]) {
        dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag
        if (dsc.alarm[partition]) {

          // Appends the mqttPartitionTopic with the partition number
          char publishTopic[strlen(mqttPartitionTopic) + 1];
          char partitionNumber[2];
          strcpy(publishTopic, mqttPartitionTopic);
          itoa(partition + 1, partitionNumber, 10);
          strcat(publishTopic, partitionNumber);

          mqtt.publish(publishTopic, "T", true);  // Alarm tripped
        }
      }

      // Publishes status when the system is disarmed during exit delay
      if (dsc.exitDelayChanged[partition] && !dsc.exitDelay[partition] && !dsc.armed[partition]) {

          // Appends the mqttPartitionTopic with the partition number
          char publishTopic[strlen(mqttPartitionTopic) + 1];
          char partitionNumber[2];
          strcpy(publishTopic, mqttPartitionTopic);
          itoa(partition + 1, partitionNumber, 10);
          strcat(publishTopic, partitionNumber);

          mqtt.publish(publishTopic, "D", true);  // Disarmed
      }

      // Publishes fire alarm status
      if (dsc.fireChanged[partition]) {
        dsc.fireChanged[partition] = false;  // Resets the fire status flag

        // Appends the mqttFireTopic with the partition number
        char firePublishTopic[strlen(mqttFireTopic) + 1];
        char partitionNumber[2];
        strcpy(firePublishTopic, mqttFireTopic);
        itoa(partition + 1, partitionNumber, 10);
        strcat(firePublishTopic, partitionNumber);

        if (dsc.fire[partition]) mqtt.publish(firePublishTopic, "1");  // Fire alarm tripped
        else mqtt.publish(firePublishTopic, "0");                           // Fire alarm restored
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
            char zonePublishTopic[strlen(mqttZoneTopic) + 2];
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

  // homebridge-mqttthing STAY_ARM
  if (payload[payloadIndex] == 'S' && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
    while (!dsc.writeReady) dsc.handlePanel();  // Continues processing Keybus data until ready to write
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write('s');  // Keypad stay arm
  }

  // homebridge-mqttthing AWAY_ARM
  else if (payload[payloadIndex] == 'A' && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
    while (!dsc.writeReady) dsc.handlePanel();  // Continues processing Keybus data until ready to write
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write('w');  // Keypad away arm
  }

  // homebridge-mqttthing NIGHT_ARM
  else if (payload[payloadIndex] == 'N' && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
    while (!dsc.writeReady) dsc.handlePanel();  // Continues processing Keybus data until ready to write
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write('n');  // Keypad arm with no entry delay
  }

  // homebridge-mqttthing DISARM
  else if (payload[payloadIndex] == 'D' && (dsc.armed[partition] || dsc.exitDelay[partition])) {
    while (!dsc.writeReady) dsc.handlePanel();  // Continues processing Keybus data until ready to write
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write(accessCode);
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
  if (mqtt.connect(mqttClientName, mqttUsername, mqttPassword)) {
    Serial.print(F("MQTT connected: "));
    Serial.println(mqttServer);
    mqtt.subscribe(mqttSubscribeTopic);
  }
  else {
    Serial.print(F("MQTT connection failed: "));
    Serial.println(mqttServer);
  }
  return mqtt.connected();
}

