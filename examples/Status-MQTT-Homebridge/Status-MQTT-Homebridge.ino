/*
 *  DSC Status with MQTT (Arduino, esp8266)
 *
 *  Processes the security system status and allows for control using Apple HomeKit, including the iOS Home app and
 *  Siri.  This uses MQTT to interface with Homebridge and the homebridge-mqttthing plugin for HomeKit integration
 *  and demonstrates using the armed and alarm states for the HomeKit securitySystem object, as well as the zone states
 *  for the contactSensor objects.
 *
 *  Homebridge: https://github.com/nfarina/homebridge
 *  homebridge-mqttthing: https://github.com/arachnetech/homebridge-mqttthing
 *  Mosquitto MQTT broker: https://mosquitto.org
 *
 *  In this example, the commands to set the alarm state are setup in Homebridge as:
 *    Stay arm: "S"
 *    Away arm: "A"
 *    Night arm (arm without an entry delay): "N"
 *    Disarm: "D"
 *
 *  The interface listens for commands in the configured mqttSubscibeTopic, and publishes alarm states to the
 *  configured mqttPublishTopic:
 *    Stay arm: "SA"
 *    Away arm: "AA"
 *    Night arm: "NA"
 *    Disarm: "D"
 *    Alarm tripped: "T"
 *
 *  Zone states are published in a separate topic per zone with the configured mqttZoneTopic appended with the zone
 *  number.  The zone state is published as an integer:
 *    "0": closed
 *    "1": open
 *
 *  Example Homebridge config.json "accessories" configuration:

        {
            "accessory": "mqttthing",
            "type": "securitySystem",
            "name": "Security System",
            "url": "http://127.0.0.1:1883",
            "topics":
            {
                "getCurrentState":    "dsc/Get",
                "setTargetState":     "dsc/Set"
            },
            "targetStateValues": ["S", "A", "N", "D"]
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
            "name": "Zone 8",
            "url": "http://127.0.0.1:1883",
            "topics":
            {
                "getContactSensorState": "dsc/Get/Zone8"
            },
            "integerValue": "true"
        }

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
const char* accessCode = "";  // An access code is required to disarm and night arm
const char* mqttServer = "";

const char* mqttClientName = "dscKeybusInterface";
const char* mqttPublishTopic = "dsc/Get";    // Provides status updates
const char* mqttSubscribeTopic = "dsc/Set";  // Writes to the panel
const char* mqttZoneTopic = "dsc/Get/Zone";  // Zone number will be appended to this topic name: dsc/Get/Zone1, dsc/Get/Zone64
unsigned long mqttPreviousTime;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// Configures the Keybus interface with the specified pins - dscWritePin is
// optional, leaving it out disables the virtual keypad
#define dscClockPin D1
#define dscReadPin D2
#define dscWritePin D8
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

    // Publishes armed status
    if (dsc.partitionArmedChanged) {
      dsc.partitionArmedChanged = false;  // Resets the partition armed status flag
      if (dsc.partitionArmed) {
        if (dsc.partitionArmedAway && dsc.armedNoEntryDelay) mqtt.publish(mqttPublishTopic, "NA", true);       // Night armed
        else if (dsc.partitionArmedAway) mqtt.publish(mqttPublishTopic, "AA", true);                           // Away armed
        else if (dsc.partitionArmedStay && dsc.armedNoEntryDelay) mqtt.publish(mqttPublishTopic, "NA", true);  // Night armed
        else if (dsc.partitionArmedStay) mqtt.publish(mqttPublishTopic, "SA", true);                           // Stay armed
      }
      else mqtt.publish(mqttPublishTopic, "D", true);                                                          // Disarmed
    }

    // Publishes alarm status
    if (dsc.partitionAlarmChanged) {
      dsc.partitionAlarmChanged = false;                                  // Resets the partition alarm status flag
      if (dsc.partitionAlarm) mqtt.publish(mqttPublishTopic, "T", true);  // Alarm tripped
    }

    // Publishes zones 1-64 status in a separate topic per zone
    // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones
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
              mqtt.publish(zonePublishTopic, "1", true);                  // Zone open
            }
            else mqtt.publish(zonePublishTopic, "0", true);               // Zone closed
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

  // homebridge-mqttthing STAY_ARM
  if (payload[0] == 'S' && !dsc.partitionArmed && !dsc.exitDelay) {
    while (!dsc.writeReady) dsc.handlePanel();  // Continues processing Keybus data until ready to write
    dsc.write('s');  // Keypad stay arm
  }

  // homebridge-mqttthing AWAY_ARM
  else if (payload[0] == 'A' && !dsc.partitionArmed && !dsc.exitDelay) {
    while (!dsc.writeReady) dsc.handlePanel();
    dsc.write('w');  // Keypad away arm
  }

  // homebridge-mqttthing NIGHT_ARM - sends *9 to set no entry delay, then arms with the access code
  else if (payload[0] == 'N' && !dsc.partitionArmed && !dsc.exitDelay) {
    while (!dsc.writeReady) dsc.handlePanel();
    dsc.write('*');
    while (!dsc.writeReady) dsc.handlePanel();
    dsc.write('9');
    while (!dsc.writeReady) dsc.handlePanel();
    dsc.write(accessCode);
  }

  // homebridge-mqttthing DISARM
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

