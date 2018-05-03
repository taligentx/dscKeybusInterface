/*
 *  DSC Status with MQTT (esp8266)
 *
 *  Processes the security system status and allows for control using Apple HomeKit, including the iOS Home app and
 *  Siri.  This uses MQTT to interface with Homebridge and homebridge-mqttthing for HomeKit integration and
 *  demonstrates using the armed and alarm states for the HomeKit securitySystem object, as well as the zone states 
 *  for the contactSensor objects.
 *
 *  Mosquitto MQTT broker: https://mosquitto.org
 *  Homebridge: https://github.com/nfarina/homebridge
 *  homebridge-mqttthing: https://github.com/arachnetech/homebridge-mqttthing
 *
 *  In this example, the alarm states are setup in Homebridge as:
 *    "S": stay arm, no access code required
 *    "A": away arm, no access code required
 *    "Nxxxx" - night arm with an access code (arms without an entry delay)
 *    "Dxxxx" - disarm with an access code
 *
 *  The interface publishes alarm states to topic "dsc/Get" and listens for commands in topic "dsc/Set".  Zone states
 *  are published on a separate topic per zone as "dsc/Get/Zonex" with values:
 *    "0": closed
 *    "1": open
 *
 *  Example Homebridge config.json "accessories" configuration:
 *
 *      {
 *          "accessory": "mqttthing",
 *          "type": "securitySystem",
 *          "name": "Security System",
 *          "url": "http://127.0.0.1:1883",
 *          "topics":
 *          {
 *              "getCurrentState":    "dsc/Get",
 *              "setTargetState":     "dsc/Set"
 *          },
 *          "targetStateValues": ["S", "A", "N1234", "D1234"]
 *      },
 *      {
 *          "accessory": "mqttthing",
 *          "type": "contactSensor",
 *          "name": "Zone 1",
 *          "url": "http://127.0.0.1:1883",
 *          "topics":
 *          {
 *              "getContactSensorState": "dsc/Get/Zone1"
 *          },
 *          "integerValue": "true"
 *      },
 *      {
 *          "accessory": "mqttthing",
 *          "type": "contactSensor",
 *          "name": "Zone 8",
 *          "url": "http://127.0.0.1:1883",
 *          "topics":
 *          {
 *              "getContactSensorState": "dsc/Get/Zone8"
 *          },
 *          "integerValue": "true"
 *      }
 *
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
const char* mqttServer = "";

const char* mqttClientName = "dscKeybusInterface";
const char* mqttPublishTopic = "dsc/Get";    // Provides status updates
const char* mqttSubscribeTopic = "dsc/Set";  // Writes to the panel
char mqttZoneTopic[] = "dsc/Get/Zone";       // Zone number will be appended to this topic name: dsc/Get/Zone1, etc
unsigned long mqttLastTime;
char publishMessage[50];  // Sets the maximum MQTT message size

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
  if (mqttConnect()) mqttLastTime = millis();
  else mqttLastTime = 0;

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();

  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {
  mqttHandle();

  if (dsc.handlePanel() && dsc.statusChanged) {  // Processes data only when a valid Keybus command has been read
    dsc.statusChanged = false;  // Reset the status tracking flag

    if (dsc.partitionArmedChanged) {
      dsc.partitionArmedChanged = false;
      if (dsc.partitionArmed) {
        if (dsc.partitionArmedAway && dsc.armedNoEntryDelay) mqtt.publish(mqttPublishTopic, "NA");
        else if (dsc.partitionArmedAway) mqtt.publish(mqttPublishTopic, "AA");
        else if (dsc.partitionArmedStay && dsc.armedNoEntryDelay) mqtt.publish(mqttPublishTopic, "NA");
        else if (dsc.partitionArmedStay) mqtt.publish(mqttPublishTopic, "SA");
      }
      else mqtt.publish(mqttPublishTopic, "D");
    }

    if (dsc.partitionAlarmChanged) {
      dsc.partitionAlarmChanged = false;
      if (dsc.partitionAlarm) mqtt.publish(mqttPublishTopic, "T");
    }

    if (dsc.openZonesGroup1Changed) {
      dsc.openZonesGroup1Changed = false;
      for (byte zoneCount = 0; zoneCount < 8; zoneCount++) {
        if (dsc.openZonesChanged[zoneCount]) {
            char zonePublishTopic[strlen(mqttZoneTopic) + 2];
            char zone[3];
            strcpy(zonePublishTopic, mqttZoneTopic);
            itoa(zoneCount + 1, zone, 10);
            strcat(zonePublishTopic, zone);
          if (dsc.openZones[zoneCount]) {
            mqtt.publish(zonePublishTopic, "1");
          }
          else {
            mqtt.publish(zonePublishTopic, "0");
          }
        }
      }
    }

    mqtt.subscribe(mqttSubscribeTopic);
  }
}


void mqttHandle() {
  if (!mqtt.connected()) {
    unsigned long mqttCurrentTime = millis();
    if (mqttCurrentTime - mqttLastTime > 5000) {
      mqttLastTime = mqttCurrentTime;
      if (mqttConnect()) {
        Serial.println("MQTT disconnected, successfully reconnected.");
        mqttLastTime = 0;
      }
      else Serial.println("MQTT disconnected, failed to reconnect.");
    }
  }
  else mqtt.loop();
}


boolean mqttConnect() {
  if (mqtt.connect(mqttClientName)) {
    Serial.print("MQTT connected: ");
    Serial.println(mqttServer);
    mqtt.subscribe(mqttSubscribeTopic);
  }
  else {
    Serial.print("MQTT connection failed: ");
    Serial.println(mqttServer);
  }
  return mqtt.connected();
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  topic = topic;

  // Homebridge-mqttthing STAY_ARM
  if (payload[0] == 'S' && !dsc.partitionArmed && !dsc.exitDelay) {
    while (!dsc.writeReady) dsc.handlePanel();
    dsc.write('s');  // Keypad stay arm
  }

  // Homebridge-mqttthing AWAY_ARM
  else if (payload[0] == 'A' && !dsc.partitionArmed && !dsc.exitDelay) {
    while (!dsc.writeReady) dsc.handlePanel();
    dsc.write('w');  // Keypad away arm
  }

  // Homebridge-mqttthing NIGHT_ARM - sends *9 to set no entry delay, then arms with the code in the payload
  else if (payload[0] == 'N' && !dsc.partitionArmed && !dsc.exitDelay) {
    while (!dsc.writeReady) dsc.handlePanel();
    dsc.write('*');
    while (!dsc.writeReady) dsc.handlePanel();
    dsc.write('9');
    for (byte i = 1; i < length; i++) {
      while (!dsc.writeReady) dsc.handlePanel();  // Blocks for ~55ms between characters
      dsc.write((char)payload[i]);
    }
  }

  // Homebridge-mqttthing DISARM
  else if (payload[0] == 'D' && (dsc.partitionArmed || dsc.exitDelay)) {
    for (byte i = 1; i < length; i++) {
      while (!dsc.writeReady) dsc.handlePanel();  // Blocks for ~55ms between characters
      dsc.write((char)payload[i]);
    }
  }
}

