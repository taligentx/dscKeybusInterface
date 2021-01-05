/*
 *  HomeAssistant-MQTT 1.4 (Arduino with Ethernet)
 *
 *  Processes the security system status and allows for control using Home Assistant via MQTT.
 *
 *  Home Assistant: https://www.home-assistant.io
 *  Mosquitto MQTT broker: https://mosquitto.org
 *
 *  Usage:
 *    1. Set the security system access code to permit disarming through Home Assistant.
 *    2. Set the MQTT server address in the sketch.
 *    3. Copy the example configuration to Home Assistant's configuration.yaml and customize.
 *    4. Upload the sketch.
 *    5. Restart Home Assistant.
 *
 *  Example Home Assistant configuration.yaml for 2 partitions, 3 zones:

# https://www.home-assistant.io/components/mqtt/
mqtt:
  broker: URL or IP address
  client_id: homeAssistant

# https://www.home-assistant.io/components/alarm_control_panel.mqtt/
alarm_control_panel:
  - platform: mqtt
    name: "Security Partition 1"
    state_topic: "dsc/Get/Partition1"
    availability_topic: "dsc/Status"
    command_topic: "dsc/Set"
    payload_disarm: "1D"
    payload_arm_home: "1S"
    payload_arm_away: "1A"
    payload_arm_night: "1N"
  - platform: mqtt
    name: "Security Partition 2"
    state_topic: "dsc/Get/Partition2"
    availability_topic: "dsc/Status"
    command_topic: "dsc/Set"
    payload_disarm: "2D"
    payload_arm_home: "2S"
    payload_arm_away: "2A"
    payload_arm_night: "2N"

# https://www.home-assistant.io/components/sensor.mqtt/
sensor:
  - platform: mqtt
    name: "Security Partition 1"
    state_topic: "dsc/Get/Partition1/Message"
    availability_topic: "dsc/Status"
    icon: "mdi:shield"
  - platform: mqtt
    name: "Security Partition 2"
    state_topic: "dsc/Get/Partition2/Message"
    availability_topic: "dsc/Status"
    icon: "mdi:shield"

# https://www.home-assistant.io/components/binary_sensor.mqtt/
binary_sensor:
  - platform: mqtt
    name: "Security Trouble"
    state_topic: "dsc/Get/Trouble"
    device_class: "problem"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "Smoke Alarm 1"
    state_topic: "dsc/Get/Fire1"
    device_class: "smoke"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "Smoke Alarm 2"
    state_topic: "dsc/Get/Fire2"
    device_class: "smoke"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "Zone 1"
    state_topic: "dsc/Get/Zone1"
    device_class: "door"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "Zone 2"
    state_topic: "dsc/Get/Zone2"
    device_class: "window"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "Zone 3"
    state_topic: "dsc/Get/Zone3"
    device_class: "motion"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "PGM 1"
    state_topic: "dsc/Get/PGM1"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "PGM 8"
    state_topic: "dsc/Get/PGM8"
    payload_on: "1"
    payload_off: "0"

 *  Example button card configuration to add a panic button: https://www.home-assistant.io/lovelace/button/

type: entity-button
name: Panic alarm
tap_action:
  action: call-service
  service: mqtt.publish
  service_data:
    payload: P
    topic: dsc/Set
hold_action:
  action: none
show_icon: true
show_name: true
entity: alarm_control_panel.security_partition_1

 *  The commands to set the alarm state are setup in Home Assistant with the partition number (1-8) as a
 *  prefix to the command, except to trigger the panic alarm:
 *    Partition 1 disarm: "1D"
 *    Partition 2 arm stay: "2S"
 *    Partition 2 arm away: "2A"
 *    Partition 1 arm night: "1N"
 *    Panic alarm: "P"
 *
 *  The interface listens for commands in the configured mqttSubscribeTopic, and publishes partition status in a
 *  separate topic per partition with the configured mqttPartitionTopic appended with the partition number:
 *    Disarmed: "disarmed"
 *    Arm stay: "armed_home"
 *    Arm away: "armed_away"
 *    Arm night: "armed_night"
 *    Exit delay in progress: "pending"
 *    Alarm tripped: "triggered"
 *
 *  The trouble state is published as an integer in the configured mqttTroubleTopic:
 *    Trouble: "1"
 *    Trouble restored: "0"
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
 *  PGM outputs states are published as an integer in a separate topic per PGM with the configured mqttPgmTopic
 *  appended with the PGM output number:
 *    Open: "1"
 *    Closed: "0"
 *
 *  Release notes
 *    1.4 - Added PGM outputs 1-14 status
 *    1.2 - Added night arm (arming with no entry delay)
 *          Added status update on initial MQTT connection and reconnection
 *          Add appendPartition() to simplify sketch
 *          Removed writeReady check, moved into library
 *    1.1 - Added status update on initial MQTT connection and reconnection
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- Arduino Vin pin
 *
 *      DSC Aux(-) --- Arduino Ground
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

// Settings
byte mac[] = { 0xAA, 0x61, 0x0A, 0x00, 0x00, 0x01 };  // Set a MAC address unique to the local network
const char* accessCode = "";    // An access code is required to disarm/night arm and may be required to arm based on panel configuration.
const char* mqttServer = "";    // MQTT server domain name or IP address
const int   mqttPort = 1883;    // MQTT server port
const char* mqttUsername = "";  // Optional, leave blank if not required
const char* mqttPassword = "";  // Optional, leave blank if not required

// MQTT topics - match to Home Assistant's configuration.yaml
const char* mqttClientName = "dscKeybusInterface";
const char* mqttPartitionTopic = "dsc/Get/Partition";  // Sends armed and alarm status per partition: dsc/Get/Partition1 ... dsc/Get/Partition8
const char* mqttZoneTopic = "dsc/Get/Zone";            // Sends zone status per zone: dsc/Get/Zone1 ... dsc/Get/Zone64
const char* mqttFireTopic = "dsc/Get/Fire";            // Sends fire status per partition: dsc/Get/Fire1 ... dsc/Get/Fire8
const char* mqttPgmTopic = "dsc/Get/PGM";              // Sends PGM status per PGM: dsc/Get/PGM1 ... dsc/Get/PGM14
const char* mqttTroubleTopic = "dsc/Get/Trouble";      // Sends trouble status
const char* mqttStatusTopic = "dsc/Status";            // Sends online/offline status
const char* mqttBirthMessage = "online";
const char* mqttLwtMessage = "offline";
const char* mqttSubscribeTopic = "dsc/Set";            // Receives messages to write to the panel

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin 3  // Arduino Uno hardware interrupt pin: 2,3
#define dscReadPin  5  // Arduino Uno: 2-12
#define dscWritePin 6  // Arduino Uno: 2-12

// Initialize components
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
EthernetClient ipClient;
PubSubClient mqtt(mqttServer, mqttPort, ipClient);
unsigned long mqttPreviousTime;


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  // Initializes ethernet with DHCP
  Serial.print(F("Ethernet...."));
  while(!Ethernet.begin(mac)) {
      Serial.println(F("DHCP failed.  Retrying..."));
      delay(5000);
  }
  Serial.print(F("connected: "));
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
      if (dsc.keybusConnected) mqtt.publish(mqttStatusTopic, mqttBirthMessage, true);
      else mqtt.publish(mqttStatusTopic, mqttLwtMessage, true);
    }

    // Sends the access code when needed by the panel for arming
    if (dsc.accessCodePrompt) {
      dsc.accessCodePrompt = false;
      dsc.write(accessCode);
    }

    if (dsc.troubleChanged) {
      dsc.troubleChanged = false;  // Resets the trouble status flag
      if (dsc.trouble) mqtt.publish(mqttTroubleTopic, "1", true);
      else mqtt.publish(mqttTroubleTopic, "0", true);
    }

    // Publishes status per partition
    for (byte partition = 0; partition < dscPartitions; partition++) {

      // Skips processing if the partition is disabled or in installer programming
      if (dsc.disabled[partition]) continue;

      // Publishes armed/disarmed status
      if (dsc.armedChanged[partition]) {
        char publishTopic[strlen(mqttPartitionTopic) + 2];
        appendPartition(mqttPartitionTopic, partition, publishTopic);  // Appends the mqttPartitionTopic with the partition number

        if (dsc.armed[partition]) {
          if (dsc.armedAway[partition] && dsc.noEntryDelay[partition]) mqtt.publish(publishTopic, "armed_night", true);
          else if (dsc.armedAway[partition]) mqtt.publish(publishTopic, "armed_away", true);
          else if (dsc.armedStay[partition] && dsc.noEntryDelay[partition]) mqtt.publish(publishTopic, "armed_night", true);
          else if (dsc.armedStay[partition]) mqtt.publish(publishTopic, "armed_home", true);
        }
        else mqtt.publish(publishTopic, "disarmed", true);
      }

      // Publishes exit delay status
      if (dsc.exitDelayChanged[partition]) {
        dsc.exitDelayChanged[partition] = false;  // Resets the exit delay status flag
        char publishTopic[strlen(mqttPartitionTopic) + 2];
        appendPartition(mqttPartitionTopic, partition, publishTopic);  // Appends the mqttPartitionTopic with the partition number

        if (dsc.exitDelay[partition]) mqtt.publish(publishTopic, "pending", true);  // Publish as a retained message
        else if (!dsc.exitDelay[partition] && !dsc.armed[partition]) mqtt.publish(publishTopic, "disarmed", true);
      }

      // Publishes alarm status
      if (dsc.alarmChanged[partition]) {
        dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag
        char publishTopic[strlen(mqttPartitionTopic) + 2];
        appendPartition(mqttPartitionTopic, partition, publishTopic);  // Appends the mqttPartitionTopic with the partition number

        if (dsc.alarm[partition]) mqtt.publish(publishTopic, "triggered", true);  // Alarm tripped
        else if (!dsc.armedChanged[partition]) mqtt.publish(publishTopic, "disarmed", true);
      }
      if (dsc.armedChanged[partition]) dsc.armedChanged[partition] = false;  // Resets the partition armed status flag

      // Publishes fire alarm status
      if (dsc.fireChanged[partition]) {
        dsc.fireChanged[partition] = false;  // Resets the fire status flag
        char publishTopic[strlen(mqttFireTopic) + 2];
        appendPartition(mqttFireTopic, partition, publishTopic);  // Appends the mqttFireTopic with the partition number

        if (dsc.fire[partition]) mqtt.publish(publishTopic, "1");  // Fire alarm tripped
        else mqtt.publish(publishTopic, "0");                      // Fire alarm restored
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

  // Panic alarm
  if (payload[payloadIndex] == 'P') {
    dsc.write('p');
  }

  // Resets status if attempting to change the armed mode while armed or not ready
  if (payload[payloadIndex] != 'D' && !dsc.ready[partition]) {
    dsc.armedChanged[partition] = true;
    dsc.statusChanged = true;
    return;
  }

  // Arm stay
  if (payload[payloadIndex] == 'S' && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
    dsc.writePartition = partition + 1;         // Sets writes to the partition number
    dsc.write('s');                             // Virtual keypad arm stay
  }

  // Arm away
  else if (payload[payloadIndex] == 'A' && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
    dsc.writePartition = partition + 1;         // Sets writes to the partition number
    dsc.write('w');                             // Virtual keypad arm away
  }

  // Arm night
  else if (payload[payloadIndex] == 'N' && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
    dsc.writePartition = partition + 1;         // Sets writes to the partition number
    dsc.write('n');                             // Virtual keypad arm away
  }

  // Disarm
  else if (payload[payloadIndex] == 'D' && (dsc.armed[partition] || dsc.exitDelay[partition] || dsc.alarm[partition])) {
    dsc.writePartition = partition + 1;         // Sets writes to the partition number
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
  Serial.print(F("MQTT...."));
  if (mqtt.connect(mqttClientName, mqttUsername, mqttPassword, mqttStatusTopic, 0, true, mqttLwtMessage)) {
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


void appendPartition(const char* sourceTopic, byte sourceNumber, char* publishTopic) {
  char partitionNumber[2];
  strcpy(publishTopic, sourceTopic);
  itoa(sourceNumber + 1, partitionNumber, 10);
  strcat(publishTopic, partitionNumber);
}
