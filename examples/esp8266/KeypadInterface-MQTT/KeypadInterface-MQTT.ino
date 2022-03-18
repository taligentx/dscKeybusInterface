/*
 *  DSC Keypad Interface-MQTT 1.2 (esp8266)
 *
 *  Emulates a DSC panel to directly interface DSC PowerSeries or Classic series keypads as physical
 *  input devices for any general purpose, without needing a DSC panel.  This sketch uses MQTT to
 *  send pressed keypad keys and receive commands to control keypad lights and tones.
 *
 *  PowerSeries keypad features:
 *    - Read keypad key button presses, including fire/aux/panic alarm keys: dsc.key
 *    - Set keypad lights: Ready, Armed, Trouble, Memory, Bypass, Fire, Program, Backlight, Zones 1-8: dsc.lightReady, dsc.lightZone1, etc
 *    - Set keypad beeps, 1-128: dsc.beep(3)
 *    - Set keypad buzzer in seconds, 1-255: dsc.tone(5)
 *    - Set keypad tone pattern with a number of beeps, an optional constant tone, and the interval in seconds between beeps:
 *        2 beeps, no constant tone, 4 second interval: dsc.tone(2, false, 4)
 *        3 beeps, constant tone, 2 second interval: dsc.tone(3, true, 2)
 *        Disable the tone: dsc.tone() or dsc.tone(0, false, 0)
 *
 *  Classic keypad features:
 *    - Read keypad key button presses, including fire/aux/panic alarm keys: dsc.key
 *    - Set keypad lights: Ready, Armed, Trouble, Memory, Bypass, Fire, Program, Zones 1-8: dsc.lightReady, dsc.lightZone1, etc
 *
 *  This interface uses a different wiring setup from the standard Keybus interface, adding
 *  an NPN transistor on dscClockPin.  The DSC keypads require a 12v DC power source, though
 *  lower voltages down to 7v may work for key presses (the LEDs will be dim).
 *
 *  Release notes:
 *    1.3 - Add Classic keypad support - PC2550RK
 *    1.2 - Add Classic keypad support - PC1500RK
 *    1.1 - Add keypad beep, buzzer, constant tone
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Keypad R --- 12v DC
 *
 *      DSC Keypad B --- esp8266 ground
 *
 *      DSC Keypad Y ---+--- 1k ohm resistor --- 12v DC
 *                      |
 *                      +--- NPN collector --\
 *                                            |-- NPN base --- 1k ohm resistor --- dscClockPin  // esp8266: D1, GPIO 5
 *                  Ground --- NPN emitter --/
 *
 *      DSC Keypad G ---+--- 1k ohm resistor --- 12v DC
 *                      |
 *                      +--- 33k ohm resistor ---+--- dscReadPin  // esp8266: D2, GPIO 4
 *                      |                        |
 *                      |                        +--- 10k ohm resistor --- Ground
 *                      |
 *                      +--- NPN collector --\
 *                                            |-- NPN base --- 1k ohm resistor --- dscWritePin  // esp8266: D8, GPIO 15
 *                  Ground --- NPN emitter --/
 *
 *  The keypad interface uses NPN transistors to pull the clock and data lines low - most small
 *  signal NPN transistors should be suitable, for example:
 *    - 2N3904
 *    - BC547, BC548, BC549
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  This example code is in the public domain.
 */

// Set the keypad type
#define dscKeypad
//#define dscClassicKeypad

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <dscKeybusInterface.h>

// Settings
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* mqttServer = "";    // MQTT server domain name or IP address
const int   mqttPort = 1883;    // MQTT server port
const char* mqttUsername = "";  // Optional, leave blank if not required
const char* mqttPassword = "";  // Optional, leave blank if not required

// MQTT topics
const char* mqttClientName = "dscKeypadInterface";
const char* mqttKeyTopic = "dsc/Key";               // Sends keypad keys
const char* mqttSubscribeTopic = "dsc/Set";         // Receives messages to send to the keypad

// Configures the Keybus interface with the specified pins
#define dscClockPin D1  // GPIO 5
#define dscReadPin  D2  // GPIO 4
#define dscWritePin D8  // GPIO 15

// Initialize components
#ifdef dscKeypad
dscKeypadInterface dsc(dscClockPin, dscReadPin, dscWritePin);
#else
dscClassicKeypadInterface dsc(dscClockPin, dscReadPin, dscWritePin);
#endif
bool lightOff, lightBlink, inputReceived;
const byte inputLimit = 255;
char input[inputLimit];
byte beepLength, buzzerLength, toneLength;
WiFiClient ipClient;
PubSubClient mqtt(mqttServer, mqttPort, ipClient);
unsigned long mqttPreviousTime;


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

  Serial.print(F("Keybus...."));
  dsc.begin();
  Serial.println(F("connected."));
  Serial.println(F("DSC Keypad Interface is online."));
}

