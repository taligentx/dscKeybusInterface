/*
 *  DSC Status with MQTT (Arduino, esp8266)
 *
 *  Processes the security system status and allows for control using Home Assistant via MQTT.
 *
 *  Home Assistant: https://www.home-assistant.io
 *  Mosquitto MQTT broker: https://mosquitto.org
 *
 *  In this example, the commands to set the alarm state are setup in Home Assistant as:
 *    Disarm: "D"
 *    Arm stay: "S"
 *    Arm away: "A"
 *
 *  The interface listens for commands in the configured mqttSubscibeTopic, and publishes alarm states to the
 *  configured mqttPublishTopic:
 *    Disarmed: "disarmed"
 *    Arm stay: "armed_home"
 *    Arm away: "armed_away"
 *    Exit delay in progress: "pending"
 *    Alarm tripped: "triggered"
 *
 *  Zone states are published in a separate topic per zone with the configured mqttZoneTopic appended with the zone
 *  number.  The zone state is published as an integer:
 *    Closed: "0"
 *    Open: "1"
 *
 *  Example Home Assistant configuration.yaml:

      # https://www.home-assistant.io/components/mqtt/
      mqtt:
        broker: URL or IP address
        client_id: homeAssistant

      # https://www.home-assistant.io/components/alarm_control_panel.mqtt/
      alarm_control_panel:
        - platform: mqtt
          name: "Security System"
          state_topic: "dsc/Get"
          command_topic: "dsc/Set"
          payload_disarm: "D"
          payload_arm_home: "S"
          payload_arm_away: "A"

      # https://www.home-assistant.io/components/binary_sensor/
      binary_sensor:
        - platform: mqtt
          name: "Zone 1"
          state_topic: "dsc/Get/Zone1"
          device_class: "door"
          payload_on: "1"
          payload_off: "0"
        - platform: mqtt
          name: "Zone 8"
          state_topic: "dsc/Get/Zone8"
          device_class: "window"
          payload_on: "1"
          payload_off: "0"
        - platform: mqtt
          name: "Smoke Alarm"
          state_topic: "dsc/Get/Fire"
          device_class: "smoke"
          payload_on: "1"
          payload_off: "0"

 *  Wiring:
 *      DSC Aux(-) --- Arduino/esp8266 ground
 *
 *                                         +--- dscClockPin (Arduino Uno: 2,3 / esp8266: D1,D2,D8)
 *      DSC Yellow --- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (Arduino Uno: 2-12 / esp8266: D1,D2,D8)
 *      DSC Green ---- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (Arduino Uno: 2-12 / esp8266: D1,D2,D8)
 *            Ground --- NPN emitter --/
 *
 *  Power (when disconnected from USB):
 *      DSC Aux(+) ---+--- Arduino Vin pin
 *                    |
 *                    +--- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
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
#include <PubSubClient.h>
#include <dscKeybusInterface.h>

const char* wifiSSID = "";
const char* wifiPassword = "";
const char* accessCode = "";  // An access code is required to disarm/night arm and may be required to arm based on panel configuration.
const char* mqttServer = "";

const char* mqttClientName = "dscKeybusInterface";
const char* mqttPublishTopic = "dsc/Get";    // Sends partition armed and alarm status
const char* mqttSubscribeTopic = "dsc/Set";  // Receives messages to write to the panel
const char* mqttZoneTopic = "dsc/Get/Zone";  // Sends zone status - the zone number will be appended to this topic name: dsc/Get/Zone1 ... dsc/Get/Zone64
const char* mqttFireTopic = "dsc/Get/Fire";  // Sends fire status
unsigned long mqttPreviousTime;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// Configures the Keybus interface with the specified pins - dscWritePin is
// optional, leaving it out disables the virtual keypad
#define dscClockPin D1   // GPIO5
#define dscReadPin D2    // GPIO4
#define dscWritePin D8   // GPIO15
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.print("WiFi connected: ");
  Serial.println(WiFi.localIP());

  mqtt.setServer(mqttServer, 1883);
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

    // Publishes exit delay status
    if (dsc.exitDelayChanged) {
      dsc.exitDelayChanged = false;  // Resets the exit delay status flag
      if (dsc.exitDelay) mqtt.publish(mqttPublishTopic, "pending", true);  // Publish as a retained message
    }

    // Publishes armed status
    if (dsc.partitionArmedChanged) {
      dsc.partitionArmedChanged = false;  // Resets the partition armed status flag
      if (dsc.partitionArmed) {
        if (dsc.partitionArmedAway) mqtt.publish(mqttPublishTopic, "armed_away", true);
        else if (dsc.partitionArmedStay) mqtt.publish(mqttPublishTopic, "armed_home", true);
      }
      else mqtt.publish(mqttPublishTopic, "disarmed", true);
    }

    // Publishes alarm status
    if (dsc.partitionAlarmChanged) {
      dsc.partitionAlarmChanged = false;  // Resets the partition alarm status flag
      if (dsc.partitionAlarm) mqtt.publish(mqttPublishTopic, "triggered", true);
    }

    // Publishes the fire alarm status
    if (dsc.fireStatusChanged) {
      dsc.fireStatusChanged = false;                         // Resets the fire alarm status flag
      if (dsc.fireStatus) mqtt.publish(mqttFireTopic, "1");  // Fire alarm tripped
      else mqtt.publish(mqttFireTopic, "0");                 // Fire alarm restored
    }

    // Publishes zones 1-64 status in a separate topic per zone
    // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones:
    //   openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
    //   openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
    //   ...
    //   openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
    if (dsc.openZonesStatusChanged) {
      dsc.openZonesStatusChanged = false;                           // Resets the open zones status flag
      for (byte zoneGroup = 0; zoneGroup < 8; zoneGroup++) {
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

  // Arm stay
  if (payload[0] == 'S' && !dsc.partitionArmed && !dsc.exitDelay) {
    while (!dsc.writeReady) dsc.handlePanel();  // Continues processing Keybus data until ready to write
    dsc.write('s');  // Virtual keypad arm stay
  }

  // Arm away
  else if (payload[0] == 'A' && !dsc.partitionArmed && !dsc.exitDelay) {
    while (!dsc.writeReady) dsc.handlePanel();
    dsc.write('w');  // Virtual keypad arm away
  }

  // Disarm
  else if (payload[0] == 'D' && (dsc.partitionArmed || dsc.exitDelay)) {
    while (!dsc.writeReady) dsc.handlePanel();
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
  if (mqtt.connect(mqttClientName)) {
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

