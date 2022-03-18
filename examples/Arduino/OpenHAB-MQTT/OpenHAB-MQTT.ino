/*
 *  OpenHAB-MQTT 1.3 (Arduino with Ethernet)
 *
 *  Processes the security system status and allows for control using OpenHAB.  This uses MQTT to
 *  interface with OpenHAB and the MQTT binding and demonstrates using panel and partition states
 *  as OpenHAB switches, and zone states as OpenHAB contacts.  Also see https://github.com/jimtng/dscalarm-mqtt
 *  for an integration using the Homie convention for OpenHAB's Homie MQTT component.
 *
 *  OpenHAB: https://www.openhab.org
 *  OpenHAB MQTT Binding: https://www.openhab.org/addons/bindings/mqtt/
 *  Mosquitto MQTT broker: https://mosquitto.org
 *
 *  Usage:
 *    1. Set the WiFi SSID and password in the sketch.
 *    2. Set the security system access code to permit disarming through OpenHAB.
 *    3. Set the MQTT server address in the sketch.
 *    4. Upload the sketch.
 *    5. Install the OpenHAB MQTT binding.
 *    6. Copy the example configuration to OpenHAB and customize.
 *
 *  Example OpenHAB configuration:
 *
 *  1. Create a "things" file for the MQTT broker as (OpenHAB configuration directory)/things/mymqtt.things:
 *     - https://www.openhab.org/docs/configuration/things.html

Bridge mqtt:broker:mymqtt "My MQTT" [host="MQTT broker IP address or hostname"]

 *  2. Create a "things" file for the security system as (OpenHAB configuration directory)/things/dsc.things:

Thing mqtt:topic:mymqtt:dsc "DSC Security System" (mqtt:broker:mymqtt) @ "Home" {
    Channels:
        Type switch : partition1_armed_away "Partition 1 Armed Away" [stateTopic="dsc/Get/Partition1", commandTopic="dsc/Set", on="1A", off="1D"]
        Type switch : partition1_armed_stay "Partition 1 Armed Stay" [stateTopic="dsc/Get/Partition1", commandTopic="dsc/Set", on="1S", off="1D"]
        Type switch : partition1_alarm "Partition 1 Alarm" [stateTopic="dsc/Get/Partition1", on="1T", off="1D"]
        Type switch : partition1_fire "Partition 1 Fire" [stateTopic="dsc/Get/Fire1", on="1", off="0"]
        Type switch : panel_online "Panel Online" [stateTopic="dsc/Status", on="online", off="offline"]
        Type switch : panel_trouble "Panel Trouble" [stateTopic="dsc/Get/Trouble", on="1", off="0"]
        Type switch : pgm1 "PGM 1" [stateTopic="dsc/Get/PGM 1", on="1", off="0"]
        Type switch : pgm8 "PGM 8" [stateTopic="dsc/Get/PGM 8", on="1", off="0"]
        Type contact : zone1 "Zone 1" [stateTopic="dsc/Get/Zone1", on="1", off="0"]
        Type contact : zone2 "Zone 2" [stateTopic="dsc/Get/Zone2", on="1", off="0"]
        Type contact : zone3 "Zone 3" [stateTopic="dsc/Get/Zone3", on="1", off="0"]
}

*   3. Create an "items" file for the security system as (OpenHAB configuration directory)/items/dsc.items:
*      - https://www.openhab.org/docs/configuration/items.html

Switch partition1_armed_away "Partition 1 Armed Away" <shield> {channel="mqtt:topic:mymqtt:dsc:partition1_armed_away"}
Switch partition1_armed_stay  "Partition 1 Armed Stay" <shield> {channel="mqtt:topic:mymqtt:dsc:partition1_armed_stay"}
Switch partition1_triggered "Partition 1 Alarm" <alarm> {channel="mqtt:topic:mymqtt:dsc:partition1_alarm"}
Switch partition1_fire "Partition 1 Fire" <fire> {channel="mqtt:topic:mymqtt:dsc:partition1_fire"}
Switch panel_online "Panel Online" <switch> {channel="mqtt:topic:mymqtt:dsc:panel_online"}
Switch panel_trouble "Panel Trouble" <error> {channel="mqtt:topic:mymqtt:dsc:panel_trouble"}
Switch pgm1 "PGM 1" <switch> {channel="mqtt:topic:mymqtt:dsc:pgm1"}
Switch pgm8 "PGM 8" <switch> {channel="mqtt:topic:mymqtt:dsc:pgm8"}
Contact zone1 "Zone 1" <door> {channel="mqtt:topic:mymqtt:dsc:zone1"}
Contact zone2 "Zone 2" <window> {channel="mqtt:topic:mymqtt:dsc:zone2"}
Contact zone3 "Zone 3" <motion> {channel="mqtt:topic:mymqtt:dsc:zone3"}


 *  The commands to set the alarm state are setup in OpenHAB with the partition number (1-8) as a prefix to the command:
 *    Partition 1 stay arm: "1S"
 *    Partition 1 away arm: "1A"
 *    Partition 2 disarm: "2D"
 *
 *  The interface listens for commands in the configured mqttSubscribeTopic, and publishes partition status in a
 *  separate topic per partition with the configured mqttPartitionTopic appended with the partition number:
 *    Partition 1 stay arm: "1S"
 *    Partition 1 away arm: "1A"
 *    Partition 1 disarm: "1D"
 *    Partition 2 alarm tripped: "2T"
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
 *  Release notes:
 *    1.3 - Added DSC Classic series support
 *    1.2 - Added PGM outputs 1-14 status
 *          Removed partition exit delay MQTT message, not used in this OpenHAB example
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
 *
 *      DSC Aux(-) --- esp8266 Ground
 *
 *                                         +--- dscClockPin (esp8266: D1, D2, D8)
 *      DSC Yellow --- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (esp8266: D1, D2, D8)
 *      DSC Green ---- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *      Classic series only, PGM configured for PC-16 output:
 *      DSC PGM ---+-- 1k ohm resistor --- DSC Aux(+)
 *                 |
 *                 |                       +--- dscPC16Pin (Arduino Uno: 2-12)
 *                 +-- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *      Virtual keypad (optional):
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

// DSC Classic series: uncomment for PC1500/PC1550 support (requires PC16-OUT configuration per README.md)
//#define dscClassicSeries

#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <dscKeybusInterface.h>

// Settings
byte mac[] = { 0xAA, 0x61, 0x0A, 0x00, 0x00, 0x01 };  // Set a MAC address unique to the local network
const char* accessCode = "";    // An access code is required to disarm/night arm and may be required to arm or enable command outputs based on panel configuration.
const char* mqttServer = "";    // MQTT server domain name or IP address
const int   mqttPort = 1883;    // MQTT server port
const char* mqttUsername = "";  // Optional, leave blank if not required
const char* mqttPassword = "";  // Optional, leave blank if not required

// MQTT topics - match to the OpenHAB "things" configuration file
const char* mqttClientName = "dscKeybusInterface";
const char* mqttPartitionTopic = "dsc/Get/Partition";  // Sends armed and alarm status per partition: dsc/Get/Partition1 ... dsc/Get/Partition4
const char* mqttZoneTopic = "dsc/Get/Zone";            // Sends zone status per zone: dsc/Get/Zone1 ... dsc/Get/Zone32
const char* mqttFireTopic = "dsc/Get/Fire";            // Sends fire status per partition: dsc/Get/Fire1 ... dsc/Get/Fire4
const char* mqttPgmTopic = "dsc/Get/PGM";              // Sends PGM status per PGM: dsc/Get/PGM1 ... dsc/Get/PGM14
const char* mqttTroubleTopic = "dsc/Get/Trouble";      // Sends trouble status
const char* mqttStatusTopic = "dsc/Status";            // Sends online/offline status
const char* mqttBirthMessage = "online";
const char* mqttLwtMessage = "offline";
const char* mqttSubscribeTopic = "dsc/Set";            // Receives messages to write to the panel

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin 3  // Arduino Uno hardware interrupt pin: 2,3
#define dscPC16Pin  4  // DSC Classic Series only, Arduino Uno: 2-12
#define dscReadPin  5  // Arduino Uno: 2-12
#define dscWritePin 6  // Arduino Uno: 2-12

// Initialize components
#ifndef dscClassicSeries
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
#else
dscClassicInterface dsc(dscClockPin, dscReadPin, dscPC16Pin, dscWritePin, accessCode);
#endif
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
    // loop() more often, or increase dscBufferSize in the library: src/dscKeybus.h or src/dscClassic.h
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

    // Sends the access code when needed by the panel for arming or command outputs
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
        if (dsc.armed[partition]) {

          // Armed away
          if (dsc.armedAway[partition]) {
            publishState(mqttPartitionTopic, partition, "A");
          }

          // Armed stay
          else if (dsc.armedStay[partition]) {
            publishState(mqttPartitionTopic, partition, "S");
          }
        }

        // Disarmed
        else publishState(mqttPartitionTopic, partition, "D");
      }

      // Checks exit delay status
      if (dsc.exitDelayChanged[partition]) {
        dsc.exitDelayChanged[partition] = false;  // Resets the exit delay status flag

        // Disarmed during exit delay
        if (!dsc.armed[partition]) {
          publishState(mqttPartitionTopic, partition, "D");
        }
      }

      // Publishes alarm triggered status
      if (dsc.alarmChanged[partition]) {
        dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag
        if (dsc.alarm[partition]) {
          publishState(mqttPartitionTopic, partition, "T");
        }
        else if (!dsc.armedChanged[partition]) publishState(mqttPartitionTopic, partition, "D");
      }
      if (dsc.armedChanged[partition]) dsc.armedChanged[partition] = false;  // Resets the partition armed status flag

      // Publishes fire alarm status
      if (dsc.fireChanged[partition]) {
        dsc.fireChanged[partition] = false;  // Resets the fire status flag

        if (dsc.fire[partition]) {
          publishState(mqttFireTopic, partition, "1");  // Fire alarm tripped
        }
        else {
          publishState(mqttFireTopic, partition, "0");  // Fire alarm restored
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

  // Resets status if attempting to change the armed mode while armed or not ready
  if (payload[payloadIndex] != 'D' && !dsc.ready[partition]) {
    dsc.armedChanged[partition] = true;
    dsc.statusChanged = true;
    return;
  }

  // Arm stay
  if (payload[payloadIndex] == 'S' && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write('s');  // Keypad arm stay
    return;
  }

  // Arm away
  if (payload[payloadIndex] == 'A' && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
    dsc.writePartition = partition + 1;    // Sets writes to the partition number
    dsc.write('w');  // Keypad arm away
    return;
  }

  // Disarm
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
        if (dsc.keybusConnected) mqtt.publish(mqttStatusTopic, mqttBirthMessage, true);
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


// Publishes the current states with partition numbers
void publishState(const char* sourceTopic, byte partition, const char* sourceSuffix) {
  char publishTopic[strlen(sourceTopic) + 2];
  char partitionNumber[2];

  // Appends the sourceTopic with the partition number
  itoa(partition + 1, partitionNumber, 10);
  strcpy(publishTopic, sourceTopic);
  strcat(publishTopic, partitionNumber);

  // Prepends the sourceSuffix with the partition number
  char currentState[strlen(sourceSuffix) + 2];
  strcpy(currentState, partitionNumber);
  strcat(currentState, sourceSuffix);

  // Publishes the current state
  mqtt.publish(publishTopic, currentState, true);
}
