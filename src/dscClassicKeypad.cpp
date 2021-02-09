/*
    DSC Keybus Interface

    https://github.com/taligentx/dscKeybusInterface

    This library is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dscClassicKeypad.h"

#if defined(ESP32)
portMUX_TYPE dscClassicKeypadInterface::timer0Mux = portMUX_INITIALIZER_UNLOCKED;

#if ESP_IDF_VERSION_MAJOR < 4
hw_timer_t * dscClassicKeypadInterface::timer0 = NULL;

#else  // ESP-IDF 4+
esp_timer_handle_t timer3;
const esp_timer_create_args_t timer3Parameters = { .callback = reinterpret_cast<esp_timer_cb_t>(&dscClassicKeypadInterface::dscClockInterrupt) };

#endif  // ESP_IDF_VERSION_MAJOR
#endif  // ESP32


dscClassicKeypadInterface::dscClassicKeypadInterface(byte setClockPin, byte setReadPin, byte setWritePin) {
  dscClockPin = setClockPin;
  dscReadPin = setReadPin;
  dscWritePin = setWritePin;
  commandReady = true;
  keyData = 0xFF;
  clockInterval = 50000;  // Sets AVR timer 1 to trigger an overflow interrupt every ~1ms to generate a 500Hz clock signal
  keyInterval = 150;
  alarmKeyInterval = 1000;
}


void dscClassicKeypadInterface::begin(Stream &_stream) {
  pinMode(dscClockPin, OUTPUT);
  pinMode(dscReadPin, INPUT);
  pinMode(dscWritePin, OUTPUT);
  digitalWrite(dscClockPin, LOW);
  digitalWrite(dscWritePin, LOW);
  stream = &_stream;

  // Platform-specific timers setup the Keybus 1kHz clock signal

  // Arduino/AVR Timer1 calls ISR(TIMER1_OVF_vect)
  #if defined(__AVR__)
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = clockInterval;
  TCCR1B |= (1 << CS10);

  // esp8266 timer1 calls dscClockInterrupt()
  #elif defined(ESP8266)
  timer1_isr_init();
  timer1_attachInterrupt(dscClockInterrupt);
  timer1_write(5000);

  // esp32 timer0 calls dscClockInterrupt()
  #elif defined(ESP32)
  #if ESP_IDF_VERSION_MAJOR < 4
  timer0 = timerBegin(0, 80, true);
  timerStop(timer0);
  timerAttachInterrupt(timer0, &dscClockInterrupt, true);
  timerAlarmWrite(timer0, 1000, true);
  timerAlarmEnable(timer0);
  #else  // IDF4+
  esp_timer_create(&timer3Parameters, &timer3);
  #endif  // ESP_IDF_VERSION_MAJOR
  #endif  // ESP32

  intervalStart = millis();

  unsigned long keybusTime = millis();
  while (millis() - keybusTime < 100) {  // Waits for the keypad to be powered on
    if (!digitalRead(dscReadPin)) keybusTime = millis();
    #if defined(ESP8266) || defined(ESP32)
    yield();
    #endif
  }
}


bool dscClassicKeypadInterface::loop() {

  // Sets up the next panel command once the previous command is complete
  if (commandReady && millis() - intervalStart >= commandInterval) {
    commandReady = false;

    // Sets lights
    if (panelLights != previousLights) {
      previousLights = panelLights;
      classicCommand[1] = panelLights;
    }

    // Sets zones
    if (panelZones != previousZones) {
      previousZones = panelZones;
      classicCommand[0] = panelZones;
    }

    // Key beep
    if (keyBeep) {
      if (!beepStart) {
        beepStart = true;
        beepInterval = millis();
        bitWrite(classicCommand[1], 0, 1);
      }
      else if (millis() - beepInterval > 100) {
        beepStart = false;
        keyBeep = false;
        bitWrite(classicCommand[1], 0, 0);
      }
    }

    // Sets next panel command
    for (byte i = 0; i < 2; i++) panelCommand[i] = classicCommand[i];
    panelCommandByteTotal = 2;

    clockCycleCount = 0;
    clockCycleTotal = panelCommandByteTotal * 16;

    #if defined(__AVR__)
    TIMSK1 |= (1 << TOIE1);  // Enables AVR Timer 1 interrupt
    #elif defined(ESP8266)
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
    #elif defined(ESP32)
    #if ESP_IDF_VERSION_MAJOR < 4
    timerStart(timer0);
    #else  // IDF4+
    esp_timer_start_periodic(timer3, 1000);
    #endif  // ESP_IDF_VERSION_MAJOR
    #endif
  }
  else if (!commandReady) intervalStart = millis();

  // Sets panel lights
  panelLight(lightReady, 7);
  panelLight(lightArmed, 6);
  panelLight(lightMemory, 5);
  panelLight(lightBypass, 4);
  panelLight(lightTrouble, 3);

  // Sets zone lights
  zoneLight(lightZone1, 7);
  zoneLight(lightZone2, 6);
  zoneLight(lightZone3, 5);
  zoneLight(lightZone4, 4);
  zoneLight(lightZone5, 3);
  zoneLight(lightZone6, 2);

  // Skips key processing if the key buffer is empty
  if (keyBufferLength == 0) return false;

  // Copies data from the buffer to keyData
  static byte keyBufferIndex = 1;
  byte dataIndex = keyBufferIndex - 1;
  keyData = keyBuffer[dataIndex];
  keyBufferIndex++;

  // Resets counters when the buffer is cleared
  #if defined(ESP32)
  portENTER_CRITICAL(&timer0Mux);
  #else
  noInterrupts();
  #endif

  if (keyBufferIndex > keyBufferLength) {
    keyBufferIndex = 1;
    keyBufferLength = 0;
  }

  #if defined(ESP32)
  portEXIT_CRITICAL(&timer0Mux);
  #else
  interrupts();
  #endif

  if (keyData != 0xFF) {
    keyAvailable = true;
    keyBeep = true;
    switch (keyData) {
      case 0xD7: key = 0x00; break;  // 0
      case 0xBE: key = 0x05; break;  // 1
      case 0xDE: key = 0x0A; break;  // 2
      case 0xEE: key = 0x0F; break;  // 3
      case 0xBD: key = 0x11; break;  // 4
      case 0xDD: key = 0x16; break;  // 5
      case 0xED: key = 0x1B; break;  // 6
      case 0xBB: key = 0x1C; break;  // 7
      case 0xDB: key = 0x22; break;  // 8
      case 0xEB: key = 0x27; break;  // 9
      case 0xB7: key = 0x28; break;  // *
      case 0xE7: key = 0x2D; break;  // #
      case 0x3F: key = 0x0B; break;  // Fire alarm
      case 0x5F: key = 0x0D; break;  // Aux alarm
      case 0x6F: key = 0x0E; break;  // Panic alarm
      default: keyAvailable = false; keyBeep = false; break;  // Skips other DSC key values and invalid data
    }
    keyData = 0xFF;
  }

  return true;
}


void dscClassicKeypadInterface::panelLight(Light lightPanel, byte zoneBit) {
  if (lightPanel == on) {
    bitWrite(panelLights, zoneBit, 1);
    bitWrite(panelBlink, zoneBit, 0);
  }
  else if (lightPanel == blink) bitWrite(panelBlink, zoneBit, 1);
  else {
    bitWrite(panelLights, zoneBit, 0);
    bitWrite(panelBlink, zoneBit, 0);
  }
}


void dscClassicKeypadInterface::zoneLight(Light lightZone, byte zoneBit) {
  if (lightZone == on ) {
    bitWrite(panelZones, zoneBit, 1);
    bitWrite(panelZonesBlink, zoneBit, 0);
  }
  else if (lightZone == blink) bitWrite(panelZonesBlink, zoneBit, 1);
  else {
    bitWrite(panelZones, zoneBit, 0);
    bitWrite(panelZonesBlink, zoneBit, 0);
  }
}



void dscClassicKeypadInterface::beep(byte beeps) {
  (void) beeps;
}


void dscClassicKeypadInterface::tone(byte beep, bool tone, byte interval) {
  (void) beep;
  (void) tone;
  (void) interval;
}


void dscClassicKeypadInterface::buzzer(byte seconds) {
  (void) seconds;
}


#if defined(__AVR__)
void dscClassicKeypadInterface::dscClockInterrupt() {
#elif defined(ESP8266)
void ICACHE_RAM_ATTR dscClassicKeypadInterface::dscClockInterrupt() {
#elif defined(ESP32)
void IRAM_ATTR dscClassicKeypadInterface::dscClockInterrupt() {
#endif

  // Toggles the clock pin for the length of a panel command
  if (clockCycleCount < clockCycleTotal) {
    static bool clockHigh = true;
    if (clockHigh) {
      clockHigh = false;
      digitalWrite(dscClockPin, HIGH);
      digitalWrite(dscWritePin, LOW);
    }
    else {
      clockHigh = true;
      digitalWrite(dscClockPin, LOW);
      if (isrModuleByteCount < dscReadSize) {

        // Data is captured in each byte by shifting left by 1 bit and writing to bit 0
        if (isrModuleBitCount < 8) {
          isrModuleData[isrModuleByteCount] <<= 1;
          if (digitalRead(dscReadPin) == HIGH) {
            isrModuleData[isrModuleByteCount] |= 1;
          }
          else {
            moduleDataDetected = true;  // Keypads and modules send data by pulling the data line low
          }
        }

        // Increments the bit counter if the byte is incomplete
        if (isrModuleBitCount < 7) {
          isrModuleBitCount++;
        }

        // Byte is complete, set the counters for the next byte
        else {
          isrModuleBitCount = 0;
          isrModuleByteCount++;
        }

        isrModuleBitTotal++;
      }

      // Write panel data
      if (isrPanelBitCount == 7) {
        if (!bitRead(panelCommand[panelCommandByteCount], 0)) digitalWrite(dscWritePin, HIGH);
        isrPanelBitCount = 0;
        isrPanelBitTotal++;
        panelCommandByteCount++;
      }

      // Panel command bytes bits 0-6
      else if (panelCommandByteCount < panelCommandByteTotal) {
        byte bitCount = 0;
        for (byte i = 7; i > 0; i--) {
          if (isrPanelBitCount == bitCount && !bitRead(panelCommand[panelCommandByteCount], i)) digitalWrite(dscWritePin, HIGH);
          bitCount++;
        }
        isrPanelBitCount++;
        isrPanelBitTotal++;
      }
    }
    clockCycleCount++;
  }

  // Panel command complete
  else {
    digitalWrite(dscClockPin, LOW);

    // Checks for module data
    if (moduleDataDetected) {
      moduleDataDetected = false;
      for (byte i = 0; i < dscReadSize; i++) moduleData[i] = isrModuleData[i];

      if (isrModuleData[0] != 0xFF) {

        // Checks that alarm keys are pressed continuously for the alarmKeyInterval before setting the key
        if (isrModuleData[0] == 0x3F || isrModuleData[0] == 0x5F || isrModuleData[0] == 0x6F) {
          if (!alarmKeyDetected) {
            alarmKeyDetected = true;
            alarmKeyTime = millis();
          }
          else if (millis() - alarmKeyTime > alarmKeyInterval) {
            keyBuffer[keyBufferLength] = isrModuleData[0];
            keyBufferLength++;
            alarmKeyDetected = false;
          }
          else {
            keyBuffer[keyBufferLength] = 0xFF;
            keyBufferLength++;
          }
        }

        // Checks for regular keys and debounces for keyInterval
        else {
          alarmKeyDetected = false;
          alarmKeyTime = millis();

          // Skips the debounce interval if a different key is pressed
          if (keyBuffer[keyBufferLength] != isrModuleData[0]) {
            keyBuffer[keyBufferLength] = isrModuleData[0];
            keyBufferLength++;
            repeatInterval = millis();
          }

          // Sets the key
          else if (millis() - repeatInterval > keyInterval) {
            keyBuffer[keyBufferLength] = isrModuleData[0];
            keyBufferLength++;
            repeatInterval = millis();
          }
        }
      }
    }
    else {
      alarmKeyDetected = false;
      alarmKeyTime = millis();
    }

    // Resets counters
    for (byte i = 0; i < dscReadSize; i++) isrModuleData[i] = 0;
    isrModuleBitTotal = 0;
    isrModuleBitCount = 0;
    isrModuleByteCount = 0;
    panelCommandByteCount = 0;
    isrPanelBitTotal = 0;
    isrPanelBitCount = 0;
    commandReady = true;

    #if defined(__AVR__)
    TIMSK1 = 0;  // Disables AVR Timer 1 interrupt
    #elif defined(ESP8266)
    timer1_disable();
    #elif defined(ESP32)
    #if ESP_IDF_VERSION_MAJOR < 4
    timerStop(timer0);
    #else  // IDF4+
    esp_timer_stop(timer3);
    #endif  // ESP_IDF_VERSION_MAJOR
    #endif
  }

  #if defined(__AVR__)
  TCNT1 = clockInterval;
  #endif
}