void loop() {
  mqttHandle();

  /*
   *  Sets keypad status via serial with the listed keys. Light status uses custom
   *  values for control: off, on, blink (example: dsc.lightReady = blink;)
   *
   *  Light on: Send the keys listed below.  Turning on the armed light: "a"
   *  Light off: Send "-" before a light key to turn it off.  Turning off the zone 4 light: "-4"
   *  Light blink: Send "!" before a light key to blink.  Blinking the ready light: "!r"
   *  Beep: Send "b" followed by the number of beeps, 1-128.  Setting 2 beeps: "b2"
   *  Buzzer: Send "z" followed by the buzzer length in seconds, 1-255.  Setting the buzzer to 5 seconds: "z5"
   *  Tone pattern: Send "n" followed by the number of beeps 1-7, constant tone true "t" or false "f", interval between beeps 1-15s
   *        Setting a tone pattern with 2 beeps, no constant tone, 4 second interval: "n2f4"
   *        Setting a tone pattern with 3 beeps, constant tone, 2 second interval: "n3t2"
   *        Disabling the tone pattern: "n"
   */
  if (inputReceived) {
    inputReceived = false;

    for (byte i = 0; i < strlen(input); i++) {
      switch (input[i]) {
        case 'r': case 'R': dsc.lightReady = setLight(); break;
        case 'a': case 'A': dsc.lightArmed = setLight(); break;
        case 'm': case 'M': dsc.lightMemory = setLight(); break;
        case 'y': case 'Y': dsc.lightBypass = setLight(); break;
        case 't': case 'T': dsc.lightTrouble = setLight(); break;
        case 'p': case 'P': dsc.lightProgram = setLight(); break;
        case 'f': case 'F': dsc.lightFire = setLight(); break;
        case 'l': case 'L': dsc.lightBacklight = setLight(); break;
        case '1': dsc.lightZone1 = setLight(); break;
        case '2': dsc.lightZone2 = setLight(); break;
        case '3': dsc.lightZone3 = setLight(); break;
        case '4': dsc.lightZone4 = setLight(); break;
        case '5': dsc.lightZone5 = setLight(); break;
        case '6': dsc.lightZone6 = setLight(); break;
        case '7': dsc.lightZone7 = setLight(); break;
        case '8': dsc.lightZone8 = setLight(); break;
        case 'b': case 'B': sendBeeps(i); i += beepLength; break;
        case 'n': case 'N': sendTone(i); i+= toneLength; break;
        case 'z': case 'Z': sendBuzzer(i); i+= buzzerLength; break;
        case '-': lightOff = true; break;
        case '!': lightBlink = true; break;
        default: break;
      }
    }
  }

  dsc.loop();

  // Checks for a keypad key press
  if (dsc.keyAvailable) {
    dsc.keyAvailable = false;
    switch (dsc.key) {
      case 0x00: mqtt.publish(mqttKeyTopic, "0", false); break;
      case 0x05: mqtt.publish(mqttKeyTopic, "1", false); break;
      case 0x0A: mqtt.publish(mqttKeyTopic, "2", false); break;
      case 0x0F: mqtt.publish(mqttKeyTopic, "3", false); break;
      case 0x11: mqtt.publish(mqttKeyTopic, "4", false); break;
      case 0x16: mqtt.publish(mqttKeyTopic, "5", false); break;
      case 0x1B: mqtt.publish(mqttKeyTopic, "6", false); break;
      case 0x1C: mqtt.publish(mqttKeyTopic, "7", false); break;
      case 0x22: mqtt.publish(mqttKeyTopic, "8", false); break;
      case 0x27: mqtt.publish(mqttKeyTopic, "9", false); break;
      case 0x28: mqtt.publish(mqttKeyTopic, "*", false); break;
      case 0x2D: mqtt.publish(mqttKeyTopic, "#", false); break;
      case 0x82: mqtt.publish(mqttKeyTopic, "Enter", false); break;
      case 0xAF: mqtt.publish(mqttKeyTopic, "Arm: Stay", false); break;
      case 0xB1: mqtt.publish(mqttKeyTopic, "Arm: Away", false); break;
      case 0xBB: mqtt.publish(mqttKeyTopic, "Door chime", false); break;
      case 0xDA: mqtt.publish(mqttKeyTopic, "Reset", false); break;
      case 0xE1: mqtt.publish(mqttKeyTopic, "Quick exit", false); break;
      case 0xF7: mqtt.publish(mqttKeyTopic, "Menu navigation", false); break;
      case 0x0B: mqtt.publish(mqttKeyTopic, "Fire alarm", false); break;
      case 0x0D: mqtt.publish(mqttKeyTopic, "Aux alarm", false); break;
      case 0x0E: mqtt.publish(mqttKeyTopic, "Panic alarm", false); break;
    }
    mqtt.subscribe(mqttSubscribeTopic);
  }
}


// Parse the number of beeps from the input
void sendBeeps(byte position) {
  char inputNumber[4];
  byte beeps = 0;
  beepLength = 0;

  for (byte i = position + 1; i < strlen(input); i++) {
    if (input[i] >= '0' && input[i] <= '9') {
      inputNumber[beepLength] = input[i];
      beepLength++;
      if (beepLength >= 3) break;
    }
    else break;
  }

  inputNumber[beepLength] = '\0';
  beeps = atoi(inputNumber);
  if (beeps > 128) beeps = 128;

  dsc.beep(beeps);
}


// Parse the buzzer length in seconds from the input
void sendBuzzer(byte position) {
  char inputNumber[4];
  byte buzzerSeconds = 0;
  buzzerLength = 0;

  for (byte i = position + 1; i < strlen(input); i++) {
    if (input[i] >= '0' && input[i] <= '9') {
      inputNumber[buzzerLength] = input[i];
      buzzerLength++;
      if (buzzerLength >= 3) break;
    }
    else break;
  }

  inputNumber[buzzerLength] = '\0';
  buzzerSeconds = atoi(inputNumber);
  dsc.buzzer(buzzerSeconds);
}


// Parse the tone pattern number of beeps, constant tone state, and interval in seconds from the input
void sendTone(byte position) {
  byte beeps = 0, interval = 0, intervalLength = 0;
  char beepNumber[2];
  bool toneState;
  char intervalNumber[3];
  toneLength = 0;

  if (strlen(input) < 4) {
    dsc.tone(0, false, 0);
    return;
  }

  // Parse beeps 0-7
  if (input[position + 1] >= '0' && input[position + 1] <= '9') {
    beepNumber[0] = input[position + 1];
    beeps = atoi(beepNumber);
    if (beeps > 7) beeps = 7;
    toneLength++;
  }
  else return;

  // Parse constant tone value
  switch (input[position + 2]) {
    case 't':
    case 'T': toneState = true; toneLength++; break;
    case 'f':
    case 'F': toneState = false; toneLength++; break;
    default: toneLength--; return;
  }

  // Parse interval
  for (byte i = position + 3; i < strlen(input); i++) {
    if (input[i] >= '0' && input[i] <= '9') {
      intervalNumber[intervalLength] = input[i];
      intervalLength++;
      toneLength++;
      if (intervalLength >= 2) break;
    }
    else break;
  }
  intervalNumber[intervalLength] = '\0';
  interval = atoi(intervalNumber);
  if (interval > 15) interval = 15;

  dsc.tone(beeps, toneState, interval);
}


// Sets keypad lights state - lights use custom values for control: off, on, blink (example: dsc.lightReady = blink;)
Light setLight() {
  if (lightOff) {
    lightOff = false;
    return off;
  }
  else if (lightBlink) {
    lightBlink = false;
    return blink;
  }
  else return on;
}


// Handles messages received in the mqttSubscribeTopic
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  // Handles unused parameters
  (void)topic;

  for (unsigned int i = 0; i < length; i++) {
    input[i] = payload[i];
  }

  input[length] = '\0';
  if (input[0] == '\0') inputReceived = false;
  else inputReceived = true;
}


void mqttHandle() {
  if (!mqtt.connected()) {
    unsigned long mqttCurrentTime = millis();
    if (mqttCurrentTime - mqttPreviousTime > 5000) {
      mqttPreviousTime = mqttCurrentTime;
      if (mqttConnect()) {
        Serial.println(F("MQTT disconnected, successfully reconnected."));
        mqttPreviousTime = 0;
        mqtt.subscribe(mqttSubscribeTopic);
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
    mqtt.subscribe(mqttSubscribeTopic);
  }
  else {
    Serial.print(F("connection error: "));
    Serial.println(mqttServer);
  }
  return mqtt.connected();
}
